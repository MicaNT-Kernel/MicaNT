#include "micant/km/core/sync.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/executive/ob.hpp"

namespace {
using namespace micant;
using namespace micant::ke;
using sync::detail::BootstrapEvent;
KThread first;
KThread second;
KThread* boot{};
alignas(16) uint8_t firstStack[16384]{};
alignas(16) uint8_t secondStack[16384]{};
Handle firstEvent{};
Handle firstEventDuplicate{};
Handle secondEvent{};
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
    if (sys::NtWaitForSingleObject(firstEvent, false, nullptr) != NtStatus::Success || stage != 2) __builtin_trap();
    stage = 3;
    if (sys::NtSetEvent(secondEvent, nullptr) != NtStatus::Success) __builtin_trap();
    first.state = ThreadState::Terminated;
    KfRaiseIrql(DISPATCH_LEVEL);
    auto* next = PriorityScheduler::get().selectNextThread();
    if (next != &second || !KiSwapContext(&first, next)) __builtin_trap();
    __builtin_trap();
}

[[noreturn]] void secondEntry(void*) noexcept {
    KeLowerIrql(PASSIVE_LEVEL);
    ob::ObjectHeader* header{};
    if (stage != 1 || first.state != ThreadState::Waiting ||
        ob::ObReferenceObjectByHandle(firstEvent, ob::detail::currentHandleTable(), ob::SYNCHRONIZE,
                                      nullptr, &header) != NtStatus::Success) __builtin_trap();
    auto* event = header->as<BootstrapEvent>();
    if (!event->cancelWaitForTermination(first) || first.state != ThreadState::Ready ||
        PriorityScheduler::get().getReadyThreadCount() != 1 ||
        !event->restoreCancelledWait(first) || first.state != ThreadState::Waiting ||
        PriorityScheduler::get().getReadyThreadCount()) __builtin_trap();
    ob::ObDereferenceObject(header);
    if (sys::NtClose(firstEvent) != NtStatus::Success ||
        sys::NtSetEvent(firstEventDuplicate, nullptr) != NtStatus::Success) __builtin_trap();
    stage = 2;
    if (sys::NtWaitForSingleObject(secondEvent, false, nullptr) != NtStatus::Success || stage != 3) __builtin_trap();
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
    constexpr uint32_t eventAccess = ob::SYNCHRONIZE | 2;
    LargeInteger poll{};
    Handle limited{};
    if (sys::NtCreateEvent(&limited, 1, nullptr, 0, false) != NtStatus::Success ||
        sys::NtSetEvent(limited, nullptr) != NtStatus::AccessDenied ||
        sys::NtWaitForSingleObject(limited, false, &poll) != NtStatus::AccessDenied ||
        sys::NtClose(limited) != NtStatus::Success ||
        sys::NtCreateEvent(&limited, eventAccess, nullptr, 2, false) != NtStatus::InvalidParameter4)
        return false;
    if (!boot || scheduler.getReadyThreadCount() ||
        sys::NtCreateEvent(&firstEvent, eventAccess, nullptr, 1, false) != NtStatus::Success ||
        sys::NtCreateEvent(&secondEvent, eventAccess, nullptr, 1, false) != NtStatus::Success ||
        ob::detail::currentHandleTable().duplicateHandle(firstEvent, ob::detail::currentHandleTable(),
                                                          firstEventDuplicate) != NtStatus::Success ||
        sys::NtWaitForSingleObject(firstEvent, false, &poll) != NtStatus::Timeout ||
        sys::NtWaitForSingleObject(firstEvent, false, nullptr) != NtStatus::Unsuccessful)
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
        scheduler.getReadyThreadCount() || sys::NtClose(firstEventDuplicate) != NtStatus::Success ||
        sys::NtClose(secondEvent) != NtStatus::Success) return false;
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
