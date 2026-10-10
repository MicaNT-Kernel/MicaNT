#ifndef MICANT_DNS_SERVER_HPP
#define MICANT_DNS_SERVER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <random>

#include "micant/scm.hpp"
#include "micant/version.hpp"

namespace micant::dns {

// ============================================================================
// DNS Protocol Constants (RFC 1035, RFC 2136, RFC 2782, RFC 3596, RFC 6891)
// ============================================================================

constexpr uint16_t DNS_PORT = 53;

// Record Types
constexpr uint16_t TYPE_A       = 1;   // IPv4 Address (RFC 1035)
constexpr uint16_t TYPE_NS      = 2;   // Authoritative Name Server (RFC 1035)
constexpr uint16_t TYPE_CNAME   = 5;   // Canonical Name / Alias (RFC 1035)
constexpr uint16_t TYPE_SOA     = 6;   // Start of Authority (RFC 1035)
constexpr uint16_t TYPE_PTR     = 12;  // Domain Name Pointer (RFC 1035)
constexpr uint16_t TYPE_MX      = 15;  // Mail Exchange (RFC 1035)
constexpr uint16_t TYPE_TXT     = 16;  // Text strings (RFC 1035)
constexpr uint16_t TYPE_AAAA    = 28;  // IPv6 Address (RFC 3596)
constexpr uint16_t TYPE_SRV     = 33;  // Service Locator (RFC 2782)
constexpr uint16_t TYPE_OPT     = 41;  // EDNS0 Option (RFC 6891)
constexpr uint16_t TYPE_DS      = 43;  // Delegation Signer (RFC 4034)
constexpr uint16_t TYPE_RRSIG   = 46;  // DNSSEC Signature (RFC 4034)
constexpr uint16_t TYPE_NSEC    = 47;  // Next Secure Record (RFC 4034)
constexpr uint16_t TYPE_DNSKEY  = 48;  // DNS Public Key (RFC 4034)
constexpr uint16_t TYPE_AXFR    = 252; // Full Zone Transfer (RFC 1035/5936)
constexpr uint16_t TYPE_ANY     = 255; // Any record (RFC 1035)

// DNS Classes
constexpr uint16_t CLASS_IN     = 1;   // Internet
constexpr uint16_t CLASS_ANY    = 255;

// DNS Response Codes (RCODE)
constexpr uint16_t RCODE_NOERROR  = 0;
constexpr uint16_t RCODE_FORMERR  = 1;
constexpr uint16_t RCODE_SERVFAIL = 2;
constexpr uint16_t RCODE_NXDOMAIN = 3;
constexpr uint16_t RCODE_NOTIMP   = 4;
constexpr uint16_t RCODE_REFUSED  = 5;
constexpr uint16_t RCODE_YXDOMAIN = 6;
constexpr uint16_t RCODE_YXRRSET  = 7;
constexpr uint16_t RCODE_NXRRSET  = 8;
constexpr uint16_t RCODE_NOTAUTH  = 9;
constexpr uint16_t RCODE_NOTZONE  = 10;

// DNS Header Flags
constexpr uint16_t FLAG_QR = 0x8000; // Query (0) or Response (1)
constexpr uint16_t FLAG_AA = 0x0400; // Authoritative Answer
constexpr uint16_t FLAG_TC = 0x0200; // TrunCation
constexpr uint16_t FLAG_RD = 0x0100; // Recursion Desired
constexpr uint16_t FLAG_RA = 0x0080; // Recursion Available

enum class ZoneType {
    Primary,
    Secondary,
    Stub,
    Forwarder
};

enum class ReplicationScope {
    ActiveDirectoryForest,
    ActiveDirectoryDomain,
    LegacyFile
};

inline std::string RecordTypeToString(uint16_t type) {
    switch (type) {
        case TYPE_A:      return "A";
        case TYPE_NS:     return "NS";
        case TYPE_CNAME:  return "CNAME";
        case TYPE_SOA:    return "SOA";
        case TYPE_PTR:    return "PTR";
        case TYPE_MX:     return "MX";
        case TYPE_TXT:    return "TXT";
        case TYPE_AAAA:   return "AAAA";
        case TYPE_SRV:    return "SRV";
        case TYPE_OPT:    return "OPT";
        case TYPE_DS:     return "DS";
        case TYPE_RRSIG:  return "RRSIG";
        case TYPE_NSEC:   return "NSEC";
        case TYPE_DNSKEY: return "DNSKEY";
        case TYPE_AXFR:   return "AXFR";
        case TYPE_ANY:    return "ANY";
        default:          return "TYPE" + std::to_string(type);
    }
}

inline uint16_t StringToRecordType(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    if (s == "A")      return TYPE_A;
    if (s == "NS")     return TYPE_NS;
    if (s == "CNAME")  return TYPE_CNAME;
    if (s == "SOA")    return TYPE_SOA;
    if (s == "PTR")    return TYPE_PTR;
    if (s == "MX")     return TYPE_MX;
    if (s == "TXT")    return TYPE_TXT;
    if (s == "AAAA")   return TYPE_AAAA;
    if (s == "SRV")    return TYPE_SRV;
    if (s == "OPT")    return TYPE_OPT;
    if (s == "DS")     return TYPE_DS;
    if (s == "RRSIG")  return TYPE_RRSIG;
    if (s == "NSEC")   return TYPE_NSEC;
    if (s == "DNSKEY") return TYPE_DNSKEY;
    if (s == "AXFR")   return TYPE_AXFR;
    if (s == "ANY")    return TYPE_ANY;
    return 0;
}

inline std::string RcodeToString(uint16_t rcode) {
    switch (rcode) {
        case RCODE_NOERROR:  return "NOERROR";
        case RCODE_FORMERR:  return "FORMERR";
        case RCODE_SERVFAIL: return "SERVFAIL";
        case RCODE_NXDOMAIN: return "NXDOMAIN";
        case RCODE_NOTIMP:   return "NOTIMP";
        case RCODE_REFUSED:  return "REFUSED";
        case RCODE_YXDOMAIN: return "YXDOMAIN";
        case RCODE_YXRRSET:  return "YXRRSET";
        case RCODE_NXRRSET:  return "NXRRSET";
        case RCODE_NOTAUTH:  return "NOTAUTH";
        case RCODE_NOTZONE:  return "NOTZONE";
        default: return "RCODE_" + std::to_string(rcode);
    }
}

inline std::string ZoneTypeToString(ZoneType zt) {
    switch (zt) {
        case ZoneType::Primary:   return "Primary";
        case ZoneType::Secondary: return "Secondary";
        case ZoneType::Stub:      return "Stub";
        case ZoneType::Forwarder: return "Forwarder";
        default: return "Unknown";
    }
}

// ============================================================================
// DNS Wire Header (RFC 1035 Section 4.1.1)
// ============================================================================
#pragma pack(push, 1)
struct DnsHeaderWire {
    uint16_t id{0};
    uint16_t flags{0};
    uint16_t qdcount{0};
    uint16_t ancount{0};
    uint16_t nscount{0};
    uint16_t arcount{0};
};
#pragma pack(pop)

// ============================================================================
// Resource Record Definition
// ============================================================================
struct DnsResourceRecord {
    std::string name;       // FQDN e.g. "dc01.titan.local"
    uint16_t    type{TYPE_A};
    uint16_t    rclass{CLASS_IN};
    uint32_t    ttl{3600};
    std::string rdata;      // e.g. "192.168.1.10", "0 100 389 dc01.titan.local", etc.
    uint64_t    timestamp{0};
};

// ============================================================================
// DNS Zone Structure
// ============================================================================
struct DnsZone {
    std::string                    zoneName; // e.g. "titan.local"
    ZoneType                       type{ZoneType::Primary};
    ReplicationScope               replication{ReplicationScope::ActiveDirectoryDomain};
    bool                           isAdIntegrated{true};
    bool                           secureDynamicUpdateOnly{true};
    bool                           isDnssecSigned{false};
    uint32_t                       soaSerial{1};
    std::string                    primaryNs{"dc01.titan.local"};
    std::string                    respPerson{"hostmaster.titan.local"};
    uint32_t                       refresh{900};
    uint32_t                       retry{600};
    uint32_t                       expire{86400};
    uint32_t                       minimumTtl{3600};
    std::vector<DnsResourceRecord> records;
};

// ============================================================================
// In-Memory Resolver Cache Entry
// ============================================================================
struct DnsCacheEntry {
    std::string                    name;
    uint16_t                       type{TYPE_A};
    std::vector<DnsResourceRecord> records;
    uint64_t                       expiryTime{0};
    bool                           isNegative{false};
    uint16_t                       negativeRcode{RCODE_NOERROR};
};

// ============================================================================
// EnterpriseDnsServer: Core Singleton Engine
// ============================================================================
class EnterpriseDnsServer {
private:
    mutable std::recursive_mutex m_mutex;
    bool m_initialized{false};

