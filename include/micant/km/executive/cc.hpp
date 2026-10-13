// ============================================================================
// MicaNT: Sovereign NT Executive Cache Manager Architecture (cc.hpp)
// Sovereign System Domain: TitanCache / SovereignCc
//
// Description:
//   Clean-room implementation of the Microsoft Windows NT Executive Cache
//   Manager (Cc* subsystem in ntoskrnl.exe). Unlike legacy physical block
//   buffer caches, the NT Cache Manager is a virtual block cache operating
//   on file streams via 256 KB Virtual Address Control Blocks (VACBs), tightly
//   integrated with the Memory Manager (Mm), Section Objects, and File System
//   Drivers (NTFS, EmeraldFS, FastFAT, ReFS).
//
//   Provides:
//     - 256 KB Virtual Address Control Block (VACB) mapping & LRU view recycling
//     - SHARED_CACHE_MAP & PRIVATE_CACHE_MAP lifecycle (CcInitializeCacheMap,
//       CcUninitializeCacheMap, CcSetFileSizes, CcGetFileSizePointer)
//     - Fast Copy I/O (CcCopyRead, CcCopyWrite, CcFastCopyRead, CcFastCopyWrite,
//       CcZeroData) with automatic cross-VACB boundary spanning
//     - Zero-Copy Direct DMA MDL Interface (CcMdlRead, CcMdlReadComplete,
//       CcPrepareMdlWrite, CcMdlWriteComplete)
//     - Metadata Pinning & Buffer Control Blocks (BCB - CcMapData, CcPinRead,
//       CcPreparePinWrite, CcPinMappedData, CcSetDirtyPinnedData with WAL LSN,
//       CcUnpinData)
//     - Intelligent Sequential Read-Ahead Engine (CcScheduleReadAhead,
//       CcSetReadAheadGranularity)
//     - Background Lazy Writer & Write-Ahead Log Flush (CcLazyWriteScan,
//       CcFlushCache, CcPurgeCacheSection, CACHE_MANAGER_CALLBACKS)
//     - Write Throttling & Deferred Write Queue (CcCanIWrite, CcDeferWrite,
//       CcSetDirtyPageThreshold)
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
#include "micant/km/executive/mm.hpp"

namespace micant::km::cc {

// ============================================================================
// 1. Standard NT Cache Manager Constants & Types
// ============================================================================

using BOOL          = int32_t;
using BOOLEAN       = uint8_t;
using USHORT        = uint16_t;
using ULONG         = uint32_t;
using PULONG        = uint32_t*;
using LONGLONG      = int64_t;
using ULONGLONG     = uint64_t;
using PVOID         = void*;
using LARGE_INTEGER = micant::LargeInteger;
using PLARGE_INTEGER= micant::LargeInteger*;
using PIO_STATUS_BLOCK = micant::IoStatusBlock*;

inline constexpr BOOL    TRUE_VAL  = 1;
inline constexpr BOOL    FALSE_VAL = 0;
inline constexpr BOOLEAN TRUE_B    = 1;
inline constexpr BOOLEAN FALSE_B   = 0;

// Canonical NT Cache Manager View Granularity (256 KB per VACB)
inline constexpr uint32_t VACB_MAPPING_GRANULARITY      = 256 * 1024; // 262,144 bytes (0x40000)
inline constexpr uint32_t VACB_OFFSET_SHIFT             = 18;         // 2^18 = 262,144
inline constexpr uint32_t DEFAULT_READ_AHEAD_GRANULARITY= 64 * 1024;  // 64 KB default read-ahead
inline constexpr uint64_t SYSTEM_CACHE_VIRTUAL_BASE     = 0xFFFFF88000000000ULL;
inline constexpr uint32_t CC_PAGE_SIZE                  = 4096;

// Pinning Flags for CcPinRead / CcMapData / CcPreparePinWrite
inline constexpr uint32_t PIN_WAIT                      = 0x00000001;
inline constexpr uint32_t PIN_EXCLUSIVE                 = 0x00000002;
inline constexpr uint32_t PIN_NO_READ                   = 0x00000004;
inline constexpr uint32_t PIN_IF_BCB                    = 0x00000008;
inline constexpr uint32_t MAP_WAIT                      = 0x00000001;
inline constexpr uint32_t MAP_NO_READ                   = 0x00000010;

// ============================================================================
// 2. WDK / IFS Cache Manager Structures
// ============================================================================

struct CC_FILE_SIZES {
    LARGE_INTEGER AllocationSize{0};
    LARGE_INTEGER FileSize{0};
    LARGE_INTEGER ValidDataLength{0};
};
using PCC_FILE_SIZES = CC_FILE_SIZES*;

// Cache Manager Synchronization Callbacks (FSD <-> Cc Contract)
using PACQUIRE_FOR_LAZY_WRITE = BOOLEAN (*)(PVOID Context, BOOLEAN Wait);
using PRELEASE_FROM_LAZY_WRITE= void (*)(PVOID Context);
using PACQUIRE_FOR_READ_AHEAD = BOOLEAN (*)(PVOID Context, BOOLEAN Wait);
using PRELEASE_FROM_READ_AHEAD= void (*)(PVOID Context);

struct CACHE_MANAGER_CALLBACKS {
    PACQUIRE_FOR_LAZY_WRITE  AcquireForLazyWrite{nullptr};
    PRELEASE_FROM_LAZY_WRITE ReleaseFromLazyWrite{nullptr};
    PACQUIRE_FOR_READ_AHEAD  AcquireForReadAhead{nullptr};
    PRELEASE_FROM_READ_AHEAD ReleaseFromReadAhead{nullptr};
};
using PCACHE_MANAGER_CALLBACKS = CACHE_MANAGER_CALLBACKS*;

// Write-Ahead Log Flush Callback (e.g. LfsFlushToLsn for NTFS / EmeraldFS)
using PFLUSH_TO_LSN = void (*)(PVOID LogHandle, LARGE_INTEGER Lsn);

// Deferred Write Callback & Context
using POST_DEFERRED_WRITE = void (*)(PVOID Context1, PVOID Context2);

struct DEFERRED_WRITE {
    int16_t             NodeTypeCode{0x02FD}; // NTC_DEFERRED_WRITE
    int16_t             NodeByteSize{sizeof(DEFERRED_WRITE)};
    PVOID               FileObject{nullptr};
    ULONG               BytesToWrite{0};
    POST_DEFERRED_WRITE PostRoutine{nullptr};
    PVOID               Context1{nullptr};
    PVOID               Context2{nullptr};
    BOOLEAN             LimitModifiedPages{TRUE_B};
};
using PDEFERRED_WRITE = DEFERRED_WRITE*;

struct CACHE_UNINITIALIZE_EVENT {
    BOOLEAN Signaled{FALSE_B};
    NtStatus Status{NtStatus::Success};
};
using PCACHE_UNINITIALIZE_EVENT = CACHE_UNINITIALIZE_EVENT*;

// Section Object Pointers inside FILE_OBJECT
struct SECTION_OBJECT_POINTERS {
    PVOID DataSectionObject{nullptr};
    PVOID SharedCacheMap{nullptr};
    PVOID ImageSectionObject{nullptr};
};
using PSECTION_OBJECT_POINTERS = SECTION_OBJECT_POINTERS*;

// Cache-enabled File Object representation
struct CC_FILE_OBJECT {
    int16_t                  Type{5}; // IO_TYPE_FILE
    int16_t                  Size{sizeof(CC_FILE_OBJECT)};
    std::wstring             FileName;
    PSECTION_OBJECT_POINTERS SectionObjectPointer{nullptr};
    PVOID                    PrivateCacheMap{nullptr};
    PVOID                    FsContext{nullptr};
    PVOID                    FsContext2{nullptr};
    LARGE_INTEGER            CurrentByteOffset{0};
    uint32_t                 Flags{0};
    SECTION_OBJECT_POINTERS  EmbeddedSectionPointers{};

