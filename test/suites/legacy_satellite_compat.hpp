// ============================================================================
// MicaNT Test Suite: Legacy Satellite Test Compatibility Fixtures
// (test/suites/legacy_satellite_compat.hpp)
//
// Contains legacy test aliases for Milestone 216-228 verification suites.
// Zero satellite files remain in include/micant/ (100% Core NT architecture).
// ============================================================================

#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <cwchar>
#include <vector>

#include "micant/ntdef.hpp"
#include "micant/ntstatus.hpp"
#include "micant/kernel32.hpp"
#include "micant/user32.hpp"
#include "micant/gdi32.hpp"
#include "micant/gdiplus.hpp"
#include "micant/advapi32.hpp"
#include "micant/shell32.hpp"
#include "micant/comctl32.hpp"
#include "micant/comdlg32.hpp"
#include "micant/sensapi.hpp"
#include "micant/msvcrt.hpp"
#include "micant/msi.hpp"
#include "micant/sspi.hpp"
#include "micant/crypt32.hpp"
#include "micant/ws2_32.hpp"
#include "micant/ole32.hpp"
#include "micant/oleaut32.hpp"
#include "micant/uxtheme.hpp"
#include "micant/propsys.hpp"
#include "micant/urlmon.hpp"
#include "micant/wininet.hpp"
#include "micant/winspool.hpp"
#include "micant/uiautomation.hpp"
#include "micant/tsf.hpp"
#include "micant/ldr.hpp"
#include "micant/mpr.hpp"
#include "micant/virtdisk.hpp"
#include "micant/wintrust.hpp"
#include "micant/setupapi.hpp"
#include "micant/ntdll.hpp"
#include "micant/aclui.hpp"
#include "micant/winsta.hpp"
#include "micant/icuuc.hpp"
#include "micant/authz.hpp"
#include "micant/userenv.hpp"
#include "micant/winhttp.hpp"
#include "micant/imm32.hpp"
#include "micant/powrprof.hpp"
#include "micant/dbgeng.hpp"
#include "micant/winrt.hpp"
#include "micant/iphlpapi.hpp"
#include "micant/cipherksp.hpp"
#include "micant/prism3d12.hpp"

// ----------------------------------------------------------------------------
// From satellite_wiztree.hpp
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// From satellite_putty.hpp
// ----------------------------------------------------------------------------
namespace micant::satellite::putty {

using BOOL = int32_t;
using DWORD = uint32_t;
using HANDLE = void*;
using HWND = void*;
using HDC = void*;
using HGDIOBJ = void*;
using HFONT = void*;
using HMENU = void*;
using HICON = void*;
using HCURSOR = void*;
using HBITMAP = void*;
using HBRUSH = void*;
using HPEN = void*;
using LPARAM = int64_t;
using WPARAM = uint64_t;
using LRESULT = int64_t;
using SIZE_T = size_t;
using UINT = uint32_t;
using LONG = int32_t;
using BYTE = uint8_t;
using WORD = uint16_t;
using ULONG_PTR = uintptr_t;
using LONG_PTR = intptr_t;
using UINT_PTR = uintptr_t;

// ----------------------------------------------------------------------------
// Type Aliases to Canonical Core Subsystems
// ----------------------------------------------------------------------------

using LOGFONTA = micant::gdi32::LOGFONTA;
using TEXTMETRICA = micant::gdi32::TEXTMETRICA;
using ABCFLOAT = micant::gdi32::ABCFLOAT;
using CHARSETINFO = micant::gdi32::CHARSETINFO;

using DCB = micant::win32::DCB;
using COMMTIMEOUTS = micant::win32::COMMTIMEOUTS;
using MEMORYSTATUS = micant::win32::MEMORYSTATUS;
using OVERLAPPED = micant::win32::OVERLAPPED;

using WNDCLASSA = micant::user32::WNDCLASSA;

// ----------------------------------------------------------------------------
// Helper: ANSI to Wide String Conversion
// ----------------------------------------------------------------------------
inline std::wstring AnsiToWide(const char* str) {
    if (!str) return L"";
    std::wstring result;
    result.reserve(std::strlen(str));
    while (*str) {
        result.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*str++)));
    }
    return result;
}

// ----------------------------------------------------------------------------
// Function Forwarders to Canonical Core Subsystems
// ----------------------------------------------------------------------------

// 1. ADVAPI32.dll
using micant::advapi32::CopySid;
using micant::advapi32::GetUserNameA;
using micant::advapi32::RegDeleteKeyA;
using micant::advapi32::RegEnumKeyA;

// 2. COMDLG32.dll
using micant::comdlg32::ChooseColorA;
using micant::comdlg32::ChooseFontA;
using micant::comdlg32::GetOpenFileNameA;
using micant::comdlg32::GetSaveFileNameA;

// 3. IMM32.dll
using micant::tsf::ImmSetCompositionFontA;

// 4. KERNEL32.dll
using micant::win32::Beep;
using micant::win32::ClearCommBreak;
using micant::win32::SetCommBreak;
using micant::win32::GetCommState;
using micant::win32::SetCommState;
using micant::win32::SetCommTimeouts;
using micant::win32::SetHandleInformation;
using micant::win32::CreateEventA;
using micant::win32::CreateMutexA;
using micant::win32::CreateFileMappingA;
using micant::win32::CreateNamedPipeA;
using micant::win32::WaitNamedPipeA;
using micant::win32::CreatePipe;
using micant::win32::FindResourceA;
using micant::win32::GetOverlappedResult;
using micant::win32::GetSystemDirectoryA;
using micant::win32::GetWindowsDirectoryA;
using micant::win32::GetTempPathA;
using micant::win32::GetThreadTimes;
using micant::win32::GlobalMemoryStatus;
using micant::win32::LocalFileTimeToFileTime;

// 5. GDI32.dll
using micant::gdi32::CreateFontA;
using micant::gdi32::CreateFontIndirectA;
using micant::gdi32::GetCharABCWidthsFloatA;
using micant::gdi32::GetCharWidth32A;
using micant::gdi32::GetCharWidth32W;
using micant::gdi32::GetCharWidthA;
using micant::gdi32::GetCharWidthW;
using micant::gdi32::GetCharacterPlacementW;
using micant::gdi32::GetObjectA;
using micant::gdi32::GetOutlineTextMetricsA;
using micant::gdi32::GetTextExtentPointA;
using micant::gdi32::GetTextMetricsA;
using micant::gdi32::TranslateCharsetInfo;
using micant::gdi32::UpdateColors;

// 6. USER32.dll
using micant::user32::CreateDialogParamA;
using micant::user32::DefDlgProcA;
using micant::user32::DefWindowProcA;
using micant::user32::DialogBoxParamA;
using micant::user32::FindWindowA;
using micant::user32::FlashWindow;
using micant::user32::GetClipboardOwner;
using micant::user32::GetMessageA;
using micant::user32::GetQueueStatus;
using micant::user32::GetWindowLongPtrA;
using micant::user32::GetWindowTextLengthA;
using micant::user32::GetWindowTextA;
using micant::user32::InsertMenuA;
using micant::user32::LoadCursorA;
using micant::user32::LoadIconA;
using micant::user32::LoadImageA;
using micant::user32::MessageBoxIndirectW;
using micant::user32::PostMessageA;
using micant::user32::RegisterClassA;
using micant::user32::RegisterClipboardFormatA;
using micant::user32::RegisterWindowMessageA;
using micant::user32::SendDlgItemMessageA;
using micant::user32::SetClassLongPtrA;
using micant::user32::SetWindowLongPtrA;
using micant::user32::SetWindowTextA;
using micant::user32::ToAsciiEx;

// ============================================================================
// Master Registration Function (0 satellite exports registered)
// All 70 PuTTY Win32 exports are registered directly by canonical Core NT
// initializers:
//   - micant::advapi32::InitializeAdvapi32SubsystemExports()
//   - micant::comdlg32::InitializeComDlg32SubsystemExports()
//   - micant::tsf::InitializeTextServicesExports()
//   - micant::win32::InitializeWin32SubsystemExports()
//   - micant::gdi32::InitializeGdi32SubsystemExports()
//   - micant::user32::InitializeUser32SubsystemExports()
// ============================================================================

template <typename TLoader>
inline void registerPuTTYExports([[maybe_unused]] TLoader& ldr) {
    // 0 satellite exports registered - all exports graduated to Core MicaNT
}

inline void InitializePuTTYWin32Exports() {
    // 0 satellite exports registered - all exports graduated to Core MicaNT
}

} // namespace micant::satellite::putty

// ----------------------------------------------------------------------------
// From satellite_gdiplus.hpp
// ----------------------------------------------------------------------------
namespace micant::satellite::gdiplus {

using namespace micant::gdiplus;

template <typename TLoader>
inline void registerGdiPlusExports(TLoader&) {
    // 0 exports defined in satellite header.
    // All 98 GDI+ exports have graduated directly to canonical core
    // include/micant/gdiplus.hpp (micant::gdiplus::InitializeGdiPlusExports).
}

inline void InitializeGdiPlusSatelliteExports() {
    // 0 exports defined in satellite header.
    // Core exports are registered via micant::gdiplus::InitializeGdiPlusExports().
}

} // namespace micant::satellite::gdiplus

