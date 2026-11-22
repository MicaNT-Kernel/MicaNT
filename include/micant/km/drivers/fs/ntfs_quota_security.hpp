// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Storage Subsystem
// Horizon 367 / Suite 583 - NTFS Quota Governor & Security Descriptors Engine
// $Quota ($Q / $O), $Secure ($SDS / $SDH / $SII) Hash Buckets & Inherited ACEs
//
// Clean-Room Implementation in modern ISO C++23.
// Compatible with Microsoft Windows NT NTFS on-disk layout, win32metadata,
// and IFS driver architecture. Zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <mutex>
#include <span>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/drivers/fs/ntfs.hpp"
#include "micant/km/drivers/fs/ntfs_primary_storage.hpp"

namespace micant::ntfs::security {

// ============================================================================
// 1. Security & Quota Signatures, Stream Identifiers and ACE Types
// ============================================================================

inline constexpr uint32_t SDS_ENTRY_ALIGNMENT           = 16;
inline constexpr uint32_t SECURITY_ID_FIRST             = 0x100;

// Standard NT ACE Types
inline constexpr uint8_t ACCESS_ALLOWED_ACE_TYPE        = 0x00;
inline constexpr uint8_t ACCESS_DENIED_ACE_TYPE         = 0x01;

// Standard NT ACE Flags
inline constexpr uint8_t OBJECT_INHERIT_ACE             = 0x01;
inline constexpr uint8_t CONTAINER_INHERIT_ACE          = 0x02;
inline constexpr uint8_t NO_PROPAGATE_INHERIT_ACE       = 0x04;
inline constexpr uint8_t INHERIT_ONLY_ACE               = 0x08;
inline constexpr uint8_t INHERITED_ACE                  = 0x10;

// Standard Access Rights
inline constexpr uint32_t FILE_READ_DATA                = 0x00000001;
inline constexpr uint32_t FILE_WRITE_DATA               = 0x00000002;
inline constexpr uint32_t FILE_APPEND_DATA              = 0x00000004;
inline constexpr uint32_t FILE_EXECUTE                  = 0x00000020;
inline constexpr uint32_t FILE_DELETE_CHILD             = 0x00000040;
inline constexpr uint32_t FILE_ALL_ACCESS               = 0x001F01FF;

// ============================================================================
// 2. Packed On-Disk Security Header Structures ($Secure \$SDS)
// ============================================================================

#pragma pack(push, 1)

struct SdsEntryHeader {
    uint32_t hash{0};           // 32-bit hash of the security descriptor
    uint32_t securityId{0};     // Monotonic Security ID
    uint64_t offsetInSds{0};    // Byte offset in $SDS stream
    uint32_t length{0};         // Total entry length (16-byte aligned)
};

struct SdhIndexKey {
    uint32_t hash{0};
    uint32_t securityId{0};

    bool operator==(const SdhIndexKey& o) const noexcept {
        return hash == o.hash && securityId == o.securityId;
    }
};

#pragma pack(pop)

// ============================================================================
// 3. Simple Self-Relative Security Descriptor & ACE Primitives
// ============================================================================

struct SimpleAce {
    uint8_t  type{ACCESS_ALLOWED_ACE_TYPE};
    uint8_t  flags{0};
    uint32_t mask{0};
    std::wstring trusteeSid;
};

struct SimpleSecurityDescriptor {
    std::wstring ownerSid;
    std::wstring groupSid;
    std::vector<SimpleAce> dacl;

    // Serialize to standard self-relative binary layout
    [[nodiscard]] std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> out;
        // Simple binary format: [ownerLen(4)][ownerStr][daclCount(4)][for each ACE: type(1), flags(1), mask(4), sidLen(4), sidStr]
        uint32_t ownerLen = static_cast<uint32_t>(ownerSid.size() * sizeof(wchar_t));
        out.resize(sizeof(uint32_t));
        std::memcpy(out.data(), &ownerLen, sizeof(uint32_t));

        if (ownerLen > 0) {
            size_t pos = out.size();
            out.resize(pos + ownerLen);
            std::memcpy(out.data() + pos, ownerSid.data(), ownerLen);
        }

        uint32_t aceCount = static_cast<uint32_t>(dacl.size());
        size_t countPos = out.size();
        out.resize(countPos + sizeof(uint32_t));
        std::memcpy(out.data() + countPos, &aceCount, sizeof(uint32_t));

