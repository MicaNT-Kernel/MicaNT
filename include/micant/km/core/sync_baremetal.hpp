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

    [[nodiscard]] NtStatus wait() noexcept {
        uint64_t flags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        NtStatus result = NtStatus::Unsuccessful;
        auto* current = ke::KeGetCurrentThread();
        if (current && current->state == ke::ThreadState::Running &&
            ke::KeGetCurrentIrql() <= ke::APC_LEVEL) {
            if (signaled_) {
                if (!notification_) signaled_ = false;
                result = NtStatus::Success;
            } else {
                Waiter waiter{current};
                if (tail_) tail_->next = &waiter;
                else head_ = &waiter;
                tail_ = &waiter;
                if (ke::detail::blockCurrentThread()) {
                    if (!waiter.satisfied) __builtin_trap();
                    result = NtStatus::Success;
                } else {
                    unlink(waiter);
                }
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
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
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

private:
    struct Waiter { ke::KThread* thread; Waiter* next{nullptr}; bool satisfied{false}; };
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