// ----------------------------------------------------------------------------
// From satellite_sumatra.hpp
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// From satellite_everything.hpp
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// From satellite_winmerge.hpp
// ----------------------------------------------------------------------------
namespace micant::satellite::winmerge {

using BOOL = int32_t;
using DWORD = uint32_t;
using HANDLE = void*;
using HWND = void*;
using HDC = void*;
using HMENU = void*;
using HKEY = void*;
using HGLOBAL = void*;
using HLOCAL = void*;
using LPARAM = int64_t;
using WPARAM = uint64_t;
using LRESULT = int64_t;
using HRESULT = int32_t;

// Backward-compatible type aliases for existing test suites
using SIZE_MOCK = gdi32::SIZE;
using POINT_MOCK = prismx::POINT;
using RECT_MOCK = prismx::RECT;
using ENUMLOGFONTW_MOCK = gdi32::ENUMLOGFONTW;
using NEWTEXTMETRICW_MOCK = gdi32::NEWTEXTMETRICW;
using ColorPaletteMock = gdiplus::ColorPalette;
using ACTCTXW_MOCK = kernel32::ACTCTXW;
using SLIST_HEADER_MOCK = kernel32::SLIST_HEADER;
using SLIST_ENTRY_MOCK = kernel32::SLIST_ENTRY;
using SYSTEMTIME_MOCK = kernel32::SYSTEMTIME;
using PROPERTYKEY_MOCK = propsys::PROPERTYKEY;
using ACCEL_MOCK = user32::ACCEL;
using MARGINS_MOCK = uxtheme::MARGINS;
using JOB_INFO_1W_MOCK = winspool::JOB_INFO_1W;

// ============================================================================
// Forwarding Bridges to Canonical Core MicaNT Subsystems
// ============================================================================

// 1. advapi32.dll
inline int32_t WINAPI RegSetValueW(void* hKey, const wchar_t* lpSubKey, uint32_t dwType, const wchar_t* lpData, uint32_t cbData) noexcept {
    return advapi32::RegSetValueW(hKey, lpSubKey, dwType, lpData, cbData);
}

inline int32_t WINAPI RegDeleteTreeW(void* hKey, const wchar_t* lpSubKey) noexcept {
    return advapi32::RegDeleteTreeW(hKey, lpSubKey);
}

// 2. comctl32.dll
inline void WINAPI InitCommonControls() noexcept {
    comctl32::InitCommonControls();
}

// 3. gdi32.dll
inline uint32_t WINAPI GetLayout(HDC hdc) noexcept {
    return gdi32::GetLayout(hdc);
}

inline int32_t WINAPI SetPolyFillMode(HDC hdc, int32_t mode) noexcept {
    return gdi32::SetPolyFillMode(hdc, mode);
}

inline int32_t WINAPI GetPolyFillMode(HDC hdc) noexcept {
    return gdi32::GetPolyFillMode(hdc);
}

inline win32::BOOL WINAPI SetViewportExtEx(HDC hdc, int32_t x, int32_t y, SIZE_MOCK* lpsz) noexcept {
    return gdi32::SetViewportExtEx(hdc, x, y, lpsz);
}

inline win32::BOOL WINAPI GetViewportExtEx(HDC hdc, SIZE_MOCK* lpsz) noexcept {
    return gdi32::GetViewportExtEx(hdc, lpsz);
}

inline win32::BOOL WINAPI SetWindowExtEx(HDC hdc, int32_t x, int32_t y, SIZE_MOCK* lpsz) noexcept {
    return gdi32::SetWindowExtEx(hdc, x, y, lpsz);
}

inline win32::BOOL WINAPI GetWindowExtEx(HDC hdc, SIZE_MOCK* lpsz) noexcept {
    return gdi32::GetWindowExtEx(hdc, lpsz);
}

inline win32::BOOL WINAPI OffsetViewportOrgEx(HDC hdc, int32_t x, int32_t y, POINT_MOCK* lppt) noexcept {
    return gdi32::OffsetViewportOrgEx(hdc, x, y, reinterpret_cast<gdi32::POINT*>(lppt));
}

inline win32::BOOL WINAPI ScaleViewportExtEx(HDC hdc, int32_t xn, int32_t xd, int32_t yn, int32_t yd, SIZE_MOCK* lpsz) noexcept {
    return gdi32::ScaleViewportExtEx(hdc, xn, xd, yn, yd, lpsz);
}

inline win32::BOOL WINAPI ScaleWindowExtEx(HDC hdc, int32_t xn, int32_t xd, int32_t yn, int32_t yd, SIZE_MOCK* lpsz) noexcept {
    return gdi32::ScaleWindowExtEx(hdc, xn, xd, yn, yd, lpsz);
}

inline int32_t WINAPI GetTextFaceW(HDC hdc, int32_t nCount, wchar_t* lpFaceName) noexcept {
    return gdi32::GetTextFaceW(hdc, nCount, lpFaceName);
}

inline void* WINAPI CreateEllipticRgn(int32_t x1, int32_t y1, int32_t x2, int32_t y2) noexcept {
    return gdi32::CreateEllipticRgn(x1, y1, x2, y2);
}

inline win32::BOOL WINAPI PtVisible(HDC hdc, int32_t x, int32_t y) noexcept {
    return gdi32::PtVisible(hdc, x, y);
}

inline int32_t WINAPI Escape(HDC hdc, int32_t nEscape, int32_t cbInput, const char* lpvInData, void* lpvOutData) noexcept {
    return gdi32::Escape(hdc, nEscape, cbInput, lpvInData, lpvOutData);
}

using FONTENUMPROCW_MOCK = int32_t(*)(const ENUMLOGFONTW_MOCK*, const NEWTEXTMETRICW_MOCK*, uint32_t, int64_t);

inline int32_t WINAPI EnumFontFamiliesW(HDC hdc, const wchar_t* lpLogfont, FONTENUMPROCW_MOCK lpProc, int64_t lParam) noexcept {
    return gdi32::EnumFontFamiliesW(hdc, lpLogfont, reinterpret_cast<void*>(lpProc), lParam);
}

inline void* WINAPI CopyMetaFileW(void* hmfSrc, const wchar_t* lpszFile) noexcept {
    return gdi32::CopyMetaFileW(hmfSrc, lpszFile);
}

// 4. gdiplus.dll
inline int32_t WINAPI GdipAddPathArcI(void* path, int32_t x, int32_t y, int32_t width, int32_t height, float startAngle, float sweepAngle) noexcept {
    return gdiplus::GdipAddPathArcI(path, x, y, width, height, startAngle, sweepAngle);
}

inline int32_t WINAPI GdipClosePathFigure(void* path) noexcept {
    return gdiplus::GdipClosePathFigure(path);
}

inline int32_t WINAPI GdipAddPathLineI(void* path, int32_t x1, int32_t y1, int32_t x2, int32_t y2) noexcept {
    return gdiplus::GdipAddPathLineI(path, x1, y1, x2, y2);
}

inline int32_t WINAPI GdipAddPathBezierI(void* path, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, int32_t x4, int32_t y4) noexcept {
    return gdiplus::GdipAddPathBezierI(path, x1, y1, x2, y2, x3, y3, x4, y4);
}

inline int32_t WINAPI GdipStartPathFigure(void* path) noexcept {
    return gdiplus::GdipStartPathFigure(path);
}

inline int32_t WINAPI GdipDrawBezierI(void* graphics, void* pen, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, int32_t x4, int32_t y4) noexcept {
    return gdiplus::GdipDrawBezierI(graphics, pen, x1, y1, x2, y2, x3, y3, x4, y4);
}

inline int32_t WINAPI GdipDrawImageRectI(void* graphics, void* image, int32_t x, int32_t y, int32_t width, int32_t height) noexcept {
    return gdiplus::GdipDrawImageRectI(graphics, image, x, y, width, height);
}

inline int32_t WINAPI GdipGetImagePalette(void* image, ColorPaletteMock* palette, int32_t size) noexcept {
    return gdiplus::GdipGetImagePalette(image, palette, size);
}

inline int32_t WINAPI GdipGetImagePaletteSize(void* image, int32_t* size) noexcept {
    return gdiplus::GdipGetImagePaletteSize(image, size);
}

inline int32_t WINAPI GdipCreateBitmapFromFile(const wchar_t* filename, void** bitmap) noexcept {
    return gdiplus::GdipCreateBitmapFromFile(filename, bitmap);
}

inline int32_t WINAPI GdipSaveImageToStream(void* image, void* stream, const void* clsidEncoder, const void* encoderParams) noexcept {
    return gdiplus::GdipSaveImageToStream(image, stream, clsidEncoder, encoderParams);
}

inline int32_t WINAPI GdipDrawLinesI(void* graphics, void* pen, const void* points, int32_t count) noexcept {
    return gdiplus::GdipDrawLinesI(graphics, pen, points, count);
}

// 5. kernel32.dll
inline void* WINAPI GlobalReAlloc(void* hMem, size_t dwBytes, uint32_t uFlags) noexcept {
    return kernel32::GlobalReAlloc(hMem, dwBytes, uFlags);
}

inline void* WINAPI LocalReAlloc(void* hMem, size_t uBytes, uint32_t uFlags) noexcept {
    return kernel32::LocalReAlloc(hMem, uBytes, uFlags);
}

inline void* WINAPI GlobalHandle(const void* pMem) noexcept {
    return kernel32::GlobalHandle(pMem);
}

inline uint32_t WINAPI GlobalFlags(void* hMem) noexcept {
    return kernel32::GlobalFlags(hMem);
}

inline uint16_t WINAPI SetThreadUILanguage(uint16_t LangId) noexcept {
    return kernel32::SetThreadUILanguage(LangId);
}

inline win32::BOOL WINAPI SetSearchPathMode(uint32_t Flags) noexcept {
    return kernel32::SetSearchPathMode(Flags);
}

inline win32::BOOL WINAPI SetDllDirectoryW(const wchar_t* lpPathName) noexcept {
    return kernel32::SetDllDirectoryW(lpPathName);
}

inline uint32_t WINAPI GetSystemWow64DirectoryW(wchar_t* lpBuffer, uint32_t uSize) noexcept {
    return kernel32::GetSystemWow64DirectoryW(lpBuffer, uSize);
}

inline uint32_t WINAPI ExpandEnvironmentStringsA(const char* lpSrc, char* lpDst, uint32_t nSize) noexcept {
    return kernel32::ExpandEnvironmentStringsA(lpSrc, lpDst, nSize);
}

inline void* WINAPI CreateActCtxW(const ACTCTXW_MOCK* pActCtx) noexcept {
    return kernel32::CreateActCtxW(pActCtx);
}

inline win32::BOOL WINAPI ActivateActCtx(void* hActCtx, uintptr_t* lpCookie) noexcept {
    return kernel32::ActivateActCtx(hActCtx, lpCookie);
}

inline win32::BOOL WINAPI DeactivateActCtx(uint32_t dwFlags, uintptr_t ulCookie) noexcept {
    return kernel32::DeactivateActCtx(dwFlags, ulCookie);
}

inline win32::BOOL WINAPI FindActCtxSectionStringW(uint32_t dwFlags, const void* lpExtensionGuid, uint32_t ulSectionId, const wchar_t* lpStringToFind, void* ReturnedData) noexcept {
    return kernel32::FindActCtxSectionStringW(dwFlags, lpExtensionGuid, ulSectionId, lpStringToFind, ReturnedData);
}

inline win32::BOOL WINAPI QueryActCtxW(uint32_t dwFlags, void* hActCtx, void* pvSubInstance, uint32_t ulInfoClass, void* pvBuffer, size_t cbBuffer, size_t* pcbWritten) noexcept {
    return kernel32::QueryActCtxW(dwFlags, hActCtx, pvSubInstance, ulInfoClass, pvBuffer, cbBuffer, pcbWritten);
}

inline uint32_t WINAPI GetProfileIntW(const wchar_t* lpAppName, const wchar_t* lpKeyName, int32_t nDefault) noexcept {
    return kernel32::GetProfileIntW(lpAppName, lpKeyName, nDefault);
}

inline uint32_t WINAPI GlobalGetAtomNameW(uint16_t nAtom, wchar_t* lpBuffer, int32_t nSize) noexcept {
    return kernel32::GlobalGetAtomNameW(nAtom, lpBuffer, nSize);
}

inline int32_t WINAPI lstrcmpA(const char* lpString1, const char* lpString2) noexcept {
    return kernel32::lstrcmpA(lpString1, lpString2);
}

inline win32::BOOL WINAPI LockFile(void* hFile, uint32_t dwFileOffsetLow, uint32_t dwFileOffsetHigh, uint32_t nNumberOfBytesToLockLow, uint32_t nNumberOfBytesToLockHigh) noexcept {
    return kernel32::LockFile(hFile, dwFileOffsetLow, dwFileOffsetHigh, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh);
}

inline win32::BOOL WINAPI UnlockFile(void* hFile, uint32_t dwFileOffsetLow, uint32_t dwFileOffsetHigh, uint32_t nNumberOfBytesToUnlockLow, uint32_t nNumberOfBytesToUnlockHigh) noexcept {
    return kernel32::UnlockFile(hFile, dwFileOffsetLow, dwFileOffsetHigh, nNumberOfBytesToUnlockLow, nNumberOfBytesToUnlockHigh);
}

inline void* WINAPI FindResourceExW(void* hModule, const wchar_t* lpType, const wchar_t* lpName, uint16_t wLanguage) noexcept {
    return kernel32::FindResourceExW(hModule, lpType, lpName, wLanguage);
}

inline SLIST_ENTRY_MOCK* WINAPI InterlockedPushEntrySList(SLIST_HEADER_MOCK* ListHead, SLIST_ENTRY_MOCK* ListEntry) noexcept {
    return kernel32::InterlockedPushEntrySList(ListHead, ListEntry);
}

inline uint32_t WINAPI GetThreadId(void* Thread) noexcept {
    return kernel32::GetThreadId(Thread);
}

// 6. ole32.dll & oledlg.dll
inline int32_t WINAPI CoCreateFreeThreadedMarshaler(void* punkOuter, void** ppunkMarshaler) noexcept {
    return ole32::CoCreateFreeThreadedMarshaler(punkOuter, ppunkMarshaler);
}

inline win32::BOOL WINAPI OleTranslateAccelerator(void* lpFrame, void* lpFrameInfo, void* lpmsg) noexcept {
    return ole32::OleTranslateAccelerator(lpFrame, lpFrameInfo, lpmsg);
}

inline int32_t WINAPI OleDestroyMenuDescriptor(void* holemenu) noexcept {
    return ole32::OleDestroyMenuDescriptor(holemenu);
}

inline void* WINAPI OleCreateMenuDescriptor(void* hmenuCombined, void* lpMenuWidths) noexcept {
    return ole32::OleCreateMenuDescriptor(hmenuCombined, lpMenuWidths);
}

inline int32_t WINAPI CoRegisterMessageFilter(void* lpMessageFilter, void** lplpMessageFilter) noexcept {
    return ole32::CoRegisterMessageFilter(lpMessageFilter, lplpMessageFilter);
}

inline void WINAPI CoFreeUnusedLibraries() noexcept {
    ole32::CoFreeUnusedLibraries();
}

inline void* WINAPI OleDuplicateData(void* hSrc, uint16_t cfFormat, uint32_t uiFlags) noexcept {
    return ole32::OleDuplicateData(hSrc, cfFormat, uiFlags);
}

inline int32_t WINAPI CoLockObjectExternal(void* pUnk, win32::BOOL fLock, win32::BOOL fLastUnlockReleases) noexcept {
    return ole32::CoLockObjectExternal(pUnk, fLock, fLastUnlockReleases);
}

inline int32_t WINAPI CoGetObject(const wchar_t* pszName, void* pBindOptions, const void* riid, void** ppv) noexcept {
    return ole32::CoGetObject(pszName, pBindOptions, riid, ppv);
}

inline int32_t WINAPI OleRun(void* pUnknown) noexcept {
    return ole32::OleRun(pUnknown);
}

inline int32_t WINAPI PropVariantClear(void* pvar) noexcept {
    return ole32::PropVariantClear(pvar);
}

inline uint32_t WINAPI OleUIBusyW(void* lp) noexcept {
    return ole32::OleUIBusyW(lp);
}

// 7. oleacc.dll (uiautomation)
inline int32_t WINAPI AccessibleObjectFromWindow(HWND hwnd, uint32_t dwId, const void* riid, void** ppvObject) noexcept {
    return uiautomation::AccessibleObjectFromWindow(hwnd, dwId, riid, ppvObject);
}

inline int32_t WINAPI CreateStdAccessibleObject(HWND hwnd, int32_t idObject, const void* riid, void** ppvObject) noexcept {
    return uiautomation::CreateStdAccessibleObject(hwnd, idObject, riid, ppvObject);
}

// 8. oleaut32.dll
inline int32_t WINAPI CreateErrorInfo(void** pperrinfo) noexcept {
    return oleaut32::CreateErrorInfo(pperrinfo);
}

inline int32_t WINAPI SetErrorInfo(uint32_t dwReserved, void* perrinfo) noexcept {
    return oleaut32::SetErrorInfo(dwReserved, perrinfo);
}

inline int32_t WINAPI VarDateFromStr(const wchar_t* strIn, uint32_t lcid, uint32_t dwFlags, double* pdateOut) noexcept {
    return oleaut32::VarDateFromStr(strIn, lcid, dwFlags, pdateOut);
}

inline win32::BOOL WINAPI VariantTimeToSystemTime(double vtime, SYSTEMTIME_MOCK* lpSystemTime) noexcept {
    return oleaut32::VariantTimeToSystemTime(vtime, lpSystemTime);
}

inline win32::BOOL WINAPI SystemTimeToVariantTime(SYSTEMTIME_MOCK* lpSystemTime, double* pvtime) noexcept {
    return oleaut32::SystemTimeToVariantTime(lpSystemTime, pvtime);
}

// 9. propsys.dll
inline int32_t WINAPI PSGetPropertyKeyFromName(const wchar_t* pszCanonicalName, PROPERTYKEY_MOCK* propkey) noexcept {
    return propsys::PSGetPropertyKeyFromName(pszCanonicalName, propkey);
}

inline int32_t WINAPI PSEnumeratePropertyDescriptions(int32_t filter, const void* riid, void** ppv) noexcept {
    return propsys::PSEnumeratePropertyDescriptions(filter, riid, ppv);
}

inline int32_t WINAPI PropVariantCompareEx(const void* propvar1, const void* propvar2, uint32_t unit, uint32_t flags) noexcept {
    return propsys::PropVariantCompareEx(propvar1, propvar2, unit, flags);
}

inline int32_t WINAPI PSGetPropertyDescription(const PROPERTYKEY_MOCK* propkey, const void* riid, void** ppv) noexcept {
    return propsys::PSGetPropertyDescription(propkey, riid, ppv);
}

inline int32_t WINAPI PSFormatForDisplayAlloc(const PROPERTYKEY_MOCK* key, const void* propvar, uint32_t pdfFlags, wchar_t** ppszDisplay) noexcept {
    return propsys::PSFormatForDisplayAlloc(key, propvar, pdfFlags, ppszDisplay);
}

inline int32_t WINAPI InitPropVariantFromBuffer(const void* pv, uint32_t cb, void* ppropvar) noexcept {
    return propsys::InitPropVariantFromBuffer(pv, cb, ppropvar);
}

// 10. shell32.dll & shlwapi.dll
inline int32_t WINAPI SHCreateShellItem(const void* pidlParent, void* psfParent, const void* pidl, void** ppsi) noexcept {
    return shell32::SHCreateShellItem(pidlParent, psfParent, pidl, ppsi);
}

inline void WINAPI ILFree(void* pidl) noexcept {
    shell32::ILFree(pidl);
}

inline int32_t WINAPI SHGetPropertyStoreFromParsingName(const wchar_t* pszPath, void* pbc, uint32_t flags, const void* riid, void** ppv) noexcept {
    return shell32::SHGetPropertyStoreFromParsingName(pszPath, pbc, flags, riid, ppv);
}

inline int32_t WINAPI SetCurrentProcessExplicitAppUserModelID(const wchar_t* AppID) noexcept {
    return shell32::SetCurrentProcessExplicitAppUserModelID(AppID);
}

inline int32_t WINAPI CDefFolderMenu_Create2(void* pidlFolder, HWND hwnd, uint32_t cidl, const void* apidl, void* psf, void* lpfn, uint32_t nKeys, const void* ahkeyClsKeys, void** ppv) noexcept {
    return shell32::CDefFolderMenu_Create2(pidlFolder, hwnd, cidl, apidl, psf, lpfn, nKeys, ahkeyClsKeys, ppv);
}

inline win32::BOOL WINAPI PathStripToRootW(wchar_t* pszPath) noexcept {
    return shell32::PathStripToRootW(pszPath);
}

inline int32_t WINAPI StrCmpLogicalW(const wchar_t* psz1, const wchar_t* psz2) noexcept {
    return shell32::StrCmpLogicalW(psz1, psz2);
}

inline uint32_t WINAPI PathGetCharTypeW(wchar_t ch) noexcept {
    return shell32::PathGetCharTypeW(ch);
}

inline win32::BOOL WINAPI UrlIsW(const wchar_t* pszUrl, int32_t UrlIs) noexcept {
    return shell32::UrlIsW(pszUrl, UrlIs);
}

inline int32_t WINAPI SHAutoComplete(HWND hwndEdit, uint32_t dwFlags) noexcept {
    return shell32::SHAutoComplete(hwndEdit, dwFlags);
}

inline win32::BOOL WINAPI PathCompactPathW(HDC hdc, wchar_t* pszPath, uint32_t dx) noexcept {
    return shell32::PathCompactPathW(hdc, pszPath, dx);
}

inline wchar_t* WINAPI StrFormatByteSizeW(int64_t qdw, wchar_t* pszBuf, uint32_t cchBuf) noexcept {
    return shell32::StrFormatByteSizeW(qdw, pszBuf, cchBuf);
}

inline win32::BOOL WINAPI StrTrimW(wchar_t* psz, const wchar_t* pszTrimChars) noexcept {
    return shell32::StrTrimW(psz, pszTrimChars);
}

inline wchar_t* WINAPI StrChrW(const wchar_t* pszStart, wchar_t wMatch) noexcept {
    return shell32::StrChrW(pszStart, wMatch);
}

inline win32::BOOL WINAPI PathIsUNCW(const wchar_t* pszPath) noexcept {
    return shell32::PathIsUNCW(pszPath);
}

inline wchar_t* WINAPI SysAllocString(const wchar_t* psz) noexcept {
    return shell32::SysAllocString_Shlwapi(psz);
}

inline int32_t WINAPI VariantCopyInd(void* pvarDest, const void* pvargSrc) noexcept {
    return shell32::VariantCopyInd_Shlwapi(pvarDest, pvargSrc);
}

// 11. user32.dll
inline int32_t WINAPI CopyAcceleratorTableW(void* hAccelSrc, ACCEL_MOCK* lpAccelDst, int32_t cAccelEntries) noexcept {
    return user32::CopyAcceleratorTableW(hAccelSrc, lpAccelDst, cAccelEntries);
}

inline HWND WINAPI RealChildWindowFromPoint(HWND hwndParent, POINT_MOCK pt) noexcept {
    return user32::RealChildWindowFromPoint(hwndParent, pt);
}

inline win32::BOOL WINAPI UnionRect(RECT_MOCK* lprcDst, const RECT_MOCK* lprcSrc1, const RECT_MOCK* lprcSrc2) noexcept {
    return user32::UnionRect(lprcDst, lprcSrc1, lprcSrc2);
}

inline int32_t WINAPI GetTabbedTextExtentW(HDC hdc, const wchar_t* lpString, int32_t chCount, int32_t nTabPositions, const int32_t* lpnTabStopPositions) noexcept {
    return user32::GetTabbedTextExtentW(hdc, lpString, chCount, nTabPositions, lpnTabStopPositions);
}

inline int64_t WINAPI ReuseDDElParam(int64_t lParam, uint32_t msgIn, uint32_t msgOut, uintptr_t uiLo, uintptr_t uiHi) noexcept {
    return user32::ReuseDDElParam(lParam, msgIn, msgOut, uiLo, uiHi);
}

inline win32::BOOL WINAPI UnpackDDElParam(uint32_t msg, int64_t lParam, uintptr_t* puiLo, uintptr_t* puiHi) noexcept {
    return user32::UnpackDDElParam(msg, lParam, puiLo, puiHi);
}

inline win32::BOOL WINAPI WinHelpW(HWND hWndMain, const wchar_t* lpszHelp, uint32_t uCommand, uintptr_t dwData) noexcept {
    return user32::WinHelpW(hWndMain, lpszHelp, uCommand, dwData);
}

inline uint32_t WINAPI GetMenuCheckMarkDimensions() noexcept {
    return user32::GetMenuCheckMarkDimensions();
}

inline HWND WINAPI ChildWindowFromPoint(HWND hWndParent, POINT_MOCK Point) noexcept {
    return user32::ChildWindowFromPoint(hWndParent, Point);
}

inline void* WINAPI GetThreadDesktop(uint32_t dwThreadId) noexcept {
    return user32::GetThreadDesktop(dwThreadId);
}

inline win32::BOOL WINAPI GetUserObjectInformationW(void* hObj, int32_t nIndex, void* pvInfo, uint32_t nLength, uint32_t* lpnLengthNeeded) noexcept {
    return user32::GetUserObjectInformationW(hObj, nIndex, pvInfo, nLength, lpnLengthNeeded);
}

inline win32::BOOL WINAPI DragDetect(HWND hwnd, POINT_MOCK pt) noexcept {
    return user32::DragDetect(hwnd, pt);
}

inline win32::BOOL WINAPI IsMenu(HMENU hMenu) noexcept {
    return user32::IsMenu(hMenu);
}

using GRAYSTRINGPROC_MOCK = user32::GRAYSTRINGPROC;

inline win32::BOOL WINAPI GrayStringW(HDC hdc, void* hbr, GRAYSTRINGPROC_MOCK lpOutputFunc, int64_t lpData, int32_t nCount, int32_t X, int32_t Y, int32_t nWidth, int32_t nHeight) noexcept {
    return user32::GrayStringW(hdc, hbr, lpOutputFunc, lpData, nCount, X, Y, nWidth, nHeight);
}

inline int32_t WINAPI TabbedTextOutW(HDC hdc, int32_t X, int32_t Y, const wchar_t* lpString, int32_t chCount, int32_t nTabPositions, const int32_t* lpnTabStopPositions, int32_t nTabOrigin) noexcept {
    return user32::TabbedTextOutW(hdc, X, Y, lpString, chCount, nTabPositions, lpnTabStopPositions, nTabOrigin);
}

inline int32_t wsprintfA(char* lpOut, const char* lpFmt, ...) noexcept {
    if (!lpOut || !lpFmt) return 0;
    std::va_list args;
    va_start(args, lpFmt);
    int res = std::vsnprintf(lpOut, 1024, lpFmt, args);
    va_end(args);
    return res > 0 ? res : 0;
}

inline const wchar_t* WINAPI CharPrevW(const wchar_t* lpszStart, const wchar_t* lpszCurrent) noexcept {
    return user32::CharPrevW(lpszStart, lpszCurrent);
}

inline win32::BOOL WINAPI GetCaretPos(POINT_MOCK* lpPoint) noexcept {
    return user32::GetCaretPos(lpPoint);
}

// 12. uxtheme.dll
inline win32::BOOL WINAPI IsThemeActive() noexcept {
    return uxtheme::IsThemeActive();
}

inline win32::BOOL WINAPI IsAppThemed() noexcept {
    return uxtheme::IsAppThemed();
}

inline win32::HRESULT WINAPI GetThemeMargins(void* hTheme, HDC hdc, int32_t iPartId, int32_t iStateId, int32_t iPropId, const void* prc, MARGINS_MOCK* pMargins) noexcept {
    return uxtheme::GetThemeMargins(hTheme, hdc, iPartId, iStateId, iPropId, prc, pMargins);
}

inline win32::HRESULT WINAPI GetThemeInt(void* hTheme, int32_t iPartId, int32_t iStateId, int32_t iPropId, int32_t* piVal) noexcept {
    return uxtheme::GetThemeInt(hTheme, iPartId, iStateId, iPropId, piVal);
}

inline win32::HRESULT WINAPI DrawThemeText(void* hTheme, HDC hdc, int32_t iPartId, int32_t iStateId, const wchar_t* pszText, int32_t iCharCount, uint32_t dwTextFlags, uint32_t dwTextFlags2, const void* pRect) noexcept {
    return uxtheme::DrawThemeText(hTheme, hdc, iPartId, iStateId, pszText, iCharCount, dwTextFlags, dwTextFlags2, pRect);
}

inline win32::BOOL WINAPI IsThemeBackgroundPartiallyTransparent(void* hTheme, int32_t iPartId, int32_t iStateId) noexcept {
    return uxtheme::IsThemeBackgroundPartiallyTransparent(hTheme, iPartId, iStateId);
}

// 13. wininet.dll
inline win32::BOOL WINAPI InternetGetLastResponseInfoW(uint32_t* lpdwError, wchar_t* lpszBuffer, uint32_t* lpdwBufferLength) noexcept {
    return wininet::InternetGetLastResponseInfoW(lpdwError, lpszBuffer, lpdwBufferLength);
}

// 14. winspool.drv
inline win32::BOOL WINAPI GetJobW(void* hPrinter, uint32_t JobId, uint32_t Level, uint8_t* pJob, uint32_t cbBuf, uint32_t* pcbNeeded) noexcept {
    return winspool::GetJobW(reinterpret_cast<uintptr_t>(hPrinter), JobId, Level, pJob, cbBuf, pcbNeeded);
}

// ============================================================================
// Master Dynamic Loader Registration Bridge (0 registered satellite exports)
// ============================================================================

inline void InitializeWinMergeExports() {
    // 0 exports defined in satellite header.
    // All 121 WinMerge exports have graduated into canonical Core NT headers!
}

} // namespace micant::satellite::winmerge

