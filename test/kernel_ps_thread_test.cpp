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
    micant::ke::KAPC firstApc{};
    micant::ke::KAPC secondApc{};
    micant::ke::KAPC normalKernelApc{};
    micant::ke::KAPC specialKernelApc{};
    micant::ke::KAPC crossThreadApc{};
    micant::ke::KAPC* ownedApc{};
    unsigned kernelCount{};
    unsigned rundownCount{};
    bool rundownInvalid{};
};

void rundownApc(micant::ke::KAPC* apc) {
    auto& state = *static_cast<State*>(apc->normalContext);
    if (micant::ke::KeGetCurrentThread() != state.owner->kernelThread() ||
        micant::ke::KeGetCurrentIrql() != micant::ke::PASSIVE_LEVEL || apc->inserted ||
        micant::ke::KeInsertQueueApc(apc) || micant::ke::detail::rundownCurrentUserApcs())
        state.rundownInvalid = true;
    const bool owned = apc == state.ownedApc;
    state.rundownCount = state.rundownCount * 10 +
        (apc == &state.firstApc ? 1 : apc == &state.secondApc ? 2 :
         apc == state.ownedApc ? 3 : 4);
    if (owned) {
        state.ownedApc = nullptr;
        delete apc;
    }
}

void kernelApc(micant::ke::KAPC*, micant::ke::PKNORMAL_ROUTINE* normal,
               void** context, void**, void**) {
    auto& state = *static_cast<State*>(*context);
    if (micant::ke::KeGetCurrentThread() != state.owner->kernelThread() ||
        micant::ke::KeGetCurrentIrql() != micant::ke::APC_LEVEL) state.rundownInvalid = true;
    state.kernelCount = state.kernelCount * 10 + (*normal ? 1 : 2);
}

void normalApc(void* context, void*, void*) {
    auto& state = *static_cast<State*>(context);
    if (micant::ke::KeGetCurrentThread() != state.owner->kernelThread() ||
        micant::ke::KeGetCurrentIrql() != micant::ke::PASSIVE_LEVEL) state.rundownInvalid = true;
    state.kernelCount = state.kernelCount * 10 + 3;
}

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
    micant::ke::KeInitializeApc(&state.normalKernelApc, current->tid, kernelApc, nullptr, normalApc, 0, &state);
    micant::ke::KeInitializeApc(&state.specialKernelApc, current->tid, kernelApc, nullptr, nullptr, 0, &state);
    micant::ke::KeInitializeApc(&state.firstApc, current->tid, nullptr, rundownApc, nullptr, 1, &state);
    micant::ke::KeInitializeApc(&state.secondApc, current->tid, nullptr, rundownApc, nullptr, 1, &state);
    state.ownedApc = new (std::nothrow_t{}) micant::ke::KAPC{};
    if (!state.ownedApc) __builtin_trap();
    micant::ke::KeInitializeApc(state.ownedApc, current->tid, nullptr, rundownApc, nullptr, 1, &state);
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (!micant::ke::KeInsertQueueApc(&state.normalKernelApc) ||
        !micant::ke::KeInsertQueueApc(&state.specialKernelApc) ||
        !micant::ke::KeInsertQueueApc(&state.firstApc) ||
        !micant::ke::KeInsertQueueApc(&state.secondApc) ||
        !micant::ke::KeInsertQueueApc(state.ownedApc)) __builtin_trap();
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    state.ran = true;
}

void terminatingEntry(void* argument) noexcept {
    auto& state = *static_cast<State*>(argument);
    using namespace micant::ke;
    KeEnterCriticalRegion();
    const auto criticalResult = micant::sys::NtTerminateProcess(
        micant::InvalidHandleValue, micant::NtStatus::AccessDenied);
    KeLeaveCriticalRegion();
    KeEnterGuardedRegion();
    const auto guardedResult = micant::sys::NtTerminateProcess(
        micant::InvalidHandleValue, micant::NtStatus::AccessDenied);
    KeLeaveGuardedRegion();
    if (criticalResult != micant::NtStatus::NotImplemented ||
        guardedResult != micant::NtStatus::NotImplemented || state.process->isTerminating())
        __builtin_trap();
    uintptr_t address{};
    size_t size = micant::mm::PageSize4KB;
    if (!state.other ||
        micant::sys::NtAllocateVirtualMemory(micant::InvalidHandleValue, &address, 0, &size,
            micant::mm::MEM_RESERVE | micant::mm::MEM_COMMIT, micant::mm::PAGE_READWRITE) != micant::NtStatus::Success ||
        !address || state.process->isTerminating() || state.process->exitStatus() != micant::NtStatus::Pending ||
        !micant::ke::PriorityScheduler::get().readyKThread(state.boot)) __builtin_trap();
    // Ensure the unstarted peer exits before the bootstrap observer resumes.
    state.other->kernelThread()->priority = state.boot->priority + 1;
    state.ran = true;
    (void)micant::sys::NtTerminateProcess(micant::InvalidHandleValue, micant::NtStatus::AccessDenied);
    __builtin_trap();
}

struct ObjectExitState {
    micant::ke::KThread* boot{};
    micant::Handle selfHandle{};
    unsigned rundownCount{};
    bool ran{};
};

void objectExitRundown(micant::ke::KAPC* apc) {
    auto& state = *static_cast<ObjectExitState*>(apc->normalContext);
    if (apc->inserted || micant::ke::KeGetCurrentIrql() != micant::ke::PASSIVE_LEVEL ||
        micant::ke::KeInsertQueueApc(apc)) __builtin_trap();
    ++state.rundownCount;
}

void objectTerminatingEntry(void* argument) noexcept {
    auto& state = *static_cast<ObjectExitState*>(argument);
    auto* current = micant::ke::KeGetCurrentThread();
    micant::ke::KAPC pending{};
    micant::ke::KeInitializeApc(&pending, current->tid, nullptr, objectExitRundown, nullptr, 1, &state);
    if (!micant::ke::KeInsertQueueApc(&pending) ||
        micant::sys::NtTerminateProcess(state.selfHandle, micant::NtStatus::AccessDenied) !=
            micant::NtStatus::Unsuccessful || !pending.inserted ||
        current->userApcList.front() != &pending) __builtin_trap();
    if (!micant::ke::PriorityScheduler::get().readyKThread(state.boot)) __builtin_trap();
    state.ran = true;
    (void)micant::sys::NtTerminateProcess(state.selfHandle, micant::NtStatus::AccessDenied);
    __builtin_trap();
}

struct ProcessWaitState {
    micant::ke::KThread* boot{};
    micant::Handle handle{};
    bool finished{};
};

alignas(16) uint8_t processWaitStack[16384]{};

[[noreturn]] void processWaiter(void* argument) noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto& state = *static_cast<ProcessWaitState*>(argument);
    KeLowerIrql(PASSIVE_LEVEL);
    if (!PriorityScheduler::get().readyKThread(state.boot) ||
        sys::NtWaitForSingleObject(state.handle, false, nullptr) != NtStatus::Success)
        __builtin_trap();
    state.finished = true;
    auto* current = KeGetCurrentThread();
    current->state = ThreadState::Terminated;
    KfRaiseIrql(DISPATCH_LEVEL);
    if (!KiSwapContext(current, state.boot)) __builtin_trap();
    __builtin_trap();
}

