#ifndef MICANT_DHCP_SERVER_HPP
#define MICANT_DHCP_SERVER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <functional>

#include "micant/scm.hpp"
#include "micant/version.hpp"
#include "micant/dns_server.hpp"

namespace micant::dhcp {

// ============================================================================
// DHCP Protocol Constants (RFC 2131, RFC 2132, RFC 8415, RFC 3074)
// ============================================================================

constexpr uint16_t DHCP_SERVER_PORT   = 67;
constexpr uint16_t DHCP_CLIENT_PORT   = 68;
constexpr uint16_t DHCPV6_SERVER_PORT = 547;
constexpr uint16_t DHCPV6_CLIENT_PORT = 546;

// DHCPv4 Message Types (Option 53 / RFC 2132 Section 9.6)
constexpr uint8_t DHCPDISCOVER = 1;
constexpr uint8_t DHCPOFFER    = 2;
constexpr uint8_t DHCPREQUEST  = 3;
constexpr uint8_t DHCPDECLINE  = 4;
constexpr uint8_t DHCPACK      = 5;
constexpr uint8_t DHCPNAK      = 6;
constexpr uint8_t DHCPRELEASE  = 7;
constexpr uint8_t DHCPINFORM   = 8;

// DHCPv6 Message Types (RFC 8415 Section 7.3)
constexpr uint8_t DHCPV6_SOLICIT      = 1;
constexpr uint8_t DHCPV6_ADVERTISE    = 2;
constexpr uint8_t DHCPV6_REQUEST      = 3;
constexpr uint8_t DHCPV6_CONFIRM      = 4;
constexpr uint8_t DHCPV6_RENEW        = 5;
constexpr uint8_t DHCPV6_REBIND       = 6;
constexpr uint8_t DHCPV6_REPLY        = 7;
constexpr uint8_t DHCPV6_RELEASE      = 8;
constexpr uint8_t DHCPV6_DECLINE      = 9;
constexpr uint8_t DHCPV6_RECONFIGURE  = 10;
constexpr uint8_t DHCPV6_INFO_REQUEST = 11;

// Standard DHCP Options
constexpr uint8_t OPTION_PAD                    = 0;
constexpr uint8_t OPTION_SUBNET_MASK            = 1;
constexpr uint8_t OPTION_ROUTER                 = 3;
constexpr uint8_t OPTION_DNS_SERVERS            = 6;
constexpr uint8_t OPTION_HOSTNAME               = 12;
constexpr uint8_t OPTION_DOMAIN_NAME            = 15;
constexpr uint8_t OPTION_BROADCAST_ADDRESS      = 28;
constexpr uint8_t OPTION_WINS_SERVERS           = 44;
constexpr uint8_t OPTION_NETBIOS_NODE_TYPE      = 46;
constexpr uint8_t OPTION_LEASE_TIME             = 51;
constexpr uint8_t OPTION_MESSAGE_TYPE           = 53;
constexpr uint8_t OPTION_SERVER_IDENTIFIER      = 54;
constexpr uint8_t OPTION_PARAMETER_REQUEST_LIST = 55;
constexpr uint8_t OPTION_RENEWAL_TIME_T1        = 58;
constexpr uint8_t OPTION_REBINDING_TIME_T2      = 59;
constexpr uint8_t OPTION_CLIENT_FQDN            = 81;
constexpr uint8_t OPTION_END                    = 255;

enum class ClientType {
    Dhcp,
    Bootp,
    Both
};

enum class LeaseState {
    Active,
    Offered,
    Expired,
    Released,
    Quarantined
};

enum class FailoverMode {
    LoadBalance,
    HotStandby
};

enum class FailoverState {
    Normal,
    PartnerDown,
    CommunicationsInterrupted
};

inline std::string ClientTypeToString(ClientType ct) {
    switch (ct) {
        case ClientType::Dhcp:  return "DHCP";
        case ClientType::Bootp: return "BOOTP";
        case ClientType::Both:  return "Both";
        default: return "Unknown";
    }
}

inline std::string LeaseStateToString(LeaseState ls) {
    switch (ls) {
        case LeaseState::Active:      return "Active";
        case LeaseState::Offered:     return "Offered";
        case LeaseState::Expired:     return "Expired";
        case LeaseState::Released:    return "Released";
        case LeaseState::Quarantined: return "Quarantined";
        default: return "Unknown";
    }
}

inline std::string FailoverModeToString(FailoverMode fm) {
    switch (fm) {
        case FailoverMode::LoadBalance: return "LoadBalance";
        case FailoverMode::HotStandby:  return "HotStandby";
        default: return "Unknown";
    }
}

inline std::string FailoverStateToString(FailoverState fs) {
    switch (fs) {
        case FailoverState::Normal:                    return "Normal";
        case FailoverState::PartnerDown:               return "PartnerDown";
        case FailoverState::CommunicationsInterrupted: return "CommInterrupted";
        default: return "Unknown";
    }
}

// ============================================================================
// IP Helper Functions
// ============================================================================

inline uint32_t ipToUint(const std::string& ip) {
    uint32_t b1 = 0, b2 = 0, b3 = 0, b4 = 0;
    char dot = '.';
    std::istringstream iss(ip);
    iss >> b1 >> dot >> b2 >> dot >> b3 >> dot >> b4;
    return (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
}

inline std::string uintToIp(uint32_t n) {
    return std::to_string((n >> 24) & 0xFF) + "." +
           std::to_string((n >> 16) & 0xFF) + "." +
           std::to_string((n >> 8) & 0xFF) + "." +
           std::to_string(n & 0xFF);
}

inline bool isIpInRange(const std::string& ip, const std::string& start, const std::string& end) {
    uint32_t uIp = ipToUint(ip);
    uint32_t uStart = ipToUint(start);
    uint32_t uEnd = ipToUint(end);
    return uIp >= uStart && uIp <= uEnd;
}

inline std::string normalizeMac(std::string mac) {
    std::string clean;
    for (char c : mac) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
            clean += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    if (clean.size() == 12) {
        std::string res;
        for (size_t i = 0; i < 12; i += 2) {
            if (i > 0) res += ":";
            res += clean.substr(i, 2);
        }
        return res;
    }
    std::transform(mac.begin(), mac.end(), mac.begin(), ::tolower);
    return mac;
}

// ============================================================================
// Core Data Structures
// ============================================================================

struct DhcpExclusionRange {
    std::string startIp;
    std::string endIp;
};

struct DhcpReservation {
    std::string ipAddress;
    std::string macAddress;
    std::string clientName;
    ClientType  clientType{ClientType::Both};
};

struct DhcpLease {
    std::string ipAddress;
    std::string macAddress;
    std::string hostName;
    uint64_t    leaseStart{0};
    uint64_t    leaseEnd{0};
    LeaseState  state{LeaseState::Active};
    ClientType  clientType{ClientType::Dhcp};
    bool        dnsRegistered{false};
};

struct DhcpFailoverRelationship {
    std::string   name;
    std::string   primaryServer{"dc01.titan.local"};
    std::string   secondaryServer{"dc02.titan.local"};
    FailoverMode  mode{FailoverMode::LoadBalance};
    FailoverState state{FailoverState::Normal};
    uint32_t      loadBalancePercent{50}; // 50/50 split
    uint32_t      mcltSeconds{3600};       // Maximum Client Lead Time
    std::string   sharedSecret{"TitanDhcpSharedSecret2026"};
    bool          isPrimary{true};
};

struct DhcpScope {
    std::string scopeId;    // Network ID, e.g. "192.168.1.0"
    std::string scopeName;
    std::string subnetMask{"255.255.255.0"};
    std::string startIp{"192.168.1.100"};
    std::string endIp{"192.168.1.200"};
    uint32_t    leaseDurationSeconds{691200}; // 8 days
    bool        isActive{true};

    // Options
    std::string router{"192.168.1.1"};
    std::vector<std::string> dnsServers{"192.168.1.10", "192.168.1.11"};
    std::string domainName{"titan.local"};

    // Databases
    std::vector<DhcpExclusionRange>      exclusions;
    std::map<std::string, DhcpReservation> reservations; // Keyed by normalized MAC
    std::map<std::string, DhcpLease>       leases;       // Keyed by IP address

    // Failover
    DhcpFailoverRelationship failover;
    bool                     hasFailover{false};

    // Dynamic DNS
    bool dynamicDnsEnabled{true};
    bool discardLeaseOnRelease{true};
};

// DHCPv6
struct Dhcpv6Lease {
    std::string ipv6Address;
    std::string duid;
    uint32_t    iaid{1};
    uint64_t    validLifetime{86400};
    uint64_t    preferredLifetime{43200};
    uint64_t    leaseStart{0};
    std::string hostName;
    LeaseState  state{LeaseState::Active};
};

struct Dhcpv6Scope {
    std::string prefix{"2001:db8:1::/64"};
    std::string startIp{"2001:db8:1::100"};
    std::string endIp{"2001:db8:1::200"};
    uint32_t    preference{255};
    std::vector<std::string> dnsServers{"2001:db8::10"};
    std::string domainSearchList{"titan.local"};
    std::map<std::string, Dhcpv6Lease> leases; // Keyed by IPv6
};

// ============================================================================
// EnterpriseDhcpServer (Singleton Engine)
// ============================================================================

class EnterpriseDhcpServer {
private:
    mutable std::recursive_mutex m_mutex;
    bool m_initialized{false};

    // Server Info
    std::string m_serverHost{"dc01.titan.local"};
    std::string m_serverIp{"192.168.1.10"};
    std::string m_domainName{"titan.local"};

    // Active Directory Authorization & Rogue Detection
    bool        m_isAuthorizedInAd{true};
    std::string m_adDirectoryPath{"CN=NetServices,CN=Services,CN=Configuration,DC=titan,DC=local"};
    bool        m_rogueDetectionSuppressed{false};

    // Scopes
    std::map<std::string, DhcpScope>   m_scopes;   // Keyed by scopeId (subnet)
    std::map<std::string, Dhcpv6Scope> m_v6Scopes; // Keyed by prefix

    // Telemetry Counters
    mutable std::atomic<uint64_t> m_totalDiscoversReceived{0};
    mutable std::atomic<uint64_t> m_totalOffersSent{0};
    mutable std::atomic<uint64_t> m_totalRequestsReceived{0};
    mutable std::atomic<uint64_t> m_totalAcksSent{0};
    mutable std::atomic<uint64_t> m_totalNaksSent{0};
    mutable std::atomic<uint64_t> m_totalReleasesReceived{0};
    mutable std::atomic<uint64_t> m_totalDeclinesReceived{0};
    mutable std::atomic<uint64_t> m_totalDhcpv6SolicitsReceived{0};
    mutable std::atomic<uint64_t> m_totalDhcpv6RepliesSent{0};
    mutable std::atomic<uint64_t> m_totalDnsUpdatesAttempted{0};
    mutable std::atomic<uint64_t> m_totalDnsUpdatesSucceeded{0};
    mutable std::atomic<uint64_t> m_totalFailoverSyncs{0};

    static std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }

public:
    static EnterpriseDhcpServer& instance() {
        static EnterpriseDhcpServer inst;
        return inst;
    }

    EnterpriseDhcpServer() {
        initialize();
    }

    void initialize() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_initialized) return;

