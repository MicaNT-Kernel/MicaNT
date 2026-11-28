/**
 * @file kernel32.cpp
 * @brief Sovereign Win32 Base API DLL (kernel32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * ZERO direct syscall instructions: authentic Windows NT 2-level DLL chaining
 * where kernel32.dll dynamically imports native Nt* / Rtl* / Ldr* APIs from ntdll.dll
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

struct NtDirEntryInfo {
    uint64_t fileSize;
    uint32_t fileAttributes;
    uint32_t nameLengthChars;
    uint16_t fileName[128];
};

struct Win32FindDataW {
    uint32_t dwFileAttributes;
    uint64_t ftCreationTime;
    uint64_t ftLastAccessTime;
    uint64_t ftLastWriteTime;
    uint32_t nFileSizeHigh;
    uint32_t nFileSizeLow;
    uint32_t dwReserved0;
    uint32_t dwReserved1;
    wchar_t  cFileName[260];
    wchar_t  cAlternateFileName[14];
};

__declspec(dllimport) void     RtlInitUnicodeString(UnicodeString* dest, const wchar_t* source) noexcept;
__declspec(dllimport) uint64_t NtTerminateProcess(void* processHandle, uint64_t exitStatus) noexcept;
__declspec(dllimport) uint64_t NtYieldExecution() noexcept;
__declspec(dllimport) uint64_t NtWaitForSingleObject(void* handle, bool alertable, uint64_t* timeoutMs) noexcept;
__declspec(dllimport) uint64_t NtQueryInformationProcess(
    void* processHandle, uint32_t processInfoClass, void* processInfoBuf, uint32_t processInfoLen, uint32_t* returnLength
) noexcept;
__declspec(dllimport) uint64_t NtQueryDirectoryFile(
    void* fileHandle, IoStatusBlock* ioStatusBlock, void* fileInformation, uint32_t length
) noexcept;
__declspec(dllimport) uint64_t NtAllocateVirtualMemory(
    void* processHandle, void** baseAddress, uint64_t zeroBits, size_t* regionSize, uint32_t allocationType, uint32_t protect
) noexcept;
__declspec(dllimport) uint64_t NtFreeVirtualMemory(
    void* processHandle, void** baseAddress, size_t* regionSize, uint32_t freeType
) noexcept;
__declspec(dllimport) uint64_t NtCreateFile(
    void** fileHandle, uint32_t desiredAccess, ObjectAttributes* objectAttributes, IoStatusBlock* ioStatusBlock,
    void* allocationSize, uint32_t fileAttributes, uint32_t shareAccess, uint32_t createDisposition,
    uint32_t createOptions, void* eaBuffer, uint32_t eaLength
) noexcept;
__declspec(dllimport) uint64_t NtReadFile(
    void* fileHandle, void* event, void* apcRoutine, void* apcContext, IoStatusBlock* ioStatusBlock,
    void* buffer, uint32_t length, void* byteOffset, uint32_t* key
) noexcept;
__declspec(dllimport) uint64_t NtWriteFile(
    void* fileHandle, void* event, void* apcRoutine, void* apcContext, IoStatusBlock* ioStatusBlock,
    const void* buffer, uint32_t length, void* byteOffset, uint32_t* key
) noexcept;
__declspec(dllimport) uint64_t NtClose(void* handle) noexcept;
__declspec(dllimport) uint64_t NtDeleteFile(ObjectAttributes* objectAttributes) noexcept;
__declspec(dllimport) uint64_t NtCreateUserProcess(
    void** processHandle, void** threadHandle, ObjectAttributes* imagePathAttr, uint64_t* outPid
) noexcept;
__declspec(dllimport) uint64_t LdrLoadDll(
    const wchar_t* pathToFile, uint32_t flags, UnicodeString* moduleFileName, void** outModuleHandle
) noexcept;
__declspec(dllimport) uint64_t LdrGetDllHandle(
    const wchar_t* dllPath, uint32_t* dllCharacteristics, UnicodeString* dllName, void** outModuleHandle
) noexcept;
__declspec(dllimport) uint64_t LdrGetProcedureAddress(
    void* moduleBase, const char* procedureName, uint32_t ordinal, void** outProcedureAddress
) noexcept;
__declspec(dllimport) uint64_t NtCreateThreadEx(
    void** threadHandle, uint64_t startAddress, uint64_t parameter, uint64_t exitThunkAddress,
    uint32_t preferredCore, uint32_t* outThreadId
) noexcept;
__declspec(dllimport) uint32_t NtGetCurrentProcessorNumber() noexcept;
__declspec(dllimport) uint64_t NtTerminateThread(void* threadHandle, uint64_t exitStatus) noexcept;
__declspec(dllimport) uint64_t NtQueryInformationThread(
    void* threadHandle, uint32_t threadInfoClass, void* threadInfoBuf, uint32_t threadInfoLen, uint32_t* returnLength
) noexcept;
__declspec(dllimport) uint64_t NtQuerySystemInformation(
    uint32_t infoClass, void* infoBuffer, uint32_t length, uint32_t* returnLength
) noexcept;

struct ProcessInformation {
    void*    hProcess;
    void*    hThread;
    uint32_t dwProcessId;
    uint32_t dwThreadId;
};

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

static inline bool NtSucceeded(uint64_t status) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(status)) >= 0;
}

// ============================================================================
// 1. Process, Thread & Synchronization Management
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

__declspec(dllexport) int32_t TerminateProcess(void* hProcess, uint32_t uExitCode) noexcept {
    if (!hProcess) return 0;
    return NtSucceeded(NtTerminateProcess(hProcess, uExitCode)) ? 1 : 0;
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

__declspec(dllexport) uint32_t WaitForSingleObject(void* hHandle, uint32_t dwMilliseconds) noexcept {
    uint64_t timeout = dwMilliseconds;
    uint64_t st = NtWaitForSingleObject(hHandle, false, &timeout);
    return NtSucceeded(st) ? 0u : 0xFFFFFFFFu; // WAIT_OBJECT_0 or WAIT_FAILED
}

__declspec(dllexport) int32_t GetExitCodeProcess(void* hProcess, uint32_t* lpExitCode) noexcept {
    if (!lpExitCode) return 0;
    uint32_t code = 0;
    uint64_t st = NtQueryInformationProcess(hProcess, 0, &code, sizeof(code), nullptr);
    if (!NtSucceeded(st)) return 0;
    *lpExitCode = code;
    return 1;
}

__declspec(dllexport) void ExitThread(uint32_t dwExitCode) noexcept {
    NtTerminateThread(GetCurrentThread(), dwExitCode);
    while (true) {}
}

// Authentic Windows NT BaseThreadInitThunk equivalent (runs in Ring 3 on the target hardware core!)
static void BaseThreadStartThunk(void* lpParameter, uint32_t (*lpStartAddress)(void*)) noexcept {
    uint32_t exitCode = lpStartAddress ? lpStartAddress(lpParameter) : 0u;
    ExitThread(exitCode);
}

__declspec(dllexport) void* CreateThread(
    void* /*lpThreadAttributes*/,
    size_t /*dwStackSize*/,
    uint32_t (*lpStartAddress)(void*),
    void* lpParameter,
    uint32_t /*dwCreationFlags*/,
    uint32_t* lpThreadId
) noexcept {
    if (!lpStartAddress) return nullptr;
    void* hThread = nullptr;
    uint32_t tid = 0;
    uint64_t st = NtCreateThreadEx(
        &hThread,
        reinterpret_cast<uint64_t>(lpStartAddress),
        reinterpret_cast<uint64_t>(lpParameter),
        reinterpret_cast<uint64_t>(&BaseThreadStartThunk),
        0,
        &tid
    );
    if (!NtSucceeded(st) || hThread == nullptr) {
        if (lpThreadId) *lpThreadId = 0;
        return nullptr;
    }
    if (lpThreadId) *lpThreadId = tid;
    return hThread;
}