bool testThreadObjectLifetime() noexcept {
    using namespace micant;
    using namespace micant::ke;
    const auto objects = ps::detail::bootstrapThreadObjectCount();
    ps::EProcess process;
    if (process.initializeAddressSpace() != NtStatus::Success) return false;
    State state{&process, KeGetCurrentThread()};
    Handle handle{};
    ps::EThread* thread{};
    if (ps::detail::createThreadHandle(process, 16384, threadEntry, &state, ob::SYNCHRONIZE,
                                       handle, thread) != NtStatus::Success || !thread ||
        ps::detail::bootstrapThreadObjectCount() != objects + 1) return false;
    if (sys::NtTerminateThread(handle, NtStatus::AccessDenied) != NtStatus::AccessDenied ||
        sys::NtResumeThread(handle, nullptr) != NtStatus::AccessDenied ||
        thread->kernelThread()->terminationRequested) return false;
    state.owner = thread;
    KeInitializeApc(&state.crossThreadApc, thread->kernelThread()->tid,
                    nullptr, rundownApc, nullptr, 1, &state);
    if (!KeInsertQueueApc(&state.crossThreadApc) || KeInsertQueueApc(&state.crossThreadApc))
        return false;
    LargeInteger poll{};
    ProcessWaitState waiterState{state.boot};
    if (sys::NtWaitForSingleObject(handle, false, &poll) != NtStatus::Timeout ||
        ob::detail::currentHandleTable().duplicateHandle(handle, ob::detail::currentHandleTable(),
                                                          waiterState.handle) != NtStatus::Success)
        return false;
    KThread waiter;
    waiter.directoryTableBase = g_CurrentCpu.currentCr3;
    if (!detail::prepareBootstrapThread(waiter, processWaitStack, sizeof(processWaitStack),
                                        processWaiter, &waiterState)) return false;
    const auto waitIrql = KfRaiseIrql(DISPATCH_LEVEL);
    const bool waited = KiSwapContext(state.boot, &waiter);
    KeLowerIrql(waitIrql);
    if (!waited || waiter.state != ThreadState::Waiting || waiterState.finished ||
        sys::NtClose(waiterState.handle) != NtStatus::Success || thread->ready() != NtStatus::Success)
        return false;
    auto& scheduler = PriorityScheduler::get();
    auto* next = scheduler.selectNextThread();
    if (next != thread->kernelThread()) return false;
    const auto old = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = KiSwapContext(state.boot, next);
    KeLowerIrql(old);
    if (!switched || !state.ran || state.rundownInvalid || state.ownedApc ||
        state.kernelCount != 213 || state.rundownCount != 4123 || state.crossThreadApc.inserted ||
        KeInsertQueueApc(&state.crossThreadApc) ||
        sys::NtWaitForSingleObject(handle, false, &poll) != NtStatus::Success) return false;
    auto* wake = scheduler.selectNextThread();
    if (wake != &waiter) return false;
    const auto wakeIrql = KfRaiseIrql(DISPATCH_LEVEL);
    const bool woken = KiSwapContext(state.boot, wake);
    KeLowerIrql(wakeIrql);
    if (!woken || !waiterState.finished || waiter.state != ThreadState::Terminated ||
        scheduler.getReadyThreadCount() || thread->reap() != NtStatus::Success ||
        thread->initialize(process, 16384, threadEntry, &state) != NtStatus::InvalidParameter ||
        sys::NtWaitForSingleObject(handle, false, &poll) != NtStatus::Success ||
        sys::NtWaitForSingleObject(handle, false, &poll) != NtStatus::Success ||
        sys::NtClose(handle) != NtStatus::Success ||
        ps::detail::bootstrapThreadObjectCount() != objects) return false;

    State closedState{&process, KeGetCurrentThread()};
    if (ps::detail::createThreadHandle(process, 16384, threadEntry, &closedState, ob::SYNCHRONIZE,
                                       handle, thread) != NtStatus::Success || !thread) return false;
    closedState.owner = thread;
    if (sys::NtClose(handle) != NtStatus::Success ||
        ps::detail::bootstrapThreadObjectCount() != objects + 1 ||
        thread->cancelBeforeStart() != NtStatus::Success || thread->reap() != NtStatus::Success)
        return false;
    // The final lifecycle reference was released by reap; thread is no longer
    // a valid borrowed pointer after this point.
    if (ps::detail::bootstrapThreadObjectCount() != objects) return false;
    return process.closeAddressSpace() == NtStatus::Success;
}

bool testLastThreadExitAndDeferredReap() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto* boot = KeGetCurrentThread();
    auto& scheduler = PriorityScheduler::get();
    const auto active = mm::PfnDatabase::get().getActiveCount();
    const auto processObjects = ps::detail::bootstrapProcessObjectCount();
    const auto threadObjects = ps::detail::bootstrapThreadObjectCount();
    Handle processHandle{}, threadHandle{};
    ps::EProcess* process{};
    ps::EThread* thread{};
    if (!boot || scheduler.getReadyThreadCount() ||
        ps::detail::createProcessHandle(ob::SYNCHRONIZE, processHandle, process) != NtStatus::Success)
        return false;
    State state{process, boot};
    if (ps::detail::createThreadHandle(*process, 16384, threadEntry, &state, ob::SYNCHRONIZE,
                                       threadHandle, thread) != NtStatus::Success) return false;
    state.owner = thread;
    if (thread->ready() != NtStatus::Success) return false;
    auto* next = scheduler.selectNextThread();
    const auto old = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = next && KiSwapContext(boot, next);
    KeLowerIrql(old);
    LargeInteger poll{};
    if (!switched || !state.ran || state.rundownCount != 123 || state.rundownInvalid ||
        !process->isTerminating() || process->exitStatus() != NtStatus::Success ||
        sys::NtWaitForSingleObject(threadHandle, false, &poll) != NtStatus::Success ||
        thread->reap() != NtStatus::Success) return false;
    if (thread->kernelThread() || thread->exitStatus() != NtStatus::Success ||
        process->getDirectoryTableBase() ||
        sys::NtWaitForSingleObject(processHandle, false, &poll) != NtStatus::Success ||
        sys::NtClose(threadHandle) != NtStatus::Success ||
        sys::NtClose(processHandle) != NtStatus::Success) return false;
    return !scheduler.getReadyThreadCount() &&
        ps::detail::bootstrapProcessObjectCount() == processObjects &&
        ps::detail::bootstrapThreadObjectCount() == threadObjects &&
        mm::PfnDatabase::get().getActiveCount() == active;
}

struct RemoteWaitState {
    micant::ke::KThread* boot{};
    micant::Handle event{};
    micant::ke::KThread* target{};
    unsigned kernelCalls{};
    unsigned userRundowns{};
    bool waiting{};
    bool userWait{true};
};

