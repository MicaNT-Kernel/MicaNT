#pragma once

/**
 * @file kernel32.hpp
 * @brief Clean-Room Win32 Base API Bridge (kernel32.dll / kernelbase.dll).
 *
 * Implements the standard Win32 API layer on top of MicaNT clean-room ntdll.dll
 * system call stubs and the CSRSS subsystem server.
 *
 * References: Microsoft win32metadata & Microsoft Learn Public Win32 API Specification.
 */

#include <cstdint>
#include <string_view>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstdlib>
#include <cwctype>
#include <mutex>
#include <unordered_map>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ntdll.hpp"
#include "ldr.hpp"
#include "csrss.hpp"
#include "conhost.hpp"
#include "hal.hpp"
#include "npfs.hpp"
#include "fs.hpp"

namespace micant::win32 {

inline uintptr_t g_CurrentExecutableBase = 0;

// ============================================================================
// 1. Standard Win32 Types & Constants
// ============================================================================

using DWORD   = uint32_t;
using LPDWORD = uint32_t*;
using PDWORD  = uint32_t*;
using UINT    = uint32_t;
using BOOL    = int32_t;
using HANDLE  = void*;
using HMODULE = void*;
using HWND    = void*;
using HINSTANCE = void*;
using HICON   = void*;
using HKEY    = void*;
using LPVOID  = void*;
using LPCVOID = const void*;
using LPCWSTR = const wchar_t*;
using LPWSTR  = wchar_t*;
using LPWCH   = wchar_t*;
using LPCSTR  = const char*;
using LPSTR   = char*;
using SIZE_T    = size_t;
using DWORD_PTR = uintptr_t;
using ULONG_PTR = uintptr_t;
using LONG_PTR  = intptr_t;
using LONG      = int32_t;
using LPLONG    = int32_t*;
using WCHAR     = wchar_t;
using PCWSTR    = const wchar_t*;
using PWSTR     = wchar_t*;
using LPCWCH    = const wchar_t*;
using ULONG     = uint32_t;
using PULONG    = uint32_t*;
using PBOOL     = int32_t*;
using LPBOOL    = int32_t*;
using PVOID     = void*;
using WORD      = uint16_t;
using LPWORD    = uint16_t*;
using BYTE      = uint8_t;
using LCID      = uint32_t;
using LPARAM    = intptr_t;
using WPARAM    = uintptr_t;

struct SRWLOCK {
    void* Ptr{nullptr};
};
using PSRWLOCK = SRWLOCK*;

struct CONDITION_VARIABLE {
    void* Ptr{nullptr};
};
using PCONDITION_VARIABLE = CONDITION_VARIABLE*;

union INIT_ONCE {
    void* Ptr{nullptr};
};
using LPINIT_ONCE = INIT_ONCE*;
using PINIT_ONCE_FN = BOOL (*)(LPINIT_ONCE, void*, void**);

using PTP_WORK = void*;
using PTP_WORK_CALLBACK = void (*)(void*, void*, void*);
using PTP_CALLBACK_ENVIRON = void*;
using PTP_CALLBACK_INSTANCE = void*;

struct PROCESSENTRY32W {
    DWORD dwSize{sizeof(PROCESSENTRY32W)};
    DWORD cntUsage{0};
    DWORD th32ProcessID{0};
    ULONG_PTR th32DefaultHeapID{0};
    DWORD th32ModuleID{0};
    DWORD cntThreads{1};
    DWORD th32ParentProcessID{0};
    LONG pcPriClassBase{0};
    DWORD dwFlags{0};
    wchar_t szExeFile[260]{};
};
using LPPROCESSENTRY32W = PROCESSENTRY32W*;

using PAPCFUNC = void (*)(ULONG_PTR);
using LOCALE_ENUMPROCW = BOOL (*)(LPWSTR);

struct DCB {
    DWORD DCBlength{sizeof(DCB)};
    DWORD BaudRate{115200};
    DWORD fBinary:1{1};
    DWORD fParity:1{0};
    DWORD fOutxCtsFlow:1{0};
    DWORD fOutxDsrFlow:1{0};
    DWORD fDtrControl:2{0};
    DWORD fDsrSensitivity:1{0};
    DWORD fTXContinueOnXoff:1{0};
    DWORD fOutX:1{0};
    DWORD fInX:1{0};
    DWORD fErrorChar:1{0};
    DWORD fNull:1{0};
    DWORD fRtsControl:2{0};
    DWORD fAbortOnError:1{0};
    DWORD fDummy2:17{0};
    WORD  wReserved{0};
    WORD  XonLim{0};
    WORD  XoffLim{0};
    BYTE  ByteSize{8};
    BYTE  Parity{0};
    BYTE  StopBits{0};
    char  XonChar{0};
    char  XoffChar{0};
    char  ErrorChar{0};
    char  EofChar{0};
    char  EvtChar{0};
    WORD  wReserved1{0};
};

struct COMMTIMEOUTS {
    DWORD ReadIntervalTimeout{0};
    DWORD ReadTotalTimeoutMultiplier{0};
    DWORD ReadTotalTimeoutConstant{5000};
    DWORD WriteTotalTimeoutMultiplier{0};
    DWORD WriteTotalTimeoutConstant{5000};
};

struct MEMORYSTATUS {
    DWORD dwLength{sizeof(MEMORYSTATUS)};
    DWORD dwMemoryLoad{25};
    SIZE_T dwTotalPhys{16ULL * 1024 * 1024 * 1024};
    SIZE_T dwAvailPhys{12ULL * 1024 * 1024 * 1024};
    SIZE_T dwTotalPageFile{32ULL * 1024 * 1024 * 1024};
    SIZE_T dwAvailPageFile{28ULL * 1024 * 1024 * 1024};
    SIZE_T dwTotalVirtual{128ULL * 1024 * 1024 * 1024 * 1024};
    SIZE_T dwAvailVirtual{127ULL * 1024 * 1024 * 1024 * 1024};
};

struct OVERLAPPED {
    ULONG_PTR Internal{0};
    ULONG_PTR InternalHigh{0};
    union {
        __extension__ struct {
            DWORD Offset;
            DWORD OffsetHigh;
        };
        void* Pointer{nullptr};
    };
    HANDLE hEvent{nullptr};
};
using LPOVERLAPPED = OVERLAPPED*;

using micant::TRUE;
using micant::FALSE;
inline const HANDLE INVALID_HANDLE_VALUE = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
inline bool g_TraceApi = false;

/**
 * @brief Retrieves the calling thread's last-error code value.
 */
inline DWORD GetLastError() noexcept {
    return ntdll::RtlGetLastWin32Error();
}

/**
 * @brief Sets the last-error code for the calling thread.
 */
inline void SetLastError(DWORD dwErrCode) noexcept {
    ntdll::RtlSetLastWin32Error(dwErrCode);
}

// Win32 System Error Codes
inline constexpr DWORD ERROR_SUCCESS            = 0;
inline constexpr DWORD ERROR_FILE_NOT_FOUND     = 2;
inline constexpr DWORD ERROR_PATH_NOT_FOUND     = 3;
inline constexpr DWORD ERROR_ACCESS_DENIED      = 5;
inline constexpr DWORD ERROR_INVALID_HANDLE     = 6;
inline constexpr DWORD ERROR_NOT_ENOUGH_MEMORY  = 8;
inline constexpr DWORD ERROR_INVALID_DATA       = 13;
inline constexpr DWORD ERROR_INVALID_PARAMETER  = 87;
inline constexpr DWORD ERROR_ALREADY_EXISTS     = 183;
inline constexpr DWORD ERROR_NO_MORE_ITEMS      = 259;

// Heap Flags
inline constexpr DWORD HEAP_NO_SERIALIZE        = 0x00000001;
inline constexpr DWORD HEAP_GENERATE_EXCEPTIONS = 0x00000004;
inline constexpr DWORD HEAP_ZERO_MEMORY         = 0x00000008;

// Standard I/O Device Pseudo-Handles
inline constexpr DWORD STD_INPUT_HANDLE  = static_cast<DWORD>(-10);
inline constexpr DWORD STD_OUTPUT_HANDLE = static_cast<DWORD>(-11);
inline constexpr DWORD STD_ERROR_HANDLE  = static_cast<DWORD>(-12);

// File Access & Creation Flags
inline constexpr DWORD GENERIC_READ     = 0x80000000;
inline constexpr DWORD GENERIC_WRITE    = 0x40000000;
inline constexpr DWORD GENERIC_EXECUTE  = 0x20000000;
inline constexpr DWORD GENERIC_ALL      = 0x10000000;

inline constexpr DWORD FILE_SHARE_READ  = 0x00000001;
inline constexpr DWORD FILE_SHARE_WRITE = 0x00000002;

inline constexpr DWORD CREATE_NEW        = 1;
inline constexpr DWORD CREATE_ALWAYS     = 2;
inline constexpr DWORD OPEN_EXISTING     = 3;
inline constexpr DWORD OPEN_ALWAYS       = 4;
inline constexpr DWORD TRUNCATE_EXISTING = 5;

// Memory Allocation Types & Protection
inline constexpr DWORD MEM_COMMIT   = 0x00001000;
inline constexpr DWORD MEM_RESERVE  = 0x00002000;
inline constexpr DWORD MEM_DECOMMIT = 0x00004000;
inline constexpr DWORD MEM_RELEASE  = 0x00008000;

inline constexpr DWORD PAGE_NOACCESS          = 0x01;
inline constexpr DWORD PAGE_READONLY          = 0x02;
inline constexpr DWORD PAGE_READWRITE         = 0x04;
inline constexpr DWORD PAGE_EXECUTE           = 0x10;
inline constexpr DWORD PAGE_EXECUTE_READ      = 0x20;
inline constexpr DWORD PAGE_EXECUTE_READWRITE = 0x40;

// Synchronization Constants
inline constexpr DWORD INFINITE     = 0xFFFFFFFF;
inline constexpr DWORD WAIT_OBJECT_0= 0x00000000;
inline constexpr DWORD WAIT_TIMEOUT = 0x00000102;
inline constexpr DWORD WAIT_FAILED  = 0xFFFFFFFF;

// Move Method Constants for SetFilePointer
inline constexpr DWORD FILE_BEGIN   = 0;
inline constexpr DWORD FILE_CURRENT = 1;
inline constexpr DWORD FILE_END     = 2;

// File Attributes Constants
inline constexpr DWORD FILE_ATTRIBUTE_READONLY  = 0x00000001;
inline constexpr DWORD FILE_ATTRIBUTE_HIDDEN    = 0x00000002;
inline constexpr DWORD FILE_ATTRIBUTE_SYSTEM    = 0x00000004;
inline constexpr DWORD FILE_ATTRIBUTE_DIRECTORY = 0x00000010;
inline constexpr DWORD FILE_ATTRIBUTE_ARCHIVE   = 0x00000020;
inline constexpr DWORD FILE_ATTRIBUTE_NORMAL    = 0x00000080;
inline constexpr DWORD INVALID_FILE_ATTRIBUTES  = static_cast<DWORD>(-1);
inline constexpr DWORD INVALID_FILE_SIZE        = static_cast<DWORD>(-1);

// Console Mode Flags
inline constexpr DWORD ENABLE_PROCESSED_INPUT   = 0x0001;
inline constexpr DWORD ENABLE_LINE_INPUT        = 0x0002;
inline constexpr DWORD ENABLE_ECHO_INPUT        = 0x0004;
inline constexpr DWORD ENABLE_WINDOW_INPUT      = 0x0008;
inline constexpr DWORD ENABLE_MOUSE_INPUT       = 0x0010;
inline constexpr DWORD ENABLE_INSERT_MODE       = 0x0020;
inline constexpr DWORD ENABLE_QUICK_EDIT_MODE   = 0x0040;
inline constexpr DWORD ENABLE_EXTENDED_FLAGS    = 0x0080;
inline constexpr DWORD ENABLE_VIRTUAL_TERMINAL_INPUT = 0x0200;

inline constexpr DWORD ENABLE_PROCESSED_OUTPUT  = 0x0001;
inline constexpr DWORD ENABLE_WRAP_AT_EOL_OUTPUT= 0x0002;
inline constexpr DWORD ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004;

// Section / File Mapping Access
inline constexpr DWORD FILE_MAP_WRITE           = 0x0002;
inline constexpr DWORD FILE_MAP_READ            = 0x0004;
inline constexpr DWORD FILE_MAP_ALL_ACCESS      = 0x001F001F;
inline constexpr DWORD FILE_MAP_EXECUTE         = 0x0020;

/**
 * @brief Win32 System Information.
 */
struct SYSTEM_INFO {
    uint16_t wProcessorArchitecture{9}; // PROCESSOR_ARCHITECTURE_AMD64
    uint16_t wReserved{0};
    DWORD dwPageSize{4096};
    LPVOID lpMinimumApplicationAddress{reinterpret_cast<LPVOID>(0x10000)};
    LPVOID lpMaximumApplicationAddress{reinterpret_cast<LPVOID>(0x00007FFFFFFEFFFFULL)};
    uintptr_t dwActiveProcessorMask{0x0F}; // 4 cores
    DWORD dwNumberOfProcessors{4};
    DWORD dwProcessorType{8664};
    DWORD dwAllocationGranularity{65536};
    uint16_t wProcessorLevel{6};
    uint16_t wProcessorRevision{0};
};

struct WIN32_FIND_DATAW {
    DWORD dwFileAttributes{0};
    uint32_t ftCreationTimeLow{0};
    uint32_t ftCreationTimeHigh{0};
    uint32_t ftLastAccessTimeLow{0};
    uint32_t ftLastAccessTimeHigh{0};
    uint32_t ftLastWriteTimeLow{0};
    uint32_t ftLastWriteTimeHigh{0};
    DWORD nFileSizeHigh{0};
    DWORD nFileSizeLow{0};
    DWORD dwReserved0{0};
    DWORD dwReserved1{0};
    wchar_t cFileName[260]{};
    wchar_t cAlternateFileName[14]{};
};

struct SYSTEMTIME {
    uint16_t wYear{2026};
    uint16_t wMonth{10};
    uint16_t wDayOfWeek{4}; // Thursday
    uint16_t wDay{1};
    uint16_t wHour{9};
    uint16_t wMinute{0};
    uint16_t wSecond{0};
    uint16_t wMilliseconds{0};
};

struct TIME_ZONE_INFORMATION {
    LONG Bias{0};
    wchar_t StandardName[32]{};
    SYSTEMTIME StandardDate{};
    LONG StandardBias{0};
    wchar_t DaylightName[32]{};
    SYSTEMTIME DaylightDate{};
    LONG DaylightBias{0};
};
using LPTIME_ZONE_INFORMATION = TIME_ZONE_INFORMATION*;


// ============================================================================
// 2. Memory Management (Heap & Virtual Memory)
// ============================================================================

/**
 * @brief Retrieves a handle to the default heap of the calling process.
 */
inline HANDLE GetProcessHeap() noexcept {
    auto* peb = ntdll::RtlGetCurrentPeb();
    if (peb && peb->processHeap != 0) {
        return reinterpret_cast<HANDLE>(peb->processHeap);
    }

    static HANDLE s_defaultProcessHeap = nullptr;
    if (!s_defaultProcessHeap) {
        s_defaultProcessHeap = ntdll::RtlCreateHeap(heap::HEAP_GROWABLE, nullptr, 0, 0x100000, nullptr, nullptr);
    }
    if (peb && peb->processHeap == 0) {
        peb->processHeap = reinterpret_cast<uint64_t>(s_defaultProcessHeap);
    }
    return s_defaultProcessHeap;
}

/**
 * @brief Allocates a block of memory from a heap.
 */
inline LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) noexcept {
    return ntdll::RtlAllocateHeap(hHeap, dwFlags, dwBytes);
}

/**
 * @brief Frees a memory block allocated from a heap.
 */
inline BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) noexcept {
    return ntdll::RtlFreeHeap(hHeap, dwFlags, lpMem) ? TRUE : FALSE;
}

/**
 * @brief Reallocates a block of memory from a heap.
 */
inline LPVOID HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes) noexcept {
    return ntdll::RtlReAllocateHeap(hHeap, dwFlags, lpMem, dwBytes);
}

/**
 * @brief Retrieves the size of a memory block allocated from a heap.
 */
inline SIZE_T HeapSize(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem) noexcept {
    return ntdll::RtlSizeHeap(hHeap, dwFlags, const_cast<LPVOID>(lpMem));
}

using HLOCAL = void*;
inline constexpr UINT LMEM_FIXED    = 0x0000;
inline constexpr UINT LMEM_MOVEABLE = 0x0002;
inline constexpr UINT LMEM_ZEROINIT = 0x0040;
inline constexpr UINT LPTR          = 0x0040;

inline HLOCAL LocalAlloc(UINT uFlags, SIZE_T uBytes) noexcept {
    DWORD flags = (uFlags & LMEM_ZEROINIT) ? heap::HEAP_ZERO_MEMORY : 0;
    return reinterpret_cast<HLOCAL>(HeapAlloc(GetProcessHeap(), flags, uBytes));
}

inline HLOCAL LocalFree(HLOCAL hMem) noexcept {
    if (hMem) {
        HeapFree(GetProcessHeap(), 0, reinterpret_cast<LPVOID>(hMem));
    }
    return nullptr;
}

/**
 * @brief Reserves, commits, or changes the state of a region of pages in virtual memory.
 */
inline LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) noexcept {
    uintptr_t base = reinterpret_cast<uintptr_t>(lpAddress);
    SIZE_T size = dwSize;
    NtStatus status = ntdll::NtAllocateVirtualMemory(
        static_cast<Handle>(-1), // CurrentProcess
        &base,
        0,
        &size,
        flAllocationType,
        flProtect
    );
    if (!NT_SUCCESS(status)) return nullptr;
    return reinterpret_cast<LPVOID>(base);
}

/**
 * @brief Releases, decommits, or releases and decommits a region of pages in virtual memory.
 */
inline BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) noexcept {
    uintptr_t base = reinterpret_cast<uintptr_t>(lpAddress);
    SIZE_T size = dwSize;
    NtStatus status = ntdll::NtFreeVirtualMemory(
        static_cast<Handle>(-1),
        &base,
        &size,
        dwFreeType
    );
    return NT_SUCCESS(status) ? TRUE : FALSE;
}

// ============================================================================
// 3. Process & Thread Management
// ============================================================================

/**
 * @brief Retrieves a pseudo handle for the current process.
 */
inline HANDLE GetCurrentProcess() noexcept {
    return reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
}

/**
 * @brief Retrieves the process identifier of the calling process.
 */
inline DWORD GetCurrentProcessId() noexcept {
    auto* teb = ntdll::RtlGetCurrentTeb();
    if (!teb || teb->clientId.uniqueProcess == 0) return 1000;
    return static_cast<DWORD>(teb->clientId.uniqueProcess);
}

/**
 * @brief Retrieves a pseudo handle for the current thread.
 */
inline HANDLE GetCurrentThread() noexcept {
    return reinterpret_cast<HANDLE>(static_cast<intptr_t>(-2));
}

/**
 * @brief Retrieves the thread identifier of the calling thread.
 */
inline DWORD GetCurrentThreadId() noexcept {
    auto* teb = ntdll::RtlGetCurrentTeb();
    if (!teb || teb->clientId.uniqueThread == 0) return 1;
    return static_cast<DWORD>(teb->clientId.uniqueThread);
}

using ExitProcessHook_t = void (*)(DWORD uExitCode);
inline ExitProcessHook_t g_ExitProcessHook = nullptr;

inline void SetExitProcessHook(ExitProcessHook_t hook) noexcept {
    g_ExitProcessHook = hook;
}

using ExitThreadHook_t = void (*)(DWORD uExitCode);
inline ExitThreadHook_t g_ExitThreadHook = nullptr;

inline void SetExitThreadHook(ExitThreadHook_t hook) noexcept {
    g_ExitThreadHook = hook;
}

/**
 * @brief Ends the calling process and all its threads.
 */
[[noreturn]] inline void ExitProcess(DWORD uExitCode) noexcept(false) {
    if (g_ExitProcessHook) {
        g_ExitProcessHook(uExitCode);
    }
    DWORD pid = GetCurrentProcessId();
    // Notify CSRSS subsystem
    csrss::CsrSubsystemServer::get().terminateProcess(pid, uExitCode);
    conhost::ConhostManager::get().freeConsole(pid);
    ntdll::NtTerminateProcess(static_cast<Handle>(-1), static_cast<NtStatus>(uExitCode));
    std::exit(static_cast<int>(uExitCode));
}

// ============================================================================
// 4. Console Management (conhost / csrss integration)
// ============================================================================

/**
 * @brief Retrieves a handle to the specified standard device (input, output, or error).
 */
inline HANDLE GetStdHandle(DWORD nStdHandle) noexcept {
    if (g_TraceApi) {
        std::cout << "[*] [TRACE] GetStdHandle(" << nStdHandle << ")\n";
    }
    auto* peb = ntdll::RtlGetCurrentPeb();
    if (!peb || peb->processParameters == 0) {
        if (nStdHandle == STD_INPUT_HANDLE) return reinterpret_cast<HANDLE>(0x10);
        if (nStdHandle == STD_OUTPUT_HANDLE) return reinterpret_cast<HANDLE>(0x14);
        if (nStdHandle == STD_ERROR_HANDLE) return reinterpret_cast<HANDLE>(0x18);
        return INVALID_HANDLE_VALUE;
    }

    auto* params = reinterpret_cast<ldr::RtlUserProcessParameters*>(peb->processParameters);
    if (nStdHandle == STD_INPUT_HANDLE) {
        return reinterpret_cast<HANDLE>(params->standardInput);
    }
    if (nStdHandle == STD_OUTPUT_HANDLE) {
        return reinterpret_cast<HANDLE>(params->standardOutput);
    }
    if (nStdHandle == STD_ERROR_HANDLE) {
        return reinterpret_cast<HANDLE>(params->standardError);
    }
    return INVALID_HANDLE_VALUE;
}

/**
 * @brief Sets the handle for the specified standard device.
 */
inline BOOL SetStdHandle(DWORD nStdHandle, HANDLE hHandle) noexcept {
    auto* peb = ntdll::RtlGetCurrentPeb();
    if (!peb || peb->processParameters == 0) return FALSE;

    auto* params = reinterpret_cast<ldr::RtlUserProcessParameters*>(peb->processParameters);
    Handle h = reinterpret_cast<Handle>(hHandle);
    if (nStdHandle == STD_INPUT_HANDLE) params->standardInput = h;
    else if (nStdHandle == STD_OUTPUT_HANDLE) params->standardOutput = h;
    else if (nStdHandle == STD_ERROR_HANDLE) params->standardError = h;
    else return FALSE;

    return TRUE;
}

/**
 * @brief Allocates a new console for the calling process.
 */
inline BOOL AllocConsole() noexcept {
    DWORD pid = GetCurrentProcessId();
    auto session = conhost::ConhostManager::get().allocateConsole(pid, L"MicaNT Console");
    if (!session) return FALSE;

    SetStdHandle(STD_INPUT_HANDLE, reinterpret_cast<HANDLE>(session->getInputHandle()));
    SetStdHandle(STD_OUTPUT_HANDLE, reinterpret_cast<HANDLE>(session->getOutputHandle()));
    SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(session->getErrorHandle()));

    csrss::CsrSubsystemServer::get().bindConsole(pid, session->getOutputHandle());
    return TRUE;
}

/**
 * @brief Detaches the calling process from its console.
 */
inline BOOL FreeConsole() noexcept {
    DWORD pid = GetCurrentProcessId();
    csrss::CsrSubsystemServer::get().unbindConsole(pid);
    return conhost::ConhostManager::get().freeConsole(pid) ? TRUE : FALSE;
}

/**
 * @brief Sets the title for the current console window.
 */
inline BOOL SetConsoleTitleW(LPCWSTR lpConsoleTitle) noexcept {
    if (!lpConsoleTitle) return FALSE;
    DWORD pid = GetCurrentProcessId();
    auto session = conhost::ConhostManager::get().getConsole(pid);
    if (!session) return FALSE;
    session->setTitle(lpConsoleTitle);
    return TRUE;
}

/**
 * @brief Retrieves the title for the current console window.
 */
inline DWORD GetConsoleTitleW(LPWSTR lpConsoleTitle, DWORD nSize) noexcept {
    if (!lpConsoleTitle || nSize == 0) return 0;
    DWORD pid = GetCurrentProcessId();
    auto session = conhost::ConhostManager::get().getConsole(pid);
    if (!session) return 0;

    const std::wstring& title = session->getTitle();
    DWORD copyLen = std::min<DWORD>(nSize - 1, static_cast<DWORD>(title.length()));
    for (DWORD i = 0; i < copyLen; ++i) {
        lpConsoleTitle[i] = title[i];
    }
    lpConsoleTitle[copyLen] = L'\0';
    return copyLen;
}

/**
 * @brief Writes a character string to a console screen buffer beginning at current cursor.
 */
inline BOOL WriteConsoleW(
    HANDLE hConsoleOutput,
    const void* lpBuffer,
    DWORD nNumberOfCharsToWrite,
    DWORD* lpNumberOfCharsWritten,
    void* lpReserved = nullptr
) noexcept {
    (void)lpReserved;
    if (!lpBuffer || nNumberOfCharsToWrite == 0) return FALSE;

    if (g_TraceApi) {
        std::cout << "[*] [TRACE] WriteConsoleW chars=" << nNumberOfCharsToWrite << "\n";
    }

    DWORD pid = GetCurrentProcessId();
    auto session = conhost::ConhostManager::get().getConsole(pid);
    if (session) {
        std::wstring_view sv(reinterpret_cast<const wchar_t*>(lpBuffer), nNumberOfCharsToWrite);
        session->writeOutput(sv);
    }

    FILE* targetStream = (hConsoleOutput == GetStdHandle(STD_ERROR_HANDLE) || hConsoleOutput == reinterpret_cast<HANDLE>(0x18)) ? stderr : stdout;
    const auto* wchars = reinterpret_cast<const wchar_t*>(lpBuffer);
    for (DWORD i = 0; i < nNumberOfCharsToWrite; ++i) {
        wchar_t ch = wchars[i];
        if (ch < 0x80) {
            std::fputc(static_cast<char>(ch), targetStream);
        } else if (ch < 0x800) {
            std::fputc(0xC0 | (ch >> 6), targetStream);
            std::fputc(0x80 | (ch & 0x3F), targetStream);
        } else {
            std::fputc(0xE0 | (ch >> 12), targetStream);
            std::fputc(0x80 | ((ch >> 6) & 0x3F), targetStream);
            std::fputc(0x80 | (ch & 0x3F), targetStream);
        }
    }
    std::fflush(targetStream);

    if (lpNumberOfCharsWritten) {
        *lpNumberOfCharsWritten = nNumberOfCharsToWrite;
    }
    return TRUE;
}

// ============================================================================
// 5. File I/O
// ============================================================================

/**
 * @brief Creates or opens a file or I/O device.
 */
inline HANDLE CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    void* lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile = nullptr
) noexcept {
    (void)dwShareMode;
    (void)lpSecurityAttributes;
    (void)dwFlagsAndAttributes;
    (void)hTemplateFile;

    if (!lpFileName) return INVALID_HANDLE_VALUE;

    UnicodeString uniPath(lpFileName);
    ObjectAttributes objAttr{};
    objAttr.objectName = &uniPath;

    Handle hFile = 0;
    IoStatusBlock iosb{};

    uint32_t createDisposition = 1; // FILE_OPEN
    if (dwCreationDisposition == CREATE_ALWAYS || dwCreationDisposition == CREATE_NEW) {
        createDisposition = 2; // FILE_CREATE
    } else if (dwCreationDisposition == OPEN_ALWAYS) {
        createDisposition = 3; // FILE_OPEN_IF
    }

    NtStatus status = ntdll::NtCreateFile(
        &hFile,
        dwDesiredAccess,
        &objAttr,
        &iosb,
        nullptr,
        0,
        dwShareMode,
        createDisposition,
        0,
        nullptr,
        0
    );

    if (!NT_SUCCESS(status)) return INVALID_HANDLE_VALUE;
    return reinterpret_cast<HANDLE>(hFile);
}

/**
 * @brief Reads data from the specified file or input device.
 */
