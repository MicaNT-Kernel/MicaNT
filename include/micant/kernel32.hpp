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
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ntdll.hpp"
#include "ldr.hpp"
#include "csrss.hpp"
#include "conhost.hpp"
#include "hal.hpp"

namespace micant::win32 {

// ============================================================================
// 1. Standard Win32 Types & Constants
// ============================================================================

using DWORD   = uint32_t;
using BOOL    = int32_t;
using HANDLE  = void*;
using HMODULE = void*;
using LPVOID  = void*;
using LPCVOID = const void*;
using LPCWSTR = const wchar_t*;
using LPWSTR  = wchar_t*;
using LPCSTR  = const char*;
using LPSTR   = char*;
using SIZE_T  = size_t;

inline constexpr BOOL TRUE  = 1;
inline constexpr BOOL FALSE = 0;
inline const HANDLE INVALID_HANDLE_VALUE = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));

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
        s_defaultProcessHeap = ntdll::RtlCreateHeap(0, nullptr, 0x100000, 0x10000, nullptr, nullptr);
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

/**
 * @brief Ends the calling process and all its threads.
 */
[[noreturn]] inline void ExitProcess(DWORD uExitCode) noexcept {
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

    DWORD pid = GetCurrentProcessId();
    auto session = conhost::ConhostManager::get().getConsole(pid);
    if (session) {
        std::wstring_view sv(reinterpret_cast<const wchar_t*>(lpBuffer), nNumberOfCharsToWrite);
        session->writeOutput(sv);
        if (lpNumberOfCharsWritten) *lpNumberOfCharsWritten = nNumberOfCharsToWrite;
        return TRUE;
    }

    // Direct NT write fallback for standard console handle
    IoStatusBlock iosb{};
    NtStatus status = ntdll::NtWriteFile(
        reinterpret_cast<Handle>(hConsoleOutput),
        0, nullptr, nullptr,
        &iosb,
        const_cast<void*>(lpBuffer),
        nNumberOfCharsToWrite * sizeof(wchar_t),
        nullptr, nullptr
    );

    if (lpNumberOfCharsWritten) {
        *lpNumberOfCharsWritten = static_cast<DWORD>(iosb.information / sizeof(wchar_t));
    }
    return NT_SUCCESS(status) ? TRUE : FALSE;
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
    return NT_SUCCESS(status) ? TRUE : FALSE;
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
    return NT_SUCCESS(status) ? TRUE : FALSE;
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
// 6. Synchronization (Events, Single/Multiple Waits)
// ============================================================================

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
    uintptr_t modBase = 0;
    UnicodeString uniPath(lpLibFileName);
    NtStatus status = ntdll::LdrLoadDll(nullptr, 0, &uniPath, &modBase);
    if (!NT_SUCCESS(status)) return nullptr;
    return reinterpret_cast<HMODULE>(modBase);
}

/**
 * @brief Retrieves the address of an exported function or variable from the specified DLL.
 */
inline void* GetProcAddress(HMODULE hModule, LPCSTR lpProcName) noexcept {
    if (!hModule || !lpProcName) return nullptr;
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
// 9. Error Handling & Thread-Local Status
// ============================================================================

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
static const wchar_t* g_CommandLineW = L"micant.exe";
static const char* g_CommandLineA = "micant.exe";

inline LPCSTR GetCommandLineA() noexcept { return g_CommandLineA; }
inline LPCWSTR GetCommandLineW() noexcept { return g_CommandLineW; }

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
    if (!lpModuleName) return reinterpret_cast<HMODULE>(0x140000000ULL); // Main exe image base
    std::wstring name(lpModuleName);
    for (auto& c : name) c = static_cast<wchar_t>(std::towlower(c));
    if (name == L"kernel32" || name == L"kernel32.dll") {
        return reinterpret_cast<HMODULE>(0x7FF800000000ULL);
    }
    if (name == L"ntdll" || name == L"ntdll.dll") {
        return reinterpret_cast<HMODULE>(0x7FF810000000ULL);
    }
    return reinterpret_cast<HMODULE>(0x140000000ULL);
}

inline HMODULE GetModuleHandleA(LPCSTR lpModuleName) noexcept {
    if (!lpModuleName) return GetModuleHandleW(nullptr);
    std::string modStr(lpModuleName);
    std::wstring wMod(modStr.begin(), modStr.end());
    return GetModuleHandleW(wMod.c_str());
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

inline BOOL CreateDirectoryW(LPCWSTR lpPathName, void* /*lpSecurityAttributes*/) noexcept {
    if (!lpPathName) return FALSE;
    NtStatus status = fs::VirtualFileSystem::get().createDirectory(lpPathName);
    if (!NT_SUCCESS(status)) {
        SetLastError(ntdll::RtlNtStatusToDosError(status));
        return FALSE;
    }
    return TRUE;
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

inline void GetSystemTimeAsFileTime(LargeInteger* lpSystemTimeAsFileTime) noexcept {
    if (!lpSystemTimeAsFileTime) return;
    lpSystemTimeAsFileTime->quadPart = 133500000000000000LL; // Win32 100ns epochs
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
    NtStatus status = ntdll::NtTerminateProcess(reinterpret_cast<Handle>(hProcess), static_cast<NtStatus>(uExitCode));
    return NT_SUCCESS(status) ? TRUE : FALSE;
}

inline BOOL SwitchToThread() noexcept {
    ntdll::NtYieldExecution();
    return TRUE;
}

// ============================================================================
// 17. Win32 Dynamic Subsystem Export Table Initializer
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
    ldr.registerExport("kernel32.dll", "VirtualAlloc", reinterpret_cast<void*>(VirtualAlloc));
    ldr.registerExport("kernel32.dll", "VirtualFree", reinterpret_cast<void*>(VirtualFree));
    ldr.registerExport("kernel32.dll", "GetCurrentProcess", reinterpret_cast<void*>(GetCurrentProcess));
    ldr.registerExport("kernel32.dll", "GetCurrentProcessId", reinterpret_cast<void*>(GetCurrentProcessId));
    ldr.registerExport("kernel32.dll", "GetCurrentThread", reinterpret_cast<void*>(GetCurrentThread));
    ldr.registerExport("kernel32.dll", "GetCurrentThreadId", reinterpret_cast<void*>(GetCurrentThreadId));
    ldr.registerExport("kernel32.dll", "ExitProcess", reinterpret_cast<void*>(ExitProcess));
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
    ldr.registerExport("kernel32.dll", "CreateDirectoryW", reinterpret_cast<void*>(CreateDirectoryW));
    ldr.registerExport("kernel32.dll", "RemoveDirectoryW", reinterpret_cast<void*>(RemoveDirectoryW));
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
}


} // namespace micant::win32
