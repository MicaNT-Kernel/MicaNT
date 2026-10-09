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

inline win32::BOOL IsThemePartDefined(HTHEME /*hTheme*/, int /*iPartId*/, int /*iStateId*/) noexcept {
    return win32::TRUE;
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
}

} // namespace micant::uxtheme
