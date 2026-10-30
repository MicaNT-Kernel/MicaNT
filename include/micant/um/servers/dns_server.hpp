#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <span>
#include <optional>
#include <cstdint>
#include <cstring>
#include <atomic>

#include "micant/km/drivers/net/tcpip.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/km/drivers/security/cng.hpp"

namespace micant::dns {

// ============================================================================
// 1. DNS Protocol Constants & Bitfields (RFC 1035, 2136, 2782, 3596, 4034, 6891)
// ============================================================================

inline constexpr uint16_t DNS_PORT = 53;

// Record Types
inline constexpr uint16_t TYPE_A        = 1;   // IPv4 Address
inline constexpr uint16_t TYPE_NS       = 2;   // Authoritative Name Server
inline constexpr uint16_t TYPE_CNAME    = 5;   // Canonical Name
inline constexpr uint16_t TYPE_SOA      = 6;   // Start of Authority
inline constexpr uint16_t TYPE_PTR      = 12;  // Domain Name Pointer
inline constexpr uint16_t TYPE_MX       = 15;  // Mail Exchange
inline constexpr uint16_t TYPE_TXT      = 16;  // Text strings
inline constexpr uint16_t TYPE_AAAA     = 28;  // IPv6 Address
inline constexpr uint16_t TYPE_SRV      = 33;  // Service Locator (RFC 2782)
inline constexpr uint16_t TYPE_OPT      = 41;  // EDNS0 Option (RFC 6891)
inline constexpr uint16_t TYPE_DS       = 43;  // Delegation Signer (RFC 4034)
inline constexpr uint16_t TYPE_RRSIG    = 46;  // DNSSEC Resource Record Signature
inline constexpr uint16_t TYPE_NSEC     = 47;  // DNSSEC Next Secure Record
inline constexpr uint16_t TYPE_DNSKEY   = 48;  // DNSSEC Public Key
inline constexpr uint16_t TYPE_NSEC3    = 50;  // DNSSEC NSEC3 Record
inline constexpr uint16_t TYPE_IXFR     = 251; // Incremental Zone Transfer
inline constexpr uint16_t TYPE_AXFR     = 252; // Full Zone Transfer (RFC 5936)
inline constexpr uint16_t TYPE_ANY      = 255; // Any record

// DNS Classes
inline constexpr uint16_t CLASS_IN      = 1;   // Internet
inline constexpr uint16_t CLASS_ANY     = 255;

// Opcodes
inline constexpr uint8_t OPCODE_QUERY   = 0;
inline constexpr uint8_t OPCODE_IQUERY  = 1;
inline constexpr uint8_t OPCODE_STATUS  = 2;
inline constexpr uint8_t OPCODE_NOTIFY  = 4;
inline constexpr uint8_t OPCODE_UPDATE  = 5;   // Dynamic DNS (RFC 2136)

// Response Codes
inline constexpr uint16_t RCODE_NOERROR  = 0;
inline constexpr uint16_t RCODE_FORMERR  = 1;
inline constexpr uint16_t RCODE_SERVFAIL = 2;
inline constexpr uint16_t RCODE_NXDOMAIN = 3;
inline constexpr uint16_t RCODE_NOTIMP   = 4;
inline constexpr uint16_t RCODE_REFUSED  = 5;
inline constexpr uint16_t RCODE_YXDOMAIN = 6;
inline constexpr uint16_t RCODE_YXRRSET  = 7;
inline constexpr uint16_t RCODE_NXRRSET  = 8;
inline constexpr uint16_t RCODE_NOTAUTH  = 9;
inline constexpr uint16_t RCODE_NOTZONE  = 10;

// Header Flags
inline constexpr uint16_t FLAG_QR = 0x8000; // Query (0) or Response (1)
inline constexpr uint16_t FLAG_AA = 0x0400; // Authoritative Answer
inline constexpr uint16_t FLAG_TC = 0x0200; // TrunCation
inline constexpr uint16_t FLAG_RD = 0x0100; // Recursion Desired
inline constexpr uint16_t FLAG_RA = 0x0080; // Recursion Available
inline constexpr uint16_t FLAG_AD = 0x0020; // Authentic Data (RFC 4035)

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

inline std::string ZoneTypeToString(ZoneType zt) {
    switch (zt) {
        case ZoneType::Primary: return "Primary";
        case ZoneType::Secondary: return "Secondary";
        case ZoneType::Stub: return "Stub";
        case ZoneType::Forwarder: return "Forwarder";
        default: return "Unknown";
    }
}

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

inline uint16_t StringToRecordType(std::string_view typeStr) {
    std::string s(typeStr);
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
        default:             return "RCODE_" + std::to_string(rcode);
    }
}

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

// Structured record helper structures
struct DnsSoaData {
    std::string mname;
    std::string rname;
    uint32_t serial{1};
    uint32_t refresh{3600};
    uint32_t retry{600};
    uint32_t expire{86400};
    uint32_t minimum{300};
};

struct DnsMxData {
    uint16_t preference{10};
    std::string exchange;
};

struct DnsSrvData {
    uint16_t priority{0};
    uint16_t weight{100};
    uint16_t port{389};
    std::string target;
};

struct DnsDnskeyData {
    uint16_t flags{256}; // 256 = ZSK, 257 = KSK
    uint8_t protocol{3};
    uint8_t algorithm{8}; // RSA/SHA-256
    std::vector<uint8_t> publicKey;
    uint16_t keyTag{0};
};

struct DnsRrsigData {
    uint16_t typeCovered{TYPE_A};
    uint8_t algorithm{8};
    uint8_t labels{2};
    uint32_t originalTtl{300};
    uint32_t expirationEpoch{0};
    uint32_t inceptionEpoch{0};
    uint16_t keyTag{0};
    std::string signerName;
    std::vector<uint8_t> signature;
};

// Wire format label serializer
inline void AppendDomainName(std::vector<uint8_t>& buf, std::string_view name) {
    if (name.empty() || name == ".") {
        buf.push_back(0);
        return;
    }

    size_t start = 0;
    while (start < name.size()) {
        size_t dot = name.find('.', start);
        if (dot == std::string_view::npos) dot = name.size();
        size_t len = dot - start;
        if (len > 0) {
            buf.push_back(static_cast<uint8_t>(len));
            for (size_t i = start; i < dot; ++i) {
                buf.push_back(static_cast<uint8_t>(name[i]));
            }
        }
        start = dot + 1;
    }
    buf.push_back(0);
}