    CC_FILE_OBJECT() {
        SectionObjectPointer = &EmbeddedSectionPointers;
    }
};
using PCC_FILE_OBJECT = CC_FILE_OBJECT*;

// Macro / helper equivalent to WDK CcIsFileCached(FO)
inline bool CcIsFileCached(const CC_FILE_OBJECT* fileObject) noexcept {
    return fileObject &&
           fileObject->SectionObjectPointer &&
           fileObject->SectionObjectPointer->SharedCacheMap != nullptr;
}

// ============================================================================
// 3. Virtual Address Control Block (VACB), BCB, Shared & Private Cache Maps
// ============================================================================

struct SHARED_CACHE_MAP;

// 256 KB Virtual Address Control Block
struct VACB {
    uint32_t                 VacbIndex{0};
    PVOID                    BaseAddress{nullptr};     // Virtual address in System Cache space
    uint64_t                 VirtualAddress{0};        // Canonical 64-bit System Cache VA
    SHARED_CACHE_MAP*        SharedCacheMap{nullptr};  // Owning file stream cache map
    uint64_t                 FileOffset{0};            // 256KB-aligned byte offset in file
    uint32_t                 ActiveCount{0};           // Pin / Map reference count
    bool                     Dirty{false};             // Contains modified bytes awaiting flush
    uint32_t                 DirtyByteCount{0};        // Number of dirty bytes in this view
    LARGE_INTEGER            OldestLsn{0};             // Oldest Write-Ahead Log LSN
    LARGE_INTEGER            NewestLsn{0};             // Newest Write-Ahead Log LSN
    uint64_t                 LastAccessTick{0};        // Monotonic tick for LRU replacement
    std::vector<uint8_t>     ViewStorage;              // 256 KB backing view buffer
};

// Public BCB (Buffer Control Block) returned by CcPinRead / CcMapData
inline constexpr uint16_t CC_NTC_BCB = 0x02FE;

struct PUBLIC_BCB {
    int16_t       NodeTypeCode{CC_NTC_BCB};
    int16_t       NodeByteSize{sizeof(PUBLIC_BCB)};
    ULONG         MappedLength{0};
    LARGE_INTEGER MappedFileOffset{0};
};

struct INTERNAL_BCB {
    PUBLIC_BCB        PublicBcb{};
    SHARED_CACHE_MAP* SharedCacheMap{nullptr};
    VACB*             Vacb{nullptr};
    PVOID             BufferPointer{nullptr};
    uint32_t          PinCount{1};
    bool              IsPinned{false};  // false = Mapped only (CcMapData), true = Pinned (CcPinRead)
    bool              Exclusive{false};
    bool              Dirty{false};
    LARGE_INTEGER     OldestLsn{0};
    LARGE_INTEGER     NewestLsn{0};
};

// Zero-Copy MDL Chain Element returned by CcMdlRead / CcPrepareMdlWrite
struct CC_MDL {
    CC_MDL*           Next{nullptr};
    int16_t           Size{sizeof(CC_MDL)};
    int16_t           MdlFlags{0x0004 /* MDL_SOURCE_IS_NONPAGED_POOL */};
    PVOID             MappedSystemVa{nullptr};
    PVOID             StartVa{nullptr};
    ULONG             ByteCount{0};
    ULONG             ByteOffset{0};
    VACB*             OwningVacb{nullptr};
    SHARED_CACHE_MAP* OwningCacheMap{nullptr};
    bool              IsWriteMdl{false};
};
using PCC_MDL = CC_MDL*;

// Per-Handle Private Cache Map (tracks sequential read-ahead history)
struct PRIVATE_CACHE_MAP {
    int16_t       NodeTypeCode{0x02FE};
    int16_t       NodeByteSize{sizeof(PRIVATE_CACHE_MAP)};
    ULONG         ReadAheadMask{DEFAULT_READ_AHEAD_GRANULARITY - 1};
    ULONG         ReadAheadGranularity{DEFAULT_READ_AHEAD_GRANULARITY};
    LARGE_INTEGER FileOffset1{0};
    LARGE_INTEGER BeyondLastByte1{0};
    LARGE_INTEGER FileOffset2{0};
    LARGE_INTEGER BeyondLastByte2{0};
    bool          ReadAheadActive{false};
    bool          SequentialAccessDetected{false};
};

// Per-Stream Shared Cache Map (tracks 256 KB VACB array, file sizes, callbacks, backing storage)
struct SHARED_CACHE_MAP {
    int16_t                   NodeTypeCode{0x02FF};
    int16_t                   NodeByteSize{sizeof(SHARED_CACHE_MAP)};
    ULONG                     OpenCount{1};
    CC_FILE_SIZES             FileSizes{};
    BOOLEAN                   PinAccess{FALSE_B};
    CACHE_MANAGER_CALLBACKS   Callbacks{};
    PVOID                     LazyWriteContext{nullptr};
    PVOID                     LogHandle{nullptr};
    PFLUSH_TO_LSN             FlushToLsnRoutine{nullptr};
    ULONG                     DirtyPages{0};
    ULONG                     DirtyPageThreshold{0};
    CC_FILE_OBJECT*           FileObject{nullptr};
    std::map<uint64_t, std::unique_ptr<VACB>> ActiveVacbs; // Key: 256KB-aligned FileOffset
    std::vector<uint8_t>      CommittedDiskBacking;        // Simulated persistent disk image for verification
};

// ============================================================================
// 4. Cache Manager Telemetry Structure
// ============================================================================

struct CC_CACHE_TELEMETRY {
    uint64_t totalCacheMapsInitialized{0};
    uint64_t totalCacheMapsUninitialized{0};
    uint64_t totalVacbsAllocated{0};
    uint64_t totalVacbHits{0};
    uint64_t totalVacbMisses{0};
    uint64_t totalCopyReads{0};
    uint64_t totalCopyWrites{0};
    uint64_t totalFastCopyReads{0};
    uint64_t totalFastCopyWrites{0};
    uint64_t totalBytesReadFromCache{0};
    uint64_t totalBytesWrittenToCache{0};
    uint64_t totalMdlReads{0};
    uint64_t totalMdlWrites{0};
    uint64_t totalBcbsMapped{0};
    uint64_t totalBcbsPinned{0};
    uint64_t totalBcbsDirtyLsn{0};
    uint64_t totalReadAheadsScheduled{0};
    uint64_t totalLazyWriteScans{0};
    uint64_t totalPagesFlushedToDisk{0};
    uint64_t totalDeferredWritesExecuted{0};
    uint32_t currentDirtyPages{0};
    uint32_t activeSharedCacheMaps{0};
};

// ============================================================================
// 5. Sovereign NT Executive Cache Manager Engine (TitanCacheManager)
// ============================================================================

class TitanCacheManager {
public:
    static TitanCacheManager& Instance() {
        static TitanCacheManager s_instance;
        return s_instance;
    }

