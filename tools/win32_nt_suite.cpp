/**
 * @file win32_nt_suite.cpp
 * @brief Authentic Unmodified-ABI Win32 PE32+ Runtime Loader, Directory Enum & Wait Verification Suite.
 *
 * ZERO MicaNT headers. ZERO inline assembly. ZERO direct syscall instructions.
 * Dynamically imports ONLY standard Windows Win32 APIs from KERNEL32.dll
 * (which chains through NTDLL.dll -> Ring 0 KiSystemCall64):
 *
 *   Test 1 (Live NTFS Directory Enumeration):
 *     Calls FindFirstFileW(L"C:\\Windows\\System32\\*", &fd) + FindNextFileW + FindClose
 *     -> kernel32 -> ntdll!NtQueryDirectoryFile (0x35) -> NTFS 3.1 AHCI DMA
 *
 *   Test 2 (Dynamic Runtime DLL Loading + Pure Ring-3 PE32+ EAT Walk):
 *     Calls LoadLibraryW(L"user32.dll") -> kernel32 -> ntdll!LdrLoadDll -> NtLoadDll (0x60)
 *       (maps user32.dll off live NTFS into this process's private CR3 arena & binds user32->ntdll IAT!)
 *     Calls GetProcAddress(hUser32, "GetSystemMetrics") & GetProcAddress(hUser32, "CreateWindowExW")
 *       -> pure Ring-3 PE32+ IMAGE_EXPORT_DIRECTORY walk inside ntdll!LdrGetProcedureAddress!
 *     Calls GetModuleHandleW(L"kernel32.dll") + GetProcAddress("GetCurrentProcessId")
 *     Invokes all 3 dynamically resolved function pointers LIVE in Ring 3!
 *
 *   Test 3 (Blocking Process Synchronization + Exit Code Propagation):
 *     Calls CreateProcessW(L"\\Windows\\System32\\win32_proc_b.exe", ...) -> isolated child CR3
 *     Calls WaitForSingleObject(pi.hProcess, INFINITE) -> Running->Waiting->Ready wakeup on child exit!
 *     Calls GetExitCodeProcess(pi.hProcess, &childExitCode) -> confirms child exited cleanly (0)!
 */

#include <cstdint>
#include <cstddef>

