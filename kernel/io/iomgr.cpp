#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/io.hpp"
#include "micant/km/drivers/fs.hpp"
#include "micant/km/drivers/npfs.hpp"
#include <unordered_map>
#include <memory>
#include <vector>
#include <iostream>
#include <cwchar>

namespace micant::sys {

static std::unordered_map<Handle, std::shared_ptr<fs::FileObject>> g_KernelFiles;
static std::unordered_map<Handle, std::shared_ptr<io::IoCompletionPort>> g_KernelIocpPorts;
static Handle g_NextFileHandle = 0x200;

namespace internal {

bool IoCloseKernelHandle(Handle h) {
    bool hadIocp = g_KernelIocpPorts.erase(h) > 0;
    auto it = g_KernelFiles.find(h);
    if (it != g_KernelFiles.end()) {
        fs::VirtualFileSystem::get().closeFile(it->second.get());
        g_KernelFiles.erase(it);
        return true;
    }
    return hadIocp;
}

NtStatus NtCreateIoCompletionSys(const SyscallFrame& f) {
    auto* outHandle = reinterpret_cast<Handle*>(f.arg1);
    if (!outHandle) return NtStatus::InvalidParameter;

    uint32_t maxThreads = static_cast<uint32_t>(f.arg4);
    Handle h = ObAllocateExecutiveHandle();
    g_KernelIocpPorts[h] = std::make_shared<io::IoCompletionPort>(maxThreads);
    *outHandle = h;
    return NtStatus::Success;
}

NtStatus NtSetIoCompletionSys(const SyscallFrame& f) {
    Handle h = static_cast<Handle>(f.arg1);
    auto it = g_KernelIocpPorts.find(h);
    if (it == g_KernelIocpPorts.end()) return NtStatus::InvalidHandle;

    uint64_t key = f.arg2;
    uintptr_t overlapped = f.arg3;
    NtStatus status = static_cast<NtStatus>(f.arg4);
    uint32_t bytes = 0;
    if (f.stackArgs && f.stackArgCount >= 1) {
        bytes = static_cast<uint32_t>(f.stackArgs[0]);
    }

    return it->second->postCompletion(key, overlapped, status, bytes);
}

NtStatus NtRemoveIoCompletionSys(const SyscallFrame& f) {
    Handle h = static_cast<Handle>(f.arg1);
    auto it = g_KernelIocpPorts.find(h);
    if (it == g_KernelIocpPorts.end()) return NtStatus::InvalidHandle;

    auto* outKey = reinterpret_cast<uint64_t*>(f.arg2);
    auto* outOverlapped = reinterpret_cast<uintptr_t*>(f.arg3);
    auto* outIoStatus = reinterpret_cast<IoStatusBlock*>(f.arg4);
    auto* timeout = (f.stackArgs && f.stackArgCount >= 1) ? reinterpret_cast<LargeInteger*>(f.stackArgs[0]) : nullptr;

    uint64_t key = 0;
    uintptr_t ov = 0;
    IoStatusBlock iosb{};
    uint32_t ms = timeout ? static_cast<uint32_t>(timeout->quadPart / -10000) : 0xFFFFFFFF;

    NtStatus res = it->second->removeCompletion(key, ov, iosb, ms);
    if (outKey) *outKey = key;
    if (outOverlapped) *outOverlapped = ov;
    if (outIoStatus) *outIoStatus = iosb;
    return res;
}

} // namespace internal

fs::FileObject* LookupKernelFileObject(Handle handle) {
    auto it = g_KernelFiles.find(handle);
    if (it != g_KernelFiles.end()) {
        return it->second.get();
    }
    return nullptr;
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
    uint32_t /*desiredAccess*/,
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

    io::Irp irp{};
    irp.majorFunction = io::IRP_MJ_DEVICE_CONTROL;
    irp.deviceObject = devObj;
    irp.systemBuffer = const_cast<void*>(inputBuffer);
    irp.userBuffer = outputBuffer;
    irp.length = outputBufferLength;
    irp.byteOffset.lowPart = ioControlCode;
    irp.byteOffset.highPart = static_cast<int32_t>(inputBufferLength);

    NtStatus status = io::IoCallDriver(devObj, &irp);

    if (ioStatusBlock) {
        *ioStatusBlock = irp.ioStatus;
    }
    return status;
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

} // namespace micant::sys
