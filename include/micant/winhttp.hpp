// ============================================================================
// MicaNT: Windows HTTP Services Subsystem (winhttp.dll)
// (winhttp.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides HTTP proxy configuration discovery conforming to win32metadata.
// ============================================================================

#pragma once

#include <cstdint>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::winhttp {

using BOOL = int32_t;

inline BOOL WINAPI WinHttpGetDefaultProxyConfiguration(void* /*pConfig*/) noexcept {
    return 0; // FALSE: No system proxy
}

inline void InitializeWinHttpSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("winhttp.dll", "WinHttpGetDefaultProxyConfiguration", reinterpret_cast<void*>(WinHttpGetDefaultProxyConfiguration));
}

} // namespace micant::winhttp