    // Server Info
    std::string m_serverHost{"dc01.titan.local"};
    std::string m_serverIp{"192.168.1.10"};
    std::string m_domainName{"titan.local"};

    // Databases
    std::map<std::string, DnsZone>       m_zones;       // Keyed by lowercase zoneName
    std::map<std::string, DnsCacheEntry> m_cache;       // Keyed by "name:type"

    // Telemetry Counters
    mutable std::atomic<uint64_t> m_totalQueriesReceived{0};
    mutable std::atomic<uint64_t> m_totalQueriesAnswered{0};
    mutable std::atomic<uint64_t> m_totalCacheHits{0};
    mutable std::atomic<uint64_t> m_totalCacheMisses{0};
    mutable std::atomic<uint64_t> m_totalDynamicUpdates{0};
    mutable std::atomic<uint64_t> m_totalZoneTransfers{0};
    mutable std::atomic<uint64_t> m_totalDnssecQueries{0};

    static std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }

public:
    static EnterpriseDnsServer& instance() {
        static EnterpriseDnsServer inst;
        return inst;
    }

    EnterpriseDnsServer() {
        initialize();
    }

    void initialize() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_initialized) return;

        seedDefaultZones();
        registerScmServices();
        registerVersionDatabase();

        m_initialized = true;
    }

    void registerScmServices() {
        auto& scm = micant::scm::ServiceControlManager::get();

        auto recDns = std::make_shared<micant::scm::ServiceRecord>();
        recDns->serviceName = L"DNS";
        recDns->displayName = L"DNS Server";
        recDns->serviceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
        recDns->startType = micant::scm::SERVICE_AUTO_START;
        recDns->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recDns->binaryPath = L"C:\\Windows\\System32\\dns.exe";
        recDns->status.dwServiceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
        recDns->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        recDns->status.dwControlsAccepted = micant::scm::SERVICE_ACCEPT_STOP | micant::scm::SERVICE_ACCEPT_SHUTDOWN | micant::scm::SERVICE_ACCEPT_PAUSE_CONTINUE;
        scm.registerServiceRecord(recDns);
    }

    void registerVersionDatabase() {
        auto& db = micant::version::VersionDatabase::Instance();
        db.RegisterModule("dns.exe", "10.0.26100.1", "Windows Enterprise DNS Server Service");
        db.RegisterModule("dnsapi.dll", "10.0.26100.1", "DNS Client and Resolver API");
        db.RegisterModule("dnslib.dll", "10.0.26100.1", "DNS Common Protocol Library");
        db.RegisterModule("dnscache.dll", "10.0.26100.1", "DNS Client Resolver Cache Service");
        db.RegisterModule("dnscmd.exe", "10.0.26100.1", "DNS Server Command-Line Management Tool");
    }

    void seedDefaultZones() {
        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        // 1. Forward Lookup Zone: titan.local (Active Directory Integrated)
        DnsZone zAd;
        zAd.zoneName = "titan.local";
        zAd.type = ZoneType::Primary;
        zAd.replication = ReplicationScope::ActiveDirectoryDomain;
        zAd.isAdIntegrated = true;
        zAd.secureDynamicUpdateOnly = true;
        zAd.soaSerial = 100;
        zAd.primaryNs = "dc01.titan.local";
        zAd.respPerson = "hostmaster.titan.local";

        // SOA & NS
        zAd.records.push_back({ "titan.local", TYPE_SOA, CLASS_IN, 3600, "dc01.titan.local hostmaster.titan.local 100 900 600 86400 3600", now });
        zAd.records.push_back({ "titan.local", TYPE_NS, CLASS_IN, 3600, "dc01.titan.local", now });

        // Domain apex & Core Domain Controllers
        zAd.records.push_back({ "titan.local", TYPE_A, CLASS_IN, 600, "192.168.1.10", now });
        zAd.records.push_back({ "titan.local", TYPE_A, CLASS_IN, 600, "192.168.1.11", now });
        zAd.records.push_back({ "dc01.titan.local", TYPE_A, CLASS_IN, 3600, "192.168.1.10", now });
        zAd.records.push_back({ "dc01.titan.local", TYPE_AAAA, CLASS_IN, 3600, "2001:db8::10", now });
        zAd.records.push_back({ "dc02.titan.local", TYPE_A, CLASS_IN, 3600, "192.168.1.11", now });

        // Infrastructure Servers
        zAd.records.push_back({ "titan-wds.titan.local", TYPE_A, CLASS_IN, 3600, "192.168.1.50", now });
        zAd.records.push_back({ "titan-ca.titan.local", TYPE_A, CLASS_IN, 3600, "192.168.1.60", now });
        zAd.records.push_back({ "mail.titan.local", TYPE_A, CLASS_IN, 3600, "192.168.1.25", now });

        // CNAME Aliases
        zAd.records.push_back({ "pki.titan.local", TYPE_CNAME, CLASS_IN, 3600, "titan-ca.titan.local", now });
        zAd.records.push_back({ "pxe.titan.local", TYPE_CNAME, CLASS_IN, 3600, "titan-wds.titan.local", now });

        // Active Directory Service Discovery SRV records (RFC 2782)
        zAd.records.push_back({ "_ldap._tcp.titan.local", TYPE_SRV, CLASS_IN, 600, "0 100 389 dc01.titan.local", now });
        zAd.records.push_back({ "_kerberos._tcp.titan.local", TYPE_SRV, CLASS_IN, 600, "0 100 88 dc01.titan.local", now });
        zAd.records.push_back({ "_kpasswd._tcp.titan.local", TYPE_SRV, CLASS_IN, 600, "0 100 464 dc01.titan.local", now });
        zAd.records.push_back({ "_gc._tcp.titan.local", TYPE_SRV, CLASS_IN, 600, "0 100 3268 dc01.titan.local", now });

        // MX & TXT
        zAd.records.push_back({ "titan.local", TYPE_MX, CLASS_IN, 3600, "10 mail.titan.local", now });
        zAd.records.push_back({ "titan.local", TYPE_TXT, CLASS_IN, 3600, "\"v=spf1 mx ip4:192.168.1.25 ~all\"", now });

        m_zones[toLower(zAd.zoneName)] = zAd;

        // 2. Active Directory Forest Locator Zone: _msdcs.titan.local
        DnsZone zMsdcs;
        zMsdcs.zoneName = "_msdcs.titan.local";
        zMsdcs.type = ZoneType::Primary;
        zMsdcs.replication = ReplicationScope::ActiveDirectoryForest;
        zMsdcs.isAdIntegrated = true;
        zMsdcs.secureDynamicUpdateOnly = true;
        zMsdcs.soaSerial = 100;
        zMsdcs.primaryNs = "dc01.titan.local";
        zMsdcs.respPerson = "hostmaster.titan.local";

        zMsdcs.records.push_back({ "_msdcs.titan.local", TYPE_SOA, CLASS_IN, 3600, "dc01.titan.local hostmaster.titan.local 100 900 600 86400 3600", now });
        zMsdcs.records.push_back({ "_msdcs.titan.local", TYPE_NS, CLASS_IN, 3600, "dc01.titan.local", now });
        zMsdcs.records.push_back({ "_ldap._tcp.dc._msdcs.titan.local", TYPE_SRV, CLASS_IN, 600, "0 100 389 dc01.titan.local", now });
        zMsdcs.records.push_back({ "_kerberos._tcp.dc._msdcs.titan.local", TYPE_SRV, CLASS_IN, 600, "0 100 88 dc01.titan.local", now });
        zMsdcs.records.push_back({ "c5b1c552-3a87-4d7a-8b1e-08992e541a77._msdcs.titan.local", TYPE_CNAME, CLASS_IN, 600, "dc01.titan.local", now });

        m_zones[toLower(zMsdcs.zoneName)] = zMsdcs;

        // 3. Reverse Lookup Zone: 1.168.192.in-addr.arpa
        DnsZone zRev;
        zRev.zoneName = "1.168.192.in-addr.arpa";
        zRev.type = ZoneType::Primary;
        zRev.replication = ReplicationScope::ActiveDirectoryDomain;
        zRev.isAdIntegrated = true;
        zRev.soaSerial = 100;
        zRev.primaryNs = "dc01.titan.local";
        zRev.respPerson = "hostmaster.titan.local";

        zRev.records.push_back({ "1.168.192.in-addr.arpa", TYPE_SOA, CLASS_IN, 3600, "dc01.titan.local hostmaster.titan.local 100 900 600 86400 3600", now });
        zRev.records.push_back({ "1.168.192.in-addr.arpa", TYPE_NS, CLASS_IN, 3600, "dc01.titan.local", now });
        zRev.records.push_back({ "10.1.168.192.in-addr.arpa", TYPE_PTR, CLASS_IN, 3600, "dc01.titan.local", now });
        zRev.records.push_back({ "11.1.168.192.in-addr.arpa", TYPE_PTR, CLASS_IN, 3600, "dc02.titan.local", now });
        zRev.records.push_back({ "50.1.168.192.in-addr.arpa", TYPE_PTR, CLASS_IN, 3600, "titan-wds.titan.local", now });
        zRev.records.push_back({ "60.1.168.192.in-addr.arpa", TYPE_PTR, CLASS_IN, 3600, "titan-ca.titan.local", now });

        m_zones[toLower(zRev.zoneName)] = zRev;
    }

    // ========================================================================
    // Zone Management
    // ========================================================================
    bool createZone(const std::string& zoneName, ZoneType zType, ReplicationScope scope, bool isAdIntegrated) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::string key = toLower(zoneName);
        if (m_zones.find(key) != m_zones.end()) return false;

        DnsZone z;
        z.zoneName = zoneName;
        z.type = zType;
        z.replication = scope;
        z.isAdIntegrated = isAdIntegrated;
        z.soaSerial = 1;
        z.primaryNs = m_serverHost;
        z.respPerson = "hostmaster." + zoneName;

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        std::string soaData = z.primaryNs + " " + z.respPerson + " 1 900 600 86400 3600";
        z.records.push_back({ zoneName, TYPE_SOA, CLASS_IN, 3600, soaData, now });
        z.records.push_back({ zoneName, TYPE_NS, CLASS_IN, 3600, z.primaryNs, now });

        m_zones[key] = z;
        return true;
    }

    bool getZone(const std::string& zoneName, DnsZone& outZone) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_zones.find(toLower(zoneName));
        if (it == m_zones.end()) return false;
        outZone = it->second;
        return true;
    }

    std::vector<DnsZone> getAllZones() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<DnsZone> res;
        res.reserve(m_zones.size());
        for (const auto& [_, z] : m_zones) {
            res.push_back(z);
        }
        return res;
    }

    bool addRecord(const std::string& zoneName, const DnsResourceRecord& rr) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_zones.find(toLower(zoneName));
        if (it == m_zones.end()) return false;

        DnsResourceRecord rec = rr;
        if (rec.timestamp == 0) {
            rec.timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
        it->second.records.push_back(rec);
        it->second.soaSerial++; // Auto-increment SOA serial

        // Invalidate cache
        m_cache.erase(toLower(rec.name) + ":" + std::to_string(rec.type));
        return true;
    }

    bool deleteRecord(const std::string& zoneName, const std::string& name, uint16_t type) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_zones.find(toLower(zoneName));
        if (it == m_zones.end()) return false;

        std::string targetName = toLower(name);
        auto& recs = it->second.records;
        size_t initialSize = recs.size();

        recs.erase(std::remove_if(recs.begin(), recs.end(), [&](const DnsResourceRecord& r) {
            return toLower(r.name) == targetName && (type == TYPE_ANY || r.type == type);
        }), recs.end());

        if (recs.size() != initialSize) {
            it->second.soaSerial++;
            m_cache.erase(targetName + ":" + std::to_string(type));
            return true;
        }
        return false;
    }

    // ========================================================================
    // Query Resolution Engine (Authoritative + In-Memory Cache)
    // ========================================================================
    std::vector<DnsResourceRecord> queryRecords(const std::string& fqdn, uint16_t type, bool* pAuthoritative = nullptr, uint16_t* pRcode = nullptr, int depth = 0) {
        if (depth > 8) {
            if (pRcode) *pRcode = RCODE_SERVFAIL;
            return {};
        }
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_totalQueriesReceived.fetch_add(1, std::memory_order_relaxed);

        std::string qName = toLower(fqdn);
        // Strip trailing dot if present
        if (!qName.empty() && qName.back() == '.') {
            qName.pop_back();
        }

        if (pAuthoritative) *pAuthoritative = false;
        if (pRcode) *pRcode = RCODE_NOERROR;

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        // 1. Check Resolver Cache
        std::string cacheKey = qName + ":" + std::to_string(type);
        auto cIt = m_cache.find(cacheKey);
        if (cIt != m_cache.end() && cIt->second.expiryTime > now) {
            m_totalCacheHits.fetch_add(1, std::memory_order_relaxed);
            m_totalQueriesAnswered.fetch_add(1, std::memory_order_relaxed);
            if (cIt->second.isNegative) {
                if (pRcode) *pRcode = cIt->second.negativeRcode;
                return {};
            }
            return cIt->second.records;
        }
        m_totalCacheMisses.fetch_add(1, std::memory_order_relaxed);

        // 2. Search Authoritative Zones
        // Find best matching zone (longest suffix match)
        const DnsZone* bestZone = nullptr;
        size_t bestMatchLen = 0;

        for (const auto& [zKey, z] : m_zones) {
            if (qName == zKey || (qName.size() > zKey.size() && qName.compare(qName.size() - zKey.size() - 1, zKey.size() + 1, "." + zKey) == 0)) {
                if (zKey.size() > bestMatchLen) {
                    bestMatchLen = zKey.size();
                    bestZone = &z;
                }
            }
        }

        std::vector<DnsResourceRecord> matches;

        if (bestZone) {
            if (pAuthoritative) *pAuthoritative = true;

            // Direct record search
            for (const auto& r : bestZone->records) {
                std::string rName = toLower(r.name);
                if (!rName.empty() && rName.back() == '.') rName.pop_back();

                if (rName == qName) {
                    if (type == TYPE_ANY || r.type == type) {
                        matches.push_back(r);
                    }
                }
            }

            // If no direct match, check if there is a CNAME
            if (matches.empty() && type != TYPE_CNAME) {
                for (const auto& r : bestZone->records) {
                    std::string rName = toLower(r.name);
                    if (!rName.empty() && rName.back() == '.') rName.pop_back();

                    if (rName == qName && r.type == TYPE_CNAME) {
                        matches.push_back(r);
                        // Recursively resolve target
                        auto targetMatches = queryRecords(r.rdata, type, nullptr, nullptr, depth + 1);
                        matches.insert(matches.end(), targetMatches.begin(), targetMatches.end());
                        break;
                    }
                }
            }

            if (matches.empty()) {
                // NXDOMAIN in authoritative zone
                if (pRcode) *pRcode = RCODE_NXDOMAIN;

                // Cache negative response for 300s
                DnsCacheEntry neg;
                neg.name = qName;
                neg.type = type;
                neg.expiryTime = now + 300;
                neg.isNegative = true;
                neg.negativeRcode = RCODE_NXDOMAIN;
                m_cache[cacheKey] = neg;

                return {};
            }

            // Populate positive cache
            DnsCacheEntry pos;
            pos.name = qName;
            pos.type = type;
            pos.records = matches;
            pos.expiryTime = now + 600;
            pos.isNegative = false;
            m_cache[cacheKey] = pos;

            m_totalQueriesAnswered.fetch_add(1, std::memory_order_relaxed);
            return matches;
        }

        // Zone not found -> NXDOMAIN
        if (pRcode) *pRcode = RCODE_NXDOMAIN;
        return {};
    }

    // ========================================================================
    // Dynamic DNS (DDNS / RFC 2136)
    // ========================================================================
    bool processDynamicUpdate(const std::string& zoneName, const std::string& hostName, uint16_t type, const std::string& data, uint32_t ttl) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_totalDynamicUpdates.fetch_add(1, std::memory_order_relaxed);

        auto it = m_zones.find(toLower(zoneName));
        if (it == m_zones.end()) return false;

        std::string fqdn = hostName;
        if (fqdn.find('.') == std::string::npos) {
            fqdn += "." + it->second.zoneName;
        }

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        // Replace existing record of same type or add new
        auto& recs = it->second.records;
        bool updated = false;

        for (auto& r : recs) {
            if (toLower(r.name) == toLower(fqdn) && r.type == type) {
                r.rdata = data;
                r.ttl = ttl;
                r.timestamp = now;
                updated = true;
                break;
            }
        }

        if (!updated) {
            recs.push_back({ fqdn, type, CLASS_IN, ttl, data, now });
        }

        it->second.soaSerial++; // Increment zone serial
        m_cache.erase(toLower(fqdn) + ":" + std::to_string(type));
        return true;
    }

    // ========================================================================
    // DNSSEC Zone Signing Engine (RFC 4034 / RFC 4035)
    // ========================================================================
    bool signZoneDnssec(const std::string& zoneName) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_zones.find(toLower(zoneName));
        if (it == m_zones.end()) return false;

        auto& z = it->second;
        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        // 1. Add Zone Signing Key (DNSKEY, Algorithm 8: RSA/SHA-256)
        std::string dnskeyData = "256 3 8 AwEAAc0N1G1N8...SyntheticZSKPublicKey...";
        z.records.push_back({ z.zoneName, TYPE_DNSKEY, CLASS_IN, 86400, dnskeyData, now });

        // 2. Add Key Signing Key (DNSKEY, Flags 257)
        std::string kskData = "257 3 8 AwEAAd7T2P4A9...SyntheticKSKPublicKey...";
        z.records.push_back({ z.zoneName, TYPE_DNSKEY, CLASS_IN, 86400, kskData, now });

        // 3. Add RRSIG covering SOA
        std::string rrsigData = "SOA 8 2 3600 " + std::to_string(now + 2592000) + " " + std::to_string(now) + " 12345 " + z.zoneName + ". SyntheticRrsigSignatureBlob...";
        z.records.push_back({ z.zoneName, TYPE_RRSIG, CLASS_IN, 3600, rrsigData, now });

        // 4. Add NSEC for authenticated denial of existence
        std::string nsecData = "z." + z.zoneName + ". A NS SOA MX TXT RRSIG NSEC DNSKEY";
        z.records.push_back({ z.zoneName, TYPE_NSEC, CLASS_IN, 3600, nsecData, now });

        z.isDnssecSigned = true;
        z.soaSerial++;
        return true;
    }

    // ========================================================================
    // Full Zone Transfer (AXFR / RFC 5936)
    // ========================================================================
    bool performAxfr(const std::string& zoneName, std::vector<DnsResourceRecord>& outRecords) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_totalZoneTransfers.fetch_add(1, std::memory_order_relaxed);

        auto it = m_zones.find(toLower(zoneName));
        if (it == m_zones.end()) return false;

        outRecords.clear();
        const auto& z = it->second;

        // AXFR stream starts with SOA
        DnsResourceRecord soaRecord;
        bool foundSoa = false;
        for (const auto& r : z.records) {
            if (r.type == TYPE_SOA) {
                soaRecord = r;
                foundSoa = true;
                break;
            }
        }

        if (foundSoa) outRecords.push_back(soaRecord);

        // All intermediate records
        for (const auto& r : z.records) {
            if (r.type != TYPE_SOA) {
                outRecords.push_back(r);
            }
        }

        // AXFR stream terminates with closing SOA
        if (foundSoa) outRecords.push_back(soaRecord);
        return true;
    }

    // ========================================================================
    // Wire Protocol Packet Parsing & Assembly (RFC 1035 / RFC 6891 EDNS0)
    // ========================================================================
    bool processWireQuery(const uint8_t* inPacket, size_t inLen, std::vector<uint8_t>& outPacket) {
        if (!inPacket || inLen < sizeof(DnsHeaderWire)) return false;

        const auto* inHdr = reinterpret_cast<const DnsHeaderWire*>(inPacket);
        uint16_t qdcount = ((inHdr->qdcount >> 8) & 0xFF) | ((inHdr->qdcount << 8) & 0xFF00);
        uint16_t arcount = ((inHdr->arcount >> 8) & 0xFF) | ((inHdr->arcount << 8) & 0xFF00);

        if (qdcount == 0) return false;

        // Parse Question Name
        size_t offset = sizeof(DnsHeaderWire);
        std::string qname;
        while (offset < inLen) {
            uint8_t len = inPacket[offset++];
            if (len == 0) break;
            if (offset + len > inLen) return false;
            if (!qname.empty()) qname += ".";
            qname.append(reinterpret_cast<const char*>(&inPacket[offset]), len);
            offset += len;
        }

        if (offset + 4 > inLen) return false;
        uint16_t qtype = (static_cast<uint16_t>(inPacket[offset]) << 8) | inPacket[offset + 1];
        // uint16_t qclass = (static_cast<uint16_t>(inPacket[offset + 2]) << 8) | inPacket[offset + 3];
        offset += 4;

        // Check for EDNS0 OPT record in Additional Section
        bool hasEdns0 = false;
        uint16_t ednsPayloadSize = 512;
        if (arcount > 0 && offset < inLen) {
            // Simplified OPT detector
            for (size_t p = offset; p + 10 <= inLen; ++p) {
                if (inPacket[p] == 0 && inPacket[p + 1] == 0 && inPacket[p + 2] == 41) { // Root + TYPE_OPT
                    hasEdns0 = true;
                    ednsPayloadSize = (static_cast<uint16_t>(inPacket[p + 3]) << 8) | inPacket[p + 4];
                    if (ednsPayloadSize < 512) ednsPayloadSize = 4096;
                    break;
                }
            }
        }

        // Execute Query
        bool isAuth = false;
        uint16_t rcode = RCODE_NOERROR;
        auto answers = queryRecords(qname, qtype, &isAuth, &rcode);

        // Build Response Packet
        outPacket.clear();
        outPacket.resize(sizeof(DnsHeaderWire));
        auto* outHdr = reinterpret_cast<DnsHeaderWire*>(outPacket.data());

        outHdr->id = inHdr->id; // Echo transaction ID
        uint16_t flags = FLAG_QR | FLAG_RA;
        if (isAuth) flags |= FLAG_AA;
        flags |= (rcode & 0x0F);
        outHdr->flags = ((flags >> 8) & 0xFF) | ((flags << 8) & 0xFF00);

        outHdr->qdcount = inHdr->qdcount; // Echo question count
        uint16_t ancount = static_cast<uint16_t>(answers.size());
        outHdr->ancount = ((ancount >> 8) & 0xFF) | ((ancount << 8) & 0xFF00);
        outHdr->nscount = 0;
        uint16_t respArcount = hasEdns0 ? 1 : 0;
        outHdr->arcount = ((respArcount >> 8) & 0xFF) | ((respArcount << 8) & 0xFF00);

        // Append Question Section
        outPacket.insert(outPacket.end(), inPacket + sizeof(DnsHeaderWire), inPacket + offset);

        // Append Answer Section
        for (const auto& a : answers) {
            // Write Name as compression pointer to question (0xC00C)
            outPacket.push_back(0xC0);
            outPacket.push_back(0x0C);

            // Type
            outPacket.push_back(static_cast<uint8_t>((a.type >> 8) & 0xFF));
            outPacket.push_back(static_cast<uint8_t>(a.type & 0xFF));

            // Class (IN)
            outPacket.push_back(0x00);
            outPacket.push_back(0x01);

            // TTL
            outPacket.push_back(static_cast<uint8_t>((a.ttl >> 24) & 0xFF));
            outPacket.push_back(static_cast<uint8_t>((a.ttl >> 16) & 0xFF));
            outPacket.push_back(static_cast<uint8_t>((a.ttl >> 8) & 0xFF));
            outPacket.push_back(static_cast<uint8_t>(a.ttl & 0xFF));

            // RDLENGTH & RDATA
            if (a.type == TYPE_A) {
                outPacket.push_back(0x00);
                outPacket.push_back(0x04);
                // Parse IPv4
                uint32_t b1 = 0, b2 = 0, b3 = 0, b4 = 0;
                char dot = '.';
                std::istringstream iss(a.rdata);
                iss >> b1 >> dot >> b2 >> dot >> b3 >> dot >> b4;
                outPacket.push_back(static_cast<uint8_t>(b1));
                outPacket.push_back(static_cast<uint8_t>(b2));
                outPacket.push_back(static_cast<uint8_t>(b3));
                outPacket.push_back(static_cast<uint8_t>(b4));
            } else {
                // String-based or binary synthetic payload
                uint16_t rdlen = static_cast<uint16_t>(a.rdata.size());
                outPacket.push_back(static_cast<uint8_t>((rdlen >> 8) & 0xFF));
                outPacket.push_back(static_cast<uint8_t>(rdlen & 0xFF));
                outPacket.insert(outPacket.end(), a.rdata.begin(), a.rdata.end());
            }
        }

        // Append EDNS0 OPT record if requested
        if (hasEdns0) {
            outPacket.push_back(0x00); // Root name
            outPacket.push_back(0x00); outPacket.push_back(TYPE_OPT); // Type 41
            outPacket.push_back(static_cast<uint8_t>((ednsPayloadSize >> 8) & 0xFF));
            outPacket.push_back(static_cast<uint8_t>(ednsPayloadSize & 0xFF));
            outPacket.push_back(0x00); // Extended RCODE
            outPacket.push_back(0x00); // EDNS version
            outPacket.push_back(0x80); outPacket.push_back(0x00); // Flags: DO (DNSSEC OK)
            outPacket.push_back(0x00); outPacket.push_back(0x00); // RDLEN = 0
            m_totalDnssecQueries.fetch_add(1, std::memory_order_relaxed);
        }

        return true;
    }

    // ========================================================================
    // Cache Management & Telemetry
    // ========================================================================
    void flushCache() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_cache.clear();
    }

    size_t getCacheSize() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_cache.size();
    }

    uint64_t getTotalQueriesReceived() const { return m_totalQueriesReceived.load(std::memory_order_relaxed); }
    uint64_t getTotalQueriesAnswered() const { return m_totalQueriesAnswered.load(std::memory_order_relaxed); }
    uint64_t getTotalCacheHits() const { return m_totalCacheHits.load(std::memory_order_relaxed); }
    uint64_t getTotalCacheMisses() const { return m_totalCacheMisses.load(std::memory_order_relaxed); }
    uint64_t getTotalDynamicUpdates() const { return m_totalDynamicUpdates.load(std::memory_order_relaxed); }
    uint64_t getTotalZoneTransfers() const { return m_totalZoneTransfers.load(std::memory_order_relaxed); }
    uint64_t getTotalDnssecQueries() const { return m_totalDnssecQueries.load(std::memory_order_relaxed); }

    std::string getServerHost() const { std::lock_guard<std::recursive_mutex> lk(m_mutex); return m_serverHost; }
    std::string getServerIp() const { std::lock_guard<std::recursive_mutex> lk(m_mutex); return m_serverIp; }
    std::string getDomainName() const { std::lock_guard<std::recursive_mutex> lk(m_mutex); return m_domainName; }
};

