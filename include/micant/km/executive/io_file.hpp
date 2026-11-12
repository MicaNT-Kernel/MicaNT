#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <memory>
#include <new>
#include <string_view>
#if defined(MICANT_FREESTANDING)
#include "micant/km/core/sync.hpp"
#else
#include <mutex>
#endif
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/executive/ob.hpp"
#include "micant/km/executive/io.hpp"

namespace micant::io {
struct DeviceObject;

using FileReadRoutine = NtStatus (*)(void* context, uint64_t offset,
                                     void* buffer, size_t length,
                                     size_t* bytesRead) noexcept;
using FileReleaseRoutine = void (*)(void* context) noexcept;
using FileHandleCloseRoutine = void (*)(void* context) noexcept;
struct FileBackend;
struct AsyncRead;
using FileSubmitReadRoutine = NtStatus (*)(void*, AsyncRead*) noexcept;
using FileCancelReadRoutine = bool (*)(void*, AsyncRead*) noexcept;
using FileOpenRoutine = NtStatus (*)(void* context, std::u16string_view name,
                                     FileBackend& backend) noexcept;

// The backend owns the storage/device state. A file object pins that backend
// until its final Ob reference is released and exposes bounded offset reads to Mm.
struct FileBackend {
    void* context{};
    uint64_t size{};
    std::u16string_view name{};
    FileReadRoutine read{};
    FileReleaseRoutine release{};
    bool stream{};
    FileHandleCloseRoutine lastHandleClosed{};
    FileHandleCloseRoutine cancelWake{};
    FileSubmitReadRoutine submitRead{};
    FileCancelReadRoutine cancelRead{};
    bool synchronous{};
};

class FileObject {
public:
    explicit FileObject(FileBackend backend, DeviceObject* device = nullptr) noexcept
        : backend_(backend), deviceObject_(device) {}
    ~FileObject() {
        if (completionReference_) ob::ObDereferenceObject(completionReference_);
        if (backend_.release) backend_.release(backend_.context);
    }
    FileObject(const FileObject&) = delete;
    FileObject& operator=(const FileObject&) = delete;

