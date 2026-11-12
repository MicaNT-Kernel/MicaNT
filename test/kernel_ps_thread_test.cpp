#include "micant/km/executive/ps.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/executive/ps_objects.hpp"

namespace {
struct State {
    micant::ps::EProcess* process{};
    micant::ke::KThread* boot{};
    bool ran{};
    micant::ps::EThread* owner{};
    micant::ps::EThread* other{};
};

void threadEntry(void* argument) noexcept {
    auto& state = *static_cast<State*>(argument);
    auto* current = micant::ke::KeGetCurrentThread();
    uintptr_t cr3{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(cr3));
    if (!current || !state.owner || state.owner->cancelBeforeStart() != micant::NtStatus::InvalidParameter ||
        micant::hal::amd64::bootstrapThreadSyscallStack() != current->kernelStackTop ||
        current->owningProcess != state.process ||
        cr3 != state.process->getDirectoryTableBase() ||
        !micant::ke::PriorityScheduler::get().readyKThread(state.boot)) __builtin_trap();
    state.ran = true;
}

void terminatingEntry(void* argument) noexcept {
    auto& state = *static_cast<State*>(argument);
    uintptr_t address{};
    size_t size = micant::mm::PageSize4KB;
    if (!state.other ||
        micant::sys::NtTerminateProcess(micant::InvalidHandleValue, micant::NtStatus::AccessDenied) !=
            micant::NtStatus::NotImplemented || state.process->isTerminating() ||
        state.other->reap() != micant::NtStatus::Success ||
        micant::sys::NtAllocateVirtualMemory(micant::InvalidHandleValue, &address, 0, &size,
            micant::mm::MEM_RESERVE | micant::mm::MEM_COMMIT, micant::mm::PAGE_READWRITE) != micant::NtStatus::Success ||
        !address ||
        micant::sys::NtTerminateProcess(micant::InvalidHandleValue, micant::NtStatus::AccessDenied) !=
            micant::NtStatus::Unsuccessful || state.process->isTerminating() ||
        state.process->exitStatus() != micant::NtStatus::Pending ||
        !micant::ke::PriorityScheduler::get().readyKThread(state.boot)) __builtin_trap();
    state.ran = true;
    (void)micant::sys::NtTerminateProcess(micant::InvalidHandleValue, micant::NtStatus::AccessDenied);
    __builtin_trap();
}

struct ObjectExitState {
    micant::ke::KThread* boot{};
    micant::Handle selfHandle{};
    bool ran{};
};

void objectTerminatingEntry(void* argument) noexcept {
    auto& state = *static_cast<ObjectExitState*>(argument);
    if (!micant::ke::PriorityScheduler::get().readyKThread(state.boot)) __builtin_trap();
    state.ran = true;
    (void)micant::sys::NtTerminateProcess(state.selfHandle, micant::NtStatus::AccessDenied);
    __builtin_trap();
}

bool testProcessObjectLifetime() noexcept {
    using namespace micant;
    const size_t active = mm::PfnDatabase::get().getActiveCount();
    const uint32_t objects = ps::detail::bootstrapProcessObjectCount();
    Handle handle{};
    ps::EProcess* process{};
    if (ps::detail::createProcessHandle(0, handle, process) != NtStatus::Success || !process ||
        ps::detail::bootstrapProcessObjectCount() != objects + 1 ||
        sys::NtTerminateProcess(handle, NtStatus::Success) != NtStatus::AccessDenied) return false;
    ob::ObjectHeader* reference{};
    ps::EProcess* resolved{};
    if (ps::detail::referenceProcessHandle(handle, 0, resolved, reference) != NtStatus::Success ||
        resolved != process || !reference) return false;
    ob::ObDereferenceObject(reference);
    ke::KThread pinned;
    if (process->attachThread(pinned) != NtStatus::Success || sys::NtClose(handle) != NtStatus::Success ||
        ps::detail::bootstrapProcessObjectCount() != objects + 1 ||
        process->closeAddressSpace() != NtStatus::InvalidParameter ||
        process->detachThread(pinned) != NtStatus::Success ||
        ps::detail::bootstrapProcessObjectCount() != objects ||
        mm::PfnDatabase::get().getActiveCount() != active) return false;

    if (ps::detail::createProcessHandle(1, handle, process) != NtStatus::Success ||
        sys::NtTerminateProcess(handle, NtStatus::Success) != NtStatus::NotImplemented ||
        sys::NtClose(handle) != NtStatus::Success ||
        sys::NtTerminateProcess(handle, NtStatus::Success) != NtStatus::InvalidHandle ||
        ps::detail::bootstrapProcessObjectCount() != objects) return false;
    Handle event{};
    if (sys::NtCreateEvent(&event, 1, nullptr, 0, false) != NtStatus::Success ||
        sys::NtTerminateProcess(event, NtStatus::Success) != NtStatus::ObjectTypeMismatch ||
        sys::NtClose(event) != NtStatus::Success) return false;
    if (mm::PfnDatabase::get().getActiveCount() != active ||
        ps::detail::createProcessHandle(1, handle, process) != NtStatus::Success) return false;
    ObjectExitState state{ke::KeGetCurrentThread()};
    if (ob::detail::currentHandleTable().duplicateHandle(handle, process->getHandleTable(), state.selfHandle) !=
        NtStatus::Success) return false;
    ps::EThread thread;
    auto& scheduler = ke::PriorityScheduler::get();
    if (thread.initialize(*process, 16384, objectTerminatingEntry, &state) != NtStatus::Success ||
        thread.ready() != NtStatus::Success) return false;
    auto* next = scheduler.selectNextThread();
    if (next != thread.kernelThread()) return false;
    const auto previous = ke::KfRaiseIrql(ke::DISPATCH_LEVEL);
    const bool switched = ke::KiSwapContext(state.boot, next);
    ke::KeLowerIrql(previous);
    if (!switched || !state.ran || !process->isTerminating() ||
        process->exitStatus() != NtStatus::AccessDenied || next->state != ke::ThreadState::Terminated ||
        thread.reap() != NtStatus::Success || process->getDirectoryTableBase() ||
        ps::detail::bootstrapProcessObjectCount() != objects + 1 ||
        ps::detail::referenceProcessHandle(handle, 0, resolved, reference) != NtStatus::Success ||
        resolved != process || resolved->exitStatus() != NtStatus::AccessDenied) return false;
    ob::ObDereferenceObject(reference);
    return sys::NtClose(handle) == NtStatus::Success &&
        ps::detail::bootstrapProcessObjectCount() == objects &&
        mm::PfnDatabase::get().getActiveCount() == active;
}
}

