#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <optional>
#include <deque>
#include "ntdef.hpp"

namespace micant::mm {

// ============================================================================
// 1. Standard Page & Address Space Constants
// ============================================================================

inline constexpr size_t PageSize4KB   = 4096;
inline constexpr size_t PageShift4KB  = 12;
inline constexpr size_t PageMask4KB   = PageSize4KB - 1;

inline constexpr size_t PageSize2MB   = 2 * 1024 * 1024;
inline constexpr size_t PageShift2MB  = 21;
inline constexpr size_t PageMask2MB   = PageSize2MB - 1;

inline constexpr size_t PageSize1GB   = 1024 * 1024 * 1024;
inline constexpr size_t PageShift1GB  = 30;

// Virtual Memory Allocation Types (MEM_*)
inline constexpr uint32_t MEM_COMMIT      = 0x00001000;
inline constexpr uint32_t MEM_RESERVE     = 0x00002000;
inline constexpr uint32_t MEM_DECOMMIT    = 0x00004000;
inline constexpr uint32_t MEM_RELEASE     = 0x00008000;
inline constexpr uint32_t MEM_FREE        = 0x00010000;
inline constexpr uint32_t MEM_RESET       = 0x00080000;
inline constexpr uint32_t MEM_TOP_DOWN    = 0x00100000;
inline constexpr uint32_t MEM_LARGE_PAGES = 0x20000000;
inline constexpr uint32_t MEM_PHYSICAL    = 0x00400000;

// Memory Page Protection Constants (PAGE_*)
inline constexpr uint32_t PAGE_NOACCESS          = 0x01;
inline constexpr uint32_t PAGE_READONLY          = 0x02;
inline constexpr uint32_t PAGE_READWRITE         = 0x04;
inline constexpr uint32_t PAGE_WRITECOPY         = 0x08;
inline constexpr uint32_t PAGE_EXECUTE           = 0x10;
inline constexpr uint32_t PAGE_EXECUTE_READ      = 0x20;
inline constexpr uint32_t PAGE_EXECUTE_READWRITE = 0x40;
inline constexpr uint32_t PAGE_EXECUTE_WRITECOPY = 0x80;
inline constexpr uint32_t PAGE_GUARD             = 0x100;
inline constexpr uint32_t PAGE_NOCACHE           = 0x200;
inline constexpr uint32_t PAGE_WRITECOMBINE      = 0x400;

// ============================================================================
// 2. Hardware x86-64 Page Table Entry (PTE)
// ============================================================================

union PageTableEntry {
    uint64_t value{0};
    struct {
        uint64_t present : 1;
        uint64_t writable : 1;
        uint64_t user : 1;
        uint64_t writeThrough : 1;
        uint64_t cacheDisable : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t largePage : 1;
        uint64_t global : 1;
        uint64_t available : 3;
        uint64_t pageFrameNumber : 40;
        uint64_t noExecute : 1;
    };

    [[nodiscard]] uintptr_t getPhysicalAddress() const noexcept {
        return static_cast<uintptr_t>(pageFrameNumber) << PageShift4KB;
    }

    [[nodiscard]] static PageTableEntry create(uint64_t pfn, bool writable, bool user, bool noExecute) noexcept {
        PageTableEntry pte{};
        pte.present = 1;
        pte.writable = writable ? 1 : 0;
        pte.user = user ? 1 : 0;
        pte.noExecute = noExecute ? 1 : 0;
        pte.pageFrameNumber = pfn;
        return pte;
    }
};

// ============================================================================
// 3. Physical Page Frame Number (PFN) Database
// ============================================================================

enum class PageLocation : uint8_t {
    ZeroedPageList       = 0,
    FreePageList         = 1,
    StandbyPageList      = 2,
    ModifiedPageList     = 3,
    ModifiedNoWriteList  = 4,
    ActiveAndValid       = 5,
    TransitionPage       = 6,
    BadPageList          = 7
};

struct Mmpfn {
    uint64_t pfn{0};
    uint32_t originalPte{0};
    uintptr_t pteAddress{0};
    uint32_t referenceCount{0};
    PageLocation location{PageLocation::FreePageList};
    bool modified{false};
    bool readInProgress{false};
    uint64_t flink{0};
    uint64_t blink{0};
};

/**
 * @brief Physical Page Frame Number (PFN) Database.
 * Canonical Windows NT physical memory tracker.
 */
class PfnDatabase {
private:
    mutable std::mutex mutex_;
    std::vector<Mmpfn> entries_;
    std::deque<uint64_t> zeroedList_;
    std::deque<uint64_t> freeList_;
    std::deque<uint64_t> standbyList_;
    std::deque<uint64_t> modifiedList_;
    size_t activeCount_{0};

