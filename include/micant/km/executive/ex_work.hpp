#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <iostream>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ke.hpp"

namespace micant::io {
    struct DeviceObject;
}

namespace micant::ex {

// ============================================================================
// 1. Standard NT Executive Work Queue Types (WORK_QUEUE_TYPE)
// ============================================================================
enum class WorkQueueType : uint32_t {
    CriticalWorkQueue       = 0, // High priority worker threads (Priority 13)
    DelayedWorkQueue        = 1, // Standard background deferred tasks (Priority 7)
    HyperCriticalWorkQueue  = 2, // Urgent/real-time kernel work items (Priority 15)
    NormalWorkQueue         = 3, // Normal priority worker tasks (Priority 9)
    BackgroundWorkQueue     = 4, // Low priority / idle background tasks (Priority 4)
    RealTimeWorkQueue       = 5, // Hard real-time kernel worker items (Priority 24)
    MaxWorkQueue            = 6
};

// ============================================================================
// 2. Work Item Callbacks & Structures (WORK_QUEUE_ITEM)
// ============================================================================
using WorkerRoutineCallback = void (*)(void* parameter);

struct WorkQueueItem {
    WorkerRoutineCallback routine{nullptr};
    void* parameter{nullptr};
    std::atomic<bool> inUse{false};
    std::atomic<bool> completed{false};
    WorkQueueType queueType{WorkQueueType::CriticalWorkQueue};
    uint64_t enqueueTimestampNs{0};
    uint64_t executionDurationNs{0};
};

// ============================================================================
// 3. Device-bound Work Items (IO_WORKITEM)
// ============================================================================
using PIO_WORKITEM_ROUTINE = void (*)(io::DeviceObject* deviceObject, void* context);
using PIO_WORKITEM_ROUTINE_EX = void (*)(void* ioObject, void* context, struct IoWorkItem* ioWorkItem);

struct IoWorkItem {
    io::DeviceObject* deviceObject{nullptr};
    PIO_WORKITEM_ROUTINE routine{nullptr};
    PIO_WORKITEM_ROUTINE_EX routineEx{nullptr};
    void* context{nullptr};
    WorkQueueItem workItem{};
    std::atomic<bool> active{false};
};

// ============================================================================
// 4. Worker Queue Statistics & Telemetry
// ============================================================================
struct WorkerQueueStatistics {
    uint64_t itemsQueued{0};
    uint64_t itemsProcessed{0};
    uint32_t currentDepth{0};
    uint32_t peakDepth{0};
    uint64_t totalExecutionTimeNs{0};
    uint32_t activeWorkers{0};
};

// ============================================================================
// 5. Sovereign Executive Worker Queue Subsystem
// ============================================================================
class ExecutiveWorkQueueManager {
public:
    static ExecutiveWorkQueueManager& get() {
        static ExecutiveWorkQueueManager instance;
        return instance;
    }

    void initialize(size_t workerThreadCount = 2, size_t maxWorkerCount = 8) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) return;
        running_ = true;
        minWorkers_ = std::max<size_t>(1, workerThreadCount);
        maxWorkers_ = std::max<size_t>(minWorkers_, maxWorkerCount);

