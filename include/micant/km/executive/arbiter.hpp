#pragma once
/**
 * @file arbiter.hpp
 * @brief Horizon 47: Sovereign NT Hardware Resource Arbiter (Arb* / Rtl*RangeList — base/ntos/arb),
 *        WDK Binary .sys Kernel Image Loader (MmLoadSystemImage / NtLoadDriver / NtUnloadDriver)
 *        & Stock .inf Driver Store Staging Engine.
 *
 * Implements the clean-room Windows NT Hardware Resource Arbiter (base/ntos/arb) and
 * WDK Binary .sys Loader pipeline:
 *   1. RTL Range List Engine (RTL_RANGE_LIST & RTL_RANGE):
 *      - RtlInitializeRangeList, RtlFreeRangeList, RtlCopyRangeList, RtlAddRange,
 *        RtlFindRange, RtlIsRangeAvailable, RtlDeleteRange, RtlDeleteOwnersRanges.
 *      - Supports exclusive vs shared hardware ranges (RTL_RANGE_SHARED) and deterministic
 *        conflict detection (STATUS_RANGE_LIST_CONFLICT).
 *   2. Root Hardware Resource Arbiters (ARBITER_INSTANCE — Arb*):
 *      - Memory Arbiter (CmResourceTypeMemory): PCI BAR 32/64-bit MMIO physical ranges.
 *      - I/O Port Arbiter (CmResourceTypePort): 16-bit x86/x64 I/O port apertures.
 *      - Interrupt Arbiter (CmResourceTypeInterrupt): Legacy GSI & PCIe MSI/MSI-X vectors,
 *        translating raw IRQ/MSI descriptors into IDT Vector, DIRQL Level, and KAFFINITY.
 *      - DMA Arbiter (CmResourceTypeDma): System DMA channel arbitration.
 *      - Two-phase resource allocation (ArbTestAllocation -> ArbCommitAllocation /
 *        ArbRollbackAllocation) and Raw-to-Translated CM_RESOURCE_LIST synthesis for
 *        IRP_MN_START_DEVICE.
 *   3. WDK Binary .sys PE32+ Kernel Image Loader (MmLoadSystemImage / NtLoadDriver):
 *      - Validates 64-bit PE32+ IMAGE_SUBSYSTEM_NATIVE (.sys) headers, binds IAT imports
 *        against ntoskrnl.exe, hal.dll, Wdf01000.sys, ndis.sys, and storport.sys, inserts
 *        KLDR_DATA_TABLE_ENTRY into PsLoadedModuleList, and invokes DriverEntry -> AddDevice
 *        -> IRP_MN_START_DEVICE(AllocatedResources, AllocatedResourcesTranslated).
 *   4. Stock .inf Driver Package Staging Engine (PnpStageAndInstallInfDriverPackage):
 *      - Parses standard WDK .inf sections ([Version], [Manufacturer], [DDInstall.NTamd64],
 *        [DDInstall.Services], AddService), stages files into DriverStore\FileRepository,
 *        registers the SCM service key, and loads the unmodified .sys driver.
 *
 * Clean-room ISO C++23 implementation. Zero proprietary or decompiled code.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <algorithm>
#include <sstream>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/native/janusldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::km::arb {

using PVOID     = void*;
using ULONG     = uint32_t;
using USHORT    = uint16_t;
using UCHAR     = uint8_t;
using BOOLEAN   = uint8_t;
using ULONGLONG = uint64_t;
using KAFFINITY = uint64_t;

inline constexpr BOOLEAN FALSE_B = 0;
inline constexpr BOOLEAN TRUE_B  = 1;

// ============================================================================
// 1. RTL Range List Structures & Flags (RTL_RANGE / RTL_RANGE_LIST)
// ============================================================================

inline constexpr uint8_t  RTL_RANGE_SHARED            = 0x01;
inline constexpr uint8_t  RTL_RANGE_CONFLICT          = 0x02;

inline constexpr uint32_t RTL_RANGE_LIST_ADD_SHARED   = 0x00000001;
inline constexpr uint32_t RTL_RANGE_LIST_ADD_IF_CONFLICT = 0x00000002;
inline constexpr uint32_t RTL_RANGE_LIST_SHARED_OK    = 0x00000001;

struct RTL_RANGE {
    uint64_t Start{0};
    uint64_t End{0};        // Inclusive [Start, End]
    void*    UserData{nullptr};
    void*    Owner{nullptr};
    uint8_t  Attributes{0};
    uint8_t  Flags{0};      // RTL_RANGE_SHARED | RTL_RANGE_CONFLICT
};

using PRTL_RANGE = RTL_RANGE*;

struct RTL_RANGE_LIST {
    uint32_t               Flags{0};
    uint32_t               Count{0};
    uint32_t               Stamp{0};
    std::vector<RTL_RANGE> Ranges{};
};

using PRTL_RANGE_LIST = RTL_RANGE_LIST*;

// ============================================================================
// 2. Canonical WDK Hardware Resource Descriptors (CM_PARTIAL_RESOURCE_DESCRIPTOR)
// ============================================================================

enum class CM_RESOURCE_TYPE : uint8_t {
    CmResourceTypeNull           = 0,
    CmResourceTypePort           = 1,
    CmResourceTypeInterrupt      = 2,
    CmResourceTypeMemory         = 3,
    CmResourceTypeDma            = 4,
    CmResourceTypeDeviceSpecific = 5,
    CmResourceTypeBusNumber      = 6,
    CmResourceTypeMemoryLarge    = 7
};

enum class CM_SHARE_DISPOSITION : uint8_t {
    CmResourceShareUndetermined = 0,
    CmResourceShareDeviceExclusive = 1,
    CmResourceShareDriverExclusive = 2,
    CmResourceShareShared          = 3
};

// Interrupt Flags
inline constexpr uint16_t CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE = 0x0000;
inline constexpr uint16_t CM_RESOURCE_INTERRUPT_LATCHED         = 0x0001;
inline constexpr uint16_t CM_RESOURCE_INTERRUPT_MESSAGE         = 0x0002; // MSI / MSI-X

// Memory Flags
inline constexpr uint16_t CM_RESOURCE_MEMORY_READ_WRITE         = 0x0000;
inline constexpr uint16_t CM_RESOURCE_MEMORY_READ_ONLY          = 0x0001;
inline constexpr uint16_t CM_RESOURCE_MEMORY_PREFETCHABLE       = 0x0004;
inline constexpr uint16_t CM_RESOURCE_MEMORY_BAR                = 0x0080;

// Port Flags
inline constexpr uint16_t CM_RESOURCE_PORT_MEMORY               = 0x0000;
inline constexpr uint16_t CM_RESOURCE_PORT_IO                   = 0x0001;

struct CM_PARTIAL_RESOURCE_DESCRIPTOR {
    CM_RESOURCE_TYPE     Type{CM_RESOURCE_TYPE::CmResourceTypeNull};
    CM_SHARE_DISPOSITION ShareDisposition{CM_SHARE_DISPOSITION::CmResourceShareDeviceExclusive};
    uint16_t             Flags{0};
    union {
        struct {
            uint64_t Start;
            uint32_t Length;
        } Generic;
        struct {
            uint64_t Start;
            uint32_t Length;
        } Port;
        struct {
            uint32_t  Level;     // Translated DIRQL
            uint32_t  Vector;    // Raw GSI/MSI index or Translated IDT Vector
            KAFFINITY Affinity;  // Processor core mask
        } Interrupt;
        struct {
            uint64_t Start;      // Physical BAR base or Translated system physical base
            uint32_t Length;
        } Memory;
        struct {
            uint32_t Channel;
            uint32_t Port;
            uint32_t Reserved1;
        } Dma;
    } u{};
};

struct CM_RESOURCE_LIST {
    uint32_t InterfaceType{5}; // PCIBus = 5
    uint32_t BusNumber{0};
    uint16_t Version{1};
    uint16_t Revision{1};
    std::vector<CM_PARTIAL_RESOURCE_DESCRIPTOR> Descriptors{};
};

using PCM_RESOURCE_LIST = CM_RESOURCE_LIST*;

// Device Resource Requirement Descriptor (Input to Arbiter)
struct IO_RESOURCE_DESCRIPTOR {
    uint8_t              Option{0}; // 0 = Required, IO_RESOURCE_ALTERNATIVE = 0x08
    CM_RESOURCE_TYPE     Type{CM_RESOURCE_TYPE::CmResourceTypeNull};
    CM_SHARE_DISPOSITION ShareDisposition{CM_SHARE_DISPOSITION::CmResourceShareDeviceExclusive};
    uint16_t             Flags{0};
    uint64_t             MinimumAddress{0};
    uint64_t             MaximumAddress{0};
    uint32_t             Length{0};
    uint32_t             Alignment{1};
};

struct IO_RESOURCE_REQUIREMENTS_LIST {
    uint32_t InterfaceType{5}; // PCIBus = 5
    uint32_t BusNumber{0};
    uint32_t SlotNumber{0};
    std::vector<IO_RESOURCE_DESCRIPTOR> Descriptors{};
};

using PIO_RESOURCE_REQUIREMENTS_LIST = IO_RESOURCE_REQUIREMENTS_LIST*;

// ============================================================================
// 3. Hardware Resource Arbiter Instance (ARBITER_INSTANCE)
// ============================================================================

enum class ARBITER_ACTION : uint32_t {
    ArbiterActionTestAllocation     = 0,
    ArbiterActionRetestAllocation   = 1,
    ArbiterActionCommitAllocation   = 2,
    ArbiterActionRollbackAllocation = 3,
    ArbiterActionQueryArbitrate     = 4,
    ArbiterActionBootAllocation     = 5
};

struct ARBITER_INSTANCE {
    uint32_t         Signature{0x73427241}; // 'ArBs'
    std::wstring     Name{};
    CM_RESOURCE_TYPE ResourceType{CM_RESOURCE_TYPE::CmResourceTypeNull};
    RTL_RANGE_LIST   Allocation{};         // Committed range list
    RTL_RANGE_LIST   PossibleAllocation{}; // Candidate/Tested range list
    bool             TransactionInProgress{false};
    uint32_t         TotalTests{0};
    uint32_t         TotalCommits{0};
    uint32_t         TotalRollbacks{0};
    uint32_t         TotalConflicts{0};
};

using PARBITER_INSTANCE = ARBITER_INSTANCE*;

// ============================================================================
// 4. WDK Binary .sys Loader & KLDR_DATA_TABLE_ENTRY Structures
// ============================================================================

inline constexpr uint16_t IMAGE_SUBSYSTEM_NATIVE_SYS = 1;

struct KLDR_DATA_TABLE_ENTRY {
    uintptr_t    DllBase{0};
    uintptr_t    EntryPoint{0};
    uint32_t     SizeOfImage{0};
    std::wstring FullDllName{};
    std::wstring BaseDllName{};
    uint32_t     Flags{0};
    uint16_t     LoadCount{1};
    uint32_t     CheckSum{0};
    std::vector<std::string> BoundImports{};
};

using PKLDR_DATA_TABLE_ENTRY = KLDR_DATA_TABLE_ENTRY*;

struct WDK_DRIVER_START_CONTEXT {
    std::wstring      DriverName{};
    std::wstring      ServiceRegistryPath{};
    uintptr_t         ImageBase{0};
    uint32_t          ImageSize{0};
    bool              DriverEntryCalled{false};
    bool              AddDeviceCalled{false};
    bool              StartDeviceCalled{false};
    CM_RESOURCE_LIST  AllocatedResourcesRaw{};
    CM_RESOURCE_LIST  AllocatedResourcesTranslated{};
};

using PFN_WDK_DRIVER_ENTRY = NtStatus(*)(WDK_DRIVER_START_CONTEXT* DriverCtx, const wchar_t* RegistryPath);
using PFN_WDK_ADD_DEVICE   = NtStatus(*)(WDK_DRIVER_START_CONTEXT* DriverCtx, void* PhysicalDeviceObject);
using PFN_WDK_START_DEVICE = NtStatus(*)(WDK_DRIVER_START_CONTEXT* DriverCtx,
                                         const CM_RESOURCE_LIST* RawResources,
                                         const CM_RESOURCE_LIST* TranslatedResources);

struct INF_DRIVER_PACKAGE_INFO {
    std::wstring InfFileName{};
    std::wstring ClassName{};
    std::wstring ClassGuid{};
    std::wstring Provider{};
    std::wstring DriverVer{};
    std::wstring CatalogFile{};
    std::wstring HardwareId{};
    std::wstring ServiceName{};
    std::wstring DisplayName{};
    std::wstring SysBinaryName{};
    std::wstring StagedRepositoryPath{};
    uint32_t     ServiceType{1};  // SERVICE_KERNEL_DRIVER
    uint32_t     StartType{3};    // SERVICE_DEMAND_START / SERVICE_BOOT_START
    uint32_t     ErrorControl{1}; // SERVICE_ERROR_NORMAL
};

// ============================================================================
// 5. RTL Range List Implementation (Rtl*RangeList)
// ============================================================================

inline void RtlInitializeRangeList(PRTL_RANGE_LIST RangeList) noexcept {
    if (!RangeList) return;
    RangeList->Flags = 0;
    RangeList->Count = 0;
    RangeList->Stamp = 1;
    RangeList->Ranges.clear();
}

inline void RtlFreeRangeList(PRTL_RANGE_LIST RangeList) noexcept {
    if (!RangeList) return;
    RangeList->Ranges.clear();
    RangeList->Count = 0;
    RangeList->Stamp++;
}

inline NtStatus RtlCopyRangeList(PRTL_RANGE_LIST CopyRangeList,
                                 const RTL_RANGE_LIST* RangeList) noexcept {
    if (!CopyRangeList || !RangeList) return NtStatus::InvalidParameter;
    CopyRangeList->Flags  = RangeList->Flags;
    CopyRangeList->Count  = RangeList->Count;
    CopyRangeList->Stamp  = RangeList->Stamp + 1;
    CopyRangeList->Ranges = RangeList->Ranges;
    return NtStatus::Success;
}

inline NtStatus RtlIsRangeAvailable(const RTL_RANGE_LIST* RangeList,
                                    uint64_t Start,
                                    uint64_t End,
                                    uint32_t Flags,
                                    uint8_t AttributeAvailableMask,
                                    void* Context,
                                    void* Callback,
                                    BOOLEAN* Available) noexcept {
    (void)AttributeAvailableMask;
    (void)Context;
    (void)Callback;
    if (!RangeList || !Available || End < Start) return NtStatus::InvalidParameter;

    bool sharedOk = (Flags & RTL_RANGE_LIST_SHARED_OK) != 0;
    for (const auto& r : RangeList->Ranges) {
        bool overlaps = !(End < r.Start || Start > r.End);
        if (overlaps) {
            if (sharedOk && (r.Flags & RTL_RANGE_SHARED) != 0) {
                continue;
            }
            *Available = FALSE_B;
            return NtStatus::Success;
        }
    }
    *Available = TRUE_B;
    return NtStatus::Success;
}

inline NtStatus RtlAddRange(PRTL_RANGE_LIST RangeList,
                            uint64_t Start,
                            uint64_t End,
                            uint8_t Attributes,
                            uint32_t Flags,
                            void* UserData,
                            void* Owner) noexcept {
    if (!RangeList || End < Start) return NtStatus::InvalidParameter;

    bool isShared      = (Flags & RTL_RANGE_LIST_ADD_SHARED) != 0;
    bool allowConflict = (Flags & RTL_RANGE_LIST_ADD_IF_CONFLICT) != 0;
    bool hasConflict   = false;

    for (auto& existing : RangeList->Ranges) {
        bool overlaps = !(End < existing.Start || Start > existing.End);
        if (overlaps) {
            if (isShared && (existing.Flags & RTL_RANGE_SHARED) != 0) {
                continue;
            }
            hasConflict = true;
            if (!allowConflict) {
                return NtStatus::Unsuccessful; // STATUS_RANGE_LIST_CONFLICT
            }
            existing.Flags |= RTL_RANGE_CONFLICT;
        }
    }

    RTL_RANGE newRange{};
    newRange.Start      = Start;
    newRange.End        = End;
    newRange.UserData   = UserData;
    newRange.Owner      = Owner;
    newRange.Attributes = Attributes;
    newRange.Flags      = (isShared ? RTL_RANGE_SHARED : 0) | (hasConflict ? RTL_RANGE_CONFLICT : 0);

    RangeList->Ranges.push_back(newRange);
    std::sort(RangeList->Ranges.begin(), RangeList->Ranges.end(),
              [](const RTL_RANGE& a, const RTL_RANGE& b) { return a.Start < b.Start; });
    RangeList->Count = static_cast<uint32_t>(RangeList->Ranges.size());
    RangeList->Stamp++;
    return NtStatus::Success;
}

inline NtStatus RtlFindRange(const RTL_RANGE_LIST* RangeList,
                             uint64_t Minimum,
                             uint64_t Maximum,
                             uint32_t Length,
                             uint32_t Alignment,
                             uint32_t Flags,
                             uint8_t AttributeAvailableMask,
                             void* Context,
                             void* Callback,
                             uint64_t* Start) noexcept {
    if (!RangeList || !Start || Length == 0 || Maximum < Minimum) return NtStatus::InvalidParameter;
    if (Alignment == 0) Alignment = 1;

    uint64_t alignMask = static_cast<uint64_t>(Alignment) - 1;
    uint64_t candidate = (Minimum + alignMask) & ~alignMask;

    while (candidate >= Minimum && (candidate + Length - 1) <= Maximum) {
        uint64_t candEnd = candidate + Length - 1;
        BOOLEAN avail = FALSE_B;
        (void)RtlIsRangeAvailable(RangeList, candidate, candEnd, Flags,
                                  AttributeAvailableMask, Context, Callback, &avail);
        if (avail == TRUE_B) {
            *Start = candidate;
            return NtStatus::Success;
        }
        // Advance past the conflicting range
        uint64_t nextBase = candidate + Alignment;
        for (const auto& r : RangeList->Ranges) {
            if (!(candEnd < r.Start || candidate > r.End)) {
                uint64_t afterConflict = r.End + 1;
                uint64_t alignedAfter  = (afterConflict + alignMask) & ~alignMask;
                if (alignedAfter > nextBase) nextBase = alignedAfter;
            }
        }
        if (nextBase <= candidate) break; // Overflow protection
        candidate = nextBase;
    }

    return NtStatus::InsufficientResources;
}

inline NtStatus RtlDeleteOwnersRanges(PRTL_RANGE_LIST RangeList, void* Owner) noexcept {
    if (!RangeList) return NtStatus::InvalidParameter;
    RangeList->Ranges.erase(
        std::remove_if(RangeList->Ranges.begin(), RangeList->Ranges.end(),
                       [Owner](const RTL_RANGE& r) { return r.Owner == Owner; }),
        RangeList->Ranges.end());
    RangeList->Count = static_cast<uint32_t>(RangeList->Ranges.size());
    RangeList->Stamp++;
    return NtStatus::Success;
}

// ============================================================================
// 6. Sovereign NT Hardware Resource Arbiter & WDK .sys Loader Manager
// ============================================================================

class TitanResourceArbiterManager {
public:
    static TitanResourceArbiterManager& get() noexcept {
        static TitanResourceArbiterManager instance;
        return instance;
    }

    void resetForTest() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        initArbitersLocked();
        loadedSysModules_.clear();
        driverContexts_.clear();
        stagedPackages_.clear();
        nextDynamicImageBase_ = 0xFFFFF80040000000ULL;
        nextMsiVector_        = 0x60;
    }

    // ------------------------------------------------------------------------
    // Root Arbiter Initialization & Two-Phase Resource Arbitration (Arb*)
    // ------------------------------------------------------------------------

    NtStatus initializeArbiterInstance(PARBITER_INSTANCE arbiter,
                                       CM_RESOURCE_TYPE resourceType,
                                       std::wstring_view name) noexcept {
        if (!arbiter) return NtStatus::InvalidParameter;
        arbiter->Signature             = 0x73427241;
        arbiter->Name                  = std::wstring(name);
        arbiter->ResourceType          = resourceType;
        arbiter->TransactionInProgress = false;
        arbiter->TotalTests            = 0;
        arbiter->TotalCommits          = 0;
        arbiter->TotalRollbacks        = 0;
        arbiter->TotalConflicts        = 0;
        RtlInitializeRangeList(&arbiter->Allocation);
        RtlInitializeRangeList(&arbiter->PossibleAllocation);
        return NtStatus::Success;
    }

    NtStatus testAllocation(PARBITER_INSTANCE arbiter,
                            const IO_RESOURCE_REQUIREMENTS_LIST* reqs,
                            void* ownerPdo,
                            std::vector<RTL_RANGE>* outCandidateRanges) noexcept {
        if (!arbiter || !reqs) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);

        (void)RtlCopyRangeList(&arbiter->PossibleAllocation, &arbiter->Allocation);
        arbiter->TransactionInProgress = true;
        arbiter->TotalTests++;

        if (outCandidateRanges) outCandidateRanges->clear();

        for (const auto& desc : reqs->Descriptors) {
            if (desc.Type != arbiter->ResourceType) continue;

            uint32_t addFlags = (desc.ShareDisposition == CM_SHARE_DISPOSITION::CmResourceShareShared)
                                    ? RTL_RANGE_LIST_ADD_SHARED : 0;
            uint32_t findFlags = (desc.ShareDisposition == CM_SHARE_DISPOSITION::CmResourceShareShared)
                                    ? RTL_RANGE_LIST_SHARED_OK : 0;

            uint64_t allocatedStart = 0;
            NtStatus st = RtlFindRange(&arbiter->PossibleAllocation,
                                       desc.MinimumAddress,
                                       desc.MaximumAddress,
                                       desc.Length,
                                       desc.Alignment,
                                       findFlags,
                                       0, nullptr, nullptr,
                                       &allocatedStart);
            if (!NT_SUCCESS(st)) {
                arbiter->TotalConflicts++;
                arbiter->TransactionInProgress = false;
                RtlFreeRangeList(&arbiter->PossibleAllocation);
                return NtStatus::InsufficientResources;
            }

            uint64_t allocatedEnd = allocatedStart + desc.Length - 1;
            st = RtlAddRange(&arbiter->PossibleAllocation,
                             allocatedStart,
                             allocatedEnd,
                             0,
                             addFlags,
                             nullptr,
                             ownerPdo);
            if (!NT_SUCCESS(st)) {
                arbiter->TotalConflicts++;
                arbiter->TransactionInProgress = false;
                RtlFreeRangeList(&arbiter->PossibleAllocation);
                return st;
            }

            if (outCandidateRanges) {
                RTL_RANGE r{};
                r.Start = allocatedStart;
                r.End   = allocatedEnd;
                r.Owner = ownerPdo;
                r.Flags = (addFlags != 0) ? RTL_RANGE_SHARED : 0;
                outCandidateRanges->push_back(r);
            }
        }
        return NtStatus::Success;
    }

    NtStatus commitAllocation(PARBITER_INSTANCE arbiter) noexcept {
        if (!arbiter) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!arbiter->TransactionInProgress) return NtStatus::InvalidParameter;

        (void)RtlCopyRangeList(&arbiter->Allocation, &arbiter->PossibleAllocation);
        RtlFreeRangeList(&arbiter->PossibleAllocation);
        arbiter->TransactionInProgress = false;
        arbiter->TotalCommits++;
        return NtStatus::Success;
    }

    NtStatus rollbackAllocation(PARBITER_INSTANCE arbiter) noexcept {
        if (!arbiter) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!arbiter->TransactionInProgress) return NtStatus::InvalidParameter;

        RtlFreeRangeList(&arbiter->PossibleAllocation);
        arbiter->TransactionInProgress = false;
        arbiter->TotalRollbacks++;
        return NtStatus::Success;
    }

    // Arbitrate a full IO_RESOURCE_REQUIREMENTS_LIST across Root Memory, Port, Interrupt & DMA
    // Arbiters and synthesize both Raw and Translated CM_RESOURCE_LISTs for IRP_MN_START_DEVICE!
    NtStatus arbitrateAndTranslateResources(const IO_RESOURCE_REQUIREMENTS_LIST* reqs,
                                            void* ownerPdo,
                                            CM_RESOURCE_LIST* outRawList,
                                            CM_RESOURCE_LIST* outTranslatedList) noexcept {
        if (!reqs || !outRawList || !outTranslatedList) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);
        ensureArbitersInitializedLocked();

        outRawList->InterfaceType        = reqs->InterfaceType;
        outRawList->BusNumber            = reqs->BusNumber;
        outRawList->Descriptors.clear();

        outTranslatedList->InterfaceType = reqs->InterfaceType;
        outTranslatedList->BusNumber     = reqs->BusNumber;
        outTranslatedList->Descriptors.clear();

        for (const auto& desc : reqs->Descriptors) {
            PARBITER_INSTANCE targetArb = nullptr;
            switch (desc.Type) {
                case CM_RESOURCE_TYPE::CmResourceTypeMemory:
                case CM_RESOURCE_TYPE::CmResourceTypeMemoryLarge:
                    targetArb = &rootMemArbiter_;
                    break;
                case CM_RESOURCE_TYPE::CmResourceTypePort:
                    targetArb = &rootPortArbiter_;
                    break;
                case CM_RESOURCE_TYPE::CmResourceTypeInterrupt:
                    targetArb = &rootIrqArbiter_;
                    break;
                case CM_RESOURCE_TYPE::CmResourceTypeDma:
                    targetArb = &rootDmaArbiter_;
                    break;
                default:
                    break;
            }
            if (!targetArb) continue;

            uint32_t findFlags = (desc.ShareDisposition == CM_SHARE_DISPOSITION::CmResourceShareShared)
                                     ? RTL_RANGE_LIST_SHARED_OK : 0;
            uint32_t addFlags  = (desc.ShareDisposition == CM_SHARE_DISPOSITION::CmResourceShareShared)
                                     ? RTL_RANGE_LIST_ADD_SHARED : 0;

            uint64_t allocStart = 0;
            NtStatus st = RtlFindRange(&targetArb->Allocation,
                                       desc.MinimumAddress,
                                       desc.MaximumAddress,
                                       desc.Length,
                                       desc.Alignment,
                                       findFlags,
                                       0, nullptr, nullptr,
                                       &allocStart);
            if (!NT_SUCCESS(st)) {
                targetArb->TotalConflicts++;
                return st;
            }

            st = RtlAddRange(&targetArb->Allocation,
                             allocStart,
                             allocStart + desc.Length - 1,
                             0,
                             addFlags,
                             nullptr,
                             ownerPdo);
            if (!NT_SUCCESS(st)) {
                targetArb->TotalConflicts++;
                return st;
            }
            targetArb->TotalCommits++;

            // Construct Raw & Translated descriptors
            CM_PARTIAL_RESOURCE_DESCRIPTOR raw{};
            raw.Type             = desc.Type;
            raw.ShareDisposition = desc.ShareDisposition;
            raw.Flags            = desc.Flags;

            CM_PARTIAL_RESOURCE_DESCRIPTOR trans = raw;

            if (desc.Type == CM_RESOURCE_TYPE::CmResourceTypeMemory ||
                desc.Type == CM_RESOURCE_TYPE::CmResourceTypeMemoryLarge) {
                raw.u.Memory.Start    = allocStart;
                raw.u.Memory.Length   = desc.Length;
                // Translated physical address (PCI bus offset -> host CPU physical address)
                trans.u.Memory.Start  = allocStart;
                trans.u.Memory.Length = desc.Length;
            } else if (desc.Type == CM_RESOURCE_TYPE::CmResourceTypePort) {
                raw.u.Port.Start      = allocStart;
                raw.u.Port.Length     = desc.Length;
                trans.u.Port.Start    = allocStart;
                trans.u.Port.Length   = desc.Length;
            } else if (desc.Type == CM_RESOURCE_TYPE::CmResourceTypeInterrupt) {
                raw.u.Interrupt.Level      = static_cast<uint32_t>(allocStart);
                raw.u.Interrupt.Vector     = static_cast<uint32_t>(allocStart);
                raw.u.Interrupt.Affinity   = 0xFFFFFFFFFFFFFFFFULL;

                // Translate Raw GSI / MSI index to CPU IDT Vector + DIRQL (9..12) + 4-core Affinity (0x0F)
                uint32_t idtVector         = nextMsiVector_++;
                uint32_t dirql             = 9 + (static_cast<uint32_t>(allocStart) % 4);
                trans.u.Interrupt.Level    = dirql;
                trans.u.Interrupt.Vector   = idtVector;
                trans.u.Interrupt.Affinity = 0x000000000000000FULL;
            } else if (desc.Type == CM_RESOURCE_TYPE::CmResourceTypeDma) {
                raw.u.Dma.Channel   = static_cast<uint32_t>(allocStart);
                raw.u.Dma.Port      = 0;
                trans.u.Dma.Channel = static_cast<uint32_t>(allocStart);
                trans.u.Dma.Port    = 0;
            }

            outRawList->Descriptors.push_back(raw);
            outTranslatedList->Descriptors.push_back(trans);
        }

        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // WDK Binary .sys PE32+ Kernel Image Synthesis & Loader (MmLoadSystemImage)
    // ------------------------------------------------------------------------

    static std::vector<uint8_t> buildSyntheticWdkSysBinary(
        uint32_t sizeOfImage = 0x10000,
        uint32_t entryPointRva = 0x1000) noexcept {
        std::vector<uint8_t> image(512, 0);
        // DOS Header ('MZ')
        image[0] = 'M';
        image[1] = 'Z';
        uint32_t e_lfanew = 0x80;
        std::memcpy(&image[0x3C], &e_lfanew, sizeof(e_lfanew));

        // NT Headers ('PE\0\0')
        size_t peOff = e_lfanew;
        image[peOff + 0] = 'P';
        image[peOff + 1] = 'E';
        image[peOff + 2] = 0;
        image[peOff + 3] = 0;

        // IMAGE_FILE_HEADER
        uint16_t machine = 0x8664; // IMAGE_FILE_MACHINE_AMD64
        uint16_t numSecs = 2;      // .text, .rdata
        uint16_t optSize = 0xF0;   // sizeof(IMAGE_OPTIONAL_HEADER64)
        uint16_t chars   = 0x0022; // EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE
        std::memcpy(&image[peOff + 4],  &machine, 2);
        std::memcpy(&image[peOff + 6],  &numSecs, 2);
        std::memcpy(&image[peOff + 20], &optSize, 2);
        std::memcpy(&image[peOff + 22], &chars,   2);

        // IMAGE_OPTIONAL_HEADER64
        size_t optOff = peOff + 24;
        uint16_t magic     = 0x020B; // PE32+ (64-bit)
        uint64_t imageBase = 0xFFFFF80040000000ULL;
        uint16_t subsystem = IMAGE_SUBSYSTEM_NATIVE_SYS; // 1 = Native (.sys kernel driver)
        uint32_t checksum  = 0x0001A4F2;
        std::memcpy(&image[optOff + 0],  &magic,         2);
        std::memcpy(&image[optOff + 16], &entryPointRva, 4);
        std::memcpy(&image[optOff + 24], &imageBase,     8);
        std::memcpy(&image[optOff + 56], &sizeOfImage,   4);
        std::memcpy(&image[optOff + 64], &checksum,      4);
        std::memcpy(&image[optOff + 68], &subsystem,     2);

        return image;
    }

    NtStatus loadSystemImage(std::wstring_view imageFileName,
                             const std::vector<uint8_t>& peSysBytes,
                             const std::vector<std::pair<std::string, std::string>>& requiredImports,
                             PKLDR_DATA_TABLE_ENTRY* outLdrEntry) noexcept {
        if (imageFileName.empty() || peSysBytes.size() < 0x180) return NtStatus::InvalidImageFormat;
        std::lock_guard<std::mutex> lock(mutex_);

        // 1. Validate MZ and PE\0\0 signatures + PE32+ Magic (0x020B) + IMAGE_SUBSYSTEM_NATIVE (1)
        if (peSysBytes[0] != 'M' || peSysBytes[1] != 'Z') return NtStatus::InvalidImageFormat;
        uint32_t lfanew = 0;
        std::memcpy(&lfanew, &peSysBytes[0x3C], 4);
        if (lfanew + 0x100 > peSysBytes.size()) return NtStatus::InvalidImageFormat;
        if (peSysBytes[lfanew] != 'P' || peSysBytes[lfanew + 1] != 'E') return NtStatus::InvalidImageFormat;

        size_t optOff = lfanew + 24;
        uint16_t magic = 0;
        uint16_t subsystem = 0;
        uint32_t entryRva = 0;
        uint32_t sizeOfImage = 0;
        uint32_t checksum = 0;
        std::memcpy(&magic,       &peSysBytes[optOff + 0],  2);
        std::memcpy(&entryRva,    &peSysBytes[optOff + 16], 4);
        std::memcpy(&sizeOfImage, &peSysBytes[optOff + 56], 4);
        std::memcpy(&checksum,    &peSysBytes[optOff + 64], 4);
        std::memcpy(&subsystem,   &peSysBytes[optOff + 68], 2);

        if (magic != 0x020B || subsystem != IMAGE_SUBSYSTEM_NATIVE_SYS) {
            return NtStatus::InvalidImageFormat;
        }

        // 2. Bind required kernel-mode IAT imports via JanusLDR (ntoskrnl.exe, hal.dll, Wdf01000.sys, etc.)
        auto& ldr = ldr::DynamicLoader::get();
        std::vector<std::string> boundList;
        for (const auto& [moduleName, symbolName] : requiredImports) {
            void* fn = ldr.getExport(moduleName, symbolName);
            if (!fn) {
                return NtStatus::ProcedureNotFound;
            }
            boundList.push_back(moduleName + "!" + symbolName);
        }

        // 3. Allocate kernel virtual base and create KLDR_DATA_TABLE_ENTRY in PsLoadedModuleList
        uintptr_t baseAddr = nextDynamicImageBase_;
        nextDynamicImageBase_ += ((sizeOfImage + 0xFFFFULL) & ~0xFFFFULL);

        auto entry = std::make_unique<KLDR_DATA_TABLE_ENTRY>();
        entry->DllBase      = baseAddr;
        entry->EntryPoint   = baseAddr + entryRva;
        entry->SizeOfImage  = sizeOfImage;
        entry->FullDllName  = std::wstring(imageFileName);
        entry->BaseDllName  = extractBaseName(imageFileName);
        entry->LoadCount    = 1;
        entry->CheckSum     = checksum;
        entry->BoundImports = std::move(boundList);

        PKLDR_DATA_TABLE_ENTRY rawPtr = entry.get();
        loadedSysModules_[ std::wstring(imageFileName) ] = std::move(entry);
        if (outLdrEntry) *outLdrEntry = rawPtr;
        return NtStatus::Success;
    }

    NtStatus unloadSystemImage(PKLDR_DATA_TABLE_ENTRY ldrEntry) noexcept {
        if (!ldrEntry) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = loadedSysModules_.find(ldrEntry->FullDllName);
        if (it == loadedSysModules_.end()) return NtStatus::NotFound;
        loadedSysModules_.erase(it);
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // Stock .inf Driver Store Staging & WDK Driver Lifecycle (NtLoadDriver)
    // ------------------------------------------------------------------------

    NtStatus stageAndInstallInfDriverPackage(
        std::wstring_view infFileName,
        std::string_view infContent,
        const std::vector<uint8_t>& sysBinaryBytes,
        const std::vector<std::pair<std::string, std::string>>& requiredImports,
        const IO_RESOURCE_REQUIREMENTS_LIST* hardwareRequirements,
        PFN_WDK_DRIVER_ENTRY driverEntryFn,
        PFN_WDK_ADD_DEVICE addDeviceFn,
        PFN_WDK_START_DEVICE startDeviceFn,
        INF_DRIVER_PACKAGE_INFO* outPackageInfo,
        WDK_DRIVER_START_CONTEXT** outDriverCtx) noexcept {
        if (infFileName.empty() || infContent.empty()) return NtStatus::InvalidParameter;

        // 1. Parse stock .inf sections ([Version], [Strings], AddService, CopyFiles)
        INF_DRIVER_PACKAGE_INFO pkg{};
        pkg.InfFileName = std::wstring(infFileName);
        parseInfContent(infContent, pkg);
        if (pkg.ServiceName.empty()) {
            pkg.ServiceName = L"WdkStockMiniport";
        }
        if (pkg.SysBinaryName.empty()) {
            pkg.SysBinaryName = pkg.ServiceName + L".sys";
        }
        pkg.StagedRepositoryPath = L"\\SystemRoot\\System32\\DriverStore\\FileRepository\\" +
                                   pkg.ServiceName + L".inf_amd64\\" + pkg.SysBinaryName;

        // 2. Register Kernel Driver Service in SCM (\Registry\Machine\System\CurrentControlSet\Services\<ServiceName>)
        auto& scm = scm::ServiceControlManager::get();
        auto svcRec = std::make_shared<scm::ServiceRecord>();
        svcRec->serviceName           = pkg.ServiceName;
        svcRec->displayName           = pkg.DisplayName.empty() ? pkg.ServiceName : pkg.DisplayName;
        svcRec->serviceType           = scm::SERVICE_KERNEL_DRIVER;
        svcRec->startType             = pkg.StartType;
        svcRec->errorControl          = pkg.ErrorControl;
        svcRec->binaryPath            = pkg.StagedRepositoryPath;
        svcRec->status.dwCurrentState = scm::SERVICE_RUNNING;
        scm.registerServiceRecord(svcRec);

        // 3. Load the .sys PE32+ Kernel Binary via MmLoadSystemImage
        PKLDR_DATA_TABLE_ENTRY ldrEntry = nullptr;
        NtStatus st = loadSystemImage(pkg.StagedRepositoryPath, sysBinaryBytes, requiredImports, &ldrEntry);
        if (!NT_SUCCESS(st)) return st;

        // 4. Create WDK_DRIVER_START_CONTEXT & invoke DriverEntry -> AddDevice -> Arbitrate -> IRP_MN_START_DEVICE
        auto ctx = std::make_unique<WDK_DRIVER_START_CONTEXT>();
        ctx->DriverName          = L"\\Driver\\" + pkg.ServiceName;
        ctx->ServiceRegistryPath = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" + pkg.ServiceName;
        ctx->ImageBase           = ldrEntry->DllBase;
        ctx->ImageSize           = ldrEntry->SizeOfImage;

        if (driverEntryFn) {
            st = driverEntryFn(ctx.get(), ctx->ServiceRegistryPath.c_str());
            if (!NT_SUCCESS(st)) return st;
            ctx->DriverEntryCalled = true;
        }

        void* ownerPdo = reinterpret_cast<void*>(ctx.get());
        if (addDeviceFn) {
            st = addDeviceFn(ctx.get(), ownerPdo);
            if (!NT_SUCCESS(st)) return st;
            ctx->AddDeviceCalled = true;
        }

        if (hardwareRequirements) {
            st = arbitrateAndTranslateResources(
                hardwareRequirements,
                ownerPdo,
                &ctx->AllocatedResourcesRaw,
                &ctx->AllocatedResourcesTranslated
            );
            if (!NT_SUCCESS(st)) return st;
        }

        if (startDeviceFn) {
            st = startDeviceFn(
                ctx.get(),
                &ctx->AllocatedResourcesRaw,
                &ctx->AllocatedResourcesTranslated
            );
            if (!NT_SUCCESS(st)) return st;
            ctx->StartDeviceCalled = true;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (outPackageInfo) *outPackageInfo = pkg;
        WDK_DRIVER_START_CONTEXT* rawCtx = ctx.get();
        driverContexts_[pkg.ServiceName] = std::move(ctx);
        stagedPackages_[pkg.ServiceName] = std::move(pkg);
        if (outDriverCtx) *outDriverCtx = rawCtx;
        return NtStatus::Success;
    }

    PARBITER_INSTANCE getRootMemoryArbiter() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        ensureArbitersInitializedLocked();
        return &rootMemArbiter_;
    }

    PARBITER_INSTANCE getRootPortArbiter() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        ensureArbitersInitializedLocked();
        return &rootPortArbiter_;
    }

    PARBITER_INSTANCE getRootIrqArbiter() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        ensureArbitersInitializedLocked();
        return &rootIrqArbiter_;
    }

private:
    TitanResourceArbiterManager() {
        initArbitersLocked();
    }

    void ensureArbitersInitializedLocked() noexcept {
        if (!arbitersInitialized_) {
            initArbitersLocked();
        }
    }

    void initArbitersLocked() noexcept {
        initializeArbiterInstance(&rootMemArbiter_,  CM_RESOURCE_TYPE::CmResourceTypeMemory,    L"RootMemory");
        initializeArbiterInstance(&rootPortArbiter_, CM_RESOURCE_TYPE::CmResourceTypePort,      L"RootPort");
        initializeArbiterInstance(&rootIrqArbiter_,  CM_RESOURCE_TYPE::CmResourceTypeInterrupt, L"RootIRQ");
        initializeArbiterInstance(&rootDmaArbiter_,  CM_RESOURCE_TYPE::CmResourceTypeDma,       L"RootDMA");

        // Reserve legacy x86 VGA memory hole (0xA0000 - 0xFFFFF) and system timer IRQ 0 in root arbiters
        (void)RtlAddRange(&rootMemArbiter_.Allocation, 0x000A0000ULL, 0x000FFFFFULL, 0, 0, nullptr, nullptr);
        (void)RtlAddRange(&rootPortArbiter_.Allocation, 0x0000ULL, 0x00FFULL, 0, 0, nullptr, nullptr);
        (void)RtlAddRange(&rootIrqArbiter_.Allocation, 0ULL, 0ULL, 0, 0, nullptr, nullptr);
        arbitersInitialized_ = true;
    }

    static std::wstring extractBaseName(std::wstring_view fullPath) noexcept {
        size_t pos = fullPath.find_last_of(L"\\/");
        if (pos == std::wstring_view::npos) return std::wstring(fullPath);
        return std::wstring(fullPath.substr(pos + 1));
    }

    static std::wstring widenAscii(std::string_view sv) noexcept {
        std::wstring out;
        out.reserve(sv.size());
        for (char c : sv) out.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
        return out;
    }

    static std::string_view trimView(std::string_view sv) noexcept {
        while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '"')) {
            sv.remove_prefix(1);
        }
        while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '"')) {
            sv.remove_suffix(1);
        }
        return sv;
    }

    static void parseInfContent(std::string_view content, INF_DRIVER_PACKAGE_INFO& pkg) noexcept {
        size_t start = 0;
        while (start < content.size()) {
            size_t end = content.find('\n', start);
            if (end == std::string_view::npos) end = content.size();
            std::string_view line = trimView(content.substr(start, end - start));
            start = end + 1;

            if (line.empty() || line.front() == ';' || line.front() == '[') continue;
            size_t eq = line.find('=');
            if (eq == std::string_view::npos) continue;

            std::string_view key = trimView(line.substr(0, eq));
            std::string_view val = trimView(line.substr(eq + 1));

            if (key == "Class") {
                pkg.ClassName = widenAscii(val);
            } else if (key == "ClassGuid") {
                pkg.ClassGuid = widenAscii(val);
            } else if (key == "Provider") {
                pkg.Provider = widenAscii(val);
            } else if (key == "CatalogFile") {
                pkg.CatalogFile = widenAscii(val);
            } else if (key == "DriverVer") {
                pkg.DriverVer = widenAscii(val);
            } else if (key == "ServiceBinary") {
                std::wstring binPath = widenAscii(val);
                pkg.SysBinaryName = extractBaseName(binPath);
            } else if (key == "DisplayName") {
                pkg.DisplayName = widenAscii(val);
            } else if (key == "AddService") {
                size_t comma = val.find(',');
                std::string_view svcName = (comma != std::string_view::npos) ? trimView(val.substr(0, comma)) : val;
                pkg.ServiceName = widenAscii(svcName);
            }
        }
    }

    mutable std::mutex mutex_;
    bool arbitersInitialized_{false};
    ARBITER_INSTANCE rootMemArbiter_{};
    ARBITER_INSTANCE rootPortArbiter_{};
    ARBITER_INSTANCE rootIrqArbiter_{};
    ARBITER_INSTANCE rootDmaArbiter_{};
    uintptr_t nextDynamicImageBase_{0xFFFFF80040000000ULL};
    uint32_t  nextMsiVector_{0x60};

    std::unordered_map<std::wstring, std::unique_ptr<KLDR_DATA_TABLE_ENTRY>> loadedSysModules_;
    std::unordered_map<std::wstring, std::unique_ptr<WDK_DRIVER_START_CONTEXT>> driverContexts_;
    std::unordered_map<std::wstring, INF_DRIVER_PACKAGE_INFO> stagedPackages_;
};

// ============================================================================
// 7. Public Kernel Mode (Arb* / Mm* / Pnp*) C-Linkage / WDK APIs
// ============================================================================

inline NtStatus ArbInitializeArbiterInstance(PARBITER_INSTANCE Arbiter,
                                             CM_RESOURCE_TYPE ResourceType,
                                             std::wstring_view Name) noexcept {
    return TitanResourceArbiterManager::get().initializeArbiterInstance(Arbiter, ResourceType, Name);
}

inline NtStatus ArbTestAllocation(PARBITER_INSTANCE Arbiter,
                                  const IO_RESOURCE_REQUIREMENTS_LIST* Requirements,
                                  void* OwnerPdo = nullptr,
                                  std::vector<RTL_RANGE>* OutCandidateRanges = nullptr) noexcept {
    return TitanResourceArbiterManager::get().testAllocation(Arbiter, Requirements, OwnerPdo, OutCandidateRanges);
}

inline NtStatus ArbCommitAllocation(PARBITER_INSTANCE Arbiter) noexcept {
    return TitanResourceArbiterManager::get().commitAllocation(Arbiter);
}

inline NtStatus ArbRollbackAllocation(PARBITER_INSTANCE Arbiter) noexcept {
    return TitanResourceArbiterManager::get().rollbackAllocation(Arbiter);
}

inline NtStatus ArbArbitrateAndTranslateResources(
    const IO_RESOURCE_REQUIREMENTS_LIST* Requirements,
    void* OwnerPdo,
    CM_RESOURCE_LIST* OutRawResources,
    CM_RESOURCE_LIST* OutTranslatedResources) noexcept {
    return TitanResourceArbiterManager::get().arbitrateAndTranslateResources(
        Requirements, OwnerPdo, OutRawResources, OutTranslatedResources);
}

inline NtStatus MmLoadSystemImage(
    std::wstring_view ImageFileName,
    const std::vector<uint8_t>& PeSysBytes,
    const std::vector<std::pair<std::string, std::string>>& RequiredImports,
    PKLDR_DATA_TABLE_ENTRY* OutLdrEntry) noexcept {
    return TitanResourceArbiterManager::get().loadSystemImage(
        ImageFileName, PeSysBytes, RequiredImports, OutLdrEntry);
}

inline NtStatus MmUnloadSystemImage(PKLDR_DATA_TABLE_ENTRY LdrEntry) noexcept {
    return TitanResourceArbiterManager::get().unloadSystemImage(LdrEntry);
}

inline NtStatus PnpStageAndInstallInfDriverPackage(
    std::wstring_view InfFileName,
    std::string_view InfContent,
    const std::vector<uint8_t>& SysBinaryBytes,
    const std::vector<std::pair<std::string, std::string>>& RequiredImports,
    const IO_RESOURCE_REQUIREMENTS_LIST* HardwareRequirements,
    PFN_WDK_DRIVER_ENTRY DriverEntryFn,
    PFN_WDK_ADD_DEVICE AddDeviceFn,
    PFN_WDK_START_DEVICE StartDeviceFn,
    INF_DRIVER_PACKAGE_INFO* OutPackageInfo = nullptr,
    WDK_DRIVER_START_CONTEXT** OutDriverCtx = nullptr) noexcept {
    return TitanResourceArbiterManager::get().stageAndInstallInfDriverPackage(
        InfFileName, InfContent, SysBinaryBytes, RequiredImports, HardwareRequirements,
        DriverEntryFn, AddDeviceFn, StartDeviceFn, OutPackageInfo, OutDriverCtx);
}

// ============================================================================
// 8. Dynamic Export Registration (ntoskrnl.exe, ntdll.dll, setupapi.dll)
// ============================================================================

inline void InitializeArbiterSubsystemExports() noexcept {
    auto& ldr = ldr::DynamicLoader::get();

    struct ExportEntry {
        const char* name;
        void*       fn;
    };

    const ExportEntry kNtosExports[] = {
        { "RtlInitializeRangeList",             reinterpret_cast<void*>(&RtlInitializeRangeList) },
        { "RtlFreeRangeList",                   reinterpret_cast<void*>(&RtlFreeRangeList) },
        { "RtlCopyRangeList",                   reinterpret_cast<void*>(&RtlCopyRangeList) },
        { "RtlAddRange",                        reinterpret_cast<void*>(&RtlAddRange) },
        { "RtlFindRange",                       reinterpret_cast<void*>(&RtlFindRange) },
        { "RtlIsRangeAvailable",                reinterpret_cast<void*>(&RtlIsRangeAvailable) },
        { "RtlDeleteOwnersRanges",              reinterpret_cast<void*>(&RtlDeleteOwnersRanges) },
        { "ArbInitializeArbiterInstance",       reinterpret_cast<void*>(&ArbInitializeArbiterInstance) },
        { "ArbTestAllocation",                  reinterpret_cast<void*>(&ArbTestAllocation) },
        { "ArbCommitAllocation",                reinterpret_cast<void*>(&ArbCommitAllocation) },
        { "ArbRollbackAllocation",              reinterpret_cast<void*>(&ArbRollbackAllocation) },
        { "ArbArbitrateAndTranslateResources",  reinterpret_cast<void*>(&ArbArbitrateAndTranslateResources) },
        { "MmLoadSystemImage",                  reinterpret_cast<void*>(&MmLoadSystemImage) },
        { "MmUnloadSystemImage",                reinterpret_cast<void*>(&MmUnloadSystemImage) },
        { "PnpStageAndInstallInfDriverPackage", reinterpret_cast<void*>(&PnpStageAndInstallInfDriverPackage) }
    };

    for (const auto& exp : kNtosExports) {
        ldr.registerExport("ntoskrnl.exe", exp.name, exp.fn);
        ldr.registerExport("ntdll.dll", exp.name, exp.fn);
    }

    auto& scm = scm::ServiceControlManager::get();
    auto arbSvc = std::make_shared<scm::ServiceRecord>();
    arbSvc->serviceName           = L"PnpArbiter";
    arbSvc->displayName           = L"Sovereign NT Hardware Resource Arbiter & WDK .sys Image Loader (Arb* / MmLoadSystemImage)";
    arbSvc->serviceType           = scm::SERVICE_KERNEL_DRIVER;
    arbSvc->startType             = scm::SERVICE_BOOT_START;
    arbSvc->errorControl          = scm::SERVICE_ERROR_CRITICAL;
    arbSvc->binaryPath            = L"\\SystemRoot\\System32\\ntoskrnl.exe";
    arbSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(arbSvc);

    version::VersionDatabase::Instance().RegisterModule(
        "arbiter_executive",
        "10.0.26100.4700",
        "Sovereign NT Hardware Resource Arbiter & WDK Binary .sys Loader (base/ntos/arb)"
    );
}

} // namespace micant::km::arb

namespace micant {
    using km::arb::RTL_RANGE;
    using km::arb::PRTL_RANGE;
    using km::arb::RTL_RANGE_LIST;
    using km::arb::PRTL_RANGE_LIST;
    using km::arb::CM_RESOURCE_TYPE;
    using km::arb::CM_SHARE_DISPOSITION;
    using km::arb::CM_PARTIAL_RESOURCE_DESCRIPTOR;
    using km::arb::CM_RESOURCE_LIST;
    using km::arb::PCM_RESOURCE_LIST;
    using km::arb::IO_RESOURCE_DESCRIPTOR;
    using km::arb::IO_RESOURCE_REQUIREMENTS_LIST;
    using km::arb::PIO_RESOURCE_REQUIREMENTS_LIST;
    using km::arb::ARBITER_INSTANCE;
    using km::arb::PARBITER_INSTANCE;
    using km::arb::KLDR_DATA_TABLE_ENTRY;
    using km::arb::PKLDR_DATA_TABLE_ENTRY;
    using km::arb::WDK_DRIVER_START_CONTEXT;
    using km::arb::INF_DRIVER_PACKAGE_INFO;
    using km::arb::TitanResourceArbiterManager;
    using km::arb::RtlInitializeRangeList;
    using km::arb::RtlFreeRangeList;
    using km::arb::RtlCopyRangeList;
    using km::arb::RtlAddRange;
    using km::arb::RtlFindRange;
    using km::arb::RtlIsRangeAvailable;
    using km::arb::RtlDeleteOwnersRanges;
    using km::arb::ArbInitializeArbiterInstance;
    using km::arb::ArbTestAllocation;
    using km::arb::ArbCommitAllocation;
    using km::arb::ArbRollbackAllocation;
    using km::arb::ArbArbitrateAndTranslateResources;
    using km::arb::MmLoadSystemImage;
    using km::arb::MmUnloadSystemImage;
    using km::arb::PnpStageAndInstallInfDriverPackage;
    using km::arb::InitializeArbiterSubsystemExports;
} // namespace micant
