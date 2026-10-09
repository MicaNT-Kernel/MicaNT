// ============================================================================
// MicaNT: Win32 Satellite Subsystems (SensApi.dll, MSIMG32.dll, COMDLG32.dll, etc.)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides Network Sensing (SensApi), Alpha Blending (MSIMG32),
// Common Dialogs (COMDLG32), Shell Controls (COMCTL32/SHELL32),
// and Desktop System Extensions.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <cwchar>
#include "ntdef.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "ldr.hpp"
#include "mpr.hpp"
#include "satellite_wiztree.hpp"
#include "satellite_putty.hpp"
#include "satellite_gdiplus.hpp"
#include "satellite_sumatra.hpp"
#include "satellite_everything.hpp"
#include "satellite_winmerge.hpp"
#include "satellite_rufus.hpp"
#include "satellite_system_informer.hpp"

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

inline win32::BOOL GetOpenFileNameW(void* /*lpofn*/) noexcept {
    return win32::FALSE; // User cancelled open file dialog
}

inline win32::BOOL GetSaveFileNameW(void* /*lpofn*/) noexcept {
    return win32::FALSE; // User cancelled save file dialog
}

inline uint32_t CommDlgExtendedError() noexcept {
    return 0; // No error
}

// ----------------------------------------------------------------------------
// 4. COMCTL32.dll Extensions
// ----------------------------------------------------------------------------

inline win32::HWND CreateToolbarEx(
    win32::HWND /*hwnd*/, uint32_t /*ws*/, uint32_t /*wID*/, int /*nBitmaps*/,
    void* /*hBMInst*/, uintptr_t /*wBMID*/, void* /*lpButtons*/, int /*iNumButtons*/,
    int /*dxButton*/, int /*dyButton*/, int /*dxBitmap*/, int /*dyBitmap*/, uint32_t /*uStructSize*/
) noexcept {
    return reinterpret_cast<win32::HWND>(0x8001);
}

inline intptr_t PropertySheetW(void* /*lppsph*/) noexcept {
    return 0; // IDOK
}

// ----------------------------------------------------------------------------
// 5. USER32.dll Extended Dialog & Monitor Extensions
// ----------------------------------------------------------------------------

inline win32::BOOL MapDialogRect(win32::HWND /*hDlg*/, void* /*lpRect*/) noexcept {
    return win32::TRUE;
}

inline win32::BOOL GetMonitorInfoA(void* /*hMonitor*/, void* lpmi) noexcept {
    if (lpmi) {
        struct MONITORINFOA { uint32_t cbSize; int32_t rcMonitor[4]; int32_t rcWork[4]; uint32_t dwFlags; };
        auto* mi = reinterpret_cast<MONITORINFOA*>(lpmi);
        mi->rcMonitor[0] = 0; mi->rcMonitor[1] = 0; mi->rcMonitor[2] = 1280; mi->rcMonitor[3] = 800;
        mi->rcWork[0] = 0; mi->rcWork[1] = 0; mi->rcWork[2] = 1280; mi->rcWork[3] = 760;
        mi->dwFlags = 1; // MONITORINFOF_PRIMARY
    }
    return win32::TRUE;
}

inline int32_t GetDialogBaseUnits() noexcept {
    return (16 << 16) | 8;
}

inline win32::BOOL CheckRadioButton(win32::HWND /*hDlg*/, int /*nIDFirstButton*/, int /*nIDLastButton*/, int /*nIDCheckButton*/) noexcept {
    return win32::TRUE;
}

inline uint32_t IsDlgButtonChecked(win32::HWND /*hDlg*/, int /*nIDButton*/) noexcept {
    return 0; // BST_UNCHECKED
}

inline win32::BOOL CheckDlgButton(win32::HWND /*hDlg*/, int /*nIDButton*/, uint32_t /*uCheck*/) noexcept {
    return win32::TRUE;
}

inline void* LoadAcceleratorsW(void* /*hInstance*/, const wchar_t* /*lpTableName*/) noexcept {
    return reinterpret_cast<void*>(0x5501);
}

