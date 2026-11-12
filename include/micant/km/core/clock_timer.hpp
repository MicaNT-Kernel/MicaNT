#pragma once

#include "micant/km/core/ke.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"

// BSP kernel clock deadlines. Storage and the DPC remain resident until the
// deadline is cancelled and any already queued DPC has completed.
#if defined(MICANT_BAREMETAL) && defined(MICANT_FREESTANDING) && (defined(_M_X64) || defined(__x86_64__))
namespace micant::ke::clock_timer {
struct Deadline {
    Deadline* next{};
    KDPC* dpc{};
    uint64_t due100ns{};
    uint64_t period100ns{};
    bool inserted{};
};
inline Deadline* deadlines{};

inline void unlink(Deadline& timer) noexcept {
    auto** p = &deadlines;
    while (*p && *p != &timer) p = &(*p)->next;
    if (*p) *p = timer.next;
    timer.next = nullptr;
    timer.inserted = false;
}

// delay and period use monotonic 100 ns units; period zero is one-shot.
[[nodiscard]] inline bool arm(Deadline& timer, KDPC& dpc,
                              uint64_t delay100ns, uint64_t period100ns = 0) noexcept {
    if (!dpc.routine || KeGetCurrentIrql() > DISPATCH_LEVEL) return false;
    uint64_t now{};
    if (hal::amd64::queryBootstrapUptime(now) != hal::amd64::BootstrapClockStatus::Success ||
        delay100ns > UINT64_MAX - now) return false;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (timer.dpc && timer.dpc != &dpc) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return false;
    }
    if (timer.inserted) unlink(timer);
    timer.dpc = &dpc;
    timer.due100ns = now + delay100ns;
    timer.period100ns = period100ns;
    timer.next = deadlines;
    deadlines = &timer;
    timer.inserted = true;
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return true;
}

[[nodiscard]] inline bool cancel(Deadline& timer) noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    const bool inserted = timer.inserted;
    if (inserted) unlink(timer);
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return inserted;
}

// At PASSIVE_LEVEL on the single BSP no DPC can be executing concurrently.
// Remove both the deadline and its pending DPC while interrupts are excluded;
// the caller must separately synchronize any work queued by that DPC.
[[nodiscard]] inline bool cancelAndRemoveDpc(Deadline& timer) noexcept {
    if (KeGetCurrentIrql() != PASSIVE_LEVEL) return false;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (g_CurrentCpu.dpcRoutineActive) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return false;
    }
    if (timer.inserted) unlink(timer);
    if (timer.dpc) (void)KeRemoveQueueDpc(timer.dpc);
    timer.dpc = nullptr;
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return true;
}

// Called by the BSP clock ISR with hardware interrupts disabled. DPC queueing
// never calls the consumer here; callback execution remains at DISPATCH_LEVEL.
inline void onClockInterrupt() noexcept {
    uint64_t now{};
    if (hal::amd64::queryBootstrapUptime(now) != hal::amd64::BootstrapClockStatus::Success) return;
    auto** link = &deadlines;
    while (*link) {
        Deadline* timer = *link;
        if (now < timer->due100ns) { link = &timer->next; continue; }
        if (timer->dpc) (void)KeInsertQueueDpc(timer->dpc);
        if (!timer->period100ns || timer->period100ns > UINT64_MAX - now) {
            *link = timer->next;
            timer->next = nullptr;
            timer->inserted = false;
        } else {
            timer->due100ns = now + timer->period100ns;
            link = &timer->next;
        }
    }
}
}
#endif