        seedDefaultScopes();
        registerScmServices();
        registerVersionDatabase();

        m_initialized = true;
    }

    void registerScmServices() {
        auto& scm = micant::scm::ServiceControlManager::get();

        auto recDhcp = std::make_shared<micant::scm::ServiceRecord>();
        recDhcp->serviceName = L"DHCPServer";
        recDhcp->displayName = L"DHCP Server";
        recDhcp->serviceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
        recDhcp->startType = micant::scm::SERVICE_AUTO_START;
        recDhcp->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recDhcp->binaryPath = L"C:\\Windows\\System32\\tcpsvcs.exe";
        recDhcp->status.dwServiceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
        recDhcp->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        recDhcp->status.dwControlsAccepted = micant::scm::SERVICE_ACCEPT_STOP | micant::scm::SERVICE_ACCEPT_SHUTDOWN | micant::scm::SERVICE_ACCEPT_PAUSE_CONTINUE;
        scm.registerServiceRecord(recDhcp);
    }

    void registerVersionDatabase() {
        auto& db = micant::version::VersionDatabase::Instance();
        db.RegisterModule("tcpsvcs.exe", "10.0.26100.1", "Microsoft TCP/IP Services Application");
        db.RegisterModule("dhcpsvc.dll", "10.0.26100.1", "DHCP Server Service");
        db.RegisterModule("dhcpsapi.dll", "10.0.26100.1", "DHCP Server Management API");
        db.RegisterModule("dhcpcore.dll", "10.0.26100.1", "DHCP Core Client and Server Library");
        db.RegisterModule("dhcpcmonitor.dll", "10.0.26100.1", "DHCP Client and Server Diagnostic Monitor");
    }

    void seedDefaultScopes() {
        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        // 1. Enterprise IPv4 Scope: 192.168.1.0/24
        DhcpScope sc;
        sc.scopeId = "192.168.1.0";
        sc.scopeName = "Titan-Enterprise-Workstations";
        sc.subnetMask = "255.255.255.0";
        sc.startIp = "192.168.1.100";
        sc.endIp = "192.168.1.200";
        sc.leaseDurationSeconds = 691200; // 8 days
        sc.isActive = true;
        sc.router = "192.168.1.1";
        sc.dnsServers = { "192.168.1.10", "192.168.1.11" };
        sc.domainName = "titan.local";

        // Exclusions: 192.168.1.150 - 192.168.1.160
        sc.exclusions.push_back({ "192.168.1.150", "192.168.1.160" });

        // Reservations:
        // Reserved Printer: 192.168.1.120 -> 00:15:5d:01:aa:01
        sc.reservations["00:15:5d:01:aa:01"] = { "192.168.1.120", "00:15:5d:01:aa:01", "titan-printer01", ClientType::Both };
        // Reserved IP Phone: 192.168.1.121 -> 00:15:5d:01:aa:02
        sc.reservations["00:15:5d:01:aa:02"] = { "192.168.1.121", "00:15:5d:01:aa:02", "titan-voip01", ClientType::Dhcp };

        // Active Seed Leases:
        DhcpLease l1;
        l1.ipAddress = "192.168.1.101";
        l1.macAddress = "00:15:5d:00:00:01";
        l1.hostName = "ws-sec01";
        l1.leaseStart = now - 3600;
        l1.leaseEnd = l1.leaseStart + sc.leaseDurationSeconds;
        l1.state = LeaseState::Active;
        l1.dnsRegistered = true;
        sc.leases[l1.ipAddress] = l1;

        // Failover Configuration
        sc.hasFailover = true;
        sc.failover.name = "Titan-Failover-DC01-DC02";
        sc.failover.primaryServer = "dc01.titan.local";
        sc.failover.secondaryServer = "dc02.titan.local";
        sc.failover.mode = FailoverMode::LoadBalance;
        sc.failover.state = FailoverState::Normal;
        sc.failover.loadBalancePercent = 50;
        sc.failover.mcltSeconds = 3600;

        m_scopes[sc.scopeId] = sc;

        // 2. Enterprise DHCPv6 Scope: 2001:db8:1::/64
        Dhcpv6Scope sc6;
        sc6.prefix = "2001:db8:1::/64";
        sc6.startIp = "2001:db8:1::100";
        sc6.endIp = "2001:db8:1::200";
        sc6.preference = 255;
        sc6.dnsServers = { "2001:db8::10" };
        sc6.domainSearchList = "titan.local";

        Dhcpv6Lease lv6;
        lv6.ipv6Address = "2001:db8:1::101";
        lv6.duid = "000100012233445500155d000001";
        lv6.iaid = 1;
        lv6.leaseStart = now - 3600;
        lv6.validLifetime = 86400;
        lv6.preferredLifetime = 43200;
        lv6.hostName = "ws-sec01.titan.local";
        lv6.state = LeaseState::Active;
        sc6.leases[lv6.ipv6Address] = lv6;

        m_v6Scopes[sc6.prefix] = sc6;
    }

    // ========================================================================
    // Active Directory Authorization & Rogue DHCP Server Suppression
    // ========================================================================
    bool authorizeServerInAd(const std::string& serverIp, const std::string& domain) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (serverIp == m_serverIp && domain == m_domainName) {
            m_isAuthorizedInAd = true;
            m_rogueDetectionSuppressed = false;
            return true;
        }
        return false;
    }

    bool unauthorizeServerInAd(const std::string& serverIp, const std::string& domain) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (serverIp == m_serverIp && domain == m_domainName) {
            m_isAuthorizedInAd = false;
            m_rogueDetectionSuppressed = true; // Rogue detection automatically halts servicing
            return true;
        }
        return false;
    }

    bool isServerAuthorized() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_isAuthorizedInAd && !m_rogueDetectionSuppressed;
    }

    bool isRogueSuppressed() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_rogueDetectionSuppressed;
    }

    std::string getAdDirectoryPath() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_adDirectoryPath;
    }

    // ========================================================================
    // Scope Management
    // ========================================================================
    bool createScope(const std::string& subnet, const std::string& mask, const std::string& startIp, const std::string& endIp, uint32_t leaseSec, const std::string& name = "") {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_scopes.find(subnet) != m_scopes.end()) return false;

        uint32_t uStart = ipToUint(startIp);
        uint32_t uEnd = ipToUint(endIp);
        if (uStart >= uEnd) return false;

        DhcpScope sc;
        sc.scopeId = subnet;
        sc.scopeName = name.empty() ? ("Scope-" + subnet) : name;
        sc.subnetMask = mask;
        sc.startIp = startIp;
        sc.endIp = endIp;
        sc.leaseDurationSeconds = (leaseSec > 0) ? leaseSec : 691200;
        sc.isActive = true;
        sc.router = uintToIp(ipToUint(subnet) + 1);
        sc.dnsServers = { m_serverIp };
        sc.domainName = m_domainName;

        m_scopes[subnet] = sc;
        return true;
    }

    bool deleteScope(const std::string& subnet) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_scopes.erase(subnet) > 0;
    }

    bool getScope(const std::string& subnet, DhcpScope& outScope) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_scopes.find(subnet);
        if (it == m_scopes.end()) return false;
        outScope = it->second;
        return true;
    }

    std::vector<DhcpScope> getAllScopes() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<DhcpScope> res;
        res.reserve(m_scopes.size());
        for (const auto& [_, sc] : m_scopes) {
            res.push_back(sc);
        }
        return res;
    }

    bool setScopeOptions(const std::string& subnet, const std::string& router, const std::vector<std::string>& dnsServers, const std::string& domain) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_scopes.find(subnet);
        if (it == m_scopes.end()) return false;
        if (!router.empty()) it->second.router = router;
        if (!dnsServers.empty()) it->second.dnsServers = dnsServers;
        if (!domain.empty()) it->second.domainName = domain;
        return true;
    }

    bool addExclusionRange(const std::string& subnet, const std::string& startIp, const std::string& endIp) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_scopes.find(subnet);
        if (it == m_scopes.end()) return false;

        uint32_t uStart = ipToUint(startIp);
        uint32_t uEnd = ipToUint(endIp);
        if (uStart > uEnd) return false;

        it->second.exclusions.push_back({ startIp, endIp });
        return true;
    }

    bool addReservation(const std::string& subnet, const std::string& ip, const std::string& mac, const std::string& name, ClientType type = ClientType::Both) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_scopes.find(subnet);
        if (it == m_scopes.end()) return false;

        std::string nMac = normalizeMac(mac);
        it->second.reservations[nMac] = { ip, nMac, name, type };
        return true;
    }

    bool deleteReservation(const std::string& subnet, const std::string& ip, const std::string& mac) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_scopes.find(subnet);
        if (it == m_scopes.end()) return false;

        std::string nMac = normalizeMac(mac);
        auto rIt = it->second.reservations.find(nMac);
        if (rIt != it->second.reservations.end() && (ip.empty() || rIt->second.ipAddress == ip)) {
            it->second.reservations.erase(rIt);
            return true;
        }
        return false;
    }

    // ========================================================================
    // DHCPv4 4-Way DORA Handshake (RFC 2131)
    // ========================================================================
    bool processDiscover(const std::string& subnetHint, const std::string& mac, const std::string& hostName, std::string& outOfferedIp, uint32_t& outLeaseSec) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_totalDiscoversReceived.fetch_add(1, std::memory_order_relaxed);

        if (m_rogueDetectionSuppressed || !m_isAuthorizedInAd) {
            return false; // Rogue DHCP Server suppression
        }

        std::string nMac = normalizeMac(mac);

        // Find target scope (by subnetHint or first active scope)
        DhcpScope* pScope = nullptr;
        if (!subnetHint.empty()) {
            auto it = m_scopes.find(subnetHint);
            if (it != m_scopes.end() && it->second.isActive) pScope = &it->second;
        }
        if (!pScope) {
            for (auto& [_, sc] : m_scopes) {
                if (sc.isActive) { pScope = &sc; break; }
            }
        }
        if (!pScope) return false;

        outLeaseSec = pScope->leaseDurationSeconds;

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        // 1. Check if MAC has a permanent Reservation
        auto resIt = pScope->reservations.find(nMac);
        if (resIt != pScope->reservations.end()) {
            outOfferedIp = resIt->second.ipAddress;
            m_totalOffersSent.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        // 2. Check if MAC already holds an active or offered lease
        for (const auto& [ip, lease] : pScope->leases) {
            if (lease.macAddress == nMac && (lease.state == LeaseState::Active || (lease.state == LeaseState::Offered && now <= lease.leaseEnd))) {
                outOfferedIp = ip;
                m_totalOffersSent.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }

        // 3. Scan available IP pool
        uint32_t uStart = ipToUint(pScope->startIp);
        uint32_t uEnd = ipToUint(pScope->endIp);

        for (uint32_t uIp = uStart; uIp <= uEnd; ++uIp) {
            std::string candidateIp = uintToIp(uIp);

            // Check exclusion ranges
            bool excluded = false;
            for (const auto& ex : pScope->exclusions) {
                if (isIpInRange(candidateIp, ex.startIp, ex.endIp)) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) continue;

            // Check if reserved for a different MAC
            bool reserved = false;
            for (const auto& [_, res] : pScope->reservations) {
                if (res.ipAddress == candidateIp && res.macAddress != nMac) {
                    reserved = true;
                    break;
                }
            }
            if (reserved) continue;

            // Check if active or offered lease exists
            auto lIt = pScope->leases.find(candidateIp);
            bool isAvailable = (lIt == pScope->leases.end()) ||
                               (lIt->second.state == LeaseState::Expired) ||
                               (lIt->second.state == LeaseState::Released) ||
                               (lIt->second.state == LeaseState::Offered && (now > lIt->second.leaseEnd || lIt->second.macAddress == nMac));

            if (isAvailable) {
                outOfferedIp = candidateIp;
                DhcpLease offer;
                offer.ipAddress = candidateIp;
                offer.macAddress = nMac;
                offer.hostName = hostName;
                offer.leaseStart = now;
                offer.leaseEnd = now + 120; // 2 minute offer window
                offer.state = LeaseState::Offered;
                pScope->leases[candidateIp] = offer;

                m_totalOffersSent.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }

        return false; // Scope exhausted
    }

    bool processRequest(const std::string& subnetHint, const std::string& mac, const std::string& requestedIp, const std::string& hostName, bool& outAck, uint32_t& outLeaseSec) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_totalRequestsReceived.fetch_add(1, std::memory_order_relaxed);
        outAck = false;
        outLeaseSec = 0;

        if (m_rogueDetectionSuppressed || !m_isAuthorizedInAd) {
            m_totalNaksSent.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        std::string nMac = normalizeMac(mac);

        // Find target scope
        DhcpScope* pScope = nullptr;
        if (!subnetHint.empty()) {
            auto it = m_scopes.find(subnetHint);
            if (it != m_scopes.end() && it->second.isActive) pScope = &it->second;
        }
        if (!pScope) {
            for (auto& [_, sc] : m_scopes) {
                if (isIpInRange(requestedIp, sc.startIp, sc.endIp) && sc.isActive) {
                    pScope = &sc;
                    break;
                }
            }
        }
        if (!pScope) {
            m_totalNaksSent.fetch_add(1, std::memory_order_relaxed);
            return false; // NAK
        }

        // Validate requested IP is within range
        if (!isIpInRange(requestedIp, pScope->startIp, pScope->endIp)) {
            m_totalNaksSent.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        // Validate exclusion ranges
        for (const auto& ex : pScope->exclusions) {
            if (isIpInRange(requestedIp, ex.startIp, ex.endIp)) {
                m_totalNaksSent.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
        }

        // Validate reservation ownership
        for (const auto& [_, res] : pScope->reservations) {
            if (res.ipAddress == requestedIp && res.macAddress != nMac) {
                m_totalNaksSent.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
        }

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        // Validate lease is not held by another active or offered client
        auto lIt = pScope->leases.find(requestedIp);
        if (lIt != pScope->leases.end() && lIt->second.macAddress != nMac &&
            (lIt->second.state == LeaseState::Active || (lIt->second.state == LeaseState::Offered && now <= lIt->second.leaseEnd))) {
            m_totalNaksSent.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        DhcpLease lease;
        lease.ipAddress = requestedIp;
        lease.macAddress = nMac;
        lease.hostName = hostName.empty() ? ("host-" + requestedIp) : hostName;
        lease.leaseStart = now;
        lease.leaseEnd = now + pScope->leaseDurationSeconds;
        lease.state = LeaseState::Active;
        lease.clientType = ClientType::Dhcp;
        lease.dnsRegistered = false;

        // Perform Option 81 Dynamic DNS registration (A and PTR)
        if (pScope->dynamicDnsEnabled && !hostName.empty()) {
            m_totalDnsUpdatesAttempted.fetch_add(1, std::memory_order_relaxed);
            auto& dns = micant::dns::EnterpriseDnsServer::instance();

            // Register Forward A Record
            bool okA = dns.processDynamicUpdate(pScope->domainName, hostName, micant::dns::TYPE_A, requestedIp, 300);

            // Register Reverse PTR Record in in-addr.arpa
            // Derive reverse FQDN (e.g. 192.168.1.105 -> 105.1.168.192.in-addr.arpa)
            uint32_t b1 = 0, b2 = 0, b3 = 0, b4 = 0;
            char dot = '.';
            std::istringstream iss(requestedIp);
            iss >> b1 >> dot >> b2 >> dot >> b3 >> dot >> b4;
            std::string revZone = std::to_string(b3) + "." + std::to_string(b2) + "." + std::to_string(b1) + ".in-addr.arpa";
            std::string revHost = std::to_string(b4);
            std::string fullFqdn = hostName;
            if (fullFqdn.find('.') == std::string::npos) fullFqdn += "." + pScope->domainName;

            bool okPtr = dns.processDynamicUpdate(revZone, revHost, micant::dns::TYPE_PTR, fullFqdn, 300);

            if (okA || okPtr) {
                lease.dnsRegistered = true;
                m_totalDnsUpdatesSucceeded.fetch_add(1, std::memory_order_relaxed);
            }
        }

        pScope->leases[requestedIp] = lease;
        outAck = true;
        outLeaseSec = pScope->leaseDurationSeconds;
        m_totalAcksSent.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    bool processRelease(const std::string& subnetHint, const std::string& ip, const std::string& mac) {
        (void)subnetHint;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_totalReleasesReceived.fetch_add(1, std::memory_order_relaxed);

        std::string nMac = normalizeMac(mac);

        for (auto& [_, sc] : m_scopes) {
            auto it = sc.leases.find(ip);
            if (it != sc.leases.end() && it->second.macAddress == nMac) {
                if (sc.discardLeaseOnRelease) {
                    sc.leases.erase(it);
                } else {
                    it->second.state = LeaseState::Released;
                }
                return true;
            }
        }
        return false;
    }

    bool processDecline(const std::string& subnetHint, const std::string& ip, const std::string& mac) {
        (void)subnetHint;
        (void)mac;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_totalDeclinesReceived.fetch_add(1, std::memory_order_relaxed);

        for (auto& [_, sc] : m_scopes) {
            auto it = sc.leases.find(ip);
            if (it != sc.leases.end()) {
                it->second.state = LeaseState::Quarantined;
                return true;
            }
        }
        return false;
    }

    // ========================================================================
    // DHCP Failover & High Availability (RFC 3074)
    // ========================================================================
    bool configureFailover(const std::string& subnet, const std::string& partnerServer, FailoverMode mode, uint32_t mcltSec = 3600, uint32_t loadBalanceSplit = 50) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_scopes.find(subnet);
        if (it == m_scopes.end()) return false;

        it->second.hasFailover = true;
        it->second.failover.name = "Failover-" + subnet;
        it->second.failover.primaryServer = m_serverHost;
        it->second.failover.secondaryServer = partnerServer;
        it->second.failover.mode = mode;
        it->second.failover.state = FailoverState::Normal;
        it->second.failover.mcltSeconds = mcltSec;
        it->second.failover.loadBalancePercent = loadBalanceSplit;
        return true;
    }

    bool syncFailoverLease(const std::string& subnet, const DhcpLease& lease) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_scopes.find(subnet);
        if (it == m_scopes.end() || !it->second.hasFailover) return false;

        it->second.leases[lease.ipAddress] = lease;
        m_totalFailoverSyncs.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    bool setFailoverState(const std::string& subnet, FailoverState newState) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_scopes.find(subnet);
        if (it == m_scopes.end() || !it->second.hasFailover) return false;

        it->second.failover.state = newState;
        return true;
    }

    // ========================================================================
    // DHCPv6 4-Way SARR Handshake (RFC 8415)
    // ========================================================================
    bool createV6Scope(const std::string& prefix, const std::string& startIp, const std::string& endIp) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_v6Scopes.find(prefix) != m_v6Scopes.end()) return false;

        Dhcpv6Scope sc6;
        sc6.prefix = prefix;
        sc6.startIp = startIp;
        sc6.endIp = endIp;
        sc6.preference = 255;
        sc6.dnsServers = { "2001:db8::10" };
        sc6.domainSearchList = m_domainName;

        m_v6Scopes[prefix] = sc6;
        return true;
    }

    bool processV6Solicit(const std::string& duid, const std::string& hostName, std::string& outAdvIp, uint32_t& outValidLifetime) {
        (void)duid;
        (void)hostName;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_totalDhcpv6SolicitsReceived.fetch_add(1, std::memory_order_relaxed);

        if (m_rogueDetectionSuppressed || !m_isAuthorizedInAd || m_v6Scopes.empty()) {
            return false;
        }

        auto& sc = m_v6Scopes.begin()->second;
        outAdvIp = sc.startIp;
        outValidLifetime = 86400;
        return true;
    }

    bool processV6Request(const std::string& duid, const std::string& requestedIp, const std::string& hostName, bool& outReply, uint32_t& outValidLifetime) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        outReply = false;
        outValidLifetime = 0;

        if (m_rogueDetectionSuppressed || !m_isAuthorizedInAd || m_v6Scopes.empty()) {
            return false;
        }

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        auto& sc = m_v6Scopes.begin()->second;
        Dhcpv6Lease l;
        l.ipv6Address = requestedIp;
        l.duid = duid;
        l.iaid = 1;
        l.leaseStart = now;
        l.validLifetime = 86400;
        l.preferredLifetime = 43200;
        l.hostName = hostName;
        l.state = LeaseState::Active;

        sc.leases[requestedIp] = l;
        outReply = true;
        outValidLifetime = 86400;
        m_totalDhcpv6RepliesSent.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    bool processV6Release(const std::string& duid, const std::string& ip) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto& [_, sc] : m_v6Scopes) {
            auto it = sc.leases.find(ip);
            if (it != sc.leases.end() && it->second.duid == duid) {
                sc.leases.erase(it);
                return true;
            }
        }
        return false;
    }

    // ========================================================================
    // Telemetry & Getters
    // ========================================================================
    size_t getScopeCount() const { std::lock_guard<std::recursive_mutex> lock(m_mutex); return m_scopes.size(); }
    size_t getLeaseCount(const std::string& subnet) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_scopes.find(subnet);
        return (it != m_scopes.end()) ? it->second.leases.size() : 0;
    }

    uint64_t getTotalDiscovers() const { return m_totalDiscoversReceived.load(std::memory_order_relaxed); }
    uint64_t getTotalOffers() const { return m_totalOffersSent.load(std::memory_order_relaxed); }
    uint64_t getTotalRequests() const { return m_totalRequestsReceived.load(std::memory_order_relaxed); }
    uint64_t getTotalAcks() const { return m_totalAcksSent.load(std::memory_order_relaxed); }
    uint64_t getTotalNaks() const { return m_totalNaksSent.load(std::memory_order_relaxed); }
    uint64_t getTotalReleases() const { return m_totalReleasesReceived.load(std::memory_order_relaxed); }
    uint64_t getTotalDeclines() const { return m_totalDeclinesReceived.load(std::memory_order_relaxed); }
    uint64_t getTotalDhcpv6Solicits() const { return m_totalDhcpv6SolicitsReceived.load(std::memory_order_relaxed); }
    uint64_t getTotalDhcpv6Replies() const { return m_totalDhcpv6RepliesSent.load(std::memory_order_relaxed); }
    uint64_t getTotalDnsUpdatesAttempted() const { return m_totalDnsUpdatesAttempted.load(std::memory_order_relaxed); }
    uint64_t getTotalDnsUpdatesSucceeded() const { return m_totalDnsUpdatesSucceeded.load(std::memory_order_relaxed); }
    uint64_t getTotalFailoverSyncs() const { return m_totalFailoverSyncs.load(std::memory_order_relaxed); }

    std::string getServerHost() const { std::lock_guard<std::recursive_mutex> lk(m_mutex); return m_serverHost; }
    std::string getServerIp() const { std::lock_guard<std::recursive_mutex> lk(m_mutex); return m_serverIp; }
    std::string getDomainName() const { std::lock_guard<std::recursive_mutex> lk(m_mutex); return m_domainName; }
};