void remoteKernelApc(micant::ke::KAPC*, micant::ke::PKNORMAL_ROUTINE*, void** context,
                     void**, void**) {
    auto& state = *static_cast<RemoteWaitState*>(*context);
    if (micant::ke::KeGetCurrentThread() != state.target ||
        micant::ke::KeGetCurrentIrql() != micant::ke::APC_LEVEL) __builtin_trap();
    ++state.kernelCalls;
}

void remoteUserRundown(micant::ke::KAPC* apc) {
    auto& state = *static_cast<RemoteWaitState*>(apc->normalContext);
    if (micant::ke::KeGetCurrentThread() != state.target || apc->inserted ||
        micant::ke::KeGetCurrentIrql() != micant::ke::PASSIVE_LEVEL ||
        micant::ke::KeInsertQueueApc(apc)) __builtin_trap();
    ++state.userRundowns;
}

void remoteWaitEntry(void* argument) noexcept {
    auto& state = *static_cast<RemoteWaitState*>(argument);
    if (!micant::ke::PriorityScheduler::get().readyKThread(state.boot)) __builtin_trap();
    state.waiting = true;
    // Model a user or kernel non-alertable wait using the same dispatcher.
    micant::ke::KeGetCurrentThread()->previousUserMode = state.userWait;
    (void)micant::sys::NtWaitForSingleObject(state.event, false, nullptr);
    // Model the syscall/interrupt return boundary after the wait has released
    // its reference and the surrounding kernel operation has unwound.
    __asm__ __volatile__("cli" : : : "memory");
    micant::ke::detail::serviceUserReturnTermination();
    __builtin_trap();
}

alignas(16) uint8_t remoteLauncherStack[16384]{};
[[noreturn]] void remoteLauncher(void* argument) noexcept {
    using namespace micant::ke;
    auto* boot = static_cast<KThread*>(argument);
    KeLowerIrql(PASSIVE_LEVEL);
    if (!PriorityScheduler::get().readyKThread(boot)) __builtin_trap();
    auto* current = KeGetCurrentThread();
    current->state = ThreadState::Terminated;
    KfRaiseIrql(DISPATCH_LEVEL);
    auto* next = PriorityScheduler::get().selectNextThread();
    if (!next || !KiSwapContext(current, next)) __builtin_trap();
    __builtin_trap();
}

bool testCancelledThreadApcRundown() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto* boot = KeGetCurrentThread();
    auto& scheduler = PriorityScheduler::get();
    if (!boot || scheduler.getReadyThreadCount()) return false;
    const auto active = mm::PfnDatabase::get().getActiveCount();
    ps::EProcess process;
    ps::EThread thread;
    State state{&process, boot, false, &thread};
    if (process.initializeAddressSpace() != NtStatus::Success ||
        thread.initialize(process, 16384, threadEntry, &state) != NtStatus::Success) return false;
    KeInitializeApc(&state.crossThreadApc, thread.kernelThread()->tid,
                    nullptr, rundownApc, nullptr, 1, &state);
    KAPC borrowed{};
    KeInitializeApc(&borrowed, thread.kernelThread()->tid, nullptr, nullptr, nullptr, 1, nullptr);
    if (!KeInsertQueueApc(&state.crossThreadApc) || !KeInsertQueueApc(&borrowed) ||
        thread.cancelBeforeStart() != NtStatus::Success ||
        thread.cancelBeforeStart() != NtStatus::InvalidParameter ||
        thread.reap() != NtStatus::InvalidParameter || !thread.kernelThread()->readyQueued) return false;
    KThread launcher;
    launcher.directoryTableBase = g_CurrentCpu.currentCr3;
    if (!detail::prepareBootstrapThread(launcher, remoteLauncherStack, sizeof(remoteLauncherStack),
                                        remoteLauncher, boot)) return false;
    const auto old = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = KiSwapContext(boot, &launcher);
    KeLowerIrql(old);
    if (!switched || state.ran || state.rundownInvalid || state.rundownCount != 4 ||
        state.crossThreadApc.inserted || borrowed.inserted || borrowed.queueNext ||
        thread.kernelThread()->state != ThreadState::Terminated ||
        thread.reap() != NtStatus::Success || process.closeAddressSpace() != NtStatus::Success)
        return false;
    return !scheduler.getReadyThreadCount() && mm::PfnDatabase::get().getActiveCount() == active;
}

bool testNativeThreadTermination() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto* boot = KeGetCurrentThread();
    if (!boot || PriorityScheduler::get().getReadyThreadCount()) return false;
    const auto pages = mm::PfnDatabase::get().getActiveCount();
    ps::EProcess process;
    if (process.initializeAddressSpace() != NtStatus::Success) return false;
    State state{&process, boot};
    Handle handle{};
    ps::EThread* thread{};
    if (ps::detail::createThreadHandle(process, 16384, threadEntry, &state,
            ob::SYNCHRONIZE | 1, handle, thread) != NtStatus::Success || !thread) return false;
    state.owner = thread;
    KeInitializeApc(&state.crossThreadApc, thread->kernelThread()->tid,
                    nullptr, rundownApc, nullptr, 1, &state);
    if (!KeInsertQueueApc(&state.crossThreadApc) ||
        sys::NtTerminateThread(handle, NtStatus::AccessDenied) != NtStatus::Success ||
        sys::NtTerminateThread(handle, NtStatus::Success) != NtStatus::Success ||
        !thread->kernelThread()->readyQueued) return false;
    KThread launcher;
    launcher.directoryTableBase = g_CurrentCpu.currentCr3;
    if (!detail::prepareBootstrapThread(launcher, remoteLauncherStack, sizeof(remoteLauncherStack),
                                        remoteLauncher, boot)) return false;
    const auto previous = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = KiSwapContext(boot, &launcher);
    KeLowerIrql(previous);
    LargeInteger poll{};
    if (!switched || state.ran || state.rundownInvalid || state.rundownCount != 4 ||
        thread->exitStatus() != NtStatus::AccessDenied ||
        sys::NtWaitForSingleObject(handle, false, &poll) != NtStatus::Success ||
        thread->reap() != NtStatus::Success ||
        sys::NtTerminateThread(handle, NtStatus::Success) != NtStatus::Success ||
        thread->exitStatus() != NtStatus::AccessDenied || sys::NtClose(handle) != NtStatus::Success ||
        process.closeAddressSpace() != NtStatus::Success) return false;
    return !PriorityScheduler::get().getReadyThreadCount() && mm::PfnDatabase::get().getActiveCount() == pages;
}

struct DeferredExitState {
    micant::ke::KThread* boot{};
    bool entered{};
};

