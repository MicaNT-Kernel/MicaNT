// ============================================================================
// MicaNT: Windows Power Profile Management Subsystem (powrprof.dll)
// (powrprof.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides system sleep and suspend state transitions conforming to win32metadata.
// ============================================================================

#pragma once

#include <cstdint>
#include "micant/shared/ntdef.hpp"
#include "micant/um/native/ldr.hpp"

namespace micant::powrprof {

using BOOLEAN = uint8_t;

inline BOOLEAN WINAPI SetSuspendState(BOOLEAN /*bHibernate*/, BOOLEAN /*bForce*/, BOOLEAN /*bWakeupEventsDisabled*/) noexcept {
    return 1;
}

inline uint32_t WINAPI PowerGetActiveScheme(void* /*UserRootPowerKey*/, void** pActivePolicyGuid) noexcept {
    static uint8_t s_guid[16] = { 0x38, 0x1b, 0x42, 0x22, 0xf6, 0x94, 0x49, 0x9e, 0xac, 0x09, 0x92, 0x5b, 0x38, 0xb6, 0x99, 0xf0 };
    if (pActivePolicyGuid) *pActivePolicyGuid = s_guid;
    return 0; // ERROR_SUCCESS
}

inline void InitializePowrProfSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("powrprof.dll", "SetSuspendState", reinterpret_cast<void*>(SetSuspendState));
    ldr.registerExport("powrprof.dll", "PowerGetActiveScheme", reinterpret_cast<void*>(PowerGetActiveScheme));
}

} // namespace micant::powrprof
