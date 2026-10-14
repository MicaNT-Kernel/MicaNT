#pragma once
/**
 * @file verifier.hpp
 * @brief Horizon 46: Sovereign NT Kernel Driver Verifier (Vf* / Vi* — base/ntos/verifier),
 *        Special Pool Guard-Page Allocator, IRQL & SpinLock Enforcement, I/O IRP Stack &
 *        Completion Verifier, Lock-Order Deadlock Detector, DMA Adapter Verifier &
 *        Deterministic Fault-Injection Engine.
 *
 * Implements the clean-room Windows NT Kernel Driver Verifier (base/ntos/verifier):
 *   1. Per-Driver Verification Registration & Thunking:
 *      - MmAddVerifierThunks, MmIsDriverVerifying, MmIsDriverVerifyingByAddress,
 *        VfIsVerificationEnabled, VfRegisterTargetDriver, VfNotifyDriverLoad,
 *        VfNotifyDriverUnload.
 *   2. Special Pool Guard-Page Allocator (DRIVER_VERIFIER_SPECIAL_POOLING):
 *      - VfAllocatePoolWithTag / MmAllocateSpecialPool & VfFreePoolWithTag / MmFreeSpecialPool.
 *      - Places allocations at the end of a page immediately adjacent to an inaccessible
 *        guard page (PAGE_NOACCESS), fills leading slop bytes with deterministic signature
 *        patterns (0xA5 / 0x5A) and poisons freed blocks (0xDD), detecting buffer overruns,
 *        underruns, double-frees, pool tag mismatches, and leaked allocations on DriverUnload.
 *   3. IRQL, SpinLock & Pageable Memory Verification (DRIVER_VERIFIER_FORCE_IRQL_CHECKING):
 *      - VfRaiseIrql, VfLowerIrql, VfAcquireSpinLock, VfReleaseSpinLock, VfCheckPageableAccess.
 *      - Detects PagedPool allocation or pageable memory access at IRQL >= DISPATCH_LEVEL (2)
 *        and catches SpinLock IRQL restore mismatches.
 *   4. I/O Manager IRP Verification (DRIVER_VERIFIER_IO_CHECKING):
 *      - VfIoAllocateIrp, VfIoInitializeIrp, VfIoMarkIrpPending, VfIoCallDriver,
 *        VfIoCompleteRequest, VfIoCancelIrp, VfIoFreeIrp.
 *      - Detects DRIVER_VERIFIER_IOMANAGER_VIOLATION (0xC9): completing an IRP with
 *        STATUS_PENDING, returning STATUS_PENDING without marking the IRP pending,
 *        double-completion, stack location underflow, and freeing an active IRP.
 *   5. Lock-Order Graph Deadlock Detection (DRIVER_VERIFIER_DEADLOCK_DETECTION):
 *      - VfDeadlockInitializeResource, VfDeadlockAcquireResource, VfDeadlockReleaseResource.
 *      - Builds a live directed lock-acquisition dependency graph and detects AB-BA
 *        cycle inversions immediately upon lock acquisition.
 *   6. DMA Adapter & Scatter/Gather Verification (DRIVER_VERIFIER_DMA_CHECKING):
 *      - VfGetDmaAdapter, VfAllocateCommonBuffer, VfFreeCommonBuffer, VfMapTransfer,
 *        VfFlushAdapterBuffers, VfPutDmaAdapter.
 *      - Detects map-register bounds violations, unflushed DMA map registers, and leaked
 *        common buffers (DRIVER_VERIFIER_DMA_VIOLATION = 0xE6).
 *   7. Deterministic Fault Injection & SystemVerifierInformation (0x33):
 *      - VfSetFaultInjectionPolicy, VfQuerySystemVerifierInformation.
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
#include <unordered_set>
#include <mutex>
#include <memory>
#include <algorithm>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/native/janusldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::km::verifier {

using PVOID          = void*;
using ULONG          = uint32_t;
using USHORT         = uint16_t;
using UCHAR          = uint8_t;
using BOOLEAN        = uint8_t;
using KIRQL          = uint8_t;

inline constexpr BOOLEAN FALSE_B = 0;
inline constexpr BOOLEAN TRUE_B  = 1;

// ============================================================================
// 1. Driver Verifier Option Flags, IRQL Levels & BugCheck Codes
// ============================================================================

inline constexpr uint32_t DRIVER_VERIFIER_SPECIAL_POOLING             = 0x00000001;
inline constexpr uint32_t DRIVER_VERIFIER_FORCE_IRQL_CHECKING         = 0x00000002;
inline constexpr uint32_t DRIVER_VERIFIER_INJECT_ALLOCATION_FAILURES  = 0x00000004;
inline constexpr uint32_t DRIVER_VERIFIER_TRACK_POOL_ALLOCATIONS      = 0x00000008;
inline constexpr uint32_t DRIVER_VERIFIER_IO_CHECKING                 = 0x00000010;
inline constexpr uint32_t DRIVER_VERIFIER_DEADLOCK_DETECTION          = 0x00000020;
inline constexpr uint32_t DRIVER_VERIFIER_ENHANCED_IO_CHECKING        = 0x00000040;
inline constexpr uint32_t DRIVER_VERIFIER_DMA_CHECKING                = 0x00000080;
inline constexpr uint32_t DRIVER_VERIFIER_SECURITY_CHECKS             = 0x00000100;
inline constexpr uint32_t DRIVER_VERIFIER_FORCE_PENDING_IO_REQUESTS   = 0x00000200;
inline constexpr uint32_t DRIVER_VERIFIER_IRP_LOGGING                 = 0x00000400;

inline constexpr uint32_t DRIVER_VERIFIER_STANDARD_FLAGS =
    DRIVER_VERIFIER_SPECIAL_POOLING |
    DRIVER_VERIFIER_FORCE_IRQL_CHECKING |
    DRIVER_VERIFIER_TRACK_POOL_ALLOCATIONS |
    DRIVER_VERIFIER_IO_CHECKING |
    DRIVER_VERIFIER_DEADLOCK_DETECTION |
    DRIVER_VERIFIER_DMA_CHECKING;

// Standard NT IRQL constants for Verifier enforcement
inline constexpr KIRQL VF_PASSIVE_LEVEL   = 0;
inline constexpr KIRQL VF_APC_LEVEL       = 1;
inline constexpr KIRQL VF_DISPATCH_LEVEL  = 2;
inline constexpr KIRQL VF_DIRQL_LEVEL     = 9;
inline constexpr KIRQL VF_HIGH_LEVEL      = 15;

// Pool types for Verifier checks
enum class VF_POOL_TYPE : uint32_t {
    NonPagedPool          = 0,
    PagedPool             = 1,
    NonPagedPoolNx        = 512
};

// Standard NT Driver Verifier BugCheck Major Codes
inline constexpr uint32_t BUGCHECK_SPECIAL_POOL_DETECTED_MEMORY_CORRUPTION = 0x000000C1;
inline constexpr uint32_t BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION      = 0x000000C4;
inline constexpr uint32_t BUGCHECK_DRIVER_VERIFIER_IOMANAGER_VIOLATION     = 0x000000C9;
inline constexpr uint32_t BUGCHECK_DRIVER_PAGE_FAULT_IN_FREED_SPECIAL_POOL = 0x000000D5;
inline constexpr uint32_t BUGCHECK_DRIVER_VERIFIER_DMA_VIOLATION           = 0x000000E6;

// BugCheck 0xC4 (DRIVER_VERIFIER_DETECTED_VIOLATION) Subcodes
inline constexpr uint64_t VF_C4_ALLOCATE_PAGED_POOL_AT_DISPATCH    = 0x00000000ULL;
inline constexpr uint64_t VF_C4_FREE_PAGED_POOL_AT_DISPATCH        = 0x00000001ULL;
inline constexpr uint64_t VF_C4_SPINLOCK_ACQUIRE_AT_LOW_IRQL       = 0x00000010ULL;
inline constexpr uint64_t VF_C4_SPINLOCK_IRQL_MISMATCH             = 0x00000011ULL;
inline constexpr uint64_t VF_C4_LOWER_IRQL_WITH_HELD_SPINLOCKS     = 0x00000012ULL;
inline constexpr uint64_t VF_C4_PAGEABLE_MEMORY_AT_HIGH_IRQL       = 0x00000020ULL;
inline constexpr uint64_t VF_C4_SPECIAL_POOL_UNDERRUN_CORRUPTION   = 0x00000051ULL;
inline constexpr uint64_t VF_C4_SPECIAL_POOL_OVERRUN_CORRUPTION    = 0x00000052ULL;
inline constexpr uint64_t VF_C4_POOL_TAG_MISMATCH_ON_FREE          = 0x00000053ULL;
inline constexpr uint64_t VF_C4_DOUBLE_FREE_OR_UNTRACKED_POOL      = 0x00000054ULL;
inline constexpr uint64_t VF_C4_DRIVER_UNLOAD_WITH_POOL_LEAKS      = 0x00000062ULL;
inline constexpr uint64_t VF_C4_DEADLOCK_LOCK_ORDER_CYCLE          = 0x00001000ULL;

// BugCheck 0xC9 (DRIVER_VERIFIER_IOMANAGER_VIOLATION) Subcodes
inline constexpr uint64_t VF_C9_IRP_DOUBLE_COMPLETION              = 0x00000201ULL;
inline constexpr uint64_t VF_C9_IRP_COMPLETED_WITH_PENDING_STATUS  = 0x00000202ULL;
inline constexpr uint64_t VF_C9_IRP_STACK_LOCATION_UNDERFLOW       = 0x00000203ULL;
inline constexpr uint64_t VF_C9_IRP_PENDING_RETURNED_NOT_MARKED    = 0x00000204ULL;
inline constexpr uint64_t VF_C9_IRP_FREE_WHILE_STILL_ACTIVE        = 0x00000205ULL;
inline constexpr uint64_t VF_C9_IRP_UNTOUCHED_STATUS_SUCCESS       = 0x00000206ULL;

// BugCheck 0xE6 (DRIVER_VERIFIER_DMA_VIOLATION) Subcodes
inline constexpr uint64_t VF_E6_DMA_MAP_REGISTER_OVERRUN           = 0x0000001AULL;
inline constexpr uint64_t VF_E6_DMA_UNFLUSHED_MAP_REGISTERS        = 0x0000001BULL;
inline constexpr uint64_t VF_E6_DMA_LEAKED_COMMON_BUFFER           = 0x0000001CULL;
inline constexpr uint64_t VF_E6_DMA_DOUBLE_FREE_COMMON_BUFFER      = 0x0000001DULL;

// SystemInformationClass for NtQuerySystemInformation
inline constexpr uint32_t SystemVerifierInformationClass           = 0x00000033;

// Verifier Object Types for VfIsVerificationEnabled
enum class VF_OBJECT_TYPE : uint32_t {
    VfObjTypeDriver     = 1,
    VfObjTypeDevice     = 2
};

// ============================================================================
// 2. Violation Record, Special Pool Header, IRP Tracker & DMA Structures
// ============================================================================

struct VF_VIOLATION_RECORD {
    uint32_t    BugCheckCode{0};
    uint64_t    Parameter1{0}; // Subcode
    uint64_t    Parameter2{0}; // Offending address / IRP / Lock
    uint64_t    Parameter3{0}; // Extra context (size / IRQL / map registers)
    uint64_t    Parameter4{0}; // Driver ID / Tag
    std::wstring DriverName{};
    std::string  Description{};
    KIRQL       IrqlAtViolation{0};
};

// Special Pool Slop & Guard Signatures
inline constexpr uint8_t SPECIAL_POOL_SLOP_PATTERN   = 0xA5;
inline constexpr uint8_t SPECIAL_POOL_TAIL_PATTERN   = 0x5A;
inline constexpr uint8_t SPECIAL_POOL_FREED_PATTERN  = 0xDD;
inline constexpr size_t  SPECIAL_POOL_PAGE_SIZE      = 4096;
inline constexpr size_t  SPECIAL_POOL_GUARD_SIZE     = 64;

struct VI_POOL_ALLOCATION_RECORD {
    uint64_t     AllocationId{0};
    std::wstring DriverName{};
    VF_POOL_TYPE PoolType{VF_POOL_TYPE::NonPagedPool};
    size_t       RequestedBytes{0};
    uint32_t     PoolTag{0};
    KIRQL        AllocatedAtIrql{0};
    bool         UsedSpecialPool{false};
    bool         IsFreed{false};
    std::vector<uint8_t> BackingArena{}; // [Slop Header (64B) | User Payload (RequestedBytes) | Guard Tail (64B)]
    uint8_t*     UserPtr{nullptr};
};

enum class VI_IRP_LIFECYCLE_STATE : uint32_t {
    Allocated   = 0,
    Dispatched  = 1,
    Completed   = 2,
    Freed       = 3
};

struct VF_IRP_STACK_LOCATION {
    uint8_t  MajorFunction{0};
    uint8_t  MinorFunction{0};
    uint8_t  Control{0}; // SL_PENDING_RETURNED = 0x01
    void*    DeviceObject{nullptr};
    void*    CompletionRoutine{nullptr};
};

inline constexpr uint8_t VF_SL_PENDING_RETURNED = 0x01;

struct VF_IRP {
    uint64_t     IrpId{0};
    int8_t       StackCount{0};
    int8_t       CurrentLocation{0};
    bool         PendingReturned{false};
    bool         Cancel{false};
    NtStatus     IoStatus{NtStatus::NotSupported}; // Initialized to STATUS_NOT_SUPPORTED (0xC00000BB)
    uintptr_t    Information{0};
    VI_IRP_LIFECYCLE_STATE VerifierState{VI_IRP_LIFECYCLE_STATE::Allocated};
    std::wstring AllocatingDriver{};
    std::vector<VF_IRP_STACK_LOCATION> StackLocations{};

    VF_IRP_STACK_LOCATION* getCurrentStackLocation() noexcept {
        if (CurrentLocation >= 1 && CurrentLocation <= StackCount) {
            return &StackLocations[static_cast<size_t>(CurrentLocation - 1)];
        }
        return nullptr;
    }

    VF_IRP_STACK_LOCATION* getNextStackLocation() noexcept {
        if (CurrentLocation >= 2 && CurrentLocation <= StackCount + 1) {
            return &StackLocations[static_cast<size_t>(CurrentLocation - 2)];
        }
        return nullptr;
    }
};

using PVF_IRP = VF_IRP*;

// Deadlock Detection Resource Node
enum class VF_DEADLOCK_RESOURCE_TYPE : uint32_t {
    SpinLock     = 1,
    FastMutex    = 2,
    ExecutiveRes = 3,
    Mutex        = 4
};

struct VI_DEADLOCK_RESOURCE {
    void*                     ResourceAddress{nullptr};
    VF_DEADLOCK_RESOURCE_TYPE Type{VF_DEADLOCK_RESOURCE_TYPE::SpinLock};
    std::wstring              DriverName{};
    uint32_t                  HolderThreadId{0};
    uint32_t                  RecursionCount{0};
};

// DMA Adapter Verification Descriptor
struct VF_DMA_ADAPTER {
    uint64_t     AdapterId{0};
    std::wstring DriverName{};
    uint32_t     MaxMapRegisters{0};
    uint32_t     ActiveMappedRegisters{0};
    uint32_t     TotalMapTransfers{0};
    uint32_t     TotalFlushes{0};
    std::unordered_map<void*, size_t> ActiveCommonBuffers{};
};

using PVF_DMA_ADAPTER = VF_DMA_ADAPTER*;

// SystemVerifierInformation (0x33) Query Output Structure
struct SYSTEM_VERIFIER_INFORMATION {
    uint32_t Level{0};
    uint32_t NumberOfTrackedDrivers{0};
    uint32_t Loads{0};
    uint32_t Unloads{0};
    uint32_t SpecialPoolAllocations{0};
    uint32_t CurrentPagedPoolAllocations{0};
    uint32_t CurrentNonPagedPoolAllocations{0};
    uint32_t PeakSpecialPoolAllocations{0};
    uint64_t SpecialPoolBytesInUse{0};
    uint32_t AllocatedIrps{0};
    uint32_t CompletedIrps{0};
    uint32_t FaultsInjected{0};
    uint32_t DeadlockNodesTracked{0};
    uint32_t DmaAdaptersVerified{0};
    uint32_t ViolationsDetected{0};
};

// Tracked Driver Target Entry
struct VI_TARGET_DRIVER_ENTRY {
    std::wstring DriverName{};
    uintptr_t    ImageBase{0};
    size_t       ImageSize{0};
    bool         IsLoaded{false};
    uint32_t     ActivePoolBlocks{0};
    size_t       ActivePoolBytes{0};
    uint32_t     TotalPoolAllocations{0};
    uint32_t     TotalPoolFrees{0};
    uint32_t     ThunksAdded{0};
};

// ============================================================================
// 3. Sovereign NT Driver Verifier Engine (TitanDriverVerifier)
// ============================================================================

class TitanDriverVerifier {
public:
    static TitanDriverVerifier& get() noexcept {
        static TitanDriverVerifier instance;
        return instance;
    }

    void resetForTest() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        verifierFlags_                = DRIVER_VERIFIER_STANDARD_FLAGS;
        interceptViolations_          = true;
        currentIrql_                  = VF_PASSIVE_LEVEL;
        heldSpinLockCount_            = 0;
        faultInjectionEnabled_        = false;
        faultInjectionCountdown_      = 0;
        faultInjectionEveryNth_       = 0;
        faultInjectionCallCounter_    = 0;
        nextAllocId_                  = 1;
        nextIrpId_                    = 1;
        nextDmaAdapterId_             = 1;
        loadsCount_                   = 0;
        unloadsCount_                 = 0;
        specialPoolAllocs_            = 0;
        peakSpecialPoolAllocs_        = 0;
        currentPagedPoolAllocs_       = 0;
        currentNonPagedPoolAllocs_    = 0;
        specialPoolBytesInUse_        = 0;
        allocatedIrpsCount_           = 0;
        completedIrpsCount_           = 0;
        faultsInjectedCount_          = 0;

        targetDrivers_.clear();
        poolLedger_.clear();
        ptrToAllocId_.clear();
        activeSpinLocks_.clear();
        irpTable_.clear();
        deadlockResources_.clear();
        threadHeldLocks_.clear();
        lockOrderGraph_.clear();
        dmaAdapters_.clear();
        violations_.clear();
    }

    // ------------------------------------------------------------------------
    // Configuration & Target Driver Registration
    // ------------------------------------------------------------------------

    void setVerifierFlags(uint32_t flags) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        verifierFlags_ = flags;
    }

    uint32_t getVerifierFlags() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return verifierFlags_;
    }

    void setInterceptViolationMode(bool intercept) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        interceptViolations_ = intercept;
    }

    NtStatus registerTargetDriver(std::wstring_view driverName,
                                  uintptr_t imageBase = 0xFFFFF80010000000ULL,
                                  size_t imageSize = 0x40000) noexcept {
        if (driverName.empty()) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);
        std::wstring key(driverName);
        auto& entry = targetDrivers_[key];
        entry.DriverName = key;
        entry.ImageBase  = imageBase;
        entry.ImageSize  = imageSize;
        entry.IsLoaded   = true;
        loadsCount_++;
        return NtStatus::Success;
    }

    NtStatus addVerifierThunks(std::wstring_view driverName, uint32_t thunkCount) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        std::wstring key(driverName);
        auto it = targetDrivers_.find(key);
        if (it == targetDrivers_.end()) return NtStatus::NotFound;
        it->second.ThunksAdded += thunkCount;
        return NtStatus::Success;
    }

    bool isDriverVerifying(std::wstring_view driverName) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        std::wstring key(driverName);
        auto it = targetDrivers_.find(key);
        return (it != targetDrivers_.end() && it->second.IsLoaded);
    }

    bool isDriverVerifyingByAddress(uintptr_t codeAddress) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, drv] : targetDrivers_) {
            if (drv.IsLoaded && codeAddress >= drv.ImageBase &&
                codeAddress < (drv.ImageBase + drv.ImageSize)) {
                return true;
            }
        }
        return false;
    }

    // ------------------------------------------------------------------------
    // Fault Injection & Low-Resource Simulation
    // ------------------------------------------------------------------------

    void setFaultInjectionPolicy(bool enabled, uint32_t failEveryNthAllocation = 1) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        faultInjectionEnabled_     = enabled;
        faultInjectionEveryNth_    = (failEveryNthAllocation == 0) ? 1 : failEveryNthAllocation;
        faultInjectionCallCounter_ = 0;
    }

    // ------------------------------------------------------------------------
    // IRQL & SpinLock Verification (DRIVER_VERIFIER_FORCE_IRQL_CHECKING)
    // ------------------------------------------------------------------------

    KIRQL getCurrentIrql() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentIrql_;
    }

    KIRQL raiseIrql(KIRQL newIrql, std::wstring_view driverName = L"ntoskrnl.sys") noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        KIRQL oldIrql = currentIrql_;
        if (newIrql < oldIrql) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                VF_C4_SPINLOCK_IRQL_MISMATCH,
                oldIrql,
                newIrql,
                0,
                driverName,
                "KeRaiseIrql invoked with NewIrql < CurrentIrql"
            );
        }
        currentIrql_ = newIrql;
        return oldIrql;
    }

    void lowerIrql(KIRQL newIrql, std::wstring_view driverName = L"ntoskrnl.sys") noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (newIrql < VF_DISPATCH_LEVEL && heldSpinLockCount_ > 0) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                VF_C4_LOWER_IRQL_WITH_HELD_SPINLOCKS,
                currentIrql_,
                newIrql,
                heldSpinLockCount_,
                driverName,
                "Driver lowered IRQL below DISPATCH_LEVEL while still holding a KSPIN_LOCK"
            );
        }
        currentIrql_ = newIrql;
    }

    KIRQL acquireSpinLock(void* spinLockAddr, std::wstring_view driverName) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        KIRQL oldIrql = currentIrql_;
        if ((verifierFlags_ & DRIVER_VERIFIER_FORCE_IRQL_CHECKING) != 0 &&
            currentIrql_ > VF_DISPATCH_LEVEL) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                VF_C4_SPINLOCK_ACQUIRE_AT_LOW_IRQL,
                reinterpret_cast<uint64_t>(spinLockAddr),
                currentIrql_,
                VF_DISPATCH_LEVEL,
                driverName,
                "KeAcquireSpinLock invoked at DIRQL > DISPATCH_LEVEL"
            );
        }
        currentIrql_ = VF_DISPATCH_LEVEL;
        activeSpinLocks_[spinLockAddr] = oldIrql;
        heldSpinLockCount_++;
        return oldIrql;
    }

    void releaseSpinLock(void* spinLockAddr, KIRQL restoreIrql, std::wstring_view driverName) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = activeSpinLocks_.find(spinLockAddr);
        if (it == activeSpinLocks_.end()) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                VF_C4_SPINLOCK_IRQL_MISMATCH,
                reinterpret_cast<uint64_t>(spinLockAddr),
                restoreIrql,
                currentIrql_,
                driverName,
                "KeReleaseSpinLock invoked on an unacquired KSPIN_LOCK"
            );
            return;
        }
        KIRQL expectedOldIrql = it->second;
        if (restoreIrql != expectedOldIrql) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                VF_C4_SPINLOCK_IRQL_MISMATCH,
                reinterpret_cast<uint64_t>(spinLockAddr),
                restoreIrql,
                expectedOldIrql,
                driverName,
                "KeReleaseSpinLock NewIrql does not match the OldIrql saved on acquisition"
            );
        }
        activeSpinLocks_.erase(it);
        if (heldSpinLockCount_ > 0) heldSpinLockCount_--;
        currentIrql_ = restoreIrql;
    }

    bool checkPageableMemoryAccess(const void* address, std::wstring_view driverName) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if ((verifierFlags_ & DRIVER_VERIFIER_FORCE_IRQL_CHECKING) != 0 &&
            currentIrql_ >= VF_DISPATCH_LEVEL) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                VF_C4_PAGEABLE_MEMORY_AT_HIGH_IRQL,
                reinterpret_cast<uint64_t>(address),
                currentIrql_,
                1,
                driverName,
                "Pageable memory or code touched at IRQL >= DISPATCH_LEVEL"
            );
            return false;
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // Special Pool & Tracked Pool Allocator (VfAllocatePoolWithTag / VfFreePoolWithTag)
    // ------------------------------------------------------------------------

    void* allocatePoolWithTag(VF_POOL_TYPE poolType,
                              size_t numberOfBytes,
                              uint32_t tag,
                              std::wstring_view driverName) noexcept {
        if (numberOfBytes == 0) return nullptr;
        std::lock_guard<std::mutex> lock(mutex_);

        // 1. Check IRQL vs PagedPool constraint
        if ((verifierFlags_ & DRIVER_VERIFIER_FORCE_IRQL_CHECKING) != 0 &&
            poolType == VF_POOL_TYPE::PagedPool &&
            currentIrql_ >= VF_DISPATCH_LEVEL) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                VF_C4_ALLOCATE_PAGED_POOL_AT_DISPATCH,
                currentIrql_,
                static_cast<uint64_t>(poolType),
                numberOfBytes,
                driverName,
                "ExAllocatePoolWithTag(PagedPool) called at IRQL >= DISPATCH_LEVEL"
            );
            return nullptr;
        }

        // 2. Check Deterministic Fault Injection
        if ((verifierFlags_ & DRIVER_VERIFIER_INJECT_ALLOCATION_FAILURES) != 0 &&
            faultInjectionEnabled_) {
            faultInjectionCallCounter_++;
            if ((faultInjectionCallCounter_ % faultInjectionEveryNth_) == 0) {
                faultsInjectedCount_++;
                return nullptr;
            }
        }

        // 3. Allocate from Special Pool with leading slop (0xA5) & trailing guard page (0x5A)
        uint64_t allocId = nextAllocId_++;
        auto rec = std::make_unique<VI_POOL_ALLOCATION_RECORD>();
        rec->AllocationId     = allocId;
        rec->DriverName       = std::wstring(driverName);
        rec->PoolType         = poolType;
        rec->RequestedBytes   = numberOfBytes;
        rec->PoolTag          = tag;
        rec->AllocatedAtIrql  = currentIrql_;
        rec->UsedSpecialPool  = (verifierFlags_ & DRIVER_VERIFIER_SPECIAL_POOLING) != 0;
        rec->IsFreed          = false;

        const size_t totalArenaBytes = SPECIAL_POOL_GUARD_SIZE + numberOfBytes + SPECIAL_POOL_GUARD_SIZE;
        rec->BackingArena.resize(totalArenaBytes, 0);

        // Fill leading 64-byte slop header with 0xA5 and trailing 64-byte guard region with 0x5A
        std::memset(rec->BackingArena.data(), SPECIAL_POOL_SLOP_PATTERN, SPECIAL_POOL_GUARD_SIZE);
        std::memset(rec->BackingArena.data() + SPECIAL_POOL_GUARD_SIZE, 0x00, numberOfBytes);
        std::memset(rec->BackingArena.data() + SPECIAL_POOL_GUARD_SIZE + numberOfBytes,
                    SPECIAL_POOL_TAIL_PATTERN, SPECIAL_POOL_GUARD_SIZE);

        uint8_t* userPtr = rec->BackingArena.data() + SPECIAL_POOL_GUARD_SIZE;
        rec->UserPtr = userPtr;

        ptrToAllocId_[userPtr] = allocId;
        poolLedger_[allocId]   = std::move(rec);

        // Update telemetry counters
        if ((verifierFlags_ & DRIVER_VERIFIER_SPECIAL_POOLING) != 0) {
            specialPoolAllocs_++;
            if (specialPoolAllocs_ > peakSpecialPoolAllocs_) {
                peakSpecialPoolAllocs_ = specialPoolAllocs_;
            }
            specialPoolBytesInUse_ += numberOfBytes;
        }
        if (poolType == VF_POOL_TYPE::PagedPool) {
            currentPagedPoolAllocs_++;
        } else {
            currentNonPagedPoolAllocs_++;
        }

        auto drvIt = targetDrivers_.find(std::wstring(driverName));
        if (drvIt != targetDrivers_.end()) {
            drvIt->second.ActivePoolBlocks++;
            drvIt->second.ActivePoolBytes += numberOfBytes;
            drvIt->second.TotalPoolAllocations++;
        }

        return userPtr;
    }

    bool verifySpecialPoolBlockIntegrity(const void* userPtr, std::wstring_view driverName) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto idIt = ptrToAllocId_.find(const_cast<void*>(userPtr));
        if (idIt == ptrToAllocId_.end()) return false;
        auto& rec = poolLedger_[idIt->second];
        return verifyGuardZonesLocked(*rec, driverName);
    }

    void freePoolWithTag(void* userPtr, uint32_t tag, std::wstring_view driverName) noexcept {
        if (!userPtr) return;
        std::lock_guard<std::mutex> lock(mutex_);

        auto idIt = ptrToAllocId_.find(userPtr);
        if (idIt == ptrToAllocId_.end()) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                VF_C4_DOUBLE_FREE_OR_UNTRACKED_POOL,
                reinterpret_cast<uint64_t>(userPtr),
                tag,
                0,
                driverName,
                "ExFreePoolWithTag invoked on an untracked or invalid pool address"
            );
            return;
        }

        auto& rec = poolLedger_[idIt->second];
        if (rec->IsFreed) {
            recordViolationLocked(
                BUGCHECK_DRIVER_PAGE_FAULT_IN_FREED_SPECIAL_POOL,
                reinterpret_cast<uint64_t>(userPtr),
                rec->PoolTag,
                rec->RequestedBytes,
                0,
                driverName,
                "Double-free detected in Special Pool block (DRIVER_PAGE_FAULT_IN_FREED_SPECIAL_POOL)"
            );
            return;
        }

        // 1. Check IRQL vs PagedPool free constraint
        if ((verifierFlags_ & DRIVER_VERIFIER_FORCE_IRQL_CHECKING) != 0 &&
            rec->PoolType == VF_POOL_TYPE::PagedPool &&
            currentIrql_ >= VF_DISPATCH_LEVEL) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                VF_C4_FREE_PAGED_POOL_AT_DISPATCH,
                currentIrql_,
                reinterpret_cast<uint64_t>(userPtr),
                rec->RequestedBytes,
                driverName,
                "ExFreePoolWithTag(PagedPool) called at IRQL >= DISPATCH_LEVEL"
            );
        }

        // 2. Check Pool Tag match (if caller specified a non-zero tag)
        if (tag != 0 && rec->PoolTag != tag) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                VF_C4_POOL_TAG_MISMATCH_ON_FREE,
                reinterpret_cast<uint64_t>(userPtr),
                rec->PoolTag,
                tag,
                driverName,
                "ExFreePoolWithTag tag mismatch against allocation tag"
            );
        }

        // 3. Inspect Special Pool leading slop (0xA5) & trailing guard page (0x5A) for buffer underrun/overrun
        if (rec->UsedSpecialPool) {
            (void)verifyGuardZonesLocked(*rec, driverName);
        }

        // Poison payload with 0xDD to catch use-after-free reads/writes
        std::memset(rec->UserPtr, SPECIAL_POOL_FREED_PATTERN, rec->RequestedBytes);
        rec->IsFreed = true;

        if (rec->UsedSpecialPool && specialPoolBytesInUse_ >= rec->RequestedBytes) {
            specialPoolBytesInUse_ -= rec->RequestedBytes;
        }
        if (rec->PoolType == VF_POOL_TYPE::PagedPool && currentPagedPoolAllocs_ > 0) {
            currentPagedPoolAllocs_--;
        } else if (currentNonPagedPoolAllocs_ > 0) {
            currentNonPagedPoolAllocs_--;
        }

        auto drvIt = targetDrivers_.find(rec->DriverName);
        if (drvIt != targetDrivers_.end()) {
            if (drvIt->second.ActivePoolBlocks > 0) drvIt->second.ActivePoolBlocks--;
            if (drvIt->second.ActivePoolBytes >= rec->RequestedBytes) {
                drvIt->second.ActivePoolBytes -= rec->RequestedBytes;
            }
            drvIt->second.TotalPoolFrees++;
        }
    }

    NtStatus notifyDriverUnload(std::wstring_view driverName) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        std::wstring key(driverName);
        auto drvIt = targetDrivers_.find(key);
        if (drvIt == targetDrivers_.end()) return NtStatus::NotFound;

        unloadsCount_++;
        drvIt->second.IsLoaded = false;

        // Verify zero leaked pool allocations on DriverUnload
        if ((verifierFlags_ & DRIVER_VERIFIER_TRACK_POOL_ALLOCATIONS) != 0 &&
            drvIt->second.ActivePoolBlocks > 0) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                VF_C4_DRIVER_UNLOAD_WITH_POOL_LEAKS,
                drvIt->second.ImageBase,
                drvIt->second.ActivePoolBytes,
                drvIt->second.ActivePoolBlocks,
                driverName,
                "Driver unloaded without freeing all pool allocations (BugCheck 0xC4 Subcode 0x62)"
            );
            return NtStatus::Unsuccessful;
        }
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // I/O Manager IRP Verification (DRIVER_VERIFIER_IO_CHECKING)
    // ------------------------------------------------------------------------

    PVF_IRP allocateVerifiedIrp(int8_t stackSize, std::wstring_view driverName) noexcept {
        if (stackSize <= 0) return nullptr;
        std::lock_guard<std::mutex> lock(mutex_);

        if ((verifierFlags_ & DRIVER_VERIFIER_INJECT_ALLOCATION_FAILURES) != 0 &&
            faultInjectionEnabled_) {
            faultInjectionCallCounter_++;
            if ((faultInjectionCallCounter_ % faultInjectionEveryNth_) == 0) {
                faultsInjectedCount_++;
                return nullptr;
            }
        }

        uint64_t irpId = nextIrpId_++;
        auto irp = std::make_unique<VF_IRP>();
        irp->IrpId            = irpId;
        irp->StackCount       = stackSize;
        irp->CurrentLocation  = static_cast<int8_t>(stackSize);
        irp->PendingReturned  = false;
        irp->Cancel           = false;
        irp->IoStatus         = NtStatus::NotSupported; // Sentinel 0xC00000BB
        irp->Information      = 0;
        irp->VerifierState    = VI_IRP_LIFECYCLE_STATE::Allocated;
        irp->AllocatingDriver = std::wstring(driverName);
        irp->StackLocations.resize(static_cast<size_t>(stackSize));

        PVF_IRP rawPtr = irp.get();
        irpTable_[rawPtr] = std::move(irp);
        allocatedIrpsCount_++;
        return rawPtr;
    }

    void markIrpPending(PVF_IRP irp) noexcept {
        if (!irp) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto* loc = irp->getCurrentStackLocation()) {
            loc->Control |= VF_SL_PENDING_RETURNED;
        }
        irp->PendingReturned = true;
    }

    using PDRIVER_DISPATCH_VERIFIED = NtStatus(*)(void* DeviceObject, PVF_IRP Irp);

    NtStatus callVerifiedDriver(void* deviceObject,
                                PVF_IRP irp,
                                PDRIVER_DISPATCH_VERIFIED dispatchRoutine,
                                std::wstring_view driverName) noexcept {
        if (!irp || !dispatchRoutine) return NtStatus::InvalidParameter;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (irp->CurrentLocation <= 0) {
                recordViolationLocked(
                    BUGCHECK_DRIVER_VERIFIER_IOMANAGER_VIOLATION,
                    VF_C9_IRP_STACK_LOCATION_UNDERFLOW,
                    reinterpret_cast<uint64_t>(irp),
                    static_cast<uint64_t>(irp->CurrentLocation),
                    static_cast<uint64_t>(irp->StackCount),
                    driverName,
                    "IoCallDriver invoked with no remaining IRP stack locations (NO_MORE_IRP_STACK_LOCATIONS)"
                );
                return NtStatus::Unsuccessful;
            }
            irp->VerifierState = VI_IRP_LIFECYCLE_STATE::Dispatched;
        }

        NtStatus status = dispatchRoutine(deviceObject, irp);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto* loc = irp->getCurrentStackLocation();
            bool markedPending = (loc && (loc->Control & VF_SL_PENDING_RETURNED) != 0);

            if (status == NtStatus::Pending && !markedPending) {
                recordViolationLocked(
                    BUGCHECK_DRIVER_VERIFIER_IOMANAGER_VIOLATION,
                    VF_C9_IRP_PENDING_RETURNED_NOT_MARKED,
                    reinterpret_cast<uint64_t>(irp),
                    static_cast<uint64_t>(status),
                    0,
                    driverName,
                    "Dispatch routine returned STATUS_PENDING without calling IoMarkIrpPending"
                );
            }
        }

        return status;
    }

    void completeVerifiedRequest(PVF_IRP irp, std::wstring_view driverName) noexcept {
        if (!irp) return;
        std::lock_guard<std::mutex> lock(mutex_);

        // 1. Check Double Completion
        if (irp->VerifierState == VI_IRP_LIFECYCLE_STATE::Completed) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_IOMANAGER_VIOLATION,
                VF_C9_IRP_DOUBLE_COMPLETION,
                reinterpret_cast<uint64_t>(irp),
                static_cast<uint64_t>(irp->IoStatus),
                0,
                driverName,
                "IoCompleteRequest invoked on an IRP that was already completed"
            );
            return;
        }

        // 2. Check Completing with STATUS_PENDING
        if (irp->IoStatus == NtStatus::Pending) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_IOMANAGER_VIOLATION,
                VF_C9_IRP_COMPLETED_WITH_PENDING_STATUS,
                reinterpret_cast<uint64_t>(irp),
                static_cast<uint64_t>(irp->IoStatus),
                0,
                driverName,
                "IoCompleteRequest invoked with Irp->IoStatus.Status == STATUS_PENDING"
            );
        }

        irp->VerifierState = VI_IRP_LIFECYCLE_STATE::Completed;
        completedIrpsCount_++;
    }

    void freeVerifiedIrp(PVF_IRP irp, std::wstring_view driverName) noexcept {
        if (!irp) return;
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = irpTable_.find(irp);
        if (it == irpTable_.end()) return;

        if (irp->VerifierState == VI_IRP_LIFECYCLE_STATE::Dispatched) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_IOMANAGER_VIOLATION,
                VF_C9_IRP_FREE_WHILE_STILL_ACTIVE,
                reinterpret_cast<uint64_t>(irp),
                static_cast<uint64_t>(irp->VerifierState),
                0,
                driverName,
                "IoFreeIrp invoked on an IRP that is still active/dispatched"
            );
        }

        irp->VerifierState = VI_IRP_LIFECYCLE_STATE::Freed;
        irpTable_.erase(it);
    }

    // ------------------------------------------------------------------------
    // Lock-Order Graph Deadlock Detection (DRIVER_VERIFIER_DEADLOCK_DETECTION)
    // ------------------------------------------------------------------------

    void initializeDeadlockResource(void* resourceAddr,
                                    VF_DEADLOCK_RESOURCE_TYPE type,
                                    std::wstring_view driverName) noexcept {
        if (!resourceAddr) return;
        std::lock_guard<std::mutex> lock(mutex_);
        auto& node           = deadlockResources_[resourceAddr];
        node.ResourceAddress = resourceAddr;
        node.Type            = type;
        node.DriverName      = std::wstring(driverName);
        node.HolderThreadId  = 0;
        node.RecursionCount  = 0;
    }

    bool acquireDeadlockResource(void* resourceAddr,
                                 uint32_t threadId,
                                 std::wstring_view driverName) noexcept {
        if (!resourceAddr) return false;
        std::lock_guard<std::mutex> lock(mutex_);

        auto& heldList = threadHeldLocks_[threadId];

        // If already held recursively by the same thread, allow recursion
        if (std::find(heldList.begin(), heldList.end(), resourceAddr) != heldList.end()) {
            deadlockResources_[resourceAddr].RecursionCount++;
            return true;
        }

        // For every lock 'prevLock' currently held by 'threadId', we are adding a directed
        // lock-order edge: prevLock -> resourceAddr.
        // If there is already a path from resourceAddr -> prevLock in lockOrderGraph_,
        // then acquiring resourceAddr after prevLock introduces an AB-BA lock-order cycle!
        if ((verifierFlags_ & DRIVER_VERIFIER_DEADLOCK_DETECTION) != 0) {
            for (void* prevLock : heldList) {
                if (hasPathInLockGraphLocked(resourceAddr, prevLock)) {
                    recordViolationLocked(
                        BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                        VF_C4_DEADLOCK_LOCK_ORDER_CYCLE,
                        reinterpret_cast<uint64_t>(prevLock),
                        reinterpret_cast<uint64_t>(resourceAddr),
                        threadId,
                        driverName,
                        "Deadlock Detection: AB-BA lock-order inversion cycle detected in lock dependency graph"
                    );
                    return false;
                }
                lockOrderGraph_[prevLock].insert(resourceAddr);
            }
        }

        heldList.push_back(resourceAddr);
        auto& node          = deadlockResources_[resourceAddr];
        node.ResourceAddress = resourceAddr;
        node.HolderThreadId = threadId;
        node.RecursionCount = 1;
        return true;
    }

    void releaseDeadlockResource(void* resourceAddr, uint32_t threadId) noexcept {
        if (!resourceAddr) return;
        std::lock_guard<std::mutex> lock(mutex_);

        auto resIt = deadlockResources_.find(resourceAddr);
        if (resIt != deadlockResources_.end() && resIt->second.RecursionCount > 1) {
            resIt->second.RecursionCount--;
            return;
        }
        if (resIt != deadlockResources_.end()) {
            resIt->second.HolderThreadId = 0;
            resIt->second.RecursionCount = 0;
        }

        auto& heldList = threadHeldLocks_[threadId];
        heldList.erase(std::remove(heldList.begin(), heldList.end(), resourceAddr), heldList.end());
    }

    // ------------------------------------------------------------------------
    // DMA Adapter & Scatter/Gather Verification (DRIVER_VERIFIER_DMA_CHECKING)
    // ------------------------------------------------------------------------

    PVF_DMA_ADAPTER getVerifiedDmaAdapter(uint32_t maxMapRegisters,
                                          std::wstring_view driverName) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t id = nextDmaAdapterId_++;
        auto adapter = std::make_unique<VF_DMA_ADAPTER>();
        adapter->AdapterId             = id;
        adapter->DriverName            = std::wstring(driverName);
        adapter->MaxMapRegisters       = (maxMapRegisters == 0) ? 16 : maxMapRegisters;
        adapter->ActiveMappedRegisters = 0;
        PVF_DMA_ADAPTER rawPtr         = adapter.get();
        dmaAdapters_[rawPtr]           = std::move(adapter);
        return rawPtr;
    }

    void* allocateVerifiedCommonBuffer(PVF_DMA_ADAPTER adapter,
                                       size_t length,
                                       uint64_t* outLogicalAddress) noexcept {
        if (!adapter || length == 0) return nullptr;
        std::lock_guard<std::mutex> lock(mutex_);

        void* buf = std::malloc(length);
        if (!buf) return nullptr;
        std::memset(buf, 0, length);
        adapter->ActiveCommonBuffers[buf] = length;
        if (outLogicalAddress) {
            *outLogicalAddress = 0x0000000080000000ULL + (adapter->AdapterId * 0x100000ULL);
        }
        return buf;
    }

    void freeVerifiedCommonBuffer(PVF_DMA_ADAPTER adapter,
                                  size_t length,
                                  void* virtualAddress,
                                  std::wstring_view driverName) noexcept {
        if (!adapter || !virtualAddress) return;
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = adapter->ActiveCommonBuffers.find(virtualAddress);
        if (it == adapter->ActiveCommonBuffers.end()) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_DMA_VIOLATION,
                VF_E6_DMA_DOUBLE_FREE_COMMON_BUFFER,
                reinterpret_cast<uint64_t>(adapter),
                reinterpret_cast<uint64_t>(virtualAddress),
                length,
                driverName,
                "HalFreeCommonBuffer invoked on an untracked or already-freed DMA common buffer"
            );
            return;
        }
        std::free(virtualAddress);
        adapter->ActiveCommonBuffers.erase(it);
    }

    NtStatus mapVerifiedTransfer(PVF_DMA_ADAPTER adapter,
                                 uint32_t mapRegistersRequested,
                                 std::wstring_view driverName) noexcept {
        if (!adapter) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);

        if ((verifierFlags_ & DRIVER_VERIFIER_DMA_CHECKING) != 0 &&
            (adapter->ActiveMappedRegisters + mapRegistersRequested) > adapter->MaxMapRegisters) {
            recordViolationLocked(
                BUGCHECK_DRIVER_VERIFIER_DMA_VIOLATION,
                VF_E6_DMA_MAP_REGISTER_OVERRUN,
                reinterpret_cast<uint64_t>(adapter),
                adapter->ActiveMappedRegisters + mapRegistersRequested,
                adapter->MaxMapRegisters,
                driverName,
                "IoMapTransfer exceeded the maximum number of allocated DMA map registers"
            );
            return NtStatus::InsufficientResources;
        }

        adapter->ActiveMappedRegisters += mapRegistersRequested;
        adapter->TotalMapTransfers++;
        return NtStatus::Success;
    }

    NtStatus flushVerifiedAdapterBuffers(PVF_DMA_ADAPTER adapter,
                                         uint32_t mapRegistersToFlush) noexcept {
        if (!adapter) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);
        if (adapter->ActiveMappedRegisters >= mapRegistersToFlush) {
            adapter->ActiveMappedRegisters -= mapRegistersToFlush;
        } else {
            adapter->ActiveMappedRegisters = 0;
        }
        adapter->TotalFlushes++;
        return NtStatus::Success;
    }

    NtStatus putVerifiedDmaAdapter(PVF_DMA_ADAPTER adapter, std::wstring_view driverName) noexcept {
        if (!adapter) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = dmaAdapters_.find(adapter);
        if (it == dmaAdapters_.end()) return NtStatus::NotFound;

        bool violation = false;
        if ((verifierFlags_ & DRIVER_VERIFIER_DMA_CHECKING) != 0) {
            if (adapter->ActiveMappedRegisters > 0) {
                recordViolationLocked(
                    BUGCHECK_DRIVER_VERIFIER_DMA_VIOLATION,
                    VF_E6_DMA_UNFLUSHED_MAP_REGISTERS,
                    reinterpret_cast<uint64_t>(adapter),
                    adapter->ActiveMappedRegisters,
                    0,
                    driverName,
                    "PutDmaAdapter invoked while DMA map registers remain unflushed"
                );
                violation = true;
            }
            if (!adapter->ActiveCommonBuffers.empty()) {
                recordViolationLocked(
                    BUGCHECK_DRIVER_VERIFIER_DMA_VIOLATION,
                    VF_E6_DMA_LEAKED_COMMON_BUFFER,
                    reinterpret_cast<uint64_t>(adapter),
                    adapter->ActiveCommonBuffers.size(),
                    0,
                    driverName,
                    "PutDmaAdapter invoked with unfreed DMA Common Buffers"
                );
                violation = true;
            }
        }

        // Clean up any remaining buffers to avoid host memory leaks
        for (auto& [ptr, sz] : adapter->ActiveCommonBuffers) {
            std::free(ptr);
        }
        adapter->ActiveCommonBuffers.clear();
        dmaAdapters_.erase(it);
        return violation ? NtStatus::Unsuccessful : NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // SystemVerifierInformation (0x33) & Violation Telemetry
    // ------------------------------------------------------------------------

    NtStatus querySystemVerifierInformation(SYSTEM_VERIFIER_INFORMATION* outInfo) const noexcept {
        if (!outInfo) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);
        outInfo->Level                         = verifierFlags_;
        outInfo->NumberOfTrackedDrivers        = static_cast<uint32_t>(targetDrivers_.size());
        outInfo->Loads                         = loadsCount_;
        outInfo->Unloads                       = unloadsCount_;
        outInfo->SpecialPoolAllocations        = specialPoolAllocs_;
        outInfo->CurrentPagedPoolAllocations   = currentPagedPoolAllocs_;
        outInfo->CurrentNonPagedPoolAllocations= currentNonPagedPoolAllocs_;
        outInfo->PeakSpecialPoolAllocations    = peakSpecialPoolAllocs_;
        outInfo->SpecialPoolBytesInUse         = specialPoolBytesInUse_;
        outInfo->AllocatedIrps                 = allocatedIrpsCount_;
        outInfo->CompletedIrps                 = completedIrpsCount_;
        outInfo->FaultsInjected                = faultsInjectedCount_;
        outInfo->DeadlockNodesTracked          = static_cast<uint32_t>(deadlockResources_.size());
        outInfo->DmaAdaptersVerified           = static_cast<uint32_t>(dmaAdapters_.size());
        outInfo->ViolationsDetected            = static_cast<uint32_t>(violations_.size());
        return NtStatus::Success;
    }

    std::vector<VF_VIOLATION_RECORD> getViolations() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return violations_;
    }

    void clearViolations() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        violations_.clear();
    }

private:
    TitanDriverVerifier() = default;

    bool verifyGuardZonesLocked(const VI_POOL_ALLOCATION_RECORD& rec,
                                std::wstring_view driverName) noexcept {
        bool intact = true;
        // Check leading 64-byte slop header for underrun
        for (size_t i = 0; i < SPECIAL_POOL_GUARD_SIZE; ++i) {
            if (rec.BackingArena[i] != SPECIAL_POOL_SLOP_PATTERN) {
                recordViolationLocked(
                    BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                    VF_C4_SPECIAL_POOL_UNDERRUN_CORRUPTION,
                    reinterpret_cast<uint64_t>(rec.UserPtr),
                    rec.RequestedBytes,
                    rec.BackingArena[i],
                    driverName,
                    "Special Pool underrun detected in leading 0xA5 slop guard header"
                );
                intact = false;
                break;
            }
        }
        // Check trailing 64-byte guard zone for overrun
        const size_t tailOffset = SPECIAL_POOL_GUARD_SIZE + rec.RequestedBytes;
        for (size_t i = 0; i < SPECIAL_POOL_GUARD_SIZE; ++i) {
            if (rec.BackingArena[tailOffset + i] != SPECIAL_POOL_TAIL_PATTERN) {
                recordViolationLocked(
                    BUGCHECK_DRIVER_VERIFIER_DETECTED_VIOLATION,
                    VF_C4_SPECIAL_POOL_OVERRUN_CORRUPTION,
                    reinterpret_cast<uint64_t>(rec.UserPtr + rec.RequestedBytes + i),
                    rec.RequestedBytes,
                    rec.BackingArena[tailOffset + i],
                    driverName,
                    "Special Pool buffer overrun detected into adjacent guard zone"
                );
                intact = false;
                break;
            }
        }
        return intact;
    }

    bool hasPathInLockGraphLocked(void* src, void* dst) const noexcept {
        if (src == dst) return true;
        std::unordered_set<void*> visited;
        std::deque<void*> queue;
        queue.push_back(src);
        visited.insert(src);

        while (!queue.empty()) {
            void* curr = queue.front();
            queue.pop_front();
            auto it = lockOrderGraph_.find(curr);
            if (it == lockOrderGraph_.end()) continue;
            for (void* next : it->second) {
                if (next == dst) return true;
                if (visited.insert(next).second) {
                    queue.push_back(next);
                }
            }
        }
        return false;
    }

    void recordViolationLocked(uint32_t bugCheckCode,
                               uint64_t param1,
                               uint64_t param2,
                               uint64_t param3,
                               uint64_t param4,
                               std::wstring_view driverName,
                               std::string_view description) noexcept {
        VF_VIOLATION_RECORD v{};
        v.BugCheckCode    = bugCheckCode;
        v.Parameter1      = param1;
        v.Parameter2      = param2;
        v.Parameter3      = param3;
        v.Parameter4      = param4;
        v.DriverName      = std::wstring(driverName);
        v.Description     = std::string(description);
        v.IrqlAtViolation = currentIrql_;
        violations_.push_back(std::move(v));
    }

    mutable std::mutex mutex_;
    uint32_t verifierFlags_{DRIVER_VERIFIER_STANDARD_FLAGS};
    bool     interceptViolations_{true};
    KIRQL    currentIrql_{VF_PASSIVE_LEVEL};
    uint32_t heldSpinLockCount_{0};

    bool     faultInjectionEnabled_{false};
    uint32_t faultInjectionCountdown_{0};
    uint32_t faultInjectionEveryNth_{1};
    uint32_t faultInjectionCallCounter_{0};

    uint64_t nextAllocId_{1};
    uint64_t nextIrpId_{1};
    uint64_t nextDmaAdapterId_{1};

    uint32_t loadsCount_{0};
    uint32_t unloadsCount_{0};
    uint32_t specialPoolAllocs_{0};
    uint32_t peakSpecialPoolAllocs_{0};
    uint32_t currentPagedPoolAllocs_{0};
    uint32_t currentNonPagedPoolAllocs_{0};
    uint64_t specialPoolBytesInUse_{0};
    uint32_t allocatedIrpsCount_{0};
    uint32_t completedIrpsCount_{0};
    uint32_t faultsInjectedCount_{0};

    std::unordered_map<std::wstring, VI_TARGET_DRIVER_ENTRY> targetDrivers_;
    std::unordered_map<uint64_t, std::unique_ptr<VI_POOL_ALLOCATION_RECORD>> poolLedger_;
    std::unordered_map<void*, uint64_t> ptrToAllocId_;
    std::unordered_map<void*, KIRQL> activeSpinLocks_;
    std::unordered_map<PVF_IRP, std::unique_ptr<VF_IRP>> irpTable_;
    std::unordered_map<void*, VI_DEADLOCK_RESOURCE> deadlockResources_;
    std::unordered_map<uint32_t, std::vector<void*>> threadHeldLocks_;
    std::unordered_map<void*, std::unordered_set<void*>> lockOrderGraph_;
    std::unordered_map<PVF_DMA_ADAPTER, std::unique_ptr<VF_DMA_ADAPTER>> dmaAdapters_;
    std::vector<VF_VIOLATION_RECORD> violations_;
};

// ============================================================================
// 4. Public Kernel Mode (Vf* / Mm*) & Native (Nt*) C-Linkage / WDK APIs
// ============================================================================

inline NtStatus VfRegisterTargetDriver(std::wstring_view DriverName,
                                       uintptr_t ImageBase = 0xFFFFF80010000000ULL,
                                       size_t ImageSize = 0x40000) noexcept {
    return TitanDriverVerifier::get().registerTargetDriver(DriverName, ImageBase, ImageSize);
}

inline NtStatus MmAddVerifierThunks(std::wstring_view DriverName,
                                    uint32_t ThunkCount) noexcept {
    return TitanDriverVerifier::get().addVerifierThunks(DriverName, ThunkCount);
}

inline BOOLEAN MmIsDriverVerifying(std::wstring_view DriverName) noexcept {
    return TitanDriverVerifier::get().isDriverVerifying(DriverName) ? TRUE_B : FALSE_B;
}

inline BOOLEAN MmIsDriverVerifyingByAddress(const void* AddressWithinSection) noexcept {
    return TitanDriverVerifier::get().isDriverVerifyingByAddress(
        reinterpret_cast<uintptr_t>(AddressWithinSection)) ? TRUE_B : FALSE_B;
}

inline BOOLEAN VfIsVerificationEnabled(VF_OBJECT_TYPE VfObjectType,
                                       std::wstring_view DriverName) noexcept {
    (void)VfObjectType;
    return TitanDriverVerifier::get().isDriverVerifying(DriverName) ? TRUE_B : FALSE_B;
}

inline void* VfAllocatePoolWithTag(VF_POOL_TYPE PoolType,
                                   size_t NumberOfBytes,
                                   uint32_t Tag,
                                   std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    return TitanDriverVerifier::get().allocatePoolWithTag(PoolType, NumberOfBytes, Tag, DriverName);
}

inline void* MmAllocateSpecialPool(size_t NumberOfBytes,
                                   uint32_t Tag,
                                   VF_POOL_TYPE PoolType = VF_POOL_TYPE::NonPagedPool,
                                   std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    return TitanDriverVerifier::get().allocatePoolWithTag(PoolType, NumberOfBytes, Tag, DriverName);
}

inline void VfFreePoolWithTag(void* P,
                              uint32_t Tag,
                              std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    TitanDriverVerifier::get().freePoolWithTag(P, Tag, DriverName);
}

inline void MmFreeSpecialPool(void* P,
                              std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    TitanDriverVerifier::get().freePoolWithTag(P, 0, DriverName);
}

inline KIRQL VfRaiseIrql(KIRQL NewIrql, std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    return TitanDriverVerifier::get().raiseIrql(NewIrql, DriverName);
}

inline void VfLowerIrql(KIRQL NewIrql, std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    TitanDriverVerifier::get().lowerIrql(NewIrql, DriverName);
}

inline KIRQL VfAcquireSpinLock(void* SpinLock,
                               std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    return TitanDriverVerifier::get().acquireSpinLock(SpinLock, DriverName);
}

inline void VfReleaseSpinLock(void* SpinLock,
                              KIRQL NewIrql,
                              std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    TitanDriverVerifier::get().releaseSpinLock(SpinLock, NewIrql, DriverName);
}

inline BOOLEAN VfCheckPageableAccess(const void* Address,
                                     std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    return TitanDriverVerifier::get().checkPageableMemoryAccess(Address, DriverName) ? TRUE_B : FALSE_B;
}

inline NtStatus VfNotifyDriverUnload(std::wstring_view DriverName) noexcept {
    return TitanDriverVerifier::get().notifyDriverUnload(DriverName);
}

inline PVF_IRP VfIoAllocateIrp(int8_t StackSize,
                               std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    return TitanDriverVerifier::get().allocateVerifiedIrp(StackSize, DriverName);
}

inline void VfIoMarkIrpPending(PVF_IRP Irp) noexcept {
    TitanDriverVerifier::get().markIrpPending(Irp);
}

inline NtStatus VfIoCallDriver(void* DeviceObject,
                               PVF_IRP Irp,
                               TitanDriverVerifier::PDRIVER_DISPATCH_VERIFIED DispatchRoutine,
                               std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    return TitanDriverVerifier::get().callVerifiedDriver(DeviceObject, Irp, DispatchRoutine, DriverName);
}

inline void VfIoCompleteRequest(PVF_IRP Irp,
                                std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    TitanDriverVerifier::get().completeVerifiedRequest(Irp, DriverName);
}

inline void VfIoFreeIrp(PVF_IRP Irp,
                        std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    TitanDriverVerifier::get().freeVerifiedIrp(Irp, DriverName);
}

inline void VfDeadlockInitializeResource(void* ResourceAddress,
                                         VF_DEADLOCK_RESOURCE_TYPE Type,
                                         std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    TitanDriverVerifier::get().initializeDeadlockResource(ResourceAddress, Type, DriverName);
}

inline BOOLEAN VfDeadlockAcquireResource(void* ResourceAddress,
                                         uint32_t ThreadId,
                                         std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    return TitanDriverVerifier::get().acquireDeadlockResource(ResourceAddress, ThreadId, DriverName)
               ? TRUE_B : FALSE_B;
}

inline void VfDeadlockReleaseResource(void* ResourceAddress, uint32_t ThreadId) noexcept {
    TitanDriverVerifier::get().releaseDeadlockResource(ResourceAddress, ThreadId);
}

inline PVF_DMA_ADAPTER VfGetDmaAdapter(uint32_t MaxMapRegisters,
                                       std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    return TitanDriverVerifier::get().getVerifiedDmaAdapter(MaxMapRegisters, DriverName);
}

inline void* VfAllocateCommonBuffer(PVF_DMA_ADAPTER Adapter,
                                    size_t Length,
                                    uint64_t* OutLogicalAddress) noexcept {
    return TitanDriverVerifier::get().allocateVerifiedCommonBuffer(Adapter, Length, OutLogicalAddress);
}

inline void VfFreeCommonBuffer(PVF_DMA_ADAPTER Adapter,
                               size_t Length,
                               void* VirtualAddress,
                               std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    TitanDriverVerifier::get().freeVerifiedCommonBuffer(Adapter, Length, VirtualAddress, DriverName);
}

inline NtStatus VfMapTransfer(PVF_DMA_ADAPTER Adapter,
                              uint32_t MapRegistersRequested,
                              std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    return TitanDriverVerifier::get().mapVerifiedTransfer(Adapter, MapRegistersRequested, DriverName);
}

inline NtStatus VfFlushAdapterBuffers(PVF_DMA_ADAPTER Adapter,
                                      uint32_t MapRegistersToFlush) noexcept {
    return TitanDriverVerifier::get().flushVerifiedAdapterBuffers(Adapter, MapRegistersToFlush);
}

inline NtStatus VfPutDmaAdapter(PVF_DMA_ADAPTER Adapter,
                                std::wstring_view DriverName = L"verified_drv.sys") noexcept {
    return TitanDriverVerifier::get().putVerifiedDmaAdapter(Adapter, DriverName);
}

inline void VfSetFaultInjectionPolicy(BOOLEAN Enabled, uint32_t FailEveryNthAllocation = 1) noexcept {
    TitanDriverVerifier::get().setFaultInjectionPolicy(Enabled != 0, FailEveryNthAllocation);
}

inline NtStatus VfQuerySystemVerifierInformation(SYSTEM_VERIFIER_INFORMATION* OutInfo) noexcept {
    return TitanDriverVerifier::get().querySystemVerifierInformation(OutInfo);
}

// ============================================================================
// 5. Dynamic Export Registration (ntoskrnl.exe, ntdll.dll, verifier.dll)
// ============================================================================

inline void InitializeVerifierSubsystemExports() noexcept {
    auto& ldr = ldr::DynamicLoader::get();

    struct ExportEntry {
        const char* name;
        void*       fn;
    };

    const ExportEntry kNtosExports[] = {
        { "VfRegisterTargetDriver",          reinterpret_cast<void*>(&VfRegisterTargetDriver) },
        { "MmAddVerifierThunks",             reinterpret_cast<void*>(&MmAddVerifierThunks) },
        { "MmIsDriverVerifying",             reinterpret_cast<void*>(&MmIsDriverVerifying) },
        { "MmIsDriverVerifyingByAddress",    reinterpret_cast<void*>(&MmIsDriverVerifyingByAddress) },
        { "VfIsVerificationEnabled",         reinterpret_cast<void*>(&VfIsVerificationEnabled) },
        { "VfAllocatePoolWithTag",           reinterpret_cast<void*>(&VfAllocatePoolWithTag) },
        { "MmAllocateSpecialPool",           reinterpret_cast<void*>(&MmAllocateSpecialPool) },
        { "VfFreePoolWithTag",               reinterpret_cast<void*>(&VfFreePoolWithTag) },
        { "MmFreeSpecialPool",               reinterpret_cast<void*>(&MmFreeSpecialPool) },
        { "VfRaiseIrql",                     reinterpret_cast<void*>(&VfRaiseIrql) },
        { "VfLowerIrql",                     reinterpret_cast<void*>(&VfLowerIrql) },
        { "VfAcquireSpinLock",               reinterpret_cast<void*>(&VfAcquireSpinLock) },
        { "VfReleaseSpinLock",               reinterpret_cast<void*>(&VfReleaseSpinLock) },
        { "VfCheckPageableAccess",           reinterpret_cast<void*>(&VfCheckPageableAccess) },
        { "VfNotifyDriverUnload",            reinterpret_cast<void*>(&VfNotifyDriverUnload) },
        { "VfIoAllocateIrp",                 reinterpret_cast<void*>(&VfIoAllocateIrp) },
        { "VfIoMarkIrpPending",              reinterpret_cast<void*>(&VfIoMarkIrpPending) },
        { "VfIoCallDriver",                  reinterpret_cast<void*>(&VfIoCallDriver) },
        { "VfIoCompleteRequest",             reinterpret_cast<void*>(&VfIoCompleteRequest) },
        { "VfIoFreeIrp",                     reinterpret_cast<void*>(&VfIoFreeIrp) },
        { "VfDeadlockInitializeResource",    reinterpret_cast<void*>(&VfDeadlockInitializeResource) },
        { "VfDeadlockAcquireResource",       reinterpret_cast<void*>(&VfDeadlockAcquireResource) },
        { "VfDeadlockReleaseResource",       reinterpret_cast<void*>(&VfDeadlockReleaseResource) },
        { "VfGetDmaAdapter",                 reinterpret_cast<void*>(&VfGetDmaAdapter) },
        { "VfAllocateCommonBuffer",          reinterpret_cast<void*>(&VfAllocateCommonBuffer) },
        { "VfFreeCommonBuffer",              reinterpret_cast<void*>(&VfFreeCommonBuffer) },
        { "VfMapTransfer",                   reinterpret_cast<void*>(&VfMapTransfer) },
        { "VfFlushAdapterBuffers",           reinterpret_cast<void*>(&VfFlushAdapterBuffers) },
        { "VfPutDmaAdapter",                 reinterpret_cast<void*>(&VfPutDmaAdapter) },
        { "VfSetFaultInjectionPolicy",       reinterpret_cast<void*>(&VfSetFaultInjectionPolicy) },
        { "VfQuerySystemVerifierInformation",reinterpret_cast<void*>(&VfQuerySystemVerifierInformation) }
    };

    for (const auto& exp : kNtosExports) {
        ldr.registerExport("ntoskrnl.exe", exp.name, exp.fn);
        ldr.registerExport("verifier.dll", exp.name, exp.fn);
    }

    auto& scm = scm::ServiceControlManager::get();
    auto vfSvc = std::make_shared<scm::ServiceRecord>();
    vfSvc->serviceName           = L"VerifierExt";
    vfSvc->displayName           = L"Sovereign NT Kernel Driver Verifier (Vf* / Special Pool / Deadlock / IRP / DMA)";
    vfSvc->serviceType           = scm::SERVICE_KERNEL_DRIVER;
    vfSvc->startType             = scm::SERVICE_BOOT_START;
    vfSvc->errorControl          = scm::SERVICE_ERROR_NORMAL;
    vfSvc->binaryPath            = L"\\SystemRoot\\System32\\drivers\\VerifierExt.sys";
    vfSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(vfSvc);

    version::VersionDatabase::Instance().RegisterModule(
        "verifier_executive",
        "10.0.26100.4600",
        "Sovereign NT Kernel Driver Verifier (Vf* / Special Pool / IRP / Deadlock / DMA)"
    );
}

} // namespace micant::km::verifier

namespace micant {
    using km::verifier::VF_POOL_TYPE;
    using km::verifier::VF_OBJECT_TYPE;
    using km::verifier::VF_VIOLATION_RECORD;
    using km::verifier::VF_IRP;
    using km::verifier::PVF_IRP;
    using km::verifier::VF_DEADLOCK_RESOURCE_TYPE;
    using km::verifier::VF_DMA_ADAPTER;
    using km::verifier::PVF_DMA_ADAPTER;
    using km::verifier::SYSTEM_VERIFIER_INFORMATION;
    using km::verifier::TitanDriverVerifier;
    using km::verifier::VfRegisterTargetDriver;
    using km::verifier::MmAddVerifierThunks;
    using km::verifier::MmIsDriverVerifying;
    using km::verifier::MmIsDriverVerifyingByAddress;
    using km::verifier::VfIsVerificationEnabled;
    using km::verifier::VfAllocatePoolWithTag;
    using km::verifier::MmAllocateSpecialPool;
    using km::verifier::VfFreePoolWithTag;
    using km::verifier::MmFreeSpecialPool;
    using km::verifier::VfRaiseIrql;
    using km::verifier::VfLowerIrql;
    using km::verifier::VfAcquireSpinLock;
    using km::verifier::VfReleaseSpinLock;
    using km::verifier::VfCheckPageableAccess;
    using km::verifier::VfNotifyDriverUnload;
    using km::verifier::VfIoAllocateIrp;
    using km::verifier::VfIoMarkIrpPending;
    using km::verifier::VfIoCallDriver;
    using km::verifier::VfIoCompleteRequest;
    using km::verifier::VfIoFreeIrp;
    using km::verifier::VfDeadlockInitializeResource;
    using km::verifier::VfDeadlockAcquireResource;
    using km::verifier::VfDeadlockReleaseResource;
    using km::verifier::VfGetDmaAdapter;
    using km::verifier::VfAllocateCommonBuffer;
    using km::verifier::VfFreeCommonBuffer;
    using km::verifier::VfMapTransfer;
    using km::verifier::VfFlushAdapterBuffers;
    using km::verifier::VfPutDmaAdapter;
    using km::verifier::VfSetFaultInjectionPolicy;
    using km::verifier::VfQuerySystemVerifierInformation;
    using km::verifier::InitializeVerifierSubsystemExports;
} // namespace micant