inline BOOL ReadFile(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    DWORD* lpNumberOfBytesRead,
    void* lpOverlapped = nullptr
) noexcept {
    (void)lpOverlapped;
    if (!lpBuffer) return FALSE;

    IoStatusBlock iosb{};
    NtStatus status = ntdll::NtReadFile(
        reinterpret_cast<Handle>(hFile),
        0, nullptr, nullptr,
        &iosb,
        lpBuffer,
        nNumberOfBytesToRead,
        nullptr, nullptr
    );

    if (lpNumberOfBytesRead) {
        *lpNumberOfBytesRead = static_cast<DWORD>(iosb.information);
    }
    if (!NT_SUCCESS(status) || status == NtStatus::Timeout) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief Writes data to the specified file or output device.
 */
inline BOOL WriteFile(
    HANDLE hFile,
    LPCVOID lpBuffer,
    DWORD nNumberOfBytesToWrite,
    DWORD* lpNumberOfBytesWritten,
    void* lpOverlapped = nullptr
) noexcept {
    (void)lpOverlapped;
    if (!lpBuffer) return FALSE;

    if (g_TraceApi) {
        std::cout << "[*] [TRACE] WriteFile h=" << hFile << " bytes=" << nNumberOfBytesToWrite << "\n";
    }

    if (hFile == GetStdHandle(STD_OUTPUT_HANDLE) || hFile == reinterpret_cast<HANDLE>(0x14) || hFile == reinterpret_cast<HANDLE>(1) ||
        hFile == GetStdHandle(STD_ERROR_HANDLE) || hFile == reinterpret_cast<HANDLE>(0x18) || hFile == reinterpret_cast<HANDLE>(2)) {
        FILE* stream = (hFile == GetStdHandle(STD_ERROR_HANDLE) || hFile == reinterpret_cast<HANDLE>(0x18) || hFile == reinterpret_cast<HANDLE>(2)) ? stderr : stdout;
        size_t written = std::fwrite(lpBuffer, 1, nNumberOfBytesToWrite, stream);
        std::fflush(stream);
        if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = static_cast<DWORD>(written);
        return TRUE;
    }

    IoStatusBlock iosb{};
    NtStatus status = ntdll::NtWriteFile(
        reinterpret_cast<Handle>(hFile),
        0, nullptr, nullptr,
        &iosb,
        const_cast<void*>(lpBuffer),
        nNumberOfBytesToWrite,
        nullptr, nullptr
    );

    if (lpNumberOfBytesWritten) {
        *lpNumberOfBytesWritten = static_cast<DWORD>(iosb.information);
    }
    if (!NT_SUCCESS(status) || status == NtStatus::Timeout) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief Closes an open object handle.
 */
inline BOOL CloseHandle(HANDLE hObject) noexcept {
    if (hObject == nullptr || hObject == INVALID_HANDLE_VALUE) return FALSE;
    NtStatus status = ntdll::NtClose(reinterpret_cast<Handle>(hObject));
    return NT_SUCCESS(status) ? TRUE : FALSE;
}

// ============================================================================
// 5.1 Named Pipes & Mailslots Inter-Process Communication (IPC)
// ============================================================================

inline constexpr DWORD PIPE_ACCESS_INBOUND         = 0x00000001;
inline constexpr DWORD PIPE_ACCESS_OUTBOUND        = 0x00000002;
inline constexpr DWORD PIPE_ACCESS_DUPLEX          = 0x00000003;

inline constexpr DWORD PIPE_WAIT                   = 0x00000000;
inline constexpr DWORD PIPE_NOWAIT                 = 0x00000001;
inline constexpr DWORD PIPE_READMODE_BYTE          = 0x00000000;
inline constexpr DWORD PIPE_READMODE_MESSAGE       = 0x00000002;
inline constexpr DWORD PIPE_TYPE_BYTE              = 0x00000000;
inline constexpr DWORD PIPE_TYPE_MESSAGE           = 0x00000004;

inline constexpr DWORD PIPE_CLIENT_END             = 0x00000000;
inline constexpr DWORD PIPE_SERVER_END             = 0x00000001;
inline constexpr DWORD PIPE_UNLIMITED_INSTANCES    = 255;

inline constexpr DWORD NMPWAIT_WAIT_FOREVER        = 0xFFFFFFFF;
inline constexpr DWORD NMPWAIT_NOWAIT              = 0x00000001;
inline constexpr DWORD NMPWAIT_USE_DEFAULT_WAIT    = 0x00000000;

inline constexpr DWORD MAILSLOT_NO_MESSAGE         = static_cast<DWORD>(-1);
inline constexpr DWORD MAILSLOT_WAIT_FOREVER       = static_cast<DWORD>(-1);

inline constexpr DWORD ERROR_PIPE_BUSY             = 231;
inline constexpr DWORD ERROR_NO_DATA               = 232;
inline constexpr DWORD ERROR_PIPE_NOT_CONNECTED    = 233;
inline constexpr DWORD ERROR_MORE_DATA             = 234;
inline constexpr DWORD ERROR_PIPE_CONNECTED        = 535;
inline constexpr DWORD ERROR_PIPE_LISTENING        = 536;
inline constexpr DWORD ERROR_BROKEN_PIPE           = 109;

inline HANDLE CreateNamedPipeW(
    LPCWSTR lpName,
    DWORD dwOpenMode,
    DWORD dwPipeMode,
    DWORD nMaxInstances,
    DWORD nOutBufferSize,
    DWORD nInBufferSize,
    DWORD nDefaultTimeOut,
    void* lpSecurityAttributes = nullptr
) noexcept {
    (void)lpSecurityAttributes;
    if (!lpName) {
        SetLastError(87); // ERROR_INVALID_PARAMETER
        return INVALID_HANDLE_VALUE;
    }

    UnicodeString uniName(lpName);
    ObjectAttributes objAttr{};
    objAttr.objectName = &uniName;

    Handle hPipe = 0;
    IoStatusBlock iosb{};

    uint32_t type = (dwPipeMode & PIPE_TYPE_MESSAGE) ? 1 : 0;
    uint32_t readMode = (dwPipeMode & PIPE_READMODE_MESSAGE) ? 1 : 0;
    uint32_t nonBlocking = (dwPipeMode & PIPE_NOWAIT) ? 1 : 0;

    LargeInteger timeout{};
    timeout.quadPart = -static_cast<int64_t>(nDefaultTimeOut) * 10000;

    NtStatus status = ntdll::NtCreateNamedPipeFile(
        &hPipe,
        dwOpenMode,
        &objAttr,
        &iosb,
        0,
        2,
        0,
        type,
        readMode,
        nonBlocking,
        nMaxInstances,
        nInBufferSize,
        nOutBufferSize,
        &timeout
    );

    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return INVALID_HANDLE_VALUE;
    }

    return reinterpret_cast<HANDLE>(hPipe);
}

inline BOOL ConnectNamedPipe(
    HANDLE hNamedPipe,
    void* lpOverlapped = nullptr
) noexcept {
    (void)lpOverlapped;
    if (hNamedPipe == nullptr || hNamedPipe == INVALID_HANDLE_VALUE) {
        SetLastError(6); // ERROR_INVALID_HANDLE
        return FALSE;
    }

    fs::FileObject* fileObj = sys::LookupKernelFileObject(reinterpret_cast<Handle>(hNamedPipe));
    if (!fileObj || !fileObj->getFsContext()) {
        SetLastError(6); // ERROR_INVALID_HANDLE
        return FALSE;
    }

    auto* pipeInst = static_cast<npfs::NamedPipeInstance*>(fileObj->getFsContext());
    NtStatus status = pipeInst->connectServer(npfs::NMPWAIT_WAIT_FOREVER);
    if (status == NtStatus::PipeConnected) {
        SetLastError(ERROR_PIPE_CONNECTED);
        return FALSE;
    }

    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }

    return TRUE;
}

inline BOOL DisconnectNamedPipe(
    HANDLE hNamedPipe
) noexcept {
    if (hNamedPipe == nullptr || hNamedPipe == INVALID_HANDLE_VALUE) {
        SetLastError(6); // ERROR_INVALID_HANDLE
        return FALSE;
    }

    fs::FileObject* fileObj = sys::LookupKernelFileObject(reinterpret_cast<Handle>(hNamedPipe));
    if (!fileObj || !fileObj->getFsContext()) {
        SetLastError(6); // ERROR_INVALID_HANDLE
        return FALSE;
    }

    auto* pipeInst = static_cast<npfs::NamedPipeInstance*>(fileObj->getFsContext());
    NtStatus status = pipeInst->disconnectServer();
    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }

    return TRUE;
}

inline BOOL WaitNamedPipeW(
    LPCWSTR lpNamedPipeName,
    DWORD nTimeOut
) noexcept {
    if (!lpNamedPipeName) {
        SetLastError(87);
        return FALSE;
    }

    NtStatus status = npfs::NamedPipeFileSystem::get().waitNamedPipe(lpNamedPipeName, nTimeOut);
    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }
    return TRUE;
}

inline BOOL PeekNamedPipe(
    HANDLE hNamedPipe,
    LPVOID lpBuffer,
    DWORD nBufferSize,
    DWORD* lpBytesRead,
    DWORD* lpTotalBytesAvail,
    DWORD* lpBytesLeftThisMessage
) noexcept {
    if (hNamedPipe == nullptr || hNamedPipe == INVALID_HANDLE_VALUE) {
        SetLastError(6); // ERROR_INVALID_HANDLE
        return FALSE;
    }

    fs::FileObject* fileObj = sys::LookupKernelFileObject(reinterpret_cast<Handle>(hNamedPipe));
    if (!fileObj || !fileObj->getFsContext()) {
        SetLastError(6);
        return FALSE;
    }

    auto* pipeInst = static_cast<npfs::NamedPipeInstance*>(fileObj->getFsContext());
    bool isServer = (reinterpret_cast<uintptr_t>(fileObj->getFsContext2()) == 1);

    uint32_t bytesRead = 0;
    uint32_t totalAvail = 0;
    uint32_t leftMsg = 0;

    NtStatus status = pipeInst->peek(
        isServer, lpBuffer, nBufferSize,
        &bytesRead, &totalAvail, &leftMsg
    );

    if (lpBytesRead) *lpBytesRead = bytesRead;
    if (lpTotalBytesAvail) *lpTotalBytesAvail = totalAvail;
    if (lpBytesLeftThisMessage) *lpBytesLeftThisMessage = leftMsg;

    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }
    return TRUE;
}

inline BOOL TransactNamedPipe(
    HANDLE hNamedPipe,
    LPVOID lpInBuffer,
    DWORD nInBufferSize,
    LPVOID lpOutBuffer,
    DWORD nOutBufferSize,
    DWORD* lpBytesRead,
    void* lpOverlapped = nullptr
) noexcept {
    (void)lpOverlapped;
    if (hNamedPipe == nullptr || hNamedPipe == INVALID_HANDLE_VALUE || !lpInBuffer || !lpOutBuffer) {
        SetLastError(87);
        return FALSE;
    }

    fs::FileObject* fileObj = sys::LookupKernelFileObject(reinterpret_cast<Handle>(hNamedPipe));
    if (!fileObj || !fileObj->getFsContext()) {
        SetLastError(6);
        return FALSE;
    }

    auto* pipeInst = static_cast<npfs::NamedPipeInstance*>(fileObj->getFsContext());
    bool isServer = (reinterpret_cast<uintptr_t>(fileObj->getFsContext2()) == 1);

    uint32_t bytesRead = 0;
    NtStatus status = pipeInst->transact(
        isServer, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, bytesRead
    );

    if (lpBytesRead) *lpBytesRead = bytesRead;

    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }
    return TRUE;
}

inline BOOL GetNamedPipeInfo(
    HANDLE hNamedPipe,
    DWORD* lpFlags,
    DWORD* lpOutBufferSize,
    DWORD* lpInBufferSize,
    DWORD* lpMaxInstances
) noexcept {
    if (hNamedPipe == nullptr || hNamedPipe == INVALID_HANDLE_VALUE) {
        SetLastError(6);
        return FALSE;
    }

    fs::FileObject* fileObj = sys::LookupKernelFileObject(reinterpret_cast<Handle>(hNamedPipe));
    if (!fileObj || !fileObj->getFsContext()) {
        SetLastError(6);
        return FALSE;
    }

    auto* pipeInst = static_cast<npfs::NamedPipeInstance*>(fileObj->getFsContext());
    bool isServer = (reinterpret_cast<uintptr_t>(fileObj->getFsContext2()) == 1);

    if (lpFlags) {
        *lpFlags = (isServer ? PIPE_SERVER_END : PIPE_CLIENT_END) |
                   (pipeInst->getPipeMode() & PIPE_TYPE_MESSAGE);
    }
    if (lpOutBufferSize) *lpOutBufferSize = pipeInst->getOutBufferSize();
    if (lpInBufferSize) *lpInBufferSize = pipeInst->getInBufferSize();
    if (lpMaxInstances) *lpMaxInstances = pipeInst->getMaxInstances();

    return TRUE;
}

inline BOOL GetNamedPipeHandleStateW(
    HANDLE hNamedPipe,
    DWORD* lpState,
    DWORD* lpCurInstances,
    DWORD* lpMaxCollectionCount,
    DWORD* lpCollectDataTimeout,
    LPWSTR lpUserName,
    DWORD nMaxUserNameSize
) noexcept {
    (void)lpMaxCollectionCount;
    (void)lpCollectDataTimeout;
    (void)lpUserName;
    (void)nMaxUserNameSize;
    if (hNamedPipe == nullptr || hNamedPipe == INVALID_HANDLE_VALUE) {
        SetLastError(6);
        return FALSE;
    }

    fs::FileObject* fileObj = sys::LookupKernelFileObject(reinterpret_cast<Handle>(hNamedPipe));
    if (!fileObj || !fileObj->getFsContext()) {
        SetLastError(6);
        return FALSE;
    }

    auto* pipeInst = static_cast<npfs::NamedPipeInstance*>(fileObj->getFsContext());
    if (lpState) *lpState = pipeInst->getPipeMode();
    if (lpCurInstances) *lpCurInstances = 1;

    return TRUE;
}

inline BOOL SetNamedPipeHandleState(
    HANDLE hNamedPipe,
    DWORD* lpMode,
    DWORD* lpMaxCollectionCount,
    DWORD* lpCollectDataTimeout
) noexcept {
    (void)lpMaxCollectionCount;
    (void)lpCollectDataTimeout;
    if (hNamedPipe == nullptr || hNamedPipe == INVALID_HANDLE_VALUE) {
        SetLastError(6);
        return FALSE;
    }

    fs::FileObject* fileObj = sys::LookupKernelFileObject(reinterpret_cast<Handle>(hNamedPipe));
    if (!fileObj || !fileObj->getFsContext()) {
        SetLastError(6);
        return FALSE;
    }

    if (lpMode) {
        auto* pipeInst = static_cast<npfs::NamedPipeInstance*>(fileObj->getFsContext());
        pipeInst->setMode(*lpMode);
    }

    return TRUE;
}

inline HANDLE CreateMailslotW(
    LPCWSTR lpName,
    DWORD nMaxMessageSize,
    DWORD lReadTimeout,
    void* lpSecurityAttributes = nullptr
) noexcept {
    (void)lpSecurityAttributes;
    if (!lpName) {
        SetLastError(87);
        return INVALID_HANDLE_VALUE;
    }

    UnicodeString uniName(lpName);
    ObjectAttributes objAttr{};
    objAttr.objectName = &uniName;

    Handle hSlot = 0;
    IoStatusBlock iosb{};

    LargeInteger timeout{};
    timeout.quadPart = (lReadTimeout == MAILSLOT_WAIT_FOREVER) ? -1 : -static_cast<int64_t>(lReadTimeout) * 10000;

    NtStatus status = ntdll::NtCreateMailslotFile(
        &hSlot,
        GENERIC_READ | FILE_SHARE_READ,
        &objAttr,
        &iosb,
        0,
        0,
        nMaxMessageSize,
        &timeout
    );

    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return INVALID_HANDLE_VALUE;
    }

    return reinterpret_cast<HANDLE>(hSlot);
}

inline BOOL GetMailslotInfo(
    HANDLE hMailslot,
    DWORD* lpMaxMessageSize,
    DWORD* lpNextSize,
    DWORD* lpMessageCount,
    DWORD* lpReadTimeout
) noexcept {
    if (hMailslot == nullptr || hMailslot == INVALID_HANDLE_VALUE) {
        SetLastError(6);
        return FALSE;
    }

    fs::FileObject* fileObj = sys::LookupKernelFileObject(reinterpret_cast<Handle>(hMailslot));
    if (!fileObj || !fileObj->getFsContext()) {
        SetLastError(6);
        return FALSE;
    }

    auto* slot = static_cast<npfs::Mailslot*>(fileObj->getFsContext());
    slot->getInfo(lpMaxMessageSize, lpNextSize, lpMessageCount, lpReadTimeout);
    return TRUE;
}

inline BOOL SetMailslotInfo(
    HANDLE hMailslot,
    DWORD lReadTimeout
) noexcept {
    if (hMailslot == nullptr || hMailslot == INVALID_HANDLE_VALUE) {
        SetLastError(6);
        return FALSE;
    }

    fs::FileObject* fileObj = sys::LookupKernelFileObject(reinterpret_cast<Handle>(hMailslot));
    if (!fileObj || !fileObj->getFsContext()) {
        SetLastError(6);
        return FALSE;
    }

    auto* slot = static_cast<npfs::Mailslot*>(fileObj->getFsContext());
    slot->setReadTimeout(lReadTimeout);
    return TRUE;
}

/**
 * @brief Creates or opens a named or unnamed event object.
 */
inline HANDLE CreateEventW(
    void* lpEventAttributes,
    BOOL bManualReset,
    BOOL bInitialState,
    LPCWSTR lpName
) noexcept {
    (void)lpEventAttributes;
    Handle hEvent = 0;
    UnicodeString uniName(lpName);
    ObjectAttributes objAttr{};
    if (lpName) objAttr.objectName = &uniName;

    NtStatus status = ntdll::NtCreateEvent(
        &hEvent,
        0x1F0003, // EVENT_ALL_ACCESS
        &objAttr,
        bManualReset ? 0 : 1, // NotificationEvent = 0, SynchronizationEvent = 1
        bInitialState ? true : false
    );

    if (!NT_SUCCESS(status)) return nullptr;
    return reinterpret_cast<HANDLE>(hEvent);
}

/**
 * @brief Sets the specified event object to the signaled state.
 */
inline BOOL SetEvent(HANDLE hEvent) noexcept {
    if (!hEvent) return FALSE;
    NtStatus status = ntdll::NtSetEvent(reinterpret_cast<Handle>(hEvent), nullptr);
    return NT_SUCCESS(status) ? TRUE : FALSE;
}

/**
 * @brief Sets the specified event object to the nonsignaled state.
 */
inline BOOL ResetEvent(HANDLE hEvent) noexcept {
    if (!hEvent) return FALSE;
    NtStatus status = ntdll::NtResetEvent(reinterpret_cast<Handle>(hEvent), nullptr);
    return NT_SUCCESS(status) ? TRUE : FALSE;
}

/**
 * @brief Waits until the specified object is in the signaled state or the time-out interval elapses.
 */
inline DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) noexcept {
    if (!hHandle) return WAIT_FAILED;

    LargeInteger timeout{};
    LargeInteger* pTimeout = nullptr;
    if (dwMilliseconds != INFINITE) {
        timeout.quadPart = -static_cast<int64_t>(dwMilliseconds) * 10000; // 100ns units
        pTimeout = &timeout;
    }

    NtStatus status = ntdll::NtWaitForSingleObject(
        reinterpret_cast<Handle>(hHandle),
        false,
        pTimeout
    );

    if (status == NtStatus::Success) return WAIT_OBJECT_0;
    if (status == NtStatus::Timeout) return WAIT_TIMEOUT;
    return WAIT_FAILED;
}

/**
 * @brief Waits until one or all of the specified objects are in the signaled state.
 */
inline DWORD WaitForMultipleObjects(
    DWORD nCount,
    const HANDLE* lpHandles,
    BOOL bWaitAll,
    DWORD dwMilliseconds
) noexcept {
    if (nCount == 0 || !lpHandles || nCount > MAXIMUM_WAIT_OBJECTS) return WAIT_FAILED;

    Handle handles[MAXIMUM_WAIT_OBJECTS];
    for (DWORD i = 0; i < nCount; ++i) {
        handles[i] = reinterpret_cast<Handle>(lpHandles[i]);
    }

    LargeInteger timeout{};
    LargeInteger* pTimeout = nullptr;
    if (dwMilliseconds != INFINITE) {
        timeout.quadPart = -static_cast<int64_t>(dwMilliseconds) * 10000;
        pTimeout = &timeout;
    }

    NtStatus status = ntdll::NtWaitForMultipleObjects(
        nCount,
        handles,
        bWaitAll ? WaitType::WaitAll : WaitType::WaitAny,
        false,
        pTimeout
    );

    if (status == NtStatus::Success) return WAIT_OBJECT_0;
    if (static_cast<uint32_t>(status) >= static_cast<uint32_t>(NtStatus::Wait0) &&
        static_cast<uint32_t>(status) < static_cast<uint32_t>(NtStatus::Wait0) + nCount) {
        return static_cast<DWORD>(status);
    }
    if (status == NtStatus::Timeout) return WAIT_TIMEOUT;
    return WAIT_FAILED;
}

// ============================================================================
// 7. Time & System Information
// ============================================================================

/**
 * @brief Suspends the execution of the current thread until the time-out interval elapses.
 */
inline void Sleep(DWORD dwMilliseconds) noexcept {
    LargeInteger interval{};
    interval.quadPart = -static_cast<int64_t>(dwMilliseconds) * 10000; // 100ns
    ntdll::NtDelayExecution(false, &interval);
}

/**
 * @brief Retrieves the number of milliseconds that have elapsed since the system was started.
 */
inline uint64_t GetTickCount64() noexcept {
    LargeInteger perf{};
    hal::HardwareAbstractionLayer::get().queryPerformanceCounter(perf);
    return static_cast<uint64_t>(perf.quadPart / 1000000ULL);
}

/**
 * @brief Retrieves information about the current system.
 */
inline void GetSystemInfo(SYSTEM_INFO* lpSystemInfo) noexcept {
    if (!lpSystemInfo) return;
    *lpSystemInfo = SYSTEM_INFO{};
}

// ============================================================================
// 8. Dynamic Linking & Module Loading
// ============================================================================

/**
 * @brief Loads the specified module into the address space of the calling process.
 */
inline HMODULE LoadLibraryW(LPCWSTR lpLibFileName) noexcept {
    if (!lpLibFileName) return nullptr;
    if (g_TraceApi) {
        std::wcout << L"[*] [TRACE] LoadLibraryW: " << lpLibFileName << L"\n";
    }
    uintptr_t modBase = 0;
    UnicodeString uniPath(lpLibFileName);
    NtStatus status = ntdll::LdrLoadDll(nullptr, 0, &uniPath, &modBase);
    if (!NT_SUCCESS(status)) return nullptr;
    return reinterpret_cast<HMODULE>(modBase);
}

inline HMODULE LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE /*hFile*/, DWORD /*dwFlags*/) noexcept {
    return LoadLibraryW(lpLibFileName);
}

/**
 * @brief Retrieves the address of an exported function or variable from the specified DLL.
 */
inline void* GetProcAddress(HMODULE hModule, LPCSTR lpProcName) noexcept {
    if (!hModule || !lpProcName) return nullptr;
    if (g_TraceApi) {
        std::cout << "[*] [TRACE] GetProcAddress: " << (lpProcName ? lpProcName : "<ord>") << "\n";
    }
    void* procAddr = nullptr;
    NtStatus status = ntdll::LdrGetProcedureAddress(reinterpret_cast<uintptr_t>(hModule), lpProcName, 0, &procAddr);
    if (!NT_SUCCESS(status)) return nullptr;
    return procAddr;
}

/**
 * @brief Frees the loaded dynamic-link library (DLL) module.
 */
inline BOOL FreeLibrary(HMODULE hLibModule) noexcept {
    (void)hLibModule;
    return TRUE;
}

// ============================================================================
// 9. Error Handling & Thread-Local Status (Defined in Section 1)
// ============================================================================

// ============================================================================
// 10. Environment, Command Line & Working Directory
// ============================================================================

static std::unordered_map<std::wstring, std::wstring> g_EnvironmentVariables = {
    { L"OS", L"MicaNT" },
    { L"SystemRoot", L"C:\\Windows" },
    { L"windir", L"C:\\Windows" },
    { L"ComSpec", L"C:\\Windows\\System32\\cmd.exe" },
    { L"PATH", L"C:\\Windows\\System32;C:\\Windows" },
    { L"NUMBER_OF_PROCESSORS", L"4" },
    { L"PROCESSOR_ARCHITECTURE", L"AMD64" },
    { L"PROCESSOR_IDENTIFIER", L"AMD64 Family 6 Model 0 Stepping 0, MicaNT" },
    { L"USERPROFILE", L"C:\\Users\\Default" },
    { L"HOMEDRIVE", L"C:" },
    { L"HOMEPATH", L"\\Users\\Default" },
    { L"PROMPT", L"$P$G" }
};
static std::wstring g_CurrentDirectory = L"C:\\Windows\\System32";
inline std::wstring g_CommandLineStorageW = L"micant.exe";
inline std::string g_CommandLineStorageA = "micant.exe";

inline void SetCurrentCommandLine(const wchar_t* wcmd, const char* acmd) noexcept {
    if (wcmd) g_CommandLineStorageW = wcmd;
    if (acmd) g_CommandLineStorageA = acmd;
}

inline LPCSTR GetCommandLineA() noexcept { return g_CommandLineStorageA.c_str(); }
inline LPCWSTR GetCommandLineW() noexcept {
    if (g_TraceApi) {
        std::wcout << L"[*] [TRACE] GetCommandLineW: " << g_CommandLineStorageW << L"\n";
    }
    return g_CommandLineStorageW.c_str();
}

inline DWORD GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize) noexcept {
    if (!lpName) {
        SetLastError(87); // ERROR_INVALID_PARAMETER
        return 0;
    }
    auto it = g_EnvironmentVariables.find(lpName);
    if (it == g_EnvironmentVariables.end()) {
        SetLastError(203); // ERROR_ENVVAR_NOT_FOUND
        return 0;
    }
    const auto& val = it->second;
    if (nSize <= val.size()) {
        return static_cast<DWORD>(val.size() + 1);
    }
    if (lpBuffer) {
        std::wcsncpy(lpBuffer, val.c_str(), nSize);
        return static_cast<DWORD>(val.size());
    }
    return 0;
}

inline DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize) noexcept {
    if (!lpName) {
        SetLastError(87);
        return 0;
    }
    std::string nameStr(lpName);
    std::wstring wName(nameStr.begin(), nameStr.end());
    std::vector<wchar_t> wBuf(nSize ? nSize : 1);
    DWORD res = GetEnvironmentVariableW(wName.c_str(), wBuf.data(), nSize);
    if (res > 0 && res < nSize && lpBuffer) {
        for (DWORD i = 0; i < res; ++i) {
            lpBuffer[i] = static_cast<char>(wBuf[i]);
        }
        lpBuffer[res] = '\0';
    }
    return res;
}

inline BOOL SetEnvironmentVariableW(LPCWSTR lpName, LPCWSTR lpValue) noexcept {
    if (!lpName) {
        SetLastError(87);
        return FALSE;
    }
    if (!lpValue) {
        g_EnvironmentVariables.erase(lpName);
    } else {
        g_EnvironmentVariables[lpName] = lpValue;
    }
    return TRUE;
}

inline BOOL SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue) noexcept {
    if (!lpName) return FALSE;
    std::string nameStr(lpName);
    std::wstring wName(nameStr.begin(), nameStr.end());
    if (!lpValue) {
        return SetEnvironmentVariableW(wName.c_str(), nullptr);
    }
    std::string valStr(lpValue);
    std::wstring wVal(valStr.begin(), valStr.end());
    return SetEnvironmentVariableW(wName.c_str(), wVal.c_str());
}

inline DWORD GetCurrentDirectoryW(DWORD nBufferLength, LPWSTR lpBuffer) noexcept {
    if (nBufferLength <= g_CurrentDirectory.size()) {
        return static_cast<DWORD>(g_CurrentDirectory.size() + 1);
    }
    if (lpBuffer) {
        std::wcsncpy(lpBuffer, g_CurrentDirectory.c_str(), nBufferLength);
        return static_cast<DWORD>(g_CurrentDirectory.size());
    }
    return 0;
}

inline DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer) noexcept {
    if (nBufferLength <= g_CurrentDirectory.size()) {
        return static_cast<DWORD>(g_CurrentDirectory.size() + 1);
    }
    if (lpBuffer) {
        for (size_t i = 0; i < g_CurrentDirectory.size(); ++i) {
            lpBuffer[i] = static_cast<char>(g_CurrentDirectory[i]);
        }
        lpBuffer[g_CurrentDirectory.size()] = '\0';
        return static_cast<DWORD>(g_CurrentDirectory.size());
    }
    return 0;
}

inline BOOL SetCurrentDirectoryW(LPCWSTR lpPathName) noexcept {
    if (!lpPathName) {
        SetLastError(87);
        return FALSE;
    }
    g_CurrentDirectory = lpPathName;
    return TRUE;
}

inline BOOL SetCurrentDirectoryA(LPCSTR lpPathName) noexcept {
    if (!lpPathName) return FALSE;
    std::string pathStr(lpPathName);
    std::wstring wPath(pathStr.begin(), pathStr.end());
    return SetCurrentDirectoryW(wPath.c_str());
}

inline DWORD GetFullPathNameW(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR* lpFilePart) noexcept {
    if (!lpFileName) return 0;
    std::wstring fullPath;
    if (lpFileName[0] == L'\\' || (lpFileName[0] != L'\0' && lpFileName[1] == L':')) {
        fullPath = lpFileName;
    } else {
        fullPath = g_CurrentDirectory + L"\\" + lpFileName;
    }
    if (nBufferLength <= fullPath.size()) {
        return static_cast<DWORD>(fullPath.size() + 1);
    }
    if (lpBuffer) {
        std::wcsncpy(lpBuffer, fullPath.c_str(), nBufferLength);
        if (lpFilePart) {
            size_t slash = fullPath.find_last_of(L"\\/");
            *lpFilePart = (slash == std::wstring::npos) ? lpBuffer : (lpBuffer + slash + 1);
        }
        return static_cast<DWORD>(fullPath.size());
    }
    return 0;
}

inline DWORD GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart) noexcept {
    if (!lpFileName) return 0;
    std::string nameStr(lpFileName);
    std::wstring wName(nameStr.begin(), nameStr.end());
    std::vector<wchar_t> wBuf(nBufferLength ? nBufferLength : 1);
    wchar_t* wPart = nullptr;
    DWORD res = GetFullPathNameW(wName.c_str(), nBufferLength, wBuf.data(), &wPart);
    if (res > 0 && res < nBufferLength && lpBuffer) {
        for (DWORD i = 0; i < res; ++i) {
            lpBuffer[i] = static_cast<char>(wBuf[i]);
        }
        lpBuffer[res] = '\0';
        if (lpFilePart && wPart) {
            *lpFilePart = lpBuffer + (wPart - wBuf.data());
        }
    }
    return res;
}

// ============================================================================
// 11. Module & Process Introspection
// ============================================================================


inline DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) noexcept {
    if (!lpFilename || nSize == 0) return 0;
    std::wstring path = L"C:\\Windows\\System32\\micant.exe";
    if (hModule == reinterpret_cast<HMODULE>(0x7FF800000000ULL)) {
        path = L"C:\\Windows\\System32\\kernel32.dll";
    } else if (hModule == reinterpret_cast<HMODULE>(0x7FF810000000ULL)) {
        path = L"C:\\Windows\\System32\\ntdll.dll";
    }
    size_t copyLen = std::min<size_t>(path.size(), nSize - 1);
    std::wcsncpy(lpFilename, path.c_str(), copyLen);
    lpFilename[copyLen] = L'\0';
    return static_cast<DWORD>(copyLen);
}

