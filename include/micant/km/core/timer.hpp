#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <memory>
#include <iostream>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/km/core/sync.hpp"
#include "micant/km/executive/ob.hpp"

namespace micant::timer {

// ============================================================================
// 1. Standard NT Kernel Timer Types & Constants
// ============================================================================
enum class TimerType : uint32_t {
    NotificationTimer    = 0, // Manual-reset: Awakens all waiters when signaled
    SynchronizationTimer = 1  // Auto-reset: Awakens a single waiter and resets
};

inline constexpr uint32_t TIMER_TABLE_SIZE           = 256;
inline constexpr uint64_t TIMER_STANDARD_TICK_NS      = 15'625'000ULL; // 15.625ms standard NT clock tick

inline constexpr uint32_t EX_TIMER_HIGH_RESOLUTION    = 0x00000001; // Sub-millisecond HPET/TSC precision
inline constexpr uint32_t EX_TIMER_NO_WAKE            = 0x00000002; // Do not wake from Modern Standby
inline constexpr uint32_t EX_TIMER_NOTIFICATION       = 0x00000004; // Manual-reset semantics

// ============================================================================
// 2. DPC Watchdog & Timer Telemetry Structures
// ============================================================================
struct KDPC_WATCHDOG_INFORMATION {
    uint32_t dpcTimeLimit{500};      // Single DPC limit in ms (default 500ms)
    uint32_t dpcTimeCount{0};        // Current elapsed DPC time in ms
    uint32_t dpcWatchdogLimit{1000}; // Cumulative DPC limit in ms (default 1000ms)
    uint32_t dpcWatchdogCount{0};    // Cumulative elapsed DPC time in ms
    uint32_t reserved{0};
};

struct TIMER_BASIC_INFORMATION {
    LargeInteger remainingTime{};
    bool timerState{false};
};

struct KTIMER;

// ============================================================================
// 3. Clean-room NT Kernel Timer Object (KTIMER / TimerObject)
// ============================================================================
class TimerObject : public sync::DispatcherObject {
public:
    explicit TimerObject(TimerType type = TimerType::NotificationTimer)
        : type_(type), signaled_(false) {}

    [[nodiscard]] bool isSignaled(Handle /*threadId*/ = 0) const override {
        std::unique_lock<std::mutex> lock(mutex_);
        return signaled_;
    }

    bool satisfyWait(Handle /*threadId*/ = 0) override {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!signaled_) return false;
        if (type_ == TimerType::SynchronizationTimer) {
            signaled_ = false; // Auto-reset
        }
        return true;
    }

    bool wait(uint32_t timeoutMs = 0xFFFFFFFF) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (signaled_) {
            if (type_ == TimerType::SynchronizationTimer) {
                signaled_ = false;
            }
            return true;
        }
        if (timeoutMs == 0) return false;

