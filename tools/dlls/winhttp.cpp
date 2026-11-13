// Native WinHTTP exports over the mode-neutral HTTP/TLS core and AFD Winsock.
#define MICANT_NATIVE_USERMODE 1
#include "micant/um/subsystems/net/winhttp.hpp"

extern "C" {
using HINTERNET = micant::winhttp::HINTERNET;
using DWORD = micant::winhttp::DWORD;
using BOOL = micant::winhttp::BOOL;

__declspec(dllexport) HINTERNET __stdcall WinHttpOpen(const wchar_t* agent, DWORD access,
    const wchar_t* proxy, const wchar_t* bypass, DWORD flags) noexcept {
    return micant::winhttp::WinHttpOpen(agent, access, proxy, bypass, flags);
}
__declspec(dllexport) HINTERNET __stdcall WinHttpConnect(HINTERNET session, const wchar_t* host,
    uint16_t port, DWORD reserved) noexcept {
    return micant::winhttp::WinHttpConnect(session, host, port, reserved);
}
__declspec(dllexport) HINTERNET __stdcall WinHttpOpenRequest(HINTERNET connection, const wchar_t* verb,
    const wchar_t* path, const wchar_t* version, const wchar_t* referrer,
    const wchar_t** acceptTypes, DWORD flags) noexcept {
    return micant::winhttp::WinHttpOpenRequest(connection, verb, path, version, referrer, acceptTypes, flags);
}
__declspec(dllexport) BOOL __stdcall WinHttpAddRequestHeaders(HINTERNET request, const wchar_t* headers,
    DWORD length, DWORD modifiers) noexcept {
    return micant::winhttp::WinHttpAddRequestHeaders(request, headers, length, modifiers);
}
__declspec(dllexport) BOOL __stdcall WinHttpWriteData(HINTERNET request, const void* buffer,
    DWORD length, DWORD* written) noexcept {
    return micant::winhttp::WinHttpWriteData(request, buffer, length, written);
}
__declspec(dllexport) BOOL __stdcall WinHttpSendRequest(HINTERNET request, const wchar_t* headers,
    DWORD headersLength, void* optional, DWORD optionalLength, DWORD totalLength, uint64_t context) noexcept {
    return micant::winhttp::WinHttpSendRequest(request, headers, headersLength, optional,
        optionalLength, totalLength, context);
}
__declspec(dllexport) BOOL __stdcall WinHttpReceiveResponse(HINTERNET request, void* reserved) noexcept {
    return micant::winhttp::WinHttpReceiveResponse(request, reserved);
}
__declspec(dllexport) BOOL __stdcall WinHttpQueryHeaders(HINTERNET request, DWORD level,
    const wchar_t* name, void* buffer, DWORD* length, DWORD* index) noexcept {
    return micant::winhttp::WinHttpQueryHeaders(request, level, name, buffer, length, index);
}
__declspec(dllexport) BOOL __stdcall WinHttpQueryDataAvailable(HINTERNET request, DWORD* available) noexcept {
    return micant::winhttp::WinHttpQueryDataAvailable(request, available);
}
__declspec(dllexport) BOOL __stdcall WinHttpReadData(HINTERNET request, void* buffer,
    DWORD length, DWORD* read) noexcept {
    return micant::winhttp::WinHttpReadData(request, buffer, length, read);
}
__declspec(dllexport) BOOL __stdcall WinHttpSetOption(HINTERNET request, DWORD option,
    void* buffer, DWORD length) noexcept {
    return micant::winhttp::WinHttpSetOption(request, option, buffer, length);
}
__declspec(dllexport) BOOL __stdcall WinHttpCloseHandle(HINTERNET handle) noexcept {
    return micant::winhttp::WinHttpCloseHandle(handle);
}
__declspec(dllexport) BOOL __stdcall WinHttpCrackUrl(const wchar_t* url, DWORD length,
    DWORD flags, void* components) noexcept {
    return micant::winhttp::WinHttpCrackUrl(url, length, flags, components);
}
__declspec(dllexport) BOOL __stdcall WinHttpGetDefaultProxyConfiguration(void* config) noexcept {
    return micant::winhttp::WinHttpGetDefaultProxyConfiguration(config);
}
__declspec(dllexport) BOOL __stdcall WinHttpSetTimeouts(HINTERNET handle, int32_t resolve,
    int32_t connect, int32_t send, int32_t receive) noexcept {
    return micant::winhttp::WinHttpSetTimeouts(handle, resolve, connect, send, receive);
}
__declspec(dllexport) void* __stdcall WinHttpSetStatusCallback(HINTERNET handle, void* callback,
    DWORD flags, uint64_t reserved) noexcept {
    return micant::winhttp::WinHttpSetStatusCallback(handle, callback, flags, reserved);
}
__declspec(dllexport) int DllMain(void*, uint32_t, void*) noexcept { return 1; }
}
