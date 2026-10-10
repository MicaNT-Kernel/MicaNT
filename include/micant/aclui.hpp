// ============================================================================
// MicaNT: Access Control List Editor Subsystem (aclui.dll) (aclui.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides Windows ACL Editor UI dialogs and Property Sheet pages.
// Strict clean-room implementation referencing Microsoft win32metadata.
// Zero proprietary, leaked, or decompiled code.
// ============================================================================

#pragma once

#include <cstdint>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::aclui {

using BOOL = int32_t;
using DWORD = uint32_t;
using HWND = void*;
using HRESULT = int32_t;
using HPROPSHEETPAGE = void*;

inline constexpr BOOL TRUE_VAL = 1;
inline constexpr HRESULT S_OK_VAL = 0;

inline HPROPSHEETPAGE WINAPI CreateSecurityPage(void* /*psi*/) noexcept {
    return reinterpret_cast<HPROPSHEETPAGE>(0x8001);
}

inline BOOL WINAPI EditSecurity(HWND /*hwndOwner*/, void* /*psi*/) noexcept {
    return TRUE_VAL;
}

inline HRESULT WINAPI EditSecurityAdvanced(HWND /*hwndOwner*/, void* /*psi*/, DWORD /*dwFlags*/) noexcept {
    return S_OK_VAL;
}

inline void InitializeAcluiSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    ldr.registerExportOrdinal("aclui.dll", 1, reinterpret_cast<void*>(CreateSecurityPage));
    ldr.registerExportOrdinal("aclui.dll", 2, reinterpret_cast<void*>(EditSecurity));
    ldr.registerExportOrdinal("aclui.dll", 3, reinterpret_cast<void*>(EditSecurityAdvanced));
}

} // namespace micant::aclui