__declspec(dllexport) int32_t GetExitCodeThread(void* hThread, uint32_t* lpExitCode) noexcept {
    if (!lpExitCode || !hThread) return 0;
    uint32_t code = 0;
    uint64_t st = NtQueryInformationThread(hThread, 0, &code, sizeof(code), nullptr);
    if (!NtSucceeded(st)) return 0;
    *lpExitCode = code;
    return 1;
}

__declspec(dllexport) uint32_t GetCurrentProcessorNumber() noexcept {
    return NtGetCurrentProcessorNumber();
}

__declspec(dllexport) int32_t QuerySystemProcessesMica(void* outBuf, uint32_t bufLen) noexcept {
    if (!outBuf || bufLen < 64) return 0;
    return NtSucceeded(NtQuerySystemInformation(5u, outBuf, bufLen, nullptr)) ? 1 : 0;
}

__declspec(dllexport) int32_t CreateProcessW(
    const wchar_t* lpApplicationName,
    wchar_t* lpCommandLine,
    void* /*lpProcessAttributes*/,
    void* /*lpThreadAttributes*/,
    int32_t /*bInheritHandles*/,
    uint32_t /*dwCreationFlags*/,
    void* /*lpEnvironment*/,
    const wchar_t* /*lpCurrentDirectory*/,
    void* /*lpStartupInfo*/,
    ProcessInformation* lpProcessInformation
) noexcept {
    if (!lpApplicationName) return 0;

    UnicodeString uPath{0, 0, nullptr};
    RtlInitUnicodeString(&uPath, lpApplicationName);

    ObjectAttributes oa{};
    oa.length = sizeof(ObjectAttributes);
    oa.rootDirectory = reinterpret_cast<void*>(lpCommandLine);
    oa.objectName = &uPath;
    oa.attributes = 0x00000040u;

    void* hProc = nullptr;
    void* hThread = nullptr;
    uint64_t childPid = 0;
    uint64_t st = NtCreateUserProcess(&hProc, &hThread, &oa, &childPid);
    if (!NtSucceeded(st) || childPid == 0) {
        return 0;
    }
    if (lpProcessInformation) {
        lpProcessInformation->hProcess = hProc;
        lpProcessInformation->hThread = hThread;
        lpProcessInformation->dwProcessId = static_cast<uint32_t>(childPid);
        lpProcessInformation->dwThreadId = static_cast<uint32_t>(childPid + 100);
    }
    return 1;
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
        GetCurrentProcess(), &base, 0, &regionSize, flAllocationType, flProtect
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
    uint64_t st = NtFreeVirtualMemory(GetCurrentProcess(), &base, &regionSize, dwFreeType);
    return NtSucceeded(st) ? 1 : 0;
}