void deferredExitEntry(void* argument) noexcept {
    using namespace micant::ke;
    auto& state = *static_cast<DeferredExitState*>(argument);
    auto* current = KeGetCurrentThread();
    if (!PriorityScheduler::get().readyKThread(state.boot)) __builtin_trap();
    state.entered = true;
    if (!detail::blockCurrentThread() || !current->terminationRequested || !current->serviceTermination)
        __builtin_trap();
    // This models the passive APC boundary without enabling early bootstrap IRQs.
    current->serviceTermination(current->terminationContext);
    __builtin_trap();
}

bool testDeferredRemoteExit() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto* boot = KeGetCurrentThread();
    auto& scheduler = PriorityScheduler::get();
    const auto active = mm::PfnDatabase::get().getActiveCount();
    Handle processHandle{};
    ps::EProcess* process{};
    if (!boot || scheduler.getReadyThreadCount() ||
        ps::detail::createProcessHandle(1 | ob::SYNCHRONIZE, processHandle, process) != NtStatus::Success)
        return false;
    ps::EThread thread;
    DeferredExitState state{boot};
    if (thread.initialize(*process, 16384, deferredExitEntry, &state) != NtStatus::Success ||
        thread.ready() != NtStatus::Success) return false;
    auto* next = scheduler.selectNextThread();
    const auto old = KfRaiseIrql(DISPATCH_LEVEL);
    const bool blocked = next && KiSwapContext(boot, next);
    KeLowerIrql(old);
    if (!blocked || !state.entered || thread.kernelThread()->state != ThreadState::Waiting ||
        sys::NtTerminateProcess(processHandle, NtStatus::AccessDenied) != NtStatus::Success ||
        !thread.kernelThread()->terminationRequested ||
        !scheduler.readyKThread(thread.kernelThread())) return false;
    KThread launcher;
    launcher.directoryTableBase = g_CurrentCpu.currentCr3;
    if (!detail::prepareBootstrapThread(launcher, remoteLauncherStack, sizeof(remoteLauncherStack),
                                        remoteLauncher, boot)) return false;
    const auto launchIrql = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = KiSwapContext(boot, &launcher);
    KeLowerIrql(launchIrql);
    LargeInteger poll{};
    if (!switched || thread.kernelThread()->state != ThreadState::Terminated ||
        thread.reap() != NtStatus::Success || process->exitStatus() != NtStatus::AccessDenied ||
        sys::NtWaitForSingleObject(processHandle, false, &poll) != NtStatus::Success ||
        sys::NtClose(processHandle) != NtStatus::Success) return false;
    return !scheduler.getReadyThreadCount() && mm::PfnDatabase::get().getActiveCount() == active;
}

struct ApcSignalState {
    micant::ke::KThread* boot{};
    micant::ke::KThread* target{};
    micant::Handle event{};
    bool completed{};
};

void signalDuringApc(micant::ke::KAPC*, micant::ke::PKNORMAL_ROUTINE*, void** context,
                     void**, void**) {
    auto& state = *static_cast<ApcSignalState*>(*context);
    if (micant::ke::KeGetCurrentThread() != state.target ||
        micant::ke::KeGetCurrentIrql() != micant::ke::APC_LEVEL ||
        micant::sys::NtSetEvent(state.event, nullptr) != micant::NtStatus::Success) __builtin_trap();
}

void apcSignalWaitEntry(void* argument) noexcept {
    auto& state = *static_cast<ApcSignalState*>(argument);
    if (!micant::ke::PriorityScheduler::get().readyKThread(state.boot) ||
        micant::sys::NtWaitForSingleObject(state.event, false, nullptr) != micant::NtStatus::Success)
        __builtin_trap();
    state.completed = true;
}

bool testApcSignalDuringWait() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto* boot = KeGetCurrentThread();
    auto& scheduler = PriorityScheduler::get();
    const auto active = mm::PfnDatabase::get().getActiveCount();
    ps::EProcess process;
    ps::EThread thread;
    Handle source{};
    ApcSignalState state{boot};
    if (!boot || scheduler.getReadyThreadCount() || process.initializeAddressSpace() != NtStatus::Success ||
        sys::NtCreateEvent(&source, ob::SYNCHRONIZE | 2, nullptr, 1, false) != NtStatus::Success ||
        ob::detail::currentHandleTable().duplicateHandle(source, process.getHandleTable(), state.event) !=
            NtStatus::Success ||
        thread.initialize(process, 16384, apcSignalWaitEntry, &state) != NtStatus::Success ||
        thread.ready() != NtStatus::Success) return false;
    state.target = thread.kernelThread();
    auto* next = scheduler.selectNextThread();
    const auto waitIrql = KfRaiseIrql(DISPATCH_LEVEL);
    const bool blocked = next && KiSwapContext(boot, next);
    KeLowerIrql(waitIrql);
    KAPC apc{};
    KeInitializeApc(&apc, state.target->tid, signalDuringApc, nullptr, nullptr, 0, &state);
    if (!blocked || state.target->state != ThreadState::Waiting || !KeInsertQueueApc(&apc)) return false;
    KThread launcher;
    launcher.directoryTableBase = g_CurrentCpu.currentCr3;
    if (!detail::prepareBootstrapThread(launcher, remoteLauncherStack, sizeof(remoteLauncherStack),
                                        remoteLauncher, boot)) return false;
    const auto old = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = KiSwapContext(boot, &launcher);
    KeLowerIrql(old);
    LargeInteger poll{};
    if (!switched || !state.completed || apc.inserted || thread.kernelThread()->state != ThreadState::Terminated ||
        sys::NtWaitForSingleObject(source, false, &poll) != NtStatus::Timeout ||
        thread.reap() != NtStatus::Success || process.closeAddressSpace() != NtStatus::Success ||
        sys::NtClose(source) != NtStatus::Success) return false;
    return !scheduler.getReadyThreadCount() && mm::PfnDatabase::get().getActiveCount() == active;
}

