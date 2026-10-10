#include "micant/km/core/syscalls.hpp"
#include "micant/km/executive/ob.hpp"
#include "micant/km/executive/mm.hpp"
#include "micant/km/drivers/fs.hpp"
#include "micant/km/core/sync.hpp"
#include "micant/km/core/timer.hpp"
#include "micant/km/executive/po.hpp"
#include "micant/km/executive/section.hpp"
#include "micant/km/hal/hal.hpp"
#include <iostream>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <cwchar>

namespace micant::sys {

// Global simulated kernel state
static ob::HandleTable g_KernelHandleTable;
static mm::ProcessAddressSpace g_KernelAddressSpace;
static std::unordered_map<Handle, std::shared_ptr<fs::FileObject>> g_KernelFiles;
static std::unordered_map<Handle, std::shared_ptr<section::SectionObject>> g_KernelSections;
static Handle g_NextFileHandle = 0x200;
static Handle g_NextSectionHandle = 0x300;

NtStatus NtAllocateVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    uintptr_t /*zeroBits*/,
    size_t* regionSize,
    uint32_t allocationType,
    uint32_t protect
) {
    if (!baseAddress || !regionSize || *regionSize == 0) {
        return NtStatus::InvalidParameter;
    }

    uintptr_t addr = *baseAddress;
    NtStatus status = g_KernelAddressSpace.allocate(addr, *regionSize, allocationType, protect);
    if (NT_SUCCESS(status)) {
        *baseAddress = addr;
    }
    return status;
}

NtStatus NtFreeVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    size_t* regionSize,
    uint32_t freeType
) {
    if (!baseAddress || *baseAddress == 0) {
        return NtStatus::InvalidParameter;
    }

    size_t sz = regionSize ? *regionSize : 0;
    return g_KernelAddressSpace.free(*baseAddress, sz, freeType);
}

NtStatus NtProtectVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    size_t* regionSize,
    uint32_t newProtect,
    uint32_t* oldProtect
) {
    if (!baseAddress || !regionSize) {
        return NtStatus::InvalidParameter;
    }
    if (oldProtect) {
        *oldProtect = mm::PAGE_READWRITE;
    }
    return NtStatus::Success;
}

NtStatus NtCreateFile(
    Handle* fileHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    IoStatusBlock* ioStatusBlock,
    LargeInteger* /*allocationSize*/,
    uint32_t /*fileAttributes*/,
    uint32_t /*shareAccess*/,
    uint32_t createDisposition,
    uint32_t /*createOptions*/,
    void* /*eaBuffer*/,
    uint32_t /*eaLength*/
) {
    if (!fileHandle || !objectAttributes || !objectAttributes->objectName) {
        return NtStatus::InvalidParameter;
    }

    std::shared_ptr<fs::FileObject> fileObj;
    NtStatus status = fs::VirtualFileSystem::get().createOrOpenFile(
        objectAttributes->objectName->view(),
        desiredAccess,
        createDisposition,
        fileObj
    );

    if (!NT_SUCCESS(status)) {
        if (ioStatusBlock) {
            ioStatusBlock->status = status;
            ioStatusBlock->information = 0;
        }
        return status;
    }

    Handle h = g_NextFileHandle;
    g_NextFileHandle += 4;
    g_KernelFiles[h] = fileObj;
    *fileHandle = h;

    if (ioStatusBlock) {
        ioStatusBlock->status = NtStatus::Success;
        ioStatusBlock->information = (createDisposition == fs::FILE_CREATE) ? 2 : 1; // FILE_CREATED / FILE_OPENED
    }
    return NtStatus::Success;
}

NtStatus NtOpenFile(
    Handle* fileHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    IoStatusBlock* ioStatusBlock,
    uint32_t /*shareAccess*/,
    uint32_t /*openOptions*/
) {
    return NtCreateFile(
        fileHandle,
        desiredAccess,
        objectAttributes,
        ioStatusBlock,
        nullptr,
        fs::FILE_ATTRIBUTE_NORMAL,
        0,
        fs::FILE_OPEN,
        0,
        nullptr,
        0
    );
}

