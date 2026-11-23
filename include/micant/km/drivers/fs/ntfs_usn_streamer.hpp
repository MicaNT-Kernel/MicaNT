#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <memory>
#include <algorithm>

#include "micant/shared/ntstatus.hpp"
#include "ntfs_primary_storage.hpp"

namespace micant::ntfs::usn {

using micant::ntfs::primary::CREATE_USN_JOURNAL_DATA;
using micant::ntfs::primary::USN_JOURNAL_DATA_V0;
using micant::ntfs::primary::READ_USN_JOURNAL_DATA_V0;
using micant::ntfs::primary::USN_RECORD_V2;

// ============================================================================
// Sovereign NTFS Asynchronous Change Journal Streamer & Real-Time USN Filter
// ============================================================================

// Additional FSCTL control codes for USN operations
#ifndef FSCTL_READ_FILE_USN_DATA
inline constexpr uint32_t FSCTL_READ_FILE_USN_DATA = 0x000900EBu;
#endif

// USN Reason Bit Flags
inline constexpr uint32_t USN_REASON_DATA_OVERWRITE                 = 0x00000001u;
inline constexpr uint32_t USN_REASON_DATA_EXTEND                    = 0x00000002u;
inline constexpr uint32_t USN_REASON_DATA_TRUNCATION                = 0x00000004u;
inline constexpr uint32_t USN_REASON_NAMED_DATA_OVERWRITE           = 0x00000010u;
inline constexpr uint32_t USN_REASON_NAMED_DATA_EXTEND              = 0x00000020u;
inline constexpr uint32_t USN_REASON_NAMED_DATA_TRUNCATION          = 0x00000040u;
inline constexpr uint32_t USN_REASON_FILE_CREATE                    = 0x00000100u;
inline constexpr uint32_t USN_REASON_FILE_DELETE                    = 0x00000200u;
inline constexpr uint32_t USN_REASON_EA_CHANGE                      = 0x00000400u;
inline constexpr uint32_t USN_REASON_SECURITY_CHANGE                = 0x00000800u;
inline constexpr uint32_t USN_REASON_RENAME_OLD_NAME                = 0x00001000u;
inline constexpr uint32_t USN_REASON_RENAME_NEW_NAME                = 0x00002000u;
inline constexpr uint32_t USN_REASON_INDEXABLE_CHANGE               = 0x00004000u;
inline constexpr uint32_t USN_REASON_BASIC_INFO_CHANGE              = 0x00008000u;
inline constexpr uint32_t USN_REASON_HARD_LINK_CHANGE               = 0x00010000u;
inline constexpr uint32_t USN_REASON_COMPRESSION_CHANGE             = 0x00020000u;
inline constexpr uint32_t USN_REASON_ENCRYPTION_CHANGE              = 0x00040000u;
inline constexpr uint32_t USN_REASON_OBJECT_ID_CHANGE               = 0x00080000u;
inline constexpr uint32_t USN_REASON_REPARSE_POINT_CHANGE           = 0x00100000u;
inline constexpr uint32_t USN_REASON_STREAM_CHANGE                  = 0x00200000u;
inline constexpr uint32_t USN_REASON_TRANSACTED_CHANGE              = 0x00400000u;
inline constexpr uint32_t USN_REASON_INTEGRITY_CHANGE               = 0x00800000u;
inline constexpr uint32_t USN_REASON_DESIRED_STORAGE_CLASS_CHANGE  = 0x01000000u;
inline constexpr uint32_t USN_REASON_CLOSE                          = 0x80000000u;

// USN Source Information Bit Flags
inline constexpr uint32_t USN_SOURCE_DATA_MANAGEMENT                = 0x00000001u;
inline constexpr uint32_t USN_SOURCE_AUXILIARY_DATA                 = 0x00000002u;
inline constexpr uint32_t USN_SOURCE_REPLICATION_MANAGEMENT         = 0x00000004u;
inline constexpr uint32_t USN_SOURCE_CLIENT_REPLICATION_MANAGEMENT  = 0x00000008u;

// USN Delete Flags
inline constexpr uint32_t USN_DELETE_FLAG_DELETE                    = 0x00000001u;
inline constexpr uint32_t USN_DELETE_FLAG_NOTIFY                    = 0x00000002u;

// 128-bit File Identifier (used for modern NTFS / ReFS 128-bit FRNs)
struct FILE_ID_128 {
    uint8_t Identifier[16]{0};