        for (size_t i = 0; i < minWorkers_; ++i) {
            spawnWorkerThreadLocked();
        }
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) return;
            running_ = false;
        }
        cv_.notify_all();
        completionCv_.notify_all();

        for (auto& t : workers_) {
            if (t.joinable()) {
                t.join();
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            workers_.clear();
            for (auto& q : queues_) {
                while (!q.empty()) {
                    auto* item = q.front();
                    q.pop();
                    if (item) {
                        item->inUse = false;
                        item->completed = true;
                    }
                }
            }
        }
    }

    ~ExecutiveWorkQueueManager() {
        shutdown();
    }

    [[nodiscard]] bool isRunning() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    void queueWorkItem(WorkQueueItem* item, WorkQueueType queueType) {
        if (!item || !item->routine) return;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                // Proactively auto-initialize if not yet running
                running_ = true;
                spawnWorkerThreadLocked();
                spawnWorkerThreadLocked();
            }

            item->inUse = true;
            item->completed = false;
            item->queueType = queueType;
            item->enqueueTimestampNs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()
                ).count()
            );

            uint32_t qIdx = getQueueIndex(queueType);
            queues_[qIdx].push(item);
            totalQueued_++;

            // Telemetry update
            stats_[qIdx].itemsQueued++;
            stats_[qIdx].currentDepth = static_cast<uint32_t>(queues_[qIdx].size());
            if (stats_[qIdx].currentDepth > stats_[qIdx].peakDepth) {
                stats_[qIdx].peakDepth = stats_[qIdx].currentDepth;
            }

            // Dynamic Worker Pool Autoscaling:
            // If queues are accumulating depth and we haven't reached maxWorkers, spawn additional worker
            if (stats_[qIdx].currentDepth > workers_.size() && workers_.size() < maxWorkers_) {
                spawnWorkerThreadLocked();
            }
        }
        cv_.notify_one();
    }

    // ------------------------------------------------------------------------
    // Synchronization & Queue Flushing
    // ------------------------------------------------------------------------
    bool flushWorkQueue(WorkQueueType queueType, uint32_t timeoutMs = 5000) {
        uint32_t qIdx = getQueueIndex(queueType);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

        std::unique_lock<std::mutex> lock(mutex_);
        return completionCv_.wait_until(lock, deadline, [this, qIdx]() {
            return queues_[qIdx].empty() && activeTaskCountPerQueue_[qIdx] == 0;
        });
    }

    bool flushAll(uint32_t timeoutMs = 5000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        std::unique_lock<std::mutex> lock(mutex_);
        return completionCv_.wait_until(lock, deadline, [this]() {
            for (size_t i = 0; i < static_cast<size_t>(WorkQueueType::MaxWorkQueue); ++i) {
                if (!queues_[i].empty() || activeTaskCountPerQueue_[i] > 0) {
                    return false;
                }
            }
            return true;
        });
    }

    bool waitForWorkItem(WorkQueueItem* item, uint32_t timeoutMs = 5000) {
        if (!item) return false;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        std::unique_lock<std::mutex> lock(mutex_);
        return completionCv_.wait_until(lock, deadline, [item]() {
            return item->completed.load(std::memory_order_acquire);
        });
    }

    // ------------------------------------------------------------------------
    // Device-bound IoWorkItem Support
    // ------------------------------------------------------------------------
    IoWorkItem* allocateIoWorkItem(io::DeviceObject* dev) {
        auto* item = new IoWorkItem();
        item->deviceObject = dev;
        item->active = false;
        return item;
    }

    void freeIoWorkItem(IoWorkItem* ioWorkItem) {
        if (!ioWorkItem) return;
        if (ioWorkItem->active.load(std::memory_order_acquire)) {
            waitForWorkItem(&ioWorkItem->workItem, 2000);
        }
        delete ioWorkItem;
    }

    void queueIoWorkItem(
        IoWorkItem* ioWorkItem,
        PIO_WORKITEM_ROUTINE routine,
        WorkQueueType queueType,
        void* context
    ) {
        if (!ioWorkItem || !routine) return;
        ioWorkItem->routine = routine;
        ioWorkItem->routineEx = nullptr;
        ioWorkItem->context = context;
        ioWorkItem->active = true;

        ioWorkItem->workItem.routine = [](void* ctx) {
            auto* ioItem = static_cast<IoWorkItem*>(ctx);
            if (ioItem && ioItem->routine) {
                ioItem->routine(ioItem->deviceObject, ioItem->context);
                ioItem->active = false;
            }
        };
        ioWorkItem->workItem.parameter = ioWorkItem;
        queueWorkItem(&ioWorkItem->workItem, queueType);
    }

    void queueIoWorkItemEx(
        IoWorkItem* ioWorkItem,
        PIO_WORKITEM_ROUTINE_EX routineEx,
        WorkQueueType queueType,
        void* context
    ) {
        if (!ioWorkItem || !routineEx) return;
        ioWorkItem->routine = nullptr;
        ioWorkItem->routineEx = routineEx;
        ioWorkItem->context = context;
        ioWorkItem->active = true;

        ioWorkItem->workItem.routine = [](void* ctx) {
            auto* ioItem = static_cast<IoWorkItem*>(ctx);
            if (ioItem && ioItem->routineEx) {
                ioItem->routineEx(ioItem->deviceObject, ioItem->context, ioItem);
                ioItem->active = false;
            }
        };
        ioWorkItem->workItem.parameter = ioWorkItem;
        queueWorkItem(&ioWorkItem->workItem, queueType);
    }

    // ------------------------------------------------------------------------
    // Telemetry & Statistics
    // ------------------------------------------------------------------------
    [[nodiscard]] uint64_t getTotalProcessed() const noexcept {
        return totalProcessed_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t getTotalQueued() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return totalQueued_;
    }

    [[nodiscard]] size_t getActiveWorkerCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return workers_.size();
    }

    [[nodiscard]] WorkerQueueStatistics getQueueStatistics(WorkQueueType queueType) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t qIdx = getQueueIndex(queueType);
        return stats_[qIdx];
    }

    void resetStatistics() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& s : stats_) {
            s = WorkerQueueStatistics{};
        }
        totalQueued_ = 0;
        totalProcessed_ = 0;
    }

