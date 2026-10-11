#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <deque>
#include <thread>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ob.hpp"
#include "ke.hpp"

namespace micant::sync {

// ============================================================================
// 1. Dispatcher Object Types (DISPATCHER_TYPE)
// ============================================================================
enum class DISPATCHER_TYPE : uint8_t {
    NotificationEvent      = 0, // Manual-reset event (KEVENT)
    SynchronizationEvent   = 1, // Auto-reset event (KEVENT)
    Mutant                 = 2, // Reentrant mutex (KMUTANT)
    Process                = 3, // Process termination dispatcher
    Queue                  = 4, // Kernel Queue (KQUEUE)
    Semaphore              = 5, // Counting semaphore (KSEMAPHORE)
    Thread                 = 6, // Thread termination dispatcher
    Gate                   = 7, // Synchronization gate
    NotificationTimer      = 8, // Manual-reset timer (KTIMER)
    SynchronizationTimer   = 9  // Auto-reset timer (KTIMER)
};

// ============================================================================
// 2. Wait Types & Reasons (WAIT_TYPE & KWAIT_REASON)
// ============================================================================
enum class WAIT_TYPE : uint32_t {
    WaitAll          = 0, // Atomically wait for all objects to be signaled
    WaitAny          = 1, // Wait for any single object to be signaled
    WaitNotification = 2,
    WaitDequeue      = 3
};

enum class KWAIT_REASON : uint32_t {
    Executive        = 0,
    FreePage         = 1,
    PageIn           = 2,
    PoolAllocation   = 3,
    DelayExecution   = 4,
    Suspended        = 5,
    UserRequest      = 6,
    WrEventPair      = 7,
    WrQueue          = 8,
    WrLpcReceive     = 9,
    WrLpcReply       = 10,
    WrVirtualMemory  = 11,
    WrPageOut        = 12,
    WrRendezvous     = 13,
    WrKeyedEvent     = 14,
    WrTerminated     = 15,
    WrProcess        = 16,
    WrMutex          = 17,
    WrResource       = 18,
    WrPushLock       = 19,
    WrDispatchInt    = 20,
    WrPreempted      = 21,
    WrYieldExecution = 22,
    WrFastMutex      = 23,
    WrGuardedMutex   = 24,
    WrRundown        = 25,
    MaximumWaitReason= 26
};

// Canonical NT Wait & Boost Constants
inline constexpr uint32_t STATUS_WAIT_0           = 0x00000000;
inline constexpr uint32_t STATUS_WAIT_1           = 0x00000001;
inline constexpr uint32_t STATUS_WAIT_2           = 0x00000002;
inline constexpr uint32_t STATUS_WAIT_3           = 0x00000003;
inline constexpr uint32_t STATUS_TIMEOUT          = 0x00000102;
inline constexpr uint32_t STATUS_ALERTED          = 0x00000101;
inline constexpr uint32_t STATUS_USER_APC         = 0x000000C0;
inline constexpr uint32_t STATUS_ABANDONED_WAIT_0 = 0x00000080;
inline constexpr uint32_t STATUS_MUTANT_NOT_OWNED = 0xC0000046;

inline constexpr size_t   MAXIMUM_WAIT_OBJECTS    = 64;
inline constexpr size_t   THREAD_WAIT_OBJECTS     = 3;

inline constexpr uint32_t IO_NO_INCREMENT         = 0;
inline constexpr uint32_t EVENT_INCREMENT         = 1;
inline constexpr uint32_t SEMAPHORE_INCREMENT     = 1;
inline constexpr uint32_t MUTEX_INCREMENT         = 1;

// Forward declaration
struct KWAIT_BLOCK;

// ============================================================================
// 3. Dispatcher Header (DISPATCHER_HEADER)
// ============================================================================
struct DISPATCHER_HEADER {
    DISPATCHER_TYPE type{DISPATCHER_TYPE::NotificationEvent};
    uint8_t abandoned{0};
    uint8_t absolute{0};
    uint8_t size{0}; // In 32-bit DWORDs
    int32_t signalState{0};
    std::vector<KWAIT_BLOCK*> waitList; // Authentic WaitListHead
    mutable std::mutex lock;
    std::condition_variable cv;
};

// ============================================================================
// 4. Wait Block Architecture (KWAIT_BLOCK)
// ============================================================================
struct KWAIT_BLOCK {
    ke::KThread* thread{nullptr};
    DISPATCHER_HEADER* object{nullptr};
    KWAIT_BLOCK* nextWaitBlock{nullptr};
    uint16_t waitKey{0};
    WAIT_TYPE waitType{WAIT_TYPE::WaitAny};
    KWAIT_REASON waitReason{KWAIT_REASON::Executive};
    bool satisfied{false};
};

// ============================================================================
// 5. Canonical Kernel Synchronization Structures
// ============================================================================
enum class EventType : uint32_t {
    NotificationEvent = 0,    // Manual reset
    SynchronizationEvent = 1  // Auto reset
};

struct KEVENT {
    DISPATCHER_HEADER header;
};

struct KMUTANT {
    DISPATCHER_HEADER header;
    ke::KThread* ownerThread{nullptr};
    Handle ownerThreadId{0};
    bool abandoned{false};
    uint32_t recursionCount{0};
    uint32_t originalPriority{8};
    bool priorityInherited{false};
};
using KMUTEX = KMUTANT;

struct KSEMAPHORE {
    DISPATCHER_HEADER header;
    int32_t limit{1};
};

// ============================================================================
// 6. Dispatcher Telemetry & Diagnostics
// ============================================================================
struct DISPATCHER_TELEMETRY {
    size_t totalSingleWaits{0};
    size_t totalMultiWaitsAny{0};
    size_t totalMultiWaitsAll{0};
    size_t totalTimeouts{0};
    size_t totalEventsSet{0};
    size_t totalEventsPulsed{0};
    size_t totalMutantsAcquired{0};
    size_t totalMutantsReleased{0};
    size_t totalSemaphoresReleased{0};
    size_t totalPriorityInheritanceBoosts{0};
};

inline DISPATCHER_TELEMETRY g_DispatcherTelemetry{};
inline std::mutex g_TelemetryMutex{};

inline DISPATCHER_TELEMETRY KeQueryDispatcherTelemetry() {
    std::lock_guard<std::mutex> lk(g_TelemetryMutex);
    return g_DispatcherTelemetry;
}

inline void KeResetDispatcherTelemetryForTesting() {
    std::lock_guard<std::mutex> lk(g_TelemetryMutex);
    g_DispatcherTelemetry = DISPATCHER_TELEMETRY{};
}

// ============================================================================
// 7. Event Operations (Ke*Event)
// ============================================================================
inline void KeInitializeEvent(KEVENT* event, DISPATCHER_TYPE type, bool initialState) {
    if (!event) return;
    std::lock_guard<std::mutex> lk(event->header.lock);
    event->header.type = type;
    event->header.signalState = initialState ? 1 : 0;
    event->header.waitList.clear();
}

inline void KeInitializeEvent(KEVENT* event, EventType type, bool initialState) {
    KeInitializeEvent(event, (type == EventType::NotificationEvent) 
        ? DISPATCHER_TYPE::NotificationEvent 
        : DISPATCHER_TYPE::SynchronizationEvent, initialState);
}

inline int32_t KeSetEvent(KEVENT* event, uint32_t increment, bool /*wait*/) {
    if (!event) return 0;
    std::unique_lock<std::mutex> lk(event->header.lock);
    int32_t oldState = event->header.signalState;
    event->header.signalState = 1;

    {
        std::lock_guard<std::mutex> tlk(g_TelemetryMutex);
        g_DispatcherTelemetry.totalEventsSet++;
    }

    if (event->header.type == DISPATCHER_TYPE::NotificationEvent) {
        // Notification event satisfies ALL waiting threads
        for (auto* wb : event->header.waitList) {
            if (wb) {
                wb->satisfied = true;
                if (wb->thread && increment > 0 && wb->thread->priority < ke::PRIORITY_REALTIME_MIN) {
                    wb->thread->priority = std::min(wb->thread->priority + increment, ke::PRIORITY_REALTIME_MIN - 1);
                }
            }
        }
        event->header.cv.notify_all();
    } else {
        // Synchronization event satisfies ONE waiting thread and auto-resets
        if (!event->header.waitList.empty()) {
            auto* wb = event->header.waitList.front();
            if (wb) {
                wb->satisfied = true;
                if (wb->thread && increment > 0 && wb->thread->priority < ke::PRIORITY_REALTIME_MIN) {
                    wb->thread->priority = std::min(wb->thread->priority + increment, ke::PRIORITY_REALTIME_MIN - 1);
                }
            }
            event->header.signalState = 0; // Auto-reset
            event->header.cv.notify_one();
        } else {
            event->header.cv.notify_one();
        }
    }

    return oldState;
}

inline int32_t KeResetEvent(KEVENT* event) {
    if (!event) return 0;
    std::lock_guard<std::mutex> lk(event->header.lock);
    int32_t oldState = event->header.signalState;
    event->header.signalState = 0;
    return oldState;
}

inline int32_t KePulseEvent(KEVENT* event, uint32_t increment, bool /*wait*/) {
    if (!event) return 0;
    std::unique_lock<std::mutex> lk(event->header.lock);
    int32_t oldState = event->header.signalState;

    {
        std::lock_guard<std::mutex> tlk(g_TelemetryMutex);
        g_DispatcherTelemetry.totalEventsPulsed++;
    }

    // Atomically signal, wake waiters, and reset to 0
    if (event->header.type == DISPATCHER_TYPE::NotificationEvent) {
        for (auto* wb : event->header.waitList) {
            if (wb) wb->satisfied = true;
        }
        event->header.cv.notify_all();
    } else {
        if (!event->header.waitList.empty()) {
            auto* wb = event->header.waitList.front();
            if (wb) wb->satisfied = true;
            event->header.cv.notify_one();
        }
    }
    event->header.signalState = 0;
    return oldState;
}

// ============================================================================
// 8. Mutant / Mutex Operations & Priority Inheritance (Ke*Mutant)
// ============================================================================
inline void KeInitializeMutant(KMUTANT* mutant, bool initialOwner, ke::KThread* ownerThread = nullptr) {
    if (!mutant) return;
    std::lock_guard<std::mutex> lk(mutant->header.lock);
    mutant->header.type = DISPATCHER_TYPE::Mutant;
    mutant->abandoned = false;
    mutant->priorityInherited = false;
    mutant->header.waitList.clear();

    if (initialOwner) {
        mutant->header.signalState = 0; // Owned = unsignaled
        mutant->ownerThread = ownerThread;
        mutant->ownerThreadId = ownerThread ? ownerThread->tid : 1;
        mutant->recursionCount = 1;
        mutant->originalPriority = ownerThread ? ownerThread->priority : 8;
    } else {
        mutant->header.signalState = 1; // Unowned = signaled
        mutant->ownerThread = nullptr;
        mutant->ownerThreadId = 0;
        mutant->recursionCount = 0;
        mutant->originalPriority = 8;
    }
}

inline void KeInitializeMutex(KMUTEX* mutex, uint32_t /*level*/ = 0) {
    KeInitializeMutant(mutex, false);
}

inline int32_t KeReleaseMutant(KMUTANT* mutant, uint32_t increment, bool abandoned, bool /*wait*/) {
    if (!mutant) return 0;
    std::unique_lock<std::mutex> lk(mutant->header.lock);
    int32_t oldState = mutant->header.signalState;

    if (mutant->ownerThreadId == 0 && mutant->header.signalState == 1) {
        return oldState; // Not owned
    }

    mutant->recursionCount--;
    if (mutant->recursionCount == 0) {
        // Priority Inheritance Restoration: restore owner thread's original priority
        if (mutant->priorityInherited && mutant->ownerThread) {
            mutant->ownerThread->priority = mutant->originalPriority;
            mutant->priorityInherited = false;
        }

        mutant->abandoned = abandoned;
        mutant->ownerThread = nullptr;
        mutant->ownerThreadId = 0;
        mutant->header.signalState = 1; // Released

        {
            std::lock_guard<std::mutex> tlk(g_TelemetryMutex);
            g_DispatcherTelemetry.totalMutantsReleased++;
        }

        // Satisfy next waiting thread in FIFO waitList
        if (!mutant->header.waitList.empty()) {
            auto* wb = mutant->header.waitList.front();
            if (wb) {
                wb->satisfied = true;
                mutant->header.signalState = 0; // Re-acquired
                mutant->ownerThread = wb->thread;
                mutant->ownerThreadId = wb->thread ? wb->thread->tid : 1;
                mutant->recursionCount = 1;
                mutant->originalPriority = wb->thread ? wb->thread->priority : 8;
                if (wb->thread && increment > 0 && wb->thread->priority < ke::PRIORITY_REALTIME_MIN) {
                    wb->thread->priority = std::min(wb->thread->priority + increment, ke::PRIORITY_REALTIME_MIN - 1);
                }
                {
                    std::lock_guard<std::mutex> tlk(g_TelemetryMutex);
                    g_DispatcherTelemetry.totalMutantsAcquired++;
                }
            }
            mutant->header.cv.notify_one();
        } else {
            mutant->header.cv.notify_one();
        }
    }

    return oldState;
}

// ============================================================================
// 9. Semaphore Operations (Ke*Semaphore)
// ============================================================================
inline void KeInitializeSemaphore(KSEMAPHORE* semaphore, int32_t count, int32_t limit) {
    if (!semaphore) return;
    std::lock_guard<std::mutex> lk(semaphore->header.lock);
    semaphore->header.type = DISPATCHER_TYPE::Semaphore;
    semaphore->header.signalState = count;
    semaphore->limit = limit;
    semaphore->header.waitList.clear();
}

inline int32_t KeReleaseSemaphore(KSEMAPHORE* semaphore, uint32_t increment, int32_t adjustment, bool /*wait*/) {
    if (!semaphore || adjustment <= 0) return 0;
    std::unique_lock<std::mutex> lk(semaphore->header.lock);
    int32_t oldState = semaphore->header.signalState;

    if (semaphore->header.signalState + adjustment > semaphore->limit) {
        return oldState; // Limit exceeded
    }

    semaphore->header.signalState += adjustment;

    {
        std::lock_guard<std::mutex> tlk(g_TelemetryMutex);
        g_DispatcherTelemetry.totalSemaphoresReleased += adjustment;
    }

    // Wake waiters while semaphore count remains positive
    while (semaphore->header.signalState > 0 && !semaphore->header.waitList.empty()) {
        auto* wb = semaphore->header.waitList.front();
        if (wb) {
            wb->satisfied = true;
            semaphore->header.signalState--;
            if (wb->thread && increment > 0 && wb->thread->priority < ke::PRIORITY_REALTIME_MIN) {
                wb->thread->priority = std::min(wb->thread->priority + increment, ke::PRIORITY_REALTIME_MIN - 1);
            }
        }
        semaphore->header.cv.notify_one();
    }

    return oldState;
}

// ============================================================================
// 10. Sovereign Wait Engine (KeWaitForSingleObject & KeWaitForMultipleObjects)
// ============================================================================

/**
 * @brief Canonical Wait for Single Object (KeWaitForSingleObject).
 */
inline NtStatus KeWaitForSingleObject(
    void* object,
    KWAIT_REASON waitReason,
    uint8_t /*waitMode*/,
    bool /*alertable*/,
    const int64_t* timeout = nullptr,
    ke::KThread* currentThread = nullptr
) {
    if (!object) return NtStatus::InvalidParameter;
    auto* hdr = reinterpret_cast<DISPATCHER_HEADER*>(object);
    std::unique_lock<std::mutex> lk(hdr->lock);

    {
        std::lock_guard<std::mutex> tlk(g_TelemetryMutex);
        g_DispatcherTelemetry.totalSingleWaits++;
    }

    Handle callerTid = currentThread ? currentThread->tid : 1;

    // 1. Check if object satisfies wait immediately without blocking
    if (hdr->type == DISPATCHER_TYPE::NotificationEvent) {
        if (hdr->signalState > 0) return NtStatus::Success; // Remains signaled
    } else if (hdr->type == DISPATCHER_TYPE::SynchronizationEvent) {
        if (hdr->signalState > 0) {
            hdr->signalState = 0; // Auto-reset
            return NtStatus::Success;
        }
    } else if (hdr->type == DISPATCHER_TYPE::Semaphore) {
        if (hdr->signalState > 0) {
            hdr->signalState--;
            return NtStatus::Success;
        }
    } else if (hdr->type == DISPATCHER_TYPE::Mutant) {
        auto* mut = reinterpret_cast<KMUTANT*>(object);
        if (mut->header.signalState == 1) {
            // Acquire unowned mutant
            mut->header.signalState = 0;
            mut->ownerThread = currentThread;
            mut->ownerThreadId = callerTid;
            mut->recursionCount = 1;
            mut->originalPriority = currentThread ? currentThread->priority : 8;
            {
                std::lock_guard<std::mutex> tlk(g_TelemetryMutex);
                g_DispatcherTelemetry.totalMutantsAcquired++;
            }
            return NtStatus::Success;
        } else if (mut->ownerThreadId == callerTid) {
            // Recursive acquisition by owner
            mut->recursionCount++;
            return NtStatus::Success;
        }
    }

    // 2. Immediate zero-timeout check (poll)
    if (timeout && *timeout == 0) {
        std::lock_guard<std::mutex> tlk(g_TelemetryMutex);
        g_DispatcherTelemetry.totalTimeouts++;
        return NtStatus::Timeout;
    }

    // 3. Priority Inversion Check & Priority Inheritance Boost
    if (hdr->type == DISPATCHER_TYPE::Mutant) {
        auto* mut = reinterpret_cast<KMUTANT*>(object);
        if (mut->ownerThread && currentThread) {
            if (currentThread->priority > mut->ownerThread->priority) {
                // Boost owner thread priority to match waiting thread
                if (!mut->priorityInherited) {
                    mut->originalPriority = mut->ownerThread->priority;
                    mut->priorityInherited = true;
                }
                mut->ownerThread->priority = currentThread->priority;
                {
                    std::lock_guard<std::mutex> tlk(g_TelemetryMutex);
                    g_DispatcherTelemetry.totalPriorityInheritanceBoosts++;
                }
            }
        }
    }

    // 4. Register KWAIT_BLOCK in object's waitList
    KWAIT_BLOCK wb{
        .thread = currentThread,
        .object = hdr,
        .nextWaitBlock = nullptr,
        .waitKey = 0,
        .waitType = WAIT_TYPE::WaitAny,
        .waitReason = waitReason,
        .satisfied = false
    };
    hdr->waitList.push_back(&wb);

    if (currentThread) {
        currentThread->state = ke::ThreadState::Waiting;
    }

    // 5. Block on condition variable
    bool satisfied = false;
    if (!timeout || *timeout < 0) {
        hdr->cv.wait(lk, [&]() { return wb.satisfied; });
        satisfied = true;
    } else {
        auto dur = std::chrono::milliseconds(*timeout);
        satisfied = hdr->cv.wait_for(lk, dur, [&]() { return wb.satisfied; });
    }

    // 6. Unlink KWAIT_BLOCK from waitList
    std::erase(hdr->waitList, &wb);

    if (currentThread) {
        currentThread->state = ke::ThreadState::Ready;
    }

    if (satisfied) {
        return NtStatus::Success;
    } else {
        std::lock_guard<std::mutex> tlk(g_TelemetryMutex);
        g_DispatcherTelemetry.totalTimeouts++;
        return NtStatus::Timeout;
    }
}

/**
 * @brief Canonical Wait for Multiple Objects (KeWaitForMultipleObjects).
 */
inline NtStatus KeWaitForMultipleObjects(
    uint32_t count,
    void* objects[],
    WAIT_TYPE waitType,
    KWAIT_REASON waitReason,
    uint8_t /*waitMode*/,
    bool /*alertable*/,
    const int64_t* timeout = nullptr,
    KWAIT_BLOCK* waitBlockArray = nullptr,
    ke::KThread* currentThread = nullptr
) {
    if (count == 0 || count > MAXIMUM_WAIT_OBJECTS || !objects) {
        return NtStatus::InvalidParameter;
    }

    {
        std::lock_guard<std::mutex> tlk(g_TelemetryMutex);
        if (waitType == WAIT_TYPE::WaitAll) g_DispatcherTelemetry.totalMultiWaitsAll++;
        else g_DispatcherTelemetry.totalMultiWaitsAny++;
    }

    // Fast check: WaitAny immediate satisfaction
    if (waitType == WAIT_TYPE::WaitAny) {
        for (uint32_t i = 0; i < count; ++i) {
            auto* hdr = reinterpret_cast<DISPATCHER_HEADER*>(objects[i]);
            std::lock_guard<std::mutex> lk(hdr->lock);
            if (hdr->signalState > 0) {
                if (hdr->type == DISPATCHER_TYPE::SynchronizationEvent ||
                    hdr->type == DISPATCHER_TYPE::Semaphore) {
                    hdr->signalState--;
                }
                return static_cast<NtStatus>(STATUS_WAIT_0 + i);
            }
        }
    }

    // Fast check: WaitAll immediate satisfaction
    if (waitType == WAIT_TYPE::WaitAll) {
        bool allSignaled = true;
        for (uint32_t i = 0; i < count; ++i) {
            auto* hdr = reinterpret_cast<DISPATCHER_HEADER*>(objects[i]);
            std::lock_guard<std::mutex> lk(hdr->lock);
            if (hdr->signalState <= 0) {
                allSignaled = false;
                break;
            }
        }
        if (allSignaled) {
            for (uint32_t i = 0; i < count; ++i) {
                auto* hdr = reinterpret_cast<DISPATCHER_HEADER*>(objects[i]);
                std::lock_guard<std::mutex> lk(hdr->lock);
                if (hdr->type == DISPATCHER_TYPE::SynchronizationEvent ||
                    hdr->type == DISPATCHER_TYPE::Semaphore) {
                    hdr->signalState--;
                }
            }
            return static_cast<NtStatus>(STATUS_WAIT_0);
        }
    }

    // If zero timeout specified, return timeout immediately
    if (timeout && *timeout == 0) {
        std::lock_guard<std::mutex> tlk(g_TelemetryMutex);
        g_DispatcherTelemetry.totalTimeouts++;
        return NtStatus::Timeout;
    }

    // Multi-wait blocking coordination structure
    struct MultiWaitState {
        std::mutex mtx;
        std::condition_variable cv;
        bool satisfied{false};
        uint32_t satisfiedIndex{0};
    };
    auto state = std::make_shared<MultiWaitState>();

    std::vector<KWAIT_BLOCK> localBlocks;
    KWAIT_BLOCK* blocks = waitBlockArray;
    if (!blocks) {
        localBlocks.resize(count);
        blocks = localBlocks.data();
    }

    for (uint32_t i = 0; i < count; ++i) {
        auto* hdr = reinterpret_cast<DISPATCHER_HEADER*>(objects[i]);
        blocks[i].thread = currentThread;
        blocks[i].object = hdr;
        blocks[i].waitKey = static_cast<uint16_t>(i);
        blocks[i].waitType = waitType;
        blocks[i].waitReason = waitReason;
        blocks[i].satisfied = false;
        blocks[i].nextWaitBlock = (i + 1 < count) ? &blocks[i + 1] : nullptr;

        std::lock_guard<std::mutex> lk(hdr->lock);
        hdr->waitList.push_back(&blocks[i]);
    }

    // Wait loop
    uint32_t resultStatus = STATUS_WAIT_0;
    bool waitDone = false;
    auto startTime = std::chrono::steady_clock::now();

    while (!waitDone) {
        if (waitType == WAIT_TYPE::WaitAny) {
            for (uint32_t i = 0; i < count; ++i) {
                auto* hdr = reinterpret_cast<DISPATCHER_HEADER*>(objects[i]);
                std::lock_guard<std::mutex> lk(hdr->lock);
                if (hdr->signalState > 0 || blocks[i].satisfied) {
                    if (hdr->type == DISPATCHER_TYPE::SynchronizationEvent ||
                        hdr->type == DISPATCHER_TYPE::Semaphore) {
                        if (hdr->signalState > 0) hdr->signalState--;
                    }
                    resultStatus = STATUS_WAIT_0 + i;
                    waitDone = true;
                    break;
                }
            }
        } else {
            // WaitAll
            bool allSig = true;
            for (uint32_t i = 0; i < count; ++i) {
                auto* hdr = reinterpret_cast<DISPATCHER_HEADER*>(objects[i]);
                std::lock_guard<std::mutex> lk(hdr->lock);
                if (hdr->signalState <= 0 && !blocks[i].satisfied) {
                    allSig = false;
                    break;
                }
            }
            if (allSig) {
                for (uint32_t i = 0; i < count; ++i) {
                    auto* hdr = reinterpret_cast<DISPATCHER_HEADER*>(objects[i]);
                    std::lock_guard<std::mutex> lk(hdr->lock);
                    if (hdr->type == DISPATCHER_TYPE::SynchronizationEvent ||
                        hdr->type == DISPATCHER_TYPE::Semaphore) {
                        if (hdr->signalState > 0) hdr->signalState--;
                    }
                }
                resultStatus = STATUS_WAIT_0;
                waitDone = true;
                break;
            }
        }

        if (waitDone) break;

        if (timeout && *timeout > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            if (elapsed >= *timeout) {
                resultStatus = STATUS_TIMEOUT;
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Clean up waitList references
    for (uint32_t i = 0; i < count; ++i) {
        auto* hdr = reinterpret_cast<DISPATCHER_HEADER*>(objects[i]);
        std::lock_guard<std::mutex> lk(hdr->lock);
        std::erase(hdr->waitList, &blocks[i]);
    }

    if (resultStatus == STATUS_TIMEOUT) {
        std::lock_guard<std::mutex> tlk(g_TelemetryMutex);
        g_DispatcherTelemetry.totalTimeouts++;
        return NtStatus::Timeout;
    }

    return static_cast<NtStatus>(resultStatus);
}

// ============================================================================
// 11. Backward-Compatible C++ Synchronization Classes
// ============================================================================

class DispatcherObject {
public:
    virtual ~DispatcherObject() = default;
    [[nodiscard]] virtual bool isSignaled(Handle threadId = 0) const = 0;
    virtual bool satisfyWait(Handle threadId = 0) = 0;

    void addWaitListener(const std::shared_ptr<std::condition_variable>& cv, const std::shared_ptr<std::mutex>& cvMutex) {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listeners_.push_back({cv, cvMutex});
    }

    void removeWaitListener(const std::shared_ptr<std::condition_variable>& cv) {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        std::erase_if(listeners_, [&](const auto& pair) {
            return pair.first.lock() == cv;
        });
    }

protected:
    void notifyListeners() {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        for (auto& [weakCv, weakMutex] : listeners_) {
            auto cv = weakCv.lock();
            auto mtx = weakMutex.lock();
            if (cv && mtx) {
                std::lock_guard<std::mutex> lk(*mtx);
                cv->notify_all();
            }
        }
    }

private:
    mutable std::mutex listenerMutex_;
    std::vector<std::pair<std::weak_ptr<std::condition_variable>, std::weak_ptr<std::mutex>>> listeners_;
};

class EventObject : public DispatcherObject {
public:
    EventObject(EventType type, bool initialState) {
        KeInitializeEvent(&event_, type, initialState);
    }

    void set() {
        KeSetEvent(&event_, EVENT_INCREMENT, false);
        notifyListeners();
    }

    void reset() {
        KeResetEvent(&event_);
    }

    void pulse() {
        KePulseEvent(&event_, EVENT_INCREMENT, false);
        notifyListeners();
    }

    [[nodiscard]] bool isSignaled(Handle /*threadId*/ = 0) const override {
        std::lock_guard<std::mutex> lk(event_.header.lock);
        return event_.header.signalState > 0;
    }

    bool satisfyWait(Handle /*threadId*/ = 0) override {
        std::lock_guard<std::mutex> lk(event_.header.lock);
        if (event_.header.signalState <= 0) return false;
        if (event_.header.type == DISPATCHER_TYPE::SynchronizationEvent) {
            event_.header.signalState = 0;
        }
        return true;
    }

    bool wait(uint32_t timeoutMs = 0xFFFFFFFF) {
        int64_t to = static_cast<int64_t>(timeoutMs);
        NtStatus st = KeWaitForSingleObject(&event_, KWAIT_REASON::Executive, 0, false, &to);
        return st == NtStatus::Success;
    }

    [[nodiscard]] EventType getType() const noexcept {
        return (event_.header.type == DISPATCHER_TYPE::NotificationEvent) 
            ? EventType::NotificationEvent 
            : EventType::SynchronizationEvent;
    }

    KEVENT* getKernelEvent() noexcept { return &event_; }

private:
    KEVENT event_{};
};

class MutantObject : public DispatcherObject {
public:
    explicit MutantObject(bool initialOwner) {
        KeInitializeMutant(&mutant_, initialOwner);
    }

    [[nodiscard]] bool isSignaled(Handle threadId = 0) const override {
        std::lock_guard<std::mutex> lk(mutant_.header.lock);
        return mutant_.ownerThreadId == 0 || (threadId != 0 && mutant_.ownerThreadId == threadId);
    }

    bool satisfyWait(Handle threadId = 0) override {
        std::lock_guard<std::mutex> lk(mutant_.header.lock);
        Handle tid = (threadId != 0) ? threadId : 1;
        if (mutant_.ownerThreadId == 0) {
            mutant_.ownerThreadId = tid;
            mutant_.recursionCount = 1;
            mutant_.header.signalState = 0;
            return true;
        }
        if (mutant_.ownerThreadId == tid) {
            mutant_.recursionCount++;
            return true;
        }
        return false;
    }

    bool acquire(Handle threadId, uint32_t timeoutMs = 0xFFFFFFFF) {
        int64_t to = static_cast<int64_t>(timeoutMs);
        ke::KThread dummyThread{};
        dummyThread.tid = threadId;
        dummyThread.priority = 8;
        NtStatus st = KeWaitForSingleObject(&mutant_, KWAIT_REASON::WrMutex, 0, false, &to, &dummyThread);
        return st == NtStatus::Success;
    }

    bool release(Handle threadId) {
        std::unique_lock<std::mutex> lk(mutant_.header.lock);
        if (mutant_.ownerThreadId != threadId && mutant_.ownerThreadId != 0) {
            return false;
        }
        lk.unlock();
        KeReleaseMutant(&mutant_, MUTEX_INCREMENT, false, false);
        notifyListeners();
        return true;
    }

    [[nodiscard]] Handle getOwner() const noexcept {
        std::lock_guard<std::mutex> lk(mutant_.header.lock);
        return mutant_.ownerThreadId;
    }

    [[nodiscard]] uint32_t getRecursionCount() const noexcept {
        std::lock_guard<std::mutex> lk(mutant_.header.lock);
        return mutant_.recursionCount;
    }

    KMUTANT* getKernelMutant() noexcept { return &mutant_; }

private:
    KMUTANT mutant_{};
};

class SemaphoreObject : public DispatcherObject {
public:
    SemaphoreObject(int32_t initialCount, int32_t maximumCount) {
        KeInitializeSemaphore(&sem_, initialCount, maximumCount);
    }

    [[nodiscard]] bool isSignaled(Handle /*threadId*/ = 0) const override {
        std::lock_guard<std::mutex> lk(sem_.header.lock);
        return sem_.header.signalState > 0;
    }

    bool satisfyWait(Handle /*threadId*/ = 0) override {
        std::lock_guard<std::mutex> lk(sem_.header.lock);
        if (sem_.header.signalState > 0) {
            sem_.header.signalState--;
            return true;
        }
        return false;
    }

    bool release(int32_t releaseCount, int32_t* previousCount = nullptr) {
        std::unique_lock<std::mutex> lk(sem_.header.lock);
        if (previousCount) *previousCount = sem_.header.signalState;
        if (sem_.header.signalState + releaseCount > sem_.limit) return false;
        lk.unlock();

        KeReleaseSemaphore(&sem_, SEMAPHORE_INCREMENT, releaseCount, false);
        notifyListeners();
        return true;
    }

    bool wait(uint32_t timeoutMs = 0xFFFFFFFF) {
        int64_t to = static_cast<int64_t>(timeoutMs);
        NtStatus st = KeWaitForSingleObject(&sem_, KWAIT_REASON::Executive, 0, false, &to);
        return st == NtStatus::Success;
    }

    [[nodiscard]] int32_t getCount() const noexcept {
        std::lock_guard<std::mutex> lk(sem_.header.lock);
        return sem_.header.signalState;
    }

    KSEMAPHORE* getKernelSemaphore() noexcept { return &sem_; }

private:
    KSEMAPHORE sem_{};
};

class DispatcherRegistry {
public:
    static DispatcherRegistry& get() {
        static DispatcherRegistry instance;
        return instance;
    }

    Handle registerObject(std::shared_ptr<DispatcherObject> obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        Handle h = nextHandle_;
        nextHandle_ += 4;
        objects_[h] = std::move(obj);
        return h;
    }

    void registerWithHandle(Handle h, std::shared_ptr<DispatcherObject> obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        objects_[h] = std::move(obj);
    }

    std::shared_ptr<DispatcherObject> lookup(Handle h) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = objects_.find(h);
        return (it != objects_.end()) ? it->second : nullptr;
    }

    template <typename T>
    std::shared_ptr<T> lookupAs(Handle h) const {
        auto base = lookup(h);
        return std::dynamic_pointer_cast<T>(base);
    }

    void unregister(Handle h) {
        std::lock_guard<std::mutex> lock(mutex_);
        objects_.erase(h);
    }

    [[nodiscard]] size_t getObjectCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return objects_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        objects_.clear();
        nextHandle_ = 0x100;
    }

private:
    DispatcherRegistry() = default;
    mutable std::mutex mutex_;
    Handle nextHandle_{0x100};
    std::unordered_map<Handle, std::shared_ptr<DispatcherObject>> objects_;
};

} // namespace micant::sync

namespace micant::ke {
    using sync::DISPATCHER_HEADER;
    using sync::DISPATCHER_TYPE;
    using sync::KWAIT_BLOCK;
    using sync::KWAIT_REASON;
    using sync::WAIT_TYPE;
    using sync::KEVENT;
    using sync::KMUTANT;
    using sync::KMUTEX;
    using sync::KSEMAPHORE;
    using sync::KeInitializeEvent;
    using sync::KeSetEvent;
    using sync::KeResetEvent;
    using sync::KePulseEvent;
    using sync::KeInitializeMutant;
    using sync::KeInitializeMutex;
    using sync::KeReleaseMutant;
    using sync::KeInitializeSemaphore;
    using sync::KeReleaseSemaphore;
    using sync::KeWaitForSingleObject;
    using sync::KeWaitForMultipleObjects;
    using sync::KeQueryDispatcherTelemetry;
    using sync::KeResetDispatcherTelemetryForTesting;
} // namespace micant::ke