bool testRemoteUnstartedThreads() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto* boot = KeGetCurrentThread();
    auto& scheduler = PriorityScheduler::get();
    const auto active = mm::PfnDatabase::get().getActiveCount();
    Handle processHandle{};
    ps::EProcess* process{};
    if (!boot || scheduler.getReadyThreadCount() ||
        ps::detail::createProcessHandle(1 | ob::SYNCHRONIZE, processHandle, process) != NtStatus::Success)
        return false;
    ps::EThread first, second;
    State firstState{process, boot, false, &first};
    State secondState{process, boot, false, &second};
    if (first.initialize(*process, 16384, threadEntry, &firstState) != NtStatus::Success ||
        second.initialize(*process, 16384, threadEntry, &secondState) != NtStatus::Success ||
        second.ready() != NtStatus::Success) return false;
    KeInitializeApc(&firstState.crossThreadApc, first.kernelThread()->tid,
                    nullptr, rundownApc, nullptr, 1, &firstState);
    KeInitializeApc(&firstState.normalKernelApc, first.kernelThread()->tid,
                    kernelApc, nullptr, normalApc, 0, &firstState);
    if (!KeInsertQueueApc(&firstState.crossThreadApc) ||
        !KeInsertQueueApc(&firstState.normalKernelApc) ||
        sys::NtTerminateProcess(processHandle, NtStatus::AccessDenied) != NtStatus::Success ||
        !first.kernelThread()->readyQueued || !second.kernelThread()->readyQueued) return false;
    KThread launcher;
    launcher.directoryTableBase = g_CurrentCpu.currentCr3;
    if (!detail::prepareBootstrapThread(launcher, remoteLauncherStack, sizeof(remoteLauncherStack),
                                        remoteLauncher, boot)) return false;
    const auto old = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = KiSwapContext(boot, &launcher);
    KeLowerIrql(old);
    LargeInteger poll{};
    if (!switched || firstState.ran || secondState.ran || firstState.rundownInvalid ||
        firstState.rundownCount != 4 || firstState.kernelCount != 13 ||
        firstState.crossThreadApc.inserted || firstState.normalKernelApc.inserted ||
        first.kernelThread()->state != ThreadState::Terminated ||
        second.kernelThread()->state != ThreadState::Terminated ||
        first.reap() != NtStatus::Success ||
        sys::NtWaitForSingleObject(processHandle, false, &poll) != NtStatus::Timeout ||
        second.reap() != NtStatus::Success ||
        sys::NtWaitForSingleObject(processHandle, false, &poll) != NtStatus::Success ||
        process->exitStatus() != NtStatus::AccessDenied ||
        sys::NtClose(processHandle) != NtStatus::Success) return false;
    return !scheduler.getReadyThreadCount() && mm::PfnDatabase::get().getActiveCount() == active;
}

bool testKernelWaitDefersTermination() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto* boot = KeGetCurrentThread();
    auto& scheduler = PriorityScheduler::get();
    const auto active = mm::PfnDatabase::get().getActiveCount();
    Handle processHandle{}, sourceEvent{};
    ps::EProcess* process{};
    if (!boot || scheduler.getReadyThreadCount() ||
        ps::detail::createProcessHandle(1 | ob::SYNCHRONIZE, processHandle, process) != NtStatus::Success ||
        sys::NtCreateEvent(&sourceEvent, ob::SYNCHRONIZE | 2, nullptr, 0, false) != NtStatus::Success)
        return false;
    RemoteWaitState state{boot};
    state.userWait = false;
    if (ob::detail::currentHandleTable().duplicateHandle(sourceEvent, process->getHandleTable(), state.event) !=
        NtStatus::Success) return false;
    ps::EThread thread;
    if (thread.initialize(*process, 16384, remoteWaitEntry, &state) != NtStatus::Success ||
        thread.ready() != NtStatus::Success) return false;
    state.target = thread.kernelThread();
    auto* next = scheduler.selectNextThread();
    const auto old = KfRaiseIrql(DISPATCH_LEVEL);
    const bool blocked = next && KiSwapContext(boot, next);
    KeLowerIrql(old);
    if (!blocked || !state.waiting || state.target->state != ThreadState::Waiting ||
        sys::NtTerminateProcess(processHandle, NtStatus::AccessDenied) != NtStatus::Success ||
        state.target->state != ThreadState::Waiting || !state.target->terminationRequested ||
        sys::NtSetEvent(sourceEvent, nullptr) != NtStatus::Success ||
        state.target->state != ThreadState::Ready) return false;
    KThread launcher;
    launcher.directoryTableBase = g_CurrentCpu.currentCr3;
    if (!detail::prepareBootstrapThread(launcher, remoteLauncherStack, sizeof(remoteLauncherStack),
                                        remoteLauncher, boot)) return false;
    const auto launchIrql = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = KiSwapContext(boot, &launcher);
    KeLowerIrql(launchIrql);
    LargeInteger poll{};
    if (!switched || state.target->state != ThreadState::Terminated ||
        thread.reap() != NtStatus::Success ||
        sys::NtWaitForSingleObject(processHandle, false, &poll) != NtStatus::Success ||
        sys::NtClose(sourceEvent) != NtStatus::Success || sys::NtClose(processHandle) != NtStatus::Success)
        return false;
    return !scheduler.getReadyThreadCount() && mm::PfnDatabase::get().getActiveCount() == active;
}

bool testRemoteProcessWait() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto* boot = KeGetCurrentThread();
    auto& scheduler = PriorityScheduler::get();
    const auto active = mm::PfnDatabase::get().getActiveCount();
    const auto objects = ps::detail::bootstrapProcessObjectCount();
    Handle processHandle{}, sourceEvent{};
    ps::EProcess* process{};
    if (!boot || scheduler.getReadyThreadCount() ||
        ps::detail::createProcessHandle(1 | ob::SYNCHRONIZE, processHandle, process) != NtStatus::Success ||
        sys::NtCreateEvent(&sourceEvent, ob::SYNCHRONIZE | 2, nullptr, 0, false) != NtStatus::Success)
        return false;
    RemoteWaitState state{boot};
    if (ob::detail::currentHandleTable().duplicateHandle(sourceEvent, process->getHandleTable(), state.event) !=
        NtStatus::Success) return false;
    ps::EThread thread;
    if (thread.initialize(*process, 16384, remoteWaitEntry, &state) != NtStatus::Success ||
        thread.ready() != NtStatus::Success) return false;
    state.target = thread.kernelThread();
    auto* next = scheduler.selectNextThread();
    const auto old = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = next && KiSwapContext(boot, next);
    KeLowerIrql(old);
    if (!switched || !state.waiting || thread.kernelThread()->state != ThreadState::Waiting) return false;
    KAPC apc{};
    KeInitializeApc(&apc, state.target->tid, remoteKernelApc, nullptr, nullptr, 0, &state);
    if (!KeInsertQueueApc(&apc) || state.target->state != ThreadState::Ready) return false;
    KThread apcLauncher;
    apcLauncher.directoryTableBase = g_CurrentCpu.currentCr3;
    if (!detail::prepareBootstrapThread(apcLauncher, remoteLauncherStack, sizeof(remoteLauncherStack),
                                        remoteLauncher, boot)) return false;
    const auto apcIrql = KfRaiseIrql(DISPATCH_LEVEL);
    const bool apcSwitched = KiSwapContext(boot, &apcLauncher);
    KeLowerIrql(apcIrql);
    KAPC userApc{};
    KeInitializeApc(&userApc, state.target->tid, nullptr, remoteUserRundown, nullptr, 1, &state);
    if (!apcSwitched || state.kernelCalls != 1 || apc.inserted ||
        state.target->state != ThreadState::Waiting || !KeInsertQueueApc(&userApc) ||
        sys::NtTerminateProcess(processHandle, NtStatus::AccessDenied) != NtStatus::Success ||
        !process->isTerminating() || thread.kernelThread()->state != ThreadState::Ready) return false;
    KThread launcher;
    launcher.directoryTableBase = g_CurrentCpu.currentCr3;
    if (!detail::prepareBootstrapThread(launcher, remoteLauncherStack, sizeof(remoteLauncherStack),
                                        remoteLauncher, boot)) return false;
    const auto launchIrql = KfRaiseIrql(DISPATCH_LEVEL);
    const bool launched = KiSwapContext(boot, &launcher);
    KeLowerIrql(launchIrql);
    LargeInteger poll{};
    if (!launched || state.userRundowns != 1 || userApc.inserted ||
        thread.kernelThread()->state != ThreadState::Terminated ||
        thread.reap() != NtStatus::Success || process->getDirectoryTableBase() ||
        process->exitStatus() != NtStatus::AccessDenied ||
        sys::NtWaitForSingleObject(processHandle, false, &poll) != NtStatus::Success ||
        sys::NtClose(sourceEvent) != NtStatus::Success ||
        sys::NtClose(processHandle) != NtStatus::Success) return false;
    return !scheduler.getReadyThreadCount() &&
        ps::detail::bootstrapProcessObjectCount() == objects &&
        mm::PfnDatabase::get().getActiveCount() == active;
}

