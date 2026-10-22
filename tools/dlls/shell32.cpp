/**
 * @file shell32.cpp
 * @brief Sovereign Windows Shell API DLL (shell32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 */

#include <cstdint>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept { return true; }

__declspec(dllexport) void* ShellExecuteA(void*, const char*, const char*, const char*, const char*, int) noexcept { return nullptr; }
__declspec(dllexport) void* ShellExecuteW(void*, const wchar_t*, const wchar_t*, const wchar_t*, const wchar_t*, int) noexcept { return nullptr; }
__declspec(dllexport) bool ShellExecuteExA(void*) noexcept { return true; }
__declspec(dllexport) bool ShellExecuteExW(void*) noexcept { return true; }
__declspec(dllexport) int SHGetFolderPathA(void*, int, void*, unsigned long, char*) noexcept { return 0; }
__declspec(dllexport) int SHGetFolderPathW(void*, int, void*, unsigned long, wchar_t*) noexcept { return 0; }
__declspec(dllexport) int SHGetKnownFolderPath(const void*, unsigned long, void*, wchar_t**) noexcept { return 0; }
__declspec(dllexport) wchar_t** CommandLineToArgvW(const wchar_t*, int*) noexcept { return nullptr; }
__declspec(dllexport) void* SHBrowseForFolderA(void*) noexcept { return nullptr; }
__declspec(dllexport) void* SHBrowseForFolderW(void*) noexcept { return nullptr; }
__declspec(dllexport) int SHFileOperationA(void*) noexcept { return 0; }
__declspec(dllexport) int SHFileOperationW(void*) noexcept { return 0; }

} // extern "C"
