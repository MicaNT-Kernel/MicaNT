#pragma once

/**
 * @file rdcb.hpp
 * @brief Clean-room Remote Desktop Connection Broker Subsystem (RD Connection Broker / MS-RDPBCGR).
 *
 * Implements the Win32 Remote Desktop Connection Broker architecture:
 * Session Directory Subsystem, Remote Desktop Connection Broker service (tssdis.exe, Tssdis),
 * MS-RDPBCGR Section 2.2.1.13 Server Redirection PDU and session routing token generation,
 * Multi-host RD Session Host (RDSH) collections (RemoteApp and Desktop Pools),
 * Dynamic load balancing (Least Sessions, Round Robin, Resource-Weighted),
 * Reconnection routing for disconnected user sessions, and host drain modes.
 *
 * Strict clean-room implementation referencing Microsoft's MIT-licensed win32metadata
 * under 17 U.S.C. § 1201(f) & Google LLC v. Oracle America, Inc. (141 S. Ct. 1183, 2021).
 * Zero proprietary, leaked, or decompiled code.
 *
 * Trademark Notice: Windows, Windows Server, and Remote Desktop are trademarks
 * or registered trademarks of Microsoft Corporation in the United States and other
 * countries. Referenced solely for interoperability and compatibility purposes.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <mutex>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <chrono>

#include "micant/shared/ntdef.hpp"
#include "micant/um/native/ldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::rdcb {

// ============================================================================
// 1. Win32 RD Connection Broker Constants & Enums (win32metadata / MS-RDPBCGR)
// ============================================================================

inline constexpr uint16_t  RDCB_DEFAULT_PORT          = 5504;
inline constexpr uint16_t  RDP_STANDARD_PORT          = 3389;

// MS-RDPBCGR Server Redirection Flags (Section 2.2.1.13)
inline constexpr uint32_t  TARGET_NET_ADDRESS         = 0x00000001;
inline constexpr uint32_t  LOAD_BALANCE_INFO          = 0x00000002;
inline constexpr uint32_t  TARGET_NET_PORTS           = 0x00000004;
inline constexpr uint32_t  TARGET_NET_ADDRESSES       = 0x00000008;
inline constexpr uint32_t  REDIRECT_USERNAME          = 0x00000010;
inline constexpr uint32_t  DONT_STORE_USERNAME        = 0x00000020;
inline constexpr uint32_t  PASSWORD_IS_SC_PIN         = 0x00000040;
inline constexpr uint32_t  REDIRECTED_SESSIONID       = 0x00000080;
inline constexpr uint32_t  TARGET_FQDN                = 0x00000100;
inline constexpr uint32_t  TARGET_NETBIOS             = 0x00000200;

// Collection Types
enum CollectionType : uint32_t {
    CollectionRemoteApp      = 0,
    CollectionPooledDesktop  = 1,
    CollectionPersonalDesktop= 2
};

// Load Balancing Algorithms
enum LoadBalancingModel : uint32_t {
    ModelLeastSessions       = 0, // Route to host with lowest active session count
    ModelRoundRobin          = 1, // Simple round-robin across online non-drained hosts
    ModelResourceWeighted    = 2  // Weighted by server capacity, CPU load, and free RAM
};

// RDSH Server Drain Modes
enum ServerDrainMode : uint32_t {
    DrainModeDisabled        = 0, // Normal operation, accepts new connections
    DrainModeEnabled         = 1, // Draining mode, no new sessions; existing/reconnecting only
    DrainModeUntilRestart    = 2  // Draining mode until host reboots
};

// Session States in Directory
enum RdcbSessionState : uint32_t {
    StateActive              = 0,
    StateConnected           = 1,
    StateDisconnected        = 2,
    StateIdle                = 3,
    StateShadowing           = 4,
    StateTerminated          = 5
};

// ============================================================================
// 2. Data Structures
// ============================================================================

struct RdshServerRecord {
    std::string serverFqdn;            // e.g. "rdsh01.corp.micant.internal"
    std::string ipAddress;             // e.g. "192.168.1.101"
    uint16_t    port = 3389;
    uint32_t    relativeWeight = 100;  // 1..100
    uint32_t    maxSessions = 100;
    ServerDrainMode drainMode = DrainModeDisabled;
    bool        isOnline = true;
    uint32_t    activeSessions = 0;
    uint32_t    disconnectedSessions = 0;
    uint32_t    cpuUsagePercent = 5;   // 0..100
    uint32_t    freeMemoryMb = 16384;  // Free RAM in MB
    uint64_t    lastHeartbeat = 0;
};

struct CollectionRecord {
    std::string collectionName;        // e.g. "Engineering-Apps", "Default-Pool"
    std::string description;
    CollectionType type = CollectionRemoteApp;
    LoadBalancingModel balancingModel = ModelLeastSessions;
    std::vector<std::string> allowedUserGroups; // e.g. "Domain Users", "Engineers"
    std::vector<RdshServerRecord> memberServers;
    uint32_t roundRobinIndex = 0;

    // User Profile Disk (UPD) Configuration
    bool        enableUpd = true;
    std::string updStoragePath = "\\\\storage.corp.micant.internal\\Profiles$";
    uint32_t    updMaxSizeGb = 20;

    // Session Timeout Policies (in minutes; 0 = infinite)
    uint32_t    disconnectedTimeoutMinutes = 180; // 3 hours
    uint32_t    activeTimeoutMinutes = 0;
    uint32_t    idleTimeoutMinutes = 60;          // 1 hour
};

struct RdcbSessionRecord {
    uint32_t    sessionId = 0;         // Server session ID
    std::string userName;              // e.g. "delliott"
    std::string domainName;            // e.g. "MICANT"
    std::string collectionName;        // e.g. "Engineering-Apps"
    std::string hostServerFqdn;        // e.g. "rdsh01.corp.micant.internal"
    std::string hostIpAddress;         // e.g. "192.168.1.101"
    uint16_t    hostPort = 3389;
    RdcbSessionState state = StateActive;
    std::string clientName;            // e.g. "CLIENT-WORKSTATION-01"
    std::string clientIp;              // e.g. "10.0.0.50"
    uint64_t    connectTimestamp = 0;
    uint64_t    disconnectTimestamp = 0;
};

struct RedirectionTarget {
    bool        isSuccess = false;
    bool        isReconnection = false;
    std::string targetFqdn;
    std::string targetIp;
    uint16_t    targetPort = 3389;
    uint32_t    sessionId = 0;
    std::string routingToken;          // "Cookie: msts=..."
    std::string errorMessage;
    std::vector<uint8_t> redirectionPdu; // Binary MS-RDPBCGR Server Redirection PDU
};

// ============================================================================
// 3. Routing Token & PDU Builders
// ============================================================================

inline uint64_t getCurrentTimeSeconds() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

/**
 * @brief Generates canonical MS-RDPBCGR routing token ("Cookie: msts=...").
 *
 * Converts IPv4 address to 32-bit integer and encodes port + flags into standard format:
 * "Cookie: msts=<IP_DECIMAL>.<PORT>.<FLAGS>\r\n"
 */