struct SystemThreadState {
    micant::ke::KThread* boot{};
    bool ran{};
    bool leaveInitialRegion{};
};

__attribute__((noinline)) bool testPrivateProcessClone(micant::Handle parentHandle, micant::ps::EProcess& parent) noexcept {
    using namespace micant;
    uintptr_t original{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(original));
    if (!hal::amd64::switchProcessPagingRootByPhysical(parent.getDirectoryTableBase())) return false;
    uintptr_t base{};
    auto status = parent.getAddressSpace().allocate(base, 2 * mm::PageSize4KB,
        mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE);
    if (status == NtStatus::Success) {
        *reinterpret_cast<uint64_t*>(base) = 0x12345678;
        *reinterpret_cast<uint64_t*>(base + mm::PageSize4KB) = 0x87654321;
        uint32_t old{};
        status = parent.getAddressSpace().protect(base + mm::PageSize4KB, mm::PageSize4KB,
                                                  mm::PAGE_READONLY, &old);
    }
    if (!hal::amd64::switchProcessPagingRootByPhysical(original) || status != NtStatus::Success) return false;
    Handle cloneHandle{};
    if (sys::NtCreateProcessEx(&cloneHandle, ob::SYNCHRONIZE | 1, nullptr, parentHandle,
                               0, 0, 0, 0, false) != NtStatus::ProcessIsTerminating) return false;
    if (sys::NtCreateProcessEx(&cloneHandle, ob::SYNCHRONIZE | 1, nullptr, InvalidHandleValue,
                               0, 0, 0, 0, false) != NtStatus::Success) return false;
    ps::EProcess* clone{};
    ob::ObjectHeader* reference{};
    if (ps::detail::referenceProcessHandle(cloneHandle, ob::SYNCHRONIZE, clone, reference) != NtStatus::Success || !clone)
        return false;
    status = parent.getAddressSpace().clonePrivateTo(clone->getAddressSpace(), parent.getDirectoryTableBase(),
        clone->getDirectoryTableBase(), hal::amd64::switchProcessPagingRootByPhysical);
    if (status != NtStatus::Success || !hal::amd64::switchProcessPagingRootByPhysical(clone->getDirectoryTableBase()))
        return false;
    bool correct = *reinterpret_cast<uint64_t*>(base) == 0x12345678 &&
        *reinterpret_cast<uint64_t*>(base + mm::PageSize4KB) == 0x87654321 &&
        !hal::amd64::bootstrapUserRange(base + mm::PageSize4KB, 1, true);
    *reinterpret_cast<uint64_t*>(base) = 0xabcdef;
    if (!hal::amd64::switchProcessPagingRootByPhysical(parent.getDirectoryTableBase())) return false;
    correct = correct && *reinterpret_cast<uint64_t*>(base) == 0x12345678;
    if (!hal::amd64::switchProcessPagingRootByPhysical(original)) return false;
    ob::ObDereferenceObject(reference);
    return correct && sys::NtTerminateProcess(cloneHandle, NtStatus::Success) == NtStatus::Success &&
        sys::NtClose(cloneHandle) == NtStatus::Success;
}

void systemThreadEntry(void* argument) {
    using namespace micant;
    using namespace micant::ke;
    auto& state = *static_cast<SystemThreadState*>(argument);
    auto* current = KeGetCurrentThread();
    LargeInteger poll{};
    if (!current || KeGetCurrentIrql() != PASSIVE_LEVEL || current->kernelApcDisable != 1 ||
        current->tebAddress || sys::NtWaitForSingleObject(InvalidHandleValue, false, &poll) != NtStatus::Timeout ||
        sys::NtWaitForSingleObject(static_cast<Handle>(-2), false, &poll) != NtStatus::Timeout ||
        sys::NtTerminateThread(0, NtStatus::Success) != NtStatus::CantTerminateSelf ||
        sys::NtTerminateProcess(0, NtStatus::Success) != NtStatus::Success ||
        current->terminationRequested || current->owningProcess->isTerminating() ||
        !PriorityScheduler::get().readyKThread(state.boot)) __builtin_trap();
    ps::EProcess* parent{};
    ob::ObjectHeader* parentReference{};
    Handle limited{}, child{};
    if (ps::detail::referenceCurrentProcess(parent, parentReference) != NtStatus::Success ||
        !parent || !parent->isSystemProcess() ||
        ob::detail::currentHandleTable().createHandle(parentReference, limited, ob::SYNCHRONIZE, OBJ_INHERIT) != NtStatus::Success)
        __builtin_trap();
    ob::ObDereferenceObject(parentReference);
    ObjectAttributes childAttributes{};
    childAttributes.attributes = OBJ_INHERIT;
    if (sys::NtCreateProcessEx(&child, ob::SYNCHRONIZE | 1, nullptr, limited,
            0, 0, 0, 0, false) != NtStatus::AccessDenied ||
        sys::NtCreateProcessEx(&child, ob::SYNCHRONIZE | 1 | 0x80, &childAttributes, InvalidHandleValue,
            4, 0, 0, 0, false) != NtStatus::Success) __builtin_trap();
    ps::EProcess* childProcess{};
    ob::ObjectHeader* childReference{};
    if (ps::detail::referenceProcessHandle(child, ob::SYNCHRONIZE, childProcess, childReference) != NtStatus::Success ||
        childProcess->getHandleTable().lookup(limited) != parentReference || childProcess->getHandleTable().lookup(child) ||
        !testPrivateProcessClone(child, *childProcess)) __builtin_trap();
    ob::ObDereferenceObject(childReference);
    if (sys::NtClose(limited) != NtStatus::Success ||
        sys::NtWaitForSingleObject(child, false, &poll) != NtStatus::Timeout ||
        sys::NtTerminateProcess(child, NtStatus::AccessDenied) != NtStatus::Success ||
        sys::NtWaitForSingleObject(child, false, &poll) != NtStatus::Success ||
        sys::NtClose(child) != NtStatus::Success) __builtin_trap();
    if (state.leaveInitialRegion) {
        KeLeaveCriticalRegion();
        if (current->kernelApcDisable || current->systemInitialRegionActive) __builtin_trap();
    }
    state.ran = true;
    (void)ps::PsTerminateSystemThread(NtStatus::AccessDenied);
    __builtin_trap();
}