// ============================================================================
// Clean-Room Win32 C ABI Exports (dhcpsapi.dll / tcpsvcs.exe Parity)
// ============================================================================
extern "C" {

inline int32_t MicaDhcpInitialize(void** ppEngine) {
    if (!ppEngine) return 0;
    auto& eng = EnterpriseDhcpServer::instance();
    eng.initialize();
    *ppEngine = &eng;
    return 1;
}

inline int32_t MicaDhcpAuthorizeServer(void* pEngine, const char* szServerIp, const char* szDomain) {
    if (!pEngine || !szServerIp || !szDomain) return 0;
    auto* eng = static_cast<EnterpriseDhcpServer*>(pEngine);
    return eng->authorizeServerInAd(szServerIp, szDomain) ? 1 : 0;
}

inline int32_t MicaDhcpCreateScope(void* pEngine, const char* szSubnet, const char* szMask, const char* szStartIp, const char* szEndIp, uint32_t leaseSec) {
    if (!pEngine || !szSubnet || !szMask || !szStartIp || !szEndIp) return 0;
    auto* eng = static_cast<EnterpriseDhcpServer*>(pEngine);
    return eng->createScope(szSubnet, szMask, szStartIp, szEndIp, leaseSec) ? 1 : 0;
}

inline int32_t MicaDhcpAddReservation(void* pEngine, const char* szSubnet, const char* szIp, const char* szMac, const char* szName) {
    if (!pEngine || !szSubnet || !szIp || !szMac || !szName) return 0;
    auto* eng = static_cast<EnterpriseDhcpServer*>(pEngine);
    return eng->addReservation(szSubnet, szIp, szMac, szName) ? 1 : 0;
}

inline int32_t MicaDhcpProcessDiscover(void* pEngine, const char* szMac, const char* szHostName, char* szOfferedIp, uint32_t cbMax) {
    if (!pEngine || !szMac || !szOfferedIp || cbMax == 0) return 0;
    auto* eng = static_cast<EnterpriseDhcpServer*>(pEngine);
    std::string ip;
    uint32_t lease = 0;
    if (!eng->processDiscover("", szMac, szHostName ? szHostName : "", ip, lease)) return 0;
    if (ip.size() >= cbMax) return 0;
    std::memcpy(szOfferedIp, ip.c_str(), ip.size() + 1);
    return 1;
}

inline int32_t MicaDhcpProcessRequest(void* pEngine, const char* szMac, const char* szReqIp, const char* szHostName, uint32_t* pLeaseSec) {
    if (!pEngine || !szMac || !szReqIp) return 0;
    auto* eng = static_cast<EnterpriseDhcpServer*>(pEngine);
    bool ack = false;
    uint32_t lease = 0;
    if (!eng->processRequest("", szMac, szReqIp, szHostName ? szHostName : "", ack, lease)) return 0;
    if (pLeaseSec) *pLeaseSec = lease;
    return ack ? 1 : 0;
}

inline int32_t MicaDhcpConfigureFailover(void* pEngine, const char* szSubnet, const char* szPartnerIp, uint32_t mode) {
    if (!pEngine || !szSubnet || !szPartnerIp) return 0;
    auto* eng = static_cast<EnterpriseDhcpServer*>(pEngine);
    FailoverMode fm = (mode == 1) ? FailoverMode::HotStandby : FailoverMode::LoadBalance;
    return eng->configureFailover(szSubnet, szPartnerIp, fm) ? 1 : 0;
}

inline int32_t MicaDhcpShutdown(void* pEngine) {
    if (!pEngine) return 0;
    return 1;
}

} // extern "C"

} // namespace micant::dhcp

#endif // MICANT_DHCP_SERVER_HPP
