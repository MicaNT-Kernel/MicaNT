#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <chrono>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ke.hpp"

namespace micant::ex {

// Standard NT Executive Pool Types
enum class PoolType : uint32_t {
    NonPagedPool                   = 0,
    PagedPool                      = 1,
    NonPagedPoolMustSucceed        = 2,
    DontUseThisType                = 3,
    NonPagedPoolCacheAligned       = 4,
    PagedPoolCacheAligned          = 5,
    NonPagedPoolNx                 = 512,
    PagedPoolNx                    = 513
};

/**
 * @brief Helper to construct standard 4-byte NT pool tags.
 * e.g. MAKE_POOL_TAG('M', 'i', 'c', 'a')
 */
[[nodiscard]] constexpr uint32_t makePoolTag(char c1, char c2, char c3, char c4) noexcept {
    return (static_cast<uint32_t>(static_cast<uint8_t>(c1))) |
           (static_cast<uint32_t>(static_cast<uint8_t>(c2)) << 8) |
           (static_cast<uint32_t>(static_cast<uint8_t>(c3)) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(c4)) << 24);
}

inline constexpr uint32_t TAG_MICA_CORE = makePoolTag('M', 'i', 'c', 'a');
inline constexpr uint32_t TAG_PROCESS   = makePoolTag('P', 'r', 'o', 'c');
inline constexpr uint32_t TAG_THREAD    = makePoolTag('T', 'h', 'r', 'e');
inline constexpr uint32_t TAG_SECTION   = makePoolTag('S', 'e', 'c', 'O');
inline constexpr uint32_t TAG_IO_BUFFER = makePoolTag('I', 'o', ' ', ' ');
inline constexpr uint32_t TAG_REGISTRY  = makePoolTag('C', 'm', 'R', 'e');
inline constexpr uint32_t TAG_SECURITY  = makePoolTag('S', 'e', 'c', 'u');
inline constexpr uint32_t TAG_LOOKASIDE = makePoolTag('L', 'o', 'o', 'k');

struct PoolBlockHeader {
    uint32_t magic{0x4D494341}; // 'MICA'
    uint32_t tag{0};
    PoolType type{PoolType::NonPagedPool};
    size_t requestedSize{0};
};

struct PoolTagStats {
    uint32_t tag{0};
    size_t activeAllocations{0};
    size_t activeBytes{0};
    size_t totalAllocations{0};
    size_t totalFrees{0};
};

/**
 * @brief Executive Memory Pool Manager.
 * Implements NonPagedPool (RAM-resident) and PagedPool with strict IRQL enforcement.
 */
class ExecutivePool {
public:
    static ExecutivePool& get() {
        static ExecutivePool instance;
        return instance;
    }

    /**
     * @brief Allocates pool memory with 4-byte diagnostic tag.
     * Enforces that PagedPool CANNOT be allocated at IRQL >= DISPATCH_LEVEL.
     */
    void* allocatePoolWithTag(PoolType poolType, size_t numberOfBytes, uint32_t tag) {
        if (numberOfBytes == 0) return nullptr;

        // IRQL CONTRACT: PagedPool cannot be accessed or allocated at DISPATCH_LEVEL or above
        ke::KIRQL currentIrql = ke::KeGetCurrentIrql();
        bool isPaged = (poolType == PoolType::PagedPool || poolType == PoolType::PagedPoolNx || poolType == PoolType::PagedPoolCacheAligned);
        
        if (isPaged && currentIrql >= ke::DISPATCH_LEVEL) {
            std::cerr << "[MicaNT BugCheck] IRQL_NOT_LESS_OR_EQUAL: Attempted to allocate PagedPool at IRQL " 
                      << static_cast<uint32_t>(currentIrql) << " (>= DISPATCH_LEVEL)\n";
            return nullptr;
        }

        size_t totalBytes = sizeof(PoolBlockHeader) + numberOfBytes;
        void* raw = std::malloc(totalBytes);
        if (!raw) return nullptr;

        auto* header = reinterpret_cast<PoolBlockHeader*>(raw);
        header->magic = 0x4D494341;
        header->tag = tag;
        header->type = poolType;
        header->requestedSize = numberOfBytes;

        void* userPtr = header + 1;

        // Record telemetry
        {
            ke::SpinLockGuard guard(lock_);
            auto& stats = tagStats_[tag];
            stats.tag = tag;
            stats.activeAllocations++;
            stats.activeBytes += numberOfBytes;
            stats.totalAllocations++;

            if (isPaged) {
                totalPagedBytes_ += numberOfBytes;
            } else {
                totalNonPagedBytes_ += numberOfBytes;
            }
        }

        return userPtr;
    }