NtStatus NtCreateNamedPipeFile(
    Handle* fileHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    IoStatusBlock* ioStatusBlock,
    uint32_t /*shareAccess*/,
    uint32_t /*createDisposition*/,
    uint32_t /*createOptions*/,
    uint32_t namedPipeType,
    uint32_t readMode,
    uint32_t completionMode,
    uint32_t maximumInstances,
    uint32_t inboundQuota,
    uint32_t outboundQuota,
    LargeInteger* defaultTimeout
) {
    if (!fileHandle || !objectAttributes || !objectAttributes->objectName) {
        return NtStatus::InvalidParameter;
    }

    uint32_t pipeMode = (namedPipeType ? npfs::PIPE_TYPE_MESSAGE : npfs::PIPE_TYPE_BYTE) |
                        (readMode ? npfs::PIPE_READMODE_MESSAGE : npfs::PIPE_READMODE_BYTE) |
                        (completionMode ? npfs::PIPE_NOWAIT : npfs::PIPE_WAIT);

    uint32_t timeoutMs = defaultTimeout ? static_cast<uint32_t>(-(defaultTimeout->quadPart / 10000)) : 50;

    std::shared_ptr<npfs::NamedPipeInstance> pipeInst;
    NtStatus status = npfs::NamedPipeFileSystem::get().createNamedPipe(
        objectAttributes->objectName->view(),
        desiredAccess,
        pipeMode,
        maximumInstances ? maximumInstances : npfs::PIPE_UNLIMITED_INSTANCES,
        outboundQuota,
        inboundQuota,
        timeoutMs,
        pipeInst
    );

    if (!NT_SUCCESS(status)) {
        if (ioStatusBlock) {
            ioStatusBlock->status = status;
            ioStatusBlock->information = 0;
        }
        return status;
    }

    auto fileObj = fs::VirtualFileSystem::get().registerServerPipe(pipeInst, desiredAccess);
    Handle h = g_NextFileHandle;
    g_NextFileHandle += 4;
    g_KernelFiles[h] = fileObj;
    *fileHandle = h;

    if (ioStatusBlock) {
        ioStatusBlock->status = NtStatus::Success;
        ioStatusBlock->information = 2; // FILE_CREATED
    }
    return NtStatus::Success;
}

NtStatus NtCreateMailslotFile(
    Handle* fileHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    IoStatusBlock* ioStatusBlock,
    uint32_t /*createOptions*/,
    uint32_t /*mailslotQuota*/,
    uint32_t maxMessageSize,
    LargeInteger* readTimeout
) {
    if (!fileHandle || !objectAttributes || !objectAttributes->objectName) {
        return NtStatus::InvalidParameter;
    }

    uint32_t timeoutMs = npfs::MAILSLOT_WAIT_FOREVER;
    if (readTimeout) {
        timeoutMs = (readTimeout->quadPart == -1) ? npfs::MAILSLOT_WAIT_FOREVER : static_cast<uint32_t>(-(readTimeout->quadPart / 10000));
    }

    std::shared_ptr<npfs::Mailslot> slot;
    NtStatus status = npfs::MailslotFileSystem::get().createMailslot(
        objectAttributes->objectName->view(),
        maxMessageSize,
        timeoutMs,
        slot
    );

    if (!NT_SUCCESS(status)) {
        if (ioStatusBlock) {
            ioStatusBlock->status = status;
            ioStatusBlock->information = 0;
        }
        return status;
    }

    auto fileObj = fs::VirtualFileSystem::get().registerServerMailslot(slot);
    Handle h = g_NextFileHandle;
    g_NextFileHandle += 4;
    g_KernelFiles[h] = fileObj;
    *fileHandle = h;

    if (ioStatusBlock) {
        ioStatusBlock->status = NtStatus::Success;
        ioStatusBlock->information = 2; // FILE_CREATED
    }
    return NtStatus::Success;
}