inline DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) noexcept {
    if (!lpFilename || nSize == 0) return 0;
    std::vector<wchar_t> wBuf(nSize);
    DWORD res = GetModuleFileNameW(hModule, wBuf.data(), nSize);
    for (DWORD i = 0; i < res; ++i) {
        lpFilename[i] = static_cast<char>(wBuf[i]);
    }
    lpFilename[res] = '\0';
    return res;
}

inline HMODULE GetModuleHandleW(LPCWSTR lpModuleName) noexcept {
    if (!lpModuleName) {
        if (g_CurrentExecutableBase != 0) return reinterpret_cast<HMODULE>(g_CurrentExecutableBase);
        return reinterpret_cast<HMODULE>(0x140000000ULL);
    }
    std::wstring name(lpModuleName);
    for (auto& c : name) c = static_cast<wchar_t>(std::towlower(c));
    if (name.find(L'.') == std::wstring::npos) {
        name += L".dll";
    }

    // Check already loaded modules in DynamicLoader
    for (const auto& mod : ldr::DynamicLoader::get().getLoadedModules()) {
        std::wstring modBase(mod->storedBaseName);
        for (auto& c : modBase) c = static_cast<wchar_t>(std::towlower(c));
        if (modBase == name || mod->storedFullName == name) {
            return reinterpret_cast<HMODULE>(mod->dllBase);
        }
    }

    if (name == L"kernel32.dll") {
        return reinterpret_cast<HMODULE>(0x7FF800000000ULL);
    }
    if (name == L"ntdll.dll") {
        return reinterpret_cast<HMODULE>(0x7FF810000000ULL);
    }

    // Register/load module in DynamicLoader table
    UnicodeString uniName(name.c_str());
    uintptr_t modBase = 0;
    if (NT_SUCCESS(ntdll::LdrLoadDll(nullptr, 0, &uniName, &modBase))) {
        return reinterpret_cast<HMODULE>(modBase);
    }

    if (g_CurrentExecutableBase != 0) return reinterpret_cast<HMODULE>(g_CurrentExecutableBase);
    return reinterpret_cast<HMODULE>(0x140000000ULL);
}

inline HMODULE GetModuleHandleA(LPCSTR lpModuleName) noexcept {
    if (!lpModuleName) return GetModuleHandleW(nullptr);
    std::string modStr(lpModuleName);
    std::wstring wMod(modStr.begin(), modStr.end());
    return GetModuleHandleW(wMod.c_str());
}

inline BOOL GetModuleHandleExW(DWORD /*dwFlags*/, LPCWSTR lpModuleName, HMODULE* phModule) noexcept {
    if (!phModule) return FALSE;
    *phModule = GetModuleHandleW(lpModuleName);
    return TRUE;
}

// ============================================================================
// 12. Extended File Operations, Sizing, Seeking & Directories
// ============================================================================

inline DWORD GetFileAttributesW(LPCWSTR lpFileName) noexcept {
    if (!lpFileName) return INVALID_FILE_ATTRIBUTES;
    uint32_t attrs = 0;
    NtStatus status = fs::VirtualFileSystem::get().queryFileAttributes(lpFileName, attrs);
    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return INVALID_FILE_ATTRIBUTES;
    }
    return attrs;
}

inline DWORD GetFileAttributesA(LPCSTR lpFileName) noexcept {
    if (!lpFileName) return INVALID_FILE_ATTRIBUTES;
    std::string nameStr(lpFileName);
    std::wstring wName(nameStr.begin(), nameStr.end());
    return GetFileAttributesW(wName.c_str());
}

inline BOOL SetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes) noexcept {
    if (!lpFileName) return FALSE;
    NtStatus status = fs::VirtualFileSystem::get().setFileAttributes(lpFileName, dwFileAttributes);
    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }
    return TRUE;
}

inline BOOL GetFileSizeEx(HANDLE hFile, LargeInteger* lpFileSize) noexcept {
    if (!hFile || !lpFileSize) {
        SetLastError(87);
        return FALSE;
    }
    IoStatusBlock iosb{};
    FileStandardInformation stdInfo{};
    NtStatus status = ntdll::NtQueryInformationFile(
        reinterpret_cast<Handle>(hFile),
        &iosb,
        &stdInfo,
        sizeof(stdInfo),
        FileInformationClass::FileStandardInformation
    );
    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }
    *lpFileSize = stdInfo.endOfFile;
    return TRUE;
}

inline DWORD GetFileSize(HANDLE hFile, DWORD* lpFileSizeHigh) noexcept {
    LargeInteger li{};
    if (!GetFileSizeEx(hFile, &li)) return INVALID_FILE_SIZE;
    if (lpFileSizeHigh) *lpFileSizeHigh = static_cast<DWORD>(li.highPart);
    return li.lowPart;
}

inline BOOL SetFilePointerEx(HANDLE hFile, LargeInteger liDistanceToMove, LargeInteger* lpNewFilePointer, DWORD dwMoveMethod) noexcept {
    if (!hFile) {
        SetLastError(6); // ERROR_INVALID_HANDLE
        return FALSE;
    }

    int64_t targetOffset = 0;
    if (dwMoveMethod == FILE_BEGIN) {
        targetOffset = liDistanceToMove.quadPart;
    } else if (dwMoveMethod == FILE_CURRENT) {
        IoStatusBlock iosb{};
        FilePositionInformation posInfo{};
        ntdll::NtQueryInformationFile(reinterpret_cast<Handle>(hFile), &iosb, &posInfo, sizeof(posInfo), FileInformationClass::FilePositionInformation);
        targetOffset = posInfo.currentByteOffset.quadPart + liDistanceToMove.quadPart;
    } else if (dwMoveMethod == FILE_END) {
        LargeInteger sz{};
        if (!GetFileSizeEx(hFile, &sz)) return FALSE;
        targetOffset = sz.quadPart + liDistanceToMove.quadPart;
    } else {
        SetLastError(87);
        return FALSE;
    }

    if (targetOffset < 0) targetOffset = 0;

    IoStatusBlock iosb{};
    FilePositionInformation posInfo{};
    posInfo.currentByteOffset.quadPart = targetOffset;
    NtStatus status = ntdll::NtSetInformationFile(
        reinterpret_cast<Handle>(hFile),
        &iosb,
        &posInfo,
        sizeof(posInfo),
        FileInformationClass::FilePositionInformation
    );

    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }

    if (lpNewFilePointer) {
        lpNewFilePointer->quadPart = targetOffset;
    }
    return TRUE;
}

inline DWORD SetFilePointer(HANDLE hFile, int32_t lDistanceToMove, int32_t* lpDistanceToMoveHigh, DWORD dwMoveMethod) noexcept {
    LargeInteger move{}, newPos{};
    move.lowPart = static_cast<uint32_t>(lDistanceToMove);
    move.highPart = lpDistanceToMoveHigh ? *lpDistanceToMoveHigh : (lDistanceToMove < 0 ? -1 : 0);
    if (!SetFilePointerEx(hFile, move, &newPos, dwMoveMethod)) return INVALID_FILE_SIZE;
    if (lpDistanceToMoveHigh) *lpDistanceToMoveHigh = newPos.highPart;
    return newPos.lowPart;
}

inline BOOL DeleteFileW(LPCWSTR lpFileName) noexcept {
    if (!lpFileName) return FALSE;
    NtStatus status = fs::VirtualFileSystem::get().deleteFile(lpFileName);
    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }
    return TRUE;
}

inline BOOL DeleteFileA(LPCSTR lpFileName) noexcept {
    if (!lpFileName) return FALSE;
    std::string s(lpFileName);
    std::wstring w(s.begin(), s.end());
    return DeleteFileW(w.c_str());
}

inline BOOL CopyFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, BOOL bFailIfExists) noexcept {
    if (!lpExistingFileName || !lpNewFileName) {
        SetLastError(87);
        return FALSE;
    }
    if (bFailIfExists) {
        uint32_t attrs = 0;
        if (NT_SUCCESS(fs::VirtualFileSystem::get().queryFileAttributes(lpNewFileName, attrs))) {
            SetLastError(80); // ERROR_FILE_EXISTS
            return FALSE;
        }
    }
    std::shared_ptr<fs::FileObject> srcObj;
    NtStatus st = fs::VirtualFileSystem::get().createOrOpenFile(lpExistingFileName, fs::FILE_GENERIC_READ, fs::FILE_OPEN, srcObj);
    if (!NT_SUCCESS(st) || !srcObj) {
        SetLastError(ntdll::RtlNtStatusToDosError(st));
        return FALSE;
    }
    std::shared_ptr<fs::FileObject> dstObj;
    st = fs::VirtualFileSystem::get().createOrOpenFile(lpNewFileName, fs::FILE_GENERIC_WRITE, fs::FILE_OVERWRITE_IF, dstObj);
    if (!NT_SUCCESS(st) || !dstObj) {
        SetLastError(ntdll::RtlNtStatusToDosError(st));
        return FALSE;
    }
    uint32_t written = 0;
    st = fs::VirtualFileSystem::get().writeFile(dstObj.get(), srcObj->getData().data(), static_cast<uint32_t>(srcObj->getData().size()), nullptr, written);
    fs::VirtualFileSystem::get().closeFile(srcObj.get());
    fs::VirtualFileSystem::get().closeFile(dstObj.get());
    if (!NT_SUCCESS(st)) {
        SetLastError(ntdll::RtlNtStatusToDosError(st));
        return FALSE;
    }
    return TRUE;
}

inline BOOL CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists) noexcept {
    if (!lpExistingFileName || !lpNewFileName) return FALSE;
    std::string sSrc(lpExistingFileName);
    std::string sDst(lpNewFileName);
    std::wstring wSrc(sSrc.begin(), sSrc.end());
    std::wstring wDst(sDst.begin(), sDst.end());
    return CopyFileW(wSrc.c_str(), wDst.c_str(), bFailIfExists);
}

inline BOOL MoveFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName) noexcept {
    if (!lpExistingFileName || !lpNewFileName) {
        SetLastError(87);
        return FALSE;
    }
    if (!CopyFileW(lpExistingFileName, lpNewFileName, FALSE)) {
        return FALSE;
    }
    return DeleteFileW(lpExistingFileName);
}

inline BOOL MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName) noexcept {
    if (!lpExistingFileName || !lpNewFileName) return FALSE;
    std::string sSrc(lpExistingFileName);
    std::string sDst(lpNewFileName);
    std::wstring wSrc(sSrc.begin(), sSrc.end());
    std::wstring wDst(sDst.begin(), sDst.end());
    return MoveFileW(wSrc.c_str(), wDst.c_str());
}

inline BOOL MoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD /*dwFlags*/) noexcept {
    return MoveFileW(lpExistingFileName, lpNewFileName);
}

inline BOOL CreateDirectoryW(LPCWSTR lpPathName, void* /*lpSecurityAttributes*/) noexcept {
    if (!lpPathName) return FALSE;
    NtStatus status = fs::VirtualFileSystem::get().createDirectory(lpPathName);
    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }
    return TRUE;
}

inline BOOL CreateDirectoryA(LPCSTR lpPathName, void* lpSecurityAttributes) noexcept {
    if (!lpPathName) return FALSE;
    std::string s(lpPathName);
    std::wstring w(s.begin(), s.end());
    return CreateDirectoryW(w.c_str(), lpSecurityAttributes);
}

inline BOOL RemoveDirectoryW(LPCWSTR lpPathName) noexcept {
    if (!lpPathName) return FALSE;
    NtStatus status = fs::VirtualFileSystem::get().removeDirectory(lpPathName);
    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }
    return TRUE;
}

inline BOOL RemoveDirectoryA(LPCSTR lpPathName) noexcept {
    if (!lpPathName) return FALSE;
    std::string s(lpPathName);
    std::wstring w(s.begin(), s.end());
    return RemoveDirectoryW(w.c_str());
}

// FindFile Context Registry
struct FindFileContext {
    std::wstring directoryPath;
    std::vector<fs::VirtualFileSystem::DirectoryEntry> entries;
    size_t currentIndex{0};
};

static std::unordered_map<HANDLE, std::unique_ptr<FindFileContext>> g_FindContexts;
static uintptr_t g_NextFindHandle = 0x500;

inline HANDLE FindFirstFileW(LPCWSTR lpFileName, WIN32_FIND_DATAW* lpFindFileData) noexcept {
    if (!lpFileName || !lpFindFileData) {
        SetLastError(87);
        return INVALID_HANDLE_VALUE;
    }

    std::wstring path(lpFileName);
    size_t lastSlash = path.find_last_of(L"\\/");
    std::wstring dir = (lastSlash == std::wstring::npos) ? L"" : path.substr(0, lastSlash);

    std::vector<fs::VirtualFileSystem::DirectoryEntry> entries;
    NtStatus status = fs::VirtualFileSystem::get().queryDirectory(dir, entries);
    if (!NT_SUCCESS(status) || entries.empty()) {
        SetLastError(2); // ERROR_FILE_NOT_FOUND
        return INVALID_HANDLE_VALUE;
    }

    auto ctx = std::make_unique<FindFileContext>();
    ctx->directoryPath = dir;
    ctx->entries = std::move(entries);
    ctx->currentIndex = 0;

    const auto& first = ctx->entries[0];
    *lpFindFileData = WIN32_FIND_DATAW{};
    lpFindFileData->dwFileAttributes = first.attributes;
    lpFindFileData->nFileSizeLow = static_cast<DWORD>(first.size);
    std::wcsncpy(lpFindFileData->cFileName, first.name.c_str(), 259);

    HANDLE hFind = reinterpret_cast<HANDLE>(g_NextFindHandle++);
    g_FindContexts[hFind] = std::move(ctx);
    return hFind;
}

inline BOOL FindNextFileW(HANDLE hFindFile, WIN32_FIND_DATAW* lpFindFileData) noexcept {
    if (!hFindFile || !lpFindFileData) {
        SetLastError(87);
        return FALSE;
    }
    auto it = g_FindContexts.find(hFindFile);
    if (it == g_FindContexts.end()) {
        SetLastError(6); // ERROR_INVALID_HANDLE
        return FALSE;
    }

    auto& ctx = it->second;
    ctx->currentIndex++;
    if (ctx->currentIndex >= ctx->entries.size()) {
        SetLastError(18); // ERROR_NO_MORE_FILES
        return FALSE;
    }

    const auto& entry = ctx->entries[ctx->currentIndex];
    *lpFindFileData = WIN32_FIND_DATAW{};
    lpFindFileData->dwFileAttributes = entry.attributes;
    lpFindFileData->nFileSizeLow = static_cast<DWORD>(entry.size);
    std::wcsncpy(lpFindFileData->cFileName, entry.name.c_str(), 259);
    return TRUE;
}

inline BOOL FindClose(HANDLE hFindFile) noexcept {
    if (!hFindFile) return FALSE;
    return g_FindContexts.erase(hFindFile) > 0 ? TRUE : FALSE;
}

// ============================================================================
// 13. High-Precision Timing & System Clock
// ============================================================================

inline BOOL QueryPerformanceCounter(LargeInteger* lpPerformanceCount) noexcept {
    if (!lpPerformanceCount) return FALSE;
    ntdll::NtQueryPerformanceCounter(lpPerformanceCount, nullptr);
    return TRUE;
}

inline BOOL QueryPerformanceFrequency(LargeInteger* lpFrequency) noexcept {
    if (!lpFrequency) return FALSE;
    LargeInteger count{};
    ntdll::NtQueryPerformanceCounter(&count, lpFrequency);
    return TRUE;
}

inline void GetSystemTime(SYSTEMTIME* lpSystemTime) noexcept {
    if (!lpSystemTime) return;
    *lpSystemTime = SYSTEMTIME{};
}

inline void GetLocalTime(SYSTEMTIME* lpSystemTime) noexcept {
    GetSystemTime(lpSystemTime);
}

// ============================================================================
// 14. Console Control Enhancements
// ============================================================================

inline BOOL GetConsoleMode(HANDLE /*hConsoleHandle*/, DWORD* lpMode) noexcept {
    if (!lpMode) return FALSE;
    *lpMode = ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return TRUE;
}

inline BOOL SetConsoleMode(HANDLE /*hConsoleHandle*/, DWORD /*dwMode*/) noexcept {
    return TRUE;
}

inline uint32_t GetConsoleOutputCP() noexcept {
    return 65001; // UTF-8
}

inline std::shared_ptr<conhost::ConsoleSession> GetActiveConsoleSession() noexcept {
    DWORD pid = GetCurrentProcessId();
    auto session = conhost::ConhostManager::get().getConsole(pid);
    if (!session) {
        session = conhost::ConhostManager::get().allocateConsole(pid);
    }
    return session;
}

inline BOOL GetConsoleScreenBufferInfo(HANDLE /*hConsoleOutput*/, conhost::ConsoleScreenBufferInfo* lpConsoleScreenBufferInfo) noexcept {
    if (!lpConsoleScreenBufferInfo) return FALSE;
    auto session = GetActiveConsoleSession();
    if (!session) return FALSE;
    *lpConsoleScreenBufferInfo = session->getScreenBuffer().getScreenBufferInfo();
    return TRUE;
}

inline BOOL SetConsoleTextAttribute(HANDLE /*hConsoleOutput*/, uint16_t wAttributes) noexcept {
    auto session = GetActiveConsoleSession();
    if (!session) return FALSE;
    session->getScreenBuffer().setAttributes(wAttributes);
    return TRUE;
}

inline BOOL SetConsoleCursorPosition(HANDLE /*hConsoleOutput*/, conhost::Coord dwCursorPosition) noexcept {
    auto session = GetActiveConsoleSession();
    if (!session) return FALSE;
    session->getScreenBuffer().setCursorPosition(dwCursorPosition);
    return TRUE;
}

inline BOOL WriteConsoleA(
    HANDLE hConsoleOutput,
    const void* lpBuffer,
    DWORD nNumberOfCharsToWrite,
    DWORD* lpNumberOfCharsWritten,
    LPVOID /*lpReserved*/
) noexcept {
    if (!lpBuffer || nNumberOfCharsToWrite == 0) {
        if (lpNumberOfCharsWritten) *lpNumberOfCharsWritten = 0;
        return TRUE;
    }
    const char* str = reinterpret_cast<const char*>(lpBuffer);
    std::wstring wstr(str, str + nNumberOfCharsToWrite);
    return WriteConsoleW(hConsoleOutput, wstr.data(), nNumberOfCharsToWrite, lpNumberOfCharsWritten, nullptr);
}

// ============================================================================
// 15. Memory Mapping & Section Objects
// ============================================================================

inline HANDLE CreateFileMappingW(
    HANDLE hFile,
    void* /*lpFileMappingAttributes*/,
    DWORD flProtect,
    DWORD dwMaximumSizeHigh,
    DWORD dwMaximumSizeLow,
    LPCWSTR lpName
) noexcept {
    LargeInteger maxSz{};
    maxSz.lowPart = dwMaximumSizeLow;
    maxSz.highPart = static_cast<int32_t>(dwMaximumSizeHigh);

    UnicodeString uniName(lpName);
    ObjectAttributes objAttr{};
    if (lpName) objAttr.objectName = &uniName;

    Handle hSec = 0;
    Handle fHandle = (hFile == INVALID_HANDLE_VALUE) ? 0 : reinterpret_cast<Handle>(hFile);
    NtStatus status = ntdll::NtCreateSection(
        &hSec,
        0xF001F, // SECTION_ALL_ACCESS
        &objAttr,
        (maxSz.quadPart > 0) ? &maxSz : nullptr,
        flProtect ? flProtect : 0x04 /* PAGE_READWRITE */,
        0x08000000 /* SEC_COMMIT */,
        fHandle
    );

    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return nullptr;
    }
    return reinterpret_cast<HANDLE>(hSec);
}

inline LPVOID MapViewOfFile(
    HANDLE hFileMappingObject,
    DWORD /*dwDesiredAccess*/,
    DWORD /*dwFileOffsetHigh*/,
    DWORD /*dwFileOffsetLow*/,
    SIZE_T dwNumberOfBytesToMap
) noexcept {
    if (!hFileMappingObject) return nullptr;
    uintptr_t baseAddr = 0;
    size_t viewSize = dwNumberOfBytesToMap;
    NtStatus status = ntdll::NtMapViewOfSection(
        reinterpret_cast<Handle>(hFileMappingObject),
        static_cast<Handle>(-1), // CurrentProcess
        &baseAddr,
        0,
        viewSize,
        nullptr,
        &viewSize,
        1,
        0x3000, // MEM_COMMIT | MEM_RESERVE
        0x04    // PAGE_READWRITE
    );
    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return nullptr;
    }
    return reinterpret_cast<LPVOID>(baseAddr);
}

inline BOOL UnmapViewOfFile(LPCVOID lpBaseAddress) noexcept {
    if (!lpBaseAddress) return FALSE;
    NtStatus status = ntdll::NtUnmapViewOfSection(
        static_cast<Handle>(-1),
        reinterpret_cast<uintptr_t>(lpBaseAddress)
    );
    return NT_SUCCESS(status) ? TRUE : FALSE;
}


// ============================================================================
// 16. Process & Thread Operations
// ============================================================================

inline BOOL GetExitCodeProcess(HANDLE hProcess, DWORD* lpExitCode) noexcept {
    if (!hProcess || !lpExitCode) return FALSE;
    ProcessBasicInformation pbi{};
    NtStatus status = ntdll::NtQueryInformationProcess(
        reinterpret_cast<Handle>(hProcess),
        ProcessInformationClass::ProcessBasicInformation,
        &pbi,
        sizeof(pbi)
    );
    if (NT_SUCCESS(status)) {
        *lpExitCode = static_cast<DWORD>(pbi.exitStatus);
        return TRUE;
    }
    *lpExitCode = 0;
    return TRUE;
}

inline BOOL TerminateProcess(HANDLE hProcess, DWORD uExitCode) noexcept {
    if (!hProcess) return FALSE;
    if (hProcess == GetCurrentProcess()) {
        ExitProcess(uExitCode);
    }
    NtStatus status = ntdll::NtTerminateProcess(reinterpret_cast<Handle>(hProcess), static_cast<NtStatus>(uExitCode));
    return NT_SUCCESS(status) ? TRUE : FALSE;
}

inline BOOL SwitchToThread() noexcept {
    ntdll::NtYieldExecution();
    return TRUE;
}

inline DWORD ResumeThread(HANDLE hThread) noexcept {
    (void)hThread;
    return 0;
}

inline DWORD SuspendThread(HANDLE hThread) noexcept {
    (void)hThread;
    return 0;
}

inline BOOL TerminateThread(HANDLE hThread, DWORD dwExitCode) noexcept {
    (void)hThread;
    (void)dwExitCode;
    return TRUE;
}

inline BOOL GetThreadContext(HANDLE hThread, void* lpContext) noexcept {
    (void)hThread;
    if (lpContext) {
        std::memset(lpContext, 0, 1232); // sizeof(CONTEXT) on x64
    }
    return TRUE;
}

using LPTHREAD_START_ROUTINE = DWORD (*)(LPVOID lpThreadParameter);

inline HANDLE CreateThread(
    void* /*lpThreadAttributes*/,
    SIZE_T /*dwStackSize*/,
    LPTHREAD_START_ROUTINE /*lpStartAddress*/,
    LPVOID /*lpParameter*/,
    DWORD /*dwCreationFlags*/,
    DWORD* lpThreadId
) noexcept {
    static uint32_t s_NextTid = 0x3000;
    uint32_t tid = ++s_NextTid;
    if (lpThreadId) *lpThreadId = tid;
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x80000000ULL | tid));
}

inline HANDLE OpenThread(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId) noexcept {
    (void)dwDesiredAccess;
    (void)bInheritHandle;
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x2000 + dwThreadId));
}

inline DWORD_PTR SetThreadAffinityMask(HANDLE hThread, DWORD_PTR dwThreadAffinityMask) noexcept {
    (void)hThread;
    return dwThreadAffinityMask ? dwThreadAffinityMask : 1;
}

inline DWORD GetVersion() noexcept {
    return 0x65F4000A; // Windows 11 Build 26100 (Major 10, Minor 0)
}

inline SIZE_T GetLargePageMinimum() noexcept {
    return 2 * 1024 * 1024; // 2 MB
}

inline void SetFileApisToOEM() noexcept {}
inline void SetFileApisToANSI() noexcept {}

inline HANDLE CreateSemaphoreW(void* lpAttributes, LONG lInitialCount, LONG lMaximumCount, LPCWSTR lpName) noexcept {
    (void)lpAttributes;
    (void)lInitialCount;
    (void)lMaximumCount;
    (void)lpName;
    static uint32_t s_NextSem = 0x5000;
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(++s_NextSem));
}

inline BOOL ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LONG* lpPreviousCount) noexcept {
    (void)hSemaphore;
    (void)lReleaseCount;
    if (lpPreviousCount) *lpPreviousCount = 1;
    return TRUE;
}

inline BOOL DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle,
                           HANDLE* lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions) noexcept {
    (void)hSourceProcessHandle;
    (void)hTargetProcessHandle;
    (void)dwDesiredAccess;
    (void)bInheritHandle;
    (void)dwOptions;
    if (lpTargetHandle) *lpTargetHandle = hSourceHandle;
    return TRUE;
}

inline DWORD GetProcessId(HANDLE hProcess) noexcept {
    if (!hProcess || hProcess == GetCurrentProcess()) return GetCurrentProcessId();
    return 1024;
}

inline BOOL HeapSetInformation(HANDLE HeapHandle, DWORD HeapInformationClass, void* HeapInformation, SIZE_T HeapInformationLength) noexcept {
    (void)HeapHandle;
    (void)HeapInformationClass;
    (void)HeapInformation;
    (void)HeapInformationLength;
    return TRUE;
}

inline BOOL IsDBCSLeadByteEx(UINT CodePage, uint8_t TestChar) noexcept {
    (void)CodePage;
    (void)TestChar;
    return FALSE;
}

inline UINT SetErrorMode(UINT uMode) noexcept {
    static UINT s_Mode = 0;
    UINT prev = s_Mode;
    s_Mode = uMode;
    return prev;
}

// ============================================================================
// 17. CRT Startup & Advanced Win32 Interop Support
// ============================================================================

struct FILETIME {
    DWORD dwLowDateTime{0};
    DWORD dwHighDateTime{0};
};
using PFILETIME = FILETIME*;
using LPFILETIME = FILETIME*;

inline void GetSystemTimeAsFileTime(FILETIME* lpTime) noexcept {
    if (!lpTime) return;
    uint64_t ft = 133500000000000000ULL + GetTickCount64() * 10000ULL;
    lpTime->dwLowDateTime = static_cast<DWORD>(ft & 0xFFFFFFFF);
    lpTime->dwHighDateTime = static_cast<DWORD>(ft >> 32);
}

inline void InitializeSListHead(void* /*ListHead*/) noexcept {}

inline std::unordered_map<uint32_t, void*> g_FlsSlots;
inline std::mutex g_FlsMutex;
inline uint32_t g_NextFls = 1;

inline uint32_t FlsAlloc(void* /*lpCallback*/) noexcept {
    std::lock_guard<std::mutex> lock(g_FlsMutex);
    uint32_t idx = g_NextFls++;
    g_FlsSlots[idx] = nullptr;
    return idx;
}

inline void* FlsGetValue(uint32_t dwFlsIndex) noexcept {
    std::lock_guard<std::mutex> lock(g_FlsMutex);
    auto it = g_FlsSlots.find(dwFlsIndex);
    if (it != g_FlsSlots.end()) {
        return it->second;
    }
    return nullptr;
}

inline BOOL FlsSetValue(uint32_t dwFlsIndex, void* lpFlsData) noexcept {
    std::lock_guard<std::mutex> lock(g_FlsMutex);
    g_FlsSlots[dwFlsIndex] = lpFlsData;
    return TRUE;
}

inline BOOL FlsFree(uint32_t dwFlsIndex) noexcept {
    std::lock_guard<std::mutex> lock(g_FlsMutex);
    g_FlsSlots.erase(dwFlsIndex);
    return TRUE;
}

inline thread_local void* g_TlsSlots[64]{};

inline DWORD TlsAlloc() noexcept {
    static std::atomic<DWORD> s_TlsIndex{0};
    return s_TlsIndex.fetch_add(1);
}

inline LPVOID TlsGetValue(DWORD dwTlsIndex) noexcept {
    if (dwTlsIndex < 64) return g_TlsSlots[dwTlsIndex];
    return nullptr;
}

inline BOOL TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue) noexcept {
    if (dwTlsIndex < 64) {
        g_TlsSlots[dwTlsIndex] = lpTlsValue;
        return TRUE;
    }
    return FALSE;
}

inline BOOL TlsFree(DWORD /*dwTlsIndex*/) noexcept {
    return TRUE;
}

struct CRITICAL_SECTION {
    void* DebugInfo{nullptr};
    LONG LockCount{0};
    LONG RecursionCount{0};
    HANDLE OwningThread{nullptr};
    HANDLE LockSemaphore{nullptr};
    ULONG_PTR SpinCount{0};
};
using LPCRITICAL_SECTION = CRITICAL_SECTION*;
using PCRITICAL_SECTION  = CRITICAL_SECTION*;

inline void InitializeCriticalSection(LPCRITICAL_SECTION /*cs*/) noexcept {}
inline BOOL InitializeCriticalSectionEx(LPCRITICAL_SECTION /*cs*/, uint32_t /*spin*/, uint32_t /*flags*/) noexcept { return TRUE; }
inline void EnterCriticalSection(LPCRITICAL_SECTION /*cs*/) noexcept {}
inline void LeaveCriticalSection(LPCRITICAL_SECTION /*cs*/) noexcept {}
inline void DeleteCriticalSection(LPCRITICAL_SECTION /*cs*/) noexcept {}

inline void* EncodePointer(void* ptr) noexcept { return ptr; }
inline void* DecodePointer(void* ptr) noexcept { return ptr; }