inline win32::BOOL GetClassInfoW(void* hInstance, const wchar_t* lpClassName, void* lpWndClass) noexcept {
    if (!lpClassName || !lpWndClass) return win32::FALSE;
    user32::WNDCLASSEXW wcx{};
    if (user32::WindowManager::get().getClassInfo(lpClassName, &wcx)) {
        struct WNDCLASS_BASIC {
            uint32_t style; user32::WNDPROC lpfnWndProc; int cbClsExtra; int cbWndExtra;
            void* hInstance; void* hIcon; void* hCursor; void* hbrBackground;
            const wchar_t* lpszMenuName; const wchar_t* lpszClassName;
        };
        auto* wc = reinterpret_cast<WNDCLASS_BASIC*>(lpWndClass);
        wc->style = wcx.style;
        wc->lpfnWndProc = wcx.lpfnWndProc;
        wc->cbClsExtra = wcx.cbClsExtra;
        wc->cbWndExtra = wcx.cbWndExtra;
        wc->hInstance = hInstance;
        wc->hIcon = wcx.hIcon;
        wc->hCursor = wcx.hCursor;
        wc->hbrBackground = wcx.hbrBackground;
        wc->lpszMenuName = wcx.lpszMenuName;
        wc->lpszClassName = wcx.lpszClassName;
        return win32::TRUE;
    }
    return win32::FALSE;
}

// ----------------------------------------------------------------------------
// 6. ADVAPI32.dll Security & Registry Extensions
// ----------------------------------------------------------------------------

inline int32_t LsaAddAccountRights(void* /*PolicyHandle*/, void* /*AccountSid*/, void* /*UserRights*/, uint32_t /*CountOfRights*/) noexcept {
    return 0; // STATUS_SUCCESS
}

inline win32::BOOL GetUserNameW(wchar_t* lpBuffer, uint32_t* pcbBuffer) noexcept {
    if (!lpBuffer || !pcbBuffer) return win32::FALSE;
    const wchar_t user[] = L"admin";
    size_t len = wcslen(user);
    if (*pcbBuffer <= len) {
        *pcbBuffer = static_cast<uint32_t>(len + 1);
        return win32::FALSE;
    }
    wcscpy_s(lpBuffer, *pcbBuffer, user);
    *pcbBuffer = static_cast<uint32_t>(len);
    return win32::TRUE;
}

inline int32_t RegDeleteKeyExW(void* /*hKey*/, const wchar_t* /*lpSubKey*/, uint32_t /*samDesired*/, uint32_t /*Reserved*/) noexcept {
    return 0; // ERROR_SUCCESS
}

// ----------------------------------------------------------------------------
// 7. SHELL32.dll Namespace & Notification Extensions
// ----------------------------------------------------------------------------

inline uint32_t ExtractIconExW(const wchar_t* /*lpszFile*/, int /*nIconIndex*/, void** /*phiconLarge*/, void** /*phiconSmall*/, uint32_t /*nIcons*/) noexcept {
    return 0;
}

inline int32_t SHGetDesktopFolder(void** ppshf) noexcept {
    if (ppshf) *ppshf = reinterpret_cast<void*>(0x7001);
    return 0; // S_OK
}

inline int32_t SHGetSpecialFolderLocation(void* /*hwndOwner*/, int /*nFolder*/, void** ppidl) noexcept {
    if (ppidl) *ppidl = reinterpret_cast<void*>(0x7002);
    return 0; // S_OK
}

inline void SHChangeNotify(int32_t /*wEventId*/, uint32_t /*uFlags*/, const void* /*dwItem1*/, const void* /*dwItem2*/) noexcept {}

inline win32::BOOL SHGetPathFromIDListW(const void* /*pidl*/, wchar_t* pszPath) noexcept {
    if (pszPath) wcscpy_s(pszPath, 260, L"C:\\MicaNT");
    return win32::TRUE;
}

inline void* SHBrowseForFolderW(void* /*lpbi*/) noexcept {
    return nullptr; // User cancelled
}

// ----------------------------------------------------------------------------
// 8. KERNEL32.dll Directory, Drive & Notification Extensions
// ----------------------------------------------------------------------------

inline uint32_t GetWindowsDirectoryW(wchar_t* lpBuffer, uint32_t uSize) noexcept {
    const wchar_t winDir[] = L"C:\\Windows";
    size_t len = wcslen(winDir);
    if (!lpBuffer || uSize <= len) return static_cast<uint32_t>(len + 1);
    wcscpy_s(lpBuffer, uSize, winDir);
    return static_cast<uint32_t>(len);
}