NtStatus NtReadFile(
    Handle fileHandle,
    Handle /*event*/,
    void* /*apcRoutine*/,
    void* /*apcContext*/,
    IoStatusBlock* ioStatusBlock,
    void* buffer,
    uint32_t length,
    LargeInteger* byteOffset,
    uint32_t* /*key*/
) {
    auto it = g_KernelFiles.find(fileHandle);
    if (it == g_KernelFiles.end()) {
        return NtStatus::InvalidHandle;
    }

    uint32_t bytesRead = 0;
    NtStatus status = fs::VirtualFileSystem::get().readFile(
        it->second.get(),
        buffer,
        length,
        byteOffset,
        bytesRead
    );

    if (ioStatusBlock) {
        ioStatusBlock->status = status;
        ioStatusBlock->information = bytesRead;
    }
    return status;
}

NtStatus NtWriteFile(
    Handle fileHandle,
    Handle /*event*/,
    void* /*apcRoutine*/,
    void* /*apcContext*/,
    IoStatusBlock* ioStatusBlock,
    const void* buffer,
    uint32_t length,
    LargeInteger* byteOffset,
    uint32_t* /*key*/
) {
    if (fileHandle == 0x14 || fileHandle == 0x18 || fileHandle == static_cast<Handle>(-11) || fileHandle == static_cast<Handle>(-12)) {
        if (buffer && length > 0) {
            std::cout.write(reinterpret_cast<const char*>(buffer), length);
            std::cout.flush();
        }
        if (ioStatusBlock) {
            ioStatusBlock->status = NtStatus::Success;
            ioStatusBlock->information = length;
        }
        return NtStatus::Success;
    }

    auto it = g_KernelFiles.find(fileHandle);
    if (it == g_KernelFiles.end()) {
        return NtStatus::InvalidHandle;
    }

    uint32_t bytesWritten = 0;
    NtStatus status = fs::VirtualFileSystem::get().writeFile(
        it->second.get(),
        buffer,
        length,
        byteOffset,
        bytesWritten
    );

    if (ioStatusBlock) {
        ioStatusBlock->status = status;
        ioStatusBlock->information = bytesWritten;
    }
    return status;
}

NtStatus NtDeviceIoControlFile(
    Handle fileHandle,
    Handle /*event*/,
    void* /*apcRoutine*/,
    void* /*apcContext*/,
    IoStatusBlock* ioStatusBlock,
    uint32_t ioControlCode,
    const void* inputBuffer,
    uint32_t inputBufferLength,
    void* outputBuffer,
    uint32_t outputBufferLength
) {
    auto it = g_KernelFiles.find(fileHandle);
    if (it == g_KernelFiles.end() || !it->second) {
        return NtStatus::InvalidHandle;
    }

    auto* devObj = it->second->getDeviceObject();
    if (!devObj || !devObj->driverObject) {
        return NtStatus::InvalidDeviceRequest;
    }

    // Build I/O Request Packet (IRP) for IRP_MJ_DEVICE_CONTROL
    io::Irp irp{};
    irp.majorFunction = io::IRP_MJ_DEVICE_CONTROL;
    irp.deviceObject = devObj;
    irp.systemBuffer = const_cast<void*>(inputBuffer);
    irp.userBuffer = outputBuffer;
    irp.length = outputBufferLength;
    irp.byteOffset.lowPart = ioControlCode;
    irp.byteOffset.highPart = static_cast<int32_t>(inputBufferLength);

    NtStatus status = fs::IoCallDriver(devObj, &irp);

    if (ioStatusBlock) {
        *ioStatusBlock = irp.ioStatus;
    }
    return status;
}

NtStatus NtClose(Handle handle) {
    if (handle == 0 || handle == InvalidHandleValue) {
        return NtStatus::InvalidHandle;
    }
    bool wasSync = sync::DispatcherRegistry::get().lookup(handle) != nullptr;
    sync::DispatcherRegistry::get().unregister(handle);
    auto it = g_KernelFiles.find(handle);
    if (it != g_KernelFiles.end()) {
        fs::VirtualFileSystem::get().closeFile(it->second.get());
        g_KernelFiles.erase(it);
        return NtStatus::Success;
    }
    auto secIt = g_KernelSections.find(handle);
    if (secIt != g_KernelSections.end()) {
        g_KernelSections.erase(secIt);
        return NtStatus::Success;
    }
    if (wasSync) {
        return NtStatus::Success;
    }
    return g_KernelHandleTable.closeHandle(handle);
}