// ============================================================================
// 3. File, Console & Directory Enumeration I/O (Routed through ntdll.dll -> NTFS 3.1)
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
    oa.attributes = 0x00000040u;

    IoStatusBlock iosb{0, 0};
    void* hFile = nullptr;
    uint64_t st = NtCreateFile(
        &hFile, dwDesiredAccess, &oa, &iosb, nullptr, dwFlagsAndAttributes,
        dwShareMode, dwCreationDisposition, 0, nullptr, 0
    );
    if (!NtSucceeded(st) || hFile == nullptr) {
        return reinterpret_cast<void*>(-1);
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
    uint64_t st = NtReadFile(hFile, nullptr, nullptr, nullptr, &iosb, lpBuffer, nNumberOfBytesToRead, nullptr, nullptr);
    if (!NtSucceeded(st)) {
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
        return 0;
    }
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = static_cast<uint32_t>(iosb.information);
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
    uint64_t st = NtWriteFile(hFile, nullptr, nullptr, nullptr, &iosb, lpBuffer, nNumberOfBytesToWrite, nullptr, nullptr);
    if (!NtSucceeded(st)) {
        if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = 0;
        return 0;
    }
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = static_cast<uint32_t>(iosb.information);
    return 1;
}

__declspec(dllexport) int32_t CloseHandle(void* hObject) noexcept {
    uint64_t st = NtClose(hObject);
    return NtSucceeded(st) ? 1 : 0;
}

