// ============================================================================
// MicaNT: Windows Server Network Load Balancing (NLB / MS-NLB) Subsystem
// (include/micant/km/drivers/net/nlb.hpp)
//
// Sovereign Subsystem: TitanNLBCore / AegisLoadBalancer
//
// Strict Clean-Room Implementation based on:
//   - Microsoft Network Load Balancing Architecture & NDIS Filter Driver (nlb.sys / wlbs.sys)
//   - Network Load Balancing Protocol Specification ([MS-NLB])
//   - Unicast, Multicast & IGMP Multicast Cluster MAC Routing Models
//   - Client Affinity Models (None, Single IP, Class C /24 Network)
//   - Bi-directional Affinity (BDA) & Consistent Hashing Packet Filter Engine
//   - Heartbeat Protocol & Convergence State Machine (Stable, Converging, Draining, Stopped)
//   - Port Rule Handling: Multiple Host (Equal/Weighted), Single Host, Disabled
//   - Supreme Court of the United States: Google LLC v. Oracle America, Inc.
//     (141 S. Ct. 1183, 2021) - API interoperability doctrine
//
// Subsystem Overview:
//   TitanNLBCore provides high-availability and horizontal scalability for
//   stateless and session-aware TCP/IP applications (IIS Web, RDS Gateway, DNS, VPN).
//   Operating as an NDIS intermediate filter driver (nlb.sys) and control service (wlbs.dll),
//   NLB presents a single virtual IP (VIP) and virtual MAC to the external network,
//   distributing incoming traffic across up to 32 cluster member hosts using a fully
//   distributed consistent hashing algorithm that requires no central load balancer hardware.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <unordered_map>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>

#include "micant/shared/ntstatus.hpp"
#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/um/servers/scm.hpp"

