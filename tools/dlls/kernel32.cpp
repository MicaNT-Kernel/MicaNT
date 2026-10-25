/**
 * @file kernel32.cpp
 * @brief Sovereign Win32 Base API DLL (kernel32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Implements core Win32 Process, Thread, Memory, and File I/O trampolines
 * backed directly by MicaNT Ring 3 syscalls (0x01, 0x11, 0x18, 0x1B, 0x24, 0x25, 0x46).
 */

#include <cstdint>
#include <cstddef>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

// Low-level syscall invocation helper
static inline uint64_t SyscallDispatch(uint64_t ssn, uint64_t arg1 = 0, uint64_t arg2 = 0) noexcept {
    uint64_t res = 0;
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %1, %%rax\n\t"
        "movq %2, %%rdx\n\t"
        "movq %3, %%r8\n\t"
        "syscall\n\t"
        "movq %%rax, %0\n\t"
        : "=r"(res)
        : "r"(ssn), "r"(arg1), "r"(arg2)
        : "rax", "rdx", "r8", "rcx", "r11", "memory"
    );
#endif
    return res;
}

// ============================================================================
// 1. Process & Thread Management
// ============================================================================

__declspec(dllexport) void ExitProcess(uint32_t uExitCode) noexcept {
    SyscallDispatch(0x01, 0, uExitCode); // NtTerminateProcess(0, exitCode)
    while (true) {}
}

__declspec(dllexport) void* GetCurrentProcess() noexcept {
    return reinterpret_cast<void*>(-1); // Pseudo handle
}

__declspec(dllexport) void* GetCurrentThread() noexcept {
    return reinterpret_cast<void*>(-2); // Pseudo handle
}

__declspec(dllexport) uint32_t GetCurrentProcessId() noexcept {
#if defined(__x86_64__)
    uint64_t teb = 0;
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (teb) {
        return *reinterpret_cast<const uint32_t*>(teb + 0x40);
    }
#endif
    return 1024;
}

__declspec(dllexport) uint32_t GetCurrentThreadId() noexcept {
#if defined(__x86_64__)
    uint64_t teb = 0;
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (teb) {
        return *reinterpret_cast<const uint32_t*>(teb + 0x48);
    }
#endif
    return 1025;
}

__declspec(dllexport) void Sleep(uint32_t /*dwMilliseconds*/) noexcept {
    SyscallDispatch(0x46); // NtYieldExecution
}

// ============================================================================
// 2. Memory Management
// ============================================================================

__declspec(dllexport) void* VirtualAlloc(void* lpAddress, size_t dwSize, uint32_t /*flAllocationType*/, uint32_t /*flProtect*/) noexcept {
    uint64_t addr = reinterpret_cast<uint64_t>(lpAddress);
    uint64_t res = SyscallDispatch(0x18, addr, dwSize); // NtAllocateVirtualMemory
    return reinterpret_cast<void*>(res);
}

__declspec(dllexport) bool VirtualFree(void* lpAddress, size_t dwSize, uint32_t /*dwFreeType*/) noexcept {
    uint64_t addr = reinterpret_cast<uint64_t>(lpAddress);
    SyscallDispatch(0x1B, addr, dwSize); // NtFreeVirtualMemory
    return true;
}

// ============================================================================
// 3. File & Console I/O
// ============================================================================

__declspec(dllexport) void* GetStdHandle(uint32_t nStdHandle) noexcept {
    // Standard Win32 constants:
    // STD_INPUT_HANDLE  = ((uint32_t)-10) = 0xFFFFFFF6
    // STD_OUTPUT_HANDLE = ((uint32_t)-11) = 0xFFFFFFF5
    // STD_ERROR_HANDLE  = ((uint32_t)-12) = 0xFFFFFFF4
    switch (nStdHandle) {
        case 0xFFFFFFF6: return reinterpret_cast<void*>(0x10); // stdin
        case 0xFFFFFFF5: return reinterpret_cast<void*>(0x14); // stdout
        case 0xFFFFFFF4: return reinterpret_cast<void*>(0x18); // stderr
        default:         return reinterpret_cast<void*>(-1);
    }
}

__declspec(dllexport) void* CreateFileW(
    const wchar_t* /*lpFileName*/,
    uint32_t /*dwDesiredAccess*/,
    uint32_t /*dwShareMode*/,
    void* /*lpSecurityAttributes*/,
    uint32_t /*dwCreationDisposition*/,
    uint32_t /*dwFlagsAndAttributes*/,
    void* /*hTemplateFile*/
) noexcept {
    return reinterpret_cast<void*>(0x20); // Valid handle placeholder
}

