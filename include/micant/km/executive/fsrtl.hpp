// ============================================================================
// MicaNT: Sovereign NT File System Runtime Library Architecture (fsrtl.hpp)
// Sovereign System Domain: TitanFsRtl / SovereignFsRtl
//
// Description:
//   Clean-room implementation of the Microsoft Windows NT File System Runtime
//   Library (FsRtl* subsystem in ntoskrnl.exe — base/ntos/fsrtl). Provides the
//   shared kernel runtime algorithms and data structures required by all
//   Installable File System (IFS) drivers (NTFS, EmeraldFS, FastFAT, ReFS,
//   CDFS) and network redirectors (MUP / RDBSS / SMB2):
//
//     1. Advanced FCB Header & Per-Stream / Per-FileObject Filter Contexts
//        (FSRTL_COMMON_FCB_HEADER, FSRTL_ADVANCED_FCB_HEADER,
//         FsRtlSetupAdvancedHeader, FsRtlInsertPerStreamContext,
//         FsRtlLookupPerStreamContextInternal, FsRtlRemovePerStreamContext,
//         FsRtlTeardownPerStreamContexts, FsRtlInsertPerFileObjectContext,
//         FsRtlLookupPerFileObjectContext, FsRtlRemovePerFileObjectContext)
//     2. Large & Base Map Control Blocks (LARGE_MCB / BASE_MCB — VBN-to-LBN
//        Extent Run Mapping, contiguous run coalescing, sparse hole tracking,
//        run splitting, truncation, and hole punching)
//     3. Byte-Range File Locking Engine (FILE_LOCK, FILE_LOCK_INFO,
//        FsRtlInitializeFileLock, FsRtlFastLock, FsRtlFastUnlockSingle,
//        FsRtlFastUnlockAll, FsRtlFastUnlockAllByKey,
//        FsRtlFastCheckLockForRead, FsRtlFastCheckLockForWrite,
//        FsRtlGetNextFileLock)
//     4. Opportunistic Locks (Oplocks — Legacy Level 1 / Level 2 / Batch /
//        Filter & Windows 7+ Atomic R / RH / RW / RWH Cache-Coherency State
//        Machine, FsRtlInitializeOplock, FsRtlOplockFsctrl, FsRtlOplockFsctrlEx,
//        FsRtlCheckOplock, FsRtlCheckOplockEx, FsRtlOplockBreakToNone,
//        FsRtlOplockIsFastIoPossible)
//     5. Directory Change Notification Engine (NOTIFY_SYNC,
//        FsRtlNotifyInitializeSync, FsRtlNotifyFullChangeDirectory,
//        FsRtlNotifyFullReportChange, FsRtlNotifyCleanup,
//        FILE_NOTIFY_INFORMATION packing)
//     6. File Name Tunnel Cache (TUNNEL — 15-Second Safe-Save Metadata &
//        Short/Long Name Preservation, FsRtlInitializeTunnelCache,
//        FsRtlAddToTunnelCache, FsRtlFindInTunnelCache,
//        FsRtlDeleteKeyFromTunnelCache, FsRtlDeleteTunnelCache)
//     7. Wildcard Expression Engine & Path Dissection
//        (FsRtlIsNameInExpression, FsRtlDoesNameContainWildCards,
//         FsRtlAreNamesEqual, FsRtlDissectName — supports *, ?, DOS_STAR '<',
//         DOS_QM '>', DOS_DOT '"')
//     8. Fast I/O Copy Bridge (FsRtlCopyRead, FsRtlCopyWrite integrated with
//        FILE_LOCK, OPLOCK, and Horizon 42 CcFastCopyRead / CcFastCopyWrite)
//
// Clean-Room Engineering Reference & Standards:
//   - Microsoft Windows Driver Kit (WDK) Installable File System (IFS) Reference
//   - Microsoft Open win32metadata repository
//   - ISO/IEC 14882:2023 C++ Standard (Pure ISO C++23)
//   - Zero proprietary, decompiled, or leaked Microsoft source code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <array>
#include <unordered_map>
#include <map>
#include <deque>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <optional>
#include <span>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/native/janusldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/km/executive/io.hpp"
#include "micant/km/executive/cc.hpp"

namespace micant::km::fsrtl {

// ============================================================================
// 1. Standard NT File System Runtime Library Types & Constants
// ============================================================================

using BOOL             = int32_t;
using BOOLEAN          = uint8_t;
using UCHAR            = uint8_t;
using USHORT           = uint16_t;
using ULONG            = uint32_t;
using PULONG           = uint32_t*;
using LONGLONG         = int64_t;
using PLONGLONG        = int64_t*;
using ULONGLONG        = uint64_t;
using PVOID            = void*;
using LARGE_INTEGER    = micant::LargeInteger;
using PLARGE_INTEGER   = micant::LargeInteger*;
using PIO_STATUS_BLOCK = micant::IoStatusBlock*;
using VBN              = int64_t;
using LBN              = int64_t;
using PVBN             = int64_t*;
using PLBN             = int64_t*;

inline constexpr BOOL    TRUE_VAL   = 1;
inline constexpr BOOL    FALSE_VAL  = 0;
inline constexpr BOOLEAN TRUE_B     = 1;
inline constexpr BOOLEAN FALSE_B    = 0;

// Unused / Sparse Hole LBN Sentinel in LARGE_MCB
inline constexpr LONGLONG FSRTL_UNUSED_LBN = -1LL;

// Special DOS Wildcard Characters (NT IFS Specification)
inline constexpr wchar_t DOS_STAR   = L'<'; // Matches 0+ chars up to last '.'
inline constexpr wchar_t DOS_QM     = L'>'; // Matches any single non-'.' char, or end/dot
inline constexpr wchar_t DOS_DOT    = L'"'; // Matches '.' or end-of-name

// Fast I/O Possibility States (FAST_IO_POSSIBLE)
enum class FAST_IO_POSSIBLE : uint8_t {
    FastIoIsNotPossible  = 0,
    FastIoIsPossible     = 1,
    FastIoIsQuestionable = 2
};

// FSRTL Common FCB Header Flags
inline constexpr uint8_t FSRTL_FLAG_FILE_MODIFIED        = 0x01;
inline constexpr uint8_t FSRTL_FLAG_FILE_LENGTH_CHANGED  = 0x02;
inline constexpr uint8_t FSRTL_FLAG_LIMIT_MODIFIED_PAGES = 0x04;
inline constexpr uint8_t FSRTL_FLAG_ACQUIRE_MAIN_RSRC_EX = 0x08;
inline constexpr uint8_t FSRTL_FLAG_ACQUIRE_MAIN_RSRC_SH = 0x10;
inline constexpr uint8_t FSRTL_FLAG_USER_MAPPED_FILE     = 0x20;
inline constexpr uint8_t FSRTL_FLAG_ADVANCED_HEADER      = 0x40;
inline constexpr uint8_t FSRTL_FLAG_EOF_ADVANCE_ACTIVE   = 0x80;

inline constexpr uint8_t FSRTL_FCB_HEADER_V1             = 0x01;
inline constexpr uint8_t FSRTL_FCB_HEADER_V2             = 0x02;

// Oplock Cache Flags (Windows 7+ Enhanced Oplock Levels)
inline constexpr uint32_t OPLOCK_LEVEL_CACHE_READ        = 0x00000001; // 'R'
inline constexpr uint32_t OPLOCK_LEVEL_CACHE_HANDLE      = 0x00000002; // 'H'
inline constexpr uint32_t OPLOCK_LEVEL_CACHE_WRITE       = 0x00000004; // 'W'

// Composite Atomic Oplock Levels
inline constexpr uint32_t OPLOCK_LEVEL_NONE              = 0x00000000;
inline constexpr uint32_t OPLOCK_LEVEL_R                 = OPLOCK_LEVEL_CACHE_READ;
inline constexpr uint32_t OPLOCK_LEVEL_RH                = OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE;
inline constexpr uint32_t OPLOCK_LEVEL_RW                = OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_WRITE;
inline constexpr uint32_t OPLOCK_LEVEL_RWH               = OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE | OPLOCK_LEVEL_CACHE_WRITE;

// Oplock FSCTL Codes (CTL_CODE(FILE_DEVICE_FILE_SYSTEM=9, ...))
inline constexpr uint32_t FSCTL_REQUEST_OPLOCK_LEVEL_1   = 0x00090000;
inline constexpr uint32_t FSCTL_REQUEST_OPLOCK_LEVEL_2   = 0x00090004;
inline constexpr uint32_t FSCTL_REQUEST_BATCH_OPLOCK     = 0x00090008;
inline constexpr uint32_t FSCTL_OPLOCK_BREAK_ACKNOWLEDGE = 0x0009000C;
inline constexpr uint32_t FSCTL_OPBATCH_ACK_CLOSE_PENDING= 0x00090010;
inline constexpr uint32_t FSCTL_OPLOCK_BREAK_NOTIFY      = 0x00090014;
inline constexpr uint32_t FSCTL_REQUEST_FILTER_OPLOCK    = 0x0009005C;
inline constexpr uint32_t FSCTL_OPLOCK_BREAK_ACK_NO_2    = 0x00090050;
inline constexpr uint32_t FSCTL_REQUEST_OPLOCK           = 0x00090240;

// REQUEST_OPLOCK_INPUT_BUFFER Flags
inline constexpr uint32_t REQUEST_OPLOCK_INPUT_FLAG_REQUEST = 0x00000001;
inline constexpr uint32_t REQUEST_OPLOCK_INPUT_FLAG_ACK     = 0x00000002;

// REQUEST_OPLOCK_OUTPUT_BUFFER Flags
inline constexpr uint32_t REQUEST_OPLOCK_OUTPUT_FLAG_ACK_REQUIRED      = 0x00000001;
inline constexpr uint32_t REQUEST_OPLOCK_OUTPUT_FLAG_MODES_PROVIDED    = 0x00000002;

// Directory Change Notification Filter Flags (FILE_NOTIFY_CHANGE_*)
inline constexpr uint32_t FILE_NOTIFY_CHANGE_FILE_NAME    = 0x00000001;
inline constexpr uint32_t FILE_NOTIFY_CHANGE_DIR_NAME     = 0x00000002;
inline constexpr uint32_t FILE_NOTIFY_CHANGE_NAME         = 0x00000003;
inline constexpr uint32_t FILE_NOTIFY_CHANGE_ATTRIBUTES   = 0x00000004;
inline constexpr uint32_t FILE_NOTIFY_CHANGE_SIZE         = 0x00000008;
inline constexpr uint32_t FILE_NOTIFY_CHANGE_LAST_WRITE   = 0x00000010;
inline constexpr uint32_t FILE_NOTIFY_CHANGE_LAST_ACCESS  = 0x00000020;
inline constexpr uint32_t FILE_NOTIFY_CHANGE_CREATION     = 0x00000040;
inline constexpr uint32_t FILE_NOTIFY_CHANGE_EA           = 0x00000080;
inline constexpr uint32_t FILE_NOTIFY_CHANGE_SECURITY     = 0x00000100;
inline constexpr uint32_t FILE_NOTIFY_CHANGE_STREAM_NAME  = 0x00000200;
inline constexpr uint32_t FILE_NOTIFY_CHANGE_STREAM_SIZE  = 0x00000400;
inline constexpr uint32_t FILE_NOTIFY_CHANGE_STREAM_WRITE = 0x00000800;

// Directory Change Notification Action Codes (FILE_ACTION_*)
inline constexpr uint32_t FILE_ACTION_ADDED               = 0x00000001;
inline constexpr uint32_t FILE_ACTION_REMOVED             = 0x00000002;
inline constexpr uint32_t FILE_ACTION_MODIFIED            = 0x00000003;
inline constexpr uint32_t FILE_ACTION_RENAMED_OLD_NAME    = 0x00000004;
inline constexpr uint32_t FILE_ACTION_RENAMED_NEW_NAME    = 0x00000005;
inline constexpr uint32_t FILE_ACTION_ADDED_STREAM        = 0x00000006;
inline constexpr uint32_t FILE_ACTION_REMOVED_STREAM      = 0x00000007;
inline constexpr uint32_t FILE_ACTION_MODIFIED_STREAM     = 0x00000008;

// Default Tunnel Cache Expiration Window (15 seconds = 15,000 ms in NT)
inline constexpr uint64_t DEFAULT_TUNNEL_CACHE_TIMEOUT_MS = 15000;

// ============================================================================
// 2. Advanced FCB Header & Per-Stream / Per-FileObject Context Structures
// ============================================================================

using PFSRTL_EXTRA_CREATE_PARAMETER_CLEANUP_CALLBACK = void (*)(PVOID Context);

struct FSRTL_PER_STREAM_CONTEXT {
    PVOID OwnerId{nullptr};
    PVOID InstanceId{nullptr};
    PFSRTL_EXTRA_CREATE_PARAMETER_CLEANUP_CALLBACK FreeCallback{nullptr};
};

struct FSRTL_PER_FILEOBJECT_CONTEXT {
    PVOID OwnerId{nullptr};
    PVOID InstanceId{nullptr};
    PFSRTL_EXTRA_CREATE_PARAMETER_CLEANUP_CALLBACK FreeCallback{nullptr};
};

struct FSRTL_ADVANCED_FCB_HEADER {
    int16_t          NodeTypeCode{0x0502};
    int16_t          NodeByteSize{sizeof(FSRTL_ADVANCED_FCB_HEADER)};
    uint8_t          Flags{FSRTL_FLAG_ADVANCED_HEADER};
    uint8_t          IsFastIoPossible{static_cast<uint8_t>(FAST_IO_POSSIBLE::FastIoIsPossible)};
    uint8_t          Flags2{0};
    uint8_t          Version{FSRTL_FCB_HEADER_V2};
    PVOID            Resource{nullptr};
    PVOID            PagingIoResource{nullptr};
    LARGE_INTEGER    AllocationSize{};
    LARGE_INTEGER    FileSize{};
    LARGE_INTEGER    ValidDataLength{};
    PVOID            FastMutex{nullptr};
    PVOID            FileContextSupportPointer{nullptr};
    PVOID            Oplock{nullptr};
    std::vector<FSRTL_PER_STREAM_CONTEXT*> StreamContexts{};
};

using FSRTL_COMMON_FCB_HEADER = FSRTL_ADVANCED_FCB_HEADER;

// ============================================================================
// 3. Large & Base Map Control Blocks (LARGE_MCB / BASE_MCB)
// ============================================================================

struct MCB_MAPPING_RUN {
    LONGLONG StartVbn{0};
    LONGLONG StartLbn{FSRTL_UNUSED_LBN}; // FSRTL_UNUSED_LBN (-1) indicates a sparse hole
    LONGLONG SectorCount{0};
};

struct BASE_MCB {
    uint32_t MaximumPairCount{16};
    uint32_t PairCount{0};
    uint16_t PoolType{0};
    uint16_t Flags{0};
    std::vector<MCB_MAPPING_RUN> Runs{};
};

struct LARGE_MCB {
    PVOID    GuardedMutex{nullptr};
    BASE_MCB BaseMcb{};
};

// ============================================================================
// 4. Byte-Range File Locking (FILE_LOCK & FILE_LOCK_INFO)
// ============================================================================

struct FILE_LOCK_INFO {
    LARGE_INTEGER StartingByte{};
    LARGE_INTEGER Length{};
    BOOLEAN       ExclusiveLock{FALSE_B};
    ULONG         Key{0};
    PVOID         FileObject{nullptr};
    PVOID         ProcessId{nullptr};
    LARGE_INTEGER EndingByte{};
};

using PCOMPLETE_LOCK_IRP_ROUTINE = NtStatus (*)(PVOID Context, io::Irp* Irp);
using PUNLOCK_ROUTINE            = void (*)(PVOID Context, FILE_LOCK_INFO* FileLockInfo);

struct FILE_LOCK {
    PCOMPLETE_LOCK_IRP_ROUTINE CompleteLockIrpRoutine{nullptr};
    PUNLOCK_ROUTINE            UnlockRoutine{nullptr};
    BOOLEAN                    FastIoIsQuestionable{FALSE_B};
    uint8_t                    Spare[3]{0, 0, 0};
    std::vector<FILE_LOCK_INFO> ActiveLocks{};
    size_t                     EnumerationCursor{0};
    bool                       Initialized{false};
};

// ============================================================================
// 5. Opportunistic Locks (OPLOCK — Legacy & Windows 7+ Enhanced R/RH/RW/RWH)
// ============================================================================

struct REQUEST_OPLOCK_INPUT_BUFFER {
    uint16_t StructureVersion{1};
    uint16_t StructureLength{sizeof(REQUEST_OPLOCK_INPUT_BUFFER)};
    uint32_t RequestedOplockLevel{OPLOCK_LEVEL_NONE};
    uint32_t Flags{REQUEST_OPLOCK_INPUT_FLAG_REQUEST};
};

struct REQUEST_OPLOCK_OUTPUT_BUFFER {
    uint16_t StructureVersion{1};
    uint16_t StructureLength{sizeof(REQUEST_OPLOCK_OUTPUT_BUFFER)};
    uint32_t OriginalOplockLevel{OPLOCK_LEVEL_NONE};
    uint32_t NewOplockLevel{OPLOCK_LEVEL_NONE};
    uint32_t Flags{0};
    uint32_t AccessMode{0};
    uint16_t ShareMode{0};
};

using POPLOCK_WAIT_COMPLETE_ROUTINE = void (*)(PVOID Context, io::Irp* Irp);
using POPLOCK_FS_PREPOST_IRP        = void (*)(PVOID Context, io::Irp* Irp);

struct OPLOCK_GRANT {
    PVOID    FileObject{nullptr};
    uint64_t OplockKey{0};
    uint32_t Level{OPLOCK_LEVEL_NONE}; // Bitmask of OPLOCK_LEVEL_CACHE_{READ,HANDLE,WRITE}
    bool     IsLegacyBatch{false};
    bool     IsLegacyFilter{false};
    bool     IsLegacyLevel1{false};
    bool     IsLegacyLevel2{false};
};

struct OPLOCK_STATE {
    uint32_t                  CurrentLevel{OPLOCK_LEVEL_NONE};
    bool                      BreakInProgress{false};
    uint32_t                  BreakTargetLevel{OPLOCK_LEVEL_NONE};
    uint32_t                  PendingBreakOriginLevel{OPLOCK_LEVEL_NONE};
    std::vector<OPLOCK_GRANT> Grants{};
};

using OPLOCK = OPLOCK_STATE*;

// ============================================================================
// 6. Directory Change Notification Structures (NOTIFY_SYNC & FILE_NOTIFY_INFORMATION)
// ============================================================================

#pragma pack(push, 1)
struct FILE_NOTIFY_INFORMATION {
    uint32_t NextEntryOffset{0};
    uint32_t Action{0};
    uint32_t FileNameLength{0}; // In bytes
    wchar_t  FileName[1]{L'\0'};
};
#pragma pack(pop)

struct NOTIFY_CHANGE_WATCHER {
    PVOID         NotifyContext{nullptr}; // Per-handle CCB identifier
    PVOID         FsContext{nullptr};     // Directory FCB identifier
    std::wstring  FullDirectoryName{};
    BOOLEAN       WatchTree{FALSE_B};
    uint32_t      CompletionFilter{0};
    PVOID         OutputBuffer{nullptr};
    uint32_t      OutputBufferLength{0};
    uint32_t      BytesWritten{0};
    bool          BufferOverflowed{false};
    bool          Completed{false};
    bool          CleanedUp{false};
    NtStatus      CompletionStatus{NtStatus::Pending};
    std::vector<std::pair<uint32_t, std::wstring>> PendingEvents{};
};

struct NOTIFY_SYNC_STATE {
    std::mutex                         Mutex{};
    uint32_t                           LockAcquireCount{0};
    std::vector<NOTIFY_CHANGE_WATCHER> Watchers{};
};

using PNOTIFY_SYNC = NOTIFY_SYNC_STATE*;

// ============================================================================
// 7. File Name Tunnel Cache Structures (TUNNEL)
// ============================================================================

struct TUNNEL_CACHE_NODE {
    ULONGLONG            DirectoryKey{0};
    std::wstring         ShortName{};
    std::wstring         LongName{};
    BOOLEAN              KeyByShortName{FALSE_B};
    std::vector<uint8_t> ExtraData{};
    uint64_t             InsertTickMs{0};
};

struct TUNNEL {
    std::mutex                     Mutex{};
    std::vector<TUNNEL_CACHE_NODE> Entries{};
    uint64_t                       ExpirationWindowMs{DEFAULT_TUNNEL_CACHE_TIMEOUT_MS};
    uint64_t                       TotalHits{0};
    uint64_t                       TotalMisses{0};
};

// ============================================================================
// 8. FsRtl Telemetry Summary
// ============================================================================

struct FSRTL_TELEMETRY {
    uint64_t TotalMcbRunsAdded{0};
    uint64_t TotalMcbLookups{0};
    uint64_t TotalMcbSplits{0};
    uint64_t TotalByteRangeLocksGranted{0};
    uint64_t TotalByteRangeLockConflicts{0};
    uint64_t TotalByteRangeUnlocks{0};
    uint64_t TotalOplockRequests{0};
    uint64_t TotalOplockBreaks{0};
    uint64_t TotalNotifyWatchersRegistered{0};
    uint64_t TotalNotifyEventsReported{0};
    uint64_t TotalTunnelEntriesAdded{0};
    uint64_t TotalTunnelCacheHits{0};
    uint64_t TotalWildcardEvaluations{0};
    uint64_t TotalFastIoReads{0};
    uint64_t TotalFastIoWrites{0};
};

// ============================================================================
// 9. Sovereign FsRtl Engine (TitanFsRtlManager)
// ============================================================================

class TitanFsRtlManager {
public:
    static TitanFsRtlManager& Instance() noexcept {
        static TitanFsRtlManager s_instance;
        return s_instance;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_initialized = true;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fileObjectContexts.clear();
        m_telemetry = FSRTL_TELEMETRY{};
        m_currentTickMs = 100000;
        m_initialized = true;
    }