fs::FileObject* LookupKernelFileObject(Handle handle) {
    auto it = g_KernelFiles.find(handle);
    if (it != g_KernelFiles.end()) {
        return it->second.get();
    }
    return nullptr;
}

NtStatus NtTerminateProcess(Handle processHandle, NtStatus exitStatus) {
    std::cout << "[MicaNT Executive] NtTerminateProcess called (PID handle: " 
              << processHandle << ", exit status: " << static_cast<uint32_t>(exitStatus) << ")\n";
    return NtStatus::Success;
}

NtStatus NtWaitForMultipleObjects(
    uint32_t count,
    const Handle* handles,
    WaitType waitType,
    bool /*alertable*/,
    LargeInteger* timeout
) {
    if (count == 0 || count > MAXIMUM_WAIT_OBJECTS) {
        return NtStatus::InvalidParameter1;
    }
    if (!handles) {
        return NtStatus::AccessViolation;
    }

    std::vector<std::shared_ptr<sync::DispatcherObject>> objects;
    objects.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        auto obj = sync::DispatcherRegistry::get().lookup(handles[i]);
        if (!obj) {
            return NtStatus::InvalidHandle;
        }
        objects.push_back(obj);
    }

    // Determine timeout in milliseconds
    uint32_t timeoutMs = 0xFFFFFFFF;
    if (timeout) {
        if (timeout->quadPart == 0) {
            timeoutMs = 0;
        } else if (timeout->quadPart < 0) {
            timeoutMs = static_cast<uint32_t>(-timeout->quadPart / 10000);
        } else {
            timeoutMs = static_cast<uint32_t>(timeout->quadPart / 10000);
        }
    }

    if (waitType == WaitType::WaitAny) {
        // Immediate check
        for (uint32_t i = 0; i < count; ++i) {
            if (objects[i]->isSignaled()) {
                objects[i]->satisfyWait();
                return STATUS_WAIT_N(i);
            }
        }

        if (timeoutMs == 0) {
            return NtStatus::Timeout;
        }

        auto cv = std::make_shared<std::condition_variable>();
        auto cvMutex = std::make_shared<std::mutex>();

        for (auto& obj : objects) {
            obj->addWaitListener(cv, cvMutex);
        }

        auto start = std::chrono::steady_clock::now();
        NtStatus result = NtStatus::Timeout;

        while (true) {
            std::unique_lock<std::mutex> lk(*cvMutex);
            for (uint32_t i = 0; i < count; ++i) {
                if (objects[i]->isSignaled()) {
                    objects[i]->satisfyWait();
                    result = STATUS_WAIT_N(i);
                    break;
                }
            }
            if (result != NtStatus::Timeout) break;

            if (timeoutMs == 0xFFFFFFFF) {
                cv->wait(lk);
            } else {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                if (elapsed >= timeoutMs) {
                    result = NtStatus::Timeout;
                    break;
                }
                cv->wait_for(lk, std::chrono::milliseconds(timeoutMs - elapsed));
            }

            for (uint32_t i = 0; i < count; ++i) {
                if (objects[i]->isSignaled()) {
                    objects[i]->satisfyWait();
                    result = STATUS_WAIT_N(i);
                    break;
                }
            }
            if (result != NtStatus::Timeout) break;

            if (timeoutMs != 0xFFFFFFFF) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                if (elapsed >= timeoutMs) {
                    result = NtStatus::Timeout;
                    break;
                }
            }
        }

        for (auto& obj : objects) {
            obj->removeWaitListener(cv);
        }
        return result;

    } else { // WaitAll
        bool allSignaled = true;
        for (uint32_t i = 0; i < count; ++i) {
            if (!objects[i]->isSignaled()) {
                allSignaled = false;
                break;
            }
        }
        if (allSignaled) {
            for (uint32_t i = 0; i < count; ++i) {
                objects[i]->satisfyWait();
            }
            return NtStatus::Success;
        }

        if (timeoutMs == 0) {
            return NtStatus::Timeout;
        }

        auto cv = std::make_shared<std::condition_variable>();
        auto cvMutex = std::make_shared<std::mutex>();

        for (auto& obj : objects) {
            obj->addWaitListener(cv, cvMutex);
        }

        auto start = std::chrono::steady_clock::now();
        NtStatus result = NtStatus::Timeout;

        while (true) {
            std::unique_lock<std::mutex> lk(*cvMutex);
            allSignaled = true;
            for (uint32_t i = 0; i < count; ++i) {
                if (!objects[i]->isSignaled()) {
                    allSignaled = false;
                    break;
                }
            }
            if (allSignaled) {
                for (uint32_t i = 0; i < count; ++i) {
                    objects[i]->satisfyWait();
                }
                result = NtStatus::Success;
                break;
            }

            if (timeoutMs == 0xFFFFFFFF) {
                cv->wait(lk);
            } else {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                if (elapsed >= timeoutMs) {
                    result = NtStatus::Timeout;
                    break;
                }
                cv->wait_for(lk, std::chrono::milliseconds(timeoutMs - elapsed));
            }

            allSignaled = true;
            for (uint32_t i = 0; i < count; ++i) {
                if (!objects[i]->isSignaled()) {
                    allSignaled = false;
                    break;
                }
            }
            if (allSignaled) {
                for (uint32_t i = 0; i < count; ++i) {
                    objects[i]->satisfyWait();
                }
                result = NtStatus::Success;
                break;
            }

            if (timeoutMs != 0xFFFFFFFF) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                if (elapsed >= timeoutMs) {
                    result = NtStatus::Timeout;
                    break;
                }
            }
        }

        for (auto& obj : objects) {
            obj->removeWaitListener(cv);
        }
        return result;
    }
}