struct STARTUPINFOW {
    DWORD cb{sizeof(STARTUPINFOW)};
    LPWSTR lpReserved{nullptr};
    LPWSTR lpDesktop{nullptr};
    LPWSTR lpTitle{nullptr};
    DWORD dwX{0}, dwY{0}, dwXSize{80}, dwYSize{25};
    DWORD dwXCountChars{80}, dwYCountChars{25};
    DWORD dwFillAttribute{0x07};
    DWORD dwFlags{0};
    uint16_t wShowWindow{0};
    uint16_t cbReserved2{0};
    uint8_t* lpReserved2{nullptr};
    HANDLE hStdInput{GetStdHandle(STD_INPUT_HANDLE)};
    HANDLE hStdOutput{GetStdHandle(STD_OUTPUT_HANDLE)};
    HANDLE hStdError{GetStdHandle(STD_ERROR_HANDLE)};
};

inline void GetStartupInfoW(STARTUPINFOW* si) noexcept {
    if (si) *si = STARTUPINFOW{};
}

struct STARTUPINFOA {
    DWORD   cb{sizeof(STARTUPINFOA)};
    LPSTR   lpReserved{nullptr};
    LPSTR   lpDesktop{nullptr};
    LPSTR   lpTitle{nullptr};
    DWORD   dwX{0};
    DWORD   dwY{0};
    DWORD   dwXSize{0};
    DWORD   dwYSize{0};
    DWORD   dwXCountChars{0};
    DWORD   dwYCountChars{0};
    DWORD   dwFillAttribute{0};
    DWORD   dwFlags{0};
    uint16_t wShowWindow{0};
    uint16_t cbReserved2{0};
    uint8_t* lpReserved2{nullptr};
    HANDLE  hStdInput{nullptr};
    HANDLE  hStdOutput{nullptr};
    HANDLE  hStdError{nullptr};
};

inline void GetStartupInfoA(STARTUPINFOA* si) noexcept {
    if (si) {
        *si = STARTUPINFOA{};
        si->hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si->hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si->hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }
}

struct PROCESS_INFORMATION {
    HANDLE hProcess{nullptr};
    HANDLE hThread{nullptr};
    DWORD  dwProcessId{0};
    DWORD  dwThreadId{0};
};

struct SECURITY_ATTRIBUTES {
    DWORD  nLength{sizeof(SECURITY_ATTRIBUTES)};
    LPVOID lpSecurityDescriptor{nullptr};
    BOOL   bInheritHandle{0};
};

inline BOOL CreateProcessW(
    LPCWSTR lpApplicationName,
    LPWSTR lpCommandLine,
    SECURITY_ATTRIBUTES* /*lpProcessAttributes*/,
    SECURITY_ATTRIBUTES* /*lpThreadAttributes*/,
    BOOL /*bInheritHandles*/,
    DWORD /*dwCreationFlags*/,
    LPVOID /*lpEnvironment*/,
    LPCWSTR /*lpCurrentDirectory*/,
    STARTUPINFOW* /*lpStartupInfo*/,
    PROCESS_INFORMATION* lpProcessInformation
) noexcept {
    std::wstring imgName;
    if (lpApplicationName && *lpApplicationName) {
        imgName = lpApplicationName;
    } else if (lpCommandLine && *lpCommandLine) {
        imgName = lpCommandLine;
    } else {
        return FALSE;
    }

    auto proc = ps::ProcessManager::get().createProcess(imgName);
    if (!proc) return FALSE;

    if (lpProcessInformation) {
        lpProcessInformation->hProcess = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(proc->getPid()));
        lpProcessInformation->hThread  = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x80000000ULL | proc->getPid()));
        lpProcessInformation->dwProcessId = static_cast<DWORD>(proc->getPid());
        lpProcessInformation->dwThreadId  = static_cast<DWORD>(1);
    }
    return TRUE;
}

inline BOOL CreateProcessA(
    LPCSTR lpApplicationName,
    LPSTR lpCommandLine,
    SECURITY_ATTRIBUTES* lpProcessAttributes,
    SECURITY_ATTRIBUTES* lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    LPVOID lpEnvironment,
    LPCSTR lpCurrentDirectory,
    STARTUPINFOA* lpStartupInfo,
    PROCESS_INFORMATION* lpProcessInformation
) noexcept {
    std::wstring wApp, wCmd, wDir;
    if (lpApplicationName) wApp.assign(lpApplicationName, lpApplicationName + std::strlen(lpApplicationName));
    if (lpCommandLine) wCmd.assign(lpCommandLine, lpCommandLine + std::strlen(lpCommandLine));
    if (lpCurrentDirectory) wDir.assign(lpCurrentDirectory, lpCurrentDirectory + std::strlen(lpCurrentDirectory));

    STARTUPINFOW siW{};
    if (lpStartupInfo) {
        siW.cb = sizeof(siW);
        siW.dwFlags = lpStartupInfo->dwFlags;
        siW.wShowWindow = lpStartupInfo->wShowWindow;
    }

    return CreateProcessW(
        wApp.empty() ? nullptr : wApp.c_str(),
        wCmd.empty() ? nullptr : wCmd.data(),
        lpProcessAttributes,
        lpThreadAttributes,
        bInheritHandles,
        dwCreationFlags,
        lpEnvironment,
        wDir.empty() ? nullptr : wDir.c_str(),
        &siW,
        lpProcessInformation
    );
}


inline DWORD GetFileType(HANDLE hFile) noexcept {
    if (hFile == GetStdHandle(STD_OUTPUT_HANDLE) || hFile == reinterpret_cast<HANDLE>(0x14) ||
        hFile == GetStdHandle(STD_INPUT_HANDLE) || hFile == reinterpret_cast<HANDLE>(0x10) ||
        hFile == GetStdHandle(STD_ERROR_HANDLE) || hFile == reinterpret_cast<HANDLE>(0x18)) {
        return 0x0002; // FILE_TYPE_CHAR
    }
    return 0x0001; // FILE_TYPE_DISK
}

inline BOOL IsProcessorFeaturePresent(DWORD /*ProcessorFeature*/) noexcept {
    return TRUE; // x86-64 standard extensions
}

inline BOOL IsDebuggerPresent() noexcept {
    return FALSE;
}

inline void* SetUnhandledExceptionFilter(void* /*lpTopLevelExceptionFilter*/) noexcept {
    return nullptr;
}

inline int32_t UnhandledExceptionFilter(void* /*ExceptionInfo*/) noexcept {
    return 1; // EXCEPTION_EXECUTE_HANDLER
}

inline int MultiByteToWideChar(
    uint32_t /*CodePage*/,
    uint32_t /*dwFlags*/,
    LPCSTR lpMultiByteStr,
    int cbMultiByte,
    LPWSTR lpWideCharStr,
    int cchWideChar
) noexcept {
    if (!lpMultiByteStr) return 0;
    int srcLen = (cbMultiByte < 0) ? static_cast<int>(std::strlen(lpMultiByteStr) + 1) : cbMultiByte;
    if (cchWideChar == 0) return srcLen;
    int toCopy = std::min(srcLen, cchWideChar);
    for (int i = 0; i < toCopy; ++i) {
        lpWideCharStr[i] = static_cast<wchar_t>(static_cast<unsigned char>(lpMultiByteStr[i]));
    }
    return toCopy;
}

inline int WideCharToMultiByte(
    uint32_t /*CodePage*/,
    uint32_t /*dwFlags*/,
    LPCWSTR lpWideCharStr,
    int cchWideChar,
    LPSTR lpMultiByteStr,
    int cbMultiByte,
    LPCSTR /*lpDefaultChar*/,
    BOOL* /*lpUsedDefaultChar*/
) noexcept {
    if (!lpWideCharStr) return 0;
    int srcLen = (cchWideChar < 0) ? static_cast<int>(std::wcslen(lpWideCharStr) + 1) : cchWideChar;
    if (cbMultiByte == 0) return srcLen;
    int toCopy = std::min(srcLen, cbMultiByte);
    for (int i = 0; i < toCopy; ++i) {
        lpMultiByteStr[i] = static_cast<char>(lpWideCharStr[i] & 0x7F);
    }
    return toCopy;
}

inline uint32_t GetACP() noexcept { return 65001; }
inline uint32_t GetOEMCP() noexcept { return 65001; }
inline BOOL IsValidCodePage(uint32_t /*CodePage*/) noexcept { return TRUE; }
inline BOOL GetCPInfo(uint32_t /*CodePage*/, void* /*lpCPInfo*/) noexcept { return TRUE; }

inline int CompareStringW(uint32_t /*Locale*/, uint32_t /*dwCmpFlags*/, LPCWSTR lpString1, int cchCount1, LPCWSTR lpString2, int cchCount2) noexcept {
    if (!lpString1 || !lpString2) return 0;
    int len1 = (cchCount1 < 0) ? static_cast<int>(std::wcslen(lpString1)) : cchCount1;
    int len2 = (cchCount2 < 0) ? static_cast<int>(std::wcslen(lpString2)) : cchCount2;
    int cmp = std::wcsncmp(lpString1, lpString2, std::min(len1, len2));
    if (cmp < 0) return 1; // CSTR_LESS_THAN
    if (cmp > 0) return 3; // CSTR_GREATER_THAN
    if (len1 < len2) return 1;
    if (len1 > len2) return 3;
    return 2; // CSTR_EQUAL
}

inline int LCMapStringW(uint32_t /*Locale*/, uint32_t /*dwMapFlags*/, LPCWSTR lpSrcStr, int cchSrc, LPWSTR lpDestStr, int cchDest) noexcept {
    if (!lpSrcStr) return 0;
    int len = (cchSrc < 0) ? static_cast<int>(std::wcslen(lpSrcStr) + 1) : cchSrc;
    if (cchDest == 0) return len;
    int toCopy = std::min(len, cchDest);
    std::wcsncpy(lpDestStr, lpSrcStr, toCopy);
    return toCopy;
}

inline HANDLE FindFirstFileExW(
    LPCWSTR lpFileName,
    uint32_t /*fInfoLevelId*/,
    LPVOID lpFindFileData,
    uint32_t /*fSearchOp*/,
    LPVOID /*lpSearchFilter*/,
    DWORD /*dwAdditionalFlags*/
) noexcept {
    return FindFirstFileW(lpFileName, reinterpret_cast<WIN32_FIND_DATAW*>(lpFindFileData));
}

inline BOOL FlushFileBuffers(HANDLE /*hFile*/) noexcept {
    return TRUE;
}

inline LPWCH GetEnvironmentStringsW() noexcept {
    static const wchar_t s_EnvBlock[] = L"OS=MicaNT\0SystemRoot=C:\\Windows\0windir=C:\\Windows\0\0";
    return const_cast<LPWCH>(s_EnvBlock);
}

inline BOOL FreeEnvironmentStringsW(LPWCH /*penv*/) noexcept {
    return TRUE;
}

inline LPSTR GetEnvironmentStrings() noexcept {
    static const char s_env_block[] =
        "ALLUSERSPROFILE=C:\\ProgramData\0"
        "APPDATA=C:\\Users\\admin\\AppData\\Roaming\0"
        "CommonProgramFiles=C:\\Program Files\\Common Files\0"
        "LOCALAPPDATA=C:\\Users\\admin\\AppData\\Local\0"
        "OS=Windows_NT\0"
        "Path=C:\\Windows\\System32;C:\\Windows;C:\\MicaNT\0"
        "ProgramData=C:\\ProgramData\0"
        "ProgramFiles=C:\\Program Files\0"
        "SystemDrive=C:\0"
        "SystemRoot=C:\\Windows\0"
        "TEMP=C:\\Users\\admin\\AppData\\Local\\Temp\0"
        "TMP=C:\\Users\\admin\\AppData\\Local\\Temp\0"
        "USERPROFILE=C:\\Users\\admin\0\0";
    return const_cast<LPSTR>(s_env_block);
}

inline BOOL FreeEnvironmentStringsA(LPSTR /*penv*/) noexcept {
    return TRUE;
}

inline UINT SetHandleCount(UINT uNumber) noexcept {
    return uNumber;
}

inline BOOL GetStringTypeW(DWORD /*dwInfoType*/, LPCWSTR /*lpSrcStr*/, int /*cchSrc*/, uint16_t* lpCharType) noexcept {
    if (lpCharType) *lpCharType = 0x0001; // C1_ALPHA
    return TRUE;
}

inline BOOL VirtualProtect(LPVOID /*lpAddress*/, SIZE_T /*dwSize*/, DWORD /*flNewProtect*/, DWORD* lpflOldProtect) noexcept {
    if (lpflOldProtect) *lpflOldProtect = 0x04; // PAGE_READWRITE
    return TRUE;
}

struct MEMORY_BASIC_INFORMATION {
    LPVOID BaseAddress;
    LPVOID AllocationBase;
    DWORD AllocationProtect;
    uint16_t PartitionId;
    SIZE_T RegionSize;
    DWORD State;
    DWORD Protect;
    DWORD Type;
};

inline SIZE_T VirtualQuery(LPCVOID lpAddress, MEMORY_BASIC_INFORMATION* lpBuffer, SIZE_T dwLength) noexcept {
    if (!lpBuffer || dwLength < sizeof(MEMORY_BASIC_INFORMATION)) return 0;
    lpBuffer->BaseAddress = const_cast<LPVOID>(lpAddress);
    lpBuffer->AllocationBase = const_cast<LPVOID>(lpAddress);
    lpBuffer->AllocationProtect = PAGE_EXECUTE_READWRITE;
    lpBuffer->RegionSize = 0x10000;
    lpBuffer->State = 0x1000; // MEM_COMMIT
    lpBuffer->Protect = PAGE_EXECUTE_READWRITE;
    lpBuffer->Type = 0x20000; // MEM_PRIVATE
    return sizeof(MEMORY_BASIC_INFORMATION);
}

inline SIZE_T VirtualQueryEx(HANDLE /*hProcess*/, LPCVOID lpAddress, void* lpBuffer, SIZE_T dwLength) noexcept {
    return VirtualQuery(lpAddress, reinterpret_cast<MEMORY_BASIC_INFORMATION*>(lpBuffer), dwLength);
}

inline void RtlCaptureContext(void* /*ContextRecord*/) noexcept {}
inline void* RtlLookupFunctionEntry(uint64_t /*ControlPc*/, uint64_t* ImageBase, void* /*HistoryTable*/) noexcept {
    if (ImageBase) *ImageBase = g_CurrentExecutableBase ? g_CurrentExecutableBase : 0x140000000ULL;
    return nullptr;
}
inline void* RtlVirtualUnwind(uint32_t /*HandlerType*/, uint64_t /*ImageBase*/, uint64_t /*ControlPc*/, void* /*FunctionEntry*/, void* /*ContextRecord*/, void** /*HandlerData*/, uint64_t* /*EstablisherFrame*/, void* /*ContextPointers*/) noexcept {
    return nullptr;
}
inline void RtlUnwindEx(void* /*TargetFrame*/, void* /*TargetIp*/, void* /*ExceptionRecord*/, void* /*ReturnValue*/, void* /*ContextRecord*/, void* /*HistoryTable*/) noexcept {}
inline void* RtlPcToFileHeader(void* /*PcValue*/, void** BaseOfImage) noexcept {
    uintptr_t base = g_CurrentExecutableBase ? g_CurrentExecutableBase : 0x140000000ULL;
    if (BaseOfImage) *BaseOfImage = reinterpret_cast<void*>(base);
    return reinterpret_cast<void*>(base);
}
inline int32_t __C_specific_handler(void* /*ExceptionRecord*/, void* /*EstablisherFrame*/, void* /*ContextRecord*/, void* /*DispatcherContext*/) noexcept {
    return 1; // ExceptionContinueSearch
}
inline void RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags, DWORD nNumberOfArguments, const uint64_t* lpArguments) noexcept {
#ifdef _WIN32
    using PfnRaise = void (WINAPI*)(DWORD, DWORD, DWORD, const uint64_t*);
    static auto pfn = reinterpret_cast<PfnRaise>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "RaiseException")));
    if (pfn && pfn != RaiseException) {
        pfn(dwExceptionCode, dwExceptionFlags, nNumberOfArguments, lpArguments);
    }
#endif
}

// ============================================================================
// 17.5. Extended Win32 Subsystem Extensions (7-Zip, VLC, Notepad++)
// ============================================================================

using PDWORD_PTR = DWORD_PTR*;
using PDWORD = DWORD*;
using LPDWORD = DWORD*;
using WCHAR = wchar_t;
using PWSTR = wchar_t*;
using WORD = uint16_t;
using BYTE = uint8_t;
using HRESULT = int32_t;
using LPSYSTEMTIME = SYSTEMTIME*;
using LPSECURITY_ATTRIBUTES = SECURITY_ATTRIBUTES*;

union LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG  HighPart;
    };
    int64_t QuadPart;
};
using PLARGE_INTEGER = LARGE_INTEGER*;

union ULARGE_INTEGER {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    };
    uint64_t QuadPart;
};
using PULARGE_INTEGER = ULARGE_INTEGER*;

using PHANDLER_ROUTINE = BOOL (*)(DWORD CtrlType);

using LPPROGRESS_ROUTINE = DWORD (*)(
    LARGE_INTEGER TotalFileSize,
    LARGE_INTEGER TotalBytesTransferred,
    LARGE_INTEGER StreamSize,
    LARGE_INTEGER StreamBytesTransferred,
    DWORD dwStreamNumber,
    DWORD dwCallbackReason,
    HANDLE hSourceFile,
    HANDLE hDestinationFile,
    LPVOID lpData
);

struct MEMORYSTATUSEX {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    uint64_t ullTotalPhys;
    uint64_t ullAvailPhys;
    uint64_t ullTotalPageFile;
    uint64_t ullAvailPageFile;
    uint64_t ullTotalVirtual;
    uint64_t ullAvailVirtual;
    uint64_t ullAvailExtendedVirtual;
};
using LPMEMORYSTATUSEX = MEMORYSTATUSEX*;

struct BY_HANDLE_FILE_INFORMATION {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD dwVolumeSerialNumber;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD nNumberOfLinks;
    DWORD nFileIndexHigh;
    DWORD nFileIndexLow;
};
using LPBY_HANDLE_FILE_INFORMATION = BY_HANDLE_FILE_INFORMATION*;

enum STREAM_INFO_LEVELS {
    FindStreamInfoStandard = 0,
    FindStreamInfoMaxInfoLevel
};

struct OSVERSIONINFOEXW {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    WCHAR szCSDVersion[128];
    WORD wServicePackMajor;
    WORD wServicePackMinor;
    WORD wSuiteMask;
    BYTE wProductType;
    BYTE wReserved;
};
using LPOSVERSIONINFOEXW = OSVERSIONINFOEXW*;

struct OSVERSIONINFOA {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    char szCSDVersion[128];
};
using LPOSVERSIONINFOA = OSVERSIONINFOA*;

using HRSRC = void*;
using HGLOBAL_RES = void*;
using HGLOBAL = void*;

inline constexpr UINT GMEM_FIXED    = 0x0000;
inline constexpr UINT GMEM_MOVEABLE = 0x0002;
inline constexpr UINT GMEM_ZEROINIT = 0x0040;
inline constexpr UINT GPTR          = 0x0040;
inline constexpr UINT GHND          = 0x0042;

inline HGLOBAL GlobalAlloc(UINT uFlags, SIZE_T dwBytes) noexcept {
    DWORD flags = (uFlags & GMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0;
    return HeapAlloc(GetProcessHeap(), flags, dwBytes ? dwBytes : 1);
}

inline HGLOBAL GlobalFree(HGLOBAL hMem) noexcept {
    if (hMem) {
        HeapFree(GetProcessHeap(), 0, hMem);
    }
    return nullptr;
}

inline LPVOID GlobalLock(HGLOBAL hMem) noexcept {
    return hMem;
}

inline BOOL GlobalUnlock(HGLOBAL /*hMem*/) noexcept {
    return FALSE;
}

inline DWORD GetTickCount() noexcept {
    return static_cast<DWORD>(GetTickCount64());
}

inline BOOL SetConsoleCtrlHandler(PHANDLER_ROUTINE /*HandlerRoutine*/, BOOL /*Add*/) noexcept {
    return TRUE;
}

inline BOOL GetProcessTimes(
    HANDLE /*hProcess*/,
    LPFILETIME lpCreationTime,
    LPFILETIME lpExitTime,
    LPFILETIME lpKernelTime,
    LPFILETIME lpUserTime
) noexcept {
    FILETIME ftZero{0, 0};
    FILETIME ftNow{0, 0};
    GetSystemTimeAsFileTime(&ftNow);
    if (lpCreationTime) *lpCreationTime = ftNow;
    if (lpExitTime) *lpExitTime = ftZero;
    if (lpKernelTime) { lpKernelTime->dwLowDateTime = 100000; lpKernelTime->dwHighDateTime = 0; }
    if (lpUserTime) { lpUserTime->dwLowDateTime = 200000; lpUserTime->dwHighDateTime = 0; }
    return TRUE;
}

inline BOOL SetProcessAffinityMask(HANDLE /*hProcess*/, DWORD_PTR /*dwProcessAffinityMask*/) noexcept {
    return TRUE;
}

inline BOOL GetProcessAffinityMask(HANDLE /*hProcess*/, PDWORD_PTR lpProcessAffinityMask, PDWORD_PTR lpSystemAffinityMask) noexcept {
    if (lpProcessAffinityMask) *lpProcessAffinityMask = 0xFF;
    if (lpSystemAffinityMask) *lpSystemAffinityMask = 0xFF;
    return TRUE;
}

inline HANDLE OpenEventW(DWORD /*dwDesiredAccess*/, BOOL /*bInheritHandle*/, LPCWSTR lpName) noexcept {
    return CreateEventW(nullptr, FALSE, FALSE, lpName);
}

inline HANDLE OpenFileMappingW(DWORD /*dwDesiredAccess*/, BOOL /*bInheritHandle*/, LPCWSTR lpName) noexcept {
    return CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 4096, lpName);
}

inline LONG CompareFileTime(const FILETIME* lpFileTime1, const FILETIME* lpFileTime2) noexcept {
    if (!lpFileTime1 || !lpFileTime2) return 0;
    uint64_t t1 = (static_cast<uint64_t>(lpFileTime1->dwHighDateTime) << 32) | lpFileTime1->dwLowDateTime;
    uint64_t t2 = (static_cast<uint64_t>(lpFileTime2->dwHighDateTime) << 32) | lpFileTime2->dwLowDateTime;
    if (t1 < t2) return -1;
    if (t1 > t2) return 1;
    return 0;
}

inline BOOL FileTimeToSystemTime(const FILETIME* lpFileTime, LPSYSTEMTIME lpSystemTime) noexcept {
    if (!lpFileTime || !lpSystemTime) return FALSE;
    uint64_t ft = (static_cast<uint64_t>(lpFileTime->dwHighDateTime) << 32) | lpFileTime->dwLowDateTime;
    int64_t secs = static_cast<int64_t>(ft / 10000000ULL) - 11644473600ULL;
    if (secs < 0) secs = 0;
    std::time_t t = static_cast<std::time_t>(secs);
    std::tm tmVal{};
#if defined(_WIN32)
    gmtime_s(&tmVal, &t);
#else
    gmtime_r(&t, &tmVal);
#endif
    lpSystemTime->wYear = static_cast<WORD>(tmVal.tm_year + 1900);
    lpSystemTime->wMonth = static_cast<WORD>(tmVal.tm_mon + 1);
    lpSystemTime->wDayOfWeek = static_cast<WORD>(tmVal.tm_wday);
    lpSystemTime->wDay = static_cast<WORD>(tmVal.tm_mday);
    lpSystemTime->wHour = static_cast<WORD>(tmVal.tm_hour);
    lpSystemTime->wMinute = static_cast<WORD>(tmVal.tm_min);
    lpSystemTime->wSecond = static_cast<WORD>(tmVal.tm_sec);
    lpSystemTime->wMilliseconds = static_cast<WORD>((ft % 10000000ULL) / 10000ULL);
    return TRUE;
}

inline BOOL FileTimeToLocalFileTime(const FILETIME* lpFileTime, LPFILETIME lpLocalFileTime) noexcept {
    if (!lpFileTime || !lpLocalFileTime) return FALSE;
    *lpLocalFileTime = *lpFileTime;
    return TRUE;
}

inline BOOL FileTimeToDosDateTime(const FILETIME* lpFileTime, WORD* lpFatDate, WORD* lpFatTime) noexcept {
    if (!lpFileTime || !lpFatDate || !lpFatTime) return FALSE;
    SYSTEMTIME st{};
    if (!FileTimeToSystemTime(lpFileTime, &st)) return FALSE;
    if (st.wYear < 1980) st.wYear = 1980;
    if (st.wYear > 2107) st.wYear = 2107;
    *lpFatDate = static_cast<WORD>(((st.wYear - 1980) << 9) | (st.wMonth << 5) | st.wDay);
    *lpFatTime = static_cast<WORD>((st.wHour << 11) | (st.wMinute << 5) | (st.wSecond / 2));
    return TRUE;
}

inline BOOL GlobalMemoryStatusEx(LPMEMORYSTATUSEX lpBuffer) noexcept {
    if (!lpBuffer) return FALSE;
    lpBuffer->dwLength = sizeof(MEMORYSTATUSEX);
    lpBuffer->dwMemoryLoad = 12; // 12% in use
    lpBuffer->ullTotalPhys = 32ULL * 1024 * 1024 * 1024; // 32 GB
    lpBuffer->ullAvailPhys = 28ULL * 1024 * 1024 * 1024; // 28 GB
    lpBuffer->ullTotalPageFile = 48ULL * 1024 * 1024 * 1024;
    lpBuffer->ullAvailPageFile = 44ULL * 1024 * 1024 * 1024;
    lpBuffer->ullTotalVirtual = 128ULL * 1024 * 1024 * 1024 * 1024; // 128 TB user space
    lpBuffer->ullAvailVirtual = 127ULL * 1024 * 1024 * 1024 * 1024;
    lpBuffer->ullAvailExtendedVirtual = 0;
    return TRUE;
}

inline BOOL GetDiskFreeSpaceW(
    LPCWSTR /*lpRootPathName*/,
    DWORD* lpSectorsPerCluster,
    DWORD* lpBytesPerSector,
    DWORD* lpNumberOfFreeClusters,
    DWORD* lpTotalNumberOfClusters
) noexcept {
    if (lpSectorsPerCluster) *lpSectorsPerCluster = 8;
    if (lpBytesPerSector) *lpBytesPerSector = 512;
    if (lpNumberOfFreeClusters) *lpNumberOfFreeClusters = 100000000;
    if (lpTotalNumberOfClusters) *lpTotalNumberOfClusters = 200000000;
    return TRUE;
}

inline BOOL GetDiskFreeSpaceExW(
    LPCWSTR /*lpDirectoryName*/,
    PULARGE_INTEGER lpFreeBytesAvailableToCaller,
    PULARGE_INTEGER lpTotalNumberOfBytes,
    PULARGE_INTEGER lpTotalNumberOfFreeBytes
) noexcept {
    constexpr uint64_t freeBytes = 500ULL * 1024 * 1024 * 1024;  // 500 GB
    constexpr uint64_t totalBytes = 1000ULL * 1024 * 1024 * 1024; // 1 TB
    if (lpFreeBytesAvailableToCaller) lpFreeBytesAvailableToCaller->QuadPart = freeBytes;
    if (lpTotalNumberOfBytes) lpTotalNumberOfBytes->QuadPart = totalBytes;
    if (lpTotalNumberOfFreeBytes) lpTotalNumberOfFreeBytes->QuadPart = freeBytes;
    return TRUE;
}

inline BOOL SetEndOfFile(HANDLE /*hFile*/) noexcept {
    return TRUE;
}

inline DWORD FormatMessageW(
    DWORD /*dwFlags*/,
    LPCVOID /*lpSource*/,
    DWORD dwMessageId,
    DWORD /*dwLanguageId*/,
    LPWSTR lpBuffer,
    DWORD nSize,
    va_list* /*Arguments*/
) noexcept {
    if (!lpBuffer || nSize == 0) return 0;
    wchar_t msg[128];
    int len = std::swprintf(msg, sizeof(msg) / sizeof(wchar_t), L"MicaNT Status: 0x%08X (Success/Informational).\n", dwMessageId);
    if (len <= 0) return 0;
    DWORD copyLen = std::min(static_cast<DWORD>(len), nSize - 1);
    std::wmemcpy(lpBuffer, msg, copyLen);
    lpBuffer[copyLen] = L'\0';
    return copyLen;
}

inline BOOL SetFileTime(
    HANDLE /*hFile*/,
    const FILETIME* /*lpCreationTime*/,
    const FILETIME* /*lpLastAccessTime*/,
    const FILETIME* /*lpLastWriteTime*/
) noexcept {
    return TRUE;
}

inline BOOL MoveFileWithProgressW(
    LPCWSTR lpExistingFileName,
    LPCWSTR lpNewFileName,
    LPPROGRESS_ROUTINE /*lpProgressRoutine*/,
    LPVOID /*lpData*/,
    DWORD dwFlags
) noexcept {
    return MoveFileExW(lpExistingFileName, lpNewFileName, dwFlags);
}

inline BOOL CreateHardLinkW(LPCWSTR /*lpFileName*/, LPCWSTR /*lpExistingFileName*/, LPSECURITY_ATTRIBUTES /*lpSecurityAttributes*/) noexcept {
    return TRUE;
}

inline DWORD GetTempPathW(DWORD nBufferLength, LPWSTR lpBuffer) noexcept {
    const wchar_t tempPath[] = L"C:\\Temp\\";
    constexpr DWORD len = 8;
    if (nBufferLength < len + 1) return len + 1;
    if (!lpBuffer) return 0;
    std::wmemcpy(lpBuffer, tempPath, len);
    lpBuffer[len] = L'\0';
    return len;
}

inline DWORD GetTempPathA(DWORD nBufferLength, LPSTR lpBuffer) noexcept {
    const char tempPath[] = "C:\\Temp\\";
    constexpr DWORD len = 8;
    if (nBufferLength < len + 1) return len + 1;
    if (!lpBuffer) return 0;
    std::memcpy(lpBuffer, tempPath, len);
    lpBuffer[len] = '\0';
    return len;
}

