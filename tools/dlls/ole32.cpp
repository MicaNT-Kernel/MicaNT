/**
 * @file ole32.cpp
 * @brief Sovereign Component Object Model (COM/OLE) DLL (ole32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 */

#include <cstdint>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept { return true; }

__declspec(dllexport) int32_t CoInitialize(void*) noexcept { return 0; }
__declspec(dllexport) int32_t CoInitializeEx(void*, unsigned long) noexcept { return 0; }
__declspec(dllexport) void CoUninitialize() noexcept {}
__declspec(dllexport) int32_t CoCreateInstance(const void*, void*, unsigned long, const void*, void**) noexcept { return 0; }
__declspec(dllexport) void* CoTaskMemAlloc(size_t) noexcept { return nullptr; }
__declspec(dllexport) void CoTaskMemFree(void*) noexcept {}
__declspec(dllexport) void* CoTaskMemRealloc(void*, size_t) noexcept { return nullptr; }
__declspec(dllexport) int32_t StringFromCLSID(const void*, wchar_t**) noexcept { return 0; }
__declspec(dllexport) int32_t CLSIDFromString(const wchar_t*, void*) noexcept { return 0; }

} // extern "C"
