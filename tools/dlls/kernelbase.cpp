/**
 * @file kernelbase.cpp
 * @brief Sovereign Kernel Base Subsystem DLL (kernelbase.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 */

#include <cstdint>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept { return true; }

__declspec(dllexport) bool QueryPerformanceCounter(int64_t*) noexcept { return true; }
__declspec(dllexport) bool QueryPerformanceFrequency(int64_t*) noexcept { return true; }
__declspec(dllexport) void* OpenProcess(uint32_t, bool, uint32_t) noexcept { return nullptr; }
__declspec(dllexport) bool TerminateProcess(void*, uint32_t) noexcept { return true; }
__declspec(dllexport) void* VirtualAllocEx(void*, void*, size_t, uint32_t, uint32_t) noexcept { return nullptr; }
__declspec(dllexport) bool VirtualFreeEx(void*, void*, size_t, uint32_t) noexcept { return true; }

} // extern "C"