    [[nodiscard]] uint64_t size() const noexcept { return backend_.size; }
    [[nodiscard]] bool isStream() const noexcept { return backend_.stream; }
    void notifyLastHandleClosed() noexcept {
        bool expected = false;
        if (backend_.lastHandleClosed && closeNotified_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            backend_.lastHandleClosed(backend_.context);
    }
    NtStatus associateCompletion(ob::ObjectHeader* port, uintptr_t key) noexcept;
    ob::ObjectHeader* referenceCompletion(uintptr_t& key) noexcept;
    [[nodiscard]] bool isSynchronous() const noexcept { return backend_.synchronous; }
    [[nodiscard]] FileSubmitReadRoutine submitReadRoutine() const noexcept { return backend_.submitRead; }
    [[nodiscard]] FileCancelReadRoutine cancelReadRoutine() const noexcept { return backend_.cancelRead; }
    [[nodiscard]] bool canCancelRead() const noexcept { return backend_.cancelWake != nullptr; }
    void wakeCancelledRead() noexcept { if (backend_.cancelWake) backend_.cancelWake(backend_.context); }
    [[nodiscard]] DeviceObject* deviceObject() const noexcept { return deviceObject_; }
    [[nodiscard]] void* context() const noexcept { return backend_.context; }
    [[nodiscard]] std::u16string_view name() const noexcept { return {name_.get(), nameLength_}; }
    NtStatus initialize() noexcept {
        if (backend_.name.size() > SIZE_MAX / sizeof(char16_t) - 1) return NtStatus::BufferTooSmall;
        if (backend_.name.empty()) return NtStatus::Success;
        name_.reset(new (std::nothrow_t{}) char16_t[backend_.name.size() + 1]);
        if (!name_) return NtStatus::NoMemory;
        for (size_t i = 0; i < backend_.name.size(); ++i) name_[i] = backend_.name[i];
        name_[backend_.name.size()] = 0;
        nameLength_ = backend_.name.size();
        return NtStatus::Success;
    }
    NtStatus read(uint64_t offset, void* buffer, size_t length,
                  size_t* bytesRead) const noexcept {
        if (!bytesRead || (!buffer && length) || !backend_.read)
            return NtStatus::InvalidParameter;
        *bytesRead = 0;
        if (!length) return NtStatus::Success;
        if (backend_.stream) {
            const auto status = backend_.read(backend_.context, 0, buffer, length, bytesRead);
            if (*bytesRead > length) { *bytesRead = 0; return NtStatus::Unsuccessful; }
            return status;
        }
        if (offset >= backend_.size) return NtStatus::EndOfFile;
        const uint64_t available = backend_.size - offset;
        const size_t boundedLength = available < length ? static_cast<size_t>(available) : length;
        const auto status = backend_.read(backend_.context, offset, buffer, boundedLength, bytesRead);
        if (*bytesRead > boundedLength) { *bytesRead = 0; return NtStatus::Unsuccessful; }
        if (status != NtStatus::Success && status != NtStatus::EndOfFile) return status;
        return *bytesRead < length ? NtStatus::EndOfFile : NtStatus::Success;
    }
    NtStatus readCurrent(void* buffer, size_t length, size_t* bytesRead) noexcept {
        if (!bytesRead || (!buffer && length)) return NtStatus::InvalidParameter;
        *bytesRead = 0;
        if (backend_.stream) return read(0, buffer, length, bytesRead);
#if defined(MICANT_FREESTANDING)
        if (ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL) return NtStatus::InvalidDeviceRequest;
        const auto waitStatus = currentOffsetGate_.wait();
        if (waitStatus != NtStatus::Success) return waitStatus;
        struct GateRelease {
            sync::detail::BootstrapEvent& gate;
            ~GateRelease() { (void)gate.set(); }
        } release{currentOffsetGate_};
#else
        std::lock_guard<std::mutex> guard(currentOffsetMutex_);
#endif
        const auto status = read(currentOffset_, buffer, length, bytesRead);
        if (!backend_.stream && *bytesRead > UINT64_MAX - currentOffset_) {
            *bytesRead = 0;
            return NtStatus::Unsuccessful;
        }
        if (!backend_.stream) currentOffset_ += *bytesRead;
        return status;
    }

private:
    FileBackend backend_{};
    ob::ObjectHeader* completionReference_{};
    uintptr_t completionKey_{};
#if defined(MICANT_FREESTANDING)
    ke::SpinLock completionLock_{};
#else
    std::mutex completionLock_{};
#endif
    DeviceObject* deviceObject_{};
    std::atomic<bool> closeNotified_{false};
    std::unique_ptr<char16_t[]> name_{};
    size_t nameLength_{};
    uint64_t currentOffset_{};
#if defined(MICANT_FREESTANDING)
    sync::detail::BootstrapEvent currentOffsetGate_{false, true};
#else
    std::mutex currentOffsetMutex_;
#endif
};

// Driver-owned linkage is valid only while the request is pending in a device
// queue. Io retains the file, process, event, and storage until completion.
struct AsyncRead {
    Irp irp{};
    AsyncRead* driverNext{};
    AsyncRead* pendingNext{};
    AsyncRead* completedNext{};
    FileObject* file{};
    ob::ObjectHeader* fileReference{};
    ob::ObjectHeader* processReference{};
    ob::ObjectHeader* threadReference{};
    ob::ObjectHeader* eventReference{};
    ke::KAPC* userApc{};
    uintptr_t userApcRoutine{};
    uintptr_t userApcContext{};
    ob::ObjectHeader* completionReference{};
    void* completionPacket{};
    uintptr_t completionKey{};
    uintptr_t completionContext{};
    void* process{};
    void* userBuffer{};
    IoStatusBlock* userStatus{};
    uint8_t* buffer{};
    size_t length{};
    IoStatusBlock result{};
    bool pending{};
};
void IoCompleteAsyncRead(AsyncRead* request, NtStatus status, size_t information) noexcept;
NtStatus IoSubmitAsyncRead(Handle handle, Handle event, uintptr_t apcRoutine, uintptr_t completionContext,
                           void* userBuffer, size_t length, IoStatusBlock* userStatus) noexcept;
NtStatus IoAssociateCompletionPort(Handle file, Handle port, uintptr_t key) noexcept;
NtStatus IoHasCompletionPort(Handle file, bool& associated) noexcept;

NtStatus IoInitializeFileObjects() noexcept;
NtStatus IoCreateCompletionPort(Handle* handle, uint32_t access, uint32_t concurrency) noexcept;
NtStatus IoPostCompletion(Handle handle, uintptr_t key, uintptr_t context,
                          NtStatus status, uintptr_t information) noexcept;
NtStatus IoRemoveCompletion(Handle handle, uintptr_t& key, uintptr_t& context,
                            IoStatusBlock& status, uint64_t timeout100ns) noexcept;
NtStatus IoRegisterFileOpenRoutine(FileOpenRoutine open, void* context) noexcept;
NtStatus IoUnregisterFileOpenRoutine(FileOpenRoutine open, void* context) noexcept;
NtStatus IoOpenBackendFile(Handle* fileHandle, uint32_t desiredAccess,
                           std::u16string_view name) noexcept;
NtStatus IoCreateBackendFile(Handle* fileHandle, uint32_t desiredAccess,
                             const FileBackend& backend) noexcept;
NtStatus IoCreateDeviceFile(Handle* fileHandle, uint32_t desiredAccess,
                            DeviceObject* deviceObject, const FileBackend& backend) noexcept;
NtStatus IoReferenceFileHandle(Handle fileHandle, uint32_t desiredAccess,
                               FileObject*& file, ob::ObjectHeader*& reference) noexcept;
NtStatus IoReadFileHandle(Handle fileHandle, bool useCurrentOffset, uint64_t offset,
                          void* buffer, size_t length, size_t* bytesRead,
                          const void* requestStatus = nullptr) noexcept;
NtStatus IoCancelFileReads(Handle fileHandle, const void* requestStatus) noexcept;
bool IoCurrentReadCancelled() noexcept;

} // namespace micant::io