// ----------------------------------------------------------------------------
// From satellite_rufus.hpp
// ----------------------------------------------------------------------------
namespace micant::satellite::rufus {

using BOOL = int32_t;
using BOOLEAN = uint8_t;
using DWORD = uint32_t;
using ULONG = uint32_t;
using USHORT = uint16_t;
using UCHAR = uint8_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using HANDLE = void*;
using HWND = void*;
using HDC = void*;
using HICON = void*;
using HKEY = void*;
using HCRYPTPROV = uintptr_t;
using HCRYPTKEY = uintptr_t;
using HCRYPTHASH = uintptr_t;
using HCRYPTMSG = void*;
using HWINEVENTHOOK = void*;
using HDEVINFO = void*;
using LSTATUS = int32_t;
using HRESULT = int32_t;
using LCID = uint32_t;
using ULONGLONG = uint64_t;
using DWORD64 = uint64_t;
using LONG_PTR = intptr_t;

inline constexpr BOOL TRUE_VAL = 1;
inline constexpr BOOL FALSE_VAL = 0;
inline constexpr DWORD ERROR_SUCCESS_VAL = 0;
inline constexpr DWORD ERROR_NO_MORE_FILES_VAL = 18;
inline constexpr HRESULT S_OK_VAL = 0;

// ============================================================================
// Forwarding Bridges to Canonical Core MicaNT Subsystems
// ============================================================================

// 1. advapi32.dll
inline BOOL WINAPI ConvertStringSecurityDescriptorToSecurityDescriptorA(
    const char* StringSecurityDescriptor,
    DWORD StringSDRevision,
    void** SecurityDescriptor,
    ULONG* SecurityDescriptorSize
) noexcept {
    return advapi32::ConvertStringSecurityDescriptorToSecurityDescriptorA(
        StringSecurityDescriptor, StringSDRevision, SecurityDescriptor, SecurityDescriptorSize);
}

inline BOOL WINAPI ConvertStringSidToSidA(const char* StringSid, void** Sid) noexcept {
    return advapi32::ConvertStringSidToSidA(StringSid, Sid);
}

inline BOOL WINAPI CryptImportKey(
    HCRYPTPROV hProv, const BYTE* pbData, DWORD dwDataLen,
    HCRYPTKEY hPubKey, DWORD dwFlags, HCRYPTKEY* phKey
) noexcept {
    return advapi32::CryptImportKey(hProv, pbData, dwDataLen, hPubKey, dwFlags, phKey);
}

inline BOOL WINAPI CryptVerifySignatureW(
    HCRYPTHASH hHash, const BYTE* pbSignature, DWORD dwSigLen,
    HCRYPTKEY hPubKey, const wchar_t* szDescription, DWORD dwFlags
) noexcept {
    return advapi32::CryptVerifySignatureW(hHash, pbSignature, dwSigLen, hPubKey, szDescription, dwFlags);
}

inline LSTATUS WINAPI RegDeleteValueA(HKEY hKey, const char* lpValueName) noexcept {
    return advapi32::RegDeleteValueA(hKey, lpValueName);
}

inline LSTATUS WINAPI RegGetValueA(
    HKEY hkey, const char* lpSubKey, const char* lpValue,
    DWORD dwFlags, DWORD* pdwType, void* pvData, DWORD* pcbData
) noexcept {
    return advapi32::RegGetValueA(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}

inline LSTATUS WINAPI RegLoadKeyA(HKEY hKey, const char* lpSubKey, const char* lpFile) noexcept {
    return advapi32::RegLoadKeyA(hKey, lpSubKey, lpFile);
}

inline LSTATUS WINAPI RegUnLoadKeyA(HKEY hKey, const char* lpSubKey) noexcept {
    return advapi32::RegUnLoadKeyA(hKey, lpSubKey);
}

inline BOOLEAN WINAPI SystemFunction036(void* RandomBuffer, ULONG RandomBufferLength) noexcept {
    return static_cast<BOOLEAN>(advapi32::SystemFunction036(RandomBuffer, RandomBufferLength));
}

// 2. crypt32.dll
inline BOOL WINAPI CertGetCertificateChain(
    void* hChainEngine, void* pCertContext, void* pTime,
    void* hAdditionalStore, void* pChainPara, DWORD dwFlags,
    void* pvReserved, void** ppChainContext
) noexcept {
    return crypt32::CertGetCertificateChain(hChainEngine, pCertContext, pTime, hAdditionalStore, pChainPara, dwFlags, pvReserved, ppChainContext);
}

inline BOOL WINAPI CryptDecodeObjectEx(
    DWORD dwCertEncodingType, const char* lpszStructType,
    const BYTE* pbEncoded, DWORD cbEncoded, DWORD dwFlags,
    void* pDecodePara, void* pvStructInfo, DWORD* pcbStructInfo
) noexcept {
    return crypt32::CryptDecodeObjectEx(dwCertEncodingType, lpszStructType, pbEncoded, cbEncoded, dwFlags, pDecodePara, pvStructInfo, pcbStructInfo);
}

inline BOOL WINAPI CryptHashCertificate(
    uintptr_t hCryptProv, DWORD Algid, DWORD dwFlags,
    const BYTE* pbEncoded, DWORD cbEncoded,
    BYTE* pbComputedHash, DWORD* pcbComputedHash
) noexcept {
    return crypt32::CryptHashCertificate(hCryptProv, Algid, dwFlags, pbEncoded, cbEncoded, pbComputedHash, pcbComputedHash);
}

inline HCRYPTMSG WINAPI CryptMsgOpenToDecode(
    DWORD dwMsgEncodingType, DWORD dwFlags, DWORD dwMsgType,
    uintptr_t hCryptProv, void* pRecipientInfo, void* pStreamInfo
) noexcept {
    return crypt32::CryptMsgOpenToDecode(dwMsgEncodingType, dwFlags, dwMsgType, hCryptProv, pRecipientInfo, pStreamInfo);
}

inline BOOL WINAPI CryptMsgUpdate(HCRYPTMSG hCryptMsg, const BYTE* pbData, DWORD cbData, BOOL fFinal) noexcept {
    return crypt32::CryptMsgUpdate(hCryptMsg, pbData, cbData, fFinal);
}

// 3. gdi32.dll
inline int WINAPI EnumFontFamiliesExA(HDC hdc, void* lpLogfont, void* lpProc, LONG_PTR lParam, DWORD dwFlags) noexcept {
    return gdi32::EnumFontFamiliesExA(hdc, lpLogfont, lpProc, lParam, dwFlags);
}

// 4. kernel32.dll
inline HANDLE WINAPI FindFirstVolumeA(char* lpszVolumeName, DWORD cchBufferLength) noexcept {
    return win32::FindFirstVolumeA(lpszVolumeName, cchBufferLength);
}

inline BOOL WINAPI FindNextVolumeA(HANDLE hFindVolume, char* lpszVolumeName, DWORD cchBufferLength) noexcept {
    return win32::FindNextVolumeA(hFindVolume, lpszVolumeName, cchBufferLength);
}

inline BOOL WINAPI FindVolumeClose(HANDLE hFindVolume) noexcept {
    return win32::FindVolumeClose(hFindVolume);
}

inline BOOL WINAPI GetVolumeNameForVolumeMountPointA(const char* lpszVolumeMountPoint, char* lpszVolumeName, DWORD cchBufferLength) noexcept {
    return win32::GetVolumeNameForVolumeMountPointA(lpszVolumeMountPoint, lpszVolumeName, cchBufferLength);
}

inline BOOL WINAPI GetVolumePathNameA(const char* lpszFileName, char* lpszVolumePathName, DWORD cchBufferLength) noexcept {
    return win32::GetVolumePathNameA(lpszFileName, lpszVolumePathName, cchBufferLength);
}

inline BOOL WINAPI GetVolumeInformationA(
    const char* lpRootPathName, char* lpVolumeNameBuffer, DWORD nVolumeNameSize,
    DWORD* lpVolumeSerialNumber, DWORD* lpMaximumComponentLength,
    DWORD* lpFileSystemFlags, char* lpFileSystemNameBuffer, DWORD nFileSystemNameSize
) noexcept {
    return win32::GetVolumeInformationA(lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize, lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags, lpFileSystemNameBuffer, nFileSystemNameSize);
}

inline BOOL WINAPI GetVolumeInformationByHandleW(
    HANDLE hFile, wchar_t* lpVolumeNameBuffer, DWORD nVolumeNameSize,
    DWORD* lpVolumeSerialNumber, DWORD* lpMaximumComponentLength,
    DWORD* lpFileSystemFlags, wchar_t* lpFileSystemNameBuffer, DWORD nFileSystemNameSize
) noexcept {
    return win32::GetVolumeInformationByHandleW(hFile, lpVolumeNameBuffer, nVolumeNameSize, lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags, lpFileSystemNameBuffer, nFileSystemNameSize);
}

inline BOOL WINAPI SetVolumeMountPointA(const char* lpszVolumeMountPoint, const char* lpszVolumeName) noexcept {
    return win32::SetVolumeMountPointA(lpszVolumeMountPoint, lpszVolumeName);
}

inline BOOL WINAPI DeleteVolumeMountPointA(const char* lpszVolumeMountPoint) noexcept {
    return win32::DeleteVolumeMountPointA(lpszVolumeMountPoint);
}

inline BOOL WINAPI SetVolumeLabelA(const char* lpRootPathName, const char* lpVolumeName) noexcept {
    return win32::SetVolumeLabelA(lpRootPathName, lpVolumeName);
}

inline BOOL WINAPI DefineDosDeviceA(DWORD dwFlags, const char* lpDeviceName, const char* lpTargetPath) noexcept {
    return win32::DefineDosDeviceA(dwFlags, lpDeviceName, lpTargetPath);
}

inline DWORD WINAPI QueryDosDeviceA(const char* lpDeviceName, char* lpTargetPath, DWORD ucchMax) noexcept {
    return win32::QueryDosDeviceA(lpDeviceName, lpTargetPath, ucchMax);
}

inline BOOL WINAPI GetDiskFreeSpaceExA(
    const char* lpDirectoryName, uint64_t* lpFreeBytesAvailableToCaller,
    uint64_t* lpTotalNumberOfBytes, uint64_t* lpTotalNumberOfFreeBytes
) noexcept {
    return win32::GetDiskFreeSpaceExA(lpDirectoryName, lpFreeBytesAvailableToCaller, lpTotalNumberOfBytes, lpTotalNumberOfFreeBytes);
}

inline DWORD WINAPI GetLogicalDriveStringsA(DWORD nBufferLength, char* lpBuffer) noexcept {
    return win32::GetLogicalDriveStringsA(nBufferLength, lpBuffer);
}

inline BOOL WINAPI CancelIoEx(HANDLE hFile, void* lpOverlapped) noexcept {
    return win32::CancelIoEx(hFile, lpOverlapped);
}

inline BOOL WINAPI CancelSynchronousIo(HANDLE hThread) noexcept {
    return win32::CancelSynchronousIo(hThread);
}

inline BOOL WINAPI GetOverlappedResultEx(
    HANDLE hFile, void* lpOverlapped, DWORD* lpNumberOfBytesTransferred,
    DWORD dwMilliseconds, BOOL bAlertable
) noexcept {
    return win32::GetOverlappedResultEx(hFile, lpOverlapped, lpNumberOfBytesTransferred, dwMilliseconds, bAlertable);
}

inline BOOL WINAPI SleepConditionVariableCS(void* ConditionVariable, void* CriticalSection, DWORD dwMilliseconds) noexcept {
    return win32::SleepConditionVariableCS(ConditionVariable, CriticalSection, dwMilliseconds);
}

inline BOOL WINAPI CreateSymbolicLinkW(const wchar_t* lpSymlinkFileName, const wchar_t* lpTargetFileName, DWORD dwFlags) noexcept {
    return win32::CreateSymbolicLinkW(lpSymlinkFileName, lpTargetFileName, dwFlags);
}

inline HWND WINAPI GetConsoleWindow() noexcept {
    return win32::GetConsoleWindow();
}

inline BOOL WINAPI EnumUILanguagesW(void* lpUILanguageEnumProc, DWORD dwFlags, LONG_PTR lParam) noexcept {
    return win32::EnumUILanguagesW(lpUILanguageEnumProc, dwFlags, lParam);
}

inline LCID WINAPI GetSystemDefaultLCID() noexcept {
    return win32::GetSystemDefaultLCID();
}

inline WORD WINAPI GetThreadUILanguage() noexcept {
    return win32::GetThreadUILanguage();
}

inline int WINAPI LCIDToLocaleName(LCID Locale, wchar_t* lpName, int cchName, DWORD dwFlags) noexcept {
    return win32::LCIDToLocaleName(Locale, lpName, cchName, dwFlags);
}

inline BOOL WINAPI SetDefaultDllDirectories(DWORD DirectoryFlags) noexcept {
    return win32::SetDefaultDllDirectories(DirectoryFlags);
}

inline BOOL WINAPI SetFileAttributesA(const char* lpFileName, DWORD dwFileAttributes) noexcept {
    return win32::SetFileAttributesA(lpFileName, dwFileAttributes);
}

inline BOOL WINAPI VerifyVersionInfoA(void* lpVersionInformation, DWORD dwTypeMask, uint64_t dwlConditionMask) noexcept {
    return win32::VerifyVersionInfoA(lpVersionInformation, dwTypeMask, dwlConditionMask);
}

inline DWORD WINAPI K32GetModuleFileNameExW(HANDLE hProcess, void* hModule, wchar_t* lpFilename, DWORD nSize) noexcept {
    return win32::K32GetModuleFileNameExW(hProcess, hModule, lpFilename, nSize);
}

inline DWORD WINAPI K32GetProcessImageFileNameW(HANDLE hProcess, wchar_t* lpImageFileName, DWORD nSize) noexcept {
    return win32::K32GetProcessImageFileNameW(hProcess, lpImageFileName, nSize);
}

// 5. ntdll.dll
inline NtStatus WINAPI NtAdjustPrivilegesToken(
    HANDLE TokenHandle, BOOLEAN DisableAllPrivileges, void* NewState,
    ULONG BufferLength, void* PreviousState, ULONG* ReturnLength
) noexcept {
    return ntdll::NtAdjustPrivilegesToken(TokenHandle, DisableAllPrivileges, NewState, BufferLength, PreviousState, ReturnLength);
}

inline NtStatus WINAPI NtCreateFile(
    HANDLE* FileHandle, uint32_t DesiredAccess, void* ObjectAttributes,
    void* IoStatusBlock, void* AllocationSize, uint32_t FileAttributes,
    uint32_t ShareAccess, uint32_t CreateDisposition, uint32_t CreateOptions,
    void* EaBuffer, uint32_t EaLength
) noexcept {
    return ntdll::NtCreateFile_Export(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, AllocationSize, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, EaBuffer, EaLength);
}

inline NtStatus WINAPI NtDelayExecution(BOOLEAN Alertable, const int64_t* DelayInterval) noexcept {
    return ntdll::NtDelayExecution_Export(Alertable, DelayInterval);
}

inline NtStatus WINAPI NtDeviceIoControlFile(
    HANDLE FileHandle, HANDLE Event, void* ApcRoutine, void* ApcContext,
    void* IoStatusBlock, uint32_t IoControlCode, const void* InputBuffer,
    uint32_t InputBufferLength, void* OutputBuffer, uint32_t OutputBufferLength
) noexcept {
    return ntdll::NtDeviceIoControlFile_Export(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, IoControlCode, InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);
}

inline NtStatus WINAPI NtDuplicateObject(
    HANDLE SourceProcessHandle, HANDLE SourceHandle, HANDLE TargetProcessHandle,
    HANDLE* TargetHandle, uint32_t DesiredAccess, uint32_t HandleAttributes, uint32_t Options
) noexcept {
    return ntdll::NtDuplicateObject(SourceProcessHandle, SourceHandle, TargetProcessHandle, TargetHandle, DesiredAccess, HandleAttributes, Options);
}

inline NtStatus WINAPI NtFlushBuffersFile(HANDLE FileHandle, void* IoStatusBlock) noexcept {
    return ntdll::NtFlushBuffersFile(FileHandle, IoStatusBlock);
}

inline NtStatus WINAPI NtFsControlFile(
    HANDLE FileHandle, HANDLE Event, void* ApcRoutine, void* ApcContext,
    void* IoStatusBlock, uint32_t FsControlCode, const void* InputBuffer,
    uint32_t InputBufferLength, void* OutputBuffer, uint32_t OutputBufferLength
) noexcept {
    return ntdll::NtFsControlFile(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, FsControlCode, InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);
}

inline NtStatus WINAPI NtOpenProcess(
    HANDLE* ProcessHandle, uint32_t DesiredAccess, void* ObjectAttributes, void* ClientId
) noexcept {
    return ntdll::NtOpenProcess(ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
}

inline NtStatus WINAPI NtOpenProcessToken(HANDLE ProcessHandle, uint32_t DesiredAccess, HANDLE* TokenHandle) noexcept {
    return ntdll::NtOpenProcessToken_Export(ProcessHandle, DesiredAccess, TokenHandle);
}

inline NtStatus WINAPI NtOpenSymbolicLinkObject(HANDLE* LinkHandle, uint32_t DesiredAccess, void* ObjectAttributes) noexcept {
    return ntdll::NtOpenSymbolicLinkObject(LinkHandle, DesiredAccess, ObjectAttributes);
}

inline NtStatus WINAPI NtQueryEaFile(
    HANDLE FileHandle, void* IoStatusBlock, void* Buffer, uint32_t Length,
    BOOLEAN ReturnSingleEntry, void* EaList, uint32_t EaListLength, uint32_t* EaIndex, BOOLEAN RestartScan
) noexcept {
    return ntdll::NtQueryEaFile(FileHandle, IoStatusBlock, Buffer, Length, ReturnSingleEntry, EaList, EaListLength, EaIndex, RestartScan);
}

inline NtStatus WINAPI NtQueryObject(
    HANDLE Handle, uint32_t ObjectInformationClass, void* ObjectInformation, uint32_t Length, uint32_t* ReturnLength
) noexcept {
    return ntdll::NtQueryObject(Handle, ObjectInformationClass, ObjectInformation, Length, ReturnLength);
}

inline NtStatus WINAPI NtQuerySecurityObject(
    HANDLE Handle, uint32_t SecurityInformation, void* SecurityDescriptor, uint32_t Length, uint32_t* LengthNeeded
) noexcept {
    return ntdll::NtQuerySecurityObject(Handle, SecurityInformation, SecurityDescriptor, Length, LengthNeeded);
}

inline NtStatus WINAPI NtQuerySystemInformation(
    uint32_t SystemInformationClass, void* SystemInformation, uint32_t SystemInformationLength, uint32_t* ReturnLength
) noexcept {
    return ntdll::NtQuerySystemInformation_Export(SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);
}

inline NtStatus WINAPI NtQueryVolumeInformationFile(
    HANDLE FileHandle, void* IoStatusBlock, void* FsInformation, uint32_t Length, uint32_t FsInformationClass
) noexcept {
    return ntdll::NtQueryVolumeInformationFile(FileHandle, IoStatusBlock, FsInformation, Length, FsInformationClass);
}

inline NtStatus WINAPI NtSetEaFile(HANDLE FileHandle, void* IoStatusBlock, void* Buffer, uint32_t Length) noexcept {
    return ntdll::NtSetEaFile(FileHandle, IoStatusBlock, Buffer, Length);
}

inline NtStatus WINAPI NtSetSecurityObject(HANDLE Handle, uint32_t SecurityInformation, void* SecurityDescriptor) noexcept {
    return ntdll::NtSetSecurityObject(Handle, SecurityInformation, SecurityDescriptor);
}

inline void WINAPI RtlCaptureContext(void* ContextRecord) noexcept {
    ntdll::RtlCaptureContext(ContextRecord);
}

inline void* WINAPI RtlLookupFunctionEntry(DWORD64 ControlPc, DWORD64* ImageBase, void* HistoryTable) noexcept {
    return ntdll::RtlLookupFunctionEntry(ControlPc, ImageBase, HistoryTable);
}

inline void* WINAPI RtlPcToFileHeader(void* PcValue, void** BaseOfImage) noexcept {
    return ntdll::RtlPcToFileHeader(PcValue, BaseOfImage);
}

inline void WINAPI RtlUnwind(void* TargetFrame, void* TargetIp, void* ExceptionRecord, void* ReturnValue) noexcept {
    ntdll::RtlUnwind(TargetFrame, TargetIp, ExceptionRecord, ReturnValue);
}

inline void WINAPI RtlUnwindEx(void* TargetFrame, void* TargetIp, void* ExceptionRecord, void* ReturnValue, void* ContextRecord, void* HistoryTable) noexcept {
    ntdll::RtlUnwindEx(TargetFrame, TargetIp, ExceptionRecord, ReturnValue, ContextRecord, HistoryTable);
}

inline DWORD64 WINAPI RtlVirtualUnwind(
    ULONG HandlerType, DWORD64 ImageBase, DWORD64 ControlPc,
    void* FunctionEntry, void* ContextRecord, void** HandlerData,
    DWORD64* EstablisherFrame, void* ContextPointers
) noexcept {
    return ntdll::RtlVirtualUnwind(HandlerType, ImageBase, ControlPc, FunctionEntry, ContextRecord, HandlerData, EstablisherFrame, ContextPointers);
}

inline uint64_t WINAPI VerSetConditionMask(uint64_t ConditionMask, uint32_t TypeMask, uint8_t Condition) noexcept {
    return ntdll::VerSetConditionMask(ConditionMask, TypeMask, Condition);
}

// 6. ole32.dll
inline HRESULT WINAPI CoInitializeSecurity(
    void* pSecDesc, int32_t cAuthSvc, void* asAuthSvc, void* pReserved1,
    DWORD dwAuthnLevel, DWORD dwImpLevel, void* pAuthList, DWORD dwCapabilities, void* pReserved3
) noexcept {
    return ole32::CoInitializeSecurity(pSecDesc, cAuthSvc, asAuthSvc, pReserved1, dwAuthnLevel, dwImpLevel, pAuthList, dwCapabilities, pReserved3);
}

// 7. setupapi.dll
inline DWORD WINAPI CM_Get_Child(ULONG* pdnDevInst, ULONG dnDevInst, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_Child(pdnDevInst, dnDevInst, ulFlags);
}

inline DWORD WINAPI CM_Get_DevNode_Registry_PropertyA(
    ULONG dnDevInst, ULONG ulProperty, ULONG* pulRegDataType,
    void* Buffer, ULONG* pulLength, ULONG ulFlags
) noexcept {
    return setupapi::CM_Get_DevNode_Registry_PropertyA(dnDevInst, ulProperty, pulRegDataType, Buffer, pulLength, ulFlags);
}

inline DWORD WINAPI CM_Get_DevNode_Status(ULONG* pulStatus, ULONG* pulProblemNumber, ULONG dnDevInst, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_DevNode_Status(pulStatus, pulProblemNumber, dnDevInst, ulFlags);
}

inline DWORD WINAPI CM_Get_Device_IDA(ULONG dnDevInst, char* Buffer, ULONG BufferLen, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_Device_IDA(dnDevInst, Buffer, BufferLen, ulFlags);
}

inline DWORD WINAPI CM_Get_Device_ID_ListA(const char* pszFilter, char* Buffer, ULONG BufferLen, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_Device_ID_ListA(pszFilter, Buffer, BufferLen, ulFlags);
}

inline DWORD WINAPI CM_Get_Device_ID_List_SizeA(ULONG* pulLen, const char* pszFilter, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_Device_ID_List_SizeA(pulLen, pszFilter, ulFlags);
}

inline DWORD WINAPI CM_Get_Parent(ULONG* pdnDevInst, ULONG dnDevInst, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_Parent(pdnDevInst, dnDevInst, ulFlags);
}

inline DWORD WINAPI CM_Get_Sibling(ULONG* pdnDevInst, ULONG dnDevInst, ULONG ulFlags) noexcept {
    return setupapi::CM_Get_Sibling(pdnDevInst, dnDevInst, ulFlags);
}

inline DWORD WINAPI CM_Locate_DevNodeA(ULONG* pdnDevInst, const char* pDeviceID, ULONG ulFlags) noexcept {
    return setupapi::CM_Locate_DevNodeA(pdnDevInst, pDeviceID, ulFlags);
}

inline BOOL WINAPI SetupDiChangeState(HDEVINFO DeviceInfoSet, void* DeviceInfoData) noexcept {
    return setupapi::SetupDiChangeState(DeviceInfoSet, DeviceInfoData);
}

inline HDEVINFO WINAPI SetupDiGetClassDevsA(const void* ClassGuid, const char* Enumerator, HWND hwndParent, DWORD Flags) noexcept {
    return setupapi::SetupDiGetClassDevsA(ClassGuid, Enumerator, hwndParent, Flags);
}

inline BOOL WINAPI SetupDiGetDeviceInstanceIdA(
    HDEVINFO DeviceInfoSet, void* DeviceInfoData, char* DeviceInstanceId,
    DWORD DeviceInstanceIdSize, DWORD* RequiredSize
) noexcept {
    return setupapi::SetupDiGetDeviceInstanceIdA(DeviceInfoSet, DeviceInfoData, DeviceInstanceId, DeviceInstanceIdSize, RequiredSize);
}

inline BOOL WINAPI SetupDiGetDeviceInterfaceDetailA(
    HDEVINFO DeviceInfoSet, void* DeviceInterfaceData, void* DeviceInterfaceDetailData,
    DWORD DeviceInterfaceDetailDataSize, DWORD* RequiredSize, void* DeviceInfoData
) noexcept {
    return setupapi::SetupDiGetDeviceInterfaceDetailA(DeviceInfoSet, DeviceInterfaceData, DeviceInterfaceDetailData, DeviceInterfaceDetailDataSize, RequiredSize, DeviceInfoData);
}

inline BOOL WINAPI SetupDiGetDeviceRegistryPropertyA(
    HDEVINFO DeviceInfoSet, void* DeviceInfoData, DWORD Property,
    DWORD* PropertyRegDataType, BYTE* PropertyBuffer, DWORD PropertyBufferSize, DWORD* RequiredSize
) noexcept {
    return setupapi::SetupDiGetDeviceRegistryPropertyA(DeviceInfoSet, DeviceInfoData, Property, PropertyRegDataType, PropertyBuffer, PropertyBufferSize, RequiredSize);
}

inline BOOL WINAPI SetupDiSetClassInstallParamsW(
    HDEVINFO DeviceInfoSet, void* DeviceInfoData, void* ClassInstallParams, DWORD ClassInstallParamsSize
) noexcept {
    return setupapi::SetupDiSetClassInstallParamsW(DeviceInfoSet, DeviceInfoData, ClassInstallParams, ClassInstallParamsSize);
}

// 8. shell32.dll
inline int WINAPI SHCreateDirectoryExA(HWND hwnd, const char* pszPath, void* psa) noexcept {
    return shell32::SHCreateDirectoryExA(hwnd, pszPath, psa);
}

inline int WINAPI SHCreateDirectoryExW(HWND hwnd, const wchar_t* pszPath, void* psa) noexcept {
    return shell32::SHCreateDirectoryExW(hwnd, pszPath, psa);
}

inline ULONG WINAPI SHChangeNotifyRegister_Ordinal2(
    HWND hwnd, int fSources, int32_t fEvents, uint32_t wMsg, int cItems, const void* pItems
) noexcept {
    return shell32::SHChangeNotifyRegister_Ordinal2(hwnd, fSources, fEvents, wMsg, cItems, pItems);
}

inline BOOL WINAPI SHChangeNotifyDeregister_Ordinal4(ULONG ulID) noexcept {
    return shell32::SHChangeNotifyDeregister_Ordinal4(ulID);
}

// 9. shlwapi.dll
inline int wnsprintfW(wchar_t* lpOut, int cchLimitIn, const wchar_t* lpFmt, ...) noexcept {
    if (!lpOut || cchLimitIn <= 0 || !lpFmt) return -1;
    va_list args;
    va_start(args, lpFmt);
    int written = std::vswprintf(lpOut, static_cast<size_t>(cchLimitIn), lpFmt, args);
    va_end(args);
    return written;
}

// 10. user32.dll
inline BOOL WINAPI ChangeWindowMessageFilterEx(HWND hwnd, uint32_t message, DWORD action, void* pChangeFilterStruct) noexcept {
    return user32::ChangeWindowMessageFilterEx(hwnd, message, action, pChangeFilterStruct);
}

inline char* WINAPI CharLowerA(char* lpsz) noexcept {
    return user32::CharLowerA(lpsz);
}

inline char* WINAPI CharUpperA(char* lpsz) noexcept {
    return user32::CharUpperA(lpsz);
}

inline HICON WINAPI CreateIconFromResourceEx(
    BYTE* pbIconBits, DWORD cbIconBits, BOOL fIcon, DWORD dwVersion,
    int cxDesired, int cyDesired, uint32_t uFlags
) noexcept {
    return user32::CreateIconFromResourceEx(pbIconBits, cbIconBits, fIcon, dwVersion, cxDesired, cyDesired, uFlags);
}

inline int WINAPI DrawTextExA(HDC hdc, char* lpchText, int cchText, void* lprc, uint32_t format, void* lpdtp) noexcept {
    return user32::DrawTextExA(hdc, lpchText, cchText, lprc, format, lpdtp);
}

inline BOOL WINAPI GetKeyboardLayoutNameA(char* pwszKLID) noexcept {
    return user32::GetKeyboardLayoutNameA(pwszKLID);
}

inline int WINAPI MessageBoxExW(HWND hWnd, const wchar_t* lpText, const wchar_t* lpCaption, uint32_t uType, WORD wLanguageId) noexcept {
    return user32::MessageBoxExW(hWnd, lpText, lpCaption, uType, wLanguageId);
}

inline BOOL WINAPI SetProcessDefaultLayout(DWORD dwDefaultLayout) noexcept {
    return user32::SetProcessDefaultLayout(dwDefaultLayout);
}

inline HWINEVENTHOOK WINAPI SetWinEventHook(
    DWORD eventMin, DWORD eventMax, void* hmodWinEventProc,
    void* pfnWinEventProc, DWORD idProcess, DWORD idThread, DWORD dwFlags
) noexcept {
    return user32::SetWinEventHook(eventMin, eventMax, hmodWinEventProc, pfnWinEventProc, idProcess, idThread, dwFlags);
}

inline BOOL WINAPI UnhookWinEvent(HWINEVENTHOOK hWinEventHook) noexcept {
    return user32::UnhookWinEvent(hWinEventHook);
}

// 11. virtdisk.dll
inline uint32_t WINAPI GetVirtualDiskOperationProgress(void* VirtualDiskHandle, void* Overlapped, void* Progress) noexcept {
    return virtdisk::GetVirtualDiskOperationProgress(VirtualDiskHandle, Overlapped, Progress);
}

// 12. wininet.dll
inline BOOL WINAPI InternetGetConnectedState(DWORD* lpdwFlags, DWORD dwReserved) noexcept {
    return wininet::InternetGetConnectedState(lpdwFlags, dwReserved);
}

// 13. wintrust.dll
inline int32_t WINAPI WinVerifyTrustEx(HWND hwnd, void* pgActionID, void* pWVTData) noexcept {
    return wintrust::WinVerifyTrustEx(hwnd, pgActionID, pWVTData);
}

// ============================================================================
// Master Dynamic Loader Registration Bridge (0 registered satellite exports)
// ============================================================================

inline void InitializeRufusExports() {
    // 0 exports defined in satellite header.
    // All 100 Rufus exports have graduated into canonical Core NT headers!
}

} // namespace micant::satellite::rufus

// ----------------------------------------------------------------------------
// From satellite_system_informer.hpp
// ----------------------------------------------------------------------------
namespace micant::satellite::system_informer {

using BOOL = int32_t;
using BOOLEAN = uint8_t;
using DWORD = uint32_t;
using ULONG = uint32_t;
using USHORT = uint16_t;
using UCHAR = uint8_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using HANDLE = void*;
using HWND = void*;
using HDC = void*;
using HICON = void*;
using HMENU = void*;
using HKEY = void*;
using HDEVINFO = void*;
using HPROPSHEETPAGE = void*;
using LSTATUS = int32_t;
using HRESULT = int32_t;
using LONG = int32_t;
using LONG_PTR = intptr_t;
using ULONG_PTR = uintptr_t;
using LPARAM = intptr_t;
using LCID = uint32_t;

inline constexpr BOOL TRUE_VAL = 1;
inline constexpr BOOL FALSE_VAL = 0;
inline constexpr HRESULT S_OK_VAL = 0;
inline constexpr DWORD ERROR_SUCCESS_VAL = 0;

// ============================================================================
// Canonical Core NT Forwarding Bridges for Test Suite & Internal Callers
// ============================================================================

// 1. aclui.dll
inline HPROPSHEETPAGE WINAPI Aclui_CreateSecurityPage_Ordinal1(void* psi) noexcept {
    return micant::aclui::CreateSecurityPage(psi);
}

inline BOOL WINAPI Aclui_EditSecurity_Ordinal2(HWND hwndOwner, void* psi) noexcept {
    return micant::aclui::EditSecurity(hwndOwner, psi);
}

inline HRESULT WINAPI Aclui_EditSecurityAdvanced_Ordinal3(HWND hwndOwner, void* psi, DWORD dwFlags) noexcept {
    return micant::aclui::EditSecurityAdvanced(hwndOwner, psi, dwFlags);
}

// 2. advapi32.dll
inline DWORD WINAPI GetEffectiveRightsFromAclW(void* pAcl, void* pTrustee, uint32_t* pAccessRights) noexcept {
    return micant::advapi32::GetEffectiveRightsFromAclW(pAcl, pTrustee, pAccessRights);
}

inline DWORD WINAPI GetSecurityInfo(HANDLE handle, uint32_t ObjectType, uint32_t SecurityInfo, void** ppsidOwner, void** ppsidGroup, void** ppDacl, void** ppSacl, void** ppSecurityDescriptor) noexcept {
    return micant::advapi32::GetSecurityInfo(handle, ObjectType, SecurityInfo, ppsidOwner, ppsidGroup, ppDacl, ppSacl, ppSecurityDescriptor);
}

inline DWORD WINAPI LsaEnumerateAccounts(HANDLE PolicyHandle, void* EnumerationContext, void** Buffer, uint32_t PreferredMaximumLength, uint32_t* CountReturned) noexcept {
    return micant::advapi32::LsaEnumerateAccounts(PolicyHandle, EnumerationContext, Buffer, PreferredMaximumLength, CountReturned);
}

inline DWORD WINAPI LsaLookupNames2(HANDLE PolicyHandle, uint32_t Flags, uint32_t Count, void* Names, void** ReferencedDomains, void** Sids) noexcept {
    return micant::advapi32::LsaLookupNames2(PolicyHandle, Flags, Count, Names, ReferencedDomains, Sids);
}

inline DWORD WINAPI LsaFreeMemory(void* Buffer) noexcept {
    return micant::advapi32::LsaFreeMemory(Buffer);
}

// 3. ntdll.dll
inline micant::NtStatus WINAPI NtCreateJobObject(HANDLE* JobHandle, uint32_t DesiredAccess, void* ObjectAttributes) noexcept {
    return micant::ntdll::NtCreateJobObject(JobHandle, DesiredAccess, ObjectAttributes);
}

inline micant::NtStatus WINAPI NtCreateKey(HANDLE* KeyHandle, uint32_t DesiredAccess, void* ObjectAttributes, uint32_t TitleIndex, void* Class, uint32_t CreateOptions, uint32_t* Disposition) noexcept {
    return micant::ntdll::NtCreateKey(KeyHandle, DesiredAccess, ObjectAttributes, TitleIndex, Class, CreateOptions, Disposition);
}

inline micant::NtStatus WINAPI NtOpenSection(HANDLE* SectionHandle, uint32_t DesiredAccess, void* ObjectAttributes) noexcept {
    return micant::ntdll::NtOpenSection(SectionHandle, DesiredAccess, ObjectAttributes);
}

inline micant::NtStatus WINAPI NtQueryVirtualMemory(HANDLE ProcessHandle, void* BaseAddress, uint32_t MemoryInformationClass, void* MemoryInformation, size_t MemoryInformationLength, size_t* ReturnLength) noexcept {
    return micant::ntdll::NtQueryVirtualMemory(ProcessHandle, BaseAddress, MemoryInformationClass, MemoryInformation, MemoryInformationLength, ReturnLength);
}

inline micant::NtStatus WINAPI NtQueryTimerResolution(uint32_t* MaximumTime, uint32_t* MinimumTime, uint32_t* CurrentTime) noexcept {
    return micant::ntdll::NtQueryTimerResolution(MaximumTime, MinimumTime, CurrentTime);
}

inline micant::NtStatus WINAPI NtConnectPort(HANDLE* PortHandle, void* PortName, void* SecurityQos, void* ClientView, void* ServerView, uint32_t* MaxMessageLength, void* ConnectionInformation, uint32_t* ConnectionInformationLength) noexcept {
    return micant::ntdll::NtConnectPort_Export(PortHandle, PortName, SecurityQos, ClientView, ServerView, MaxMessageLength, ConnectionInformation, ConnectionInformationLength);
}

inline micant::NtStatus WINAPI RtlGetVersion(void* lpVersionInformation) noexcept {
    return micant::ntdll::RtlGetVersion(lpVersionInformation);
}

inline micant::NtStatus WINAPI RtlIpv4AddressToStringExW(const void* Address, uint16_t Port, wchar_t* AddressString, uint32_t* AddressStringLength) noexcept {
    return micant::ntdll::RtlIpv4AddressToStringExW(Address, Port, AddressString, AddressStringLength);
}

inline uint32_t WINAPI RtlRandomEx(uint32_t* Seed) noexcept {
    return micant::ntdll::RtlRandomEx(Seed);
}

// 4. user32.dll
inline HANDLE WINAPI OpenWindowStationW(const wchar_t* lpwinsta, BOOL fInherit, DWORD dwDesiredAccess) noexcept {
    return micant::user32::OpenWindowStationW(lpwinsta, fInherit, dwDesiredAccess);
}

inline HANDLE WINAPI GetProcessWindowStation() noexcept {
    return micant::user32::GetProcessWindowStation();
}

inline BOOL WINAPI CloseWindowStation(HANDLE hWinSta) noexcept {
    return micant::user32::CloseWindowStation(hWinSta);
}

inline BOOL WINAPI EnumDesktopsW(HANDLE hwinsta, void* lpEnumFunc, LPARAM lParam) noexcept {
    return user32::EnumDesktopsW(hwinsta, lpEnumFunc, lParam);
}

inline HWND WINAPI GetShellWindow() noexcept {
    return user32::GetShellWindow();
}

inline DWORD WINAPI GetGuiResources(HANDLE hProcess, DWORD uiFlags) noexcept {
    return user32::GetGuiResources(hProcess, uiFlags);
}

// 5. winsta.dll
inline BOOL WINAPI WinStationConnectW(HANDLE hServer, DWORD SessionId, DWORD TargetSessionId, const wchar_t* pPassword, BOOL bWait) noexcept {
    return micant::winsta::WinStationConnectW(hServer, SessionId, TargetSessionId, pPassword, bWait);
}

inline BOOL WINAPI WinStationQueryInformationW(HANDLE hServer, DWORD SessionId, DWORD WinStationInformationClass, void* pWinStationInformation, DWORD WinStationInformationLength, DWORD* pReturnLength) noexcept {
    return micant::winsta::WinStationQueryInformationW(hServer, SessionId, WinStationInformationClass, pWinStationInformation, WinStationInformationLength, pReturnLength);
}

inline BOOL WINAPI WinStationSendMessageW(HANDLE hServer, DWORD SessionId, const wchar_t* pTitle, DWORD TitleLength, const wchar_t* pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait) noexcept {
    return micant::winsta::WinStationSendMessageW(hServer, SessionId, pTitle, TitleLength, pMessage, MessageLength, Style, Timeout, pResponse, bWait);
}

// ============================================================================
// Master Dynamic Loader Registration Bridge (0 registered satellite exports)
// ============================================================================

inline void InitializeSystemInformerExports() noexcept {
    // 0 exports defined in satellite header.
    // All 274 System Informer exports have graduated into canonical Core NT headers!
}

} // namespace micant::satellite::system_informer

// ----------------------------------------------------------------------------
// From satellite_qbittorrent.hpp
// ----------------------------------------------------------------------------
namespace micant::satellite::qbittorrent {

using BOOL = int32_t;
using BOOLEAN = uint8_t;
using DWORD = uint32_t;
using ULONG = uint32_t;
using USHORT = uint16_t;
using UCHAR = uint8_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using HANDLE = void*;
using HWND = void*;
using HDC = void*;
using HICON = void*;
using HMENU = void*;
using HKEY = void*;
using HDEVINFO = void*;
using HPROPSHEETPAGE = void*;
using LSTATUS = int32_t;
using HRESULT = int32_t;
using LONG = int32_t;
using LONG_PTR = intptr_t;
using ULONG_PTR = uintptr_t;
using DWORD_PTR = uintptr_t;
using LCID = uint32_t;
using SOCKET = uintptr_t;
using HCRYPTPROV = uintptr_t;

// ============================================================================
// Forwarding Bridges to Canonical Core MicaNT Subsystems
// ============================================================================

// 1. ws2_32.dll / wsock32.dll
inline int WINAPI Wsock_WSAStartup(WORD wVersionRequired, void* lpWSAData) noexcept {
    return ws2_32::WSAStartup(wVersionRequired, reinterpret_cast<ws2_32::WSADATA*>(lpWSAData));
}

inline SOCKET WINAPI Wsock_socket(int af, int type, int protocol) noexcept {
    return ws2_32::socket(af, type, protocol);
}

inline uint16_t WINAPI Wsock_htons(uint16_t hostshort) noexcept {
    return ws2_32::htons(hostshort);
}

inline uint16_t WINAPI Wsock_ntohs(uint16_t netshort) noexcept {
    return ws2_32::ntohs(netshort);
}

inline uint32_t WINAPI Wsock_htonl(uint32_t hostlong) noexcept {
    return ws2_32::htonl(hostlong);
}

inline uint32_t WINAPI Wsock_ntohl(uint32_t netlong) noexcept {
    return ws2_32::ntohl(netlong);
}

inline int WINAPI Ws2_WSAConnect(SOCKET s, const void* name, int namelen, void* lpCallerData, void* lpCalleeData, void* lpSQOS, void* lpGQOS) noexcept {
    return ws2_32::WSAConnect(s, name, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS);
}

inline int WINAPI Ws2_WSASend(SOCKET s, void* lpBuffers, DWORD dwBufferCount, DWORD* lpNumberOfBytesSent, DWORD dwFlags, void* lpOverlapped, void* lpCompletionRoutine) noexcept {
    return ws2_32::WSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}

inline int WINAPI Ws2_getaddrinfo(const char* nodename, const char* servname, const void* hints, void** res) noexcept {
    return ws2_32::getaddrinfo(nodename, servname, hints, res);
}

inline void WINAPI Ws2_freeaddrinfo(void* ai) noexcept {
    ws2_32::freeaddrinfo(ai);
}

inline int WINAPI Wsock_closesocket(SOCKET s) noexcept {
    return ws2_32::closesocket(s);
}

inline int WINAPI Wsock_WSACleanup() noexcept {
    return ws2_32::WSACleanup();
}

// 2. iphlpapi.dll
inline DWORD WINAPI Iphlp_GetAdaptersAddresses(ULONG Family, ULONG Flags, void* Reserved, void* AdapterAddresses, ULONG* SizePointer) noexcept {
    return iphlpapi::GetAdaptersAddresses(Family, Flags, Reserved, AdapterAddresses, SizePointer);
}

inline DWORD WINAPI Iphlp_ConvertInterfaceNameToLuidW(const wchar_t* InterfaceName, void* InterfaceLuid) noexcept {
    return iphlpapi::ConvertInterfaceNameToLuidW(InterfaceName, InterfaceLuid);
}

inline DWORD WINAPI Iphlp_ConvertInterfaceLuidToIndex(const void* InterfaceLuid, DWORD* InterfaceIndex) noexcept {
    return iphlpapi::ConvertInterfaceLuidToIndex(InterfaceLuid, InterfaceIndex);
}

inline DWORD WINAPI Iphlp_NotifyUnicastIpAddressChange(uint16_t Family, void* Callback, void* CallerContext, uint8_t InitialNotification, void** NotificationHandle) noexcept {
    return iphlpapi::NotifyUnicastIpAddressChange(Family, Callback, CallerContext, InitialNotification, NotificationHandle);
}

inline DWORD WINAPI Iphlp_CancelMibChangeNotify2(void* NotificationHandle) noexcept {
    return iphlpapi::CancelMibChangeNotify2(NotificationHandle);
}

// 3. kernel32.dll
inline HANDLE WINAPI K32_CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort, ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads) noexcept {
    return kernel32::CreateIoCompletionPort(FileHandle, ExistingCompletionPort, CompletionKey, NumberOfConcurrentThreads);
}

inline BOOL WINAPI K32_PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred, ULONG_PTR dwCompletionKey, void* lpOverlapped) noexcept {
    return kernel32::PostQueuedCompletionStatus(CompletionPort, dwNumberOfBytesTransferred, dwCompletionKey, lpOverlapped);
}

inline BOOL WINAPI K32_GetQueuedCompletionStatus(HANDLE CompletionPort, DWORD* lpNumberOfBytesTransferred, ULONG_PTR* lpCompletionKey, void** lpOverlapped, DWORD dwMilliseconds) noexcept {
    return kernel32::GetQueuedCompletionStatus(CompletionPort, lpNumberOfBytesTransferred, lpCompletionKey, lpOverlapped, dwMilliseconds);
}

inline BOOL WINAPI K32_LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, void* lpOverlapped) noexcept {
    return kernel32::LockFileEx(hFile, dwFlags, dwReserved, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, lpOverlapped);
}

