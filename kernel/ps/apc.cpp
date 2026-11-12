#include "micant/km/executive/ps.hpp"
#include "micant/km/executive/usermode.hpp"

#if defined(MICANT_BAREMETAL) && (defined(_M_X64) || defined(__x86_64__))
#include <cstring>
#include <new>
#include "micant/km/core/syscalls.hpp"
#include "micant/km/executive/ps_objects.hpp"
#include "micant/km/core/passive_work.hpp"

extern "C" uint32_t MicantAmd64XsaveMask;

namespace {
constexpr uint32_t ThreadSetContext = 0x0010;

}

namespace micant::sys {
NtStatus NtQueueApcThread(Handle threadHandle, uintptr_t apcRoutine,
                          void* normalContext, void* systemArgument1,
                          void* systemArgument2) {
    if (!apcRoutine || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL)
        return NtStatus::InvalidParameter;
    ps::EThread* target{};
    ob::ObjectHeader* reference{};
    auto status = ps::detail::referenceThreadHandle(threadHandle, ThreadSetContext, target, reference);
    if (status != NtStatus::Success) return status;

    auto* kernelThread = target->kernelThread();
    auto* process = kernelThread ? kernelThread->owningProcess : nullptr;
    uint64_t flags{};
    uintptr_t previousRoot{};
    __asm__ __volatile__("pushfq; popq %0; cli; movq %%cr3, %1"
                         : "=r"(flags), "=r"(previousRoot) : : "memory");
    uintptr_t targetRoot{};
    if (!kernelThread || target->isSystemThread() || !process ||
        kernelThread->state == ke::ThreadState::Terminated || process->isTerminating()) {
        status = NtStatus::InvalidHandle;
    } else {
        targetRoot = process->getDirectoryTableBase();
        if (previousRoot != targetRoot &&
            !hal::amd64::switchProcessPagingRootByPhysical(targetRoot)) {
            status = NtStatus::Unsuccessful;
        } else if (!hal::amd64::bootstrapUserRange(apcRoutine, 1, false, true)) {
            status = NtStatus::AccessViolation;
        }
        if (previousRoot != targetRoot &&
            !hal::amd64::switchProcessPagingRootByPhysical(previousRoot)) __builtin_trap();
    }
    if (status == NtStatus::Success) {
        auto* apc = new (std::nothrow_t{}) ke::KAPC{};
        if (!apc) {
            status = NtStatus::NoMemory;
        } else {
            ke::KeInitializeApc(apc, kernelThread, ke::ApcEnvironment::OriginalApcEnvironment,
                ke::passive_work::ownedUserApcKernel, ke::passive_work::ownedUserApcRundown,
                reinterpret_cast<ke::PKNORMAL_ROUTINE>(apcRoutine), ke::UserMode, normalContext);
            if (!ke::KeInsertQueueApc(apc, systemArgument1, systemArgument2)) {
                delete apc;
                status = NtStatus::Unsuccessful;
            }
        }
    }
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    ob::ObDereferenceObject(reference);
    return status;
}
}

