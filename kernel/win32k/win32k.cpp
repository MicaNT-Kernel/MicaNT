#define MICANT_KERNEL_WIN32K_CORE
#include "micant/km/win32k/win32k.hpp"

namespace micant::sys {

NtStatus DispatchWin32kShadowSyscall(const micant::sys::SyscallFrame& frame) noexcept {
    if (frame.ssn < micant::win32k::SSN_NTUSER_CREATEWINDOWEX ||
        frame.ssn > micant::win32k::SSN_NTCALLBACKRETURN) {
        return NtStatus::InvalidDeviceRequest;
    }

    auto& subsystem = micant::win32k::TitanWin32kSubsystem::Instance();
    subsystem.initialize();
    return subsystem.dispatchShadowSyscall(frame);
}

} // namespace micant::sys
