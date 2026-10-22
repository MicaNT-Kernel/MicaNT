/**
 * @file gdi32.cpp
 * @brief Sovereign Graphics Device Interface DLL (gdi32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 */

#include <cstdint>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept { return true; }

__declspec(dllexport) void* CreateCompatibleDC(void*) noexcept { return nullptr; }
__declspec(dllexport) void* CreateCompatibleBitmap(void*, int, int) noexcept { return nullptr; }
__declspec(dllexport) void* SelectObject(void*, void*) noexcept { return nullptr; }
__declspec(dllexport) bool BitBlt(void*, int, int, int, int, void*, int, int, uint32_t) noexcept { return true; }
__declspec(dllexport) void* GetStockObject(int) noexcept { return nullptr; }
__declspec(dllexport) bool DeleteObject(void*) noexcept { return true; }
__declspec(dllexport) bool TextOutW(void*, int, int, const wchar_t*, int) noexcept { return true; }

} // extern "C"