inline std::string FormatMstsRoutingToken(const std::string& ipStr, uint16_t port) {
    uint32_t b0 = 0, b1 = 0, b2 = 0, b3 = 0;
    char dot = 0;
    std::istringstream iss(ipStr);
    if (iss >> b0 >> dot >> b1 >> dot >> b2 >> dot >> b3) {
        // Little-endian packed IP
        uint32_t ipVal = (b0) | (b1 << 8) | (b2 << 16) | (b3 << 24);
        std::ostringstream oss;
        oss << "Cookie: msts=" << ipVal << "." << port << ".0000\r\n";
        return oss.str();
    }
    // Fallback format
    return "Cookie: msts=364020290.15628.0000\r\n";
}

/**
 * @brief Encodes MS-RDPBCGR Server Redirection Packet (TS_SERVER_REDIRECTION_PACKET).
 */
inline std::vector<uint8_t> BuildServerRedirectionPdu(
    uint32_t flags,
    uint32_t sessionId,
    const std::string& targetIp,
    const std::string& routingToken,
    const std::string& userName
) {
    std::vector<uint8_t> pdu;

    // TPKT Header (4 bytes)
    pdu.push_back(0x03); // Version 3
    pdu.push_back(0x00); // Reserved
    pdu.push_back(0x00); // Length high (placeholder)
    pdu.push_back(0x00); // Length low (placeholder)

    // X.224 Data TPDU (3 bytes)
    pdu.push_back(0x02); // Length indicator
    pdu.push_back(0xF0); // Data TPDU
    pdu.push_back(0x80); // EOT

    // Redirection Packet Header
    // uint16_t flags, uint16_t length
    size_t payloadStart = pdu.size();
    
    // Flags (2 bytes)
    uint16_t f16 = static_cast<uint16_t>(flags);
    pdu.push_back(static_cast<uint8_t>(f16 & 0xFF));
    pdu.push_back(static_cast<uint8_t>((f16 >> 8) & 0xFF));

    // Length placeholder (2 bytes)
    pdu.push_back(0x00);
    pdu.push_back(0x00);

    // Session ID (4 bytes)
    pdu.push_back(static_cast<uint8_t>(sessionId & 0xFF));
    pdu.push_back(static_cast<uint8_t>((sessionId >> 8) & 0xFF));
    pdu.push_back(static_cast<uint8_t>((sessionId >> 16) & 0xFF));
    pdu.push_back(static_cast<uint8_t>((sessionId >> 24) & 0xFF));

    // Target Net Address (UTF-16LE with null terminator)
    if (flags & TARGET_NET_ADDRESS) {
        uint32_t lenBytes = static_cast<uint32_t>((targetIp.size() + 1) * 2);
        pdu.push_back(static_cast<uint8_t>(lenBytes & 0xFF));
        pdu.push_back(static_cast<uint8_t>((lenBytes >> 8) & 0xFF));
        pdu.push_back(static_cast<uint8_t>((lenBytes >> 16) & 0xFF));
        pdu.push_back(static_cast<uint8_t>((lenBytes >> 24) & 0xFF));
        for (char c : targetIp) {
            pdu.push_back(static_cast<uint8_t>(c));
            pdu.push_back(0x00);
        }
        pdu.push_back(0x00);
        pdu.push_back(0x00);
    }

    // Load Balance Info / Routing Token (ASCII with CRLF)
    if (flags & LOAD_BALANCE_INFO) {
        uint32_t tokenLen = static_cast<uint32_t>(routingToken.size());
        pdu.push_back(static_cast<uint8_t>(tokenLen & 0xFF));
        pdu.push_back(static_cast<uint8_t>((tokenLen >> 8) & 0xFF));
        pdu.push_back(static_cast<uint8_t>((tokenLen >> 16) & 0xFF));
        pdu.push_back(static_cast<uint8_t>((tokenLen >> 24) & 0xFF));
        for (char c : routingToken) {
            pdu.push_back(static_cast<uint8_t>(c));
        }
    }

    // Username (UTF-16LE)
    if ((flags & REDIRECT_USERNAME) && !userName.empty()) {
        uint32_t userLen = static_cast<uint32_t>((userName.size() + 1) * 2);
        pdu.push_back(static_cast<uint8_t>(userLen & 0xFF));
        pdu.push_back(static_cast<uint8_t>((userLen >> 8) & 0xFF));
        pdu.push_back(static_cast<uint8_t>((userLen >> 16) & 0xFF));
        pdu.push_back(static_cast<uint8_t>((userLen >> 24) & 0xFF));
        for (char c : userName) {
            pdu.push_back(static_cast<uint8_t>(c));
            pdu.push_back(0x00);
        }
        pdu.push_back(0x00);
        pdu.push_back(0x00);
    }

    // Finalize lengths
    uint16_t payloadLen = static_cast<uint16_t>(pdu.size() - payloadStart);
    pdu[payloadStart + 2] = static_cast<uint8_t>(payloadLen & 0xFF);
    pdu[payloadStart + 3] = static_cast<uint8_t>((payloadLen >> 8) & 0xFF);

    uint16_t totalLen = static_cast<uint16_t>(pdu.size());
    pdu[2] = static_cast<uint8_t>((totalLen >> 8) & 0xFF);
    pdu[3] = static_cast<uint8_t>(totalLen & 0xFF);

    return pdu;
}

