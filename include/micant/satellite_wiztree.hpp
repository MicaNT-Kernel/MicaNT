// ============================================================================
// MicaNT: Sovereign Operating System Executive
// include/micant/satellite_wiztree.hpp - WizTree Satellite Forwarding Bridge
//
// Modern Clean-Room Forwarding Bridge in pure ISO C++23.
// All 291 WizTree exports have graduated directly to Canonical Core MicaNT Subsystems:
//   - msvcrt.dll        -> include/micant/msvcrt.hpp
//   - comdlg32.dll      -> include/micant/comdlg32.hpp
//   - dwmapi.dll        -> include/micant/dwmapi.hpp
//   - imm32.dll         -> include/micant/imm32.hpp
//   - oleacc.dll        -> include/micant/uiautomation.hpp
//   - windowscodecs.dll -> include/micant/gdiplus.hpp
//   - winspool.drv      -> include/micant/winspool.hpp
//   - ntdll.dll         -> include/micant/ntdll.hpp
//   - crypt32.dll       -> include/micant/crypt32.hpp
//   - oleaut32.dll      -> include/micant/oleaut32.hpp
//   - uxtheme.dll       -> include/micant/uxtheme.hpp
//   - advapi32.dll      -> include/micant/advapi32.hpp
//   - shell32.dll       -> include/micant/shell32.hpp
//   - ole32.dll         -> include/micant/ole32.hpp
//   - winhttp.dll       -> include/micant/winhttp.hpp
//   - comctl32.dll      -> include/micant/comctl32.hpp
//   - gdi32.dll         -> include/micant/gdi32.hpp
//   - msimg32.dll       -> include/micant/gdi32.hpp
//   - kernel32.dll      -> include/micant/kernel32.hpp
//   - user32.dll        -> include/micant/user32.hpp
//   - mpr.dll           -> include/micant/mpr.hpp
//
// 100% Clean-Room Architecture authored from Microsoft win32metadata interface definitions.
// Zero proprietary code used or referenced. Google LLC v. Oracle America (2021).
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <cwchar>
#include <cstring>
#include <vector>

#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "gdi32.hpp"
#include "advapi32.hpp"
#include "comctl32.hpp"
#include "comdlg32.hpp"
#include "shell32.hpp"
#include "ole32.hpp"
#include "oleaut32.hpp"
#include "crypt32.hpp"
#include "winhttp.hpp"
#include "winspool.hpp"
#include "imm32.hpp"
#include "dwmapi.hpp"
#include "uxtheme.hpp"
#include "uiautomation.hpp"
#include "gdiplus.hpp"
#include "msvcrt.hpp"
#include "mpr.hpp"
#include "ntdll.hpp"
#include "ldr.hpp"

namespace micant::satellite::wiztree {

using BOOL = int32_t;
using DWORD = uint32_t;
using HANDLE = void*;
using HWND = void*;
using HDC = void*;

// Forwarding aliases for test suite compatibility
using micant::winhttp::WinHttpOpen;
using micant::winhttp::WinHttpConnect;
using micant::winhttp::WinHttpCloseHandle;

// ============================================================================
// Master Dynamic Loader Registration Bridge (0 registered satellite exports)
// ============================================================================

inline void InitializeWizTreeWin32Exports() noexcept {
    // 0 exports defined in satellite header.
    // All 291 WizTree exports have graduated into canonical Core NT headers!
}

} // namespace micant::satellite::wiztree