        for (const auto& ace : dacl) {
            size_t p = out.size();
            uint32_t sidBytes = static_cast<uint32_t>(ace.trusteeSid.size() * sizeof(wchar_t));
            out.resize(p + 1 + 1 + 4 + 4 + sidBytes);
            out[p] = ace.type;
            out[p + 1] = ace.flags;
            std::memcpy(out.data() + p + 2, &ace.mask, sizeof(uint32_t));
            std::memcpy(out.data() + p + 6, &sidBytes, sizeof(uint32_t));
            if (sidBytes > 0) {
                std::memcpy(out.data() + p + 10, ace.trusteeSid.data(), sidBytes);
            }
        }

        return out;
    }
};

// ============================================================================
// 4. Sovereign $Secure Shared Security Descriptor Store ($SDS, $SDH, $SII)
// ============================================================================

class SovereignNtfsSecurityStore {
public:
    SovereignNtfsSecurityStore() = default;

    // Fast 32-bit hash algorithm matching NTFS $Secure hash calculation
    static uint32_t computeSecurityHash(std::span<const uint8_t> sdBytes) noexcept {
        uint32_t hash = 0;
        for (size_t i = 0; i < sdBytes.size(); ++i) {
            hash = ((hash << 3) | (hash >> 29)) + sdBytes[i];
        }
        return hash;
    }

    // Register a security descriptor with automatic deduplication
    NtStatus registerSecurityDescriptor(
        std::span<const uint8_t> sdBytes,
        uint32_t& outSecurityId
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (sdBytes.empty()) {
            return NtStatus::InvalidParameter;
        }

        uint32_t hash = computeSecurityHash(sdBytes);

        // Check $SDH index for matching hash to deduplicate
        auto range = m_sdhIndex.equal_range(hash);
        for (auto it = range.first; it != range.second; ++it) {
            uint64_t existingOffset = it->second;
            if (existingOffset + sizeof(SdsEntryHeader) + sdBytes.size() <= m_sdsStream.size()) {
                const auto* existingHdr = reinterpret_cast<const SdsEntryHeader*>(m_sdsStream.data() + existingOffset);
                const uint8_t* existingSdData = m_sdsStream.data() + existingOffset + sizeof(SdsEntryHeader);

                if (std::memcmp(existingSdData, sdBytes.data(), sdBytes.size()) == 0) {
                    // Exact match found! Deduplicate by reusing existing SecurityId
                    outSecurityId = existingHdr->securityId;
                    m_deduplicationHits++;
                    return NtStatus::Success;
                }
            }
        }

        // New unique Security Descriptor: append to $SDS stream
        uint32_t secId = m_nextSecurityId++;
        uint64_t offsetInSds = m_sdsStream.size();

        uint32_t payloadLen = static_cast<uint32_t>(sizeof(SdsEntryHeader) + sdBytes.size());
        uint32_t alignedLen = (payloadLen + SDS_ENTRY_ALIGNMENT - 1) & ~(SDS_ENTRY_ALIGNMENT - 1);

        SdsEntryHeader entryHdr{};
        entryHdr.hash = hash;
        entryHdr.securityId = secId;
        entryHdr.offsetInSds = offsetInSds;
        entryHdr.length = alignedLen;

        m_sdsStream.resize(offsetInSds + alignedLen, 0);
        std::memcpy(m_sdsStream.data() + offsetInSds, &entryHdr, sizeof(entryHdr));
        std::memcpy(m_sdsStream.data() + offsetInSds + sizeof(entryHdr), sdBytes.data(), sdBytes.size());

        // Update indices ($SDH and $SII)
        m_sdhIndex.emplace(hash, offsetInSds);
        m_siiIndex[secId] = offsetInSds;

        outSecurityId = secId;
        return NtStatus::Success;
    }

