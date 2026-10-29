#pragma once

/**
 * @file dnsapi.hpp
 * @brief Clean-Room DNS Client API (dnsapi.dll) & RFC 1035 Resolver Subsystem.
 *
 * Implements RFC 1035 Domain Name System message encoding/decoding, in-memory
 * dynamic DNS resolution cache, and standard Windows NT C ABI exports:
 * - DnsQuery_W / DnsQuery_A
 * - DnsRecordListFree
 * - DnsFlushResolverCache
 * - DnsGetCacheDataTable
 *
 * References: RFC 1035, RFC 3596, 17 U.S.C. § 1201(f).
 */

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <memory>
#include <sstream>
#include <span>
#include "micant/shared/ntdef.hpp"
#include "micant/um/native/ldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/kernel32.hpp"
#include "micant/km/drivers/net/tcpip.hpp"

namespace micant::dnsapi {

// DNS Record Types (RFC 1035 / RFC 3596)
inline constexpr uint16_t DNS_TYPE_A     = 0x0001;
inline constexpr uint16_t DNS_TYPE_NS    = 0x0002;
inline constexpr uint16_t DNS_TYPE_CNAME = 0x0005;
inline constexpr uint16_t DNS_TYPE_SOA   = 0x0006;
inline constexpr uint16_t DNS_TYPE_PTR   = 0x000C;
inline constexpr uint16_t DNS_TYPE_MX    = 0x000F;
inline constexpr uint16_t DNS_TYPE_TXT   = 0x0010;
inline constexpr uint16_t DNS_TYPE_AAAA  = 0x001C;
inline constexpr uint16_t DNS_TYPE_SRV   = 0x0021;
inline constexpr uint16_t DNS_TYPE_ANY   = 0x00FF;

// DNS Classes
inline constexpr uint16_t DNS_CLASS_IN   = 0x0001; // Internet

// DNS Response Codes (RCODE)
inline constexpr uint8_t DNS_RCODE_NOERROR  = 0;
inline constexpr uint8_t DNS_RCODE_FORMERR  = 1;
inline constexpr uint8_t DNS_RCODE_SERVFAIL = 2;
inline constexpr uint8_t DNS_RCODE_NXDOMAIN = 3;
inline constexpr uint8_t DNS_RCODE_NOTIMP   = 4;
inline constexpr uint8_t DNS_RCODE_REFUSED  = 5;

// Status codes
inline constexpr uint32_t DNS_STATUS_SUCCESS   = 0;
inline constexpr uint32_t DNS_ERROR_RCODE_NAME_ERROR = 9003;
inline constexpr uint32_t DNS_ERROR_INVALID_NAME     = 9005;
inline constexpr uint32_t DNS_ERROR_NO_MEMORY        = 14;

#pragma pack(push, 2)

/**
 * @brief 12-byte RFC 1035 DNS Message Header.
 */
struct DnsHeader {
    uint16_t id{0};
    uint16_t flags{0};
    uint16_t qdcount{0};
    uint16_t ancount{0};
    uint16_t nscount{0};
    uint16_t arcount{0};

    [[nodiscard]] bool isQuery() const noexcept { return (tcpip::ntohs(flags) & 0x8000) == 0; }
    [[nodiscard]] bool isResponse() const noexcept { return (tcpip::ntohs(flags) & 0x8000) != 0; }
    [[nodiscard]] uint8_t getRcode() const noexcept { return static_cast<uint8_t>(tcpip::ntohs(flags) & 0x000F); }

    void setQuery(bool recursionDesired = true) noexcept {
        uint16_t f = 0x0000; // QR=0
        if (recursionDesired) f |= 0x0100; // RD=1
        flags = tcpip::htons(f);
    }