        auto pred = [this]() { return signaled_; };
        if (timeoutMs == 0xFFFFFFFF) {
            cv_.wait(lock, pred);
        } else {
            if (!cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), pred)) {
                return false;
            }
        }
        if (type_ == TimerType::SynchronizationTimer) {
            signaled_ = false;
        }
        return true;
    }

    void setSignaled() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            signaled_ = true;
            if (type_ == TimerType::NotificationTimer) {
                cv_.notify_all();
            } else {
                cv_.notify_one();
            }
        }
        notifyListeners();
    }

    void reset() {
        std::unique_lock<std::mutex> lock(mutex_);
        signaled_ = false;
    }

    [[nodiscard]] TimerType getType() const noexcept { return type_; }
    void setType(TimerType type) noexcept { type_ = type; }

    [[nodiscard]] LargeInteger getDueTime() const noexcept { return dueTime_; }
    void setDueTime(LargeInteger dueTime) noexcept { dueTime_ = dueTime; }

    [[nodiscard]] uint32_t getPeriodMs() const noexcept { return periodMs_; }
    void setPeriodMs(uint32_t periodMs) noexcept { periodMs_ = periodMs; }

    [[nodiscard]] LargeInteger getTolerableDelay() const noexcept { return tolerableDelay_; }
    void setTolerableDelay(LargeInteger delay) noexcept { tolerableDelay_ = delay; }

    [[nodiscard]] bool isHighResolution() const noexcept { return highResolution_; }
    void setHighResolution(bool highRes) noexcept { highResolution_ = highRes; }

    [[nodiscard]] ke::KDPC* getDpc() const noexcept { return dpc_; }
    void setDpc(ke::KDPC* dpc) noexcept { dpc_ = dpc; }

    [[nodiscard]] bool isInserted() const noexcept { return inserted_; }
    void setInserted(bool inserted) noexcept { inserted_ = inserted; }

    [[nodiscard]] uint32_t getTableBucket() const noexcept { return tableBucket_; }
    void setTableBucket(uint32_t bucket) noexcept { tableBucket_ = bucket; }

    [[nodiscard]] std::chrono::steady_clock::time_point getDeadline() const noexcept { return deadline_; }
    void setDeadline(std::chrono::steady_clock::time_point deadline) noexcept { deadline_ = deadline; }

private:
    TimerType type_{TimerType::NotificationTimer};
    bool signaled_{false};
    bool inserted_{false};
    bool highResolution_{false};
    uint32_t tableBucket_{0};
    LargeInteger dueTime_{};
    uint32_t periodMs_{0};
    LargeInteger tolerableDelay_{};
    ke::KDPC* dpc_{nullptr};
    std::chrono::steady_clock::time_point deadline_{};
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

/**
 * @brief Standard NT Kernel Timer structure (KTIMER).
 */
struct KTIMER {
    TimerObject obj;

    explicit KTIMER(TimerType type = TimerType::NotificationTimer) : obj(type) {}
};

// ============================================================================
// 4. Modern Executive Timer (EX_TIMER) with Coalescing & High-Resolution
// ============================================================================
struct EX_TIMER;
using PEX_TIMER_CALLBACK = void (*)(struct EX_TIMER* timer, void* context);

struct EX_TIMER {
    KTIMER kernelTimer;
    PEX_TIMER_CALLBACK callback{nullptr};
    void* context{nullptr};
    uint32_t attributes{0};
    uint32_t periodMs{0};
    LargeInteger tolerableDelay{};
    ke::KDPC dpc{};
    std::atomic<bool> active{false};
};

// ============================================================================
// 5. Timer Wheel Table Bucket Structure
// ============================================================================
struct TimerWheelBucket {
    std::vector<KTIMER*> timers;
    std::chrono::steady_clock::time_point earliestDeadline{std::chrono::steady_clock::time_point::max()};
};

// ============================================================================
// 6. Sovereign Kernel Timer Manager Subsystem
// ============================================================================
class TimerManager {
public:
    static TimerManager& get() {
        static TimerManager instance;
        return instance;
    }

    bool setTimer(
        KTIMER* timer,
        LargeInteger dueTime,
        uint32_t periodMs = 0,
        ke::KDPC* dpc = nullptr,
        LargeInteger tolerableDelay = {},
        bool highResolution = false
    ) {
        if (!timer) return false;
        std::lock_guard<std::mutex> lock(mutex_);

        bool wasInserted = timer->obj.isInserted();
        if (wasInserted) {
            removeTimerFromTableLocked(timer);
        }

        timer->obj.reset();
        timer->obj.setDueTime(dueTime);
        timer->obj.setPeriodMs(periodMs);
        timer->obj.setDpc(dpc);
        timer->obj.setTolerableDelay(tolerableDelay);
        timer->obj.setHighResolution(highResolution);

        // Calculate absolute expiration time
        std::chrono::nanoseconds delayNs{0};
        if (dueTime.quadPart < 0) {
            delayNs = std::chrono::nanoseconds(-dueTime.quadPart * 100);
        } else if (dueTime.quadPart > 0) {
            delayNs = std::chrono::nanoseconds(dueTime.quadPart * 100);
        }

        auto deadline = std::chrono::steady_clock::now() + delayNs;
        timer->obj.setDeadline(deadline);
        timer->obj.setInserted(true);

        // Bucket into Timer Wheel or High-Resolution List
        if (highResolution) {
            highResTimers_.push_back(timer);
        } else {
            uint64_t targetNs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(deadline.time_since_epoch()).count()
            );
            uint32_t bucketIdx = static_cast<uint32_t>((targetNs / TIMER_STANDARD_TICK_NS) % TIMER_TABLE_SIZE);
            timer->obj.setTableBucket(bucketIdx);
            timerTable_[bucketIdx].timers.push_back(timer);
            if (deadline < timerTable_[bucketIdx].earliestDeadline) {
                timerTable_[bucketIdx].earliestDeadline = deadline;
            }
        }