namespace micant::nlb {

// ============================================================================
// 1. Constants and Enumerations ([MS-NLB], wlbs.h)
// ============================================================================
inline constexpr uint32_t NLB_MAGIC                        = 0x4E4C4253; // 'NLBS'
inline constexpr uint32_t NLB_VERSION_MAJOR                = 10;
inline constexpr uint32_t NLB_VERSION_MINOR                = 0;
inline constexpr uint32_t NLB_MAX_HOSTS                    = 32;
inline constexpr uint32_t NLB_MAX_PORT_RULES               = 64;
inline constexpr uint32_t NLB_DEFAULT_HEARTBEAT_INTERVAL_MS= 1000;
inline constexpr uint32_t NLB_DEFAULT_HEARTBEAT_TIMEOUT_MS = 5000; // 5 misses
inline constexpr uint32_t NLB_CONVERGENCE_PERIOD_MS        = 3000;

// Cluster Operation Modes
enum class NlbClusterMode : uint32_t {
    Unicast       = 0, // Clustered hosts share single virtual unicast MAC (02-BF-...)
    Multicast     = 1, // Multicast MAC (03-BF-...) with unicast IP resolution
    IgmpMulticast = 2  // IGMP join for Cisco/switch multicast snooping (01-00-5E-7F-...)
};

inline const char* NlbClusterModeToString(NlbClusterMode mode) noexcept {
    switch (mode) {
        case NlbClusterMode::Unicast:       return "Unicast";
        case NlbClusterMode::Multicast:     return "Multicast";
        case NlbClusterMode::IgmpMulticast: return "IGMP Multicast";
        default:                            return "Unknown";
    }
}

// Client Affinity Models
enum class NlbAffinity : uint32_t {
    None    = 0, // Multiple connections from same IP distributed to different hosts
    Single  = 1, // Requests from same client IP always sent to same cluster host
    Network = 2  // Requests from same Class C /24 subnet sent to same cluster host
};

inline const char* NlbAffinityToString(NlbAffinity aff) noexcept {
    switch (aff) {
        case NlbAffinity::None:    return "None";
        case NlbAffinity::Single:  return "Single IP";
        case NlbAffinity::Network: return "Network (Class C /24)";
        default:                   return "Unknown";
    }
}

// Port Rule Protocols
enum class NlbProtocol : uint32_t {
    Tcp  = 6,
    Udp  = 17,
    Both = 255
};

inline const char* NlbProtocolToString(NlbProtocol proto) noexcept {
    switch (proto) {
        case NlbProtocol::Tcp:  return "TCP";
        case NlbProtocol::Udp:  return "UDP";
        case NlbProtocol::Both: return "Both (TCP/UDP)";
        default:                return "Unknown";
    }
}

// Port Rule Filtering Modes
enum class NlbFilteringMode : uint32_t {
    MultipleHosts = 0, // Load distributed across cluster hosts by weight
    SingleHost    = 1, // Single host with handling priority processes all traffic
    Disabled      = 2  // Traffic matching this port rule is dropped/fenced
};

inline const char* NlbFilteringModeToString(NlbFilteringMode mode) noexcept {
    switch (mode) {
        case NlbFilteringMode::MultipleHosts: return "Multiple Hosts";
        case NlbFilteringMode::SingleHost:    return "Single Host";
        case NlbFilteringMode::Disabled:      return "Disabled";
        default:                              return "Unknown";
    }
}

// Host Operational States
enum class NlbHostState : uint32_t {
    Stopped    = 0, // Host not filtering or processing cluster traffic
    Converged  = 1, // Host is active and converged in cluster membership
    Converging = 2, // Cluster membership change in progress (negotiating)
    Draining   = 3, // Completing existing connections; rejecting new traffic
    Suspended  = 4  // Administratively suspended (maintenance)
};

inline const char* NlbHostStateToString(NlbHostState state) noexcept {
    switch (state) {
        case NlbHostState::Stopped:    return "Stopped";
        case NlbHostState::Converged:  return "Converged";
        case NlbHostState::Converging: return "Converging";
        case NlbHostState::Draining:   return "Draining";
        case NlbHostState::Suspended:  return "Suspended";
        default:                       return "Unknown";
    }
}

// Cluster Convergence Status
enum class NlbConvergenceState : uint32_t {
    Stable     = 0, // All online hosts agreed on cluster membership
    Converging = 1, // Membership negotiation active
    Degraded   = 2  // One or more hosts down, but surviving quorum operational
};

inline const char* NlbConvergenceStateToString(NlbConvergenceState state) noexcept {
    switch (state) {
        case NlbConvergenceState::Stable:     return "Converged (Stable)";
        case NlbConvergenceState::Converging: return "Converging";
        case NlbConvergenceState::Degraded:   return "Degraded (Sub-optimal)";
        default:                              return "Unknown";
    }
}

// Filter Action Decision
enum class NlbFilterAction : uint32_t {
    Accept = 0, // This host accepts and processes the incoming packet
    Drop   = 1  // Another host handles this packet; drop without processing
};

// ============================================================================
// 2. Data Structures
// ============================================================================
struct NlbPortRule {
    std::string ruleName;
    std::string virtualIp{"All"}; // "All" or specific VIP
    uint16_t startPort{0};
    uint16_t endPort{65535};
    NlbProtocol protocol{NlbProtocol::Both};
    NlbFilteringMode filteringMode{NlbFilteringMode::MultipleHosts};
    NlbAffinity affinity{NlbAffinity::Single};
    uint32_t loadWeight{50}; // 0-100 (equal = 50, or custom)
    uint32_t handlingPriority{1}; // Priority 1-32 for SingleHost mode
};

struct NlbHostInfo {
    uint32_t hostId{1}; // Priority / Host ID 1..32 (Unique)
    std::string hostName{"MICA-NLB-01"};
    std::string dedicatedIp{"192.168.1.11"};
    std::string dedicatedMask{"255.255.255.0"};
    NlbHostState state{NlbHostState::Converged};
    uint32_t activeConnections{0};
    uint32_t totalPacketsProcessed{0};
    uint32_t totalPacketsDropped{0};
    uint64_t lastHeartbeatUs{0};
    uint32_t missedHeartbeats{0};
    bool isDefaultHost{false};
};

struct NlbHeartbeatPacket {
    uint32_t magic{NLB_MAGIC};
    uint32_t senderHostId{1};
    uint32_t clusterEpoch{1};
    uint32_t hostState{static_cast<uint32_t>(NlbHostState::Converged)};
    uint32_t activeHostBitmap{0x00000001}; // Bits 1..32 represent online hosts
    uint64_t timestampUs{0};
};

// ============================================================================
// 3. NLB Cluster Definition
// ============================================================================
struct NlbClusterConfig {
    std::string clusterName{"MicaNlbCluster"};
    std::string clusterFqdn{"cluster.micant.internal"};
    std::string virtualIp{"192.168.1.100"};
    std::string virtualMask{"255.255.255.0"};
    std::string virtualMac{"02-BF-C0-A8-01-64"};
    NlbClusterMode mode{NlbClusterMode::Multicast};
    bool remoteControlEnabled{true};
    uint16_t remoteControlPort{2504};
};

// ============================================================================
// 4. Consistent Hashing Packet Distribution Engine
// ============================================================================
class NlbHashEngine {
public:
    // Pure clean-room 32-bit FNV-1a consistent hash for connection tuple
    static uint32_t hashTuple(uint32_t clientIp, uint16_t clientPort, uint16_t serverPort, NlbAffinity affinity) noexcept {
        uint32_t h = 0x811C9DC5; // FNV-1a 32-bit offset basis

        auto hashByte = [&](uint8_t b) noexcept {
            h ^= static_cast<uint32_t>(b);
            h *= 0x01000193; // FNV-1a 32-bit prime
        };

        // 1. Client IP Hash Component (masked according to affinity model)
        uint32_t effectiveIp = clientIp;
        if (affinity == NlbAffinity::Network) {
            effectiveIp &= 0xFFFFFF00; // Mask to Class C /24 subnet
        }

        hashByte(static_cast<uint8_t>(effectiveIp & 0xFF));
        hashByte(static_cast<uint8_t>((effectiveIp >> 8) & 0xFF));
        hashByte(static_cast<uint8_t>((effectiveIp >> 16) & 0xFF));
        hashByte(static_cast<uint8_t>((effectiveIp >> 24) & 0xFF));

        // 2. Client Port (Only included if affinity is None)
        if (affinity == NlbAffinity::None) {
            hashByte(static_cast<uint8_t>(clientPort & 0xFF));
            hashByte(static_cast<uint8_t>((clientPort >> 8) & 0xFF));
        }

        // 3. Server Port
        hashByte(static_cast<uint8_t>(serverPort & 0xFF));
        hashByte(static_cast<uint8_t>((serverPort >> 8) & 0xFF));

        return h;
    }