    void initialize() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_initialized) return;
        resetInternalState();
        m_initialized = true;
    }

    void resetForTesting() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        resetInternalState();
        m_initialized = true;
    }

    // ------------------------------------------------------------------------
    // Cache Map Lifecycle: CcInitializeCacheMap / CcUninitializeCacheMap
    // ------------------------------------------------------------------------

    void initializeCacheMap(
        CC_FILE_OBJECT* fileObject,
        const CC_FILE_SIZES* fileSizes,
        BOOLEAN pinAccess,
        const CACHE_MANAGER_CALLBACKS* callbacks,
        PVOID lazyWriteContext
    ) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!fileObject || !fileObject->SectionObjectPointer) return;

        SHARED_CACHE_MAP* scm = static_cast<SHARED_CACHE_MAP*>(fileObject->SectionObjectPointer->SharedCacheMap);
        if (!scm) {
            auto newScm = std::make_unique<SHARED_CACHE_MAP>();
            newScm->OpenCount        = 1;
            newScm->PinAccess        = pinAccess;
            newScm->LazyWriteContext = lazyWriteContext;
            newScm->FileObject       = fileObject;
            if (fileSizes) {
                newScm->FileSizes = *fileSizes;
                size_t allocBytes = static_cast<size_t>(std::max<int64_t>(0, fileSizes->AllocationSize.QuadPart));
                newScm->CommittedDiskBacking.resize(allocBytes, 0);
            }
            if (callbacks) {
                newScm->Callbacks = *callbacks;
            }
            scm = newScm.get();
            fileObject->SectionObjectPointer->SharedCacheMap = scm;
            m_sharedMaps[scm] = std::move(newScm);
            m_telemetry.totalCacheMapsInitialized++;
        } else {
            scm->OpenCount++;
        }

        if (!fileObject->PrivateCacheMap) {
            auto pcm = std::make_unique<PRIVATE_CACHE_MAP>();
            fileObject->PrivateCacheMap = pcm.get();
            m_privateMaps[pcm.get()] = std::move(pcm);
        }
    }

    BOOLEAN uninitializeCacheMap(
        CC_FILE_OBJECT* fileObject,
        const LARGE_INTEGER* truncateSize,
        PCACHE_UNINITIALIZE_EVENT uninitializeEvent
    ) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!fileObject || !fileObject->SectionObjectPointer) return FALSE_B;

        if (fileObject->PrivateCacheMap) {
            auto* pcm = static_cast<PRIVATE_CACHE_MAP*>(fileObject->PrivateCacheMap);
            m_privateMaps.erase(pcm);
            fileObject->PrivateCacheMap = nullptr;
        }

        auto* scm = static_cast<SHARED_CACHE_MAP*>(fileObject->SectionObjectPointer->SharedCacheMap);
        if (!scm) return TRUE_B;

        if (truncateSize && truncateSize->QuadPart == 0) {
            // Truncating to 0 discards dirty pages without disk flush
            for (auto& [offset, vacb] : scm->ActiveVacbs) {
                if (vacb->Dirty) {
                    uint32_t pages = (vacb->DirtyByteCount + CC_PAGE_SIZE - 1) / CC_PAGE_SIZE;
                    if (m_totalDirtyPages >= pages) m_totalDirtyPages -= pages;
                    vacb->Dirty = false;
                    vacb->DirtyByteCount = 0;
                }
            }
            scm->DirtyPages = 0;
        } else {
            // Flush dirty views before teardown
            IoStatusBlock iosb{};
            flushCacheInternal(fileObject->SectionObjectPointer, nullptr, 0, &iosb);
        }

        if (scm->OpenCount > 0) scm->OpenCount--;
        if (scm->OpenCount == 0) {
            fileObject->SectionObjectPointer->SharedCacheMap = nullptr;
            m_sharedMaps.erase(scm);
            m_telemetry.totalCacheMapsUninitialized++;
        }

        if (uninitializeEvent) {
            uninitializeEvent->Signaled = TRUE_B;
            uninitializeEvent->Status   = NtStatus::Success;
        }
        return TRUE_B;
    }

    void setFileSizes(CC_FILE_OBJECT* fileObject, const CC_FILE_SIZES* fileSizes) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!fileObject || !fileObject->SectionObjectPointer || !fileSizes) return;
        auto* scm = static_cast<SHARED_CACHE_MAP*>(fileObject->SectionObjectPointer->SharedCacheMap);
        if (!scm) return;

        scm->FileSizes = *fileSizes;
        size_t newAlloc = static_cast<size_t>(std::max<int64_t>(0, fileSizes->AllocationSize.QuadPart));
        if (newAlloc != scm->CommittedDiskBacking.size()) {
            scm->CommittedDiskBacking.resize(newAlloc, 0);
        }

        // Purge any VACBs beyond new AllocationSize
        for (auto it = scm->ActiveVacbs.begin(); it != scm->ActiveVacbs.end(); ) {
            if (it->first >= newAlloc && it->second->ActiveCount == 0) {
                it = scm->ActiveVacbs.erase(it);
            } else {
                ++it;
            }
        }
    }

    PLARGE_INTEGER getFileSizePointer(CC_FILE_OBJECT* fileObject) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!fileObject || !fileObject->SectionObjectPointer) return nullptr;
        auto* scm = static_cast<SHARED_CACHE_MAP*>(fileObject->SectionObjectPointer->SharedCacheMap);
        if (!scm) return nullptr;
        return &scm->FileSizes.FileSize;
    }

    void setAdditionalCacheAttributes(
        CC_FILE_OBJECT* fileObject,
        BOOLEAN disableReadAhead,
        BOOLEAN disableWriteBehind
    ) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        (void)disableWriteBehind;
        if (!fileObject || !fileObject->PrivateCacheMap) return;
        auto* pcm = static_cast<PRIVATE_CACHE_MAP*>(fileObject->PrivateCacheMap);
        pcm->ReadAheadActive = (disableReadAhead == FALSE_B);
    }

    void setLogHandleForFile(
        CC_FILE_OBJECT* fileObject,
        PVOID logHandle,
        PFLUSH_TO_LSN flushToLsnRoutine
    ) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!fileObject || !fileObject->SectionObjectPointer) return;
        auto* scm = static_cast<SHARED_CACHE_MAP*>(fileObject->SectionObjectPointer->SharedCacheMap);
        if (!scm) return;
        scm->LogHandle         = logHandle;
        scm->FlushToLsnRoutine = flushToLsnRoutine;
    }

    // ------------------------------------------------------------------------
    // VACB Lookup & On-Demand 256 KB View Mapping
    // ------------------------------------------------------------------------

    VACB* getOrMapVacb(SHARED_CACHE_MAP* scm, uint64_t fileOffset) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!scm) return nullptr;

        uint64_t alignedOffset = (fileOffset >> VACB_OFFSET_SHIFT) << VACB_OFFSET_SHIFT;
        auto it = scm->ActiveVacbs.find(alignedOffset);
        if (it != scm->ActiveVacbs.end()) {
            m_telemetry.totalVacbHits++;
            it->second->LastAccessTick = ++m_monotonicTick;
            return it->second.get();
        }

        // Cache Miss -> Allocate a new 256 KB VACB view and populate from backing store
        m_telemetry.totalVacbMisses++;
        m_telemetry.totalVacbsAllocated++;

        auto vacb = std::make_unique<VACB>();
        uint32_t vacbIdx        = m_nextVacbIndex++;
        vacb->VacbIndex         = vacbIdx;
        vacb->SharedCacheMap    = scm;
        vacb->FileOffset        = alignedOffset;
        vacb->LastAccessTick    = ++m_monotonicTick;
        vacb->ViewStorage.resize(VACB_MAPPING_GRANULARITY, 0);
        vacb->BaseAddress       = vacb->ViewStorage.data();
        vacb->VirtualAddress    = SYSTEM_CACHE_VIRTUAL_BASE + static_cast<uint64_t>(vacbIdx) * VACB_MAPPING_GRANULARITY;

        // Hydrate from backing store if within committed bounds
        if (alignedOffset < scm->CommittedDiskBacking.size()) {
            size_t avail = std::min<size_t>(VACB_MAPPING_GRANULARITY, scm->CommittedDiskBacking.size() - alignedOffset);
            std::memcpy(vacb->ViewStorage.data(), scm->CommittedDiskBacking.data() + alignedOffset, avail);
        }

        VACB* rawPtr = vacb.get();
        scm->ActiveVacbs[alignedOffset] = std::move(vacb);
        return rawPtr;
    }

    // ------------------------------------------------------------------------
    // Fast Copy Interface: CcCopyRead, CcCopyWrite, CcFastCopyRead/Write, CcZeroData
    // ------------------------------------------------------------------------

    BOOLEAN copyRead(
        CC_FILE_OBJECT* fileObject,
        const LARGE_INTEGER* fileOffset,
        ULONG length,
        BOOLEAN wait,
        PVOID buffer,
        PIO_STATUS_BLOCK ioStatus
    ) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!fileObject || !fileObject->SectionObjectPointer || !fileOffset || !buffer) {
            if (ioStatus) { ioStatus->Status = NtStatus::InvalidParameter; ioStatus->Information = 0; }
            return FALSE_B;
        }
        if (!wait) {
            // Check if all required VACBs are already resident
            auto* scm = static_cast<SHARED_CACHE_MAP*>(fileObject->SectionObjectPointer->SharedCacheMap);
            if (!scm) return FALSE_B;
            uint64_t cur = static_cast<uint64_t>(fileOffset->QuadPart);
            uint64_t end = cur + length;
            while (cur < end) {
                uint64_t aligned = (cur >> VACB_OFFSET_SHIFT) << VACB_OFFSET_SHIFT;
                if (scm->ActiveVacbs.find(aligned) == scm->ActiveVacbs.end()) return FALSE_B;
                cur = aligned + VACB_MAPPING_GRANULARITY;
            }
        }

        auto* scm = static_cast<SHARED_CACHE_MAP*>(fileObject->SectionObjectPointer->SharedCacheMap);
        if (!scm) return FALSE_B;

        uint64_t startOffset = static_cast<uint64_t>(fileOffset->QuadPart);
        uint64_t fileSize    = static_cast<uint64_t>(std::max<int64_t>(0, scm->FileSizes.FileSize.QuadPart));
        if (startOffset >= fileSize || length == 0) {
            if (ioStatus) { ioStatus->Status = NtStatus::EndOfFile; ioStatus->Information = 0; }
            return TRUE_B;
        }

        uint64_t bytesToRead = std::min<uint64_t>(length, fileSize - startOffset);
        uint8_t* dst         = static_cast<uint8_t*>(buffer);
        uint64_t remaining   = bytesToRead;
        uint64_t curOffset   = startOffset;

        while (remaining > 0) {
            VACB* vacb = getOrMapVacb(scm, curOffset);
            uint32_t offsetInVacb = static_cast<uint32_t>(curOffset & (VACB_MAPPING_GRANULARITY - 1));
            uint32_t chunk        = static_cast<uint32_t>(std::min<uint64_t>(remaining, VACB_MAPPING_GRANULARITY - offsetInVacb));

            std::memcpy(dst, vacb->ViewStorage.data() + offsetInVacb, chunk);
            dst       += chunk;
            curOffset += chunk;
            remaining -= chunk;
        }

        // Update sequential read-ahead history in PRIVATE_CACHE_MAP
        if (fileObject->PrivateCacheMap) {
            auto* pcm = static_cast<PRIVATE_CACHE_MAP*>(fileObject->PrivateCacheMap);
            if (pcm->BeyondLastByte1.QuadPart == static_cast<int64_t>(startOffset)) {
                pcm->SequentialAccessDetected = true;
            }
            pcm->FileOffset2     = pcm->FileOffset1;
            pcm->BeyondLastByte2 = pcm->BeyondLastByte1;
            pcm->FileOffset1.QuadPart     = static_cast<int64_t>(startOffset);
            pcm->BeyondLastByte1.QuadPart = static_cast<int64_t>(startOffset + bytesToRead);

            if (pcm->SequentialAccessDetected) {
                scheduleReadAhead(fileObject, fileOffset, length);
            }
        }

        m_telemetry.totalCopyReads++;
        m_telemetry.totalBytesReadFromCache += bytesToRead;

        if (ioStatus) {
            ioStatus->Status      = NtStatus::Success;
            ioStatus->Information = static_cast<uintptr_t>(bytesToRead);
        }
        return TRUE_B;
    }

    BOOLEAN copyWrite(
        CC_FILE_OBJECT* fileObject,
        const LARGE_INTEGER* fileOffset,
        ULONG length,
        BOOLEAN wait,
        PVOID buffer
    ) {
        (void)wait;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!fileObject || !fileObject->SectionObjectPointer || !fileOffset || !buffer) return FALSE_B;

        auto* scm = static_cast<SHARED_CACHE_MAP*>(fileObject->SectionObjectPointer->SharedCacheMap);
        if (!scm) return FALSE_B;

        uint64_t startOffset = static_cast<uint64_t>(fileOffset->QuadPart);
        uint64_t endOffset   = startOffset + length;

        if (endOffset > static_cast<uint64_t>(scm->FileSizes.AllocationSize.QuadPart)) {
            scm->FileSizes.AllocationSize.QuadPart = static_cast<int64_t>(endOffset);
            scm->CommittedDiskBacking.resize(endOffset, 0);
        }
        if (endOffset > static_cast<uint64_t>(scm->FileSizes.FileSize.QuadPart)) {
            scm->FileSizes.FileSize.QuadPart        = static_cast<int64_t>(endOffset);
            scm->FileSizes.ValidDataLength.QuadPart = static_cast<int64_t>(endOffset);
        }

        const uint8_t* src = static_cast<const uint8_t*>(buffer);
        uint64_t remaining = length;
        uint64_t curOffset = startOffset;

        while (remaining > 0) {
            VACB* vacb = getOrMapVacb(scm, curOffset);
            uint32_t offsetInVacb = static_cast<uint32_t>(curOffset & (VACB_MAPPING_GRANULARITY - 1));
            uint32_t chunk        = static_cast<uint32_t>(std::min<uint64_t>(remaining, VACB_MAPPING_GRANULARITY - offsetInVacb));

            std::memcpy(vacb->ViewStorage.data() + offsetInVacb, src, chunk);
            markVacbDirty(scm, vacb, chunk);

            src       += chunk;
            curOffset += chunk;
            remaining -= chunk;
        }

        m_telemetry.totalCopyWrites++;
        m_telemetry.totalBytesWrittenToCache += length;
        return TRUE_B;
    }

    void fastCopyRead(
        CC_FILE_OBJECT* fileObject,
        ULONG fileOffset,
        ULONG length,
        ULONG pageCount,
        PVOID buffer,
        PIO_STATUS_BLOCK ioStatus
    ) {
        (void)pageCount;
        LARGE_INTEGER off{};
        off.QuadPart = fileOffset;
        copyRead(fileObject, &off, length, TRUE_B, buffer, ioStatus);
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_telemetry.totalFastCopyReads++;
    }

    void fastCopyWrite(
        CC_FILE_OBJECT* fileObject,
        ULONG fileOffset,
        ULONG length,
        PVOID buffer
    ) {
        LARGE_INTEGER off{};
        off.QuadPart = fileOffset;
        copyWrite(fileObject, &off, length, TRUE_B, buffer);
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_telemetry.totalFastCopyWrites++;
    }

    BOOLEAN zeroData(
        CC_FILE_OBJECT* fileObject,
        const LARGE_INTEGER* startOffset,
        const LARGE_INTEGER* endOffset,
        BOOLEAN wait
    ) {
        if (!startOffset || !endOffset || endOffset->QuadPart <= startOffset->QuadPart) return TRUE_B;
        ULONG len = static_cast<ULONG>(endOffset->QuadPart - startOffset->QuadPart);
        std::vector<uint8_t> zeros(len, 0);
        return copyWrite(fileObject, startOffset, len, wait, zeros.data());
    }

    // ------------------------------------------------------------------------
    // Zero-Copy MDL Interface: CcMdlRead, CcMdlReadComplete, CcPrepareMdlWrite, CcMdlWriteComplete
    // ------------------------------------------------------------------------

    void mdlRead(
        CC_FILE_OBJECT* fileObject,
        const LARGE_INTEGER* fileOffset,
        ULONG length,
        PCC_MDL* mdlChain,
        PIO_STATUS_BLOCK ioStatus
    ) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!fileObject || !fileObject->SectionObjectPointer || !fileOffset || !mdlChain) {
            if (ioStatus) { ioStatus->Status = NtStatus::InvalidParameter; ioStatus->Information = 0; }
            return;
        }
        auto* scm = static_cast<SHARED_CACHE_MAP*>(fileObject->SectionObjectPointer->SharedCacheMap);
        if (!scm) {
            if (ioStatus) { ioStatus->Status = NtStatus::InvalidDeviceRequest; ioStatus->Information = 0; }
            return;
        }

        uint64_t curOffset = static_cast<uint64_t>(fileOffset->QuadPart);
        uint64_t remaining = length;
        CC_MDL* head       = nullptr;
        CC_MDL* tail       = nullptr;

        while (remaining > 0) {
            VACB* vacb = getOrMapVacb(scm, curOffset);
            vacb->ActiveCount++;
            uint32_t offsetInVacb = static_cast<uint32_t>(curOffset & (VACB_MAPPING_GRANULARITY - 1));
            uint32_t chunk        = static_cast<uint32_t>(std::min<uint64_t>(remaining, VACB_MAPPING_GRANULARITY - offsetInVacb));

            auto* mdl            = new CC_MDL();
            mdl->MappedSystemVa  = vacb->ViewStorage.data() + offsetInVacb;
            mdl->StartVa         = vacb->ViewStorage.data();
            mdl->ByteOffset      = offsetInVacb;
            mdl->ByteCount       = chunk;
            mdl->OwningVacb      = vacb;
            mdl->OwningCacheMap  = scm;
            mdl->IsWriteMdl      = false;

            if (!head) head = mdl;
            else tail->Next = mdl;
            tail = mdl;

            curOffset += chunk;
            remaining -= chunk;
        }

        *mdlChain = head;
        m_telemetry.totalMdlReads++;
        m_telemetry.totalBytesReadFromCache += length;
        if (ioStatus) {
            ioStatus->Status      = NtStatus::Success;
            ioStatus->Information = length;
        }
    }

    void mdlReadComplete(CC_FILE_OBJECT* fileObject, PCC_MDL mdlChain) {
        (void)fileObject;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        CC_MDL* cur = mdlChain;
        while (cur) {
            CC_MDL* next = cur->Next;
            if (cur->OwningVacb && cur->OwningVacb->ActiveCount > 0) {
                cur->OwningVacb->ActiveCount--;
            }
            delete cur;
            cur = next;
        }
    }

    void prepareMdlWrite(
        CC_FILE_OBJECT* fileObject,
        const LARGE_INTEGER* fileOffset,
        ULONG length,
        PCC_MDL* mdlChain,
        PIO_STATUS_BLOCK ioStatus
    ) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        mdlRead(fileObject, fileOffset, length, mdlChain, ioStatus);
        for (CC_MDL* m = *mdlChain; m != nullptr; m = m->Next) {
            m->IsWriteMdl = true;
        }
        m_telemetry.totalMdlWrites++;
    }

    void mdlWriteComplete(
        CC_FILE_OBJECT* fileObject,
        const LARGE_INTEGER* fileOffset,
        PCC_MDL mdlChain
    ) {
        (void)fileObject;
        (void)fileOffset;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        CC_MDL* cur = mdlChain;
        while (cur) {
            CC_MDL* next = cur->Next;
            if (cur->OwningVacb && cur->OwningCacheMap) {
                markVacbDirty(cur->OwningCacheMap, cur->OwningVacb, cur->ByteCount);
                m_telemetry.totalBytesWrittenToCache += cur->ByteCount;
                if (cur->OwningVacb->ActiveCount > 0) {
                    cur->OwningVacb->ActiveCount--;
                }
            }
            delete cur;
            cur = next;
        }
    }

    // ------------------------------------------------------------------------
    // Metadata Pinning & BCB Interface: CcMapData, CcPinRead, CcPreparePinWrite,
    // CcPinMappedData, CcSetDirtyPinnedData, CcUnpinData
    // ------------------------------------------------------------------------

    BOOLEAN mapData(
        CC_FILE_OBJECT* fileObject,
        const LARGE_INTEGER* fileOffset,
        ULONG length,
        ULONG flags,
        PVOID* bcbOut,
        PVOID* bufferOut
    ) {
        (void)flags;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!fileObject || !fileObject->SectionObjectPointer || !fileOffset || !bcbOut || !bufferOut) {
            return FALSE_B;
        }
        auto* scm = static_cast<SHARED_CACHE_MAP*>(fileObject->SectionObjectPointer->SharedCacheMap);
        if (!scm) return FALSE_B;

        uint64_t startOffset  = static_cast<uint64_t>(fileOffset->QuadPart);
        VACB* vacb            = getOrMapVacb(scm, startOffset);
        uint32_t offsetInVacb = static_cast<uint32_t>(startOffset & (VACB_MAPPING_GRANULARITY - 1));
        vacb->ActiveCount++;

        auto* bcb                        = new INTERNAL_BCB();
        bcb->PublicBcb.NodeTypeCode      = CC_NTC_BCB;
        bcb->PublicBcb.NodeByteSize      = sizeof(PUBLIC_BCB);
        bcb->PublicBcb.MappedLength      = length;
        bcb->PublicBcb.MappedFileOffset  = *fileOffset;
        bcb->SharedCacheMap              = scm;
        bcb->Vacb                        = vacb;
        bcb->BufferPointer               = vacb->ViewStorage.data() + offsetInVacb;
        bcb->PinCount                    = 1;
        bcb->IsPinned                    = false;

        *bcbOut    = bcb;
        *bufferOut = bcb->BufferPointer;
        m_telemetry.totalBcbsMapped++;
        return TRUE_B;
    }

    BOOLEAN pinMappedData(
        CC_FILE_OBJECT* fileObject,
        const LARGE_INTEGER* fileOffset,
        ULONG length,
        ULONG flags,
        PVOID* bcbIo
    ) {
        (void)fileObject;
        (void)fileOffset;
        (void)length;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!bcbIo || !*bcbIo) return FALSE_B;
        auto* bcb = static_cast<INTERNAL_BCB*>(*bcbIo);
        bcb->IsPinned  = true;
        bcb->Exclusive = (flags & PIN_EXCLUSIVE) != 0;
        m_telemetry.totalBcbsPinned++;
        return TRUE_B;
    }

    BOOLEAN pinRead(
        CC_FILE_OBJECT* fileObject,
        const LARGE_INTEGER* fileOffset,
        ULONG length,
        ULONG flags,
        PVOID* bcbOut,
        PVOID* bufferOut
    ) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!mapData(fileObject, fileOffset, length, flags, bcbOut, bufferOut)) {
            return FALSE_B;
        }
        return pinMappedData(fileObject, fileOffset, length, flags, bcbOut);
    }

    BOOLEAN preparePinWrite(
        CC_FILE_OBJECT* fileObject,
        const LARGE_INTEGER* fileOffset,
        ULONG length,
        BOOLEAN zero,
        ULONG flags,
        PVOID* bcbOut,
        PVOID* bufferOut
    ) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!pinRead(fileObject, fileOffset, length, flags, bcbOut, bufferOut)) {
            return FALSE_B;
        }
        if (zero && *bufferOut && length > 0) {
            std::memset(*bufferOut, 0, length);
        }
        setDirtyPinnedData(*bcbOut, nullptr);
        return TRUE_B;
    }

    void setDirtyPinnedData(PVOID bcbVoid, const LARGE_INTEGER* lsn) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!bcbVoid) return;
        auto* bcb = static_cast<INTERNAL_BCB*>(bcbVoid);
        bcb->Dirty = true;
        if (lsn && lsn->QuadPart != 0) {
            if (bcb->OldestLsn.QuadPart == 0 || lsn->QuadPart < bcb->OldestLsn.QuadPart) {
                bcb->OldestLsn = *lsn;
            }
            if (lsn->QuadPart > bcb->NewestLsn.QuadPart) {
                bcb->NewestLsn = *lsn;
            }
            if (bcb->Vacb) {
                if (bcb->Vacb->OldestLsn.QuadPart == 0 || lsn->QuadPart < bcb->Vacb->OldestLsn.QuadPart) {
                    bcb->Vacb->OldestLsn = *lsn;
                }
                if (lsn->QuadPart > bcb->Vacb->NewestLsn.QuadPart) {
                    bcb->Vacb->NewestLsn = *lsn;
                }
            }
            m_telemetry.totalBcbsDirtyLsn++;
        }
        if (bcb->SharedCacheMap && bcb->Vacb) {
            markVacbDirty(bcb->SharedCacheMap, bcb->Vacb, bcb->PublicBcb.MappedLength);
        }
    }

    void repinBcb(PVOID bcbVoid) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!bcbVoid) return;
        auto* bcb = static_cast<INTERNAL_BCB*>(bcbVoid);
        bcb->PinCount++;
        if (bcb->Vacb) bcb->Vacb->ActiveCount++;
    }

    void unpinData(PVOID bcbVoid) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!bcbVoid) return;
        auto* bcb = static_cast<INTERNAL_BCB*>(bcbVoid);
        if (bcb->Vacb && bcb->Vacb->ActiveCount > 0) {
            bcb->Vacb->ActiveCount--;
        }
        if (bcb->PinCount > 1) {
            bcb->PinCount--;
            return;
        }
        delete bcb;
    }

    // ------------------------------------------------------------------------
    // Intelligent Read-Ahead Engine: CcSetReadAheadGranularity & CcScheduleReadAhead
    // ------------------------------------------------------------------------

    void setReadAheadGranularity(CC_FILE_OBJECT* fileObject, ULONG granularity) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!fileObject || !fileObject->PrivateCacheMap || granularity == 0) return;
        auto* pcm = static_cast<PRIVATE_CACHE_MAP*>(fileObject->PrivateCacheMap);
        pcm->ReadAheadGranularity = granularity;
        pcm->ReadAheadMask        = granularity - 1;
    }

    void scheduleReadAhead(
        CC_FILE_OBJECT* fileObject,
        const LARGE_INTEGER* fileOffset,
        ULONG length
    ) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!fileObject || !fileObject->SectionObjectPointer || !fileOffset) return;
        auto* scm = static_cast<SHARED_CACHE_MAP*>(fileObject->SectionObjectPointer->SharedCacheMap);
        if (!scm) return;

        ULONG gran = DEFAULT_READ_AHEAD_GRANULARITY;
        if (fileObject->PrivateCacheMap) {
            gran = static_cast<PRIVATE_CACHE_MAP*>(fileObject->PrivateCacheMap)->ReadAheadGranularity;
        }

        // Acquire FSD ReadAhead lock if callback is provided
        if (scm->Callbacks.AcquireForReadAhead) {
            if (!scm->Callbacks.AcquireForReadAhead(scm->LazyWriteContext, TRUE_B)) {
                return;
            }
        }

        uint64_t nextOffset = static_cast<uint64_t>(fileOffset->QuadPart) + length;
        uint64_t fileSize   = static_cast<uint64_t>(std::max<int64_t>(0, scm->FileSizes.FileSize.QuadPart));
        if (nextOffset < fileSize) {
            // Prefetch VACB covering the upcoming sequential window
            getOrMapVacb(scm, nextOffset);
            if (nextOffset + gran < fileSize) {
                getOrMapVacb(scm, nextOffset + gran);
            }
        }

        if (scm->Callbacks.ReleaseFromReadAhead) {
            scm->Callbacks.ReleaseFromReadAhead(scm->LazyWriteContext);
        }
        m_telemetry.totalReadAheadsScheduled++;
    }

    // ------------------------------------------------------------------------
    // Flushing, Lazy Writer & Write Throttling
    // ------------------------------------------------------------------------

    void flushCache(
        PSECTION_OBJECT_POINTERS sectionObjectPointer,
        const LARGE_INTEGER* fileOffset,
        ULONG length,
        PIO_STATUS_BLOCK ioStatus
    ) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        flushCacheInternal(sectionObjectPointer, fileOffset, length, ioStatus);
    }

    BOOLEAN purgeCacheSection(
        PSECTION_OBJECT_POINTERS sectionObjectPointer,
        const LARGE_INTEGER* fileOffset,
        ULONG length,
        BOOLEAN uninitializeCacheMaps
    ) {
        (void)uninitializeCacheMaps;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!sectionObjectPointer || !sectionObjectPointer->SharedCacheMap) return TRUE_B;
        auto* scm = static_cast<SHARED_CACHE_MAP*>(sectionObjectPointer->SharedCacheMap);

        uint64_t startOff = fileOffset ? static_cast<uint64_t>(fileOffset->QuadPart) : 0;
        uint64_t endOff   = (fileOffset && length > 0) ? (startOff + length) : UINT64_MAX;

        for (auto it = scm->ActiveVacbs.begin(); it != scm->ActiveVacbs.end(); ) {
            uint64_t vStart = it->first;
            uint64_t vEnd   = vStart + VACB_MAPPING_GRANULARITY;
            if (vEnd > startOff && vStart < endOff) {
                if (it->second->ActiveCount > 0) {
                    return FALSE_B; // Cannot purge an actively pinned view
                }
                if (it->second->Dirty) {
                    uint32_t pages = (it->second->DirtyByteCount + CC_PAGE_SIZE - 1) / CC_PAGE_SIZE;
                    if (scm->DirtyPages >= pages) scm->DirtyPages -= pages;
                    if (m_totalDirtyPages >= pages) m_totalDirtyPages -= pages;
                }
                it = scm->ActiveVacbs.erase(it);
            } else {
                ++it;
            }
        }
        return TRUE_B;
    }

    // Execute one sweep of the NT Cache Manager Lazy Writer thread
    uint32_t lazyWriteScan() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_telemetry.totalLazyWriteScans++;
        uint32_t totalPagesFlushed = 0;

        for (auto& [ptr, scmPtr] : m_sharedMaps) {
            SHARED_CACHE_MAP* scm = scmPtr.get();
            if (scm->DirtyPages == 0) continue;

            if (scm->Callbacks.AcquireForLazyWrite) {
                if (!scm->Callbacks.AcquireForLazyWrite(scm->LazyWriteContext, TRUE_B)) {
                    continue;
                }
            }

            uint32_t beforePages = scm->DirtyPages;
            IoStatusBlock iosb{};
            flushCacheInternal(scm->FileObject ? scm->FileObject->SectionObjectPointer : nullptr, nullptr, 0, &iosb);
            totalPagesFlushed += beforePages;

            if (scm->Callbacks.ReleaseFromLazyWrite) {
                scm->Callbacks.ReleaseFromLazyWrite(scm->LazyWriteContext);
            }
        }

        // Drain any Deferred Writes that are now unblocked
        drainDeferredWrites();
        return totalPagesFlushed;
    }

    void setDirtyPageThreshold(CC_FILE_OBJECT* fileObject, ULONG dirtyPageThreshold) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!fileObject || !fileObject->SectionObjectPointer) return;
        auto* scm = static_cast<SHARED_CACHE_MAP*>(fileObject->SectionObjectPointer->SharedCacheMap);
        if (!scm) return;
        scm->DirtyPageThreshold = dirtyPageThreshold;
    }

    BOOLEAN canIWrite(
        CC_FILE_OBJECT* fileObject,
        ULONG bytesToWrite,
        BOOLEAN wait,
        BOOLEAN retrying
    ) {
        (void)wait;
        (void)retrying;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!fileObject || !fileObject->SectionObjectPointer) return TRUE_B;
        auto* scm = static_cast<SHARED_CACHE_MAP*>(fileObject->SectionObjectPointer->SharedCacheMap);
        if (!scm || scm->DirtyPageThreshold == 0) return TRUE_B;

        uint32_t additionalPages = (bytesToWrite + CC_PAGE_SIZE - 1) / CC_PAGE_SIZE;
        if (scm->DirtyPages + additionalPages > scm->DirtyPageThreshold) {
            return FALSE_B;
        }
        return TRUE_B;
    }

    void deferWrite(
        CC_FILE_OBJECT* fileObject,
        POST_DEFERRED_WRITE postRoutine,
        PVOID context1,
        PVOID context2,
        ULONG bytesToWrite,
        BOOLEAN retrying
    ) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        DEFERRED_WRITE dw{};
        dw.FileObject   = fileObject;
        dw.PostRoutine  = postRoutine;
        dw.Context1     = context1;
        dw.Context2     = context2;
        dw.BytesToWrite = bytesToWrite;

        if (canIWrite(fileObject, bytesToWrite, FALSE_B, retrying) == TRUE_B) {
            if (postRoutine) {
                postRoutine(context1, context2);
                m_telemetry.totalDeferredWritesExecuted++;
            }
        } else {
            if (retrying) {
                m_deferredWrites.push_front(dw);
            } else {
                m_deferredWrites.push_back(dw);
            }
        }
    }

    CC_CACHE_TELEMETRY getTelemetry() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_telemetry.currentDirtyPages     = m_totalDirtyPages;
        m_telemetry.activeSharedCacheMaps = static_cast<uint32_t>(m_sharedMaps.size());
        return m_telemetry;
    }

