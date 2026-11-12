#pragma once

#include <cstddef>
#include <cstdint>
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

namespace micant::io {

using FileReadRoutine = NtStatus (*)(void* context, uint64_t offset,
                                     void* buffer, size_t length,
                                     size_t* bytesRead) noexcept;
using FileReleaseRoutine = void (*)(void* context) noexcept;
struct FileBackend;
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
};

class FileObject {
public:
    explicit FileObject(FileBackend backend) noexcept : backend_(backend) {}
    ~FileObject() { if (backend_.release) backend_.release(backend_.context); }
    FileObject(const FileObject&) = delete;
    FileObject& operator=(const FileObject&) = delete;

    [[nodiscard]] uint64_t size() const noexcept { return backend_.size; }
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
        if (*bytesRead > UINT64_MAX - currentOffset_) {
            *bytesRead = 0;
            return NtStatus::Unsuccessful;
        }
        currentOffset_ += *bytesRead;
        return status;
    }

private:
    FileBackend backend_{};
    std::unique_ptr<char16_t[]> name_{};
    size_t nameLength_{};
    uint64_t currentOffset_{};
#if defined(MICANT_FREESTANDING)
    sync::detail::BootstrapEvent currentOffsetGate_{false, true};
#else
    std::mutex currentOffsetMutex_;
#endif
};

NtStatus IoInitializeFileObjects() noexcept;
// Initializes the optional bare-metal AHCI/FAT32 file provider. A missing
// controller, disk, or supported mounted volume is reported as failure.
NtStatus IoInitializeAhciFileProvider() noexcept;
NtStatus IoRegisterFileOpenRoutine(FileOpenRoutine open, void* context) noexcept;
NtStatus IoOpenBackendFile(Handle* fileHandle, uint32_t desiredAccess,
                           std::u16string_view name) noexcept;
NtStatus IoCreateBackendFile(Handle* fileHandle, uint32_t desiredAccess,
                             const FileBackend& backend) noexcept;
NtStatus IoReferenceFileHandle(Handle fileHandle, uint32_t desiredAccess,
                               FileObject*& file, ob::ObjectHeader*& reference) noexcept;
NtStatus IoReadFileHandle(Handle fileHandle, bool useCurrentOffset, uint64_t offset,
                          void* buffer, size_t length, size_t* bytesRead) noexcept;

} // namespace micant::io