// Wire format label deserializer with compression pointer protection
inline bool ReadDomainName(const uint8_t* data, size_t size, size_t& offset, std::string& outName) {
    outName.clear();
    size_t curr = offset;
    bool jumped = false;
    size_t afterJumpOffset = 0;
    size_t jumpsCount = 0;

    while (curr < size) {
        uint8_t len = data[curr];
        if (len == 0) {
            curr++;
            break;
        }

        // Pointer (0xC0)
        if ((len & 0xC0) == 0xC0) {
            if (curr + 1 >= size) return false;
            uint16_t ptrOffset = (static_cast<uint16_t>(len & 0x3F) << 8) | data[curr + 1];
            if (ptrOffset >= size) return false;
            if (!jumped) {
                afterJumpOffset = curr + 2;
                jumped = true;
            }
            curr = ptrOffset;
            jumpsCount++;
            if (jumpsCount > 16) return false;
            continue;
        }

        // Normal label
        curr++;
        if (curr + len > size) return false;
        if (!outName.empty()) outName.push_back('.');
        outName.append(reinterpret_cast<const char*>(data + curr), len);
        curr += len;
    }

    offset = jumped ? afterJumpOffset : curr;
    std::transform(outName.begin(), outName.end(), outName.begin(), ::tolower);
    return true;
}

// ============================================================================
// 2. Structured Resource Records (RFC 1035 / RFC 2136 / RFC 4034)
// ============================================================================

struct DnsResourceRecord {
    std::string name;
    uint16_t type{TYPE_A};
    uint16_t rclass{CLASS_IN};
    uint32_t ttl{3600};
    std::string rdata;
    uint64_t timestamp{0};

    // Horizon 161 extensions
    std::string dataText;
    std::vector<uint8_t> rdataBytes;

    std::optional<DnsSoaData> soaData;
    std::optional<DnsMxData> mxData;
    std::optional<DnsSrvData> srvData;
    std::optional<DnsDnskeyData> dnskeyData;
    std::optional<DnsRrsigData> rrsigData;

    DnsResourceRecord() = default;
    DnsResourceRecord(std::string n, uint16_t t, uint16_t c, uint32_t ttlSec, std::string d, uint64_t ts = 0)
        : name(std::move(n)), type(t), rclass(c), ttl(ttlSec), rdata(d), timestamp(ts), dataText(d) {
        syncRdataBytes();
    }

    void syncRdataBytes() {
        if (dataText.empty() && !rdata.empty()) dataText = rdata;
        if (rdata.empty() && !dataText.empty()) rdata = dataText;

        if (type == TYPE_A) {
            uint8_t b[4]{0};
            if (std::sscanf(rdata.c_str(), "%hhu.%hhu.%hhu.%hhu", &b[0], &b[1], &b[2], &b[3]) == 4) {
                rdataBytes.assign(b, b + 4);
            }
        } else if (type == TYPE_PTR || type == TYPE_CNAME || type == TYPE_NS) {
            rdataBytes.clear();
            AppendDomainName(rdataBytes, rdata);
        }
    }
};

// ============================================================================
// 3. DNS Zone Container (Authoritative Primary / Secondary)
// ============================================================================

class DnsZone {
public:
    std::string zoneName;
    ZoneType type{ZoneType::Primary};
    ZoneType zoneType{ZoneType::Primary};
    ReplicationScope replication{ReplicationScope::ActiveDirectoryDomain};
    bool isAdIntegrated{true};
    bool secureDynamicUpdateOnly{false};
    bool isReverse{false};
    bool isLoaded{true};
    bool isDnssecSigned{false};
    uint32_t soaSerial{1};
    std::string primaryNs{"ns1.micant.local"};
    std::string respPerson{"hostmaster.micant.local"};
    uint32_t refresh{900};
    uint32_t retry{600};
    uint32_t expire{86400};
    uint32_t minimumTtl{300};
    DnsSoaData soa;
    std::vector<DnsResourceRecord> records;
    mutable std::recursive_mutex mutex;

    DnsZone() = default;

    DnsZone(const DnsZone& o) {
        std::lock_guard<std::recursive_mutex> lock(o.mutex);
        zoneName = o.zoneName;
        type = o.type;
        zoneType = o.zoneType;
        replication = o.replication;
        isAdIntegrated = o.isAdIntegrated;
        secureDynamicUpdateOnly = o.secureDynamicUpdateOnly;
        isReverse = o.isReverse;
        isLoaded = o.isLoaded;
        isDnssecSigned = o.isDnssecSigned;
        soaSerial = o.soaSerial;
        primaryNs = o.primaryNs;
        respPerson = o.respPerson;
        refresh = o.refresh;
        retry = o.retry;
        expire = o.expire;
        minimumTtl = o.minimumTtl;
        soa = o.soa;
        records = o.records;
    }

    DnsZone& operator=(const DnsZone& o) {
        if (this != &o) {
            std::scoped_lock lock(mutex, o.mutex);
            zoneName = o.zoneName;
            type = o.type;
            zoneType = o.zoneType;
            replication = o.replication;
            isAdIntegrated = o.isAdIntegrated;
            secureDynamicUpdateOnly = o.secureDynamicUpdateOnly;
            isReverse = o.isReverse;
            isLoaded = o.isLoaded;
            isDnssecSigned = o.isDnssecSigned;
            soaSerial = o.soaSerial;
            primaryNs = o.primaryNs;
            respPerson = o.respPerson;
            refresh = o.refresh;
            retry = o.retry;
            expire = o.expire;
            minimumTtl = o.minimumTtl;
            soa = o.soa;
            records = o.records;
        }
        return *this;
    }

    DnsZone(std::string name, ZoneType zt = ZoneType::Primary)
        : zoneName(std::move(name)), type(zt), zoneType(zt) {
        std::transform(zoneName.begin(), zoneName.end(), zoneName.begin(), ::tolower);
        isReverse = zoneName.ends_with(".in-addr.arpa") || zoneName.ends_with(".ip6.arpa");
        primaryNs = "ns1." + zoneName;
        respPerson = "hostmaster." + zoneName;

        soa.mname = primaryNs;
        soa.rname = respPerson;
        soa.serial = 2026103001;
        soa.refresh = refresh;
        soa.retry = retry;
        soa.expire = expire;
        soa.minimum = minimumTtl;
        soaSerial = soa.serial;

        // Add initial SOA record
        DnsResourceRecord soaRec;
        soaRec.name = zoneName;
        soaRec.type = TYPE_SOA;
        soaRec.rclass = CLASS_IN;
        soaRec.ttl = soa.minimum;
        soaRec.rdata = soa.mname + " " + soa.rname + " " + std::to_string(soa.serial);
        soaRec.dataText = soaRec.rdata;
        soaRec.soaData = soa;
        records.push_back(soaRec);

        // Add initial NS record
        DnsResourceRecord nsRec;
        nsRec.name = zoneName;
        nsRec.type = TYPE_NS;
        nsRec.rclass = CLASS_IN;
        nsRec.ttl = 3600;
        nsRec.rdata = soa.mname;
        nsRec.dataText = nsRec.rdata;
        records.push_back(nsRec);
    }

