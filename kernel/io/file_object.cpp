#include "micant/km/executive/io_file.hpp"
#include "micant/km/executive/ob.hpp"
#include <new>

namespace micant::io {
namespace {
ob::ObjectType g_fileType{};
FileOpenRoutine g_fileOpen{};
void* g_fileOpenContext{};
std::atomic_flag g_fileBackendLock = ATOMIC_FLAG_INIT;

void lockFileBackend() noexcept {
    while (g_fileBackendLock.test_and_set(std::memory_order_acquire)) __asm__ __volatile__("pause");
}
void unlockFileBackend() noexcept { g_fileBackendLock.clear(std::memory_order_release); }

void deleteFileObject(ob::ObjectHeader* header) noexcept {
    header->as<FileObject>()->~FileObject();
}
}

NtStatus IoInitializeFileObjects() noexcept {
    g_fileType.typeId = ob::ObjectTypeId::File;
    g_fileType.validAccessMask = 0x001F01FF;
    g_fileType.genericMapping = {0x00120089, 0x00120116, 0x001200A0, 0x001F01FF};
    g_fileType.deleteProcedure = deleteFileObject;
    return NtStatus::Success;
}

NtStatus IoRegisterFileOpenRoutine(FileOpenRoutine open, void* context) noexcept {
    if (!open) return NtStatus::InvalidParameter;
    lockFileBackend();
    if (g_fileOpen) { unlockFileBackend(); return NtStatus::Conflict; }
    g_fileOpenContext = context;
    g_fileOpen = open;
    unlockFileBackend();
    return NtStatus::Success;
}

NtStatus IoOpenBackendFile(Handle* fileHandle, uint32_t desiredAccess,
                           std::u16string_view name) noexcept {
    if (!fileHandle || name.empty()) return NtStatus::InvalidParameter;
    FileOpenRoutine open{};
    void* context{};
    lockFileBackend();
    open = g_fileOpen;
    context = g_fileOpenContext;
    unlockFileBackend();
    if (!open) return NtStatus::NoSuchDevice;
    FileBackend backend{};
    auto status = open(context, name, backend);
    if (status != NtStatus::Success) return status;
    if (!backend.name.size()) backend.name = name;
    if (!backend.size || !backend.read || !backend.release) {
        if (backend.release) backend.release(backend.context);
        return NtStatus::InvalidParameter;
    }
    return IoCreateBackendFile(fileHandle, desiredAccess, backend);
}

NtStatus IoCreateBackendFile(Handle* fileHandle, uint32_t desiredAccess,
                             const FileBackend& backend) noexcept {
    constexpr uint32_t fileReadOnlyAccess = 0x001200A9; // FILE_GENERIC_READ | FILE_EXECUTE
    const uint32_t mappedAccess = ob::MapGenericAccess(desiredAccess, g_fileType.genericMapping);
    if (!fileHandle || !backend.read || !backend.size ||
        (mappedAccess & ~g_fileType.validAccessMask) || (mappedAccess & ~fileReadOnlyAccess)) {
        if (backend.release) backend.release(backend.context);
        return NtStatus::InvalidParameter;
    }
    void* body{};
    ob::ObjectHeader* header{};
    auto status = ob::ObCreateObject(ob::KernelMode, &g_fileType, nullptr,
                                     sizeof(FileObject), &body, &header);
    if (status != NtStatus::Success) {
        if (backend.release) backend.release(backend.context);
        return status;
    }
    auto* created = new (body) FileObject(backend);
    status = created->initialize();
    if (status != NtStatus::Success) {
        ob::ObDereferenceObject(header);
        return status;
    }
    status = ob::detail::currentHandleTable().createHandle(header, *fileHandle, mappedAccess);
    ob::ObDereferenceObject(header);
    return status;
}

NtStatus IoReferenceFileHandle(Handle fileHandle, uint32_t desiredAccess,
                               FileObject*& file, ob::ObjectHeader*& reference) noexcept {
    file = nullptr;
    reference = nullptr;
    ob::HandleTable* table{};
    Handle raw{};
    auto status = ob::detail::resolveHandle(fileHandle, table, raw);
    if (status != NtStatus::Success) return status;
    status = ob::ObReferenceObjectByHandle(raw, *table, desiredAccess, &g_fileType, &reference);
    if (status == NtStatus::Success) file = reference->as<FileObject>();
    return status;
}

NtStatus IoReadFileHandle(Handle fileHandle, bool useCurrentOffset, uint64_t offset,
                          void* buffer, size_t length, size_t* bytesRead) noexcept {
    if (!bytesRead || (!buffer && length)) return NtStatus::InvalidParameter;
    *bytesRead = 0;
    FileObject* file{};
    ob::ObjectHeader* reference{};
    auto status = IoReferenceFileHandle(fileHandle, 0x0001, file, reference);
    if (status != NtStatus::Success) return status;
    status = useCurrentOffset ? file->readCurrent(buffer, length, bytesRead)
                              : file->read(offset, buffer, length, bytesRead);
    ob::ObDereferenceObject(reference);
    return status;
}
} // namespace micant::io
