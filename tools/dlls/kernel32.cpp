/**
 * @file kernel32.cpp
 * @brief Sovereign Win32 Base API DLL (kernel32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * ZERO direct syscall instructions: authentic Windows NT 2-level DLL chaining
 * where kernel32.dll dynamically imports native Nt* / Rtl* APIs from ntdll.dll
 * via a genuine PE32+ Import Directory Table (IAT):
 *   Win32 App -> kernel32.dll (IAT) -> ntdll.dll (IAT) -> Ring 0 KiSystemCall64
 */

#include <cstdint>
#include <cstddef>

extern "C" {

// ============================================================================
// 0. Dynamic Imports from C:\Windows\System32\ntdll.dll (Real PE32+ IAT)
// ============================================================================

struct UnicodeString {
    uint16_t length;
    uint16_t maximumLength;
    wchar_t* buffer;
};

struct ObjectAttributes {
    uint32_t       length;
    void*          rootDirectory;
    UnicodeString* objectName;
    uint32_t       attributes;
    void*          securityDescriptor;
    void*          securityQualityOfService;
};

struct IoStatusBlock {
    uint64_t status;
    uint64_t information;
};

__declspec(dllimport) void     RtlInitUnicodeString(UnicodeString* dest, const wchar_t* source) noexcept;
__declspec(dllimport) uint64_t NtTerminateProcess(void* processHandle, uint64_t exitStatus) noexcept;
__declspec(dllimport) uint64_t NtYieldExecution() noexcept;
__declspec(dllimport) uint64_t NtAllocateVirtualMemory(
    void* processHandle,
    void** baseAddress,
    uint64_t zeroBits,
    size_t* regionSize,
    uint32_t allocationType,
    uint32_t protect
) noexcept;
__declspec(dllimport) uint64_t NtFreeVirtualMemory(
    void* processHandle,
    void** baseAddress,
    size_t* regionSize,
    uint32_t freeType
) noexcept;
__declspec(dllimport) uint64_t NtCreateFile(
    void** fileHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    IoStatusBlock* ioStatusBlock,
    void* allocationSize,
    uint32_t fileAttributes,
    uint32_t shareAccess,
    uint32_t createDisposition,
    uint32_t createOptions,
    void* eaBuffer,
    uint32_t eaLength
) noexcept;
__declspec(dllimport) uint64_t NtReadFile(
    void* fileHandle,
    void* event,
    void* apcRoutine,
    void* apcContext,
    IoStatusBlock* ioStatusBlock,
    void* buffer,
    uint32_t length,
    void* byteOffset,
    uint32_t* key
) noexcept;
__declspec(dllimport) uint64_t NtWriteFile(
    void* fileHandle,
    void* event,
    void* apcRoutine,
    void* apcContext,
    IoStatusBlock* ioStatusBlock,
    const void* buffer,
    uint32_t length,
    void* byteOffset,
    uint32_t* key
) noexcept;
__declspec(dllimport) uint64_t NtClose(void* handle) noexcept;

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

static inline bool NtSucceeded(uint64_t status) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(status)) >= 0;
}

// ============================================================================
// 1. Process & Thread Management
// ============================================================================

__declspec(dllexport) void* GetCurrentProcess() noexcept {
    return reinterpret_cast<void*>(-1);
}

__declspec(dllexport) void* GetCurrentThread() noexcept {
    return reinterpret_cast<void*>(-2);
}

__declspec(dllexport) void ExitProcess(uint32_t uExitCode) noexcept {
    NtTerminateProcess(GetCurrentProcess(), uExitCode);
    while (true) {}
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
    NtYieldExecution();
}

// ============================================================================
// 2. Memory Management (Routed through ntdll.dll)
// ============================================================================

__declspec(dllexport) void* VirtualAlloc(
    void* lpAddress,
    size_t dwSize,
    uint32_t flAllocationType,
    uint32_t flProtect
) noexcept {
    void* base = lpAddress;
    size_t regionSize = dwSize ? dwSize : 4096;
    uint64_t st = NtAllocateVirtualMemory(
        GetCurrentProcess(),
        &base,
        0,
        &regionSize,
        flAllocationType,
        flProtect
    );
    return NtSucceeded(st) ? base : nullptr;
}

__declspec(dllexport) int32_t VirtualFree(
    void* lpAddress,
    size_t dwSize,
    uint32_t dwFreeType
) noexcept {
    void* base = lpAddress;
    size_t regionSize = dwSize;
    uint64_t st = NtFreeVirtualMemory(
        GetCurrentProcess(),
        &base,
        &regionSize,
        dwFreeType
    );
    return NtSucceeded(st) ? 1 : 0;
}

// ============================================================================
// 3. File & Console I/O (Routed through ntdll.dll -> Ring 0 NTFS 3.1)
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
    uint32_t dwShareMode,
    void* /*lpSecurityAttributes*/,
    uint32_t dwCreationDisposition,
    uint32_t dwFlagsAndAttributes,
    void* /*hTemplateFile*/
) noexcept {
    if (!lpFileName) return reinterpret_cast<void*>(-1);

    UnicodeString uPath{0, 0, nullptr};
    RtlInitUnicodeString(&uPath, lpFileName);

    ObjectAttributes oa{};
    oa.length = sizeof(ObjectAttributes);
    oa.rootDirectory = nullptr;
    oa.objectName = &uPath;
    oa.attributes = 0x00000040u; // OBJ_CASE_INSENSITIVE
    oa.securityDescriptor = nullptr;
    oa.securityQualityOfService = nullptr;

    IoStatusBlock iosb{0, 0};
    void* hFile = nullptr;
    uint64_t st = NtCreateFile(
        &hFile,
        dwDesiredAccess,
        &oa,
        &iosb,
        nullptr,
        dwFlagsAndAttributes,
        dwShareMode,
        dwCreationDisposition,
        0,
        nullptr,
        0
    );
    if (!NtSucceeded(st) || hFile == nullptr) {
        return reinterpret_cast<void*>(-1); // INVALID_HANDLE_VALUE
    }
    return hFile;
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
    IoStatusBlock iosb{0, 0};
    uint64_t st = NtReadFile(
        hFile,
        nullptr,
        nullptr,
        nullptr,
        &iosb,
        lpBuffer,
        nNumberOfBytesToRead,
        nullptr,
        nullptr
    );
    if (!NtSucceeded(st)) {
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
        return 0;
    }
    if (lpNumberOfBytesRead) {
        *lpNumberOfBytesRead = static_cast<uint32_t>(iosb.information);
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
    IoStatusBlock iosb{0, 0};
    uint64_t st = NtWriteFile(
        hFile,
        nullptr,
        nullptr,
        nullptr,
        &iosb,
        lpBuffer,
        nNumberOfBytesToWrite,
        nullptr,
        nullptr
    );
    if (!NtSucceeded(st)) {
        if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = 0;
        return 0;
    }
    if (lpNumberOfBytesWritten) {
        *lpNumberOfBytesWritten = static_cast<uint32_t>(iosb.information);
    }
    return 1;
}

__declspec(dllexport) int32_t CloseHandle(void* hObject) noexcept {
    uint64_t st = NtClose(hObject);
    return NtSucceeded(st) ? 1 : 0;
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