        activeTimersList_.push_back(timer);
        totalTimersSet_++;
        return wasInserted;
    }

    bool cancelTimer(KTIMER* timer) {
        if (!timer) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!timer->obj.isInserted()) return false;

        removeTimerFromTableLocked(timer);
        timer->obj.setInserted(false);
        totalTimersCancelled_++;
        return true;
    }

    size_t processTimers() {
        std::vector<KTIMER*> expired;
        auto now = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(mutex_);

            // 1. Process High-Resolution Timers
            for (auto it = highResTimers_.begin(); it != highResTimers_.end(); ) {
                KTIMER* timer = *it;
                if (timer && timer->obj.isInserted() && now >= timer->obj.getDeadline()) {
                    expired.push_back(timer);
                    if (timer->obj.getPeriodMs() > 0) {
                        // Re-arm periodic timer with phase preservation
                        timer->obj.setDeadline(timer->obj.getDeadline() + std::chrono::milliseconds(timer->obj.getPeriodMs()));
                        ++it;
                    } else {
                        timer->obj.setInserted(false);
                        std::erase(activeTimersList_, timer);
                        it = highResTimers_.erase(it);
                    }
                } else {
                    ++it;
                }
            }

            // 2. Process Timer Wheel Table Buckets
            uint64_t currentNs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count()
            );
            uint32_t currentBucket = static_cast<uint32_t>((currentNs / TIMER_STANDARD_TICK_NS) % TIMER_TABLE_SIZE);
            (void)currentBucket;

            // Check current bucket and nearby active buckets
            for (uint32_t b = 0; b < TIMER_TABLE_SIZE; ++b) {
                auto& bucket = timerTable_[b];
                if (bucket.timers.empty()) continue;

                for (auto it = bucket.timers.begin(); it != bucket.timers.end(); ) {
                    KTIMER* timer = *it;
                    if (timer && timer->obj.isInserted() && now >= timer->obj.getDeadline()) {
                        expired.push_back(timer);
                        if (timer->obj.getPeriodMs() > 0) {
                            // Re-arm periodic timer
                            auto nextDeadline = timer->obj.getDeadline() + std::chrono::milliseconds(timer->obj.getPeriodMs());
                            timer->obj.setDeadline(nextDeadline);
                            uint64_t nextNs = static_cast<uint64_t>(
                                std::chrono::duration_cast<std::chrono::nanoseconds>(nextDeadline.time_since_epoch()).count()
                            );
                            uint32_t newBucket = static_cast<uint32_t>((nextNs / TIMER_STANDARD_TICK_NS) % TIMER_TABLE_SIZE);
                            if (newBucket != b) {
                                timer->obj.setTableBucket(newBucket);
                                timerTable_[newBucket].timers.push_back(timer);
                                it = bucket.timers.erase(it);
                            } else {
                                ++it;
                            }
                        } else {
                            timer->obj.setInserted(false);
                            std::erase(activeTimersList_, timer);
                            it = bucket.timers.erase(it);
                        }
                    } else {
                        ++it;
                    }
                }
            }

            totalTimersExpired_ += expired.size();
        }

        // 3. Dispatch Expiration Signals & Queue KDPCs at DISPATCH_LEVEL
        for (auto* timer : expired) {
            timer->obj.setSignaled();
            if (timer->obj.getDpc()) {
                ke::DpcQueue::get().queueDpc(
                    timer->obj.getDpc(),
                    reinterpret_cast<void*>(timer),
                    reinterpret_cast<void*>(static_cast<uintptr_t>(timer->obj.getPeriodMs()))
                );
                totalDpcsQueued_++;
            }
        }

        return expired.size();
    }

    [[nodiscard]] size_t getActiveTimerCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return activeTimersList_.size();
    }

    [[nodiscard]] uint64_t getTotalTimersSet() const noexcept {
        return totalTimersSet_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t getTotalTimersExpired() const noexcept {
        return totalTimersExpired_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t getTotalTimersCancelled() const noexcept {
        return totalTimersCancelled_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t getTotalDpcsQueued() const noexcept {
        return totalDpcsQueued_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] KDPC_WATCHDOG_INFORMATION getDpcWatchdogInfo() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return watchdogInfo_;
    }

    void updateDpcWatchdog(uint32_t elapsedMs) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        watchdogInfo_.dpcTimeCount = elapsedMs;
        watchdogInfo_.dpcWatchdogCount += elapsedMs;
    }

    void resetDpcWatchdog() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        watchdogInfo_.dpcTimeCount = 0;
        watchdogInfo_.dpcWatchdogCount = 0;
    }

    // ------------------------------------------------------------------------
    // Modern Executive Timer (EX_TIMER) Support
    // ------------------------------------------------------------------------
    EX_TIMER* allocateExTimer(PEX_TIMER_CALLBACK callback, void* context, uint32_t attributes) {
        auto* exTimer = new EX_TIMER();
        exTimer->callback = callback;
        exTimer->context = context;
        exTimer->attributes = attributes;
        exTimer->active = false;

        // Initialize internal KDPC to bridge to user callback
        exTimer->dpc.routine = [](ke::KDPC*, void* deferredContext, void*, void*) {
            auto* pTimer = static_cast<EX_TIMER*>(deferredContext);
            if (pTimer && pTimer->callback) {
                pTimer->callback(pTimer, pTimer->context);
            }
        };
        exTimer->dpc.deferredContext = exTimer;
        exTimer->dpc.importance = (attributes & EX_TIMER_HIGH_RESOLUTION) 
            ? ke::DpcImportance::High 
            : ke::DpcImportance::Medium;

        TimerType tType = (attributes & EX_TIMER_NOTIFICATION) 
            ? TimerType::NotificationTimer 
            : TimerType::SynchronizationTimer;
        exTimer->kernelTimer.obj.setType(tType);

        return exTimer;
    }

    void deleteExTimer(EX_TIMER* exTimer) {
        if (!exTimer) return;
        cancelExTimer(exTimer);
        delete exTimer;
    }

    bool setExTimer(
        EX_TIMER* exTimer,
        LargeInteger dueTime,
        uint32_t periodMs,
        LargeInteger tolerableDelay = {}
    ) {
        if (!exTimer) return false;
        exTimer->periodMs = periodMs;
        exTimer->tolerableDelay = tolerableDelay;
        exTimer->active = true;

        bool highRes = (exTimer->attributes & EX_TIMER_HIGH_RESOLUTION) != 0;
        return setTimer(&exTimer->kernelTimer, dueTime, periodMs, &exTimer->dpc, tolerableDelay, highRes);
    }

    bool cancelExTimer(EX_TIMER* exTimer) {
        if (!exTimer) return false;
        exTimer->active = false;
        return cancelTimer(&exTimer->kernelTimer);
    }

    void resetForTesting() {
        std::lock_guard<std::mutex> lock(mutex_);
        activeTimersList_.clear();
        highResTimers_.clear();
        for (auto& b : timerTable_) {
            b.timers.clear();
            b.earliestDeadline = std::chrono::steady_clock::time_point::max();
        }
        totalTimersSet_ = 0;
        totalTimersExpired_ = 0;
        totalTimersCancelled_ = 0;
        totalDpcsQueued_ = 0;
        watchdogInfo_.dpcTimeCount = 0;
        watchdogInfo_.dpcWatchdogCount = 0;
    }