__declspec(dllexport) bool ReadFile(
    void* hFile,
    void* lpBuffer,
    uint32_t nNumberOfBytesToRead,
    uint32_t* lpNumberOfBytesRead,
    void* /*lpOverlapped*/
) noexcept {
    uint64_t h = reinterpret_cast<uint64_t>(hFile);
    uint64_t buf = reinterpret_cast<uint64_t>(lpBuffer);
    uint64_t res = SyscallDispatch(0x24, h, buf); // NtReadFile
    if (lpNumberOfBytesRead) {
        *lpNumberOfBytesRead = (res == 0) ? nNumberOfBytesToRead : 0;
    }
    return (res == 0);
}

__declspec(dllexport) bool WriteFile(
    void* hFile,
    const void* lpBuffer,
    uint32_t nNumberOfBytesToWrite,
    uint32_t* lpNumberOfBytesWritten,
    void* /*lpOverlapped*/
) noexcept {
    uint64_t h = reinterpret_cast<uint64_t>(hFile);
    uint64_t buf = reinterpret_cast<uint64_t>(lpBuffer);
    uint64_t res = SyscallDispatch(0x25, h, buf); // NtWriteFile
    if (lpNumberOfBytesWritten) {
        *lpNumberOfBytesWritten = (res == 0) ? nNumberOfBytesToWrite : 0;
    }
    return (res == 0);
}

__declspec(dllexport) bool CloseHandle(void* hObject) noexcept {
    uint64_t h = reinterpret_cast<uint64_t>(hObject);
    SyscallDispatch(0x1E, h, 0); // NtClose
    return true;
}

// ============================================================================
// 4. Module & Environment APIs
// ============================================================================

static const char s_cmdLineA[] = "app.exe";
static const wchar_t s_cmdLineW[] = L"app.exe";

__declspec(dllexport) const char* GetCommandLineA() noexcept {
    return s_cmdLineA;
}

__declspec(dllexport) const wchar_t* GetCommandLineW() noexcept {
    return s_cmdLineW;
}

__declspec(dllexport) void* GetModuleHandleW(const wchar_t* /*lpModuleName*/) noexcept {
    return reinterpret_cast<void*>(0x00007FF600000000ULL); // Canonical imageBase
}

__declspec(dllexport) void* LoadLibraryW(const wchar_t* /*lpLibFileName*/) noexcept {
    return nullptr;
}

__declspec(dllexport) void* GetProcAddress(void* /*hModule*/, const char* /*lpProcName*/) noexcept {
    return nullptr;
}

// ============================================================================
// 5. System Info & Diagnostics
// ============================================================================

struct SystemInfo {
    uint16_t processorArchitecture{9}; // PROCESSOR_ARCHITECTURE_AMD64
    uint16_t reserved{0};
    uint32_t pageSize{4096};
    void*    minimumApplicationAddress{reinterpret_cast<void*>(0x10000)};
    void*    maximumApplicationAddress{reinterpret_cast<void*>(0x7FFFFFFFFFFF)};
    uint64_t activeProcessorMask{0xFF}; // 8 cores/threads
    uint32_t numberOfProcessors{8};
    uint32_t processorType{8664};
    uint32_t allocationGranularity{65536};
    uint16_t processorLevel{6};
    uint16_t processorRevision{0x9E09};
};

__declspec(dllexport) void GetSystemInfo(void* lpSystemInfo) noexcept {
    if (!lpSystemInfo) return;
    auto* si = static_cast<SystemInfo*>(lpSystemInfo);
    *si = SystemInfo{};
}

__declspec(dllexport) uint64_t GetTickCount64() noexcept {
    // Ring 0 KUSER_SHARED_DATA TickCountLowDeprecated at 0x7FFE0320
    const volatile auto* pTick = reinterpret_cast<const volatile uint64_t*>(0x7FFE0320);
    return *pTick;
}

__declspec(dllexport) uint32_t GetLastError() noexcept {
#if defined(__x86_64__)
    uint64_t teb = 0;
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (teb) {
        return *reinterpret_cast<const uint32_t*>(teb + 0x68);
    }
#endif
    return 0;
}

__declspec(dllexport) void SetLastError(uint32_t dwErrCode) noexcept {
#if defined(__x86_64__)
    uint64_t teb = 0;
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (teb) {
        *reinterpret_cast<uint32_t*>(teb + 0x68) = dwErrCode;
    }
#else
    (void)dwErrCode;
#endif
}

} // extern "C"