    PfnDatabase() {
        // Default physical memory pool: 262,144 pages (1 GB)
        initialize(262144);
    }

public:
    static PfnDatabase& get() noexcept {
        static PfnDatabase instance;
        return instance;
    }

    PfnDatabase(const PfnDatabase&) = delete;
    PfnDatabase& operator=(const PfnDatabase&) = delete;

    void initialize(size_t totalPages) {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
        entries_.resize(totalPages);
        zeroedList_.clear();
        freeList_.clear();
        standbyList_.clear();
        modifiedList_.clear();
        activeCount_ = 0;

        for (uint64_t i = 0; i < totalPages; ++i) {
            entries_[i].pfn = i;
            if (i < 256) {
                // Reserve first 1MB low memory as Bad/Reserved
                entries_[i].location = PageLocation::BadPageList;
            } else if (i % 2 == 0) {
                entries_[i].location = PageLocation::ZeroedPageList;
                zeroedList_.push_back(i);
            } else {
                entries_[i].location = PageLocation::FreePageList;
                freeList_.push_back(i);
            }
        }
    }

    [[nodiscard]] uint64_t allocatePage(bool preferZeroed = true) {
        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t pfn = 0;

        if (preferZeroed && !zeroedList_.empty()) {
            pfn = zeroedList_.front();
            zeroedList_.pop_front();
        } else if (!freeList_.empty()) {
            pfn = freeList_.front();
            freeList_.pop_front();
        } else if (!zeroedList_.empty()) {
            pfn = zeroedList_.front();
            zeroedList_.pop_front();
        } else if (!standbyList_.empty()) {
            pfn = standbyList_.front();
            standbyList_.pop_front();
        } else {
            return 0; // Out of physical memory (STATUS_NO_MEMORY)
        }

        if (pfn < entries_.size()) {
            entries_[pfn].location = PageLocation::ActiveAndValid;
            entries_[pfn].referenceCount = 1;
            entries_[pfn].modified = false;
            activeCount_++;
        }
        return pfn;
    }

    void freePage(uint64_t pfn) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pfn >= entries_.size()) return;

        auto& entry = entries_[pfn];
        if (entry.referenceCount > 0) {
            entry.referenceCount--;
        }

        if (entry.referenceCount == 0) {
            if (activeCount_ > 0) activeCount_--;
            if (entry.modified) {
                entry.location = PageLocation::ModifiedPageList;
                modifiedList_.push_back(pfn);
            } else {
                entry.location = PageLocation::FreePageList;
                freeList_.push_back(pfn);
            }
        }
    }

    void referencePage(uint64_t pfn) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pfn < entries_.size()) {
            entries_[pfn].referenceCount++;
        }
    }

    void zeroPage(uint64_t pfn) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pfn >= entries_.size()) return;
        entries_[pfn].location = PageLocation::ZeroedPageList;
        entries_[pfn].modified = false;
        zeroedList_.push_back(pfn);
    }

    [[nodiscard]] size_t getZeroedCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return zeroedList_.size();
    }
    [[nodiscard]] size_t getFreeCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return freeList_.size();
    }
    [[nodiscard]] size_t getStandbyCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return standbyList_.size();
    }
    [[nodiscard]] size_t getModifiedCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return modifiedList_.size();
    }
    [[nodiscard]] size_t getActiveCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return activeCount_;
    }
    [[nodiscard]] size_t getTotalPages() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }
};

