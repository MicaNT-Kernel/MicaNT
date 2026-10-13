#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/core/sync.hpp"
#include "micant/km/core/timer.hpp"
#include "micant/km/hal/hal.hpp"
#include <vector>
#include <memory>
#include <thread>
#include <chrono>

namespace micant::sys {

NtStatus NtWaitForMultipleObjects(
    uint32_t count,
    const Handle* handles,
    WaitType waitType,
    bool /*alertable*/,
    LargeInteger* timeout
) {
    if (count == 0 || count > MAXIMUM_WAIT_OBJECTS) {
        return NtStatus::InvalidParameter1;
    }
    if (!handles) {
        return NtStatus::AccessViolation;
    }

    std::vector<std::shared_ptr<sync::DispatcherObject>> objects;
    objects.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        auto obj = sync::DispatcherRegistry::get().lookup(handles[i]);
        if (!obj) {
            return NtStatus::InvalidHandle;
        }
        objects.push_back(obj);
    }

    uint32_t timeoutMs = 0xFFFFFFFF;
    if (timeout) {
        if (timeout->quadPart == 0) {
            timeoutMs = 0;
        } else if (timeout->quadPart < 0) {
            timeoutMs = static_cast<uint32_t>(-timeout->quadPart / 10000);
        } else {
            timeoutMs = static_cast<uint32_t>(timeout->quadPart / 10000);
        }
    }

    if (waitType == WaitType::WaitAny) {
        for (uint32_t i = 0; i < count; ++i) {
            if (objects[i]->isSignaled()) {
                objects[i]->satisfyWait();
                return STATUS_WAIT_N(i);
            }
        }

        if (timeoutMs == 0) {
            return NtStatus::Timeout;
        }

        auto cv = std::make_shared<std::condition_variable>();
        auto cvMutex = std::make_shared<std::mutex>();

        for (auto& obj : objects) {
            obj->addWaitListener(cv, cvMutex);
        }

        auto start = std::chrono::steady_clock::now();
        NtStatus result = NtStatus::Timeout;

        while (true) {
            std::unique_lock<std::mutex> lk(*cvMutex);
            for (uint32_t i = 0; i < count; ++i) {
                if (objects[i]->isSignaled()) {
                    objects[i]->satisfyWait();
                    result = STATUS_WAIT_N(i);
                    break;
                }
            }
            if (result != NtStatus::Timeout) break;

            if (timeoutMs == 0xFFFFFFFF) {
                cv->wait(lk);
            } else {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                if (elapsed >= timeoutMs) {
                    result = NtStatus::Timeout;
                    break;
                }
                cv->wait_for(lk, std::chrono::milliseconds(timeoutMs - elapsed));
            }

            for (uint32_t i = 0; i < count; ++i) {
                if (objects[i]->isSignaled()) {
                    objects[i]->satisfyWait();
                    result = STATUS_WAIT_N(i);
                    break;
                }
            }
            if (result != NtStatus::Timeout) break;

            if (timeoutMs != 0xFFFFFFFF) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                if (elapsed >= timeoutMs) {
                    result = NtStatus::Timeout;
                    break;
                }
            }
        }

        for (auto& obj : objects) {
            obj->removeWaitListener(cv);
        }
        return result;

    } else { // WaitAll
        bool allSignaled = true;
        for (uint32_t i = 0; i < count; ++i) {
            if (!objects[i]->isSignaled()) {
                allSignaled = false;
                break;
            }
        }
        if (allSignaled) {
            for (uint32_t i = 0; i < count; ++i) {
                objects[i]->satisfyWait();
            }
            return NtStatus::Success;
        }

        if (timeoutMs == 0) {
            return NtStatus::Timeout;
        }

        auto cv = std::make_shared<std::condition_variable>();
        auto cvMutex = std::make_shared<std::mutex>();

        for (auto& obj : objects) {
            obj->addWaitListener(cv, cvMutex);
        }

        auto start = std::chrono::steady_clock::now();
        NtStatus result = NtStatus::Timeout;

        while (true) {
            std::unique_lock<std::mutex> lk(*cvMutex);
            allSignaled = true;
            for (uint32_t i = 0; i < count; ++i) {
                if (!objects[i]->isSignaled()) {
                    allSignaled = false;
                    break;
                }
            }
            if (allSignaled) {
                for (uint32_t i = 0; i < count; ++i) {
                    objects[i]->satisfyWait();
                }
                result = NtStatus::Success;
                break;
            }

            if (timeoutMs == 0xFFFFFFFF) {
                cv->wait(lk);
            } else {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                if (elapsed >= timeoutMs) {
                    result = NtStatus::Timeout;
                    break;
                }
                cv->wait_for(lk, std::chrono::milliseconds(timeoutMs - elapsed));
            }

            allSignaled = true;
            for (uint32_t i = 0; i < count; ++i) {
                if (!objects[i]->isSignaled()) {
                    allSignaled = false;
                    break;
                }
            }
            if (allSignaled) {
                for (uint32_t i = 0; i < count; ++i) {
                    objects[i]->satisfyWait();
                }
                result = NtStatus::Success;
                break;
            }

            if (timeoutMs != 0xFFFFFFFF) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                if (elapsed >= timeoutMs) {
                    result = NtStatus::Timeout;
                    break;
                }
            }
        }

        for (auto& obj : objects) {
            obj->removeWaitListener(cv);
        }
        return result;
    }
}