private:
    ExecutiveWorkQueueManager() = default;

    void spawnWorkerThreadLocked() {
        workers_.emplace_back([this]() {
            workerThreadLoop();
        });
    }

    static constexpr uint32_t getQueueIndex(WorkQueueType type) noexcept {
        uint32_t idx = static_cast<uint32_t>(type);
        if (idx >= static_cast<uint32_t>(WorkQueueType::MaxWorkQueue)) {
            return static_cast<uint32_t>(WorkQueueType::CriticalWorkQueue);
        }
        return idx;
    }

    void workerThreadLoop() {
        // Executive worker threads run strictly at PASSIVE_LEVEL (IRQL 0)
        ke::KeLowerIrql(ke::PASSIVE_LEVEL);

        while (true) {
            WorkQueueItem* itemToRun = nullptr;
            uint32_t activeQueueIdx = 0;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() {
                    if (!running_) return true;
                    for (int i = 0; i < static_cast<int>(WorkQueueType::MaxWorkQueue); ++i) {
                        if (!queues_[i].empty()) return true;
                    }
                    return false;
                });

                if (!running_) {
                    // Check if any work remains
                    bool hasItems = false;
                    for (int i = 0; i < static_cast<int>(WorkQueueType::MaxWorkQueue); ++i) {
                        if (!queues_[i].empty()) { hasItems = true; break; }
                    }
                    if (!hasItems) break;
                }

                // Strict Priority Dispatch Order:
                // RealTime (5) > HyperCritical (2) > Critical (0) > Normal (3) > Delayed (1) > Background (4)
                constexpr uint32_t priorityOrder[] = {
                    static_cast<uint32_t>(WorkQueueType::RealTimeWorkQueue),
                    static_cast<uint32_t>(WorkQueueType::HyperCriticalWorkQueue),
                    static_cast<uint32_t>(WorkQueueType::CriticalWorkQueue),
                    static_cast<uint32_t>(WorkQueueType::NormalWorkQueue),
                    static_cast<uint32_t>(WorkQueueType::DelayedWorkQueue),
                    static_cast<uint32_t>(WorkQueueType::BackgroundWorkQueue)
                };

                for (uint32_t pIdx : priorityOrder) {
                    if (!queues_[pIdx].empty()) {
                        itemToRun = queues_[pIdx].front();
                        queues_[pIdx].pop();
                        activeQueueIdx = pIdx;
                        stats_[pIdx].currentDepth = static_cast<uint32_t>(queues_[pIdx].size());
                        activeTaskCountPerQueue_[pIdx]++;
                        break;
                    }
                }
            }

            if (itemToRun && itemToRun->routine) {
                auto startTs = std::chrono::steady_clock::now();

                // Assert PASSIVE_LEVEL before calling worker routine
                if (ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL) {
                    try {
                        itemToRun->routine(itemToRun->parameter);
                    } catch (...) {
                        std::cerr << "[MicaNT Ex] Worker routine threw unhandled exception!\n";
                    }
                }

                // Restore PASSIVE_LEVEL if worker improperly changed IRQL
                if (ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL) {
                    ke::KeLowerIrql(ke::PASSIVE_LEVEL);
                }

                auto endTs = std::chrono::steady_clock::now();
                uint64_t durationNs = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(endTs - startTs).count()
                );
                itemToRun->executionDurationNs = durationNs;
                itemToRun->inUse = false;
                itemToRun->completed.store(true, std::memory_order_release);

                totalProcessed_.fetch_add(1, std::memory_order_relaxed);

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    activeTaskCountPerQueue_[activeQueueIdx]--;
                    stats_[activeQueueIdx].itemsProcessed++;
                    stats_[activeQueueIdx].totalExecutionTimeNs += durationNs;
                }

                completionCv_.notify_all();
            }
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable completionCv_;
    bool running_{false};
    size_t minWorkers_{2};
    size_t maxWorkers_{8};
    std::vector<std::thread> workers_;
    std::queue<WorkQueueItem*> queues_[static_cast<size_t>(WorkQueueType::MaxWorkQueue)];
    uint32_t activeTaskCountPerQueue_[static_cast<size_t>(WorkQueueType::MaxWorkQueue)]{};
    WorkerQueueStatistics stats_[static_cast<size_t>(WorkQueueType::MaxWorkQueue)]{};

    uint64_t totalQueued_{0};
    std::atomic<uint64_t> totalProcessed_{0};
};

