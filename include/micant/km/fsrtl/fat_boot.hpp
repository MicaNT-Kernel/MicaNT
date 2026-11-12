#pragma once
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include <cstdint>
#include <string_view>

namespace micant::io { struct FileBackend; }

namespace micant::fsrtl::fat {
NtStatus initialize() noexcept;
NtStatus open(std::u16string_view name, io::FileBackend& backend) noexcept;
NtStatus mount(const void* owner, Handle blockHandle, uint64_t& token) noexcept;
// Marks the volume offline immediately. Busy means open files still pin it;
// the owner must retry before releasing the underlying controller.
NtStatus unmount(const void* owner, uint64_t token) noexcept;
}