NtStatus NtWaitForSingleObject(Handle handle, bool alertable, LargeInteger* timeout) {
    if (handle == 0 || handle == InvalidHandleValue) {
        return NtStatus::InvalidHandle;
    }
    return NtWaitForMultipleObjects(1, &handle, WaitType::WaitAny, alertable, timeout);
}

NtStatus NtDelayExecution(bool /*alertable*/, const LargeInteger* interval) {
    if (!interval) {
        return NtStatus::AccessViolation;
    }

    if (interval->quadPart == 0) {
        std::this_thread::yield();
        return NtStatus::Success;
    }

    if (interval->quadPart < 0) {
        int64_t nanoseconds = -interval->quadPart * 100;
        std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
        return NtStatus::Success;
    }

    int64_t nanoseconds = interval->quadPart * 100;
    std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
    return NtStatus::Success;
}

NtStatus NtShutdownSystem(uint32_t action) {
    auto shutdownAction = static_cast<po::ShutdownAction>(action);
    return po::PowerManager::get().shutdownSystem(shutdownAction);
}

NtStatus NtCreateTimer(
    Handle* timerHandle,
    uint32_t /*desiredAccess*/,
    ObjectAttributes* /*objectAttributes*/,
    uint32_t timerType
) {
    if (!timerHandle) return NtStatus::InvalidParameter;

    timer::TimerType type = (timerType == 0) 
        ? timer::TimerType::NotificationTimer 
        : timer::TimerType::SynchronizationTimer;

    auto timerObj = std::make_shared<timer::TimerObject>(type);
    Handle h = sync::DispatcherRegistry::get().registerObject(timerObj);
    *timerHandle = h;
    return NtStatus::Success;
}

