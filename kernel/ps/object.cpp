#include "micant/km/executive/ps_objects.hpp"
#include "micant/km/executive/ps.hpp"
#include <new>

namespace micant::ps::detail {
namespace {
ob::ObjectType processType;

void deleteProcess(ob::ObjectHeader* header) {
    auto* process = header->as<EProcess>();
    if (process->getDirectoryTableBase() && process->closeAddressSpace() != NtStatus::Success)
        __builtin_trap();
    process->~EProcess();
}

void initializeProcessType() noexcept {
    if (processType.typeId == ob::ObjectTypeId::Process) return;
    processType.typeId = ob::ObjectTypeId::Process;
    processType.deleteProcedure = deleteProcess;
}
}

struct ProcessObjectFactory {
    static void bind(EProcess& process, ob::ObjectHeader* header) noexcept {
        process.objectHeader_ = header;
    }
};

NtStatus createProcessHandle(uint32_t access, Handle& handle, EProcess*& process) noexcept {
    initializeProcessType();
    process = nullptr;
    void* body{};
    ob::ObjectHeader* header{};
    auto status = ob::ObCreateObject(ob::KernelMode, &processType, nullptr, sizeof(EProcess), &body, &header);
    if (status != NtStatus::Success) return status;
    auto* created = new (body) EProcess{};
    ProcessObjectFactory::bind(*created, header);
    status = created->initializeAddressSpace();
    if (status == NtStatus::Success)
        status = ob::detail::currentHandleTable().createHandle(header, handle, access);
    if (status == NtStatus::Success) process = created;
    ob::ObDereferenceObject(header);
    return status;
}

NtStatus referenceProcessHandle(Handle handle, uint32_t access, EProcess*& process,
                                ob::ObjectHeader*& reference) noexcept {
    initializeProcessType();
    process = nullptr;
    reference = nullptr;
    const auto status = ob::ObReferenceObjectByHandle(handle, ob::detail::currentHandleTable(),
                                                       access, &processType, &reference);
    if (status == NtStatus::Success) process = reference->as<EProcess>();
    return status;
}

uint32_t bootstrapProcessObjectCount() noexcept {
    return processType.totalNumberOfObjects.load(std::memory_order_relaxed);
}
}
