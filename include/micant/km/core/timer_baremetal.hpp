#pragma once

#include "micant/km/core/clock_timer.hpp"
#include "micant/km/core/passive_work.hpp"
#include "micant/km/core/sync_baremetal.hpp"
#include "micant/shared/ntdef.hpp"

namespace micant::timer {
enum class TimerType : uint32_t { NotificationTimer = 0, SynchronizationTimer = 1 };

// Caller retains the timer, event waiters and optional DPC until cancellation
// and any queued DPC have completed. Deadlines share the Ke clock queue.
struct KTIMER {
    explicit KTIMER(TimerType type = TimerType::NotificationTimer) noexcept
        : event(type == TimerType::NotificationTimer) {}
    sync::detail::BootstrapEvent event;
    ke::clock_timer::Deadline deadline{};
    ke::KDPC timerDpc{};
    ke::KDPC* userDpc{};
};

inline void KiTimerDpc(ke::KDPC*, void* context, void*, void*) {
    auto* timer = static_cast<KTIMER*>(context);
    (void)timer->event.set();
    if (timer->userDpc) (void)ke::KeInsertQueueDpc(timer->userDpc);
}

inline void KeInitializeTimerEx(KTIMER* timer, TimerType type) noexcept {
    if (!timer) return;
    if (timer->deadline.inserted || timer->timerDpc.inserted) __builtin_trap();
    timer->event.initialize(type == TimerType::NotificationTimer);
    timer->deadline = {};
    timer->userDpc = nullptr;
    ke::KeInitializeDpc(&timer->timerDpc, KiTimerDpc, timer);
}
inline void KeInitializeTimer(KTIMER* timer) noexcept {
    KeInitializeTimerEx(timer, TimerType::NotificationTimer);
}

inline NtStatus KiSetTimerChecked(KTIMER* timer, LargeInteger dueTime, uint32_t periodMs,
                                  ke::KDPC* dpc, bool& previous) noexcept {
    if (!timer || ke::KeGetCurrentIrql() > ke::DISPATCH_LEVEL) return NtStatus::InvalidParameter;
    uint64_t delay{};
    if (dueTime.quadPart < 0) {
        delay = static_cast<uint64_t>(-(dueTime.quadPart + 1)) + 1;
    } else if (dueTime.quadPart > 0) {
        uint64_t utc{};
        if (hal::amd64::queryBootstrapSystemTime(utc) != hal::amd64::BootstrapClockStatus::Success)
            return NtStatus::DeviceNotReady;
        const auto target = static_cast<uint64_t>(dueTime.quadPart);
        delay = target > utc ? target - utc : 0;
    }
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    previous = timer->deadline.inserted;
    const bool armed = ke::clock_timer::arm(timer->deadline, timer->timerDpc, delay,
                                             static_cast<uint64_t>(periodMs) * 10'000u);
    if (armed) {
        // Discard an undelivered expiration from the previous setting before
        // publishing the replacement callback; an already queued user DPC is
        // independent and remains the caller's responsibility.
        (void)ke::KeRemoveQueueDpc(&timer->timerDpc);
        timer->userDpc = dpc;
        (void)timer->event.reset();
    }
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return armed ? NtStatus::Success : NtStatus::DeviceNotReady;
}
inline bool KeSetTimerEx(KTIMER* timer, LargeInteger dueTime, uint32_t periodMs,
                         ke::KDPC* dpc = nullptr) noexcept {
    bool previous{};
    (void)KiSetTimerChecked(timer, dueTime, periodMs, dpc, previous);
    return previous;
}
inline bool KeSetTimer(KTIMER* timer, LargeInteger dueTime, ke::KDPC* dpc = nullptr) noexcept {
    return KeSetTimerEx(timer, dueTime, 0, dpc);
}
inline bool KeCancelTimer(KTIMER* timer) noexcept {
    return timer && ke::clock_timer::cancel(timer->deadline);
}
inline bool KeReadStateTimer(const KTIMER* timer) noexcept {
    return timer && timer->event.readState();
}
}