private:
    TitanCacheManager() {
        resetInternalState();
    }

    void resetInternalState() {
        m_sharedMaps.clear();
        m_privateMaps.clear();
        m_deferredWrites.clear();
        m_telemetry       = CC_CACHE_TELEMETRY{};
        m_nextVacbIndex   = 1;
        m_monotonicTick   = 1;
        m_totalDirtyPages = 0;
    }

    void markVacbDirty(SHARED_CACHE_MAP* scm, VACB* vacb, uint32_t bytesModified) {
        if (!scm || !vacb) return;
        uint32_t oldPages = vacb->Dirty ? ((vacb->DirtyByteCount + CC_PAGE_SIZE - 1) / CC_PAGE_SIZE) : 0;
        vacb->Dirty = true;
        vacb->DirtyByteCount = std::min<uint32_t>(VACB_MAPPING_GRANULARITY, vacb->DirtyByteCount + bytesModified);
        uint32_t newPages = (vacb->DirtyByteCount + CC_PAGE_SIZE - 1) / CC_PAGE_SIZE;
        if (newPages > oldPages) {
            uint32_t delta = newPages - oldPages;
            scm->DirtyPages   += delta;
            m_totalDirtyPages += delta;
        }
    }

    void flushCacheInternal(
        PSECTION_OBJECT_POINTERS sectionObjectPointer,
        const LARGE_INTEGER* fileOffset,
        ULONG length,
        PIO_STATUS_BLOCK ioStatus
    ) {
        if (!sectionObjectPointer || !sectionObjectPointer->SharedCacheMap) {
            if (ioStatus) { ioStatus->Status = NtStatus::Success; ioStatus->Information = 0; }
            return;
        }
        auto* scm = static_cast<SHARED_CACHE_MAP*>(sectionObjectPointer->SharedCacheMap);

        uint64_t startOff = fileOffset ? static_cast<uint64_t>(fileOffset->QuadPart) : 0;
        uint64_t endOff   = (fileOffset && length > 0) ? (startOff + length) : UINT64_MAX;

        LARGE_INTEGER maxLsnToFlush{0};
        size_t bytesFlushed = 0;

        for (auto& [vOffset, vacb] : scm->ActiveVacbs) {
            uint64_t vEnd = vOffset + VACB_MAPPING_GRANULARITY;
            if (vEnd <= startOff || vOffset >= endOff) continue;
            if (!vacb->Dirty) continue;

            if (vacb->NewestLsn.QuadPart > maxLsnToFlush.QuadPart) {
                maxLsnToFlush = vacb->NewestLsn;
            }
        }

        // Write-Ahead Logging rule: flush log up to newest LSN before writing dirty data pages!
        if (maxLsnToFlush.QuadPart > 0 && scm->FlushToLsnRoutine) {
            scm->FlushToLsnRoutine(scm->LogHandle, maxLsnToFlush);
        }

        for (auto& [vOffset, vacb] : scm->ActiveVacbs) {
            uint64_t vEnd = vOffset + VACB_MAPPING_GRANULARITY;
            if (vEnd <= startOff || vOffset >= endOff) continue;
            if (!vacb->Dirty) continue;

            if (vOffset < scm->CommittedDiskBacking.size()) {
                size_t toWrite = std::min<size_t>(VACB_MAPPING_GRANULARITY, scm->CommittedDiskBacking.size() - vOffset);
                std::memcpy(scm->CommittedDiskBacking.data() + vOffset, vacb->ViewStorage.data(), toWrite);
                bytesFlushed += toWrite;
            }

            uint32_t pages = (vacb->DirtyByteCount + CC_PAGE_SIZE - 1) / CC_PAGE_SIZE;
            if (scm->DirtyPages >= pages) scm->DirtyPages -= pages;
            else scm->DirtyPages = 0;

            if (m_totalDirtyPages >= pages) m_totalDirtyPages -= pages;
            else m_totalDirtyPages = 0;

            m_telemetry.totalPagesFlushedToDisk += pages;
            vacb->Dirty          = false;
            vacb->DirtyByteCount = 0;
            vacb->OldestLsn.QuadPart = 0;
            vacb->NewestLsn.QuadPart = 0;
        }

        if (ioStatus) {
            ioStatus->Status      = NtStatus::Success;
            ioStatus->Information = bytesFlushed;
        }
    }

    void drainDeferredWrites() {
        auto it = m_deferredWrites.begin();
        while (it != m_deferredWrites.end()) {
            auto* fo = static_cast<CC_FILE_OBJECT*>(it->FileObject);
            if (canIWrite(fo, it->BytesToWrite, FALSE_B, FALSE_B) == TRUE_B) {
                auto post = it->PostRoutine;
                PVOID c1  = it->Context1;
                PVOID c2  = it->Context2;
                it = m_deferredWrites.erase(it);
                if (post) {
                    post(c1, c2);
                    m_telemetry.totalDeferredWritesExecuted++;
                }
            } else {
                ++it;
            }
        }
    }

    std::recursive_mutex m_mutex;
    bool                 m_initialized{false};
    uint32_t             m_nextVacbIndex{1};
    uint64_t             m_monotonicTick{1};
    uint32_t             m_totalDirtyPages{0};

    std::unordered_map<SHARED_CACHE_MAP*, std::unique_ptr<SHARED_CACHE_MAP>>   m_sharedMaps;
    std::unordered_map<PRIVATE_CACHE_MAP*, std::unique_ptr<PRIVATE_CACHE_MAP>> m_privateMaps;
    std::deque<DEFERRED_WRITE>                                                 m_deferredWrites;
    CC_CACHE_TELEMETRY                                                         m_telemetry{};
};

