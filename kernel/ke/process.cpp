#include "micant/km/core/ke.hpp"

#if defined(MICANT_FREESTANDING) && (defined(_M_X64) || defined(__x86_64__))
#include "micant/km/executive/ps.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"

namespace micant::ke {
namespace {
constexpr uintptr_t kSameProcessAttach = 1;

void initializeApcStateToken(KAPC_STATE& state, void* previousProcess) noexcept {
    for (auto& list : state.ApcListHead) {
        list.Flink = &list;
        list.Blink = &list;
    }
    state.Process = previousProcess;
    state.KernelApcInProgress = false;
    state.KernelApcPending = false;
    state.UserApcPending = false;
}

void clearApcStateToken(KAPC_STATE& state) noexcept {
    for (auto& list : state.ApcListHead) list = {};
    state.Process = nullptr;
    state.KernelApcInProgress = false;
    state.KernelApcPending = false;
    state.UserApcPending = false;
}

bool validApcStateToken(const KAPC_STATE& state, const KThread::ProcessAttachFrame& frame) noexcept {
    if (frame.noOp) return reinterpret_cast<uintptr_t>(state.Process) == kSameProcessAttach;
    void* expected = frame.previousApcStateIndex == ApcStateIndexOriginal
        ? nullptr : static_cast<void*>(frame.previousProcess);
    return state.Process == expected;
}
}

extern "C" __declspec(dllexport) void KeStackAttachProcess(
    ps::EProcess* process, KAPC_STATE* apcState) noexcept {
    if (!process || !apcState || KeGetCurrentIrql() >= DISPATCH_LEVEL ||
        g_CurrentCpu.processorNumber != 0 || g_CurrentCpu.dpcRoutineActive)
        return;

    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    auto* thread = KeGetCurrentThread();
    if (!thread || thread->processAttachDepth >= thread->processAttachFrames.size()) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return;
    }
    for (size_t index = 0; index < thread->processAttachDepth; ++index) {
        if (thread->processAttachFrames[index].callerState == apcState) {
            if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
            return;
        }
    }

    uintptr_t targetRoot{};
    ob::ObjectHeader* targetReference{};
    const auto referenceStatus = process->referenceAddressSpaceForAttach(targetRoot, targetReference);
    if (referenceStatus != NtStatus::Success) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return;
    }

    auto* previousProcess = thread->attachedProcess ? thread->attachedProcess : thread->owningProcess;
    uintptr_t previousRoot{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(previousRoot));
    if (!previousRoot || previousRoot != thread->directoryTableBase ||
        previousRoot != g_CurrentCpu.currentCr3) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        process->releaseAddressSpaceAttachReference(targetReference);
        return;
    }

    const uint8_t previousApcStateIndex = thread->apcStateIndex;
    if (previousApcStateIndex > ApcStateIndexAttached) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        process->releaseAddressSpaceAttachReference(targetReference);
        return;
    }

    KThread::ProcessAttachFrame frame{};
    frame.callerState = apcState;
    frame.previousProcess = previousProcess;
    frame.targetProcess = process;
    frame.previousRoot = previousRoot;
    frame.previousApcStateIndex = previousApcStateIndex;
    if (process == previousProcess) {
        frame.noOp = true;
        initializeApcStateToken(*apcState, reinterpret_cast<void*>(kSameProcessAttach));
        thread->processAttachFrames[thread->processAttachDepth++] = frame;
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        process->releaseAddressSpaceAttachReference(targetReference);
        return;
    }

    if (thread->normalKernelApcActive ||
        !hal::amd64::switchProcessPagingRootByPhysical(targetRoot)) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        process->releaseAddressSpaceAttachReference(targetReference);
        return;
    }

    frame.targetReference = targetReference;
    initializeApcStateToken(*apcState,
        previousApcStateIndex == ApcStateIndexOriginal ? nullptr : static_cast<void*>(previousProcess));
    thread->attachedProcess = process;
    thread->directoryTableBase = targetRoot;
    thread->apcStateIndex = ApcStateIndexAttached;
    g_CurrentCpu.currentCr3 = targetRoot;
    thread->processAttachFrames[thread->processAttachDepth++] = frame;
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
}

extern "C" __declspec(dllexport) void KeUnstackDetachProcess(KAPC_STATE* apcState) noexcept {
    if (!apcState || KeGetCurrentIrql() >= DISPATCH_LEVEL ||
        g_CurrentCpu.processorNumber != 0 || g_CurrentCpu.dpcRoutineActive)
        return;

    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    auto* thread = KeGetCurrentThread();
    if (!thread || !thread->processAttachDepth) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return;
    }
    auto& frame = thread->processAttachFrames[thread->processAttachDepth - 1];
    uintptr_t currentRoot{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(currentRoot));
    if (frame.callerState != apcState || !validApcStateToken(*apcState, frame) ||
        (frame.noOp ? false : (thread->attachedProcess != frame.targetProcess ||
         thread->apcStateIndex != ApcStateIndexAttached || currentRoot != thread->directoryTableBase ||
         currentRoot != g_CurrentCpu.currentCr3 || thread->normalKernelApcActive))) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return;
    }

    auto* targetProcess = frame.targetProcess;
    auto* targetReference = static_cast<ob::ObjectHeader*>(frame.targetReference);
    if (!frame.noOp) {
        if (!hal::amd64::switchProcessPagingRootByPhysical(frame.previousRoot)) {
            if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
            return;
        }
        thread->attachedProcess = frame.previousApcStateIndex == ApcStateIndexOriginal
            ? nullptr : frame.previousProcess;
        thread->directoryTableBase = frame.previousRoot;
        thread->apcStateIndex = frame.previousApcStateIndex;
        g_CurrentCpu.currentCr3 = frame.previousRoot;
    }
    frame = {};
    --thread->processAttachDepth;
    clearApcStateToken(*apcState);
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    if (targetReference) targetProcess->releaseAddressSpaceAttachReference(targetReference);
}

namespace detail {
bool unwindCurrentProcessAttachments() noexcept {
    auto* thread = KeGetCurrentThread();
    if (!thread) return false;
    while (thread->processAttachDepth) {
        auto* state = thread->processAttachFrames[thread->processAttachDepth - 1].callerState;
        const auto previousDepth = thread->processAttachDepth;
        if (!state) return false;
        KeUnstackDetachProcess(state);
        if (thread->processAttachDepth + 1 != previousDepth) return false;
    }
    return true;
}
}
}
#endif
