// ============================================================================
// MicaNT: Input Method Manager Subsystem (imm32.dll)
// (imm32.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides Input Method Editor (IME) context and window management conforming to win32metadata.
// ============================================================================

#pragma once

#include <cstdint>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::imm32 {

using HWND = void*;
using HIMC = void*;

inline uint32_t WINAPI ImmGetVirtualKey(HWND /*hWnd*/) noexcept {
    return 0;
}

inline HWND WINAPI ImmGetDefaultIMEWnd(HWND /*hWnd*/) noexcept {
    return reinterpret_cast<HWND>(0xC001);
}

inline HIMC WINAPI ImmAssociateContext(HWND /*hWnd*/, HIMC /*hIMC*/) noexcept {
    return nullptr;
}

inline void InitializeImm32SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("imm32.dll", "ImmGetVirtualKey", reinterpret_cast<void*>(ImmGetVirtualKey));
    ldr.registerExport("imm32.dll", "ImmGetDefaultIMEWnd", reinterpret_cast<void*>(ImmGetDefaultIMEWnd));
    ldr.registerExport("imm32.dll", "ImmAssociateContext", reinterpret_cast<void*>(ImmAssociateContext));
}

} // namespace micant::imm32