private:
    TimerManager() = default;

    void removeTimerFromTableLocked(KTIMER* timer) {
        std::erase(activeTimersList_, timer);
        std::erase(highResTimers_, timer);
        uint32_t b = timer->obj.getTableBucket();
        if (b < TIMER_TABLE_SIZE) {
            std::erase(timerTable_[b].timers, timer);
        }
    }

    mutable std::mutex mutex_;
    std::vector<KTIMER*> activeTimersList_;
    std::vector<KTIMER*> highResTimers_;
    std::array<TimerWheelBucket, TIMER_TABLE_SIZE> timerTable_{};

    std::atomic<uint64_t> totalTimersSet_{0};
    std::atomic<uint64_t> totalTimersExpired_{0};
    std::atomic<uint64_t> totalTimersCancelled_{0};
    std::atomic<uint64_t> totalDpcsQueued_{0};
    KDPC_WATCHDOG_INFORMATION watchdogInfo_{};
};

// ============================================================================
// 7. Standard NT Kernel DDK Exports (evntrace/ntddk)
// ============================================================================
inline void KeInitializeTimer(KTIMER* timer) {
    if (timer) {
        timer->obj.setType(TimerType::NotificationTimer);
        timer->obj.reset();
        timer->obj.setInserted(false);
    }
}

