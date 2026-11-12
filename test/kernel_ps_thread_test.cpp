#include "micant/km/executive/ps.hpp"
#include "micant/km/core/ke.hpp"

namespace {
struct State {
    micant::ps::EProcess* process{};
    micant::ke::KThread* boot{};
    bool ran{};
};

void threadEntry(void* argument) noexcept {
    auto& state = *static_cast<State*>(argument);
    auto* current = micant::ke::KeGetCurrentThread();
    uintptr_t cr3{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(cr3));
    if (!current || current->owningProcess != state.process ||
        cr3 != state.process->getDirectoryTableBase() ||
        !micant::ke::PriorityScheduler::get().readyKThread(state.boot)) __builtin_trap();
    state.ran = true;
}
}

extern "C" bool MicantTestPsThreadLifecycle() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto& scheduler = PriorityScheduler::get();
    auto* boot = KeGetCurrentThread();
    if (!boot || scheduler.getReadyThreadCount()) return false;
    const size_t active = mm::PfnDatabase::get().getActiveCount();
    {
        ps::EProcess process;
        ps::EThread thread;
        if (thread.initialize(process, 1, threadEntry, nullptr) != NtStatus::InvalidParameter ||
            thread.initialize(process, 16384, threadEntry, nullptr) != NtStatus::InvalidParameter ||
            process.initializeAddressSpace() != NtStatus::Success) return false;
        State state{&process, boot, false};
        if (thread.initialize(process, 16384, threadEntry, &state) != NtStatus::Success ||
            process.closeAddressSpace() != NtStatus::InvalidParameter ||
            thread.ready() != NtStatus::Success || thread.ready() != NtStatus::InvalidParameter ||
            thread.reap() != NtStatus::InvalidParameter) return false;
        auto* next = scheduler.selectNextThread();
        if (next != thread.kernelThread()) return false;
        const KIRQL previous = KfRaiseIrql(DISPATCH_LEVEL);
        const bool switched = KiSwapContext(boot, next);
        KeLowerIrql(previous);
        if (!switched || !state.ran || KeGetCurrentThread() != boot ||
            next->state != ThreadState::Terminated || scheduler.getReadyThreadCount() ||
            thread.reap() != NtStatus::Success || process.closeAddressSpace() != NtStatus::Success)
            return false;
    }
    return mm::PfnDatabase::get().getActiveCount() == active;
}
