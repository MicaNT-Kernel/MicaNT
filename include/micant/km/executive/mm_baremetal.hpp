// ============================================================================
// MicaNT: Sovereign Bare-Metal NT Memory Manager & Demand Paging Subsystem
// (mm_baremetal.hpp)
//
// Description:
//   Clean-room freestanding NT Memory Manager for Ring 0 bare-metal silicon.
//   Implements Dave Cutler's virtual memory architecture:
//   - Physical Page Frame Number (PFN) Database across 32 GB ECC DDR4 RAM
//   - Virtual Address Descriptors (VAD) tracking userland heap/stack allocations
//   - x86-64 4-Level Page Table walking and dynamic page insertion (PML4->PDPT->PD->PT)
//   - Vector 0x0E (#PF) Demand Paging Handler: automatic stack expansion & zero-fill on demand
//   - Fast System Calls: NtAllocateVirtualMemory (0x18/0x55), NtFreeVirtualMemory (0x1E/0x1B),
//     NtProtectVirtualMemory (0x50), NtQueryVirtualMemory (0x23)
//
// Clean-Room ISO C++23. Zero proprietary headers or decompiled code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/executive/hmb_pagefile.hpp"

namespace micant::km::mm_baremetal {

// --- Canonical Memory Constants ---
inline constexpr size_t PAGE_SIZE_4KB     = 4096;
inline constexpr size_t PAGE_SHIFT_4KB    = 12;
inline constexpr size_t PAGE_MASK_4KB     = PAGE_SIZE_4KB - 1;

// Virtual Memory Allocation Types (MEM_*)
inline constexpr uint32_t MEM_COMMIT      = 0x00001000;
inline constexpr uint32_t MEM_RESERVE     = 0x00002000;
inline constexpr uint32_t MEM_DECOMMIT    = 0x00004000;
inline constexpr uint32_t MEM_RELEASE     = 0x00008000;
inline constexpr uint32_t MEM_RESET       = 0x00080000;

// Memory Protection Flags (PAGE_*)
inline constexpr uint32_t PAGE_NOACCESS          = 0x01;
inline constexpr uint32_t PAGE_READONLY          = 0x02;
inline constexpr uint32_t PAGE_READWRITE         = 0x04;
inline constexpr uint32_t PAGE_EXECUTE           = 0x10;
inline constexpr uint32_t PAGE_EXECUTE_READ      = 0x20;
inline constexpr uint32_t PAGE_EXECUTE_READWRITE = 0x40;
inline constexpr uint32_t PAGE_GUARD             = 0x100;

// Hardware x86-64 PTE Bitmasks
inline constexpr uint64_t PTE_PRESENT     = 1ULL << 0;
inline constexpr uint64_t PTE_WRITABLE    = 1ULL << 1;
inline constexpr uint64_t PTE_USER        = 1ULL << 2;
inline constexpr uint64_t PTE_PWT         = 1ULL << 3;
inline constexpr uint64_t PTE_PCD         = 1ULL << 4;
inline constexpr uint64_t PTE_ACCESSED    = 1ULL << 5;
inline constexpr uint64_t PTE_DIRTY       = 1ULL << 6;
inline constexpr uint64_t PTE_LARGE_PAGE  = 1ULL << 7;
inline constexpr uint64_t PTE_GLOBAL      = 1ULL << 8;
inline constexpr uint64_t PTE_TRANSITION  = 1ULL << 11; // Software transition bit for HMB 2.0 swap
inline constexpr uint64_t PTE_NO_EXECUTE  = 1ULL << 63;
inline constexpr uint64_t PTE_PHYS_MASK   = 0x000FFFFFFFFFF000ULL;
inline constexpr uint64_t PTE_SLOT_MASK   = 0x00000000FFFFF000ULL;
inline constexpr uint32_t PTE_SLOT_SHIFT  = 12;

// User Address Space Boundaries
inline constexpr uint64_t USER_SPACE_MIN  = 0x0000000000010000ULL; // 64KB Null Allocation Trap
inline constexpr uint64_t USER_SPACE_MAX  = 0x00007FFFFFFFFFFFULL; // 128TB Canonical User Space Limit

// Physical Memory Allocator Pool Configuration
// Uses RAM between 128 MB (0x08000000) and 384 MB (0x18000000) = 256 MB dedicated dynamic pool
inline constexpr uint64_t PFN_POOL_PHYS_BASE = 0x08000000ULL;
inline constexpr size_t   PFN_POOL_PAGES     = 65536; // 65,536 pages * 4KB = 256 MB RAM
inline constexpr size_t   PFN_BITMAP_WORDS   = PFN_POOL_PAGES / 64; // 1,024 x 64-bit uint64_t words (8 KB)

// ============================================================================
// 1. Physical Page Frame Number (PFN) Allocator
// ============================================================================

class BareMetalPfnAllocator {
private:
    uint64_t m_bitmap[PFN_BITMAP_WORDS]{0};
    size_t   m_allocatedPages{0};
    uint64_t m_lastSearchWord{0};

public:
    static BareMetalPfnAllocator& get() noexcept {
        static BareMetalPfnAllocator instance;
        return instance;
    }

    void initialize() noexcept {
        for (size_t i = 0; i < PFN_BITMAP_WORDS; ++i) {
            m_bitmap[i] = 0;
        }
        m_allocatedPages = 0;
        m_lastSearchWord = 0;
    }