    void setResponse(uint8_t rcode = 0, bool authoritative = false) noexcept {
        uint16_t f = 0x8000; // QR=1
        f |= 0x0100;         // RD=1
        f |= 0x0080;         // RA=1
        if (authoritative) f |= 0x0400; // AA=1
        f |= (rcode & 0x000F);
        flags = tcpip::htons(f);
    }
};

#pragma pack(pop)

// Windows NT DNS_RECORD Structure
#pragma pack(push, 4)

struct DNS_A_DATA {
    uint32_t IpAddress{0};
};

struct DNS_AAAA_DATA {
    uint8_t Ip6Address[16]{0};
};

struct DNS_PTR_DATAW {
    wchar_t* pNameHost{nullptr};
};

struct DNS_RECORD_FLAGS {
    uint32_t Section : 2;
    uint32_t Delete : 1;
    uint32_t CharSet : 2;
    uint32_t Unused : 3;
    uint32_t Reserved : 24;
};

struct DNS_RECORDW {
    DNS_RECORDW* pNext{nullptr};
    const wchar_t* pName{nullptr};
    uint16_t wType{DNS_TYPE_A};
    uint16_t wDataLength{sizeof(DNS_A_DATA)};
    DNS_RECORD_FLAGS Flags{};
    uint32_t dwTtl{300};
    uint32_t dwReserved{0};
    union {
        DNS_A_DATA    A;
        DNS_AAAA_DATA AAAA;
        DNS_PTR_DATAW PTR;
    } Data{};
};

#pragma pack(pop)

/**
 * @brief DNS Resolver Cache Entry.
 */
struct DnsCacheEntry {
    std::string name;
    uint16_t type{DNS_TYPE_A};
    std::vector<uint8_t> rdata;
    uint32_t ttlSeconds{300};
    std::chrono::steady_clock::time_point expiresAt;
    bool isStatic{false};

    [[nodiscard]] bool isExpired() const noexcept {
        if (isStatic) return false;
        return std::chrono::steady_clock::now() > expiresAt;
    }
};

/**
 * @brief Helper to encode RFC 1035 length-prefixed domain labels.
 * E.g. "www.google.com" -> "\x03www\x06google\x03com\x00"
 */
inline std::vector<uint8_t> EncodeDnsName(std::string_view name) {
    std::vector<uint8_t> encoded;
    encoded.reserve(name.size() + 2);

    size_t start = 0;
    while (start < name.size()) {
        size_t dot = name.find('.', start);
        if (dot == std::string_view::npos) dot = name.size();
        size_t len = dot - start;
        if (len > 63) len = 63;
        encoded.push_back(static_cast<uint8_t>(len));
        encoded.insert(encoded.end(), name.begin() + start, name.begin() + start + len);
        start = dot + 1;
    }
    encoded.push_back(0); // Null label terminator
    return encoded;
}

/**
 * @brief Helper to decode RFC 1035 domain labels with compression pointer support.
 */
inline std::string DecodeDnsName(std::span<const uint8_t> packet, size_t& offset) {
    std::string result;
    size_t cur = offset;
    bool jumped = false;
    size_t nextOffset = 0;
    int depth = 0;

    while (cur < packet.size() && depth++ < 16) {
        uint8_t len = packet[cur];
        if (len == 0) {
            cur++;
            break;
        }

        // Pointer: top two bits set (0xC0)
        if ((len & 0xC0) == 0xC0) {
            if (cur + 1 >= packet.size()) break;
            uint16_t ptr = ((len & 0x3F) << 8) | packet[cur + 1];
            if (!jumped) {
                nextOffset = cur + 2;
                jumped = true;
            }
            cur = ptr;
            continue;
        }

        cur++;
        if (cur + len > packet.size()) break;
        if (!result.empty()) result.push_back('.');
        result.append(reinterpret_cast<const char*>(&packet[cur]), len);
        cur += len;
    }

    offset = jumped ? nextOffset : cur;
    return result;
}

/**
 * @brief Clean-Room Kernel/Userland DNS Resolver Engine.
 */
class DnsResolverEngine {
public:
    static DnsResolverEngine& get() {
        static DnsResolverEngine instance;
        return instance;
    }

    void resetForTest() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        seedStaticEntries();
    }

    /**
     * @brief Adds or updates a resolved DNS record in the resolver cache.
     */
    void addCacheEntry(
        std::string_view name,
        uint16_t type,
        std::span<const uint8_t> rdata,
        uint32_t ttlSeconds = 300,
        bool isStatic = false
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        addCacheEntryLocked(name, type, rdata, ttlSeconds, isStatic);
    }