NtStatus NtWaitForSingleObject(Handle handle, bool alertable, LargeInteger* timeout) {
    if (handle == 0 || handle == InvalidHandleValue) {
        return NtStatus::InvalidHandle;
    }
    return NtWaitForMultipleObjects(1, &handle, WaitType::WaitAny, alertable, timeout);
}

NtStatus NtDelayExecution(bool /*alertable*/, const LargeInteger* interval) {
    if (!interval) {
        return NtStatus::AccessViolation;
    }

    if (interval->quadPart == 0) {
        std::this_thread::yield();
        return NtStatus::Success;
    }

    if (interval->quadPart < 0) {
        int64_t nanoseconds = -interval->quadPart * 100;
        std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
        return NtStatus::Success;
    }

    int64_t nanoseconds = interval->quadPart * 100;
    std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
    return NtStatus::Success;
}

NtStatus NtCreateTimer(
    Handle* timerHandle,
    uint32_t /*desiredAccess*/,
    ObjectAttributes* /*objectAttributes*/,
    uint32_t timerType
) {
    if (!timerHandle) return NtStatus::InvalidParameter;

    timer::TimerType type = (timerType == 0) 
        ? timer::TimerType::NotificationTimer 
        : timer::TimerType::SynchronizationTimer;

    auto timerObj = std::make_shared<timer::TimerObject>(type);
    Handle h = sync::DispatcherRegistry::get().registerObject(timerObj);
    *timerHandle = h;
    return NtStatus::Success;
}

NtStatus NtSetTimer(
    Handle timerHandle,
    LargeInteger* dueTime,
    void* /*timerApcRoutine*/,
    void* /*timerContext*/,
    bool /*resumeTimer*/,
    uint32_t period,
    bool* previousState
) {
    if (!dueTime) return NtStatus::InvalidParameter;

    auto timerObj = sync::DispatcherRegistry::get().lookupAs<timer::TimerObject>(timerHandle);
    if (!timerObj) return NtStatus::InvalidHandle;

    if (previousState) {
        *previousState = timerObj->isSignaled();
    }

    timerObj->reset();
    timerObj->setDueTime(*dueTime);
    timerObj->setPeriodMs(period);

    std::chrono::nanoseconds delayNs{0};
    if (dueTime->quadPart < 0) {
        delayNs = std::chrono::nanoseconds(-dueTime->quadPart * 100);
    } else if (dueTime->quadPart > 0) {
        delayNs = std::chrono::nanoseconds(dueTime->quadPart * 100);
    }
    timerObj->setDeadline(std::chrono::steady_clock::now() + delayNs);
    timerObj->setInserted(true);

    return NtStatus::Success;
}

NtStatus NtCancelTimer(Handle timerHandle, bool* currentSignaledState) {
    auto timerObj = sync::DispatcherRegistry::get().lookupAs<timer::TimerObject>(timerHandle);
    if (!timerObj) return NtStatus::InvalidHandle;

    if (currentSignaledState) {
        *currentSignaledState = timerObj->isSignaled();
    }
    timerObj->setInserted(false);
    return NtStatus::Success;
}

NtStatus NtQueryPerformanceCounter(
    LargeInteger* performanceCounter,
    LargeInteger* performanceFrequency
) {
    if (!performanceCounter) return NtStatus::InvalidParameter;
    hal::KeQueryPerformanceCounter(*performanceCounter, performanceFrequency);
    return NtStatus::Success;
}

NtStatus NtYieldExecution() {
    std::this_thread::yield();
    return NtStatus::Success;
}

} // namespace micant::sys