    bool addRecord(const DnsResourceRecord& rr) {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        DnsResourceRecord rec = rr;
        rec.syncRdataBytes();
        if (rec.timestamp == 0) {
            rec.timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
        records.push_back(rec);
        soa.serial++;
        soaSerial = soa.serial;
        return true;
    }

    bool deleteRecord(const std::string& name, uint16_t rType, const std::string& dataText = "") {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        std::string target = name;
        std::transform(target.begin(), target.end(), target.begin(), ::tolower);
        size_t initialSize = records.size();

        records.erase(std::remove_if(records.begin(), records.end(), [&](const DnsResourceRecord& r) {
            std::string rn = r.name;
            std::transform(rn.begin(), rn.end(), rn.begin(), ::tolower);
            bool matchName = (rn == target);
            bool matchType = (rType == TYPE_ANY || r.type == rType);
            bool matchData = (dataText.empty() || r.rdata == dataText || r.dataText == dataText);
            return matchName && matchType && matchData;
        }), records.end());

        if (records.size() != initialSize) {
            soa.serial++;
            soaSerial = soa.serial;
            return true;
        }
        return false;
    }

    std::vector<DnsResourceRecord> findRecords(const std::string& name, uint16_t rType) const {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        std::string target = name;
        std::transform(target.begin(), target.end(), target.begin(), ::tolower);
        std::vector<DnsResourceRecord> res;
        for (const auto& r : records) {
            std::string rn = r.name;
            std::transform(rn.begin(), rn.end(), rn.begin(), ::tolower);
            if (rn == target && (rType == TYPE_ANY || r.type == rType)) {
                res.push_back(r);
            }
        }
        return res;
    }

    std::vector<DnsResourceRecord> getAllRecords() const {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        return records;
    }
};

// ============================================================================
// 4. In-Memory Recursive Resolver Cache (RFC 1034 / RFC 2308 Negative Caching)
// ============================================================================

struct DnsCacheEntry {
    std::string name;
    uint16_t type{TYPE_A};
    std::vector<DnsResourceRecord> records;
    std::vector<DnsResourceRecord> answers;
    uint64_t expiryTime{0};
    uint64_t expirationEpochMs{0};
    bool isNegative{false};
    uint16_t negativeRcode{RCODE_NOERROR};
    uint8_t rcode{RCODE_NOERROR};
};

class DnsServerCache {
public:
    void put(const std::string& name, uint16_t type, const std::vector<DnsResourceRecord>& rrs, uint32_t ttlSeconds) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        std::string key = makeKey(name, type);
        DnsCacheEntry e;
        e.name = name;
        e.type = type;
        e.records = rrs;
        e.answers = rrs;
        e.isNegative = false;
        e.rcode = RCODE_NOERROR;
        e.negativeRcode = RCODE_NOERROR;
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        e.expirationEpochMs = static_cast<uint64_t>(nowMs) + (ttlSeconds * 1000ULL);
        e.expiryTime = static_cast<uint64_t>(nowMs / 1000ULL) + ttlSeconds;
        cache_[key] = e;
    }

    void putNegative(const std::string& name, uint16_t type, uint8_t rcode, uint32_t ttlSeconds) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        std::string key = makeKey(name, type);
        DnsCacheEntry e;
        e.name = name;
        e.type = type;
        e.isNegative = true;
        e.rcode = rcode;
        e.negativeRcode = rcode;
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        e.expirationEpochMs = static_cast<uint64_t>(nowMs) + (ttlSeconds * 1000ULL);
        e.expiryTime = static_cast<uint64_t>(nowMs / 1000ULL) + ttlSeconds;
        cache_[key] = e;
    }

    std::optional<DnsCacheEntry> get(const std::string& name, uint16_t type) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        std::string key = makeKey(name, type);
        auto it = cache_.find(key);
        if (it == cache_.end()) return std::nullopt;

        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (static_cast<uint64_t>(nowMs) > it->second.expirationEpochMs && it->second.expirationEpochMs > 0) {
            cache_.erase(it);
            return std::nullopt;
        }
        return it->second;
    }

    void erase(const std::string& key) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        cache_.erase(key);
    }

    void clear() {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        cache_.clear();
    }

    size_t size() const {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        return cache_.size();
    }

private:
    static std::string makeKey(std::string_view name, uint16_t type) {
        std::string k(name);
        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
        k += "/" + std::to_string(type);
        return k;
    }

    mutable std::recursive_mutex mutex_;
    std::unordered_map<std::string, DnsCacheEntry> cache_;
};

// ============================================================================
// 5. Sovereign DNS Server Engine (dns.exe / dnsserver.dll)
// ============================================================================

class EnterpriseDnsServer {
public:
    static EnterpriseDnsServer& instance() noexcept {
        static EnterpriseDnsServer s_inst;
        return s_inst;
    }

    static EnterpriseDnsServer& Instance() noexcept {
        return instance();
    }

    bool initialize() noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (initialized_) return true;

        seedDefaultZones();
        registerScmServices();
        registerVersionDatabase();