__declspec(dllexport) int32_t CreateDirectoryW(
    const wchar_t* lpPathName,
    void* /*lpSecurityAttributes*/
) noexcept {
    if (!lpPathName) return 0;
    UnicodeString uPath{0, 0, nullptr};
    RtlInitUnicodeString(&uPath, lpPathName);

    ObjectAttributes oa{};
    oa.length = sizeof(ObjectAttributes);
    oa.rootDirectory = nullptr;
    oa.objectName = &uPath;
    oa.attributes = 0x00000040u;

    IoStatusBlock iosb{0, 0};
    void* hDir = nullptr;
    // FILE_CREATE = 2, FILE_DIRECTORY_FILE = 0x00000001
    uint64_t st = NtCreateFile(
        &hDir, 0x00100001u, &oa, &iosb, nullptr, 0x00000010u,
        3u, 2u, 0x00000001u, nullptr, 0
    );
    if (!NtSucceeded(st) || hDir == nullptr) return 0;
    NtClose(hDir);
    return 1;
}

__declspec(dllexport) int32_t CreateDirectoryA(const char* lpPathName, void* lpSecurityAttributes) noexcept {
    if (!lpPathName) return 0;
    wchar_t wideBuf[256];
    size_t i = 0;
    while (lpPathName[i] && i + 1 < 256) {
        wideBuf[i] = static_cast<wchar_t>(static_cast<unsigned char>(lpPathName[i]));
        ++i;
    }
    wideBuf[i] = L'\0';
    return CreateDirectoryW(wideBuf, lpSecurityAttributes);
}

__declspec(dllexport) int32_t DeleteFileW(const wchar_t* lpFileName) noexcept {
    if (!lpFileName) return 0;
    UnicodeString uPath{0, 0, nullptr};
    RtlInitUnicodeString(&uPath, lpFileName);

    ObjectAttributes oa{};
    oa.length = sizeof(ObjectAttributes);
    oa.rootDirectory = nullptr;
    oa.objectName = &uPath;
    oa.attributes = 0x00000040u;

    uint64_t st = NtDeleteFile(&oa);
    return NtSucceeded(st) ? 1 : 0;
}

__declspec(dllexport) int32_t DeleteFileA(const char* lpFileName) noexcept {
    if (!lpFileName) return 0;
    wchar_t wideBuf[256];
    size_t i = 0;
    while (lpFileName[i] && i + 1 < 256) {
        wideBuf[i] = static_cast<wchar_t>(static_cast<unsigned char>(lpFileName[i]));
        ++i;
    }
    wideBuf[i] = L'\0';
    return DeleteFileW(wideBuf);
}

static void populateFindData(Win32FindDataW* dst, const NtDirEntryInfo& src) noexcept {
    dst->dwFileAttributes = src.fileAttributes;
    dst->ftCreationTime = 0;
    dst->ftLastAccessTime = 0;
    dst->ftLastWriteTime = 0;
    dst->nFileSizeHigh = static_cast<uint32_t>(src.fileSize >> 32);
    dst->nFileSizeLow  = static_cast<uint32_t>(src.fileSize & 0xFFFFFFFFu);
    dst->dwReserved0 = 0;
    dst->dwReserved1 = 0;
    size_t n = (src.nameLengthChars < 127) ? src.nameLengthChars : 127;
    for (size_t i = 0; i < n; ++i) {
        dst->cFileName[i] = static_cast<wchar_t>(src.fileName[i]);
    }
    dst->cFileName[n] = L'\0';
    dst->cAlternateFileName[0] = L'\0';
}

__declspec(dllexport) int32_t FindNextFileW(void* hFindFile, Win32FindDataW* lpFindFileData) noexcept {
    if (!lpFindFileData || hFindFile == reinterpret_cast<void*>(-1) || hFindFile == nullptr) return 0;
    NtDirEntryInfo info{};
    IoStatusBlock iosb{0, 0};
    uint64_t st = NtQueryDirectoryFile(hFindFile, &iosb, &info, sizeof(info));
    if (!NtSucceeded(st)) return 0;
    populateFindData(lpFindFileData, info);
    return 1;
}