extern "C" bool MicantTestPsThreadLifecycle() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto& scheduler = PriorityScheduler::get();
    auto* boot = KeGetCurrentThread();
    if (!boot || scheduler.getReadyThreadCount()) return false;
    const size_t active = mm::PfnDatabase::get().getActiveCount();
    const uintptr_t bootEntryStack = hal::amd64::bootstrapThreadSyscallStack();
    {
        ps::EProcess process;
        ps::EThread thread;
        if (thread.initialize(process, 1, threadEntry, nullptr) != NtStatus::InvalidParameter ||
            thread.initialize(process, 16384, threadEntry, nullptr) != NtStatus::InvalidParameter ||
            process.initializeAddressSpace() != NtStatus::Success) return false;
        State state{&process, boot, false, &thread};
        ps::EThread cancelled;
        if (cancelled.cancelBeforeStart() != NtStatus::InvalidParameter ||
            cancelled.initialize(process, 16384, threadEntry, &state) != NtStatus::Success) return false;
        KAPC pending{};
        pending.inserted = true;
        cancelled.kernelThread()->kernelApcList.push_back(&pending);
        if (cancelled.reap() != NtStatus::NotImplemented) return false;
        cancelled.kernelThread()->kernelApcList.pop_front();
        pending.inserted = false;
        if (cancelled.ready() != NtStatus::Success || scheduler.getReadyThreadCount() != 1 ||
            cancelled.cancelBeforeStart() != NtStatus::Success || scheduler.getReadyThreadCount() ||
            cancelled.cancelBeforeStart() != NtStatus::InvalidParameter ||
            cancelled.ready() != NtStatus::InvalidParameter || cancelled.reap() != NtStatus::Success ||
            state.ran) return false;
        if (cancelled.initialize(process, 16384, threadEntry, &state) != NtStatus::Success ||
            cancelled.cancelBeforeStart() != NtStatus::Success ||
            cancelled.reap() != NtStatus::Success) return false;
        if (thread.initialize(process, 16384, threadEntry, &state) != NtStatus::Success ||
            process.closeAddressSpace() != NtStatus::InvalidParameter ||
            thread.ready() != NtStatus::Success || thread.ready() != NtStatus::InvalidParameter ||
            thread.reap() != NtStatus::InvalidParameter) return false;
        auto* next = scheduler.selectNextThread();
        if (next != thread.kernelThread() || thread.cancelBeforeStart() != NtStatus::InvalidParameter)
            return false;
        const KIRQL previous = KfRaiseIrql(DISPATCH_LEVEL);
        const bool switched = KiSwapContext(boot, next);
        KeLowerIrql(previous);
        if (!switched || !state.ran || KeGetCurrentThread() != boot ||
            hal::amd64::bootstrapThreadSyscallStack() != bootEntryStack ||
            next->state != ThreadState::Terminated || scheduler.getReadyThreadCount() ||
            thread.reap() != NtStatus::Success || process.closeAddressSpace() != NtStatus::Success)
            return false;
    }
    {
        ps::EProcess process;
        ps::EThread thread;
        ps::EThread other;
        if (process.initializeAddressSpace() != NtStatus::Success) return false;
        State state{&process, boot, false, &thread, &other};
        if (other.initialize(process, 16384, threadEntry, &state) != NtStatus::Success ||
            thread.initialize(process, 16384, terminatingEntry, &state) != NtStatus::Success ||
            thread.ready() != NtStatus::Success) return false;
        auto* next = scheduler.selectNextThread();
        if (next != thread.kernelThread()) return false;
        const KIRQL previous = KfRaiseIrql(DISPATCH_LEVEL);
        const bool switched = KiSwapContext(boot, next);
        KeLowerIrql(previous);
        KThread late;
        if (!switched || !state.ran || !process.isTerminating() ||
            process.exitStatus() != NtStatus::AccessDenied || next->state != ThreadState::Terminated ||
            process.attachThread(late) != NtStatus::InvalidParameter ||
            process.closeAddressSpace() != NtStatus::InvalidParameter ||
            thread.reap() != NtStatus::Success || process.closeAddressSpace() != NtStatus::Success)
            return false;
    }
    return mm::PfnDatabase::get().getActiveCount() == active && testProcessObjectLifetime();
}
