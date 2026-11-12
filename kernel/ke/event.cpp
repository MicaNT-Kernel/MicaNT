#include "micant/km/core/sync.hpp"
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
using Event = sync::detail::BootstrapEvent;

void deleteEvent(ob::ObjectHeader* header) { header->as<Event>()->~Event(); }
}

void initializeBootstrapEventServices() noexcept {
    eventType.typeId = ob::ObjectTypeId::Event;
    eventType.deleteProcedure = deleteEvent;
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
    if (alertable || (timeout && timeout->quadPart != 0)) return NtStatus::NotImplemented;
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
    const auto result = waitObject->wait(timeout != nullptr);
    current->eventWaitInProgress = false;
    current->eventWaitUserMode = false;
    current->eventWaitCanTerminate = false;
    ob::ObDereferenceObject(header);
    // A cancelled user wait unwinds through its caller before termination is
    // serviced at the user-return boundary.
    return result;
}
}