NtStatus NtSetTimer(
    Handle timerHandle,
    LargeInteger* dueTime,
    void* /*timerApcRoutine*/,
    void* /*timerContext*/,
    bool /*resumeTimer*/,
    uint32_t period,
    bool* previousState
) {
    if (!dueTime) return NtStatus::InvalidParameter;

    auto timerObj = sync::DispatcherRegistry::get().lookupAs<timer::TimerObject>(timerHandle);
    if (!timerObj) return NtStatus::InvalidHandle;

    if (previousState) {
        *previousState = timerObj->isSignaled();
    }

    timerObj->reset();
    timerObj->setDueTime(*dueTime);
    timerObj->setPeriodMs(period);

    std::chrono::nanoseconds delayNs{0};
    if (dueTime->quadPart < 0) {
        delayNs = std::chrono::nanoseconds(-dueTime->quadPart * 100);
    } else if (dueTime->quadPart > 0) {
        delayNs = std::chrono::nanoseconds(dueTime->quadPart * 100);
    }
    timerObj->setDeadline(std::chrono::steady_clock::now() + delayNs);
    timerObj->setInserted(true);

    return NtStatus::Success;
}

NtStatus NtCancelTimer(Handle timerHandle, bool* currentSignaledState) {
    auto timerObj = sync::DispatcherRegistry::get().lookupAs<timer::TimerObject>(timerHandle);
    if (!timerObj) return NtStatus::InvalidHandle;

    if (currentSignaledState) {
        *currentSignaledState = timerObj->isSignaled();
    }
    timerObj->setInserted(false);
    return NtStatus::Success;
}

NtStatus NtQuerySystemInformation(
    uint32_t systemInformationClass,
    void* systemInformation,
    uint32_t systemInformationLength,
    uint32_t* returnLength
) {
    if (!systemInformation && systemInformationLength > 0) {
        return NtStatus::AccessViolation;
    }
    if (returnLength) {
        *returnLength = 0;
    }
    return NtStatus::Success;
}

NtStatus NtCreateSection(
    Handle* sectionHandle,
    uint32_t desiredAccess,
    ObjectAttributes* /*objectAttributes*/,
    LargeInteger* maximumSize,
    uint32_t sectionPageProtection,
    uint32_t allocationAttributes,
    Handle fileHandle
) {
    if (!sectionHandle) return NtStatus::InvalidParameter;
    size_t size = maximumSize ? static_cast<size_t>(maximumSize->quadPart) : 4096;

    std::shared_ptr<fs::FileObject> fileObj;
    if (fileHandle != 0) {
        auto it = g_KernelFiles.find(fileHandle);
        if (it != g_KernelFiles.end()) {
            fileObj = it->second;
            if (size == 0 || (maximumSize && maximumSize->quadPart == 0)) {
                size = fileObj->getFileSize();
            }
        }
    }
    if (size == 0) size = 4096;

    auto sec = std::make_shared<section::SectionObject>(size, allocationAttributes, sectionPageProtection);
    if (fileObj && fileObj->getFileSize() > 0) {
        sec->loadData(fileObj->getData().data(), fileObj->getFileSize());
    }

    Handle h = g_NextSectionHandle++;
    g_KernelSections[h] = sec;
    *sectionHandle = h;
    return NtStatus::Success;
}

NtStatus NtMapViewOfSection(
    Handle sectionHandle,
    Handle /*processHandle*/,
    uintptr_t* baseAddress,
    uintptr_t /*zeroBits*/,
    size_t /*commitSize*/,
    LargeInteger* /*sectionOffset*/,
    size_t* viewSize,
    uint32_t /*inheritDisposition*/,
    uint32_t allocationType,
    uint32_t win32Protect
) {
    if (!baseAddress || !viewSize) return NtStatus::InvalidParameter;
    auto it = g_KernelSections.find(sectionHandle);
    if (it == g_KernelSections.end() || !it->second) return NtStatus::InvalidHandle;

    auto sec = it->second;
    size_t sz = (*viewSize == 0 || *viewSize > sec->getSize()) ? sec->getSize() : *viewSize;

    uintptr_t addr = *baseAddress;
    if (addr == 0) {
        addr = reinterpret_cast<uintptr_t>(sec->getData());
    }

    // Register in address space tracker
    g_KernelAddressSpace.allocate(
        addr,
        sz,
        allocationType ? allocationType : (mm::MEM_COMMIT | mm::MEM_RESERVE),
        win32Protect ? win32Protect : sec->getPageProtection()
    );

    *baseAddress = addr;
    *viewSize = sz;
    return NtStatus::Success;
}

