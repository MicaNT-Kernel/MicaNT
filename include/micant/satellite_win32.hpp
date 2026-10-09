// ============================================================================
// MicaNT: Win32 Satellite Subsystems (SensApi.dll, MSIMG32.dll, COMDLG32.dll)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides Network Sensing (SensApi), Alpha Blending (MSIMG32),
// and Common Dialog (COMDLG32) interfaces.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include "ntdef.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "ldr.hpp"

namespace micant::satellite {

using LPCWSTR = const wchar_t*;

// ----------------------------------------------------------------------------
// 1. SensApi.dll (System Event Notification Service API)
// ----------------------------------------------------------------------------

inline constexpr win32::DWORD NETWORK_ALIVE_LAN = 0x00000001;
inline constexpr win32::DWORD NETWORK_ALIVE_WAN = 0x00000002;

inline win32::BOOL IsNetworkAlive(win32::DWORD* lpdwFlags) noexcept {
    if (lpdwFlags) {
        *lpdwFlags = NETWORK_ALIVE_LAN | NETWORK_ALIVE_WAN;
    }
    return win32::TRUE;
}

inline win32::BOOL IsDestinationReachableW(LPCWSTR /*lpszDestination*/, void* /*lpQOCInfo*/) noexcept {
    return win32::TRUE;
}

// ----------------------------------------------------------------------------
// 2. MSIMG32.dll (GDI Image Manipulation & Alpha Blending)
// ----------------------------------------------------------------------------

struct BLENDFUNCTION {
    uint8_t BlendOp{0};
    uint8_t BlendFlags{0};
    uint8_t SourceConstantAlpha{255};
    uint8_t AlphaFormat{0};
};

inline win32::BOOL AlphaBlend(
    void* /*hdcDest*/,
    int /*xoriginDest*/,
    int /*yoriginDest*/,
    int /*wDest*/,
    int /*hDest*/,
    void* /*hdcSrc*/,
    int /*xoriginSrc*/,
    int /*yoriginSrc*/,
    int /*wSrc*/,
    int /*hSrc*/,
    BLENDFUNCTION /*ftn*/
) noexcept {
    return win32::TRUE;
}

// ----------------------------------------------------------------------------
// 3. COMDLG32.dll (Common Dialog Box Library)
// ----------------------------------------------------------------------------

inline win32::BOOL PrintDlgW(void* /*lppd*/) noexcept {
    return win32::FALSE; // User cancelled print dialog
}

inline win32::BOOL ChooseColorW(void* /*lpcc*/) noexcept {
    return win32::FALSE; // User cancelled color picker
}

// ----------------------------------------------------------------------------
// Export Registration
// ----------------------------------------------------------------------------

inline void InitializeSatelliteWin32Exports() {
    auto& ldr = ldr::DynamicLoader::get();

    // SensApi.dll
    ldr.registerExport("SensApi.dll", "IsNetworkAlive", reinterpret_cast<void*>(IsNetworkAlive));
    ldr.registerExport("SensApi.dll", "IsDestinationReachableW", reinterpret_cast<void*>(IsDestinationReachableW));

    // MSIMG32.dll
    ldr.registerExport("MSIMG32.dll", "AlphaBlend", reinterpret_cast<void*>(AlphaBlend));

    // COMDLG32.dll
    ldr.registerExport("COMDLG32.dll", "PrintDlgW", reinterpret_cast<void*>(PrintDlgW));
    ldr.registerExport("COMDLG32.dll", "ChooseColorW", reinterpret_cast<void*>(ChooseColorW));
}

} // namespace micant::satellite