    // Maps hash value across sorted list of active host IDs based on assigned weights
    static uint32_t mapHashToHost(uint32_t hashVal, const std::vector<uint32_t>& activeHostIds) noexcept {
        if (activeHostIds.empty()) return 1;
        size_t idx = hashVal % activeHostIds.size();
        return activeHostIds[idx];
    }
};

// ============================================================================
// 5. NLB Subsystem Core Engine (TitanNLBCore)
// ============================================================================
class NetworkLoadBalancingSubsystem {
public:
    static NetworkLoadBalancingSubsystem& get() noexcept {
        static NetworkLoadBalancingSubsystem instance;
        return instance;
    }

    void initialize() {
        std::unique_lock lock(m_mutex);
        initializeLocked();
    }

    void reset() {
        std::unique_lock lock(m_mutex);
        m_hosts.clear();
        m_portRules.clear();
        m_initialized = false;
        m_heartbeatsSent = 0;
        m_totalPacketsFiltered = 0;
        m_convergenceEpoch = 1;
        m_convergenceState = NlbConvergenceState::Stable;
        initializeLocked();
    }

    bool isInitialized() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_initialized;
    }

    // --- Cluster Configuration ---
    NlbClusterConfig getClusterConfig() const {
        std::shared_lock lock(m_mutex);
        return m_cluster;
    }

    void setClusterMode(NlbClusterMode mode) {
        std::unique_lock lock(m_mutex);
        m_cluster.mode = mode;
        if (mode == NlbClusterMode::Unicast) {
            m_cluster.virtualMac = "02-BF-C0-A8-01-64";
        } else if (mode == NlbClusterMode::Multicast) {
            m_cluster.virtualMac = "03-BF-C0-A8-01-64";
        } else {
            m_cluster.virtualMac = "01-00-5E-7F-01-64";
        }
    }

