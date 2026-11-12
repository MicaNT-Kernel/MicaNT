#include "micant/km/executive/io_file.hpp"
#include "micant/km/executive/ob.hpp"
#if defined(MICANT_FREESTANDING)
#include "micant/km/core/passive_work.hpp"
#include "micant/km/executive/ps.hpp"
#include "micant/km/executive/ps_objects.hpp"
#include "micant/km/executive/probe.hpp"
#endif
#include <new>

namespace micant::sys { NtStatus referenceBootstrapEventForIo(Handle, ob::ObjectHeader*&) noexcept; }

namespace micant::io {
namespace {
ob::ObjectType g_fileType{};
ob::ObjectType g_completionType{};
FileOpenRoutine g_fileOpen{};
void* g_fileOpenContext{};
std::atomic_flag g_fileBackendLock = ATOMIC_FLAG_INIT;

void lockFileBackend() noexcept {
    while (g_fileBackendLock.test_and_set(std::memory_order_acquire)) __asm__ __volatile__("pause");
}
void unlockFileBackend() noexcept { g_fileBackendLock.clear(std::memory_order_release); }

void deleteCompletionPort(ob::ObjectHeader* header) noexcept {
    header->as<IoCompletionPort>()->~IoCompletionPort();
}
void deleteFileObject(ob::ObjectHeader* header) noexcept {
    header->as<FileObject>()->~FileObject();
}
void closeFileHandle(ob::ObjectHeader* header, Handle) {
    if (header->handleCount.load(std::memory_order_acquire) == 0)
        header->as<FileObject>()->notifyLastHandleClosed();
}
#if defined(MICANT_FREESTANDING)
// Requests remain on the issuing thread's kernel stack until completion.  The
// scheduler is currently BSP-only; interrupt exclusion protects the list.
struct PendingRead {
    PendingRead* next{};
    FileObject* file{};
    ke::KThread* thread{};
    void* process{};
    const void* statusBlock{};
    bool cancelled{};
};
PendingRead* pendingReads{};
AsyncRead* asyncReads{};
AsyncRead* completedReads{};
ke::passive_work::Item asyncCompletionWork{};
void finishAsyncReads(void*) noexcept;
void cancelOwnedRead(DeviceObject*, Irp* irp) {
    auto* request = reinterpret_cast<AsyncRead*>(irp);
    const auto previousIrql = irp->cancelIrql;
    IoReleaseCancelSpinLock(previousIrql);
    if (request && request->file && request->file->cancelReadRoutine())
        (void)request->file->cancelReadRoutine()(request->file->context(), request);
}
void cleanupAsyncRead(AsyncRead* request) noexcept {
#if defined(MICANT_BAREMETAL)
    if (request->completionPacket) delete static_cast<IoCompletionPort::Node*>(request->completionPacket);
#endif
    if (request->userApc) delete request->userApc;
    if (request->threadReference) ob::ObDereferenceObject(request->threadReference);
    if (request->completionReference) ob::ObDereferenceObject(request->completionReference);
    if (request->eventReference) ob::ObDereferenceObject(request->eventReference);
    if (request->processReference) ob::ObDereferenceObject(request->processReference);
    if (request->fileReference) ob::ObDereferenceObject(request->fileReference);
    delete[] request->buffer;
    delete request;
}
uint64_t disableReadInterrupts() noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}
void restoreReadInterrupts(uint64_t flags) noexcept {
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
}
ob::ObjectHeader* deferredFileDeletes{};
ke::passive_work::Item fileDeleteWork{};
void finishDeferredFiles(void*) noexcept {
    for (;;) {
        uint64_t flags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        auto* header = deferredFileDeletes;
        if (header) deferredFileDeletes = header->deferredDeleteNext;
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        if (!header) return;
        header->deferredDeleteNext = nullptr;
        ob::ObFinishDelete(header);
    }
}
bool deferFileDelete(ob::ObjectHeader* header) noexcept {
    if (ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL && !ke::g_CurrentCpu.dpcRoutineActive) return false;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    header->deferredDeleteNext = deferredFileDeletes;
    deferredFileDeletes = header;
    fileDeleteWork.routine = finishDeferredFiles;
    if (!fileDeleteWork.queued && !ke::passive_work::queue(fileDeleteWork)) __builtin_trap();
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return true;
}
#endif
}

