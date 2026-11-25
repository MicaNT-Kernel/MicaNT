/**
 * @file win32_ntfs_probe.cpp
 * @brief Authentic Unmodified-ABI Win32 PE32+ Console Executable.
 *
 * ZERO MicaNT headers. ZERO inline assembly. ZERO direct syscalls.
 * Dynamically imports standard Windows Win32 APIs from KERNEL32.dll via
 * a genuine PE32+ Import Directory Table (IAT).
 *
 * Executed live in Ring 3 (CPL=3) by MicaNT's bare-metal PE32+ dynamic loader
 * after loading both this executable AND kernel32.dll off the mounted NTFS 3.1
 * AHCI SATA disk image:
 *   1. Prints startup banner via WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), ...)
 *   2. Queries live PID via GetCurrentProcessId() (reads Ring 3 TEB via GS:[0x30])
 *   3. Allocates 4096-byte page via VirtualAlloc()
 *   4. Opens C:\MicaNT\system.ini via CreateFileW() -> live NTFS Ring-0 handle
 *   5. Reads file contents via ReadFile() -> live AHCI Bus-Master DMA from NTFS
 *   6. Prints live file contents via WriteFile()
 *   7. Closes handle via CloseHandle(), frees memory via VirtualFree(), exits via ExitProcess(0)
 */

#include <cstdint>
#include <cstddef>

extern "C" {

// Standard Win32 API Imports dynamically bound from C:\Windows\System32\kernel32.dll
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
__declspec(dllimport) void     ExitProcess(uint32_t uExitCode);

static constexpr uint32_t STD_OUTPUT_HANDLE  = 0xFFFFFFF5u;
static constexpr uint32_t GENERIC_READ       = 0x80000000u;
static constexpr uint32_t FILE_SHARE_READ    = 0x00000001u;
static constexpr uint32_t OPEN_EXISTING      = 3u;
static constexpr uint32_t FILE_ATTR_NORMAL   = 0x00000080u;
static constexpr uint32_t MEM_COMMIT_RESERVE = 0x00003000u;
static constexpr uint32_t MEM_RELEASE        = 0x00008000u;
static constexpr uint32_t PAGE_READWRITE     = 0x00000004u;

static size_t cstrlen(const char* s) {
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}

static void writeStr(void* hOut, const char* s) {
    uint32_t written = 0;
    WriteFile(hOut, s, static_cast<uint32_t>(cstrlen(s)), &written, nullptr);
}

static void appendDec(char* dst, size_t& pos, uint32_t val) {
    char tmp[16];
    size_t n = 0;
    if (val == 0) {
        tmp[n++] = '0';
    } else {
        while (val > 0 && n < 15) {
            tmp[n++] = static_cast<char>('0' + (val % 10));
            val /= 10;
        }
    }
    while (n > 0) {
        dst[pos++] = tmp[--n];
    }
}

void WinMainEntry() {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    writeStr(hOut, "[Win32 PE32+ App] Live Ring-3 Win32 app -> kernel32.dll -> ntdll.dll -> Ring 0!\r\n");

    uint32_t pid = GetCurrentProcessId();
    char pidLine[64];
    const char* prefix = "[Win32 PE32+ App] GetCurrentProcessId() = ";
    size_t p = 0;
    while (prefix[p]) { pidLine[p] = prefix[p]; ++p; }
    appendDec(pidLine, p, pid);
    pidLine[p++] = '\r';
    pidLine[p++] = '\n';
    pidLine[p] = '\0';
    writeStr(hOut, pidLine);

    // Allocate 4096-byte Ring 3 buffer via kernel32!VirtualAlloc -> NtAllocateVirtualMemory
    char* fileBuf = static_cast<char*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT_RESERVE, PAGE_READWRITE));
    if (!fileBuf) {
        writeStr(hOut, "[Win32 PE32+ App] ERROR: VirtualAlloc failed!\r\n");
        ExitProcess(1);
    }

    // Open C:\MicaNT\system.ini directly via kernel32!CreateFileW -> NtCreateFile (NTFS 3.1)
    void* hFile = CreateFileW(
        L"C:\\MicaNT\\system.ini",
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTR_NORMAL,
        nullptr
    );
    if (hFile == reinterpret_cast<void*>(-1) || hFile == nullptr) {
        writeStr(hOut, "[Win32 PE32+ App] ERROR: CreateFileW(C:\\MicaNT\\system.ini) failed!\r\n");
        VirtualFree(fileBuf, 0, MEM_RELEASE);
        ExitProcess(2);
    }

    // Read real file bytes via kernel32!ReadFile -> NtReadFile -> AHCI DMA
    uint32_t bytesRead = 0;
    int32_t readOk = ReadFile(hFile, fileBuf, 4095, &bytesRead, nullptr);
    if (readOk && bytesRead > 0) {
        fileBuf[bytesRead] = '\0';
        writeStr(hOut, "[Win32 PE32+ App] ReadFile(C:\\MicaNT\\system.ini) via NTFS DMA succeeded:\r\n");
        uint32_t written = 0;
        WriteFile(hOut, fileBuf, bytesRead, &written, nullptr);
    } else {
        writeStr(hOut, "[Win32 PE32+ App] ERROR: ReadFile failed!\r\n");
    }

    CloseHandle(hFile);
    VirtualFree(fileBuf, 0, MEM_RELEASE);
    writeStr(hOut, "[Win32 PE32+ App] CloseHandle + VirtualFree + ExitProcess(0) COMPLETE!\r\n");
    ExitProcess(0);
}

} // extern "C"
