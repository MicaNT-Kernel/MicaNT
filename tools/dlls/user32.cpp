/**
 * @file user32.cpp
 * @brief Sovereign User Windowing Subsystem DLL (user32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 */

#include <cstdint>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept { return true; }

__declspec(dllexport) void* CreateWindowExW(uint32_t, const wchar_t*, const wchar_t*, uint32_t, int, int, int, int, void*, void*, void*, void*) noexcept { return nullptr; }
__declspec(dllexport) bool DestroyWindow(void*) noexcept { return true; }
__declspec(dllexport) bool ShowWindow(void*, int) noexcept { return true; }
__declspec(dllexport) bool UpdateWindow(void*) noexcept { return true; }
__declspec(dllexport) bool GetMessageW(void*, void*, uint32_t, uint32_t) noexcept { return false; }
__declspec(dllexport) bool TranslateMessage(const void*) noexcept { return false; }
__declspec(dllexport) int64_t DispatchMessageW(const void*) noexcept { return 0; }
__declspec(dllexport) int64_t DefWindowProcW(void*, uint32_t, uint64_t, int64_t) noexcept { return 0; }
__declspec(dllexport) void PostQuitMessage(int) noexcept {}

} // extern "C"
