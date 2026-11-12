#if defined(MICANT_FREESTANDING)
#include "micant/km/core/ke.hpp"

// Requires a single CPU, disabled interrupts, an initialized C++ runtime and
// heap, and an empty scheduler. Does not switch contexts or enable interrupts.
extern "C" bool MicantTestKeReadyQueue() noexcept {
    using namespace micant::ke;
    auto& scheduler = PriorityScheduler::get();
    if (scheduler.getReadyThreadCount() != 0) return false;

    KThread a, b, c, d, other;
    struct Cleanup {
        PriorityScheduler& scheduler;
        KThread* threads[5];
        ~Cleanup() {
            for (auto* thread : threads) {
                if (thread->readyQueued) (void)scheduler.removeReadyThread(thread);
            }
        }
    } cleanup{scheduler, {&a, &b, &c, &d, &other}};

    a.priority = b.priority = c.priority = d.priority = 12;
    other.priority = 4;
    if (scheduler.removeReadyThread(nullptr) ||
        scheduler.removeReadyThread(&a) ||
        !scheduler.readyKThread(&a) || !scheduler.readyKThread(&b) ||
        !scheduler.readyKThread(&c) || !scheduler.readyKThread(&d) ||
        !scheduler.readyKThread(&other) || scheduler.getReadyThreadCount() != 5)
        return false;

    // Withdraw the middle, tail and head, including a changed priority.
    b.priority = 31;
    if (!scheduler.removeReadyThread(&b) || b.readyQueued || b.readyNext ||
        b.state != ThreadState::Transition || scheduler.removeReadyThread(&b) ||
        !scheduler.removeReadyThread(&d) || !scheduler.removeReadyThread(&a) ||
        scheduler.getReadyThreadCount() != 2)
        return false;
    d.state = ThreadState::Waiting;
    if (!scheduler.readyKThread(&d)) return false;
    if (scheduler.selectNextThread() != &c || c.state != ThreadState::Standby ||
        scheduler.removeReadyThread(&c) || scheduler.selectNextThread() != &d ||
        scheduler.selectNextThread() != &other || scheduler.selectNextThread() ||
        scheduler.getReadyThreadCount() != 0)
        return false;

    // A withdrawn thread may only be readied after its owner changes state.
    if (scheduler.readyKThread(&b)) return false;
    b.state = ThreadState::Waiting;
    if (!scheduler.readyKThread(&b) || !scheduler.removeReadyThread(&b) ||
        scheduler.getReadyThreadCount() != 0)
        return false;
    b.state = ThreadState::Terminated;
    if (scheduler.readyKThread(&b) || scheduler.removeReadyThread(&b))
        return false;

    // The current thread must never be placed on its own ready queue.
    KThread* const saved = g_CurrentCpu.currentThread;
    g_CurrentCpu.currentThread = &a;
    a.state = ThreadState::Ready;
    const bool currentRejected = !scheduler.readyKThread(&a);
    a.state = ThreadState::Running;
    a.priority = 12;
    a.quantumRemaining = 2;
    b.state = ThreadState::Waiting;
    b.priority = 12;
    const bool preemptionPrepared = scheduler.readyKThread(&b) &&
        scheduler.selectOnClockTick(&a) == nullptr && a.quantumRemaining == 1 &&
        scheduler.selectOnClockTick(&a) == &b && a.readyQueued &&
        b.state == ThreadState::Standby && scheduler.getReadyThreadCount() == 1;
    const bool rolledBack = preemptionPrepared && scheduler.cancelQuantumPreemption(&a, &b) &&
        !a.readyQueued && b.readyQueued && b.state == ThreadState::Ready &&
        scheduler.selectNextThread() == &b && scheduler.getReadyThreadCount() == 0;
    b.state = ThreadState::Waiting;
    b.priority = 13;
    a.quantumRemaining = 5;
    const bool higherPriority = scheduler.readyKThread(&b) &&
        scheduler.selectOnClockTick(&a) == &b && a.quantumRemaining == 4 && a.readyQueued &&
        scheduler.cancelQuantumPreemption(&a, &b) && scheduler.selectNextThread() == &b;
    b.state = ThreadState::Waiting;
    b.priority = 11;
    a.quantumRemaining = 1;
    const bool lowerPriority = scheduler.readyKThread(&b) &&
        scheduler.selectOnClockTick(&a) == nullptr && !a.readyQueued &&
        scheduler.selectNextThread() == &b;
    g_CurrentCpu.currentThread = saved;
    return currentRejected && rolledBack && higherPriority && lowerPriority &&
        scheduler.getReadyThreadCount() == 0;
}
#endif
