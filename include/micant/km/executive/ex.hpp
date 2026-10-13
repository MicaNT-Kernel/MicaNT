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
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/km/executive/probe.hpp"

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
    GetSyncTelemetry().totalFastMutexAcquisitions++;
}

inline BOOLEAN ExTryToAcquireFastMutex(PFAST_MUTEX fastMutex) {
    if (!fastMutex) return FALSE;
    ke::KIRQL old = ke::KfRaiseIrql(ke::APC_LEVEL);
    if (fastMutex->gateLock.try_lock()) {
        fastMutex->count.store(0, std::memory_order_relaxed);
        fastMutex->oldIrql = old;
        fastMutex->owner = ExGetCurrentThreadId();
        GetSyncTelemetry().totalFastMutexAcquisitions++;
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

struct OWNER_ENTRY {
    uint64_t ownerThread{0};
    int32_t ownerCount{0};
};

struct ERESOURCE {
    mutable std::mutex lock;
    std::condition_variable cv;
    uint32_t activeCount{0};
    uint32_t sharedOwners{0};
    uint32_t exclusiveOwners{0};
    uint64_t exclusiveOwnerThread{0};
    uint32_t waiters{0};
    uint32_t numberOfSharedWaiters{0};
    uint32_t numberOfExclusiveWaiters{0};
    uint16_t flag{0};
    std::vector<OWNER_ENTRY> ownerTable;
};
using PERESOURCE = ERESOURCE*;

inline void ExInitializeResourceLite(PERESOURCE resource) {
    if (!resource) return;
    std::unique_lock<std::mutex> lk(resource->lock);
    resource->activeCount = 0;
    resource->sharedOwners = 0;
    resource->exclusiveOwners = 0;
    resource->exclusiveOwnerThread = 0;
    resource->waiters = 0;
    resource->numberOfSharedWaiters = 0;
    resource->numberOfExclusiveWaiters = 0;
    resource->flag = 0;
    resource->ownerTable.clear();
}

inline void ExReinitializeResourceLite(PERESOURCE resource) {
    ExInitializeResourceLite(resource);
}

inline BOOLEAN ExAcquireResourceExclusiveLite(PERESOURCE resource, BOOLEAN wait) {
    if (!resource) return FALSE;
    uint64_t currentTid = ExGetCurrentThreadId();
    std::unique_lock<std::mutex> lk(resource->lock);

    // 1. Reentrant recursive exclusive acquisition by current owner
    if (resource->exclusiveOwnerThread == currentTid) {
        resource->exclusiveOwners++;
        GetSyncTelemetry().totalExclusiveAcquisitions++;
        return TRUE;
    }

    // 2. Uncontended acquisition
    if (resource->exclusiveOwners == 0 && resource->sharedOwners == 0) {
        resource->exclusiveOwnerThread = currentTid;
        resource->exclusiveOwners = 1;
        resource->activeCount = 1;
        GetSyncTelemetry().totalExclusiveAcquisitions++;
        return TRUE;
    }

    // 3. Contended non-blocking attempt
    if (!wait) {
        GetSyncTelemetry().totalResourceTryAcquireFailures++;
        return FALSE;
    }

    // 4. Wait for resource to be completely free
    resource->waiters++;
    resource->numberOfExclusiveWaiters++;
    resource->cv.wait(lk, [&]() {
        return resource->exclusiveOwners == 0 && resource->sharedOwners == 0;
    });
    resource->waiters--;
    resource->numberOfExclusiveWaiters--;

    resource->exclusiveOwnerThread = currentTid;
    resource->exclusiveOwners = 1;
    resource->activeCount = 1;
    GetSyncTelemetry().totalExclusiveAcquisitions++;
    return TRUE;
}

inline BOOLEAN ExTryToAcquireResourceExclusiveLite(PERESOURCE resource) {
    return ExAcquireResourceExclusiveLite(resource, FALSE);
}

inline BOOLEAN ExAcquireResourceSharedLite(PERESOURCE resource, BOOLEAN wait) {
    if (!resource) return FALSE;
    uint64_t currentTid = ExGetCurrentThreadId();
    std::unique_lock<std::mutex> lk(resource->lock);

    auto findOwner = [&](uint64_t tid) -> OWNER_ENTRY* {
        for (auto& entry : resource->ownerTable) {
            if (entry.ownerThread == tid) return &entry;
        }
        return nullptr;
    };

    // 1. Reentrant acquisition if caller already holds exclusive lock
    if (resource->exclusiveOwnerThread == currentTid) {
        resource->sharedOwners++;
        resource->activeCount = resource->sharedOwners;
        GetSyncTelemetry().totalSharedAcquisitions++;
        return TRUE;
    }

    // 2. Reentrant acquisition if caller already holds shared lock
    OWNER_ENTRY* existing = findOwner(currentTid);
    if (existing != nullptr) {
        existing->ownerCount++;
        resource->sharedOwners++;
        resource->activeCount = resource->sharedOwners;
        GetSyncTelemetry().totalSharedAcquisitions++;
        return TRUE;
    }

    // 3. New acquisition - verify no exclusive owner and prevent writer starvation
    if (resource->exclusiveOwners == 0 && resource->numberOfExclusiveWaiters == 0) {
        resource->ownerTable.push_back({currentTid, 1});
        resource->sharedOwners++;
        resource->activeCount = resource->sharedOwners;
        GetSyncTelemetry().totalSharedAcquisitions++;
        return TRUE;
    }

    if (!wait) {
        GetSyncTelemetry().totalResourceTryAcquireFailures++;
        return FALSE;
    }

    // 4. Wait for exclusive holders and exclusive waiters to clear
    resource->waiters++;
    resource->numberOfSharedWaiters++;
    resource->cv.wait(lk, [&]() {
        return resource->exclusiveOwners == 0 && resource->numberOfExclusiveWaiters == 0;
    });
    resource->waiters--;
    resource->numberOfSharedWaiters--;

    resource->ownerTable.push_back({currentTid, 1});
    resource->sharedOwners++;
    resource->activeCount = resource->sharedOwners;
    GetSyncTelemetry().totalSharedAcquisitions++;
    return TRUE;
}

inline BOOLEAN ExTryToAcquireResourceSharedLite(PERESOURCE resource) {
    return ExAcquireResourceSharedLite(resource, FALSE);
}

inline void ExReleaseResourceForThreadLite(PERESOURCE resource, uint64_t threadId) {
    if (!resource) return;
    std::unique_lock<std::mutex> lk(resource->lock);

    // 1. Exclusive release
    if (resource->exclusiveOwnerThread == threadId && resource->exclusiveOwners > 0) {
        resource->exclusiveOwners--;
        if (resource->exclusiveOwners == 0) {
            resource->exclusiveOwnerThread = 0;
            resource->activeCount = resource->sharedOwners;
            resource->cv.notify_all();
        }
        return;
    }

    // 2. Shared release with ownerTable tracking
    for (auto it = resource->ownerTable.begin(); it != resource->ownerTable.end(); ++it) {
        if (it->ownerThread == threadId) {
            it->ownerCount--;
            if (resource->sharedOwners > 0) resource->sharedOwners--;
            if (it->ownerCount <= 0) {
                resource->ownerTable.erase(it);
            }
            resource->activeCount = resource->sharedOwners;
            if (resource->sharedOwners == 0) {
                resource->cv.notify_all();
            }
            return;
        }
    }

    // Fallback release if not registered in ownerTable
    if (resource->sharedOwners > 0) {
        resource->sharedOwners--;
        resource->activeCount = resource->sharedOwners;
        if (resource->sharedOwners == 0) {
            resource->cv.notify_all();
        }
    }
}

inline void ExReleaseResourceLite(PERESOURCE resource) {
    if (!resource) return;
    ExReleaseResourceForThreadLite(resource, ExGetCurrentThreadId());
}

inline void ExConvertExclusiveToSharedLite(PERESOURCE resource) {
    if (!resource) return;
    uint64_t currentTid = ExGetCurrentThreadId();
    std::unique_lock<std::mutex> lk(resource->lock);

    if (resource->exclusiveOwnerThread == currentTid && resource->exclusiveOwners > 0) {
        uint32_t count = resource->exclusiveOwners;
        resource->exclusiveOwners = 0;
        resource->exclusiveOwnerThread = 0;
        resource->sharedOwners += count;
        resource->activeCount = resource->sharedOwners;
        resource->ownerTable.push_back({currentTid, static_cast<int32_t>(count)});
        GetSyncTelemetry().totalResourceConversions++;
        resource->cv.notify_all();
    }
}

inline BOOLEAN ExTryConvertSharedToExclusiveLite(PERESOURCE resource) {
    if (!resource) return FALSE;
    uint64_t currentTid = ExGetCurrentThreadId();
    std::unique_lock<std::mutex> lk(resource->lock);

    if (resource->exclusiveOwners != 0) return FALSE;

    int32_t callerCount = 0;
    auto it = resource->ownerTable.begin();
    while (it != resource->ownerTable.end()) {
        if (it->ownerThread == currentTid) {
            callerCount = it->ownerCount;
            break;
        }
        ++it;
    }

    if (callerCount > 0 && resource->sharedOwners == static_cast<uint32_t>(callerCount)) {
        resource->ownerTable.erase(it);
        resource->sharedOwners = 0;
        resource->exclusiveOwnerThread = currentTid;
        resource->exclusiveOwners = static_cast<uint32_t>(callerCount);
        resource->activeCount = 1;
        GetSyncTelemetry().totalResourceConversions++;
        return TRUE;
    }
    return FALSE;
}

inline BOOLEAN ExIsResourceAcquiredExclusiveLite(PERESOURCE resource) {
    if (!resource) return FALSE;
    std::unique_lock<std::mutex> lk(resource->lock);
    return (resource->exclusiveOwnerThread == ExGetCurrentThreadId()) ? TRUE : FALSE;
}

inline uint32_t ExIsResourceAcquiredSharedLite(PERESOURCE resource) {
    if (!resource) return 0;
    uint64_t currentTid = ExGetCurrentThreadId();
    std::unique_lock<std::mutex> lk(resource->lock);
    if (resource->exclusiveOwnerThread == currentTid) {
        return resource->exclusiveOwners + resource->sharedOwners;
    }
    for (const auto& entry : resource->ownerTable) {
        if (entry.ownerThread == currentTid) return static_cast<uint32_t>(entry.ownerCount);
    }
    return 0;
}

inline uint32_t ExGetSharedWaiterCount(PERESOURCE resource) {
    if (!resource) return 0;
    std::unique_lock<std::mutex> lk(resource->lock);
    return resource->numberOfSharedWaiters;
}

inline uint32_t ExGetExclusiveWaiterCount(PERESOURCE resource) {
    if (!resource) return 0;
    std::unique_lock<std::mutex> lk(resource->lock);
    return resource->numberOfExclusiveWaiters;
}

inline void ExDeleteResourceLite(PERESOURCE resource) {
    if (!resource) return;
    std::unique_lock<std::mutex> lk(resource->lock);
    resource->activeCount = 0;
    resource->sharedOwners = 0;
    resource->exclusiveOwners = 0;
    resource->exclusiveOwnerThread = 0;
    resource->waiters = 0;
    resource->numberOfSharedWaiters = 0;
    resource->numberOfExclusiveWaiters = 0;
    resource->ownerTable.clear();
}

class ResourceSharedLock {
public:
    explicit ResourceSharedLock(PERESOURCE res, BOOLEAN wait = TRUE)
        : resource_(res) {
        if (resource_) acquired_ = ExAcquireResourceSharedLite(resource_, wait);
    }
    ~ResourceSharedLock() {
        if (resource_ && acquired_) ExReleaseResourceLite(resource_);
    }
    [[nodiscard]] BOOLEAN isAcquired() const noexcept { return acquired_; }
    ResourceSharedLock(const ResourceSharedLock&) = delete;
    ResourceSharedLock& operator=(const ResourceSharedLock&) = delete;
private:
    PERESOURCE resource_{nullptr};
    BOOLEAN acquired_{FALSE};
};

class ResourceExclusiveLock {
public:
    explicit ResourceExclusiveLock(PERESOURCE res, BOOLEAN wait = TRUE)
        : resource_(res) {
        if (resource_) acquired_ = ExAcquireResourceExclusiveLite(resource_, wait);
    }
    ~ResourceExclusiveLock() {
        if (resource_ && acquired_) ExReleaseResourceLite(resource_);
    }
    [[nodiscard]] BOOLEAN isAcquired() const noexcept { return acquired_; }
    ResourceExclusiveLock(const ResourceExclusiveLock&) = delete;
    ResourceExclusiveLock& operator=(const ResourceExclusiveLock&) = delete;
private:
    PERESOURCE resource_{nullptr};
    BOOLEAN acquired_{FALSE};
};

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
    GetSyncTelemetry().totalPushLocksAcquiredExclusive++;
}

inline BOOLEAN ExTryAcquirePushLockExclusive(PEX_PUSH_LOCK pushLock) {
    if (!pushLock) return FALSE;
    std::unique_lock<std::mutex> lk(pushLock->lock);
    if (pushLock->locked == 0 && pushLock->shared == 0) {
        pushLock->locked = 1;
        GetSyncTelemetry().totalPushLocksAcquiredExclusive++;
        return TRUE;
    }
    return FALSE;
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
    GetSyncTelemetry().totalPushLocksAcquiredShared++;
}

inline BOOLEAN ExTryAcquirePushLockShared(PEX_PUSH_LOCK pushLock) {
    if (!pushLock) return FALSE;
    std::unique_lock<std::mutex> lk(pushLock->lock);
    if (pushLock->locked == 0) {
        pushLock->shared++;
        GetSyncTelemetry().totalPushLocksAcquiredShared++;
        return TRUE;
    }
    return FALSE;
}

inline void ExConvertPushLockExclusiveToShared(PEX_PUSH_LOCK pushLock) {
    if (!pushLock) return;
    std::unique_lock<std::mutex> lk(pushLock->lock);
    if (pushLock->locked == 1) {
        pushLock->locked = 0;
        pushLock->shared = 1;
        GetSyncTelemetry().totalPushLockConversions++;
        pushLock->cv.notify_all();
    }
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

class PushLockSharedGuard {
public:
    explicit PushLockSharedGuard(PEX_PUSH_LOCK lock) : lock_(lock) {
        if (lock_) ExAcquirePushLockShared(lock_);
    }
    ~PushLockSharedGuard() {
        if (lock_) ExReleasePushLockShared(lock_);
    }
    PushLockSharedGuard(const PushLockSharedGuard&) = delete;
    PushLockSharedGuard& operator=(const PushLockSharedGuard&) = delete;
private:
    PEX_PUSH_LOCK lock_{nullptr};
};

class PushLockExclusiveGuard {
public:
    explicit PushLockExclusiveGuard(PEX_PUSH_LOCK lock) : lock_(lock) {
        if (lock_) ExAcquirePushLockExclusive(lock_);
    }
    ~PushLockExclusiveGuard() {
        if (lock_) ExReleasePushLockExclusive(lock_);
    }
    PushLockExclusiveGuard(const PushLockExclusiveGuard&) = delete;
    PushLockExclusiveGuard& operator=(const PushLockExclusiveGuard&) = delete;
private:
    PEX_PUSH_LOCK lock_{nullptr};
};

// ============================================================================
// Guarded Mutex (KGUARDED_MUTEX)
// ============================================================================
struct KGUARDED_MUTEX {
    std::atomic<int32_t> count{1};
    uint64_t owner{0};
    uint32_t contention{0};
    std::mutex gateLock;
};
using PKGUARDED_MUTEX = KGUARDED_MUTEX*;

inline void KeInitializeGuardedMutex(PKGUARDED_MUTEX mutex) {
    if (!mutex) return;
    mutex->count.store(1, std::memory_order_relaxed);
    mutex->owner = 0;
    mutex->contention = 0;
}

inline void KeAcquireGuardedMutex(PKGUARDED_MUTEX mutex) {
    if (!mutex) return;
    ke::KeEnterGuardedRegion();
    mutex->gateLock.lock();
    mutex->count.store(0, std::memory_order_relaxed);
    mutex->owner = ExGetCurrentThreadId();
    GetSyncTelemetry().totalGuardedMutexAcquisitions++;
}

inline BOOLEAN KeTryAcquireGuardedMutex(PKGUARDED_MUTEX mutex) {
    if (!mutex) return FALSE;
    ke::KeEnterGuardedRegion();
    if (mutex->gateLock.try_lock()) {
        mutex->count.store(0, std::memory_order_relaxed);
        mutex->owner = ExGetCurrentThreadId();
        GetSyncTelemetry().totalGuardedMutexAcquisitions++;
        return TRUE;
    }
    ke::KeLeaveGuardedRegion();
    return FALSE;
}

inline void KeReleaseGuardedMutex(PKGUARDED_MUTEX mutex) {
    if (!mutex) return;
    mutex->owner = 0;
    mutex->count.store(1, std::memory_order_relaxed);
    mutex->gateLock.unlock();
    ke::KeLeaveGuardedRegion();
}

inline void KeAcquireGuardedMutexUnsafe(PKGUARDED_MUTEX mutex) {
    if (!mutex) return;
    mutex->gateLock.lock();
    mutex->count.store(0, std::memory_order_relaxed);
    mutex->owner = ExGetCurrentThreadId();
    GetSyncTelemetry().totalGuardedMutexAcquisitions++;
}

inline void KeReleaseGuardedMutexUnsafe(PKGUARDED_MUTEX mutex) {
    if (!mutex) return;
    mutex->owner = 0;
    mutex->count.store(1, std::memory_order_relaxed);
    mutex->gateLock.unlock();
}

class GuardedMutexGuard {
public:
    explicit GuardedMutexGuard(PKGUARDED_MUTEX mutex) : mutex_(mutex) {
        if (mutex_) KeAcquireGuardedMutex(mutex_);
    }
    ~GuardedMutexGuard() {
        if (mutex_) KeReleaseGuardedMutex(mutex_);
    }
    GuardedMutexGuard(const GuardedMutexGuard&) = delete;
    GuardedMutexGuard& operator=(const GuardedMutexGuard&) = delete;
private:
    PKGUARDED_MUTEX mutex_{nullptr};
};


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

inline void ExAdjustLookasideDepth(GENERAL_LOOKASIDE* lookaside) {
    if (!lookaside) return;
    if (lookaside->totalAllocates > 0) {
        if (lookaside->allocateMisses * 2 > lookaside->totalAllocates) {
            if (lookaside->depth + 4 <= lookaside->maximumDepth) {
                lookaside->depth += 4;
            } else {
                lookaside->depth = lookaside->maximumDepth;
            }
        } else if (lookaside->freeMisses * 2 > lookaside->totalFrees && lookaside->depth > 4) {
            lookaside->depth -= 2;
        }
    }
    GetSyncTelemetry().totalLookasideDynamicAdjustments++;
}

inline void* ExAllocateFromNPagedLookasideList(PNPAGED_LOOKASIDE_LIST lookaside) {
    if (!lookaside) return nullptr;
    lookaside->l.totalAllocates++;
    GetSyncTelemetry().totalLookasideAllocations++;
    auto* entry = InterlockedPopEntrySList(&lookaside->l.listHead);
    if (entry) {
        GetSyncTelemetry().totalLookasideHits++;
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
    GetSyncTelemetry().totalLookasideFrees++;
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
    GetSyncTelemetry().totalLookasideAllocations++;
    auto* entry = InterlockedPopEntrySList(&lookaside->l.listHead);
    if (entry) {
        GetSyncTelemetry().totalLookasideHits++;
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
    GetSyncTelemetry().totalLookasideFrees++;
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