// ============================================================================
// 6. Standard C ABI Exports (ntoskrnl.exe Cc* Cache Manager Interface)
// ============================================================================

extern "C" {

inline void WINAPI CcInitializeCacheMap(
    CC_FILE_OBJECT* FileObject,
    const CC_FILE_SIZES* FileSizes,
    BOOLEAN PinAccess,
    const CACHE_MANAGER_CALLBACKS* Callbacks,
    PVOID LazyWriteContext
) {
    TitanCacheManager::Instance().initializeCacheMap(FileObject, FileSizes, PinAccess, Callbacks, LazyWriteContext);
}

inline BOOLEAN WINAPI CcUninitializeCacheMap(
    CC_FILE_OBJECT* FileObject,
    const LARGE_INTEGER* TruncateSize,
    PCACHE_UNINITIALIZE_EVENT UninitializeCompleteEvent
) {
    return TitanCacheManager::Instance().uninitializeCacheMap(FileObject, TruncateSize, UninitializeCompleteEvent);
}

inline void WINAPI CcSetFileSizes(
    CC_FILE_OBJECT* FileObject,
    const CC_FILE_SIZES* FileSizes
) {
    TitanCacheManager::Instance().setFileSizes(FileObject, FileSizes);
}

inline PLARGE_INTEGER WINAPI CcGetFileSizePointer(CC_FILE_OBJECT* FileObject) {
    return TitanCacheManager::Instance().getFileSizePointer(FileObject);
}

inline void WINAPI CcSetAdditionalCacheAttributes(
    CC_FILE_OBJECT* FileObject,
    BOOLEAN DisableReadAhead,
    BOOLEAN DisableWriteBehind
) {
    TitanCacheManager::Instance().setAdditionalCacheAttributes(FileObject, DisableReadAhead, DisableWriteBehind);
}

inline void WINAPI CcSetLogHandleForFile(
    CC_FILE_OBJECT* FileObject,
    PVOID LogHandle,
    PFLUSH_TO_LSN FlushToLsnRoutine
) {
    TitanCacheManager::Instance().setLogHandleForFile(FileObject, LogHandle, FlushToLsnRoutine);
}

inline BOOLEAN WINAPI CcCopyRead(
    CC_FILE_OBJECT* FileObject,
    const LARGE_INTEGER* FileOffset,
    ULONG Length,
    BOOLEAN Wait,
    PVOID Buffer,
    PIO_STATUS_BLOCK IoStatus
) {
    return TitanCacheManager::Instance().copyRead(FileObject, FileOffset, Length, Wait, Buffer, IoStatus);
}

inline BOOLEAN WINAPI CcCopyWrite(
    CC_FILE_OBJECT* FileObject,
    const LARGE_INTEGER* FileOffset,
    ULONG Length,
    BOOLEAN Wait,
    PVOID Buffer
) {
    return TitanCacheManager::Instance().copyWrite(FileObject, FileOffset, Length, Wait, Buffer);
}

inline void WINAPI CcFastCopyRead(
    CC_FILE_OBJECT* FileObject,
    ULONG FileOffset,
    ULONG Length,
    ULONG PageCount,
    PVOID Buffer,
    PIO_STATUS_BLOCK IoStatus
) {
    TitanCacheManager::Instance().fastCopyRead(FileObject, FileOffset, Length, PageCount, Buffer, IoStatus);
}

inline void WINAPI CcFastCopyWrite(
    CC_FILE_OBJECT* FileObject,
    ULONG FileOffset,
    ULONG Length,
    PVOID Buffer
) {
    TitanCacheManager::Instance().fastCopyWrite(FileObject, FileOffset, Length, Buffer);
}

inline BOOLEAN WINAPI CcZeroData(
    CC_FILE_OBJECT* FileObject,
    const LARGE_INTEGER* StartOffset,
    const LARGE_INTEGER* EndOffset,
    BOOLEAN Wait
) {
    return TitanCacheManager::Instance().zeroData(FileObject, StartOffset, EndOffset, Wait);
}

inline void WINAPI CcMdlRead(
    CC_FILE_OBJECT* FileObject,
    const LARGE_INTEGER* FileOffset,
    ULONG Length,
    PCC_MDL* MdlChain,
    PIO_STATUS_BLOCK IoStatus
) {
    TitanCacheManager::Instance().mdlRead(FileObject, FileOffset, Length, MdlChain, IoStatus);
}

inline void WINAPI CcMdlReadComplete(
    CC_FILE_OBJECT* FileObject,
    PCC_MDL MdlChain
) {
    TitanCacheManager::Instance().mdlReadComplete(FileObject, MdlChain);
}

inline void WINAPI CcPrepareMdlWrite(
    CC_FILE_OBJECT* FileObject,
    const LARGE_INTEGER* FileOffset,
    ULONG Length,
    PCC_MDL* MdlChain,
    PIO_STATUS_BLOCK IoStatus
) {
    TitanCacheManager::Instance().prepareMdlWrite(FileObject, FileOffset, Length, MdlChain, IoStatus);
}

inline void WINAPI CcMdlWriteComplete(
    CC_FILE_OBJECT* FileObject,
    const LARGE_INTEGER* FileOffset,
    PCC_MDL MdlChain
) {
    TitanCacheManager::Instance().mdlWriteComplete(FileObject, FileOffset, MdlChain);
}

inline BOOLEAN WINAPI CcMapData(
    CC_FILE_OBJECT* FileObject,
    const LARGE_INTEGER* FileOffset,
    ULONG Length,
    ULONG Flags,
    PVOID* Bcb,
    PVOID* Buffer
) {
    return TitanCacheManager::Instance().mapData(FileObject, FileOffset, Length, Flags, Bcb, Buffer);
}

inline BOOLEAN WINAPI CcPinMappedData(
    CC_FILE_OBJECT* FileObject,
    const LARGE_INTEGER* FileOffset,
    ULONG Length,
    ULONG Flags,
    PVOID* Bcb
) {
    return TitanCacheManager::Instance().pinMappedData(FileObject, FileOffset, Length, Flags, Bcb);
}

inline BOOLEAN WINAPI CcPinRead(
    CC_FILE_OBJECT* FileObject,
    const LARGE_INTEGER* FileOffset,
    ULONG Length,
    ULONG Flags,
    PVOID* Bcb,
    PVOID* Buffer
) {
    return TitanCacheManager::Instance().pinRead(FileObject, FileOffset, Length, Flags, Bcb, Buffer);
}

inline BOOLEAN WINAPI CcPreparePinWrite(
    CC_FILE_OBJECT* FileObject,
    const LARGE_INTEGER* FileOffset,
    ULONG Length,
    BOOLEAN Zero,
    ULONG Flags,
    PVOID* Bcb,
    PVOID* Buffer
) {
    return TitanCacheManager::Instance().preparePinWrite(FileObject, FileOffset, Length, Zero, Flags, Bcb, Buffer);
}

inline void WINAPI CcSetDirtyPinnedData(
    PVOID BcbVoid,
    const LARGE_INTEGER* Lsn
) {
    TitanCacheManager::Instance().setDirtyPinnedData(BcbVoid, Lsn);
}

inline void WINAPI CcRepinBcb(PVOID Bcb) {
    TitanCacheManager::Instance().repinBcb(Bcb);
}

inline void WINAPI CcUnpinData(PVOID Bcb) {
    TitanCacheManager::Instance().unpinData(Bcb);
}

inline void WINAPI CcSetReadAheadGranularity(
    CC_FILE_OBJECT* FileObject,
    ULONG Granularity
) {
    TitanCacheManager::Instance().setReadAheadGranularity(FileObject, Granularity);
}

inline void WINAPI CcScheduleReadAhead(
    CC_FILE_OBJECT* FileObject,
    const LARGE_INTEGER* FileOffset,
    ULONG Length
) {
    TitanCacheManager::Instance().scheduleReadAhead(FileObject, FileOffset, Length);
}

inline void WINAPI CcFlushCache(
    PSECTION_OBJECT_POINTERS SectionObjectPointer,
    const LARGE_INTEGER* FileOffset,
    ULONG Length,
    PIO_STATUS_BLOCK IoStatus
) {
    TitanCacheManager::Instance().flushCache(SectionObjectPointer, FileOffset, Length, IoStatus);
}

inline BOOLEAN WINAPI CcPurgeCacheSection(
    PSECTION_OBJECT_POINTERS SectionObjectPointer,
    const LARGE_INTEGER* FileOffset,
    ULONG Length,
    BOOLEAN UninitializeCacheMaps
) {
    return TitanCacheManager::Instance().purgeCacheSection(SectionObjectPointer, FileOffset, Length, UninitializeCacheMaps);
}

inline uint32_t WINAPI CcLazyWriteScan() {
    return TitanCacheManager::Instance().lazyWriteScan();
}

inline void WINAPI CcSetDirtyPageThreshold(
    CC_FILE_OBJECT* FileObject,
    ULONG DirtyPageThreshold
) {
    TitanCacheManager::Instance().setDirtyPageThreshold(FileObject, DirtyPageThreshold);
}

inline BOOLEAN WINAPI CcCanIWrite(
    CC_FILE_OBJECT* FileObject,
    ULONG BytesToWrite,
    BOOLEAN Wait,
    BOOLEAN Retrying
) {
    return TitanCacheManager::Instance().canIWrite(FileObject, BytesToWrite, Wait, Retrying);
}

inline void WINAPI CcDeferWrite(
    CC_FILE_OBJECT* FileObject,
    POST_DEFERRED_WRITE PostRoutine,
    PVOID Context1,
    PVOID Context2,
    ULONG BytesToWrite,
    BOOLEAN Retrying
) {
    TitanCacheManager::Instance().deferWrite(FileObject, PostRoutine, Context1, Context2, BytesToWrite, Retrying);
}

inline BOOL WINAPI CcGetCacheTelemetry(CC_CACHE_TELEMETRY* outTelemetry) {
    if (!outTelemetry) return FALSE_VAL;
    *outTelemetry = TitanCacheManager::Instance().getTelemetry();
    return TRUE_VAL;
}

} // extern "C"

