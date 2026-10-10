// ============================================================================
// MicaNT: Sovereign Operating System Executive
// include/micant/comdlg32.hpp - Windows Common Dialog Box Subsystem (comdlg32.dll)
//
// 100% Clean-Room Architecture authored from Microsoft win32metadata interface definitions.
// Zero proprietary code used or referenced. Google LLC v. Oracle America (2021).
// ============================================================================

#pragma once

#include <cstdint>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::comdlg32 {

inline int32_t __stdcall PrintDlgW([[maybe_unused]] void* lppd) noexcept {
    return 0; // FALSE / User cancelled print dialog
}

inline int32_t __stdcall ChooseColorW([[maybe_unused]] void* lpcc) noexcept {
    return 0; // FALSE / User cancelled color picker
}

inline int32_t __stdcall GetOpenFileNameW([[maybe_unused]] void* lpofn) noexcept {
    return 0; // FALSE / User cancelled open file dialog
}

inline int32_t __stdcall GetSaveFileNameW([[maybe_unused]] void* lpofn) noexcept {
    return 0; // FALSE / User cancelled save file dialog
}

inline uint32_t __stdcall CommDlgExtendedError() noexcept {
    return 0; // No error
}

inline void* __stdcall ReplaceTextW([[maybe_unused]] void* lpfr) noexcept {
    return nullptr;
}

inline void InitializeComDlg32SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("comdlg32.dll", "PrintDlgW", reinterpret_cast<void*>(PrintDlgW));
    ldr.registerExport("comdlg32.dll", "ChooseColorW", reinterpret_cast<void*>(ChooseColorW));
    ldr.registerExport("comdlg32.dll", "GetOpenFileNameW", reinterpret_cast<void*>(GetOpenFileNameW));
    ldr.registerExport("comdlg32.dll", "GetSaveFileNameW", reinterpret_cast<void*>(GetSaveFileNameW));
    ldr.registerExport("comdlg32.dll", "CommDlgExtendedError", reinterpret_cast<void*>(CommDlgExtendedError));
    ldr.registerExport("comdlg32.dll", "ReplaceTextW", reinterpret_cast<void*>(ReplaceTextW));
}

} // namespace micant::comdlg32