    void advanceSimulatedClockMs(uint64_t deltaMs) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentTickMs += deltaMs;
    }

    uint64_t currentTickMs() const noexcept {
        return m_currentTickMs;
    }

    // ------------------------------------------------------------------------
    // 9.1 Per-Stream & Per-FileObject Contexts
    // ------------------------------------------------------------------------

    NtStatus insertPerStreamContext(FSRTL_ADVANCED_FCB_HEADER* header, FSRTL_PER_STREAM_CONTEXT* ctx) {
        if (!header || !ctx) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(m_mutex);
        header->StreamContexts.push_back(ctx);
        return NtStatus::Success;
    }

    FSRTL_PER_STREAM_CONTEXT* lookupPerStreamContext(
        FSRTL_ADVANCED_FCB_HEADER* header,
        PVOID ownerId,
        PVOID instanceId
    ) {
        if (!header) return nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto* ctx : header->StreamContexts) {
            if (!ctx) continue;
            if (ownerId && ctx->OwnerId != ownerId) continue;
            if (instanceId && ctx->InstanceId != instanceId) continue;
            return ctx;
        }
        return nullptr;
    }

    FSRTL_PER_STREAM_CONTEXT* removePerStreamContext(
        FSRTL_ADVANCED_FCB_HEADER* header,
        PVOID ownerId,
        PVOID instanceId
    ) {
        if (!header) return nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = header->StreamContexts.begin(); it != header->StreamContexts.end(); ++it) {
            auto* ctx = *it;
            if (!ctx) continue;
            if (ownerId && ctx->OwnerId != ownerId) continue;
            if (instanceId && ctx->InstanceId != instanceId) continue;
            header->StreamContexts.erase(it);
            return ctx;
        }
        return nullptr;
    }

    void teardownPerStreamContexts(FSRTL_ADVANCED_FCB_HEADER* header) {
        if (!header) return;
        std::vector<FSRTL_PER_STREAM_CONTEXT*> toFree;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            toFree.swap(header->StreamContexts);
        }
        for (auto* ctx : toFree) {
            if (ctx && ctx->FreeCallback) {
                ctx->FreeCallback(ctx);
            }
        }
    }

    NtStatus insertPerFileObjectContext(PVOID fileObject, FSRTL_PER_FILEOBJECT_CONTEXT* ctx) {
        if (!fileObject || !ctx) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fileObjectContexts[fileObject].push_back(ctx);
        return NtStatus::Success;
    }

    FSRTL_PER_FILEOBJECT_CONTEXT* lookupPerFileObjectContext(
        PVOID fileObject,
        PVOID ownerId,
        PVOID instanceId
    ) {
        if (!fileObject) return nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_fileObjectContexts.find(fileObject);
        if (it == m_fileObjectContexts.end()) return nullptr;
        for (auto* ctx : it->second) {
            if (!ctx) continue;
            if (ownerId && ctx->OwnerId != ownerId) continue;
            if (instanceId && ctx->InstanceId != instanceId) continue;
            return ctx;
        }
        return nullptr;
    }

    FSRTL_PER_FILEOBJECT_CONTEXT* removePerFileObjectContext(
        PVOID fileObject,
        PVOID ownerId,
        PVOID instanceId
    ) {
        if (!fileObject) return nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_fileObjectContexts.find(fileObject);
        if (it == m_fileObjectContexts.end()) return nullptr;
        auto& vec = it->second;
        for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
            auto* ctx = *vit;
            if (!ctx) continue;
            if (ownerId && ctx->OwnerId != ownerId) continue;
            if (instanceId && ctx->InstanceId != instanceId) continue;
            vec.erase(vit);
            return ctx;
        }
        return nullptr;
    }

    // ------------------------------------------------------------------------
    // 9.2 Large Map Control Block (LARGE_MCB / BASE_MCB) Engine
    // ------------------------------------------------------------------------

    void initializeBaseMcb(BASE_MCB* mcb, uint16_t poolType) {
        if (!mcb) return;
        mcb->MaximumPairCount = 16;
        mcb->PairCount        = 0;
        mcb->PoolType         = poolType;
        mcb->Flags            = 0;
        mcb->Runs.clear();
    }

    void uninitializeBaseMcb(BASE_MCB* mcb) {
        if (!mcb) return;
        mcb->Runs.clear();
        mcb->PairCount = 0;
    }

    void resetBaseMcb(BASE_MCB* mcb) {
        if (!mcb) return;
        mcb->Runs.clear();
        mcb->PairCount = 0;
    }

    BOOLEAN addBaseMcbEntry(BASE_MCB* mcb, LONGLONG vbn, LONGLONG lbn, LONGLONG sectorCount) {
        if (!mcb || vbn < 0 || sectorCount < 0) return FALSE_B;
        if (sectorCount == 0) return TRUE_B;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.TotalMcbRunsAdded++;

        // First remove any existing mapping in [vbn, vbn + sectorCount)
        removeBaseMcbRangeUnlocked(mcb, vbn, sectorCount);

        // Find highest existing VBN to insert sparse holes if there is a gap
        LONGLONG endOfLastRun = 0;
        if (!mcb->Runs.empty()) {
            const auto& last = mcb->Runs.back();
            endOfLastRun = last.StartVbn + last.SectorCount;
        }

        if (vbn > endOfLastRun) {
            MCB_MAPPING_RUN hole{};
            hole.StartVbn    = endOfLastRun;
            hole.StartLbn    = FSRTL_UNUSED_LBN;
            hole.SectorCount = vbn - endOfLastRun;
            mcb->Runs.push_back(hole);
        }

        MCB_MAPPING_RUN newRun{};
        newRun.StartVbn    = vbn;
        newRun.StartLbn    = lbn;
        newRun.SectorCount = sectorCount;
        mcb->Runs.push_back(newRun);

        normalizeMcbRunsUnlocked(mcb);
        return TRUE_B;
    }

    void removeBaseMcbEntry(BASE_MCB* mcb, LONGLONG vbn, LONGLONG sectorCount) {
        if (!mcb || vbn < 0 || sectorCount <= 0) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        removeBaseMcbRangeUnlocked(mcb, vbn, sectorCount);
        // Replace removed range with a sparse hole if there are runs after it
        LONGLONG maxVbnAfter = 0;
        for (const auto& r : mcb->Runs) {
            maxVbnAfter = std::max(maxVbnAfter, r.StartVbn + r.SectorCount);
        }
        if (vbn < maxVbnAfter) {
            LONGLONG holeLen = std::min(sectorCount, maxVbnAfter - vbn);
            MCB_MAPPING_RUN hole{};
            hole.StartVbn    = vbn;
            hole.StartLbn    = FSRTL_UNUSED_LBN;
            hole.SectorCount = holeLen;
            mcb->Runs.push_back(hole);
        }
        normalizeMcbRunsUnlocked(mcb);
    }

    BOOLEAN lookupBaseMcbEntry(
        BASE_MCB* mcb,
        LONGLONG vbn,
        PLONGLONG lbn,
        PLONGLONG sectorCountFromLbn,
        PLONGLONG startingLbn,
        PLONGLONG countFromStartingLbn,
        PULONG index
    ) {
        if (!mcb || vbn < 0) return FALSE_B;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.TotalMcbLookups++;

        for (size_t i = 0; i < mcb->Runs.size(); ++i) {
            const auto& r = mcb->Runs[i];
            if (vbn >= r.StartVbn && vbn < (r.StartVbn + r.SectorCount)) {
                LONGLONG offsetInRun = vbn - r.StartVbn;
                LONGLONG remaining   = r.SectorCount - offsetInRun;
                if (lbn) {
                    *lbn = (r.StartLbn == FSRTL_UNUSED_LBN) ? FSRTL_UNUSED_LBN : (r.StartLbn + offsetInRun);
                }
                if (sectorCountFromLbn) *sectorCountFromLbn = remaining;
                if (startingLbn) *startingLbn = r.StartLbn;
                if (countFromStartingLbn) *countFromStartingLbn = r.SectorCount;
                if (index) *index = static_cast<ULONG>(i);
                return TRUE_B;
            }
        }
        return FALSE_B;
    }

    BOOLEAN lookupLastBaseMcbEntryAndIndex(
        BASE_MCB* mcb,
        PLONGLONG vbn,
        PLONGLONG lbn,
        PULONG index
    ) {
        if (!mcb || mcb->Runs.empty()) return FALSE_B;
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto& last = mcb->Runs.back();
        if (vbn) *vbn = last.StartVbn + last.SectorCount - 1;
        if (lbn) {
            *lbn = (last.StartLbn == FSRTL_UNUSED_LBN)
                ? FSRTL_UNUSED_LBN
                : (last.StartLbn + last.SectorCount - 1);
        }
        if (index) *index = static_cast<ULONG>(mcb->Runs.size() - 1);
        return TRUE_B;
    }

    ULONG numberOfRunsInBaseMcb(BASE_MCB* mcb) {
        if (!mcb) return 0;
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<ULONG>(mcb->Runs.size());
    }

    BOOLEAN getNextBaseMcbEntry(
        BASE_MCB* mcb,
        ULONG runIndex,
        PLONGLONG vbn,
        PLONGLONG lbn,
        PLONGLONG sectorCount
    ) {
        if (!mcb) return FALSE_B;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (runIndex >= mcb->Runs.size()) return FALSE_B;
        const auto& r = mcb->Runs[runIndex];
        if (vbn) *vbn = r.StartVbn;
        if (lbn) *lbn = r.StartLbn;
        if (sectorCount) *sectorCount = r.SectorCount;
        return TRUE_B;
    }

    void truncateBaseMcb(BASE_MCB* mcb, LONGLONG vbn) {
        if (!mcb || vbn < 0) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (vbn == 0) {
            mcb->Runs.clear();
            mcb->PairCount = 0;
            return;
        }
        std::vector<MCB_MAPPING_RUN> kept;
        for (auto r : mcb->Runs) {
            if (r.StartVbn >= vbn) continue;
            if (r.StartVbn + r.SectorCount > vbn) {
                r.SectorCount = vbn - r.StartVbn;
            }
            if (r.SectorCount > 0) {
                kept.push_back(r);
            }
        }
        mcb->Runs.swap(kept);
        normalizeMcbRunsUnlocked(mcb);
    }

    BOOLEAN splitBaseMcb(BASE_MCB* mcb, LONGLONG vbn, LONGLONG amount) {
        if (!mcb || vbn < 0 || amount <= 0) return FALSE_B;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.TotalMcbSplits++;

        std::vector<MCB_MAPPING_RUN> updated;
        bool insertedHole = false;

        for (const auto& r : mcb->Runs) {
            LONGLONG rEnd = r.StartVbn + r.SectorCount;
            if (rEnd <= vbn) {
                updated.push_back(r);
            } else if (r.StartVbn < vbn && rEnd > vbn) {
                // Split existing run across vbn
                LONGLONG leftLen  = vbn - r.StartVbn;
                LONGLONG rightLen = rEnd - vbn;

                MCB_MAPPING_RUN leftRun = r;
                leftRun.SectorCount = leftLen;
                updated.push_back(leftRun);

                MCB_MAPPING_RUN hole{};
                hole.StartVbn    = vbn;
                hole.StartLbn    = FSRTL_UNUSED_LBN;
                hole.SectorCount = amount;
                updated.push_back(hole);
                insertedHole = true;

                MCB_MAPPING_RUN rightRun = r;
                rightRun.StartVbn    = vbn + amount;
                rightRun.StartLbn    = (r.StartLbn == FSRTL_UNUSED_LBN) ? FSRTL_UNUSED_LBN : (r.StartLbn + leftLen);
                rightRun.SectorCount = rightLen;
                updated.push_back(rightRun);
            } else {
                if (!insertedHole) {
                    MCB_MAPPING_RUN hole{};
                    hole.StartVbn    = vbn;
                    hole.StartLbn    = FSRTL_UNUSED_LBN;
                    hole.SectorCount = amount;
                    updated.push_back(hole);
                    insertedHole = true;
                }
                MCB_MAPPING_RUN shifted = r;
                shifted.StartVbn += amount;
                updated.push_back(shifted);
            }
        }

        mcb->Runs.swap(updated);
        normalizeMcbRunsUnlocked(mcb);
        return TRUE_B;
    }

    // ------------------------------------------------------------------------
    // 9.3 Byte-Range File Locking (FILE_LOCK)
    // ------------------------------------------------------------------------

    BOOLEAN fastLock(
        FILE_LOCK* fileLock,
        PVOID fileObject,
        PLARGE_INTEGER fileOffset,
        PLARGE_INTEGER length,
        PVOID processId,
        ULONG key,
        BOOLEAN failImmediately,
        BOOLEAN exclusiveLock,
        PIO_STATUS_BLOCK ioStatus,
        PVOID context
    ) {
        (void)failImmediately;
        (void)context;
        if (!fileLock || !fileOffset || !length) {
            if (ioStatus) ioStatus->status = NtStatus::InvalidParameter;
            return FALSE_B;
        }

        LONGLONG start = fileOffset->quadPart;
        LONGLONG len   = length->quadPart;
        if (start < 0 || len < 0) {
            if (ioStatus) ioStatus->status = NtStatus::InvalidParameter;
            return FALSE_B;
        }
        LONGLONG end = (len > 0) ? (start + len - 1) : start;

        std::lock_guard<std::mutex> lock(m_mutex);

        // Zero-length locks only conflict with existing locks that strictly contain `start`
        for (const auto& existing : fileLock->ActiveLocks) {
            if (!rangesOverlap(start, len, existing.StartingByte.quadPart, existing.Length.quadPart)) {
                continue;
            }
            // In Windows NT:
            // - Two shared locks never conflict.
            // - An exclusive lock conflicts with ANY overlapping lock (even from the same process/key).
            // - A shared lock conflicts with ANY overlapping exclusive lock.
            if (exclusiveLock != FALSE_B || existing.ExclusiveLock != FALSE_B) {
                m_telemetry.TotalByteRangeLockConflicts++;
                if (ioStatus) {
                    ioStatus->status      = NtStatus::FileLockConflict;
                    ioStatus->information = 0;
                }
                return FALSE_B;
            }
        }

        FILE_LOCK_INFO info{};
        info.StartingByte.quadPart = start;
        info.Length.quadPart       = len;
        info.EndingByte.quadPart   = end;
        info.ExclusiveLock         = exclusiveLock ? TRUE_B : FALSE_B;
        info.Key                   = key;
        info.FileObject            = fileObject;
        info.ProcessId             = processId;

        fileLock->ActiveLocks.push_back(info);
        fileLock->FastIoIsQuestionable = TRUE_B;
        m_telemetry.TotalByteRangeLocksGranted++;

        if (ioStatus) {
            ioStatus->status      = NtStatus::Success;
            ioStatus->information = 0;
        }
        return TRUE_B;
    }

    NtStatus fastUnlockSingle(
        FILE_LOCK* fileLock,
        PVOID fileObject,
        PLARGE_INTEGER fileOffset,
        PLARGE_INTEGER length,
        PVOID processId,
        ULONG key,
        PVOID context
    ) {
        if (!fileLock || !fileOffset || !length) return NtStatus::InvalidParameter;
        LONGLONG start = fileOffset->quadPart;
        LONGLONG len   = length->quadPart;

        FILE_LOCK_INFO unlockedInfo{};
        PUNLOCK_ROUTINE unlockCb = nullptr;
        bool found = false;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto it = fileLock->ActiveLocks.begin(); it != fileLock->ActiveLocks.end(); ++it) {
                if (it->StartingByte.quadPart == start &&
                    it->Length.quadPart == len &&
                    it->FileObject == fileObject &&
                    it->ProcessId == processId &&
                    it->Key == key) {
                    unlockedInfo = *it;
                    unlockCb = fileLock->UnlockRoutine;
                    fileLock->ActiveLocks.erase(it);
                    m_telemetry.TotalByteRangeUnlocks++;
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            return NtStatus::RangeNotLocked;
        }
        if (unlockCb) {
            unlockCb(context, &unlockedInfo);
        }
        return NtStatus::Success;
    }

    NtStatus fastUnlockAll(
        FILE_LOCK* fileLock,
        PVOID fileObject,
        PVOID processId,
        std::optional<ULONG> keyFilter,
        PVOID context
    ) {
        if (!fileLock) return NtStatus::InvalidParameter;

        std::vector<FILE_LOCK_INFO> removed;
        PUNLOCK_ROUTINE unlockCb = nullptr;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            unlockCb = fileLock->UnlockRoutine;
            auto it = fileLock->ActiveLocks.begin();
            while (it != fileLock->ActiveLocks.end()) {
                bool match = (it->FileObject == fileObject && it->ProcessId == processId);
                if (match && keyFilter.has_value()) {
                    match = (it->Key == *keyFilter);
                }
                if (match) {
                    removed.push_back(*it);
                    it = fileLock->ActiveLocks.erase(it);
                    m_telemetry.TotalByteRangeUnlocks++;
                } else {
                    ++it;
                }
            }
        }

        if (unlockCb) {
            for (auto& info : removed) {
                unlockCb(context, &info);
            }
        }
        return NtStatus::Success;
    }

    BOOLEAN fastCheckLockForRead(
        FILE_LOCK* fileLock,
        PLARGE_INTEGER startingByte,
        PLARGE_INTEGER length,
        ULONG key,
        PVOID fileObject,
        PVOID processId
    ) {
        if (!fileLock || !startingByte || !length) return TRUE_B;
        LONGLONG start = startingByte->quadPart;
        LONGLONG len   = length->quadPart;
        if (len <= 0) return TRUE_B;

        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& existing : fileLock->ActiveLocks) {
            if (!rangesOverlap(start, len, existing.StartingByte.quadPart, existing.Length.quadPart)) {
                continue;
            }
            // Shared locks permit read access by anyone; exclusive locks only permit read by the exact owner
            if (existing.ExclusiveLock != FALSE_B) {
                if (existing.ProcessId != processId ||
                    existing.Key != key ||
                    existing.FileObject != fileObject) {
                    return FALSE_B;
                }
            }
        }
        return TRUE_B;
    }

    BOOLEAN fastCheckLockForWrite(
        FILE_LOCK* fileLock,
        PLARGE_INTEGER startingByte,
        PLARGE_INTEGER length,
        ULONG key,
        PVOID fileObject,
        PVOID processId
    ) {
        if (!fileLock || !startingByte || !length) return TRUE_B;
        LONGLONG start = startingByte->quadPart;
        LONGLONG len   = length->quadPart;
        if (len <= 0) return TRUE_B;

        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& existing : fileLock->ActiveLocks) {
            if (!rangesOverlap(start, len, existing.StartingByte.quadPart, existing.Length.quadPart)) {
                continue;
            }
            // Write conflicts with ANY shared lock (even owned by the same process!)
            if (existing.ExclusiveLock == FALSE_B) {
                return FALSE_B;
            }
            // Write conflicts with an exclusive lock unless owned by (processId, key, fileObject)
            if (existing.ProcessId != processId ||
                existing.Key != key ||
                existing.FileObject != fileObject) {
                return FALSE_B;
            }
        }
        return TRUE_B;
    }

    FILE_LOCK_INFO* getNextFileLock(FILE_LOCK* fileLock, BOOLEAN restart) {
        if (!fileLock) return nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (restart != FALSE_B) {
            fileLock->EnumerationCursor = 0;
        }
        if (fileLock->EnumerationCursor >= fileLock->ActiveLocks.size()) {
            return nullptr;
        }
        return &fileLock->ActiveLocks[fileLock->EnumerationCursor++];
    }

    // ------------------------------------------------------------------------
    // 9.4 Opportunistic Lock (OPLOCK) State Machine
    // ------------------------------------------------------------------------

    void initializeOplock(OPLOCK* oplock) {
        if (!oplock) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!*oplock) {
            *oplock = new OPLOCK_STATE();
        } else {
            (*oplock)->CurrentLevel            = OPLOCK_LEVEL_NONE;
            (*oplock)->BreakInProgress         = false;
            (*oplock)->BreakTargetLevel        = OPLOCK_LEVEL_NONE;
            (*oplock)->PendingBreakOriginLevel = OPLOCK_LEVEL_NONE;
            (*oplock)->Grants.clear();
        }
    }

    void uninitializeOplock(OPLOCK* oplock) {
        if (!oplock || !*oplock) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        delete *oplock;
        *oplock = nullptr;
    }

    NtStatus oplockFsctrl(
        OPLOCK* oplock,
        PVOID fileObject,
        uint64_t oplockKey,
        ULONG fsctlCode,
        const REQUEST_OPLOCK_INPUT_BUFFER* inputBuf,
        REQUEST_OPLOCK_OUTPUT_BUFFER* outputBuf
    ) {
        if (!oplock) return NtStatus::InvalidParameter;
        if (!*oplock) initializeOplock(oplock);

        std::lock_guard<std::mutex> lock(m_mutex);
        OPLOCK_STATE* st = *oplock;
        m_telemetry.TotalOplockRequests++;

        switch (fsctlCode) {
            case FSCTL_REQUEST_OPLOCK_LEVEL_1:
            case FSCTL_REQUEST_BATCH_OPLOCK:
            case FSCTL_REQUEST_FILTER_OPLOCK: {
                if (!st->Grants.empty() && st->Grants.front().OplockKey != oplockKey) {
                    return NtStatus::OplockNotGranted;
                }
                st->Grants.clear();
                OPLOCK_GRANT g{};
                g.FileObject     = fileObject;
                g.OplockKey      = oplockKey;
                g.IsLegacyLevel1 = (fsctlCode == FSCTL_REQUEST_OPLOCK_LEVEL_1);
                g.IsLegacyBatch  = (fsctlCode == FSCTL_REQUEST_BATCH_OPLOCK);
                g.IsLegacyFilter = (fsctlCode == FSCTL_REQUEST_FILTER_OPLOCK);
                g.Level          = (fsctlCode == FSCTL_REQUEST_OPLOCK_LEVEL_1)
                    ? OPLOCK_LEVEL_RW
                    : OPLOCK_LEVEL_RWH;
                st->Grants.push_back(g);
                st->CurrentLevel    = g.Level;
                st->BreakInProgress = false;
                return NtStatus::Pending; // In NT, granted oplock FSCTL stays pending until break
            }

            case FSCTL_REQUEST_OPLOCK_LEVEL_2: {
                // Shared Read oplock: incompatible if someone else holds Write
                if ((st->CurrentLevel & OPLOCK_LEVEL_CACHE_WRITE) != 0) {
                    return NtStatus::OplockNotGranted;
                }
                OPLOCK_GRANT g{};
                g.FileObject     = fileObject;
                g.OplockKey      = oplockKey;
                g.IsLegacyLevel2 = true;
                g.Level          = OPLOCK_LEVEL_R;
                st->Grants.push_back(g);
                st->CurrentLevel |= OPLOCK_LEVEL_R;
                return NtStatus::Pending;
            }

            case FSCTL_REQUEST_OPLOCK: {
                if (!inputBuf) return NtStatus::InvalidParameter;
                if ((inputBuf->Flags & REQUEST_OPLOCK_INPUT_FLAG_ACK) != 0) {
                    // Acknowledge an in-progress break
                    if (!st->BreakInProgress) return NtStatus::InvalidOplockProtocol;
                    st->CurrentLevel    = st->BreakTargetLevel;
                    st->BreakInProgress = false;
                    if (st->CurrentLevel == OPLOCK_LEVEL_NONE) {
                        st->Grants.clear();
                    } else {
                        for (auto& g : st->Grants) {
                            g.Level = st->CurrentLevel;
                        }
                    }
                    return NtStatus::Success;
                }

                uint32_t reqLevel = inputBuf->RequestedOplockLevel;
                if ((reqLevel & OPLOCK_LEVEL_CACHE_READ) == 0) {
                    return NtStatus::InvalidParameter;
                }
                // Exclusive Write ('W') requires no other incompatible oplock holders
                if ((reqLevel & OPLOCK_LEVEL_CACHE_WRITE) != 0) {
                    for (const auto& g : st->Grants) {
                        if (g.OplockKey != oplockKey) {
                            return NtStatus::OplockNotGranted;
                        }
                    }
                    st->Grants.clear();
                } else {
                    // Shared Read ('R' or 'RH') cannot be granted while another key holds 'W'
                    for (const auto& g : st->Grants) {
                        if (g.OplockKey != oplockKey && (g.Level & OPLOCK_LEVEL_CACHE_WRITE) != 0) {
                            return NtStatus::OplockNotGranted;
                        }
                    }
                }

                // Update or insert grant for this OplockKey
                bool updatedExisting = false;
                for (auto& g : st->Grants) {
                    if (g.OplockKey == oplockKey && (g.FileObject == fileObject || oplockKey != 0)) {
                        g.Level = reqLevel;
                        updatedExisting = true;
                        break;
                    }
                }
                if (!updatedExisting) {
                    OPLOCK_GRANT g{};
                    g.FileObject = fileObject;
                    g.OplockKey  = oplockKey;
                    g.Level      = reqLevel;
                    st->Grants.push_back(g);
                }

                recomputeCurrentOplockLevelUnlocked(st);
                if (outputBuf) {
                    outputBuf->OriginalOplockLevel = st->CurrentLevel;
                    outputBuf->NewOplockLevel      = st->CurrentLevel;
                    outputBuf->Flags               = 0;
                }
                return NtStatus::Pending;
            }

            case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
            case FSCTL_OPBATCH_ACK_CLOSE_PENDING:
            case FSCTL_OPLOCK_BREAK_ACK_NO_2: {
                if (!st->BreakInProgress) return NtStatus::InvalidOplockProtocol;
                if (fsctlCode == FSCTL_OPLOCK_BREAK_ACK_NO_2) {
                    st->BreakTargetLevel = OPLOCK_LEVEL_NONE;
                }
                st->CurrentLevel    = st->BreakTargetLevel;
                st->BreakInProgress = false;
                if (st->CurrentLevel == OPLOCK_LEVEL_NONE) {
                    st->Grants.clear();
                } else {
                    for (auto& g : st->Grants) g.Level = st->CurrentLevel;
                }
                return NtStatus::Success;
            }

            case FSCTL_OPLOCK_BREAK_NOTIFY:
                return st->BreakInProgress ? NtStatus::Pending : NtStatus::Success;

            default:
                return NtStatus::InvalidDeviceRequest;
        }
    }

    NtStatus checkOplock(
        OPLOCK* oplock,
        uint64_t callerOplockKey,
        uint32_t majorFunction, // IRP_MJ_CREATE, IRP_MJ_WRITE, IRP_MJ_SET_INFORMATION, etc.
        bool isConflictingShareOrDelete,
        REQUEST_OPLOCK_OUTPUT_BUFFER* outBreakNotification
    ) {
        if (!oplock || !*oplock) return NtStatus::Success;

        std::lock_guard<std::mutex> lock(m_mutex);
        OPLOCK_STATE* st = *oplock;
        if (st->CurrentLevel == OPLOCK_LEVEL_NONE || st->Grants.empty()) {
            return NtStatus::Success;
        }

        // Check if all active grants match callerOplockKey (non-zero dual-handle same-key access does not break!)
        bool foreignHolderExists = false;
        for (const auto& g : st->Grants) {
            if (callerOplockKey == 0 || g.OplockKey != callerOplockKey) {
                foreignHolderExists = true;
                break;
            }
        }
        if (!foreignHolderExists) {
            return NtStatus::Success;
        }

        // Determine target break level based on conflicting operation
        uint32_t newTargetLevel = st->CurrentLevel;
        bool breakRequired      = false;

        if (majorFunction == io::IRP_MJ_WRITE || isConflictingShareOrDelete) {
            // Write or delete/supersede breaks any Read/Write/Handle caching down to NONE
            newTargetLevel = OPLOCK_LEVEL_NONE;
            breakRequired  = true;
        } else if (majorFunction == io::IRP_MJ_CREATE) {
            // A second reader opening the file breaks RW -> R (or RWH -> RH)
            if ((st->CurrentLevel & OPLOCK_LEVEL_CACHE_WRITE) != 0) {
                newTargetLevel = st->CurrentLevel & ~OPLOCK_LEVEL_CACHE_WRITE;
                breakRequired  = true;
            }
        } else if (majorFunction == io::IRP_MJ_CLEANUP) {
            newTargetLevel = OPLOCK_LEVEL_NONE;
            breakRequired  = true;
        }

        if (!breakRequired) {
            return NtStatus::Success;
        }

        st->PendingBreakOriginLevel = st->CurrentLevel;
        st->BreakTargetLevel        = newTargetLevel;
        m_telemetry.TotalOplockBreaks++;

        // Break of Read-only ('R' without 'W' or 'H') does not require acknowledgement!
        bool ackRequired = ((st->CurrentLevel & (OPLOCK_LEVEL_CACHE_WRITE | OPLOCK_LEVEL_CACHE_HANDLE)) != 0);
        if (outBreakNotification) {
            outBreakNotification->OriginalOplockLevel = st->CurrentLevel;
            outBreakNotification->NewOplockLevel      = newTargetLevel;
            outBreakNotification->Flags               = ackRequired ? REQUEST_OPLOCK_OUTPUT_FLAG_ACK_REQUIRED : 0;
        }

        if (!ackRequired) {
            st->CurrentLevel    = newTargetLevel;
            st->BreakInProgress = false;
            if (newTargetLevel == OPLOCK_LEVEL_NONE) {
                st->Grants.clear();
            }
            return NtStatus::Success;
        }

        st->BreakInProgress = true;
        return NtStatus::OplockBreakInProgress;
    }

    NtStatus breakOplockToNone(OPLOCK* oplock) {
        if (!oplock || !*oplock) return NtStatus::Success;
        std::lock_guard<std::mutex> lock(m_mutex);
        OPLOCK_STATE* st = *oplock;
        if (st->CurrentLevel == OPLOCK_LEVEL_NONE) return NtStatus::Success;
        st->PendingBreakOriginLevel = st->CurrentLevel;
        st->BreakTargetLevel        = OPLOCK_LEVEL_NONE;
        st->CurrentLevel            = OPLOCK_LEVEL_NONE;
        st->BreakInProgress         = false;
        st->Grants.clear();
        m_telemetry.TotalOplockBreaks++;
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // 9.5 Directory Change Notification Engine (NOTIFY_SYNC)
    // ------------------------------------------------------------------------

    void notifyInitializeSync(PNOTIFY_SYNC* notifySync) {
        if (!notifySync) return;
        *notifySync = new NOTIFY_SYNC_STATE();
    }

    void notifyUninitializeSync(PNOTIFY_SYNC* notifySync) {
        if (!notifySync || !*notifySync) return;
        delete *notifySync;
        *notifySync = nullptr;
    }

    void notifyFullChangeDirectory(
        PNOTIFY_SYNC notifySync,
        PVOID notifyContext,
        PVOID fsContext,
        std::wstring_view fullDirectoryName,
        BOOLEAN watchTree,
        uint32_t completionFilter,
        PVOID outputBuffer,
        uint32_t outputBufferLength
    ) {
        if (!notifySync) return;
        std::lock_guard<std::mutex> lock(notifySync->Mutex);

        for (auto& w : notifySync->Watchers) {
            if (w.NotifyContext == notifyContext && !w.CleanedUp) {
                w.OutputBuffer       = outputBuffer;
                w.OutputBufferLength = outputBufferLength;
                w.Completed          = false;
                w.CompletionStatus   = NtStatus::Pending;
                if (!w.PendingEvents.empty() && outputBuffer && outputBufferLength >= sizeof(FILE_NOTIFY_INFORMATION)) {
                    flushPendingEventsToBuffer(w);
                }
                return;
            }
        }

        NOTIFY_CHANGE_WATCHER watcher{};
        watcher.NotifyContext      = notifyContext;
        watcher.FsContext          = fsContext;
        watcher.FullDirectoryName  = normalizeNotifyPath(fullDirectoryName);
        watcher.WatchTree          = watchTree;
        watcher.CompletionFilter   = completionFilter;
        watcher.OutputBuffer       = outputBuffer;
        watcher.OutputBufferLength = outputBufferLength;
        notifySync->Watchers.push_back(std::move(watcher));

        std::lock_guard<std::mutex> glock(m_mutex);
        m_telemetry.TotalNotifyWatchersRegistered++;
    }

    void notifyFullReportChange(
        PNOTIFY_SYNC notifySync,
        std::wstring_view fullTargetName,
        uint16_t targetNameOffset,
        uint32_t filterMatch,
        uint32_t action
    ) {
        if (!notifySync) return;
        std::wstring normFull = normalizeNotifyPath(fullTargetName);

        std::wstring parentDir;
        std::wstring leafOrRelative;
        if (targetNameOffset > 0 && (targetNameOffset / sizeof(wchar_t)) <= normFull.size()) {
            size_t charOff = targetNameOffset / sizeof(wchar_t);
            parentDir      = normFull.substr(0, (charOff > 1 && normFull[charOff - 1] == L'\\') ? (charOff - 1) : charOff);
            leafOrRelative = normFull.substr(charOff);
        } else {
            auto lastSlash = normFull.find_last_of(L'\\');
            if (lastSlash != std::wstring::npos) {
                parentDir      = (lastSlash == 0) ? L"\\" : normFull.substr(0, lastSlash);
                leafOrRelative = normFull.substr(lastSlash + 1);
            } else {
                parentDir      = L"\\";
                leafOrRelative = normFull;
            }
        }

        {
            std::lock_guard<std::mutex> glock(m_mutex);
            m_telemetry.TotalNotifyEventsReported++;
        }

        std::lock_guard<std::mutex> lock(notifySync->Mutex);
        for (auto& w : notifySync->Watchers) {
            if (w.CleanedUp) continue;
            if ((w.CompletionFilter & filterMatch) == 0) continue;

            std::wstring relName;
            if (equalsIgnoreCase(parentDir, w.FullDirectoryName)) {
                relName = leafOrRelative;
            } else if (w.WatchTree != FALSE_B && isSubpathIgnoreCase(normFull, w.FullDirectoryName, relName)) {
                // Subtree match: relName holds the relative path beneath FullDirectoryName
            } else {
                continue;
            }

            w.PendingEvents.emplace_back(action, relName);
            if (w.OutputBuffer && w.OutputBufferLength >= sizeof(FILE_NOTIFY_INFORMATION) && !w.Completed) {
                flushPendingEventsToBuffer(w);
            } else {
                w.Completed        = true;
                w.CompletionStatus = NtStatus::Success;
            }
        }
    }

    void notifyCleanup(PNOTIFY_SYNC notifySync, PVOID notifyContext) {
        if (!notifySync) return;
        std::lock_guard<std::mutex> lock(notifySync->Mutex);
        for (auto& w : notifySync->Watchers) {
            if (w.NotifyContext == notifyContext) {
                w.CleanedUp        = true;
                w.Completed        = true;
                w.CompletionStatus = NtStatus::NotifyCleanup;
                w.PendingEvents.clear();
            }
        }
    }

    // ------------------------------------------------------------------------
    // 9.6 File Name Tunnel Cache (TUNNEL)
    // ------------------------------------------------------------------------

    void initializeTunnelCache(TUNNEL* cache) {
        if (!cache) return;
        std::lock_guard<std::mutex> lock(cache->Mutex);
        cache->Entries.clear();
        cache->ExpirationWindowMs = DEFAULT_TUNNEL_CACHE_TIMEOUT_MS;
        cache->TotalHits          = 0;
        cache->TotalMisses        = 0;
    }

    void deleteTunnelCache(TUNNEL* cache) {
        if (!cache) return;
        std::lock_guard<std::mutex> lock(cache->Mutex);
        cache->Entries.clear();
    }

    void addToTunnelCache(
        TUNNEL* cache,
        ULONGLONG directoryKey,
        std::wstring_view shortName,
        std::wstring_view longName,
        BOOLEAN keyByShortName,
        ULONG dataLength,
        const void* data
    ) {
        if (!cache) return;
        uint64_t nowMs = currentTickMs();

        {
            std::lock_guard<std::mutex> glock(m_mutex);
            m_telemetry.TotalTunnelEntriesAdded++;
        }

        std::lock_guard<std::mutex> lock(cache->Mutex);
        pruneExpiredTunnelEntriesUnlocked(cache, nowMs);

        // Remove any existing conflicting entry for the same directoryKey and key name
        std::wstring_view primaryKey = (keyByShortName != FALSE_B) ? shortName : longName;
        auto it = cache->Entries.begin();
        while (it != cache->Entries.end()) {
            std::wstring_view existingKey = (it->KeyByShortName != FALSE_B) ? it->ShortName : it->LongName;
            if (it->DirectoryKey == directoryKey &&
                it->KeyByShortName == keyByShortName &&
                equalsIgnoreCase(existingKey, primaryKey)) {
                it = cache->Entries.erase(it);
            } else {
                ++it;
            }
        }

        TUNNEL_CACHE_NODE node{};
        node.DirectoryKey   = directoryKey;
        node.ShortName      = std::wstring(shortName);
        node.LongName       = std::wstring(longName);
        node.KeyByShortName = keyByShortName;
        node.InsertTickMs   = nowMs;
        if (data && dataLength > 0) {
            const auto* bytes = static_cast<const uint8_t*>(data);
            node.ExtraData.assign(bytes, bytes + dataLength);
        }
        cache->Entries.push_back(std::move(node));
    }

    BOOLEAN findInTunnelCache(
        TUNNEL* cache,
        ULONGLONG directoryKey,
        std::wstring_view name,
        std::wstring* outShortName,
        std::wstring* outLongName,
        ULONG* inOutDataLength,
        void* outData
    ) {
        if (!cache) return FALSE_B;
        uint64_t nowMs = currentTickMs();

        std::lock_guard<std::mutex> lock(cache->Mutex);
        pruneExpiredTunnelEntriesUnlocked(cache, nowMs);

        for (auto it = cache->Entries.begin(); it != cache->Entries.end(); ++it) {
            if (it->DirectoryKey != directoryKey) continue;
            bool match = false;
            if (it->KeyByShortName != FALSE_B && !it->ShortName.empty() && equalsIgnoreCase(it->ShortName, name)) {
                match = true;
            } else if (!it->LongName.empty() && equalsIgnoreCase(it->LongName, name)) {
                match = true;
            } else if (!it->ShortName.empty() && equalsIgnoreCase(it->ShortName, name)) {
                match = true;
            }

            if (match) {
                if (outShortName) *outShortName = it->ShortName;
                if (outLongName)  *outLongName  = it->LongName;
                if (inOutDataLength) {
                    ULONG copyLen = std::min<ULONG>(*inOutDataLength, static_cast<ULONG>(it->ExtraData.size()));
                    if (outData && copyLen > 0) {
                        std::memcpy(outData, it->ExtraData.data(), copyLen);
                    }
                    *inOutDataLength = static_cast<ULONG>(it->ExtraData.size());
                }
                cache->Entries.erase(it); // Consumed upon recreation
                cache->TotalHits++;
                {
                    std::lock_guard<std::mutex> glock(m_mutex);
                    m_telemetry.TotalTunnelCacheHits++;
                }
                return TRUE_B;
            }
        }

        cache->TotalMisses++;
        return FALSE_B;
    }

    void deleteKeyFromTunnelCache(TUNNEL* cache, ULONGLONG directoryKey) {
        if (!cache) return;
        std::lock_guard<std::mutex> lock(cache->Mutex);
        auto it = cache->Entries.begin();
        while (it != cache->Entries.end()) {
            if (it->DirectoryKey == directoryKey) {
                it = cache->Entries.erase(it);
            } else {
                ++it;
            }
        }
    }

    // ------------------------------------------------------------------------
    // 9.7 Wildcard Expression Engine & Path Dissection
    // ------------------------------------------------------------------------

    BOOLEAN doesNameContainWildCards(std::wstring_view expression) noexcept {
        for (wchar_t ch : expression) {
            if (ch == L'*' || ch == L'?' || ch == DOS_STAR || ch == DOS_QM || ch == DOS_DOT) {
                return TRUE_B;
            }
        }
        return FALSE_B;
    }

    BOOLEAN areNamesEqual(std::wstring_view name1, std::wstring_view name2, BOOLEAN ignoreCase) noexcept {
        if (name1.size() != name2.size()) return FALSE_B;
        if (ignoreCase == FALSE_B) {
            return (name1 == name2) ? TRUE_B : FALSE_B;
        }
        return equalsIgnoreCase(name1, name2) ? TRUE_B : FALSE_B;
    }

    BOOLEAN isNameInExpression(
        std::wstring_view expression,
        std::wstring_view name,
        BOOLEAN ignoreCase
    ) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.TotalWildcardEvaluations++;
        }
        return matchWildcardRecursive(expression, 0, name, 0, ignoreCase != FALSE_B) ? TRUE_B : FALSE_B;
    }

    static void dissectName(
        std::wstring_view path,
        std::wstring_view& firstName,
        std::wstring_view& remainingPart
    ) noexcept {
        size_t start = 0;
        if (!path.empty() && path[0] == L'\\') {
            start = 1;
        }
        if (start >= path.size()) {
            firstName     = std::wstring_view{};
            remainingPart = std::wstring_view{};
            return;
        }
        size_t nextSlash = path.find(L'\\', start);
        if (nextSlash == std::wstring_view::npos) {
            firstName     = path.substr(start);
            remainingPart = std::wstring_view{};
        } else {
            firstName     = path.substr(start, nextSlash - start);
            size_t remPos = nextSlash + 1;
            remainingPart = (remPos < path.size()) ? path.substr(remPos) : std::wstring_view{};
        }
    }

    // ------------------------------------------------------------------------
    // 9.8 Fast I/O Copy Bridge (FsRtlCopyRead / FsRtlCopyWrite -> Cc*)
    // ------------------------------------------------------------------------

    BOOLEAN fastIoCopyRead(
        cc::CC_FILE_OBJECT* fileObject,
        FILE_LOCK* fileLock,
        PLARGE_INTEGER fileOffset,
        ULONG length,
        BOOLEAN wait,
        ULONG lockKey,
        PVOID buffer,
        PIO_STATUS_BLOCK ioStatus,
        PVOID processId
    ) {
        if (!fileObject || !fileOffset || (!buffer && length > 0)) return FALSE_B;

        auto* fcb = static_cast<FSRTL_ADVANCED_FCB_HEADER*>(fileObject->FsContext);
        if (fcb) {
            if (fcb->IsFastIoPossible == static_cast<uint8_t>(FAST_IO_POSSIBLE::FastIoIsNotPossible)) {
                return FALSE_B;
            }
        }

        if (fileLock) {
            LARGE_INTEGER lenLi{};
            lenLi.quadPart = static_cast<LONGLONG>(length);
            if (fastCheckLockForRead(fileLock, fileOffset, &lenLi, lockKey, fileObject, processId) == FALSE_B) {
                return FALSE_B;
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.TotalFastIoReads++;
        }

        return cc::CcCopyRead(fileObject, fileOffset, length, wait, buffer, ioStatus);
    }

    BOOLEAN fastIoCopyWrite(
        cc::CC_FILE_OBJECT* fileObject,
        FILE_LOCK* fileLock,
        PLARGE_INTEGER fileOffset,
        ULONG length,
        BOOLEAN wait,
        ULONG lockKey,
        const void* buffer,
        PIO_STATUS_BLOCK ioStatus,
        PVOID processId
    ) {
        if (!fileObject || !fileOffset || (!buffer && length > 0)) return FALSE_B;

        auto* fcb = static_cast<FSRTL_ADVANCED_FCB_HEADER*>(fileObject->FsContext);
        if (fcb) {
            if (fcb->IsFastIoPossible == static_cast<uint8_t>(FAST_IO_POSSIBLE::FastIoIsNotPossible)) {
                return FALSE_B;
            }
        }

        if (fileLock) {
            LARGE_INTEGER lenLi{};
            lenLi.quadPart = static_cast<LONGLONG>(length);
            if (fastCheckLockForWrite(fileLock, fileOffset, &lenLi, lockKey, fileObject, processId) == FALSE_B) {
                return FALSE_B;
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_telemetry.TotalFastIoWrites++;
        }

        BOOLEAN ok = cc::CcCopyWrite(fileObject, fileOffset, length, wait, const_cast<void*>(buffer));
        if (ok != FALSE_B && ioStatus) {
            ioStatus->status      = NtStatus::Success;
            ioStatus->information = length;
        }
        return ok;
    }

    FSRTL_TELEMETRY getTelemetry() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_telemetry;
    }

private:
    TitanFsRtlManager() = default;

    static wchar_t upcaseChar(wchar_t ch) noexcept {
        if (ch >= L'a' && ch <= L'z') return static_cast<wchar_t>(ch - L'a' + L'A');
        return static_cast<wchar_t>(std::towupper(ch));
    }

    static bool equalsIgnoreCase(std::wstring_view a, std::wstring_view b) noexcept {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (upcaseChar(a[i]) != upcaseChar(b[i])) return false;
        }
        return true;
    }

    static std::wstring normalizeNotifyPath(std::wstring_view p) {
        std::wstring res(p);
        while (res.size() > 1 && res.back() == L'\\') {
            res.pop_back();
        }
        return res;
    }

    static bool isSubpathIgnoreCase(std::wstring_view fullTarget, std::wstring_view rootDir, std::wstring& outRel) {
        if (fullTarget.size() <= rootDir.size()) return false;
        if (!equalsIgnoreCase(fullTarget.substr(0, rootDir.size()), rootDir)) return false;
        size_t pos = rootDir.size();
        if (rootDir != L"\\" && fullTarget[pos] != L'\\') return false;
        if (fullTarget[pos] == L'\\') ++pos;
        if (pos >= fullTarget.size()) return false;
        outRel = std::wstring(fullTarget.substr(pos));
        return true;
    }

    static void flushPendingEventsToBuffer(NOTIFY_CHANGE_WATCHER& w) {
        if (!w.OutputBuffer || w.OutputBufferLength < sizeof(FILE_NOTIFY_INFORMATION)) return;
        uint8_t* basePtr = static_cast<uint8_t*>(w.OutputBuffer);
        uint32_t offset  = 0;
        uint32_t prevRecOffset = 0;
        bool wroteAny    = false;

        while (!w.PendingEvents.empty()) {
            const auto& ev = w.PendingEvents.front();
            uint32_t nameBytes = static_cast<uint32_t>(ev.second.size() * sizeof(wchar_t));
            uint32_t recSize   = static_cast<uint32_t>(offsetof(FILE_NOTIFY_INFORMATION, FileName)) + nameBytes;
            uint32_t alignedSize = (recSize + 3u) & ~3u;

            if (offset + recSize > w.OutputBufferLength) {
                if (!wroteAny) {
                    w.BufferOverflowed = true;
                    w.PendingEvents.clear();
                }
                break;
            }

            if (wroteAny) {
                auto* prevInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(basePtr + prevRecOffset);
                prevInfo->NextEntryOffset = offset - prevRecOffset;
            }

            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(basePtr + offset);
            info->NextEntryOffset = 0;
            info->Action          = ev.first;
            info->FileNameLength  = nameBytes;
            if (nameBytes > 0) {
                std::memcpy(info->FileName, ev.second.data(), nameBytes);
            }

            prevRecOffset = offset;
            offset += alignedSize;
            w.BytesWritten = prevRecOffset + recSize;
            wroteAny = true;
            w.PendingEvents.erase(w.PendingEvents.begin());
        }

        w.Completed        = true;
        w.CompletionStatus = w.BufferOverflowed ? NtStatus::NotifyEnumDir : NtStatus::Success;
    }

    static bool rangesOverlap(LONGLONG s1, LONGLONG len1, LONGLONG s2, LONGLONG len2) noexcept {
        if (len1 == 0 && len2 == 0) return false;
        if (len1 == 0) {
            // Zero-length lock at s1 conflicts only if s2 < s1 < s2 + len2
            return (s1 > s2 && s1 < (s2 + len2));
        }
        if (len2 == 0) {
            return (s2 > s1 && s2 < (s1 + len1));
        }
        LONGLONG e1 = s1 + len1;
        LONGLONG e2 = s2 + len2;
        return (s1 < e2 && s2 < e1);
    }

    static void removeBaseMcbRangeUnlocked(BASE_MCB* mcb, LONGLONG vbn, LONGLONG sectorCount) {
        if (!mcb || sectorCount <= 0) return;
        LONGLONG cutStart = vbn;
        LONGLONG cutEnd   = vbn + sectorCount;

        std::vector<MCB_MAPPING_RUN> remaining;
        for (const auto& r : mcb->Runs) {
            LONGLONG rStart = r.StartVbn;
            LONGLONG rEnd   = r.StartVbn + r.SectorCount;

            if (rEnd <= cutStart || rStart >= cutEnd) {
                remaining.push_back(r);
                continue;
            }

            // Left portion before cutStart
            if (rStart < cutStart) {
                MCB_MAPPING_RUN left = r;
                left.SectorCount = cutStart - rStart;
                remaining.push_back(left);
            }

            // Right portion after cutEnd
            if (rEnd > cutEnd) {
                LONGLONG rightOffset = cutEnd - rStart;
                MCB_MAPPING_RUN right = r;
                right.StartVbn    = cutEnd;
                right.StartLbn    = (r.StartLbn == FSRTL_UNUSED_LBN) ? FSRTL_UNUSED_LBN : (r.StartLbn + rightOffset);
                right.SectorCount = rEnd - cutEnd;
                remaining.push_back(right);
            }
        }
        mcb->Runs.swap(remaining);
    }

    static void normalizeMcbRunsUnlocked(BASE_MCB* mcb) {
        if (!mcb) return;

        // Sort by StartVbn
        std::sort(mcb->Runs.begin(), mcb->Runs.end(), [](const MCB_MAPPING_RUN& a, const MCB_MAPPING_RUN& b) {
            return a.StartVbn < b.StartVbn;
        });

        // Coalesce contiguous runs (either contiguous holes or contiguous VBN + LBN extents)
        std::vector<MCB_MAPPING_RUN> merged;
        for (const auto& r : mcb->Runs) {
            if (r.SectorCount <= 0) continue;
            if (!merged.empty()) {
                auto& prev = merged.back();
                if (prev.StartVbn + prev.SectorCount == r.StartVbn) {
                    if (prev.StartLbn == FSRTL_UNUSED_LBN && r.StartLbn == FSRTL_UNUSED_LBN) {
                        prev.SectorCount += r.SectorCount;
                        continue;
                    }
                    if (prev.StartLbn != FSRTL_UNUSED_LBN &&
                        r.StartLbn != FSRTL_UNUSED_LBN &&
                        (prev.StartLbn + prev.SectorCount) == r.StartLbn) {
                        prev.SectorCount += r.SectorCount;
                        continue;
                    }
                }
            }
            merged.push_back(r);
        }

        // Trim any trailing sparse holes at the end of the MCB (matching NT FsRtl semantics)
        while (!merged.empty() && merged.back().StartLbn == FSRTL_UNUSED_LBN) {
            merged.pop_back();
        }

        mcb->Runs.swap(merged);
        mcb->PairCount = static_cast<uint32_t>(mcb->Runs.size());
    }

    static void recomputeCurrentOplockLevelUnlocked(OPLOCK_STATE* st) noexcept {
        if (!st) return;
        uint32_t combined = OPLOCK_LEVEL_NONE;
        for (const auto& g : st->Grants) {
            combined |= g.Level;
        }
        st->CurrentLevel = combined;
    }

    static void pruneExpiredTunnelEntriesUnlocked(TUNNEL* cache, uint64_t nowMs) {
        if (!cache) return;
        auto it = cache->Entries.begin();
        while (it != cache->Entries.end()) {
            if (nowMs >= it->InsertTickMs && (nowMs - it->InsertTickMs) > cache->ExpirationWindowMs) {
                it = cache->Entries.erase(it);
            } else {
                ++it;
            }
        }
    }

    static bool matchWildcardRecursive(
        std::wstring_view expr,
        size_t exprIdx,
        std::wstring_view name,
        size_t nameIdx,
        bool ignoreCase
    ) noexcept {
        while (exprIdx < expr.size()) {
            wchar_t ec = expr[exprIdx];

            if (ec == L'*') {
                while (exprIdx < expr.size() && expr[exprIdx] == L'*') {
                    ++exprIdx;
                }
                if (exprIdx == expr.size()) return true;
                for (size_t k = nameIdx; k <= name.size(); ++k) {
                    if (matchWildcardRecursive(expr, exprIdx, name, k, ignoreCase)) {
                        return true;
                    }
                }
                return false;
            }

            if (ec == DOS_STAR) {
                // DOS_STAR ('<') matches 0+ characters up to (and not beyond) the last '.' in `name`
                size_t lastDot = name.find_last_of(L'.');
                size_t maxStop = (lastDot == std::wstring_view::npos || lastDot < nameIdx)
                    ? name.size()
                    : lastDot;
                for (size_t k = nameIdx; k <= maxStop; ++k) {
                    if (matchWildcardRecursive(expr, exprIdx + 1, name, k, ignoreCase)) {
                        return true;
                    }
                }
                return false;
            }

            if (ec == DOS_QM) {
                // DOS_QM ('>') matches a single non-dot character, or matches 0 characters at a '.' or end-of-name
                if (nameIdx == name.size() || name[nameIdx] == L'.') {
                    if (matchWildcardRecursive(expr, exprIdx + 1, name, nameIdx, ignoreCase)) {
                        return true;
                    }
                }
                if (nameIdx < name.size() && name[nameIdx] != L'.') {
                    return matchWildcardRecursive(expr, exprIdx + 1, name, nameIdx + 1, ignoreCase);
                }
                return false;
            }

            if (ec == DOS_DOT) {
                // DOS_DOT ('"') matches a '.' or end-of-name
                if (nameIdx == name.size()) {
                    return matchWildcardRecursive(expr, exprIdx + 1, name, nameIdx, ignoreCase);
                }
                if (name[nameIdx] == L'.') {
                    if (matchWildcardRecursive(expr, exprIdx + 1, name, nameIdx + 1, ignoreCase)) {
                        return true;
                    }
                }
                return false;
            }

            if (nameIdx >= name.size()) {
                return false;
            }

            if (ec == L'?') {
                ++exprIdx;
                ++nameIdx;
                continue;
            }

            wchar_t nc = name[nameIdx];
            bool charMatch = ignoreCase ? (upcaseChar(ec) == upcaseChar(nc)) : (ec == nc);
            if (!charMatch) return false;
            ++exprIdx;
            ++nameIdx;
        }

        return (nameIdx == name.size());
    }

    mutable std::mutex m_mutex{};
    std::unordered_map<PVOID, std::vector<FSRTL_PER_FILEOBJECT_CONTEXT*>> m_fileObjectContexts{};
    FSRTL_TELEMETRY m_telemetry{};
    uint64_t m_currentTickMs{100000};
    bool m_initialized{false};
};

