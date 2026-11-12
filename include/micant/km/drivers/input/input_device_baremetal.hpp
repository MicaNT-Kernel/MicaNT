#pragma once
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include <string_view>

namespace micant::km::hid {
NtStatus initializeInputDevices() noexcept;
bool isInputDeviceName(std::u16string_view name) noexcept;
NtStatus openInputDevice(Handle* handle, uint32_t access, std::u16string_view name, bool synchronous = false) noexcept;
}