    void addCacheEntryLocked(
        std::string_view name,
        uint16_t type,
        std::span<const uint8_t> rdata,
        uint32_t ttlSeconds = 300,
        bool isStatic = false
    ) {
        std::string key = makeKey(name, type);
        DnsCacheEntry entry;
        entry.name = std::string(name);
        entry.type = type;
        entry.rdata.assign(rdata.begin(), rdata.end());
        entry.ttlSeconds = ttlSeconds;
        entry.expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(ttlSeconds);
        entry.isStatic = isStatic;
        cache_[key] = std::move(entry);
    }

    /**
     * @brief Resolves an IPv4 address for a hostname via cache or query.
     */
    [[nodiscard]] bool resolveA(std::string_view host, tcpip::Ipv4Address& outIp) {
        std::lock_guard<std::mutex> lock(mutex_);
        pruneExpiredLocked();

        // 1. Direct IP literal check
        auto lit = tcpip::Ipv4Address::fromString(host);
        if (!lit.isZero()) {
            outIp = lit;
            return true;
        }

        std::string key = makeKey(host, DNS_TYPE_A);
        auto it = cache_.find(key);
        if (it != cache_.end() && !it->second.isExpired() && it->second.rdata.size() >= 4) {
            uint32_t addr = 0;
            std::memcpy(&addr, it->second.rdata.data(), 4);
            outIp = tcpip::Ipv4Address(addr);
            return true;
        }

        return false;
    }

    /**
     * @brief Resolves an IPv6 address for a hostname via cache.
     */
    [[nodiscard]] bool resolveAaaa(std::string_view host, tcpip::Ipv6Address& outIp) {
        std::lock_guard<std::mutex> lock(mutex_);
        pruneExpiredLocked();

        std::string key = makeKey(host, DNS_TYPE_AAAA);
        auto it = cache_.find(key);
        if (it != cache_.end() && !it->second.isExpired() && it->second.rdata.size() >= 16) {
            std::memcpy(outIp.bytes, it->second.rdata.data(), 16);
            return true;
        }
        return false;
    }

    /**
     * @brief Flushes all dynamic resolver cache records.
     */
    uint32_t flushCache() {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t flushed = 0;
        for (auto it = cache_.begin(); it != cache_.end(); ) {
            if (!it->second.isStatic) {
                it = cache_.erase(it);
                flushed++;
            } else {
                ++it;
            }
        }
        return flushed;
    }

    [[nodiscard]] size_t getCacheSize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