NtStatus NtUnmapViewOfSection(Handle /*processHandle*/, uintptr_t baseAddress) {
    return g_KernelAddressSpace.free(baseAddress, 0, mm::MEM_RELEASE);
}

NtStatus NtQueryInformationFile(
    Handle fileHandle,
    IoStatusBlock* ioStatusBlock,
    void* fileInformation,
    uint32_t length,
    FileInformationClass fileInformationClass
) {
    if (!fileInformation) return NtStatus::InvalidParameter;
    auto it = g_KernelFiles.find(fileHandle);
    if (it == g_KernelFiles.end() || !it->second) return NtStatus::InvalidHandle;
    auto fileObj = it->second;

    switch (fileInformationClass) {
        case FileInformationClass::FileStandardInformation: {
            if (length < sizeof(FileStandardInformation)) return NtStatus::InfoLengthMismatch;
            auto* info = reinterpret_cast<FileStandardInformation*>(fileInformation);
            info->allocationSize.quadPart = static_cast<int64_t>(fileObj->getFileSize());
            info->endOfFile.quadPart = static_cast<int64_t>(fileObj->getFileSize());
            info->numberOfLinks = 1;
            info->deletePending = false;
            info->directory = fileObj->isDirectory();
            if (ioStatusBlock) {
                ioStatusBlock->status = NtStatus::Success;
                ioStatusBlock->information = sizeof(FileStandardInformation);
            }
            return NtStatus::Success;
        }
        case FileInformationClass::FilePositionInformation: {
            if (length < sizeof(FilePositionInformation)) return NtStatus::InfoLengthMismatch;
            auto* info = reinterpret_cast<FilePositionInformation*>(fileInformation);
            info->currentByteOffset.quadPart = fileObj->getCurrentByteOffset();
            if (ioStatusBlock) {
                ioStatusBlock->status = NtStatus::Success;
                ioStatusBlock->information = sizeof(FilePositionInformation);
            }
            return NtStatus::Success;
        }
        case FileInformationClass::FileBasicInformation: {
            if (length < sizeof(FileBasicInformation)) return NtStatus::InfoLengthMismatch;
            auto* info = reinterpret_cast<FileBasicInformation*>(fileInformation);
            info->fileAttributes = fileObj->isDirectory() ? fs::FILE_ATTRIBUTE_DIRECTORY : fs::FILE_ATTRIBUTE_NORMAL;
            if (ioStatusBlock) {
                ioStatusBlock->status = NtStatus::Success;
                ioStatusBlock->information = sizeof(FileBasicInformation);
            }
            return NtStatus::Success;
        }
        default:
            return NtStatus::NotImplemented;
    }
}

NtStatus NtSetInformationFile(
    Handle fileHandle,
    IoStatusBlock* ioStatusBlock,
    const void* fileInformation,
    uint32_t length,
    FileInformationClass fileInformationClass
) {
    if (!fileInformation) return NtStatus::InvalidParameter;
    auto it = g_KernelFiles.find(fileHandle);
    if (it == g_KernelFiles.end() || !it->second) return NtStatus::InvalidHandle;
    auto fileObj = it->second;

    switch (fileInformationClass) {
        case FileInformationClass::FilePositionInformation: {
            if (length < sizeof(FilePositionInformation)) return NtStatus::InfoLengthMismatch;
            auto* info = reinterpret_cast<const FilePositionInformation*>(fileInformation);
            fileObj->setCurrentByteOffset(info->currentByteOffset.quadPart);
            if (ioStatusBlock) {
                ioStatusBlock->status = NtStatus::Success;
                ioStatusBlock->information = sizeof(FilePositionInformation);
            }
            return NtStatus::Success;
        }
        case FileInformationClass::FileEndOfFileInformation: {
            if (length < sizeof(FileEndOfFileInformation)) return NtStatus::InfoLengthMismatch;
            auto* info = reinterpret_cast<const FileEndOfFileInformation*>(fileInformation);
            fileObj->getData().resize(static_cast<size_t>(info->endOfFile.quadPart));
            if (ioStatusBlock) {
                ioStatusBlock->status = NtStatus::Success;
                ioStatusBlock->information = sizeof(FileEndOfFileInformation);
            }
            return NtStatus::Success;
        }
        default:
            return NtStatus::NotImplemented;
    }
}