// ============================================================================
// 10. Public C-Linkage WDK / IFS FsRtl* Executive APIs
// ============================================================================

extern "C" {

// --- 10.1 Advanced FCB Header & Per-Stream / Per-FileObject Contexts ---

inline void WINAPI FsRtlSetupAdvancedHeader(
    FSRTL_ADVANCED_FCB_HEADER* AdvHdr,
    PVOID FastMutex
) {
    if (!AdvHdr) return;
    AdvHdr->Flags            |= FSRTL_FLAG_ADVANCED_HEADER;
    AdvHdr->Version           = FSRTL_FCB_HEADER_V2;
    AdvHdr->IsFastIoPossible  = static_cast<uint8_t>(FAST_IO_POSSIBLE::FastIoIsPossible);
    AdvHdr->FastMutex         = FastMutex;
    AdvHdr->StreamContexts.clear();
}

inline void WINAPI FsRtlTeardownPerStreamContexts(FSRTL_ADVANCED_FCB_HEADER* AdvancedHeader) {
    TitanFsRtlManager::Instance().teardownPerStreamContexts(AdvancedHeader);
}

inline NtStatus WINAPI FsRtlInsertPerStreamContext(
    FSRTL_ADVANCED_FCB_HEADER* PerStreamContext,
    FSRTL_PER_STREAM_CONTEXT* Ptr
) {
    return TitanFsRtlManager::Instance().insertPerStreamContext(PerStreamContext, Ptr);
}

inline FSRTL_PER_STREAM_CONTEXT* WINAPI FsRtlLookupPerStreamContextInternal(
    FSRTL_ADVANCED_FCB_HEADER* PerStreamContext,
    PVOID OwnerId,
    PVOID InstanceId
) {
    return TitanFsRtlManager::Instance().lookupPerStreamContext(PerStreamContext, OwnerId, InstanceId);
}

inline FSRTL_PER_STREAM_CONTEXT* WINAPI FsRtlRemovePerStreamContext(
    FSRTL_ADVANCED_FCB_HEADER* PerStreamContext,
    PVOID OwnerId,
    PVOID InstanceId
) {
    return TitanFsRtlManager::Instance().removePerStreamContext(PerStreamContext, OwnerId, InstanceId);
}

inline NtStatus WINAPI FsRtlInsertPerFileObjectContext(
    PVOID FileObject,
    FSRTL_PER_FILEOBJECT_CONTEXT* Ptr
) {
    return TitanFsRtlManager::Instance().insertPerFileObjectContext(FileObject, Ptr);
}

inline FSRTL_PER_FILEOBJECT_CONTEXT* WINAPI FsRtlLookupPerFileObjectContext(
    PVOID FileObject,
    PVOID OwnerId,
    PVOID InstanceId
) {
    return TitanFsRtlManager::Instance().lookupPerFileObjectContext(FileObject, OwnerId, InstanceId);
}

inline FSRTL_PER_FILEOBJECT_CONTEXT* WINAPI FsRtlRemovePerFileObjectContext(
    PVOID FileObject,
    PVOID OwnerId,
    PVOID InstanceId
) {
    return TitanFsRtlManager::Instance().removePerFileObjectContext(FileObject, OwnerId, InstanceId);
}

// --- 10.2 Large & Base Map Control Blocks (LARGE_MCB / BASE_MCB) ---

inline void WINAPI FsRtlInitializeBaseMcb(BASE_MCB* Mcb, uint16_t PoolType) {
    TitanFsRtlManager::Instance().initializeBaseMcb(Mcb, PoolType);
}

inline void WINAPI FsRtlUninitializeBaseMcb(BASE_MCB* Mcb) {
    TitanFsRtlManager::Instance().uninitializeBaseMcb(Mcb);
}

inline void WINAPI FsRtlResetBaseMcb(BASE_MCB* Mcb) {
    TitanFsRtlManager::Instance().resetBaseMcb(Mcb);
}

inline void WINAPI FsRtlInitializeLargeMcb(LARGE_MCB* Mcb, uint16_t PoolType) {
    if (!Mcb) return;
    Mcb->GuardedMutex = nullptr;
    TitanFsRtlManager::Instance().initializeBaseMcb(&Mcb->BaseMcb, PoolType);
}

inline void WINAPI FsRtlUninitializeLargeMcb(LARGE_MCB* Mcb) {
    if (!Mcb) return;
    TitanFsRtlManager::Instance().uninitializeBaseMcb(&Mcb->BaseMcb);
}

inline void WINAPI FsRtlResetLargeMcb(LARGE_MCB* Mcb, BOOLEAN SelfSynchronized) {
    (void)SelfSynchronized;
    if (!Mcb) return;
    TitanFsRtlManager::Instance().resetBaseMcb(&Mcb->BaseMcb);
}

inline BOOLEAN WINAPI FsRtlAddLargeMcbEntry(
    LARGE_MCB* Mcb,
    LONGLONG Vbn,
    LONGLONG Lbn,
    LONGLONG SectorCount
) {
    if (!Mcb) return FALSE_B;
    return TitanFsRtlManager::Instance().addBaseMcbEntry(&Mcb->BaseMcb, Vbn, Lbn, SectorCount);
}

inline void WINAPI FsRtlRemoveLargeMcbEntry(
    LARGE_MCB* Mcb,
    LONGLONG Vbn,
    LONGLONG SectorCount
) {
    if (!Mcb) return;
    TitanFsRtlManager::Instance().removeBaseMcbEntry(&Mcb->BaseMcb, Vbn, SectorCount);
}

inline BOOLEAN WINAPI FsRtlLookupLargeMcbEntry(
    LARGE_MCB* Mcb,
    LONGLONG Vbn,
    PLONGLONG Lbn,
    PLONGLONG SectorCountFromLbn,
    PLONGLONG StartingLbn,
    PLONGLONG CountFromStartingLbn,
    PULONG Index
) {
    if (!Mcb) return FALSE_B;
    return TitanFsRtlManager::Instance().lookupBaseMcbEntry(
        &Mcb->BaseMcb, Vbn, Lbn, SectorCountFromLbn, StartingLbn, CountFromStartingLbn, Index);
}

inline BOOLEAN WINAPI FsRtlLookupLastLargeMcbEntryAndIndex(
    LARGE_MCB* OpaqueMcb,
    PLONGLONG LargeVbn,
    PLONGLONG LargeLbn,
    PULONG Index
) {
    if (!OpaqueMcb) return FALSE_B;
    return TitanFsRtlManager::Instance().lookupLastBaseMcbEntryAndIndex(
        &OpaqueMcb->BaseMcb, LargeVbn, LargeLbn, Index);
}

inline BOOLEAN WINAPI FsRtlLookupLastLargeMcbEntry(
    LARGE_MCB* Mcb,
    PLONGLONG Vbn,
    PLONGLONG Lbn
) {
    return FsRtlLookupLastLargeMcbEntryAndIndex(Mcb, Vbn, Lbn, nullptr);
}

inline ULONG WINAPI FsRtlNumberOfRunsInLargeMcb(LARGE_MCB* Mcb) {
    if (!Mcb) return 0;
    return TitanFsRtlManager::Instance().numberOfRunsInBaseMcb(&Mcb->BaseMcb);
}

inline BOOLEAN WINAPI FsRtlGetNextLargeMcbEntry(
    LARGE_MCB* Mcb,
    ULONG RunIndex,
    PLONGLONG Vbn,
    PLONGLONG Lbn,
    PLONGLONG SectorCount
) {
    if (!Mcb) return FALSE_B;
    return TitanFsRtlManager::Instance().getNextBaseMcbEntry(&Mcb->BaseMcb, RunIndex, Vbn, Lbn, SectorCount);
}

inline void WINAPI FsRtlTruncateLargeMcb(LARGE_MCB* Mcb, LONGLONG Vbn) {
    if (!Mcb) return;
    TitanFsRtlManager::Instance().truncateBaseMcb(&Mcb->BaseMcb, Vbn);
}

inline BOOLEAN WINAPI FsRtlSplitLargeMcb(
    LARGE_MCB* Mcb,
    LONGLONG Vbn,
    LONGLONG Amount
) {
    if (!Mcb) return FALSE_B;
    return TitanFsRtlManager::Instance().splitBaseMcb(&Mcb->BaseMcb, Vbn, Amount);
}

// --- 10.3 Byte-Range File Locking (FILE_LOCK) ---

inline void WINAPI FsRtlInitializeFileLock(
    FILE_LOCK* FileLock,
    PCOMPLETE_LOCK_IRP_ROUTINE CompleteLockIrpRoutine,
    PUNLOCK_ROUTINE UnlockRoutine
) {
    if (!FileLock) return;
    FileLock->CompleteLockIrpRoutine = CompleteLockIrpRoutine;
    FileLock->UnlockRoutine          = UnlockRoutine;
    FileLock->FastIoIsQuestionable   = FALSE_B;
    FileLock->ActiveLocks.clear();
    FileLock->EnumerationCursor      = 0;
    FileLock->Initialized            = true;
}

inline void WINAPI FsRtlUninitializeFileLock(FILE_LOCK* FileLock) {
    if (!FileLock) return;
    FileLock->ActiveLocks.clear();
    FileLock->EnumerationCursor = 0;
    FileLock->Initialized       = false;
}

inline FILE_LOCK* WINAPI FsRtlAllocateFileLock(
    PCOMPLETE_LOCK_IRP_ROUTINE CompleteLockIrpRoutine,
    PUNLOCK_ROUTINE UnlockRoutine
) {
    auto* fl = new FILE_LOCK();
    FsRtlInitializeFileLock(fl, CompleteLockIrpRoutine, UnlockRoutine);
    return fl;
}

inline void WINAPI FsRtlFreeFileLock(FILE_LOCK* FileLock) {
    if (!FileLock) return;
    FsRtlUninitializeFileLock(FileLock);
    delete FileLock;
}

inline BOOLEAN WINAPI FsRtlFastLock(
    FILE_LOCK* FileLock,
    PVOID FileObject,
    PLARGE_INTEGER FileOffset,
    PLARGE_INTEGER Length,
    PVOID ProcessId,
    ULONG Key,
    BOOLEAN FailImmediately,
    BOOLEAN ExclusiveLock,
    PIO_STATUS_BLOCK IoStatus,
    PVOID Context,
    BOOLEAN AlreadySynchronized
) {
    (void)AlreadySynchronized;
    return TitanFsRtlManager::Instance().fastLock(
        FileLock, FileObject, FileOffset, Length, ProcessId, Key,
        FailImmediately, ExclusiveLock, IoStatus, Context);
}

inline NtStatus WINAPI FsRtlFastUnlockSingle(
    FILE_LOCK* FileLock,
    PVOID FileObject,
    PLARGE_INTEGER FileOffset,
    PLARGE_INTEGER Length,
    PVOID ProcessId,
    ULONG Key,
    PVOID Context,
    BOOLEAN AlreadySynchronized
) {
    (void)AlreadySynchronized;
    return TitanFsRtlManager::Instance().fastUnlockSingle(
        FileLock, FileObject, FileOffset, Length, ProcessId, Key, Context);
}

inline NtStatus WINAPI FsRtlFastUnlockAll(
    FILE_LOCK* FileLock,
    PVOID FileObject,
    PVOID ProcessId,
    PVOID Context
) {
    return TitanFsRtlManager::Instance().fastUnlockAll(
        FileLock, FileObject, ProcessId, std::nullopt, Context);
}

inline NtStatus WINAPI FsRtlFastUnlockAllByKey(
    FILE_LOCK* FileLock,
    PVOID FileObject,
    PVOID ProcessId,
    ULONG Key,
    PVOID Context
) {
    return TitanFsRtlManager::Instance().fastUnlockAll(
        FileLock, FileObject, ProcessId, Key, Context);
}

inline BOOLEAN WINAPI FsRtlFastCheckLockForRead(
    FILE_LOCK* FileLock,
    PLARGE_INTEGER StartingByte,
    PLARGE_INTEGER Length,
    ULONG Key,
    PVOID FileObject,
    PVOID ProcessId
) {
    return TitanFsRtlManager::Instance().fastCheckLockForRead(
        FileLock, StartingByte, Length, Key, FileObject, ProcessId);
}

inline BOOLEAN WINAPI FsRtlFastCheckLockForWrite(
    FILE_LOCK* FileLock,
    PLARGE_INTEGER StartingByte,
    PLARGE_INTEGER Length,
    ULONG Key,
    PVOID FileObject,
    PVOID ProcessId
) {
    return TitanFsRtlManager::Instance().fastCheckLockForWrite(
        FileLock, StartingByte, Length, Key, FileObject, ProcessId);
}

inline FILE_LOCK_INFO* WINAPI FsRtlGetNextFileLock(FILE_LOCK* FileLock, BOOLEAN Restart) {
    return TitanFsRtlManager::Instance().getNextFileLock(FileLock, Restart);
}

// --- 10.4 Opportunistic Locks (OPLOCK) ---

inline void WINAPI FsRtlInitializeOplock(OPLOCK* Oplock) {
    TitanFsRtlManager::Instance().initializeOplock(Oplock);
}

inline void WINAPI FsRtlUninitializeOplock(OPLOCK* Oplock) {
    TitanFsRtlManager::Instance().uninitializeOplock(Oplock);
}

inline NtStatus WINAPI FsRtlOplockFsctrlEx(
    OPLOCK* Oplock,
    PVOID FileObject,
    uint64_t OplockKey,
    ULONG FsctlCode,
    const REQUEST_OPLOCK_INPUT_BUFFER* InputBuffer,
    REQUEST_OPLOCK_OUTPUT_BUFFER* OutputBuffer
) {
    return TitanFsRtlManager::Instance().oplockFsctrl(
        Oplock, FileObject, OplockKey, FsctlCode, InputBuffer, OutputBuffer);
}

inline NtStatus WINAPI FsRtlCheckOplockEx(
    OPLOCK* Oplock,
    uint64_t CallerOplockKey,
    uint32_t MajorFunction,
    BOOLEAN IsConflictingShareOrDelete,
    REQUEST_OPLOCK_OUTPUT_BUFFER* OutBreakNotification
) {
    return TitanFsRtlManager::Instance().checkOplock(
        Oplock, CallerOplockKey, MajorFunction, IsConflictingShareOrDelete != FALSE_B, OutBreakNotification);
}

inline NtStatus WINAPI FsRtlOplockBreakToNone(OPLOCK* Oplock) {
    return TitanFsRtlManager::Instance().breakOplockToNone(Oplock);
}

inline BOOLEAN WINAPI FsRtlCurrentOplock(OPLOCK* Oplock) {
    if (!Oplock || !*Oplock) return FALSE_B;
    return ((*Oplock)->CurrentLevel != OPLOCK_LEVEL_NONE) ? TRUE_B : FALSE_B;
}

inline BOOLEAN WINAPI FsRtlCurrentOplockH(OPLOCK* Oplock) {
    if (!Oplock || !*Oplock) return FALSE_B;
    return (((*Oplock)->CurrentLevel & OPLOCK_LEVEL_CACHE_HANDLE) != 0) ? TRUE_B : FALSE_B;
}

inline BOOLEAN WINAPI FsRtlCurrentBatchOplock(OPLOCK* Oplock) {
    if (!Oplock || !*Oplock) return FALSE_B;
    for (const auto& g : (*Oplock)->Grants) {
        if (g.IsLegacyBatch || g.IsLegacyFilter) return TRUE_B;
    }
    return FALSE_B;
}

inline BOOLEAN WINAPI FsRtlOplockIsFastIoPossible(OPLOCK* Oplock) {
    if (!Oplock || !*Oplock) return TRUE_B;
    return (*Oplock)->BreakInProgress ? FALSE_B : TRUE_B;
}

// --- 10.5 Directory Change Notifications (NOTIFY_SYNC) ---

inline void WINAPI FsRtlNotifyInitializeSync(PNOTIFY_SYNC* NotifySync) {
    TitanFsRtlManager::Instance().notifyInitializeSync(NotifySync);
}

inline void WINAPI FsRtlNotifyUninitializeSync(PNOTIFY_SYNC* NotifySync) {
    TitanFsRtlManager::Instance().notifyUninitializeSync(NotifySync);
}

inline void WINAPI FsRtlNotifyFullChangeDirectory(
    PNOTIFY_SYNC NotifySync,
    PVOID NotifyContext,
    PVOID FsContext,
    const wchar_t* FullDirectoryName,
    BOOLEAN WatchTree,
    ULONG CompletionFilter,
    PVOID OutputBuffer,
    ULONG OutputBufferLength
) {
    TitanFsRtlManager::Instance().notifyFullChangeDirectory(
        NotifySync,
        NotifyContext,
        FsContext,
        FullDirectoryName ? std::wstring_view(FullDirectoryName) : std::wstring_view{},
        WatchTree,
        CompletionFilter,
        OutputBuffer,
        OutputBufferLength);
}

inline void WINAPI FsRtlNotifyFullReportChange(
    PNOTIFY_SYNC NotifySync,
    const wchar_t* FullTargetName,
    USHORT TargetNameOffset,
    ULONG FilterMatch,
    ULONG Action
) {
    TitanFsRtlManager::Instance().notifyFullReportChange(
        NotifySync,
        FullTargetName ? std::wstring_view(FullTargetName) : std::wstring_view{},
        TargetNameOffset,
        FilterMatch,
        Action);
}

inline void WINAPI FsRtlNotifyCleanup(PNOTIFY_SYNC NotifySync, PVOID NotifyContext) {
    TitanFsRtlManager::Instance().notifyCleanup(NotifySync, NotifyContext);
}

// --- 10.6 File Name Tunnel Cache (TUNNEL) ---

inline void WINAPI FsRtlInitializeTunnelCache(TUNNEL* Cache) {
    TitanFsRtlManager::Instance().initializeTunnelCache(Cache);
}

inline void WINAPI FsRtlDeleteTunnelCache(TUNNEL* Cache) {
    TitanFsRtlManager::Instance().deleteTunnelCache(Cache);
}

inline void WINAPI FsRtlAddToTunnelCache(
    TUNNEL* Cache,
    ULONGLONG DirectoryKey,
    const wchar_t* ShortName,
    const wchar_t* LongName,
    BOOLEAN KeyByShortName,
    ULONG DataLength,
    const void* Data
) {
    TitanFsRtlManager::Instance().addToTunnelCache(
        Cache,
        DirectoryKey,
        ShortName ? std::wstring_view(ShortName) : std::wstring_view{},
        LongName ? std::wstring_view(LongName) : std::wstring_view{},
        KeyByShortName,
        DataLength,
        Data);
}

inline BOOLEAN WINAPI FsRtlFindInTunnelCache(
    TUNNEL* Cache,
    ULONGLONG DirectoryKey,
    const wchar_t* Name,
     std::wstring* OutShortName,
    std::wstring* OutLongName,
    ULONG* DataLength,
    void* Data
) {
    return TitanFsRtlManager::Instance().findInTunnelCache(
        Cache,
        DirectoryKey,
        Name ? std::wstring_view(Name) : std::wstring_view{},
        OutShortName,
        OutLongName,
        DataLength,
        Data);
}

inline void WINAPI FsRtlDeleteKeyFromTunnelCache(TUNNEL* Cache, ULONGLONG DirectoryKey) {
    TitanFsRtlManager::Instance().deleteKeyFromTunnelCache(Cache, DirectoryKey);
}

// --- 10.7 Wildcard Expression Engine & Path Dissection ---

inline BOOLEAN WINAPI FsRtlDoesNameContainWildCards(const wchar_t* Name) {
    if (!Name) return FALSE_B;
    return TitanFsRtlManager::Instance().doesNameContainWildCards(Name);
}

inline BOOLEAN WINAPI FsRtlAreNamesEqual(
    const wchar_t* ConstantNameA,
    const wchar_t* ConstantNameB,
    BOOLEAN IgnoreCase
) {
    if (!ConstantNameA || !ConstantNameB) return FALSE_B;
    return TitanFsRtlManager::Instance().areNamesEqual(ConstantNameA, ConstantNameB, IgnoreCase);
}

inline BOOLEAN WINAPI FsRtlIsNameInExpression(
    const wchar_t* Expression,
    const wchar_t* Name,
    BOOLEAN IgnoreCase,
    const wchar_t* UpcaseTable
) {
    (void)UpcaseTable;
    if (!Expression || !Name) return FALSE_B;
    return TitanFsRtlManager::Instance().isNameInExpression(Expression, Name, IgnoreCase);
}

// --- 10.8 Fast I/O Copy Bridge ---

inline BOOLEAN WINAPI FsRtlCopyRead(
    cc::CC_FILE_OBJECT* FileObject,
    PLARGE_INTEGER FileOffset,
    ULONG Length,
    BOOLEAN Wait,
    ULONG LockKey,
    PVOID Buffer,
    PIO_STATUS_BLOCK IoStatus,
    PVOID DeviceObject,
    FILE_LOCK* FileLock,
    PVOID ProcessId
) {
    (void)DeviceObject;
    return TitanFsRtlManager::Instance().fastIoCopyRead(
        FileObject, FileLock, FileOffset, Length, Wait, LockKey, Buffer, IoStatus, ProcessId);
}

inline BOOLEAN WINAPI FsRtlCopyWrite(
    cc::CC_FILE_OBJECT* FileObject,
    PLARGE_INTEGER FileOffset,
    ULONG Length,
    BOOLEAN Wait,
    ULONG LockKey,
    const void* Buffer,
    PIO_STATUS_BLOCK IoStatus,
    PVOID DeviceObject,
    FILE_LOCK* FileLock,
    PVOID ProcessId
) {
    (void)DeviceObject;
    return TitanFsRtlManager::Instance().fastIoCopyWrite(
        FileObject, FileLock, FileOffset, Length, Wait, LockKey, Buffer, IoStatus, ProcessId);
}

inline BOOL WINAPI FsRtlGetTelemetry(FSRTL_TELEMETRY* outTelemetry) {
    if (!outTelemetry) return FALSE_VAL;
    *outTelemetry = TitanFsRtlManager::Instance().getTelemetry();
    return TRUE_VAL;
}

} // extern "C"