// ============================================================================
// 4. Remote Desktop Connection Broker Subsystem Manager (Singleton)
// ============================================================================

class RdConnectionBrokerManager {
public:
    static RdConnectionBrokerManager& get() {
        static RdConnectionBrokerManager instance;
        return instance;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        collections_.clear();
        sessions_.clear();
        nextSessionId_ = 2000;
        totalRedirectsIssued_ = 0;
        totalReconnectsRouted_ = 0;
        totalHeartbeatsReceived_ = 0;

        // Initialize Default Farm: "QuickSessionCollection"
        CollectionRecord defColl{};
        defColl.collectionName = "QuickSessionCollection";
        defColl.description = "Primary Remote Desktop Session Host Collection";
        defColl.type = CollectionRemoteApp;
        defColl.balancingModel = ModelLeastSessions;
        defColl.allowedUserGroups = {"Domain Users", "Remote Desktop Users"};
        defColl.enableUpd = true;
        defColl.updStoragePath = "\\\\storage.micant.internal\\Profiles$";
        defColl.updMaxSizeGb = 20;

        // Host 1: RDSH-01
        RdshServerRecord srv1{};
        srv1.serverFqdn = "rdsh01.corp.micant.internal";
        srv1.ipAddress = "192.168.1.101";
        srv1.port = 3389;
        srv1.relativeWeight = 100;
        srv1.maxSessions = 50;
        srv1.drainMode = DrainModeDisabled;
        srv1.isOnline = true;
        srv1.activeSessions = 0;
        srv1.cpuUsagePercent = 8;
        srv1.freeMemoryMb = 24576;
        srv1.lastHeartbeat = getCurrentTimeSeconds();
        defColl.memberServers.push_back(srv1);

        // Host 2: RDSH-02
        RdshServerRecord srv2{};
        srv2.serverFqdn = "rdsh02.corp.micant.internal";
        srv2.ipAddress = "192.168.1.102";
        srv2.port = 3389;
        srv2.relativeWeight = 100;
        srv2.maxSessions = 50;
        srv2.drainMode = DrainModeDisabled;
        srv2.isOnline = true;
        srv2.activeSessions = 0;
        srv2.cpuUsagePercent = 12;
        srv2.freeMemoryMb = 20480;
        srv2.lastHeartbeat = getCurrentTimeSeconds();
        defColl.memberServers.push_back(srv2);

        collections_[defColl.collectionName] = defColl;

        // Initialize Specialized Farm: "Engineering-Apps"
        CollectionRecord engColl{};
        engColl.collectionName = "Engineering-Apps";
        engColl.description = "Dedicated High-Performance CAD & Engineering Farm";
        engColl.type = CollectionRemoteApp;
        engColl.balancingModel = ModelResourceWeighted;
        engColl.allowedUserGroups = {"Engineers", "Architects"};
        engColl.enableUpd = true;
        engColl.updStoragePath = "\\\\storage.micant.internal\\EngProfiles$";
        engColl.updMaxSizeGb = 50;

        RdshServerRecord eng1{};
        eng1.serverFqdn = "rdsh-eng01.corp.micant.internal";
        eng1.ipAddress = "192.168.1.110";
        eng1.port = 3389;
        eng1.relativeWeight = 100;
        eng1.maxSessions = 25;
        eng1.drainMode = DrainModeDisabled;
        eng1.isOnline = true;
        eng1.activeSessions = 0;
        eng1.cpuUsagePercent = 15;
        eng1.freeMemoryMb = 49152;
        eng1.lastHeartbeat = getCurrentTimeSeconds();
        engColl.memberServers.push_back(eng1);

        collections_[engColl.collectionName] = engColl;
    }

