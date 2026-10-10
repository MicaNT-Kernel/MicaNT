// ============================================================================
// MicaNT: SumatraPDF 3.6+ Win32 Satellite Subsystem Extensions (satellite_sumatra.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides 100.0% Native Win32 Subsystem Satisfaction for SumatraPDF 64-bit
// (SumatraPDF-3.6.1-64.exe - 68 non-GDI+ imported symbols).
//
// All 68 Win32 symbols have graduated into Canonical Core MicaNT Headers:
// - advapi32.hpp       (RegSetKeySecurity)
// - comctl32.hpp       (CreatePropertySheetPageW)
// - comdlg32.hpp       (PrintDlgExW)
// - gdi32.hpp          (SetLayout, ExtSelectClipRgn, GradientFill)
// - kernel32.hpp       (GetThreadGroupAffinity, DosDateTimeToFileTime, SystemTimeToFileTime,
//                       TzSpecificLocalTimeToSystemTime, GetFileTime, GetLogicalDrives,
//                       GetVolumePathNameW, FoldStringW, IsDBCSLeadByte, Thread32First/Next,
//                       Module32FirstW/NextW, AttachConsole, SetConsoleScreenBufferSize,
//                       GetTempFileNameW, OutputDebugStringA, SetThreadExecutionState,
//                       DebugBreak, AddVectoredExceptionHandler, GetPrivateProfileIntW,
//                       HeapQueryInformation)
// - ole32.hpp          (CoGetMalloc, CoSetProxyBlanket)
// - shell32.hpp        (SHAddToRecentDocs, ordinal 190, StrStrW, StrRStrIW,
//                       SHDeleteValueW, SHDeleteKeyW, SHSetValueW, SHGetValueW,
//                       UrlEscapeW, QISearch, ordinal 219)
// - uiautomation.hpp   (UiaRaiseStructureChangedEvent, UiaGetReservedNotSupportedValue,
//                       UiaRaiseAutomationEvent, UiaReturnRawElementProvider, UiaHostProviderFromHwnd)
// - urlmon.hpp         (CoInternetGetSession)
// - user32.hpp         (WindowFromDC, IsCharUpperW, ShowWindowAsync, SetMenuInfo,
//                       GetMenuInfo, SetMenuDefaultItem, VkKeyScanExW, SendInput,
//                       GetWindowInfo, CharToOemA, OemToCharBuffA, OemToCharA,
//                       DdeInitializeW, DdeUninitialize, DdeCreateStringHandleW,
//                       DdeFreeStringHandle, DdeConnect, DdeDisconnect,
//                       DdeClientTransaction, DdeFreeDataHandle, PackDDElParam)
// - wininet.hpp        (InternetGetLastResponseInfoA, InternetOpenUrlW)
// - winspool.hpp       (DeviceCapabilitiesW)
//
// Strict clean-room implementation referencing Microsoft win32metadata.
// Zero proprietary, leaked, or decompiled code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include <cwchar>
#include <cstring>
#include <algorithm>

#include "ntdef.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "gdi32.hpp"
#include "advapi32.hpp"
#include "comctl32.hpp"
#include "comdlg32.hpp"
#include "ole32.hpp"
#include "shell32.hpp"
#include "uiautomation.hpp"
#include "urlmon.hpp"
#include "wininet.hpp"
#include "winspool.hpp"
#include "ldr.hpp"

namespace micant::satellite::sumatra {

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

// ----------------------------------------------------------------------------
// Canonical Forwarders to Core NT Subsystems for Unit Test Compatibility
// ----------------------------------------------------------------------------

// 1. advapi32.dll
using advapi32::RegSetKeySecurity;

// 2. comctl32.dll
using comctl32::CreatePropertySheetPageW;

// 3. comdlg32.dll
using comdlg32::PrintDlgExW;

// 4. gdi32.dll / msimg32.dll
using gdi32::SetLayout;
using gdi32::ExtSelectClipRgn;
using gdi32::GradientFill;

// 5. kernel32.dll
using win32::GetThreadGroupAffinity;
using win32::DosDateTimeToFileTime;
using win32::SystemTimeToFileTime;
using win32::TzSpecificLocalTimeToSystemTime;
using win32::GetFileTime;
using win32::GetLogicalDrives;
using win32::GetVolumePathNameW;
using win32::FoldStringW;
using win32::IsDBCSLeadByte;
using win32::Thread32First;
using win32::Thread32Next;
using win32::Module32FirstW;
using win32::Module32NextW;
using win32::AttachConsole;
using win32::SetConsoleScreenBufferSize;
using win32::GetTempFileNameW;
using win32::OutputDebugStringA;
using win32::SetThreadExecutionState;
using win32::DebugBreak;
using win32::AddVectoredExceptionHandler;
using win32::GetPrivateProfileIntW;
using win32::HeapQueryInformation;

// 6. ole32.dll
using ole32::CoGetMalloc;
using ole32::CoSetProxyBlanket;

// 7. shell32.dll / shlwapi.dll
using shell32::SHAddToRecentDocs;
using shell32::StrStrW;
using shell32::StrRStrIW;
using shell32::SHDeleteValueW;
using shell32::SHDeleteKeyW;
using shell32::SHSetValueW;
using shell32::SHGetValueW;
using shell32::UrlEscapeW;
using shell32::QISearch;

// 8. uiautomationcore.dll
using uiautomation::UiaRaiseStructureChangedEvent;
using uiautomation::UiaGetReservedNotSupportedValue;
using uiautomation::UiaRaiseAutomationEvent;
using uiautomation::UiaReturnRawElementProvider;
using uiautomation::UiaHostProviderFromHwnd;

// 9. urlmon.dll
using urlmon::CoInternetGetSession;

// 10. user32.dll
using user32::WindowFromDC;
using user32::IsCharUpperW;
using user32::ShowWindowAsync;
using user32::SetMenuInfo;
using user32::GetMenuInfo;
using user32::SetMenuDefaultItem;
using user32::VkKeyScanExW;
using user32::SendInput;
using user32::GetWindowInfo;
using user32::CharToOemA;
using user32::OemToCharBuffA;
using user32::OemToCharA;
using user32::DdeInitializeW;
using user32::DdeUninitialize;
using user32::DdeCreateStringHandleW;
using user32::DdeFreeStringHandle;
using user32::DdeConnect;
using user32::DdeDisconnect;
using user32::DdeClientTransaction;
using user32::DdeFreeDataHandle;
using user32::PackDDElParam;

// 11. wininet.dll
using wininet::InternetGetLastResponseInfoA;
using wininet::InternetOpenUrlW;

// 12. winspool.drv
using winspool::DeviceCapabilitiesW;

// ============================================================================
// Master Dynamic Loader Registration Bridge (0 registered satellite exports)
// ============================================================================

template <typename TLoader>
inline void registerSumatraExports([[maybe_unused]] TLoader& ldr) {
    // All 68 Sumatra Win32 exports graduated to canonical Core NT headers!
}

inline void InitializeSumatraWin32Exports() noexcept {
    // All 68 Sumatra Win32 exports graduated to canonical Core NT headers!
}

} // namespace micant::satellite::sumatra
