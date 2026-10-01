#pragma once

/**
 * @file user32.hpp
 * @brief MicaNT Clean-Room User Interface Subsystem (user32.dll) Bridge.
 *
 * Implements window message pump and visibility query stubs for console applications.
 */

#include <cstdint>
#include "ntdef.hpp"
#include "kernel32.hpp"
#include "ldr.hpp"

namespace micant::user32 {

inline win32::BOOL ShowWindow(win32::HWND /*hWnd*/, int /*nCmdShow*/) noexcept {
    return win32::TRUE;
}

inline win32::BOOL IsWindowVisible(win32::HWND /*hWnd*/) noexcept {
    return win32::TRUE;
}

inline win32::BOOL IsIconic(win32::HWND /*hWnd*/) noexcept {
    return win32::FALSE;
}

inline win32::BOOL PeekMessageA(
    void* /*lpMsg*/,
    win32::HWND /*hWnd*/,
    uint32_t /*wMsgFilterMin*/,
    uint32_t /*wMsgFilterMax*/,
    uint32_t /*wRemoveMsg*/
) noexcept {
    return win32::FALSE;
}

inline win32::BOOL TranslateMessage(const void* /*lpMsg*/) noexcept {
    return win32::FALSE;
}

inline int64_t DispatchMessageA(const void* /*lpMsg*/) noexcept {
    return 0;
}

inline uint32_t MsgWaitForMultipleObjects(
    uint32_t nCount,
    const win32::HANDLE* pHandles,
    win32::BOOL fWaitAll,
    uint32_t dwMilliseconds,
    uint32_t /*dwWakeMask*/
) noexcept {
    return win32::WaitForMultipleObjects(nCount, pHandles, fWaitAll, dwMilliseconds);
}

inline void InitializeUser32SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("user32.dll", "ShowWindow", reinterpret_cast<void*>(ShowWindow));
    ldr.registerExport("user32.dll", "IsWindowVisible", reinterpret_cast<void*>(IsWindowVisible));
    ldr.registerExport("user32.dll", "IsIconic", reinterpret_cast<void*>(IsIconic));
    ldr.registerExport("user32.dll", "PeekMessageA", reinterpret_cast<void*>(PeekMessageA));
    ldr.registerExport("user32.dll", "TranslateMessage", reinterpret_cast<void*>(TranslateMessage));
    ldr.registerExport("user32.dll", "DispatchMessageA", reinterpret_cast<void*>(DispatchMessageA));
    ldr.registerExport("user32.dll", "MsgWaitForMultipleObjects", reinterpret_cast<void*>(MsgWaitForMultipleObjects));
}

} // namespace micant::user32