    /**
     * @brief Frees pool memory previously allocated with allocatePoolWithTag.
     */
    void freePoolWithTag(void* p, uint32_t tag) {
        if (!p) return;

        auto* header = reinterpret_cast<PoolBlockHeader*>(p) - 1;
        if (header->magic != 0x4D494341) {
            std::cerr << "[MicaNT BugCheck] BAD_POOL_CALLER: Memory block corrupted or double freed\n";
            return;
        }

        // Verify tag if provided
        if (tag != 0 && header->tag != tag) {
            std::cerr << "[MicaNT BugCheck] BAD_POOL_CALLER: Pool tag mismatch on free\n";
        }

        size_t size = header->requestedSize;
        bool isPaged = (header->type == PoolType::PagedPool || header->type == PoolType::PagedPoolNx);

        // Clear magic to detect double frees
        header->magic = 0xDEADBEEF;

        {
            ke::SpinLockGuard guard(lock_);
            auto it = tagStats_.find(header->tag);
            if (it != tagStats_.end()) {
                it->second.activeAllocations--;
                it->second.activeBytes -= size;
                it->second.totalFrees++;
            }

            if (isPaged) {
                totalPagedBytes_ -= size;
            } else {
                totalNonPagedBytes_ -= size;
            }
        }

        std::free(header);
    }

    [[nodiscard]] size_t getTotalNonPagedBytes() const noexcept { return totalNonPagedBytes_; }
    [[nodiscard]] size_t getTotalPagedBytes() const noexcept { return totalPagedBytes_; }

    [[nodiscard]] PoolTagStats getTagStats(uint32_t tag) const {
        ke::SpinLockGuard guard(lock_);
        auto it = tagStats_.find(tag);
        if (it != tagStats_.end()) return it->second;
        return PoolTagStats{ .tag = tag };
    }

    void dumpPoolTelemetry() const {
        ke::SpinLockGuard guard(lock_);
        std::cout << "\n================ Executive Pool Telemetry ================\n";
        std::cout << "  - NonPaged Pool Active: " << (totalNonPagedBytes_ / 1024) << " KB\n";
        std::cout << "  - Paged Pool Active:    " << (totalPagedBytes_ / 1024) << " KB\n";
        std::cout << "  - Active Tags:\n";
        for (const auto& [tag, stat] : tagStats_) {
            char tagStr[5] = {
                static_cast<char>(tag & 0xFF),
                static_cast<char>((tag >> 8) & 0xFF),
                static_cast<char>((tag >> 16) & 0xFF),
                static_cast<char>((tag >> 24) & 0xFF),
                '\0'
            };
            std::cout << "      * Tag: ['" << tagStr << "'] Active Allocs: " 
                      << stat.activeAllocations << " (" << stat.activeBytes << " bytes)\n";
        }
        std::cout << "==========================================================\n\n";
    }

private:
    ExecutivePool() = default;
    mutable ke::SpinLock lock_;
    std::unordered_map<uint32_t, PoolTagStats> tagStats_;
    std::atomic<size_t> totalNonPagedBytes_{0};
    std::atomic<size_t> totalPagedBytes_{0};
};

// Standard NT Executive Pool Allocation API
inline void* ExAllocatePoolWithTag(PoolType poolType, size_t numberOfBytes, uint32_t tag) {
    return ExecutivePool::get().allocatePoolWithTag(poolType, numberOfBytes, tag);
}

inline void ExFreePoolWithTag(void* p, uint32_t tag) {
    ExecutivePool::get().freePoolWithTag(p, tag);
}