inline uint32_t GetDriveTypeW(const wchar_t* /*lpRootPathName*/) noexcept {
    return 3; // DRIVE_FIXED
}

inline win32::BOOL GetVolumeInformationW(
    const wchar_t* /*lpRootPathName*/,
    wchar_t* lpVolumeNameBuffer,
    uint32_t nVolumeNameSize,
    uint32_t* lpVolumeSerialNumber,
    uint32_t* lpMaximumComponentLength,
    uint32_t* lpFileSystemFlags,
    wchar_t* lpFileSystemNameBuffer,
    uint32_t nFileSystemNameSize
) noexcept {
    if (lpVolumeNameBuffer && nVolumeNameSize > 6) wcscpy_s(lpVolumeNameBuffer, nVolumeNameSize, L"MicaNT");
    if (lpVolumeSerialNumber) *lpVolumeSerialNumber = 0x19851120;
    if (lpMaximumComponentLength) *lpMaximumComponentLength = 255;
    if (lpFileSystemFlags) *lpFileSystemFlags = 0x00000003; // CASE_SENSITIVE | CASE_PRESERVED
    if (lpFileSystemNameBuffer && nFileSystemNameSize > 4) wcscpy_s(lpFileSystemNameBuffer, nFileSystemNameSize, L"NTFS");
    return win32::TRUE;
}

inline win32::BOOL SetPriorityClass(win32::HANDLE /*hProcess*/, uint32_t /*dwPriorityClass*/) noexcept {
    return win32::TRUE;
}

inline uint16_t GetSystemDefaultLangID() noexcept {
    return 0x0409; // en-US
}

inline uint16_t GetUserDefaultLangID() noexcept {
    return 0x0409; // en-US
}

inline uint32_t GetCompressedFileSizeW(const wchar_t* /*lpFileName*/, uint32_t* lpFileSizeHigh) noexcept {
    if (lpFileSizeHigh) *lpFileSizeHigh = 0;
    return 1024 * 1024;
}

inline win32::HANDLE FindFirstChangeNotificationW(const wchar_t* /*lpPathName*/, win32::BOOL /*bWatchSubtree*/, uint32_t /*dwNotifyFilter*/) noexcept {
    return reinterpret_cast<win32::HANDLE>(0x6001);
}

inline win32::BOOL FindNextChangeNotification(win32::HANDLE /*hChangeHandle*/) noexcept {
    return win32::TRUE;
}

inline win32::BOOL FindCloseChangeNotification(win32::HANDLE /*hChangeHandle*/) noexcept {
    return win32::TRUE;
}

// ----------------------------------------------------------------------------
// 9. MSVCRT.dll Pure Call & PRNG Extensions
// ----------------------------------------------------------------------------