inline BOOL WINAPI K32_UnlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh, void* lpOverlapped) noexcept {
    return kernel32::UnlockFileEx(hFile, dwReserved, nNumberOfBytesToUnlockLow, nNumberOfBytesToUnlockHigh, lpOverlapped);
}

inline BOOL WINAPI K32_FlushViewOfFile(const void* lpBaseAddress, size_t dwNumberOfBytesToFlush) noexcept {
    return kernel32::FlushViewOfFile(lpBaseAddress, dwNumberOfBytesToFlush);
}

// 4. icuuc.dll
inline void* Wsock_ucnv_open(const char* name, int32_t* err) noexcept {
    return icuuc::ucnv_open(name, err);
}

inline const char* Wsock_ucnv_getName(const void* converter, int32_t* err) noexcept {
    return icuuc::ucnv_getName(converter, err);
}

inline int8_t Wsock_ucnv_getMaxCharSize(const void* converter) noexcept {
    return icuuc::ucnv_getMaxCharSize(converter);
}

inline void Wsock_ucnv_close(void* converter) noexcept {
    icuuc::ucnv_close(converter);
}

// 5. user32.dll
inline BOOL WINAPI User32_SetProcessDpiAwarenessContext(void* value) noexcept {
    return user32::SetProcessDpiAwarenessContext(value);
}