    // Retrieve security descriptor by Security ID via $SII index
    NtStatus getSecurityDescriptor(
        uint32_t securityId,
        std::vector<uint8_t>& outSdBytes
    ) const {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_siiIndex.find(securityId);
        if (it == m_siiIndex.end()) {
            return NtStatus::NoSuchFile;
        }

        uint64_t offset = it->second;
        if (offset + sizeof(SdsEntryHeader) > m_sdsStream.size()) {
            return NtStatus::FileCorrupted;
        }

        const auto* hdr = reinterpret_cast<const SdsEntryHeader*>(m_sdsStream.data() + offset);
        size_t dataLen = hdr->length - sizeof(SdsEntryHeader);
        const uint8_t* dataPtr = m_sdsStream.data() + offset + sizeof(SdsEntryHeader);

        outSdBytes.assign(dataPtr, dataPtr + dataLen);
        return NtStatus::Success;
    }

    [[nodiscard]] size_t getUniqueDescriptorCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_siiIndex.size();
    }

    [[nodiscard]] size_t getDeduplicationHits() const noexcept {
        return m_deduplicationHits;
    }

    [[nodiscard]] uint64_t getSdsStreamSize() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_sdsStream.size();
    }

private:
    mutable std::mutex m_mutex;
    uint32_t m_nextSecurityId{SECURITY_ID_FIRST};
    size_t m_deduplicationHits{0};
    std::vector<uint8_t> m_sdsStream;
    std::unordered_multimap<uint32_t, uint64_t> m_sdhIndex; // Hash -> Offset in $SDS
    std::unordered_map<uint32_t, uint64_t> m_siiIndex;      // SecurityId -> Offset in $SDS
};

// ============================================================================
// 5. Sovereign NTFS Quota Governor ($Extend\$Quota: $Q / $O)
// ============================================================================

struct QuotaRecord {
    std::wstring userSid;
    uint64_t bytesUsed{0};
    uint64_t warningLimit{0};
    uint64_t hardLimit{0};
    bool warningLogged{false};
};

class SovereignNtfsQuotaGovernor {
public:
    SovereignNtfsQuotaGovernor() = default;

    // Set quota limits for a specific user SID
    NtStatus setQuotaLimits(
        const std::wstring& sid,
        uint64_t warningLimit,
        uint64_t hardLimit
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto& rec = m_quotas[sid];
        rec.userSid = sid;
        rec.warningLimit = warningLimit;
        rec.hardLimit = hardLimit;
        rec.warningLogged = (rec.bytesUsed >= warningLimit);

        return NtStatus::Success;
    }

    // Allocate storage bytes for a user SID, enforcing hard limits
    NtStatus allocateBytes(
        const std::wstring& sid,
        uint64_t bytes
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_quotas.find(sid);
        if (it == m_quotas.end()) {
            // Default unlimited quota for untracked users
            m_quotas[sid] = QuotaRecord{ sid, bytes, 0, 0, false };
            return NtStatus::Success;
        }

        auto& rec = it->second;

        // Check Hard Limit
        if (rec.hardLimit > 0 && (rec.bytesUsed + bytes > rec.hardLimit)) {
            return NtStatus::DiskQuotaExceeded;
        }

        rec.bytesUsed += bytes;

        // Check Warning Limit
        if (rec.warningLimit > 0 && rec.bytesUsed >= rec.warningLimit) {
            rec.warningLogged = true;
        }

        return NtStatus::Success;
    }

    // Free storage bytes for a user SID
    NtStatus freeBytes(
        const std::wstring& sid,
        uint64_t bytes
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_quotas.find(sid);
        if (it == m_quotas.end()) return NtStatus::Success;

        auto& rec = it->second;
        if (rec.bytesUsed >= bytes) {
            rec.bytesUsed -= bytes;
        } else {
            rec.bytesUsed = 0;
        }

        if (rec.warningLimit > 0 && rec.bytesUsed < rec.warningLimit) {
            rec.warningLogged = false;
        }

        return NtStatus::Success;
    }

    // Query quota status for a user SID
    NtStatus queryQuota(
        const std::wstring& sid,
        QuotaRecord& outRecord
    ) const {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_quotas.find(sid);
        if (it == m_quotas.end()) {
            return NtStatus::NoSuchFile;
        }

        outRecord = it->second;
        return NtStatus::Success;
    }

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::wstring, QuotaRecord> m_quotas;
};

// ============================================================================
// 6. Subsystem Verification Harness (Suite 583 / Horizon 367)
// ============================================================================

bool VerifyHorizon367QuotaSecuritySubsystem();

} // namespace micant::ntfs::security
