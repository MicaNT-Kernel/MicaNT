/**
 * @file ntdll.cpp
 * @brief Sovereign NT Layer DLL (ntdll.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Native system call stubs and runtime library exports.
 */

#include <cstdint>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept { return true; }

__declspec(dllexport) uint64_t NtClose() noexcept { return 0; }
__declspec(dllexport) uint64_t NtReadFile() noexcept { return 0; }
__declspec(dllexport) uint64_t NtWriteFile() noexcept { return 0; }
__declspec(dllexport) uint64_t NtAllocateVirtualMemory() noexcept { return 0; }
__declspec(dllexport) uint64_t NtFreeVirtualMemory() noexcept { return 0; }
__declspec(dllexport) uint64_t NtQueryVirtualMemory() noexcept { return 0; }
__declspec(dllexport) uint64_t NtQuerySystemInformation() noexcept { return 0; }
__declspec(dllexport) uint64_t NtYieldExecution() noexcept { return 0; }
__declspec(dllexport) uint64_t NtTerminateProcess() noexcept { return 0; }
__declspec(dllexport) uint64_t NtGdiGetVideoInfo() noexcept { return 0; }
__declspec(dllexport) uint64_t NtGdiPresentSurface() noexcept { return 0; }
__declspec(dllexport) uint64_t NtIcmpSendEcho() noexcept { return 0; }

__declspec(dllexport) void RtlInitUnicodeString() noexcept {}
__declspec(dllexport) void* RtlAllocateHeap(void*, uint32_t, size_t) noexcept { return nullptr; }
__declspec(dllexport) bool RtlFreeHeap(void*, uint32_t, void*) noexcept { return true; }
__declspec(dllexport) void LdrInitializeThunk() noexcept {}
__declspec(dllexport) void LdrLoadDll() noexcept {}
__declspec(dllexport) void LdrGetProcedureAddress() noexcept {}

} // extern "C"