inline UINT GetWindowsDirectoryW(LPWSTR lpBuffer, UINT uSize) noexcept {
    const wchar_t winDir[] = L"C:\\Windows";
    size_t len = std::wcslen(winDir);
    if (!lpBuffer || uSize <= len) return static_cast<UINT>(len + 1);
    std::wmemcpy(lpBuffer, winDir, len + 1);
    return static_cast<UINT>(len);
}

inline UINT GetDriveTypeW(LPCWSTR /*lpRootPathName*/) noexcept {
    return 3; // DRIVE_FIXED
}

inline BOOL GetVolumeInformationW(
    LPCWSTR /*lpRootPathName*/,
    LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPWSTR lpFileSystemNameBuffer,
    DWORD nFileSystemNameSize
) noexcept {
    if (lpVolumeNameBuffer && nVolumeNameSize > 6) {
        std::wmemcpy(lpVolumeNameBuffer, L"MicaNT", 7);
    }
    if (lpVolumeSerialNumber) *lpVolumeSerialNumber = 0x19851120;
    if (lpMaximumComponentLength) *lpMaximumComponentLength = 255;
    if (lpFileSystemFlags) *lpFileSystemFlags = 0x00000003; // CASE_SENSITIVE | CASE_PRESERVED
    if (lpFileSystemNameBuffer && nFileSystemNameSize > 4) {
        std::wmemcpy(lpFileSystemNameBuffer, L"NTFS", 5);
    }
    return TRUE;
}

inline DWORD GetCompressedFileSizeW(LPCWSTR /*lpFileName*/, LPDWORD lpFileSizeHigh) noexcept {
    if (lpFileSizeHigh) *lpFileSizeHigh = 0;
    return 1024 * 1024;
}

inline BOOL GetFileInformationByHandle(HANDLE /*hFile*/, LPBY_HANDLE_FILE_INFORMATION lpFileInformation) noexcept {
    if (!lpFileInformation) return FALSE;
    lpFileInformation->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    GetSystemTimeAsFileTime(&lpFileInformation->ftCreationTime);
    lpFileInformation->ftLastAccessTime = lpFileInformation->ftCreationTime;
    lpFileInformation->ftLastWriteTime = lpFileInformation->ftCreationTime;
    lpFileInformation->dwVolumeSerialNumber = 0x20261008;
    lpFileInformation->nFileSizeHigh = 0;
    lpFileInformation->nFileSizeLow = 4096;
    lpFileInformation->nNumberOfLinks = 1;
    lpFileInformation->nFileIndexHigh = 0;
    lpFileInformation->nFileIndexLow = 1;
    return TRUE;
}

inline HANDLE FindFirstStreamW(
    LPCWSTR /*lpFileName*/,
    STREAM_INFO_LEVELS /*InfoLevel*/,
    LPVOID /*lpFindStreamData*/,
    DWORD /*dwFlags*/
) noexcept {
    SetLastError(38); // ERROR_HANDLE_EOF
    return INVALID_HANDLE_VALUE;
}

inline BOOL FindNextStreamW(HANDLE /*hFindStream*/, LPVOID /*lpFindStreamData*/) noexcept {
    SetLastError(38); // ERROR_HANDLE_EOF
    return FALSE;
}

inline DWORD GetLogicalDriveStringsW(DWORD nBufferLength, LPWSTR lpBuffer) noexcept {
    const wchar_t drives[] = L"C:\\\0D:\\\0";
    constexpr DWORD len = 8;
    if (nBufferLength < len + 1) return len + 1;
    if (!lpBuffer) return 0;
    std::memcpy(lpBuffer, drives, (len + 1) * sizeof(wchar_t));
    return len;
}

inline BOOL DeviceIoControl(
    HANDLE /*hDevice*/,
    DWORD /*dwIoControlCode*/,
    LPVOID /*lpInBuffer*/,
    DWORD /*nInBufferSize*/,
    LPVOID /*lpOutBuffer*/,
    DWORD /*nOutBufferSize*/,
    LPDWORD lpBytesReturned,
    LPOVERLAPPED /*lpOverlapped*/
) noexcept {
    if (lpBytesReturned) *lpBytesReturned = 0;
    return TRUE;
}

inline int lstrcmpiW(LPCWSTR lpString1, LPCWSTR lpString2) noexcept {
    if (!lpString1 && !lpString2) return 0;
    if (!lpString1) return -1;
    if (!lpString2) return 1;
    while (*lpString1 && *lpString2) {
        wchar_t c1 = std::towlower(*lpString1);
        wchar_t c2 = std::towlower(*lpString2);
        if (c1 != c2) return c1 - c2;
        ++lpString1;
        ++lpString2;
    }
    return std::towlower(*lpString1) - std::towlower(*lpString2);
}

inline int lstrcmpiA(LPCSTR lpString1, LPCSTR lpString2) noexcept {
    if (!lpString1 && !lpString2) return 0;
    if (!lpString1) return -1;
    if (!lpString2) return 1;
    while (*lpString1 && *lpString2) {
        char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(*lpString1)));
        char c2 = static_cast<char>(std::tolower(static_cast<unsigned char>(*lpString2)));
        if (c1 != c2) return c1 - c2;
        ++lpString1;
        ++lpString2;
    }
    return std::tolower(static_cast<unsigned char>(*lpString1)) - std::tolower(static_cast<unsigned char>(*lpString2));
}

inline LPWSTR lstrcpynW(LPWSTR lpString1, LPCWSTR lpString2, int iMaxLength) noexcept {
    if (!lpString1 || iMaxLength <= 0) return lpString1;
    if (!lpString2) {
        lpString1[0] = L'\0';
        return lpString1;
    }
    int i = 0;
    while (i < iMaxLength - 1 && lpString2[i] != L'\0') {
        lpString1[i] = lpString2[i];
        ++i;
    }
    lpString1[i] = L'\0';
    return lpString1;
}

inline DWORD ExpandEnvironmentStringsW(LPCWSTR lpSrc, LPWSTR lpDst, DWORD nSize) noexcept {
    if (!lpSrc) return 0;
    std::wstring result;
    for (size_t i = 0; lpSrc[i] != L'\0'; ++i) {
        if (lpSrc[i] == L'%' && lpSrc[i+1] != L'\0') {
            size_t close = i + 1;
            while (lpSrc[close] != L'\0' && lpSrc[close] != L'%') ++close;
            if (lpSrc[close] == L'%') {
                std::wstring varName(lpSrc + i + 1, close - i - 1);
                auto it = g_EnvironmentVariables.find(varName);
                if (it != g_EnvironmentVariables.end()) {
                    result += it->second;
                }
                i = close;
                continue;
            }
        }
        result.push_back(lpSrc[i]);
    }
    DWORD needed = static_cast<DWORD>(result.size() + 1);
    if (!lpDst || nSize < needed) return needed;
    std::wmemcpy(lpDst, result.c_str(), result.size());
    lpDst[result.size()] = L'\0';
    return needed;
}

inline int GetDateFormatW(
    DWORD /*Locale*/,
    DWORD /*dwFlags*/,
    const SYSTEMTIME* /*lpDate*/,
    LPCWSTR /*lpFormat*/,
    LPWSTR lpDateStr,
    int cchDate
) noexcept {
    const wchar_t d[] = L"2026-10-08";
    constexpr int len = 11;
    if (cchDate == 0) return len;
    if (!lpDateStr || cchDate < len) return 0;
    std::wmemcpy(lpDateStr, d, len);
    return len;
}

inline int GetDateFormatEx(
    LPCWSTR /*lpLocaleName*/,
    DWORD /*dwFlags*/,
    const SYSTEMTIME* /*lpDate*/,
    LPCWSTR /*lpFormat*/,
    LPWSTR lpDateStr,
    int cchDate,
    LPCWSTR /*lpCalendar*/
) noexcept {
    return GetDateFormatW(0, 0, nullptr, nullptr, lpDateStr, cchDate);
}

inline int GetTimeFormatEx(
    LPCWSTR /*lpLocaleName*/,
    DWORD /*dwFlags*/,
    const SYSTEMTIME* /*lpTime*/,
    LPCWSTR /*lpFormat*/,
    LPWSTR lpTimeStr,
    int cchTime
) noexcept {
    const wchar_t t[] = L"19:15:00";
    constexpr int len = 9;
    if (cchTime == 0) return len;
    if (!lpTimeStr || cchTime < len) return 0;
    std::wmemcpy(lpTimeStr, t, len);
    return len;
}

inline constexpr DWORD PRODUCT_ENTERPRISE = 0x00000004;
inline BOOL GetProductInfo(
    DWORD /*dwOSMajorVersion*/,
    DWORD /*dwOSMinorVersion*/,
    DWORD /*dwSpMajorVersion*/,
    DWORD /*dwSpMinorVersion*/,
    PDWORD pdwReturnedProductType
) noexcept {
    if (pdwReturnedProductType) *pdwReturnedProductType = PRODUCT_ENTERPRISE;
    return TRUE;
}

inline BOOL GetVersionExW(LPOSVERSIONINFOEXW lpVersionInformation) noexcept {
    if (!lpVersionInformation) return FALSE;
    lpVersionInformation->dwMajorVersion = 10;
    lpVersionInformation->dwMinorVersion = 0;
    lpVersionInformation->dwBuildNumber = 26100;
    lpVersionInformation->dwPlatformId = 2; // VER_PLATFORM_WIN32_NT
    std::wmemset(lpVersionInformation->szCSDVersion, 0, 128);
    lpVersionInformation->wServicePackMajor = 0;
    lpVersionInformation->wServicePackMinor = 0;
    lpVersionInformation->wSuiteMask = 0x0100; // VER_SUITE_ENTERPRISE
    lpVersionInformation->wProductType = 1;     // VER_NT_WORKSTATION
    return TRUE;
}

inline BOOL GetVersionExA(void* lpVersionInformation) noexcept {
    if (!lpVersionInformation) return FALSE;
    auto* vi = static_cast<OSVERSIONINFOA*>(lpVersionInformation);
    vi->dwMajorVersion = 10;
    vi->dwMinorVersion = 0;
    vi->dwBuildNumber = 19045;
    vi->dwPlatformId = 2; // VER_PLATFORM_WIN32_NT
    std::strncpy(vi->szCSDVersion, "MicaNT Sovereign 64-bit", sizeof(vi->szCSDVersion) - 1);
    vi->szCSDVersion[sizeof(vi->szCSDVersion) - 1] = '\0';
    return TRUE;
}

inline HRSRC FindResourceW(HMODULE /*hModule*/, LPCWSTR /*lpName*/, LPCWSTR /*lpType*/) noexcept {
    return reinterpret_cast<HRSRC>(0x1000);
}

inline HGLOBAL_RES LoadResource(HMODULE /*hModule*/, HRSRC hResInfo) noexcept {
    return reinterpret_cast<HGLOBAL_RES>(hResInfo);
}

inline LPVOID LockResource(HGLOBAL_RES hResData) noexcept {
    static const uint8_t s_dummyResource[64] = {0};
    return (hResData != nullptr) ? const_cast<uint8_t*>(s_dummyResource) : nullptr;
}

inline DWORD SizeofResource(HMODULE /*hModule*/, HRSRC /*hResInfo*/) noexcept {
    return 64;
}

inline DWORD GetLongPathNameW(LPCWSTR lpszShortPath, LPWSTR lpszLongPath, DWORD cchBuffer) noexcept {
    if (!lpszShortPath) return 0;
    size_t len = std::wcslen(lpszShortPath);
    if (cchBuffer <= len) return static_cast<DWORD>(len + 1);
    if (!lpszLongPath) return 0;
    std::wmemcpy(lpszLongPath, lpszShortPath, len + 1);
    return static_cast<DWORD>(len);
}

inline DWORD GetFinalPathNameByHandleW(
    HANDLE /*hFile*/,
    LPWSTR lpszFilePath,
    DWORD cchFilePath,
    DWORD /*dwFlags*/
) noexcept {
    const wchar_t p[] = L"\\\\?\\C:\\Windows\\System32\\micant.exe";
    constexpr DWORD len = 34;
    if (cchFilePath <= len) return len + 1;
    if (!lpszFilePath) return 0;
    std::wmemcpy(lpszFilePath, p, len + 1);
    return len;
}

inline BOOL ReleaseMutex(HANDLE /*hMutex*/) noexcept {
    return TRUE;
}

inline HANDLE OpenProcess(DWORD /*dwDesiredAccess*/, BOOL /*bInheritHandle*/, DWORD dwProcessId) noexcept {
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(dwProcessId ? dwProcessId : 0x100));
}

inline HRESULT GetApplicationRestartSettings(HANDLE /*hProcess*/, PWSTR /*pwzCommandLine*/, PDWORD pcchSize, PDWORD pdwFlags) noexcept {
    if (pcchSize) *pcchSize = 0;
    if (pdwFlags) *pdwFlags = 0;
    return 1; // S_FALSE
}

inline HRESULT UnregisterApplicationRestart() noexcept {
    return 0; // S_OK
}

inline HRESULT RegisterApplicationRestart(PCWSTR /*pwzCommandLine*/, DWORD /*dwFlags*/) noexcept {
    return 0; // S_OK
}

// Slim Reader/Writer (SRW) Locks
inline void AcquireSRWLockExclusive(PSRWLOCK /*SRWLock*/) noexcept {}
inline void ReleaseSRWLockExclusive(PSRWLOCK /*SRWLock*/) noexcept {}
inline BOOL TryAcquireSRWLockExclusive(PSRWLOCK /*SRWLock*/) noexcept { return TRUE; }
inline void AcquireSRWLockShared(PSRWLOCK /*SRWLock*/) noexcept {}
inline void ReleaseSRWLockShared(PSRWLOCK /*SRWLock*/) noexcept {}
inline BOOL TryAcquireSRWLockShared(PSRWLOCK /*SRWLock*/) noexcept { return TRUE; }
inline void InitializeSRWLock(PSRWLOCK SRWLock) noexcept { if (SRWLock) SRWLock->Ptr = nullptr; }

// Condition Variables
inline BOOL SleepConditionVariableSRW(PCONDITION_VARIABLE /*ConditionVariable*/, PSRWLOCK /*SRWLock*/, DWORD /*dwMilliseconds*/, ULONG /*Flags*/) noexcept { return TRUE; }
inline void WakeConditionVariable(PCONDITION_VARIABLE /*ConditionVariable*/) noexcept {}
inline void WakeAllConditionVariable(PCONDITION_VARIABLE /*ConditionVariable*/) noexcept {}
inline void InitializeConditionVariable(PCONDITION_VARIABLE ConditionVariable) noexcept { if (ConditionVariable) ConditionVariable->Ptr = nullptr; }

// One-Time Initialization
inline BOOL InitOnceBeginInitialize(LPINIT_ONCE /*lpInitOnce*/, DWORD /*dwFlags*/, PBOOL fPending, LPVOID* /*lpContext*/) noexcept {
    if (fPending) *fPending = FALSE;
    return TRUE;
}
inline BOOL InitOnceComplete(LPINIT_ONCE /*lpInitOnce*/, DWORD /*dwFlags*/, LPVOID /*lpContext*/) noexcept { return TRUE; }
inline BOOL InitOnceExecuteOnce(LPINIT_ONCE InitOnce, PINIT_ONCE_FN InitFn, PVOID Parameter, LPVOID* Context) noexcept {
    if (InitFn) return InitFn(InitOnce, Parameter, Context);
    return TRUE;
}

// Threadpool Work APIs
inline PTP_WORK CreateThreadpoolWork(PTP_WORK_CALLBACK /*pfnwk*/, PVOID /*pv*/, PTP_CALLBACK_ENVIRON /*pcbe*/) noexcept {
    static uint64_t dummyWork = 0x1234;
    return reinterpret_cast<PTP_WORK>(&dummyWork);
}
inline void SubmitThreadpoolWork(PTP_WORK /*pwk*/) noexcept {}
inline void CloseThreadpoolWork(PTP_WORK /*pwk*/) noexcept {}
inline void FreeLibraryWhenCallbackReturns(PTP_CALLBACK_INSTANCE /*pci*/, HMODULE /*mod*/) noexcept {}

// Integer and String Helper APIs
inline int MulDiv(int nNumber, int nNumerator, int nDenominator) noexcept {
    if (nDenominator == 0) return -1;
    int64_t res = (static_cast<int64_t>(nNumber) * static_cast<int64_t>(nNumerator)) / nDenominator;
    return static_cast<int>(res);
}

inline int CompareStringOrdinal(LPCWCH lpString1, int cchCount1, LPCWCH lpString2, int cchCount2, BOOL bIgnoreCase) noexcept {
    if (!lpString1 && !lpString2) return 2;
    if (!lpString1) return 1;
    if (!lpString2) return 3;
    std::wstring s1(lpString1, (cchCount1 >= 0) ? cchCount1 : wcslen(lpString1));
    std::wstring s2(lpString2, (cchCount2 >= 0) ? cchCount2 : wcslen(lpString2));
    if (bIgnoreCase) {
        for (auto& c : s1) c = towlower(c);
        for (auto& c : s2) c = towlower(c);
    }
    if (s1 < s2) return 1;
    if (s1 > s2) return 3;
    return 2;
}

inline int CompareStringEx(LPCWSTR /*lpLocaleName*/, DWORD dwCmpFlags, LPCWCH lpString1, int cchCount1, LPCWCH lpString2, int cchCount2, LPVOID, LPVOID, LPARAM) noexcept {
    return CompareStringOrdinal(lpString1, cchCount1, lpString2, cchCount2, (dwCmpFlags & 0x00000001) != 0);
}

inline int LCMapStringEx(LPCWSTR, DWORD dwMapFlags, LPCWSTR lpSrcStr, int cchSrc, LPWSTR lpDestStr, int cchDest, LPVOID, LPVOID, LPARAM) noexcept {
    return LCMapStringW(0, dwMapFlags, lpSrcStr, cchSrc, lpDestStr, cchDest);
}

inline int lstrcmpW(LPCWSTR s1, LPCWSTR s2) noexcept {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    return wcscmp(s1, s2);
}

inline LPWSTR lstrcpyW(LPWSTR dst, LPCWSTR src) noexcept {
    if (!dst || !src) return dst;
    return wcscpy(dst, src);
}

inline LPSTR lstrcpynA(LPSTR dst, LPCSTR src, int maxLen) noexcept {
    if (!dst || maxLen <= 0) return dst;
    if (!src) { *dst = '\0'; return dst; }
    strncpy(dst, src, maxLen - 1);
    dst[maxLen - 1] = '\0';
    return dst;
}

inline int lstrlenW(LPCWSTR s) noexcept {
    return s ? static_cast<int>(wcslen(s)) : 0;
}

inline void GetNativeSystemInfo(SYSTEM_INFO* lpSystemInfo) noexcept {
    GetSystemInfo(lpSystemInfo);
}

inline BOOL QueryFullProcessImageNameW(HANDLE /*hProcess*/, DWORD /*dwFlags*/, LPWSTR lpExeName, PDWORD lpdwSize) noexcept {
    if (!lpExeName || !lpdwSize || *lpdwSize == 0) return FALSE;
    const wchar_t* p = L"C:\\Program Files\\Notepad++\\notepad++.exe";
    size_t len = wcslen(p);
    if (*lpdwSize <= len) { *lpdwSize = static_cast<DWORD>(len + 1); return FALSE; }
    wcscpy(lpExeName, p);
    *lpdwSize = static_cast<DWORD>(len);
    return TRUE;
}

inline SIZE_T GlobalSize(HANDLE /*hMem*/) noexcept { return 4096; }

inline BOOL CopyFileExW(LPCWSTR src, LPCWSTR dst, LPPROGRESS_ROUTINE, LPVOID, LPBOOL, DWORD) noexcept {
    return CopyFileW(src, dst, FALSE);
}

inline BOOL ReplaceFileW(LPCWSTR replaced, LPCWSTR replacement, LPCWSTR backup, DWORD, LPVOID, LPVOID) noexcept {
    if (backup) CopyFileW(replaced, backup, FALSE);
    return MoveFileExW(replacement, replaced, 1);
}

struct WIN32_FILE_ATTRIBUTE_DATA {
    DWORD dwFileAttributes{0};
    FILETIME ftCreationTime{};
    FILETIME ftLastAccessTime{};
    FILETIME ftLastWriteTime{};
    DWORD nFileSizeHigh{0};
    DWORD nFileSizeLow{0};
};

inline BOOL GetFileAttributesExW(LPCWSTR lpFileName, int /*fInfoLevelId*/, LPVOID lpFileInformation) noexcept {
    if (!lpFileName || !lpFileInformation) return FALSE;
    auto* pData = reinterpret_cast<WIN32_FILE_ATTRIBUTE_DATA*>(lpFileInformation);
    pData->dwFileAttributes = GetFileAttributesW(lpFileName);
    if (pData->dwFileAttributes == INVALID_FILE_ATTRIBUTES) return FALSE;
    return TRUE;
}

inline BOOL GetFileInformationByHandleEx(HANDLE /*hFile*/, int /*FileInformationClass*/, LPVOID lpFileInformation, DWORD dwBufferSize) noexcept {
    if (lpFileInformation && dwBufferSize > 0) memset(lpFileInformation, 0, dwBufferSize);
    return TRUE;
}

inline BOOL AreFileApisANSI() noexcept { return TRUE; }

inline HANDLE CreateToolhelp32Snapshot(DWORD /*dwFlags*/, DWORD /*th32ProcessID*/) noexcept {
    static uint64_t dummySnapshot = 0x5555;
    return reinterpret_cast<HANDLE>(&dummySnapshot);
}

inline BOOL Process32FirstW(HANDLE /*hSnapshot*/, LPPROCESSENTRY32W lppe) noexcept {
    if (!lppe || lppe->dwSize < sizeof(PROCESSENTRY32W)) return FALSE;
    lppe->th32ProcessID = 1000;
    lppe->cntThreads = 1;
    lppe->th32ParentProcessID = 0;
    wcscpy(lppe->szExeFile, L"notepad++.exe");
    return TRUE;
}

inline BOOL Process32NextW(HANDLE /*hSnapshot*/, LPPROCESSENTRY32W /*lppe*/) noexcept {
    return FALSE;
}

inline BOOL GetFileAttributesExA(LPCSTR lpFileName, int fInfoLevelId, LPVOID lpFileInformation) noexcept {
    if (!lpFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    std::string s(lpFileName);
    std::wstring w(s.begin(), s.end());
    return GetFileAttributesExW(w.c_str(), fInfoLevelId, lpFileInformation);
}

struct MODULEENTRY32 {
    DWORD   dwSize;
    DWORD   th32ModuleID;
    DWORD   th32ProcessID;
    DWORD   GlblcntUsage;
    DWORD   ProccntUsage;
    BYTE*   modBaseAddr;
    DWORD   modBaseSize;
    HMODULE hModule;
    char    szModule[256];
    char    szExePath[260];
};

inline BOOL Module32First([[maybe_unused]] HANDLE hSnapshot, MODULEENTRY32* lpme) noexcept {
    if (!lpme || lpme->dwSize < sizeof(MODULEENTRY32)) return FALSE;
    lpme->th32ModuleID = 1;
    lpme->th32ProcessID = 1000;
    lpme->modBaseAddr = reinterpret_cast<BYTE*>(0x140000000ULL);
    lpme->modBaseSize = 0x1000000;
    lpme->hModule = reinterpret_cast<HMODULE>(0x140000000ULL);
    std::strcpy(lpme->szModule, "micant_app.exe");
    std::strcpy(lpme->szExePath, "C:\\Windows\\System32\\micant_app.exe");
    return TRUE;
}

inline BOOL Module32Next([[maybe_unused]] HANDLE hSnapshot, [[maybe_unused]] MODULEENTRY32* lpme) noexcept {
    return FALSE;
}

struct PROCESSENTRY32 {
    DWORD   dwSize;
    DWORD   cntUsage;
    DWORD   th32ProcessID;
    ULONG_PTR th32DefaultHeapID;
    DWORD   th32ModuleID;
    DWORD   cntThreads;
    DWORD   th32ParentProcessID;
    LONG    pcPriClassBase;
    DWORD   dwFlags;
    char    szExeFile[260];
};

inline BOOL Process32First([[maybe_unused]] HANDLE hSnapshot, PROCESSENTRY32* lppe) noexcept {
    if (!lppe || lppe->dwSize < sizeof(PROCESSENTRY32)) return FALSE;
    lppe->cntUsage = 1;
    lppe->th32ProcessID = 1000;
    lppe->cntThreads = 4;
    lppe->th32ParentProcessID = 500;
    lppe->pcPriClassBase = 8;
    lppe->dwFlags = 0;
    std::strcpy(lppe->szExeFile, "micant_app.exe");
    return TRUE;
}

inline BOOL Process32Next([[maybe_unused]] HANDLE hSnapshot, [[maybe_unused]] PROCESSENTRY32* lppe) noexcept {
    return FALSE;
}

inline HANDLE CreateJobObjectA([[maybe_unused]] void* lpJobAttributes, [[maybe_unused]] LPCSTR lpName) noexcept {
    static uint64_t dummyJob = 0x50B00001;
    return reinterpret_cast<HANDLE>(&dummyJob);
}

inline HANDLE CreateWaitableTimerExW([[maybe_unused]] void* lpTimerAttributes,
                                    [[maybe_unused]] LPCWSTR lpTimerName,
                                    [[maybe_unused]] DWORD dwFlags,
                                    [[maybe_unused]] DWORD dwDesiredAccess) noexcept {
    static uint64_t dummyTimer = 0x713E0001;
    return reinterpret_cast<HANDLE>(&dummyTimer);
}

inline BOOL GetComputerNameA(LPSTR lpBuffer, LPDWORD nSize) noexcept {
    if (!lpBuffer || !nSize || *nSize < 7) return FALSE;
    const char name[] = "MICANT";
    std::memcpy(lpBuffer, name, sizeof(name));
    *nSize = sizeof(name) - 1;
    return TRUE;
}

inline BOOL K32EnumProcessModules([[maybe_unused]] HANDLE hProcess, void* lphModule, DWORD cb, LPDWORD lpcbNeeded) noexcept {
    if (lpcbNeeded) *lpcbNeeded = sizeof(HMODULE);
    if (lphModule && cb >= sizeof(HMODULE)) {
        *reinterpret_cast<HMODULE*>(lphModule) = reinterpret_cast<HMODULE>(0x140000000ULL);
    }
    return TRUE;
}

inline DWORD K32GetMappedFileNameW([[maybe_unused]] HANDLE hProcess, [[maybe_unused]] LPVOID lpv, LPWSTR lpFilename, DWORD nSize) noexcept {
    if (!lpFilename || nSize == 0) return 0;
    const wchar_t path[] = L"\\Device\\HarddiskVolume1\\Windows\\System32\\micant_app.exe";
    size_t len = std::min<size_t>(std::wcslen(path), nSize - 1);
    std::wcsncpy(lpFilename, path, len);
    lpFilename[len] = L'\0';
    return static_cast<DWORD>(len);
}

inline DWORD K32GetModuleFileNameExA([[maybe_unused]] HANDLE hProcess, [[maybe_unused]] HMODULE hModule, LPSTR lpFilename, DWORD nSize) noexcept {
    if (!lpFilename || nSize == 0) return 0;
    const char path[] = "C:\\Windows\\System32\\micant_app.exe";
    size_t len = std::min<size_t>(std::strlen(path), nSize - 1);
    std::strncpy(lpFilename, path, len);
    lpFilename[len] = '\0';
    return static_cast<DWORD>(len);
}

inline BOOL K32GetProcessMemoryInfo([[maybe_unused]] HANDLE hProcess, void* ppsmemCounters, DWORD cb) noexcept {
    if (!ppsmemCounters || cb < 72) return FALSE;
    std::memset(ppsmemCounters, 0, cb);
    auto* p = reinterpret_cast<uint64_t*>(ppsmemCounters);
    p[0] = cb;
    p[2] = 64 * 1024 * 1024ULL; // PeakWorkingSetSize (64MB)
    p[3] = 32 * 1024 * 1024ULL; // WorkingSetSize (32MB)
    return TRUE;
}

inline BOOL SetProcessInformation([[maybe_unused]] HANDLE hProcess,
                                  [[maybe_unused]] int ProcessInformationClass,
                                  [[maybe_unused]] void* ProcessInformation,
                                  [[maybe_unused]] DWORD ProcessInformationSize) noexcept {
    return TRUE;
}

inline BOOL SetProcessShutdownParameters([[maybe_unused]] DWORD dwLevel, [[maybe_unused]] DWORD dwFlags) noexcept {
    return TRUE;
}

inline BOOL UnmapViewOfFileEx(LPCVOID lpBaseAddress, [[maybe_unused]] DWORD UnmapFlags) noexcept {
    return UnmapViewOfFile(lpBaseAddress);
}

inline ULONG RemoveVectoredExceptionHandler([[maybe_unused]] void* Handle) noexcept {
    return 1;
}

inline BOOL ReadDirectoryChangesW(HANDLE, LPVOID, DWORD, BOOL, DWORD, LPDWORD lpBytesReturned, LPOVERLAPPED, void*) noexcept {
    if (lpBytesReturned) *lpBytesReturned = 0;
    return TRUE;
}

inline HANDLE FindFirstChangeNotificationW(LPCWSTR /*lpPathName*/, BOOL /*bWatchSubtree*/, DWORD /*dwNotifyFilter*/) noexcept {
    return reinterpret_cast<HANDLE>(0x6001);
}

inline BOOL FindNextChangeNotification(HANDLE /*hChangeHandle*/) noexcept {
    return TRUE;
}

inline BOOL FindCloseChangeNotification(HANDLE /*hChangeHandle*/) noexcept {
    return TRUE;
}

inline DWORD SleepEx(DWORD dwMilliseconds, BOOL /*bAlertable*/) noexcept {
    Sleep(dwMilliseconds);
    return 0;
}

inline DWORD WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL /*bAlertable*/) noexcept {
    return WaitForSingleObject(hHandle, dwMilliseconds);
}

inline DWORD QueueUserAPC(PAPCFUNC /*pfnAPC*/, HANDLE /*hThread*/, ULONG_PTR /*dwData*/) noexcept { return 1; }
inline BOOL CancelIo(HANDLE /*hFile*/) noexcept { return TRUE; }