NtStatus FileObject::associateCompletion(ob::ObjectHeader* port, uintptr_t key) noexcept {
    if (!port || backend_.synchronous) return NtStatus::InvalidParameter;
#if defined(MICANT_FREESTANDING)
    ke::SpinLockGuard guard(completionLock_);
#else
    std::lock_guard<std::mutex> guard(completionLock_);
#endif
    if (completionReference_) return NtStatus::InvalidParameter;
    port->addReference(); completionKey_ = key; completionReference_ = port;
    return NtStatus::Success;
}
ob::ObjectHeader* FileObject::referenceCompletion(uintptr_t& key) noexcept {
#if defined(MICANT_FREESTANDING)
    ke::SpinLockGuard guard(completionLock_);
#else
    std::lock_guard<std::mutex> guard(completionLock_);
#endif
    auto* reference = completionReference_;
    if (reference) { reference->addReference(); key = completionKey_; }
    return reference;
}
NtStatus IoAssociateCompletionPort(Handle fileHandle, Handle portHandle, uintptr_t key) noexcept {
    FileObject* file{}; ob::ObjectHeader* fileReference{};
    auto status = IoReferenceFileHandle(fileHandle, 0, file, fileReference);
    if (status != NtStatus::Success) return status;
    ob::HandleTable* table{}; Handle raw{}; ob::ObjectHeader* portReference{};
    status = ob::detail::resolveHandle(portHandle, table, raw);
    if (status == NtStatus::Success)
        status = ob::ObReferenceObjectByHandle(raw, *table, 2, &g_completionType, &portReference);
    if (status == NtStatus::Success) {
        status = file->associateCompletion(portReference, key);
        ob::ObDereferenceObject(portReference);
    }
    ob::ObDereferenceObject(fileReference);
    return status;
}
NtStatus IoHasCompletionPort(Handle fileHandle, bool& associated) noexcept {
    associated = false;
    FileObject* file{}; ob::ObjectHeader* reference{};
    auto status = IoReferenceFileHandle(fileHandle, 1, file, reference);
    if (status != NtStatus::Success) return status;
    uintptr_t key{}; auto* port = file->referenceCompletion(key);
    associated = port != nullptr;
    if (port) ob::ObDereferenceObject(port);
    ob::ObDereferenceObject(reference);
    return NtStatus::Success;
}

NtStatus IoInitializeFileObjects() noexcept {
    g_completionType.typeId = ob::ObjectTypeId::IoCompletion;
    g_completionType.validAccessMask = 0x001F0003;
    g_completionType.genericMapping = {0x00120001, 0x00120002, 0x00120000, 0x001F0003};
    g_completionType.deleteProcedure = deleteCompletionPort;
    g_fileType.typeId = ob::ObjectTypeId::File;
    g_fileType.validAccessMask = 0x001F01FF;
    g_fileType.genericMapping = {0x00120089, 0x00120116, 0x001200A0, 0x001F01FF};
    g_fileType.deleteProcedure = deleteFileObject;
    g_fileType.closeProcedure = closeFileHandle;
#if defined(MICANT_FREESTANDING)
    g_fileType.deferDelete = deferFileDelete;
    g_completionType.deferDelete = deferFileDelete;
#endif
    return NtStatus::Success;
}