NtStatus NtQueryDirectoryFile(
    Handle fileHandle,
    Handle /*event*/,
    void* /*apcRoutine*/,
    void* /*apcContext*/,
    IoStatusBlock* ioStatusBlock,
    void* fileInformation,
    uint32_t length,
    FileInformationClass /*fileInformationClass*/,
    bool /*returnSingleEntry*/,
    UnicodeString* /*fileName*/,
    bool /*restartScan*/
) {
    if (!fileInformation || length < sizeof(FileBothDirInformation)) return NtStatus::InvalidParameter;
    auto it = g_KernelFiles.find(fileHandle);
    if (it == g_KernelFiles.end() || !it->second) return NtStatus::InvalidHandle;
    auto fileObj = it->second;

    std::vector<fs::VirtualFileSystem::DirectoryEntry> entries;
    NtStatus status = fs::VirtualFileSystem::get().queryDirectory(fileObj->getFileName(), entries);
    if (!NT_SUCCESS(status)) return status;

    if (entries.empty()) {
        return NtStatus::NoMoreFiles;
    }

    auto* dirInfo = reinterpret_cast<FileBothDirInformation*>(fileInformation);
    const auto& entry = entries[0];
    dirInfo->nextEntryOffset = 0;
    dirInfo->fileAttributes = entry.attributes;
    dirInfo->endOfFile.quadPart = static_cast<int64_t>(entry.size);
    dirInfo->allocationSize.quadPart = static_cast<int64_t>(entry.size);
    dirInfo->fileNameLength = static_cast<uint32_t>(entry.name.size() * sizeof(wchar_t));
    std::wcsncpy(dirInfo->fileName, entry.name.c_str(), 259);

    if (ioStatusBlock) {
        ioStatusBlock->status = NtStatus::Success;
        ioStatusBlock->information = sizeof(FileBothDirInformation);
    }
    return NtStatus::Success;
}

NtStatus NtQueryPerformanceCounter(
    LargeInteger* performanceCounter,
    LargeInteger* performanceFrequency
) {
    if (!performanceCounter) return NtStatus::InvalidParameter;
    hal::KeQueryPerformanceCounter(*performanceCounter, performanceFrequency);
    return NtStatus::Success;
}

NtStatus NtYieldExecution() {
    std::this_thread::yield();
    return NtStatus::Success;
}

NtStatus NtQueryInformationProcess(
    Handle /*processHandle*/,
    ProcessInformationClass processInformationClass,
    void* processInformation,
    uint32_t processInformationLength,
    uint32_t* returnLength
) {
    if (!processInformation) return NtStatus::InvalidParameter;
    if (processInformationClass == ProcessInformationClass::ProcessBasicInformation) {
        if (processInformationLength < sizeof(ProcessBasicInformation)) return NtStatus::InfoLengthMismatch;
        auto* pbi = reinterpret_cast<ProcessBasicInformation*>(processInformation);
        pbi->exitStatus = NtStatus::Success;
        pbi->pebBaseAddress = 0x00007FFDF0000000ULL;
        pbi->affinityMask = 0x0F;
        pbi->basePriority = 8;
        pbi->uniqueProcessId = 1000;
        pbi->inheritedFromUniqueProcessId = 0;
        if (returnLength) *returnLength = sizeof(ProcessBasicInformation);
        return NtStatus::Success;
    }
    return NtStatus::NotImplemented;
}

} // namespace micant::sys