inline DWORD GetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation) noexcept {
    if (lpTimeZoneInformation) {
        memset(lpTimeZoneInformation, 0, sizeof(*lpTimeZoneInformation));
        lpTimeZoneInformation->Bias = 0;
        wcscpy(lpTimeZoneInformation->StandardName, L"UTC");
    }
    return 1; // TIME_ZONE_ID_STANDARD
}

inline BOOL SystemTimeToTzSpecificLocalTime(const TIME_ZONE_INFORMATION*, const SYSTEMTIME* u, LPSYSTEMTIME l) noexcept {
    if (!u || !l) return FALSE;
    *l = *u;
    return TRUE;
}

inline int GetTimeFormatW(DWORD, DWORD, const SYSTEMTIME*, LPCWSTR, LPWSTR lpTimeStr, int cchTime) noexcept {
    if (cchTime > 0 && lpTimeStr) wcscpy(lpTimeStr, L"12:00:00");
    return 9;
}

inline int GetLocaleInfoW(LCID, DWORD, LPWSTR lpLCData, int cchData) noexcept {
    if (cchData > 0 && lpLCData) lpLCData[0] = L'\0';
    return 1;
}

inline int GetLocaleInfoA(LCID, DWORD, LPSTR lpLCData, int cchData) noexcept {
    if (cchData > 0 && lpLCData) lpLCData[0] = '\0';
    return 1;
}

inline int GetLocaleInfoEx(LPCWSTR, DWORD, LPWSTR lpLCData, int cchData) noexcept {
    if (cchData > 0 && lpLCData) lpLCData[0] = L'\0';
    return 1;
}

inline LCID GetUserDefaultLCID() noexcept { return 0x0409; }
inline WORD GetSystemDefaultLangID() noexcept { return 0x0409; }
inline WORD GetUserDefaultLangID() noexcept { return 0x0409; }

inline BOOL GetStringTypeExW(LCID, DWORD, LPCWCH, int, LPWORD lpCharType) noexcept {
    if (lpCharType) *lpCharType = 0;
    return TRUE;
}

inline BOOL GetStringTypeExA(LCID, DWORD, LPCSTR, int, LPWORD lpCharType) noexcept {
    if (lpCharType) *lpCharType = 0;
    return TRUE;
}

inline int GetNumberFormatW(LCID /*Locale*/, DWORD /*dwFlags*/, LPCWSTR lpValue, const void* /*lpFormat*/, LPWSTR lpNumberStr, int cchNumber) noexcept {
    if (!lpValue) return 0;
    size_t len = std::wcslen(lpValue);
    if (cchNumber == 0) return static_cast<int>(len + 1);
    if (lpNumberStr && cchNumber > 0) {
        size_t copy_len = (std::min)(len, static_cast<size_t>(cchNumber - 1));
        std::wcsncpy(lpNumberStr, lpValue, copy_len);
        lpNumberStr[copy_len] = L'\0';
        return static_cast<int>(copy_len + 1);
    }
    return 0;
}

inline int GetCalendarInfoW(LCID /*Locale*/, uint32_t /*Calendar*/, uint32_t /*CalType*/, LPWSTR lpCalData, int cchData, LPDWORD lpValue) noexcept {
    if (lpValue) {
        *lpValue = 1;
    }
    if (lpCalData && cchData > 0) {
        lpCalData[0] = L'1';
        lpCalData[1] = L'\0';
        return 1;
    }
    return 1;
}

inline BOOL GetStringTypeA(LCID /*Locale*/, DWORD /*dwInfoType*/, LPCSTR lpSrcStr, int cchSrc, LPWORD lpCharType) noexcept {
    if (!lpSrcStr || !lpCharType) return FALSE;
    int len = cchSrc < 0 ? static_cast<int>(std::strlen(lpSrcStr)) : cchSrc;
    for (int i = 0; i < len; ++i) {
        unsigned char c = static_cast<unsigned char>(lpSrcStr[i]);
        uint16_t flags = 0;
        if (std::isupper(c)) flags |= 0x0001; // C1_UPPER
        if (std::islower(c)) flags |= 0x0002; // C1_LOWER
        if (std::isdigit(c)) flags |= 0x0004; // C1_DIGIT
        if (std::isspace(c)) flags |= 0x0008; // C1_SPACE
        if (std::ispunct(c)) flags |= 0x0010; // C1_PUNCT
        if (std::iscntrl(c)) flags |= 0x0020; // C1_CNTRL
        if (std::isblank(c)) flags |= 0x0040; // C1_BLANK
        if (std::isxdigit(c)) flags |= 0x0080; // C1_XDIGIT
        lpCharType[i] = flags;
    }
    return TRUE;
}

inline int LCMapStringA(LCID, DWORD, LPCSTR, int, LPSTR, int) noexcept { return 0; }
inline BOOL IsValidLocale(LCID, DWORD) noexcept { return TRUE; }

inline BOOL EnumSystemLocalesW(LOCALE_ENUMPROCW p, DWORD) noexcept {
    if (p) { wchar_t loc[] = L"00000409"; p(loc); }
    return TRUE;
}

inline HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES, BOOL, LPCWSTR) noexcept {
    static uint64_t dummyMutex = 0x6666;
    return reinterpret_cast<HANDLE>(&dummyMutex);
}

inline DWORD GetTempPath2W(DWORD nBufferLength, LPWSTR lpBuffer) noexcept {
    return GetTempPathW(nBufferLength, lpBuffer);
}

inline DWORD GetTempPath2A(DWORD nBufferLength, LPSTR lpBuffer) noexcept {
    return GetTempPathA(nBufferLength, lpBuffer);
}

// Job Objects
inline HANDLE CreateJobObjectW(void* /*lpJobAttributes*/, const wchar_t* /*lpName*/) noexcept {
    return reinterpret_cast<HANDLE>(0x7101);
}

inline HANDLE OpenJobObjectW(DWORD /*dwDesiredAccess*/, BOOL /*bInheritHandle*/, const wchar_t* /*lpName*/) noexcept {
    return reinterpret_cast<HANDLE>(0x7101);
}

inline BOOL AssignProcessToJobObject(HANDLE /*hJob*/, HANDLE /*hProcess*/) noexcept {
    return TRUE;
}

inline BOOL SetInformationJobObject(HANDLE /*hJob*/, int /*JobObjectInformationClass*/, void* /*lpJobObjectInformation*/, DWORD /*cbJobObjectInformationLength*/) noexcept {
    return TRUE;
}

// Memory & File Mapping
inline HANDLE OpenFileMappingA(DWORD /*dwDesiredAccess*/, BOOL /*bInheritHandle*/, const char* /*lpName*/) noexcept {
    return reinterpret_cast<HANDLE>(0x7102);
}

inline BOOL VirtualLock(void* /*lpAddress*/, size_t /*dwSize*/) noexcept {
    return TRUE;
}

inline BOOL FlushInstructionCache(HANDLE /*hProcess*/, const void* /*lpBaseAddress*/, size_t /*dwSize*/) noexcept {
    return TRUE;
}

// Console Input
inline BOOL FlushConsoleInputBuffer(HANDLE /*hConsoleInput*/) noexcept {
    return TRUE;
}

inline BOOL PeekConsoleInputW(HANDLE /*hConsoleInput*/, void* /*lpBuffer*/, DWORD /*nLength*/, DWORD* lpNumberOfEventsRead) noexcept {
    if (lpNumberOfEventsRead) *lpNumberOfEventsRead = 0;
    return TRUE;
}

inline BOOL ReadConsoleInputW(HANDLE /*hConsoleInput*/, void* /*lpBuffer*/, DWORD /*nLength*/, DWORD* lpNumberOfEventsRead) noexcept {
    if (lpNumberOfEventsRead) *lpNumberOfEventsRead = 0;
    return TRUE;
}

inline BOOL WriteConsoleInputW(HANDLE /*hConsoleInput*/, const void* /*lpBuffer*/, DWORD nLength, DWORD* lpNumberOfEventsWritten) noexcept {
    if (lpNumberOfEventsWritten) *lpNumberOfEventsWritten = nLength;
    return TRUE;
}

// Interlocked Atomics
inline LONG InterlockedIncrement(volatile LONG* Addend) noexcept {
    LONG old = *Addend;
    *Addend = old + 1;
    return old + 1;
}

inline LONG InterlockedDecrement(volatile LONG* Addend) noexcept {
    LONG old = *Addend;
    *Addend = old - 1;
    return old - 1;
}

inline LONG InterlockedExchange(volatile LONG* Target, LONG Value) noexcept {
    LONG old = *Target;
    *Target = Value;
    return old;
}

inline LONG InterlockedExchangeAdd(volatile LONG* Addend, LONG Value) noexcept {
    LONG old = *Addend;
    *Addend += Value;
    return old;
}

inline LONG InterlockedCompareExchange(volatile LONG* Destination, LONG Exchange, LONG Comperand) noexcept {
    LONG old = *Destination;
    if (old == Comperand) {
        *Destination = Exchange;
    }
    return old;
}

// Formatting & Module
inline int GetDateFormatA(LCID /*Locale*/, DWORD /*dwFlags*/, const void* /*lpDate*/, const char* /*lpFormat*/, char* lpDateStr, int cchDate) noexcept {
    const char* dateStr = "2026-10-09";
    if (lpDateStr && cchDate > 10) {
        std::strcpy(lpDateStr, dateStr);
        return 11;
    }
    return 11;
}

inline BOOL GetModuleHandleExA(DWORD /*dwFlags*/, const char* /*lpModuleName*/, HMODULE* phModule) noexcept {
    if (phModule) *phModule = reinterpret_cast<HMODULE>(0x7FFE0000);
    return TRUE;
}

inline BOOL IsBadWritePtr(void* /*lp*/, size_t /*ucb*/) noexcept {
    return FALSE;
}

inline HMODULE LoadLibraryA(LPCSTR lpLibFileName) noexcept {
    if (!lpLibFileName) return nullptr;
    std::string s(lpLibFileName);
    std::wstring ws(s.begin(), s.end());
    return LoadLibraryW(ws.c_str());
}

inline HMODULE LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags) noexcept {
    return LoadLibraryA(lpLibFileName);
}

inline DWORD FormatMessageA(DWORD /*dwFlags*/, LPCVOID /*lpSource*/, DWORD dwMessageId, DWORD /*dwLanguageId*/, LPSTR lpBuffer, DWORD nSize, va_list* /*Arguments*/) noexcept {
    if (lpBuffer && nSize > 0) {
        snprintf(lpBuffer, nSize, "MicaNT Error 0x%X", static_cast<unsigned int>(dwMessageId));
        return static_cast<DWORD>(strlen(lpBuffer));
    }
    return 0;
}

inline BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD /*dwSpinCount*/) noexcept {
    InitializeCriticalSection(lpCriticalSection);
    return TRUE;
}

inline BOOL ReadConsoleW(HANDLE, LPVOID, DWORD, LPDWORD lpNumberOfCharsRead, LPVOID) noexcept {
    if (lpNumberOfCharsRead) *lpNumberOfCharsRead = 0;
    return TRUE;
}

inline void RtlUnwind(void*, void*, void*, void*) noexcept {}

[[noreturn]] inline void ExitThread(DWORD dwExitCode) noexcept {
    if (g_ExitThreadHook) {
        g_ExitThreadHook(dwExitCode);
    }
    DWORD pid = GetCurrentProcessId();
    csrss::CsrSubsystemServer::get().terminateProcess(pid, dwExitCode);
    std::exit(static_cast<int>(dwExitCode));
}

[[noreturn]] inline void FreeLibraryAndExitThread(HMODULE hLib, DWORD code) noexcept {
    FreeLibrary(hLib);
    ExitThread(code);
}

struct SYSTEM_POWER_STATUS {
    uint8_t  ACLineStatus;
    uint8_t  BatteryFlag;
    uint8_t  BatteryLifePercent;
    uint8_t  SystemStatusFlag;
    uint32_t BatteryLifeTime;
    uint32_t BatteryFullLifeTime;
};

inline BOOL WINAPI DisableThreadLibraryCalls([[maybe_unused]] HMODULE hLibModule) noexcept {
    return TRUE;
}

inline BOOL WINAPI SetProcessDEPPolicy([[maybe_unused]] DWORD dwFlags) noexcept {
    return TRUE;
}

inline BOOL WINAPI SetDllDirectoryA([[maybe_unused]] LPCSTR lpPathName) noexcept {
    return TRUE;
}

inline BOOL WINAPI GetSystemPowerStatus(SYSTEM_POWER_STATUS* lpSystemPowerStatus) noexcept {
    if (!lpSystemPowerStatus) return FALSE;
    lpSystemPowerStatus->ACLineStatus = 1;         // AC Online
    lpSystemPowerStatus->BatteryFlag = 128;        // No system battery (desktop)
    lpSystemPowerStatus->BatteryLifePercent = 255; // Unknown
    lpSystemPowerStatus->SystemStatusFlag = 0;
    lpSystemPowerStatus->BatteryLifeTime = 0xFFFFFFFF;
    lpSystemPowerStatus->BatteryFullLifeTime = 0xFFFFFFFF;
    return TRUE;
}

inline DWORD WINAPI GetActiveProcessorCount([[maybe_unused]] uint16_t groupNumber) noexcept {
    return 8; // Sovereign 8-core logical configuration
}

inline BOOL WINAPI GetHandleInformation([[maybe_unused]] HANDLE hObject, DWORD* lpdwFlags) noexcept {
    if (lpdwFlags) {
        *lpdwFlags = 0;
    }
    return TRUE;
}

inline DWORD WINAPI GetProfileStringW([[maybe_unused]] LPCWSTR lpAppName,
                                      [[maybe_unused]] LPCWSTR lpKeyName,
                                      LPCWSTR lpDefault,
                                      LPWSTR lpReturnedString,
                                      DWORD nSize) noexcept {
    if (!lpReturnedString || nSize == 0) return 0;
    if (lpDefault) {
        size_t len = std::wcslen(lpDefault);
        if (len >= nSize) len = nSize - 1;
        std::wmemcpy(lpReturnedString, lpDefault, len);
        lpReturnedString[len] = L'\0';
        return static_cast<DWORD>(len);
    }
    lpReturnedString[0] = L'\0';
    return 0;
}

inline int WINAPI IdnToAscii([[maybe_unused]] DWORD dwFlags,
                              LPCWSTR lpUnicodeCharStr,
                              int cchUnicodeChar,
                              LPWSTR lpASCIICharStr,
                              int cchASCIIChar) noexcept {
    if (!lpUnicodeCharStr) return 0;
    int srcLen = (cchUnicodeChar == -1) ? static_cast<int>(std::wcslen(lpUnicodeCharStr) + 1) : cchUnicodeChar;
    if (cchASCIIChar == 0) return srcLen;
    if (!lpASCIICharStr) return 0;
    int copyLen = (srcLen < cchASCIIChar) ? srcLen : cchASCIIChar;
    std::wmemcpy(lpASCIICharStr, lpUnicodeCharStr, copyLen);
    return copyLen;
}

inline BOOL WINAPI IsBadStringPtrA([[maybe_unused]] LPCSTR lpsz, [[maybe_unused]] size_t ucchMax) noexcept {
    return FALSE; // Sovereign memory is valid
}

inline BOOL WINAPI SetProcessPriorityBoost([[maybe_unused]] HANDLE hProcess,
                                            [[maybe_unused]] BOOL bDisablePriorityBoost) noexcept {
    return TRUE;
}

inline BOOL WINAPI SetPriorityClass([[maybe_unused]] HANDLE hProcess, [[maybe_unused]] DWORD dwPriorityClass) noexcept {
    return TRUE;
}

inline BOOL WINAPI SetThreadContext([[maybe_unused]] HANDLE hThread,
                                     [[maybe_unused]] const void* lpContext) noexcept {
    return TRUE;
}

inline HANDLE WINAPI FindFirstVolumeW(LPWSTR lpszVolumeName, DWORD cchBufferLength) noexcept {
    if (!lpszVolumeName || cchBufferLength < 50) return nullptr;
    const wchar_t mockVolume[] = L"\\\\?\\Volume{11111111-2222-3333-4444-555555555555}\\";
    std::wmemcpy(lpszVolumeName, mockVolume, std::wcslen(mockVolume) + 1);
    return reinterpret_cast<HANDLE>(0xFEED0001);
}

inline BOOL WINAPI FindNextVolumeW([[maybe_unused]] HANDLE hFindVolume,
                                    [[maybe_unused]] LPWSTR lpszVolumeName,
                                    [[maybe_unused]] DWORD cchBufferLength) noexcept {
    SetLastError(18); // ERROR_NO_MORE_FILES
    return FALSE;
}

inline BOOL WINAPI FindVolumeClose([[maybe_unused]] HANDLE hFindVolume) noexcept {
    return TRUE;
}

inline DWORD WINAPI GetFinalPathNameByHandleA([[maybe_unused]] HANDLE hFile,
                                              char* lpszFilePath,
                                              DWORD cchFilePath,
                                              [[maybe_unused]] DWORD dwFlags) noexcept {
    if (!lpszFilePath || cchFilePath < 16) return 0;
    const char mockPath[] = "C:\\MicaNT\\Volume";
    size_t len = std::strlen(mockPath);
    std::memcpy(lpszFilePath, mockPath, len + 1);
    return static_cast<DWORD>(len);
}

inline BOOL WINAPI FillConsoleOutputCharacterW([[maybe_unused]] HANDLE hConsoleOutput,
                                                [[maybe_unused]] wchar_t cCharacter,
                                                DWORD nLength,
                                                [[maybe_unused]] void* dwWriteCoord,
                                                DWORD* lpNumberOfCharsWritten) noexcept {
    if (lpNumberOfCharsWritten) {
        *lpNumberOfCharsWritten = nLength;
    }
    return TRUE;
}

inline BOOL WINAPI ReadConsoleOutputCharacterA([[maybe_unused]] HANDLE hConsoleOutput,
                                                [[maybe_unused]] char* lpCharacter,
                                                [[maybe_unused]] DWORD nLength,
                                                [[maybe_unused]] void* dwReadCoord,
                                                DWORD* lpNumberOfCharsRead) noexcept {
    if (lpNumberOfCharsRead) {
        *lpNumberOfCharsRead = 0;
    }
    return TRUE;
}

inline BOOL WINAPI GetThreadGroupAffinity([[maybe_unused]] HANDLE hThread, void* GroupAffinity) noexcept {
    if (!GroupAffinity) return FALSE;
    struct GROUP_AFFINITY_MOCK {
        uint64_t Mask{0x0F}; // 4 cores
        uint16_t Group{0};
        uint16_t Reserved[3]{0};
    };
    *reinterpret_cast<GROUP_AFFINITY_MOCK*>(GroupAffinity) = GROUP_AFFINITY_MOCK{};
    return TRUE;
}

inline BOOL WINAPI DosDateTimeToFileTime(uint16_t wFatDate, uint16_t wFatTime, void* lpFileTime) noexcept {
    if (!lpFileTime) return FALSE;
    auto* ft = reinterpret_cast<uint64_t*>(lpFileTime);
    uint32_t year = 1980 + ((wFatDate >> 9) & 0x7F);
    uint32_t month = (wFatDate >> 5) & 0x0F;
    uint32_t day = wFatDate & 0x1F;
    uint32_t hour = (wFatTime >> 11) & 0x1F;
    uint32_t min = (wFatTime >> 5) & 0x3F;
    uint32_t sec = (wFatTime & 0x1F) * 2;
    // Approximate 100-nanosecond intervals since Jan 1, 1601
    uint64_t totalSeconds = static_cast<uint64_t>(year - 1601) * 31536000ULL +
                            static_cast<uint64_t>(month * 30 + day) * 86400ULL +
                            static_cast<uint64_t>(hour * 3600 + min * 60 + sec);
    *ft = totalSeconds * 10000000ULL;
    return TRUE;
}

inline BOOL WINAPI SystemTimeToFileTime(const void* lpSystemTime, void* lpFileTime) noexcept {
    if (!lpSystemTime || !lpFileTime) return FALSE;
    auto* st = reinterpret_cast<const uint16_t*>(lpSystemTime);
    auto* ft = reinterpret_cast<uint64_t*>(lpFileTime);
    uint16_t year = st[0];
    uint16_t month = st[1];
    uint16_t day = st[3];
    uint16_t hour = st[4];
    uint16_t min = st[5];
    uint16_t sec = st[6];
    uint64_t totalSeconds = static_cast<uint64_t>(year > 1601 ? year - 1601 : 0) * 31536000ULL +
                            static_cast<uint64_t>(month * 30 + day) * 86400ULL +
                            static_cast<uint64_t>(hour * 3600 + min * 60 + sec);
    *ft = totalSeconds * 10000000ULL;
    return TRUE;
}

inline BOOL WINAPI TzSpecificLocalTimeToSystemTime([[maybe_unused]] const void* lpTimeZoneInformation, const void* lpLocalTime, void* lpUniversalTime) noexcept {
    if (!lpLocalTime || !lpUniversalTime) return FALSE;
    std::memcpy(lpUniversalTime, lpLocalTime, 16);
    return TRUE;
}

inline BOOL WINAPI GetFileTime([[maybe_unused]] HANDLE hFile, void* lpCreationTime, void* lpLastAccessTime, void* lpLastWriteTime) noexcept {
    uint64_t mockTime = 133500000000000000ULL;
    if (lpCreationTime) *reinterpret_cast<uint64_t*>(lpCreationTime) = mockTime;
    if (lpLastAccessTime) *reinterpret_cast<uint64_t*>(lpLastAccessTime) = mockTime;
    if (lpLastWriteTime) *reinterpret_cast<uint64_t*>(lpLastWriteTime) = mockTime;
    return TRUE;
}

inline DWORD WINAPI GetLogicalDrives() noexcept {
    return 0x0000000C; // Bit 2 = C:, Bit 3 = D:
}

inline BOOL WINAPI GetVolumePathNameW(LPCWSTR lpszFileName, LPWSTR lpszVolumePathName, DWORD cchBufferLength) noexcept {
    if (!lpszVolumePathName || cchBufferLength < 4) return FALSE;
    if (lpszFileName && lpszFileName[0] != L'\0' && lpszFileName[1] == L':') {
        lpszVolumePathName[0] = lpszFileName[0];
        lpszVolumePathName[1] = L':';
        lpszVolumePathName[2] = L'\\';
        lpszVolumePathName[3] = L'\0';
    } else {
        std::wcsncpy(lpszVolumePathName, L"C:\\", cchBufferLength);
    }
    return TRUE;
}

inline int WINAPI FoldStringW([[maybe_unused]] DWORD dwMapFlags, LPCWSTR lpSrcStr, int cchSrc, LPWSTR lpDestStr, int cchDest) noexcept {
    if (!lpSrcStr) return 0;
    int srcLen = (cchSrc < 0) ? static_cast<int>(std::wcslen(lpSrcStr) + 1) : cchSrc;
    if (cchDest == 0 || !lpDestStr) return srcLen;
    int copyLen = std::min<int>(srcLen, cchDest);
    std::wcsncpy(lpDestStr, lpSrcStr, copyLen);
    return copyLen;
}

inline BOOL WINAPI IsDBCSLeadByte([[maybe_unused]] uint8_t TestChar) noexcept {
    return FALSE; // Pure Unicode / single-byte ASCII baseline
}

inline BOOL WINAPI Thread32First([[maybe_unused]] HANDLE hSnapshot, void* lpte) noexcept {
    if (!lpte) return FALSE;
    struct THREADENTRY32 {
        uint32_t dwSize;
        uint32_t cntUsage;
        uint32_t th32ThreadID;
        uint32_t th32OwnerProcessID;
        int32_t  tpBasePri;
        int32_t  tpDeltaPri;
        uint32_t dwFlags;
    };
    auto* te = reinterpret_cast<THREADENTRY32*>(lpte);
    te->th32ThreadID = 1001;
    te->th32OwnerProcessID = 500;
    return TRUE;
}

inline BOOL WINAPI Thread32Next([[maybe_unused]] HANDLE hSnapshot, [[maybe_unused]] void* lpte) noexcept {
    return FALSE; // End of enumeration
}

inline BOOL WINAPI Module32FirstW([[maybe_unused]] HANDLE hSnapshot, void* lpme) noexcept {
    if (!lpme) return FALSE;
    struct MODULEENTRY32W {
        uint32_t dwSize;
        uint32_t th32ModuleID;
        uint32_t th32ProcessID;
        uint32_t GlblcntUsage;
        uint32_t ProccntUsage;
        uint8_t* modBaseAddr;
        uint32_t modBaseSize;
        void*    hModule;
        wchar_t  szModule[256];
        wchar_t  szExePath[260];
    };
    auto* me = reinterpret_cast<MODULEENTRY32W*>(lpme);
    me->th32ModuleID = 1;
    me->th32ProcessID = 500;
    me->modBaseAddr = reinterpret_cast<uint8_t*>(0x140000000);
    me->modBaseSize = 0x200000;
    std::wcsncpy(me->szModule, L"SumatraPDF-64.exe", 255);
    std::wcsncpy(me->szExePath, L"D:\\MicaNT_Apps\\Tier1\\SumatraPDF\\SumatraPDF-3.6.1-64.exe", 259);
    return TRUE;
}

inline BOOL WINAPI Module32NextW([[maybe_unused]] HANDLE hSnapshot, [[maybe_unused]] void* lpme) noexcept {
    return FALSE;
}

inline BOOL WINAPI AttachConsole([[maybe_unused]] DWORD dwProcessId) noexcept {
    return TRUE;
}

inline BOOL WINAPI SetConsoleScreenBufferSize([[maybe_unused]] HANDLE hConsoleOutput, [[maybe_unused]] uint32_t dwSize) noexcept {
    return TRUE;
}

inline DWORD WINAPI GetTempFileNameW(LPCWSTR lpPathName, LPCWSTR lpPrefixString, UINT uUnique, LPWSTR lpTempFileName) noexcept {
    if (!lpTempFileName) return 0;
    static uint32_t s_uid = 1;
    uint32_t id = (uUnique != 0) ? uUnique : s_uid++;
    const wchar_t* prefix = lpPrefixString ? lpPrefixString : L"TMP";
    const wchar_t* path = lpPathName ? lpPathName : L"C:\\Temp\\";
    std::swprintf(lpTempFileName, 260, L"%s%s%04X.tmp", path, prefix, id);
    return id;
}

inline void WINAPI OutputDebugStringA([[maybe_unused]] LPCSTR lpOutputString) noexcept {}

inline DWORD WINAPI SetThreadExecutionState(DWORD esFlags) noexcept {
    return esFlags;
}

inline void WINAPI DebugBreak() noexcept {}

inline void* WINAPI AddVectoredExceptionHandler([[maybe_unused]] ULONG First, [[maybe_unused]] void* Handler) noexcept {
    return reinterpret_cast<void*>(0x1000);
}

inline DWORD WINAPI GetPrivateProfileIntW([[maybe_unused]] LPCWSTR lpAppName, [[maybe_unused]] LPCWSTR lpKeyName, int nDefault, [[maybe_unused]] LPCWSTR lpFileName) noexcept {
    return static_cast<DWORD>(nDefault);
}

inline BOOL WINAPI HeapQueryInformation([[maybe_unused]] HANDLE HeapHandle, [[maybe_unused]] int HeapInformationClass, [[maybe_unused]] void* HeapInformation, [[maybe_unused]] size_t HeapInformationLength, size_t* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 4;
    return TRUE;
}

inline BOOL Beep([[maybe_unused]] DWORD dwFreq, [[maybe_unused]] DWORD dwDuration) noexcept {
    return TRUE;
}

inline BOOL ClearCommBreak([[maybe_unused]] HANDLE hFile) noexcept { return TRUE; }
inline BOOL SetCommBreak([[maybe_unused]] HANDLE hFile) noexcept { return TRUE; }

inline BOOL GetCommState([[maybe_unused]] HANDLE hFile, DCB* lpDCB) noexcept {
    if (!lpDCB) return FALSE;
    lpDCB->DCBlength = sizeof(DCB);
    lpDCB->BaudRate = 115200;
    lpDCB->ByteSize = 8;
    lpDCB->Parity = 0;
    lpDCB->StopBits = 0;
    return TRUE;
}

inline BOOL SetCommState([[maybe_unused]] HANDLE hFile, [[maybe_unused]] DCB* lpDCB) noexcept { return TRUE; }

inline BOOL SetCommTimeouts([[maybe_unused]] HANDLE hFile, [[maybe_unused]] COMMTIMEOUTS* lpCommTimeouts) noexcept { return TRUE; }

inline BOOL SetHandleInformation([[maybe_unused]] HANDLE hObject, [[maybe_unused]] DWORD dwMask, [[maybe_unused]] DWORD dwFlags) noexcept { return TRUE; }

inline HANDLE CreateEventA([[maybe_unused]] void* lpEventAttributes, [[maybe_unused]] BOOL bManualReset, [[maybe_unused]] BOOL bInitialState, [[maybe_unused]] const char* lpName) noexcept {
    static uintptr_t s_eventHandle = 0x3000;
    return reinterpret_cast<HANDLE>(++s_eventHandle);
}

inline HANDLE CreateMutexA([[maybe_unused]] void* lpMutexAttributes, [[maybe_unused]] BOOL bInitialOwner, [[maybe_unused]] const char* lpName) noexcept {
    static uintptr_t s_mutexHandle = 0x4000;
    return reinterpret_cast<HANDLE>(++s_mutexHandle);
}

inline HANDLE CreateFileMappingA(HANDLE hFile, void* lpAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, [[maybe_unused]] const char* lpName) noexcept {
    return CreateFileMappingW(hFile, lpAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, nullptr);
}

inline HANDLE CreateNamedPipeA(const char* lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances, DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut, void* lpSecurityAttributes) noexcept {
    std::wstring wName;
    if (lpName) {
        while (*lpName) wName.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*lpName++)));
    }
    return CreateNamedPipeW(wName.c_str(), dwOpenMode, dwPipeMode, nMaxInstances, nOutBufferSize, nInBufferSize, nDefaultTimeOut, lpSecurityAttributes);
}