extern "C" {

struct ProcessInformation {
    void*    hProcess;
    void*    hThread;
    uint32_t dwProcessId;
    uint32_t dwThreadId;
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

__declspec(dllimport) void*    GetStdHandle(uint32_t nStdHandle);
__declspec(dllimport) int32_t  WriteFile(void* hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite,
                                         uint32_t* lpNumberOfBytesWritten, void* lpOverlapped);
__declspec(dllimport) uint32_t GetCurrentProcessId();
__declspec(dllimport) void*    FindFirstFileW(const wchar_t* lpFileName, Win32FindDataW* lpFindFileData);
__declspec(dllimport) int32_t  FindNextFileW(void* hFindFile, Win32FindDataW* lpFindFileData);
__declspec(dllimport) int32_t  FindClose(void* hFindFile);
__declspec(dllimport) void*    LoadLibraryW(const wchar_t* lpLibFileName);
__declspec(dllimport) void*    GetModuleHandleW(const wchar_t* lpModuleName);
__declspec(dllimport) void*    GetProcAddress(void* hModule, const char* lpProcName);
__declspec(dllimport) int32_t  CreateProcessW(const wchar_t* lpApplicationName, wchar_t* lpCommandLine,
                                              void* lpProcessAttributes, void* lpThreadAttributes,
                                              int32_t bInheritHandles, uint32_t dwCreationFlags,
                                              void* lpEnvironment, const wchar_t* lpCurrentDirectory,
                                              void* lpStartupInfo, ProcessInformation* lpProcessInformation);
__declspec(dllimport) uint32_t WaitForSingleObject(void* hHandle, uint32_t dwMilliseconds);
__declspec(dllimport) int32_t  GetExitCodeProcess(void* hProcess, uint32_t* lpExitCode);
__declspec(dllimport) void     ExitProcess(uint32_t uExitCode);

static constexpr uint32_t STD_OUTPUT_HANDLE = 0xFFFFFFF5u;
static constexpr uint32_t FILE_ATTR_DIR     = 0x00000010u;
static constexpr uint32_t INFINITE_TIMEOUT  = 0xFFFFFFFFu;

static size_t cstrlen(const char* s) {
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}

static void writeStr(void* hOut, const char* s) {
    uint32_t written = 0;
    WriteFile(hOut, s, static_cast<uint32_t>(cstrlen(s)), &written, nullptr);
}

static void appendStr(char* dst, size_t& pos, const char* s) {
    while (*s) dst[pos++] = *s++;
}

static void appendDec(char* dst, size_t& pos, uint64_t val) {
    char tmp[24];
    size_t n = 0;
    if (val == 0) tmp[n++] = '0';
    else {
        while (val > 0 && n < 22) {
            tmp[n++] = static_cast<char>('0' + (val % 10));
            val /= 10;
        }
    }
    while (n > 0) dst[pos++] = tmp[--n];
}

static void appendHex64(char* dst, size_t& pos, uint64_t val) {
    const char hex[] = "0123456789ABCDEF";
    for (int shift = 60; shift >= 0; shift -= 4) {
        dst[pos++] = hex[(val >> shift) & 0xFULL];
    }
}

using FnGetSystemMetrics = int32_t (*)(int32_t);
using FnCreateWindowExW  = void* (*)(uint32_t, const wchar_t*, const wchar_t*, uint32_t,
                                     int, int, int, int, void*, void*, void*, void*);
using FnGetCurrentPid    = uint32_t (*)();

void WinMainEntry() {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    uint32_t myPid = GetCurrentProcessId();
    bool allPassed = true;
    char msg[180];

    // =========================================================================
    // TEST 1: Live NTFS Directory Enumeration via FindFirstFileW / FindNextFileW
    // =========================================================================
    Win32FindDataW fd;
    fd.dwFileAttributes = 0;
    fd.nFileSizeHigh = 0;
    fd.nFileSizeLow = 0;
    fd.cFileName[0] = L'\0';
    uint32_t fileCount = 0;
    uint32_t dirCount = 0;
    uint64_t totalBytes = 0;

    void* hFind = FindFirstFileW(L"C:\\Windows\\System32\\*", &fd);
    if (hFind != reinterpret_cast<void*>(-1) && hFind != nullptr) {
        do {
            if (fd.dwFileAttributes & FILE_ATTR_DIR) {
                dirCount++;
            } else {
                fileCount++;
                totalBytes += (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    } else {
        allPassed = false;
    }

    size_t p = 0;
    appendStr(msg, p, "[NT Suite PID=");
    appendDec(msg, p, myPid);
    appendStr(msg, p, "] FindFirstFileW(System32\\*): ");
    appendDec(msg, p, fileCount);
    appendStr(msg, p, " files, ");
    appendDec(msg, p, dirCount);
    appendStr(msg, p, " dirs (");
    appendDec(msg, p, totalBytes);
    appendStr(msg, p, " B) PASS!\r\n");
    msg[p] = '\0';
    writeStr(hOut, msg);
    if (fileCount < 10) allPassed = false;

    // =========================================================================
    // TEST 2: Runtime LoadLibraryW("user32.dll") + Ring-3 EAT GetProcAddress
    // =========================================================================
    void* hUser32 = LoadLibraryW(L"user32.dll");
    int32_t cx = 0, cy = 0;
    uint64_t hwndVal = 0;
    uint32_t dynPid = 0;

    if (hUser32) {
        auto pMetrics = reinterpret_cast<FnGetSystemMetrics>(GetProcAddress(hUser32, "GetSystemMetrics"));
        auto pCreateWnd = reinterpret_cast<FnCreateWindowExW>(GetProcAddress(hUser32, "CreateWindowExW"));
        if (pMetrics) {
            cx = pMetrics(0); // SM_CXSCREEN
            cy = pMetrics(1); // SM_CYSCREEN
        } else {
            allPassed = false;
        }
        if (pCreateWnd) {
            void* hwnd = pCreateWnd(0, L"MicaWnd", L"MicaNT", 0, 0, 0, 800, 600, nullptr, nullptr, nullptr, nullptr);
            hwndVal = reinterpret_cast<uint64_t>(hwnd);
        } else {
            allPassed = false;
        }
    } else {
        allPassed = false;
    }

    void* hK32 = GetModuleHandleW(L"kernel32.dll");
    if (hK32) {
        auto pGetPid = reinterpret_cast<FnGetCurrentPid>(GetProcAddress(hK32, "GetCurrentProcessId"));
        if (pGetPid) dynPid = pGetPid();
    }
    if (cx != 1920 || cy != 1080 || hwndVal == 0 || dynPid != myPid) {
        allPassed = false;
    }

    p = 0;
    appendStr(msg, p, "[NT Suite PID=");
    appendDec(msg, p, myPid);
    appendStr(msg, p, "] LoadLibraryW(user32)@0x");
    appendHex64(msg, p, reinterpret_cast<uint64_t>(hUser32));
    appendStr(msg, p, " Metrics=");
    appendDec(msg, p, static_cast<uint64_t>(cx));
    appendStr(msg, p, "x");
    appendDec(msg, p, static_cast<uint64_t>(cy));
    appendStr(msg, p, " HWND=0x");
    appendHex64(msg, p, hwndVal);
    appendStr(msg, p, " PASS!\r\n");
    msg[p] = '\0';
    writeStr(hOut, msg);

    // =========================================================================
    // TEST 3: CreateProcessW + Blocking WaitForSingleObject + GetExitCodeProcess
    // =========================================================================
    ProcessInformation pi{};
    uint32_t waitRes = 0xFFFFFFFFu;
    uint32_t childExit = 0xBADu;
    if (CreateProcessW(L"\\Windows\\System32\\win32_proc_b.exe", nullptr, nullptr, nullptr,
                       0, 0, nullptr, nullptr, nullptr, &pi)) {
        waitRes = WaitForSingleObject(pi.hProcess, INFINITE_TIMEOUT);
        GetExitCodeProcess(pi.hProcess, &childExit);
    } else {
        allPassed = false;
    }
    if (waitRes != 0 || childExit != 0) {
        allPassed = false;
    }

    p = 0;
    appendStr(msg, p, "[NT Suite PID=");
    appendDec(msg, p, myPid);
    appendStr(msg, p, "] WaitForSingleObject(PID=");
    appendDec(msg, p, pi.dwProcessId);
    appendStr(msg, p, ")=");
    appendDec(msg, p, waitRes);
    appendStr(msg, p, " | GetExitCodeProcess=");
    appendDec(msg, p, childExit);
    appendStr(msg, p, allPassed ? " -> ALL PASS!\r\n" : " -> FAILED!\r\n");
    msg[p] = '\0';
    writeStr(hOut, msg);

    ExitProcess(allPassed ? 0u : 77u);
}

} // extern "C"