inline uint64_t ExGetCurrentThreadId() noexcept {
    if (auto* t = ke::KeGetCurrentThread(); t != nullptr && t->tid != 0) {
        return static_cast<uint64_t>(t->tid);
    }
    return static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

// ============================================================================
// Executive Fast Mutex (FAST_MUTEX)
// ============================================================================
struct FAST_MUTEX {
    std::atomic<int32_t> count{1};
    uint64_t owner{0};
    ke::KIRQL oldIrql{ke::PASSIVE_LEVEL};
    std::mutex gateLock;
};
using PFAST_MUTEX = FAST_MUTEX*;

inline void ExInitializeFastMutex(PFAST_MUTEX fastMutex) {
    if (!fastMutex) return;
    fastMutex->count.store(1, std::memory_order_relaxed);
    fastMutex->owner = 0;
    fastMutex->oldIrql = ke::PASSIVE_LEVEL;
}

inline void ExAcquireFastMutex(PFAST_MUTEX fastMutex) {
    if (!fastMutex) return;
    ke::KIRQL old = ke::KfRaiseIrql(ke::APC_LEVEL);
    fastMutex->gateLock.lock();
    fastMutex->count.store(0, std::memory_order_relaxed);
    fastMutex->oldIrql = old;
    fastMutex->owner = ExGetCurrentThreadId();
}

inline BOOLEAN ExTryToAcquireFastMutex(PFAST_MUTEX fastMutex) {
    if (!fastMutex) return FALSE;
    ke::KIRQL old = ke::KfRaiseIrql(ke::APC_LEVEL);
    if (fastMutex->gateLock.try_lock()) {
        fastMutex->count.store(0, std::memory_order_relaxed);
        fastMutex->oldIrql = old;
        fastMutex->owner = ExGetCurrentThreadId();
        return TRUE;
    }
    ke::KeLowerIrql(old);
    return FALSE;
}

inline void ExReleaseFastMutex(PFAST_MUTEX fastMutex) {
    if (!fastMutex) return;
    ke::KIRQL old = fastMutex->oldIrql;
    fastMutex->owner = 0;
    fastMutex->count.store(1, std::memory_order_relaxed);
    fastMutex->gateLock.unlock();
    ke::KeLowerIrql(old);
}

// ============================================================================
// Executive Resource (ERESOURCE) - Multi-Reader / Single-Writer Synchronization
// ============================================================================
struct ERESOURCE {
    mutable std::mutex lock;
    std::condition_variable cv;
    uint32_t activeCount{0};
    uint32_t sharedOwners{0};
    uint32_t exclusiveOwners{0};
    uint64_t exclusiveOwnerThread{0};
    uint32_t waiters{0};
};
using PERESOURCE = ERESOURCE*;

inline void ExInitializeResourceLite(PERESOURCE resource) {
    if (!resource) return;
    resource->activeCount = 0;
    resource->sharedOwners = 0;
    resource->exclusiveOwners = 0;
    resource->exclusiveOwnerThread = 0;
    resource->waiters = 0;
}

inline BOOLEAN ExAcquireResourceExclusiveLite(PERESOURCE resource, BOOLEAN wait) {
    if (!resource) return FALSE;
    uint64_t currentTid = ExGetCurrentThreadId();
    std::unique_lock<std::mutex> lk(resource->lock);

    if (resource->exclusiveOwnerThread == currentTid) {
        resource->exclusiveOwners++;
        return TRUE;
    }

    if (resource->activeCount == 0 && resource->sharedOwners == 0) {
        resource->exclusiveOwnerThread = currentTid;
        resource->exclusiveOwners = 1;
        resource->activeCount = 1;
        return TRUE;
    }

    if (!wait) return FALSE;

    resource->waiters++;
    resource->cv.wait(lk, [&]() {
        return resource->activeCount == 0 && resource->sharedOwners == 0;
    });
    resource->waiters--;

    resource->exclusiveOwnerThread = currentTid;
    resource->exclusiveOwners = 1;
    resource->activeCount = 1;
    return TRUE;
}

inline BOOLEAN ExAcquireResourceSharedLite(PERESOURCE resource, BOOLEAN wait) {
    if (!resource) return FALSE;
    uint64_t currentTid = ExGetCurrentThreadId();
    std::unique_lock<std::mutex> lk(resource->lock);

    if (resource->exclusiveOwnerThread == currentTid) {
        resource->sharedOwners++;
        return TRUE;
    }

    if (resource->exclusiveOwners == 0 && resource->waiters == 0) {
        resource->sharedOwners++;
        resource->activeCount = resource->sharedOwners;
        return TRUE;
    }

    if (!wait) return FALSE;

    resource->cv.wait(lk, [&]() {
        return resource->exclusiveOwners == 0;
    });

    resource->sharedOwners++;
    resource->activeCount = resource->sharedOwners;
    return TRUE;
}

inline void ExReleaseResourceLite(PERESOURCE resource) {
    if (!resource) return;
    uint64_t currentTid = ExGetCurrentThreadId();
    std::unique_lock<std::mutex> lk(resource->lock);

    if (resource->exclusiveOwnerThread == currentTid) {
        resource->exclusiveOwners--;
        if (resource->exclusiveOwners == 0) {
            resource->exclusiveOwnerThread = 0;
            resource->activeCount = resource->sharedOwners;
            resource->cv.notify_all();
        }
        return;
    }

    if (resource->sharedOwners > 0) {
        resource->sharedOwners--;
        resource->activeCount = resource->sharedOwners;
        if (resource->sharedOwners == 0) {
            resource->cv.notify_all();
        }
    }
}

inline void ExConvertExclusiveToSharedLite(PERESOURCE resource) {
    if (!resource) return;
    uint64_t currentTid = ExGetCurrentThreadId();
    std::unique_lock<std::mutex> lk(resource->lock);

    if (resource->exclusiveOwnerThread == currentTid && resource->exclusiveOwners > 0) {
        resource->sharedOwners += resource->exclusiveOwners;
        resource->exclusiveOwners = 0;
        resource->exclusiveOwnerThread = 0;
        resource->activeCount = resource->sharedOwners;
        resource->cv.notify_all();
    }
}

inline void ExDeleteResourceLite(PERESOURCE resource) {
    if (!resource) return;
    std::unique_lock<std::mutex> lk(resource->lock);
    resource->activeCount = 0;
    resource->sharedOwners = 0;
    resource->exclusiveOwners = 0;
    resource->exclusiveOwnerThread = 0;
}

// ============================================================================
// Executive Push Lock (EX_PUSH_LOCK)
// ============================================================================
struct EX_PUSH_LOCK {
    union {
        struct {
            uintptr_t locked : 1;
            uintptr_t waiting : 1;
            uintptr_t shared : 10;
            uintptr_t ptr : 52;
        };
        uintptr_t value{0};
    };
    mutable std::mutex lock;
    std::condition_variable cv;
};
using PEX_PUSH_LOCK = EX_PUSH_LOCK*;

inline void ExInitializePushLock(PEX_PUSH_LOCK pushLock) {
    if (pushLock) {
        pushLock->value = 0;
    }
}

inline void ExAcquirePushLockExclusive(PEX_PUSH_LOCK pushLock) {
    if (!pushLock) return;
    std::unique_lock<std::mutex> lk(pushLock->lock);
    pushLock->cv.wait(lk, [&]() {
        return (pushLock->locked == 0 && pushLock->shared == 0);
    });
    pushLock->locked = 1;
}

inline void ExReleasePushLockExclusive(PEX_PUSH_LOCK pushLock) {
    if (!pushLock) return;
    std::unique_lock<std::mutex> lk(pushLock->lock);
    pushLock->locked = 0;
    pushLock->cv.notify_all();
}

inline void ExAcquirePushLockShared(PEX_PUSH_LOCK pushLock) {
    if (!pushLock) return;
    std::unique_lock<std::mutex> lk(pushLock->lock);
    pushLock->cv.wait(lk, [&]() {
        return pushLock->locked == 0;
    });
    pushLock->shared++;
}

inline void ExReleasePushLockShared(PEX_PUSH_LOCK pushLock) {
    if (!pushLock) return;
    std::unique_lock<std::mutex> lk(pushLock->lock);
    if (pushLock->shared > 0) {
        pushLock->shared--;
        if (pushLock->shared == 0) {
            pushLock->cv.notify_all();
        }
    }
}

// ============================================================================
// Interlocked Singly-Linked Lists (SLIST_HEADER, SLIST_ENTRY)
// ============================================================================
struct SLIST_ENTRY {
    SLIST_ENTRY* next{nullptr};
};
using PSLIST_ENTRY = SLIST_ENTRY*;

struct alignas(16) SLIST_HEADER {
    std::atomic<uint64_t> depthAndSequence{0};
    std::atomic<SLIST_ENTRY*> next{nullptr};
    mutable std::mutex mutex;
};
using PSLIST_HEADER = SLIST_HEADER*;

inline void InitializeSListHead(PSLIST_HEADER listHead) {
    if (listHead) {
        listHead->depthAndSequence.store(0, std::memory_order_relaxed);
        listHead->next.store(nullptr, std::memory_order_relaxed);
    }
}

inline PSLIST_ENTRY InterlockedPushEntrySList(PSLIST_HEADER listHead, PSLIST_ENTRY listEntry) {
    if (!listHead || !listEntry) return nullptr;
    std::lock_guard<std::mutex> guard(listHead->mutex);
    auto* old = listHead->next.load(std::memory_order_relaxed);
    listEntry->next = old;
    listHead->next.store(listEntry, std::memory_order_release);
    listHead->depthAndSequence.fetch_add(1, std::memory_order_relaxed);
    return old;
}

inline PSLIST_ENTRY InterlockedPopEntrySList(PSLIST_HEADER listHead) {
    if (!listHead) return nullptr;
    std::lock_guard<std::mutex> guard(listHead->mutex);
    auto* first = listHead->next.load(std::memory_order_relaxed);
    if (!first) return nullptr;
    listHead->next.store(first->next, std::memory_order_release);
    first->next = nullptr;
    listHead->depthAndSequence.fetch_sub(1, std::memory_order_relaxed);
    return first;
}

inline PSLIST_ENTRY InterlockedFlushSList(PSLIST_HEADER listHead) {
    if (!listHead) return nullptr;
    std::lock_guard<std::mutex> guard(listHead->mutex);
    auto* first = listHead->next.load(std::memory_order_relaxed);
    listHead->next.store(nullptr, std::memory_order_release);
    listHead->depthAndSequence.store(0, std::memory_order_relaxed);
    return first;
}

inline uint16_t QueryDepthSList(PSLIST_HEADER listHead) {
    if (!listHead) return 0;
    return static_cast<uint16_t>(listHead->depthAndSequence.load(std::memory_order_relaxed) & 0xFFFF);
}

// ============================================================================
// Lookaside Lists (NPAGED_LOOKASIDE_LIST, PAGED_LOOKASIDE_LIST)
// ============================================================================
using PALLOCATE_FUNCTION = void* (*)(PoolType poolType, size_t numberOfBytes, uint32_t tag);
using PFREE_FUNCTION = void (*)(void* buffer);

struct GENERAL_LOOKASIDE {
    SLIST_HEADER listHead;
    uint16_t depth{4};
    uint16_t maximumDepth{256};
    uint32_t totalAllocates{0};
    uint32_t allocateMisses{0};
    uint32_t totalFrees{0};
    uint32_t freeMisses{0};
    PoolType type{PoolType::NonPagedPool};
    uint32_t tag{0};
    size_t size{0};
    PALLOCATE_FUNCTION allocate{nullptr};
    PFREE_FUNCTION free{nullptr};
};

struct NPAGED_LOOKASIDE_LIST {
    GENERAL_LOOKASIDE l;
};
using PNPAGED_LOOKASIDE_LIST = NPAGED_LOOKASIDE_LIST*;

struct PAGED_LOOKASIDE_LIST {
    GENERAL_LOOKASIDE l;
    FAST_MUTEX lock;
};
using PPAGED_LOOKASIDE_LIST = PAGED_LOOKASIDE_LIST*;

inline void ExInitializeNPagedLookasideList(
    PNPAGED_LOOKASIDE_LIST lookaside,
    PALLOCATE_FUNCTION allocate,
    PFREE_FUNCTION free,
    uint32_t flags,
    size_t size,
    uint32_t tag,
    uint16_t depth
) {
    if (!lookaside) return;
    (void)flags;
    InitializeSListHead(&lookaside->l.listHead);
    lookaside->l.depth = (depth > 0) ? depth : 4;
    lookaside->l.maximumDepth = 256;
    lookaside->l.totalAllocates = 0;
    lookaside->l.allocateMisses = 0;
    lookaside->l.totalFrees = 0;
    lookaside->l.freeMisses = 0;
    lookaside->l.type = PoolType::NonPagedPool;
    lookaside->l.tag = tag;
    lookaside->l.size = (size >= sizeof(SLIST_ENTRY)) ? size : sizeof(SLIST_ENTRY);
    lookaside->l.allocate = allocate;
    lookaside->l.free = free;
}

inline void* ExAllocateFromNPagedLookasideList(PNPAGED_LOOKASIDE_LIST lookaside) {
    if (!lookaside) return nullptr;
    lookaside->l.totalAllocates++;
    auto* entry = InterlockedPopEntrySList(&lookaside->l.listHead);
    if (entry) {
        return entry;
    }
    lookaside->l.allocateMisses++;
    if (lookaside->l.allocate) {
        return lookaside->l.allocate(lookaside->l.type, lookaside->l.size, lookaside->l.tag);
    }
    return ExAllocatePoolWithTag(lookaside->l.type, lookaside->l.size, lookaside->l.tag);
}

inline void ExFreeToNPagedLookasideList(PNPAGED_LOOKASIDE_LIST lookaside, void* entry) {
    if (!lookaside || !entry) return;
    lookaside->l.totalFrees++;
    if (QueryDepthSList(&lookaside->l.listHead) < lookaside->l.depth) {
        InterlockedPushEntrySList(&lookaside->l.listHead, reinterpret_cast<PSLIST_ENTRY>(entry));
    } else {
        lookaside->l.freeMisses++;
        if (lookaside->l.free) {
            lookaside->l.free(entry);
        } else {
            ExFreePoolWithTag(entry, lookaside->l.tag);
        }
    }
}

inline void ExDeleteNPagedLookasideList(PNPAGED_LOOKASIDE_LIST lookaside) {
    if (!lookaside) return;
    auto* entry = InterlockedFlushSList(&lookaside->l.listHead);
    while (entry) {
        auto* next = entry->next;
        if (lookaside->l.free) {
            lookaside->l.free(entry);
        } else {
            ExFreePoolWithTag(entry, lookaside->l.tag);
        }
        entry = next;
    }
}

// Paged Lookaside List API
inline void ExInitializePagedLookasideList(
    PPAGED_LOOKASIDE_LIST lookaside,
    PALLOCATE_FUNCTION allocate,
    PFREE_FUNCTION free,
    uint32_t flags,
    size_t size,
    uint32_t tag,
    uint16_t depth
) {
    if (!lookaside) return;
    (void)flags;
    InitializeSListHead(&lookaside->l.listHead);
    ExInitializeFastMutex(&lookaside->lock);
    lookaside->l.depth = (depth > 0) ? depth : 4;
    lookaside->l.maximumDepth = 256;
    lookaside->l.totalAllocates = 0;
    lookaside->l.allocateMisses = 0;
    lookaside->l.totalFrees = 0;
    lookaside->l.freeMisses = 0;
    lookaside->l.type = PoolType::PagedPool;
    lookaside->l.tag = tag;
    lookaside->l.size = (size >= sizeof(SLIST_ENTRY)) ? size : sizeof(SLIST_ENTRY);
    lookaside->l.allocate = allocate;
    lookaside->l.free = free;
}

inline void* ExAllocateFromPagedLookasideList(PPAGED_LOOKASIDE_LIST lookaside) {
    if (!lookaside) return nullptr;
    if (ke::KeGetCurrentIrql() >= ke::DISPATCH_LEVEL) {
        std::cerr << "[MicaNT BugCheck] IRQL_NOT_LESS_OR_EQUAL: PagedLookasideList allocate at IRQL >= DISPATCH_LEVEL\n";
        return nullptr;
    }
    lookaside->l.totalAllocates++;
    auto* entry = InterlockedPopEntrySList(&lookaside->l.listHead);
    if (entry) {
        return entry;
    }
    lookaside->l.allocateMisses++;
    if (lookaside->l.allocate) {
        return lookaside->l.allocate(lookaside->l.type, lookaside->l.size, lookaside->l.tag);
    }
    return ExAllocatePoolWithTag(lookaside->l.type, lookaside->l.size, lookaside->l.tag);
}

inline void ExFreeToPagedLookasideList(PPAGED_LOOKASIDE_LIST lookaside, void* entry) {
    if (!lookaside || !entry) return;
    lookaside->l.totalFrees++;
    if (QueryDepthSList(&lookaside->l.listHead) < lookaside->l.depth) {
        InterlockedPushEntrySList(&lookaside->l.listHead, reinterpret_cast<PSLIST_ENTRY>(entry));
    } else {
        lookaside->l.freeMisses++;
        if (lookaside->l.free) {
            lookaside->l.free(entry);
        } else {
            ExFreePoolWithTag(entry, lookaside->l.tag);
        }
    }
}

inline void ExDeletePagedLookasideList(PPAGED_LOOKASIDE_LIST lookaside) {
    if (!lookaside) return;
    auto* entry = InterlockedFlushSList(&lookaside->l.listHead);
    while (entry) {
        auto* next = entry->next;
        if (lookaside->l.free) {
            lookaside->l.free(entry);
        } else {
            ExFreePoolWithTag(entry, lookaside->l.tag);
        }
        entry = next;
    }
}

// ============================================================================
// System Time Architecture (100-ns intervals since January 1, 1601 NT Epoch)
// ============================================================================
inline uint64_t KeQuerySystemTime() noexcept {
    constexpr uint64_t NT_EPOCH_OFFSET = 116444736000000000ULL;
    auto now = std::chrono::system_clock::now().time_since_epoch();
    uint64_t ns100 = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count() / 100;
    return NT_EPOCH_OFFSET + ns100;
}

} // namespace micant::ex
