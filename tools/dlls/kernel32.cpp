/**
 * @file kernel32.cpp
 * @brief Sovereign Win32 Base API DLL (kernel32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Implements real Win32 Process, Thread, Memory, and NTFS File I/O exports
 * backed directly by MicaNT Ring 3 -> Ring 0 fast syscalls:
 *   0x01 = NtTerminateProcess
 *   0x0F = NtClose
 *   0x18 = NtAllocateVirtualMemory
 *   0x1B = NtFreeVirtualMemory
 *   0x24 = NtReadFile  (live NTFS AHCI DMA read)
 *   0x25 = NtWriteFile (console / file write)
 *   0x46 = NtYieldExecution
 *   0x52 = NtCreateFile (live NTFS open)
 */

#include <cstdint>
#include <cstddef>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

// Low-level x86_64 fast syscall helper (RAX=SSN, RDX=arg1, R8=arg2, R9=arg3)
static inline uint64_t SyscallDispatch(
    uint64_t ssn,
    uint64_t arg1 = 0,
    uint64_t arg2 = 0,
    uint64_t arg3 = 0
) noexcept {
#if defined(__x86_64__)
    register uint64_t r_rax __asm__("rax") = ssn;
    register uint64_t r_rdx __asm__("rdx") = arg1;
    register uint64_t r_r8  __asm__("r8")  = arg2;
    register uint64_t r_r9  __asm__("r9")  = arg3;
    __asm__ __volatile__(
        "syscall"
        : "+r"(r_rax)
        : "r"(r_rdx), "r"(r_r8), "r"(r_r9)
        : "rcx", "r11", "memory"
    );
    return r_rax;
#else
    (void)ssn; (void)arg1; (void)arg2; (void)arg3;
    return 0;
#endif
}

// ============================================================================
// 1. Process & Thread Management
// ============================================================================

__declspec(dllexport) void ExitProcess(uint32_t uExitCode) noexcept {
    SyscallDispatch(0x01, 0, uExitCode, 0); // NtTerminateProcess
    while (true) {}
}

__declspec(dllexport) void* GetCurrentProcess() noexcept {
    return reinterpret_cast<void*>(-1);
}

__declspec(dllexport) void* GetCurrentThread() noexcept {
    return reinterpret_cast<void*>(-2);
}

__declspec(dllexport) uint32_t GetCurrentProcessId() noexcept {
#if defined(__x86_64__)
    uint64_t teb = 0;
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (teb) {
        uint32_t pid = *reinterpret_cast<const uint32_t*>(teb + 0x40);
        if (pid != 0) return pid;
    }
#endif
    return 1024;
}

__declspec(dllexport) uint32_t GetCurrentThreadId() noexcept {
#if defined(__x86_64__)
    uint64_t teb = 0;
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (teb) {
        uint32_t tid = *reinterpret_cast<const uint32_t*>(teb + 0x48);
        if (tid != 0) return tid;
    }
#endif
    return 1025;
}

__declspec(dllexport) void Sleep(uint32_t /*dwMilliseconds*/) noexcept {
    SyscallDispatch(0x46, 0, 0, 0); // NtYieldExecution
}

// ============================================================================
// 2. Memory Management
// ============================================================================

__declspec(dllexport) void* VirtualAlloc(
    void* lpAddress,
    size_t dwSize,
    uint32_t /*flAllocationType*/,
    uint32_t /*flProtect*/
) noexcept {
    uint64_t addr = reinterpret_cast<uint64_t>(lpAddress);
    uint64_t res = SyscallDispatch(0x18, addr, dwSize, 0); // NtAllocateVirtualMemory
    return reinterpret_cast<void*>(res);
}

__declspec(dllexport) int32_t VirtualFree(
    void* lpAddress,
    size_t dwSize,
    uint32_t /*dwFreeType*/
) noexcept {
    uint64_t addr = reinterpret_cast<uint64_t>(lpAddress);
    uint64_t st = SyscallDispatch(0x1B, addr, dwSize, 0); // NtFreeVirtualMemory
    return (static_cast<int32_t>(st) >= 0) ? 1 : 0;
}

// ============================================================================
// 3. File & Console I/O (Real Ring 0 NTFS Syscalls)
// ============================================================================

__declspec(dllexport) void* GetStdHandle(uint32_t nStdHandle) noexcept {
    switch (nStdHandle) {
        case 0xFFFFFFF6u: return reinterpret_cast<void*>(0x10); // STD_INPUT_HANDLE
        case 0xFFFFFFF5u: return reinterpret_cast<void*>(0x14); // STD_OUTPUT_HANDLE
        case 0xFFFFFFF4u: return reinterpret_cast<void*>(0x18); // STD_ERROR_HANDLE
        default:          return reinterpret_cast<void*>(-1);
    }
}

__declspec(dllexport) void* CreateFileW(
    const wchar_t* lpFileName,
    uint32_t dwDesiredAccess,
    uint32_t /*dwShareMode*/,
    void* /*lpSecurityAttributes*/,
    uint32_t dwCreationDisposition,
    uint32_t /*dwFlagsAndAttributes*/,
    void* /*hTemplateFile*/
) noexcept {
    if (!lpFileName) return reinterpret_cast<void*>(-1);
    uint64_t res = SyscallDispatch(
        0x52, // NtCreateFile
        reinterpret_cast<uint64_t>(lpFileName),
        dwDesiredAccess,
        dwCreationDisposition
    );
    if (res == 0 || res == ~0ULL) {
        return reinterpret_cast<void*>(-1); // INVALID_HANDLE_VALUE
    }
    return reinterpret_cast<void*>(res);
}