inline BOOL WaitNamedPipeA([[maybe_unused]] const char* lpNamedPipeName, [[maybe_unused]] DWORD nTimeOut) noexcept {
    return TRUE;
}

inline BOOL CreatePipe(HANDLE* hReadPipe, HANDLE* hWritePipe, [[maybe_unused]] void* lpPipeAttributes, [[maybe_unused]] DWORD nSize) noexcept {
    if (!hReadPipe || !hWritePipe) return FALSE;
    static uintptr_t s_pipeHandle = 0x5000;
    *hReadPipe = reinterpret_cast<HANDLE>(++s_pipeHandle);
    *hWritePipe = reinterpret_cast<HANDLE>(++s_pipeHandle);
    return TRUE;
}

inline void* FindResourceA(void* hModule, const char* lpName, const char* lpType) noexcept {
    std::wstring wName, wType;
    if (lpName) {
        if (reinterpret_cast<uintptr_t>(lpName) <= 0xFFFF) {
            wName = reinterpret_cast<const wchar_t*>(lpName);
        } else {
            const char* p = lpName;
            while (*p) wName.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p++)));
        }
    }
    if (lpType) {
        if (reinterpret_cast<uintptr_t>(lpType) <= 0xFFFF) {
            wType = reinterpret_cast<const wchar_t*>(lpType);
        } else {
            const char* p = lpType;
            while (*p) wType.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p++)));
        }
    }
    return FindResourceW(hModule, wName.c_str(), wType.c_str());
}

inline BOOL GetOverlappedResult([[maybe_unused]] HANDLE hFile, OVERLAPPED* lpOverlapped, DWORD* lpNumberOfBytesTransferred, [[maybe_unused]] BOOL bWait) noexcept {
    if (lpNumberOfBytesTransferred) *lpNumberOfBytesTransferred = 0;
    if (lpOverlapped) lpOverlapped->Internal = 0;
    return TRUE;
}

inline UINT GetSystemDirectoryA(char* lpBuffer, UINT uSize) noexcept {
    static constexpr const char* SYS_DIR = "C:\\Windows\\System32";
    UINT len = static_cast<UINT>(std::strlen(SYS_DIR));
    if (!lpBuffer || uSize < len + 1) return len + 1;
    std::memcpy(lpBuffer, SYS_DIR, len + 1);
    return len;
}

inline UINT GetWindowsDirectoryA(char* lpBuffer, UINT uSize) noexcept {
    static constexpr const char* WIN_DIR = "C:\\Windows";
    UINT len = static_cast<UINT>(std::strlen(WIN_DIR));
    if (!lpBuffer || uSize < len + 1) return len + 1;
    std::memcpy(lpBuffer, WIN_DIR, len + 1);
    return len;
}

inline BOOL GetThreadTimes([[maybe_unused]] HANDLE hThread, void* lpCreationTime, void* lpExitTime, void* lpKernelTime, void* lpUserTime) noexcept {
    if (lpCreationTime) std::memset(lpCreationTime, 0, 8);
    if (lpExitTime) std::memset(lpExitTime, 0, 8);
    if (lpKernelTime) std::memset(lpKernelTime, 0, 8);
    if (lpUserTime) std::memset(lpUserTime, 0, 8);
    return TRUE;
}

inline void GlobalMemoryStatus(MEMORYSTATUS* lpBuffer) noexcept {
    if (!lpBuffer) return;
    *lpBuffer = MEMORYSTATUS{};
}

inline BOOL LocalFileTimeToFileTime(const void* lpLocalFileTime, void* lpFileTime) noexcept {
    if (!lpLocalFileTime || !lpFileTime) return FALSE;
    std::memcpy(lpFileTime, lpLocalFileTime, 8);
    return TRUE;
}

// ============================================================================
// 18. Win32 Dynamic Subsystem Export Table Initializer
// ============================================================================

/**
 * @brief Registers all clean-room Win32 and NTDLL exports into the userland dynamic loader.
 */