// ============================================================================
// 4. Virtual Address Descriptor (VAD) & Balanced AVL Tree
// ============================================================================

/**
 * @brief Virtual Address Descriptor (VAD).
 * Represents a contiguous range of committed or reserved virtual memory in an NT process.
 */
struct VirtualAddressDescriptor {
    uintptr_t startingAddress{0};
    uintptr_t endingAddress{0};
    uint64_t  startingVpn{0};
    uint64_t  endingVpn{0};
    uint32_t  protection{PAGE_NOACCESS};
    uint32_t  allocationType{MEM_RESERVE};
    bool      committed{false};
    bool      isCopyOnWrite{false};
    bool      isGuardPage{false};
    size_t    commitCharge{0};

    // AVL Tree Topology
    VirtualAddressDescriptor* parent{nullptr};
    std::unique_ptr<VirtualAddressDescriptor> left{nullptr};
    std::unique_ptr<VirtualAddressDescriptor> right{nullptr};
    int height{1};

    // Demand-Paged Physical Page Frame Map (VPN -> PFN)
    std::unordered_map<uint64_t, uint64_t> physicalPages;

    [[nodiscard]] size_t size() const noexcept {
        return endingAddress >= startingAddress ? (endingAddress - startingAddress + 1) : 0;
    }

    [[nodiscard]] size_t pageCount() const noexcept {
        return (size() + PageMask4KB) / PageSize4KB;
    }

    [[nodiscard]] bool contains(uintptr_t addr) const noexcept {
        return addr >= startingAddress && addr <= endingAddress;
    }

    [[nodiscard]] bool containsVpn(uint64_t vpn) const noexcept {
        return vpn >= startingVpn && vpn <= endingVpn;
    }
};

/**
 * @brief Balanced AVL Tree for Fast O(log N) Virtual Address Descriptor Lookups.
 * Clean-room implementation of Dave Cutler's MM_AVL_TABLE.
 */
class MmAvlTable {
private:
    std::unique_ptr<VirtualAddressDescriptor> root_{nullptr};
    size_t count_{0};
    mutable std::mutex mutex_;

    static int getHeight(const VirtualAddressDescriptor* node) noexcept {
        return node ? node->height : 0;
    }

    static int getBalanceFactor(const VirtualAddressDescriptor* node) noexcept {
        return node ? (getHeight(node->left.get()) - getHeight(node->right.get())) : 0;
    }

    static void updateHeight(VirtualAddressDescriptor* node) noexcept {
        if (node) {
            node->height = 1 + std::max(getHeight(node->left.get()), getHeight(node->right.get()));
        }
    }

    static std::unique_ptr<VirtualAddressDescriptor> rotateRight(std::unique_ptr<VirtualAddressDescriptor> y) {
        if (!y || !y->left) return y;
        auto x = std::move(y->left);
        y->left = std::move(x->right);
        if (y->left) y->left->parent = y.get();

        x->parent = y->parent;
        y->parent = x.get();
        x->right = std::move(y);

        updateHeight(x->right.get());
        updateHeight(x.get());
        return x;
    }

    static std::unique_ptr<VirtualAddressDescriptor> rotateLeft(std::unique_ptr<VirtualAddressDescriptor> x) {
        if (!x || !x->right) return x;
        auto y = std::move(x->right);
        x->right = std::move(y->left);
        if (x->right) x->right->parent = x.get();

        y->parent = x->parent;
        x->parent = y.get();
        y->left = std::move(x);

        updateHeight(y->left.get());
        updateHeight(y.get());
        return y;
    }