    // ------------------------------------------------------------------------
    // Collection Management
    // ------------------------------------------------------------------------

    bool createCollection(const CollectionRecord& col) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (col.collectionName.empty()) return false;
        if (collections_.find(col.collectionName) != collections_.end()) return false;
        collections_[col.collectionName] = col;
        return true;
    }

    bool removeCollection(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = collections_.find(name);
        if (it == collections_.end()) return false;
        collections_.erase(it);
        return true;
    }

    bool getCollection(const std::string& name, CollectionRecord& outRecord) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = collections_.find(name);
        if (it == collections_.end()) return false;
        outRecord = it->second;
        return true;
    }

    std::vector<CollectionRecord> getAllCollections() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<CollectionRecord> res;
        for (const auto& [name, col] : collections_) {
            res.push_back(col);
        }
        return res;
    }

    // ------------------------------------------------------------------------
    // RDSH Server Management within Collections
    // ------------------------------------------------------------------------

    bool addServerToCollection(const std::string& collectionName, const RdshServerRecord& srv) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = collections_.find(collectionName);
        if (it == collections_.end()) return false;

        for (const auto& existing : it->second.memberServers) {
            if (existing.serverFqdn == srv.serverFqdn || existing.ipAddress == srv.ipAddress) {
                return false; // Server already in collection
            }
        }
        it->second.memberServers.push_back(srv);
        return true;
    }

    bool removeServerFromCollection(const std::string& collectionName, const std::string& fqdn) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = collections_.find(collectionName);
        if (it == collections_.end()) return false;

        auto& srvs = it->second.memberServers;
        auto sIt = std::remove_if(srvs.begin(), srvs.end(), [&](const RdshServerRecord& s) {
            return s.serverFqdn == fqdn || s.ipAddress == fqdn;
        });
        if (sIt == srvs.end()) return false;
        srvs.erase(sIt, srvs.end());
        return true;
    }

    bool setServerDrainMode(const std::string& collectionName, const std::string& fqdn, ServerDrainMode mode) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = collections_.find(collectionName);
        if (it == collections_.end()) return false;

        for (auto& s : it->second.memberServers) {
            if (s.serverFqdn == fqdn || s.ipAddress == fqdn) {
                s.drainMode = mode;
                return true;
            }
        }
        return false;
    }

    bool updateServerVitals(
        const std::string& collectionName,
        const std::string& fqdn,
        uint32_t activeSessions,
        uint32_t cpuPercent,
        uint32_t freeRamMb
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        totalHeartbeatsReceived_++;
        auto it = collections_.find(collectionName);
        if (it == collections_.end()) return false;

        for (auto& s : it->second.memberServers) {
            if (s.serverFqdn == fqdn || s.ipAddress == fqdn) {
                s.activeSessions = activeSessions;
                s.cpuUsagePercent = cpuPercent;
                s.freeMemoryMb = freeRamMb;
                s.lastHeartbeat = getCurrentTimeSeconds();
                return true;
            }
        }
        return false;
    }

    // ------------------------------------------------------------------------
    // Session Directory Tracking
    // ------------------------------------------------------------------------

    uint32_t registerSession(
        const std::string& userName,
        const std::string& domainName,
        const std::string& collectionName,
        const std::string& hostFqdn,
        const std::string& hostIp,
        uint16_t hostPort,
        RdcbSessionState state,
        const std::string& clientName = "CLIENT",
        const std::string& clientIp = "127.0.0.1"
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t sId = nextSessionId_++;

        RdcbSessionRecord rec{};
        rec.sessionId = sId;
        rec.userName = userName;
        rec.domainName = domainName;
        rec.collectionName = collectionName;
        rec.hostServerFqdn = hostFqdn;
        rec.hostIpAddress = hostIp;
        rec.hostPort = hostPort;
        rec.state = state;
        rec.clientName = clientName;
        rec.clientIp = clientIp;
        rec.connectTimestamp = getCurrentTimeSeconds();
        rec.disconnectTimestamp = (state == StateDisconnected) ? getCurrentTimeSeconds() : 0;

        sessions_[sId] = rec;

        // Update server active session count
        auto it = collections_.find(collectionName);
        if (it != collections_.end()) {
            for (auto& s : it->second.memberServers) {
                if (s.serverFqdn == hostFqdn || s.ipAddress == hostIp) {
                    if (state == StateActive || state == StateConnected) {
                        s.activeSessions++;
                    } else if (state == StateDisconnected) {
                        s.disconnectedSessions++;
                    }
                    break;
                }
            }
        }

        return sId;
    }

    bool updateSessionState(uint32_t sessionId, RdcbSessionState newState) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) return false;

        RdcbSessionState oldState = it->second.state;
        it->second.state = newState;
        if (newState == StateDisconnected && oldState != StateDisconnected) {
            it->second.disconnectTimestamp = getCurrentTimeSeconds();
        }

        // Adjust host session counters
        auto cIt = collections_.find(it->second.collectionName);
        if (cIt != collections_.end()) {
            for (auto& s : cIt->second.memberServers) {
                if (s.serverFqdn == it->second.hostServerFqdn || s.ipAddress == it->second.hostIpAddress) {
                    if ((oldState == StateActive || oldState == StateConnected) && newState == StateDisconnected) {
                        if (s.activeSessions > 0) s.activeSessions--;
                        s.disconnectedSessions++;
                    } else if (oldState == StateDisconnected && (newState == StateActive || newState == StateConnected)) {
                        if (s.disconnectedSessions > 0) s.disconnectedSessions--;
                        s.activeSessions++;
                    } else if (newState == StateTerminated) {
                        if (oldState == StateActive && s.activeSessions > 0) s.activeSessions--;
                        if (oldState == StateDisconnected && s.disconnectedSessions > 0) s.disconnectedSessions--;
                    }
                    break;
                }
            }
        }

        return true;
    }

    std::vector<RdcbSessionRecord> getAllSessions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<RdcbSessionRecord> res;
        for (const auto& [id, s] : sessions_) {
            res.push_back(s);
        }
        return res;
    }

    // ------------------------------------------------------------------------
    // Target Resolution & Load Balancing (MS-RDPBCGR Redirection Engine)
    // ------------------------------------------------------------------------

    RedirectionTarget resolveConnectionTarget(
        const std::string& userName,
        const std::string& domainName,
        const std::string& requestedCollection
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        RedirectionTarget target{};

        // 1. Validate requested collection
        std::string colName = requestedCollection.empty() ? "QuickSessionCollection" : requestedCollection;
        auto colIt = collections_.find(colName);
        if (colIt == collections_.end()) {
            target.isSuccess = false;
            target.errorMessage = "Specified collection does not exist: " + colName;
            return target;
        }

        CollectionRecord& col = colIt->second;
        if (col.memberServers.empty()) {
            target.isSuccess = false;
            target.errorMessage = "No available RDSH servers provisioned in collection: " + colName;
            return target;
        }

        // 2. Pillar: Reconnection Check (Look for Disconnected Session)
        for (auto& [sId, sRec] : sessions_) {
            if (sRec.collectionName == colName &&
                sRec.userName == userName &&
                sRec.state == StateDisconnected) {
                // Found disconnected session! Verify host is still online
                for (const auto& srv : col.memberServers) {
                    if ((srv.serverFqdn == sRec.hostServerFqdn || srv.ipAddress == sRec.hostIpAddress) && srv.isOnline) {
                        // Reconnection route found!
                        target.isSuccess = true;
                        target.isReconnection = true;
                        target.targetFqdn = srv.serverFqdn;
                        target.targetIp = srv.ipAddress;
                        target.targetPort = srv.port;
                        target.sessionId = sRec.sessionId;
                        target.routingToken = FormatMstsRoutingToken(srv.ipAddress, srv.port);

                        uint32_t flags = TARGET_NET_ADDRESS | LOAD_BALANCE_INFO | REDIRECTED_SESSIONID | REDIRECT_USERNAME;
                        target.redirectionPdu = BuildServerRedirectionPdu(flags, sRec.sessionId, srv.ipAddress, target.routingToken, userName);

                        totalReconnectsRouted_++;
                        totalRedirectsIssued_++;
                        return target;
                    }
                }
            }
        }

        // 3. Pillar: Dynamic Load Balancing for New Sessions
        // Filter out offline and draining servers
        std::vector<size_t> eligibleIndices;
        for (size_t i = 0; i < col.memberServers.size(); ++i) {
            const auto& s = col.memberServers[i];
            if (s.isOnline && s.drainMode == DrainModeDisabled && s.activeSessions < s.maxSessions) {
                eligibleIndices.push_back(i);
            }
        }

        if (eligibleIndices.empty()) {
            target.isSuccess = false;
            target.errorMessage = "All servers in collection are currently offline, drained, or at maximum capacity.";
            return target;
        }

        size_t chosenIndex = eligibleIndices[0];

        switch (col.balancingModel) {
            case ModelLeastSessions: {
                // Find server with lowest activeSessions / weight ratio
                double minRatio = 1e9;
                for (size_t idx : eligibleIndices) {
                    const auto& s = col.memberServers[idx];
                    double weight = (s.relativeWeight > 0) ? static_cast<double>(s.relativeWeight) : 100.0;
                    double ratio = static_cast<double>(s.activeSessions) / weight;
                    if (ratio < minRatio) {
                        minRatio = ratio;
                        chosenIndex = idx;
                    }
                }
                break;
            }
            case ModelRoundRobin: {
                // Round-robin over eligible servers
                size_t rr = col.roundRobinIndex % eligibleIndices.size();
                chosenIndex = eligibleIndices[rr];
                col.roundRobinIndex = static_cast<uint32_t>((rr + 1) % eligibleIndices.size());
                break;
            }
            case ModelResourceWeighted: {
                // Score = (100 - CPU) * (FreeRAM_MB / 1024) - (activeSessions * 10)
                double bestScore = -1e9;
                for (size_t idx : eligibleIndices) {
                    const auto& s = col.memberServers[idx];
                    double cpuAvailable = (s.cpuUsagePercent < 100) ? (100.0 - s.cpuUsagePercent) : 1.0;
                    double ramGb = static_cast<double>(s.freeMemoryMb) / 1024.0;
                    double score = (cpuAvailable * ramGb) - (s.activeSessions * 10.0);
                    if (score > bestScore) {
                        bestScore = score;
                        chosenIndex = idx;
                    }
                }
                break;
            }
        }

        RdshServerRecord& targetServer = col.memberServers[chosenIndex];

        // 4. Provision Session & Build Redirection Token
        uint32_t newSessionId = nextSessionId_++;
        RdcbSessionRecord newSession{};
        newSession.sessionId = newSessionId;
        newSession.userName = userName;
        newSession.domainName = domainName.empty() ? "MICANT" : domainName;
        newSession.collectionName = colName;
        newSession.hostServerFqdn = targetServer.serverFqdn;
        newSession.hostIpAddress = targetServer.ipAddress;
        newSession.hostPort = targetServer.port;
        newSession.state = StateConnected;
        newSession.connectTimestamp = getCurrentTimeSeconds();
        sessions_[newSessionId] = newSession;

        targetServer.activeSessions++;

        target.isSuccess = true;
        target.isReconnection = false;
        target.targetFqdn = targetServer.serverFqdn;
        target.targetIp = targetServer.ipAddress;
        target.targetPort = targetServer.port;
        target.sessionId = newSessionId;
        target.routingToken = FormatMstsRoutingToken(targetServer.ipAddress, targetServer.port);

        uint32_t flags = TARGET_NET_ADDRESS | LOAD_BALANCE_INFO | REDIRECTED_SESSIONID | REDIRECT_USERNAME;
        target.redirectionPdu = BuildServerRedirectionPdu(flags, newSessionId, targetServer.ipAddress, target.routingToken, userName);

        totalRedirectsIssued_++;
        return target;
    }

    // ------------------------------------------------------------------------
    // Metrics & Telemetry
    // ------------------------------------------------------------------------

    uint32_t getActiveFarmsCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<uint32_t>(collections_.size());
    }

    uint32_t getTotalRegisteredHosts() const {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t count = 0;
        for (const auto& [name, c] : collections_) {
            count += static_cast<uint32_t>(c.memberServers.size());
        }
        return count;
    }

    uint32_t getIndexedSessionsCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<uint32_t>(sessions_.size());
    }

    uint64_t getTotalRedirectsIssued() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return totalRedirectsIssued_;
    }

    uint64_t getTotalReconnectsRouted() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return totalReconnectsRouted_;
    }

    uint64_t getTotalHeartbeatsReceived() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return totalHeartbeatsReceived_;
    }