    // --- Host Management ---
    bool addHost(const NlbHostInfo& host) {
        std::unique_lock lock(m_mutex);
        if (host.hostId < 1 || host.hostId > NLB_MAX_HOSTS) return false;
        if (m_hosts.find(host.hostId) != m_hosts.end()) return false;
        m_hosts[host.hostId] = host;
        triggerConvergenceLocked();
        return true;
    }

    bool removeHost(uint32_t hostId) {
        std::unique_lock lock(m_mutex);
        bool removed = (m_hosts.erase(hostId) > 0);
        if (removed) triggerConvergenceLocked();
        return removed;
    }

    NlbHostInfo* getHost(uint32_t hostId) {
        std::shared_lock lock(m_mutex);
        auto it = m_hosts.find(hostId);
        return (it != m_hosts.end()) ? &it->second : nullptr;
    }

    NlbHostInfo* findHostByNameOrId(std::string_view query) {
        std::shared_lock lock(m_mutex);
        try {
            size_t idx = 0;
            unsigned long id = std::stoul(std::string(query), &idx);
            if (idx == query.size()) {
                auto it = m_hosts.find(static_cast<uint32_t>(id));
                if (it != m_hosts.end()) return &it->second;
            }
        } catch (...) {}

        for (auto& [_, h] : m_hosts) {
            if (h.hostName == query || h.dedicatedIp == query) return &h;
        }
        return nullptr;
    }