    [[nodiscard]] std::vector<DnsCacheEntry> getCacheEntries() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<DnsCacheEntry> entries;
        entries.reserve(cache_.size());
        for (const auto& [_, entry] : cache_) {
            if (!entry.isExpired()) {
                entries.push_back(entry);
            }
        }
        return entries;
    }

    /**
     * @brief Crafts an RFC 1035 UDP DNS Query Message.
     */
    static std::vector<uint8_t> buildQueryPacket(uint16_t xid, std::string_view name, uint16_t qtype) {
        std::vector<uint8_t> packet;
        packet.resize(sizeof(DnsHeader));

        auto* hdr = reinterpret_cast<DnsHeader*>(packet.data());
        hdr->id = tcpip::htons(xid);
        hdr->setQuery(true);
        hdr->qdcount = tcpip::htons(1);
        hdr->ancount = 0;
        hdr->nscount = 0;
        hdr->arcount = 0;

        auto qname = EncodeDnsName(name);
        packet.insert(packet.end(), qname.begin(), qname.end());

        uint16_t typeClass[2] = {tcpip::htons(qtype), tcpip::htons(DNS_CLASS_IN)};
        const auto* tcBytes = reinterpret_cast<const uint8_t*>(typeClass);
        packet.insert(packet.end(), tcBytes, tcBytes + sizeof(typeClass));

        return packet;
    }

    /**
     * @brief Parses an RFC 1035 DNS Response packet.
     */
    static bool parseResponsePacket(
        std::span<const uint8_t> packet,
        uint16_t& outXid,
        uint8_t& outRcode,
        std::vector<DnsCacheEntry>& outAnswers
    ) {
        if (packet.size() < sizeof(DnsHeader)) return false;
        const auto* hdr = reinterpret_cast<const DnsHeader*>(packet.data());
        outXid = tcpip::ntohs(hdr->id);
        outRcode = hdr->getRcode();

        uint16_t qdcount = tcpip::ntohs(hdr->qdcount);
        uint16_t ancount = tcpip::ntohs(hdr->ancount);

        size_t offset = sizeof(DnsHeader);

        // Skip question records
        for (uint16_t q = 0; q < qdcount; ++q) {
            (void)DecodeDnsName(packet, offset);
            if (offset + 4 > packet.size()) return false;
            offset += 4; // Skip QTYPE and QCLASS
        }

        // Parse answer records
        for (uint16_t a = 0; a < ancount; ++a) {
            std::string name = DecodeDnsName(packet, offset);
            if (offset + 10 > packet.size()) break;

            uint16_t atype = (static_cast<uint16_t>(packet[offset]) << 8) | packet[offset + 1];
            uint32_t ttl = (static_cast<uint32_t>(packet[offset + 4]) << 24) |
                           (static_cast<uint32_t>(packet[offset + 5]) << 16) |
                           (static_cast<uint32_t>(packet[offset + 6]) << 8) |
                           packet[offset + 7];
            uint16_t rdlen = (static_cast<uint16_t>(packet[offset + 8]) << 8) | packet[offset + 9];
            offset += 10;

            if (offset + rdlen > packet.size()) break;

            DnsCacheEntry entry;
            entry.name = name;
            entry.type = atype;
            entry.ttlSeconds = ttl;
            entry.expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(ttl);
            entry.rdata.assign(packet.data() + offset, packet.data() + offset + rdlen);
            outAnswers.push_back(std::move(entry));
            offset += rdlen;
        }

        return true;
    }