bool testSystemThreadCreation() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto* boot = KeGetCurrentThread();
    auto& scheduler = PriorityScheduler::get();
    if (!boot || scheduler.getReadyThreadCount() ||
        ps::PsTerminateSystemThread(NtStatus::Success) != NtStatus::InvalidParameter) return false;
    const auto threadObjects = ps::detail::bootstrapThreadObjectCount();
    SystemThreadState state{boot};
    Handle limited{};
    ps::EProcess* limitedProcess{};
    if (ps::detail::createProcessHandle(0, limited, limitedProcess) != NtStatus::Success ||
        ps::PsCreateSystemThread(&limited, ob::SYNCHRONIZE, nullptr, limited, nullptr,
                                 systemThreadEntry, &state) != NtStatus::AccessDenied ||
        sys::NtClose(limited) != NtStatus::Success) return false;
    Handle handle{};
    ClientId client{};
    if (ps::PsCreateSystemThread(&handle, ob::GENERIC_ALL, nullptr, 0, &client,
                                 systemThreadEntry, &state) != NtStatus::Success ||
        handle >= 0 || !client.uniqueProcess || !client.uniqueThread ||
        client.uniqueProcess == client.uniqueThread) return false;
    LargeInteger poll{};
    boot->previousUserMode = true;
    const auto userWait = sys::NtWaitForSingleObject(handle, false, &poll);
    const auto userClose = sys::NtClose(handle);
    constexpr uintptr_t kernelTag = uintptr_t{1} << (sizeof(Handle) * 8 - 1);
    const auto untagged = static_cast<Handle>(static_cast<uintptr_t>(handle) & ~kernelTag);
    const auto untaggedWait = sys::NtWaitForSingleObject(untagged, false, &poll);
    const auto untaggedClose = sys::NtClose(untagged);
    const auto previousModeClose = sys::ZwClose(handle);
    boot->previousUserMode = false;
    if (userWait != NtStatus::InvalidHandle || userClose != NtStatus::InvalidHandle ||
        untaggedWait != NtStatus::InvalidHandle || untaggedClose != NtStatus::InvalidHandle ||
        previousModeClose != NtStatus::InvalidHandle ||
        sys::NtTerminateThread(handle, NtStatus::Success) != NtStatus::AccessDenied ||
        sys::NtWaitForSingleObject(handle, false, &poll) != NtStatus::Timeout) return false;
    auto* next = scheduler.selectNextThread();
    const auto old = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = next && KiSwapContext(boot, next);
    KeLowerIrql(old);
    ps::EThread* thread{};
    ob::ObjectHeader* reference{};
    if (!switched || !state.ran ||
        ps::detail::referenceThreadHandle(handle, ob::SYNCHRONIZE, thread, reference) != NtStatus::Success ||
        !thread || thread->exitStatus() != NtStatus::AccessDenied ||
        sys::NtWaitForSingleObject(handle, false, &poll) != NtStatus::Success ||
        thread->reap() != NtStatus::Success) return false;
    if (thread->kernelThread() || thread->exitStatus() != NtStatus::AccessDenied) return false;
    ob::ObDereferenceObject(reference);
    if (sys::ZwClose(handle) != NtStatus::Success || scheduler.getReadyThreadCount() ||
        ps::detail::bootstrapThreadObjectCount() != threadObjects) return false;
    SystemThreadState leftRegion{boot};
    leftRegion.leaveInitialRegion = true;
    if (ps::PsCreateSystemThread(&handle, ob::SYNCHRONIZE, nullptr, 0, nullptr,
                                 systemThreadEntry, &leftRegion) != NtStatus::Success) return false;
    next = scheduler.selectNextThread();
    const auto secondIrql = KfRaiseIrql(DISPATCH_LEVEL);
    const bool secondSwitched = next && KiSwapContext(boot, next);
    KeLowerIrql(secondIrql);
    thread = nullptr;
    reference = nullptr;
    if (!secondSwitched || !leftRegion.ran ||
        ps::detail::referenceThreadHandle(handle, ob::SYNCHRONIZE, thread, reference) != NtStatus::Success ||
        !thread || thread->exitStatus() != NtStatus::AccessDenied || thread->reap() != NtStatus::Success)
        return false;
    ob::ObDereferenceObject(reference);
    return sys::ZwClose(handle) == NtStatus::Success && !scheduler.getReadyThreadCount() &&
        ps::detail::bootstrapThreadObjectCount() == threadObjects;
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

    LargeInteger poll{};
    if (ps::detail::createProcessHandle(1 | ob::SYNCHRONIZE, handle, process) != NtStatus::Success ||
        sys::NtTerminateProcess(handle, NtStatus::AccessDenied) != NtStatus::Success ||
        !process->isTerminating() || process->exitStatus() != NtStatus::AccessDenied ||
        process->getDirectoryTableBase() || process->initializeAddressSpace() != NtStatus::InvalidParameter ||
        sys::NtWaitForSingleObject(handle, false, &poll) != NtStatus::Success ||
        sys::NtClose(handle) != NtStatus::Success ||
        sys::NtTerminateProcess(handle, NtStatus::Success) != NtStatus::InvalidHandle ||
        ps::detail::bootstrapProcessObjectCount() != objects) return false;
    Handle event{};
    if (sys::NtCreateEvent(&event, 1, nullptr, 0, false) != NtStatus::Success ||
        sys::NtTerminateProcess(event, NtStatus::Success) != NtStatus::ObjectTypeMismatch ||
        sys::NtClose(event) != NtStatus::Success) return false;
    if (mm::PfnDatabase::get().getActiveCount() != active ||
        ps::detail::createProcessHandle(1 | ob::SYNCHRONIZE, handle, process) != NtStatus::Success) return false;
    if (sys::NtWaitForSingleObject(handle, false, &poll) != NtStatus::Timeout) return false;
    ObjectExitState state{ke::KeGetCurrentThread()};
    if (ob::detail::currentHandleTable().duplicateHandle(handle, process->getHandleTable(), state.selfHandle) !=
        NtStatus::Success) return false;
    ps::EThread thread;
    auto& scheduler = ke::PriorityScheduler::get();
    ProcessWaitState waiterState{state.boot};
    if (ob::detail::currentHandleTable().duplicateHandle(handle, ob::detail::currentHandleTable(),
                                                          waiterState.handle) != NtStatus::Success)
        return false;
    ke::KThread waiter;
    waiter.directoryTableBase = ke::g_CurrentCpu.currentCr3;
    if (!ke::detail::prepareBootstrapThread(waiter, processWaitStack, sizeof(processWaitStack),
                                            processWaiter, &waiterState)) return false;
    const auto waitIrql = ke::KfRaiseIrql(ke::DISPATCH_LEVEL);
    const bool waited = ke::KiSwapContext(state.boot, &waiter);
    ke::KeLowerIrql(waitIrql);
    if (!waited || waiter.state != ke::ThreadState::Waiting || waiterState.finished ||
        sys::NtClose(waiterState.handle) != NtStatus::Success ||
        thread.initialize(*process, 16384, objectTerminatingEntry, &state) != NtStatus::Success ||
        thread.ready() != NtStatus::Success) return false;
    auto* next = scheduler.selectNextThread();
    if (next != thread.kernelThread()) return false;
    const auto previous = ke::KfRaiseIrql(ke::DISPATCH_LEVEL);
    const bool switched = ke::KiSwapContext(state.boot, next);
    ke::KeLowerIrql(previous);
    if (!switched || !state.ran || state.rundownCount != 1 || !process->isTerminating() ||
        process->exitStatus() != NtStatus::AccessDenied || next->state != ke::ThreadState::Terminated ||
        thread.reap() != NtStatus::Success || process->getDirectoryTableBase() ||
        ps::detail::bootstrapProcessObjectCount() != objects + 1 ||
        ps::detail::referenceProcessHandle(handle, 0, resolved, reference) != NtStatus::Success ||
        resolved != process || resolved->exitStatus() != NtStatus::AccessDenied) return false;
    ob::ObDereferenceObject(reference);
    auto* resumed = scheduler.selectNextThread();
    if (resumed != &waiter) return false;
    const auto resumeIrql = ke::KfRaiseIrql(ke::DISPATCH_LEVEL);
    const bool resumedWait = ke::KiSwapContext(state.boot, resumed);
    ke::KeLowerIrql(resumeIrql);
    return resumedWait && waiterState.finished && waiter.state == ke::ThreadState::Terminated &&
        !scheduler.getReadyThreadCount() &&
        sys::NtWaitForSingleObject(handle, false, &poll) == NtStatus::Success &&
        sys::NtWaitForSingleObject(handle, false, &poll) == NtStatus::Success &&
        sys::NtClose(handle) == NtStatus::Success &&
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
        uint32_t previousSuspend{};
        if (cancelled.initialize(process, 16384, threadEntry, &state) != NtStatus::Success ||
            cancelled.suspendInitially() != NtStatus::Success ||
            cancelled.ready() != NtStatus::InvalidParameter ||
            cancelled.resumeInitial(previousSuspend) != NtStatus::Success || previousSuspend != 1 ||
            cancelled.kernelThread()->state != ThreadState::Ready ||
            cancelled.cancelBeforeStart() != NtStatus::Success ||
            cancelled.reap() != NtStatus::Success) return false;
        if (cancelled.initialize(process, 16384, threadEntry, &state) != NtStatus::Success)
            return false;
        cancelled.kernelThread()->kernelApcDisable = 1;
        if (cancelled.terminateOther(NtStatus::AccessDenied) != NtStatus::NotImplemented ||
            cancelled.kernelThread()->terminationRequested || cancelled.kernelThread()->readyQueued)
            return false;
        cancelled.kernelThread()->kernelApcDisable = 0;
        if (cancelled.terminateOther(NtStatus::AccessDenied) != NtStatus::Success ||
            cancelled.terminateOther(NtStatus::Success) != NtStatus::Success) return false;
        auto* terminating = scheduler.selectNextThread();
        if (terminating != cancelled.kernelThread()) return false;
        auto* previousIdle = g_CurrentCpu.idleThread;
        g_CurrentCpu.idleThread = boot;
        const auto terminationIrql = KfRaiseIrql(DISPATCH_LEVEL);
        const bool terminated = KiSwapContext(boot, terminating);
        KeLowerIrql(terminationIrql);
        g_CurrentCpu.idleThread = previousIdle;
        if (!terminated || state.ran || cancelled.exitStatus() != NtStatus::AccessDenied ||
            terminating->state != ThreadState::Terminated || cancelled.reap() != NtStatus::Success)
            return false;
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
        if (!switched || !state.ran || state.rundownInvalid || state.rundownCount != 123 || state.ownedApc ||
            state.kernelCount != 213 || state.normalKernelApc.inserted || state.specialKernelApc.inserted ||
            state.firstApc.inserted || state.secondApc.inserted || KeGetCurrentThread() != boot ||
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
            other.kernelThread()->state != ThreadState::Terminated || other.reap() != NtStatus::Success ||
            thread.reap() != NtStatus::Success || process.closeAddressSpace() != NtStatus::Success)
            return false;
    }
    return mm::PfnDatabase::get().getActiveCount() == active &&
        testThreadObjectLifetime() && testCancelledThreadApcRundown() && testNativeThreadTermination() &&
        testLastThreadExitAndDeferredReap() && testDeferredRemoteExit() && testApcSignalDuringWait() && testRemoteUnstartedThreads() &&
        testKernelWaitDefersTermination() && testRemoteProcessWait() &&
        testProcessObjectLifetime() && testSystemThreadCreation();
}