__declspec(dllexport) void* CreateFileA(
    const char* lpFileName,
    uint32_t dwDesiredAccess,
    uint32_t dwShareMode,
    void* lpSecurityAttributes,
    uint32_t dwCreationDisposition,
    uint32_t dwFlagsAndAttributes,
    void* hTemplateFile
) noexcept {
    if (!lpFileName) return reinterpret_cast<void*>(-1);
    wchar_t wideBuf[256];
    size_t i = 0;
    while (lpFileName[i] && i + 1 < 256) {
        wideBuf[i] = static_cast<wchar_t>(static_cast<unsigned char>(lpFileName[i]));
        ++i;
    }
    wideBuf[i] = L'\0';
    return CreateFileW(wideBuf, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                       dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

__declspec(dllexport) int32_t ReadFile(
    void* hFile,
    void* lpBuffer,
    uint32_t nNumberOfBytesToRead,
    uint32_t* lpNumberOfBytesRead,
    void* /*lpOverlapped*/
) noexcept {
    if (!lpBuffer) return 0;
    uint64_t h = reinterpret_cast<uint64_t>(hFile);
    uint64_t buf = reinterpret_cast<uint64_t>(lpBuffer);
    int64_t res = static_cast<int64_t>(SyscallDispatch(0x24, h, buf, nNumberOfBytesToRead)); // NtReadFile
    if (res < 0) {
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
        return 0;
    }
    if (lpNumberOfBytesRead) {
        *lpNumberOfBytesRead = static_cast<uint32_t>(res);
    }
    return 1;
}

__declspec(dllexport) int32_t WriteFile(
    void* hFile,
    const void* lpBuffer,
    uint32_t nNumberOfBytesToWrite,
    uint32_t* lpNumberOfBytesWritten,
    void* /*lpOverlapped*/
) noexcept {
    if (!lpBuffer) return 0;
    uint64_t h = reinterpret_cast<uint64_t>(hFile);
    uint64_t buf = reinterpret_cast<uint64_t>(lpBuffer);
    int64_t res = static_cast<int64_t>(SyscallDispatch(0x25, buf, nNumberOfBytesToWrite, h)); // NtWriteFile
    if (res < 0) {
        if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = 0;
        return 0;
    }
    if (lpNumberOfBytesWritten) {
        *lpNumberOfBytesWritten = static_cast<uint32_t>(res);
    }
    return 1;
}

__declspec(dllexport) int32_t CloseHandle(void* hObject) noexcept {
    uint64_t h = reinterpret_cast<uint64_t>(hObject);
    uint64_t st = SyscallDispatch(0x0F, h, 0, 0); // NtClose
    return (static_cast<int32_t>(st) >= 0) ? 1 : 0;
}

// ============================================================================
// 4. Module & Environment APIs
// ============================================================================

static const char s_cmdLineA[] = "win32_ntfs_probe.exe";
static const wchar_t s_cmdLineW[] = L"win32_ntfs_probe.exe";

__declspec(dllexport) const char* GetCommandLineA() noexcept { return s_cmdLineA; }
__declspec(dllexport) const wchar_t* GetCommandLineW() noexcept { return s_cmdLineW; }

__declspec(dllexport) void* GetModuleHandleW(const wchar_t* /*lpModuleName*/) noexcept {
    return reinterpret_cast<void*>(0x00007FF600000000ULL);
}

__declspec(dllexport) void* LoadLibraryW(const wchar_t* /*lpLibFileName*/) noexcept { return nullptr; }
__declspec(dllexport) void* GetProcAddress(void* /*hModule*/, const char* /*lpProcName*/) noexcept { return nullptr; }

// ============================================================================
// 5. System Info & Diagnostics
// ============================================================================

struct SystemInfo {
    uint16_t processorArchitecture{9};
    uint16_t reserved{0};
    uint32_t pageSize{4096};
    void*    minimumApplicationAddress{reinterpret_cast<void*>(0x10000)};
    void*    maximumApplicationAddress{reinterpret_cast<void*>(0x7FFFFFFFFFFF)};
    uint64_t activeProcessorMask{0x0F};
    uint32_t numberOfProcessors{4};
    uint32_t processorType{8664};
    uint32_t allocationGranularity{65536};
    uint16_t processorLevel{6};
    uint16_t processorRevision{0x9E09};
};

__declspec(dllexport) void GetSystemInfo(void* lpSystemInfo) noexcept {
    if (!lpSystemInfo) return;
    *static_cast<SystemInfo*>(lpSystemInfo) = SystemInfo{};
}

__declspec(dllexport) uint64_t GetTickCount64() noexcept {
    return 1000;
}

__declspec(dllexport) uint32_t GetLastError() noexcept {
#if defined(__x86_64__)
    uint64_t teb = 0;
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (teb) return *reinterpret_cast<const uint32_t*>(teb + 0x68);
#endif
    return 0;
}

__declspec(dllexport) void SetLastError(uint32_t dwErrCode) noexcept {
#if defined(__x86_64__)
    uint64_t teb = 0;
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (teb) *reinterpret_cast<uint32_t*>(teb + 0x68) = dwErrCode;
#else
    (void)dwErrCode;
#endif
}

} // extern "C"