inline BOOL WINAPI User32_UpdateLayeredWindow(HWND hWnd, HDC hdcDst, void* pptDst, void* psize, HDC hdcSrc, void* pptSrc, uint32_t crKey, void* pblend, DWORD dwFlags) noexcept {
    return user32::UpdateLayeredWindow(hWnd, hdcDst, pptDst, psize, hdcSrc, pptSrc, crKey, pblend, dwFlags);
}

inline BOOL WINAPI User32_RegisterTouchWindow(HWND hWnd, ULONG ulFlags) noexcept {
    return user32::RegisterTouchWindow(hWnd, ulFlags);
}

inline void* WINAPI User32_RegisterPowerSettingNotification(HANDLE hRecipient, const void* PowerSettingGuid, DWORD Flags) noexcept {
    return user32::RegisterPowerSettingNotification(hRecipient, PowerSettingGuid, Flags);
}

inline BOOL WINAPI User32_UnregisterPowerSettingNotification(void* Handle) noexcept {
    return user32::UnregisterPowerSettingNotification(Handle);
}

// ============================================================================
// Master Dynamic Loader Registration Bridge (0 registered satellite exports)
// ============================================================================

inline void InitializeQBittorrentExports() noexcept {
    // 0 exports defined in satellite header.
    // All 200 qBittorrent exports have graduated into canonical Core NT headers!
}

} // namespace micant::satellite::qbittorrent

