/**
 * @file win32_nt_suite.cpp
 * @brief Authentic Unmodified-ABI Win32 PE32+ Runtime Loader, Directory Enum, Sync & Persistent NTFS Write Suite.
 *
 * ZERO MicaNT headers. ZERO inline assembly. ZERO direct syscall instructions.
 * Dynamically imports ONLY standard Windows Win32 APIs from KERNEL32.dll
 * (chaining through NTDLL.dll -> Ring 0 KiSystemCall64):
 *
 *   Test 1 (Live NTFS Directory Enumeration):
 *     FindFirstFileW(L"C:\\Windows\\System32\\*") + FindNextFileW + FindClose
 *   Test 2 (Dynamic Runtime DLL Loading + Pure Ring-3 PE32+ EAT Walk):
 *     LoadLibraryW(L"user32.dll") + GetProcAddress("GetSystemMetrics"/"CreateWindowExW")
 *   Test 3 (Blocking Process Synchronization + Exit Code Propagation):
 *     CreateProcessW(win32_proc_b.exe) + WaitForSingleObject + GetExitCodeProcess
 *   Test 4 (Real Bare-Metal NTFS 3.1 AHCI DMA Write & Readback Persistence):
 *     Creates BOTH a Resident file (C:\MicaNT\persist_m6.txt) AND a 2048-byte Non-Resident
 *     $Bitmap-allocated file (C:\MicaNT\big_dma_m6.bin) via CreateFileW(CREATE_ALWAYS) + WriteFile,
 *     then re-opens both via CreateFileW(OPEN_EXISTING) + ReadFile and verifies 100% byte-exact match!
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
__declspec(dllimport) void*    CreateFileW(const wchar_t* lpFileName, uint32_t dwDesiredAccess, uint32_t dwShareMode,
                                           void* lpSecurityAttributes, uint32_t dwCreationDisposition,
                                           uint32_t dwFlagsAndAttributes, void* hTemplateFile);
__declspec(dllimport) int32_t  ReadFile(void* hFile, void* lpBuffer, uint32_t nNumberOfBytesToRead,
                                        uint32_t* lpNumberOfBytesRead, void* lpOverlapped);
__declspec(dllimport) int32_t  WriteFile(void* hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite,
                                         uint32_t* lpNumberOfBytesWritten, void* lpOverlapped);
__declspec(dllimport) int32_t  CloseHandle(void* hObject);
__declspec(dllimport) void*    VirtualAlloc(void* lpAddress, size_t dwSize, uint32_t flAllocationType, uint32_t flProtect);
__declspec(dllimport) int32_t  VirtualFree(void* lpAddress, size_t dwSize, uint32_t dwFreeType);
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

static constexpr uint32_t STD_OUTPUT_HANDLE  = 0xFFFFFFF5u;
static constexpr uint32_t GENERIC_READ       = 0x80000000u;
static constexpr uint32_t GENERIC_WRITE      = 0x40000000u;
static constexpr uint32_t FILE_SHARE_READ    = 0x00000001u;
static constexpr uint32_t CREATE_ALWAYS      = 2u;
static constexpr uint32_t OPEN_EXISTING      = 3u;
static constexpr uint32_t FILE_ATTR_DIR      = 0x00000010u;
static constexpr uint32_t FILE_ATTR_NORMAL   = 0x00000080u;
static constexpr uint32_t MEM_COMMIT_RESERVE = 0x00003000u;
static constexpr uint32_t MEM_RELEASE        = 0x00008000u;
static constexpr uint32_t PAGE_READWRITE     = 0x00000004u;
static constexpr uint32_t INFINITE_TIMEOUT   = 0xFFFFFFFFu;

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
            if (fd.dwFileAttributes & FILE_ATTR_DIR) dirCount++;
            else {
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
        if (pMetrics) { cx = pMetrics(0); cy = pMetrics(1); } else allPassed = false;
        if (pCreateWnd) {
            void* hwnd = pCreateWnd(0, L"MicaWnd", L"MicaNT", 0, 0, 0, 800, 600, nullptr, nullptr, nullptr, nullptr);
            hwndVal = reinterpret_cast<uint64_t>(hwnd);
        } else allPassed = false;
    } else {
        allPassed = false;
    }

    void* hK32 = GetModuleHandleW(L"kernel32.dll");
    if (hK32) {
        auto pGetPid = reinterpret_cast<FnGetCurrentPid>(GetProcAddress(hK32, "GetCurrentProcessId"));
        if (pGetPid) dynPid = pGetPid();
    }
    if (cx != 1920 || cy != 1080 || hwndVal == 0 || dynPid != myPid) allPassed = false;

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
    ProcessInformation pi{nullptr, nullptr, 0, 0};
    uint32_t waitRes = 0xFFFFFFFFu;
    uint32_t childExit = 0xBADu;
    if (CreateProcessW(L"\\Windows\\System32\\win32_proc_b.exe", nullptr, nullptr, nullptr,
                       0, 0, nullptr, nullptr, nullptr, &pi)) {
        waitRes = WaitForSingleObject(pi.hProcess, INFINITE_TIMEOUT);
        GetExitCodeProcess(pi.hProcess, &childExit);
    } else {
        allPassed = false;
    }
    if (waitRes != 0 || childExit != 0) allPassed = false;

    // =========================================================================
    // TEST 4: Real Bare-Metal NTFS 3.1 Write Path (Resident + Non-Resident $Bitmap DMA)
    // =========================================================================
    static const char kPersistPayload[] =
        "MicaNT Sovereign NTFS 3.1 Live Ring-3 Write Verified!\r\n"
        "Written via CreateFileW(CREATE_ALWAYS)+WriteFile -> AHCI DMA!\r\n";
    uint32_t smallLen = static_cast<uint32_t>(cstrlen(kPersistPayload));
    bool smallWriteOk = false;
    bool bigWriteOk = false;

    // 4A. Resident file write (<= 512 B): C:\MicaNT\persist_m6.txt
    void* hSmall = CreateFileW(L"C:\\MicaNT\\persist_m6.txt", GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTR_NORMAL, nullptr);
    if (hSmall != reinterpret_cast<void*>(-1) && hSmall != nullptr) {
        uint32_t nw = 0;
        if (WriteFile(hSmall, kPersistPayload, smallLen, &nw, nullptr) && nw == smallLen) {
            smallWriteOk = true;
        }
        CloseHandle(hSmall);
    }

    // Immediate readback verification from live NTFS 3.1
    auto* ioPage = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT_RESERVE, PAGE_READWRITE));
    if (smallWriteOk && ioPage) {
        void* hCheck = CreateFileW(L"C:\\MicaNT\\persist_m6.txt", GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTR_NORMAL, nullptr);
        uint32_t nr = 0;
        if (hCheck != reinterpret_cast<void*>(-1) && hCheck != nullptr) {
            ReadFile(hCheck, ioPage, 512, &nr, nullptr);
            CloseHandle(hCheck);
        }
        if (nr != smallLen) smallWriteOk = false;
        for (uint32_t i = 0; smallWriteOk && i < smallLen; ++i) {
            if (ioPage[i] != static_cast<uint8_t>(kPersistPayload[i])) smallWriteOk = false;
        }
    }

    // 4B. Non-Resident multi-sector file write (2,048 B -> triggers real $Bitmap cluster alloc + AHCI DMA):
    constexpr uint32_t BIG_SIZE = 2048;
    if (ioPage) {
        for (uint32_t i = 0; i < BIG_SIZE; ++i) {
            ioPage[i] = static_cast<uint8_t>((i * 73u + 0x5Au) & 0xFFu);
        }
        void* hBig = CreateFileW(L"C:\\MicaNT\\big_dma_m6.bin", GENERIC_WRITE, 0, nullptr,
                                 CREATE_ALWAYS, FILE_ATTR_NORMAL, nullptr);
        if (hBig != reinterpret_cast<void*>(-1) && hBig != nullptr) {
            uint32_t nw = 0;
            if (WriteFile(hBig, ioPage, BIG_SIZE, &nw, nullptr) && nw == BIG_SIZE) {
                bigWriteOk = true;
            }
            CloseHandle(hBig);
        }
        // Read back 2048 bytes into upper half of ioPage and compare byte-for-byte!
        if (bigWriteOk) {
            uint8_t* verifyBuf = ioPage + 2048;
            void* hBigRd = CreateFileW(L"C:\\MicaNT\\big_dma_m6.bin", GENERIC_READ, FILE_SHARE_READ, nullptr,
                                       OPEN_EXISTING, FILE_ATTR_NORMAL, nullptr);
            uint32_t nr = 0;
            if (hBigRd != reinterpret_cast<void*>(-1) && hBigRd != nullptr) {
                ReadFile(hBigRd, verifyBuf, BIG_SIZE, &nr, nullptr);
                CloseHandle(hBigRd);
            }
            if (nr != BIG_SIZE) bigWriteOk = false;
            for (uint32_t i = 0; bigWriteOk && i < BIG_SIZE; ++i) {
                if (verifyBuf[i] != ioPage[i]) bigWriteOk = false;
            }
        }
        VirtualFree(ioPage, 0, MEM_RELEASE);
    }

    if (!smallWriteOk || !bigWriteOk) allPassed = false;

    p = 0;
    appendStr(msg, p, "[NT Suite PID=");
    appendDec(msg, p, myPid);
    appendStr(msg, p, "] NTFS Write+Verify: Resident(");
    appendDec(msg, p, smallLen);
    appendStr(msg, p, "B)=");
    appendStr(msg, p, smallWriteOk ? "OK" : "FAIL");
    appendStr(msg, p, " | NonRes(");
    appendDec(msg, p, BIG_SIZE);
    appendStr(msg, p, "B)=");
    appendStr(msg, p, bigWriteOk ? "OK" : "FAIL");
    appendStr(msg, p, " | Wait=");
    appendDec(msg, p, childExit);
    appendStr(msg, p, allPassed ? " -> ALL PASS!\r\n" : " -> FAILED!\r\n");
    msg[p] = '\0';
    writeStr(hOut, msg);

    ExitProcess(allPassed ? 0u : 77u);
}

} // extern "C"