    std::vector<NlbHostInfo> getAllHosts() const {
        std::shared_lock lock(m_mutex);
        std::vector<NlbHostInfo> list;
        list.reserve(m_hosts.size());
        for (const auto& [_, h] : m_hosts) list.push_back(h);
        std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.hostId < b.hostId; });
        return list;
    }

    size_t getHostCount() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_hosts.size();
    }

    // --- Host State Control (start, stop, drain, suspend, resume) ---
    bool startHost(uint32_t hostId) {
        std::unique_lock lock(m_mutex);
        auto it = m_hosts.find(hostId);
        if (it == m_hosts.end()) return false;
        it->second.state = NlbHostState::Converged;
        it->second.missedHeartbeats = 0;
        triggerConvergenceLocked();
        return true;
    }

    bool stopHost(uint32_t hostId) {
        std::unique_lock lock(m_mutex);
        auto it = m_hosts.find(hostId);
        if (it == m_hosts.end()) return false;
        it->second.state = NlbHostState::Stopped;
        triggerConvergenceLocked();
        return true;
    }

    bool drainHost(uint32_t hostId) {
        std::unique_lock lock(m_mutex);
        auto it = m_hosts.find(hostId);
        if (it == m_hosts.end()) return false;
        it->second.state = NlbHostState::Draining;
        triggerConvergenceLocked();
        return true;
    }

    bool suspendHost(uint32_t hostId) {
        std::unique_lock lock(m_mutex);
        auto it = m_hosts.find(hostId);
        if (it == m_hosts.end()) return false;
        it->second.state = NlbHostState::Suspended;
        triggerConvergenceLocked();
        return true;
    }

    bool resumeHost(uint32_t hostId) {
        std::unique_lock lock(m_mutex);
        auto it = m_hosts.find(hostId);
        if (it == m_hosts.end()) return false;
        it->second.state = NlbHostState::Converged;
        it->second.missedHeartbeats = 0;
        triggerConvergenceLocked();
        return true;
    }

    // --- Port Rules Management ---
    bool addPortRule(const NlbPortRule& rule) {
        std::unique_lock lock(m_mutex);
        if (m_portRules.size() >= NLB_MAX_PORT_RULES) return false;
        m_portRules.push_back(rule);
        return true;
    }

    bool setPortRuleEnabled(uint16_t port, bool enabled) {
        std::unique_lock lock(m_mutex);
        for (auto& r : m_portRules) {
            if (port >= r.startPort && port <= r.endPort) {
                r.filteringMode = enabled ? NlbFilteringMode::MultipleHosts : NlbFilteringMode::Disabled;
                return true;
            }
        }
        return false;
    }

    std::vector<NlbPortRule> getAllPortRules() const {
        std::shared_lock lock(m_mutex);
        return m_portRules;
    }

    // --- Packet Filtering & Consistent Hashing (nlb.sys NDIS Filter) ---
    NlbFilterAction filterIncomingPacket(uint32_t clientIp, uint16_t clientPort,
                                         uint16_t serverPort, NlbProtocol protocol,
                                         uint32_t evaluatingHostId, uint32_t* pAssignedHostId = nullptr) {
        std::shared_lock lock(m_mutex);
        m_totalPacketsFiltered.fetch_add(1);

        auto itHost = m_hosts.find(evaluatingHostId);
        if (itHost == m_hosts.end() || itHost->second.state == NlbHostState::Stopped ||
            itHost->second.state == NlbHostState::Suspended) {
            return NlbFilterAction::Drop;
        }

        // 1. Locate matching port rule
        const NlbPortRule* matchedRule = nullptr;
        for (const auto& r : m_portRules) {
            if (serverPort >= r.startPort && serverPort <= r.endPort) {
                if (r.protocol == NlbProtocol::Both || r.protocol == protocol) {
                    matchedRule = &r;
                    break;
                }
            }
        }

        // Default handling if no explicit port rule (handled by default host)
        if (!matchedRule) {
            uint32_t defHost = getDefaultHostIdLocked();
            if (pAssignedHostId) *pAssignedHostId = defHost;
            return (evaluatingHostId == defHost) ? NlbFilterAction::Accept : NlbFilterAction::Drop;
        }

        // 2. Evaluate Rule Filtering Mode
        if (matchedRule->filteringMode == NlbFilteringMode::Disabled) {
            return NlbFilterAction::Drop;
        }

        if (matchedRule->filteringMode == NlbFilteringMode::SingleHost) {
            uint32_t designated = matchedRule->handlingPriority;
            if (pAssignedHostId) *pAssignedHostId = designated;
            return (evaluatingHostId == designated) ? NlbFilterAction::Accept : NlbFilterAction::Drop;
        }

        // MultipleHosts mode: Get list of active converged hosts
        std::vector<uint32_t> activeHosts;
        for (const auto& [id, h] : m_hosts) {
            // Draining hosts do NOT take new incoming connections
            if (h.state == NlbHostState::Converged) {
                activeHosts.push_back(id);
            }
        }

        if (activeHosts.empty()) {
            return NlbFilterAction::Drop;
        }

        std::sort(activeHosts.begin(), activeHosts.end());
        uint32_t hashVal = NlbHashEngine::hashTuple(clientIp, clientPort, serverPort, matchedRule->affinity);
        uint32_t assignedHost = NlbHashEngine::mapHashToHost(hashVal, activeHosts);

        if (pAssignedHostId) *pAssignedHostId = assignedHost;

        return (evaluatingHostId == assignedHost) ? NlbFilterAction::Accept : NlbFilterAction::Drop;
    }

    // --- Heartbeats & Convergence ---
    bool sendHeartbeat(uint32_t senderHostId) {
        std::unique_lock lock(m_mutex);
        auto it = m_hosts.find(senderHostId);
        if (it == m_hosts.end() || it->second.state == NlbHostState::Stopped) return false;

        it->second.lastHeartbeatUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        it->second.missedHeartbeats = 0;
        m_heartbeatsSent.fetch_add(1);
        return true;
    }

    void injectHeartbeatTimeout(uint32_t hostId) {
        std::unique_lock lock(m_mutex);
        auto it = m_hosts.find(hostId);
        if (it != m_hosts.end()) {
            it->second.missedHeartbeats = 6; // > 5 threshold
            it->second.state = NlbHostState::Stopped;
            triggerConvergenceLocked();
        }
    }

    NlbConvergenceState getConvergenceState() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_convergenceState;
    }

    uint32_t getConvergenceEpoch() const noexcept {
        std::shared_lock lock(m_mutex);
        return m_convergenceEpoch;
    }

    uint64_t getHeartbeatCount() const noexcept {
        return m_heartbeatsSent.load();
    }

    uint64_t getPacketsFilteredCount() const noexcept {
        return m_totalPacketsFiltered.load();
    }

    uint32_t getTotalActiveConnections() const {
        std::shared_lock lock(m_mutex);
        uint32_t total = 0;
        for (const auto& [_, h] : m_hosts) {
            total += h.activeConnections;
        }
        return total;
    }

    void recordConnection(uint32_t hostId, int32_t delta) {
        std::unique_lock lock(m_mutex);
        auto it = m_hosts.find(hostId);
        if (it != m_hosts.end()) {
            if (delta < 0 && static_cast<uint32_t>(-delta) > it->second.activeConnections) {
                it->second.activeConnections = 0;
            } else {
                it->second.activeConnections += delta;
            }

            // If draining and connections reach 0, complete drain to Stopped
            if (it->second.state == NlbHostState::Draining && it->second.activeConnections == 0) {
                it->second.state = NlbHostState::Stopped;
                triggerConvergenceLocked();
            }
        }
    }