    std::unique_ptr<VirtualAddressDescriptor> insertNode(
        std::unique_ptr<VirtualAddressDescriptor> node,
        std::unique_ptr<VirtualAddressDescriptor> newNode
    ) {
        if (!node) {
            count_++;
            return newNode;
        }

        uint64_t newVpn = newNode->startingVpn;

        if (newVpn < node->startingVpn) {
            newNode->parent = node.get();
            node->left = insertNode(std::move(node->left), std::move(newNode));
        } else {
            newNode->parent = node.get();
            node->right = insertNode(std::move(node->right), std::move(newNode));
        }

        updateHeight(node.get());
        int balance = getBalanceFactor(node.get());

        // Left-Left Case
        if (balance > 1 && node->left && newVpn < node->left->startingVpn) {
            return rotateRight(std::move(node));
        }
        // Right-Right Case
        if (balance < -1 && node->right && newVpn > node->right->startingVpn) {
            return rotateLeft(std::move(node));
        }
        // Left-Right Case
        if (balance > 1 && node->left && newVpn > node->left->startingVpn) {
            node->left = rotateLeft(std::move(node->left));
            return rotateRight(std::move(node));
        }
        // Right-Left Case
        if (balance < -1 && node->right && newVpn < node->right->startingVpn) {
            node->right = rotateRight(std::move(node->right));
            return rotateLeft(std::move(node));
        }

        return node;
    }

    static VirtualAddressDescriptor* findMin(VirtualAddressDescriptor* node) noexcept {
        while (node && node->left) {
            node = node->left.get();
        }
        return node;
    }

    std::unique_ptr<VirtualAddressDescriptor> removeNode(
        std::unique_ptr<VirtualAddressDescriptor> node,
        uintptr_t startingAddress,
        bool& removed
    ) {
        if (!node) return nullptr;

        uint64_t targetVpn = startingAddress >> PageShift4KB;
        if (targetVpn < node->startingVpn) {
            node->left = removeNode(std::move(node->left), startingAddress, removed);
        } else if (targetVpn > node->startingVpn) {
            node->right = removeNode(std::move(node->right), startingAddress, removed);
        } else {
            // Node found
            removed = true;

            if (!node->left || !node->right) {
                if (count_ > 0) count_--;
                auto temp = node->left ? std::move(node->left) : std::move(node->right);
                if (temp) temp->parent = node->parent;
                return temp;
            }

            // Node with two children
            VirtualAddressDescriptor* temp = findMin(node->right.get());
            // Copy data from successor
            uintptr_t succStart = temp->startingAddress;
            node->startingAddress = temp->startingAddress;
            node->endingAddress = temp->endingAddress;
            node->startingVpn = temp->startingVpn;
            node->endingVpn = temp->endingVpn;
            node->protection = temp->protection;
            node->allocationType = temp->allocationType;
            node->committed = temp->committed;
            node->isCopyOnWrite = temp->isCopyOnWrite;
            node->isGuardPage = temp->isGuardPage;
            node->commitCharge = temp->commitCharge;
            node->physicalPages = std::move(temp->physicalPages);

            bool subRemoved = false;
            node->right = removeNode(std::move(node->right), succStart, subRemoved);
        }

        updateHeight(node.get());
        int balance = getBalanceFactor(node.get());

        if (balance > 1 && getBalanceFactor(node->left.get()) >= 0) {
            return rotateRight(std::move(node));
        }
        if (balance > 1 && getBalanceFactor(node->left.get()) < 0) {
            node->left = rotateLeft(std::move(node->left));
            return rotateRight(std::move(node));
        }
        if (balance < -1 && getBalanceFactor(node->right.get()) <= 0) {
            return rotateLeft(std::move(node));
        }
        if (balance < -1 && getBalanceFactor(node->right.get()) > 0) {
            node->right = rotateRight(std::move(node->right));
            return rotateLeft(std::move(node));
        }

        return node;
    }

    void collectInOrder(const VirtualAddressDescriptor* node, std::vector<VirtualAddressDescriptor*>& out) const {
        if (!node) return;
        collectInOrder(node->left.get(), out);
        out.push_back(const_cast<VirtualAddressDescriptor*>(node));
        collectInOrder(node->right.get(), out);
    }

public:
    MmAvlTable() = default;

