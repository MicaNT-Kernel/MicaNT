#include "micant/km/executive/ps_objects.hpp"
#include "micant/km/executive/ps.hpp"
#include "micant/km/core/syscalls.hpp"
#include <new>
#include <cstring>

namespace micant::ps::detail {
namespace {
ob::ObjectType processType;
ob::ObjectType threadType;

void deleteProcess(ob::ObjectHeader* header) {
    auto* process = header->as<EProcess>();
    if (process->getDirectoryTableBase() && process->closeAddressSpace() != NtStatus::Success)
        __builtin_trap();
    process->~EProcess();
}

void deleteThread(ob::ObjectHeader* header) {
    header->as<EThread>()->~EThread();
}

void initializeThreadType() noexcept {
    if (threadType.typeId == ob::ObjectTypeId::Thread) return;
    threadType.typeId = ob::ObjectTypeId::Thread;
    threadType.validAccessMask = 0x001fffff;
    threadType.genericMapping = {
        ob::STANDARD_RIGHTS_READ | 0x0008 | 0x0040,
        ob::STANDARD_RIGHTS_WRITE | 0x0001 | 0x0002 | 0x0004 | 0x0010 | 0x0020 | 0x0400,
        ob::STANDARD_RIGHTS_EXECUTE | ob::SYNCHRONIZE | 0x0800,
        0x001fffff
    };
    threadType.deleteProcedure = deleteThread;
}

void initializeProcessType() noexcept {
    if (processType.typeId == ob::ObjectTypeId::Process) return;
    processType.typeId = ob::ObjectTypeId::Process;
    processType.validAccessMask = 0x001fffff;
    processType.genericMapping = {
        ob::STANDARD_RIGHTS_READ | 0x0400 | 0x0010,
        ob::STANDARD_RIGHTS_WRITE | 0x0080 | 0x0002 | 0x0008 | 0x0020 | 0x0040 | 0x0001 | 0x0100 | 0x0200 | 0x0800,
        ob::STANDARD_RIGHTS_EXECUTE | ob::SYNCHRONIZE,
        0x001fffff
    };
    processType.deleteProcedure = deleteProcess;
    processType.deferDelete = deferProcessObjectDelete;
}
}

struct ProcessObjectFactory {
    static void bind(EProcess& process, ob::ObjectHeader* header) noexcept {
        process.objectHeader_ = header;
    }
    static void markSystem(EProcess& process) noexcept { process.systemProcess_ = true; }
    static ob::ObjectHeader* header(EProcess& process) noexcept { return process.objectHeader_; }
    template<typename T>
    static NtStatus capture(std::span<const T> source, std::unique_ptr<T[]>& storage) noexcept {
        if (source.empty()) return NtStatus::Success;
        if (source.size() > SIZE_MAX / sizeof(T) ||
            !hal::amd64::bootstrapKernelRange(reinterpret_cast<uintptr_t>(source.data()),
                                              source.size_bytes(), false))
            return NtStatus::InvalidParameter;
        storage.reset(new (std::nothrow_t{}) T[source.size()]);
        if (!storage) return NtStatus::NoMemory;
        std::memcpy(storage.get(), source.data(), source.size_bytes());
        return NtStatus::Success;
    }
    static NtStatus captureInputs(EProcess& process, std::span<const BootstrapProcessImage> source,
                                  std::u16string_view imagePath, std::u16string_view commandLine,
                                  std::array<BootstrapProcessImage, 16>& captured,
                                  std::u16string_view& capturedPath,
                                  std::u16string_view& capturedCommand) noexcept {
        process.ownedImageInputs_.reset(new (std::nothrow_t{}) EProcess::OwnedImageInput[source.size()]);
        if (!process.ownedImageInputs_) return NtStatus::NoMemory;
        for (size_t i = 0; i < source.size(); ++i) {
            auto& storage = process.ownedImageInputs_[i];
            auto status = capture(std::span{source[i].bytes}, storage.bytes);
            if (status != NtStatus::Success) return status;
            status = capture(std::span{source[i].name.data(), source[i].name.size()}, storage.name);
            if (status != NtStatus::Success) return status;
            status = capture(std::span{source[i].path.data(), source[i].path.size()}, storage.path);
            if (status != NtStatus::Success) return status;
            captured[i] = {{storage.name.get(), source[i].name.size()},
                           {storage.bytes.get(), source[i].bytes.size()},
                           {storage.path.get(), source[i].path.size()}};
        }
        auto status = capture(std::span{imagePath.data(), imagePath.size()}, process.ownedImagePath_);
        if (status != NtStatus::Success) return status;
        status = capture(std::span{commandLine.data(), commandLine.size()}, process.ownedCommandLine_);
        if (status != NtStatus::Success) return status;
        capturedPath = {process.ownedImagePath_.get(), imagePath.size()};
        capturedCommand = {process.ownedCommandLine_.get(), commandLine.size()};
        return NtStatus::Success;
    }
};

struct ThreadObjectFactory {
    static void bind(EThread& thread, ob::ObjectHeader* header) noexcept {
        thread.objectHeader_ = header;
        thread.objectOwned_ = true;
    }
    static ob::ObjectHeader* header(EThread& thread) noexcept { return thread.objectHeader_; }
};

namespace {
EProcess* systemProcess{};
ob::ObjectHeader* systemProcessReference{};

NtStatus getSystemProcess(EProcess*& process) noexcept {
    if (systemProcess) { process = systemProcess; return NtStatus::Success; }
    initializeProcessType();
    void* body{};
    ob::ObjectHeader* header{};
    auto status = ob::ObCreateObject(ob::KernelMode, &processType, nullptr, sizeof(EProcess), &body, &header);
    if (status != NtStatus::Success) return status;
    auto* created = new (body) EProcess{};
    ProcessObjectFactory::bind(*created, header);
    ProcessObjectFactory::markSystem(*created);
    status = created->initializeAddressSpace();
    if (status != NtStatus::Success) { ob::ObDereferenceObject(header); return status; }
    // The system process and its initial reference live for the kernel lifetime.
    systemProcessReference = header;
    systemProcess = created;
    process = created;
    return NtStatus::Success;
}

NtStatus createSystemThreadHandle(EProcess& process, EThread::SystemStartRoutine start,
                                  void* argument, uint32_t access, Handle& handle,
                                  EThread*& thread) noexcept {
    initializeThreadType();
    thread = nullptr;
    const uint32_t mapped = ob::MapGenericAccess(access, threadType.genericMapping);
    if (mapped & ~threadType.validAccessMask) return NtStatus::NotImplemented;
    void* body{};
    ob::ObjectHeader* header{};
    auto status = ob::ObCreateObject(ob::KernelMode, &threadType, nullptr, sizeof(EThread), &body, &header);
    if (status != NtStatus::Success) return status;
    auto* created = new (body) EThread{};
    status = created->initializeSystem(process, 65536, start, argument);
    if (status == NtStatus::Success) {
        header->addReference();
        ThreadObjectFactory::bind(*created, header);
        Handle raw{};
        status = ob::detail::kernelHandleTable().createHandle(header, raw, mapped);
        if (status == NtStatus::Success) {
            handle = ob::detail::encodeKernelHandle(raw);
            thread = created;
        } else if (created->reap() != NtStatus::Success) __builtin_trap();
    }
    ob::ObDereferenceObject(header);
    return status;
}
}

NtStatus createThreadHandle(EProcess& process, size_t stackSize,
                            void (*start)(void*) noexcept, void* argument, uint32_t access,
                            Handle& handle, EThread*& thread) noexcept {
    ob::detail::HandleTableGuard guard;
    initializeThreadType();
    thread = nullptr;
    access = ob::MapGenericAccess(access, threadType.genericMapping);
    if (access & ~threadType.validAccessMask) return NtStatus::NotImplemented;
    void* body{};
    ob::ObjectHeader* header{};
    auto status = ob::ObCreateObject(ob::KernelMode, &threadType, nullptr, sizeof(EThread), &body, &header);
    if (status != NtStatus::Success) return status;
    auto* created = new (body) EThread{};
    status = created->initialize(process, stackSize, start, argument);
    if (status == NtStatus::Success) {
        header->addReference();
        ThreadObjectFactory::bind(*created, header);
        status = ob::detail::currentHandleTable().createHandle(header, handle, access);
        if (status == NtStatus::Success) thread = created;
        else if (created->reap() != NtStatus::Success) __builtin_trap();
    }
    ob::ObDereferenceObject(header);
    return status;
}

NtStatus createUserThreadHandle(EProcess& process, uintptr_t entry, size_t kernelStackSize,
                                size_t userStackSize, bool interruptible, uint32_t access,
                                Handle& handle, EThread*& thread) noexcept {
    ob::detail::HandleTableGuard guard;
    initializeThreadType();
    thread = nullptr;
    access = ob::MapGenericAccess(access, threadType.genericMapping);
    if (access & ~threadType.validAccessMask) return NtStatus::NotImplemented;
    void* body{};
    ob::ObjectHeader* header{};
    auto status = ob::ObCreateObject(ob::KernelMode, &threadType, nullptr, sizeof(EThread), &body, &header);
    if (status != NtStatus::Success) return status;
    auto* created = new (body) EThread{};
    status = created->initializeUser(process, entry, kernelStackSize, userStackSize, interruptible);
    if (status == NtStatus::Success) {
        header->addReference();
        ThreadObjectFactory::bind(*created, header);
        status = ob::detail::currentHandleTable().createHandle(header, handle, access);
        if (status == NtStatus::Success) thread = created;
        else if (created->reap() != NtStatus::Success) __builtin_trap();
    }
    ob::ObDereferenceObject(header);
    return status;
}

NtStatus createProcessHandle(uint32_t access, Handle& handle, EProcess*& process) noexcept {
    ob::detail::HandleTableGuard guard;
    initializeProcessType();
    process = nullptr;
    access = ob::MapGenericAccess(access, processType.genericMapping);
    if (access & ~processType.validAccessMask) return NtStatus::NotImplemented;
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

NtStatus createBootstrapUserProcess(std::span<const BootstrapProcessImage> images,
                                    std::u16string_view imagePath, std::u16string_view commandLine,
                                    size_t kernelStackSize, size_t userStackSize, bool interruptible,
                                    uint32_t processAccess, uint32_t threadAccess,
                                    Handle& processHandle, Handle& threadHandle) noexcept {
    processHandle = threadHandle = 0;
    if (images.empty() || images.size() > 16 ||
        !hal::amd64::bootstrapKernelRange(reinterpret_cast<uintptr_t>(images.data()),
                                          images.size_bytes(), false))
        return NtStatus::InvalidParameter;
    uint64_t flags{}, previousRoot{};
    __asm__ __volatile__("pushfq; popq %0; cli; movq %%cr3, %1"
                         : "=r"(flags), "=r"(previousRoot) : : "memory");
    Handle processResult{}, threadResult{};
    EProcess* process{};
    EThread* thread{};
    auto status = createProcessHandle(processAccess, processResult, process);
    std::array<BootstrapProcessImage, 16> captured{};
    std::u16string_view capturedPath, capturedCommand;
    if (status == NtStatus::Success)
        status = ProcessObjectFactory::captureInputs(*process, images, imagePath, commandLine,
                                                     captured, capturedPath, capturedCommand);
    pe::PeImageView64 executable;
    if (status == NtStatus::Success && captured[0].name.empty()) status = NtStatus::InvalidParameter;
    if (status == NtStatus::Success) status = executable.inspect(captured[0].bytes);
    uintptr_t entry{};
    if (status == NtStatus::Success) {
        const auto& optional = executable.headers().optionalHeader;
        if (!optional.addressOfEntryPoint || optional.imageBase > UINTPTR_MAX - optional.addressOfEntryPoint)
            status = NtStatus::InvalidImageFormat;
        else entry = optional.imageBase + optional.addressOfEntryPoint;
    }
    bool targetActive = false;
    if (status == NtStatus::Success) {
        targetActive = hal::amd64::switchProcessPagingRootByPhysical(process->getDirectoryTableBase());
        if (!targetActive) status = NtStatus::Unsuccessful;
    }
    if (status == NtStatus::Success) {
        for (const auto& image : std::span{captured.data(), images.size()}) {
            status = process->loadNativeImage(image.name, image.bytes, image.path);
            if (status != NtStatus::Success) break;
        }
    }
    if (status == NtStatus::Success) status = process->setNativeProcessParameters(capturedPath, capturedCommand);
    if (status == NtStatus::Success) status = process->finalizeNativeImages();
    if (status == NtStatus::Success)
        status = createUserThreadHandle(*process, entry, kernelStackSize, userStackSize,
                                        interruptible, threadAccess, threadResult, thread);
    if (targetActive && !hal::amd64::switchProcessPagingRootByPhysical(previousRoot)) __builtin_trap();
    if (status == NtStatus::Success) status = thread->ready();
    if (status != NtStatus::Success) {
        if (thread) {
            if (thread->cancelBeforeStart() != NtStatus::Success || thread->reap() != NtStatus::Success)
                __builtin_trap();
            if (sys::NtClose(threadResult) != NtStatus::Success) __builtin_trap();
        }
        if (processResult && sys::NtClose(processResult) != NtStatus::Success) __builtin_trap();
    } else {
        processHandle = processResult;
        threadHandle = threadResult;
    }
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return status;
}

NtStatus referenceProcessHandle(Handle handle, uint32_t access, EProcess*& process,
                                ob::ObjectHeader*& reference) noexcept {
    initializeProcessType();
    process = nullptr;
    reference = nullptr;
    ob::HandleTable* table{};
    Handle raw{};
    auto status = ob::detail::resolveHandle(handle, table, raw);
    if (status == NtStatus::Success)
        status = ob::ObReferenceObjectByHandle(raw, *table, access, &processType, &reference);
    if (status == NtStatus::Success) process = reference->as<EProcess>();
    return status;
}

NtStatus referenceThreadHandle(Handle handle, uint32_t access, EThread*& thread,
                               ob::ObjectHeader*& reference) noexcept {
    initializeThreadType();
    thread = nullptr;
    reference = nullptr;
    ob::HandleTable* table{};
    Handle raw{};
    auto status = ob::detail::resolveHandle(handle, table, raw);
    if (status == NtStatus::Success)
        status = ob::ObReferenceObjectByHandle(raw, *table, access, &threadType, &reference);
    if (status == NtStatus::Success) thread = reference->as<EThread>();
    return status;
}

NtStatus referenceCurrentProcess(EProcess*& process, ob::ObjectHeader*& reference) noexcept {
    ob::detail::HandleTableGuard guard;
    auto* current = ke::KeGetCurrentThread();
    process = current ? current->owningProcess : nullptr;
    reference = process ? ProcessObjectFactory::header(*process) : nullptr;
    if (!reference) { process = nullptr; return NtStatus::InvalidHandle; }
    reference->addReference();
    return NtStatus::Success;
}

NtStatus referenceCurrentThread(EThread*& thread, ob::ObjectHeader*& reference) noexcept {
    ob::detail::HandleTableGuard guard;
    auto* current = ke::KeGetCurrentThread();
    thread = current ? current->owningThread : nullptr;
    reference = thread ? ThreadObjectFactory::header(*thread) : nullptr;
    if (!reference) { thread = nullptr; return NtStatus::InvalidHandle; }
    reference->addReference();
    return NtStatus::Success;
}

bool isProcessObject(const ob::ObjectHeader& header) noexcept {
    return header.type == &processType;
}

bool isThreadObject(const ob::ObjectHeader& header) noexcept {
    return header.type == &threadType;
}

uint32_t bootstrapProcessObjectCount() noexcept {
    return processType.totalNumberOfObjects.load(std::memory_order_relaxed);
}

uint32_t bootstrapThreadObjectCount() noexcept {
    return threadType.totalNumberOfObjects.load(std::memory_order_relaxed);
}
}

namespace micant::ps {
extern "C" __declspec(dllexport) NtStatus PsCreateSystemThread(
    Handle* out, uint32_t access, ObjectAttributes* attributes, Handle processHandle,
    ClientId* clientId, void (*start)(void*), void* context) noexcept {
    if (!out || !start || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL)
        return NtStatus::InvalidParameter;
    if (attributes) {
        if (attributes->length != sizeof(ObjectAttributes)) return NtStatus::InvalidParameter;
        if (attributes->rootDirectory || attributes->objectName || attributes->securityDescriptor ||
            attributes->securityQualityOfService) return NtStatus::NotImplemented;
        if (attributes->attributes & ~OBJ_KERNEL_HANDLE) return NtStatus::InvalidParameter;
    }
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    EProcess* process{};
    ob::ObjectHeader* reference{};
    NtStatus status{};
    if (!processHandle) {
        status = detail::getSystemProcess(process);
    } else if (processHandle == InvalidHandleValue) {
        auto* current = ke::KeGetCurrentThread();
        process = current ? current->owningProcess : nullptr;
        status = process ? NtStatus::Success : detail::getSystemProcess(process);
    } else {
        ob::HandleTable* table{};
        Handle raw{};
        status = ob::detail::resolveHandle(processHandle, table, raw, true);
        if (status == NtStatus::Success) {
            detail::initializeProcessType();
            status = ob::ObReferenceObjectByHandle(raw, *table, 0x0002, &detail::processType, &reference);
            if (status == NtStatus::Success) process = reference->as<EProcess>();
        }
    }
    Handle handle{};
    EThread* thread{};
    if (status == NtStatus::Success && process->isTerminating()) status = NtStatus::ProcessIsTerminating;
    if (status == NtStatus::Success) status = detail::createSystemThreadHandle(*process, start, context, access, handle, thread);
    if (status == NtStatus::Success) {
        status = thread->ready();
        if (status != NtStatus::Success) {
            if (thread->cancelBeforeStart() != NtStatus::Success || thread->reap() != NtStatus::Success)
                __builtin_trap();
            ob::HandleTable* table{}; Handle raw{};
            if (ob::detail::resolveHandle(handle, table, raw, true) != NtStatus::Success ||
                table->closeHandle(raw) != NtStatus::Success) __builtin_trap();
        } else {
            if (clientId) *clientId = {process->processId(), thread->kernelThread()->tid};
            *out = handle;
        }
    }
    if (reference) ob::ObDereferenceObject(reference);
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return status;
}
}