// ============================================================================
// 6. Standard NT Executive Work Item Functions (DDK Surface)
// ============================================================================
inline void ExInitializeWorkItem(
    WorkQueueItem* item,
    WorkerRoutineCallback routine,
    void* context
) noexcept {
    if (item) {
        item->routine = routine;
        item->parameter = context;
        item->inUse = false;
        item->completed = false;
        item->queueType = WorkQueueType::CriticalWorkQueue;
        item->enqueueTimestampNs = 0;
        item->executionDurationNs = 0;
    }
}

inline void ExQueueWorkItem(
    WorkQueueItem* item,
    WorkQueueType queueType
) {
    ExecutiveWorkQueueManager::get().queueWorkItem(item, queueType);
}

inline bool ExFlushWorkQueue(
    WorkQueueType queueType,
    uint32_t timeoutMs = 5000
) {
    return ExecutiveWorkQueueManager::get().flushWorkQueue(queueType, timeoutMs);
}

inline bool ExWaitForWorkItem(
    WorkQueueItem* item,
    uint32_t timeoutMs = 5000
) {
    return ExecutiveWorkQueueManager::get().waitForWorkItem(item, timeoutMs);
}

// ============================================================================
// 7. Standard NT I/O Work Item Functions (DDK Surface)
// ============================================================================
inline IoWorkItem* IoAllocateWorkItem(io::DeviceObject* deviceObject) {
    return ExecutiveWorkQueueManager::get().allocateIoWorkItem(deviceObject);
}

inline void IoFreeWorkItem(IoWorkItem* ioWorkItem) {
    ExecutiveWorkQueueManager::get().freeIoWorkItem(ioWorkItem);
}

inline void IoQueueWorkItem(
    IoWorkItem* ioWorkItem,
    PIO_WORKITEM_ROUTINE routine,
    WorkQueueType queueType,
    void* context
) {
    ExecutiveWorkQueueManager::get().queueIoWorkItem(ioWorkItem, routine, queueType, context);
}

inline void IoQueueWorkItemEx(
    IoWorkItem* ioWorkItem,
    PIO_WORKITEM_ROUTINE_EX routineEx,
    WorkQueueType queueType,
    void* context
) {
    ExecutiveWorkQueueManager::get().queueIoWorkItemEx(ioWorkItem, routineEx, queueType, context);
}

} // namespace micant::ex
