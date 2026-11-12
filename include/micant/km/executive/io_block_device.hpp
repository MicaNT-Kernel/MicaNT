#pragma once

#include <cstddef>
#include <cstdint>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/executive/ob.hpp"

namespace micant::io {

using BlockDeviceReadRoutine = NtStatus (*)(void* context, uint64_t lba,
                                             uint32_t blockCount, void* buffer,
                                             uint32_t* blocksRead) noexcept;
using BlockDeviceReleaseRoutine = void (*)(void* context) noexcept;

// Read-only backend contract. Io consumes context when create is called and
// releases it on failure or once when the final Ob reference ends.
struct BlockDeviceBackend {
    void* context{};
    uint64_t totalBlocks{};
    uint32_t blockSize{};
    BlockDeviceReadRoutine read{};
    BlockDeviceReleaseRoutine release{};
};

class BlockDeviceObject {
public:
    explicit BlockDeviceObject(BlockDeviceBackend backend) noexcept : backend_(backend) {}
    ~BlockDeviceObject() { if (backend_.release) backend_.release(backend_.context); }
    BlockDeviceObject(const BlockDeviceObject&) = delete;
    BlockDeviceObject& operator=(const BlockDeviceObject&) = delete;

    [[nodiscard]] uint64_t totalBlocks() const noexcept { return backend_.totalBlocks; }
    [[nodiscard]] uint32_t blockSize() const noexcept { return backend_.blockSize; }
    [[nodiscard]] uint64_t capacityBytes() const noexcept {
        return backend_.totalBlocks * backend_.blockSize;
    }

    NtStatus read(uint64_t lba, uint32_t blockCount, void* buffer,
                  uint32_t* blocksRead) const noexcept;

private:
    BlockDeviceBackend backend_{};
};

// Initialize the Io-owned Ob type once during kernel startup before creating
// backend objects. Handles use the current canonical handle table.
NtStatus IoInitializeBlockDevices() noexcept;
NtStatus IoCreateBlockDevice(Handle* deviceHandle, uint32_t desiredAccess,
                             const BlockDeviceBackend& backend) noexcept;
NtStatus IoReferenceBlockDeviceHandle(Handle deviceHandle, uint32_t desiredAccess,
                                      BlockDeviceObject*& device,
                                      ob::ObjectHeader*& reference) noexcept;
NtStatus IoReadBlockDeviceHandle(Handle deviceHandle, uint64_t lba,
                                 uint32_t blockCount, void* buffer,
                                 uint32_t* blocksRead) noexcept;

} // namespace micant::io
