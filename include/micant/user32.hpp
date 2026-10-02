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
#include "winlogon.hpp"

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

inline win32::BOOL LockWorkStation() noexcept {
    return winlogon::WinlogonManager::get().lockWorkstation() ? win32::TRUE : win32::FALSE;
}

inline win32::HANDLE OpenDesktopW(
    const wchar_t* lpszDesktop,
    uint32_t /*dwFlags*/,
    win32::BOOL /*fInherit*/,
    uint32_t /*dwDesiredAccess*/
) noexcept {
    if (!lpszDesktop) return nullptr;
    if (_wcsicmp(lpszDesktop, L"Winlogon") == 0) {
        return reinterpret_cast<win32::HANDLE>(0x0000000000000010ULL);
    }
    if (_wcsicmp(lpszDesktop, L"Default") == 0) {
        return reinterpret_cast<win32::HANDLE>(0x0000000000000020ULL);
    }
    return nullptr;
}

inline win32::BOOL SwitchDesktop(win32::HANDLE hDesktop) noexcept {
    if (hDesktop == reinterpret_cast<win32::HANDLE>(0x0000000000000010ULL)) {
        winlogon::WinlogonManager::get().switchDesktop(winlogon::DesktopType::Winlogon);
        return win32::TRUE;
    }
    if (hDesktop == reinterpret_cast<win32::HANDLE>(0x0000000000000020ULL)) {
        winlogon::WinlogonManager::get().switchDesktop(winlogon::DesktopType::Default);
        return win32::TRUE;
    }
    return win32::FALSE;
}

inline win32::BOOL CloseDesktop(win32::HANDLE /*hDesktop*/) noexcept {
    return win32::TRUE;
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
    ldr.registerExport("user32.dll", "LockWorkStation", reinterpret_cast<void*>(LockWorkStation));
    ldr.registerExport("user32.dll", "OpenDesktopW", reinterpret_cast<void*>(OpenDesktopW));
    ldr.registerExport("user32.dll", "SwitchDesktop", reinterpret_cast<void*>(SwitchDesktop));
    ldr.registerExport("user32.dll", "CloseDesktop", reinterpret_cast<void*>(CloseDesktop));
}

} // namespace micant::user32