// ----------------------------------------------------------------------------
// From satellite_winscp.hpp
// ----------------------------------------------------------------------------
namespace micant::satellite::winscp {

using BOOL = int32_t;
using BOOLEAN = uint8_t;
using DWORD = uint32_t;
using ULONG = uint32_t;
using USHORT = uint16_t;
using UCHAR = uint8_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using HANDLE = void*;
using HWND = void*;
using HDC = void*;
using HICON = void*;
using HMENU = void*;
using HKEY = void*;
using HDEVINFO = void*;
using HPROPSHEETPAGE = void*;
using LSTATUS = int32_t;
using HRESULT = int32_t;
using LONG = int32_t;
using LONG_PTR = intptr_t;
using ULONG_PTR = uintptr_t;
using DWORD_PTR = uintptr_t;
using LCID = uint32_t;
using SOCKET = uintptr_t;

inline constexpr BOOL TRUE_VAL = 1;
inline constexpr BOOL FALSE_VAL = 0;
inline constexpr HRESULT S_OK_VAL = 0;
inline constexpr DWORD ERROR_SUCCESS_VAL = 0;

#ifndef WINAPI
#define WINAPI __stdcall
#endif

// ============================================================================
// Compatibility Type Forwarders
// ============================================================================

using MicaServEnt = ws2_32::MicaServEnt;

// ============================================================================
// Canonical Function Forwarders for Unit Tests
// ============================================================================

// 1. ws2_32.dll
inline HANDLE WINAPI Ws2_WSAAsyncGetHostByName(HWND hWnd, unsigned int wMsg, const char* name, char* buf, int buflen) noexcept {
    return ws2_32::WSAAsyncGetHostByName(hWnd, wMsg, name, buf, buflen);
}

inline int WINAPI Ws2_WSACancelAsyncRequest(HANDLE hAsyncTaskHandle) noexcept {
    return ws2_32::WSACancelAsyncRequest(hAsyncTaskHandle);
}

inline int WINAPI Ws2_WSAEnumNetworkEvents(SOCKET s, HANDLE hEventObject, void* lpNetworkEvents) noexcept {
    return ws2_32::WSAEnumNetworkEvents(s, hEventObject, lpNetworkEvents);
}

inline int WINAPI Ws2_WSAEventSelect(SOCKET s, HANDLE hEventObject, long lNetworkEvents) noexcept {
    return ws2_32::WSAEventSelect(s, hEventObject, lNetworkEvents);
}

inline void* WINAPI Ws2_getservbyname(const char* name, const char* proto) noexcept {
    return ws2_32::getservbyname(name, proto);
}

inline int WINAPI Ws2_getsockopt(SOCKET s, int level, int optname, char* optval, int* optlen) noexcept {
    return ws2_32::getsockopt(s, level, optname, optval, optlen);
}

inline int WINAPI Ws2_ioctlsocket(SOCKET s, long cmd, ULONG* argp) noexcept {
    return ws2_32::ioctlsocket(s, cmd, argp);
}

inline int WINAPI Ws2_select(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout) noexcept {
    return ws2_32::select(nfds, readfds, writefds, exceptfds, timeout);
}

inline const char* WINAPI Ws2_inet_ntop(int af, const void* src, char* dst, size_t size) noexcept {
    return ws2_32::inet_ntop(af, src, dst, size);
}

inline int WINAPI Ws2_inet_pton(int af, const char* src, void* dst) noexcept {
    return ws2_32::inet_pton(af, src, dst);
}

// 2. kernel32.dll
inline HANDLE WINAPI K32_CreateJobObjectW(void* lpJobAttributes, const wchar_t* lpName) noexcept {
    return kernel32::CreateJobObjectW(lpJobAttributes, lpName);
}

inline HANDLE WINAPI K32_OpenJobObjectW(DWORD dwDesiredAccess, BOOL bInheritHandle, const wchar_t* lpName) noexcept {
    return kernel32::OpenJobObjectW(dwDesiredAccess, bInheritHandle, lpName);
}

inline BOOL WINAPI K32_AssignProcessToJobObject(HANDLE hJob, HANDLE hProcess) noexcept {
    return kernel32::AssignProcessToJobObject(hJob, hProcess);
}

inline BOOL WINAPI K32_SetInformationJobObject(HANDLE hJob, int JobObjectInformationClass, void* lpJobObjectInformation, DWORD cbJobObjectInformationLength) noexcept {
    return kernel32::SetInformationJobObject(hJob, JobObjectInformationClass, lpJobObjectInformation, cbJobObjectInformationLength);
}

inline HANDLE WINAPI K32_OpenFileMappingA(DWORD dwDesiredAccess, BOOL bInheritHandle, const char* lpName) noexcept {
    return kernel32::OpenFileMappingA(dwDesiredAccess, bInheritHandle, lpName);
}

inline BOOL WINAPI K32_VirtualLock(void* lpAddress, size_t dwSize) noexcept {
    return kernel32::VirtualLock(lpAddress, dwSize);
}

inline BOOL WINAPI K32_FlushInstructionCache(HANDLE hProcess, const void* lpBaseAddress, size_t dwSize) noexcept {
    return kernel32::FlushInstructionCache(hProcess, lpBaseAddress, dwSize);
}

inline BOOL WINAPI K32_FlushConsoleInputBuffer(HANDLE hConsoleInput) noexcept {
    return kernel32::FlushConsoleInputBuffer(hConsoleInput);
}

inline BOOL WINAPI K32_PeekConsoleInputW(HANDLE hConsoleInput, void* lpBuffer, DWORD nLength, DWORD* lpNumberOfEventsRead) noexcept {
    return kernel32::PeekConsoleInputW(hConsoleInput, lpBuffer, nLength, lpNumberOfEventsRead);
}

inline BOOL WINAPI K32_ReadConsoleInputW(HANDLE hConsoleInput, void* lpBuffer, DWORD nLength, DWORD* lpNumberOfEventsRead) noexcept {
    return kernel32::ReadConsoleInputW(hConsoleInput, lpBuffer, nLength, lpNumberOfEventsRead);
}

inline BOOL WINAPI K32_WriteConsoleInputW(HANDLE hConsoleInput, const void* lpBuffer, DWORD nLength, DWORD* lpNumberOfEventsWritten) noexcept {
    return kernel32::WriteConsoleInputW(hConsoleInput, lpBuffer, nLength, lpNumberOfEventsWritten);
}

inline LONG WINAPI K32_InterlockedIncrement(volatile LONG* Addend) noexcept {
    return kernel32::InterlockedIncrement(Addend);
}

inline LONG WINAPI K32_InterlockedDecrement(volatile LONG* Addend) noexcept {
    return kernel32::InterlockedDecrement(Addend);
}

inline LONG WINAPI K32_InterlockedExchange(volatile LONG* Target, LONG Value) noexcept {
    return kernel32::InterlockedExchange(Target, Value);
}

inline LONG WINAPI K32_InterlockedExchangeAdd(volatile LONG* Addend, LONG Value) noexcept {
    return kernel32::InterlockedExchangeAdd(Addend, Value);
}

inline LONG WINAPI K32_InterlockedCompareExchange(volatile LONG* Destination, LONG Exchange, LONG Comperand) noexcept {
    return kernel32::InterlockedCompareExchange(Destination, Exchange, Comperand);
}

inline int WINAPI K32_GetDateFormatA(LCID Locale, DWORD dwFlags, const void* lpDate, const char* lpFormat, char* lpDateStr, int cchDate) noexcept {
    return kernel32::GetDateFormatA(Locale, dwFlags, lpDate, lpFormat, lpDateStr, cchDate);
}

inline BOOL WINAPI K32_GetModuleHandleExA(DWORD dwFlags, const char* lpModuleName, void** phModule) noexcept {
    return kernel32::GetModuleHandleExA(dwFlags, lpModuleName, reinterpret_cast<kernel32::HMODULE*>(phModule));
}

inline BOOL WINAPI K32_IsBadWritePtr(void* lp, size_t ucb) noexcept {
    return kernel32::IsBadWritePtr(lp, ucb);
}

// 3. comdlg32.dll & crypt32.dll
inline HWND WINAPI Comdlg_ReplaceTextW(void* lpfr) noexcept {
    return comdlg32::ReplaceTextW(lpfr);
}

inline BOOL WINAPI Crypt32_CertCreateCertificateChainEngine(void* pConfig, void** phChainEngine) noexcept {
    return crypt32::CertCreateCertificateChainEngine(pConfig, phChainEngine);
}

inline void WINAPI Crypt32_CertFreeCertificateChainEngine(void* hChainEngine) noexcept {
    crypt32::CertFreeCertificateChainEngine(hChainEngine);
}

inline BOOL WINAPI Crypt32_CertVerifyCertificateChainPolicy(const char* pszPolicyOID, void* pChainContext, void* pPolicyPara, void* pPolicyStatus) noexcept {
    return crypt32::CertVerifyCertificateChainPolicy(const_cast<char*>(pszPolicyOID), pChainContext, pPolicyPara, pPolicyStatus);
}

// 4. gdi32.dll, iphlpapi.dll, msi.dll, secur32.dll, shell32.dll & shlwapi.dll
inline BOOL WINAPI Gdi_PolyPolyline(HDC hdc, const void* apt, const DWORD* asz, DWORD csz) noexcept {
    return gdi32::PolyPolyline(hdc, apt, asz, csz);
}

inline uint32_t WINAPI Iphlp_if_nametoindex(const char* InterfaceName) noexcept {
    return iphlpapi::if_nametoindex(InterfaceName);
}

inline char* WINAPI Iphlp_if_indextoname(ULONG InterfaceIndex, char* InterfaceName) noexcept {
    return iphlpapi::if_indextoname(InterfaceIndex, InterfaceName);
}

inline uint32_t WINAPI Msi_Ordinal70() noexcept { return ERROR_SUCCESS_VAL; }
inline uint32_t WINAPI Msi_Ordinal205() noexcept { return ERROR_SUCCESS_VAL; }

inline BOOLEAN WINAPI Secur32_GetUserNameExW(int NameFormat, wchar_t* lpNameBuffer, ULONG* nSize) noexcept {
    return sspi::GetUserNameExW(NameFormat, lpNameBuffer, nSize);
}

inline uintptr_t WINAPI Shell_FindExecutableW(const wchar_t* lpFile, const wchar_t* lpDirectory, wchar_t* lpResult) noexcept {
    return reinterpret_cast<uintptr_t>(shell32::FindExecutableW(lpFile, lpDirectory, lpResult));
}

inline void WINAPI Shell_SHFreeNameMappings(void* hNameMapping) noexcept {
    shell32::SHFreeNameMappings(hNameMapping);
}

inline BOOL WINAPI Shell_Ordinal644() noexcept { return TRUE_VAL; }
inline BOOL WINAPI Shell_Ordinal645() noexcept { return TRUE_VAL; }

inline const wchar_t* WINAPI Shlwapi_PathSkipRootW(const wchar_t* pszPath) noexcept {
    return shell32::PathSkipRootW(pszPath);
}

inline HRESULT WINAPI Shlwapi_SHCreateStreamOnFileW(const wchar_t* pszFile, DWORD grfMode, void** ppstm) noexcept {
    return shell32::SHCreateStreamOnFileW(pszFile, grfMode, ppstm);
}

// 5. user32.dll
inline BOOL WINAPI User32_DrawCaption(HWND hwnd, HDC hdc, const void* lprect, uint32_t flags) noexcept {
    return user32::DrawCaption(hwnd, hdc, lprect, flags);
}

inline DWORD WINAPI User32_GetClassLongW(HWND hWnd, int nIndex) noexcept {
    return user32::GetClassLongW(hWnd, nIndex);
}

inline DWORD WINAPI User32_SetClassLongW(HWND hWnd, int nIndex, LONG dwNewLong) noexcept {
    return user32::SetClassLongW(hWnd, nIndex, dwNewLong);
}

inline BOOL WINAPI User32_SendNotifyMessageW(HWND hWnd, uint32_t Msg, uintptr_t wParam, intptr_t lParam) noexcept {
    return user32::SendNotifyMessageW(hWnd, Msg, wParam, lParam);
}

// ============================================================================
// Master Dynamic Loader Registration Bridge (0 registered satellite exports)
// ============================================================================

inline void InitializeWinScpExports() noexcept {
    // All 45 WinSCP exports graduated into canonical Core NT headers!
}

} // namespace micant::satellite::winscp

// ----------------------------------------------------------------------------
// From satellite_wireshark.hpp
// ----------------------------------------------------------------------------
namespace micant::satellite::wireshark {

using BOOL = win32::BOOL;
using DWORD = win32::DWORD;
using HMODULE = win32::HMODULE;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPWSTR = wchar_t*;
using SIZE_T = size_t;
using ULONG64 = uint64_t;

// 1. KERNEL32 Forwarders
inline BOOL WINAPI K32_DisableThreadLibraryCalls(HMODULE h) noexcept { return win32::DisableThreadLibraryCalls(h); }
inline BOOL WINAPI K32_SetProcessDEPPolicy(DWORD d) noexcept { return win32::SetProcessDEPPolicy(d); }
inline BOOL WINAPI K32_SetDllDirectoryA(LPCSTR s) noexcept { return win32::SetDllDirectoryA(s); }

// 2. IPHLPAPI Forwarders
using NET_LUID = iphlpapi::NET_LUID;
using PNET_LUID = iphlpapi::PNET_LUID;
using NETIO_STATUS = iphlpapi::NETIO_STATUS;

inline NETIO_STATUS WINAPI Iphlp_ConvertInterfaceGuidToLuid(const GUID* g, PNET_LUID l) noexcept {
    return iphlpapi::ConvertInterfaceGuidToLuid(g, l);
}
inline NETIO_STATUS WINAPI Iphlp_ConvertInterfaceLuidToAlias(const NET_LUID* l, LPWSTR a, SIZE_T s) noexcept {
    return iphlpapi::ConvertInterfaceLuidToAlias(l, a, s);
}

// 3. Winsock Forwarders
inline uint16_t WINAPI WS2_ntohs(uint16_t s) noexcept { return ws2_32::ntohs(s); }

// 4. ADVAPI32 Forwarders
inline BOOL WINAPI Advapi_CreateWellKnownSid(DWORD t, void* c, void* s, DWORD* cb) noexcept {
    return advapi32::CreateWellKnownSid(t, c, s, cb);
}

// 5. Universal C Runtime Forwarders
using timespec64 = msvcrt::timespec64;

inline double CRT_sin(double x) noexcept { return msvcrt::CRT_sin(x); }
inline double CRT_cos(double x) noexcept { return msvcrt::CRT_cos(x); }
inline double CRT_tan(double x) noexcept { return msvcrt::CRT_tan(x); }
inline double CRT_asin(double x) noexcept { return msvcrt::CRT_asin(x); }
inline double CRT_acos(double x) noexcept { return msvcrt::CRT_acos(x); }
inline double CRT_atan(double x) noexcept { return msvcrt::CRT_atan(x); }
inline double CRT_atan2(double y, double x) noexcept { return msvcrt::CRT_atan2(y, x); }
inline double CRT_exp(double x) noexcept { return msvcrt::CRT_exp(x); }
inline double CRT_log(double x) noexcept { return msvcrt::CRT_log(x); }
inline double CRT_log10(double x) noexcept { return msvcrt::CRT_log10(x); }
inline double CRT_log2(double x) noexcept { return msvcrt::CRT_log2(x); }
inline double CRT_pow(double x, double y) noexcept { return msvcrt::CRT_pow(x, y); }
inline double CRT_sqrt(double x) noexcept { return msvcrt::CRT_sqrt(x); }
inline double CRT_cbrt(double x) noexcept { return msvcrt::CRT_cbrt(x); }
inline double CRT_ceil(double x) noexcept { return msvcrt::CRT_ceil(x); }
inline double CRT_floor(double x) noexcept { return msvcrt::CRT_floor(x); }
inline double CRT_round(double x) noexcept { return msvcrt::CRT_round(x); }
inline long CRT_lround(double x) noexcept { return msvcrt::CRT_lround(x); }
inline double CRT_fmod(double x, double y) noexcept { return msvcrt::CRT_fmod(x, y); }
inline double CRT_modf(double x, double* iptr) noexcept { return msvcrt::CRT_modf(x, iptr); }
inline double CRT_ldexp(double x, int exp) noexcept { return msvcrt::CRT_ldexp(x, exp); }
inline double CRT_copysign(double x, double y) noexcept { return msvcrt::CRT_copysign(x, y); }
inline int CRT_dclass(double x) noexcept { return msvcrt::CRT_dclass(x); }
inline int CRT_dpcomp(double x, double y) noexcept { return msvcrt::CRT_dpcomp(x, y); }

inline int CRT_tolower(int c) noexcept { return msvcrt::CRT_tolower(c); }
inline int CRT_toupper(int c) noexcept { return msvcrt::CRT_toupper(c); }
inline int CRT_isdigit(int c) noexcept { return msvcrt::CRT_isdigit(c); }
inline int CRT_islower(int c) noexcept { return msvcrt::CRT_islower(c); }
inline int CRT_isupper(int c) noexcept { return msvcrt::CRT_isupper(c); }
inline int CRT_isxdigit(int c) noexcept { return msvcrt::CRT_isxdigit(c); }
inline int CRT_iswspace(wint_t wc) noexcept { return msvcrt::CRT_iswspace(wc); }

inline size_t CRT_strspn(const char* s, const char* accept) noexcept { return msvcrt::CRT_strspn(s, accept); }
inline size_t CRT_strcspn(const char* s, const char* reject) noexcept { return msvcrt::CRT_strcspn(s, reject); }
inline char* CRT_strpbrk(const char* s, const char* accept) noexcept { return msvcrt::CRT_strpbrk(s, accept); }
inline char* CRT_strtok(char* str, const char* delim) noexcept { return msvcrt::CRT_strtok(str, delim); }
inline size_t CRT_strnlen(const char* s, size_t maxlen) noexcept { return msvcrt::CRT_strnlen(s, maxlen); }
inline char* CRT_strncat(char* dest, const char* src, size_t n) noexcept { return msvcrt::CRT_strncat(dest, src, n); }
inline int CRT_mblen(const char* s, size_t n) noexcept { return msvcrt::CRT_mblen(s, n); }
inline int CRT_mbtowc(wchar_t* pwc, const char* s, size_t n) noexcept { return msvcrt::CRT_mbtowc(pwc, s, n); }
inline int CRT_wcscat_s(wchar_t* dest, size_t destSz, const wchar_t* src) noexcept { return msvcrt::CRT_wcscat_s(dest, destSz, src); }
inline long CRT_strtol(const char* nptr, char** endptr, int base) noexcept { return msvcrt::CRT_strtol(nptr, endptr, base); }
inline int CRT_atoi(const char* nptr) noexcept { return msvcrt::CRT_atoi(nptr); }

inline void* CRT_bsearch(const void* k, const void* b, size_t num, size_t sz, int (*cmp)(const void*, const void*)) noexcept {
    return msvcrt::CRT_bsearch(k, b, num, sz, cmp);
}
inline void CRT_qsort(void* b, size_t num, size_t sz, int (*cmp)(const void*, const void*)) noexcept {
    msvcrt::CRT_qsort(b, num, sz, cmp);
}

inline int CRT_localtime64_s(struct tm* tmDest, const time_t* sourceTime) noexcept { return msvcrt::CRT_localtime64_s(tmDest, sourceTime); }
inline int CRT_gmtime64_s(struct tm* tmDest, const time_t* sourceTime) noexcept { return msvcrt::CRT_gmtime64_s(tmDest, sourceTime); }
inline int CRT_timespec64_get(timespec64* ts, int base) noexcept { return msvcrt::CRT_timespec64_get(ts, base); }

// 6. VCRuntime 140 Forwarders
inline void* VCRT_memcpy(void* dest, const void* src, size_t count) noexcept { return msvcrt::VCRT_memcpy(dest, src, count); }
inline void* VCRT_memset(void* dest, int c, size_t count) noexcept { return msvcrt::VCRT_memset(dest, c, count); }
inline void* VCRT_memmove(void* dest, const void* src, size_t count) noexcept { return msvcrt::VCRT_memmove(dest, src, count); }
inline int VCRT_memcmp(const void* buf1, const void* buf2, size_t count) noexcept { return msvcrt::VCRT_memcmp(buf1, buf2, count); }
inline void* VCRT_memchr(const void* buf, int c, size_t count) noexcept { return msvcrt::VCRT_memchr(buf, c, count); }
inline char* VCRT_strchr(const char* str, int c) noexcept { return msvcrt::VCRT_strchr(str, c); }
inline char* VCRT_strrchr(const char* str, int c) noexcept { return msvcrt::VCRT_strrchr(str, c); }
inline char* VCRT_strstr(const char* str, const char* strSearch) noexcept { return msvcrt::VCRT_strstr(str, strSearch); }

// 7. MSVCP140 Forwarders
inline void MSVC_Mtx_lock(void* mtx) noexcept { msvcrt::MSVC_Mtx_lock(mtx); }
inline int MSVC_Mtx_trylock(void* mtx) noexcept { return msvcrt::MSVC_Mtx_trylock(mtx); }
inline void MSVC_Mtx_unlock(void* mtx) noexcept { msvcrt::MSVC_Mtx_unlock(mtx); }
inline void MSVC_Cnd_wait(void* cnd, void* mtx) noexcept { msvcrt::MSVC_Cnd_wait(cnd, mtx); }
inline void MSVC_Cnd_broadcast(void* cnd) noexcept { msvcrt::MSVC_Cnd_broadcast(cnd); }
inline unsigned int MSVC_Random_device() noexcept { return msvcrt::MSVC_Random_device(); }
inline const wchar_t* MSVC_W_Getdays() noexcept { return msvcrt::MSVC_W_Getdays(); }
inline const wchar_t* MSVC_W_Getmonths() noexcept { return msvcrt::MSVC_W_Getmonths(); }

// 8. Initialization Forwarder
inline void InitializeWiresharkExports() noexcept {
    // Delegated to canonical core initializers
    msvcrt::InitializeMsvcrtSubsystemExports();
    win32::InitializeWin32SubsystemExports();
    advapi32::InitializeAdvapi32SubsystemExports();
    iphlpapi::InitializeIpHlpApiSubsystemExports();
    ws2_32::InitializeWs2_32SubsystemExports();
}

} // namespace micant::satellite::wireshark