inline void KeInitializeTimerEx(KTIMER* timer, TimerType type) {
    if (timer) {
        timer->obj.setType(type);
        timer->obj.reset();
        timer->obj.setInserted(false);
    }
}

inline bool KeSetTimer(KTIMER* timer, LargeInteger dueTime, ke::KDPC* dpc = nullptr) {
    return TimerManager::get().setTimer(timer, dueTime, 0, dpc);
}

inline bool KeSetTimerEx(
    KTIMER* timer,
    LargeInteger dueTime,
    uint32_t periodMs,
    ke::KDPC* dpc = nullptr
) {
    return TimerManager::get().setTimer(timer, dueTime, periodMs, dpc);
}

inline bool KeCancelTimer(KTIMER* timer) {
    return TimerManager::get().cancelTimer(timer);
}

inline bool KeReadStateTimer(const KTIMER* timer) {
    return timer ? timer->obj.isSignaled() : false;
}

inline void KeQueryDpcWatchdogInformation(KDPC_WATCHDOG_INFORMATION* watchdogInfo) {
    if (watchdogInfo) {
        *watchdogInfo = TimerManager::get().getDpcWatchdogInfo();
    }
}

// ============================================================================
// 8. Modern Executive Timer DDK Exports (ExTimer)
// ============================================================================
inline EX_TIMER* ExAllocateTimer(
    PEX_TIMER_CALLBACK callback,
    void* context,
    uint32_t attributes
) {
    return TimerManager::get().allocateExTimer(callback, context, attributes);
}

inline bool ExSetTimer(
    EX_TIMER* timer,
    LargeInteger dueTime,
    uint32_t periodMs,
    LargeInteger tolerableDelay = {}
) {
    return TimerManager::get().setExTimer(timer, dueTime, periodMs, tolerableDelay);
}

inline bool ExCancelTimer(EX_TIMER* timer) {
    return TimerManager::get().cancelExTimer(timer);
}

inline void ExDeleteTimer(EX_TIMER* timer) {
    TimerManager::get().deleteExTimer(timer);
}

} // namespace micant::timer