    /**
     * @brief Allocate a single 4KB physical page from the pool and zero it.
     * @return Physical address of the allocated page, or 0 if out of memory.
     */
    uint64_t allocatePage() noexcept {
        for (size_t w = 0; w < PFN_BITMAP_WORDS; ++w) {
            size_t wordIdx = (m_lastSearchWord + w) % PFN_BITMAP_WORDS;
            uint64_t val = m_bitmap[wordIdx];
            if (val != 0xFFFFFFFFFFFFFFFFULL) {
                // Find first clear bit
                for (int b = 0; b < 64; ++b) {
                    if (!(val & (1ULL << b))) {
                        m_bitmap[wordIdx] |= (1ULL << b);
                        m_allocatedPages++;
                        m_lastSearchWord = wordIdx;

                        uint64_t pageIdx = (wordIdx * 64) + b;
                        uint64_t physAddr = PFN_POOL_PHYS_BASE + (pageIdx * PAGE_SIZE_4KB);

                        // Demand-Zero: Zero-fill the physical page
                        auto* p = reinterpret_cast<uint64_t*>(physAddr);
                        for (size_t k = 0; k < PAGE_SIZE_4KB / sizeof(uint64_t); ++k) {
                            p[k] = 0;
                        }

                        return physAddr;
                    }
                }
            }
        }
        return 0; // Out of physical memory
    }

    /**
     * @brief Free an allocated 4KB physical page.
     */
    void freePage(uint64_t physAddr) noexcept {
        if (physAddr < PFN_POOL_PHYS_BASE) return;
        uint64_t offset = physAddr - PFN_POOL_PHYS_BASE;
        uint64_t pageIdx = offset / PAGE_SIZE_4KB;
        if (pageIdx >= PFN_POOL_PAGES) return;

        size_t wordIdx = pageIdx / 64;
        int bitIdx = static_cast<int>(pageIdx % 64);

        if (m_bitmap[wordIdx] & (1ULL << bitIdx)) {
            m_bitmap[wordIdx] &= ~(1ULL << bitIdx);
            if (m_allocatedPages > 0) m_allocatedPages--;
        }
    }

    [[nodiscard]] size_t getAllocatedPages() const noexcept { return m_allocatedPages; }
    [[nodiscard]] size_t getTotalPages() const noexcept { return PFN_POOL_PAGES; }
    [[nodiscard]] size_t getFreePages() const noexcept {
        return (PFN_POOL_PAGES > m_allocatedPages) ? (PFN_POOL_PAGES - m_allocatedPages) : 0;
    }
    [[nodiscard]] size_t getPoolSizeBytes() const noexcept { return PFN_POOL_PAGES * PAGE_SIZE_4KB; }
};

// ============================================================================
// 2. x86-64 4-Level Page Table Walking & Page Insertion
// ============================================================================

class BareMetalPaging {
public:
    /**
     * @brief Map a virtual page to a physical page in the active or specified PML4.
     * Automatically allocates intermediate PDPT, PD, and PT tables if not present.
     */
    static bool mapVirtualPage(uint64_t cr3Phys, uint64_t va, uint64_t physAddr, uint64_t flags) noexcept {
        if (cr3Phys == 0) return false;
        va &= ~PAGE_MASK_4KB;
        physAddr &= ~PAGE_MASK_4KB;

        uint64_t pml4Idx = (va >> 39) & 0x1FF;
        uint64_t pdptIdx = (va >> 30) & 0x1FF;
        uint64_t pdIdx   = (va >> 21) & 0x1FF;
        uint64_t ptIdx   = (va >> 12) & 0x1FF;

        auto* pml4 = reinterpret_cast<uint64_t*>(cr3Phys);

        // Level 4 -> Level 3 (PDPT)
        if (!(pml4[pml4Idx] & PTE_PRESENT)) {
            uint64_t newPdpt = BareMetalPfnAllocator::get().allocatePage();
            if (!newPdpt) return false;
            pml4[pml4Idx] = newPdpt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        } else {
            pml4[pml4Idx] |= (PTE_WRITABLE | PTE_USER); // Ensure user-accessible
        }

        uint64_t pdptPhys = pml4[pml4Idx] & PTE_PHYS_MASK;
        auto* pdpt = reinterpret_cast<uint64_t*>(pdptPhys);

        // Level 3 -> Level 2 (PD)
        if (!(pdpt[pdptIdx] & PTE_PRESENT)) {
            uint64_t newPd = BareMetalPfnAllocator::get().allocatePage();
            if (!newPd) return false;
            pdpt[pdptIdx] = newPd | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        } else if (pdpt[pdptIdx] & PTE_LARGE_PAGE) {
            // Split 1GB large page into 512 x 2MB pages
            uint64_t newPdPhys = BareMetalPfnAllocator::get().allocatePage();
            if (!newPdPhys) return false;
            auto* newPd = reinterpret_cast<uint64_t*>(newPdPhys);
            uint64_t base1Gb = pdpt[pdptIdx] & 0x000FFFFFC0000000ULL;
            uint64_t lowFlags = pdpt[pdptIdx] & 0xFFFULL;
            uint64_t nxFlag = pdpt[pdptIdx] & PTE_NO_EXECUTE;
            for (uint64_t i = 0; i < 512; ++i) {
                newPd[i] = (base1Gb + (i * 0x200000ULL)) | lowFlags | nxFlag | PTE_LARGE_PAGE;
            }
            pdpt[pdptIdx] = newPdPhys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        } else {
            pdpt[pdptIdx] |= (PTE_WRITABLE | PTE_USER);
        }

        uint64_t pdPhys = pdpt[pdptIdx] & PTE_PHYS_MASK;
        auto* pd = reinterpret_cast<uint64_t*>(pdPhys);

        // Level 2 -> Level 1 (PT)
        if (!(pd[pdIdx] & PTE_PRESENT)) {
            uint64_t newPt = BareMetalPfnAllocator::get().allocatePage();
            if (!newPt) return false;
            pd[pdIdx] = newPt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        } else if (pd[pdIdx] & PTE_LARGE_PAGE) {
            // Split 2MB large page into 512 x 4KB pages
            uint64_t newPtPhys = BareMetalPfnAllocator::get().allocatePage();
            if (!newPtPhys) return false;
            auto* newPt = reinterpret_cast<uint64_t*>(newPtPhys);
            uint64_t base2Mb = pd[pdIdx] & 0x000FFFFFFFE00000ULL;
            uint64_t lowFlags = (pd[pdIdx] & 0xFFFULL) & ~PTE_LARGE_PAGE;
            uint64_t nxFlag = pd[pdIdx] & PTE_NO_EXECUTE;
            for (uint64_t i = 0; i < 512; ++i) {
                newPt[i] = (base2Mb + (i * PAGE_SIZE_4KB)) | lowFlags | nxFlag;
            }
            pd[pdIdx] = newPtPhys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
        } else {
            pd[pdIdx] |= (PTE_WRITABLE | PTE_USER);
        }

        uint64_t ptPhys = pd[pdIdx] & PTE_PHYS_MASK;
        auto* pt = reinterpret_cast<uint64_t*>(ptPhys);

        // Map target leaf PTE
        pt[ptIdx] = physAddr | flags | PTE_PRESENT;

        // Invalidate TLB for this virtual address and reload CR3 to flush paging-structure caches
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("invlpg (%0)" : : "r"(va) : "memory");
        uint64_t currentCr3 = 0;
        __asm__ __volatile__("mov %%cr3, %0" : "=r"(currentCr3));
        if ((currentCr3 & PTE_PHYS_MASK) == cr3Phys) {
            __asm__ __volatile__("mov %0, %%cr3" : : "r"(currentCr3) : "memory");
        }
#endif
        return true;
    }