private:
    NetworkLoadBalancingSubsystem() = default;

    void initializeLocked() {
        if (m_initialized) return;

        m_cluster.clusterName = "TitanNlbCluster";
        m_cluster.clusterFqdn = "nlb.micant.internal";
        m_cluster.virtualIp = "192.168.1.100";
        m_cluster.virtualMask = "255.255.255.0";
        m_cluster.virtualMac = "03-BF-C0-A8-01-64";
        m_cluster.mode = NlbClusterMode::Multicast;
        m_cluster.remoteControlEnabled = true;
        m_cluster.remoteControlPort = 2504;

        // Pre-seed 3 Enterprise NLB Member Hosts
        NlbHostInfo h1{};
        h1.hostId = 1;
        h1.hostName = "TITAN-NLB-01";
        h1.dedicatedIp = "192.168.1.11";
        h1.dedicatedMask = "255.255.255.0";
        h1.state = NlbHostState::Converged;
        h1.activeConnections = 120;
        h1.isDefaultHost = true;
        m_hosts[1] = h1;

        NlbHostInfo h2{};
        h2.hostId = 2;
        h2.hostName = "TITAN-NLB-02";
        h2.dedicatedIp = "192.168.1.12";
        h2.dedicatedMask = "255.255.255.0";
        h2.state = NlbHostState::Converged;
        h2.activeConnections = 115;
        h2.isDefaultHost = false;
        m_hosts[2] = h2;

        NlbHostInfo h3{};
        h3.hostId = 3;
        h3.hostName = "TITAN-NLB-03";
        h3.dedicatedIp = "192.168.1.13";
        h3.dedicatedMask = "255.255.255.0";
        h3.state = NlbHostState::Converged;
        h3.activeConnections = 118;
        h3.isDefaultHost = false;
        m_hosts[3] = h3;

        // Pre-seed Port Rules
        // Rule 1: HTTP Port 80 (Multiple Hosts, Single IP Affinity)
        NlbPortRule rHttp{};
        rHttp.ruleName = "HTTP-WebServices";
        rHttp.virtualIp = "All";
        rHttp.startPort = 80;
        rHttp.endPort = 80;
        rHttp.protocol = NlbProtocol::Tcp;
        rHttp.filteringMode = NlbFilteringMode::MultipleHosts;
        rHttp.affinity = NlbAffinity::Single;
        rHttp.loadWeight = 50;
        m_portRules.push_back(rHttp);

        // Rule 2: HTTPS Port 443 (Multiple Hosts, Single IP Affinity)
        NlbPortRule rHttps{};
        rHttps.ruleName = "HTTPS-SecureWeb";
        rHttps.virtualIp = "All";
        rHttps.startPort = 443;
        rHttps.endPort = 443;
        rHttps.protocol = NlbProtocol::Tcp;
        rHttps.filteringMode = NlbFilteringMode::MultipleHosts;
        rHttps.affinity = NlbAffinity::Single;
        rHttps.loadWeight = 50;
        m_portRules.push_back(rHttps);

        // Rule 3: DNS Port 53 (Multiple Hosts, None Affinity for maximal UDP throughput)
        NlbPortRule rDns{};
        rDns.ruleName = "DNS-QueryPool";
        rDns.virtualIp = "All";
        rDns.startPort = 53;
        rDns.endPort = 53;
        rDns.protocol = NlbProtocol::Both;
        rDns.filteringMode = NlbFilteringMode::MultipleHosts;
        rDns.affinity = NlbAffinity::None;
        rDns.loadWeight = 50;
        m_portRules.push_back(rDns);

        // Rule 4: Administrative Management Port 3389 (Single Host Priority 1)
        NlbPortRule rRdp{};
        rRdp.ruleName = "RDP-AdminConsole";
        rRdp.virtualIp = "All";
        rRdp.startPort = 3389;
        rRdp.endPort = 3389;
        rRdp.protocol = NlbProtocol::Tcp;
        rRdp.filteringMode = NlbFilteringMode::SingleHost;
        rRdp.handlingPriority = 1;
        m_portRules.push_back(rRdp);

        m_convergenceState = NlbConvergenceState::Stable;
        m_convergenceEpoch = 1;
        m_initialized = true;
    }

    void triggerConvergenceLocked() {
        m_convergenceEpoch++;
        m_convergenceState = NlbConvergenceState::Converging;

        uint32_t activeHosts = 0;
        for (const auto& [_, h] : m_hosts) {
            if (h.state == NlbHostState::Converged) activeHosts++;
        }

        if (activeHosts == 0) {
            m_convergenceState = NlbConvergenceState::Degraded;
        } else if (activeHosts < m_hosts.size()) {
            m_convergenceState = NlbConvergenceState::Degraded;
        } else {
            m_convergenceState = NlbConvergenceState::Stable;
        }
    }

    uint32_t getDefaultHostIdLocked() const noexcept {
        for (const auto& [id, h] : m_hosts) {
            if (h.isDefaultHost && h.state == NlbHostState::Converged) return id;
        }
        for (const auto& [id, h] : m_hosts) {
            if (h.state == NlbHostState::Converged) return id;
        }
        return 1;
    }

    mutable std::shared_mutex m_mutex;
    bool m_initialized{false};
    NlbClusterConfig m_cluster;
    std::unordered_map<uint32_t, NlbHostInfo> m_hosts;
    std::vector<NlbPortRule> m_portRules;

    NlbConvergenceState m_convergenceState{NlbConvergenceState::Stable};
    uint32_t m_convergenceEpoch{1};
    std::atomic<uint64_t> m_heartbeatsSent{0};
    std::atomic<uint64_t> m_totalPacketsFiltered{0};
};

