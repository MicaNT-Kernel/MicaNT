#pragma once

#include "micant/km/core/ke.hpp"

#if defined(MICANT_BAREMETAL) && (defined(_M_X64) || defined(__x86_64__))
namespace micant::sync::detail {

// Single-BSP dispatcher building block. This is not a public KEVENT or a
// Windows wait API: object references, APCs, timeouts, boosts and SMP are not
// implemented yet. The owner must keep the event alive until all waits end.
class BootstrapEvent {
public:
    explicit BootstrapEvent(bool notification, bool signaled = false) noexcept
        : notification_(notification), signaled_(signaled) {}
    BootstrapEvent(const BootstrapEvent&) = delete;
    BootstrapEvent& operator=(const BootstrapEvent&) = delete;

    ~BootstrapEvent() { if (head_) __builtin_trap(); }

    [[nodiscard]] NtStatus wait(bool poll = false) noexcept {
        uint64_t flags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        NtStatus result = NtStatus::Unsuccessful;
        auto* current = ke::KeGetCurrentThread();
        if (current && current->state == ke::ThreadState::Running &&
            ke::KeGetCurrentIrql() <= ke::APC_LEVEL) {
            if (ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL && !current->kernelApcList.empty()) {
                if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
                ke::KiDeliverApc(0);
                __asm__ __volatile__("cli" : : : "memory");
            }
            if (current->terminationRequested && current->eventWaitUserMode && current->eventWaitCanTerminate) {
                result = NtStatus::Unsuccessful;
            } else if (signaled_) {
                if (!notification_) signaled_ = false;
                result = NtStatus::Success;
            } else if (poll) {
                result = NtStatus::Timeout;
            } else {
                Waiter waiter{current};
                current->eventWaitObject = this;
                current->eventWaitBlock = &waiter;
                // A kernel APC interrupts the wait only to run its callback;
                // the original wait resumes unless it was signaled/cancelled.
                if (ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL)
                    current->interruptWaitForApc = interruptForKernelApc;
                for (;;) {
                    waiter.next = nullptr;
                    if (tail_) tail_->next = &waiter; else head_ = &waiter;
                    tail_ = &waiter;
                    if (!ke::detail::blockCurrentThread()) {
                        unlink(waiter);
                        break;
                    }
                    if (waiter.satisfied || waiter.cancelled) {
                        result = waiter.cancelled ? NtStatus::Unsuccessful : NtStatus::Success;
                        break;
                    }
                    if (!waiter.apcInterrupted) __builtin_trap();
                    waiter.apcInterrupted = false;
                    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
                    ke::KiDeliverApc(0);
                    __asm__ __volatile__("cli" : : : "memory");
                    if (waiter.cancelled || current->eventWaitCancelled ||
                        (current->terminationRequested && current->eventWaitUserMode && current->eventWaitCanTerminate)) break;
                    // A signal may arrive while the waiter is detached for
                    // APC delivery; consume it before re-entering the queue.
                    if (signaled_) {
                        if (!notification_) signaled_ = false;
                        result = NtStatus::Success;
                        break;
                    }
                }
                current->interruptWaitForApc = nullptr;
                current->eventWaitObject = nullptr;
                current->eventWaitBlock = nullptr;
                current->eventWaitCancelled = false;
            }
        }
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return result;
    }

    [[nodiscard]] bool set() noexcept {
        uint64_t flags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        const bool previous = signaled_;
        signaled_ = true;
        while (head_) {
            Waiter* waiter = head_;
            head_ = waiter->next;
            if (!head_) tail_ = nullptr;
            waiter->next = nullptr;
            waiter->satisfied = true;
            if (!ke::PriorityScheduler::get().readyKThread(waiter->thread)) __builtin_trap();
            if (!notification_) { signaled_ = false; break; }
        }
        if (flags & (uint64_t{1} << 9)) {
            ke::detail::servicePendingDpcs();
            ke::detail::dispatchHigherPriority();
            ke::detail::servicePendingKernelApcs();
            __asm__ __volatile__("sti" : : : "memory");
        }
        return previous;
    }

    [[nodiscard]] bool reset() noexcept {
        uint64_t flags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        const bool previous = signaled_;
        signaled_ = false;
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return previous;
    }

    // Caller holds the BSP dispatcher exclusion (interrupts disabled).
    [[nodiscard]] bool cancelWaitForTermination(ke::KThread& thread) noexcept {
        if (thread.eventWaitObject != this || !thread.eventWaitBlock ||
            thread.state != ke::ThreadState::Waiting || thread.eventWaitCancelled) return false;
        auto& waiter = *static_cast<Waiter*>(thread.eventWaitBlock);
        unlink(waiter);
        waiter.cancelled = true;
        thread.eventWaitCancelled = true;
        if (!ke::PriorityScheduler::get().readyKThread(&thread)) __builtin_trap();
        return true;
    }

    // Reinsert a cancelled wait if the terminating thread could not switch.
    [[nodiscard]] bool restoreCancelledWait(ke::KThread& thread) noexcept {
        if (thread.eventWaitObject != this || !thread.eventWaitBlock || !thread.eventWaitCancelled ||
            !ke::PriorityScheduler::get().removeReadyThread(&thread)) return false;
        auto& waiter = *static_cast<Waiter*>(thread.eventWaitBlock);
        waiter.cancelled = false;
        waiter.next = nullptr;
        if (tail_) tail_->next = &waiter; else head_ = &waiter;
        tail_ = &waiter;
        thread.eventWaitCancelled = false;
        thread.state = ke::ThreadState::Waiting;
        return true;
    }

private:
    struct Waiter {
        ke::KThread* thread;
        Waiter* next{nullptr};
        bool satisfied{false};
        bool cancelled{false};
        bool apcInterrupted{false};
    };
    static bool interruptForKernelApc(ke::KThread& thread) noexcept {
        if (!thread.eventWaitObject || !thread.eventWaitBlock ||
            thread.state != ke::ThreadState::Waiting || thread.eventWaitCancelled) return false;
        auto& event = *static_cast<BootstrapEvent*>(thread.eventWaitObject);
        auto& waiter = *static_cast<Waiter*>(thread.eventWaitBlock);
        event.unlink(waiter);
        waiter.apcInterrupted = true;
        if (!ke::PriorityScheduler::get().readyKThread(&thread)) __builtin_trap();
        return true;
    }
    void unlink(Waiter& waiter) noexcept {
        Waiter* previous = nullptr;
        for (Waiter* item = head_; item; item = item->next) {
            if (item == &waiter) {
                if (previous) previous->next = item->next; else head_ = item->next;
                if (tail_ == item) tail_ = previous;
                return;
            }
            previous = item;
        }
        __builtin_trap();
    }
    bool notification_;
    bool signaled_;
    Waiter* head_{nullptr};
    Waiter* tail_{nullptr};
};
}
#endif