namespace micant::ps {
NtStatus EThread::prepareUserApc(const ke::UserApcDelivery* deliveries,
                                 size_t deliveryCount,
                                 uint64_t* hardwareFrame, uint64_t* savedRegisters,
                                 const void* floatingState) noexcept {
    struct UserApcCallRecord {
        uintptr_t routine;
        void* normalContext;
        void* systemArgument1;
        void* systemArgument2;
    };
    static_assert(sizeof(UserApcCallRecord) == 32);
    ob::detail::HandleTableGuard guard;
    uintptr_t currentRoot{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(currentRoot));
    if (!thread_ || ke::KeGetCurrentThread() != thread_.get() || !process_ ||
        thread_->owningProcess != process_ || thread_->apcStateIndex != ke::ApcStateIndexOriginal ||
        !deliveries || !deliveryCount || deliveryCount > ke::UserApcDeliveryBatchCapacity ||
        !deliveries[0].routine || !hardwareFrame || !savedRegisters || !floatingState ||
        (hardwareFrame[1] & 3) != 3 || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL ||
        currentRoot != process_->getDirectoryTableBase() || !userTeb_)
        return NtStatus::InvalidParameter;

    static_assert(sizeof(micant::CONTEXT) == 0x4d0);
    static_assert(sizeof(micant::CONTEXT_EX) == 0x20);
    const auto* images = process_->nativeImages();
    uint64_t dispatcher{};
    if (!images || !process_->nativeImagesFinalized() ||
        images->resolve("ntdll.dll", "KiUserApcDispatcher", 0, dispatcher) != NtStatus::Success ||
        !hal::amd64::bootstrapUserRange(dispatcher, 1, false, true) ||
        !hal::amd64::bootstrapUserRange(userTeb_, sizeof(um::Teb64), true))
        return NtStatus::ProcedureNotFound;
    for (size_t i = 0; i < deliveryCount; ++i) {
        if (!deliveries[i].routine ||
            !hal::amd64::bootstrapUserRange(deliveries[i].routine, 1, false, true))
            return NtStatus::AccessViolation;
    }

    const bool hasExtendedXstate = (MicantAmd64XsaveMask & ~uint32_t{3}) != 0;
    const size_t extendedSize = ke::MicantAmd64ExtendedSize;
    if (hasExtendedXstate && (extendedSize < 576 || extendedSize > 4096))
        return NtStatus::NotSupported;
    const size_t xstateLength = hasExtendedXstate ? extendedSize - 512 : 0;
    constexpr size_t xstateAlignmentSlack = 63;
    constexpr size_t dispatcherStackReserve = 16 * 1024;
    constexpr size_t xstateOffsetSlack = xstateAlignmentSlack;
    const size_t contextSize = sizeof(micant::CONTEXT) + sizeof(micant::CONTEXT_EX) +
        xstateOffsetSlack + xstateLength;
    constexpr size_t callAlignmentSlack = alignof(UserApcCallRecord) - 1;
    if (contextSize > SIZE_MAX - callAlignmentSlack ||
        deliveryCount > (SIZE_MAX - contextSize - callAlignmentSlack) / sizeof(UserApcCallRecord))
        return NtStatus::StackOverflow;
    const size_t callsSize = deliveryCount * sizeof(UserApcCallRecord);
    const size_t payloadContextSize = contextSize + callsSize + callAlignmentSlack;
    const size_t dispatcherStackBytes = dispatcherStackReserve + 64;
    if (payloadContextSize > SIZE_MAX - dispatcherStackBytes) return NtStatus::StackOverflow;
    const size_t payloadSize = payloadContextSize + dispatcherStackBytes;
    if (payloadSize > SIZE_MAX - mm::PageMask4KB) return NtStatus::StackOverflow;
    const size_t guaranteeBytes = (payloadSize + mm::PageMask4KB) & ~mm::PageMask4KB;
    const uintptr_t userStackTop = static_cast<uintptr_t>(hardwareFrame[3]);
    if (!userStack_ || userStackTop_ <= userStackLimit_ || userStackLimit_ < userStack_ ||
        userStackLimit_ > UINTPTR_MAX - guaranteeBytes ||
        userStackTop_ - userStackLimit_ < guaranteeBytes)
        return NtStatus::StackOverflow;

    const bool stackPointerInOwnedStack = userStackTop >= userStackLimit_ && userStackTop <= userStackTop_;
    uintptr_t frameAddress = stackPointerInOwnedStack && userStackTop >= payloadSize
        ? (userStackTop - payloadSize) & ~uintptr_t{15} : 0;
    const bool needsGuarantee = !frameAddress || frameAddress < userStackLimit_ + dispatcherStackBytes ||
        frameAddress < dispatcherStackBytes ||
        !hal::amd64::bootstrapUserRange(frameAddress - dispatcherStackBytes, payloadSize, true);
    if (needsGuarantee) {
        frameAddress = (userStackLimit_ + dispatcherStackBytes + 15) & ~uintptr_t{15};
        if (frameAddress < userStackLimit_ || frameAddress < dispatcherStackBytes ||
            frameAddress > UINTPTR_MAX - payloadContextSize ||
            !hal::amd64::bootstrapUserRange(frameAddress - dispatcherStackBytes,
                                            payloadSize, true))
            return NtStatus::StackOverflow;
    }

    auto* context = reinterpret_cast<micant::CONTEXT*>(frameAddress);
    auto* contextEx = reinterpret_cast<micant::CONTEXT_EX*>(frameAddress + sizeof(*context));
    *context = {};
    *contextEx = {};
    context->ContextFlags = CONTEXT_FULL | CONTEXT_SEGMENTS |
        (hasExtendedXstate ? CONTEXT_XSTATE : 0);
    context->P1Home = reinterpret_cast<uintptr_t>(deliveries[0].normalContext);
    context->P2Home = reinterpret_cast<uintptr_t>(deliveries[0].systemArgument1);
    context->P3Home = reinterpret_cast<uintptr_t>(deliveries[0].systemArgument2);
    context->P4Home = deliveries[0].routine;
    context->SegCs = static_cast<uint16_t>(hardwareFrame[1]);
    context->SegSs = static_cast<uint16_t>(hardwareFrame[4]);
    __asm__ __volatile__("movw %%ds, %0" : "=rm"(context->SegDs));
    __asm__ __volatile__("movw %%es, %0" : "=rm"(context->SegEs));
    __asm__ __volatile__("movw %%fs, %0" : "=rm"(context->SegFs));
    __asm__ __volatile__("movw %%gs, %0" : "=rm"(context->SegGs));
    context->EFlags = static_cast<uint32_t>(hardwareFrame[2]);
    context->Rip = hardwareFrame[0];
    context->Rsp = userStackTop;
    context->R15 = savedRegisters[0]; context->R14 = savedRegisters[1];
    context->R13 = savedRegisters[2]; context->R12 = savedRegisters[3];
    context->R11 = savedRegisters[4]; context->R10 = savedRegisters[5];
    context->R9 = savedRegisters[6];  context->R8 = savedRegisters[7];
    context->Rdi = savedRegisters[8]; context->Rsi = savedRegisters[9];
    context->Rbp = savedRegisters[10]; context->Rbx = savedRegisters[11];
    context->Rdx = savedRegisters[12]; context->Rcx = savedRegisters[13];
    context->Rax = savedRegisters[14];
    std::memcpy(&context->FltSave, floatingState, sizeof(context->FltSave));
    context->MxCsr = context->FltSave.MxCsr;

    contextEx->All.Offset = -static_cast<int32_t>(sizeof(*context));
    contextEx->All.Length = static_cast<uint32_t>(sizeof(*context) + sizeof(*contextEx));
    contextEx->Legacy.Offset = -static_cast<int32_t>(sizeof(*context));
    contextEx->Legacy.Length = sizeof(*context);
    if (hasExtendedXstate) {
        const uintptr_t contextExAddress = reinterpret_cast<uintptr_t>(contextEx);
        const uintptr_t xstateAddress = (contextExAddress + sizeof(*contextEx) + 63) & ~uintptr_t{63};
        const size_t xstateOffset = xstateAddress - contextExAddress;
        contextEx->XState.Offset = static_cast<int32_t>(xstateOffset);
        contextEx->XState.Length = static_cast<uint32_t>(xstateLength);
        contextEx->All.Length = static_cast<uint32_t>(sizeof(*context) + xstateOffset + xstateLength);
        std::memcpy(reinterpret_cast<void*>(xstateAddress),
                    static_cast<const uint8_t*>(floatingState) + 512, xstateLength);
    } else {
        contextEx->XState.Offset = static_cast<int32_t>(sizeof(*contextEx) + 1);
    }
    auto* calls = reinterpret_cast<UserApcCallRecord*>(
        (frameAddress + contextSize + alignof(UserApcCallRecord) - 1) &
        ~(uintptr_t{alignof(UserApcCallRecord) - 1}));
    for (size_t i = 0; i < deliveryCount; ++i) {
        calls[i] = {deliveries[i].routine, deliveries[i].normalContext,
                    deliveries[i].systemArgument1, deliveries[i].systemArgument2};
    }
    context->P5Home = reinterpret_cast<uintptr_t>(calls);
    context->P6Home = deliveryCount;

    // NTDLL drains this bounded call list before restoring CONTEXT/XSTATE.
    hardwareFrame[0] = dispatcher;
    hardwareFrame[2] = (hardwareFrame[2] & ~uint64_t{0x3000}) | 2;
    hardwareFrame[3] = frameAddress;
    savedRegisters[12] = 0;
    savedRegisters[13] = 0;
    savedRegisters[14] = 0;
    return NtStatus::Success;
}

extern "C" bool MicantPsPrepareUserApc(uint64_t* hardwareFrame,
                                        uint64_t* savedRegisters,
                                        const void* floatingState) noexcept {
    auto* current = ke::KeGetCurrentThread();
    if (!current || !current->owningThread ||
        (!current->userApcDeliveryPending && !current->userApcDrainOnReturn))
        return false;
    ke::UserApcDelivery deliveries[ke::UserApcDeliveryBatchCapacity]{};
    size_t deliveryCount{};
    if (!ke::KePrepareUserApcDelivery(deliveries, ke::UserApcDeliveryBatchCapacity,
                                      deliveryCount)) return false;
    if (current->owningThread->prepareUserApc(deliveries, deliveryCount,
                                               hardwareFrame, savedRegisters, floatingState) == NtStatus::Success)
        return true;
    if (current->owningProcess)
        (void)current->owningProcess->terminateCurrent(NtStatus::StackOverflow);
    else if (current->serviceTermination)
        current->serviceTermination(current->terminationContext);
    return false;
}

}
#endif
