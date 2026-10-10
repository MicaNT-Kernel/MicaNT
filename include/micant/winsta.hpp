// ============================================================================
// MicaNT: Windows Station & Remote Desktop Subsystem (winsta.dll) (winsta.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides Windows Station / Remote Desktop Session Management APIs.
// Strict clean-room implementation referencing Microsoft win32metadata.
// Zero proprietary, leaked, or decompiled code.
// ============================================================================

#pragma once

#include <cstdint>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::winsta {

using BOOL = int32_t;
using DWORD = uint32_t;
using BYTE = uint8_t;

inline constexpr BOOL TRUE_VAL = 1;

inline BOOL WINAPI WinStationConnectW(void* /*hServer*/, DWORD /*SessionId*/, DWORD /*TargetSessionId*/, const wchar_t* /*pPassword*/, BOOL /*bWait*/) noexcept {
    return TRUE_VAL;
}

inline BOOL WINAPI WinStationDisconnect(void* /*hServer*/, DWORD /*SessionId*/, BOOL /*bWait*/) noexcept {
    return TRUE_VAL;
}

inline BOOL WINAPI WinStationEnumerateW(void* /*hServer*/, void** ppSessionInfo, DWORD* pCount) noexcept {
    if (ppSessionInfo) *ppSessionInfo = nullptr;
    if (pCount) *pCount = 0;
    return TRUE_VAL;
}

inline BOOL WINAPI WinStationFreeMemory(void* /*pBuffer*/) noexcept {
    return TRUE_VAL;
}

inline BOOL WINAPI WinStationQueryInformationW(void* /*hServer*/, DWORD /*SessionId*/, DWORD /*WinStationInformationClass*/, void* /*pWinStationInformation*/, DWORD /*WinStationInformationLength*/, DWORD* pReturnLength) noexcept {
    if (pReturnLength) *pReturnLength = 64;
    return TRUE_VAL;
}

inline BOOL WINAPI WinStationReset(void* /*hServer*/, DWORD /*SessionId*/, BOOL /*bWait*/) noexcept {
    return TRUE_VAL;
}

inline BOOL WINAPI WinStationSendMessageW(void* /*hServer*/, DWORD /*SessionId*/, const wchar_t* /*pTitle*/, DWORD /*TitleLength*/, const wchar_t* /*pMessage*/, DWORD /*MessageLength*/, DWORD /*Style*/, DWORD /*Timeout*/, DWORD* pResponse, BOOL /*bWait*/) noexcept {
    if (pResponse) *pResponse = 1; // IDOK
    return TRUE_VAL;
}

inline BOOL WINAPI WinStationShadow(void* /*hServer*/, const wchar_t* /*pServerName*/, DWORD /*SessionId*/, BYTE /*Hotkey*/, DWORD /*HotkeyModifiers*/) noexcept {
    return TRUE_VAL;
}

inline void InitializeWinStaSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    ldr.registerExport("winsta.dll", "WinStationConnectW", reinterpret_cast<void*>(WinStationConnectW));
    ldr.registerExport("winsta.dll", "WinStationDisconnect", reinterpret_cast<void*>(WinStationDisconnect));
    ldr.registerExport("winsta.dll", "WinStationEnumerateW", reinterpret_cast<void*>(WinStationEnumerateW));
    ldr.registerExport("winsta.dll", "WinStationFreeMemory", reinterpret_cast<void*>(WinStationFreeMemory));
    ldr.registerExport("winsta.dll", "WinStationQueryInformationW", reinterpret_cast<void*>(WinStationQueryInformationW));
    ldr.registerExport("winsta.dll", "WinStationReset", reinterpret_cast<void*>(WinStationReset));
    ldr.registerExport("winsta.dll", "WinStationSendMessageW", reinterpret_cast<void*>(WinStationSendMessageW));
    ldr.registerExport("winsta.dll", "WinStationShadow", reinterpret_cast<void*>(WinStationShadow));
}

} // namespace micant::winsta
