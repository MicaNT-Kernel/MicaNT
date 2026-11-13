#pragma once
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace micant::io { struct FileBackend; }

namespace micant::fsrtl::fat {
NtStatus initialize() noexcept;
NtStatus open(std::u16string_view name, bool openIf, uint32_t desiredAccess,
              io::FileBackend& backend) noexcept;
// Resolve the installed system directory on the unique mounted volume that
// actually contains the system NTDLL. Returns an absolute DOS path.
NtStatus systemDirectory(char16_t* output, size_t capacity, size_t& length) noexcept;
NtStatus mount(const void* owner, Handle blockHandle, uint64_t& token) noexcept;
// Marks the volume offline immediately. Busy means open files still pin it;
// the owner must retry before releasing the underlying controller.
NtStatus unmount(const void* owner, uint64_t token) noexcept;
}