__declspec(dllexport) void* FindFirstFileW(const wchar_t* lpFileName, Win32FindDataW* lpFindFileData) noexcept {
    if (!lpFileName || !lpFindFileData) return reinterpret_cast<void*>(-1);

    // Strip trailing wildcard ("\*", "\*.*", or "*") to resolve parent NTFS directory path
    wchar_t dirBuf[256];
    size_t len = 0;
    while (lpFileName[len] && len + 1 < 256) {
        dirBuf[len] = lpFileName[len];
        ++len;
    }
    dirBuf[len] = L'\0';

    while (len > 0 && (dirBuf[len - 1] == L'*' || dirBuf[len - 1] == L'.' || dirBuf[len - 1] == L'?')) {
        dirBuf[--len] = L'\0';
    }
    if (len > 1 && (dirBuf[len - 1] == L'\\' || dirBuf[len - 1] == L'/')) {
        // Keep root "\" or "C:\" intact
        if (!(len == 3 && dirBuf[1] == L':')) {
            dirBuf[--len] = L'\0';
        }
    }
    if (len == 0) {
        dirBuf[0] = L'\\';
        dirBuf[1] = L'\0';
    }

    void* hDir = CreateFileW(dirBuf, 0x80000000u, 1u, nullptr, 3u, 0x02000000u, nullptr);
    if (hDir == reinterpret_cast<void*>(-1) || hDir == nullptr) {
        return reinterpret_cast<void*>(-1);
    }
    if (!FindNextFileW(hDir, lpFindFileData)) {
        CloseHandle(hDir);
        return reinterpret_cast<void*>(-1);
    }
    return hDir;
}

__declspec(dllexport) int32_t FindClose(void* hFindFile) noexcept {
    return CloseHandle(hFindFile);
}

// ============================================================================
// 4. Dynamic Runtime Loader & Module APIs (Routed through ntdll!Ldr*)
// ============================================================================

static const char s_cmdLineA[] = "micant_app.exe";
static const wchar_t s_cmdLineW[] = L"micant_app.exe";

__declspec(dllexport) const char* GetCommandLineA() noexcept {
#if defined(__x86_64__)
    uint64_t peb = 0;
    __asm__ __volatile__("movq %%gs:0x60, %0" : "=r"(peb));
    if (peb) {
        const char* p = reinterpret_cast<const char*>(peb + 0x1000ULL);
        if (p[0] != '\0') return p;
    }
#endif
    return s_cmdLineA;
}

__declspec(dllexport) const wchar_t* GetCommandLineW() noexcept {
#if defined(__x86_64__)
    uint64_t peb = 0;
    __asm__ __volatile__("movq %%gs:0x60, %0" : "=r"(peb));
    if (peb) {
        const wchar_t* p = reinterpret_cast<const wchar_t*>(peb + 0x1400ULL);
        if (p[0] != L'\0') return p;
    }
#endif
    return s_cmdLineW;
}

__declspec(dllexport) void* GetModuleHandleW(const wchar_t* lpModuleName) noexcept {
    if (!lpModuleName) {
        return reinterpret_cast<void*>(0x0000008000000000ULL);
    }
    UnicodeString uName{0, 0, nullptr};
    RtlInitUnicodeString(&uName, lpModuleName);
    void* hMod = nullptr;
    uint64_t st = LdrGetDllHandle(nullptr, nullptr, &uName, &hMod);
    return NtSucceeded(st) ? hMod : nullptr;
}

__declspec(dllexport) void* LoadLibraryW(const wchar_t* lpLibFileName) noexcept {
    if (!lpLibFileName) return nullptr;
    UnicodeString uName{0, 0, nullptr};
    RtlInitUnicodeString(&uName, lpLibFileName);
    void* hMod = nullptr;
    uint64_t st = LdrLoadDll(nullptr, 0, &uName, &hMod);
    return NtSucceeded(st) ? hMod : nullptr;
}

__declspec(dllexport) void* LoadLibraryA(const char* lpLibFileName) noexcept {
    if (!lpLibFileName) return nullptr;
    wchar_t wideBuf[128];
    size_t i = 0;
    while (lpLibFileName[i] && i + 1 < 128) {
        wideBuf[i] = static_cast<wchar_t>(static_cast<unsigned char>(lpLibFileName[i]));
        ++i;
    }
    wideBuf[i] = L'\0';
    return LoadLibraryW(wideBuf);
}

__declspec(dllexport) void* GetProcAddress(void* hModule, const char* lpProcName) noexcept {
    if (!hModule || !lpProcName) return nullptr;
    void* procAddr = nullptr;
    uint64_t st = LdrGetProcedureAddress(hModule, lpProcName, 0, &procAddr);
    return NtSucceeded(st) ? procAddr : nullptr;
}

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
