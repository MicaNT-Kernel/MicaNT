#include "micant/km/core/ke.hpp"

namespace {
using namespace micant::ke;
KThread first;
KThread second;
KThread* boot{};
alignas(16) uint8_t firstStack[16384]{};
alignas(16) uint8_t secondStack[16384]{};
uint32_t stage{};

[[noreturn]] void firstEntry(void*) noexcept {
    KeLowerIrql(PASSIVE_LEVEL);
    stage = 1;
    if (!detail::blockCurrentThread() || stage != 2 || KeGetCurrentThread() != &first)
        __builtin_trap();
    stage = 3;
    if (!PriorityScheduler::get().readyKThread(&second)) __builtin_trap();
    first.state = ThreadState::Terminated;
    KfRaiseIrql(DISPATCH_LEVEL);
    auto* next = PriorityScheduler::get().selectNextThread();
    if (next != &second || !KiSwapContext(&first, next)) __builtin_trap();
    __builtin_trap();
}

[[noreturn]] void secondEntry(void*) noexcept {
    KeLowerIrql(PASSIVE_LEVEL);
    if (stage != 1 || first.state != ThreadState::Waiting ||
        !PriorityScheduler::get().readyKThread(&first)) __builtin_trap();
    stage = 2;
    if (!detail::blockCurrentThread() || stage != 3 || KeGetCurrentThread() != &second)
        __builtin_trap();
    stage = 4;
    second.state = ThreadState::Terminated;
    KfRaiseIrql(DISPATCH_LEVEL);
    if (!KiSwapContext(&second, boot)) __builtin_trap();
    __builtin_trap();
}
}

extern "C" bool MicantTestKeBlockWake() noexcept {
    using namespace micant::ke;
    auto& scheduler = PriorityScheduler::get();
    boot = KeGetCurrentThread();
    if (!boot || boot->state != ThreadState::Running || scheduler.getReadyThreadCount() ||
        detail::blockCurrentThread() || boot->state != ThreadState::Running) return false;
    KThread invalid;
    invalid.directoryTableBase = g_CurrentCpu.currentCr3;
    if (!scheduler.readyKThread(&invalid) || detail::blockCurrentThread() ||
        boot->state != ThreadState::Running || scheduler.getReadyThreadCount() != 1 ||
        scheduler.selectNextThread() != &invalid || scheduler.getReadyThreadCount() != 0) return false;
    first.directoryTableBase = second.directoryTableBase = g_CurrentCpu.currentCr3;
    if (!detail::prepareBootstrapThread(first, firstStack, sizeof(firstStack), firstEntry, nullptr) ||
        !detail::prepareBootstrapThread(second, secondStack, sizeof(secondStack), secondEntry, nullptr) ||
        !scheduler.readyKThread(&second)) return false;
    const KIRQL previous = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = KiSwapContext(boot, &first);
    KeLowerIrql(previous);
    return switched && stage == 4 && KeGetCurrentThread() == boot &&
        first.state == ThreadState::Terminated && second.state == ThreadState::Terminated &&
        scheduler.getReadyThreadCount() == 0;
}
