#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ob.hpp"

namespace micant::sync {

enum class EventType : uint32_t {
    NotificationEvent = 0,    // Manual reset
    SynchronizationEvent = 1  // Auto reset
};

/**
 * @brief NT Event Object (KEVENT).
 */
class EventObject {
public:
    EventObject(EventType type, bool initialState)
        : type_(type), signaled_(initialState) {}

    void set() {
        std::unique_lock<std::mutex> lock(mutex_);
        signaled_ = true;
        if (type_ == EventType::NotificationEvent) {
            cv_.notify_all();
        } else {
            cv_.notify_one();
        }
    }

    void reset() {
        std::unique_lock<std::mutex> lock(mutex_);
        signaled_ = false;
    }

    bool wait(uint32_t timeoutMs = 0xFFFFFFFF) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (signaled_) {
            if (type_ == EventType::SynchronizationEvent) {
                signaled_ = false; // Auto-reset
            }
            return true;
        }

        if (timeoutMs == 0) {
            return false;
        }

        if (timeoutMs == 0xFFFFFFFF) {
            cv_.wait(lock, [this]() { return signaled_; });
        } else {
            if (!cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() { return signaled_; })) {
                return false; // Timeout
            }
        }

        if (type_ == EventType::SynchronizationEvent) {
            signaled_ = false; // Auto-reset
        }
        return true;
    }

    [[nodiscard]] bool isSignaled() const noexcept {
        return signaled_;
    }

    [[nodiscard]] EventType getType() const noexcept {
        return type_;
    }

private:
    EventType type_;
    bool signaled_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
};

/**
 * @brief NT Mutant / Mutex Object (KMUTANT).
 * Supports recursive acquisition by owning thread.
 */
class MutantObject {
public:
    explicit MutantObject(bool initialOwner) : ownerThreadId_(initialOwner ? 1 : 0), recursionCount_(initialOwner ? 1 : 0) {}

    bool acquire(Handle threadId, uint32_t timeoutMs = 0xFFFFFFFF) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (ownerThreadId_ == threadId) {
            recursionCount_++;
            return true;
        }

        auto pred = [this]() { return ownerThreadId_ == 0; };
        if (timeoutMs == 0xFFFFFFFF) {
            cv_.wait(lock, pred);
        } else if (timeoutMs > 0) {
            if (!cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), pred)) {
                return false;
            }
        } else {
            if (ownerThreadId_ != 0) return false;
        }

        ownerThreadId_ = threadId;
        recursionCount_ = 1;
        return true;
    }

    bool release(Handle threadId) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (ownerThreadId_ != threadId) {
            return false; // Mutex not owned by caller
        }

        recursionCount_--;
        if (recursionCount_ == 0) {
            ownerThreadId_ = 0;
            cv_.notify_one();
        }
        return true;
    }

    [[nodiscard]] Handle getOwner() const noexcept { return ownerThreadId_; }
    [[nodiscard]] uint32_t getRecursionCount() const noexcept { return recursionCount_; }

private:
    Handle ownerThreadId_{0};
    uint32_t recursionCount_{0};
    std::mutex mutex_;
    std::condition_variable cv_;
};

/**
 * @brief NT Semaphore Object (KSEMAPHORE).
 */
class SemaphoreObject {
public:
    SemaphoreObject(int32_t initialCount, int32_t maximumCount)
        : currentCount_(initialCount), maximumCount_(maximumCount) {}

    bool release(int32_t releaseCount, int32_t* previousCount = nullptr) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (previousCount) *previousCount = currentCount_;
        if (currentCount_ + releaseCount > maximumCount_) {
            return false;
        }
        currentCount_ += releaseCount;
        for (int32_t i = 0; i < releaseCount; ++i) {
            cv_.notify_one();
        }
        return true;
    }

    bool wait(uint32_t timeoutMs = 0xFFFFFFFF) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto pred = [this]() { return currentCount_ > 0; };
        if (timeoutMs == 0xFFFFFFFF) {
            cv_.wait(lock, pred);
        } else {
            if (!cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), pred)) {
                return false;
            }
        }
        currentCount_--;
        return true;
    }

    [[nodiscard]] int32_t getCount() const noexcept { return currentCount_; }

private:
    int32_t currentCount_{0};
    int32_t maximumCount_{1};
    std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace micant::sync
