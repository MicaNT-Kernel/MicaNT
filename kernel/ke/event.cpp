#include "micant/km/core/sync.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/executive/ob.hpp"
#include "micant/km/executive/ps.hpp"
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
    const auto status = ob::ObReferenceObjectByHandle(handle, ob::detail::currentHandleTable(),
                                                       EventModifyState, &eventType, &header);
    if (status != NtStatus::Success) return status;
    const bool previous = header->as<Event>()->set();
    if (previousState) *previousState = previous ? 1 : 0;
    ob::ObDereferenceObject(header);
    return NtStatus::Success;
}

NtStatus NtWaitForSingleObject(Handle handle, bool alertable, LargeInteger* timeout) {
    if (alertable || (timeout && timeout->quadPart != 0)) return NtStatus::NotImplemented;
    ob::ObjectHeader* header{};
    const auto status = ob::ObReferenceObjectByHandle(handle, ob::detail::currentHandleTable(),
                                                       ob::SYNCHRONIZE, &eventType, &header);
    if (status != NtStatus::Success) return status;
    auto* current = ke::KeGetCurrentThread();
    if (!current || current->eventWaitInProgress) {
        ob::ObDereferenceObject(header);
        return NtStatus::InvalidParameter;
    }
    current->eventWaitInProgress = true;
    const auto result = header->as<Event>()->wait(timeout != nullptr);
    current->eventWaitInProgress = false;
    ob::ObDereferenceObject(header);
    if (current->owningProcess && current->owningProcess->isTerminating()) {
        (void)current->owningProcess->terminateCurrent(current->owningProcess->exitStatus());
        __builtin_trap();
    }
    return result;
}
}