// ============================================================================
// 6. SCM Driver & VersionDatabase Registration Helper
// ============================================================================
inline void RegisterNlbSubsystem() {
    // 1. Version Database Registration
    auto& vdb = micant::version::VersionDatabase::Instance();
    vdb.RegisterModule("nlb.exe",     "10.0.26100.1", "Network Load Balancing Command-Line Tool");
    vdb.RegisterModule("nlbmgr.exe",  "10.0.26100.1", "Network Load Balancing Manager Host");
    vdb.RegisterModule("nlb.sys",     "10.0.26100.1", "Network Load Balancing NDIS Intermediate Driver");
    vdb.RegisterModule("wlbs.dll",    "10.0.26100.1", "Windows Load Balancing Service API Library");
    vdb.RegisterModule("wlbsctrl.dll","10.0.26100.1", "Network Load Balancing Control Provider");

    // 2. SCM Registration
    auto& scm = micant::scm::ServiceControlManager::get();

    // Driver: NLB (nlb.sys)
    auto drvRec = std::make_shared<micant::scm::ServiceRecord>();
    drvRec->serviceName = L"NLB";
    drvRec->displayName = L"Network Load Balancing Driver";
    drvRec->serviceType = micant::scm::SERVICE_KERNEL_DRIVER;
    drvRec->startType = micant::scm::SERVICE_SYSTEM_START;
    drvRec->binaryPath = L"C:\\Windows\\System32\\drivers\\nlb.sys";
    drvRec->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
    scm.registerServiceRecord(drvRec);

    // User-mode Service: NlbSvc
    auto svcRec = std::make_shared<micant::scm::ServiceRecord>();
    svcRec->serviceName = L"NlbSvc";
    svcRec->displayName = L"Network Load Balancing Service";
    svcRec->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
    svcRec->startType = micant::scm::SERVICE_AUTO_START;
    svcRec->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k netsvcs";
    svcRec->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
    scm.registerServiceRecord(svcRec);

    // Initialize core subsystem singleton
    NetworkLoadBalancingSubsystem::get().initialize();
}

} // namespace micant::nlb