inline FSRTL_TELEMETRY FsRtlQueryTelemetry() noexcept {
    return TitanFsRtlManager::Instance().getTelemetry();
}

// ============================================================================
// 11. Subsystem Export Registration (JanusLDR, SCM, VersionDatabase)
// ============================================================================

inline void InitializeFsRtlSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    // Register ntoskrnl.exe File System Runtime Library (FsRtl*) exports
    ldr.registerExport("ntoskrnl.exe", "FsRtlSetupAdvancedHeader", reinterpret_cast<void*>(FsRtlSetupAdvancedHeader));
    ldr.registerExport("ntoskrnl.exe", "FsRtlTeardownPerStreamContexts", reinterpret_cast<void*>(FsRtlTeardownPerStreamContexts));
    ldr.registerExport("ntoskrnl.exe", "FsRtlInsertPerStreamContext", reinterpret_cast<void*>(FsRtlInsertPerStreamContext));
    ldr.registerExport("ntoskrnl.exe", "FsRtlLookupPerStreamContextInternal", reinterpret_cast<void*>(FsRtlLookupPerStreamContextInternal));
    ldr.registerExport("ntoskrnl.exe", "FsRtlRemovePerStreamContext", reinterpret_cast<void*>(FsRtlRemovePerStreamContext));
    ldr.registerExport("ntoskrnl.exe", "FsRtlInsertPerFileObjectContext", reinterpret_cast<void*>(FsRtlInsertPerFileObjectContext));
    ldr.registerExport("ntoskrnl.exe", "FsRtlLookupPerFileObjectContext", reinterpret_cast<void*>(FsRtlLookupPerFileObjectContext));
    ldr.registerExport("ntoskrnl.exe", "FsRtlRemovePerFileObjectContext", reinterpret_cast<void*>(FsRtlRemovePerFileObjectContext));

    ldr.registerExport("ntoskrnl.exe", "FsRtlInitializeBaseMcb", reinterpret_cast<void*>(FsRtlInitializeBaseMcb));
    ldr.registerExport("ntoskrnl.exe", "FsRtlUninitializeBaseMcb", reinterpret_cast<void*>(FsRtlUninitializeBaseMcb));
    ldr.registerExport("ntoskrnl.exe", "FsRtlResetBaseMcb", reinterpret_cast<void*>(FsRtlResetBaseMcb));
    ldr.registerExport("ntoskrnl.exe", "FsRtlInitializeLargeMcb", reinterpret_cast<void*>(FsRtlInitializeLargeMcb));
    ldr.registerExport("ntoskrnl.exe", "FsRtlUninitializeLargeMcb", reinterpret_cast<void*>(FsRtlUninitializeLargeMcb));
    ldr.registerExport("ntoskrnl.exe", "FsRtlResetLargeMcb", reinterpret_cast<void*>(FsRtlResetLargeMcb));
    ldr.registerExport("ntoskrnl.exe", "FsRtlAddLargeMcbEntry", reinterpret_cast<void*>(FsRtlAddLargeMcbEntry));
    ldr.registerExport("ntoskrnl.exe", "FsRtlRemoveLargeMcbEntry", reinterpret_cast<void*>(FsRtlRemoveLargeMcbEntry));
    ldr.registerExport("ntoskrnl.exe", "FsRtlLookupLargeMcbEntry", reinterpret_cast<void*>(FsRtlLookupLargeMcbEntry));
    ldr.registerExport("ntoskrnl.exe", "FsRtlLookupLastLargeMcbEntry", reinterpret_cast<void*>(FsRtlLookupLastLargeMcbEntry));
    ldr.registerExport("ntoskrnl.exe", "FsRtlLookupLastLargeMcbEntryAndIndex", reinterpret_cast<void*>(FsRtlLookupLastLargeMcbEntryAndIndex));
    ldr.registerExport("ntoskrnl.exe", "FsRtlNumberOfRunsInLargeMcb", reinterpret_cast<void*>(FsRtlNumberOfRunsInLargeMcb));
    ldr.registerExport("ntoskrnl.exe", "FsRtlGetNextLargeMcbEntry", reinterpret_cast<void*>(FsRtlGetNextLargeMcbEntry));
    ldr.registerExport("ntoskrnl.exe", "FsRtlTruncateLargeMcb", reinterpret_cast<void*>(FsRtlTruncateLargeMcb));
    ldr.registerExport("ntoskrnl.exe", "FsRtlSplitLargeMcb", reinterpret_cast<void*>(FsRtlSplitLargeMcb));

    ldr.registerExport("ntoskrnl.exe", "FsRtlInitializeFileLock", reinterpret_cast<void*>(FsRtlInitializeFileLock));
    ldr.registerExport("ntoskrnl.exe", "FsRtlUninitializeFileLock", reinterpret_cast<void*>(FsRtlUninitializeFileLock));
    ldr.registerExport("ntoskrnl.exe", "FsRtlAllocateFileLock", reinterpret_cast<void*>(FsRtlAllocateFileLock));
    ldr.registerExport("ntoskrnl.exe", "FsRtlFreeFileLock", reinterpret_cast<void*>(FsRtlFreeFileLock));
    ldr.registerExport("ntoskrnl.exe", "FsRtlFastLock", reinterpret_cast<void*>(FsRtlFastLock));
    ldr.registerExport("ntoskrnl.exe", "FsRtlFastUnlockSingle", reinterpret_cast<void*>(FsRtlFastUnlockSingle));
    ldr.registerExport("ntoskrnl.exe", "FsRtlFastUnlockAll", reinterpret_cast<void*>(FsRtlFastUnlockAll));
    ldr.registerExport("ntoskrnl.exe", "FsRtlFastUnlockAllByKey", reinterpret_cast<void*>(FsRtlFastUnlockAllByKey));
    ldr.registerExport("ntoskrnl.exe", "FsRtlFastCheckLockForRead", reinterpret_cast<void*>(FsRtlFastCheckLockForRead));
    ldr.registerExport("ntoskrnl.exe", "FsRtlFastCheckLockForWrite", reinterpret_cast<void*>(FsRtlFastCheckLockForWrite));
    ldr.registerExport("ntoskrnl.exe", "FsRtlGetNextFileLock", reinterpret_cast<void*>(FsRtlGetNextFileLock));

    ldr.registerExport("ntoskrnl.exe", "FsRtlInitializeOplock", reinterpret_cast<void*>(FsRtlInitializeOplock));
    ldr.registerExport("ntoskrnl.exe", "FsRtlUninitializeOplock", reinterpret_cast<void*>(FsRtlUninitializeOplock));
    ldr.registerExport("ntoskrnl.exe", "FsRtlOplockFsctrlEx", reinterpret_cast<void*>(FsRtlOplockFsctrlEx));
    ldr.registerExport("ntoskrnl.exe", "FsRtlCheckOplockEx", reinterpret_cast<void*>(FsRtlCheckOplockEx));
    ldr.registerExport("ntoskrnl.exe", "FsRtlOplockBreakToNone", reinterpret_cast<void*>(FsRtlOplockBreakToNone));
    ldr.registerExport("ntoskrnl.exe", "FsRtlCurrentOplock", reinterpret_cast<void*>(FsRtlCurrentOplock));
    ldr.registerExport("ntoskrnl.exe", "FsRtlCurrentOplockH", reinterpret_cast<void*>(FsRtlCurrentOplockH));
    ldr.registerExport("ntoskrnl.exe", "FsRtlCurrentBatchOplock", reinterpret_cast<void*>(FsRtlCurrentBatchOplock));
    ldr.registerExport("ntoskrnl.exe", "FsRtlOplockIsFastIoPossible", reinterpret_cast<void*>(FsRtlOplockIsFastIoPossible));

    ldr.registerExport("ntoskrnl.exe", "FsRtlNotifyInitializeSync", reinterpret_cast<void*>(FsRtlNotifyInitializeSync));
    ldr.registerExport("ntoskrnl.exe", "FsRtlNotifyUninitializeSync", reinterpret_cast<void*>(FsRtlNotifyUninitializeSync));
    ldr.registerExport("ntoskrnl.exe", "FsRtlNotifyFullChangeDirectory", reinterpret_cast<void*>(FsRtlNotifyFullChangeDirectory));
    ldr.registerExport("ntoskrnl.exe", "FsRtlNotifyFullReportChange", reinterpret_cast<void*>(FsRtlNotifyFullReportChange));
    ldr.registerExport("ntoskrnl.exe", "FsRtlNotifyCleanup", reinterpret_cast<void*>(FsRtlNotifyCleanup));

    ldr.registerExport("ntoskrnl.exe", "FsRtlInitializeTunnelCache", reinterpret_cast<void*>(FsRtlInitializeTunnelCache));
    ldr.registerExport("ntoskrnl.exe", "FsRtlDeleteTunnelCache", reinterpret_cast<void*>(FsRtlDeleteTunnelCache));
    ldr.registerExport("ntoskrnl.exe", "FsRtlAddToTunnelCache", reinterpret_cast<void*>(FsRtlAddToTunnelCache));
    ldr.registerExport("ntoskrnl.exe", "FsRtlFindInTunnelCache", reinterpret_cast<void*>(FsRtlFindInTunnelCache));
    ldr.registerExport("ntoskrnl.exe", "FsRtlDeleteKeyFromTunnelCache", reinterpret_cast<void*>(FsRtlDeleteKeyFromTunnelCache));

    ldr.registerExport("ntoskrnl.exe", "FsRtlDoesNameContainWildCards", reinterpret_cast<void*>(FsRtlDoesNameContainWildCards));
    ldr.registerExport("ntoskrnl.exe", "FsRtlAreNamesEqual", reinterpret_cast<void*>(FsRtlAreNamesEqual));
    ldr.registerExport("ntoskrnl.exe", "FsRtlIsNameInExpression", reinterpret_cast<void*>(FsRtlIsNameInExpression));

    ldr.registerExport("ntoskrnl.exe", "FsRtlCopyRead", reinterpret_cast<void*>(FsRtlCopyRead));
    ldr.registerExport("ntoskrnl.exe", "FsRtlCopyWrite", reinterpret_cast<void*>(FsRtlCopyWrite));
    ldr.registerExport("ntoskrnl.exe", "FsRtlGetTelemetry", reinterpret_cast<void*>(FsRtlGetTelemetry));

    auto& scm = scm::ServiceControlManager::get();
    auto fsrtlSvc = std::make_shared<scm::ServiceRecord>();
    fsrtlSvc->serviceName           = L"FsRtlRuntime";
    fsrtlSvc->displayName           = L"Sovereign NT File System Runtime Library (FsRtl)";
    fsrtlSvc->serviceType           = scm::SERVICE_KERNEL_DRIVER;
    fsrtlSvc->startType             = scm::SERVICE_BOOT_START;
    fsrtlSvc->errorControl          = scm::SERVICE_ERROR_CRITICAL;
    fsrtlSvc->binaryPath            = L"\\SystemRoot\\System32\\ntoskrnl.exe";
    fsrtlSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(fsrtlSvc);

    version::VersionDatabase::Instance().RegisterModule(
        "fsrtl_executive", "10.0.26100.1", "Sovereign NT File System Runtime Library (FsRtl)");

    TitanFsRtlManager::Instance().initialize();
}

} // namespace micant::km::fsrtl