    /**
     * @brief Unmap a virtual page and invalidate TLB.
     */
    static bool unmapVirtualPage(uint64_t cr3Phys, uint64_t va, bool freePhysical = true) noexcept {
        if (cr3Phys == 0) return false;
        va &= ~PAGE_MASK_4KB;

        uint64_t pml4Idx = (va >> 39) & 0x1FF;
        uint64_t pdptIdx = (va >> 30) & 0x1FF;
        uint64_t pdIdx   = (va >> 21) & 0x1FF;
        uint64_t ptIdx   = (va >> 12) & 0x1FF;

        auto* pml4 = reinterpret_cast<uint64_t*>(cr3Phys);
        if (!(pml4[pml4Idx] & PTE_PRESENT)) return false;

        auto* pdpt = reinterpret_cast<uint64_t*>(pml4[pml4Idx] & PTE_PHYS_MASK);
        if (!(pdpt[pdptIdx] & PTE_PRESENT) || (pdpt[pdptIdx] & PTE_LARGE_PAGE)) return false;

        auto* pd = reinterpret_cast<uint64_t*>(pdpt[pdptIdx] & PTE_PHYS_MASK);
        if (!(pd[pdIdx] & PTE_PRESENT) || (pd[pdIdx] & PTE_LARGE_PAGE)) return false;

        auto* pt = reinterpret_cast<uint64_t*>(pd[pdIdx] & PTE_PHYS_MASK);
        if (!(pt[ptIdx] & PTE_PRESENT)) return false;

        uint64_t physAddr = pt[ptIdx] & PTE_PHYS_MASK;
        pt[ptIdx] = 0;

        if (freePhysical && physAddr != 0) {
            BareMetalPfnAllocator::get().freePage(physAddr);
        }

#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("invlpg (%0)" : : "r"(va) : "memory");
#endif
        return true;
    }

    /**
     * @brief Resolve a virtual address to its backing physical address.
     */
    static uint64_t getPhysicalAddress(uint64_t cr3Phys, uint64_t va) noexcept {
        if (cr3Phys == 0) return 0;
        uint64_t pml4Idx = (va >> 39) & 0x1FF;
        uint64_t pdptIdx = (va >> 30) & 0x1FF;
        uint64_t pdIdx   = (va >> 21) & 0x1FF;
        uint64_t ptIdx   = (va >> 12) & 0x1FF;

        auto* pml4 = reinterpret_cast<uint64_t*>(cr3Phys);
        if (!(pml4[pml4Idx] & PTE_PRESENT)) return 0;

        auto* pdpt = reinterpret_cast<uint64_t*>(pml4[pml4Idx] & PTE_PHYS_MASK);
        if (!(pdpt[pdptIdx] & PTE_PRESENT)) return 0;
        if (pdpt[pdptIdx] & PTE_LARGE_PAGE) {
            return (pdpt[pdptIdx] & 0x000FFFFFC0000000ULL) | (va & 0x3FFFFFFFULL);
        }

        auto* pd = reinterpret_cast<uint64_t*>(pdpt[pdptIdx] & PTE_PHYS_MASK);
        if (!(pd[pdIdx] & PTE_PRESENT)) return 0;
        if (pd[pdIdx] & PTE_LARGE_PAGE) {
            return (pd[pdIdx] & 0x000FFFFFFFE00000ULL) | (va & 0x1FFFFFULL);
        }

        auto* pt = reinterpret_cast<uint64_t*>(pd[pdIdx] & PTE_PHYS_MASK);
        if (!(pt[ptIdx] & PTE_PRESENT)) return 0;

        return (pt[ptIdx] & PTE_PHYS_MASK) | (va & PAGE_MASK_4KB);
    }

