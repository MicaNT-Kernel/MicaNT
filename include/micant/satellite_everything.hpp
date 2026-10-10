// ============================================================================
// MicaNT: Everything 1.4+ Subsystem Graduation Bridge (satellite_everything.hpp)
//
// Modern Clean-Room Forwarding Bridge in pure ISO C++23.
// All 34 exported symbols have graduated to Canonical Core MicaNT Subsystems:
//   - advapi32.dll -> include/micant/advapi32.hpp (Service Dispatcher, Config, Registry)
//   - gdi32.dll    -> include/micant/gdi32.hpp (TextAlign, OffsetClipRgn, DCOrg, RegionData)
//   - kernel32.dll -> include/micant/kernel32.hpp (__C_specific_handler, Locale, EnvStrings)
//   - ole32.dll    -> include/micant/ole32.hpp (CreateBindCtx)
//   - shell32.dll  -> include/micant/shell32.hpp (PathIsRootW, SHRegGetUSValueW, Ordinal 16)
//   - user32.dll   -> include/micant/user32.hpp (Geometry, HotKeys, Scrolling, Messaging)
// ============================================================================

#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include <cwchar>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <unordered_map>
#include <mutex>

#include "ntdef.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "gdi32.hpp"
#include "advapi32.hpp"
#include "shell32.hpp"
#include "ole32.hpp"
#include "scm.hpp"
#include "ldr.hpp"

namespace micant::satellite::everything {

using BOOL = int32_t;
using DWORD = uint32_t;
using HANDLE = void*;
using HWND = void*;
using HDC = void*;
using HMENU = void*;
using HKEY = void*;
using LPARAM = int64_t;
using WPARAM = uint64_t;
using LRESULT = int64_t;

// Compatibility Type Forwarders
struct SERVICE_TABLE_ENTRYW_MOCK {
    const wchar_t* lpServiceName;
    void* lpServiceProc;
};

using SERVICE_STATUS_MOCK = scm::SERVICE_STATUS;
using QUERY_SERVICE_CONFIGW_MOCK = advapi32::QUERY_SERVICE_CONFIGW;

using POINT_MOCK = gdi32::POINT;
using RECT_MOCK = gdi32::RECT;
using RGNDATAHEADER_MOCK = gdi32::RGNDATAHEADER;
using RGNDATA_MOCK = gdi32::RGNDATA;
using BITMAP_MOCK = gdi32::BITMAP;

using OSVERSIONINFOA_MOCK = kernel32::OSVERSIONINFOA;
using IBindCtxVtbl_Mock = ole32::IBindCtxVtbl;
using IBindCtx_Mock = ole32::IBindCtx;

// Canonical Function Forwarders
// 1. advapi32.dll
using advapi32::RegisterServiceCtrlHandlerW;
using advapi32::StartServiceCtrlDispatcherW;
using advapi32::SetServiceStatus;
using advapi32::QueryServiceConfigW;
using advapi32::RegOpenKeyA;
using advapi32::RegQueryValueW;

// 2. gdi32.dll
using gdi32::GetTextAlign;
using gdi32::OffsetClipRgn;
using gdi32::GetDCOrgEx;
using gdi32::GetRegionData;
using gdi32::GetNearestColor;
using gdi32::CreateBitmapIndirect;

// 3. kernel32.dll
using kernel32::__C_specific_handler;
using kernel32::GetVersionExA;
using kernel32::GetNumberFormatW;
using kernel32::GetCalendarInfoW;
using kernel32::FreeEnvironmentStringsA;
using kernel32::GetEnvironmentStrings;
using kernel32::SetHandleCount;
using kernel32::GetStringTypeA;

// 4. ole32.dll
using ole32::CreateBindCtx;

// 5. shlwapi.dll
using shell32::PathIsRootW;
using shell32::SHRegGetUSValueW;

// 6. user32.dll
using user32::ScrollWindowEx;
using user32::AdjustWindowRect;
using user32::CopyRect;
using user32::OpenIcon;
using user32::GetNextDlgTabItem;
using user32::ReplyMessage;
using user32::RegisterHotKey;
using user32::UnregisterHotKey;
using user32::PostThreadMessageW;
using user32::SendMessageTimeoutW;
using user32::MapVirtualKeyExW;

// ----------------------------------------------------------------------------
// Master Registration Bridge (0 satellite exports)
// ----------------------------------------------------------------------------

inline void registerEverythingExports(ldr::DynamicLoader& /*ldr*/) {
    // All 34 Everything exports graduated to canonical core MicaNT headers!
}

inline void InitializeEverythingExports() {
    advapi32::InitializeAdvapi32SubsystemExports();
    gdi32::InitializeGdi32SubsystemExports();
    win32::InitializeWin32SubsystemExports();
    ole32::InitializeOle32SubsystemExports();
    shell32::InitializeShell32SubsystemExports();
    user32::InitializeUser32SubsystemExports();
}

} // namespace micant::satellite::everything