    bool operator==(const FILE_ID_128& other) const noexcept {
        return std::memcmp(Identifier, other.Identifier, 16) == 0;
    }
    bool operator!=(const FILE_ID_128& other) const noexcept {
        return !(*this == other);
    }
    uint64_t low64() const noexcept {
        uint64_t val = 0;
        std::memcpy(&val, Identifier, sizeof(uint64_t));
        return val;
    }
    uint64_t high64() const noexcept {
        uint64_t val = 0;
        std::memcpy(&val, Identifier + 8, sizeof(uint64_t));
        return val;
    }
    static FILE_ID_128 from64(uint64_t val) noexcept {
        FILE_ID_128 id{};
        std::memcpy(id.Identifier, &val, sizeof(uint64_t));
        return id;
    }
    static FILE_ID_128 from128(uint64_t low, uint64_t high) noexcept {
        FILE_ID_128 id{};
        std::memcpy(id.Identifier, &low, sizeof(uint64_t));
        std::memcpy(id.Identifier + 8, &high, sizeof(uint64_t));
        return id;
    }
};

// Extent tracking for USN Record v4
struct USN_RECORD_EXTENT {
    uint64_t Offset{0};
    uint64_t Length{0};
};

// Common header to examine major version of any USN record
struct USN_RECORD_COMMON_HEADER {
    uint32_t RecordLength{0};
    uint16_t MajorVersion{0};
    uint16_t MinorVersion{0};
};

// USN Record Version 3 (128-bit File ID support)
struct USN_RECORD_V3 {
    uint32_t    RecordLength{sizeof(USN_RECORD_V3)};
    uint16_t    MajorVersion{3};
    uint16_t    MinorVersion{0};
    FILE_ID_128 FileReferenceNumber{};
    FILE_ID_128 ParentFileReferenceNumber{};
    uint64_t    Usn{0};
    uint64_t    TimeStamp{0};
    uint32_t    Reason{0};
    uint32_t    SourceInfo{0};
    uint32_t    SecurityId{0};
    uint32_t    FileAttributes{0};
    uint16_t    FileNameLength{0};
    uint16_t    FileNameOffset{sizeof(USN_RECORD_V3) - sizeof(wchar_t)};
    wchar_t     FileName[1]{L'\0'};
};

// USN Record Version 4 (Extent Delta tracking)
struct USN_RECORD_V4 {
    uint32_t          RecordLength{sizeof(USN_RECORD_V4)};
    uint16_t          MajorVersion{4};
    uint16_t          MinorVersion{0};
    FILE_ID_128       FileReferenceNumber{};
    FILE_ID_128       ParentFileReferenceNumber{};
    uint64_t          Usn{0};
    uint32_t          Reason{0};
    uint32_t          SourceInfo{0};
    uint32_t          RemainingExtents{0};
    uint16_t          NumberOfExtents{0};
    uint16_t          ExtentOffset{sizeof(USN_RECORD_V4) - sizeof(USN_RECORD_EXTENT)};
    USN_RECORD_EXTENT Extents[1]{};
};

// Journal Query Structures
struct USN_JOURNAL_DATA_V1 {
    uint64_t UsnJournalID{0};
    uint64_t FirstUsn{0};
    uint64_t NextUsn{0};
    uint64_t LowestValidUsn{0};
    uint64_t MaxUsn{0xFFFFFFFFULL};
    uint64_t MaximumSize{0x8000000ULL};     // 128 MB default
    uint64_t AllocationDelta{0x1000000ULL};  // 16 MB default
    uint16_t MinSupportedMajorVersion{2};
    uint16_t MaxSupportedMajorVersion{4};
};

struct USN_JOURNAL_DATA_V2 {
    uint64_t UsnJournalID{0};
    uint64_t FirstUsn{0};
    uint64_t NextUsn{0};
    uint64_t LowestValidUsn{0};
    uint64_t MaxUsn{0xFFFFFFFFULL};
    uint64_t MaximumSize{0x8000000ULL};
    uint64_t AllocationDelta{0x1000000ULL};
    uint16_t MinSupportedMajorVersion{2};
    uint16_t MaxSupportedMajorVersion{4};
    uint32_t Flags{0};
    uint64_t RangeTrackChunkSize{0};
    int64_t  RangeTrackThreshold{0};
};

// Read Request Structure (supports versions 2, 3, 4)
struct READ_USN_JOURNAL_DATA_V1 {
    uint64_t StartUsn{0};
    uint32_t ReasonMask{0xFFFFFFFFu};
    uint32_t ReturnOnlyOnClose{0};
    uint64_t Timeout{0};
    uint64_t BytesToWaitFor{0};
    uint64_t UsnJournalID{0};
    uint16_t MinMajorVersion{2};
    uint16_t MaxMajorVersion{4};
};

// Delete Request Structure
struct DELETE_USN_JOURNAL_DATA {
    uint64_t UsnJournalID{0};
    uint32_t DeleteFlags{USN_DELETE_FLAG_DELETE};
};

// Event structure passed to registered real-time filters
struct SovereignUsnEvent {
    uint64_t                      Usn{0};
    uint16_t                      MajorVersion{2};
    FILE_ID_128                   FileId{};
    FILE_ID_128                   ParentFileId{};
    uint64_t                      TimeStamp{0};
    uint32_t                      Reason{0};
    uint32_t                      SourceInfo{0};
    uint32_t                      SecurityId{0};
    uint32_t                      FileAttributes{0};
    std::wstring                  FileName;
    std::vector<USN_RECORD_EXTENT> Extents;
};

using UsnFilterCallback = std::function<void(const SovereignUsnEvent& event)>;

struct RegisteredUsnFilter {
    uint64_t          RegistrationId{0};
    uint32_t          Altitude{0};
    uint32_t          ReasonMask{0xFFFFFFFFu};
    UsnFilterCallback Callback;
};

// ============================================================================
// SovereignNtfsUsnStreamer Class
// ============================================================================

class SovereignNtfsUsnStreamer {
public:
    SovereignNtfsUsnStreamer();
    ~SovereignNtfsUsnStreamer();

