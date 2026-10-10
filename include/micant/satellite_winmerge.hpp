// ============================================================================
// MicaNT: WinMerge 2.16+ Win32 Satellite Subsystem Extensions (satellite_winmerge.hpp)
//
// Modern Clean-Room Forwarding Bridge in pure ISO C++23.
// All 121 exports have graduated directly to Canonical Core MicaNT Subsystems:
//   - advapi32.dll -> include/micant/advapi32.hpp
//   - comctl32.dll -> include/micant/comctl32.hpp
//   - gdi32.dll    -> include/micant/gdi32.hpp
//   - gdiplus.dll  -> include/micant/gdiplus.hpp
//   - kernel32.dll -> include/micant/kernel32.hpp
//   - ole32.dll    -> include/micant/ole32.hpp
//   - oledlg.dll   -> include/micant/ole32.hpp
//   - oleacc.dll   -> include/micant/uiautomation.hpp
//   - oleaut32.dll -> include/micant/oleaut32.hpp
//   - propsys.dll  -> include/micant/propsys.hpp
//   - shell32.dll  -> include/micant/shell32.hpp
//   - shlwapi.dll  -> include/micant/shell32.hpp
//   - user32.dll   -> include/micant/user32.hpp
//   - uxtheme.dll  -> include/micant/uxtheme.hpp
//   - wininet.dll  -> include/micant/wininet.hpp
//   - winspool.drv -> include/micant/winspool.hpp
//
// Strict clean-room implementation referencing Microsoft win32metadata.
// Zero proprietary, leaked, or decompiled code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#include <cwchar>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <cstdarg>
#include <cmath>

#include "ntdef.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "gdi32.hpp"
#include "gdiplus.hpp"
#include "advapi32.hpp"
#include "comctl32.hpp"
#include "ole32.hpp"
#include "oleaut32.hpp"
#include "uiautomation.hpp"
#include "propsys.hpp"
#include "shell32.hpp"
#include "uxtheme.hpp"
#include "wininet.hpp"
#include "winspool.hpp"
#include "ldr.hpp"

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
