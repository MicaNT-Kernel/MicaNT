#include "micant/km/core/sync.hpp"
#include "micant/km/core/timer.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/executive/ob.hpp"
#include "micant/km/executive/ps.hpp"
#include "micant/km/executive/ps_objects.hpp"
#include <new>

namespace micant::sys {
namespace {
constexpr uint32_t EventModifyState = 0x0002;
constexpr uint32_t EventAllAccess = 0x001F0003;
ob::ObjectType eventType;
ob::ObjectType timerType;
using Event = sync::detail::BootstrapEvent;

void deleteEvent(ob::ObjectHeader* header) { header->as<Event>()->~Event(); }
void deleteTimer(ob::ObjectHeader* header) {
    auto* timer = header->as<timer::KTIMER>();
    if (!ke::clock_timer::cancelAndRemoveDpc(timer->deadline)) __builtin_trap();
    timer->~KTIMER();
}
ob::ObjectHeader* deferredTimers{};
ke::passive_work::Item timerDeleteWork{};
void finishDeferredTimers(void*) noexcept {
    for (;;) {
        uint64_t flags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        auto* header = deferredTimers;
        if (header) deferredTimers = header->deferredDeleteNext;
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        if (!header) return;
        header->deferredDeleteNext = nullptr;
        ob::ObFinishDelete(header);
    }
}
bool deferTimerDelete(ob::ObjectHeader* header) noexcept {
    if (ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL && !ke::g_CurrentCpu.dpcRoutineActive)
        return false;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    header->deferredDeleteNext = deferredTimers;
    deferredTimers = header;
    timerDeleteWork.routine = finishDeferredTimers;
    if (!timerDeleteWork.queued && !ke::passive_work::queue(timerDeleteWork)) __builtin_trap();
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return true;
}
}

void initializeBootstrapEventServices() noexcept {
    eventType.typeId = ob::ObjectTypeId::Event;
    eventType.deleteProcedure = deleteEvent;
    timerType.typeId = ob::ObjectTypeId::Timer;
    timerType.deleteProcedure = deleteTimer;
    timerType.deferDelete = deferTimerDelete;
}

NtStatus NtCreateTimer(Handle* out, uint32_t access, ObjectAttributes* attributes, uint32_t type) {
    constexpr uint32_t TimerAllAccess = 0x001F0003;
    if (!out || type > 1) return NtStatus::InvalidParameter;
    if (attributes) {
        if (attributes->length != sizeof(ObjectAttributes)) return NtStatus::InvalidParameter;
        if (attributes->rootDirectory || attributes->objectName || attributes->attributes ||
            attributes->securityDescriptor || attributes->securityQualityOfService)
            return NtStatus::NotImplemented;
    }
    if (access & ~TimerAllAccess) return NtStatus::NotImplemented;
    void* body{};
    ob::ObjectHeader* header{};
    auto status = ob::ObCreateObject(ob::KernelMode, &timerType, nullptr, sizeof(timer::KTIMER), &body, &header);
    if (status != NtStatus::Success) return status;
    auto* object = new (body) timer::KTIMER{};
    timer::KeInitializeTimerEx(object, static_cast<timer::TimerType>(type));
    status = ob::detail::currentHandleTable().createHandle(header, *out, access);
    ob::ObDereferenceObject(header);
    return status;
}

NtStatus NtSetTimer(Handle handle, LargeInteger* dueTime, void* apc, void* context,
                    bool resume, uint32_t period, bool* previousState) {
    if (!dueTime) return NtStatus::InvalidParameter;
    if (apc || context || resume || period > INT32_MAX) return NtStatus::NotImplemented;
    ob::HandleTable* table{}; Handle raw{}; ob::ObjectHeader* header{};
    auto status = ob::detail::resolveHandle(handle, table, raw);
    if (status == NtStatus::Success)
        status = ob::ObReferenceObjectByHandle(raw, *table, 0x0002, &timerType, &header);
    if (status != NtStatus::Success) return status;
    auto* object = header->as<timer::KTIMER>();
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    const bool priorState = timer::KeReadStateTimer(object);
    bool previousInsertion{};
    status = timer::KiSetTimerChecked(object, *dueTime, period, nullptr, previousInsertion);
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    if (status == NtStatus::Success && previousState) *previousState = priorState;
    ob::ObDereferenceObject(header);
    return status;
}

NtStatus NtCancelTimer(Handle handle, bool* currentState) {
    ob::HandleTable* table{}; Handle raw{}; ob::ObjectHeader* header{};
    auto status = ob::detail::resolveHandle(handle, table, raw);
    if (status == NtStatus::Success)
        status = ob::ObReferenceObjectByHandle(raw, *table, 0x0002, &timerType, &header);
    if (status != NtStatus::Success) return status;
    auto* object = header->as<timer::KTIMER>();
    if (currentState) *currentState = timer::KeReadStateTimer(object);
    (void)timer::KeCancelTimer(object);
    ob::ObDereferenceObject(header);
    return NtStatus::Success;
}

NtStatus referenceBootstrapEventForIo(Handle handle, ob::ObjectHeader*& header) noexcept {
    header = nullptr;
    ob::HandleTable* table{}; Handle raw{};
    auto status = ob::detail::resolveHandle(handle, table, raw);
    if (status == NtStatus::Success)
        status = ob::ObReferenceObjectByHandle(raw, *table, EventModifyState, &eventType, &header);
    return status;
}

uint32_t bootstrapEventObjectCount() noexcept {
    return eventType.totalNumberOfObjects.load(std::memory_order_relaxed);
}

NtStatus NtCreateEvent(Handle* out, uint32_t access, ObjectAttributes* attributes,
                       uint32_t type, bool initialState) {
    if (!out) return NtStatus::InvalidParameter;
    if (type > 1) return NtStatus::InvalidParameter4;
    if (attributes) {
        if (attributes->length != sizeof(ObjectAttributes)) return NtStatus::InvalidParameter;
        if (attributes->rootDirectory || attributes->objectName || attributes->attributes ||
            attributes->securityDescriptor || attributes->securityQualityOfService)
            return NtStatus::NotImplemented;
    }
    if (access & ~EventAllAccess) return NtStatus::NotImplemented;
    void* body{};
    ob::ObjectHeader* header{};
    auto status = ob::ObCreateObject(ob::KernelMode, &eventType, nullptr, sizeof(Event), &body, &header);
    if (status != NtStatus::Success) return status;
    new (body) Event(type == 0, initialState);
    status = ob::detail::currentHandleTable().createHandle(header, *out, access);
    ob::ObDereferenceObject(header);
    return status;
}

NtStatus NtSetEvent(Handle handle, int32_t* previousState) {
    ob::ObjectHeader* header{};
    ob::HandleTable* table{};
    Handle raw{};
    auto status = ob::detail::resolveHandle(handle, table, raw);
    if (status == NtStatus::Success)
        status = ob::ObReferenceObjectByHandle(raw, *table, EventModifyState, &eventType, &header);
    if (status != NtStatus::Success) return status;
    const bool previous = header->as<Event>()->set();
    if (previousState) *previousState = previous ? 1 : 0;
    ob::ObDereferenceObject(header);
    return NtStatus::Success;
}

NtStatus NtWaitForSingleObject(Handle handle, bool alertable, LargeInteger* timeout) {
    if (timeout && timeout->quadPart != 0) return NtStatus::NotImplemented;
    ob::ObjectHeader* header{};
    NtStatus status{};
    if (handle == InvalidHandleValue) {
        ps::EProcess* process{};
        status = ps::detail::referenceCurrentProcess(process, header);
    } else if (handle == static_cast<Handle>(-2)) {
        ps::EThread* thread{};
        status = ps::detail::referenceCurrentThread(thread, header);
    } else {
        ob::HandleTable* table{};
        Handle raw{};
        status = ob::detail::resolveHandle(handle, table, raw);
        if (status == NtStatus::Success)
            status = ob::ObReferenceObjectByHandle(raw, *table, ob::SYNCHRONIZE, nullptr, &header);
    }
    if (status != NtStatus::Success) return status;
    Event* waitObject{};
    if (header->type == &eventType) {
        waitObject = header->as<Event>();
    } else if (header->type == &timerType) {
        waitObject = &header->as<timer::KTIMER>()->event;
    } else if (ps::detail::isProcessObject(*header)) {
        waitObject = &header->as<ps::EProcess>()->terminationEvent();
    } else if (ps::detail::isThreadObject(*header)) {
        waitObject = &header->as<ps::EThread>()->terminationEvent();
    } else {
        ob::ObDereferenceObject(header);
        return NtStatus::ObjectTypeMismatch;
    }
    auto* current = ke::KeGetCurrentThread();
    if (!current || current->eventWaitInProgress) {
        ob::ObDereferenceObject(header);
        return NtStatus::InvalidParameter;
    }
    current->eventWaitInProgress = true;
    current->eventWaitUserMode = current->previousUserMode;
    current->eventWaitCanTerminate = ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL;
    const auto result = waitObject->wait(timeout != nullptr, alertable);
    // The wait object has unlinked its stack waiter before returning. Defer
    // termination until this syscall has also cleared the thread's wait
    // registration and released the Ob reference that pins the waited object.
    const bool serviceTermination = current->terminationRequested &&
        current->eventWaitCanTerminate && current->serviceTermination;
    auto terminate = current->serviceTermination;
    void* terminationContext = current->terminationContext;
    current->eventWaitInProgress = false;
    current->eventWaitUserMode = false;
    current->eventWaitCanTerminate = false;
    ob::ObDereferenceObject(header);
    if (serviceTermination) terminate(terminationContext);
    return result;
}
}