    void insert(std::unique_ptr<VirtualAddressDescriptor> vad) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!vad) return;
        vad->startingVpn = vad->startingAddress >> PageShift4KB;
        vad->endingVpn = vad->endingAddress >> PageShift4KB;
        root_ = insertNode(std::move(root_), std::move(vad));
    }

    bool remove(uintptr_t startingAddress) {
        std::lock_guard<std::mutex> lock(mutex_);
        bool removed = false;
        root_ = removeNode(std::move(root_), startingAddress, removed);
        return removed;
    }

    [[nodiscard]] VirtualAddressDescriptor* find(uintptr_t address) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        VirtualAddressDescriptor* curr = root_.get();
        uint64_t vpn = address >> PageShift4KB;

        while (curr) {
            if (curr->contains(address)) {
                return curr;
            }
            if (vpn < curr->startingVpn) {
                curr = curr->left.get();
            } else {
                curr = curr->right.get();
            }
        }
        return nullptr;
    }

    [[nodiscard]] const VirtualAddressDescriptor* find(uintptr_t address) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        const VirtualAddressDescriptor* curr = root_.get();
        uint64_t vpn = address >> PageShift4KB;

        while (curr) {
            if (curr->contains(address)) {
                return curr;
            }
            if (vpn < curr->startingVpn) {
                curr = curr->left.get();
            } else {
                curr = curr->right.get();
            }
        }
        return nullptr;
    }

    [[nodiscard]] uintptr_t findFreeRange(
        size_t size,
        size_t alignment = PageSize4KB,
        bool topDown = false,
        uintptr_t minAddr = 0x0000000000010000ULL,
        uintptr_t maxAddr = 0x00007FFFFFFFFFFFULL
    ) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<VirtualAddressDescriptor*> sorted;
        collectInOrder(root_.get(), sorted);

        size_t align = std::max(alignment, PageSize4KB);
        size_t alignedSize = (size + PageMask4KB) & ~PageMask4KB;

        if (!topDown) {
            // Bottom-Up Search
            uintptr_t candidate = (minAddr + align - 1) & ~(align - 1);
            for (const auto* vad : sorted) {
                if (candidate + alignedSize <= vad->startingAddress) {
                    return candidate;
                }
                candidate = (vad->endingAddress + 1 + align - 1) & ~(align - 1);
            }
            if (candidate + alignedSize <= maxAddr) {
                return candidate;
            }
        } else {
            // Top-Down Search
            uintptr_t candidate = (maxAddr - alignedSize) & ~(align - 1);
            for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
                const auto* vad = *it;
                if (candidate >= vad->endingAddress) {
                    return candidate;
                }
                if (vad->startingAddress >= alignedSize) {
                    candidate = (vad->startingAddress - alignedSize) & ~(align - 1);
                } else {
                    break;
                }
            }
            if (candidate >= minAddr) {
                return candidate;
            }
        }

        return 0; // Out of virtual address space
    }

    [[nodiscard]] size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        root_.reset();
        count_ = 0;
    }
};

// ============================================================================
// 5. Working Set Metrics (MMWSL)
// ============================================================================

struct WorkingSetList {
    size_t residentPages{0};
    size_t peakResidentPages{0};
    size_t commitCharge{0};
    size_t peakCommitCharge{0};
    size_t workingSetMinimum{50};
    size_t workingSetMaximum{1000};
};

// ============================================================================
// 6. Hardware Paging Engine (4-Level PML4 Table Simulation)
// ============================================================================

class HardwarePagingEngine {
private:
    mutable std::mutex mutex_;
    // Simulated Page Directories: CR3 -> (VA -> PTE)
    std::unordered_map<uintptr_t, std::unordered_map<uintptr_t, PageTableEntry>> pageTables_;

public:
    HardwarePagingEngine() = default;

