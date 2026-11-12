#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "micant/shared/ntstatus.hpp"
#include "micant/shared/ntdef.hpp"

namespace micant::io { class FileObject; struct DeviceObject; struct Irp; }

namespace micant::km::afdwsk {

// Kernel-only handoff between the native Io file-handle table and the AFD
// endpoint owner. Ring 3 sees only the shared AFD ABI and native syscalls.
[[nodiscard]] NtStatus IoAfdInitialize() noexcept;
[[nodiscard]] NtStatus IoAfdCreateEndpointHandle(
    Handle* fileHandle, std::wstring_view path, uint32_t desiredAccess,
    const void* eaBuffer, size_t eaLength) noexcept;
[[nodiscard]] bool IoAfdIsEndpointFile(const io::FileObject* fileObject) noexcept;
[[nodiscard]] io::DeviceObject* IoAfdDeviceObject() noexcept;
[[nodiscard]] NtStatus IoAfdDispatchDeviceControl(io::Irp* irp) noexcept;

} // namespace micant::km::afdwsk