private:
    RdConnectionBrokerManager() {
        reset();
    }

    mutable std::mutex mutex_;
    std::map<std::string, CollectionRecord> collections_;
    std::map<uint32_t, RdcbSessionRecord> sessions_;
    uint32_t nextSessionId_ = 2000;
    uint64_t totalRedirectsIssued_ = 0;
    uint64_t totalReconnectsRouted_ = 0;
    uint64_t totalHeartbeatsReceived_ = 0;
};

// ============================================================================
// 5. Win32 C ABI Parity Exports (tssdis.dll / rdcbapi.dll)
// ============================================================================

extern "C" {

inline int32_t RDCBOpenConnectionBroker(void** ppBroker) {
    if (!ppBroker) return -1;
    *ppBroker = &RdConnectionBrokerManager::get();
    return 0;
}

inline int32_t RDCBCloseConnectionBroker(void* pBroker) {
    return pBroker ? 0 : -1;
}

inline int32_t RDCBResolveTarget(
    const char* userName,
    const char* domainName,
    const char* collectionName,
    char* outIp,
    size_t outIpMax,
    uint16_t* outPort,
    uint32_t* outSessionId,
    uint8_t* outIsRecon
) {
    if (!userName || !outIp || !outPort || !outSessionId || !outIsRecon) return -1;

    std::string user(userName);
    std::string domain(domainName ? domainName : "");
    std::string col(collectionName ? collectionName : "");

    auto target = RdConnectionBrokerManager::get().resolveConnectionTarget(user, domain, col);
    if (!target.isSuccess) return -2;

    if (target.targetIp.size() + 1 > outIpMax) return -3;
    std::strncpy(outIp, target.targetIp.c_str(), outIpMax);
    *outPort = target.targetPort;
    *outSessionId = target.sessionId;
    *outIsRecon = target.isReconnection ? 1 : 0;
    return 0;
}

inline int32_t RDCBReportSessionState(uint32_t sessionId, uint32_t state) {
    return RdConnectionBrokerManager::get().updateSessionState(
        sessionId,
        static_cast<RdcbSessionState>(state)
    ) ? 0 : -1;
}

inline int32_t RDCBSetServerDrain(const char* collection, const char* fqdn, uint8_t drain) {
    if (!collection || !fqdn) return -1;
    return RdConnectionBrokerManager::get().setServerDrainMode(
        collection,
        fqdn,
        drain ? DrainModeEnabled : DrainModeDisabled
    ) ? 0 : -1;
}

} // extern "C"

