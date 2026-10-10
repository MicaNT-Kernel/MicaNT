// ============================================================================
// MicaNT: Sovereign Operating System Executive
// include/micant/msi.hpp - Windows Installer Subsystem (msi.dll)
//
// 100% Clean-Room Architecture authored from Microsoft win32metadata interface definitions.
// Zero proprietary code used or referenced. Google LLC v. Oracle America (2021).
// ============================================================================

#pragma once

#include <cstdint>
#include <cwchar>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::msi {

inline constexpr uint32_t ERROR_UNKNOWN_PRODUCT = 1605;
inline constexpr int32_t INSTALLSTATE_ADVERTISED = 1;

inline uint32_t __stdcall MsiGetProductInfoW([[maybe_unused]] const wchar_t* szProduct, [[maybe_unused]] const wchar_t* szProperty, wchar_t* lpValueBuf, uint32_t* pcchValueBuf) noexcept {
    if (pcchValueBuf) *pcchValueBuf = 0;
    if (lpValueBuf) lpValueBuf[0] = L'\0';
    return ERROR_UNKNOWN_PRODUCT;
}

inline int32_t __stdcall MsiQueryProductStateW([[maybe_unused]] const wchar_t* szProduct) noexcept {
    return INSTALLSTATE_ADVERTISED;
}

inline void InitializeMsiSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    // Named exports
    ldr.registerExport("msi.dll", "MsiGetProductInfoW", reinterpret_cast<void*>(MsiGetProductInfoW));
    ldr.registerExport("msi.dll", "MsiQueryProductStateW", reinterpret_cast<void*>(MsiQueryProductStateW));

    // Ordinal exports (for WinSCP and legacy setup engines)
    ldr.registerExportOrdinal("msi.dll", 70, reinterpret_cast<void*>(MsiGetProductInfoW));
    ldr.registerExportOrdinal("msi.dll", 205, reinterpret_cast<void*>(MsiQueryProductStateW));
}

} // namespace micant::msi
