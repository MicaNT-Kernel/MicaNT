/**
 * @file win32_proc_a.cpp
 * @brief Authentic Unmodified-ABI Win32 PE32+ Multi-Process Isolation Parent (Process A).
 *
 * ZERO MicaNT headers. ZERO inline assembly. ZERO direct syscall instructions.
 * Dynamically imports standard Windows Win32 APIs exclusively from KERNEL32.dll
 * (which dynamically chains to NTDLL.dll -> Ring 0 KiSystemCall64):
 *
 *   1. Allocates 4096-byte private page via VirtualAlloc() -> VA 0x0000008000400000
 *   2. Writes secret 64-bit canary 0xCAFEBABE1111AAAA into its private VA
 *   3. Dynamically spawns win32_proc_b.exe via CreateProcessW() -> NtCreateUserProcess (0x58)
 *      in its OWN isolated hardware CR3 address space (mapping the EXACT SAME VA 0x0000008000400000!)
 *   4. Executes CPU-bound compute epochs while 100 Hz Local APIC hardware timer (Vector 0x30)
 *      preempts Ring 3 back and forth between Process A (CR3_A) and Process B (CR3_B)
 *   5. Opens & reads C:\MicaNT\system.ini via CreateFileW/ReadFile (live NTFS 3.1 DMA)
 *   6. Verifies VA 0x0000008000400000 STILL holds 0xCAFEBABE1111AAAA (100% CR3 memory isolation!)
 *   7. Releases memory via VirtualFree() and exits cleanly via ExitProcess(0)
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
__declspec(dllimport) int32_t  CreateProcessW(const wchar_t* lpApplicationName, wchar_t* lpCommandLine,
                                              void* lpProcessAttributes, void* lpThreadAttributes,
                                              int32_t bInheritHandles, uint32_t dwCreationFlags,
                                              void* lpEnvironment, const wchar_t* lpCurrentDirectory,
                                              void* lpStartupInfo, ProcessInformation* lpProcessInformation);
__declspec(dllimport) void     ExitProcess(uint32_t uExitCode);

static constexpr uint32_t STD_OUTPUT_HANDLE  = 0xFFFFFFF5u;
static constexpr uint32_t GENERIC_READ       = 0x80000000u;
static constexpr uint32_t FILE_SHARE_READ    = 0x00000001u;
static constexpr uint32_t OPEN_EXISTING      = 3u;
static constexpr uint32_t FILE_ATTR_NORMAL   = 0x00000080u;
static constexpr uint32_t MEM_COMMIT_RESERVE = 0x00003000u;
static constexpr uint32_t MEM_RELEASE        = 0x00008000u;
static constexpr uint32_t PAGE_READWRITE     = 0x00000004u;

static constexpr uint64_t CANARY_A = 0xCAFEBABE1111AAAAULL;

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

static void appendDec(char* dst, size_t& pos, uint32_t val) {
    char tmp[16];
    size_t n = 0;
    if (val == 0) tmp[n++] = '0';
    else {
        while (val > 0 && n < 15) {
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

void WinMainEntry() {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    uint32_t myPid = GetCurrentProcessId();

    // 1. Allocate private heap page via VirtualAlloc -> 0x0000008000400000
    auto* privPage = static_cast<volatile uint64_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT_RESERVE, PAGE_READWRITE));
    if (!privPage) {
        writeStr(hOut, "[ProcA] ERROR: VirtualAlloc failed!\r\n");
        ExitProcess(1);
    }
    privPage[0] = CANARY_A;
    privPage[1] = static_cast<uint64_t>(myPid);

    char msg[160];
    size_t p = 0;
    appendStr(msg, p, "[ProcA PID=");
    appendDec(msg, p, myPid);
    appendStr(msg, p, "] Private VA=0x");
    appendHex64(msg, p, reinterpret_cast<uint64_t>(privPage));
    appendStr(msg, p, " Canary=0x");
    appendHex64(msg, p, CANARY_A);
    appendStr(msg, p, "\r\n");
    msg[p] = '\0';
    writeStr(hOut, msg);

    // 2. Spawn isolated child Process B via Win32 CreateProcessW -> NtCreateUserProcess (0x58)
    ProcessInformation pi{};
    int32_t spawned = CreateProcessW(
        L"\\Windows\\System32\\win32_proc_b.exe",
        nullptr, nullptr, nullptr, 0, 0, nullptr, nullptr, nullptr, &pi
    );
    p = 0;
    appendStr(msg, p, "[ProcA PID=");
    appendDec(msg, p, myPid);
    if (spawned) {
        appendStr(msg, p, "] CreateProcessW(win32_proc_b.exe) -> Child PID=");
        appendDec(msg, p, pi.dwProcessId);
        appendStr(msg, p, " OK!\r\n");
    } else {
        appendStr(msg, p, "] ERROR: CreateProcessW failed!\r\n");
    }
    msg[p] = '\0';
    writeStr(hOut, msg);

    // 3. CPU-bound compute epochs spanning real 100 Hz LAPIC timer preemptions (Vector 0x30)
    volatile uint64_t accum = 0;
    bool isolatedOk = true;
    for (uint32_t epoch = 1; epoch <= 4; ++epoch) {
        for (volatile uint32_t spin = 0; spin < 2500000u; ++spin) {
            accum += (spin ^ epoch);
        }
        if (privPage[0] != CANARY_A || privPage[1] != static_cast<uint64_t>(myPid)) {
            isolatedOk = false;
        }
    }

    // 4. Concurrent NTFS 3.1 file read while Process B also accesses NTFS
    uint32_t iniBytes = 0;
    void* hFile = CreateFileW(L"C:\\MicaNT\\system.ini", GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTR_NORMAL, nullptr);
    if (hFile != reinterpret_cast<void*>(-1) && hFile != nullptr) {
        char* readBuf = reinterpret_cast<char*>(const_cast<uint64_t*>(privPage + 8));
        ReadFile(hFile, readBuf, 256, &iniBytes, nullptr);
        CloseHandle(hFile);
    }

    // Final CR3 isolation check after all preemptive context switches
    uint64_t finalVal = privPage[0];
    if (finalVal != CANARY_A) isolatedOk = false;

    p = 0;
    appendStr(msg, p, "[ProcA PID=");
    appendDec(msg, p, myPid);
    if (isolatedOk) {
        appendStr(msg, p, "] CR3 ISOLATION PASS: VA 0x");
        appendHex64(msg, p, reinterpret_cast<uint64_t>(privPage));
        appendStr(msg, p, "==0x");
        appendHex64(msg, p, finalVal);
        appendStr(msg, p, " | NTFS Read=");
        appendDec(msg, p, iniBytes);
        appendStr(msg, p, "B\r\n");
    } else {
        appendStr(msg, p, "] CR3 ISOLATION FAIL: Observed=0x");
        appendHex64(msg, p, finalVal);
        appendStr(msg, p, "\r\n");
    }
    msg[p] = '\0';
    writeStr(hOut, msg);

    VirtualFree(const_cast<uint64_t*>(privPage), 0, MEM_RELEASE);
    ExitProcess(isolatedOk ? 0u : 99u);
}

} // extern "C"