    void mapPage(uintptr_t cr3, uintptr_t va, uint64_t pfn, uint32_t protect, bool user = true) {
        std::lock_guard<std::mutex> lock(mutex_);
        uintptr_t pageVa = va & ~PageMask4KB;
        bool writable = (protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) != 0;
        bool noExec   = (protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) == 0;

        PageTableEntry pte = PageTableEntry::create(pfn, writable, user, noExec);
        pageTables_[cr3][pageVa] = pte;
    }

    void unmapPage(uintptr_t cr3, uintptr_t va) {
        std::lock_guard<std::mutex> lock(mutex_);
        uintptr_t pageVa = va & ~PageMask4KB;
        auto it = pageTables_.find(cr3);
        if (it != pageTables_.end()) {
            it->second.erase(pageVa);
        }
    }

    [[nodiscard]] std::optional<PageTableEntry> queryPte(uintptr_t cr3, uintptr_t va) const {
        std::lock_guard<std::mutex> lock(mutex_);
        uintptr_t pageVa = va & ~PageMask4KB;
        auto it = pageTables_.find(cr3);
        if (it != pageTables_.end()) {
            auto pit = it->second.find(pageVa);
            if (pit != it->second.end()) {
                return pit->second;
            }
        }
        return std::nullopt;
    }

    void modifyProtection(uintptr_t cr3, uintptr_t va, uint32_t newProtect) {
        std::lock_guard<std::mutex> lock(mutex_);
        uintptr_t pageVa = va & ~PageMask4KB;
        auto it = pageTables_.find(cr3);
        if (it != pageTables_.end()) {
            auto pit = it->second.find(pageVa);
            if (pit != it->second.end()) {
                pit->second.writable = (newProtect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) ? 1 : 0;
                pit->second.noExecute = (newProtect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) ? 0 : 1;
            }
        }
    }
};

// ============================================================================
// 7. Process Virtual Address Space & Demand Paging
// ============================================================================

/**
 * @brief Memory Manager address space abstraction.
 * Integrates VAD AVL trees, physical PFN database, working set quotas, and demand-paging.
 */
class ProcessAddressSpace {
public:
    static constexpr uintptr_t UserSpaceMin = 0x0000000000010000ULL;
    static constexpr uintptr_t UserSpaceMax = 0x00007FFFFFFFFFFFULL;

    ProcessAddressSpace() : nextFreeAddress_(0x0000000100000000ULL) {}

    NtStatus allocate(uintptr_t& baseAddress, size_t size, uint32_t allocationType, uint32_t protect) {
        if (size == 0) return NtStatus::InvalidParameter;

        std::lock_guard<std::mutex> lock(mutex_);
        size_t alignedSize = (size + PageMask4KB) & ~PageMask4KB;

        uintptr_t targetAddr = baseAddress;
        if (targetAddr == 0) {
            bool topDown = (allocationType & MEM_TOP_DOWN) != 0;
            targetAddr = vadTree_.findFreeRange(alignedSize, PageSize4KB, topDown, nextFreeAddress_, UserSpaceMax);
            if (targetAddr == 0) {
                targetAddr = vadTree_.findFreeRange(alignedSize, PageSize4KB, topDown, UserSpaceMin, UserSpaceMax);
            }
            if (targetAddr == 0) {
                return NtStatus::NoMemory;
            }
            nextFreeAddress_ = targetAddr + alignedSize + PageSize4KB;
        }

        auto vad = std::make_unique<VirtualAddressDescriptor>();
        vad->startingAddress = targetAddr;
        vad->endingAddress = targetAddr + alignedSize - 1;
        vad->protection = protect;
        vad->allocationType = allocationType;
        vad->committed = (allocationType & MEM_COMMIT) != 0;
        vad->isCopyOnWrite = (protect == PAGE_WRITECOPY || protect == PAGE_EXECUTE_WRITECOPY);
        vad->isGuardPage = (protect & PAGE_GUARD) != 0;
        vad->commitCharge = vad->committed ? vad->pageCount() : 0;

        if (vad->committed) {
            workingSet_.commitCharge += vad->commitCharge;
            workingSet_.peakCommitCharge = std::max(workingSet_.peakCommitCharge, workingSet_.commitCharge);
        }

        vadTree_.insert(std::move(vad));
        baseAddress = targetAddr;
        return NtStatus::Success;
    }