// ----------------------------------------------------------------------------
// From satellite_filezilla.hpp
// ----------------------------------------------------------------------------
namespace micant::satellite::filezilla {

using BOOL = win32::BOOL;
using DWORD = win32::DWORD;
using ULONG = uint32_t;
using HMODULE = win32::HMODULE;
using HDC = void*;
using HRGN = void*;
using HBRUSH = void*;
using HCURSOR = void*;
using HWND = win32::HWND;
using HANDLE = win32::HANDLE;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPWSTR = wchar_t*;
using SIZE_T = size_t;
using ULONG64 = uint64_t;
using UINT = unsigned int;
using LONG = long;
using SHORT = short;
using LPARAM = intptr_t;
using WPARAM = uintptr_t;

// 1. KERNEL32 Forwarders
using SYSTEM_POWER_STATUS = win32::SYSTEM_POWER_STATUS;

inline BOOL WINAPI K32_GetSystemPowerStatus(SYSTEM_POWER_STATUS* p) noexcept { return win32::GetSystemPowerStatus(p); }
inline DWORD WINAPI K32_GetActiveProcessorCount(uint16_t g) noexcept { return win32::GetActiveProcessorCount(g); }
inline BOOL WINAPI K32_GetHandleInformation(HANDLE h, DWORD* f) noexcept { return win32::GetHandleInformation(h, f); }
inline DWORD WINAPI K32_GetProfileStringW(LPCWSTR a, LPCWSTR k, LPCWSTR d, LPWSTR r, DWORD s) noexcept {
    return win32::GetProfileStringW(a, k, d, r, s);
}
inline int WINAPI K32_IdnToAscii(DWORD f, LPCWSTR u, int cu, LPWSTR a, int ca) noexcept {
    return win32::IdnToAscii(f, u, cu, a, ca);
}
inline BOOL WINAPI K32_IsBadStringPtrA(LPCSTR l, size_t m) noexcept { return win32::IsBadStringPtrA(l, m); }
inline BOOL WINAPI K32_SetProcessPriorityBoost(HANDLE h, BOOL d) noexcept { return win32::SetProcessPriorityBoost(h, d); }
inline BOOL WINAPI K32_SetThreadContext(HANDLE h, const void* c) noexcept { return win32::SetThreadContext(h, c); }
inline HANDLE WINAPI K32_FindFirstVolumeW(LPWSTR v, DWORD l) noexcept { return win32::FindFirstVolumeW(v, l); }
inline BOOL WINAPI K32_FindNextVolumeW(HANDLE h, LPWSTR v, DWORD l) noexcept { return win32::FindNextVolumeW(h, v, l); }
inline BOOL WINAPI K32_FindVolumeClose(HANDLE h) noexcept { return win32::FindVolumeClose(h); }
inline DWORD WINAPI K32_GetFinalPathNameByHandleA(HANDLE h, char* p, DWORD c, DWORD f) noexcept {
    return win32::GetFinalPathNameByHandleA(h, p, c, f);
}
inline BOOL WINAPI K32_FillConsoleOutputCharacterW(HANDLE h, wchar_t c, DWORD l, void* w, DWORD* o) noexcept {
    return win32::FillConsoleOutputCharacterW(h, c, l, w, o);
}
inline BOOL WINAPI K32_ReadConsoleOutputCharacterA(HANDLE h, char* c, DWORD l, void* r, DWORD* o) noexcept {
    return win32::ReadConsoleOutputCharacterA(h, c, l, r, o);
}

// 2. USER32 Forwarders
inline BOOL WINAPI U32_AnimateWindow(HWND w, DWORD t, DWORD f) noexcept { return user32::AnimateWindow(w, t, f); }
inline LONG WINAPI U32_ChangeDisplaySettingsExW(LPCWSTR d, void* m, HWND w, DWORD f, void* l) noexcept {
    return user32::ChangeDisplaySettingsExW(d, m, w, f, l);
}
inline BOOL WINAPI U32_EnumDisplaySettingsW(LPCWSTR d, DWORD m, void* dm) noexcept { return user32::EnumDisplaySettingsW(d, m, dm); }
inline BOOL WINAPI U32_DrawStateW(HDC h, HBRUSH b, void* c, LPARAM l, WPARAM w, int x, int y, int cx, int cy, UINT f) noexcept {
    return user32::DrawStateW(h, b, c, l, w, x, y, cx, cy, f);
}
inline BOOL WINAPI U32_GetProcessDefaultLayout(DWORD* l) noexcept { return user32::GetProcessDefaultLayout(l); }
inline HCURSOR WINAPI U32_LoadCursorFromFileW(LPCWSTR f) noexcept { return reinterpret_cast<HCURSOR>(user32::LoadCursorFromFileW(f)); }
inline BOOL WINAPI U32_SetCaretBlinkTime(UINT m) noexcept { return user32::SetCaretBlinkTime(m); }
inline BOOL WINAPI U32_ValidateRgn(HWND w, HRGN r) noexcept { return user32::ValidateRgn(w, r); }
inline SHORT WINAPI U32_VkKeyScanW(wchar_t c) noexcept { return user32::VkKeyScanW(c); }
inline DWORD WINAPI U32_WaitForInputIdle(HANDLE p, DWORD m) noexcept { return user32::WaitForInputIdle(p, m); }
inline void WINAPI U32_keybd_event(uint8_t v, uint8_t s, DWORD f, uintptr_t e) noexcept { user32::keybd_event(v, s, f, e); }
inline void* WINAPI U32_DdeCreateDataHandle(DWORD i, uint8_t* p, DWORD cb, DWORD cbo, void* item, UINT fmt, UINT cmd) noexcept {
    return user32::DdeCreateDataHandle(i, p, cb, cbo, item, fmt, cmd);
}
inline DWORD WINAPI U32_DdeGetData(void* h, uint8_t* d, DWORD max, DWORD off) noexcept { return user32::DdeGetData(h, d, max, off); }
inline UINT WINAPI U32_DdeGetLastError(DWORD i) noexcept { return user32::DdeGetLastError(i); }
inline void* WINAPI U32_DdeNameService(DWORD i, void* s1, void* s2, UINT cmd) noexcept { return user32::DdeNameService(i, s1, s2, cmd); }
inline BOOL WINAPI U32_DdePostAdvise(DWORD i, void* top, void* item) noexcept { return user32::DdePostAdvise(i, top, item); }
inline DWORD WINAPI U32_DdeQueryStringW(DWORD i, void* h, LPWSTR p, DWORD max, int cp) noexcept {
    return user32::DdeQueryStringW(i, h, p, max, cp);
}

// 3. GDI32 Forwarders
inline HRGN WINAPI GDI_CreatePolygonRgn(const void* pt, int cp, int m) noexcept { return gdi32::CreatePolygonRgn(pt, cp, m); }
inline BOOL WINAPI GDI_PolyPolygon(HDC h, const void* pt, const int* sz, int c) noexcept { return gdi32::PolyPolygon(h, pt, sz, c); }
inline BOOL WINAPI GDI_EqualRgn(HRGN r1, HRGN r2) noexcept { return gdi32::EqualRgn(r1, r2); }
inline BOOL WINAPI GDI_PtInRegion(HRGN r, int x, int y) noexcept { return gdi32::PtInRegion(r, x, y); }
inline BOOL WINAPI GDI_RectInRegion(HRGN r, const void* rc) noexcept { return gdi32::RectInRegion(r, rc); }
inline void* WINAPI GDI_GetEnhMetaFileW(LPCWSTR n) noexcept { return gdi32::GetEnhMetaFileW(n); }
inline void* WINAPI GDI_SetMetaFileBitsEx(UINT cb, const uint8_t* d) noexcept { return gdi32::SetMetaFileBitsEx(cb, d); }
inline int WINAPI GDI_GetGraphicsMode(HDC h) noexcept { return gdi32::GetGraphicsMode(h); }
inline BOOL WINAPI GDI_GetWorldTransform(HDC h, void* x) noexcept { return gdi32::GetWorldTransform(h, x); }
inline BOOL WINAPI GDI_ModifyWorldTransform(HDC h, const void* x, DWORD m) noexcept { return gdi32::ModifyWorldTransform(h, x, m); }

// 4. ADVAPI32 Forwarders
inline BOOL WINAPI ADV_AllocateLocallyUniqueId(void* l) noexcept { return advapi32::AllocateLocallyUniqueId(l); }
inline BOOL WINAPI ADV_CredReadW(LPCWSTR t, DWORD tp, DWORD f, void** c) noexcept { return advapi32::CredReadW(t, tp, f, c); }
inline void WINAPI ADV_CredFree(void* b) noexcept { advapi32::CredFree(b); }
inline BOOL WINAPI ADV_CredWriteW(void* c, DWORD f) noexcept { return advapi32::CredWriteW(c, f); }
inline BOOL WINAPI ADV_CredDeleteW(LPCWSTR t, DWORD tp, DWORD f) noexcept { return advapi32::CredDeleteW(t, tp, f); }
inline BOOL WINAPI ADV_DuplicateTokenEx(HANDLE h, DWORD a, void* attr, int lvl, int tp, HANDLE* n) noexcept {
    return advapi32::DuplicateTokenEx(h, a, attr, lvl, tp, n);
}
inline BOOL WINAPI ADV_ImpersonateLoggedOnUser(HANDLE h) noexcept { return advapi32::ImpersonateLoggedOnUser(h); }
inline BOOL WINAPI ADV_RevertToSelf() noexcept { return advapi32::RevertToSelf(); }
inline BOOL WINAPI ADV_LogonUserExW(LPCWSTR u, LPCWSTR d, LPCWSTR p, DWORD lt, DWORD lp, HANDLE* t, void** s, void** pr, DWORD* pl, void* q) noexcept {
    return advapi32::LogonUserExW(u, d, p, lt, lp, t, s, pr, pl, q);
}
inline DWORD WINAPI ADV_SetEntriesInAclW(ULONG c, void* p, void* o, void** n) noexcept { return advapi32::SetEntriesInAclW(c, p, o, n); }
inline BOOL WINAPI ADV_SetSecurityDescriptorSacl(void* sd, BOOL sp, void* s, BOOL df) noexcept {
    return advapi32::SetSecurityDescriptorSacl(sd, sp, s, df);
}
inline BOOL WINAPI ADV_SetTokenInformation(HANDLE t, int c, void* i, DWORD l) noexcept { return advapi32::SetTokenInformation(t, c, i, l); }
inline BOOL WINAPI ADV_CryptSetProvParam(uintptr_t h, DWORD p, const uint8_t* d, DWORD f) noexcept { return advapi32::CryptSetProvParam(h, p, d, f); }
inline BOOL WINAPI ADV_CryptSignHashA(uintptr_t h, DWORD k, LPCSTR d, DWORD f, uint8_t* s, DWORD* l) noexcept {
    return advapi32::CryptSignHashA(h, k, d, f, s, l);
}

// 5. CRYPT32 Forwarders
inline BOOL WINAPI CRYPT_CertDeleteCertificateFromStore(void* c) noexcept { return crypt32::CertDeleteCertificateFromStore(c); }
inline void* WINAPI CRYPT_CertEnumCRLsInStore(void* s, void* p) noexcept { return crypt32::CertEnumCRLsInStore(s, p); }
inline BOOL WINAPI CRYPT_CryptProtectMemory(void* d, DWORD cb, DWORD f) noexcept { return crypt32::CryptProtectMemory(d, cb, f); }

// 6. NCRYPT Forwarders
inline NTSTATUS WINAPI NC_NCryptDecrypt(uintptr_t k, const uint8_t* in, DWORD cbi, void* p, uint8_t* out, DWORD cbo, DWORD* res, DWORD f) noexcept {
    return crypto::NCryptDecrypt(k, in, cbi, p, out, cbo, res, f);
}
inline NTSTATUS WINAPI NC_NCryptGetProperty(uintptr_t o, LPCWSTR p, uint8_t* out, DWORD cb, DWORD* res, DWORD f) noexcept {
    return crypto::NCryptGetProperty(o, p, out, cb, res, f);
}
inline NTSTATUS WINAPI NC_NCryptSignHash(uintptr_t k, void* p, const uint8_t* h, DWORD cbh, uint8_t* sig, DWORD cbs, DWORD* res, DWORD f) noexcept {
    return crypto::NCryptSignHash(k, p, h, cbh, sig, cbs, res, f);
}
inline NTSTATUS WINAPI NC_BCryptOpenAlgorithmProvider(crypto::BCRYPT_ALG_HANDLE* a, const wchar_t* id, const wchar_t* impl, uint32_t f) noexcept {
    return crypto::BCryptOpenAlgorithmProvider(a, id, impl, f);
}
inline NTSTATUS WINAPI NC_BCryptCloseAlgorithmProvider(crypto::BCRYPT_ALG_HANDLE a, uint32_t f) noexcept {
    return crypto::BCryptCloseAlgorithmProvider(a, f);
}
inline NTSTATUS WINAPI NC_BCryptGenRandom(crypto::BCRYPT_ALG_HANDLE a, uint8_t* b, uint32_t c, uint32_t f) noexcept {
    return crypto::BCryptGenRandom(a, b, c, f);
}

// 7. SHELL32 Forwarders
inline int32_t WINAPI SHL_SHDefExtractIconW(LPCWSTR f, int i, UINT flg, void** l, void** s, UINT sz) noexcept {
    return shell32::SHDefExtractIconW(f, i, flg, l, s, sz);
}
inline int WINAPI SHL_SHGetIconOverlayIndexW(LPCWSTR p, int i) noexcept { return shell32::SHGetIconOverlayIndexW(p, i); }

// 8. UXTHEME Forwarders
inline int32_t WINAPI UXT_GetThemeBackgroundExtent(void* th, HDC h, int p, int s, const void* cr, void* er) noexcept {
    return uxtheme::GetThemeBackgroundExtent(th, h, p, s, cr, er);
}
inline DWORD WINAPI UXT_GetThemeSysColor(void* th, int c) noexcept { return uxtheme::GetThemeSysColor(th, c); }
inline int32_t WINAPI UXT_GetThemeSysFont(void* th, int f, void* lf) noexcept { return uxtheme::GetThemeSysFont(th, f, lf); }
inline BOOL WINAPI UXT_IsThemePartDefined(void* th, int p, int s) noexcept { return uxtheme::IsThemePartDefined(th, p, s); }

// 9. WS2_32 Forwarders
inline win32::HANDLE WINAPI WS2_WSACreateEvent() noexcept { return ws2_32::WSACreateEvent(); }
inline BOOL WINAPI WS2_WSACloseEvent(win32::HANDLE h) noexcept { return ws2_32::WSACloseEvent(h); }
inline BOOL WINAPI WS2_WSASetEvent(win32::HANDLE h) noexcept { return ws2_32::WSASetEvent(h); }
inline DWORD WINAPI WS2_WSAWaitForMultipleEvents(DWORD c, const win32::HANDLE* e, BOOL a, DWORD t, BOOL al) noexcept {
    return ws2_32::WSAWaitForMultipleEvents(c, e, a, t, al);
}

// 10. MSVCRT Forwarders
inline double CRT_cosh(double x) noexcept { return msvcrt::CRT_cosh(x); }
inline double CRT_sinh(double x) noexcept { return msvcrt::CRT_sinh(x); }
inline double CRT_tanh(double x) noexcept { return msvcrt::CRT_tanh(x); }
inline double CRT_atof(const char* s) noexcept { return msvcrt::CRT_atof(s); }
inline long   CRT_atol(const char* s) noexcept { return msvcrt::CRT_atol(s); }
inline double CRT_difftime(time_t t1, time_t t0) noexcept { return msvcrt::CRT_difftime(t1, t0); }
inline int    CRT_raise(int sig) noexcept { return msvcrt::CRT_raise(sig); }
inline wchar_t* CRT_wcsdup(const wchar_t* s) noexcept { return msvcrt::CRT_wcsdup(s); }
inline wchar_t* CRT_wcsncpy(wchar_t* d, const wchar_t* s, size_t n) noexcept { return msvcrt::CRT_wcsncpy(d, s, n); }
inline wchar_t* CRT_wcscat(wchar_t* d, const wchar_t* s) noexcept { return msvcrt::CRT_wcscat(d, s); }
inline wchar_t* CRT_wcschr(const wchar_t* s, wchar_t c) noexcept { return msvcrt::CRT_wcschr(s, c); }
inline wchar_t* CRT_wcspbrk(const wchar_t* s, const wchar_t* c) noexcept { return msvcrt::CRT_wcspbrk(s, c); }
inline size_t   CRT_wcsspn(const wchar_t* s, const wchar_t* c) noexcept { return msvcrt::CRT_wcsspn(s, c); }
inline size_t   CRT_wcstombs(char* mb, const wchar_t* wc, size_t max) noexcept { return msvcrt::CRT_wcstombs(mb, wc, max); }
inline unsigned long CRT_wcstoul(const wchar_t* s, wchar_t** e, int b) noexcept { return msvcrt::CRT_wcstoul(s, e, b); }
inline int      CRT_wtoi(const wchar_t* s) noexcept { return msvcrt::CRT_wtoi(s); }
inline int      CRT_wcsnicmp(const wchar_t* s1, const wchar_t* s2, size_t c) noexcept { return msvcrt::CRT_wcsnicmp(s1, s2, c); }
inline void*    CRT_aligned_malloc(size_t s, size_t a) noexcept { return msvcrt::CRT_aligned_malloc(s, a); }
inline void     CRT_aligned_free(void* p) noexcept { msvcrt::CRT_aligned_free(p); }
inline int      CRT_waccess(const wchar_t* p, int m) noexcept { return msvcrt::CRT_waccess(p, m); }
inline int      CRT_wchdir(const wchar_t* d) noexcept { return msvcrt::CRT_wchdir(d); }
inline int      CRT_wchmod(const wchar_t* f, int p) noexcept { return msvcrt::CRT_wchmod(f, p); }
inline wchar_t* CRT_wfullpath(wchar_t* r, const wchar_t* p, size_t max) noexcept { return msvcrt::CRT_wfullpath(r, p, max); }
inline wchar_t* CRT_wgetcwd(wchar_t* b, int max) noexcept { return msvcrt::CRT_wgetcwd(b, max); }
inline wchar_t* CRT_wgetenv(const wchar_t* v) noexcept { return msvcrt::CRT_wgetenv(v); }
inline int      CRT_wputenv(const wchar_t* e) noexcept { return msvcrt::CRT_wputenv(e); }
inline int      CRT_wrename(const wchar_t* o, const wchar_t* n) noexcept { return msvcrt::CRT_wrename(o, n); }
inline int      CRT_wutime64(const wchar_t* f, void* t) noexcept { return msvcrt::CRT_wutime64(f, t); }
inline intptr_t CRT_wfindfirst64(const wchar_t* f, void* d) noexcept { return msvcrt::CRT_wfindfirst64(f, d); }
inline int      CRT_wfindnext64(intptr_t h, void* d) noexcept { return msvcrt::CRT_wfindnext64(h, d); }
inline int      CRT_findclose(intptr_t h) noexcept { return msvcrt::CRT_findclose(h); }
inline char*    CRT_getcwd(char* b, int max) noexcept { return msvcrt::CRT_getcwd(b, max); }
inline int      CRT_getdrive() noexcept { return msvcrt::CRT_getdrive(); }
inline int      CRT_chdrive(int d) noexcept { return msvcrt::CRT_chdrive(d); }
inline int      CRT_mkdir(const char* d) noexcept { return msvcrt::CRT_mkdir(d); }
inline int      CRT_commit(int f) noexcept { return msvcrt::CRT_commit(f); }
inline int      CRT_dup2(int f1, int f2) noexcept { return msvcrt::CRT_dup2(f1, f2); }
inline int64_t  CRT_filelengthi64(int f) noexcept { return msvcrt::CRT_filelengthi64(f); }
inline int64_t  CRT_telli64(int f) noexcept { return msvcrt::CRT_telli64(f); }
inline uintptr_t CRT_beginthread(void (*s)(void*), unsigned int st, void* a) noexcept { return msvcrt::CRT_beginthread(s, st, a); }
inline void     CRT_endthreadex(unsigned int r) noexcept { msvcrt::CRT_endthreadex(r); }
inline wint_t   CRT_fgetwc(std::FILE* s) noexcept { return msvcrt::CRT_fgetwc(s); }
inline int      CRT_fputws(const wchar_t* str, std::FILE* s) noexcept { return msvcrt::CRT_fputws(str, s); }
inline wint_t   CRT_getwc(std::FILE* s) noexcept { return msvcrt::CRT_getwc(s); }
inline wint_t   CRT_putwc(wchar_t c, std::FILE* s) noexcept { return msvcrt::CRT_putwc(c, s); }
inline wint_t   CRT_ungetwc(wint_t c, std::FILE* s) noexcept { return msvcrt::CRT_ungetwc(c, s); }
inline int      CRT_putws(const wchar_t* str) noexcept { return msvcrt::CRT_putws(str); }
inline int      CRT_fseek(std::FILE* s, long o, int o2) noexcept { return msvcrt::CRT_fseek(s, o, o2); }
inline long     CRT_ftell(std::FILE* s) noexcept { return msvcrt::CRT_ftell(s); }
inline int      CRT_fgetpos(std::FILE* s, fpos_t* p) noexcept { return msvcrt::CRT_fgetpos(s, p); }
inline int      CRT_fsetpos(std::FILE* s, const fpos_t* p) noexcept { return msvcrt::CRT_fsetpos(s, p); }
inline int      CRT_remove(const char* f) noexcept { return msvcrt::CRT_remove(f); }
inline int      CRT_setmaxstdio(int m) noexcept { return msvcrt::CRT_setmaxstdio(m); }
inline int      CRT_getmaxstdio() noexcept { return msvcrt::CRT_getmaxstdio(); }
inline void     CRT_wperror(const wchar_t* s) noexcept { msvcrt::CRT_wperror(s); }
inline int      CRT_isalnum(int c) noexcept { return msvcrt::CRT_isalnum(c); }
inline int      CRT_isalpha(int c) noexcept { return msvcrt::CRT_isalpha(c); }
inline int      CRT_isspace(int c) noexcept { return msvcrt::CRT_isspace(c); }
inline int      CRT_iswalnum(wint_t c) noexcept { return msvcrt::CRT_iswalnum(c); }
inline int      CRT_iswalpha(wint_t c) noexcept { return msvcrt::CRT_iswalpha(c); }
inline int      CRT_iswdigit(wint_t c) noexcept { return msvcrt::CRT_iswdigit(c); }
inline int      CRT_iswprint(wint_t c) noexcept { return msvcrt::CRT_iswprint(c); }
inline int      CRT_iswpunct(wint_t c) noexcept { return msvcrt::CRT_iswpunct(c); }
inline int      CRT_iswxdigit(wint_t c) noexcept { return msvcrt::CRT_iswxdigit(c); }
inline void     CRT_assert(const char* m, const char* f, unsigned l) noexcept { msvcrt::CRT_assert(m, f, l); }
inline int      CRT_setjmp(void* b) noexcept { return msvcrt::CRT_setjmp(b); }
inline int64_t  CRT_strtoi64(const char* n, char** e, int b) noexcept { return msvcrt::CRT_strtoi64(n, e, b); }
inline uint64_t CRT_strtoui64(const char* n, char** e, int b) noexcept { return msvcrt::CRT_strtoui64(n, e, b); }
inline char*    CRT_ctime64(const int64_t* t) noexcept { return msvcrt::CRT_ctime64(t); }

inline long& CRT_timezone_val = msvcrt::CRT_timezone_val;

// 11. Initialization Forwarder
inline void InitializeFileZillaExports() noexcept {
    // Delegated to canonical core initializers
    win32::InitializeWin32SubsystemExports();
    user32::InitializeUser32SubsystemExports();
    gdi32::InitializeGdi32SubsystemExports();
    advapi32::InitializeAdvapi32SubsystemExports();
    crypt32::InitializeCrypt32SubsystemExports();
    crypto::InitializeBCryptSubsystemExports();
    shell32::InitializeShell32SubsystemExports();
    uxtheme::InitializeUxThemeSubsystemExports();
    ws2_32::InitializeWs2_32SubsystemExports();
    msvcrt::InitializeMsvcrtSubsystemExports();
}

} // namespace micant::satellite::filezilla

