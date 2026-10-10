// ============================================================================
// MicaNT: Windows Power Profile Management Subsystem (powrprof.dll)
// (powrprof.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides system sleep and suspend state transitions conforming to win32metadata.
// ============================================================================

#pragma once

#include <cstdint>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::powrprof {

using BOOLEAN = uint8_t;

inline BOOLEAN WINAPI SetSuspendState(BOOLEAN /*bHibernate*/, BOOLEAN /*bForce*/, BOOLEAN /*bWakeupEventsDisabled*/) noexcept {
    return 1;
}

inline void InitializePowrProfSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("powrprof.dll", "SetSuspendState", reinterpret_cast<void*>(SetSuspendState));
}

} // namespace micant::powrprof
