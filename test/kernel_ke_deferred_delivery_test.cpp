#include "micant/km/core/ke.hpp"

namespace {
using namespace micant::ke;

struct DeliveryState {
    uint32_t order{};
    uint32_t normalCalls{};
    bool invalid{};
};

bool interruptsEnabled() noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0" : "=r"(flags) : : "memory");
    return (flags & (uint64_t{1} << 9)) != 0;
}

void dpcRoutine(KDPC* dpc, void* context, void* arg1, void* arg2) {
    auto& state = *static_cast<DeliveryState*>(context);
    const auto value = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg1));
    if (KeGetCurrentIrql() != DISPATCH_LEVEL || !interruptsEnabled() ||
        !g_CurrentCpu.dpcRoutineActive || arg2 != context || (value != 1 && value != 2))
        state.invalid = true;
    state.order = state.order * 10 + value;
    if (value == 1 && state.normalCalls++ == 0 &&
        !KeInsertQueueDpc(dpc, reinterpret_cast<void*>(uintptr_t{1}), context))
        state.invalid = true;
}

void apcKernelRoutine(KAPC*, PKNORMAL_ROUTINE* normal, void** context,
                      void**, void**) {
    auto& state = *static_cast<DeliveryState*>(*context);
    if (KeGetCurrentIrql() != APC_LEVEL || !interruptsEnabled()) state.invalid = true;
    state.order = state.order * 10 + (*normal ? 2 : 1);
}

void apcNormalRoutine(void* context, void* arg1, void* arg2) {
    auto& state = *static_cast<DeliveryState*>(context);
    if (KeGetCurrentIrql() != PASSIVE_LEVEL || !interruptsEnabled() ||
        arg1 != context || arg2 != context) state.invalid = true;
    state.order = state.order * 10 + 3;
}
}

// Requires the initialized single-BSP runtime clock and IF disabled. It uses
// the production IRQL-lowering and region-exit paths, rather than directly
// draining either queue, then restores the initial IF and IRQL states.
extern "C" bool MicantTestKeDeferredDelivery() noexcept {
    using namespace micant::ke;
    auto* current = KeGetCurrentThread();
    if (!current || KeGetCurrentIrql() != PASSIVE_LEVEL || interruptsEnabled() ||
        !current->kernelApcList.empty() || current->pendingNormalCount ||
        current->kernelApcDisable || current->specialApcDisable ||
        DpcQueue::get().getQueuedCount()) return false;

    DeliveryState dpcState{};
    KDPC normal{}, high{}, removed{};
    KeInitializeDpc(&normal, dpcRoutine, &dpcState);
    KeInitializeDpc(&high, dpcRoutine, &dpcState);
    KeInitializeDpc(&removed, dpcRoutine, &dpcState);
    high.importance = DpcImportance::High;
    const auto previous = KfRaiseIrql(DISPATCH_LEVEL);
    const bool queued = KeInsertQueueDpc(&normal, reinterpret_cast<void*>(uintptr_t{1}), &dpcState) &&
        KeInsertQueueDpc(&high, reinterpret_cast<void*>(uintptr_t{2}), &dpcState) &&
        KeInsertQueueDpc(&removed, reinterpret_cast<void*>(uintptr_t{2}), &dpcState) &&
        !KeInsertQueueDpc(&normal) && KeRemoveQueueDpc(&removed) && !KeRemoveQueueDpc(&removed);
    __asm__ __volatile__("sti" : : : "memory");
    KeLowerIrql(previous);
    __asm__ __volatile__("cli" : : : "memory");
    if (!queued || previous != PASSIVE_LEVEL || dpcState.invalid || dpcState.order != 211 ||
        dpcState.normalCalls != 2 || normal.inserted || high.inserted || removed.inserted ||
        DpcQueue::get().getQueuedCount() || KeGetCurrentIrql() != PASSIVE_LEVEL) return false;

    DeliveryState apcState{};
    KAPC normalApc{}, specialApc{};
    KeInitializeApc(&normalApc, current->tid, apcKernelRoutine, nullptr,
                    apcNormalRoutine, 0, &apcState);
    KeInitializeApc(&specialApc, current->tid, apcKernelRoutine, nullptr,
                    nullptr, 0, &apcState);
    KeEnterCriticalRegion();
    KeEnterCriticalRegion();
    KeEnterGuardedRegion();
    KeEnterGuardedRegion();
    const bool apcsQueued = KeInsertQueueApc(&normalApc, &apcState, &apcState) &&
        KeInsertQueueApc(&specialApc, &apcState, &apcState) &&
        !KeInsertQueueApc(&normalApc);
    __asm__ __volatile__("sti" : : : "memory");
    KeLeaveGuardedRegion();
    const bool nestedGuarded = apcState.order == 0;
    KeLeaveGuardedRegion();
    const bool specialDelivered = apcState.order == 1 && !specialApc.inserted && normalApc.inserted;
    KeLeaveCriticalRegion();
    const bool nestedCritical = apcState.order == 1 && normalApc.inserted;
    KeLeaveCriticalRegion();
    __asm__ __volatile__("cli" : : : "memory");
    return apcsQueued && nestedGuarded && specialDelivered && nestedCritical &&
        !apcState.invalid && apcState.order == 123 && !normalApc.inserted &&
        current->kernelApcList.empty() && !current->pendingNormalCount &&
        !current->kernelApcDisable && !current->specialApcDisable &&
        KeGetCurrentIrql() == PASSIVE_LEVEL && !interruptsEnabled();
}