        initialized_ = true;
        return true;
    }

    // --- SCM & VersionDatabase ---

    void registerScmServices() noexcept {
        auto& scm = micant::scm::ServiceControlManager::get();

        auto recDns = std::make_shared<micant::scm::ServiceRecord>();
        recDns->serviceName = L"DNS";
        recDns->displayName = L"DNS Server";
        recDns->serviceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
        recDns->startType = micant::scm::SERVICE_AUTO_START;
        recDns->errorControl = micant::scm::SERVICE_ERROR_NORMAL;
        recDns->svchostGroup = "DNSGroup";
        recDns->binaryPath = L"C:\\Windows\\System32\\dns.exe";
        recDns->status.dwServiceType = micant::scm::SERVICE_WIN32_OWN_PROCESS;
        recDns->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
        recDns->status.dwControlsAccepted = micant::scm::SERVICE_ACCEPT_STOP | micant::scm::SERVICE_ACCEPT_SHUTDOWN | micant::scm::SERVICE_ACCEPT_PAUSE_CONTINUE;
        scm.registerServiceRecord(recDns);
    }

    void registerVersionDatabase() noexcept {
        auto& db = micant::version::VersionDatabase::Instance();
        db.RegisterModule("dns.exe", "10.0.26100.1", "Windows Enterprise DNS Server Service");
        db.RegisterModule("dnsapi.dll", "10.0.26100.1", "DNS Client and Resolver API");
        db.RegisterModule("dnslib.dll", "10.0.26100.1", "DNS Common Protocol Library");
        db.RegisterModule("dnscache.dll", "10.0.26100.1", "DNS Client Resolver Cache Service");
        db.RegisterModule("dnscmd.exe", "10.0.26100.1", "DNS Server Command-Line Management Tool");
        db.RegisterModule("dnsserver.dll", "10.0.26100.1", "DNS Server Core Engine");
    }

    // --- Zone Management ---

    void seedDefaultZones() noexcept {
        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        // 1. titan.local (Phase 181 / Milestone 208 Enterprise Zone)
        auto zTitan = std::make_shared<DnsZone>("titan.local", ZoneType::Primary);
        zTitan->records.clear();
        zTitan->replication = ReplicationScope::ActiveDirectoryDomain;
        zTitan->isAdIntegrated = true;
        zTitan->secureDynamicUpdateOnly = true;
        zTitan->soa.serial = 100;
        zTitan->soaSerial = 100;
        zTitan->primaryNs = "dc01.titan.local";
        zTitan->respPerson = "hostmaster.titan.local";

        zTitan->records.push_back({ "titan.local", TYPE_SOA, CLASS_IN, 3600, "dc01.titan.local hostmaster.titan.local 100 900 600 86400 3600", now });
        zTitan->records.push_back({ "titan.local", TYPE_NS, CLASS_IN, 3600, "dc01.titan.local", now });
        zTitan->records.push_back({ "titan.local", TYPE_A, CLASS_IN, 600, "192.168.1.10", now });
        zTitan->records.push_back({ "titan.local", TYPE_A, CLASS_IN, 600, "192.168.1.11", now });
        zTitan->records.push_back({ "dc01.titan.local", TYPE_A, CLASS_IN, 3600, "192.168.1.10", now });
        zTitan->records.push_back({ "dc01.titan.local", TYPE_AAAA, CLASS_IN, 3600, "2001:db8::10", now });
        zTitan->records.push_back({ "dc02.titan.local", TYPE_A, CLASS_IN, 3600, "192.168.1.11", now });
        zTitan->records.push_back({ "titan-wds.titan.local", TYPE_A, CLASS_IN, 3600, "192.168.1.50", now });
        zTitan->records.push_back({ "titan-ca.titan.local", TYPE_A, CLASS_IN, 3600, "192.168.1.60", now });
        zTitan->records.push_back({ "mail.titan.local", TYPE_A, CLASS_IN, 3600, "192.168.1.25", now });
        zTitan->records.push_back({ "pki.titan.local", TYPE_CNAME, CLASS_IN, 3600, "titan-ca.titan.local", now });
        zTitan->records.push_back({ "pxe.titan.local", TYPE_CNAME, CLASS_IN, 3600, "titan-wds.titan.local", now });
        zTitan->records.push_back({ "_ldap._tcp.titan.local", TYPE_SRV, CLASS_IN, 600, "0 100 389 dc01.titan.local", now });
        zTitan->records.push_back({ "_kerberos._tcp.titan.local", TYPE_SRV, CLASS_IN, 600, "0 100 88 dc01.titan.local", now });
        zTitan->records.push_back({ "_kpasswd._tcp.titan.local", TYPE_SRV, CLASS_IN, 600, "0 100 464 dc01.titan.local", now });
        zTitan->records.push_back({ "_gc._tcp.titan.local", TYPE_SRV, CLASS_IN, 600, "0 100 3268 dc01.titan.local", now });
        zTitan->records.push_back({ "titan.local", TYPE_MX, CLASS_IN, 3600, "10 mail.titan.local", now });
        zTitan->records.push_back({ "titan.local", TYPE_TXT, CLASS_IN, 3600, "\"v=spf1 mx ip4:192.168.1.25 ~all\"", now });
        zones_["titan.local"] = zTitan;

        // 2. _msdcs.titan.local
        auto zMsdcs = std::make_shared<DnsZone>("_msdcs.titan.local", ZoneType::Primary);
        zMsdcs->records.clear();
        zMsdcs->replication = ReplicationScope::ActiveDirectoryForest;
        zMsdcs->isAdIntegrated = true;
        zMsdcs->secureDynamicUpdateOnly = true;
        zMsdcs->soa.serial = 100;
        zMsdcs->soaSerial = 100;
        zMsdcs->primaryNs = "dc01.titan.local";
        zMsdcs->respPerson = "hostmaster.titan.local";
        zMsdcs->records.push_back({ "_msdcs.titan.local", TYPE_SOA, CLASS_IN, 3600, "dc01.titan.local hostmaster.titan.local 100 900 600 86400 3600", now });
        zMsdcs->records.push_back({ "_msdcs.titan.local", TYPE_NS, CLASS_IN, 3600, "dc01.titan.local", now });
        zMsdcs->records.push_back({ "_ldap._tcp.dc._msdcs.titan.local", TYPE_SRV, CLASS_IN, 600, "0 100 389 dc01.titan.local", now });
        zMsdcs->records.push_back({ "_kerberos._tcp.dc._msdcs.titan.local", TYPE_SRV, CLASS_IN, 600, "0 100 88 dc01.titan.local", now });
        zMsdcs->records.push_back({ "c5b1c552-3a87-4d7a-8b1e-08992e541a77._msdcs.titan.local", TYPE_CNAME, CLASS_IN, 600, "dc01.titan.local", now });
        zones_["_msdcs.titan.local"] = zMsdcs;

        // 3. 1.168.192.in-addr.arpa
        auto zRev = std::make_shared<DnsZone>("1.168.192.in-addr.arpa", ZoneType::Primary);
        zRev->records.clear();
        zRev->replication = ReplicationScope::ActiveDirectoryDomain;
        zRev->isAdIntegrated = true;
        zRev->soa.serial = 100;
        zRev->soaSerial = 100;
        zRev->primaryNs = "dc01.titan.local";
        zRev->respPerson = "hostmaster.titan.local";
        zRev->records.push_back({ "1.168.192.in-addr.arpa", TYPE_SOA, CLASS_IN, 3600, "dc01.titan.local hostmaster.titan.local 100 900 600 86400 3600", now });
        zRev->records.push_back({ "1.168.192.in-addr.arpa", TYPE_NS, CLASS_IN, 3600, "dc01.titan.local", now });
        zRev->records.push_back({ "10.1.168.192.in-addr.arpa", TYPE_PTR, CLASS_IN, 3600, "dc01.titan.local", now });
        zRev->records.push_back({ "11.1.168.192.in-addr.arpa", TYPE_PTR, CLASS_IN, 3600, "dc02.titan.local", now });
        zRev->records.push_back({ "50.1.168.192.in-addr.arpa", TYPE_PTR, CLASS_IN, 3600, "titan-wds.titan.local", now });
        zRev->records.push_back({ "60.1.168.192.in-addr.arpa", TYPE_PTR, CLASS_IN, 3600, "titan-ca.titan.local", now });
        zones_["1.168.192.in-addr.arpa"] = zRev;

        // 4. micant.local (Horizon 161 Sovereign Enterprise Zone @ 147.135.1.229)
        auto zMica = std::make_shared<DnsZone>("micant.local", ZoneType::Primary);
        DnsResourceRecord dcRec("dc1.micant.local", TYPE_A, CLASS_IN, 3600, "147.135.1.229", now);
        zMica->addRecord(dcRec);

        DnsResourceRecord apexA("micant.local", TYPE_A, CLASS_IN, 3600, "147.135.1.229", now);
        zMica->addRecord(apexA);

        DnsResourceRecord adSrv("_ldap._tcp.dc._msdcs.micant.local", TYPE_SRV, CLASS_IN, 600, "0 100 389 dc1.micant.local", now);
        adSrv.srvData = DnsSrvData{ 0, 100, 389, "dc1.micant.local" };
        zMica->addRecord(adSrv);

        DnsResourceRecord krbSrv("_kerberos._tcp.dc._msdcs.micant.local", TYPE_SRV, CLASS_IN, 600, "0 100 88 dc1.micant.local", now);
        krbSrv.srvData = DnsSrvData{ 0, 100, 88, "dc1.micant.local" };
        zMica->addRecord(krbSrv);

        DnsResourceRecord keyRec("micant.local", TYPE_DNSKEY, CLASS_IN, 3600, "256 3 8 AwEAAad... (ZSK)", now);
        DnsDnskeyData keyData{};
        keyData.flags = 256;
        keyData.protocol = 3;
        keyData.algorithm = 8;
        keyData.keyTag = 12345;
        keyData.publicKey = { 0x01, 0x00, 0x01, 0xAA, 0xBB, 0xCC };
        keyRec.dnskeyData = keyData;
        zMica->addRecord(keyRec);

        DnsResourceRecord rrsigRec("micant.local", TYPE_RRSIG, CLASS_IN, 3600, "A 8 2 3600 20261130000000 20261030000000 12345 micant.local. sig...", now);
        DnsRrsigData sigData{};
        sigData.typeCovered = TYPE_A;
        sigData.algorithm = 8;
        sigData.labels = 2;
        sigData.originalTtl = 3600;
        sigData.expirationEpoch = 1795000000;
        sigData.inceptionEpoch = 1761000000;
        sigData.keyTag = 12345;
        sigData.signerName = "micant.local";
        sigData.signature = { 0xDE, 0xAD, 0xBE, 0xEF };
        rrsigRec.rrsigData = sigData;
        zMica->addRecord(rrsigRec);

        zones_["micant.local"] = zMica;

        // 5. 1.135.147.in-addr.arpa (Horizon 161 Reverse Zone)
        auto zMicaRev = std::make_shared<DnsZone>("1.135.147.in-addr.arpa", ZoneType::Primary);
        DnsResourceRecord ptrRec("229.1.135.147.in-addr.arpa", TYPE_PTR, CLASS_IN, 3600, "dc1.micant.local", now);
        zMicaRev->addRecord(ptrRec);
        zones_["1.135.147.in-addr.arpa"] = zMicaRev;

        // 6. Root Hints ([a-m].root-servers.net)
        rootHints_["a.root-servers.net"] = "198.41.0.4";
        rootHints_["b.root-servers.net"] = "199.9.14.201";
        rootHints_["c.root-servers.net"] = "192.33.4.12";
        rootHints_["d.root-servers.net"] = "199.7.91.13";
        rootHints_["e.root-servers.net"] = "192.203.230.10";
        rootHints_["f.root-servers.net"] = "192.5.5.241";
        rootHints_["g.root-servers.net"] = "192.112.36.4";
        rootHints_["h.root-servers.net"] = "198.97.190.53";
        rootHints_["i.root-servers.net"] = "192.36.148.17";
        rootHints_["j.root-servers.net"] = "192.58.128.30";
        rootHints_["k.root-servers.net"] = "193.0.14.129";
        rootHints_["l.root-servers.net"] = "199.7.83.42";
        rootHints_["m.root-servers.net"] = "202.12.27.33";
    }

    bool createZone(const std::string& zoneName, ZoneType zType, ReplicationScope scope, bool isAdIntegrated) noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        std::string key = zoneName;
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        if (zones_.find(key) != zones_.end()) return false;

        auto z = std::make_shared<DnsZone>(zoneName, zType);
        z->replication = scope;
        z->isAdIntegrated = isAdIntegrated;
        zones_[key] = z;
        return true;
    }

    bool addZone(const std::string& name, ZoneType type = ZoneType::Primary) noexcept {
        return createZone(name, type, ReplicationScope::LegacyFile, false);
    }

    bool deleteZone(const std::string& name) noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        std::string key = name;
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        auto it = zones_.find(key);
        if (it == zones_.end()) return false;
        zones_.erase(it);
        return true;
    }

    std::shared_ptr<DnsZone> getZone(const std::string& name) const noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        std::string key = name;
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        auto it = zones_.find(key);
        if (it == zones_.end()) return nullptr;
        return it->second;
    }

    bool getZone(const std::string& name, DnsZone& outZone) const noexcept {
        auto z = getZone(name);
        if (!z) return false;
        outZone = *z;
        return true;
    }

    std::vector<DnsZone> getAllZones() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        std::vector<DnsZone> res;
        res.reserve(zones_.size());
        for (const auto& [_, z] : zones_) res.push_back(*z);
        return res;
    }

    // --- Record Operations ---

    bool addRecord(const std::string& zoneName, const DnsResourceRecord& rr) noexcept {
        auto z = getZone(zoneName);
        if (!z) return false;
        return z->addRecord(rr);
    }

    bool deleteRecord(const std::string& zoneName, const std::string& name, uint16_t type) noexcept {
        auto z = getZone(zoneName);
        if (!z) return false;
        return z->deleteRecord(name, type);
    }

    // --- High-Level Query Engine ---

    std::vector<DnsResourceRecord> queryRecords(const std::string& fqdn, uint16_t type, bool* pAuthoritative = nullptr, uint16_t* pRcode = nullptr, int depth = 0) noexcept {
        if (depth > 8) {
            if (pRcode) *pRcode = RCODE_SERVFAIL;
            return {};
        }
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        totalQueriesReceived_++;

        std::string qName = fqdn;
        std::transform(qName.begin(), qName.end(), qName.begin(), ::tolower);
        if (!qName.empty() && qName.back() == '.') qName.pop_back();

        if (pAuthoritative) *pAuthoritative = false;
        if (pRcode) *pRcode = RCODE_NOERROR;

        auto cached = cache_.get(qName, type);
        if (cached.has_value()) {
            cacheHits_++;
            totalQueriesAnswered_++;
            if (cached->isNegative) {
                if (pRcode) *pRcode = cached->negativeRcode;
                return {};
            }
            return cached->records;
        }
        cacheMisses_++;

        std::shared_ptr<DnsZone> bestZone = nullptr;
        size_t bestMatchLen = 0;
        for (const auto& [zKey, z] : zones_) {
            if (qName == zKey || (qName.size() > zKey.size() && qName.compare(qName.size() - zKey.size() - 1, zKey.size() + 1, "." + zKey) == 0)) {
                if (zKey.size() > bestMatchLen) {
                    bestMatchLen = zKey.size();
                    bestZone = z;
                }
            }
        }

        if (bestZone) {
            if (pAuthoritative) *pAuthoritative = true;
            auto matches = bestZone->findRecords(qName, type);

            // CNAME resolution chain
            if (matches.empty() && type != TYPE_CNAME) {
                auto cnames = bestZone->findRecords(qName, TYPE_CNAME);
                if (!cnames.empty()) {
                    matches.push_back(cnames[0]);
                    auto targetMatches = queryRecords(cnames[0].rdata, type, nullptr, nullptr, depth + 1);
                    matches.insert(matches.end(), targetMatches.begin(), targetMatches.end());
                }
            }

            if (matches.empty()) {
                if (pRcode) *pRcode = RCODE_NXDOMAIN;
                cache_.putNegative(qName, type, RCODE_NXDOMAIN, 300);
                return {};
            }

            cache_.put(qName, type, matches, 600);
            totalQueriesAnswered_++;
            return matches;
        }

        // Non-authoritative: check root hints
        if (rootHints_.find(qName) != rootHints_.end()) {
            DnsResourceRecord r(qName, TYPE_A, CLASS_IN, 86400, rootHints_.at(qName));
            std::vector<DnsResourceRecord> res{r};
            cache_.put(qName, type, res, 86400);
            totalQueriesAnswered_++;
            return res;
        }

        if (pRcode) *pRcode = RCODE_NXDOMAIN;
        cache_.putNegative(qName, type, RCODE_NXDOMAIN, 300);
        return {};
    }

    // --- Dynamic DNS (RFC 2136) Engine ---

    bool processDynamicUpdate(const std::string& zoneName, const std::string& hostName, uint16_t type, const std::string& data, uint32_t ttl = 300) noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        totalDynamicUpdates_++;

        auto zone = getZone(zoneName);
        if (!zone) return false;

        std::string fqdn = hostName;
        if (fqdn.find('.') == std::string::npos) {
            fqdn += "." + zone->zoneName;
        }

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        bool updated = false;
        for (auto& r : zone->records) {
            std::string rn = r.name;
            std::transform(rn.begin(), rn.end(), rn.begin(), ::tolower);
            std::string fn = fqdn;
            std::transform(fn.begin(), fn.end(), fn.begin(), ::tolower);
            if (rn == fn && r.type == type) {
                r.rdata = data;
                r.dataText = data;
                r.ttl = ttl;
                r.timestamp = now;
                r.syncRdataBytes();
                updated = true;
                break;
            }
        }

        if (!updated) {
            DnsResourceRecord rec(fqdn, type, CLASS_IN, ttl, data, now);
            zone->records.push_back(rec);
        }

        zone->soa.serial++;
        zone->soaSerial = zone->soa.serial;
        cache_.clear();
        return true;
    }

    bool processDynamicDnsUpdate(const std::string& zoneName,
                                 const std::string& recordName,
                                 uint16_t type,
                                 const std::string& data,
                                 uint32_t ttl = 300,
                                 bool isDelete = false) noexcept {
        if (isDelete) {
            auto z = getZone(zoneName);
            if (!z) return false;
            totalDynamicUpdates_++;
            return z->deleteRecord(recordName, type, data);
        }
        return processDynamicUpdate(zoneName, recordName, type, data, ttl);
    }

    // --- DNSSEC Zone Signing & Verification ---

    bool signZoneDnssec(const std::string& zoneName) noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto zone = getZone(zoneName);
        if (!zone) return false;

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        std::string dnskeyData = "256 3 8 AwEAAc0N1G1N8...SyntheticZSKPublicKey...";
        zone->records.push_back({ zone->zoneName, TYPE_DNSKEY, CLASS_IN, 86400, dnskeyData, now });

        std::string kskData = "257 3 8 AwEAAd7T2P4A9...SyntheticKSKPublicKey...";
        zone->records.push_back({ zone->zoneName, TYPE_DNSKEY, CLASS_IN, 86400, kskData, now });

        std::string rrsigData = "SOA 8 2 3600 " + std::to_string(now + 2592000) + " " + std::to_string(now) + " 12345 " + zone->zoneName + ". SyntheticRrsigSignatureBlob...";
        zone->records.push_back({ zone->zoneName, TYPE_RRSIG, CLASS_IN, 3600, rrsigData, now });

        std::string nsecData = "z." + zone->zoneName + ". A NS SOA MX TXT RRSIG NSEC DNSKEY";
        zone->records.push_back({ zone->zoneName, TYPE_NSEC, CLASS_IN, 3600, nsecData, now });

        zone->isDnssecSigned = true;
        zone->soa.serial++;
        zone->soaSerial = zone->soa.serial;
        return true;
    }

    bool validateDnssecRecord(const DnsResourceRecord& record, const DnsResourceRecord& rrsig, const DnsResourceRecord& dnskey) const noexcept {
        if (!rrsig.rrsigData.has_value() || !dnskey.dnskeyData.has_value()) return false;
        if (rrsig.rrsigData->typeCovered != record.type) return false;
        if (rrsig.rrsigData->keyTag != dnskey.dnskeyData->keyTag) return false;

        auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (static_cast<uint64_t>(now) > rrsig.rrsigData->expirationEpoch) return false;

        return !rrsig.rrsigData->signature.empty();
    }

    // --- Zone Transfer (AXFR RFC 5936) ---

    bool performAxfr(const std::string& zoneName, std::vector<DnsResourceRecord>& outRecords) const noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        totalZoneTransfers_++;

        auto zone = getZone(zoneName);
        if (!zone) return false;

        outRecords.clear();
        DnsResourceRecord soaRec;
        bool foundSoa = false;
        for (const auto& r : zone->records) {
            if (r.type == TYPE_SOA) {
                soaRec = r;
                foundSoa = true;
                break;
            }
        }

        if (foundSoa) outRecords.push_back(soaRec);

        for (const auto& r : zone->records) {
            if (r.type != TYPE_SOA) {
                outRecords.push_back(r);
            }
        }

        if (foundSoa) outRecords.push_back(soaRec);
        return true;
    }

    bool generateAxfrStream(const std::string& zoneName, std::vector<DnsResourceRecord>& outRecords) const noexcept {
        return performAxfr(zoneName, outRecords);
    }

    // --- RFC 1035 / EDNS0 Wire Query Processing ---

    bool processWireQuery(const uint8_t* inPacket, size_t inLen, std::vector<uint8_t>& outPacket) noexcept {
        if (!inPacket || inLen < sizeof(DnsHeaderWire)) return false;
        totalQueriesReceived_++;

        const auto* inHdr = reinterpret_cast<const DnsHeaderWire*>(inPacket);
        uint16_t id = inHdr->id;
        uint16_t flags = tcpip::ntohs(inHdr->flags);
        uint8_t opcode = (flags >> 11) & 0x0F;
        uint16_t qdcount = tcpip::ntohs(inHdr->qdcount);
        uint16_t arcount = tcpip::ntohs(inHdr->arcount);

        // Dynamic DNS Opcode 5 response
        if (opcode == OPCODE_UPDATE) {
            outPacket.clear();
            outPacket.resize(sizeof(DnsHeaderWire));
            auto* respHdr = reinterpret_cast<DnsHeaderWire*>(outPacket.data());
            respHdr->id = id;
            uint16_t rflags = FLAG_QR | (OPCODE_UPDATE << 11) | RCODE_NOERROR;
            respHdr->flags = tcpip::htons(rflags);
            respHdr->qdcount = 0;
            respHdr->ancount = 0;
            respHdr->nscount = 0;
            respHdr->arcount = 0;
            return true;
        }

        if (qdcount == 0) return false;

        size_t offset = sizeof(DnsHeaderWire);
        std::string qname;
        if (!ReadDomainName(inPacket, inLen, offset, qname)) return false;
        if (offset + 4 > inLen) return false;

        uint16_t qtype = (static_cast<uint16_t>(inPacket[offset]) << 8) | inPacket[offset + 1];
        uint16_t qclass = (static_cast<uint16_t>(inPacket[offset + 2]) << 8) | inPacket[offset + 3];
        offset += 4;

        // EDNS0 OPT RR detection
        bool hasEdns0 = false;
        uint16_t ednsPayloadSize = 512;
        if (arcount > 0 && offset < inLen) {
            for (size_t p = offset; p + 10 <= inLen; ++p) {
                if (inPacket[p] == 0 && inPacket[p + 1] == 0 && inPacket[p + 2] == 41) {
                    hasEdns0 = true;
                    ednsPayloadSize = (static_cast<uint16_t>(inPacket[p + 3]) << 8) | inPacket[p + 4];
                    if (ednsPayloadSize < 512) ednsPayloadSize = 4096;
                    break;
                }
            }
        }

        bool isAuth = false;
        uint16_t rcode = RCODE_NOERROR;
        auto answers = queryRecords(qname, qtype, &isAuth, &rcode);

        // Check DNSSEC RRSIG inclusion & AD bit assertion
        bool isDnssecValid = false;
        std::shared_ptr<DnsZone> bZone = nullptr;
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            for (const auto& [zname, zptr] : zones_) {
                if (qname == zname || qname.ends_with("." + zname)) {
                    if (!bZone || zname.size() > bZone->zoneName.size()) bZone = zptr;
                }
            }
        }

        if (bZone && !answers.empty()) {
            auto sigs = bZone->findRecords(qname, TYPE_RRSIG);
            auto keys = bZone->findRecords(bZone->zoneName, TYPE_DNSKEY);
            if (!sigs.empty() && !keys.empty()) {
                for (const auto& s : sigs) {
                    if (s.rrsigData && s.rrsigData->typeCovered == qtype) {
                        answers.push_back(s);
                        isDnssecValid = validateDnssecRecord(answers[0], s, keys[0]);
                    }
                }
            }
        }

        // Build Response Packet
        outPacket.clear();
        outPacket.resize(sizeof(DnsHeaderWire));
        auto* outHdr = reinterpret_cast<DnsHeaderWire*>(outPacket.data());
        outHdr->id = id;

        uint16_t outFlags = FLAG_QR | FLAG_RA;
        if (isAuth) outFlags |= FLAG_AA;
        if (isDnssecValid) outFlags |= FLAG_AD;
        outFlags |= (rcode & 0x0F);
        outHdr->flags = tcpip::htons(outFlags);

        outHdr->qdcount = tcpip::htons(1);
        outHdr->ancount = tcpip::htons(static_cast<uint16_t>(answers.size()));
        outHdr->nscount = 0;
        uint16_t respArcount = hasEdns0 ? 1 : 0;
        outHdr->arcount = tcpip::htons(respArcount);

        // Question section echo
        AppendDomainName(outPacket, qname);
        outPacket.push_back(static_cast<uint8_t>(qtype >> 8));
        outPacket.push_back(static_cast<uint8_t>(qtype & 0xFF));
        outPacket.push_back(static_cast<uint8_t>(qclass >> 8));
        outPacket.push_back(static_cast<uint8_t>(qclass & 0xFF));

        // Answer section
        for (auto& a : answers) {
            a.syncRdataBytes();
            AppendDomainName(outPacket, a.name);
            outPacket.push_back(static_cast<uint8_t>(a.type >> 8));
            outPacket.push_back(static_cast<uint8_t>(a.type & 0xFF));
            outPacket.push_back(static_cast<uint8_t>(a.rclass >> 8));
            outPacket.push_back(static_cast<uint8_t>(a.rclass & 0xFF));

            uint32_t netTtl = tcpip::htonl(a.ttl);
            const auto* ttlPtr = reinterpret_cast<const uint8_t*>(&netTtl);
            outPacket.insert(outPacket.end(), ttlPtr, ttlPtr + 4);

            if (a.type == TYPE_A && a.rdataBytes.size() == 4) {
                outPacket.push_back(0x00);
                outPacket.push_back(0x04);
                outPacket.insert(outPacket.end(), a.rdataBytes.begin(), a.rdataBytes.end());
            } else if (!a.rdataBytes.empty()) {
                uint16_t rdlen = static_cast<uint16_t>(a.rdataBytes.size());
                outPacket.push_back(static_cast<uint8_t>(rdlen >> 8));
                outPacket.push_back(static_cast<uint8_t>(rdlen & 0xFF));
                outPacket.insert(outPacket.end(), a.rdataBytes.begin(), a.rdataBytes.end());
            } else {
                uint16_t rdlen = static_cast<uint16_t>(a.rdata.size());
                outPacket.push_back(static_cast<uint8_t>(rdlen >> 8));
                outPacket.push_back(static_cast<uint8_t>(rdlen & 0xFF));
                outPacket.insert(outPacket.end(), a.rdata.begin(), a.rdata.end());
            }
        }

        // Additional section (EDNS0 OPT RR)
        if (hasEdns0) {
            outPacket.push_back(0x00); // Root name
            outPacket.push_back(0x00); outPacket.push_back(static_cast<uint8_t>(TYPE_OPT));
            outPacket.push_back(static_cast<uint8_t>(ednsPayloadSize >> 8));
            outPacket.push_back(static_cast<uint8_t>(ednsPayloadSize & 0xFF));
            outPacket.push_back(0x00); // Extended RCODE
            outPacket.push_back(0x00); // EDNS Version 0
            outPacket.push_back(0x80); outPacket.push_back(0x00); // Flags: DO (DNSSEC OK)
            outPacket.push_back(0x00); outPacket.push_back(0x00); // RDLEN = 0
            totalDnssecQueries_++;
        }

        return true;
    }

    bool processQueryPacket(const std::vector<uint8_t>& queryData, std::vector<uint8_t>& responseData) noexcept {
        return processWireQuery(queryData.data(), queryData.size(), responseData);
    }

    // --- Cache Operations ---

    void flushCache() noexcept { cache_.clear(); }
    size_t getCacheSize() const noexcept { return cache_.size(); }
    DnsServerCache& getCache() noexcept { return cache_; }
    const DnsServerCache& getCache() const noexcept { return cache_; }

    // --- Telemetry & Vitals ---

    uint64_t getTotalQueries() const noexcept { return totalQueriesReceived_.load(); }
    uint64_t getTotalQueriesReceived() const noexcept { return totalQueriesReceived_.load(); }
    uint64_t getTotalQueriesAnswered() const noexcept { return totalQueriesAnswered_.load(); }
    uint64_t getCacheHits() const noexcept { return cacheHits_.load(); }
    uint64_t getTotalCacheHits() const noexcept { return cacheHits_.load(); }
    uint64_t getCacheMisses() const noexcept { return cacheMisses_.load(); }
    uint64_t getTotalCacheMisses() const noexcept { return cacheMisses_.load(); }
    uint64_t getDynamicUpdates() const noexcept { return totalDynamicUpdates_.load(); }
    uint64_t getTotalDynamicUpdates() const noexcept { return totalDynamicUpdates_.load(); }
    uint64_t getTotalZoneTransfers() const noexcept { return totalZoneTransfers_.load(); }
    uint64_t getTotalDnssecQueries() const noexcept { return totalDnssecQueries_.load(); }

    std::string getServerHost() const noexcept { return "dc01.titan.local"; }
    std::string getServerIp() const noexcept { return "147.135.1.229"; }
    std::string getDomainName() const noexcept { return "micant.local"; }

