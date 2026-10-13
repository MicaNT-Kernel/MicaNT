#pragma once
/**
 * @file ktm.hpp
 * @brief Horizon 45: Sovereign NT Kernel Transaction Manager (KTM / Tm* — base/ntos/tm),
 *        Two-Phase Commit (2PC) Coordinator, Common Log File System (CLFS) Write-Ahead Log
 *        & ktmw32.dll User-Mode Bridge.
 *
 * Implements the clean-room Windows NT Kernel Transaction Manager (base/ntos/tm) and
 * Common Log File System (CLFS / clfsw32.sys):
 *   1. Four First-Class Executive Object Types:
 *      - KTRANSACTION_MANAGER (TmTransactionManagerObjectType)
 *      - KRESOURCE_MANAGER    (TmResourceManagerObjectType)
 *      - KTRANSACTION         (TmTransactionObjectType)
 *      - KENLISTMENT          (TmEnlistmentObjectType)
 *   2. Common Log File System (CLFS — Clfs* API):
 *      - 64-bit structured CLFS_LSN (ContainerId, BlockOffset, RecordSequence).
 *      - ClfsCreateLogFile, ClfsCloseLogFileObject, ClfsReserveAndAppendLog,
 *        ClfsWriteRestartArea, ClfsReadRestartArea, ClfsReadLogRecord, ClfsFlushToLsn.
 *   3. Single-Phase (1PC) & Two-Phase Commit (2PC) Protocol State Machine:
 *      - Automatic Single-Phase Commit (TRANSACTION_NOTIFY_SINGLE_PHASE_COMMIT) when
 *        only a single active non-read-only Resource Manager is enlisted, with seamless
 *        fallback to full 2PC if TmSinglePhaseReject / NtSinglePhaseReject is invoked.
 *      - Full Two-Phase Commit (PrePrepare -> Prepare -> Commit) across multiple
 *        enlisted Resource Managers (e.g., Transactional Registry TxR + File System TxF).
 *      - Read-Only Enlistment optimization (TmReadOnlyEnlistment / NtReadOnlyEnlistment).
 *      - Atomic Veto & Rollback broadcast (TRANSACTION_NOTIFY_ROLLBACK) when any
 *        enlisted RM aborts during Active, PrePrepare, or Prepare phases.
 *   4. Crash Recovery & CLFS Log Replay:
 *      - NtRecoverTransactionManager & NtRecoverResourceManager replay CLFS commit/prepare
 *        records to deterministically roll forward committed transactions and roll back
 *        in-doubt/aborted transactions.
 *
 * Clean-room ISO C++23 implementation. Zero proprietary or decompiled code.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <algorithm>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/native/janusldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::km::ktm {

using PVOID          = void*;
using ULONG          = uint32_t;
using USHORT         = uint16_t;
using UCHAR          = uint8_t;
using BOOLEAN        = uint8_t;
using LONGLONG       = int64_t;
using ULONGLONG      = uint64_t;
using LARGE_INTEGER  = micant::LargeInteger;
using PLARGE_INTEGER = micant::LargeInteger*;

inline constexpr BOOLEAN FALSE_B = 0;
inline constexpr BOOLEAN TRUE_B  = 1;

// ============================================================================
// 1. KTM GUID, Access Masks, Notification Flags & State Enumerations
// ============================================================================

struct KTM_GUID {
    uint32_t Data1{0};
    uint16_t Data2{0};
    uint16_t Data3{0};
    uint8_t  Data4[8]{0};

    constexpr bool operator==(const KTM_GUID& other) const noexcept {
        if (Data1 != other.Data1 || Data2 != other.Data2 || Data3 != other.Data3) return false;
        for (int i = 0; i < 8; ++i) {
            if (Data4[i] != other.Data4[i]) return false;
        }
        return true;
    }
    constexpr bool operator!=(const KTM_GUID& other) const noexcept {
        return !(*this == other);
    }
};

// Transaction Notification Masks
inline constexpr uint32_t TRANSACTION_NOTIFY_MASK                = 0x3FFFFFFF;
inline constexpr uint32_t TRANSACTION_NOTIFY_PREPREPARE          = 0x00000001;
inline constexpr uint32_t TRANSACTION_NOTIFY_PREPARE             = 0x00000002;
inline constexpr uint32_t TRANSACTION_NOTIFY_COMMIT              = 0x00000004;
inline constexpr uint32_t TRANSACTION_NOTIFY_ROLLBACK            = 0x00000008;
inline constexpr uint32_t TRANSACTION_NOTIFY_PREPREPARE_COMPLETE = 0x00000010;
inline constexpr uint32_t TRANSACTION_NOTIFY_PREPARE_COMPLETE    = 0x00000020;
inline constexpr uint32_t TRANSACTION_NOTIFY_COMMIT_COMPLETE     = 0x00000040;
inline constexpr uint32_t TRANSACTION_NOTIFY_ROLLBACK_COMPLETE   = 0x00000080;
inline constexpr uint32_t TRANSACTION_NOTIFY_RECOVER             = 0x00000100;
inline constexpr uint32_t TRANSACTION_NOTIFY_SINGLE_PHASE_COMMIT = 0x00000200;
inline constexpr uint32_t TRANSACTION_NOTIFY_DELEGATE_COMMIT     = 0x00000400;
inline constexpr uint32_t TRANSACTION_NOTIFY_RECOVER_QUERY       = 0x00000800;
inline constexpr uint32_t TRANSACTION_NOTIFY_ENLIST_PREPREPARE   = 0x00001000;
inline constexpr uint32_t TRANSACTION_NOTIFY_LAST_RECOVER        = 0x00002000;
inline constexpr uint32_t TRANSACTION_NOTIFY_INDOUBT             = 0x00004000;

// Resource Manager & Transaction Options
inline constexpr uint32_t RESOURCE_MANAGER_VOLATILE              = 0x00000001;
inline constexpr uint32_t RESOURCE_MANAGER_COMMUNICATION         = 0x00000002;
inline constexpr uint32_t TRANSACTION_DO_NOT_PROMOTE             = 0x00000001;
inline constexpr uint32_t TRANSACTION_MANAGER_VOLATILE           = 0x00000001;
inline constexpr uint32_t TRANSACTION_MANAGER_COMMIT_DEFAULT     = 0x00000000;

// Standard Access Masks
inline constexpr uint32_t TRANSACTIONMANAGER_ALL_ACCESS          = 0x000F003F;
inline constexpr uint32_t RESOURCEMANAGER_ALL_ACCESS             = 0x000F007F;
inline constexpr uint32_t TRANSACTION_ALL_ACCESS                 = 0x000F003F;
inline constexpr uint32_t ENLISTMENT_ALL_ACCESS                  = 0x000F001F;

enum TRANSACTION_STATE : uint32_t {
    TransactionStateNormal          = 1,
    TransactionStateIndoubt         = 2,
    TransactionStateCommittedNotify = 3
};

enum TRANSACTION_OUTCOME : uint32_t {
    TransactionOutcomeUndetermined = 1,
    TransactionOutcomeCommitted    = 2,
    TransactionOutcomeAborted      = 3
};

enum TRANSACTION_INFORMATION_CLASS : uint32_t {
    TransactionBasicInformation              = 0,
    TransactionPropertiesInformation         = 1,
    TransactionEnlistmentInformation         = 2,
    TransactionSuperiorEnlistmentInformation = 3
};

enum KENLISTMENT_STATE : uint32_t {
    EnlistmentStateInitialized       = 0,
    EnlistmentStateActive            = 1,
    EnlistmentStatePrePreparing      = 2,
    EnlistmentStatePrePrepared       = 3,
    EnlistmentStatePreparing         = 4,
    EnlistmentStatePrepared          = 5,
    EnlistmentStateSinglePhaseCommit = 6,
    EnlistmentStateCommitting        = 7,
    EnlistmentStateCommitted         = 8,
    EnlistmentStateAborted           = 9,
    EnlistmentStateReadOnly          = 10
};

struct TRANSACTION_BASIC_INFORMATION {
    KTM_GUID TransactionId{};
    uint32_t State{TransactionStateNormal};
    uint32_t Outcome{TransactionOutcomeUndetermined};
};

struct TRANSACTION_PROPERTIES_INFORMATION {
    uint32_t      IsolationLevel{0};
    uint32_t      IsolationFlags{0};
    LARGE_INTEGER Timeout{};
    uint32_t      Outcome{TransactionOutcomeUndetermined};
    uint32_t      DescriptionLength{0};
    wchar_t       Description[64]{0};
};

struct TRANSACTION_NOTIFICATION {
    PVOID         TransactionKey{nullptr};
    uint32_t      TransactionNotification{0};
    LARGE_INTEGER TmVirtualClock{};
    uint32_t      ArgumentLength{0};
};

// Kernel-mode Resource Manager Callback Routine (TmEnableCallbacks)
using PTM_RM_NOTIFICATION = NtStatus (*)(
    Handle                    EnlistmentObjectHandle,
    PVOID                     RMContext,
    PVOID                     TransactionContext,
    uint32_t                  TransactionNotification,
    PLARGE_INTEGER            TmVirtualClock,
    uint32_t                  ArgumentLength,
    PVOID                     Argument
);

// ============================================================================
// 2. Common Log File System (CLFS — clfsw32.sys) Structures & LSN Primitives
// ============================================================================

struct CLFS_LSN {
    uint64_t ullOffset{0};

    static constexpr CLFS_LSN make(uint32_t containerId, uint32_t blockOffset, uint32_t recordSeq) noexcept {
        uint64_t val = (static_cast<uint64_t>(containerId) << 32) |
                       ((static_cast<uint64_t>(blockOffset) & 0xFFFFFULL) << 12) |
                       (static_cast<uint64_t>(recordSeq) & 0xFFFULL);
        return CLFS_LSN{ val };
    }

    constexpr uint32_t containerId() const noexcept {
        return static_cast<uint32_t>(ullOffset >> 32);
    }
    constexpr uint32_t blockOffset() const noexcept {
        return static_cast<uint32_t>((ullOffset >> 12) & 0xFFFFFULL);
    }
    constexpr uint32_t recordSequence() const noexcept {
        return static_cast<uint32_t>(ullOffset & 0xFFFULL);
    }
};

inline constexpr CLFS_LSN CLFS_LSN_NULL    = { 0ULL };
inline constexpr CLFS_LSN CLFS_LSN_INVALID = { 0xFFFFFFFFFFFFFFFFULL };

inline constexpr BOOLEAN ClfsLsnEqual(const CLFS_LSN* lsn1, const CLFS_LSN* lsn2) noexcept {
    if (!lsn1 || !lsn2) return FALSE_B;
    return (lsn1->ullOffset == lsn2->ullOffset) ? TRUE_B : FALSE_B;
}

inline constexpr BOOLEAN ClfsLsnLess(const CLFS_LSN* lsn1, const CLFS_LSN* lsn2) noexcept {
    if (!lsn1 || !lsn2) return FALSE_B;
    return (lsn1->ullOffset < lsn2->ullOffset) ? TRUE_B : FALSE_B;
}

inline constexpr BOOLEAN ClfsLsnGreater(const CLFS_LSN* lsn1, const CLFS_LSN* lsn2) noexcept {
    if (!lsn1 || !lsn2) return FALSE_B;
    return (lsn1->ullOffset > lsn2->ullOffset) ? TRUE_B : FALSE_B;
}

enum CLFS_RECORD_TYPE : uint8_t {
    ClfsNullRecord    = 0x00,
    ClfsDataRecord    = 0x01,
    ClfsRestartRecord = 0x02
};

enum KTM_LOG_RECORD_KIND : uint32_t {
    KtmLogRecordBeginTransaction = 1,
    KtmLogRecordPrepare          = 2,
    KtmLogRecordCommit           = 3,
    KtmLogRecordAbort            = 4,
    KtmLogRecordCheckpoint       = 5
};

struct CLFS_LOG_RECORD {
    CLFS_LSN            Lsn{CLFS_LSN_NULL};
    CLFS_LSN            UndoNextLsn{CLFS_LSN_NULL};
    CLFS_LSN            PreviousLsn{CLFS_LSN_NULL};
    CLFS_RECORD_TYPE    RecordType{ClfsDataRecord};
    KTM_LOG_RECORD_KIND KtmKind{KtmLogRecordBeginTransaction};
    KTM_GUID            TransactionUow{};
    int64_t             VirtualClock{0};
    std::vector<uint8_t> Payload{};
};

struct CLFS_LOG_FILE_OBJECT {
    Handle                        LogFileHandle{0};
    std::wstring                  LogFileName{};
    uint32_t                      CurrentContainerId{1};
    uint32_t                      CurrentBlockOffset{1};
    uint32_t                      CurrentRecordSeq{1};
    CLFS_LSN                      BaseLsn{CLFS_LSN::make(1, 1, 1)};
    CLFS_LSN                      LastFlushedLsn{CLFS_LSN_NULL};
    CLFS_LSN                      LastRestartLsn{CLFS_LSN_NULL};
    std::vector<uint8_t>          RestartAreaData{};
    std::vector<CLFS_LOG_RECORD>  Records{};
};

// ============================================================================
// 3. Internal Executive Objects (KTRANSACTION_MANAGER, KRESOURCE_MANAGER,
//    KTRANSACTION, KENLISTMENT)
// ============================================================================

struct KTRANSACTION_MANAGER {
    Handle       TmHandle{0};
    KTM_GUID     TmIdentity{};
    std::wstring LogFileName{};
    Handle       ClfsLogHandle{0};
    uint32_t     CreateOptions{0};
    int64_t      VirtualClock{1000};
    CLFS_LSN     LastCommitLsn{CLFS_LSN_NULL};
    bool         Recovered{false};
};

struct KRESOURCE_MANAGER {
    Handle                             RmHandle{0};
    Handle                             TmHandle{0};
    KTM_GUID                           RmGuid{};
    uint32_t                           CreateOptions{0};
    std::wstring                       Description{};
    PTM_RM_NOTIFICATION                CallbackRoutine{nullptr};
    PVOID                              RMContext{nullptr};
    bool                               CallbacksEnabled{false};
    std::deque<TRANSACTION_NOTIFICATION> NotificationQueue{};
};

struct KENLISTMENT {
    Handle            EnlistmentHandle{0};
    Handle            RmHandle{0};
    Handle            TransactionHandle{0};
    KTM_GUID          EnlistmentId{};
    uint32_t          NotificationMask{0};
    PVOID             EnlistmentKey{nullptr};
    KENLISTMENT_STATE State{EnlistmentStateActive};
    bool              SinglePhaseRejected{false};
};

struct KTRANSACTION {
    Handle              TransactionHandle{0};
    Handle              TmHandle{0};
    KTM_GUID            Uow{};
    TRANSACTION_STATE   State{TransactionStateNormal};
    TRANSACTION_OUTCOME Outcome{TransactionOutcomeUndetermined};
    uint32_t            CreateOptions{0};
    uint32_t            IsolationLevel{0};
    uint32_t            IsolationFlags{0};
    LARGE_INTEGER       Timeout{};
    std::wstring        Description{};
    CLFS_LSN            PrepareLsn{CLFS_LSN_NULL};
    CLFS_LSN            CommitLsn{CLFS_LSN_NULL};
    std::vector<Handle> EnlistmentHandles{};
};

// ============================================================================
// 4. KTM & CLFS Executive Telemetry
// ============================================================================

struct KTM_TELEMETRY {
    uint64_t TotalTransactionManagersCreated{0};
    uint64_t TotalResourceManagersCreated{0};
    uint64_t TotalTransactionsCreated{0};
    uint64_t TotalEnlistmentsCreated{0};
    uint64_t TotalSinglePhaseCommits{0};
    uint64_t TotalSinglePhaseRejections{0};
    uint64_t TotalTwoPhaseCommits{0};
    uint64_t TotalTransactionsCommitted{0};
    uint64_t TotalTransactionsAborted{0};
    uint64_t TotalReadOnlyEnlistments{0};
    uint64_t TotalRmNotificationsDelivered{0};
    uint64_t TotalClfsRecordsAppended{0};
    uint64_t TotalClfsRestartWrites{0};
    uint64_t TotalRecoveriesExecuted{0};
};

// ============================================================================
// 5. Sovereign NT Kernel Transaction Manager & CLFS Engine (TitanKtmManager)
// ============================================================================

class TitanKtmManager {
public:
    static TitanKtmManager& Instance() noexcept {
        static TitanKtmManager s_instance;
        return s_instance;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_clfsLogs.clear();
        m_tms.clear();
        m_rms.clear();
        m_txs.clear();
        m_enlistments.clear();
        m_nextHandle = 0x5400;
        m_guidSeq    = 1;
        m_telemetry  = KTM_TELEMETRY{};
    }

    // ------------------------------------------------------------------------
    // 5.1 Common Log File System (CLFS) Operations
    // ------------------------------------------------------------------------

    NtStatus clfsCreateLogFile(
        Handle*        outLogFileHandle,
        const wchar_t* logFileName,
        uint32_t       desiredAccess,
        uint32_t       shareMode,
        uint32_t       createDisposition
    ) {
        (void)desiredAccess;
        (void)shareMode;
        (void)createDisposition;
        if (!outLogFileHandle) return NtStatus::InvalidParameter;

        std::lock_guard<std::mutex> lock(m_mutex);
        Handle hLog = allocateHandleUnlocked();
        auto logObj = std::make_unique<CLFS_LOG_FILE_OBJECT>();
        logObj->LogFileHandle = hLog;
        logObj->LogFileName   = logFileName ? logFileName : L"log:\\SystemVolumeInformation\\KtmLog.blf";
        m_clfsLogs[hLog]      = std::move(logObj);
        *outLogFileHandle     = hLog;
        return NtStatus::Success;
    }

    NtStatus clfsCloseLogFileObject(Handle logFileHandle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_clfsLogs.find(logFileHandle);
        if (it == m_clfsLogs.end()) return NtStatus::InvalidHandle;
        m_clfsLogs.erase(it);
        return NtStatus::Success;
    }

    NtStatus clfsReserveAndAppendLog(
        Handle              logFileHandle,
        const void*         buffer,
        uint32_t            bufferLength,
        const CLFS_LSN*     undoNextLsn,
        const CLFS_LSN*     previousLsn,
        CLFS_RECORD_TYPE    recordType,
        KTM_LOG_RECORD_KIND ktmKind,
        const KTM_GUID&     txUow,
        int64_t             virtualClock,
        CLFS_LSN*           outLsn
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_clfsLogs.find(logFileHandle);
        if (it == m_clfsLogs.end()) return NtStatus::InvalidHandle;

        CLFS_LSN lsn = appendClfsRecordUnlocked(
            it->second.get(),
            buffer,
            bufferLength,
            undoNextLsn ? *undoNextLsn : CLFS_LSN_NULL,
            previousLsn ? *previousLsn : CLFS_LSN_NULL,
            recordType,
            ktmKind,
            txUow,
            virtualClock
        );
        if (outLsn) *outLsn = lsn;
        return NtStatus::Success;
    }

    NtStatus clfsWriteRestartArea(
        Handle      logFileHandle,
        const void* restartBuffer,
        uint32_t    restartBufferLength,
        CLFS_LSN*   outRestartLsn
    ) {
        if (!restartBuffer || restartBufferLength == 0) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_clfsLogs.find(logFileHandle);
        if (it == m_clfsLogs.end()) return NtStatus::InvalidHandle;

        CLFS_LOG_FILE_OBJECT* logObj = it->second.get();
        logObj->RestartAreaData.assign(
            static_cast<const uint8_t*>(restartBuffer),
            static_cast<const uint8_t*>(restartBuffer) + restartBufferLength
        );

        CLFS_LSN rLsn = appendClfsRecordUnlocked(
            logObj,
            restartBuffer,
            restartBufferLength,
            CLFS_LSN_NULL,
            logObj->LastRestartLsn,
            ClfsRestartRecord,
            KtmLogRecordCheckpoint,
            KTM_GUID{},
            0
        );
        logObj->LastRestartLsn = rLsn;
        logObj->LastFlushedLsn = rLsn;
        if (outRestartLsn) *outRestartLsn = rLsn;
        m_telemetry.TotalClfsRestartWrites++;
        return NtStatus::Success;
    }

    NtStatus clfsReadRestartArea(
        Handle    logFileHandle,
        void*     outRestartBuffer,
        uint32_t  bufferCapacity,
        uint32_t* outBytesRead,
        CLFS_LSN* outRestartLsn
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_clfsLogs.find(logFileHandle);
        if (it == m_clfsLogs.end()) return NtStatus::InvalidHandle;

        const CLFS_LOG_FILE_OBJECT* logObj = it->second.get();
        if (logObj->RestartAreaData.empty()) return NtStatus::NotFound;

        uint32_t sz = static_cast<uint32_t>(logObj->RestartAreaData.size());
        if (outBytesRead) *outBytesRead = sz;
        if (outRestartLsn) *outRestartLsn = logObj->LastRestartLsn;
        if (!outRestartBuffer || bufferCapacity < sz) return NtStatus::BufferTooSmall;

        std::memcpy(outRestartBuffer, logObj->RestartAreaData.data(), sz);
        return NtStatus::Success;
    }

    NtStatus clfsFlushToLsn(Handle logFileHandle, const CLFS_LSN* targetLsn, CLFS_LSN* outLastFlushedLsn) {
        if (!targetLsn) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_clfsLogs.find(logFileHandle);
        if (it == m_clfsLogs.end()) return NtStatus::InvalidHandle;

        if (targetLsn->ullOffset > it->second->LastFlushedLsn.ullOffset) {
            it->second->LastFlushedLsn = *targetLsn;
        }
        if (outLastFlushedLsn) *outLastFlushedLsn = it->second->LastFlushedLsn;
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // 5.2 Transaction Manager (KTRANSACTION_MANAGER) Lifecycle & Recovery
    // ------------------------------------------------------------------------

    NtStatus createTransactionManager(
        Handle*           outTmHandle,
        uint32_t          desiredAccess,
        ObjectAttributes* objectAttributes,
        const wchar_t*    logFileName,
        uint32_t          createOptions,
        uint32_t          commitStrength
    ) {
        (void)desiredAccess;
        (void)objectAttributes;
        (void)commitStrength;
        if (!outTmHandle) return NtStatus::InvalidParameter;

        std::lock_guard<std::mutex> lock(m_mutex);
        Handle hTm = allocateHandleUnlocked();
        Handle hLog = allocateHandleUnlocked();

        auto logObj = std::make_unique<CLFS_LOG_FILE_OBJECT>();
        logObj->LogFileHandle = hLog;
        logObj->LogFileName   = logFileName ? logFileName : L"\\??\\C:\\SystemVolumeInformation\\KtmLog.blf";
        m_clfsLogs[hLog]      = std::move(logObj);

        auto tm = std::make_unique<KTRANSACTION_MANAGER>();
        tm->TmHandle      = hTm;
        tm->TmIdentity    = generateGuidUnlocked(0x544D0000u);
        tm->LogFileName   = logFileName ? logFileName : L"\\??\\C:\\SystemVolumeInformation\\KtmLog.blf";
        tm->ClfsLogHandle = hLog;
        tm->CreateOptions = createOptions;
        tm->VirtualClock  = 1000;

        m_tms[hTm] = std::move(tm);
        *outTmHandle = hTm;
        m_telemetry.TotalTransactionManagersCreated++;
        return NtStatus::Success;
    }

    NtStatus recoverTransactionManager(Handle tmHandle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto tmIt = m_tms.find(tmHandle);
        if (tmIt == m_tms.end()) return NtStatus::InvalidHandle;

        KTRANSACTION_MANAGER* tm = tmIt->second.get();
        tm->Recovered = true;

        // Inspect any transactions belonging to this TM that are still in-doubt/prepared
        for (auto& [txHandle, txPtr] : m_txs) {
            if (txPtr->TmHandle != tmHandle) continue;
            if (txPtr->Outcome == TransactionOutcomeUndetermined) {
                if (txPtr->CommitLsn.ullOffset != 0) {
                    // Durable commit record exists in CLFS -> roll forward!
                    txPtr->Outcome = TransactionOutcomeCommitted;
                    txPtr->State   = TransactionStateCommittedNotify;
                    broadcastNotificationUnlocked(txPtr.get(), TRANSACTION_NOTIFY_COMMIT);
                } else {
                    // Prepared or active without durable commit record -> roll back!
                    txPtr->Outcome = TransactionOutcomeAborted;
                    broadcastNotificationUnlocked(txPtr.get(), TRANSACTION_NOTIFY_ROLLBACK);
                }
            }
        }

        m_telemetry.TotalRecoveriesExecuted++;
        return NtStatus::Success;
    }

    NtStatus rollforwardTransactionManager(Handle tmHandle, const LARGE_INTEGER* targetVirtualClock) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto tmIt = m_tms.find(tmHandle);
        if (tmIt == m_tms.end()) return NtStatus::InvalidHandle;
        if (targetVirtualClock && targetVirtualClock->quadPart > tmIt->second->VirtualClock) {
            tmIt->second->VirtualClock = targetVirtualClock->quadPart;
        }
        return NtStatus::Success;
    }

    Handle getDefaultTransactionManagerUnlocked() {
        if (!m_tms.empty()) {
            return m_tms.begin()->first;
        }
        Handle hTm = allocateHandleUnlocked();
        Handle hLog = allocateHandleUnlocked();

        auto logObj = std::make_unique<CLFS_LOG_FILE_OBJECT>();
        logObj->LogFileHandle = hLog;
        logObj->LogFileName   = L"\\??\\C:\\SystemVolumeInformation\\DefaultKtmLog.blf";
        m_clfsLogs[hLog]      = std::move(logObj);

        auto tm = std::make_unique<KTRANSACTION_MANAGER>();
        tm->TmHandle      = hTm;
        tm->TmIdentity    = generateGuidUnlocked(0x544D0000u);
        tm->LogFileName   = L"\\??\\C:\\SystemVolumeInformation\\DefaultKtmLog.blf";
        tm->ClfsLogHandle = hLog;
        m_tms[hTm]        = std::move(tm);
        m_telemetry.TotalTransactionManagersCreated++;
        return hTm;
    }

    // ------------------------------------------------------------------------
    // 5.3 Resource Manager (KRESOURCE_MANAGER) Lifecycle & Callbacks
    // ------------------------------------------------------------------------

    NtStatus createResourceManager(
        Handle*           outRmHandle,
        uint32_t          desiredAccess,
        ObjectAttributes* objectAttributes,
        Handle            tmHandle,
        const KTM_GUID*   rmGuid,
        uint32_t          createOptions,
        const wchar_t*    description
    ) {
        (void)desiredAccess;
        (void)objectAttributes;
        if (!outRmHandle) return NtStatus::InvalidParameter;

        std::lock_guard<std::mutex> lock(m_mutex);
        if (tmHandle == 0) {
            tmHandle = getDefaultTransactionManagerUnlocked();
        } else if (!m_tms.contains(tmHandle)) {
            return NtStatus::InvalidHandle;
        }

        Handle hRm = allocateHandleUnlocked();
        auto rm = std::make_unique<KRESOURCE_MANAGER>();
        rm->RmHandle      = hRm;
        rm->TmHandle      = tmHandle;
        rm->RmGuid        = rmGuid ? *rmGuid : generateGuidUnlocked(0x524D0000u);
        rm->CreateOptions = createOptions;
        rm->Description   = description ? description : L"MicaNT Transactional Resource Manager";

        m_rms[hRm] = std::move(rm);
        *outRmHandle = hRm;
        m_telemetry.TotalResourceManagersCreated++;
        return NtStatus::Success;
    }

    NtStatus enableRmCallbacks(
        Handle              rmHandle,
        PTM_RM_NOTIFICATION callbackRoutine,
        PVOID               rmContext
    ) {
        if (!callbackRoutine) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_rms.find(rmHandle);
        if (it == m_rms.end()) return NtStatus::InvalidHandle;

        it->second->CallbackRoutine  = callbackRoutine;
        it->second->RMContext        = rmContext;
        it->second->CallbacksEnabled = true;
        return NtStatus::Success;
    }

    NtStatus getNotificationResourceManager(
        Handle                    rmHandle,
        TRANSACTION_NOTIFICATION* outNotification,
        uint32_t                  notificationLength,
        PLARGE_INTEGER            timeout,
        uint32_t*                 outReturnLength
    ) {
        (void)timeout;
        if (!outNotification || notificationLength < sizeof(TRANSACTION_NOTIFICATION)) {
            return NtStatus::BufferTooSmall;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_rms.find(rmHandle);
        if (it == m_rms.end()) return NtStatus::InvalidHandle;

        if (it->second->NotificationQueue.empty()) {
            return NtStatus::Timeout;
        }

        *outNotification = it->second->NotificationQueue.front();
        it->second->NotificationQueue.pop_front();
        if (outReturnLength) *outReturnLength = sizeof(TRANSACTION_NOTIFICATION);
        return NtStatus::Success;
    }

    NtStatus recoverResourceManager(Handle rmHandle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto rmIt = m_rms.find(rmHandle);
        if (rmIt == m_rms.end()) return NtStatus::InvalidHandle;

        KRESOURCE_MANAGER* rm = rmIt->second.get();
        int64_t vclock = 1000;
        auto tmIt = m_tms.find(rm->TmHandle);
        if (tmIt != m_tms.end()) {
            vclock = tmIt->second->VirtualClock;
        }

        TRANSACTION_NOTIFICATION lastRec{};
        lastRec.TransactionKey          = nullptr;
        lastRec.TransactionNotification = TRANSACTION_NOTIFY_LAST_RECOVER;
        lastRec.TmVirtualClock.quadPart = vclock;
        rm->NotificationQueue.push_back(lastRec);
        m_telemetry.TotalRecoveriesExecuted++;
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // 5.4 Transaction (KTRANSACTION) Creation, Query & Enlistment
    // ------------------------------------------------------------------------

    NtStatus createTransaction(
        Handle*           outTransactionHandle,
        uint32_t          desiredAccess,
        ObjectAttributes* objectAttributes,
        const KTM_GUID*   uow,
        Handle            tmHandle,
        uint32_t          createOptions,
        uint32_t          isolationLevel,
        uint32_t          isolationFlags,
        PLARGE_INTEGER    timeout,
        const wchar_t*    description
    ) {
        (void)desiredAccess;
        (void)objectAttributes;
        if (!outTransactionHandle) return NtStatus::InvalidParameter;

        std::lock_guard<std::mutex> lock(m_mutex);
        if (tmHandle == 0) {
            tmHandle = getDefaultTransactionManagerUnlocked();
        } else if (!m_tms.contains(tmHandle)) {
            return NtStatus::InvalidHandle;
        }

        Handle hTx = allocateHandleUnlocked();
        auto tx = std::make_unique<KTRANSACTION>();
        tx->TransactionHandle = hTx;
        tx->TmHandle          = tmHandle;
        tx->Uow               = uow ? *uow : generateGuidUnlocked(0x54580000u);
        tx->State             = TransactionStateNormal;
        tx->Outcome           = TransactionOutcomeUndetermined;
        tx->CreateOptions     = createOptions;
        tx->IsolationLevel    = isolationLevel;
        tx->IsolationFlags    = isolationFlags;
        if (timeout) tx->Timeout = *timeout;
        tx->Description       = description ? description : L"MicaNT Kernel Transaction";

        // Append BeginTransaction record to TM's CLFS stream
        auto* tm = m_tms[tmHandle].get();
        auto logIt = m_clfsLogs.find(tm->ClfsLogHandle);
        if (logIt != m_clfsLogs.end()) {
            appendClfsRecordUnlocked(
                logIt->second.get(),
                &tx->Uow,
                sizeof(KTM_GUID),
                CLFS_LSN_NULL,
                CLFS_LSN_NULL,
                ClfsDataRecord,
                KtmLogRecordBeginTransaction,
                tx->Uow,
                ++tm->VirtualClock
            );
        }

        m_txs[hTx] = std::move(tx);
        *outTransactionHandle = hTx;
        m_telemetry.TotalTransactionsCreated++;
        return NtStatus::Success;
    }

    NtStatus queryInformationTransaction(
        Handle                        transactionHandle,
        TRANSACTION_INFORMATION_CLASS infoClass,
        PVOID                         infoBuffer,
        uint32_t                      infoLength,
        uint32_t*                     outReturnLength
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_txs.find(transactionHandle);
        if (it == m_txs.end()) return NtStatus::InvalidHandle;
        const KTRANSACTION* tx = it->second.get();

        if (infoClass == TransactionBasicInformation) {
            if (outReturnLength) *outReturnLength = sizeof(TRANSACTION_BASIC_INFORMATION);
            if (!infoBuffer || infoLength < sizeof(TRANSACTION_BASIC_INFORMATION)) {
                return NtStatus::BufferTooSmall;
            }
            auto* basic = static_cast<TRANSACTION_BASIC_INFORMATION*>(infoBuffer);
            basic->TransactionId = tx->Uow;
            basic->State         = tx->State;
            basic->Outcome       = tx->Outcome;
            return NtStatus::Success;
        } else if (infoClass == TransactionPropertiesInformation) {
            if (outReturnLength) *outReturnLength = sizeof(TRANSACTION_PROPERTIES_INFORMATION);
            if (!infoBuffer || infoLength < sizeof(TRANSACTION_PROPERTIES_INFORMATION)) {
                return NtStatus::BufferTooSmall;
            }
            auto* props = static_cast<TRANSACTION_PROPERTIES_INFORMATION*>(infoBuffer);
            std::memset(props, 0, sizeof(TRANSACTION_PROPERTIES_INFORMATION));
            props->IsolationLevel = tx->IsolationLevel;
            props->IsolationFlags = tx->IsolationFlags;
            props->Timeout        = tx->Timeout;
            props->Outcome        = tx->Outcome;
            size_t chars = std::min<size_t>(tx->Description.size(), 63);
            props->DescriptionLength = static_cast<uint32_t>(chars * sizeof(wchar_t));
            std::memcpy(props->Description, tx->Description.data(), props->DescriptionLength);
            return NtStatus::Success;
        }
        return NtStatus::InvalidInfoClass;
    }

    NtStatus createEnlistment(
        Handle*           outEnlistmentHandle,
        uint32_t          desiredAccess,
        Handle            rmHandle,
        Handle            transactionHandle,
        ObjectAttributes* objectAttributes,
        uint32_t          createOptions,
        uint32_t          notificationMask,
        PVOID             enlistmentKey
    ) {
        (void)desiredAccess;
        (void)objectAttributes;
        (void)createOptions;
        if (!outEnlistmentHandle) return NtStatus::InvalidParameter;

        std::lock_guard<std::mutex> lock(m_mutex);
        auto rmIt = m_rms.find(rmHandle);
        auto txIt = m_txs.find(transactionHandle);
        if (rmIt == m_rms.end() || txIt == m_txs.end()) return NtStatus::InvalidHandle;

        KTRANSACTION* tx = txIt->second.get();
        if (tx->Outcome == TransactionOutcomeAborted) return NtStatus::TransactionAlreadyAborted;
        if (tx->Outcome == TransactionOutcomeCommitted) return NtStatus::TransactionAlreadyCommitted;

        Handle hEnl = allocateHandleUnlocked();
        auto enl = std::make_unique<KENLISTMENT>();
        enl->EnlistmentHandle  = hEnl;
        enl->RmHandle          = rmHandle;
        enl->TransactionHandle = transactionHandle;
        enl->EnlistmentId      = generateGuidUnlocked(0x454E0000u);
        enl->NotificationMask  = notificationMask ? notificationMask :
            (TRANSACTION_NOTIFY_PREPREPARE | TRANSACTION_NOTIFY_PREPARE | TRANSACTION_NOTIFY_COMMIT | TRANSACTION_NOTIFY_ROLLBACK);
        enl->EnlistmentKey     = enlistmentKey;
        enl->State             = EnlistmentStateActive;

        tx->EnlistmentHandles.push_back(hEnl);
        m_enlistments[hEnl] = std::move(enl);
        *outEnlistmentHandle = hEnl;
        m_telemetry.TotalEnlistmentsCreated++;
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // 5.5 Two-Phase Commit (2PC) & Single-Phase Commit (1PC) Execution
    // ------------------------------------------------------------------------

    NtStatus commitTransaction(Handle transactionHandle, BOOLEAN wait) {
        (void)wait;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto txIt = m_txs.find(transactionHandle);
        if (txIt == m_txs.end()) return NtStatus::InvalidHandle;

        KTRANSACTION* tx = txIt->second.get();
        if (tx->Outcome == TransactionOutcomeAborted) return NtStatus::TransactionAlreadyAborted;
        if (tx->Outcome == TransactionOutcomeCommitted) return NtStatus::TransactionAlreadyCommitted;

        // Collect active (non-read-only) enlistments
        std::vector<KENLISTMENT*> activeEnl;
        for (Handle hEnl : tx->EnlistmentHandles) {
            auto eit = m_enlistments.find(hEnl);
            if (eit != m_enlistments.end() && eit->second->State != EnlistmentStateReadOnly) {
                activeEnl.push_back(eit->second.get());
            }
        }

        // Case A: Zero active enlistments -> Commit immediately
        if (activeEnl.empty()) {
            finalizeCommitUnlocked(tx);
            return NtStatus::Success;
        }

        // Case B: Single-Phase Commit Optimization (1PC) if exactly 1 active enlistment supports it
        if (activeEnl.size() == 1 &&
            (activeEnl[0]->NotificationMask & TRANSACTION_NOTIFY_SINGLE_PHASE_COMMIT) != 0 &&
            !activeEnl[0]->SinglePhaseRejected) {

            KENLISTMENT* singleEnl = activeEnl[0];
            singleEnl->State = EnlistmentStateSinglePhaseCommit;
            deliverNotificationToEnlistmentUnlocked(tx, singleEnl, TRANSACTION_NOTIFY_SINGLE_PHASE_COMMIT);

            // Check if the RM completed single-phase commit synchronously in its callback:
            if (tx->Outcome == TransactionOutcomeAborted) {
                return NtStatus::TransactionAlreadyAborted;
            }
            if (singleEnl->State == EnlistmentStateCommitted) {
                m_telemetry.TotalSinglePhaseCommits++;
                finalizeCommitUnlocked(tx);
                return NtStatus::Success;
            }
            // If the RM did not reject 1PC yet (async RM queueing), wait for NtCommitComplete or NtSinglePhaseReject
            if (!singleEnl->SinglePhaseRejected) {
                return NtStatus::Success;
            }
        }

        // Case C: Full Two-Phase Commit (2PC: PrePrepare -> Prepare -> Commit)
        return runTwoPhaseCommitProgressionUnlocked(tx);
    }

    NtStatus rollbackTransaction(Handle transactionHandle, BOOLEAN wait) {
        (void)wait;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto txIt = m_txs.find(transactionHandle);
        if (txIt == m_txs.end()) return NtStatus::InvalidHandle;

        KTRANSACTION* tx = txIt->second.get();
        if (tx->Outcome == TransactionOutcomeCommitted) {
            return NtStatus::TransactionAlreadyCommitted;
        }
        if (tx->Outcome == TransactionOutcomeAborted) {
            return NtStatus::Success;
        }

        abortAndBroadcastRollbackUnlocked(tx);
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // 5.6 Resource Manager Enlistment Responses (PrePrepare, Prepare, Commit,
    //     Rollback, SinglePhaseReject, ReadOnly)
    // ------------------------------------------------------------------------

    NtStatus prePrepareComplete(Handle enlistmentHandle, PLARGE_INTEGER tmVirtualClock) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto eit = m_enlistments.find(enlistmentHandle);
        if (eit == m_enlistments.end()) return NtStatus::InvalidHandle;

        KENLISTMENT* enl = eit->second.get();
        auto txIt = m_txs.find(enl->TransactionHandle);
        if (txIt == m_txs.end()) return NtStatus::InvalidHandle;
        KTRANSACTION* tx = txIt->second.get();
        updateTmClockUnlocked(tx->TmHandle, tmVirtualClock);

        enl->State = EnlistmentStatePrePrepared;
        return advanceTwoPhaseCommitStateUnlocked(tx);
    }

    NtStatus prepareComplete(Handle enlistmentHandle, PLARGE_INTEGER tmVirtualClock) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto eit = m_enlistments.find(enlistmentHandle);
        if (eit == m_enlistments.end()) return NtStatus::InvalidHandle;

        KENLISTMENT* enl = eit->second.get();
        auto txIt = m_txs.find(enl->TransactionHandle);
        if (txIt == m_txs.end()) return NtStatus::InvalidHandle;
        KTRANSACTION* tx = txIt->second.get();
        updateTmClockUnlocked(tx->TmHandle, tmVirtualClock);

        enl->State = EnlistmentStatePrepared;
        return advanceTwoPhaseCommitStateUnlocked(tx);
    }

    NtStatus commitComplete(Handle enlistmentHandle, PLARGE_INTEGER tmVirtualClock) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto eit = m_enlistments.find(enlistmentHandle);
        if (eit == m_enlistments.end()) return NtStatus::InvalidHandle;

        KENLISTMENT* enl = eit->second.get();
        auto txIt = m_txs.find(enl->TransactionHandle);
        if (txIt == m_txs.end()) return NtStatus::InvalidHandle;
        KTRANSACTION* tx = txIt->second.get();
        updateTmClockUnlocked(tx->TmHandle, tmVirtualClock);

        bool wasSinglePhase = (enl->State == EnlistmentStateSinglePhaseCommit);
        enl->State = EnlistmentStateCommitted;

        if (wasSinglePhase && tx->Outcome == TransactionOutcomeUndetermined) {
            m_telemetry.TotalSinglePhaseCommits++;
            finalizeCommitUnlocked(tx);
        }
        return NtStatus::Success;
    }

    NtStatus rollbackComplete(Handle enlistmentHandle, PLARGE_INTEGER tmVirtualClock) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto eit = m_enlistments.find(enlistmentHandle);
        if (eit == m_enlistments.end()) return NtStatus::InvalidHandle;

        KENLISTMENT* enl = eit->second.get();
        auto txIt = m_txs.find(enl->TransactionHandle);
        if (txIt != m_txs.end()) {
            updateTmClockUnlocked(txIt->second->TmHandle, tmVirtualClock);
        }
        enl->State = EnlistmentStateAborted;
        return NtStatus::Success;
    }

    NtStatus rollbackEnlistment(Handle enlistmentHandle, PLARGE_INTEGER tmVirtualClock) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto eit = m_enlistments.find(enlistmentHandle);
        if (eit == m_enlistments.end()) return NtStatus::InvalidHandle;

        KENLISTMENT* enl = eit->second.get();
        auto txIt = m_txs.find(enl->TransactionHandle);
        if (txIt == m_txs.end()) return NtStatus::InvalidHandle;

        KTRANSACTION* tx = txIt->second.get();
        if (tx->Outcome == TransactionOutcomeCommitted) {
            return NtStatus::TransactionAlreadyCommitted;
        }
        updateTmClockUnlocked(tx->TmHandle, tmVirtualClock);
        enl->State = EnlistmentStateAborted;
        abortAndBroadcastRollbackUnlocked(tx);
        return NtStatus::Success;
    }

    NtStatus singlePhaseReject(Handle enlistmentHandle, PLARGE_INTEGER tmVirtualClock) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto eit = m_enlistments.find(enlistmentHandle);
        if (eit == m_enlistments.end()) return NtStatus::InvalidHandle;

        KENLISTMENT* enl = eit->second.get();
        auto txIt = m_txs.find(enl->TransactionHandle);
        if (txIt == m_txs.end()) return NtStatus::InvalidHandle;

        KTRANSACTION* tx = txIt->second.get();
        updateTmClockUnlocked(tx->TmHandle, tmVirtualClock);

        enl->SinglePhaseRejected = true;
        enl->State               = EnlistmentStateActive;
        m_telemetry.TotalSinglePhaseRejections++;

        // Seamlessly fall back to full Two-Phase Commit (PrePrepare -> Prepare -> Commit)!
        return runTwoPhaseCommitProgressionUnlocked(tx);
    }

    NtStatus readOnlyEnlistment(Handle enlistmentHandle, PLARGE_INTEGER tmVirtualClock) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto eit = m_enlistments.find(enlistmentHandle);
        if (eit == m_enlistments.end()) return NtStatus::InvalidHandle;

        KENLISTMENT* enl = eit->second.get();
        auto txIt = m_txs.find(enl->TransactionHandle);
        if (txIt == m_txs.end()) return NtStatus::InvalidHandle;

        KTRANSACTION* tx = txIt->second.get();
        updateTmClockUnlocked(tx->TmHandle, tmVirtualClock);

        enl->State = EnlistmentStateReadOnly;
        m_telemetry.TotalReadOnlyEnlistments++;
        return advanceTwoPhaseCommitStateUnlocked(tx);
    }

    KTM_TELEMETRY getTelemetry() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_telemetry;
    }

private:
    TitanKtmManager() = default;

    Handle allocateHandleUnlocked() noexcept {
        Handle h = m_nextHandle;
        m_nextHandle += 4;
        return h;
    }

    KTM_GUID generateGuidUnlocked(uint32_t prefix) noexcept {
        uint64_t seq = m_guidSeq++;
        KTM_GUID g{};
        g.Data1 = prefix | static_cast<uint32_t>(seq & 0xFFFFu);
        g.Data2 = 0x2026;
        g.Data3 = 0x4500;
        for (int i = 0; i < 8; ++i) {
            g.Data4[i] = static_cast<uint8_t>((seq >> (i * 8)) & 0xFFu);
        }
        return g;
    }

    CLFS_LSN appendClfsRecordUnlocked(
        CLFS_LOG_FILE_OBJECT* logObj,
        const void*           buffer,
        uint32_t              bufferLength,
        CLFS_LSN              undoNextLsn,
        CLFS_LSN              previousLsn,
        CLFS_RECORD_TYPE      recordType,
        KTM_LOG_RECORD_KIND   ktmKind,
        const KTM_GUID&       txUow,
        int64_t               virtualClock
    ) {
        CLFS_LSN lsn = CLFS_LSN::make(
            logObj->CurrentContainerId,
            logObj->CurrentBlockOffset++,
            logObj->CurrentRecordSeq++
        );
        CLFS_LOG_RECORD rec{};
        rec.Lsn          = lsn;
        rec.UndoNextLsn  = undoNextLsn;
        rec.PreviousLsn  = previousLsn;
        rec.RecordType   = recordType;
        rec.KtmKind      = ktmKind;
        rec.TransactionUow = txUow;
        rec.VirtualClock = virtualClock;
        if (buffer && bufferLength > 0) {
            rec.Payload.assign(
                static_cast<const uint8_t*>(buffer),
                static_cast<const uint8_t*>(buffer) + bufferLength
            );
        }
        logObj->Records.push_back(std::move(rec));
        m_telemetry.TotalClfsRecordsAppended++;
        return lsn;
    }

    void updateTmClockUnlocked(Handle tmHandle, const LARGE_INTEGER* clock) noexcept {
        if (!clock) return;
        auto it = m_tms.find(tmHandle);
        if (it != m_tms.end() && clock->quadPart > it->second->VirtualClock) {
            it->second->VirtualClock = clock->quadPart;
        }
    }

    void deliverNotificationToEnlistmentUnlocked(
        KTRANSACTION* tx,
        KENLISTMENT*  enl,
        uint32_t      notificationFlag
    ) {
        auto rmIt = m_rms.find(enl->RmHandle);
        if (rmIt == m_rms.end()) return;
        KRESOURCE_MANAGER* rm = rmIt->second.get();

        int64_t vclock = 1000;
        auto tmIt = m_tms.find(tx->TmHandle);
        if (tmIt != m_tms.end()) {
            vclock = ++tmIt->second->VirtualClock;
        }

        TRANSACTION_NOTIFICATION notif{};
        notif.TransactionKey          = enl->EnlistmentKey;
        notif.TransactionNotification = notificationFlag;
        notif.TmVirtualClock.quadPart = vclock;
        notif.ArgumentLength          = 0;

        rm->NotificationQueue.push_back(notif);
        m_telemetry.TotalRmNotificationsDelivered++;

        if (rm->CallbacksEnabled && rm->CallbackRoutine) {
            LARGE_INTEGER vc{};
            vc.quadPart = vclock;
            NtStatus cbStatus = rm->CallbackRoutine(
                enl->EnlistmentHandle,
                rm->RMContext,
                enl->EnlistmentKey,
                notificationFlag,
                &vc,
                0,
                nullptr
            );
            // Allow kernel-mode RM callback to auto-advance state or veto:
            if (cbStatus == NtStatus::TransactionAlreadyAborted || cbStatus == NtStatus::Unsuccessful) {
                enl->State = EnlistmentStateAborted;
                abortAndBroadcastRollbackUnlocked(tx);
            } else if (cbStatus == NtStatus::NotSupported && notificationFlag == TRANSACTION_NOTIFY_SINGLE_PHASE_COMMIT) {
                enl->SinglePhaseRejected = true;
                enl->State               = EnlistmentStateActive;
                m_telemetry.TotalSinglePhaseRejections++;
            } else if (NT_SUCCESS(cbStatus)) {
                if (notificationFlag == TRANSACTION_NOTIFY_SINGLE_PHASE_COMMIT && !enl->SinglePhaseRejected) {
                    enl->State = EnlistmentStateCommitted;
                } else if (notificationFlag == TRANSACTION_NOTIFY_PREPREPARE && enl->State == EnlistmentStatePrePreparing) {
                    enl->State = EnlistmentStatePrePrepared;
                } else if (notificationFlag == TRANSACTION_NOTIFY_PREPARE && enl->State == EnlistmentStatePreparing) {
                    enl->State = EnlistmentStatePrepared;
                } else if (notificationFlag == TRANSACTION_NOTIFY_COMMIT && enl->State == EnlistmentStateCommitting) {
                    enl->State = EnlistmentStateCommitted;
                } else if (notificationFlag == TRANSACTION_NOTIFY_ROLLBACK) {
                    enl->State = EnlistmentStateAborted;
                }
            }
        }
    }

    void broadcastNotificationUnlocked(KTRANSACTION* tx, uint32_t notificationFlag) {
        for (Handle hEnl : tx->EnlistmentHandles) {
            auto eit = m_enlistments.find(hEnl);
            if (eit == m_enlistments.end()) continue;
            KENLISTMENT* enl = eit->second.get();
            if (enl->State == EnlistmentStateReadOnly) continue;
            if ((enl->NotificationMask & notificationFlag) != 0) {
                deliverNotificationToEnlistmentUnlocked(tx, enl, notificationFlag);
            }
        }
    }

    void abortAndBroadcastRollbackUnlocked(KTRANSACTION* tx) {
        if (tx->Outcome == TransactionOutcomeAborted) return;
        tx->Outcome = TransactionOutcomeAborted;
        tx->State   = TransactionStateNormal;

        auto tmIt = m_tms.find(tx->TmHandle);
        if (tmIt != m_tms.end()) {
            auto logIt = m_clfsLogs.find(tmIt->second->ClfsLogHandle);
            if (logIt != m_clfsLogs.end()) {
                appendClfsRecordUnlocked(
                    logIt->second.get(),
                    &tx->Uow,
                    sizeof(KTM_GUID),
                    CLFS_LSN_NULL,
                    tx->PrepareLsn,
                    ClfsDataRecord,
                    KtmLogRecordAbort,
                    tx->Uow,
                    ++tmIt->second->VirtualClock
                );
            }
        }

        for (Handle hEnl : tx->EnlistmentHandles) {
            auto eit = m_enlistments.find(hEnl);
            if (eit == m_enlistments.end()) continue;
            KENLISTMENT* enl = eit->second.get();
            if (enl->State == EnlistmentStateReadOnly || enl->State == EnlistmentStateAborted) continue;
            enl->State = EnlistmentStateAborted;
            if ((enl->NotificationMask & TRANSACTION_NOTIFY_ROLLBACK) != 0) {
                deliverNotificationToEnlistmentUnlocked(tx, enl, TRANSACTION_NOTIFY_ROLLBACK);
            }
        }
        m_telemetry.TotalTransactionsAborted++;
    }

    void finalizeCommitUnlocked(KTRANSACTION* tx) {
        if (tx->Outcome == TransactionOutcomeCommitted) return;
        tx->Outcome = TransactionOutcomeCommitted;
        tx->State   = TransactionStateCommittedNotify;

        auto tmIt = m_tms.find(tx->TmHandle);
        if (tmIt != m_tms.end()) {
            auto logIt = m_clfsLogs.find(tmIt->second->ClfsLogHandle);
            if (logIt != m_clfsLogs.end()) {
                tx->CommitLsn = appendClfsRecordUnlocked(
                    logIt->second.get(),
                    &tx->Uow,
                    sizeof(KTM_GUID),
                    CLFS_LSN_NULL,
                    tx->PrepareLsn,
                    ClfsDataRecord,
                    KtmLogRecordCommit,
                    tx->Uow,
                    ++tmIt->second->VirtualClock
                );
                tmIt->second->LastCommitLsn = tx->CommitLsn;
            }
        }

        for (Handle hEnl : tx->EnlistmentHandles) {
            auto eit = m_enlistments.find(hEnl);
            if (eit == m_enlistments.end()) continue;
            KENLISTMENT* enl = eit->second.get();
            if (enl->State == EnlistmentStateReadOnly || enl->State == EnlistmentStateCommitted) continue;
            enl->State = EnlistmentStateCommitting;
            if ((enl->NotificationMask & TRANSACTION_NOTIFY_COMMIT) != 0) {
                deliverNotificationToEnlistmentUnlocked(tx, enl, TRANSACTION_NOTIFY_COMMIT);
            } else {
                enl->State = EnlistmentStateCommitted;
            }
        }
        m_telemetry.TotalTransactionsCommitted++;
    }

    NtStatus runTwoPhaseCommitProgressionUnlocked(KTRANSACTION* tx) {
        // Phase 1a: Send PrePrepare to all active enlistments
        for (Handle hEnl : tx->EnlistmentHandles) {
            auto eit = m_enlistments.find(hEnl);
            if (eit == m_enlistments.end()) continue;
            KENLISTMENT* enl = eit->second.get();
            if (enl->State != EnlistmentStateActive) continue;

            if ((enl->NotificationMask & TRANSACTION_NOTIFY_PREPREPARE) != 0) {
                enl->State = EnlistmentStatePrePreparing;
                deliverNotificationToEnlistmentUnlocked(tx, enl, TRANSACTION_NOTIFY_PREPREPARE);
                if (tx->Outcome == TransactionOutcomeAborted) {
                    return NtStatus::TransactionAlreadyAborted;
                }
            } else {
                enl->State = EnlistmentStatePrePrepared;
            }
        }

        return advanceTwoPhaseCommitStateUnlocked(tx);
    }

    NtStatus advanceTwoPhaseCommitStateUnlocked(KTRANSACTION* tx) {
        if (tx->Outcome == TransactionOutcomeAborted) return NtStatus::TransactionAlreadyAborted;
        if (tx->Outcome == TransactionOutcomeCommitted) return NtStatus::Success;

        // Check if any enlistment is still PrePreparing
        bool anyPrePreparing = false;
        for (Handle hEnl : tx->EnlistmentHandles) {
            auto eit = m_enlistments.find(hEnl);
            if (eit != m_enlistments.end() && eit->second->State == EnlistmentStatePrePreparing) {
                anyPrePreparing = true;
                break;
            }
        }
        if (anyPrePreparing) return NtStatus::Success;

        // Transition all PrePrepared enlistments into Prepare phase
        for (Handle hEnl : tx->EnlistmentHandles) {
            auto eit = m_enlistments.find(hEnl);
            if (eit == m_enlistments.end()) continue;
            KENLISTMENT* enl = eit->second.get();
            if (enl->State == EnlistmentStatePrePrepared) {
                if ((enl->NotificationMask & TRANSACTION_NOTIFY_PREPARE) != 0) {
                    enl->State = EnlistmentStatePreparing;
                    deliverNotificationToEnlistmentUnlocked(tx, enl, TRANSACTION_NOTIFY_PREPARE);
                    if (tx->Outcome == TransactionOutcomeAborted) {
                        return NtStatus::TransactionAlreadyAborted;
                    }
                } else {
                    enl->State = EnlistmentStatePrepared;
                }
            }
        }

        // Check if all non-ReadOnly enlistments have reached Prepared
        bool allPrepared = true;
        for (Handle hEnl : tx->EnlistmentHandles) {
            auto eit = m_enlistments.find(hEnl);
            if (eit == m_enlistments.end()) continue;
            KENLISTMENT_STATE st = eit->second->State;
            if (st != EnlistmentStatePrepared && st != EnlistmentStateReadOnly) {
                allPrepared = false;
                break;
            }
        }

        if (allPrepared) {
            // Write durable Prepare record to CLFS, then execute Phase 2 Commit!
            auto tmIt = m_tms.find(tx->TmHandle);
            if (tmIt != m_tms.end()) {
                auto logIt = m_clfsLogs.find(tmIt->second->ClfsLogHandle);
                if (logIt != m_clfsLogs.end()) {
                    tx->PrepareLsn = appendClfsRecordUnlocked(
                        logIt->second.get(),
                        &tx->Uow,
                        sizeof(KTM_GUID),
                        CLFS_LSN_NULL,
                        CLFS_LSN_NULL,
                        ClfsDataRecord,
                        KtmLogRecordPrepare,
                        tx->Uow,
                        ++tmIt->second->VirtualClock
                    );
                }
            }
            m_telemetry.TotalTwoPhaseCommits++;
            finalizeCommitUnlocked(tx);
        }

        return NtStatus::Success;
    }

    mutable std::mutex                                                  m_mutex{};
    std::unordered_map<Handle, std::unique_ptr<CLFS_LOG_FILE_OBJECT>>   m_clfsLogs{};
    std::unordered_map<Handle, std::unique_ptr<KTRANSACTION_MANAGER>>   m_tms{};
    std::unordered_map<Handle, std::unique_ptr<KRESOURCE_MANAGER>>      m_rms{};
    std::unordered_map<Handle, std::unique_ptr<KTRANSACTION>>           m_txs{};
    std::unordered_map<Handle, std::unique_ptr<KENLISTMENT>>            m_enlistments{};
    Handle                                                              m_nextHandle{0x5400};
    uint64_t                                                            m_guidSeq{1};
    KTM_TELEMETRY                                                       m_telemetry{};
};

// ============================================================================
// 6. Exported NT Kernel (Nt* / Tm*), CLFS (Clfs*) & Win32 (ktmw32.dll) APIs
// ============================================================================

// 6.1 Nt* System Services (ntoskrnl.exe & ntdll.dll)
inline NtStatus NtCreateTransactionManager(
    Handle*           TmHandle,
    uint32_t          DesiredAccess,
    ObjectAttributes* ObjectAttributes,
    const wchar_t*    LogFileName,
    uint32_t          CreateOptions,
    uint32_t          CommitStrength
) {
    return TitanKtmManager::Instance().createTransactionManager(
        TmHandle, DesiredAccess, ObjectAttributes, LogFileName, CreateOptions, CommitStrength
    );
}

inline NtStatus NtRecoverTransactionManager(Handle TmHandle) {
    return TitanKtmManager::Instance().recoverTransactionManager(TmHandle);
}

inline NtStatus NtRollforwardTransactionManager(Handle TmHandle, PLARGE_INTEGER TmVirtualClock) {
    return TitanKtmManager::Instance().rollforwardTransactionManager(TmHandle, TmVirtualClock);
}

inline NtStatus NtCreateResourceManager(
    Handle*           ResourceManagerHandle,
    uint32_t          DesiredAccess,
    Handle            TmHandle,
    const KTM_GUID*   ResourceManagerGuid,
    ObjectAttributes* ObjectAttributes,
    uint32_t          CreateOptions,
    const wchar_t*    Description
) {
    return TitanKtmManager::Instance().createResourceManager(
        ResourceManagerHandle, DesiredAccess, ObjectAttributes, TmHandle, ResourceManagerGuid, CreateOptions, Description
    );
}

inline NtStatus NtGetNotificationResourceManager(
    Handle                    ResourceManagerHandle,
    TRANSACTION_NOTIFICATION* TransactionNotification,
    uint32_t                  NotificationLength,
    PLARGE_INTEGER            Timeout,
    uint32_t*                 ReturnLength,
    uint32_t                  Asynchronous,
     uintptr_t                AsynchronousContext
) {
    (void)Asynchronous;
    (void)AsynchronousContext;
    return TitanKtmManager::Instance().getNotificationResourceManager(
        ResourceManagerHandle, TransactionNotification, NotificationLength, Timeout, ReturnLength
    );
}

inline NtStatus NtRecoverResourceManager(Handle ResourceManagerHandle) {
    return TitanKtmManager::Instance().recoverResourceManager(ResourceManagerHandle);
}

inline NtStatus NtCreateTransaction(
    Handle*           TransactionHandle,
    uint32_t          DesiredAccess,
    ObjectAttributes* ObjectAttributes,
    const KTM_GUID*   Uow,
    Handle            TmHandle,
    uint32_t          CreateOptions,
    uint32_t          IsolationLevel,
    uint32_t          IsolationFlags,
    PLARGE_INTEGER    Timeout,
    const wchar_t*    Description
) {
    return TitanKtmManager::Instance().createTransaction(
        TransactionHandle, DesiredAccess, ObjectAttributes, Uow, TmHandle,
        CreateOptions, IsolationLevel, IsolationFlags, Timeout, Description
    );
}

inline NtStatus NtQueryInformationTransaction(
    Handle                        TransactionHandle,
    TRANSACTION_INFORMATION_CLASS TransactionInformationClass,
    PVOID                         TransactionInformation,
    uint32_t                      TransactionInformationLength,
    uint32_t*                     ReturnLength
) {
    return TitanKtmManager::Instance().queryInformationTransaction(
        TransactionHandle, TransactionInformationClass, TransactionInformation, TransactionInformationLength, ReturnLength
    );
}

inline NtStatus NtCommitTransaction(Handle TransactionHandle, BOOLEAN Wait) {
    return TitanKtmManager::Instance().commitTransaction(TransactionHandle, Wait);
}

inline NtStatus NtRollbackTransaction(Handle TransactionHandle, BOOLEAN Wait) {
    return TitanKtmManager::Instance().rollbackTransaction(TransactionHandle, Wait);
}

inline NtStatus NtCreateEnlistment(
    Handle*           EnlistmentHandle,
    uint32_t          DesiredAccess,
    Handle            ResourceManagerHandle,
    Handle            TransactionHandle,
    ObjectAttributes* ObjectAttributes,
    uint32_t          CreateOptions,
    uint32_t          NotificationMask,
    PVOID             EnlistmentKey
) {
    return TitanKtmManager::Instance().createEnlistment(
        EnlistmentHandle, DesiredAccess, ResourceManagerHandle, TransactionHandle,
        ObjectAttributes, CreateOptions, NotificationMask, EnlistmentKey
    );
}

inline NtStatus NtPrePrepareComplete(Handle EnlistmentHandle, PLARGE_INTEGER TmVirtualClock) {
    return TitanKtmManager::Instance().prePrepareComplete(EnlistmentHandle, TmVirtualClock);
}

inline NtStatus NtPrepareComplete(Handle EnlistmentHandle, PLARGE_INTEGER TmVirtualClock) {
    return TitanKtmManager::Instance().prepareComplete(EnlistmentHandle, TmVirtualClock);
}

inline NtStatus NtCommitComplete(Handle EnlistmentHandle, PLARGE_INTEGER TmVirtualClock) {
    return TitanKtmManager::Instance().commitComplete(EnlistmentHandle, TmVirtualClock);
}

inline NtStatus NtRollbackComplete(Handle EnlistmentHandle, PLARGE_INTEGER TmVirtualClock) {
    return TitanKtmManager::Instance().rollbackComplete(EnlistmentHandle, TmVirtualClock);
}

inline NtStatus NtRollbackEnlistment(Handle EnlistmentHandle, PLARGE_INTEGER TmVirtualClock) {
    return TitanKtmManager::Instance().rollbackEnlistment(EnlistmentHandle, TmVirtualClock);
}

inline NtStatus NtSinglePhaseReject(Handle EnlistmentHandle, PLARGE_INTEGER TmVirtualClock) {
    return TitanKtmManager::Instance().singlePhaseReject(EnlistmentHandle, TmVirtualClock);
}

inline NtStatus NtReadOnlyEnlistment(Handle EnlistmentHandle, PLARGE_INTEGER TmVirtualClock) {
    return TitanKtmManager::Instance().readOnlyEnlistment(EnlistmentHandle, TmVirtualClock);
}

// 6.2 Kernel-Mode Tm* Executive Wrappers (ntoskrnl.exe)
inline NtStatus TmEnableCallbacks(
    Handle              ResourceManager,
    PTM_RM_NOTIFICATION CallbackRoutine,
    PVOID               RMKey
) {
    return TitanKtmManager::Instance().enableRmCallbacks(ResourceManager, CallbackRoutine, RMKey);
}

inline NtStatus TmCreateEnlistment(
    Handle*           EnlistmentHandle,
    Handle            ResourceManager,
    Handle            Transaction,
    ObjectAttributes* ObjectAttributes,
    uint32_t          DesiredAccess,
    uint32_t          CreateOptions,
    uint32_t          NotificationMask,
    PVOID             EnlistmentKey
) {
    return NtCreateEnlistment(
        EnlistmentHandle, DesiredAccess, ResourceManager, Transaction,
        ObjectAttributes, CreateOptions, NotificationMask, EnlistmentKey
    );
}

inline NtStatus TmPrePrepareComplete(Handle Enlistment, PLARGE_INTEGER TmVirtualClock) {
    return NtPrePrepareComplete(Enlistment, TmVirtualClock);
}

inline NtStatus TmPrepareComplete(Handle Enlistment, PLARGE_INTEGER TmVirtualClock) {
    return NtPrepareComplete(Enlistment, TmVirtualClock);
}

inline NtStatus TmCommitComplete(Handle Enlistment, PLARGE_INTEGER TmVirtualClock) {
    return NtCommitComplete(Enlistment, TmVirtualClock);
}

inline NtStatus TmRollbackComplete(Handle Enlistment, PLARGE_INTEGER TmVirtualClock) {
    return NtRollbackComplete(Enlistment, TmVirtualClock);
}

inline NtStatus TmRollbackEnlistment(Handle Enlistment, PLARGE_INTEGER TmVirtualClock) {
    return NtRollbackEnlistment(Enlistment, TmVirtualClock);
}

inline NtStatus TmSinglePhaseReject(Handle Enlistment, PLARGE_INTEGER TmVirtualClock) {
    return NtSinglePhaseReject(Enlistment, TmVirtualClock);
}

inline NtStatus TmReadOnlyEnlistment(Handle Enlistment, PLARGE_INTEGER TmVirtualClock) {
    return NtReadOnlyEnlistment(Enlistment, TmVirtualClock);
}

// 6.3 Common Log File System (CLFS — clfsw32.sys) Exports
inline NtStatus ClfsCreateLogFile(
    Handle*        pplfoLog,
    const wchar_t* puszLogFileName,
    uint32_t       fDesiredAccess,
    uint32_t       dwShareMode,
    uint32_t       fCreateDisposition
) {
    return TitanKtmManager::Instance().clfsCreateLogFile(
        pplfoLog, puszLogFileName, fDesiredAccess, dwShareMode, fCreateDisposition
    );
}

inline NtStatus ClfsCloseLogFileObject(Handle plfoLog) {
    return TitanKtmManager::Instance().clfsCloseLogFileObject(plfoLog);
}

inline NtStatus ClfsWriteRestartArea(
    Handle      pvMarshalContext,
    const void* pvRestartBuffer,
    uint32_t    cbRestartBuffer,
    CLFS_LSN*   plsnBase
) {
    return TitanKtmManager::Instance().clfsWriteRestartArea(
        pvMarshalContext, pvRestartBuffer, cbRestartBuffer, plsnBase
    );
}

inline NtStatus ClfsReadRestartArea(
    Handle    pvMarshalContext,
    void*     pvRestartBuffer,
    uint32_t  cbRestartBufferCapacity,
    uint32_t* pcbRestartBufferRead,
    CLFS_LSN* plsnRestart
) {
    return TitanKtmManager::Instance().clfsReadRestartArea(
        pvMarshalContext, pvRestartBuffer, cbRestartBufferCapacity, pcbRestartBufferRead, plsnRestart
    );
}

inline NtStatus ClfsFlushToLsn(
    Handle          pvMarshalContext,
    const CLFS_LSN* plsnFlush,
    CLFS_LSN*       plsnLastFlushed
) {
    return TitanKtmManager::Instance().clfsFlushToLsn(pvMarshalContext, plsnFlush, plsnLastFlushed);
}

// 6.4 Win32 ktmw32.dll User-Mode Bridge
inline Handle CreateTransactionWin32(
    PVOID          lpTransactionAttributes,
    KTM_GUID*      UOW,
    uint32_t       CreateOptions,
    uint32_t       IsolationLevel,
    uint32_t       IsolationFlags,
    uint32_t       Timeout,
    const wchar_t* Description
) {
    (void)lpTransactionAttributes;
    Handle hTx = 0;
    LARGE_INTEGER to{};
    to.quadPart = static_cast<int64_t>(Timeout) * -10000LL;
    NtStatus st = NtCreateTransaction(
        &hTx, TRANSACTION_ALL_ACCESS, nullptr, UOW, 0,
        CreateOptions, IsolationLevel, IsolationFlags, Timeout ? &to : nullptr, Description
    );
    return NT_SUCCESS(st) ? hTx : static_cast<Handle>(-1);
}

inline BOOLEAN CommitTransactionWin32(Handle TransactionHandle) {
    return NT_SUCCESS(NtCommitTransaction(TransactionHandle, TRUE_B)) ? TRUE_B : FALSE_B;
}

inline BOOLEAN RollbackTransactionWin32(Handle TransactionHandle) {
    return NT_SUCCESS(NtRollbackTransaction(TransactionHandle, TRUE_B)) ? TRUE_B : FALSE_B;
}

inline BOOLEAN GetTransactionIdWin32(Handle TransactionHandle, KTM_GUID* lpTransactionId) {
    if (!lpTransactionId) return FALSE_B;
    TRANSACTION_BASIC_INFORMATION info{};
    NtStatus st = NtQueryInformationTransaction(
        TransactionHandle, TransactionBasicInformation, &info, sizeof(info), nullptr
    );
    if (!NT_SUCCESS(st)) return FALSE_B;
    *lpTransactionId = info.TransactionId;
    return TRUE_B;
}

inline BOOLEAN KtmGetTelemetry(KTM_TELEMETRY* outTelemetry) noexcept {
    if (!outTelemetry) return FALSE_B;
    *outTelemetry = TitanKtmManager::Instance().getTelemetry();
    return TRUE_B;
}

// ============================================================================
// 7. Subsystem Initialization & Dynamic Export Registration
// ============================================================================

inline void InitializeKtmSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    struct ExportEntry {
        const char* name;
        void*       fn;
    };

    const ExportEntry kNtosExports[] = {
        { "NtCreateTransactionManager",      reinterpret_cast<void*>(&NtCreateTransactionManager) },
        { "NtRecoverTransactionManager",     reinterpret_cast<void*>(&NtRecoverTransactionManager) },
        { "NtRollforwardTransactionManager", reinterpret_cast<void*>(&NtRollforwardTransactionManager) },
        { "NtCreateResourceManager",         reinterpret_cast<void*>(&NtCreateResourceManager) },
        { "NtGetNotificationResourceManager",reinterpret_cast<void*>(&NtGetNotificationResourceManager) },
        { "NtRecoverResourceManager",        reinterpret_cast<void*>(&NtRecoverResourceManager) },
        { "NtCreateTransaction",             reinterpret_cast<void*>(&NtCreateTransaction) },
        { "NtQueryInformationTransaction",   reinterpret_cast<void*>(&NtQueryInformationTransaction) },
        { "NtCommitTransaction",             reinterpret_cast<void*>(&NtCommitTransaction) },
        { "NtRollbackTransaction",           reinterpret_cast<void*>(&NtRollbackTransaction) },
        { "NtCreateEnlistment",              reinterpret_cast<void*>(&NtCreateEnlistment) },
        { "NtPrePrepareComplete",            reinterpret_cast<void*>(&NtPrePrepareComplete) },
        { "NtPrepareComplete",               reinterpret_cast<void*>(&NtPrepareComplete) },
        { "NtCommitComplete",                reinterpret_cast<void*>(&NtCommitComplete) },
        { "NtRollbackComplete",              reinterpret_cast<void*>(&NtRollbackComplete) },
        { "NtRollbackEnlistment",            reinterpret_cast<void*>(&NtRollbackEnlistment) },
        { "NtSinglePhaseReject",             reinterpret_cast<void*>(&NtSinglePhaseReject) },
        { "NtReadOnlyEnlistment",            reinterpret_cast<void*>(&NtReadOnlyEnlistment) },
        { "TmEnableCallbacks",               reinterpret_cast<void*>(&TmEnableCallbacks) },
        { "TmCreateEnlistment",              reinterpret_cast<void*>(&TmCreateEnlistment) },
        { "TmPrePrepareComplete",            reinterpret_cast<void*>(&TmPrePrepareComplete) },
        { "TmPrepareComplete",               reinterpret_cast<void*>(&TmPrepareComplete) },
        { "TmCommitComplete",                reinterpret_cast<void*>(&TmCommitComplete) },
        { "TmRollbackComplete",              reinterpret_cast<void*>(&TmRollbackComplete) },
        { "TmRollbackEnlistment",            reinterpret_cast<void*>(&TmRollbackEnlistment) },
        { "TmSinglePhaseReject",             reinterpret_cast<void*>(&TmSinglePhaseReject) },
        { "TmReadOnlyEnlistment",            reinterpret_cast<void*>(&TmReadOnlyEnlistment) },
        { "KtmGetTelemetry",                 reinterpret_cast<void*>(&KtmGetTelemetry) }
    };

    for (const auto& exp : kNtosExports) {
        ldr.registerExport("ntoskrnl.exe", exp.name, exp.fn);
        ldr.registerExport("ntdll.dll", exp.name, exp.fn);
    }

    const ExportEntry kClfsExports[] = {
        { "ClfsCreateLogFile",      reinterpret_cast<void*>(&ClfsCreateLogFile) },
        { "ClfsCloseLogFileObject", reinterpret_cast<void*>(&ClfsCloseLogFileObject) },
        { "ClfsWriteRestartArea",   reinterpret_cast<void*>(&ClfsWriteRestartArea) },
        { "ClfsReadRestartArea",    reinterpret_cast<void*>(&ClfsReadRestartArea) },
        { "ClfsFlushToLsn",         reinterpret_cast<void*>(&ClfsFlushToLsn) },
        { "ClfsLsnEqual",           reinterpret_cast<void*>(&ClfsLsnEqual) },
        { "ClfsLsnLess",            reinterpret_cast<void*>(&ClfsLsnLess) },
        { "ClfsLsnGreater",         reinterpret_cast<void*>(&ClfsLsnGreater) }
    };

    for (const auto& exp : kClfsExports) {
        ldr.registerExport("clfsw32.sys", exp.name, exp.fn);
        ldr.registerExport("ntoskrnl.exe", exp.name, exp.fn);
    }

    const ExportEntry kKtmw32Exports[] = {
        { "CreateTransaction",   reinterpret_cast<void*>(&CreateTransactionWin32) },
        { "CommitTransaction",   reinterpret_cast<void*>(&CommitTransactionWin32) },
        { "RollbackTransaction", reinterpret_cast<void*>(&RollbackTransactionWin32) },
        { "GetTransactionId",    reinterpret_cast<void*>(&GetTransactionIdWin32) }
    };

    for (const auto& exp : kKtmw32Exports) {
        ldr.registerExport("ktmw32.dll", exp.name, exp.fn);
    }

    auto& scm = scm::ServiceControlManager::get();
    auto ktmSvc = std::make_shared<scm::ServiceRecord>();
    ktmSvc->serviceName           = L"KtmRm";
    ktmSvc->displayName           = L"Sovereign NT Kernel Transaction Manager (KTM / Tm* / CLFS)";
    ktmSvc->serviceType           = scm::SERVICE_KERNEL_DRIVER;
    ktmSvc->startType             = scm::SERVICE_BOOT_START;
    ktmSvc->errorControl          = scm::SERVICE_ERROR_CRITICAL;
    ktmSvc->binaryPath            = L"\\SystemRoot\\System32\\ntoskrnl.exe";
    ktmSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(ktmSvc);

    version::VersionDatabase::Instance().RegisterModule(
        "ktm_executive",
        "10.0.26100.4500",
        "Sovereign NT Kernel Transaction Manager (KTM / Tm* / 2PC / CLFS)"
    );
}

} // namespace micant::km::ktm

namespace micant {
    using km::ktm::KTM_GUID;
    using km::ktm::CLFS_LSN;
    using km::ktm::TRANSACTION_STATE;
    using km::ktm::TRANSACTION_OUTCOME;
    using km::ktm::TRANSACTION_INFORMATION_CLASS;
    using km::ktm::TRANSACTION_BASIC_INFORMATION;
    using km::ktm::TRANSACTION_PROPERTIES_INFORMATION;
    using km::ktm::TRANSACTION_NOTIFICATION;
    using km::ktm::KTM_TELEMETRY;
    using km::ktm::TitanKtmManager;
    using km::ktm::NtCreateTransactionManager;
    using km::ktm::NtRecoverTransactionManager;
    using km::ktm::NtRollforwardTransactionManager;
    using km::ktm::NtCreateResourceManager;
    using km::ktm::NtGetNotificationResourceManager;
    using km::ktm::NtRecoverResourceManager;
    using km::ktm::NtCreateTransaction;
    using km::ktm::NtQueryInformationTransaction;
    using km::ktm::NtCommitTransaction;
    using km::ktm::NtRollbackTransaction;
    using km::ktm::NtCreateEnlistment;
    using km::ktm::NtPrePrepareComplete;
    using km::ktm::NtPrepareComplete;
    using km::ktm::NtCommitComplete;
    using km::ktm::NtRollbackComplete;
    using km::ktm::NtRollbackEnlistment;
    using km::ktm::NtSinglePhaseReject;
    using km::ktm::NtReadOnlyEnlistment;
    using km::ktm::TmEnableCallbacks;
    using km::ktm::TmCreateEnlistment;
    using km::ktm::TmPrePrepareComplete;
    using km::ktm::TmPrepareComplete;
    using km::ktm::TmCommitComplete;
    using km::ktm::TmRollbackComplete;
    using km::ktm::TmRollbackEnlistment;
    using km::ktm::TmSinglePhaseReject;
    using km::ktm::TmReadOnlyEnlistment;
    using km::ktm::ClfsCreateLogFile;
    using km::ktm::ClfsCloseLogFileObject;
    using km::ktm::ClfsWriteRestartArea;
    using km::ktm::ClfsReadRestartArea;
    using km::ktm::ClfsFlushToLsn;
    using km::ktm::InitializeKtmSubsystemExports;
} // namespace micant