// ----------------------------------------------------------------------------
// From satellite_win32.hpp
// ----------------------------------------------------------------------------
namespace micant::satellite {

using LPCWSTR = const wchar_t*;

// Canonical Core Forwarding Aliases
using sensapi::NETWORK_ALIVE_LAN;
using sensapi::NETWORK_ALIVE_WAN;
using sensapi::IsNetworkAlive;
using sensapi::IsDestinationReachableW;

using gdi32::AlphaBlend;
using gdi32::BLENDFUNCTION;

using comdlg32::PrintDlgW;
using comdlg32::ChooseColorW;
using comdlg32::GetOpenFileNameW;
using comdlg32::GetSaveFileNameW;
using comdlg32::CommDlgExtendedError;

using comctl32::CreateToolbarEx;
using comctl32::PropertySheetW;

using user32::MapDialogRect;
using user32::GetMonitorInfoA;
using user32::GetDialogBaseUnits;
using user32::CheckRadioButton;
using user32::IsDlgButtonChecked;
using user32::CheckDlgButton;
using user32::LoadAcceleratorsW;
using user32::GetClassInfoW;

using advapi32::LsaAddAccountRights;
using advapi32::GetUserNameW;
using advapi32::RegDeleteKeyExW;

using shell32::ExtractIconExW;
using shell32::SHGetDesktopFolder;
using shell32::SHGetSpecialFolderLocation;
using shell32::SHChangeNotify;
using shell32::SHGetPathFromIDListW;
using shell32::SHBrowseForFolderW;

using win32::GetWindowsDirectoryW;
using win32::GetDriveTypeW;
using win32::GetVolumeInformationW;
using win32::SetPriorityClass;
using win32::GetSystemDefaultLangID;
using win32::GetUserDefaultLangID;
using win32::GetCompressedFileSizeW;
using win32::FindFirstChangeNotificationW;
using win32::FindNextChangeNotification;
using win32::FindCloseChangeNotification;

inline void Mica_PureCall() noexcept {}
inline void Mica_Srand(unsigned int seed) noexcept { std::srand(seed); }

// ----------------------------------------------------------------------------
// Subsystem Initialization Orchestrator
// ----------------------------------------------------------------------------

inline void InitializeSatelliteWin32Exports() {
    auto& ldr = ldr::DynamicLoader::get();

    // Canonical Core Subsystem Initializations
    sensapi::InitializeSensApiSubsystemExports();
    comdlg32::InitializeComDlg32SubsystemExports();
    comctl32::InitializeComCtl32SubsystemExports();
    gdi32::InitializeGdi32SubsystemExports();
    user32::InitializeUser32SubsystemExports();
    advapi32::InitializeAdvapi32SubsystemExports();
    shell32::InitializeShell32SubsystemExports();
    win32::InitializeWin32SubsystemExports();
    msvcrt::InitializeMsvcrtSubsystemExports();
    msi::InitializeMsiSubsystemExports();
    sspi::InitializeSspiSubsystemExports();
    crypt32::InitializeCrypt32SubsystemExports();
    ws2_32::InitializeWs2_32SubsystemExports();
    ole32::InitializeOle32SubsystemExports();
    oleaut32::InitializeOleAut32SubsystemExports();
    propsys::InitializePropSysSubsystemExports();
    uxtheme::InitializeUxThemeSubsystemExports();
    urlmon::InitializeUrlMonSubsystemExports();
    wininet::InitializeWinINetSubsystemExports();
    winspool::InitializePrintSpoolerSubsystemExports();
    uiautomation::InitializeUIAutomationSubsystemExports();
    tsf::InitializeTextServicesExports();
    mpr::InitializeMprSubsystemExports();
    setupapi::InitializeSetupApiSubsystemExports();
    iphlpapi::InitializeIpHlpApiSubsystemExports();
    crypto::InitializeBCryptSubsystemExports();
    prism3d12::InitializePrism3D12SubsystemExports();
    icuuc::InitializeIcuucSubsystemExports();
    authz::InitializeAuthzSubsystemExports();
    userenv::InitializeUserenvSubsystemExports();
    winhttp::InitializeWinHttpSubsystemExports();
    imm32::InitializeImm32SubsystemExports();
    powrprof::InitializePowrProfSubsystemExports();
    dbgeng::InitializeDbgEngSubsystemExports();
    winrt::InitializeWinRTSubsystemExports();

    // WizTree 4.x Subsystem Extensions
    wiztree::InitializeWizTreeWin32Exports();

    // PuTTY 0.82+ Subsystem Extensions
    putty::registerPuTTYExports(ldr);

    // GDI+ 2D Vector & Imaging Subsystem
    micant::gdiplus::InitializeGdiPlusExports();

    // SumatraPDF 3.6+ Subsystem Extensions
    sumatra::InitializeSumatraWin32Exports();

    // Everything 1.4+ Search Subsystem Extensions
    everything::InitializeEverythingExports();

    // WinMerge 2.16+ Visual Diff & Merge Subsystem Extensions
    winmerge::InitializeWinMergeExports();

    // Rufus 4.x Storage & Low-Level Hardware Subsystem Extensions
    rufus::InitializeRufusExports();

    // System Informer 4.0 Native NT & Diagnostics Subsystem Extensions
    system_informer::InitializeSystemInformerExports();

    // qBittorrent 5.2+ High-Throughput I/O & Networking Subsystem Extensions
    qbittorrent::InitializeQBittorrentExports();

    // WinSCP 6.5+ Secure Remote File Management Subsystem Extensions
    winscp::InitializeWinScpExports();

    // Wireshark 4.x / Universal C Runtime & MSVC STL Subsystem Extensions
    wireshark::InitializeWiresharkExports();

    // FileZilla 3.x / Sovereign Networking & Enterprise FTP Subsystem Extensions
    filezilla::InitializeFileZillaExports();

    // MPR 1.0 Network Provider Router Subsystem
    mpr::InitializeMprSubsystemExports();

    // Storage, Setup, Trust, and Native NT Subsystems
    virtdisk::InitializeVirtualDiskSubsystemExports();
    wintrust::InitializeWinTrustSubsystemExports();
    setupapi::InitializeSetupApiSubsystemExports();
    ntdll::InitializeNtdllSubsystemExports();
    aclui::InitializeAcluiSubsystemExports();
    winsta::InitializeWinStaSubsystemExports();
}

} // namespace micant::satellite