    // Journal Life-Cycle Management
    NtStatus createJournal(const CREATE_USN_JOURNAL_DATA& config);
    NtStatus queryJournal(USN_JOURNAL_DATA_V0& outData) const;
    NtStatus queryJournal(USN_JOURNAL_DATA_V1& outData) const;
    NtStatus queryJournal(USN_JOURNAL_DATA_V2& outData) const;
    NtStatus deleteJournal(const DELETE_USN_JOURNAL_DATA& deleteData);

    // Record Posting (v2, v3, v4)
    uint64_t postRecordV2(
        uint64_t frn,
        uint64_t parentFrn,
        uint32_t reason,
        uint32_t attributes,
        std::wstring_view fileName,
        uint32_t sourceInfo = 0,
        uint32_t securityId = 0
    );

    uint64_t postRecordV3(
        const FILE_ID_128& fileId,
        const FILE_ID_128& parentFileId,
        uint32_t reason,
        uint32_t attributes,
        std::wstring_view fileName,
        uint32_t sourceInfo = 0,
        uint32_t securityId = 0
    );

    uint64_t postRecordV4(
        const FILE_ID_128& fileId,
        const FILE_ID_128& parentFileId,
        uint32_t reason,
        const std::vector<USN_RECORD_EXTENT>& extents,
        uint32_t sourceInfo = 0
    );

    // Synchronous & Asynchronous Range Streaming
    NtStatus readJournal(
        const READ_USN_JOURNAL_DATA_V1& readParams,
        std::vector<uint8_t>& outBuffer,
        uint64_t& nextUsnBookmark
    ) const;

    NtStatus readJournalAsync(
        const READ_USN_JOURNAL_DATA_V1& readParams,
        std::vector<uint8_t>& outBuffer,
        uint64_t& nextUsnBookmark,
        uint32_t timeoutMs = 0
    );

    // Real-Time Filter Interlock (Filter Manager / FLTMGR altitude stacking)
    uint64_t registerFilter(uint32_t altitude, uint32_t reasonMask, UsnFilterCallback callback);
    void unregisterFilter(uint64_t registrationId);

    // Sparse Stream Truncation & Hole Punching
    uint64_t pruneJournalSparse();

    // Diagnostics & Metrics
    bool     isActive() const;
    uint64_t getRecordCount() const;
    uint64_t getLowestValidUsn() const;
    uint64_t getNextUsn() const;
    uint64_t getJournalId() const;
    uint64_t getPruneCount() const;
    uint64_t getActiveByteSpan() const;

private:
    mutable std::shared_mutex               m_mutex;
    std::condition_variable_any             m_cv;

    bool                                    m_active{false};
    bool                                    m_deleteInProgress{false};
    USN_JOURNAL_DATA_V1                     m_meta{};
    std::deque<SovereignUsnEvent>           m_records;
    std::vector<RegisteredUsnFilter>        m_filters;
    uint64_t                                m_nextFilterId{1};
    uint64_t                                m_pruneCount{0};

    void dispatchFilters(const SovereignUsnEvent& event);
    void checkAutoPruneLocked();
};

bool VerifyHorizon375NtfsUsnStreamer();

} // namespace micant::ntfs::usn