extern "C" inline void Mica_PureCall() noexcept {}
extern "C" inline void Mica_Srand(unsigned int seed) noexcept { std::srand(seed); }

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
    ldr.registerExport("COMDLG32.dll", "GetOpenFileNameW", reinterpret_cast<void*>(GetOpenFileNameW));
    ldr.registerExport("COMDLG32.dll", "GetSaveFileNameW", reinterpret_cast<void*>(GetSaveFileNameW));
    ldr.registerExport("COMDLG32.dll", "CommDlgExtendedError", reinterpret_cast<void*>(CommDlgExtendedError));

    // COMCTL32.dll
    ldr.registerExport("COMCTL32.dll", "CreateToolbarEx", reinterpret_cast<void*>(CreateToolbarEx));
    ldr.registerExport("COMCTL32.dll", "PropertySheetW", reinterpret_cast<void*>(PropertySheetW));

    // USER32.dll
    ldr.registerExport("USER32.dll", "MapDialogRect", reinterpret_cast<void*>(MapDialogRect));
    ldr.registerExport("USER32.dll", "GetMonitorInfoA", reinterpret_cast<void*>(GetMonitorInfoA));
    ldr.registerExport("USER32.dll", "GetDialogBaseUnits", reinterpret_cast<void*>(GetDialogBaseUnits));
    ldr.registerExport("USER32.dll", "CheckRadioButton", reinterpret_cast<void*>(CheckRadioButton));
    ldr.registerExport("USER32.dll", "IsDlgButtonChecked", reinterpret_cast<void*>(IsDlgButtonChecked));
    ldr.registerExport("USER32.dll", "CheckDlgButton", reinterpret_cast<void*>(CheckDlgButton));
    ldr.registerExport("USER32.dll", "LoadAcceleratorsW", reinterpret_cast<void*>(LoadAcceleratorsW));
    ldr.registerExport("USER32.dll", "GetClassInfoW", reinterpret_cast<void*>(GetClassInfoW));

    // ADVAPI32.dll
    ldr.registerExport("ADVAPI32.dll", "LsaAddAccountRights", reinterpret_cast<void*>(LsaAddAccountRights));
    ldr.registerExport("ADVAPI32.dll", "GetUserNameW", reinterpret_cast<void*>(GetUserNameW));
    ldr.registerExport("ADVAPI32.dll", "RegDeleteKeyExW", reinterpret_cast<void*>(RegDeleteKeyExW));

    // SHELL32.dll
    ldr.registerExport("SHELL32.dll", "ExtractIconExW", reinterpret_cast<void*>(ExtractIconExW));
    ldr.registerExport("SHELL32.dll", "SHGetDesktopFolder", reinterpret_cast<void*>(SHGetDesktopFolder));
    ldr.registerExport("SHELL32.dll", "SHGetSpecialFolderLocation", reinterpret_cast<void*>(SHGetSpecialFolderLocation));
    ldr.registerExport("SHELL32.dll", "SHChangeNotify", reinterpret_cast<void*>(SHChangeNotify));
    ldr.registerExport("SHELL32.dll", "SHGetPathFromIDListW", reinterpret_cast<void*>(SHGetPathFromIDListW));
    ldr.registerExport("SHELL32.dll", "SHBrowseForFolderW", reinterpret_cast<void*>(SHBrowseForFolderW));

    // KERNEL32.dll
    ldr.registerExport("KERNEL32.dll", "GetWindowsDirectoryW", reinterpret_cast<void*>(GetWindowsDirectoryW));
    ldr.registerExport("KERNEL32.dll", "GetDriveTypeW", reinterpret_cast<void*>(GetDriveTypeW));
    ldr.registerExport("KERNEL32.dll", "GetVolumeInformationW", reinterpret_cast<void*>(GetVolumeInformationW));
    ldr.registerExport("KERNEL32.dll", "SetPriorityClass", reinterpret_cast<void*>(SetPriorityClass));
    ldr.registerExport("KERNEL32.dll", "GetSystemDefaultLangID", reinterpret_cast<void*>(GetSystemDefaultLangID));
    ldr.registerExport("KERNEL32.dll", "GetUserDefaultLangID", reinterpret_cast<void*>(GetUserDefaultLangID));
    ldr.registerExport("KERNEL32.dll", "GetCompressedFileSizeW", reinterpret_cast<void*>(GetCompressedFileSizeW));
    ldr.registerExport("KERNEL32.dll", "FindFirstChangeNotificationW", reinterpret_cast<void*>(FindFirstChangeNotificationW));
    ldr.registerExport("KERNEL32.dll", "FindNextChangeNotification", reinterpret_cast<void*>(FindNextChangeNotification));
    ldr.registerExport("KERNEL32.dll", "FindCloseChangeNotification", reinterpret_cast<void*>(FindCloseChangeNotification));

    // MSVCRT.dll
    ldr.registerExport("msvcrt.dll", "_purecall", reinterpret_cast<void*>(Mica_PureCall));
    ldr.registerExport("msvcrt.dll", "srand", reinterpret_cast<void*>(Mica_Srand));

    // WizTree 4.x Subsystem Extensions
    wiztree::InitializeWizTreeWin32Exports();

    // PuTTY 0.82+ Subsystem Extensions
    putty::registerPuTTYExports(ldr);

    // GDI+ 2D Vector & Imaging Subsystem
    gdiplus::InitializeGdiPlusSatelliteExports();

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

    // MPR 1.0 Network Provider Router Subsystem
    mpr::InitializeMprSubsystemExports();
}

} // namespace micant::satellite