// Forward into micant parent namespace
namespace micant {
    using km::fsrtl::TitanFsRtlManager;
    using km::fsrtl::FSRTL_ADVANCED_FCB_HEADER;
    using km::fsrtl::FSRTL_COMMON_FCB_HEADER;
    using km::fsrtl::FSRTL_PER_STREAM_CONTEXT;
    using km::fsrtl::FSRTL_PER_FILEOBJECT_CONTEXT;
    using km::fsrtl::BASE_MCB;
    using km::fsrtl::LARGE_MCB;
    using km::fsrtl::FILE_LOCK;
    using km::fsrtl::FILE_LOCK_INFO;
    using km::fsrtl::OPLOCK;
    using km::fsrtl::REQUEST_OPLOCK_INPUT_BUFFER;
    using km::fsrtl::REQUEST_OPLOCK_OUTPUT_BUFFER;
    using km::fsrtl::FILE_NOTIFY_INFORMATION;
    using km::fsrtl::PNOTIFY_SYNC;
    using km::fsrtl::TUNNEL;
    using km::fsrtl::FSRTL_TELEMETRY;
    using km::fsrtl::FsRtlSetupAdvancedHeader;
    using km::fsrtl::FsRtlTeardownPerStreamContexts;
    using km::fsrtl::FsRtlInsertPerStreamContext;
    using km::fsrtl::FsRtlLookupPerStreamContextInternal;
    using km::fsrtl::FsRtlRemovePerStreamContext;
    using km::fsrtl::FsRtlInsertPerFileObjectContext;
    using km::fsrtl::FsRtlLookupPerFileObjectContext;
    using km::fsrtl::FsRtlRemovePerFileObjectContext;
    using km::fsrtl::FsRtlInitializeBaseMcb;
    using km::fsrtl::FsRtlUninitializeBaseMcb;
    using km::fsrtl::FsRtlResetBaseMcb;
    using km::fsrtl::FsRtlInitializeLargeMcb;
    using km::fsrtl::FsRtlUninitializeLargeMcb;
    using km::fsrtl::FsRtlResetLargeMcb;
    using km::fsrtl::FsRtlAddLargeMcbEntry;
    using km::fsrtl::FsRtlRemoveLargeMcbEntry;
    using km::fsrtl::FsRtlLookupLargeMcbEntry;
    using km::fsrtl::FsRtlLookupLastLargeMcbEntry;
    using km::fsrtl::FsRtlLookupLastLargeMcbEntryAndIndex;
    using km::fsrtl::FsRtlNumberOfRunsInLargeMcb;
    using km::fsrtl::FsRtlGetNextLargeMcbEntry;
    using km::fsrtl::FsRtlTruncateLargeMcb;
    using km::fsrtl::FsRtlSplitLargeMcb;
    using km::fsrtl::FsRtlInitializeFileLock;
    using km::fsrtl::FsRtlUninitializeFileLock;
    using km::fsrtl::FsRtlAllocateFileLock;
    using km::fsrtl::FsRtlFreeFileLock;
    using km::fsrtl::FsRtlFastLock;
    using km::fsrtl::FsRtlFastUnlockSingle;
    using km::fsrtl::FsRtlFastUnlockAll;
    using km::fsrtl::FsRtlFastUnlockAllByKey;
    using km::fsrtl::FsRtlFastCheckLockForRead;
    using km::fsrtl::FsRtlFastCheckLockForWrite;
    using km::fsrtl::FsRtlGetNextFileLock;
    using km::fsrtl::FsRtlInitializeOplock;
    using km::fsrtl::FsRtlUninitializeOplock;
    using km::fsrtl::FsRtlOplockFsctrlEx;
    using km::fsrtl::FsRtlCheckOplockEx;
    using km::fsrtl::FsRtlOplockBreakToNone;
    using km::fsrtl::FsRtlCurrentOplock;
    using km::fsrtl::FsRtlCurrentOplockH;
    using km::fsrtl::FsRtlCurrentBatchOplock;
    using km::fsrtl::FsRtlOplockIsFastIoPossible;
    using km::fsrtl::FsRtlNotifyInitializeSync;
    using km::fsrtl::FsRtlNotifyUninitializeSync;
    using km::fsrtl::FsRtlNotifyFullChangeDirectory;
    using km::fsrtl::FsRtlNotifyFullReportChange;
    using km::fsrtl::FsRtlNotifyCleanup;
    using km::fsrtl::FsRtlInitializeTunnelCache;
    using km::fsrtl::FsRtlDeleteTunnelCache;
    using km::fsrtl::FsRtlAddToTunnelCache;
    using km::fsrtl::FsRtlFindInTunnelCache;
    using km::fsrtl::FsRtlDeleteKeyFromTunnelCache;
    using km::fsrtl::FsRtlDoesNameContainWildCards;
    using km::fsrtl::FsRtlAreNamesEqual;
    using km::fsrtl::FsRtlIsNameInExpression;
    using km::fsrtl::FsRtlCopyRead;
    using km::fsrtl::FsRtlCopyWrite;
    using km::fsrtl::FsRtlGetTelemetry;
    using km::fsrtl::FsRtlQueryTelemetry;
    using km::fsrtl::InitializeFsRtlSubsystemExports;
} // namespace micant
