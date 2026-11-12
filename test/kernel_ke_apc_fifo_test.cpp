#include "micant/km/core/ke.hpp"

namespace {
struct Sequence { uint32_t value{}; };

void record(void* context, void* argument, void*) {
    auto& sequence = *static_cast<Sequence*>(context);
    sequence.value = sequence.value * 10 + static_cast<uint32_t>(reinterpret_cast<uintptr_t>(argument));
}
}

extern "C" bool MicantTestKeApcFifo() noexcept {
    using namespace micant::ke;
    auto* current = KeGetCurrentThread();
    if (!current || !current->kernelApcList.empty()) return false;
    Sequence sequence;
    KAPC first{}, second{}, third{};
    KeInitializeApc(&first, current->tid, nullptr, nullptr, record, 0, &sequence);
    KeInitializeApc(&second, current->tid, nullptr, nullptr, record, 0, &sequence);
    KeInitializeApc(&third, current->tid, nullptr, nullptr, record, 0, &sequence);
    if (!KeInsertQueueApc(&first, reinterpret_cast<void*>(1)) ||
        !KeInsertQueueApc(&second, reinterpret_cast<void*>(2)) ||
        !KeInsertQueueApc(&third, reinterpret_cast<void*>(3)) ||
        KeInsertQueueApc(&first) || current->kernelApcList.front() != &first) return false;
    KiDeliverApc(0);
    return sequence.value == 123 && current->kernelApcList.empty() &&
        !first.inserted && !second.inserted && !third.inserted &&
        !first.queueNext && !second.queueNext && !third.queueNext;
}
