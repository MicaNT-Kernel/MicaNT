#pragma once

#include "micant/km/core/ke.hpp"
#include "micant/km/core/clock_timer.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"

// Resident single-BSP passive work queue. Items and contexts remain resident
// until the callback returns. Requeued items never run concurrently with themselves.
#if defined(MICANT_BAREMETAL) && defined(MICANT_FREESTANDING) && (defined(_M_X64) || defined(__x86_64__))
namespace micant::ke::passive_work {
using Routine = void (*)(void*) noexcept;
struct Item {
    Routine routine{};
    void* context{};
    Item* next{};
    bool queued{};
    bool running{};
};

namespace detail {
inline constexpr size_t WorkerCount = 2;
inline KThread workers[WorkerCount]{};
alignas(16) inline uint8_t stacks[WorkerCount][65536]{};
inline Item* head{};
inline Item* tail{};
inline size_t initializedWorkers{};
inline bool initialized{};

// Caller holds BSP interrupt exclusion.
inline void wakeAvailableWorker() noexcept {
    auto* current = KeGetCurrentThread();
    for (auto& worker : workers) {
        if (&worker != current && !worker.readyQueued &&
            (worker.state == ThreadState::Initialized || worker.state == ThreadState::Waiting)) {
            if (!PriorityScheduler::get().readyKThread(&worker)) __builtin_trap();
            break;
        }
    }
}

[[noreturn]] inline void run(void*) noexcept {
    __asm__ __volatile__("cli" : : : "memory");
    KeLowerIrql(PASSIVE_LEVEL);
    for (;;) {
        Item* previous = nullptr;
        Item* item = head;
        while (item && item->running) { previous = item; item = item->next; }
        if (!item) {
            // Queue inspection and the transition to Waiting share the BSP
            // exclusion, including when only a running item is requeued.
            if (!ke::detail::blockCurrentThread())
                __asm__ __volatile__("sti; hlt; cli" : : : "memory");
            continue;
        }
        if (previous) previous->next = item->next; else head = item->next;
        if (tail == item) tail = previous;
        item->next = nullptr;
        item->queued = false;
        item->running = true;
        auto routine = item->routine;
        void* context = item->context;
        __asm__ __volatile__("sti" : : : "memory");
        routine(context);
        __asm__ __volatile__("cli" : : : "memory");
        item->running = false;
        if (head) wakeAvailableWorker();
    }
}

// False means the callback is executing; its owner must keep the item alive
// and retry after the callback completes. Interrupt exclusion covers dequeue.
[[nodiscard]] inline bool cancelPending(Item& item) noexcept {
    if (KeGetCurrentIrql() != PASSIVE_LEVEL) return false;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    bool safe = !item.running;
    if (safe && item.queued) {
        Item* previous = nullptr;
        Item* current = detail::head;
        while (current && current != &item) { previous = current; current = current->next; }
        if (!current) __builtin_trap();
        if (previous) previous->next = item.next;
        else detail::head = item.next;
        if (detail::tail == &item) detail::tail = previous;
        item.next = nullptr;
        item.queued = false;
    }
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return safe;
}
}

inline void scheduleRetiredApcs() noexcept;

// Initialize after the scheduler clock and idle thread have been installed.
// The worker and stack are kernel-resident for the life of the system.
[[nodiscard]] inline bool initialize() noexcept {
    if (KeGetCurrentIrql() != PASSIVE_LEVEL || !g_CurrentCpu.idleThread ||
        !hal::amd64::bootstrapSchedulerClockActive()) return false;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (!detail::initialized) {
        const uintptr_t root = hal::amd64::bootstrapKernelPagingRoot();
        while (root && detail::initializedWorkers < detail::WorkerCount) {
            const auto index = detail::initializedWorkers;
            auto& worker = detail::workers[index];
            worker.directoryTableBase = root;
            worker.priority = PRIORITY_NORMAL;
            if (!ke::detail::prepareBootstrapThread(worker, detail::stacks[index],
                    sizeof(detail::stacks[index]), detail::run, nullptr)) break;
            ++detail::initializedWorkers;
        }
        detail::initialized = detail::initializedWorkers == detail::WorkerCount;
    }
    const bool result = detail::initialized;
    if (result) scheduleRetiredApcs();
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return result;
}

[[nodiscard]] inline bool isInitialized() noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    const bool ready = detail::initialized;
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return ready;
}

// The caller owns the item and serializes changes to its routine and context.
// False means uninitialized, invalid, or already queued; no callback was added.
[[nodiscard]] inline bool queue(Item& item) noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    bool result = false;
    if (detail::initialized && item.routine && !item.queued) {
        item.next = nullptr;
        item.queued = true;
        if (detail::tail) detail::tail->next = &item;
        else detail::head = &item;
        detail::tail = &item;
        detail::wakeAvailableWorker();
        result = true;
    }
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return result;
}

// User APC kernel callbacks run at APC_LEVEL. They transfer storage here so
// heap release occurs on the resident PASSIVE worker, after Ke has copied the
// user callback arguments and dropped the APC queue pin.
inline KAPC* retiredApcs{};
inline Item retiredApcWork{};
inline void freeRetiredApcs(void*) noexcept {
    for (;;) {
        uint64_t flags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        auto* apc = retiredApcs;
        if (apc) retiredApcs = apc->queueNext;
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        if (!apc) return;
        apc->queueNext = nullptr;
        delete apc;
    }
}
inline void scheduleRetiredApcs() noexcept {
    // A callback can run before the resident workers have started. Keep the
    // APC on the retired list until initialization makes PASSIVE cleanup safe.
    if (!detail::initialized || !retiredApcs || retiredApcWork.queued) return;
    retiredApcWork.routine = freeRetiredApcs;
    if (!queue(retiredApcWork)) __builtin_trap();
}
inline void retireOwnedUserApc(KAPC* apc) noexcept {
    if (!apc) return;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    apc->queueNext = retiredApcs;
    retiredApcs = apc;
    scheduleRetiredApcs();
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
}
inline void ownedUserApcKernel(KAPC* apc, PKNORMAL_ROUTINE*, void**, void**, void**) {
    retireOwnedUserApc(apc);
}
inline void ownedUserApcRundown(KAPC* apc) { retireOwnedUserApc(apc); }

// A periodic item is resident for the lifetime of its timer and any callback
// already running. A queued item is coalesced while its previous run waits.
struct PeriodicItem {
    Item work{};
    KDPC dpc{};
    clock_timer::Deadline deadline{};
};

inline void periodicDpc(KDPC*, void* context, void*, void*) {
    auto* item = static_cast<PeriodicItem*>(context);
    (void)queue(item->work);
}

[[nodiscard]] inline bool startPeriodic(PeriodicItem& item, Routine routine,
                                         void* context, uint64_t period100ns) noexcept {
    if (!routine || !period100ns || item.deadline.inserted || item.dpc.inserted ||
        item.work.queued || item.work.running || !detail::initialized) return false;
    item.work.routine = routine;
    item.work.context = context;
    KeInitializeDpc(&item.dpc, periodicDpc, &item);
    return clock_timer::arm(item.deadline, item.dpc, period100ns, period100ns);
}

[[nodiscard]] inline bool stopPeriodic(PeriodicItem& item) noexcept {
    if (!clock_timer::cancelAndRemoveDpc(item.deadline)) return false;
    return detail::cancelPending(item.work);
}
}
#endif