// ============================================================================
// 6. Registration & Subsystem Initialization
// ============================================================================

inline void InitializeRdcbExports() {
    auto& ldr = ldr::DynamicLoader::get();

    // 1. tssdis.dll exports
    ldr.registerExport("tssdis.dll", "RDCBOpenConnectionBroker", reinterpret_cast<void*>(RDCBOpenConnectionBroker));
    ldr.registerExport("tssdis.dll", "RDCBCloseConnectionBroker", reinterpret_cast<void*>(RDCBCloseConnectionBroker));
    ldr.registerExport("tssdis.dll", "RDCBResolveTarget", reinterpret_cast<void*>(RDCBResolveTarget));
    ldr.registerExport("tssdis.dll", "RDCBReportSessionState", reinterpret_cast<void*>(RDCBReportSessionState));
    ldr.registerExport("tssdis.dll", "RDCBSetServerDrain", reinterpret_cast<void*>(RDCBSetServerDrain));

    // 2. rdcbapi.dll exports
    ldr.registerExport("rdcbapi.dll", "RDCBOpenConnectionBroker", reinterpret_cast<void*>(RDCBOpenConnectionBroker));
    ldr.registerExport("rdcbapi.dll", "RDCBResolveTarget", reinterpret_cast<void*>(RDCBResolveTarget));

    // 3. SCM Service: Tssdis ("Remote Desktop Connection Broker")
    auto& scm = scm::ServiceControlManager::get();
    auto cbRecord = std::make_shared<scm::ServiceRecord>();
    cbRecord->serviceName = L"Tssdis";
    cbRecord->displayName = L"Remote Desktop Connection Broker";
    cbRecord->serviceType = scm::SERVICE_WIN32_OWN_PROCESS;
    cbRecord->startType = scm::SERVICE_AUTO_START;
    cbRecord->errorControl = scm::SERVICE_ERROR_NORMAL;
    cbRecord->binaryPath = L"C:\\Windows\\System32\\tssdis.exe";
    cbRecord->status.dwServiceType = cbRecord->serviceType;
    cbRecord->status.dwCurrentState = scm::SERVICE_RUNNING;
    cbRecord->status.dwProcessId = 1252;
    scm.registerServiceRecord(cbRecord);
}

inline void RegisterRdcbSubsystem() {
    // 1. SCM Services & Exports Initialization
    InitializeRdcbExports();

    // 2. Version Database PE Module Registration (6 modules)
    auto& vdb = micant::version::VersionDatabase::Instance();
    vdb.RegisterModule("tssdis.exe", "10.0.26100.1", "Remote Desktop Connection Broker Service Engine");
    vdb.RegisterModule("tssdis.dll", "10.0.26100.1", "Remote Desktop Connection Broker Core Library");
    vdb.RegisterModule("rdcbapi.dll", "10.0.26100.1", "Remote Desktop Connection Broker Management API");
    vdb.RegisterModule("cbadmin.exe", "10.0.26100.1", "Remote Desktop Connection Broker Administration Tool");
    vdb.RegisterModule("rdpbroker.dll", "10.0.26100.1", "Remote Desktop Session Directory Redirection Provider");
    vdb.RegisterModule("rdcb.sys", "10.0.26100.1", "Remote Desktop Session Routing Filter Driver");

    // 3. Subsystem Singleton Default State
    RdConnectionBrokerManager::get().reset();
}

} // namespace micant::rdcb