inline void InitializeWin32SubsystemExports() {
    static bool s_Initialized = false;
    if (s_Initialized) return;
    s_Initialized = true;

    auto& ldr = ldr::DynamicLoader::get();

    // kernel32.dll exports
    ldr.registerExport("kernel32.dll", "GetProcessHeap", reinterpret_cast<void*>(GetProcessHeap));
    ldr.registerExport("kernel32.dll", "HeapAlloc", reinterpret_cast<void*>(HeapAlloc));
    ldr.registerExport("kernel32.dll", "HeapFree", reinterpret_cast<void*>(HeapFree));
    ldr.registerExport("kernel32.dll", "HeapReAlloc", reinterpret_cast<void*>(HeapReAlloc));
    ldr.registerExport("kernel32.dll", "HeapSize", reinterpret_cast<void*>(HeapSize));
    ldr.registerExport("kernel32.dll", "LocalAlloc", reinterpret_cast<void*>(LocalAlloc));
    ldr.registerExport("kernel32.dll", "LocalFree", reinterpret_cast<void*>(LocalFree));
    ldr.registerExport("kernel32.dll", "VirtualAlloc", reinterpret_cast<void*>(VirtualAlloc));
    ldr.registerExport("kernel32.dll", "VirtualFree", reinterpret_cast<void*>(VirtualFree));
    ldr.registerExport("kernel32.dll", "GetCurrentProcess", reinterpret_cast<void*>(GetCurrentProcess));
    ldr.registerExport("kernel32.dll", "GetCurrentProcessId", reinterpret_cast<void*>(GetCurrentProcessId));
    ldr.registerExport("kernel32.dll", "GetCurrentThread", reinterpret_cast<void*>(GetCurrentThread));
    ldr.registerExport("kernel32.dll", "GetCurrentThreadId", reinterpret_cast<void*>(GetCurrentThreadId));
    ldr.registerExport("kernel32.dll", "ExitProcess", reinterpret_cast<void*>(ExitProcess));
    ldr.registerExport("kernel32.dll", "CreateProcessW", reinterpret_cast<void*>(CreateProcessW));
    ldr.registerExport("kernel32.dll", "CreateProcessA", reinterpret_cast<void*>(CreateProcessA));
    ldr.registerExport("kernel32.dll", "AllocConsole", reinterpret_cast<void*>(AllocConsole));
    ldr.registerExport("kernel32.dll", "FreeConsole", reinterpret_cast<void*>(FreeConsole));
    ldr.registerExport("kernel32.dll", "SetConsoleTitleW", reinterpret_cast<void*>(SetConsoleTitleW));
    ldr.registerExport("kernel32.dll", "GetConsoleTitleW", reinterpret_cast<void*>(GetConsoleTitleW));
    ldr.registerExport("kernel32.dll", "GetStdHandle", reinterpret_cast<void*>(GetStdHandle));
    ldr.registerExport("kernel32.dll", "SetStdHandle", reinterpret_cast<void*>(SetStdHandle));
    ldr.registerExport("kernel32.dll", "WriteConsoleW", reinterpret_cast<void*>(WriteConsoleW));
    ldr.registerExport("kernel32.dll", "WriteConsoleA", reinterpret_cast<void*>(WriteConsoleA));
    ldr.registerExport("kernel32.dll", "GetConsoleMode", reinterpret_cast<void*>(GetConsoleMode));
    ldr.registerExport("kernel32.dll", "SetConsoleMode", reinterpret_cast<void*>(SetConsoleMode));
    ldr.registerExport("kernel32.dll", "GetConsoleScreenBufferInfo", reinterpret_cast<void*>(GetConsoleScreenBufferInfo));
    ldr.registerExport("kernel32.dll", "SetConsoleTextAttribute", reinterpret_cast<void*>(SetConsoleTextAttribute));
    ldr.registerExport("kernel32.dll", "SetConsoleCursorPosition", reinterpret_cast<void*>(SetConsoleCursorPosition));
    ldr.registerExport("kernel32.dll", "CreateFileW", reinterpret_cast<void*>(CreateFileW));
    ldr.registerExport("kernel32.dll", "ReadFile", reinterpret_cast<void*>(ReadFile));
    ldr.registerExport("kernel32.dll", "WriteFile", reinterpret_cast<void*>(WriteFile));
    ldr.registerExport("kernel32.dll", "CloseHandle", reinterpret_cast<void*>(CloseHandle));
    ldr.registerExport("kernel32.dll", "GetFileSize", reinterpret_cast<void*>(GetFileSize));
    ldr.registerExport("kernel32.dll", "GetFileSizeEx", reinterpret_cast<void*>(GetFileSizeEx));
    ldr.registerExport("kernel32.dll", "SetFilePointer", reinterpret_cast<void*>(SetFilePointer));
    ldr.registerExport("kernel32.dll", "SetFilePointerEx", reinterpret_cast<void*>(SetFilePointerEx));
    ldr.registerExport("kernel32.dll", "GetFileAttributesW", reinterpret_cast<void*>(GetFileAttributesW));
    ldr.registerExport("kernel32.dll", "GetFileAttributesA", reinterpret_cast<void*>(GetFileAttributesA));
    ldr.registerExport("kernel32.dll", "SetFileAttributesW", reinterpret_cast<void*>(SetFileAttributesW));
    ldr.registerExport("kernel32.dll", "DeleteFileW", reinterpret_cast<void*>(DeleteFileW));
    ldr.registerExport("kernel32.dll", "DeleteFileA", reinterpret_cast<void*>(DeleteFileA));
    ldr.registerExport("kernel32.dll", "CopyFileW", reinterpret_cast<void*>(CopyFileW));
    ldr.registerExport("kernel32.dll", "CopyFileA", reinterpret_cast<void*>(CopyFileA));
    ldr.registerExport("kernel32.dll", "MoveFileW", reinterpret_cast<void*>(MoveFileW));
    ldr.registerExport("kernel32.dll", "MoveFileA", reinterpret_cast<void*>(MoveFileA));
    ldr.registerExport("kernel32.dll", "MoveFileExW", reinterpret_cast<void*>(MoveFileExW));
    ldr.registerExport("kernel32.dll", "CreateDirectoryW", reinterpret_cast<void*>(CreateDirectoryW));
    ldr.registerExport("kernel32.dll", "CreateDirectoryA", reinterpret_cast<void*>(CreateDirectoryA));
    ldr.registerExport("kernel32.dll", "RemoveDirectoryW", reinterpret_cast<void*>(RemoveDirectoryW));
    ldr.registerExport("kernel32.dll", "RemoveDirectoryA", reinterpret_cast<void*>(RemoveDirectoryA));
    ldr.registerExport("kernel32.dll", "FindFirstFileW", reinterpret_cast<void*>(FindFirstFileW));
    ldr.registerExport("kernel32.dll", "FindNextFileW", reinterpret_cast<void*>(FindNextFileW));
    ldr.registerExport("kernel32.dll", "FindClose", reinterpret_cast<void*>(FindClose));
    ldr.registerExport("kernel32.dll", "CreateEventW", reinterpret_cast<void*>(CreateEventW));
    ldr.registerExport("kernel32.dll", "SetEvent", reinterpret_cast<void*>(SetEvent));
    ldr.registerExport("kernel32.dll", "ResetEvent", reinterpret_cast<void*>(ResetEvent));
    ldr.registerExport("kernel32.dll", "WaitForSingleObject", reinterpret_cast<void*>(WaitForSingleObject));
    ldr.registerExport("kernel32.dll", "WaitForMultipleObjects", reinterpret_cast<void*>(WaitForMultipleObjects));
    ldr.registerExport("kernel32.dll", "Sleep", reinterpret_cast<void*>(Sleep));
    ldr.registerExport("kernel32.dll", "GetTickCount64", reinterpret_cast<void*>(GetTickCount64));
    ldr.registerExport("kernel32.dll", "GetSystemInfo", reinterpret_cast<void*>(GetSystemInfo));
    ldr.registerExport("kernel32.dll", "LoadLibraryW", reinterpret_cast<void*>(LoadLibraryW));
    ldr.registerExport("kernel32.dll", "GetProcAddress", reinterpret_cast<void*>(GetProcAddress));
    ldr.registerExport("kernel32.dll", "FreeLibrary", reinterpret_cast<void*>(FreeLibrary));
    ldr.registerExport("kernel32.dll", "GetLastError", reinterpret_cast<void*>(GetLastError));
    ldr.registerExport("kernel32.dll", "SetLastError", reinterpret_cast<void*>(SetLastError));
    ldr.registerExport("kernel32.dll", "GetCommandLineA", reinterpret_cast<void*>(GetCommandLineA));
    ldr.registerExport("kernel32.dll", "GetCommandLineW", reinterpret_cast<void*>(GetCommandLineW));
    ldr.registerExport("kernel32.dll", "GetEnvironmentVariableA", reinterpret_cast<void*>(GetEnvironmentVariableA));
    ldr.registerExport("kernel32.dll", "GetEnvironmentVariableW", reinterpret_cast<void*>(GetEnvironmentVariableW));
    ldr.registerExport("kernel32.dll", "SetEnvironmentVariableA", reinterpret_cast<void*>(SetEnvironmentVariableA));
    ldr.registerExport("kernel32.dll", "SetEnvironmentVariableW", reinterpret_cast<void*>(SetEnvironmentVariableW));
    ldr.registerExport("kernel32.dll", "GetCurrentDirectoryA", reinterpret_cast<void*>(GetCurrentDirectoryA));
    ldr.registerExport("kernel32.dll", "GetCurrentDirectoryW", reinterpret_cast<void*>(GetCurrentDirectoryW));
    ldr.registerExport("kernel32.dll", "SetCurrentDirectoryA", reinterpret_cast<void*>(SetCurrentDirectoryA));
    ldr.registerExport("kernel32.dll", "SetCurrentDirectoryW", reinterpret_cast<void*>(SetCurrentDirectoryW));
    ldr.registerExport("kernel32.dll", "GetFullPathNameA", reinterpret_cast<void*>(GetFullPathNameA));
    ldr.registerExport("kernel32.dll", "GetFullPathNameW", reinterpret_cast<void*>(GetFullPathNameW));
    ldr.registerExport("kernel32.dll", "GetModuleFileNameA", reinterpret_cast<void*>(GetModuleFileNameA));
    ldr.registerExport("kernel32.dll", "GetModuleFileNameW", reinterpret_cast<void*>(GetModuleFileNameW));
    ldr.registerExport("kernel32.dll", "GetModuleHandleA", reinterpret_cast<void*>(GetModuleHandleA));
    ldr.registerExport("kernel32.dll", "GetModuleHandleW", reinterpret_cast<void*>(GetModuleHandleW));
    ldr.registerExport("kernel32.dll", "QueryPerformanceCounter", reinterpret_cast<void*>(QueryPerformanceCounter));
    ldr.registerExport("kernel32.dll", "QueryPerformanceFrequency", reinterpret_cast<void*>(QueryPerformanceFrequency));
    ldr.registerExport("kernel32.dll", "GetSystemTime", reinterpret_cast<void*>(GetSystemTime));
    ldr.registerExport("kernel32.dll", "GetLocalTime", reinterpret_cast<void*>(GetLocalTime));
    ldr.registerExport("kernel32.dll", "GetSystemTimeAsFileTime", reinterpret_cast<void*>(GetSystemTimeAsFileTime));
    ldr.registerExport("kernel32.dll", "CreateFileMappingW", reinterpret_cast<void*>(CreateFileMappingW));
    ldr.registerExport("kernel32.dll", "MapViewOfFile", reinterpret_cast<void*>(MapViewOfFile));
    ldr.registerExport("kernel32.dll", "UnmapViewOfFile", reinterpret_cast<void*>(UnmapViewOfFile));
    ldr.registerExport("kernel32.dll", "GetExitCodeProcess", reinterpret_cast<void*>(GetExitCodeProcess));
    ldr.registerExport("kernel32.dll", "TerminateProcess", reinterpret_cast<void*>(TerminateProcess));
    ldr.registerExport("kernel32.dll", "SwitchToThread", reinterpret_cast<void*>(SwitchToThread));
    ldr.registerExport("kernel32.dll", "ResumeThread", reinterpret_cast<void*>(ResumeThread));
    ldr.registerExport("kernel32.dll", "SuspendThread", reinterpret_cast<void*>(SuspendThread));
    ldr.registerExport("kernel32.dll", "TerminateThread", reinterpret_cast<void*>(TerminateThread));
    ldr.registerExport("kernel32.dll", "GetThreadContext", reinterpret_cast<void*>(GetThreadContext));
    ldr.registerExport("kernel32.dll", "CreateThread", reinterpret_cast<void*>(CreateThread));
    ldr.registerExport("kernel32.dll", "OpenThread", reinterpret_cast<void*>(OpenThread));
    ldr.registerExport("kernel32.dll", "SetThreadAffinityMask", reinterpret_cast<void*>(SetThreadAffinityMask));
    ldr.registerExport("kernel32.dll", "GetVersion", reinterpret_cast<void*>(GetVersion));
    ldr.registerExport("kernel32.dll", "GetLargePageMinimum", reinterpret_cast<void*>(GetLargePageMinimum));
    ldr.registerExport("kernel32.dll", "SetFileApisToOEM", reinterpret_cast<void*>(SetFileApisToOEM));
    ldr.registerExport("kernel32.dll", "SetFileApisToANSI", reinterpret_cast<void*>(SetFileApisToANSI));
    ldr.registerExport("kernel32.dll", "CreateSemaphoreW", reinterpret_cast<void*>(CreateSemaphoreW));
    ldr.registerExport("kernel32.dll", "ReleaseSemaphore", reinterpret_cast<void*>(ReleaseSemaphore));
    ldr.registerExport("kernel32.dll", "DuplicateHandle", reinterpret_cast<void*>(DuplicateHandle));
    ldr.registerExport("kernel32.dll", "GetProcessId", reinterpret_cast<void*>(GetProcessId));
    ldr.registerExport("kernel32.dll", "GetStartupInfoA", reinterpret_cast<void*>(GetStartupInfoA));
    ldr.registerExport("kernel32.dll", "HeapSetInformation", reinterpret_cast<void*>(HeapSetInformation));
    ldr.registerExport("kernel32.dll", "IsDBCSLeadByteEx", reinterpret_cast<void*>(IsDBCSLeadByteEx));
    ldr.registerExport("kernel32.dll", "SetErrorMode", reinterpret_cast<void*>(SetErrorMode));
    ldr.registerExport("kernel32.dll", "VirtualQueryEx", reinterpret_cast<void*>(VirtualQueryEx));

    // CRT startup & interop helpers
    ldr.registerExport("kernel32.dll", "InitializeSListHead", reinterpret_cast<void*>(InitializeSListHead));
    ldr.registerExport("kernel32.dll", "FlsAlloc", reinterpret_cast<void*>(FlsAlloc));
    ldr.registerExport("kernel32.dll", "FlsGetValue", reinterpret_cast<void*>(FlsGetValue));
    ldr.registerExport("kernel32.dll", "FlsSetValue", reinterpret_cast<void*>(FlsSetValue));
    ldr.registerExport("kernel32.dll", "FlsFree", reinterpret_cast<void*>(FlsFree));
    ldr.registerExport("kernel32.dll", "InitializeCriticalSection", reinterpret_cast<void*>(InitializeCriticalSection));
    ldr.registerExport("kernel32.dll", "InitializeCriticalSectionEx", reinterpret_cast<void*>(InitializeCriticalSectionEx));
    ldr.registerExport("kernel32.dll", "EnterCriticalSection", reinterpret_cast<void*>(EnterCriticalSection));
    ldr.registerExport("kernel32.dll", "LeaveCriticalSection", reinterpret_cast<void*>(LeaveCriticalSection));
    ldr.registerExport("kernel32.dll", "DeleteCriticalSection", reinterpret_cast<void*>(DeleteCriticalSection));
    ldr.registerExport("kernel32.dll", "EncodePointer", reinterpret_cast<void*>(EncodePointer));
    ldr.registerExport("kernel32.dll", "DecodePointer", reinterpret_cast<void*>(DecodePointer));
    ldr.registerExport("kernel32.dll", "GetStartupInfoW", reinterpret_cast<void*>(GetStartupInfoW));
    ldr.registerExport("kernel32.dll", "GetFileType", reinterpret_cast<void*>(GetFileType));
    ldr.registerExport("kernel32.dll", "IsProcessorFeaturePresent", reinterpret_cast<void*>(IsProcessorFeaturePresent));
    ldr.registerExport("kernel32.dll", "IsDebuggerPresent", reinterpret_cast<void*>(IsDebuggerPresent));
    ldr.registerExport("kernel32.dll", "SetUnhandledExceptionFilter", reinterpret_cast<void*>(SetUnhandledExceptionFilter));
    ldr.registerExport("kernel32.dll", "UnhandledExceptionFilter", reinterpret_cast<void*>(UnhandledExceptionFilter));
    ldr.registerExport("kernel32.dll", "MultiByteToWideChar", reinterpret_cast<void*>(MultiByteToWideChar));
    ldr.registerExport("kernel32.dll", "WideCharToMultiByte", reinterpret_cast<void*>(WideCharToMultiByte));
    ldr.registerExport("kernel32.dll", "GetACP", reinterpret_cast<void*>(GetACP));
    ldr.registerExport("kernel32.dll", "GetOEMCP", reinterpret_cast<void*>(GetOEMCP));
    ldr.registerExport("kernel32.dll", "IsValidCodePage", reinterpret_cast<void*>(IsValidCodePage));
    ldr.registerExport("kernel32.dll", "GetCPInfo", reinterpret_cast<void*>(GetCPInfo));
    ldr.registerExport("kernel32.dll", "CompareStringW", reinterpret_cast<void*>(CompareStringW));
    ldr.registerExport("kernel32.dll", "LCMapStringW", reinterpret_cast<void*>(LCMapStringW));
    ldr.registerExport("kernel32.dll", "FindFirstFileExW", reinterpret_cast<void*>(FindFirstFileExW));
    ldr.registerExport("kernel32.dll", "FlushFileBuffers", reinterpret_cast<void*>(FlushFileBuffers));
    ldr.registerExport("kernel32.dll", "GetEnvironmentStringsW", reinterpret_cast<void*>(GetEnvironmentStringsW));
    ldr.registerExport("kernel32.dll", "FreeEnvironmentStringsW", reinterpret_cast<void*>(FreeEnvironmentStringsW));
    ldr.registerExport("kernel32.dll", "GetStringTypeW", reinterpret_cast<void*>(GetStringTypeW));
    ldr.registerExport("kernel32.dll", "VirtualProtect", reinterpret_cast<void*>(VirtualProtect));
    ldr.registerExport("kernel32.dll", "VirtualQuery", reinterpret_cast<void*>(VirtualQuery));
    ldr.registerExport("kernel32.dll", "TlsAlloc", reinterpret_cast<void*>(TlsAlloc));
    ldr.registerExport("kernel32.dll", "TlsGetValue", reinterpret_cast<void*>(TlsGetValue));
    ldr.registerExport("kernel32.dll", "TlsSetValue", reinterpret_cast<void*>(TlsSetValue));
    ldr.registerExport("kernel32.dll", "TlsFree", reinterpret_cast<void*>(TlsFree));
    ldr.registerExport("kernel32.dll", "RtlCaptureContext", reinterpret_cast<void*>(RtlCaptureContext));
    ldr.registerExport("kernel32.dll", "RtlLookupFunctionEntry", reinterpret_cast<void*>(RtlLookupFunctionEntry));
    ldr.registerExport("kernel32.dll", "RtlVirtualUnwind", reinterpret_cast<void*>(RtlVirtualUnwind));
    ldr.registerExport("kernel32.dll", "RtlUnwindEx", reinterpret_cast<void*>(RtlUnwindEx));
    ldr.registerExport("kernel32.dll", "RtlPcToFileHeader", reinterpret_cast<void*>(RtlPcToFileHeader));
    ldr.registerExport("kernel32.dll", "RaiseException", reinterpret_cast<void*>(RaiseException));
    ldr.registerExport("kernel32.dll", "LoadLibraryExW", reinterpret_cast<void*>(LoadLibraryExW));
    ldr.registerExport("kernel32.dll", "GetModuleHandleExW", reinterpret_cast<void*>(GetModuleHandleExW));
    ldr.registerExport("kernel32.dll", "GetConsoleOutputCP", reinterpret_cast<void*>(GetConsoleOutputCP));

    // Named Pipes & Mailslots IPC exports
    ldr.registerExport("kernel32.dll", "CreateNamedPipeW", reinterpret_cast<void*>(CreateNamedPipeW));
    ldr.registerExport("kernel32.dll", "ConnectNamedPipe", reinterpret_cast<void*>(ConnectNamedPipe));
    ldr.registerExport("kernel32.dll", "DisconnectNamedPipe", reinterpret_cast<void*>(DisconnectNamedPipe));
    ldr.registerExport("kernel32.dll", "WaitNamedPipeW", reinterpret_cast<void*>(WaitNamedPipeW));
    ldr.registerExport("kernel32.dll", "PeekNamedPipe", reinterpret_cast<void*>(PeekNamedPipe));
    ldr.registerExport("kernel32.dll", "TransactNamedPipe", reinterpret_cast<void*>(TransactNamedPipe));
    ldr.registerExport("kernel32.dll", "GetNamedPipeInfo", reinterpret_cast<void*>(GetNamedPipeInfo));
    ldr.registerExport("kernel32.dll", "GetNamedPipeHandleStateW", reinterpret_cast<void*>(GetNamedPipeHandleStateW));
    ldr.registerExport("kernel32.dll", "SetNamedPipeHandleState", reinterpret_cast<void*>(SetNamedPipeHandleState));
    ldr.registerExport("kernel32.dll", "CreateMailslotW", reinterpret_cast<void*>(CreateMailslotW));
    ldr.registerExport("kernel32.dll", "GetMailslotInfo", reinterpret_cast<void*>(GetMailslotInfo));
    ldr.registerExport("kernel32.dll", "SetMailslotInfo", reinterpret_cast<void*>(SetMailslotInfo));

    // Extended Win32 Subsystem exports (7-Zip, VLC, Notepad++)
    ldr.registerExport("kernel32.dll", "GetTickCount", reinterpret_cast<void*>(GetTickCount));
    ldr.registerExport("kernel32.dll", "SetConsoleCtrlHandler", reinterpret_cast<void*>(SetConsoleCtrlHandler));
    ldr.registerExport("kernel32.dll", "GetProcessTimes", reinterpret_cast<void*>(GetProcessTimes));
    ldr.registerExport("kernel32.dll", "SetProcessAffinityMask", reinterpret_cast<void*>(SetProcessAffinityMask));
    ldr.registerExport("kernel32.dll", "GetProcessAffinityMask", reinterpret_cast<void*>(GetProcessAffinityMask));
    ldr.registerExport("kernel32.dll", "OpenEventW", reinterpret_cast<void*>(OpenEventW));
    ldr.registerExport("kernel32.dll", "OpenFileMappingW", reinterpret_cast<void*>(OpenFileMappingW));
    ldr.registerExport("kernel32.dll", "CompareFileTime", reinterpret_cast<void*>(CompareFileTime));
    ldr.registerExport("kernel32.dll", "FileTimeToSystemTime", reinterpret_cast<void*>(FileTimeToSystemTime));
    ldr.registerExport("kernel32.dll", "FileTimeToLocalFileTime", reinterpret_cast<void*>(FileTimeToLocalFileTime));
    ldr.registerExport("kernel32.dll", "FileTimeToDosDateTime", reinterpret_cast<void*>(FileTimeToDosDateTime));
    ldr.registerExport("kernel32.dll", "GlobalMemoryStatusEx", reinterpret_cast<void*>(GlobalMemoryStatusEx));
    ldr.registerExport("kernel32.dll", "GetDiskFreeSpaceW", reinterpret_cast<void*>(GetDiskFreeSpaceW));
    ldr.registerExport("kernel32.dll", "GetDiskFreeSpaceExW", reinterpret_cast<void*>(GetDiskFreeSpaceExW));
    ldr.registerExport("kernel32.dll", "SetEndOfFile", reinterpret_cast<void*>(SetEndOfFile));
    ldr.registerExport("kernel32.dll", "FormatMessageW", reinterpret_cast<void*>(FormatMessageW));
    ldr.registerExport("kernel32.dll", "SetFileTime", reinterpret_cast<void*>(SetFileTime));
    ldr.registerExport("kernel32.dll", "MoveFileWithProgressW", reinterpret_cast<void*>(MoveFileWithProgressW));
    ldr.registerExport("kernel32.dll", "CreateHardLinkW", reinterpret_cast<void*>(CreateHardLinkW));
    ldr.registerExport("kernel32.dll", "GetTempPathW", reinterpret_cast<void*>(GetTempPathW));
    ldr.registerExport("kernel32.dll", "GetFileInformationByHandle", reinterpret_cast<void*>(GetFileInformationByHandle));
    ldr.registerExport("kernel32.dll", "FindFirstStreamW", reinterpret_cast<void*>(FindFirstStreamW));
    ldr.registerExport("kernel32.dll", "FindNextStreamW", reinterpret_cast<void*>(FindNextStreamW));
    ldr.registerExport("kernel32.dll", "GetLogicalDriveStringsW", reinterpret_cast<void*>(GetLogicalDriveStringsW));
    ldr.registerExport("kernel32.dll", "DeviceIoControl", reinterpret_cast<void*>(DeviceIoControl));

    // Global memory and string utilities (Notepad++)
    ldr.registerExport("kernel32.dll", "GlobalAlloc", reinterpret_cast<void*>(GlobalAlloc));
    ldr.registerExport("kernel32.dll", "GlobalFree", reinterpret_cast<void*>(GlobalFree));
    ldr.registerExport("kernel32.dll", "GlobalLock", reinterpret_cast<void*>(GlobalLock));
    ldr.registerExport("kernel32.dll", "GlobalUnlock", reinterpret_cast<void*>(GlobalUnlock));
    ldr.registerExport("kernel32.dll", "lstrcmpiW", reinterpret_cast<void*>(lstrcmpiW));
    ldr.registerExport("kernel32.dll", "lstrcmpiA", reinterpret_cast<void*>(lstrcmpiA));
    ldr.registerExport("kernel32.dll", "lstrcpynW", reinterpret_cast<void*>(lstrcpynW));
    ldr.registerExport("kernel32.dll", "ExpandEnvironmentStringsW", reinterpret_cast<void*>(ExpandEnvironmentStringsW));
    ldr.registerExport("kernel32.dll", "GetDateFormatW", reinterpret_cast<void*>(GetDateFormatW));
    ldr.registerExport("kernel32.dll", "GetDateFormatEx", reinterpret_cast<void*>(GetDateFormatEx));
    ldr.registerExport("kernel32.dll", "GetTimeFormatEx", reinterpret_cast<void*>(GetTimeFormatEx));
    ldr.registerExport("kernel32.dll", "GetProductInfo", reinterpret_cast<void*>(GetProductInfo));
    ldr.registerExport("kernel32.dll", "GetVersionExW", reinterpret_cast<void*>(GetVersionExW));
    ldr.registerExport("kernel32.dll", "FindResourceW", reinterpret_cast<void*>(FindResourceW));
    ldr.registerExport("kernel32.dll", "LoadResource", reinterpret_cast<void*>(LoadResource));
    ldr.registerExport("kernel32.dll", "LockResource", reinterpret_cast<void*>(LockResource));
    ldr.registerExport("kernel32.dll", "SizeofResource", reinterpret_cast<void*>(SizeofResource));
    ldr.registerExport("kernel32.dll", "GetLongPathNameW", reinterpret_cast<void*>(GetLongPathNameW));
    ldr.registerExport("kernel32.dll", "GetFinalPathNameByHandleW", reinterpret_cast<void*>(GetFinalPathNameByHandleW));
    ldr.registerExport("kernel32.dll", "ReleaseMutex", reinterpret_cast<void*>(ReleaseMutex));
    ldr.registerExport("kernel32.dll", "OpenProcess", reinterpret_cast<void*>(OpenProcess));
    ldr.registerExport("kernel32.dll", "GetApplicationRestartSettings", reinterpret_cast<void*>(GetApplicationRestartSettings));
    ldr.registerExport("kernel32.dll", "UnregisterApplicationRestart", reinterpret_cast<void*>(UnregisterApplicationRestart));
    ldr.registerExport("kernel32.dll", "ExitThread", reinterpret_cast<void*>(ExitThread));
    ldr.registerExport("kernel32.dll", "RegisterApplicationRestart", reinterpret_cast<void*>(RegisterApplicationRestart));
    ldr.registerExport("kernel32.dll", "AcquireSRWLockExclusive", reinterpret_cast<void*>(AcquireSRWLockExclusive));
    ldr.registerExport("kernel32.dll", "ReleaseSRWLockExclusive", reinterpret_cast<void*>(ReleaseSRWLockExclusive));
    ldr.registerExport("kernel32.dll", "TryAcquireSRWLockExclusive", reinterpret_cast<void*>(TryAcquireSRWLockExclusive));
    ldr.registerExport("kernel32.dll", "AcquireSRWLockShared", reinterpret_cast<void*>(AcquireSRWLockShared));
    ldr.registerExport("kernel32.dll", "ReleaseSRWLockShared", reinterpret_cast<void*>(ReleaseSRWLockShared));
    ldr.registerExport("kernel32.dll", "TryAcquireSRWLockShared", reinterpret_cast<void*>(TryAcquireSRWLockShared));
    ldr.registerExport("kernel32.dll", "InitializeSRWLock", reinterpret_cast<void*>(InitializeSRWLock));
    ldr.registerExport("kernel32.dll", "SleepConditionVariableSRW", reinterpret_cast<void*>(SleepConditionVariableSRW));
    ldr.registerExport("kernel32.dll", "WakeConditionVariable", reinterpret_cast<void*>(WakeConditionVariable));
    ldr.registerExport("kernel32.dll", "WakeAllConditionVariable", reinterpret_cast<void*>(WakeAllConditionVariable));
    ldr.registerExport("kernel32.dll", "InitializeConditionVariable", reinterpret_cast<void*>(InitializeConditionVariable));
    ldr.registerExport("kernel32.dll", "InitOnceBeginInitialize", reinterpret_cast<void*>(InitOnceBeginInitialize));
    ldr.registerExport("kernel32.dll", "InitOnceComplete", reinterpret_cast<void*>(InitOnceComplete));
    ldr.registerExport("kernel32.dll", "InitOnceExecuteOnce", reinterpret_cast<void*>(InitOnceExecuteOnce));
    ldr.registerExport("kernel32.dll", "CreateThreadpoolWork", reinterpret_cast<void*>(CreateThreadpoolWork));
    ldr.registerExport("kernel32.dll", "SubmitThreadpoolWork", reinterpret_cast<void*>(SubmitThreadpoolWork));
    ldr.registerExport("kernel32.dll", "CloseThreadpoolWork", reinterpret_cast<void*>(CloseThreadpoolWork));
    ldr.registerExport("kernel32.dll", "FreeLibraryWhenCallbackReturns", reinterpret_cast<void*>(FreeLibraryWhenCallbackReturns));
    ldr.registerExport("kernel32.dll", "MulDiv", reinterpret_cast<void*>(MulDiv));
    ldr.registerExport("kernel32.dll", "CompareStringOrdinal", reinterpret_cast<void*>(CompareStringOrdinal));
    ldr.registerExport("kernel32.dll", "CompareStringEx", reinterpret_cast<void*>(CompareStringEx));
    ldr.registerExport("kernel32.dll", "LCMapStringEx", reinterpret_cast<void*>(LCMapStringEx));
    ldr.registerExport("kernel32.dll", "lstrcmpW", reinterpret_cast<void*>(lstrcmpW));
    ldr.registerExport("kernel32.dll", "lstrcpyW", reinterpret_cast<void*>(lstrcpyW));
    ldr.registerExport("kernel32.dll", "lstrcpynA", reinterpret_cast<void*>(lstrcpynA));
    ldr.registerExport("kernel32.dll", "lstrlenW", reinterpret_cast<void*>(lstrlenW));
    ldr.registerExport("kernel32.dll", "GetNativeSystemInfo", reinterpret_cast<void*>(GetNativeSystemInfo));
    ldr.registerExport("kernel32.dll", "QueryFullProcessImageNameW", reinterpret_cast<void*>(QueryFullProcessImageNameW));
    ldr.registerExport("kernel32.dll", "GlobalSize", reinterpret_cast<void*>(GlobalSize));
    ldr.registerExport("kernel32.dll", "CopyFileExW", reinterpret_cast<void*>(CopyFileExW));
    ldr.registerExport("kernel32.dll", "ReplaceFileW", reinterpret_cast<void*>(ReplaceFileW));
    ldr.registerExport("kernel32.dll", "GetFileAttributesExW", reinterpret_cast<void*>(GetFileAttributesExW));
    ldr.registerExport("kernel32.dll", "GetFileInformationByHandleEx", reinterpret_cast<void*>(GetFileInformationByHandleEx));
    ldr.registerExport("kernel32.dll", "AreFileApisANSI", reinterpret_cast<void*>(AreFileApisANSI));
    ldr.registerExport("kernel32.dll", "CreateToolhelp32Snapshot", reinterpret_cast<void*>(CreateToolhelp32Snapshot));
    ldr.registerExport("kernel32.dll", "Process32FirstW", reinterpret_cast<void*>(Process32FirstW));
    ldr.registerExport("kernel32.dll", "Process32NextW", reinterpret_cast<void*>(Process32NextW));
    ldr.registerExport("kernel32.dll", "ReadDirectoryChangesW", reinterpret_cast<void*>(ReadDirectoryChangesW));
    ldr.registerExport("kernel32.dll", "SleepEx", reinterpret_cast<void*>(SleepEx));
    ldr.registerExport("kernel32.dll", "WaitForSingleObjectEx", reinterpret_cast<void*>(WaitForSingleObjectEx));
    ldr.registerExport("kernel32.dll", "QueueUserAPC", reinterpret_cast<void*>(QueueUserAPC));
    ldr.registerExport("kernel32.dll", "CancelIo", reinterpret_cast<void*>(CancelIo));
    ldr.registerExport("kernel32.dll", "GetTimeZoneInformation", reinterpret_cast<void*>(GetTimeZoneInformation));
    ldr.registerExport("kernel32.dll", "SystemTimeToTzSpecificLocalTime", reinterpret_cast<void*>(SystemTimeToTzSpecificLocalTime));
    ldr.registerExport("kernel32.dll", "GetTimeFormatW", reinterpret_cast<void*>(GetTimeFormatW));
    ldr.registerExport("kernel32.dll", "GetLocaleInfoW", reinterpret_cast<void*>(GetLocaleInfoW));
    ldr.registerExport("kernel32.dll", "GetLocaleInfoA", reinterpret_cast<void*>(GetLocaleInfoA));
    ldr.registerExport("kernel32.dll", "GetLocaleInfoEx", reinterpret_cast<void*>(GetLocaleInfoEx));
    ldr.registerExport("kernel32.dll", "GetUserDefaultLCID", reinterpret_cast<void*>(GetUserDefaultLCID));
    ldr.registerExport("kernel32.dll", "GetStringTypeExW", reinterpret_cast<void*>(GetStringTypeExW));
    ldr.registerExport("kernel32.dll", "GetStringTypeExA", reinterpret_cast<void*>(GetStringTypeExA));
    ldr.registerExport("kernel32.dll", "LCMapStringA", reinterpret_cast<void*>(LCMapStringA));
    ldr.registerExport("kernel32.dll", "IsValidLocale", reinterpret_cast<void*>(IsValidLocale));
    ldr.registerExport("kernel32.dll", "EnumSystemLocalesW", reinterpret_cast<void*>(EnumSystemLocalesW));
    ldr.registerExport("kernel32.dll", "CreateMutexW", reinterpret_cast<void*>(CreateMutexW));
    ldr.registerExport("kernel32.dll", "GetTempPath2W", reinterpret_cast<void*>(GetTempPath2W));
    ldr.registerExport("kernel32.dll", "GetTempPath2A", reinterpret_cast<void*>(GetTempPath2A));
    ldr.registerExport("kernel32.dll", "LoadLibraryA", reinterpret_cast<void*>(LoadLibraryA));
    ldr.registerExport("kernel32.dll", "LoadLibraryExA", reinterpret_cast<void*>(LoadLibraryExA));
    ldr.registerExport("kernel32.dll", "FormatMessageA", reinterpret_cast<void*>(FormatMessageA));
    ldr.registerExport("kernel32.dll", "InitializeCriticalSectionAndSpinCount", reinterpret_cast<void*>(InitializeCriticalSectionAndSpinCount));
    ldr.registerExport("kernel32.dll", "ReadConsoleW", reinterpret_cast<void*>(ReadConsoleW));
    ldr.registerExport("kernel32.dll", "RtlUnwind", reinterpret_cast<void*>(RtlUnwind));
    ldr.registerExport("kernel32.dll", "FreeLibraryAndExitThread", reinterpret_cast<void*>(FreeLibraryAndExitThread));
    ldr.registerExport("kernel32.dll", "DisableThreadLibraryCalls", reinterpret_cast<void*>(DisableThreadLibraryCalls));
    ldr.registerExport("kernel32.dll", "SetProcessDEPPolicy", reinterpret_cast<void*>(SetProcessDEPPolicy));
    ldr.registerExport("kernel32.dll", "SetDllDirectoryA", reinterpret_cast<void*>(SetDllDirectoryA));
    ldr.registerExport("api-ms-win-core-kernel32-legacy-ansi-l1-1-0.dll", "SetDllDirectoryA", reinterpret_cast<void*>(SetDllDirectoryA));
    ldr.registerExport("kernel32.dll", "GetSystemPowerStatus", reinterpret_cast<void*>(GetSystemPowerStatus));
    ldr.registerExport("kernel32.dll", "GetActiveProcessorCount", reinterpret_cast<void*>(GetActiveProcessorCount));
    ldr.registerExport("kernel32.dll", "GetHandleInformation", reinterpret_cast<void*>(GetHandleInformation));
    ldr.registerExport("api-ms-win-core-handle-l1-1-0.dll", "GetHandleInformation", reinterpret_cast<void*>(GetHandleInformation));
    ldr.registerExport("kernel32.dll", "GetProfileStringW", reinterpret_cast<void*>(GetProfileStringW));
    ldr.registerExport("kernel32.dll", "IdnToAscii", reinterpret_cast<void*>(IdnToAscii));
    ldr.registerExport("api-ms-win-core-normalization-l1-1-0.dll", "IdnToAscii", reinterpret_cast<void*>(IdnToAscii));
    ldr.registerExport("kernel32.dll", "IsBadStringPtrA", reinterpret_cast<void*>(IsBadStringPtrA));
    ldr.registerExport("kernel32.dll", "SetProcessPriorityBoost", reinterpret_cast<void*>(SetProcessPriorityBoost));
    ldr.registerExport("kernel32.dll", "SetThreadContext", reinterpret_cast<void*>(SetThreadContext));
    ldr.registerExport("kernel32.dll", "FindFirstVolumeW", reinterpret_cast<void*>(FindFirstVolumeW));
    ldr.registerExport("kernel32.dll", "FindNextVolumeW", reinterpret_cast<void*>(FindNextVolumeW));
    ldr.registerExport("kernel32.dll", "FindVolumeClose", reinterpret_cast<void*>(FindVolumeClose));
    ldr.registerExport("kernel32.dll", "GetFinalPathNameByHandleA", reinterpret_cast<void*>(GetFinalPathNameByHandleA));
    ldr.registerExport("kernel32.dll", "FillConsoleOutputCharacterW", reinterpret_cast<void*>(FillConsoleOutputCharacterW));
    ldr.registerExport("kernel32.dll", "ReadConsoleOutputCharacterA", reinterpret_cast<void*>(ReadConsoleOutputCharacterA));
    ldr.registerExport("kernel32.dll", "CreateJobObjectA", reinterpret_cast<void*>(CreateJobObjectA));
    ldr.registerExport("kernel32.dll", "CreateWaitableTimerExW", reinterpret_cast<void*>(CreateWaitableTimerExW));
    ldr.registerExport("api-ms-win-core-synch-l1-1-0.dll", "CreateWaitableTimerExW", reinterpret_cast<void*>(CreateWaitableTimerExW));
    ldr.registerExport("kernel32.dll", "GetComputerNameA", reinterpret_cast<void*>(GetComputerNameA));
    ldr.registerExport("kernel32.dll", "GetFileAttributesExA", reinterpret_cast<void*>(GetFileAttributesExA));
    ldr.registerExport("kernel32.dll", "K32EnumProcessModules", reinterpret_cast<void*>(K32EnumProcessModules));
    ldr.registerExport("kernel32.dll", "K32GetMappedFileNameW", reinterpret_cast<void*>(K32GetMappedFileNameW));
    ldr.registerExport("kernel32.dll", "K32GetModuleFileNameExA", reinterpret_cast<void*>(K32GetModuleFileNameExA));
    ldr.registerExport("kernel32.dll", "K32GetProcessMemoryInfo", reinterpret_cast<void*>(K32GetProcessMemoryInfo));
    ldr.registerExport("kernel32.dll", "Module32First", reinterpret_cast<void*>(Module32First));
    ldr.registerExport("kernel32.dll", "Module32Next", reinterpret_cast<void*>(Module32Next));
    ldr.registerExport("kernel32.dll", "Process32First", reinterpret_cast<void*>(Process32First));
    ldr.registerExport("kernel32.dll", "Process32Next", reinterpret_cast<void*>(Process32Next));
    ldr.registerExport("kernel32.dll", "SetProcessInformation", reinterpret_cast<void*>(SetProcessInformation));
    ldr.registerExport("kernel32.dll", "SetProcessShutdownParameters", reinterpret_cast<void*>(SetProcessShutdownParameters));
    ldr.registerExport("kernel32.dll", "UnmapViewOfFileEx", reinterpret_cast<void*>(UnmapViewOfFileEx));
    ldr.registerExport("kernel32.dll", "RemoveVectoredExceptionHandler", reinterpret_cast<void*>(RemoveVectoredExceptionHandler));
    ldr.registerExport("api-ms-win-core-errorhandling-l1-1-1.dll", "RemoveVectoredExceptionHandler", reinterpret_cast<void*>(RemoveVectoredExceptionHandler));
    ldr.registerExport("kernel32.dll", "GetWindowsDirectoryW", reinterpret_cast<void*>(GetWindowsDirectoryW));
    ldr.registerExport("kernel32.dll", "GetDriveTypeW", reinterpret_cast<void*>(GetDriveTypeW));
    ldr.registerExport("kernel32.dll", "GetVolumeInformationW", reinterpret_cast<void*>(GetVolumeInformationW));
    ldr.registerExport("kernel32.dll", "SetPriorityClass", reinterpret_cast<void*>(SetPriorityClass));
    ldr.registerExport("kernel32.dll", "GetSystemDefaultLangID", reinterpret_cast<void*>(GetSystemDefaultLangID));
    ldr.registerExport("kernel32.dll", "GetUserDefaultLangID", reinterpret_cast<void*>(GetUserDefaultLangID));
    ldr.registerExport("kernel32.dll", "GetCompressedFileSizeW", reinterpret_cast<void*>(GetCompressedFileSizeW));
    ldr.registerExport("kernel32.dll", "FindFirstChangeNotificationW", reinterpret_cast<void*>(FindFirstChangeNotificationW));
    ldr.registerExport("kernel32.dll", "FindNextChangeNotification", reinterpret_cast<void*>(FindNextChangeNotification));
    ldr.registerExport("kernel32.dll", "FindCloseChangeNotification", reinterpret_cast<void*>(FindCloseChangeNotification));
    ldr.registerExport("kernel32.dll", "__C_specific_handler", reinterpret_cast<void*>(__C_specific_handler));
    ldr.registerExport("kernel32.dll", "GetVersionExA", reinterpret_cast<void*>(GetVersionExA));
    ldr.registerExport("kernel32.dll", "GetNumberFormatW", reinterpret_cast<void*>(GetNumberFormatW));
    ldr.registerExport("kernel32.dll", "GetCalendarInfoW", reinterpret_cast<void*>(GetCalendarInfoW));
    ldr.registerExport("kernel32.dll", "FreeEnvironmentStringsA", reinterpret_cast<void*>(FreeEnvironmentStringsA));
    ldr.registerExport("kernel32.dll", "GetEnvironmentStrings", reinterpret_cast<void*>(GetEnvironmentStrings));
    ldr.registerExport("kernel32.dll", "SetHandleCount", reinterpret_cast<void*>(SetHandleCount));
    ldr.registerExport("kernel32.dll", "GetStringTypeA", reinterpret_cast<void*>(GetStringTypeA));
    ldr.registerExport("kernel32.dll", "CreateJobObjectW", reinterpret_cast<void*>(CreateJobObjectW));
    ldr.registerExport("kernel32.dll", "OpenJobObjectW", reinterpret_cast<void*>(OpenJobObjectW));
    ldr.registerExport("kernel32.dll", "AssignProcessToJobObject", reinterpret_cast<void*>(AssignProcessToJobObject));
    ldr.registerExport("kernel32.dll", "SetInformationJobObject", reinterpret_cast<void*>(SetInformationJobObject));
    ldr.registerExport("kernel32.dll", "OpenFileMappingA", reinterpret_cast<void*>(OpenFileMappingA));
    ldr.registerExport("kernel32.dll", "VirtualLock", reinterpret_cast<void*>(VirtualLock));
    ldr.registerExport("kernel32.dll", "FlushInstructionCache", reinterpret_cast<void*>(FlushInstructionCache));
    ldr.registerExport("kernel32.dll", "FlushConsoleInputBuffer", reinterpret_cast<void*>(FlushConsoleInputBuffer));
    ldr.registerExport("kernel32.dll", "PeekConsoleInputW", reinterpret_cast<void*>(PeekConsoleInputW));
    ldr.registerExport("kernel32.dll", "ReadConsoleInputW", reinterpret_cast<void*>(ReadConsoleInputW));
    ldr.registerExport("kernel32.dll", "WriteConsoleInputW", reinterpret_cast<void*>(WriteConsoleInputW));
    ldr.registerExport("kernel32.dll", "InterlockedIncrement", reinterpret_cast<void*>(InterlockedIncrement));
    ldr.registerExport("kernel32.dll", "InterlockedDecrement", reinterpret_cast<void*>(InterlockedDecrement));
    ldr.registerExport("kernel32.dll", "InterlockedExchange", reinterpret_cast<void*>(InterlockedExchange));
    ldr.registerExport("kernel32.dll", "InterlockedExchangeAdd", reinterpret_cast<void*>(InterlockedExchangeAdd));
    ldr.registerExport("kernel32.dll", "InterlockedCompareExchange", reinterpret_cast<void*>(InterlockedCompareExchange));
    ldr.registerExport("kernel32.dll", "GetDateFormatA", reinterpret_cast<void*>(GetDateFormatA));
    ldr.registerExport("kernel32.dll", "GetModuleHandleExA", reinterpret_cast<void*>(GetModuleHandleExA));
    ldr.registerExport("kernel32.dll", "IsBadWritePtr", reinterpret_cast<void*>(IsBadWritePtr));
    ldr.registerExport("kernel32.dll", "GetThreadGroupAffinity", reinterpret_cast<void*>(GetThreadGroupAffinity));
    ldr.registerExport("kernel32.dll", "DosDateTimeToFileTime", reinterpret_cast<void*>(DosDateTimeToFileTime));
    ldr.registerExport("kernel32.dll", "SystemTimeToFileTime", reinterpret_cast<void*>(SystemTimeToFileTime));
    ldr.registerExport("kernel32.dll", "TzSpecificLocalTimeToSystemTime", reinterpret_cast<void*>(TzSpecificLocalTimeToSystemTime));
    ldr.registerExport("kernel32.dll", "GetFileTime", reinterpret_cast<void*>(GetFileTime));
    ldr.registerExport("kernel32.dll", "GetLogicalDrives", reinterpret_cast<void*>(GetLogicalDrives));
    ldr.registerExport("kernel32.dll", "GetVolumePathNameW", reinterpret_cast<void*>(GetVolumePathNameW));
    ldr.registerExport("kernel32.dll", "FoldStringW", reinterpret_cast<void*>(FoldStringW));
    ldr.registerExport("kernel32.dll", "IsDBCSLeadByte", reinterpret_cast<void*>(IsDBCSLeadByte));
    ldr.registerExport("kernel32.dll", "Thread32First", reinterpret_cast<void*>(Thread32First));
    ldr.registerExport("kernel32.dll", "Thread32Next", reinterpret_cast<void*>(Thread32Next));
    ldr.registerExport("kernel32.dll", "Module32FirstW", reinterpret_cast<void*>(Module32FirstW));
    ldr.registerExport("kernel32.dll", "Module32NextW", reinterpret_cast<void*>(Module32NextW));
    ldr.registerExport("kernel32.dll", "AttachConsole", reinterpret_cast<void*>(AttachConsole));
    ldr.registerExport("kernel32.dll", "SetConsoleScreenBufferSize", reinterpret_cast<void*>(SetConsoleScreenBufferSize));
    ldr.registerExport("kernel32.dll", "GetTempFileNameW", reinterpret_cast<void*>(GetTempFileNameW));
    ldr.registerExport("kernel32.dll", "OutputDebugStringA", reinterpret_cast<void*>(OutputDebugStringA));
    ldr.registerExport("kernel32.dll", "SetThreadExecutionState", reinterpret_cast<void*>(SetThreadExecutionState));
    ldr.registerExport("kernel32.dll", "DebugBreak", reinterpret_cast<void*>(DebugBreak));
    ldr.registerExport("kernel32.dll", "AddVectoredExceptionHandler", reinterpret_cast<void*>(AddVectoredExceptionHandler));
    ldr.registerExport("kernel32.dll", "GetPrivateProfileIntW", reinterpret_cast<void*>(GetPrivateProfileIntW));
    ldr.registerExport("kernel32.dll", "HeapQueryInformation", reinterpret_cast<void*>(HeapQueryInformation));
    ldr.registerExport("kernel32.dll", "Beep", reinterpret_cast<void*>(Beep));
    ldr.registerExport("kernel32.dll", "ClearCommBreak", reinterpret_cast<void*>(ClearCommBreak));
    ldr.registerExport("kernel32.dll", "SetCommBreak", reinterpret_cast<void*>(SetCommBreak));
    ldr.registerExport("kernel32.dll", "GetCommState", reinterpret_cast<void*>(GetCommState));
    ldr.registerExport("kernel32.dll", "SetCommState", reinterpret_cast<void*>(SetCommState));
    ldr.registerExport("kernel32.dll", "SetCommTimeouts", reinterpret_cast<void*>(SetCommTimeouts));
    ldr.registerExport("kernel32.dll", "SetHandleInformation", reinterpret_cast<void*>(SetHandleInformation));
    ldr.registerExport("kernel32.dll", "CreateEventA", reinterpret_cast<void*>(CreateEventA));
    ldr.registerExport("kernel32.dll", "CreateMutexA", reinterpret_cast<void*>(CreateMutexA));
    ldr.registerExport("kernel32.dll", "CreateFileMappingA", reinterpret_cast<void*>(CreateFileMappingA));
    ldr.registerExport("kernel32.dll", "CreateNamedPipeA", reinterpret_cast<void*>(CreateNamedPipeA));
    ldr.registerExport("kernel32.dll", "WaitNamedPipeA", reinterpret_cast<void*>(WaitNamedPipeA));
    ldr.registerExport("kernel32.dll", "CreatePipe", reinterpret_cast<void*>(CreatePipe));
    ldr.registerExport("kernel32.dll", "FindResourceA", reinterpret_cast<void*>(FindResourceA));
    ldr.registerExport("kernel32.dll", "GetOverlappedResult", reinterpret_cast<void*>(GetOverlappedResult));
    ldr.registerExport("kernel32.dll", "GetSystemDirectoryA", reinterpret_cast<void*>(GetSystemDirectoryA));
    ldr.registerExport("kernel32.dll", "GetWindowsDirectoryA", reinterpret_cast<void*>(GetWindowsDirectoryA));
    ldr.registerExport("kernel32.dll", "GetTempPathA", reinterpret_cast<void*>(GetTempPathA));
    ldr.registerExport("kernel32.dll", "GetThreadTimes", reinterpret_cast<void*>(GetThreadTimes));
    ldr.registerExport("kernel32.dll", "GlobalMemoryStatus", reinterpret_cast<void*>(GlobalMemoryStatus));
    ldr.registerExport("kernel32.dll", "LocalFileTimeToFileTime", reinterpret_cast<void*>(LocalFileTimeToFileTime));

    // ntdll.dll exports
    ldr.registerExport("ntdll.dll", "RtlAllocateHeap", reinterpret_cast<void*>(ntdll::RtlAllocateHeap));
    ldr.registerExport("ntdll.dll", "RtlFreeHeap", reinterpret_cast<void*>(ntdll::RtlFreeHeap));
    ldr.registerExport("ntdll.dll", "RtlCreateHeap", reinterpret_cast<void*>(ntdll::RtlCreateHeap));
    ldr.registerExport("ntdll.dll", "RtlDestroyHeap", reinterpret_cast<void*>(ntdll::RtlDestroyHeap));
    ldr.registerExport("ntdll.dll", "RtlSizeHeap", reinterpret_cast<void*>(ntdll::RtlSizeHeap));
    ldr.registerExport("ntdll.dll", "RtlSetLastWin32Error", reinterpret_cast<void*>(ntdll::RtlSetLastWin32Error));
    ldr.registerExport("ntdll.dll", "RtlGetLastWin32Error", reinterpret_cast<void*>(ntdll::RtlGetLastWin32Error));
    ldr.registerExport("ntdll.dll", "RtlNtStatusToDosError", reinterpret_cast<void*>(ntdll::RtlNtStatusToDosError));
    ldr.registerExport("ntdll.dll", "NtAllocateVirtualMemory", reinterpret_cast<void*>(ntdll::NtAllocateVirtualMemory));
    ldr.registerExport("ntdll.dll", "NtFreeVirtualMemory", reinterpret_cast<void*>(ntdll::NtFreeVirtualMemory));
    ldr.registerExport("ntdll.dll", "NtWriteFile", reinterpret_cast<void*>(ntdll::NtWriteFile));
    ldr.registerExport("ntdll.dll", "NtReadFile", reinterpret_cast<void*>(ntdll::NtReadFile));
    ldr.registerExport("ntdll.dll", "NtClose", reinterpret_cast<void*>(ntdll::NtClose));
    ldr.registerExport("ntdll.dll", "NtWaitForSingleObject", reinterpret_cast<void*>(ntdll::NtWaitForSingleObject));
    ldr.registerExport("ntdll.dll", "NtCreateSection", reinterpret_cast<void*>(ntdll::NtCreateSection));
    ldr.registerExport("ntdll.dll", "NtMapViewOfSection", reinterpret_cast<void*>(ntdll::NtMapViewOfSection));
    ldr.registerExport("ntdll.dll", "NtUnmapViewOfSection", reinterpret_cast<void*>(ntdll::NtUnmapViewOfSection));
    ldr.registerExport("ntdll.dll", "NtQueryInformationFile", reinterpret_cast<void*>(ntdll::NtQueryInformationFile));
    ldr.registerExport("ntdll.dll", "NtSetInformationFile", reinterpret_cast<void*>(ntdll::NtSetInformationFile));
    ldr.registerExport("ntdll.dll", "NtQueryDirectoryFile", reinterpret_cast<void*>(ntdll::NtQueryDirectoryFile));
    ldr.registerExport("ntdll.dll", "NtQueryPerformanceCounter", reinterpret_cast<void*>(ntdll::NtQueryPerformanceCounter));
    ldr.registerExport("ntdll.dll", "NtYieldExecution", reinterpret_cast<void*>(ntdll::NtYieldExecution));
    ldr.registerExport("ntdll.dll", "NtQueryInformationProcess", reinterpret_cast<void*>(ntdll::NtQueryInformationProcess));
    ldr.registerExport("ntdll.dll", "NtCreateNamedPipeFile", reinterpret_cast<void*>(ntdll::NtCreateNamedPipeFile));
    ldr.registerExport("ntdll.dll", "NtCreateMailslotFile", reinterpret_cast<void*>(ntdll::NtCreateMailslotFile));
}


} // namespace micant::win32

namespace micant {
namespace kernel32 = win32;
}