// Run after the persistent BSP clock is active. The last thread handle is
// closed before execution; the deferred worker must reap and signal its process.
extern "C" bool MicantTestPsDeferredReaper() noexcept {
    using namespace micant;
    using namespace micant::ke;
    auto* boot = KeGetCurrentThread();
    auto& scheduler = PriorityScheduler::get();
    if (!boot || !hal::amd64::bootstrapSchedulerClockActive() || scheduler.getReadyThreadCount())
        return false;
    const auto pages = mm::PfnDatabase::get().getActiveCount();
    const auto processes = ps::detail::bootstrapProcessObjectCount();
    const auto threads = ps::detail::bootstrapThreadObjectCount();
    Handle processHandle{}, threadHandle{};
    ps::EProcess* process{};
    ps::EThread* thread{};
    if (ps::detail::createProcessHandle(ob::SYNCHRONIZE, processHandle, process) != NtStatus::Success)
        return false;
    State state{process, boot};
    if (ps::detail::createThreadHandle(*process, 32768, threadEntry, &state, ob::SYNCHRONIZE,
                                       threadHandle, thread) != NtStatus::Success || !thread)
        return false;
    state.owner = thread;
    thread->kernelThread()->priority = boot->priority + 1;
    if (sys::NtClose(threadHandle) != NtStatus::Success || thread->ready() != NtStatus::Success)
        return false;
    auto* next = scheduler.selectNextThread();
    const auto old = KfRaiseIrql(DISPATCH_LEVEL);
    const bool switched = next && KiSwapContext(boot, next);
    KeLowerIrql(old);
    if (!switched || !state.ran || state.rundownInvalid || state.rundownCount != 123 ||
        state.kernelCount != 213 || state.ownedApc) return false;
    if (sys::NtWaitForSingleObject(processHandle, false, nullptr) != NtStatus::Success ||
        process->getDirectoryTableBase() || process->exitStatus() != NtStatus::Success ||
        sys::NtClose(processHandle) != NtStatus::Success) return false;
    return ps::detail::bootstrapProcessObjectCount() == processes &&
        ps::detail::bootstrapThreadObjectCount() == threads &&
        mm::PfnDatabase::get().getActiveCount() == pages;
}