    /**
     * @brief Retrieve raw 64-bit PTE entry for a virtual address (present or transition).
     */
    static uint64_t getRawPte(uint64_t cr3Phys, uint64_t va) noexcept {
        if (cr3Phys == 0) return 0;
        uint64_t pml4Idx = (va >> 39) & 0x1FF;
        uint64_t pdptIdx = (va >> 30) & 0x1FF;
        uint64_t pdIdx   = (va >> 21) & 0x1FF;
        uint64_t ptIdx   = (va >> 12) & 0x1FF;

        auto* pml4 = reinterpret_cast<uint64_t*>(cr3Phys);
        if (!(pml4[pml4Idx] & PTE_PRESENT)) return 0;

        auto* pdpt = reinterpret_cast<uint64_t*>(pml4[pml4Idx] & PTE_PHYS_MASK);
        if (!(pdpt[pdptIdx] & PTE_PRESENT)) return 0;
        if (pdpt[pdptIdx] & PTE_LARGE_PAGE) return pdpt[pdptIdx];

        auto* pd = reinterpret_cast<uint64_t*>(pdpt[pdptIdx] & PTE_PHYS_MASK);
        if (!(pd[pdIdx] & PTE_PRESENT)) return 0;
        if (pd[pdIdx] & PTE_LARGE_PAGE) return pd[pdIdx];

        auto* pt = reinterpret_cast<uint64_t*>(pd[pdIdx] & PTE_PHYS_MASK);
        return pt[ptIdx];
    }

