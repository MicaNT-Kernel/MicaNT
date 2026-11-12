#include "micant/km/core/sync.hpp"

namespace {
using namespace micant;
using namespace micant::ke;
using sync::detail::BootstrapEvent;
KThread first;
KThread second;
KThread* boot{};
alignas(16) uint8_t firstStack[16384]{};
alignas(16) uint8_t secondStack[16384]{};
BootstrapEvent firstEvent{false};
BootstrapEvent secondEvent{false};
uint32_t stage{};
KThread notificationFirst;
KThread notificationSecond;
KThread notifier;
alignas(16) uint8_t notificationFirstStack[16384]{};
alignas(16) uint8_t notificationSecondStack[16384]{};
alignas(16) uint8_t notifierStack[16384]{};
BootstrapEvent broadcast{true};
uint32_t broadcastStage{};

[[noreturn]] void notificationEntry(void* argument) noexcept {
    KeLowerIrql(PASSIVE_LEVEL);
    const bool firstWaiter = argument != nullptr;
    ++broadcastStage;
    if (broadcast.wait() != NtStatus::Success || broadcastStage < 3) __builtin_trap();
    ++broadcastStage;
    auto* current = KeGetCurrentThread();
    current->state = ThreadState::Terminated;
    KfRaiseIrql(DISPATCH_LEVEL);
    auto* next = firstWaiter ? PriorityScheduler::get().selectNextThread() : boot;
    if (!next || !KiSwapContext(current, next)) __builtin_trap();
    __builtin_trap();
}

[[noreturn]] void notifierEntry(void*) noexcept {
    KeLowerIrql(PASSIVE_LEVEL);
    if (broadcastStage != 2 || broadcast.set() ||
        PriorityScheduler::get().getReadyThreadCount() != 2) __builtin_trap();
    ++broadcastStage;
    notifier.state = ThreadState::Terminated;
    KfRaiseIrql(DISPATCH_LEVEL);
    auto* next = PriorityScheduler::get().selectNextThread();
    if (next != &notificationFirst || !KiSwapContext(&notifier, next)) __builtin_trap();
    __builtin_trap();
}

[[noreturn]] void firstEntry(void*) noexcept {
    KeLowerIrql(PASSIVE_LEVEL);
    stage = 1;
    if (firstEvent.wait() != NtStatus::Success || stage != 2) __builtin_trap();
    stage = 3;
    if (secondEvent.set()) __builtin_trap();
    first.state = ThreadState::Terminated;
    KfRaiseIrql(DISPATCH_LEVEL);
    auto* next = PriorityScheduler::get().selectNextThread();
    if (next != &second || !KiSwapContext(&first, next)) __builtin_trap();
    __builtin_trap();
}

[[noreturn]] void secondEntry(void*) noexcept {
    KeLowerIrql(PASSIVE_LEVEL);
    if (stage != 1 || first.state != ThreadState::Waiting || firstEvent.set()) __builtin_trap();
    stage = 2;
    if (secondEvent.wait() != NtStatus::Success || stage != 3) __builtin_trap();
    stage = 4;
    second.state = ThreadState::Terminated;
    KfRaiseIrql(DISPATCH_LEVEL);
    if (!KiSwapContext(&second, boot)) __builtin_trap();
    __builtin_trap();
}
}

extern "C" bool MicantTestKeEventWait() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto& scheduler = PriorityScheduler::get();
    boot = KeGetCurrentThread();
    if (!boot || scheduler.getReadyThreadCount() || firstEvent.wait() != NtStatus::Unsuccessful)
        return false;
    BootstrapEvent notification{true};
    BootstrapEvent synchronization{false};
    if (notification.set() || !notification.set() ||
        notification.wait() != NtStatus::Success || notification.wait() != NtStatus::Success ||
        !notification.reset() || notification.reset() || synchronization.set() ||
        synchronization.wait() != NtStatus::Success || synchronization.reset()) return false;
    first.directoryTableBase = second.directoryTableBase = g_CurrentCpu.currentCr3;
    if (!detail::prepareBootstrapThread(first, firstStack, sizeof(firstStack), firstEntry, nullptr) ||
        !detail::prepareBootstrapThread(second, secondStack, sizeof(secondStack), secondEntry, nullptr) ||
        !scheduler.readyKThread(&second)) return false;
    const KIRQL previous = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = KiSwapContext(boot, &first);
    KeLowerIrql(previous);
    if (!switched || stage != 4 || KeGetCurrentThread() != boot ||
        first.state != ThreadState::Terminated || second.state != ThreadState::Terminated ||
        scheduler.getReadyThreadCount()) return false;
    notificationFirst.directoryTableBase = notificationSecond.directoryTableBase =
        notifier.directoryTableBase = g_CurrentCpu.currentCr3;
    if (!detail::prepareBootstrapThread(notificationFirst, notificationFirstStack,
            sizeof(notificationFirstStack), notificationEntry, &notificationFirst) ||
        !detail::prepareBootstrapThread(notificationSecond, notificationSecondStack,
            sizeof(notificationSecondStack), notificationEntry, nullptr) ||
        !detail::prepareBootstrapThread(notifier, notifierStack, sizeof(notifierStack), notifierEntry, nullptr) ||
        !scheduler.readyKThread(&notificationSecond) || !scheduler.readyKThread(&notifier)) return false;
    const KIRQL broadcastPrevious = KfRaiseIrql(DISPATCH_LEVEL);
    const bool broadcastSwitched = KiSwapContext(boot, &notificationFirst);
    KeLowerIrql(broadcastPrevious);
    return broadcastSwitched && broadcastStage == 5 && scheduler.getReadyThreadCount() == 0 &&
        broadcast.reset() && notificationFirst.state == ThreadState::Terminated &&
        notificationSecond.state == ThreadState::Terminated && notifier.state == ThreadState::Terminated;
}