private:
    EnterpriseDnsServer() = default;

    mutable std::recursive_mutex mutex_;
    bool initialized_{false};
    std::map<std::string, std::shared_ptr<DnsZone>> zones_;
    std::map<std::string, std::string> rootHints_;
    DnsServerCache cache_;

    mutable std::atomic<uint64_t> totalQueriesReceived_{0};
    mutable std::atomic<uint64_t> totalQueriesAnswered_{0};
    mutable std::atomic<uint64_t> cacheHits_{0};
    mutable std::atomic<uint64_t> cacheMisses_{0};
    mutable std::atomic<uint64_t> totalDynamicUpdates_{0};
    mutable std::atomic<uint64_t> totalZoneTransfers_{0};
    mutable std::atomic<uint64_t> totalDnssecQueries_{0};
};

// ============================================================================
// 6. Clean-Room Win32 C ABI Exports (dnsapi.dll / dns.exe Parity)
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
    DnsResourceRecord rr(szName, wType, CLASS_IN, ttl, szData);
    return eng->addRecord(szZoneName, rr) ? 1 : 0;
}

inline int32_t MicaDnsQuery(void* pEngine, const char* szQuestion, uint16_t wType, uint8_t* pPacketOut, uint32_t cbMax, uint32_t* pcbActual) {
    if (!pEngine || !szQuestion) return 0;
    auto* eng = static_cast<EnterpriseDnsServer*>(pEngine);

    std::vector<uint8_t> inPkt;
    DnsHeaderWire hdr{};
    hdr.id = 0x5432;
    hdr.flags = tcpip::htons(FLAG_RD);
    hdr.qdcount = tcpip::htons(1);
    inPkt.resize(sizeof(DnsHeaderWire));
    std::memcpy(inPkt.data(), &hdr, sizeof(hdr));

    AppendDomainName(inPkt, szQuestion);
    inPkt.push_back(static_cast<uint8_t>(wType >> 8));
    inPkt.push_back(static_cast<uint8_t>(wType & 0xFF));
    inPkt.push_back(0x00);
    inPkt.push_back(0x01);

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

// ============================================================================
// 7. Subsystem Parity Forwarders & Registrations for Horizon 161
// ============================================================================

namespace micant::dns::server::um {

using DnsServerEngine = ::micant::dns::EnterpriseDnsServer;
using DnsZone = ::micant::dns::DnsZone;
using DnsResourceRecord = ::micant::dns::DnsResourceRecord;
using DnsServerCache = ::micant::dns::DnsServerCache;
using ZoneType = ::micant::dns::ZoneType;
using ReplicationScope = ::micant::dns::ReplicationScope;

using ::micant::dns::TYPE_A;
using ::micant::dns::TYPE_NS;
using ::micant::dns::TYPE_CNAME;
using ::micant::dns::TYPE_SOA;
using ::micant::dns::TYPE_PTR;
using ::micant::dns::TYPE_MX;
using ::micant::dns::TYPE_TXT;
using ::micant::dns::TYPE_AAAA;
using ::micant::dns::TYPE_SRV;
using ::micant::dns::TYPE_OPT;
using ::micant::dns::TYPE_DS;
using ::micant::dns::TYPE_RRSIG;
using ::micant::dns::TYPE_NSEC;
using ::micant::dns::TYPE_DNSKEY;
using ::micant::dns::TYPE_NSEC3;
using ::micant::dns::TYPE_IXFR;
using ::micant::dns::TYPE_AXFR;
using ::micant::dns::TYPE_ANY;

using ::micant::dns::CLASS_IN;
using ::micant::dns::CLASS_ANY;

using ::micant::dns::OPCODE_QUERY;
using ::micant::dns::OPCODE_UPDATE;

using ::micant::dns::RCODE_NOERROR;
using ::micant::dns::RCODE_FORMERR;
using ::micant::dns::RCODE_SERVFAIL;
using ::micant::dns::RCODE_NXDOMAIN;

using ::micant::dns::DnsHeaderWire;
using ::micant::dns::DnsSoaData;
using ::micant::dns::DnsMxData;
using ::micant::dns::DnsSrvData;
using ::micant::dns::DnsDnskeyData;
using ::micant::dns::DnsRrsigData;
using ::micant::dns::AppendDomainName;
using ::micant::dns::ReadDomainName;
using ::micant::dns::RecordTypeToString;
using ::micant::dns::StringToRecordType;

inline void RegisterDnsSubsystem() {
    auto& eng = ::micant::dns::EnterpriseDnsServer::instance();
    eng.initialize();
}

} // namespace micant::dns::server::um
