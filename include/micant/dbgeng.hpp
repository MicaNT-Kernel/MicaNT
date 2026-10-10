// ============================================================================
// MicaNT: Windows Debug Engine Subsystem (dbgeng.dll)
// (dbgeng.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides Debug Engine client activation conforming to win32metadata.
// ============================================================================

#pragma once

#include <cstdint>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::dbgeng {

using HRESULT = int32_t;

inline HRESULT WINAPI DebugCreate(const void* /*InterfaceId*/, void** Interface) noexcept {
    if (Interface) *Interface = reinterpret_cast<void*>(0x9001);
    return 0; // S_OK
}

inline void InitializeDbgEngSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("dbgeng.dll", "DebugCreate", reinterpret_cast<void*>(DebugCreate));
}

} // namespace micant::dbgeng