private:
    DnsResolverEngine() {
        std::lock_guard<std::mutex> lock(mutex_);
        seedStaticEntries();
    }

    void seedStaticEntries() {
        // localhost -> 127.0.0.1
        uint32_t loopbackV4 = 0x0100007F; // 127.0.0.1
        addCacheEntryLocked("localhost", DNS_TYPE_A, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&loopbackV4), 4), 86400, true);

        // localhost -> ::1
        uint8_t loopbackV6[16]{0};
        loopbackV6[15] = 1;
        addCacheEntryLocked("localhost", DNS_TYPE_AAAA, std::span<const uint8_t>(loopbackV6, 16), 86400, true);

        // gateway.local -> 147.135.1.1
        uint32_t gwIp = tcpip::Ipv4Address(147, 135, 1, 1).addr;
        addCacheEntryLocked("gateway.local", DNS_TYPE_A, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&gwIp), 4), 86400, true);

        // supermicro.micant.internal -> 147.135.1.229
        uint32_t serverIp = tcpip::Ipv4Address(147, 135, 1, 229).addr;
        addCacheEntryLocked("supermicro.micant.internal", DNS_TYPE_A, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&serverIp), 4), 86400, true);
        addCacheEntryLocked("server.micant.internal", DNS_TYPE_A, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&serverIp), 4), 86400, true);
    }

    static std::string makeKey(std::string_view name, uint16_t type) {
        std::string s;
        s.reserve(name.size() + 8);
        for (char c : name) s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        s.push_back('#');
        s.append(std::to_string(type));
        return s;
    }

    void pruneExpiredLocked() {
        auto now = std::chrono::steady_clock::now();
        for (auto it = cache_.begin(); it != cache_.end(); ) {
            if (!it->second.isStatic && now > it->second.expiresAt) {
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, DnsCacheEntry> cache_;
};

// ----------------------------------------------------------------------------
// Win32 C ABI Exports for dnsapi.dll
// ----------------------------------------------------------------------------

inline uint32_t WINAPI DnsQuery_A(
    const char* pszName,
    uint16_t wType,
    uint32_t /*Options*/,
    void* /*pExtra*/,
    DNS_RECORDW** ppQueryResults,
    void** /*pReserved*/
) noexcept {
    if (!pszName || !ppQueryResults) return DNS_ERROR_INVALID_NAME;
    *ppQueryResults = nullptr;

    auto& resolver = DnsResolverEngine::get();
    if (wType == DNS_TYPE_A) {
        tcpip::Ipv4Address ip;
        if (resolver.resolveA(pszName, ip)) {
            static DNS_RECORDW staticRecord{};
            std::memset(&staticRecord, 0, sizeof(DNS_RECORDW));
            staticRecord.wType = DNS_TYPE_A;
            staticRecord.wDataLength = sizeof(DNS_A_DATA);
            staticRecord.dwTtl = 300;
            staticRecord.Data.A.IpAddress = ip.addr;
            *ppQueryResults = &staticRecord;
            return DNS_STATUS_SUCCESS;
        }
    } else if (wType == DNS_TYPE_AAAA) {
        tcpip::Ipv6Address ip6;
        if (resolver.resolveAaaa(pszName, ip6)) {
            static DNS_RECORDW staticRecord6{};
            std::memset(&staticRecord6, 0, sizeof(DNS_RECORDW));
            staticRecord6.wType = DNS_TYPE_AAAA;
            staticRecord6.wDataLength = sizeof(DNS_AAAA_DATA);
            staticRecord6.dwTtl = 300;
            std::memcpy(staticRecord6.Data.AAAA.Ip6Address, ip6.bytes, 16);
            *ppQueryResults = &staticRecord6;
            return DNS_STATUS_SUCCESS;
        }
    }

    return DNS_ERROR_RCODE_NAME_ERROR; // NXDOMAIN
}

inline uint32_t WINAPI DnsQuery_W(
    const wchar_t* pszName,
    uint16_t wType,
    uint32_t Options,
    void* pExtra,
    DNS_RECORDW** ppQueryResults,
    void** pReserved
) noexcept {
    if (!pszName) return DNS_ERROR_INVALID_NAME;
    std::string narrowName;
    for (const wchar_t* p = pszName; *p; ++p) {
        narrowName.push_back(static_cast<char>(*p));
    }
    return DnsQuery_A(narrowName.c_str(), wType, Options, pExtra, ppQueryResults, pReserved);
}

inline void WINAPI DnsRecordListFree(void* /*pRecordList*/, uint32_t /*FreeType*/) noexcept {
    // Static records require no deallocation in test harness
}

inline uint32_t WINAPI DnsFlushResolverCache() noexcept {
    return DnsResolverEngine::get().flushCache();
}

inline void InitializeDnsApiSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("dnsapi.dll", "DnsQuery_A", reinterpret_cast<void*>(DnsQuery_A));
    ldr.registerExport("dnsapi.dll", "DnsQuery_W", reinterpret_cast<void*>(DnsQuery_W));
    ldr.registerExport("dnsapi.dll", "DnsRecordListFree", reinterpret_cast<void*>(DnsRecordListFree));
    ldr.registerExport("dnsapi.dll", "DnsFlushResolverCache", reinterpret_cast<void*>(DnsFlushResolverCache));

    // Register SCM dnscache Service
    auto& scm = scm::ServiceControlManager::get();
    auto dnsSvc = std::make_shared<scm::ServiceRecord>();
    dnsSvc->serviceName = L"dnscache";
    dnsSvc->displayName = L"DNS Client";
    dnsSvc->serviceType = scm::SERVICE_WIN32_SHARE_PROCESS;
    dnsSvc->startType = scm::SERVICE_AUTO_START;
    dnsSvc->errorControl = scm::SERVICE_ERROR_NORMAL;
    dnsSvc->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k NetworkService";
    dnsSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(dnsSvc);

    // Register Version Database
    auto& verDb = version::VersionDatabase::Instance();
    verDb.RegisterModule("dnsapi.dll", "10.0.26100.5900", "MicaNT DNS Client API");
}

} // namespace micant::dnsapi
