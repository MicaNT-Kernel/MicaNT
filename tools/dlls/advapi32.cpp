/**
 * @file advapi32.cpp
 * @brief Sovereign Advanced Windows 32 Security & Registry DLL (advapi32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 */

#include <cstdint>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept { return true; }

__declspec(dllexport) int32_t RegOpenKeyExW(void*, const wchar_t*, uint32_t, uint32_t, void**) noexcept { return 0; }
__declspec(dllexport) int32_t RegQueryValueExW(void*, const wchar_t*, uint32_t*, uint32_t*, uint8_t*, uint32_t*) noexcept { return 0; }
__declspec(dllexport) int32_t RegCloseKey(void*) noexcept { return 0; }
__declspec(dllexport) bool OpenProcessToken(void*, uint32_t, void**) noexcept { return true; }
__declspec(dllexport) bool GetTokenInformation(void*, int, void*, uint32_t, uint32_t*) noexcept { return true; }
__declspec(dllexport) void* OpenSCManagerW(const wchar_t*, const wchar_t*, uint32_t) noexcept { return nullptr; }
__declspec(dllexport) void* OpenServiceW(void*, const wchar_t*, uint32_t) noexcept { return nullptr; }
__declspec(dllexport) bool StartServiceW(void*, uint32_t, const wchar_t**) noexcept { return true; }

} // extern "C"
