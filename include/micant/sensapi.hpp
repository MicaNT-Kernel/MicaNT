// ============================================================================
// MicaNT: Sovereign Operating System Executive
// include/micant/sensapi.hpp - System Event Notification Service API (sensapi.dll)
//
// 100% Clean-Room Architecture authored from Microsoft win32metadata interface definitions.
// Zero proprietary code used or referenced. Google LLC v. Oracle America (2021).
// ============================================================================

#pragma once

#include <cstdint>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::sensapi {

inline constexpr uint32_t NETWORK_ALIVE_LAN = 0x00000001;
inline constexpr uint32_t NETWORK_ALIVE_WAN = 0x00000002;

inline int32_t __stdcall IsNetworkAlive(uint32_t* lpdwFlags) noexcept {
    if (lpdwFlags) {
        *lpdwFlags = NETWORK_ALIVE_LAN | NETWORK_ALIVE_WAN;
    }
    return 1; // TRUE
}

inline int32_t __stdcall IsDestinationReachableW([[maybe_unused]] const wchar_t* lpszDestination, [[maybe_unused]] void* lpQOCInfo) noexcept {
    return 1; // TRUE
}

inline void InitializeSensApiSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("sensapi.dll", "IsNetworkAlive", reinterpret_cast<void*>(IsNetworkAlive));
    ldr.registerExport("sensapi.dll", "IsDestinationReachableW", reinterpret_cast<void*>(IsDestinationReachableW));
}

} // namespace micant::sensapi