    NtStatus free(uintptr_t baseAddress, size_t size, uint32_t freeType) {
        std::lock_guard<std::mutex> lock(mutex_);
        VirtualAddressDescriptor* vad = vadTree_.find(baseAddress);
        if (!vad) return NtStatus::InvalidParameter;

        if (freeType & MEM_RELEASE) {
            // Unmap and free all physical page frames allocated for this VAD
            auto& pfnDb = PfnDatabase::get();
            for (const auto& [vpn, pfn] : vad->physicalPages) {
                pagingEngine_.unmapPage(directoryTableBase_, vpn << PageShift4KB);
                pfnDb.freePage(pfn);
                if (workingSet_.residentPages > 0) workingSet_.residentPages--;
            }
            vad->physicalPages.clear();

            if (vad->committed && workingSet_.commitCharge >= vad->commitCharge) {
                workingSet_.commitCharge -= vad->commitCharge;
            }

            vadTree_.remove(vad->startingAddress);
            return NtStatus::Success;
        } else if (freeType & MEM_DECOMMIT) {
            auto& pfnDb = PfnDatabase::get();
            for (const auto& [vpn, pfn] : vad->physicalPages) {
                pagingEngine_.unmapPage(directoryTableBase_, vpn << PageShift4KB);
                pfnDb.freePage(pfn);
                if (workingSet_.residentPages > 0) workingSet_.residentPages--;
            }
            vad->physicalPages.clear();

            if (vad->committed && workingSet_.commitCharge >= vad->commitCharge) {
                workingSet_.commitCharge -= vad->commitCharge;
            }
            vad->committed = false;
            vad->commitCharge = 0;
            return NtStatus::Success;
        }

        (void)size;
        return NtStatus::InvalidParameter;
    }

    NtStatus protect(uintptr_t baseAddress, size_t size, uint32_t newProtect, uint32_t* oldProtect) {
        std::lock_guard<std::mutex> lock(mutex_);
        VirtualAddressDescriptor* vad = vadTree_.find(baseAddress);
        if (!vad) return NtStatus::InvalidParameter;

        if (oldProtect) {
            *oldProtect = vad->protection;
        }

        vad->protection = newProtect;
        vad->isCopyOnWrite = (newProtect == PAGE_WRITECOPY || newProtect == PAGE_EXECUTE_WRITECOPY);
        vad->isGuardPage = (newProtect & PAGE_GUARD) != 0;

        // Propagate updated protection to hardware page tables
        for (const auto& [vpn, pfn] : vad->physicalPages) {
            pagingEngine_.modifyProtection(directoryTableBase_, vpn << PageShift4KB, newProtect);
        }

        (void)size;
        return NtStatus::Success;
    }

    [[nodiscard]] size_t getRegionCount() const noexcept {
        return vadTree_.size();
    }

    [[nodiscard]] VirtualAddressDescriptor* findVad(uintptr_t addr) noexcept {
        return vadTree_.find(addr);
    }

    [[nodiscard]] const VirtualAddressDescriptor* findVad(uintptr_t addr) const noexcept {
        return vadTree_.find(addr);
    }

    /**
     * @brief High-Performance Demand Paging & Page Fault Handler (Vector 14 / #PF).
     *
     * @param faultingAddress Faulting virtual address
     * @param isWrite True if access was a write, false for read
     * @param isUser True if user-mode fault, false for kernel-mode
     * @param isExecute True if instruction fetch fault
     */
    NtStatus handlePageFault(uintptr_t faultingAddress, bool isWrite, bool isUser, bool isExecute) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 1. Locate VAD in AVL Tree in O(log N)
        VirtualAddressDescriptor* vad = vadTree_.find(faultingAddress);
        if (!vad) {
            return NtStatus::AccessViolation; // 0xC0000005
        }