inline CC_CACHE_TELEMETRY CcQueryCacheTelemetry() noexcept {
    return TitanCacheManager::Instance().getTelemetry();
}

// ============================================================================
// 7. Subsystem Export Registration (JanusLDR, SCM, VersionDatabase)
// ============================================================================

inline void InitializeCcCacheSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    // Register ntoskrnl.exe Cache Manager exports
    ldr.registerExport("ntoskrnl.exe", "CcInitializeCacheMap", reinterpret_cast<void*>(CcInitializeCacheMap));
    ldr.registerExport("ntoskrnl.exe", "CcUninitializeCacheMap", reinterpret_cast<void*>(CcUninitializeCacheMap));
    ldr.registerExport("ntoskrnl.exe", "CcSetFileSizes", reinterpret_cast<void*>(CcSetFileSizes));
    ldr.registerExport("ntoskrnl.exe", "CcGetFileSizePointer", reinterpret_cast<void*>(CcGetFileSizePointer));
    ldr.registerExport("ntoskrnl.exe", "CcSetAdditionalCacheAttributes", reinterpret_cast<void*>(CcSetAdditionalCacheAttributes));
    ldr.registerExport("ntoskrnl.exe", "CcSetLogHandleForFile", reinterpret_cast<void*>(CcSetLogHandleForFile));
    ldr.registerExport("ntoskrnl.exe", "CcCopyRead", reinterpret_cast<void*>(CcCopyRead));
    ldr.registerExport("ntoskrnl.exe", "CcCopyWrite", reinterpret_cast<void*>(CcCopyWrite));
    ldr.registerExport("ntoskrnl.exe", "CcFastCopyRead", reinterpret_cast<void*>(CcFastCopyRead));
    ldr.registerExport("ntoskrnl.exe", "CcFastCopyWrite", reinterpret_cast<void*>(CcFastCopyWrite));
    ldr.registerExport("ntoskrnl.exe", "CcZeroData", reinterpret_cast<void*>(CcZeroData));
    ldr.registerExport("ntoskrnl.exe", "CcMdlRead", reinterpret_cast<void*>(CcMdlRead));
    ldr.registerExport("ntoskrnl.exe", "CcMdlReadComplete", reinterpret_cast<void*>(CcMdlReadComplete));
    ldr.registerExport("ntoskrnl.exe", "CcPrepareMdlWrite", reinterpret_cast<void*>(CcPrepareMdlWrite));
    ldr.registerExport("ntoskrnl.exe", "CcMdlWriteComplete", reinterpret_cast<void*>(CcMdlWriteComplete));
    ldr.registerExport("ntoskrnl.exe", "CcMapData", reinterpret_cast<void*>(CcMapData));
    ldr.registerExport("ntoskrnl.exe", "CcPinMappedData", reinterpret_cast<void*>(CcPinMappedData));
    ldr.registerExport("ntoskrnl.exe", "CcPinRead", reinterpret_cast<void*>(CcPinRead));
    ldr.registerExport("ntoskrnl.exe", "CcPreparePinWrite", reinterpret_cast<void*>(CcPreparePinWrite));
    ldr.registerExport("ntoskrnl.exe", "CcSetDirtyPinnedData", reinterpret_cast<void*>(CcSetDirtyPinnedData));
    ldr.registerExport("ntoskrnl.exe", "CcRepinBcb", reinterpret_cast<void*>(CcRepinBcb));
    ldr.registerExport("ntoskrnl.exe", "CcUnpinData", reinterpret_cast<void*>(CcUnpinData));
    ldr.registerExport("ntoskrnl.exe", "CcSetReadAheadGranularity", reinterpret_cast<void*>(CcSetReadAheadGranularity));
    ldr.registerExport("ntoskrnl.exe", "CcScheduleReadAhead", reinterpret_cast<void*>(CcScheduleReadAhead));
    ldr.registerExport("ntoskrnl.exe", "CcFlushCache", reinterpret_cast<void*>(CcFlushCache));
    ldr.registerExport("ntoskrnl.exe", "CcPurgeCacheSection", reinterpret_cast<void*>(CcPurgeCacheSection));
    ldr.registerExport("ntoskrnl.exe", "CcLazyWriteScan", reinterpret_cast<void*>(CcLazyWriteScan));
    ldr.registerExport("ntoskrnl.exe", "CcSetDirtyPageThreshold", reinterpret_cast<void*>(CcSetDirtyPageThreshold));
    ldr.registerExport("ntoskrnl.exe", "CcCanIWrite", reinterpret_cast<void*>(CcCanIWrite));
    ldr.registerExport("ntoskrnl.exe", "CcDeferWrite", reinterpret_cast<void*>(CcDeferWrite));
    ldr.registerExport("ntoskrnl.exe", "CcGetCacheTelemetry", reinterpret_cast<void*>(CcGetCacheTelemetry));

    auto& scm = scm::ServiceControlManager::get();
    auto ccSvc = std::make_shared<scm::ServiceRecord>();
    ccSvc->serviceName           = L"CcCacheMgr";
    ccSvc->displayName           = L"Sovereign NT Executive Virtual Block Cache Manager";
    ccSvc->serviceType           = scm::SERVICE_KERNEL_DRIVER;
    ccSvc->startType             = scm::SERVICE_BOOT_START;
    ccSvc->errorControl          = scm::SERVICE_ERROR_CRITICAL;
    ccSvc->binaryPath            = L"\\SystemRoot\\System32\\ntoskrnl.exe";
    ccSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(ccSvc);

    version::VersionDatabase::Instance().RegisterModule(
        "cc_executive", "10.0.26100.1", "Sovereign NT Executive Virtual Block Cache Manager");

    TitanCacheManager::Instance().initialize();
}

} // namespace micant::km::cc