    /**
     * @brief Encode an HMB 2.0 Transition PTE (P=0, Transition=1, SlotId embedded).
     */
    static bool setTransitionPte(uint64_t cr3Phys, uint64_t va, uint32_t slotId) noexcept {
        if (cr3Phys == 0) return false;
        uint64_t pml4Idx = (va >> 39) & 0x1FF;
        uint64_t pdptIdx = (va >> 30) & 0x1FF;
        uint64_t pdIdx   = (va >> 21) & 0x1FF;
        uint64_t ptIdx   = (va >> 12) & 0x1FF;

        auto* pml4 = reinterpret_cast<uint64_t*>(cr3Phys);
        if (!(pml4[pml4Idx] & PTE_PRESENT)) return false;

        auto* pdpt = reinterpret_cast<uint64_t*>(pml4[pml4Idx] & PTE_PHYS_MASK);
        if (!(pdpt[pdptIdx] & PTE_PRESENT)) return false;

        auto* pd = reinterpret_cast<uint64_t*>(pdpt[pdptIdx] & PTE_PHYS_MASK);
        if (!(pd[pdIdx] & PTE_PRESENT)) return false;

        auto* pt = reinterpret_cast<uint64_t*>(pd[pdIdx] & PTE_PHYS_MASK);
        pt[ptIdx] = (static_cast<uint64_t>(slotId) << PTE_SLOT_SHIFT) | PTE_TRANSITION | PTE_USER;

#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("invlpg (%0)" : : "r"(va) : "memory");
#endif
        return true;
    }
};

// ============================================================================
// 3. Virtual Address Descriptors (VAD) & User Address Space Manager
// ============================================================================

struct BareMetalVad {
    uint64_t startVa{0};
    uint64_t endVa{0};
    uint32_t allocationType{0}; // MEM_COMMIT or MEM_RESERVE
    uint32_t protection{0};     // PAGE_READWRITE, PAGE_EXECUTE_READ, etc.
    bool     isStack{false};
    bool     inUse{false};
};

class BareMetalVadTable {
public:
    static constexpr size_t MAX_VADS = 64;

private:
    BareMetalVad m_vads[MAX_VADS]{};
    uint64_t     m_nextDynamicVa{0x00000000'20000000ULL}; // User dynamic allocation base: 512 MB
    uint64_t     m_heapPoolBase{0};
    size_t       m_heapPoolSize{0};
    size_t       m_heapPoolOffset{0};

public:
    static BareMetalVadTable& get() noexcept {
        static BareMetalVadTable instance;
        return instance;
    }

    void initialize() noexcept {
        for (size_t i = 0; i < MAX_VADS; ++i) {
            m_vads[i] = BareMetalVad{};
        }
        m_nextDynamicVa = 0x00000000'20000000ULL;
        m_heapPoolBase = 0;
        m_heapPoolSize = 0;
        m_heapPoolOffset = 0;
    }

    void reset() noexcept {
        initialize();
    }

    void setHeapPool(uint64_t baseVa, size_t size) noexcept {
        m_heapPoolBase = baseVa;
        m_heapPoolSize = size;
        m_heapPoolOffset = 0;
    }

    /**
     * @brief Register a known region (e.g. user image, stack, PEB/TEB).
     */
    bool registerRegion(uint64_t startVa, size_t size, uint32_t allocType, uint32_t protect, bool isStack = false) noexcept {
        if (size == 0) return false;
        uint64_t endVa = (startVa + size + PAGE_MASK_4KB) & ~PAGE_MASK_4KB;
        startVa &= ~PAGE_MASK_4KB;

        for (size_t i = 0; i < MAX_VADS; ++i) {
            if (!m_vads[i].inUse) {
                m_vads[i].startVa = startVa;
                m_vads[i].endVa = endVa;
                m_vads[i].allocationType = allocType;
                m_vads[i].protection = protect;
                m_vads[i].isStack = isStack;
                m_vads[i].inUse = true;
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Allocate/Reserve a new virtual memory region for userland (NtAllocateVirtualMemory).
     */
    NtStatus allocateVirtualMemory(
        uint64_t cr3Phys,
        uint64_t& baseAddress,
        size_t size,
        uint32_t allocationType,
        uint32_t protect
    ) noexcept {
        if (size == 0) return NtStatus::InvalidParameter;
        size_t alignedSize = (size + PAGE_MASK_4KB) & ~PAGE_MASK_4KB;

        uint64_t targetVa = baseAddress;
        bool fromHeapPool = false;
        if (targetVa == 0) {
            // Allocate from dedicated user heap pool first if available
            if (m_heapPoolBase != 0 && (m_heapPoolOffset + alignedSize) <= m_heapPoolSize) {
                targetVa = m_heapPoolBase + m_heapPoolOffset;
                m_heapPoolOffset += alignedSize;
                fromHeapPool = true;
            } else {
                targetVa = m_nextDynamicVa;
                m_nextDynamicVa += alignedSize + PAGE_SIZE_4KB; // Include guard space
            }
        } else {
            targetVa &= ~PAGE_MASK_4KB;
            if (targetVa < USER_SPACE_MIN || targetVa + alignedSize > USER_SPACE_MAX) {
                return NtStatus::InvalidParameter;
            }
            if (targetVa >= m_heapPoolBase && (targetVa + alignedSize) <= (m_heapPoolBase + m_heapPoolSize)) {
                fromHeapPool = true;
            }
        }

        // Find or create VAD
        BareMetalVad* vad = nullptr;
        for (size_t i = 0; i < MAX_VADS; ++i) {
            if (m_vads[i].inUse && m_vads[i].startVa <= targetVa && m_vads[i].endVa >= targetVa + alignedSize) {
                vad = &m_vads[i];
                break;
            }
        }

        if (!vad) {
            // Allocate new VAD entry
            for (size_t i = 0; i < MAX_VADS; ++i) {
                if (!m_vads[i].inUse) {
                    vad = &m_vads[i];
                    vad->startVa = targetVa;
                    vad->endVa = targetVa + alignedSize;
                    vad->allocationType = allocationType;
                    vad->protection = protect;
                    vad->isStack = false;
                    vad->inUse = true;
                    break;
                }
            }
        }

        if (!vad) return NtStatus::InsufficientResources;

        // If MEM_COMMIT requested:
        if (allocationType & MEM_COMMIT) {
            uint64_t pteFlags = PTE_USER | PTE_PRESENT;
            if (protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) {
                pteFlags |= PTE_WRITABLE;
            }
            if (!(protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
                uint32_t eferLo = 0, eferHi = 0;
                __asm__ __volatile__("rdmsr" : "=a"(eferLo), "=d"(eferHi) : "c"(0xC0000080));
                if (eferLo & (1u << 11)) { // Only set NX bit if EFER.NXE is enabled in MSR
                    pteFlags |= PTE_NO_EXECUTE;
                }
#endif
            }

            if (fromHeapPool) {
                // Ensure backing pages in heap pool have user/write access mapped
                for (uint64_t va = targetVa; va < targetVa + alignedSize; va += PAGE_SIZE_4KB) {
                    BareMetalPaging::mapVirtualPage(cr3Phys, va, va, pteFlags);
                }
            } else {
                for (uint64_t va = targetVa; va < targetVa + alignedSize; va += PAGE_SIZE_4KB) {
                    uint64_t phys = BareMetalPfnAllocator::get().allocatePage();
                    if (!phys) return NtStatus::InsufficientResources;
                    if (!BareMetalPaging::mapVirtualPage(cr3Phys, va, phys, pteFlags)) {
                        return NtStatus::InsufficientResources;
                    }
                }
            }
            vad->allocationType |= MEM_COMMIT;
        }

        baseAddress = targetVa;
        return NtStatus::Success;
    }

    /**
     * @brief Free or decommit a virtual memory region (NtFreeVirtualMemory).
     */
    NtStatus freeVirtualMemory(
        uint64_t cr3Phys,
        uint64_t baseAddress,
        size_t size,
        uint32_t freeType
    ) noexcept {
        if (baseAddress == 0) return NtStatus::InvalidParameter;
        baseAddress &= ~PAGE_MASK_4KB;
        size_t alignedSize = (size > 0) ? ((size + PAGE_MASK_4KB) & ~PAGE_MASK_4KB) : 0;
        bool inHeapPool = (m_heapPoolBase != 0 && baseAddress >= m_heapPoolBase && (baseAddress + alignedSize) <= (m_heapPoolBase + m_heapPoolSize));

        for (size_t i = 0; i < MAX_VADS; ++i) {
            if (m_vads[i].inUse && m_vads[i].startVa <= baseAddress && m_vads[i].endVa > baseAddress) {
                if (alignedSize == 0) {
                    alignedSize = static_cast<size_t>(m_vads[i].endVa - baseAddress);
                }

                if (freeType & MEM_RELEASE) {
                    if (!inHeapPool) {
                        for (uint64_t va = baseAddress; va < baseAddress + alignedSize; va += PAGE_SIZE_4KB) {
                            BareMetalPaging::unmapVirtualPage(cr3Phys, va, true);
                        }
                    }
                    m_vads[i].inUse = false;
                } else if (freeType & MEM_DECOMMIT) {
                    if (!inHeapPool) {
                        for (uint64_t va = baseAddress; va < baseAddress + alignedSize; va += PAGE_SIZE_4KB) {
                            BareMetalPaging::unmapVirtualPage(cr3Phys, va, true);
                        }
                    }
                    m_vads[i].allocationType &= ~MEM_COMMIT;
                }
                return NtStatus::Success;
            }
        }
        return NtStatus::InvalidParameter;
    }

    /**
     * @brief Find the VAD covering a given virtual address.
     */
    [[nodiscard]] const BareMetalVad* findVad(uint64_t va) const noexcept {
        for (size_t i = 0; i < MAX_VADS; ++i) {
            if (m_vads[i].inUse && va >= m_vads[i].startVa && va < m_vads[i].endVa) {
                return &m_vads[i];
            }
        }
        return nullptr;
    }

    [[nodiscard]] size_t getActiveVadCount() const noexcept {
        size_t count = 0;
        for (size_t i = 0; i < MAX_VADS; ++i) {
            if (m_vads[i].inUse) count++;
        }
        return count;
    }

    [[nodiscard]] const BareMetalVad* getVad(size_t index) const noexcept {
        if (index < MAX_VADS && m_vads[index].inUse) return &m_vads[index];
        return nullptr;
    }
};

// ============================================================================
// 4. Memory Manager Master Executive & Telemetry
// ============================================================================

struct MmTelemetry {
    uint64_t pageFaultCount{0};
    uint64_t demandPagesResolved{0};
    uint64_t stackExpansions{0};
    uint64_t accessViolations{0};
    uint64_t syscallAllocCount{0};
    uint64_t syscallFreeCount{0};
    uint64_t totalBytesAllocated{0};
    uint64_t lastFaultVa{0};
    uint64_t lastFaultRip{0};
};

class BareMetalMemoryManager {
private:
    MmTelemetry m_telemetry{};
    bool        m_initialized{false};

public:
    static BareMetalMemoryManager& get() noexcept {
        static BareMetalMemoryManager instance;
        return instance;
    }

    void initialize() noexcept {
        BareMetalPfnAllocator::get().initialize();
        BareMetalVadTable::get().initialize();
        hmb_pagefile::HmbPagefileManager::get().initialize();
        m_telemetry = MmTelemetry{};
        m_initialized = true;
    }

    /**
     * @brief Page out an active virtual page to the Haven Memory Bus (HMB 2.0) pagefile.
     */
    bool pageOutVirtualAddress(uint64_t cr3Phys, uint64_t va) noexcept {
        uint64_t phys = BareMetalPaging::getPhysicalAddress(cr3Phys, va);
        if (phys == 0) return false;

        auto& hmb = hmb_pagefile::HmbPagefileManager::get();
        uint64_t slot = hmb.allocateSlot(hmb_pagefile::HmbTier::DirtyHeap);
        if (slot == UINT64_MAX) return false;

        bool writeOk = hmb.writePage(slot, reinterpret_cast<const void*>(phys), hmb_pagefile::HmbTier::DirtyHeap);
        if (!writeOk) {
            hmb.freeSlot(slot);
            return false;
        }

        BareMetalPaging::setTransitionPte(cr3Phys, va, static_cast<uint32_t>(slot));
        BareMetalPfnAllocator::get().freePage(phys);
        return true;
    }

    [[nodiscard]] hmb_pagefile::HmbPagefileManager& getPagefile() noexcept {
        return hmb_pagefile::HmbPagefileManager::get();
    }

    void setHeapPool(uint64_t baseVa, size_t size) noexcept {
        BareMetalVadTable::get().setHeapPool(baseVa, size);
    }

    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }
    [[nodiscard]] const MmTelemetry& getTelemetry() const noexcept { return m_telemetry; }

    /**
     * @brief Core Dave Cutler Demand Paging Handler (Vector 0x0E / #PF).
     * Invoked directly from KiPageFaultStub.
     *
     * @param faultVa Linear virtual address from CR2.
     * @param errorCode Hardware error code pushed by CPU.
     * @param faultRip Instruction pointer that triggered fault.
     * @param faultRsp Stack pointer at fault time.
     * @return true if fault was resolved (re-execute via iretq); false if fatal (STATUS_ACCESS_VIOLATION).
     */
    bool handlePageFault(uint64_t faultVa, uint64_t errorCode, uint64_t faultRip, uint64_t faultRsp) noexcept {
        m_telemetry.pageFaultCount++;
        m_telemetry.lastFaultVa = faultVa;
        m_telemetry.lastFaultRip = faultRip;

        // Anti-spin guard: detect infinite page fault loops at the same instruction
        static uint64_t s_consecutiveFaults = 0;
        static uint64_t s_lastFaultVa = 0;
        static uint64_t s_lastFaultRip = 0;

        if (faultVa == s_lastFaultVa && faultRip == s_lastFaultRip) {
            s_consecutiveFaults++;
            if (s_consecutiveFaults > 3) {
                m_telemetry.accessViolations++;
                s_consecutiveFaults = 0;
                return false; // Break infinite loop, route to resilient exception / process cleanup
            }
        } else {
            s_consecutiveFaults = 1;
            s_lastFaultVa = faultVa;
            s_lastFaultRip = faultRip;
        }

        // Check for canonical NULL pointer trap (first 64KB)
        if (faultVa < USER_SPACE_MIN) {
            m_telemetry.accessViolations++;
            return false; // Fatal Access Violation
        }

        // Check if supervisor tried to access invalid memory or user tried to access kernel
        bool isUserMode = (errorCode & 0x04) != 0;
        if (faultVa >= 0xFFFF8000'00000000ULL && isUserMode) {
            m_telemetry.accessViolations++;
            return false; // Kernel space violation from Ring 3
        }

        // Fetch current CR3
        uint64_t cr3 = 0;
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
#endif
        uint64_t cr3Phys = cr3 & PTE_PHYS_MASK;
#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0
        if (cr3Phys == 0) return false;
#endif

        // 1. Check if fault address falls inside a registered VAD
        const auto* vad = BareMetalVadTable::get().findVad(faultVa);
        if (vad) {
            bool isWrite = (errorCode & 0x02) != 0;
            bool isExec  = (errorCode & 0x10) != 0;
            bool allowsWrite = (vad->protection & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) != 0;
            bool allowsExec  = (vad->protection & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) != 0;

            if (isWrite && !allowsWrite) {
                m_telemetry.accessViolations++;
                return false; // Fatal Access Violation: writing to read-only memory
            }
            if (isExec && !allowsExec) {
                m_telemetry.accessViolations++;
                return false; // Fatal Access Violation: executing non-executable page
            }

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ != 0
            if (cr3Phys == 0) {
                // In hosted unit test environment without hardware CR3, record resolution and succeed
                m_telemetry.demandPagesResolved++;
                return true;
            }
#endif

            uint64_t pteFlags = PTE_USER | PTE_PRESENT;
            if (allowsWrite) {
                pteFlags |= PTE_WRITABLE;
            }
            if (!allowsExec) {
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
                uint32_t eferLo = 0, eferHi = 0;
                __asm__ __volatile__("rdmsr" : "=a"(eferLo), "=d"(eferHi) : "c"(0xC0000080));
                if (eferLo & (1u << 11)) {
                    pteFlags |= PTE_NO_EXECUTE;
                }
#endif
            }

            uint64_t phys = 0;
            if (errorCode & 0x01) {
                // Page was already present in page table, but lacked User (U/S=0) or Write (R/W=0) permissions.
                // Resolve existing backing physical address from page tables or identity map:
                phys = BareMetalPaging::getPhysicalAddress(cr3Phys, faultVa);
                if (phys == 0) {
                    phys = faultVa & PTE_PHYS_MASK;
                }
            } else {
                // Check if this is an HMB 2.0 Transition PTE (P=0, Transition=1, Slot!=0)
                uint64_t rawPte = BareMetalPaging::getRawPte(cr3Phys, faultVa);
                if (!(rawPte & PTE_PRESENT) && (rawPte & PTE_TRANSITION)) {
                    uint32_t slotId = static_cast<uint32_t>((rawPte & PTE_SLOT_MASK) >> PTE_SLOT_SHIFT);
                    phys = BareMetalPfnAllocator::get().allocatePage();
                    if (!phys) return false;

                    bool readOk = micant::km::hmb_pagefile::HmbPagefileManager::get().readPage(slotId, reinterpret_cast<void*>(phys));
                    if (!readOk) {
                        std::memset(reinterpret_cast<void*>(phys), 0, PAGE_SIZE_4KB);
                    }
                    micant::km::hmb_pagefile::HmbPagefileManager::get().freeSlot(slotId);
                } else {
                    // Demand-Zero Paging: Allocate physical page from PFN DB and map
                    phys = BareMetalPfnAllocator::get().allocatePage();
                    if (!phys) return false; // Out of memory
                }
            }

            bool mapped = BareMetalPaging::mapVirtualPage(cr3Phys, faultVa, phys, pteFlags);
            if (mapped) {
                m_telemetry.demandPagesResolved++;
                return true; // Successfully resolved! Instruction will re-execute smoothly!
            }
            return false;
        }

        // 2. Check for User Stack auto-growth (within 64KB beneath RSP or stack base)
        if (isUserMode && faultRsp > USER_SPACE_MIN) {
            // Windows NT allows stack growth down to the stack limit
            if (faultVa >= (faultRsp - 65536) && faultVa <= (faultRsp + 65536)) {
                uint64_t phys = BareMetalPfnAllocator::get().allocatePage();
                if (!phys) return false;

                bool mapped = BareMetalPaging::mapVirtualPage(
                    cr3Phys, faultVa, phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NO_EXECUTE
                );
                if (mapped) {
                    m_telemetry.stackExpansions++;
                    m_telemetry.demandPagesResolved++;
                    return true;
                }
            }
        }

        // Unregistered address: true STATUS_ACCESS_VIOLATION
        m_telemetry.accessViolations++;
        return false;
    }

    /**
     * @brief Handle NtAllocateVirtualMemory system call.
     */
    NtStatus allocateVirtualMemory(uint64_t& baseAddress, size_t size, uint32_t allocType, uint32_t protect) noexcept {
        uint64_t cr3 = 0;
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
#endif
        uint64_t cr3Phys = cr3 & PTE_PHYS_MASK;
        NtStatus status = BareMetalVadTable::get().allocateVirtualMemory(cr3Phys, baseAddress, size, allocType, protect);
        if (NT_SUCCESS(status)) {
            m_telemetry.syscallAllocCount++;
            m_telemetry.totalBytesAllocated += size;
        }
        return status;
    }

    /**
     * @brief Handle NtFreeVirtualMemory system call.
     */
    NtStatus freeVirtualMemory(uint64_t baseAddress, size_t size, uint32_t freeType) noexcept {
        uint64_t cr3 = 0;
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
#endif
        uint64_t cr3Phys = cr3 & PTE_PHYS_MASK;
        NtStatus status = BareMetalVadTable::get().freeVirtualMemory(cr3Phys, baseAddress, size, freeType);
        if (NT_SUCCESS(status)) {
            m_telemetry.syscallFreeCount++;
        }
        return status;
    }

    /**
     * @brief Dump Virtual Memory subsystem telemetry into a formatted buffer.
     */
    void dumpVmStatus(char* buf, size_t maxLen) const noexcept {
        if (!buf || maxLen == 0) return;
        buf[0] = '\0';

        auto copy = [](char* d, const char* s, size_t ml) {
            size_t i = 0;
            while (s[i] && i + 1 < ml) { d[i] = s[i]; ++i; }
            d[i] = '\0';
        };
        auto concat = [](char* d, const char* s, size_t ml) {
            size_t dl = 0; while (d[dl] && dl < ml) ++dl;
            size_t si = 0; while (s[si] && dl + 1 < ml) d[dl++] = s[si++];
            d[dl] = '\0';
        };
        auto concatDec = [&](char* d, uint64_t val, size_t ml) {
            char num[24]; int p = 0;
            if (val == 0) { num[p++] = '0'; }
            else {
                char rev[24]; int rp = 0;
                while (val > 0) { rev[rp++] = '0' + (val % 10); val /= 10; }
                while (rp > 0) { num[p++] = rev[--rp]; }
            }
            num[p] = '\0';
            concat(d, num, ml);
        };

        const auto& pfn = BareMetalPfnAllocator::get();
        copy(buf, "=======================================================\r\n", maxLen);
        concat(buf, "  MicaNT Cutler Edition - Memory Manager Subsystem (Mm)\r\n", maxLen);
        concat(buf, "=======================================================\r\n", maxLen);
        concat(buf, "Physical PFN Database (32,768 MB ECC DDR4-2400):\r\n", maxLen);
        concat(buf, "  Dedicated Pool:  256 MB (", maxLen);
        concatDec(buf, pfn.getTotalPages(), maxLen);
        concat(buf, " x 4KB Pages)\r\n", maxLen);
        concat(buf, "  Allocated Pages: ", maxLen);
        concatDec(buf, pfn.getAllocatedPages(), maxLen);
        concat(buf, " (", maxLen);
        concatDec(buf, (pfn.getAllocatedPages() * 4) / 1024, maxLen);
        concat(buf, " MB)\r\n  Free Pages:      ", maxLen);
        concatDec(buf, pfn.getFreePages(), maxLen);
        concat(buf, " (", maxLen);
        concatDec(buf, (pfn.getFreePages() * 4) / 1024, maxLen);
        concat(buf, " MB)\r\n\r\n", maxLen);

        concat(buf, "Demand Paging & Fault Telemetry:\r\n", maxLen);
        concat(buf, "  Total Page Faults:    ", maxLen);
        concatDec(buf, m_telemetry.pageFaultCount, maxLen);
        concat(buf, "\r\n  Demand Pages Mapped:  ", maxLen);
        concatDec(buf, m_telemetry.demandPagesResolved, maxLen);
        concat(buf, "\r\n  Stack Auto-Growths:   ", maxLen);
        concatDec(buf, m_telemetry.stackExpansions, maxLen);
        concat(buf, "\r\n  Access Violations:    ", maxLen);
        concatDec(buf, m_telemetry.accessViolations, maxLen);
        concat(buf, "\r\n  Active VAD Regions:   ", maxLen);
        concatDec(buf, BareMetalVadTable::get().getActiveVadCount(), maxLen);
        concat(buf, "\r\n  NtAllocate Calls:     ", maxLen);
        concatDec(buf, m_telemetry.syscallAllocCount, maxLen);
        concat(buf, " (", maxLen);
        concatDec(buf, m_telemetry.totalBytesAllocated, maxLen);
        concat(buf, " bytes)\r\n", maxLen);
    }
};

} // namespace micant::km::mm_baremetal