        // 2. Validate Access Rights Against Protection
        if (vad->protection == PAGE_NOACCESS) {
            return NtStatus::AccessViolation;
        }
        if (isWrite && (vad->protection == PAGE_READONLY || vad->protection == PAGE_EXECUTE_READ)) {
            return NtStatus::AccessViolation;
        }
        if (isExecute && (vad->protection & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) == 0) {
            return NtStatus::AccessViolation;
        }

        // 3. Handle Guard Page (Stack Growth)
        if (vad->isGuardPage) {
            vad->isGuardPage = false;
            vad->protection &= ~PAGE_GUARD;
            return NtStatus::GuardPageViolation; // 0x80000001 (triggers stack expansion)
        }

        uint64_t faultVpn = faultingAddress >> PageShift4KB;
        auto& pfnDb = PfnDatabase::get();

        // 4. Handle Copy-On-Write (COW)
        if (vad->isCopyOnWrite && isWrite) {
            uint64_t privatePfn = pfnDb.allocatePage(true);
            if (privatePfn == 0) return NtStatus::NoMemory;

            vad->physicalPages[faultVpn] = privatePfn;
            pagingEngine_.mapPage(directoryTableBase_, faultVpn << PageShift4KB, privatePfn, PAGE_READWRITE, isUser);
            vad->isCopyOnWrite = false;
            vad->protection = PAGE_READWRITE;
            return NtStatus::Success;
        }

        // 5. Demand-Zero Page Allocation
        if (vad->committed) {
            auto it = vad->physicalPages.find(faultVpn);
            if (it == vad->physicalPages.end()) {
                uint64_t newPfn = pfnDb.allocatePage(true);
                if (newPfn == 0) return NtStatus::NoMemory;

                vad->physicalPages[faultVpn] = newPfn;
                pagingEngine_.mapPage(directoryTableBase_, faultVpn << PageShift4KB, newPfn, vad->protection, isUser);

                workingSet_.residentPages++;
                workingSet_.peakResidentPages = std::max(workingSet_.peakResidentPages, workingSet_.residentPages);
            }
            return NtStatus::Success;
        }

        return NtStatus::AccessViolation;
    }

    [[nodiscard]] const WorkingSetList& getWorkingSet() const noexcept { return workingSet_; }
    [[nodiscard]] size_t getCommitCharge() const noexcept { return workingSet_.commitCharge; }
    [[nodiscard]] size_t getResidentPages() const noexcept { return workingSet_.residentPages; }
    [[nodiscard]] uintptr_t getDirectoryTableBase() const noexcept { return directoryTableBase_; }
    void setDirectoryTableBase(uintptr_t cr3) noexcept { directoryTableBase_ = cr3; }

private:
    MmAvlTable vadTree_;
    WorkingSetList workingSet_;
    HardwarePagingEngine pagingEngine_;
    uintptr_t directoryTableBase_{0x1000};
    uintptr_t nextFreeAddress_{0x0000000100000000ULL};
    mutable std::mutex mutex_;
};

// ============================================================================
// 8. Top-Level Executive Memory Manager Handlers
// ============================================================================

/**
 * @brief Dispatcher for Vector 14 Page Faults (#PF).
 */
inline NtStatus MmAccessFault(
    ProcessAddressSpace& addressSpace,
    uintptr_t faultingAddress,
    bool isWrite,
    bool isUser,
    bool isExecute
) {
    return addressSpace.handlePageFault(faultingAddress, isWrite, isUser, isExecute);
}

/**
 * @brief Phase 0 / Phase 1 Memory Manager Initialization.
 */
inline NtStatus MmInitSystem(uint32_t phase, const void* /*loaderBlock*/ = nullptr) {
    if (phase == 0) {
        // Initialize PFN database with 1GB default pool
        PfnDatabase::get().initialize(262144);
        return NtStatus::Success;
    }
    return NtStatus::Success;
}

} // namespace micant::mm