// ============================================================================
// Clean-Room Win32 C ABI Exports (dnsapi.dll / dns.exe Parity)
// ============================================================================
extern "C" {

inline int32_t MicaDnsInitialize(void** ppEngine) {
    if (!ppEngine) return 0;
    auto& eng = EnterpriseDnsServer::instance();
    eng.initialize();
    *ppEngine = &eng;
    return 1;
}

inline int32_t MicaDnsCreateZone(void* pEngine, const char* szZoneName, uint32_t zoneType, uint32_t bAdIntegrated) {
    if (!pEngine || !szZoneName) return 0;
    auto* eng = static_cast<EnterpriseDnsServer*>(pEngine);
    ZoneType zt = static_cast<ZoneType>(zoneType);
    ReplicationScope sc = bAdIntegrated ? ReplicationScope::ActiveDirectoryDomain : ReplicationScope::LegacyFile;
    return eng->createZone(szZoneName, zt, sc, bAdIntegrated != 0) ? 1 : 0;
}

inline int32_t MicaDnsAddRecord(void* pEngine, const char* szZoneName, const char* szName, uint16_t wType, const char* szData, uint32_t ttl) {
    if (!pEngine || !szZoneName || !szName || !szData) return 0;
    auto* eng = static_cast<EnterpriseDnsServer*>(pEngine);
    DnsResourceRecord rr;
    rr.name = szName;
    rr.type = wType;
    rr.rclass = CLASS_IN;
    rr.ttl = ttl;
    rr.rdata = szData;
    return eng->addRecord(szZoneName, rr) ? 1 : 0;
}

inline int32_t MicaDnsQuery(void* pEngine, const char* szQuestion, uint16_t wType, uint8_t* pPacketOut, uint32_t cbMax, uint32_t* pcbActual) {
    if (!pEngine || !szQuestion) return 0;
    auto* eng = static_cast<EnterpriseDnsServer*>(pEngine);

    // Synthesize simple RFC 1035 wire query
    std::vector<uint8_t> inPkt;
    DnsHeaderWire hdr{};
    hdr.id = 0x5432;
    hdr.flags = 0x0100; // RD = 1
    hdr.qdcount = 0x0100; // 1 in big-endian
    inPkt.resize(sizeof(DnsHeaderWire));
    std::memcpy(inPkt.data(), &hdr, sizeof(hdr));

    // Encode Question Name labels
    std::string q = szQuestion;
    std::stringstream ss(q);
    std::string label;
    while (std::getline(ss, label, '.')) {
        if (!label.empty()) {
            inPkt.push_back(static_cast<uint8_t>(label.size()));
            inPkt.insert(inPkt.end(), label.begin(), label.end());
        }
    }
    inPkt.push_back(0x00); // Root

    // Type & Class
    inPkt.push_back(static_cast<uint8_t>((wType >> 8) & 0xFF));
    inPkt.push_back(static_cast<uint8_t>(wType & 0xFF));
    inPkt.push_back(0x00);
    inPkt.push_back(0x01); // IN

    std::vector<uint8_t> outPkt;
    if (!eng->processWireQuery(inPkt.data(), inPkt.size(), outPkt)) return 0;

    if (pcbActual) *pcbActual = static_cast<uint32_t>(outPkt.size());
    if (pPacketOut && cbMax >= outPkt.size()) {
        std::memcpy(pPacketOut, outPkt.data(), outPkt.size());
    }
    return 1;
}

inline int32_t MicaDnsDynamicUpdate(void* pEngine, const char* szZoneName, const char* szHost, uint16_t wType, const char* szData, uint32_t ttl) {
    if (!pEngine || !szZoneName || !szHost || !szData) return 0;
    auto* eng = static_cast<EnterpriseDnsServer*>(pEngine);
    return eng->processDynamicUpdate(szZoneName, szHost, wType, szData, ttl) ? 1 : 0;
}

inline int32_t MicaDnsZoneTransfer(void* pEngine, const char* szZoneName, uint32_t* pRecordCount) {
    if (!pEngine || !szZoneName) return 0;
    auto* eng = static_cast<EnterpriseDnsServer*>(pEngine);
    std::vector<DnsResourceRecord> rrs;
    if (!eng->performAxfr(szZoneName, rrs)) return 0;
    if (pRecordCount) *pRecordCount = static_cast<uint32_t>(rrs.size());
    return 1;
}

inline int32_t MicaDnsShutdown(void* pEngine) {
    if (!pEngine) return 0;
    return 1;
}

} // extern "C"

} // namespace micant::dns

#endif // MICANT_DNS_SERVER_HPP
