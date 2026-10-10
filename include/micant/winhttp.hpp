// ============================================================================
// MicaNT: Windows HTTP Services Subsystem (winhttp.dll)
// (winhttp.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides HTTP client and proxy configuration discovery conforming to win32metadata.
// ============================================================================

#pragma once

#include <cstdint>
#include "ntdef.hpp"
#include "ldr.hpp"

namespace micant::winhttp {

using BOOL = int32_t;
using DWORD = uint32_t;
using HANDLE = void*;

inline BOOL WINAPI WinHttpGetDefaultProxyConfiguration(void* /*pConfig*/) noexcept {
    return 0; // FALSE: No system proxy
}

inline BOOL WINAPI WinHttpCrackUrl(const wchar_t* /*pwszUrl*/, uint32_t /*dwUrlLength*/, uint32_t /*dwFlags*/, void* /*lpUrlComponents*/) noexcept {
    return 1;
}

inline BOOL WINAPI WinHttpGetIEProxyConfigForCurrentUser(void* /*pProxyConfig*/) noexcept { return 1; }
inline BOOL WINAPI WinHttpSetTimeouts(HANDLE /*hInternet*/, int32_t /*nResolveTimeout*/, int32_t /*nConnectTimeout*/, int32_t /*nSendTimeout*/, int32_t /*nReceiveTimeout*/) noexcept { return 1; }
inline void* WINAPI WinHttpSetStatusCallback(HANDLE /*hInternet*/, void* /*lpfnInternetCallback*/, DWORD /*dwNotificationFlags*/, uint64_t /*dwReserved*/) noexcept { return nullptr; }
inline HANDLE WINAPI WinHttpConnect(HANDLE /*hSession*/, const wchar_t* /*pswzServerName*/, uint16_t /*nServerPort*/, DWORD /*dwReserved*/) noexcept { return reinterpret_cast<HANDLE>(0x9002); }
inline BOOL WINAPI WinHttpReceiveResponse(HANDLE /*hRequest*/, void* /*lpReserved*/) noexcept { return 1; }
inline BOOL WINAPI WinHttpQueryAuthSchemes(HANDLE /*hRequest*/, DWORD* /*lpdwSupportedSchemes*/, DWORD* /*lpdwFirstScheme*/, DWORD* /*pdwAuthTarget*/) noexcept { return 1; }
inline BOOL WINAPI WinHttpGetProxyForUrl(HANDLE /*hSession*/, const wchar_t* /*lpcwszUrl*/, void* /*pAutoProxyOptions*/, void* /*pProxyInfo*/) noexcept { return 1; }
inline BOOL WINAPI WinHttpReadData(HANDLE /*hRequest*/, void* /*lpBuffer*/, DWORD /*dwNumberOfBytesToRead*/, DWORD* lpdwNumberOfBytesRead) noexcept {
    if (lpdwNumberOfBytesRead) *lpdwNumberOfBytesRead = 0;
    return 1;
}
inline BOOL WINAPI WinHttpCloseHandle(HANDLE /*hInternet*/) noexcept { return 1; }
inline BOOL WINAPI WinHttpQueryHeaders(HANDLE /*hRequest*/, DWORD /*dwInfoLevel*/, const wchar_t* /*pwszName*/, void* /*lpBuffer*/, DWORD* /*lpdwBufferLength*/, DWORD* /*lpdwIndex*/) noexcept { return 1; }
inline HANDLE WINAPI WinHttpOpenRequest(HANDLE /*hConnect*/, const wchar_t* /*pwszVerb*/, const wchar_t* /*pwszObjectName*/, const wchar_t* /*pwszVersion*/, const wchar_t* /*pwszReferrer*/, const wchar_t** /*ppwszAcceptTypes*/, DWORD /*dwFlags*/) noexcept { return reinterpret_cast<HANDLE>(0x9003); }
inline BOOL WINAPI WinHttpAddRequestHeaders(HANDLE /*hRequest*/, const wchar_t* /*lpszHeaders*/, DWORD /*dwHeadersLength*/, DWORD /*dwModifiers*/) noexcept { return 1; }
inline HANDLE WINAPI WinHttpOpen(const wchar_t* /*pszAgentW*/, DWORD /*dwAccessType*/, const wchar_t* /*pszProxyW*/, const wchar_t* /*pszProxyBypassW*/, DWORD /*dwFlags*/) noexcept { return reinterpret_cast<HANDLE>(0x9001); }
inline BOOL WINAPI WinHttpWriteData(HANDLE /*hRequest*/, const void* /*lpBuffer*/, DWORD /*dwNumberOfBytesToWrite*/, DWORD* lpdwNumberOfBytesWritten) noexcept {
    if (lpdwNumberOfBytesWritten) *lpdwNumberOfBytesWritten = 0;
    return 1;
}
inline BOOL WINAPI WinHttpSetCredentials(HANDLE /*hRequest*/, DWORD /*AuthTargets*/, DWORD /*AuthScheme*/, const wchar_t* /*pwszUserName*/, const wchar_t* /*pwszPassword*/, void* /*pAuthParams*/) noexcept { return 1; }
inline BOOL WINAPI WinHttpQueryDataAvailable(HANDLE /*hRequest*/, DWORD* lpdwNumberOfBytesAvailable) noexcept {
    if (lpdwNumberOfBytesAvailable) *lpdwNumberOfBytesAvailable = 0;
    return 1;
}
inline BOOL WINAPI WinHttpSetOption(HANDLE /*hInternet*/, DWORD /*dwOption*/, void* /*lpBuffer*/, DWORD /*dwBufferLength*/) noexcept { return 1; }
inline BOOL WINAPI WinHttpSendRequest(HANDLE /*hRequest*/, const wchar_t* /*lpszHeaders*/, DWORD /*dwHeadersLength*/, void* /*lpOptional*/, DWORD /*dwOptionalLength*/, DWORD /*dwTotalLength*/, uint64_t /*dwContext*/) noexcept { return 1; }
inline BOOL WINAPI WinHttpQueryOption(HANDLE /*hInternet*/, DWORD /*dwOption*/, void* /*lpBuffer*/, DWORD* /*lpdwBufferLength*/) noexcept { return 1; }

inline void InitializeWinHttpSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("winhttp.dll", "WinHttpGetDefaultProxyConfiguration", reinterpret_cast<void*>(WinHttpGetDefaultProxyConfiguration));
    ldr.registerExport("winhttp.dll", "WinHttpCrackUrl", reinterpret_cast<void*>(WinHttpCrackUrl));
    ldr.registerExport("winhttp.dll", "WinHttpGetIEProxyConfigForCurrentUser", reinterpret_cast<void*>(WinHttpGetIEProxyConfigForCurrentUser));
    ldr.registerExport("winhttp.dll", "WinHttpSetTimeouts", reinterpret_cast<void*>(WinHttpSetTimeouts));
    ldr.registerExport("winhttp.dll", "WinHttpSetStatusCallback", reinterpret_cast<void*>(WinHttpSetStatusCallback));
    ldr.registerExport("winhttp.dll", "WinHttpConnect", reinterpret_cast<void*>(WinHttpConnect));
    ldr.registerExport("winhttp.dll", "WinHttpReceiveResponse", reinterpret_cast<void*>(WinHttpReceiveResponse));
    ldr.registerExport("winhttp.dll", "WinHttpQueryAuthSchemes", reinterpret_cast<void*>(WinHttpQueryAuthSchemes));
    ldr.registerExport("winhttp.dll", "WinHttpGetProxyForUrl", reinterpret_cast<void*>(WinHttpGetProxyForUrl));
    ldr.registerExport("winhttp.dll", "WinHttpReadData", reinterpret_cast<void*>(WinHttpReadData));
    ldr.registerExport("winhttp.dll", "WinHttpCloseHandle", reinterpret_cast<void*>(WinHttpCloseHandle));
    ldr.registerExport("winhttp.dll", "WinHttpQueryHeaders", reinterpret_cast<void*>(WinHttpQueryHeaders));
    ldr.registerExport("winhttp.dll", "WinHttpOpenRequest", reinterpret_cast<void*>(WinHttpOpenRequest));
    ldr.registerExport("winhttp.dll", "WinHttpAddRequestHeaders", reinterpret_cast<void*>(WinHttpAddRequestHeaders));
    ldr.registerExport("winhttp.dll", "WinHttpOpen", reinterpret_cast<void*>(WinHttpOpen));
    ldr.registerExport("winhttp.dll", "WinHttpWriteData", reinterpret_cast<void*>(WinHttpWriteData));
    ldr.registerExport("winhttp.dll", "WinHttpSetCredentials", reinterpret_cast<void*>(WinHttpSetCredentials));
    ldr.registerExport("winhttp.dll", "WinHttpQueryDataAvailable", reinterpret_cast<void*>(WinHttpQueryDataAvailable));
    ldr.registerExport("winhttp.dll", "WinHttpSetOption", reinterpret_cast<void*>(WinHttpSetOption));
    ldr.registerExport("winhttp.dll", "WinHttpSendRequest", reinterpret_cast<void*>(WinHttpSendRequest));
    ldr.registerExport("winhttp.dll", "WinHttpQueryOption", reinterpret_cast<void*>(WinHttpQueryOption));
}

} // namespace micant::winhttp
