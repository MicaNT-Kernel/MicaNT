/**
 * @file shlwapi.cpp
 * @brief Sovereign Shell Lightweight Utility DLL (shlwapi.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 */

#include <cstdint>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept { return true; }

__declspec(dllexport) bool PathFileExistsA(const char*) noexcept { return true; }
__declspec(dllexport) bool PathFileExistsW(const wchar_t*) noexcept { return true; }
__declspec(dllexport) bool PathIsDirectoryA(const char*) noexcept { return false; }
__declspec(dllexport) bool PathIsDirectoryW(const wchar_t*) noexcept { return false; }
__declspec(dllexport) char* PathFindFileNameA(const char* path) noexcept { return const_cast<char*>(path); }
__declspec(dllexport) wchar_t* PathFindFileNameW(const wchar_t* path) noexcept { return const_cast<wchar_t*>(path); }
__declspec(dllexport) char* PathFindExtensionA(const char* path) noexcept { return const_cast<char*>(path); }
__declspec(dllexport) wchar_t* PathFindExtensionW(const wchar_t* path) noexcept { return const_cast<wchar_t*>(path); }
__declspec(dllexport) char* PathCombineA(char* dest, const char* dir, const char* file) noexcept { return dest; }
__declspec(dllexport) wchar_t* PathCombineW(wchar_t* dest, const wchar_t* dir, const wchar_t* file) noexcept { return dest; }
__declspec(dllexport) bool PathRemoveFileSpecA(char* path) noexcept { return true; }
__declspec(dllexport) bool PathRemoveFileSpecW(wchar_t* path) noexcept { return true; }

} // extern "C"
