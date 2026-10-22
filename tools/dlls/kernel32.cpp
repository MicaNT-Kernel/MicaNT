/**
 * @file kernel32.cpp
 * @brief Sovereign Win32 Base API DLL (kernel32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 */

#include <cstdint>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept { return true; }

__declspec(dllexport) void* CreateFileW(const wchar_t*, uint32_t, uint32_t, void*, uint32_t, uint32_t, void*) noexcept { return reinterpret_cast<void*>(-1); }
__declspec(dllexport) bool ReadFile(void*, void*, uint32_t, uint32_t*, void*) noexcept { return false; }
__declspec(dllexport) bool WriteFile(void*, const void*, uint32_t, uint32_t*, void*) noexcept { return false; }
__declspec(dllexport) bool CloseHandle(void*) noexcept { return true; }
__declspec(dllexport) void* GetProcAddress(void*, const char*) noexcept { return nullptr; }
__declspec(dllexport) void* LoadLibraryW(const wchar_t*) noexcept { return nullptr; }
__declspec(dllexport) void* GetModuleHandleW(const wchar_t*) noexcept { return nullptr; }
__declspec(dllexport) void* VirtualAlloc(void*, size_t, uint32_t, uint32_t) noexcept { return nullptr; }
__declspec(dllexport) bool VirtualFree(void*, size_t, uint32_t) noexcept { return true; }
__declspec(dllexport) void ExitProcess(uint32_t) noexcept {}
__declspec(dllexport) void GetSystemInfo(void*) noexcept {}
__declspec(dllexport) uint64_t GetTickCount64() noexcept { return 0; }
__declspec(dllexport) uint32_t GetLastError() noexcept { return 0; }
__declspec(dllexport) void SetLastError(uint32_t) noexcept {}

} // extern "C"