NtStatus IoCreateCompletionPort(Handle* handle, uint32_t access, uint32_t concurrency) noexcept {
    if (!handle) return NtStatus::InvalidParameter;
    const auto mapped = ob::MapGenericAccess(access, g_completionType.genericMapping);
    if (mapped & ~g_completionType.validAccessMask) return NtStatus::AccessDenied;
    // Scheduling concurrency is not yet implemented beyond the single BSP.
    if (concurrency > 1) return NtStatus::NotSupported;
    void* body{}; ob::ObjectHeader* header{};
    auto status = ob::ObCreateObject(ob::KernelMode, &g_completionType, nullptr,
                                     sizeof(IoCompletionPort), &body, &header);
    if (status != NtStatus::Success) return status;
    new (body) IoCompletionPort(concurrency);
    status = ob::detail::currentHandleTable().createHandle(header, *handle, mapped);
    ob::ObDereferenceObject(header);
    return status;
}
NtStatus IoPostCompletion(Handle handle, uintptr_t key, uintptr_t context,
                          NtStatus result, uintptr_t information) noexcept {
    ob::HandleTable* table{}; Handle raw{}; ob::ObjectHeader* reference{};
    auto status = ob::detail::resolveHandle(handle, table, raw);
    if (status == NtStatus::Success)
        status = ob::ObReferenceObjectByHandle(raw, *table, 2, &g_completionType, &reference);
    if (status != NtStatus::Success) return status;
    status = reference->as<IoCompletionPort>()->postCompletion(key, context, result, information);
    ob::ObDereferenceObject(reference);
    return status;
}
NtStatus IoRemoveCompletion(Handle handle, uintptr_t& key, uintptr_t& context,
                            IoStatusBlock& result, uint64_t timeout100ns) noexcept {
    ob::HandleTable* table{}; Handle raw{}; ob::ObjectHeader* reference{};
    auto status = ob::detail::resolveHandle(handle, table, raw);
    if (status == NtStatus::Success)
        status = ob::ObReferenceObjectByHandle(raw, *table, 1, &g_completionType, &reference);
    if (status != NtStatus::Success) return status;
    uint64_t completionKey{};
#if defined(MICANT_FREESTANDING) && defined(MICANT_BAREMETAL)
    auto* thread = ke::KeGetCurrentThread();
    if (timeout100ns && (!thread || thread->eventWaitInProgress || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL)) {
        ob::ObDereferenceObject(reference); return NtStatus::InvalidParameter;
    }
    if (timeout100ns) {
        thread->eventWaitInProgress = true;
        thread->eventWaitUserMode = thread->previousUserMode;
        thread->eventWaitCanTerminate = true;
    }
#endif
#if defined(MICANT_FREESTANDING) && defined(MICANT_BAREMETAL)
    status = reference->as<IoCompletionPort>()->removeCompletionTimed(completionKey, context, result, timeout100ns);
#else
    if (timeout100ns != 0 && timeout100ns != UINT64_MAX) status = NtStatus::NotSupported;
    else status = reference->as<IoCompletionPort>()->removeCompletion(completionKey, context, result,
                                                                     timeout100ns ? 0xffffffffu : 0);
#endif
#if defined(MICANT_FREESTANDING) && defined(MICANT_BAREMETAL)
    if (timeout100ns) {
        thread->eventWaitInProgress = false;
        thread->eventWaitUserMode = false;
        thread->eventWaitCanTerminate = false;
    }
#endif
    key = static_cast<uintptr_t>(completionKey);
    ob::ObDereferenceObject(reference);
    return status;
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

NtStatus IoUnregisterFileOpenRoutine(FileOpenRoutine open, void* context) noexcept {
    if (!open) return NtStatus::InvalidParameter;
    lockFileBackend();
    if (g_fileOpen != open || g_fileOpenContext != context) {
        unlockFileBackend();
        return NtStatus::ObjectNameNotFound;
    }
    g_fileOpen = nullptr;
    g_fileOpenContext = nullptr;
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

NtStatus IoCreateDeviceFile(Handle* fileHandle, uint32_t desiredAccess,
                            DeviceObject* deviceObject, const FileBackend& backend) noexcept {
    const uint32_t mappedAccess = ob::MapGenericAccess(desiredAccess, g_fileType.genericMapping);
    if (!fileHandle || !deviceObject || !backend.context || !backend.release ||
        (mappedAccess & ~g_fileType.validAccessMask) ||
        (mappedAccess & ~0x0012008bu)) {
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
    auto* created = new (body) FileObject(backend, deviceObject);
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
                          void* buffer, size_t length, size_t* bytesRead, const void* requestStatus) noexcept {
    if (!bytesRead || (!buffer && length)) return NtStatus::InvalidParameter;
    *bytesRead = 0;
    FileObject* file{};
    ob::ObjectHeader* reference{};
    auto status = IoReferenceFileHandle(fileHandle, 0x0001, file, reference);
    if (status != NtStatus::Success) return status;
    if (file->isStream() && !useCurrentOffset) {
        ob::ObDereferenceObject(reference);
        return NtStatus::InvalidParameter;
    }
#if defined(MICANT_FREESTANDING) && defined(MICANT_BAREMETAL)
    auto* current = file->isStream() ? ke::KeGetCurrentThread() : nullptr;
    if (file->isStream()) {
        if (!current || current->eventWaitInProgress || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL) {
            ob::ObDereferenceObject(reference);
            return NtStatus::InvalidParameter;
        }
        current->eventWaitInProgress = true;
        current->eventWaitUserMode = current->previousUserMode;
        current->eventWaitCanTerminate = true;
    }
#endif
#if defined(MICANT_FREESTANDING) && defined(MICANT_BAREMETAL)
    PendingRead request{};
    if (current) {
        request.file = file;
        request.thread = current;
        request.process = ke::KeGetCurrentProcess();
        request.statusBlock = requestStatus;
        const auto flags = disableReadInterrupts();
        request.next = pendingReads; pendingReads = &request;
        restoreReadInterrupts(flags);
    }
#endif
    status = useCurrentOffset ? file->readCurrent(buffer, length, bytesRead)
                              : file->read(offset, buffer, length, bytesRead);
#if defined(MICANT_FREESTANDING) && defined(MICANT_BAREMETAL)
    if (current) {
        const auto flags = disableReadInterrupts();
        auto** link = &pendingReads;
        while (*link && *link != &request) link = &(*link)->next;
        if (*link) *link = request.next;
        restoreReadInterrupts(flags);
        current->eventWaitInProgress = false;
        current->eventWaitUserMode = false;
        current->eventWaitCanTerminate = false;
    }
#endif
    ob::ObDereferenceObject(reference);
    return status;
}
#if defined(MICANT_FREESTANDING) && defined(MICANT_BAREMETAL)
namespace {
void finishAsyncReads(void*) noexcept {
    for (;;) {
        const auto flags = disableReadInterrupts();
        auto* request = completedReads;
        if (request) completedReads = request->completedNext;
        restoreReadInterrupts(flags);
        if (!request) return;
        request->irp.ioStatus = request->result;
        // A completion routine returning MORE_PROCESSING_REQUIRED owns the
        // IRP and may already have freed it. Do not touch it in that case.
        if (IoCompleteRequest(&request->irp) == NtStatus::MoreProcessingRequired) continue;
        auto* process = static_cast<ps::EProcess*>(request->process);
        auto* thread = ke::KeGetCurrentThread();
        const auto depth = thread ? thread->processAttachDepth : 0;
        ke::KAPC_STATE state{};
        ke::KeStackAttachProcess(process, &state);
        const bool attached = thread && thread->processAttachDepth == depth + 1 &&
                              ke::KeGetCurrentProcess() == process;
        if (!attached) {
            request->result = {NtStatus::Cancelled, 0};
        } else {
            // The bootstrap probes require the address space to stay stable
            // between probing and copy. The scheduler is BSP-only.
            const auto copyFlags = disableReadInterrupts();
            if (request->result.information &&
                ex::ExSafeCopyToUser(request->userBuffer, request->buffer,
                                     request->result.information) != NtStatus::Success) {
                request->result.status = NtStatus::AccessViolation;
                request->result.information = 0;
            }
            (void)ex::ExSafeCopyToUser(request->userStatus, &request->result, sizeof(request->result));
            restoreReadInterrupts(copyFlags);
            ke::KeUnstackDetachProcess(&state);
        }
        if (request->eventReference)
            (void)request->eventReference->as<sync::detail::BootstrapEvent>()->set();
        if (attached && request->userApc && request->threadReference) {
            const auto apcFlags = disableReadInterrupts();
            auto* target = request->threadReference->as<ps::EThread>()->kernelThread();
            ke::KeInitializeApc(request->userApc, target, ke::ApcEnvironment::OriginalApcEnvironment,
                ke::passive_work::ownedUserApcKernel, ke::passive_work::ownedUserApcRundown,
                reinterpret_cast<ke::PKNORMAL_ROUTINE>(request->userApcRoutine), ke::UserMode,
                reinterpret_cast<void*>(request->userApcContext));
            if (ke::KeInsertQueueApc(request->userApc, request->userStatus, nullptr))
                request->userApc = nullptr; // Ke now owns the queued APC.
            restoreReadInterrupts(apcFlags);
        }
        if (request->completionPacket) {
            auto* packet = static_cast<IoCompletionPort::Node*>(request->completionPacket);
            request->completionPacket = nullptr;
            request->completionReference->as<IoCompletionPort>()->postReservedCompletion(
                packet, request->completionKey, request->completionContext,
                request->result.status, request->result.information);
        }
        cleanupAsyncRead(request);
    }
}
}
#endif

void IoCompleteAsyncRead(AsyncRead* request, NtStatus status, size_t information) noexcept {
#if defined(MICANT_FREESTANDING) && defined(MICANT_BAREMETAL)
    if (!request) return;
    const auto flags = disableReadInterrupts();
    if (!request->pending) { restoreReadInterrupts(flags); return; }
    auto** link = &asyncReads;
    while (*link && *link != request) link = &(*link)->pendingNext;
    if (*link != request) { restoreReadInterrupts(flags); return; }
    *link = request->pendingNext;
    request->pending = false;
    ke::KIRQL oldIrql{};
    IoAcquireCancelSpinLock(&oldIrql);
    (void)IoSetCancelRoutine(&request->irp, nullptr);
    IoReleaseCancelSpinLock(oldIrql);
    if (information > request->length) { status = NtStatus::InvalidParameter; information = 0; }
    request->result = {status, information};
    request->completedNext = completedReads; completedReads = request;
    asyncCompletionWork.routine = finishAsyncReads;
    if (!asyncCompletionWork.queued && !ke::passive_work::queue(asyncCompletionWork)) __builtin_trap();
    restoreReadInterrupts(flags);
#else
    (void)request; (void)status; (void)information;
#endif
}

NtStatus IoSubmitAsyncRead(Handle handle, Handle event, uintptr_t apcRoutine, uintptr_t completionContext,
                           void* userBuffer, size_t length, IoStatusBlock* userStatus) noexcept {
#if defined(MICANT_FREESTANDING) && defined(MICANT_BAREMETAL)
    if (!userStatus || (!userBuffer && length) || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL)
        return NtStatus::InvalidParameter;
    auto* request = new (std::nothrow_t{}) AsyncRead{};
    if (!request) return NtStatus::NoMemory;
    auto status = IoReferenceFileHandle(handle, 1, request->file, request->fileReference);
    if (status != NtStatus::Success) { cleanupAsyncRead(request); return status; }
    if (request->file->isSynchronous() || !request->file->submitReadRoutine() || !request->file->cancelReadRoutine()) {
        cleanupAsyncRead(request); return NtStatus::NotSupported;
    }
    ps::EProcess* process{};
    status = ps::detail::referenceCurrentProcess(process, request->processReference);
    if (status != NtStatus::Success) { cleanupAsyncRead(request); return status; }
    request->process = process;
    if (event) {
        status = sys::referenceBootstrapEventForIo(event, request->eventReference);
        if (status != NtStatus::Success) { cleanupAsyncRead(request); return status; }
    }
    request->completionReference = request->file->referenceCompletion(request->completionKey);
    request->completionContext = completionContext;
    if (apcRoutine) {
        if (request->completionReference) { cleanupAsyncRead(request); return NtStatus::InvalidParameter; }
        const auto flags = disableReadInterrupts();
        const bool executable = hal::amd64::bootstrapUserRange(apcRoutine, 1, false, true);
        restoreReadInterrupts(flags);
        if (!executable) { cleanupAsyncRead(request); return NtStatus::AccessViolation; }
        ps::EThread* thread{};
        status = ps::detail::referenceCurrentThread(thread, request->threadReference);
        if (status != NtStatus::Success) { cleanupAsyncRead(request); return status; }
        request->userApc = new (std::nothrow_t{}) ke::KAPC{};
        if (!request->userApc) { cleanupAsyncRead(request); return NtStatus::NoMemory; }
        request->userApcRoutine = apcRoutine; request->userApcContext = completionContext;
    }
    if (request->completionReference && completionContext) {
        request->completionPacket = IoCompletionPort::reserveCompletion();
        if (!request->completionPacket) { cleanupAsyncRead(request); return NtStatus::NoMemory; }
    }
    if (!request->eventReference && !request->completionPacket && !request->userApc) {
        cleanupAsyncRead(request); return NtStatus::InvalidParameter;
    }
    request->length = length; request->userBuffer = userBuffer; request->userStatus = userStatus;
    request->irp.majorFunction = IRP_MJ_READ;
    request->irp.fileObject = request->file;
    request->irp.deviceObject = request->file->deviceObject();
    request->irp.length = static_cast<uint32_t>(length);
    if (length) {
        request->buffer = new (std::nothrow_t{}) uint8_t[length];
        if (!request->buffer) { cleanupAsyncRead(request); return NtStatus::NoMemory; }
        request->irp.systemBuffer = request->buffer;
    }
    const IoStatusBlock initial{NtStatus::Pending, 0};
    const auto flags = disableReadInterrupts();
    if (ex::ExSafeCopyToUser(userStatus, &initial, sizeof(initial)) != NtStatus::Success) {
        restoreReadInterrupts(flags); cleanupAsyncRead(request); return NtStatus::AccessViolation;
    }
    if (request->eventReference)
        (void)request->eventReference->as<sync::detail::BootstrapEvent>()->reset();
    IoMarkIrpPending(&request->irp);
    request->pending = true; request->pendingNext = asyncReads; asyncReads = request;
    ke::KIRQL cancelIrql{};
    IoAcquireCancelSpinLock(&cancelIrql);
    (void)IoSetCancelRoutine(&request->irp, cancelOwnedRead);
    IoReleaseCancelSpinLock(cancelIrql);
    status = request->file->submitReadRoutine()(request->file->context(), request);
    if (status != NtStatus::Pending && request->pending) {
        auto** link = &asyncReads;
        while (*link && *link != request) link = &(*link)->pendingNext;
        if (*link) *link = request->pendingNext;
        request->pending = false;
        IoAcquireCancelSpinLock(&cancelIrql);
        (void)IoSetCancelRoutine(&request->irp, nullptr);
        IoReleaseCancelSpinLock(cancelIrql);
        restoreReadInterrupts(flags); cleanupAsyncRead(request); return status;
    }
    restoreReadInterrupts(flags);
    return NtStatus::Pending;
#else
    (void)handle; (void)event; (void)apcRoutine; (void)completionContext; (void)userBuffer; (void)length; (void)userStatus;
    return NtStatus::NotSupported;
#endif
}

bool IoCurrentReadCancelled() noexcept {
#if defined(MICANT_FREESTANDING) && defined(MICANT_BAREMETAL)
    const auto flags = disableReadInterrupts();
    auto* thread = ke::KeGetCurrentThread();
    bool cancelled = false;
    for (auto* request = pendingReads; request; request = request->next)
        if (request->thread == thread) { cancelled = request->cancelled; break; }
    restoreReadInterrupts(flags);
    return cancelled;
#else
    return false;
#endif
}

NtStatus IoCancelFileReads(Handle handle, const void* statusBlock) noexcept {
#if defined(MICANT_FREESTANDING) && defined(MICANT_BAREMETAL)
    FileObject* file{}; ob::ObjectHeader* reference{};
    const auto status = IoReferenceFileHandle(handle, 0, file, reference);
    if (status != NtStatus::Success) return status;
    auto* current = ke::KeGetCurrentThread();
    if (!current || !ke::KeGetCurrentProcess()) { ob::ObDereferenceObject(reference); return NtStatus::InvalidParameter; }
    const auto flags = disableReadInterrupts();
    bool found = false, wakeSynchronous = false;
    for (auto* request = pendingReads; request; request = request->next) {
        if (file->canCancelRead() && request->file == file && request->process == ke::KeGetCurrentProcess() &&
            (!statusBlock || request->statusBlock == statusBlock) && !request->cancelled) {
            request->cancelled = true; found = wakeSynchronous = true;
        }
    }
    for (auto* request = asyncReads; request;) {
        auto* next = request->pendingNext;
        if (request->file == file && request->process == ke::KeGetCurrentProcess() &&
            (!statusBlock || request->userStatus == statusBlock) && IoCancelIrp(&request->irp)) found = true;
        request = next;
    }
    restoreReadInterrupts(flags);
    if (wakeSynchronous) file->wakeCancelledRead();
    ob::ObDereferenceObject(reference);
    return found ? NtStatus::Success : NtStatus::NotFound;
#else
    (void)handle; (void)statusBlock; return NtStatus::NotSupported;
#endif
}
} // namespace micant::io
