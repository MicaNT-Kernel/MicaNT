// ============================================================================
// MicaNT: Windows Theme and Visual Styles Subsystem (uxtheme.dll)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides UxTheme visual styling, buffered animation, and dark mode hooks.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include "ntdef.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "ldr.hpp"

namespace micant::uxtheme {

using LPCWSTR = const wchar_t*;
using HTHEME = void*;
using HPAINTBUFFER = void*;
using HANAnimation = void*;

inline HTHEME OpenThemeData(win32::HWND /*hwnd*/, LPCWSTR /*pszClassList*/) noexcept {
    static uint64_t dummyTheme = 0x8888;
    return reinterpret_cast<HTHEME>(&dummyTheme);
}

inline win32::HRESULT CloseThemeData(HTHEME /*hTheme*/) noexcept {
    return 0; // S_OK
}

inline win32::HRESULT GetThemeColor(HTHEME /*hTheme*/, int /*iPartId*/, int /*iStateId*/, int /*iPropId*/, win32::DWORD* pColor) noexcept {
    if (pColor) *pColor = 0x00FFFFFF; // White
    return 0;
}

inline win32::HRESULT GetThemeFont(HTHEME /*hTheme*/, void* /*hdc*/, int /*iPartId*/, int /*iStateId*/, int /*iPropId*/, void* /*pFont*/) noexcept {
    return 0;
}

inline win32::HRESULT GetThemePartSize(HTHEME /*hTheme*/, void* /*hdc*/, int /*iPartId*/, int /*iStateId*/, void* /*prc*/, int /*eSize*/, void* psz) noexcept {
    if (psz) {
        auto* sz = reinterpret_cast<int32_t*>(psz);
        sz[0] = 16;
        sz[1] = 16;
    }
    return 0;
}

inline win32::HRESULT GetThemeBackgroundContentRect(HTHEME /*hTheme*/, void* /*hdc*/, int /*iPartId*/, int /*iStateId*/, const void* pBoundingRect, void* pContentRect) noexcept {
    if (pBoundingRect && pContentRect) {
        std::memcpy(pContentRect, pBoundingRect, sizeof(user32::RECT));
    }
    return 0;
}

inline win32::HRESULT DrawThemeBackground(HTHEME /*hTheme*/, void* /*hdc*/, int /*iPartId*/, int /*iStateId*/, const void* /*pRect*/, const void* /*pClipRect*/) noexcept {
    return 0;
}

inline win32::HRESULT DrawThemeParentBackground(win32::HWND /*hwnd*/, void* /*hdc*/, const void* /*prc*/) noexcept {
    return 0;
}

inline win32::HRESULT DrawThemeTextEx(HTHEME /*hTheme*/, void* /*hdc*/, int /*iPartId*/, int /*iStateId*/, LPCWSTR /*pszText*/, int /*iCharCount*/, win32::DWORD /*dwFlags*/, void* /*pRect*/, void* /*pOptions*/) noexcept {
    return 0;
}

inline win32::HRESULT SetWindowTheme(win32::HWND /*hwnd*/, LPCWSTR /*pszSubAppName*/, LPCWSTR /*pszSubIdList*/) noexcept {
    return 0;
}

inline win32::HRESULT EnableThemeDialogTexture(win32::HWND /*hwnd*/, win32::DWORD /*dwFlags*/) noexcept {
    return 0;
}

inline win32::HRESULT GetThemeTransitionDuration(HTHEME /*hTheme*/, int /*iPartId*/, int /*iStateIdFrom*/, int /*iStateIdTo*/, int /*iPropId*/, win32::DWORD* pdwDuration) noexcept {
    if (pdwDuration) *pdwDuration = 0;
    return 0;
}

inline HANAnimation BeginBufferedAnimation(win32::HWND /*hwnd*/, void* /*hdcTarget*/, const void* /*rcTarget*/, int /*dwFormat*/, void* /*pPaintParams*/, void* /*pAnimationParams*/, void* /*phdcFrom*/, void* /*phdcTo*/) noexcept {
    return nullptr;
}

inline win32::HRESULT EndBufferedAnimation(HANAnimation /*hbpAnimation*/, win32::BOOL /*fUpdateTarget*/) noexcept {
    return 0;
}

inline win32::BOOL BufferedPaintRenderAnimation(win32::HWND /*hwnd*/, void* /*hdcTarget*/) noexcept {
    return win32::FALSE;
}

inline win32::HRESULT BufferedPaintStopAllAnimations(win32::HWND /*hwnd*/) noexcept {
    return 0;
}

inline win32::HRESULT GetThemeBackgroundExtent(HTHEME /*hTheme*/, void* /*hdc*/, int /*iPartId*/, int /*iStateId*/, const void* pContentRect, void* pExtentRect) noexcept {
    if (pContentRect && pExtentRect) {
        std::memcpy(pExtentRect, pContentRect, 16); // RECT copy
    }
    return 0; // S_OK
}

inline win32::DWORD GetThemeSysColor(HTHEME /*hTheme*/, int /*iColorId*/) noexcept {
    return 0x00FFFFFF; // White
}

inline win32::HRESULT GetThemeSysFont(HTHEME /*hTheme*/, int /*iFontId*/, void* /*plf*/) noexcept {
    return 0; // S_OK
}

struct MARGINS {
    int32_t cxLeftWidth{2};
    int32_t cxRightWidth{2};
    int32_t cyTopHeight{2};
    int32_t cyBottomHeight{2};
};

inline win32::BOOL IsThemeActive() noexcept {
    return win32::TRUE;
}

inline win32::BOOL IsAppThemed() noexcept {
    return win32::TRUE;
}

inline win32::HRESULT GetThemeMargins(HTHEME /*hTheme*/, void* /*hdc*/, int /*iPartId*/, int /*iStateId*/, int /*iPropId*/, const void* /*prc*/, MARGINS* pMargins) noexcept {
    if (pMargins) {
        pMargins->cxLeftWidth = 2;
        pMargins->cxRightWidth = 2;
        pMargins->cyTopHeight = 2;
        pMargins->cyBottomHeight = 2;
    }
    return 0; // S_OK
}

inline win32::HRESULT GetThemeInt(HTHEME /*hTheme*/, int /*iPartId*/, int /*iStateId*/, int /*iPropId*/, int* piVal) noexcept {
    if (piVal) {
        *piVal = 0;
    }
    return 0; // S_OK
}

inline win32::HRESULT DrawThemeText(HTHEME /*hTheme*/, void* /*hdc*/, int /*iPartId*/, int /*iStateId*/, LPCWSTR /*pszText*/, int /*iCharCount*/, win32::DWORD /*dwTextFlags*/, win32::DWORD /*dwTextFlags2*/, const void* /*pRect*/) noexcept {
    return 0; // S_OK
}

inline win32::BOOL IsThemeBackgroundPartiallyTransparent(HTHEME /*hTheme*/, int /*iPartId*/, int /*iStateId*/) noexcept {
    return win32::FALSE;
}

inline win32::BOOL IsThemePartDefined(HTHEME /*hTheme*/, int /*iPartId*/, int /*iStateId*/) noexcept {
    return win32::TRUE;
}

inline win32::HRESULT WINAPI GetThemeBackgroundRegion(HTHEME /*hTheme*/, void* /*hdc*/, int32_t /*iPartId*/, int32_t /*iStateId*/, const void* /*pRect*/, void** pRegion) noexcept {
    if (pRegion) *pRegion = reinterpret_cast<void*>(0xF001);
    return 0; // S_OK
}

inline win32::BOOL WINAPI ThemeOrdinal47(HTHEME /*hTheme*/, int32_t /*iPartId*/, int32_t /*iStateId*/, void* /*pRect*/) noexcept {
    return win32::TRUE;
}

inline win32::HRESULT WINAPI GetThemeBool(HTHEME /*hTheme*/, int32_t /*iPartId*/, int32_t /*iStateId*/, int32_t /*iPropId*/, win32::BOOL* pfVal) noexcept {
    if (pfVal) *pfVal = win32::TRUE;
    return 0; // S_OK
}

inline win32::HRESULT WINAPI GetThemePropertyOrigin(HTHEME /*hTheme*/, int32_t /*iPartId*/, int32_t /*iStateId*/, int32_t /*iPropId*/, int32_t* pOrigin) noexcept {
    if (pOrigin) *pOrigin = 1; // PO_PART
    return 0; // S_OK
}

inline win32::HRESULT WINAPI GetThemeEnumValue(HTHEME /*hTheme*/, int32_t /*iPartId*/, int32_t /*iStateId*/, int32_t /*iPropId*/, int32_t* piVal) noexcept {
    if (piVal) *piVal = 0;
    return 0; // S_OK
}

inline win32::HRESULT WINAPI GetCurrentThemeName(wchar_t* pszThemeFileName, int32_t cchMaxNameChars, wchar_t* pszColorBuff, int32_t cchMaxColorChars, wchar_t* pszSizeBuff, int32_t cchMaxSizeChars) noexcept {
    if (pszThemeFileName && cchMaxNameChars > 0) pszThemeFileName[0] = L'\0';
    if (pszColorBuff && cchMaxColorChars > 0) pszColorBuff[0] = L'\0';
    if (pszSizeBuff && cchMaxSizeChars > 0) pszSizeBuff[0] = L'\0';
    return 0; // S_OK
}

inline win32::HRESULT WINAPI BufferedPaintInit() noexcept { return 0; }
inline win32::HRESULT WINAPI BufferedPaintUnInit() noexcept { return 0; }
inline HPAINTBUFFER WINAPI BeginBufferedPaint(void* hdcTarget, const void* /*prcTarget*/, int32_t /*dwFormat*/, void* /*pPaintParams*/, void** phdc) noexcept {
    if (phdc) *phdc = hdcTarget ? hdcTarget : reinterpret_cast<void*>(0x8800);
    return reinterpret_cast<HPAINTBUFFER>(0x8801);
}
inline win32::HRESULT WINAPI EndBufferedPaint(HPAINTBUFFER /*hBufferedPaint*/, int32_t /*fUpdateTarget*/) noexcept { return 0; }
inline win32::HRESULT WINAPI BufferedPaintSetAlpha(HPAINTBUFFER /*hBufferedPaint*/, const void* /*prcTarget*/, uint8_t /*alpha*/) noexcept { return 0; }
inline HTHEME WINAPI OpenThemeDataForDpi(win32::HWND /*hwnd*/, LPCWSTR /*pszClassList*/, uint32_t /*dpi*/) noexcept {
    static uint64_t dummyDpiTheme = 0x8889;
    return reinterpret_cast<HTHEME>(&dummyDpiTheme);
}

inline void InitializeUxThemeSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    ldr.registerExport("uxtheme.dll", "OpenThemeData", reinterpret_cast<void*>(OpenThemeData));
    ldr.registerExport("uxtheme.dll", "CloseThemeData", reinterpret_cast<void*>(CloseThemeData));
    ldr.registerExport("uxtheme.dll", "GetThemeColor", reinterpret_cast<void*>(GetThemeColor));
    ldr.registerExport("uxtheme.dll", "GetThemeFont", reinterpret_cast<void*>(GetThemeFont));
    ldr.registerExport("uxtheme.dll", "GetThemePartSize", reinterpret_cast<void*>(GetThemePartSize));
    ldr.registerExport("uxtheme.dll", "GetThemeBackgroundContentRect", reinterpret_cast<void*>(GetThemeBackgroundContentRect));
    ldr.registerExport("uxtheme.dll", "DrawThemeBackground", reinterpret_cast<void*>(DrawThemeBackground));
    ldr.registerExport("uxtheme.dll", "DrawThemeParentBackground", reinterpret_cast<void*>(DrawThemeParentBackground));
    ldr.registerExport("uxtheme.dll", "DrawThemeTextEx", reinterpret_cast<void*>(DrawThemeTextEx));
    ldr.registerExport("uxtheme.dll", "SetWindowTheme", reinterpret_cast<void*>(SetWindowTheme));
    ldr.registerExport("uxtheme.dll", "EnableThemeDialogTexture", reinterpret_cast<void*>(EnableThemeDialogTexture));
    ldr.registerExport("uxtheme.dll", "GetThemeTransitionDuration", reinterpret_cast<void*>(GetThemeTransitionDuration));
    ldr.registerExport("uxtheme.dll", "BeginBufferedAnimation", reinterpret_cast<void*>(BeginBufferedAnimation));
    ldr.registerExport("uxtheme.dll", "EndBufferedAnimation", reinterpret_cast<void*>(EndBufferedAnimation));
    ldr.registerExport("uxtheme.dll", "BufferedPaintRenderAnimation", reinterpret_cast<void*>(BufferedPaintRenderAnimation));
    ldr.registerExport("uxtheme.dll", "BufferedPaintStopAllAnimations", reinterpret_cast<void*>(BufferedPaintStopAllAnimations));
    ldr.registerExport("uxtheme.dll", "GetThemeBackgroundExtent", reinterpret_cast<void*>(GetThemeBackgroundExtent));
    ldr.registerExport("uxtheme.dll", "GetThemeSysColor", reinterpret_cast<void*>(GetThemeSysColor));
    ldr.registerExport("uxtheme.dll", "GetThemeSysFont", reinterpret_cast<void*>(GetThemeSysFont));
    ldr.registerExport("uxtheme.dll", "IsThemePartDefined", reinterpret_cast<void*>(IsThemePartDefined));
    ldr.registerExport("uxtheme.dll", "IsThemeActive", reinterpret_cast<void*>(IsThemeActive));
    ldr.registerExport("uxtheme.dll", "IsAppThemed", reinterpret_cast<void*>(IsAppThemed));
    ldr.registerExport("uxtheme.dll", "GetThemeMargins", reinterpret_cast<void*>(GetThemeMargins));
    ldr.registerExport("uxtheme.dll", "GetThemeInt", reinterpret_cast<void*>(GetThemeInt));
    ldr.registerExport("uxtheme.dll", "DrawThemeText", reinterpret_cast<void*>(DrawThemeText));
    ldr.registerExport("uxtheme.dll", "IsThemeBackgroundPartiallyTransparent", reinterpret_cast<void*>(IsThemeBackgroundPartiallyTransparent));
    ldr.registerExport("uxtheme.dll", "GetThemeBackgroundRegion", reinterpret_cast<void*>(GetThemeBackgroundRegion));
    ldr.registerExportOrdinal("uxtheme.dll", 47, reinterpret_cast<void*>(ThemeOrdinal47));
    ldr.registerExport("uxtheme.dll", "GetThemeBool", reinterpret_cast<void*>(GetThemeBool));
    ldr.registerExport("uxtheme.dll", "GetThemePropertyOrigin", reinterpret_cast<void*>(GetThemePropertyOrigin));
    ldr.registerExport("uxtheme.dll", "GetThemeEnumValue", reinterpret_cast<void*>(GetThemeEnumValue));
    ldr.registerExport("uxtheme.dll", "GetCurrentThemeName", reinterpret_cast<void*>(GetCurrentThemeName));
    ldr.registerExport("uxtheme.dll", "BufferedPaintInit", reinterpret_cast<void*>(BufferedPaintInit));
    ldr.registerExport("uxtheme.dll", "BufferedPaintUnInit", reinterpret_cast<void*>(BufferedPaintUnInit));
    ldr.registerExport("uxtheme.dll", "BeginBufferedPaint", reinterpret_cast<void*>(BeginBufferedPaint));
    ldr.registerExport("uxtheme.dll", "EndBufferedPaint", reinterpret_cast<void*>(EndBufferedPaint));
    ldr.registerExport("uxtheme.dll", "BufferedPaintSetAlpha", reinterpret_cast<void*>(BufferedPaintSetAlpha));
    ldr.registerExport("uxtheme.dll", "OpenThemeDataForDpi", reinterpret_cast<void*>(OpenThemeDataForDpi));
}

} // namespace micant::uxtheme