// Forward into micant parent namespace
namespace micant {
    using km::cc::TitanCacheManager;
    using km::cc::CC_FILE_SIZES;
    using km::cc::CACHE_MANAGER_CALLBACKS;
    using km::cc::CC_FILE_OBJECT;
    using km::cc::VACB;
    using km::cc::PUBLIC_BCB;
    using km::cc::CC_MDL;
    using km::cc::PRIVATE_CACHE_MAP;
    using km::cc::SHARED_CACHE_MAP;
    using km::cc::CC_CACHE_TELEMETRY;
    using km::cc::CcIsFileCached;
    using km::cc::CcInitializeCacheMap;
    using km::cc::CcUninitializeCacheMap;
    using km::cc::CcSetFileSizes;
    using km::cc::CcGetFileSizePointer;
    using km::cc::CcSetAdditionalCacheAttributes;
    using km::cc::CcSetLogHandleForFile;
    using km::cc::CcCopyRead;
    using km::cc::CcCopyWrite;
    using km::cc::CcFastCopyRead;
    using km::cc::CcFastCopyWrite;
    using km::cc::CcZeroData;
    using km::cc::CcMdlRead;
    using km::cc::CcMdlReadComplete;
    using km::cc::CcPrepareMdlWrite;
    using km::cc::CcMdlWriteComplete;
    using km::cc::CcMapData;
    using km::cc::CcPinMappedData;
    using km::cc::CcPinRead;
    using km::cc::CcPreparePinWrite;
    using km::cc::CcSetDirtyPinnedData;
    using km::cc::CcRepinBcb;
    using km::cc::CcUnpinData;
    using km::cc::CcSetReadAheadGranularity;
    using km::cc::CcScheduleReadAhead;
    using km::cc::CcFlushCache;
    using km::cc::CcPurgeCacheSection;
    using km::cc::CcLazyWriteScan;
    using km::cc::CcSetDirtyPageThreshold;
    using km::cc::CcCanIWrite;
    using km::cc::CcDeferWrite;
    using km::cc::CcGetCacheTelemetry;
    using km::cc::CcQueryCacheTelemetry;
    using km::cc::InitializeCcCacheSubsystemExports;
} // namespace micant
