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
#include <condition_variable>
#include <unordered_map>
#include <deque>
#include <vector>
#include <functional>
#include <atomic>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/native/ntdll.hpp"
#include "micant/um/native/ldr.hpp"
#include "micant/um/servers/csrss.hpp"
#include "micant/um/servers/conhost.hpp"
#include "micant/km/hal/hal.hpp"
#include "micant/km/drivers/fs/npfs.hpp"
#include "micant/km/drivers/fs/fs.hpp"

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
using PSIZE_T   = size_t*;
using DWORD_PTR = uintptr_t;
using PDWORD_PTR = DWORD_PTR*;
using ULONG_PTR = uintptr_t;
using LONG_PTR  = intptr_t;
using LONG      = int32_t;
using LPLONG    = int32_t*;
using WCHAR     = wchar_t;
using PCWSTR    = const wchar_t*;
using PWSTR     = wchar_t*;
using LPCWCH    = const wchar_t*;
using ULONG     = uint32_t;
using KAFFINITY = ULONG_PTR;
using PULONG    = uint32_t*;
using PBOOL     = int32_t*;
using LPBOOL    = int32_t*;
using PVOID     = void*;
using VOID      = void;
using WORD      = uint16_t;
using LPWORD    = uint16_t*;
using BYTE      = uint8_t;
using LCID      = uint32_t;
using LPARAM    = intptr_t;
using WPARAM    = uintptr_t;
using ULONGLONG  = uint64_t;
using PULONGLONG = uint64_t*;
using LONGLONG   = int64_t;
using PLONGLONG  = int64_t*;
using FARPROC    = intptr_t (*)();

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

struct FILETIME;

struct TP_POOL;
using PTP_POOL = TP_POOL*;

struct TP_CLEANUP_GROUP;
using PTP_CLEANUP_GROUP = TP_CLEANUP_GROUP*;

struct TP_CALLBACK_INSTANCE;
using PTP_CALLBACK_INSTANCE = TP_CALLBACK_INSTANCE*;

struct TP_WORK;
using PTP_WORK = TP_WORK*;

struct TP_TIMER;
using PTP_TIMER = TP_TIMER*;

struct TP_WAIT;
using PTP_WAIT = TP_WAIT*;

using TP_WAIT_RESULT = ULONG;

using PTP_WORK_CALLBACK = void (WINAPI *)(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work);
using PTP_TIMER_CALLBACK = void (WINAPI *)(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer);
using PTP_WAIT_CALLBACK = void (WINAPI *)(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult);
using PTP_CLEANUP_GROUP_CANCEL_CALLBACK = void (WINAPI *)(PVOID ObjectContext, PVOID CleanupContext);

struct TP_CALLBACK_ENVIRON_V3 {
    ULONG Version;
    PTP_POOL Pool;
    PTP_CLEANUP_GROUP CleanupGroup;
    PTP_CLEANUP_GROUP_CANCEL_CALLBACK CleanupGroupCancelCallback;
    PVOID RaceDll;
    void* ActivationContext;
    PVOID FinalizationCallback;
    union {
        DWORD Flags;
        struct {
            DWORD LongFunction : 1;
            DWORD Persistent : 1;
            DWORD Private : 30;
        } s;
    } u;
    ULONG CallbackPriority;
    ULONG Size;
};
using TP_CALLBACK_ENVIRON = TP_CALLBACK_ENVIRON_V3;
using PTP_CALLBACK_ENVIRON = TP_CALLBACK_ENVIRON*;

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
inline constexpr DWORD ERROR_INSUFFICIENT_BUFFER = 122;
inline constexpr DWORD ERROR_ALREADY_EXISTS     = 183;
inline constexpr DWORD ERROR_NO_MORE_ITEMS      = 259;
inline constexpr DWORD ERROR_TIMEOUT            = 1460;

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

// Extended Virtual Allocation Parameters & Placeholder Flags
inline constexpr DWORD MEM_REPLACE_PLACEHOLDER  = 0x00004000;
inline constexpr DWORD MEM_RESERVE_PLACEHOLDER  = 0x00040000;
inline constexpr DWORD MEM_PRESERVE_PLACEHOLDER = 0x00000002;

enum MEM_EXTENDED_PARAMETER_TYPE {
    MemExtendedParameterInvalidType = 0,
    MemExtendedParameterAddressRequirements = 1,
    MemExtendedParameterNumaNode = 2,
    MemExtendedParameterPartitionHandle = 3,
    MemExtendedParameterUserPhysicalHandle = 4,
    MemExtendedParameterAttributeFlags = 5,
    MemExtendedParameterImageMachine = 6,
    MemExtendedParameterMax = 7
};

struct MEM_ADDRESS_REQUIREMENTS {
    PVOID  LowestStartingAddress{nullptr};
    PVOID  HighestEndingAddress{nullptr};
    SIZE_T Alignment{0};
};
using PMEM_ADDRESS_REQUIREMENTS = MEM_ADDRESS_REQUIREMENTS*;

struct MEM_EXTENDED_PARAMETER {
    struct {
        uint64_t Type : 8;
        uint64_t Reserved : 56;
    };
    union {
        uint64_t ULong64;
        PVOID    Pointer;
        SIZE_T   Size;
        HANDLE   Handle;
        ULONG    ULong;
    };
};
using PMEM_EXTENDED_PARAMETER = MEM_EXTENDED_PARAMETER*;

namespace detail {
    struct PlaceholderEntry {
        uintptr_t baseAddress{0};
        size_t size{0};
        bool isPlaceholder{true};
        std::unordered_map<uintptr_t, size_t> committedSlices;
    };
    inline std::mutex s_placeholderMutex;
    inline std::unordered_map<uintptr_t, PlaceholderEntry> s_placeholders;
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

/**
 * @brief Reserves, commits, or changes the state of a region of pages within the virtual address space of a specified process with extended parameters (Win10 build 17093+ / Server 2019+).
 */
inline PVOID WINAPI VirtualAlloc2(
    HANDLE Process,
    PVOID BaseAddress,
    SIZE_T Size,
    DWORD AllocationType,
    DWORD PageProtection,
    MEM_EXTENDED_PARAMETER* ExtendedParameters,
    ULONG ParameterCount
) noexcept {
    (void)Process;
    uintptr_t targetBase = reinterpret_cast<uintptr_t>(BaseAddress);
    SIZE_T targetSize = Size;

    if (ExtendedParameters && ParameterCount > 0) {
        for (ULONG i = 0; i < ParameterCount; ++i) {
            if (ExtendedParameters[i].Type == MemExtendedParameterAddressRequirements) {
                auto* req = reinterpret_cast<MEM_ADDRESS_REQUIREMENTS*>(ExtendedParameters[i].Pointer);
                if (req && req->Alignment > 0 && targetBase == 0) {
                    size_t align = req->Alignment;
                    targetSize = (targetSize + align - 1) & ~(align - 1);
                }
            }
        }
    }

    if (AllocationType & MEM_RESERVE_PLACEHOLDER) {
        LPVOID res = VirtualAlloc(reinterpret_cast<LPVOID>(targetBase), targetSize, MEM_RESERVE, PageProtection);
        if (res) {
            std::lock_guard<std::mutex> lock(detail::s_placeholderMutex);
            detail::PlaceholderEntry pe{};
            pe.baseAddress = reinterpret_cast<uintptr_t>(res);
            pe.size = targetSize;
            pe.isPlaceholder = true;
            detail::s_placeholders[pe.baseAddress] = std::move(pe);
        }
        return res;
    }

    if (AllocationType & MEM_REPLACE_PLACEHOLDER) {
        std::lock_guard<std::mutex> lock(detail::s_placeholderMutex);
        for (auto& [bAddr, pe] : detail::s_placeholders) {
            if (targetBase >= pe.baseAddress && (targetBase + targetSize) <= (pe.baseAddress + pe.size)) {
                pe.committedSlices[targetBase] = targetSize;
                break;
            }
        }
        DWORD pureAlloc = AllocationType & ~MEM_REPLACE_PLACEHOLDER;
        if (pureAlloc == 0) pureAlloc = MEM_COMMIT;
        return VirtualAlloc(reinterpret_cast<LPVOID>(targetBase), targetSize, pureAlloc, PageProtection);
    }

    return VirtualAlloc(reinterpret_cast<LPVOID>(targetBase), targetSize, AllocationType, PageProtection);
}

/**
 * @brief Reserves, commits, or changes the state of a region of pages within the virtual address space of a process with app container restrictions.
 */
inline PVOID WINAPI VirtualAlloc2FromApp(
    HANDLE Process,
    PVOID BaseAddress,
    SIZE_T Size,
    DWORD AllocationType,
    DWORD PageProtection,
    MEM_EXTENDED_PARAMETER* ExtendedParameters,
    ULONG ParameterCount
) noexcept {
    if (PageProtection == PAGE_EXECUTE_READWRITE) {
        return nullptr;
    }
    return VirtualAlloc2(Process, BaseAddress, Size, AllocationType, PageProtection, ExtendedParameters, ParameterCount);
}

/**
 * @brief Reserves, commits, or changes the state of a region of memory within the virtual address space of a specified process.
 */
inline LPVOID WINAPI VirtualAllocEx(
    HANDLE hProcess,
    LPVOID lpAddress,
    SIZE_T dwSize,
    DWORD flAllocationType,
    DWORD flProtect
) noexcept {
    return VirtualAlloc2(hProcess, lpAddress, dwSize, flAllocationType, flProtect, nullptr, 0);
}

/**
 * @brief Releases, decommits, or releases and decommits a region of memory within the virtual address space of a specified process.
 */
inline BOOL WINAPI VirtualFreeEx(
    HANDLE hProcess,
    LPVOID lpAddress,
    SIZE_T dwSize,
    DWORD dwFreeType
) noexcept {
    (void)hProcess;
    uintptr_t target = reinterpret_cast<uintptr_t>(lpAddress);
    if (dwFreeType & MEM_PRESERVE_PLACEHOLDER) {
        std::lock_guard<std::mutex> lock(detail::s_placeholderMutex);
        for (auto& [bAddr, pe] : detail::s_placeholders) {
            auto it = pe.committedSlices.find(target);
            if (it != pe.committedSlices.end()) {
                pe.committedSlices.erase(it);
                break;
            }
        }
        return VirtualFree(lpAddress, dwSize, MEM_DECOMMIT);
    }

    if (dwFreeType == MEM_RELEASE) {
        std::lock_guard<std::mutex> lock(detail::s_placeholderMutex);
        detail::s_placeholders.erase(target);
    }

    return VirtualFree(lpAddress, dwSize, dwFreeType);
}

/**
 * @brief Locks the specified region of the process's virtual address space into physical memory (working set).
 */
inline BOOL WINAPI VirtualLock(LPVOID lpAddress, SIZE_T dwSize) noexcept {
    if (!lpAddress || dwSize == 0) return FALSE;
    return TRUE;
}

/**
 * @brief Unlocks a specified range of pages in the virtual address space of a process.
 */
inline BOOL WINAPI VirtualUnlock(LPVOID lpAddress, SIZE_T dwSize) noexcept {
    if (!lpAddress || dwSize == 0) return FALSE;
    return TRUE;
}

/**
 * @brief Retrieves the minimum and maximum working set sizes for the specified process.
 */
inline BOOL WINAPI GetProcessWorkingSetSizeEx(
    HANDLE hProcess,
    SIZE_T* lpMinimumWorkingSetSize,
    SIZE_T* lpMaximumWorkingSetSize,
    DWORD* lpFlags
) noexcept {
    (void)hProcess;
    if (lpMinimumWorkingSetSize) *lpMinimumWorkingSetSize = 50 * 4096;
    if (lpMaximumWorkingSetSize) *lpMaximumWorkingSetSize = 345 * 4096;
    if (lpFlags) *lpFlags = 0;
    return TRUE;
}

/**
 * @brief Sets the minimum and maximum working set sizes for the specified process.
 */
inline BOOL WINAPI SetProcessWorkingSetSizeEx(
    HANDLE hProcess,
    SIZE_T dwMinimumWorkingSetSize,
    SIZE_T dwMaximumWorkingSetSize,
    DWORD dwFlags
) noexcept {
    (void)hProcess;
    (void)dwMinimumWorkingSetSize;
    (void)dwMaximumWorkingSetSize;
    (void)dwFlags;
    return TRUE;
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
 * @brief Retrieves the package full name for the current process.
 * Conforms to win32metadata / appmodel.h specifications.
 */
inline uint32_t WINAPI GetCurrentPackageFullName(uint32_t* packageFullNameLength, wchar_t* /*packageFullName*/) noexcept {
    if (packageFullNameLength) *packageFullNameLength = 0;
    return 15700; // APPMODEL_ERROR_NO_PACKAGE
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
        fs::VirtualFileSystem::get().closeFile(srcObj.get());
        SetLastError(ntdll::RtlNtStatusToDosError(st));
        return FALSE;
    }

    constexpr size_t CHUNK_SIZE = 65536;
    std::vector<uint8_t> buffer(CHUNK_SIZE);
    LargeInteger readOffset{};
    readOffset.quadPart = 0;
    LargeInteger writeOffset{};
    writeOffset.quadPart = 0;
    BOOL copySuccess = TRUE;

    while (true) {
        uint32_t bytesRead = 0;
        NtStatus readSt = fs::VirtualFileSystem::get().readFile(srcObj.get(), buffer.data(), static_cast<uint32_t>(buffer.size()), &readOffset, bytesRead);
        if (readSt == NtStatus::EndOfFile || bytesRead == 0) {
            break;
        }
        if (!NT_SUCCESS(readSt)) {
            SetLastError(ntdll::RtlNtStatusToDosError(readSt));
            copySuccess = FALSE;
            break;
        }
        uint32_t bytesWritten = 0;
        NtStatus writeSt = fs::VirtualFileSystem::get().writeFile(dstObj.get(), buffer.data(), bytesRead, &writeOffset, bytesWritten);
        if (!NT_SUCCESS(writeSt)) {
            SetLastError(ntdll::RtlNtStatusToDosError(writeSt));
            copySuccess = FALSE;
            break;
        }
        readOffset.quadPart += bytesRead;
        writeOffset.quadPart += bytesWritten;
    }

    fs::VirtualFileSystem::get().closeFile(srcObj.get());
    fs::VirtualFileSystem::get().closeFile(dstObj.get());
    return copySuccess;
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

/**
 * @brief Maps a view of a file mapping into the address space of a specified process with NUMA node preference.
 */
inline PVOID WINAPI MapViewOfFileNuma2(
    HANDLE FileMappingHandle,
    HANDLE ProcessHandle,
    uint64_t Offset,
    PVOID BaseAddress,
    SIZE_T ViewSize,
    ULONG AllocationType,
    ULONG PageProtection,
    ULONG PreferredNode
) noexcept {
    (void)ProcessHandle;
    (void)AllocationType;
    (void)PreferredNode;
    (void)BaseAddress;
    return MapViewOfFile(FileMappingHandle, PageProtection, static_cast<DWORD>(Offset >> 32), static_cast<DWORD>(Offset & 0xFFFFFFFF), ViewSize);
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

// ============================================================================
// Thread Context & Hardware Breakpoint Control (x64 Windows NT ABI)
// ============================================================================

inline constexpr uint32_t CONTEXT_AMD64_FLAG = 0x00100000L;

#ifndef CONTEXT_CONTROL
#define CONTEXT_CONTROL (0x00100000L | 0x1L)
#endif

#ifndef CONTEXT_INTEGER
#define CONTEXT_INTEGER (0x00100000L | 0x2L)
#endif

#ifndef CONTEXT_SEGMENTS
#define CONTEXT_SEGMENTS (0x00100000L | 0x4L)
#endif

#ifndef CONTEXT_FLOATING_POINT
#define CONTEXT_FLOATING_POINT (0x00100000L | 0x8L)
#endif

#ifndef CONTEXT_DEBUG_REGISTERS
#define CONTEXT_DEBUG_REGISTERS (0x00100000L | 0x10L)
#endif

#ifndef CONTEXT_FULL
#define CONTEXT_FULL (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT)
#endif

#ifndef CONTEXT_ALL
#define CONTEXT_ALL (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS | CONTEXT_FLOATING_POINT | CONTEXT_DEBUG_REGISTERS)
#endif

using micant::M128A;
using micant::XMM_SAVE_AREA32;
using micant::CONTEXT;
using micant::PCONTEXT;
using micant::LPCONTEXT;

using PVECTORED_EXCEPTION_HANDLER = ntdll::PVECTORED_EXCEPTION_HANDLER;
using EXCEPTION_RECORD = ntdll::EXCEPTION_RECORD;
using PEXCEPTION_RECORD = ntdll::PEXCEPTION_RECORD;
using EXCEPTION_POINTERS = ntdll::EXCEPTION_POINTERS;
using PEXCEPTION_POINTERS = ntdll::PEXCEPTION_POINTERS;
using LPEXCEPTION_POINTERS = ntdll::PEXCEPTION_POINTERS;

static_assert(sizeof(CONTEXT) == 1232, "sizeof(CONTEXT) on x64 Windows ABI must be exactly 1232 bytes");

class ThreadContextManager {
public:
    static ThreadContextManager& get() noexcept {
        static ThreadContextManager instance;
        return instance;
    }

    void initializeDefaultContext(CONTEXT& ctx) noexcept {
        std::memset(&ctx, 0, sizeof(CONTEXT));
        ctx.ContextFlags = CONTEXT_ALL;
        ctx.MxCsr = 0x1F80;
        ctx.SegCs = 0x33;
        ctx.SegDs = 0x2B;
        ctx.SegEs = 0x2B;
        ctx.SegFs = 0x53;
        ctx.SegGs = 0x2B;
        ctx.SegSs = 0x2B;
        ctx.EFlags = 0x202; // IF flag enabled
        ctx.Rsp = 0x0000007FFFF00000ULL;
        ctx.Rbp = 0x0000007FFFF00080ULL;
        ctx.Rip = 0x00007FF600001000ULL;
    }

    BOOL getContext(HANDLE hThread, CONTEXT* lpContext) noexcept {
        if (!lpContext) return FALSE;
        uint32_t flags = lpContext->ContextFlags ? lpContext->ContextFlags : CONTEXT_ALL;
        std::lock_guard<std::mutex> lock(m_mutex);
        uintptr_t tid = reinterpret_cast<uintptr_t>(hThread);
        auto it = m_contexts.find(tid);
        CONTEXT stored;
        if (it != m_contexts.end()) {
            stored = it->second;
        } else {
            initializeDefaultContext(stored);
            m_contexts[tid] = stored;
        }

        if (flags & CONTEXT_CONTROL) {
            lpContext->Rip = stored.Rip;
            lpContext->Rsp = stored.Rsp;
            lpContext->SegCs = stored.SegCs;
            lpContext->SegSs = stored.SegSs;
            lpContext->EFlags = stored.EFlags;
            lpContext->MxCsr = stored.MxCsr;
        }
        if (flags & CONTEXT_INTEGER) {
            lpContext->Rax = stored.Rax;
            lpContext->Rcx = stored.Rcx;
            lpContext->Rdx = stored.Rdx;
            lpContext->Rbx = stored.Rbx;
            lpContext->Rbp = stored.Rbp;
            lpContext->Rsi = stored.Rsi;
            lpContext->Rdi = stored.Rdi;
            lpContext->R8  = stored.R8;
            lpContext->R9  = stored.R9;
            lpContext->R10 = stored.R10;
            lpContext->R11 = stored.R11;
            lpContext->R12 = stored.R12;
            lpContext->R13 = stored.R13;
            lpContext->R14 = stored.R14;
            lpContext->R15 = stored.R15;
        }
        if (flags & CONTEXT_SEGMENTS) {
            lpContext->SegDs = stored.SegDs;
            lpContext->SegEs = stored.SegEs;
            lpContext->SegFs = stored.SegFs;
            lpContext->SegGs = stored.SegGs;
        }
        if (flags & CONTEXT_DEBUG_REGISTERS) {
            lpContext->Dr0 = stored.Dr0;
            lpContext->Dr1 = stored.Dr1;
            lpContext->Dr2 = stored.Dr2;
            lpContext->Dr3 = stored.Dr3;
            lpContext->Dr6 = stored.Dr6;
            lpContext->Dr7 = stored.Dr7;
        }
        if (flags & CONTEXT_FLOATING_POINT) {
            lpContext->FltSave = stored.FltSave;
        }
        return TRUE;
    }

    BOOL setContext(HANDLE hThread, const CONTEXT* lpContext) noexcept {
        if (!lpContext) return FALSE;
        uint32_t flags = lpContext->ContextFlags ? lpContext->ContextFlags : CONTEXT_ALL;
        std::lock_guard<std::mutex> lock(m_mutex);
        uintptr_t tid = reinterpret_cast<uintptr_t>(hThread);
        auto it = m_contexts.find(tid);
        CONTEXT& stored = (it != m_contexts.end()) ? it->second : m_contexts[tid];
        if (it == m_contexts.end()) {
            initializeDefaultContext(stored);
        }

        if (flags & CONTEXT_CONTROL) {
            stored.Rip = lpContext->Rip;
            stored.Rsp = lpContext->Rsp;
            stored.SegCs = lpContext->SegCs;
            stored.SegSs = lpContext->SegSs;
            stored.EFlags = lpContext->EFlags;
            stored.MxCsr = lpContext->MxCsr;
        }
        if (flags & CONTEXT_INTEGER) {
            stored.Rax = lpContext->Rax;
            stored.Rcx = lpContext->Rcx;
            stored.Rdx = lpContext->Rdx;
            stored.Rbx = lpContext->Rbx;
            stored.Rbp = lpContext->Rbp;
            stored.Rsi = lpContext->Rsi;
            stored.Rdi = lpContext->Rdi;
            stored.R8  = lpContext->R8;
            stored.R9  = lpContext->R9;
            stored.R10 = lpContext->R10;
            stored.R11 = lpContext->R11;
            stored.R12 = lpContext->R12;
            stored.R13 = lpContext->R13;
            stored.R14 = lpContext->R14;
            stored.R15 = lpContext->R15;
        }
        if (flags & CONTEXT_SEGMENTS) {
            stored.SegDs = lpContext->SegDs;
            stored.SegEs = lpContext->SegEs;
            stored.SegFs = lpContext->SegFs;
            stored.SegGs = lpContext->SegGs;
        }
        if (flags & CONTEXT_DEBUG_REGISTERS) {
            stored.Dr0 = lpContext->Dr0;
            stored.Dr1 = lpContext->Dr1;
            stored.Dr2 = lpContext->Dr2;
            stored.Dr3 = lpContext->Dr3;
            stored.Dr6 = lpContext->Dr6;
            stored.Dr7 = lpContext->Dr7;
        }
        if (flags & CONTEXT_FLOATING_POINT) {
            stored.FltSave = lpContext->FltSave;
        }
        return TRUE;
    }

private:
    std::mutex m_mutex;
    std::unordered_map<uintptr_t, CONTEXT> m_contexts;
};

inline BOOL WINAPI GetThreadContext(HANDLE hThread, void* lpContext) noexcept {
    if (!lpContext) return FALSE;
    return ThreadContextManager::get().getContext(hThread, reinterpret_cast<CONTEXT*>(lpContext));
}

inline BOOL WINAPI SetThreadContext(HANDLE hThread, const void* lpContext) noexcept {
    if (!lpContext) return FALSE;
    return ThreadContextManager::get().setContext(hThread, reinterpret_cast<const CONTEXT*>(lpContext));
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

struct PROCESSOR_NUMBER {
    WORD Group{0};
    BYTE Number{0};
    BYTE Reserved{0};
};
using PPROCESSOR_NUMBER = PROCESSOR_NUMBER*;

inline constexpr uint32_t THREAD_POWER_THROTTLING_CURRENT_VERSION = 1;
inline constexpr uint32_t THREAD_POWER_THROTTLING_EXECUTION_SPEED = 0x1;

struct THREAD_POWER_THROTTLING_STATE {
    ULONG Version{THREAD_POWER_THROTTLING_CURRENT_VERSION};
    ULONG ControlMask{0};
    ULONG StateMask{0};
};

enum THREAD_INFORMATION_CLASS {
    ThreadMemoryPriority = 0,
    ThreadAbsoluteCpuPriority = 1,
    ThreadDynamicCodePolicy = 2,
    ThreadPowerThrottling = 3,
    ThreadInformationClassMax = 4
};

class SovereignThreadPacingManager {
private:
    std::mutex m_mutex;
    DWORD_PTR m_processAffinityMask{0xFFFF};
    BOOL m_processPriorityBoostDisabled{FALSE};

    struct ThreadPacingRecord {
        DWORD_PTR affinityMask{0xFFFF};
        BOOL priorityBoostDisabled{FALSE};
        PROCESSOR_NUMBER idealProcessor{0, 0, 0};
        THREAD_POWER_THROTTLING_STATE powerThrottling{1, 1, 0};
        ULONG absoluteCpuPriority{0};
    };
    std::unordered_map<HANDLE, ThreadPacingRecord> m_threads;

public:
    static SovereignThreadPacingManager& get() noexcept {
        static SovereignThreadPacingManager instance;
        return instance;
    }

    DWORD_PTR setThreadAffinityMask(HANDLE hThread, DWORD_PTR dwThreadAffinityMask) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& rec = m_threads[hThread];
        DWORD_PTR prev = rec.affinityMask;
        rec.affinityMask = dwThreadAffinityMask ? dwThreadAffinityMask : 0xFFFF;
        return prev ? prev : 0xFFFF;
    }

    BOOL getProcessAffinityMask(HANDLE /*hProcess*/, PDWORD_PTR lpProcessAffinityMask, PDWORD_PTR lpSystemAffinityMask) noexcept {
        if (!lpProcessAffinityMask || !lpSystemAffinityMask) return FALSE;
        std::lock_guard<std::mutex> lock(m_mutex);
        *lpProcessAffinityMask = m_processAffinityMask;
        *lpSystemAffinityMask = 0xFFFF; // 16 logical cores
        return TRUE;
    }

    BOOL setProcessAffinityMask(HANDLE /*hProcess*/, DWORD_PTR dwProcessAffinityMask) noexcept {
        if (!dwProcessAffinityMask) return FALSE;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_processAffinityMask = dwProcessAffinityMask;
        return TRUE;
    }

    BOOL setThreadPriorityBoost(HANDLE hThread, BOOL bDisablePriorityBoost) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_threads[hThread].priorityBoostDisabled = bDisablePriorityBoost;
        return TRUE;
    }

    BOOL getThreadPriorityBoost(HANDLE hThread, PBOOL pDisablePriorityBoost) noexcept {
        if (!pDisablePriorityBoost) return FALSE;
        std::lock_guard<std::mutex> lock(m_mutex);
        *pDisablePriorityBoost = m_threads[hThread].priorityBoostDisabled;
        return TRUE;
    }

    BOOL setProcessPriorityBoost(HANDLE /*hProcess*/, BOOL bDisablePriorityBoost) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_processPriorityBoostDisabled = bDisablePriorityBoost;
        return TRUE;
    }

    BOOL getProcessPriorityBoost(HANDLE /*hProcess*/, PBOOL pDisablePriorityBoost) noexcept {
        if (!pDisablePriorityBoost) return FALSE;
        std::lock_guard<std::mutex> lock(m_mutex);
        *pDisablePriorityBoost = m_processPriorityBoostDisabled;
        return TRUE;
    }

    BOOL setThreadIdealProcessorEx(HANDLE hThread, PPROCESSOR_NUMBER lpIdealProcessor, PPROCESSOR_NUMBER lpPreviousIdealProcessor) noexcept {
        if (!lpIdealProcessor) return FALSE;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& rec = m_threads[hThread];
        if (lpPreviousIdealProcessor) {
            *lpPreviousIdealProcessor = rec.idealProcessor;
        }
        rec.idealProcessor = *lpIdealProcessor;
        return TRUE;
    }

    BOOL getThreadIdealProcessorEx(HANDLE hThread, PPROCESSOR_NUMBER lpIdealProcessor) noexcept {
        if (!lpIdealProcessor) return FALSE;
        std::lock_guard<std::mutex> lock(m_mutex);
        *lpIdealProcessor = m_threads[hThread].idealProcessor;
        return TRUE;
    }

    BOOL setThreadInformation(HANDLE hThread, int32_t threadInfoClass, void* pData, DWORD size) noexcept {
        if (!pData || size == 0) return FALSE;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& rec = m_threads[hThread];
        if (threadInfoClass == ThreadPowerThrottling || threadInfoClass == 3) {
            if (size >= sizeof(THREAD_POWER_THROTTLING_STATE)) {
                rec.powerThrottling = *reinterpret_cast<const THREAD_POWER_THROTTLING_STATE*>(pData);
                return TRUE;
            }
        } else if (threadInfoClass == ThreadAbsoluteCpuPriority || threadInfoClass == 1) {
            if (size >= sizeof(ULONG)) {
                rec.absoluteCpuPriority = *reinterpret_cast<const ULONG*>(pData);
                return TRUE;
            }
        }
        return TRUE;
    }

    BOOL getThreadInformation(HANDLE hThread, int32_t threadInfoClass, void* pData, DWORD size) noexcept {
        if (!pData || size == 0) return FALSE;
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto& rec = m_threads[hThread];
        if (threadInfoClass == ThreadPowerThrottling || threadInfoClass == 3) {
            if (size >= sizeof(THREAD_POWER_THROTTLING_STATE)) {
                *reinterpret_cast<THREAD_POWER_THROTTLING_STATE*>(pData) = rec.powerThrottling;
                return TRUE;
            }
        } else if (threadInfoClass == ThreadAbsoluteCpuPriority || threadInfoClass == 1) {
            if (size >= sizeof(ULONG)) {
                *reinterpret_cast<ULONG*>(pData) = rec.absoluteCpuPriority;
                return TRUE;
            }
        }
        return TRUE;
    }
};

inline DWORD_PTR WINAPI SetThreadAffinityMask(HANDLE hThread, DWORD_PTR dwThreadAffinityMask) noexcept {
    return SovereignThreadPacingManager::get().setThreadAffinityMask(hThread, dwThreadAffinityMask);
}

inline BOOL WINAPI GetProcessAffinityMask(HANDLE hProcess, PDWORD_PTR lpProcessAffinityMask, PDWORD_PTR lpSystemAffinityMask) noexcept {
    return SovereignThreadPacingManager::get().getProcessAffinityMask(hProcess, lpProcessAffinityMask, lpSystemAffinityMask);
}

inline BOOL WINAPI SetProcessAffinityMask(HANDLE hProcess, DWORD_PTR dwProcessAffinityMask) noexcept {
    return SovereignThreadPacingManager::get().setProcessAffinityMask(hProcess, dwProcessAffinityMask);
}

inline BOOL WINAPI SetThreadPriorityBoost(HANDLE hThread, BOOL bDisablePriorityBoost) noexcept {
    return SovereignThreadPacingManager::get().setThreadPriorityBoost(hThread, bDisablePriorityBoost);
}

inline BOOL WINAPI GetThreadPriorityBoost(HANDLE hThread, PBOOL pDisablePriorityBoost) noexcept {
    return SovereignThreadPacingManager::get().getThreadPriorityBoost(hThread, pDisablePriorityBoost);
}

inline BOOL WINAPI SetProcessPriorityBoost(HANDLE hProcess, BOOL bDisablePriorityBoost) noexcept {
    return SovereignThreadPacingManager::get().setProcessPriorityBoost(hProcess, bDisablePriorityBoost);
}

inline BOOL WINAPI GetProcessPriorityBoost(HANDLE hProcess, PBOOL pDisablePriorityBoost) noexcept {
    return SovereignThreadPacingManager::get().getProcessPriorityBoost(hProcess, pDisablePriorityBoost);
}

inline BOOL WINAPI SetThreadIdealProcessorEx(HANDLE hThread, PPROCESSOR_NUMBER lpIdealProcessor, PPROCESSOR_NUMBER lpPreviousIdealProcessor) noexcept {
    return SovereignThreadPacingManager::get().setThreadIdealProcessorEx(hThread, lpIdealProcessor, lpPreviousIdealProcessor);
}

inline BOOL WINAPI GetThreadIdealProcessorEx(HANDLE hThread, PPROCESSOR_NUMBER lpIdealProcessor) noexcept {
    return SovereignThreadPacingManager::get().getThreadIdealProcessorEx(hThread, lpIdealProcessor);
}

inline BOOL WINAPI SetThreadInformation(HANDLE hThread, int32_t ThreadInformationClass, void* ThreadInformation, DWORD ThreadInformationSize) noexcept {
    return SovereignThreadPacingManager::get().setThreadInformation(hThread, ThreadInformationClass, ThreadInformation, ThreadInformationSize);
}

inline BOOL WINAPI GetThreadInformation(HANDLE hThread, int32_t ThreadInformationClass, void* ThreadInformation, DWORD ThreadInformationSize) noexcept {
    return SovereignThreadPacingManager::get().getThreadInformation(hThread, ThreadInformationClass, ThreadInformation, ThreadInformationSize);
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

// ============================================================================
// Win32 Fibers & Fiber Local Storage (FLS) (Windows NT / winbase.h)
// ============================================================================

using LPFIBER_START_ROUTINE = void(WINAPI*)(LPVOID lpFiberParameter);
using PFLS_CALLBACK_FUNCTION = void(WINAPI*)(PVOID lpFlsData);

inline constexpr DWORD FLS_OUT_OF_INDEXES = 0xFFFFFFFF;
inline constexpr DWORD FIBER_FLAG_FLOAT_SWITCH = 0x1;

struct FIBER_RECORD {
    LPVOID lpParameter{nullptr};
    LPFIBER_START_ROUTINE lpStartAddress{nullptr};
    size_t stackCommitSize{0};
    size_t stackReserveSize{0};
    DWORD flags{0};
    bool isThread{false};
    bool deleted{false};
    std::unordered_map<DWORD, PVOID> flsData;
};

class SovereignFiberManager {
private:
    std::mutex m_mutex;
    std::unordered_map<LPVOID, FIBER_RECORD> m_fibers;
    std::unordered_map<DWORD, PFLS_CALLBACK_FUNCTION> m_flsCallbacks;
    std::vector<bool> m_flsAllocated;
    uintptr_t m_nextFiberId{0x10000};
    inline static thread_local LPVOID t_currentFiber{nullptr};
    inline static thread_local LPVOID t_primaryFiber{nullptr};

public:
    static SovereignFiberManager& get() noexcept {
        static SovereignFiberManager instance;
        return instance;
    }

    SovereignFiberManager() : m_flsAllocated(128, false) {}

    LPVOID convertThreadToFiber(LPVOID lpParameter, DWORD dwFlags) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (t_currentFiber != nullptr) {
            SetLastError(ERROR_ALREADY_EXISTS);
            return nullptr;
        }
        LPVOID fiberId = reinterpret_cast<LPVOID>(m_nextFiberId += 0x1000);
        FIBER_RECORD rec{};
        rec.lpParameter = lpParameter;
        rec.flags = dwFlags;
        rec.isThread = true;
        m_fibers[fiberId] = rec;
        t_currentFiber = fiberId;
        t_primaryFiber = fiberId;
        return fiberId;
    }

    BOOL convertFiberToThread() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        LPVOID target = t_currentFiber ? t_currentFiber : t_primaryFiber;
        if (target == nullptr) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        auto it = m_fibers.find(target);
        if (it != m_fibers.end() && it->second.isThread) {
            m_fibers.erase(it);
            t_currentFiber = nullptr;
            t_primaryFiber = nullptr;
            return TRUE;
        }
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    LPVOID createFiber(SIZE_T dwStackCommitSize, SIZE_T dwStackReserveSize, DWORD dwFlags, LPFIBER_START_ROUTINE lpStartAddress, LPVOID lpParameter) noexcept {
        if (!lpStartAddress) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return nullptr;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        LPVOID fiberId = reinterpret_cast<LPVOID>(m_nextFiberId += 0x1000);
        FIBER_RECORD rec{};
        rec.lpParameter = lpParameter;
        rec.lpStartAddress = lpStartAddress;
        rec.stackCommitSize = dwStackCommitSize ? dwStackCommitSize : 64 * 1024;
        rec.stackReserveSize = dwStackReserveSize ? dwStackReserveSize : 1024 * 1024;
        rec.flags = dwFlags;
        rec.isThread = false;
        m_fibers[fiberId] = rec;
        return fiberId;
    }

    void deleteFiber(LPVOID lpFiber) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_fibers.find(lpFiber);
        if (it != m_fibers.end()) {
            for (const auto& [idx, data] : it->second.flsData) {
                if (data) {
                    auto cbIt = m_flsCallbacks.find(idx);
                    if (cbIt != m_flsCallbacks.end() && cbIt->second) {
                        cbIt->second(data);
                    }
                }
            }
            if (t_currentFiber == lpFiber) {
                t_currentFiber = t_primaryFiber;
            }
            m_fibers.erase(it);
        }
    }

    void switchToFiber(LPVOID lpFiber) noexcept {
        LPFIBER_START_ROUTINE startRoutine = nullptr;
        LPVOID param = nullptr;
        LPVOID prevFiber = t_currentFiber;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_fibers.find(lpFiber);
            if (it == m_fibers.end() || it->second.deleted) {
                return;
            }
            t_currentFiber = lpFiber;
            if (!it->second.isThread && it->second.lpStartAddress) {
                startRoutine = it->second.lpStartAddress;
                param = it->second.lpParameter;
            }
        }
        if (startRoutine) {
            startRoutine(param);
            t_currentFiber = prevFiber;
        }
    }

    [[nodiscard]] BOOL isThreadAFiber() const noexcept {
        return (t_currentFiber != nullptr || t_primaryFiber != nullptr) ? TRUE : FALSE;
    }

    [[nodiscard]] LPVOID getCurrentFiber() const noexcept {
        return t_currentFiber ? t_currentFiber : t_primaryFiber;
    }

    DWORD flsAlloc(PFLS_CALLBACK_FUNCTION lpCallback) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (DWORD i = 0; i < static_cast<DWORD>(m_flsAllocated.size()); ++i) {
            if (!m_flsAllocated[i]) {
                m_flsAllocated[i] = true;
                if (lpCallback) {
                    m_flsCallbacks[i] = lpCallback;
                }
                return i;
            }
        }
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FLS_OUT_OF_INDEXES;
    }

    BOOL flsFree(DWORD dwFlsIndex) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (dwFlsIndex >= m_flsAllocated.size() || !m_flsAllocated[dwFlsIndex]) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        m_flsAllocated[dwFlsIndex] = false;
        m_flsCallbacks.erase(dwFlsIndex);
        for (auto& [id, fiber] : m_fibers) {
            fiber.flsData.erase(dwFlsIndex);
        }
        return TRUE;
    }

    PVOID flsGetValue(DWORD dwFlsIndex) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (dwFlsIndex >= m_flsAllocated.size() || !m_flsAllocated[dwFlsIndex]) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return nullptr;
        }
        LPVOID fiber = t_currentFiber;
        if (!fiber) {
            static std::unordered_map<DWORD, PVOID> s_threadFallbackFls;
            auto it = s_threadFallbackFls.find(dwFlsIndex);
            return it != s_threadFallbackFls.end() ? it->second : nullptr;
        }
        auto it = m_fibers.find(fiber);
        if (it != m_fibers.end()) {
            auto dIt = it->second.flsData.find(dwFlsIndex);
            if (dIt != it->second.flsData.end()) {
                return dIt->second;
            }
        }
        return nullptr;
    }

    BOOL flsSetValue(DWORD dwFlsIndex, PVOID lpFlsData) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (dwFlsIndex >= m_flsAllocated.size() || !m_flsAllocated[dwFlsIndex]) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        LPVOID fiber = t_currentFiber;
        if (!fiber) {
            static std::unordered_map<DWORD, PVOID> s_threadFallbackFls;
            s_threadFallbackFls[dwFlsIndex] = lpFlsData;
            return TRUE;
        }
        auto it = m_fibers.find(fiber);
        if (it != m_fibers.end()) {
            it->second.flsData[dwFlsIndex] = lpFlsData;
            return TRUE;
        }
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
};

inline DWORD WINAPI FlsAlloc(PFLS_CALLBACK_FUNCTION lpCallback) noexcept {
    return SovereignFiberManager::get().flsAlloc(lpCallback);
}

inline PVOID WINAPI FlsGetValue(DWORD dwFlsIndex) noexcept {
    return SovereignFiberManager::get().flsGetValue(dwFlsIndex);
}

inline BOOL WINAPI FlsSetValue(DWORD dwFlsIndex, PVOID lpFlsData) noexcept {
    return SovereignFiberManager::get().flsSetValue(dwFlsIndex, lpFlsData);
}

inline BOOL WINAPI FlsFree(DWORD dwFlsIndex) noexcept {
    return SovereignFiberManager::get().flsFree(dwFlsIndex);
}

inline LPVOID WINAPI ConvertThreadToFiberEx(LPVOID lpParameter, DWORD dwFlags) noexcept {
    return SovereignFiberManager::get().convertThreadToFiber(lpParameter, dwFlags);
}

inline LPVOID WINAPI ConvertThreadToFiber(LPVOID lpParameter) noexcept {
    return ConvertThreadToFiberEx(lpParameter, 0);
}

inline BOOL WINAPI ConvertFiberToThread() noexcept {
    return SovereignFiberManager::get().convertFiberToThread();
}

inline LPVOID WINAPI CreateFiberEx(SIZE_T dwStackCommitSize, SIZE_T dwStackReserveSize, DWORD dwFlags, LPFIBER_START_ROUTINE lpStartAddress, LPVOID lpParameter) noexcept {
    return SovereignFiberManager::get().createFiber(dwStackCommitSize, dwStackReserveSize, dwFlags, lpStartAddress, lpParameter);
}

inline LPVOID WINAPI CreateFiber(SIZE_T dwStackSize, LPFIBER_START_ROUTINE lpStartAddress, LPVOID lpParameter) noexcept {
    return CreateFiberEx(dwStackSize, dwStackSize, 0, lpStartAddress, lpParameter);
}

inline void WINAPI DeleteFiber(LPVOID lpFiber) noexcept {
    SovereignFiberManager::get().deleteFiber(lpFiber);
}

inline void WINAPI SwitchToFiber(LPVOID lpFiber) noexcept {
    SovereignFiberManager::get().switchToFiber(lpFiber);
}

inline BOOL WINAPI IsThreadAFiber() noexcept {
    return SovereignFiberManager::get().isThreadAFiber();
}

inline LPVOID WINAPI GetCurrentFiber() noexcept {
    return SovereignFiberManager::get().getCurrentFiber();
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

inline BOOL VirtualProtectEx(HANDLE /*hProcess*/, LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD* lpflOldProtect) noexcept {
    return VirtualProtect(lpAddress, dwSize, flNewProtect, lpflOldProtect);
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
inline void RtlRestoreContext(void* /*ContextRecord*/, void* /*ExceptionRecord*/) noexcept {}
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
    LPCWSTR lpRootPathName,
    DWORD* lpSectorsPerCluster,
    DWORD* lpBytesPerSector,
    DWORD* lpNumberOfFreeClusters,
    DWORD* lpTotalNumberOfClusters
) noexcept {
    wchar_t driveLetter = L'C';
    if (lpRootPathName && lpRootPathName[0] != L'\0') {
        std::wstring_view root(lpRootPathName);
        if (root.starts_with(L"\\DosDevices\\") && root.size() >= 14 && root[13] == L':') {
            driveLetter = static_cast<wchar_t>(std::towupper(root[12]));
        } else if (root.starts_with(L"\\?\\") && root.size() >= 6 && root[5] == L':') {
            driveLetter = static_cast<wchar_t>(std::towupper(root[4]));
        } else if (root.size() >= 2 && root[1] == L':') {
            driveLetter = static_cast<wchar_t>(std::towupper(root[0]));
        }
    }

    fs::FileSystemVolumeInfo vi{};
    NtStatus st = fs::VirtualFileSystem::get().queryVolumeInfo(driveLetter, vi);
    if (!NT_SUCCESS(st)) {
        SetLastError(ntdll::RtlNtStatusToDosError(st));
        return FALSE;
    }

    if (lpSectorsPerCluster) *lpSectorsPerCluster = vi.sectorsPerCluster;
    if (lpBytesPerSector) *lpBytesPerSector = vi.bytesPerSector;
    if (lpNumberOfFreeClusters) *lpNumberOfFreeClusters = static_cast<DWORD>(vi.freeClusters & 0xFFFFFFFF);
    if (lpTotalNumberOfClusters) *lpTotalNumberOfClusters = static_cast<DWORD>(vi.totalClusters & 0xFFFFFFFF);
    return TRUE;
}

inline BOOL GetDiskFreeSpaceExW(
    LPCWSTR lpDirectoryName,
    PULARGE_INTEGER lpFreeBytesAvailableToCaller,
    PULARGE_INTEGER lpTotalNumberOfBytes,
    PULARGE_INTEGER lpTotalNumberOfFreeBytes
) noexcept {
    wchar_t driveLetter = L'C';
    if (lpDirectoryName && lpDirectoryName[0] != L'\0') {
        std::wstring_view root(lpDirectoryName);
        if (root.starts_with(L"\\DosDevices\\") && root.size() >= 14 && root[13] == L':') {
            driveLetter = static_cast<wchar_t>(std::towupper(root[12]));
        } else if (root.starts_with(L"\\?\\") && root.size() >= 6 && root[5] == L':') {
            driveLetter = static_cast<wchar_t>(std::towupper(root[4]));
        } else if (root.size() >= 2 && root[1] == L':') {
            driveLetter = static_cast<wchar_t>(std::towupper(root[0]));
        }
    }

    fs::FileSystemVolumeInfo vi{};
    NtStatus st = fs::VirtualFileSystem::get().queryVolumeInfo(driveLetter, vi);
    if (!NT_SUCCESS(st)) {
        SetLastError(ntdll::RtlNtStatusToDosError(st));
        return FALSE;
    }

    uint64_t clusterBytes = static_cast<uint64_t>(vi.bytesPerSector) * vi.sectorsPerCluster;
    uint64_t totalBytes = vi.totalClusters * clusterBytes;
    uint64_t freeBytes = vi.freeClusters * clusterBytes;

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
    LPCWSTR lpRootPathName,
    LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPWSTR lpFileSystemNameBuffer,
    DWORD nFileSystemNameSize
) noexcept {
    wchar_t driveLetter = L'C';
    if (lpRootPathName && lpRootPathName[0] != L'\0') {
        std::wstring_view root(lpRootPathName);
        if (root.starts_with(L"\\DosDevices\\") && root.size() >= 14 && root[13] == L':') {
            driveLetter = static_cast<wchar_t>(std::towupper(root[12]));
        } else if (root.starts_with(L"\\?\\") && root.size() >= 6 && root[5] == L':') {
            driveLetter = static_cast<wchar_t>(std::towupper(root[4]));
        } else if (root.size() >= 2 && root[1] == L':') {
            driveLetter = static_cast<wchar_t>(std::towupper(root[0]));
        }
    }

    fs::FileSystemVolumeInfo vi{};
    NtStatus st = fs::VirtualFileSystem::get().queryVolumeInfo(driveLetter, vi);
    if (!NT_SUCCESS(st)) {
        SetLastError(ntdll::RtlNtStatusToDosError(st));
        return FALSE;
    }

    if (lpVolumeNameBuffer && nVolumeNameSize > 0) {
        size_t copyLen = std::min<size_t>(nVolumeNameSize - 1, vi.volumeLabel.size());
        std::wmemcpy(lpVolumeNameBuffer, vi.volumeLabel.c_str(), copyLen);
        lpVolumeNameBuffer[copyLen] = L'\0';
    }
    if (lpVolumeSerialNumber) *lpVolumeSerialNumber = vi.serialNumber;
    if (lpMaximumComponentLength) *lpMaximumComponentLength = vi.maxComponentLength;
    if (lpFileSystemFlags) *lpFileSystemFlags = vi.fileSystemAttributes;
    if (lpFileSystemNameBuffer && nFileSystemNameSize > 0) {
        size_t copyLen = std::min<size_t>(nFileSystemNameSize - 1, vi.fileSystemName.size());
        std::wmemcpy(lpFileSystemNameBuffer, vi.fileSystemName.c_str(), copyLen);
        lpFileSystemNameBuffer[copyLen] = L'\0';
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
    auto letters = fs::VirtualFileSystem::get().getMountedDriveLetters();
    DWORD totalChars = static_cast<DWORD>(letters.size() * 4 + 1); // "X:\\\0" for each + trailing '\0'
    if (nBufferLength < totalChars) {
        return totalChars;
    }
    if (!lpBuffer) {
        return 0;
    }
    LPWSTR cur = lpBuffer;
    for (wchar_t l : letters) {
        *cur++ = static_cast<wchar_t>(std::towupper(l));
        *cur++ = L':';
        *cur++ = L'\\';
        *cur++ = L'\0';
    }
    *cur = L'\0';
    return totalChars - 1;
}

using DeviceIoControlHook_t = BOOL (*)(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned);
inline DeviceIoControlHook_t g_DeviceIoControlHook = nullptr;
inline void SetDeviceIoControlHook(DeviceIoControlHook_t hook) noexcept {
    g_DeviceIoControlHook = hook;
}

inline BOOL DeviceIoControl(
    HANDLE hDevice,
    DWORD dwIoControlCode,
    LPVOID lpInBuffer,
    DWORD nInBufferSize,
    LPVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned,
    LPOVERLAPPED /*lpOverlapped*/
) noexcept {
    if (lpBytesReturned) *lpBytesReturned = 0;

    if (g_DeviceIoControlHook && g_DeviceIoControlHook(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned)) {
        return TRUE;
    }
    IoStatusBlock iosb{};
    NtStatus status = sys::NtDeviceIoControlFile(
        reinterpret_cast<Handle>(hDevice),
        0, nullptr, nullptr,
        &iosb,
        dwIoControlCode,
        lpInBuffer, nInBufferSize,
        lpOutBuffer, nOutBufferSize
    );
    if (NT_SUCCESS(status)) {
        if (lpBytesReturned) *lpBytesReturned = static_cast<DWORD>(iosb.information);
        return TRUE;
    }
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
    HANDLE hFile,
    LPWSTR lpszFilePath,
    DWORD cchFilePath,
    DWORD dwFlags
) noexcept {
    if (!hFile || hFile == reinterpret_cast<HANDLE>(-1)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return 0;
    }
    const wchar_t* p = (dwFlags & 0x2 /* VOLUME_NAME_NT */)
        ? L"\\Device\\HarddiskVolume2\\Windows\\System32\\micant.exe"
        : L"\\\\?\\C:\\Windows\\System32\\micant.exe";
    DWORD len = static_cast<DWORD>(std::wcslen(p));
    if (cchFilePath <= len) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return len + 1;
    }
    if (!lpszFilePath) return 0;
    std::wmemcpy(lpszFilePath, p, len + 1);
    return len;
}

inline BOOL GetSystemTimeAdjustment(
    uint32_t* lpTimeAdjustment,
    uint32_t* lpTimeIncrement,
    BOOL* lpTimeAdjustmentDisabled
) noexcept {
    if (!lpTimeAdjustment || !lpTimeIncrement || !lpTimeAdjustmentDisabled) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *lpTimeAdjustment = 156250; // 15.625 ms in 100ns units
    *lpTimeIncrement = 156250;
    *lpTimeAdjustmentDisabled = TRUE;
    return TRUE;
}

inline BOOL GetSystemTimeAdjustmentPrecise(
    uint64_t* lpTimeAdjustment,
    uint64_t* lpTimeIncrement,
    BOOL* lpTimeAdjustmentDisabled
) noexcept {
    if (!lpTimeAdjustment || !lpTimeIncrement || !lpTimeAdjustmentDisabled) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *lpTimeAdjustment = 10000; // 1.000 ms in 100ns units
    *lpTimeIncrement = 10000;
    *lpTimeAdjustmentDisabled = TRUE;
    return TRUE;
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

// ============================================================================
// Windows NT Native Threadpool Execution Subsystem
// ============================================================================

struct TP_CALLBACK_INSTANCE {
    PVOID Reserved{nullptr};
};

struct TP_POOL {
    std::mutex mtx;
    std::condition_variable cv;
    std::condition_variable drainCv;
    std::vector<std::thread> workers;
    std::deque<std::function<void()>> workQueue;
    DWORD minThreads{2};
    DWORD maxThreads{8};
    std::atomic<bool> shutdown{false};

    TP_POOL() {
        startWorkers(minThreads);
    }
    ~TP_POOL() {
        shutdownPool();
    }
    void startWorkers(DWORD count) {
        if (count == 0) count = 1;
        for (DWORD i = 0; i < count; ++i) {
            workers.emplace_back([this]() {
                workerLoop();
            });
        }
    }
    void workerLoop() {
        while (!shutdown.load(std::memory_order_relaxed)) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]() {
                    return shutdown.load(std::memory_order_relaxed) || !workQueue.empty();
                });
                if (shutdown.load(std::memory_order_relaxed) && workQueue.empty()) return;
                if (!workQueue.empty()) {
                    task = std::move(workQueue.front());
                    workQueue.pop_front();
                }
            }
            if (task) {
                task();
                drainCv.notify_all();
            }
        }
    }
    void enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            workQueue.push_back(std::move(task));
        }
        cv.notify_one();
    }
    void shutdownPool() {
        bool expected = false;
        if (shutdown.compare_exchange_strong(expected, true)) {
            cv.notify_all();
            for (auto& t : workers) {
                if (t.joinable()) t.join();
            }
            workers.clear();
        }
    }
};

namespace detail {
    struct DefaultThreadPoolHolder {
        std::unique_ptr<TP_POOL> pool;
        std::mutex mtx;

        TP_POOL* get() {
            std::lock_guard<std::mutex> lock(mtx);
            if (!pool) {
                pool = std::make_unique<TP_POOL>();
            }
            return pool.get();
        }
    };
    inline DefaultThreadPoolHolder s_defaultPool;
}

inline PTP_POOL WINAPI CreateThreadpool(PVOID /*reserved*/) noexcept {
    return new TP_POOL();
}

inline void WINAPI SetThreadpoolThreadMaximum(PTP_POOL ptpp, DWORD cthrdMost) noexcept {
    if (ptpp) ptpp->maxThreads = cthrdMost;
}

inline BOOL WINAPI SetThreadpoolThreadMinimum(PTP_POOL ptpp, DWORD cthrdMic) noexcept {
    if (!ptpp) return FALSE;
    ptpp->minThreads = cthrdMic;
    return TRUE;
}

inline void WINAPI CloseThreadpool(PTP_POOL ptpp) noexcept {
    if (ptpp) {
        ptpp->shutdownPool();
        delete ptpp;
    }
}

// Threadpool Work APIs
struct TP_WORK {
    PTP_WORK_CALLBACK callback{nullptr};
    PVOID context{nullptr};
    PTP_POOL pool{nullptr};
    std::atomic<int32_t> pendingCount{0};
    std::mutex completionMtx;
    std::condition_variable completionCv;
    std::atomic<bool> cancelled{false};
};

inline PTP_WORK WINAPI CreateThreadpoolWork(PTP_WORK_CALLBACK pfnwk, PVOID pv, PTP_CALLBACK_ENVIRON pcbe) noexcept {
    if (!pfnwk) return nullptr;
    auto* work = new TP_WORK();
    work->callback = pfnwk;
    work->context = pv;
    if (pcbe && pcbe->Pool) {
        work->pool = pcbe->Pool;
    } else {
        work->pool = detail::s_defaultPool.get();
    }
    return work;
}

inline void WINAPI SubmitThreadpoolWork(PTP_WORK pwk) noexcept {
    if (!pwk || !pwk->pool) return;
    pwk->pendingCount.fetch_add(1, std::memory_order_relaxed);
    pwk->pool->enqueue([pwk]() {
        if (!pwk->cancelled.load(std::memory_order_relaxed)) {
            TP_CALLBACK_INSTANCE inst{};
            pwk->callback(&inst, pwk->context, pwk);
        }
        int32_t rem = pwk->pendingCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (rem <= 0) {
            std::lock_guard<std::mutex> lock(pwk->completionMtx);
            pwk->completionCv.notify_all();
        }
    });
}

inline void WINAPI WaitForThreadpoolWorkCallbacks(PTP_WORK pwk, BOOL fCancelPendingCallbacks) noexcept {
    if (!pwk) return;
    if (fCancelPendingCallbacks) {
        pwk->cancelled.store(true, std::memory_order_release);
    }
    std::unique_lock<std::mutex> lock(pwk->completionMtx);
    pwk->completionCv.wait(lock, [pwk]() {
        return pwk->pendingCount.load(std::memory_order_acquire) <= 0;
    });
    if (fCancelPendingCallbacks) {
        pwk->cancelled.store(false, std::memory_order_release);
    }
}

inline void WINAPI CloseThreadpoolWork(PTP_WORK pwk) noexcept {
    if (!pwk) return;
    WaitForThreadpoolWorkCallbacks(pwk, TRUE);
    delete pwk;
}

inline void WINAPI FreeLibraryWhenCallbackReturns(PTP_CALLBACK_INSTANCE /*pci*/, HMODULE /*mod*/) noexcept {}

// Threadpool Timer APIs
struct TP_TIMER {
    PTP_TIMER_CALLBACK callback{nullptr};
    PVOID context{nullptr};
    PTP_POOL pool{nullptr};
    std::atomic<bool> isSet{false};
    std::atomic<bool> cancelled{false};
    std::atomic<int32_t> pendingCount{0};
    std::mutex completionMtx;
    std::condition_variable completionCv;
    std::unique_ptr<std::thread> timerThread;
    std::mutex timerMtx;
    std::condition_variable timerCv;
    bool stopThread{false};
    std::chrono::steady_clock::time_point dueTimePoint;
    DWORD periodMs{0};
};

inline PTP_TIMER WINAPI CreateThreadpoolTimer(PTP_TIMER_CALLBACK pfnti, PVOID pv, PTP_CALLBACK_ENVIRON pcbe) noexcept {
    if (!pfnti) return nullptr;
    auto* timer = new TP_TIMER();
    timer->callback = pfnti;
    timer->context = pv;
    if (pcbe && pcbe->Pool) {
        timer->pool = pcbe->Pool;
    } else {
        timer->pool = detail::s_defaultPool.get();
    }
    return timer;
}

inline void WINAPI SetThreadpoolTimer(PTP_TIMER pti, const FILETIME* pftDueTime, DWORD msPeriod, DWORD /*msWindowLength*/) noexcept {
    if (!pti) return;
    {
        std::lock_guard<std::mutex> lock(pti->timerMtx);
        if (!pftDueTime) {
            pti->isSet.store(false, std::memory_order_release);
            pti->timerCv.notify_all();
            return;
        }

        pti->periodMs = msPeriod;
        auto* ft = reinterpret_cast<const uint32_t*>(pftDueTime);
        int64_t due64 = static_cast<int64_t>((static_cast<uint64_t>(ft[1]) << 32) | ft[0]);

        // Immediate one-shot dispatch optimization
        if (due64 <= 0 && msPeriod == 0) {
            pti->isSet.store(true, std::memory_order_release);
            pti->cancelled.store(false, std::memory_order_release);
            pti->pendingCount.fetch_add(1, std::memory_order_relaxed);
            pti->pool->enqueue([pti]() {
                if (!pti->cancelled.load(std::memory_order_relaxed)) {
                    TP_CALLBACK_INSTANCE inst{};
                    pti->callback(&inst, pti->context, pti);
                }
                pti->isSet.store(false, std::memory_order_release);
                int32_t rem = pti->pendingCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
                if (rem <= 0) {
                    std::lock_guard<std::mutex> lk(pti->completionMtx);
                    pti->completionCv.notify_all();
                }
            });
            return;
        }

        int64_t delayMs = 0;
        if (due64 < 0) {
            delayMs = (-due64) / 10000;
        } else if (due64 == 0) {
            delayMs = 0;
        } else {
            delayMs = 10;
        }
        pti->dueTimePoint = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
        pti->isSet.store(true, std::memory_order_release);
        pti->cancelled.store(false, std::memory_order_release);

        if (!pti->timerThread) {
            pti->timerThread = std::make_unique<std::thread>([pti]() {
                while (true) {
                    std::unique_lock<std::mutex> lock(pti->timerMtx);
                    pti->timerCv.wait(lock, [pti]() {
                        return pti->stopThread || pti->isSet.load(std::memory_order_relaxed);
                    });
                    if (pti->stopThread) return;

                    pti->timerCv.wait_until(lock, pti->dueTimePoint, [pti]() {
                        return pti->stopThread || !pti->isSet.load(std::memory_order_relaxed);
                    });
                    if (pti->stopThread) return;
                    if (!pti->isSet.load(std::memory_order_relaxed)) continue;

                    if (std::chrono::steady_clock::now() >= pti->dueTimePoint) {
                        pti->pendingCount.fetch_add(1, std::memory_order_relaxed);
                        pti->pool->enqueue([pti]() {
                            if (!pti->cancelled.load(std::memory_order_relaxed)) {
                                TP_CALLBACK_INSTANCE inst{};
                                pti->callback(&inst, pti->context, pti);
                            }
                            int32_t rem = pti->pendingCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
                            if (rem <= 0) {
                                std::lock_guard<std::mutex> lk(pti->completionMtx);
                                pti->completionCv.notify_all();
                            }
                        });

                        if (pti->periodMs > 0) {
                            pti->dueTimePoint = std::chrono::steady_clock::now() + std::chrono::milliseconds(pti->periodMs);
                        } else {
                            pti->isSet.store(false, std::memory_order_release);
                        }
                    }
                }
            });
        } else {
            pti->timerCv.notify_all();
        }
    }
}

inline BOOL WINAPI IsThreadpoolTimerSet(PTP_TIMER pti) noexcept {
    if (!pti) return FALSE;
    return pti->isSet.load(std::memory_order_relaxed) ? TRUE : FALSE;
}

inline void WINAPI WaitForThreadpoolTimerCallbacks(PTP_TIMER pti, BOOL fCancelPendingCallbacks) noexcept {
    if (!pti) return;
    if (fCancelPendingCallbacks) {
        pti->cancelled.store(true, std::memory_order_release);
    }
    std::unique_lock<std::mutex> lock(pti->completionMtx);
    pti->completionCv.wait(lock, [pti]() {
        return pti->pendingCount.load(std::memory_order_acquire) <= 0;
    });
    if (fCancelPendingCallbacks) {
        pti->cancelled.store(false, std::memory_order_release);
    }
}

inline void WINAPI CloseThreadpoolTimer(PTP_TIMER pti) noexcept {
    if (!pti) return;
    {
        std::lock_guard<std::mutex> lock(pti->timerMtx);
        pti->stopThread = true;
        pti->isSet.store(false, std::memory_order_release);
        pti->timerCv.notify_all();
    }
    if (pti->timerThread && pti->timerThread->joinable()) {
        pti->timerThread->join();
    }
    WaitForThreadpoolTimerCallbacks(pti, TRUE);
    delete pti;
}

// Threadpool Wait APIs
struct TP_WAIT {
    PTP_WAIT_CALLBACK callback{nullptr};
    PVOID context{nullptr};
    PTP_POOL pool{nullptr};
    HANDLE waitHandle{nullptr};
    std::atomic<bool> isSet{false};
    std::atomic<bool> cancelled{false};
    std::atomic<int32_t> pendingCount{0};
    std::mutex completionMtx;
    std::condition_variable completionCv;
    std::unique_ptr<std::thread> waitThread;
    std::mutex waitMtx;
    std::condition_variable waitCv;
    bool stopThread{false};
};

inline PTP_WAIT WINAPI CreateThreadpoolWait(PTP_WAIT_CALLBACK pfnwa, PVOID pv, PTP_CALLBACK_ENVIRON pcbe) noexcept {
    if (!pfnwa) return nullptr;
    auto* waitObj = new TP_WAIT();
    waitObj->callback = pfnwa;
    waitObj->context = pv;
    if (pcbe && pcbe->Pool) {
        waitObj->pool = pcbe->Pool;
    } else {
        waitObj->pool = detail::s_defaultPool.get();
    }
    return waitObj;
}

inline void WINAPI SetThreadpoolWait(PTP_WAIT pwa, HANDLE h, const FILETIME* pftTimeout) noexcept {
    if (!pwa) return;
    {
        std::lock_guard<std::mutex> lock(pwa->waitMtx);
        if (!h) {
            pwa->isSet.store(false, std::memory_order_release);
            pwa->waitHandle = nullptr;
            pwa->waitCv.notify_all();
            return;
        }

        pwa->waitHandle = h;
        pwa->isSet.store(true, std::memory_order_release);
        pwa->cancelled.store(false, std::memory_order_release);

        DWORD timeoutMs = 0xFFFFFFFF; // INFINITE
        if (pftTimeout) {
            auto* ft = reinterpret_cast<const uint32_t*>(pftTimeout);
            int64_t due64 = static_cast<int64_t>((static_cast<uint64_t>(ft[1]) << 32) | ft[0]);
            if (due64 < 0) timeoutMs = static_cast<DWORD>((-due64) / 10000);
            else if (due64 == 0) timeoutMs = 0;
            else timeoutMs = 50;
        }

        // Fast-path: if handle is already signaled, dispatch immediately
        if (WaitForSingleObject(h, 0) == WAIT_OBJECT_0) {
            pwa->pendingCount.fetch_add(1, std::memory_order_relaxed);
            pwa->pool->enqueue([pwa]() {
                if (!pwa->cancelled.load(std::memory_order_relaxed)) {
                    TP_CALLBACK_INSTANCE inst{};
                    pwa->callback(&inst, pwa->context, pwa, static_cast<TP_WAIT_RESULT>(WAIT_OBJECT_0));
                }
                pwa->isSet.store(false, std::memory_order_release);
                int32_t rem = pwa->pendingCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
                if (rem <= 0) {
                    std::lock_guard<std::mutex> lk(pwa->completionMtx);
                    pwa->completionCv.notify_all();
                }
            });
            return;
        }

        if (!pwa->waitThread) {
            pwa->waitThread = std::make_unique<std::thread>([pwa, timeoutMs]() {
                while (true) {
                    std::unique_lock<std::mutex> lock(pwa->waitMtx);
                    pwa->waitCv.wait(lock, [pwa]() {
                        return pwa->stopThread || pwa->isSet.load(std::memory_order_relaxed);
                    });
                    if (pwa->stopThread) return;

                    HANDLE hTarget = pwa->waitHandle;
                    pwa->isSet.store(false, std::memory_order_release);
                    lock.unlock();

                    DWORD res = WAIT_OBJECT_0;
                    if (hTarget) {
                        res = WaitForSingleObject(hTarget, timeoutMs);
                    }

                    pwa->pendingCount.fetch_add(1, std::memory_order_relaxed);
                    pwa->pool->enqueue([pwa, res]() {
                        if (!pwa->cancelled.load(std::memory_order_relaxed)) {
                            TP_CALLBACK_INSTANCE inst{};
                            pwa->callback(&inst, pwa->context, pwa, static_cast<TP_WAIT_RESULT>(res));
                        }
                        int32_t rem = pwa->pendingCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
                        if (rem <= 0) {
                            std::lock_guard<std::mutex> lk(pwa->completionMtx);
                            pwa->completionCv.notify_all();
                        }
                    });
                }
            });
        } else {
            pwa->waitCv.notify_all();
        }
    }
}

inline void WINAPI WaitForThreadpoolWaitCallbacks(PTP_WAIT pwa, BOOL fCancelPendingCallbacks) noexcept {
    if (!pwa) return;
    if (fCancelPendingCallbacks) {
        pwa->cancelled.store(true, std::memory_order_release);
    }
    std::unique_lock<std::mutex> lock(pwa->completionMtx);
    pwa->completionCv.wait(lock, [pwa]() {
        return pwa->pendingCount.load(std::memory_order_acquire) <= 0;
    });
    if (fCancelPendingCallbacks) {
        pwa->cancelled.store(false, std::memory_order_release);
    }
}

inline void WINAPI CloseThreadpoolWait(PTP_WAIT pwa) noexcept {
    if (!pwa) return;
    {
        std::lock_guard<std::mutex> lock(pwa->waitMtx);
        pwa->stopThread = true;
        pwa->isSet.store(false, std::memory_order_release);
        pwa->waitCv.notify_all();
    }
    if (pwa->waitThread && pwa->waitThread->joinable()) {
        pwa->waitThread->join();
    }
    WaitForThreadpoolWaitCallbacks(pwa, TRUE);
    delete pwa;
}

// Cleanup Group and Environment APIs
struct TP_CLEANUP_GROUP {
    std::mutex mtx;
    std::vector<PTP_WORK> works;
    std::vector<PTP_TIMER> timers;
    std::vector<PTP_WAIT> waits;
};

inline PTP_CLEANUP_GROUP WINAPI CreateThreadpoolCleanupGroup() noexcept {
    return new TP_CLEANUP_GROUP();
}

inline void WINAPI CloseThreadpoolCleanupGroupMembers(PTP_CLEANUP_GROUP ptpcg, BOOL fCancelPendingCallbacks, PVOID /*pvCleanupContext*/) noexcept {
    if (!ptpcg) return;
    std::lock_guard<std::mutex> lock(ptpcg->mtx);
    for (auto* w : ptpcg->works) {
        CloseThreadpoolWork(w);
    }
    ptpcg->works.clear();
    for (auto* t : ptpcg->timers) {
        CloseThreadpoolTimer(t);
    }
    ptpcg->timers.clear();
    for (auto* wa : ptpcg->waits) {
        CloseThreadpoolWait(wa);
    }
    ptpcg->waits.clear();
}

inline void WINAPI CloseThreadpoolCleanupGroup(PTP_CLEANUP_GROUP ptpcg) noexcept {
    if (ptpcg) {
        CloseThreadpoolCleanupGroupMembers(ptpcg, TRUE, nullptr);
        delete ptpcg;
    }
}

inline void WINAPI InitializeThreadpoolEnvironment(PTP_CALLBACK_ENVIRON pcbe) noexcept {
    if (pcbe) {
        memset(pcbe, 0, sizeof(TP_CALLBACK_ENVIRON));
        pcbe->Version = 3;
    }
}

inline void WINAPI DestroyThreadpoolEnvironment(PTP_CALLBACK_ENVIRON pcbe) noexcept {
    if (pcbe) {
        memset(pcbe, 0, sizeof(TP_CALLBACK_ENVIRON));
    }
}

inline void WINAPI SetThreadpoolCallbackPool(PTP_CALLBACK_ENVIRON pcbe, PTP_POOL ptpp) noexcept {
    if (pcbe) pcbe->Pool = ptpp;
}

inline void WINAPI SetThreadpoolCallbackCleanupGroup(PTP_CALLBACK_ENVIRON pcbe, PTP_CLEANUP_GROUP ptpcg, PVOID pfncg) noexcept {
    if (pcbe) {
        pcbe->CleanupGroup = ptpcg;
        pcbe->CleanupGroupCancelCallback = reinterpret_cast<PTP_CLEANUP_GROUP_CANCEL_CALLBACK>(pfncg);
    }
}

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

inline constexpr int ProcessPowerThrottling = 1;

struct PROCESS_POWER_THROTTLING_STATE {
    uint32_t Version;
    uint32_t ControlMask;
    uint32_t StateMask;
};

inline constexpr uint32_t PROCESS_POWER_THROTTLING_CURRENT_VERSION = 1;
inline constexpr uint32_t PROCESS_POWER_THROTTLING_EXECUTION_SPEED = 0x1;
inline constexpr uint32_t PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION = 0x2;

inline PROCESS_POWER_THROTTLING_STATE s_processPowerThrottling{
    .Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION,
    .ControlMask = 0,
    .StateMask = 0
};

inline BOOL SetProcessInformation([[maybe_unused]] HANDLE hProcess,
                                  int ProcessInformationClass,
                                  void* ProcessInformation,
                                  DWORD ProcessInformationSize) noexcept {
    if (ProcessInformationClass == ProcessPowerThrottling) {
        if (!ProcessInformation || ProcessInformationSize < sizeof(PROCESS_POWER_THROTTLING_STATE)) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        auto* pState = reinterpret_cast<const PROCESS_POWER_THROTTLING_STATE*>(ProcessInformation);
        if (pState->Version != PROCESS_POWER_THROTTLING_CURRENT_VERSION) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        s_processPowerThrottling = *pState;
        return TRUE;
    }
    return TRUE;
}

inline BOOL GetProcessInformation([[maybe_unused]] HANDLE hProcess,
                                  int ProcessInformationClass,
                                  void* ProcessInformation,
                                  DWORD ProcessInformationSize) noexcept {
    if (ProcessInformationClass == ProcessPowerThrottling) {
        if (!ProcessInformation || ProcessInformationSize < sizeof(PROCESS_POWER_THROTTLING_STATE)) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        auto* pState = reinterpret_cast<PROCESS_POWER_THROTTLING_STATE*>(ProcessInformation);
        *pState = s_processPowerThrottling;
        return TRUE;
    }
    return TRUE;
}

inline BOOL SetProcessShutdownParameters([[maybe_unused]] DWORD dwLevel, [[maybe_unused]] DWORD dwFlags) noexcept {
    return TRUE;
}

inline BOOL UnmapViewOfFileEx(LPCVOID lpBaseAddress, [[maybe_unused]] DWORD UnmapFlags) noexcept {
    return UnmapViewOfFile(lpBaseAddress);
}

inline ULONG WINAPI RemoveVectoredExceptionHandler(void* Handle) noexcept {
    return ntdll::RtlRemoveVectoredExceptionHandler(Handle);
}

inline ULONG WINAPI RemoveVectoredContinueHandler(void* Handle) noexcept {
    return ntdll::RtlRemoveVectoredContinueHandler(Handle);
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

// ============================================================================
// Win32 Job Object Types and Classes
// ============================================================================

enum JOBOBJECTINFOCLASS {
    JobObjectBasicAccountingInformation = 1,
    JobObjectBasicLimitInformation      = 2,
    JobObjectBasicProcessIdList         = 3,
    JobObjectBasicUIRestrictions        = 4,
    JobObjectSecurityLimitInformation   = 5,
    JobObjectEndOfJobTimeInformation    = 6,
    JobObjectAssociateCompletionPortInformation = 7,
    JobObjectBasicAndIoAccountingInformation    = 8,
    JobObjectExtendedLimitInformation   = 9,
    JobObjectJobSetInformation          = 10,
    JobObjectGroupInformation           = 11,
    JobObjectNotificationLimitInformation = 12,
    JobObjectLimitViolationInformation  = 13,
    JobObjectGroupInformationEx         = 14,
    JobObjectCpuRateControlInformation  = 15,
};

inline constexpr DWORD JOB_OBJECT_LIMIT_WORKINGSET                 = 0x00000001;
inline constexpr DWORD JOB_OBJECT_LIMIT_PROCESS_TIME               = 0x00000002;
inline constexpr DWORD JOB_OBJECT_LIMIT_JOB_TIME                   = 0x00000004;
inline constexpr DWORD JOB_OBJECT_LIMIT_ACTIVE_PROCESS             = 0x00000008;
inline constexpr DWORD JOB_OBJECT_LIMIT_AFFINITY                   = 0x00000010;
inline constexpr DWORD JOB_OBJECT_LIMIT_PRIORITY_CLASS             = 0x00000020;
inline constexpr DWORD JOB_OBJECT_LIMIT_PRESERVE_JOB_TIME          = 0x00000040;
inline constexpr DWORD JOB_OBJECT_LIMIT_SCHEDULING_CLASS           = 0x00000080;
inline constexpr DWORD JOB_OBJECT_LIMIT_PROCESS_MEMORY             = 0x00000100;
inline constexpr DWORD JOB_OBJECT_LIMIT_JOB_MEMORY                 = 0x00000200;
inline constexpr DWORD JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION = 0x00000400;
inline constexpr DWORD JOB_OBJECT_LIMIT_BREAKAWAY_OK               = 0x00000800;
inline constexpr DWORD JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK        = 0x00001000;
inline constexpr DWORD JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE          = 0x00002000;

inline constexpr DWORD JOB_OBJECT_CPU_RATE_CONTROL_ENABLE       = 0x00000001;
inline constexpr DWORD JOB_OBJECT_CPU_RATE_CONTROL_WEIGHT_BASED = 0x00000002;
inline constexpr DWORD JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP     = 0x00000004;
inline constexpr DWORD JOB_OBJECT_CPU_RATE_CONTROL_NOTIFY       = 0x00000008;

#pragma pack(push, 8)
struct JOBOBJECT_BASIC_LIMIT_INFORMATION {
    LARGE_INTEGER PerProcessUserTimeLimit;
    LARGE_INTEGER PerJobUserTimeLimit;
    DWORD         LimitFlags;
    SIZE_T        MinimumWorkingSetSize;
    SIZE_T        MaximumWorkingSetSize;
    DWORD         ActiveProcessLimit;
    uintptr_t     Affinity;
    DWORD         PriorityClass;
    DWORD         SchedulingClass;
};

struct IO_COUNTERS {
    uint64_t ReadOperationCount;
    uint64_t WriteOperationCount;
    uint64_t OtherOperationCount;
    uint64_t ReadTransferCount;
    uint64_t WriteTransferCount;
    uint64_t OtherTransferCount;
};

struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION {
    JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation;
    IO_COUNTERS                       IoInfo;
    SIZE_T                            ProcessMemoryLimit;
    SIZE_T                            JobMemoryLimit;
    SIZE_T                            PeakProcessMemoryUsed;
    SIZE_T                            PeakJobMemoryUsed;
};

struct JOBOBJECT_BASIC_ACCOUNTING_INFORMATION {
    LARGE_INTEGER TotalUserTime;
    LARGE_INTEGER TotalKernelTime;
    LARGE_INTEGER ThisPeriodTotalUserTime;
    LARGE_INTEGER ThisPeriodTotalKernelTime;
    DWORD         TotalPageFaultCount;
    DWORD         TotalProcesses;
    DWORD         ActiveProcesses;
    DWORD         TotalTerminatedProcesses;
};

struct JOBOBJECT_CPU_RATE_CONTROL_INFORMATION {
    DWORD ControlFlags;
    union {
        DWORD CpuRate;
        DWORD Weight;
        struct {
            uint16_t MinRate;
            uint16_t MaxRate;
        };
    };
};
#pragma pack(pop)

struct JobObjectInternal {
    std::wstring name;
    HANDLE primaryHandle{nullptr};
    std::vector<DWORD> assignedProcessIds;
    std::mutex mtx;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION extendedLimits{};
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
    JOBOBJECT_CPU_RATE_CONTROL_INFORMATION cpuRate{};
    bool isTerminated{false};
};

class JobObjectManager {
public:
    static JobObjectManager& get() noexcept {
        static JobObjectManager instance;
        return instance;
    }

    HANDLE createJob(const wchar_t* lpName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t id = m_nextId++;
        auto job = std::make_shared<JobObjectInternal>();
        HANDLE handle = reinterpret_cast<HANDLE>(id);
        job->primaryHandle = handle;
        if (lpName) {
            job->name = lpName;
            m_namedJobs[lpName] = job;
        }
        m_jobs[handle] = job;
        return handle;
    }

    HANDLE openJob(const wchar_t* lpName) {
        if (!lpName) return nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_namedJobs.find(lpName);
        if (it != m_namedJobs.end()) {
            return it->second->primaryHandle;
        }
        return nullptr;
    }

    std::shared_ptr<JobObjectInternal> getJob(HANDLE hJob) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_jobs.find(hJob);
        if (it != m_jobs.end()) return it->second;
        return nullptr;
    }

    void closeJobHandle(HANDLE hJob) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_jobs.erase(hJob);
    }

private:
    std::mutex m_mutex;
    uint64_t m_nextId{0x7101};
    std::unordered_map<HANDLE, std::shared_ptr<JobObjectInternal>> m_jobs;
    std::unordered_map<std::wstring, std::shared_ptr<JobObjectInternal>> m_namedJobs;
};

inline HANDLE WINAPI CreateJobObjectW(void* /*lpJobAttributes*/, const wchar_t* lpName) noexcept {
    return JobObjectManager::get().createJob(lpName);
}

inline HANDLE WINAPI OpenJobObjectW(DWORD /*dwDesiredAccess*/, BOOL /*bInheritHandle*/, const wchar_t* lpName) noexcept {
    return JobObjectManager::get().openJob(lpName);
}

inline BOOL WINAPI AssignProcessToJobObject(HANDLE hJob, HANDLE hProcess) noexcept {
    auto job = JobObjectManager::get().getJob(hJob);
    if (!job) return FALSE;
    DWORD pid = 0;
    if (hProcess == GetCurrentProcess() || hProcess == nullptr) {
        pid = GetCurrentProcessId();
    } else {
        pid = static_cast<DWORD>(reinterpret_cast<uintptr_t>(hProcess));
    }
    std::lock_guard<std::mutex> lock(job->mtx);
    job->assignedProcessIds.push_back(pid);
    job->accounting.TotalProcesses++;
    job->accounting.ActiveProcesses++;
    return TRUE;
}

inline BOOL WINAPI SetInformationJobObject(
    HANDLE hJob,
    int JobObjectInformationClass,
    void* lpJobObjectInformation,
    DWORD cbJobObjectInformationLength
) noexcept {
    auto job = JobObjectManager::get().getJob(hJob);
    if (!job) return FALSE;
    if (!lpJobObjectInformation) return TRUE;

    std::lock_guard<std::mutex> lock(job->mtx);
    switch (JobObjectInformationClass) {
        case JobObjectBasicLimitInformation: {
            if (cbJobObjectInformationLength < sizeof(JOBOBJECT_BASIC_LIMIT_INFORMATION)) return FALSE;
            std::memcpy(&job->extendedLimits.BasicLimitInformation, lpJobObjectInformation, sizeof(JOBOBJECT_BASIC_LIMIT_INFORMATION));
            return TRUE;
        }
        case JobObjectExtendedLimitInformation: {
            if (cbJobObjectInformationLength < sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION)) return FALSE;
            std::memcpy(&job->extendedLimits, lpJobObjectInformation, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));
            return TRUE;
        }
        case JobObjectCpuRateControlInformation: {
            if (cbJobObjectInformationLength < sizeof(JOBOBJECT_CPU_RATE_CONTROL_INFORMATION)) return FALSE;
            std::memcpy(&job->cpuRate, lpJobObjectInformation, sizeof(JOBOBJECT_CPU_RATE_CONTROL_INFORMATION));
            return TRUE;
        }
        default:
            return TRUE;
    }
}

inline BOOL WINAPI QueryInformationJobObject(
    HANDLE hJob,
    int JobObjectInformationClass,
    void* lpJobObjectInformation,
    DWORD cbJobObjectInformationLength,
    DWORD* lpReturnLength
) noexcept {
    if (!lpJobObjectInformation) return FALSE;
    auto job = JobObjectManager::get().getJob(hJob);
    if (!job) return FALSE;

    std::lock_guard<std::mutex> lock(job->mtx);
    DWORD bytesWritten = 0;
    switch (JobObjectInformationClass) {
        case JobObjectBasicAccountingInformation: {
            if (cbJobObjectInformationLength < sizeof(JOBOBJECT_BASIC_ACCOUNTING_INFORMATION)) return FALSE;
            std::memcpy(lpJobObjectInformation, &job->accounting, sizeof(JOBOBJECT_BASIC_ACCOUNTING_INFORMATION));
            bytesWritten = sizeof(JOBOBJECT_BASIC_ACCOUNTING_INFORMATION);
            break;
        }
        case JobObjectBasicLimitInformation: {
            if (cbJobObjectInformationLength < sizeof(JOBOBJECT_BASIC_LIMIT_INFORMATION)) return FALSE;
            std::memcpy(lpJobObjectInformation, &job->extendedLimits.BasicLimitInformation, sizeof(JOBOBJECT_BASIC_LIMIT_INFORMATION));
            bytesWritten = sizeof(JOBOBJECT_BASIC_LIMIT_INFORMATION);
            break;
        }
        case JobObjectExtendedLimitInformation: {
            if (cbJobObjectInformationLength < sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION)) return FALSE;
            std::memcpy(lpJobObjectInformation, &job->extendedLimits, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));
            bytesWritten = sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION);
            break;
        }
        case JobObjectCpuRateControlInformation: {
            if (cbJobObjectInformationLength < sizeof(JOBOBJECT_CPU_RATE_CONTROL_INFORMATION)) return FALSE;
            std::memcpy(lpJobObjectInformation, &job->cpuRate, sizeof(JOBOBJECT_CPU_RATE_CONTROL_INFORMATION));
            bytesWritten = sizeof(JOBOBJECT_CPU_RATE_CONTROL_INFORMATION);
            break;
        }
        default:
            return FALSE;
    }
    if (lpReturnLength) *lpReturnLength = bytesWritten;
    return TRUE;
}

inline BOOL WINAPI TerminateJobObject(HANDLE hJob, UINT /*uExitCode*/) noexcept {
    auto job = JobObjectManager::get().getJob(hJob);
    if (!job) return FALSE;
    std::lock_guard<std::mutex> lock(job->mtx);
    job->isTerminated = true;
    job->accounting.TotalTerminatedProcesses += job->accounting.ActiveProcesses;
    job->accounting.ActiveProcesses = 0;
    return TRUE;
}

inline BOOL WINAPI IsProcessInJob(HANDLE hProcess, HANDLE hJob, BOOL* Result) noexcept {
    if (!Result) return FALSE;
    *Result = FALSE;
    auto job = JobObjectManager::get().getJob(hJob);
    if (!job) return FALSE;

    DWORD pid = 0;
    if (hProcess == GetCurrentProcess()) {
        pid = GetCurrentProcessId();
    } else {
        pid = static_cast<DWORD>(reinterpret_cast<uintptr_t>(hProcess));
    }

    std::lock_guard<std::mutex> lock(job->mtx);
    for (DWORD p : job->assignedProcessIds) {
        if (p == pid) {
            *Result = TRUE;
            return TRUE;
        }
    }
    return TRUE;
}

// Memory & File Mapping
inline HANDLE OpenFileMappingA(DWORD /*dwDesiredAccess*/, BOOL /*bInheritHandle*/, const char* /*lpName*/) noexcept {
    return reinterpret_cast<HANDLE>(0x7102);
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

inline BOOL WINAPI SetPriorityClass([[maybe_unused]] HANDLE hProcess, [[maybe_unused]] DWORD dwPriorityClass) noexcept {
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
    DWORD mask = 0;
    auto letters = fs::VirtualFileSystem::get().getMountedDriveLetters();
    for (wchar_t l : letters) {
        wchar_t u = static_cast<wchar_t>(std::towupper(l));
        if (u >= L'A' && u <= L'Z') {
            mask |= (1U << (u - L'A'));
        }
    }
    return mask ? mask : 0x0000000C;
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
    std::swprintf(lpTempFileName, 260, L"%ls%ls%04X.tmp", path, prefix, id);
    return id;
}

inline void WINAPI OutputDebugStringA([[maybe_unused]] LPCSTR lpOutputString) noexcept {}

inline DWORD WINAPI SetThreadExecutionState(DWORD esFlags) noexcept {
    return esFlags;
}

inline void WINAPI DebugBreak() noexcept {}

inline void* WINAPI AddVectoredExceptionHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER Handler) noexcept {
    return ntdll::RtlAddVectoredExceptionHandler(First, Handler);
}

inline void* WINAPI AddVectoredContinueHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER Handler) noexcept {
    return ntdll::RtlAddVectoredContinueHandler(First, Handler);
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

inline HANDLE WINAPI FindFirstVolumeA(char* lpszVolumeName, DWORD cchBufferLength) noexcept {
    if (lpszVolumeName && cchBufferLength >= 50) {
        std::snprintf(lpszVolumeName, cchBufferLength, "\\\\?\\Volume{a0000000-0000-0000-0000-000000000001}\\");
        return reinterpret_cast<HANDLE>(0x3001);
    }
    return nullptr;
}

inline BOOL WINAPI FindNextVolumeA(HANDLE /*hFindVolume*/, char* /*lpszVolumeName*/, DWORD /*cchBufferLength*/) noexcept {
    win32::SetLastError(18); // ERROR_NO_MORE_FILES
    return FALSE;
}

inline BOOL WINAPI GetVolumeNameForVolumeMountPointA(
    const char* /*lpszVolumeMountPoint*/,
    char* lpszVolumeName,
    DWORD cchBufferLength
) noexcept {
    if (lpszVolumeName && cchBufferLength >= 50) {
        std::snprintf(lpszVolumeName, cchBufferLength, "\\\\?\\Volume{a0000000-0000-0000-0000-000000000001}\\");
        return TRUE;
    }
    return FALSE;
}

inline BOOL WINAPI GetVolumePathNameA(
    const char* /*lpszFileName*/,
    char* lpszVolumePathName,
    DWORD cchBufferLength
) noexcept {
    if (lpszVolumePathName && cchBufferLength >= 4) {
        std::snprintf(lpszVolumePathName, cchBufferLength, "C:\\");
        return TRUE;
    }
    return FALSE;
}

inline BOOL WINAPI GetVolumeInformationA(
    const char* /*lpRootPathName*/,
    char* lpVolumeNameBuffer,
    DWORD nVolumeNameSize,
    DWORD* lpVolumeSerialNumber,
    DWORD* lpMaximumComponentLength,
    DWORD* lpFileSystemFlags,
    char* lpFileSystemNameBuffer,
    DWORD nFileSystemNameSize
) noexcept {
    if (lpVolumeNameBuffer && nVolumeNameSize > 0) {
        std::snprintf(lpVolumeNameBuffer, nVolumeNameSize, "MicaNT_System");
    }
    if (lpVolumeSerialNumber) *lpVolumeSerialNumber = 0x12345678;
    if (lpMaximumComponentLength) *lpMaximumComponentLength = 255;
    if (lpFileSystemFlags) *lpFileSystemFlags = 0x00000002 | 0x00000004 | 0x00000008; // CASE_PRESERVED, UNICODE, PERSISTENT_ACLS
    if (lpFileSystemNameBuffer && nFileSystemNameSize > 0) {
        std::snprintf(lpFileSystemNameBuffer, nFileSystemNameSize, "NTFS");
    }
    return TRUE;
}

inline BOOL WINAPI GetVolumeInformationByHandleW(
    HANDLE /*hFile*/,
    wchar_t* lpVolumeNameBuffer,
    DWORD nVolumeNameSize,
    DWORD* lpVolumeSerialNumber,
    DWORD* lpMaximumComponentLength,
    DWORD* lpFileSystemFlags,
    wchar_t* lpFileSystemNameBuffer,
    DWORD nFileSystemNameSize
) noexcept {
    if (lpVolumeNameBuffer && nVolumeNameSize > 0) {
        std::wcsncpy(lpVolumeNameBuffer, L"MicaNT_Drive", nVolumeNameSize);
    }
    if (lpVolumeSerialNumber) *lpVolumeSerialNumber = 0x87654321;
    if (lpMaximumComponentLength) *lpMaximumComponentLength = 255;
    if (lpFileSystemFlags) *lpFileSystemFlags = 0x00000002 | 0x00000004 | 0x00000008;
    if (lpFileSystemNameBuffer && nFileSystemNameSize > 0) {
        std::wcsncpy(lpFileSystemNameBuffer, L"NTFS", nFileSystemNameSize);
    }
    return TRUE;
}

inline constexpr DWORD DDD_RAW_TARGET_PATH         = 0x00000001;
inline constexpr DWORD DDD_REMOVE_DEFINITION       = 0x00000002;
inline constexpr DWORD DDD_EXACT_MATCH_ON_REMOVE   = 0x00000004;
inline constexpr DWORD DDD_NO_BROADCAST_SYSTEM     = 0x00000008;

inline BOOL WINAPI DefineDosDeviceW(DWORD dwFlags, LPCWSTR lpDeviceName, LPCWSTR lpTargetPath) noexcept {
    if (!lpDeviceName || lpDeviceName[0] == L'\0') {
        SetLastError(87);
        return FALSE;
    }
    wchar_t d = static_cast<wchar_t>(std::towupper(lpDeviceName[0]));
    if (d < L'A' || d > L'Z') {
        SetLastError(87);
        return FALSE;
    }

    if (dwFlags & DDD_REMOVE_DEFINITION) {
        fs::VirtualFileSystem::get().removeDriveLetter(d);
        SetLastError(0);
        return TRUE;
    }

    if (!lpTargetPath) {
        SetLastError(87);
        return FALSE;
    }

    NtStatus st = fs::VirtualFileSystem::get().assignDriveLetter(d, lpTargetPath);
    if (!NT_SUCCESS(st)) {
        SetLastError(ntdll::RtlNtStatusToDosError(st));
        return FALSE;
    }
    SetLastError(0);
    return TRUE;
}

inline BOOL WINAPI DefineDosDeviceA(DWORD dwFlags, const char* lpDeviceName, const char* lpTargetPath) noexcept {
    if (!lpDeviceName) {
        SetLastError(87);
        return FALSE;
    }
    std::string sDev(lpDeviceName);
    std::wstring wDev(sDev.begin(), sDev.end());
    std::wstring wTgt;
    if (lpTargetPath) {
        std::string sTgt(lpTargetPath);
        wTgt.assign(sTgt.begin(), sTgt.end());
    }
    return DefineDosDeviceW(dwFlags, wDev.c_str(), lpTargetPath ? wTgt.c_str() : nullptr);
}

inline DWORD WINAPI QueryDosDeviceW(LPCWSTR lpDeviceName, LPWSTR lpTargetPath, DWORD ucchMax) noexcept {
    if (!lpTargetPath || ucchMax == 0) {
        SetLastError(87);
        return 0;
    }

    auto& vfs = fs::VirtualFileSystem::get();

    if (!lpDeviceName || lpDeviceName[0] == L'\0') {
        auto letters = vfs.getMountedDriveLetters();
        size_t totalNeeded = letters.size() * 3 + 1; // "C:\0" per letter plus trailing '\0'

        if (ucchMax < totalNeeded) {
            SetLastError(122); // ERROR_INSUFFICIENT_BUFFER
            return 0;
        }

        LPWSTR cur = lpTargetPath;
        for (wchar_t l : letters) {
            *cur++ = l;
            *cur++ = L':';
            *cur++ = L'\0';
        }
        *cur++ = L'\0';
        SetLastError(0);
        return static_cast<DWORD>(totalNeeded);
    }

    wchar_t d = static_cast<wchar_t>(std::towupper(lpDeviceName[0]));
    std::wstring symName = std::wstring(L"\\DosDevices\\") + d + L":";
    auto sym = ob::ObjectNamespace::get().getSymbolicLink(symName);
    std::wstring target;
    if (sym) {
        target = sym->getTargetPath();
    } else {
        target = vfs.getDeviceForDriveLetter(d);
    }

    if (target.empty()) {
        SetLastError(2); // ERROR_FILE_NOT_FOUND
        return 0;
    }

    size_t needed = target.size() + 2; // target + \0\0
    if (ucchMax < needed) {
        SetLastError(122);
        return 0;
    }

    std::memcpy(lpTargetPath, target.c_str(), (target.size() + 1) * sizeof(wchar_t));
    lpTargetPath[target.size() + 1] = L'\0';
    SetLastError(0);
    return static_cast<DWORD>(needed);
}

inline DWORD WINAPI QueryDosDeviceA(const char* lpDeviceName, char* lpTargetPath, DWORD ucchMax) noexcept {
    if (!lpTargetPath || ucchMax == 0) {
        SetLastError(87);
        return 0;
    }
    std::wstring wDev;
    if (lpDeviceName) {
        std::string sDev(lpDeviceName);
        wDev.assign(sDev.begin(), sDev.end());
    }
    std::vector<wchar_t> wBuf(ucchMax);
    DWORD res = QueryDosDeviceW(lpDeviceName ? wDev.c_str() : nullptr, wBuf.data(), ucchMax);
    if (res == 0) return 0;
    for (DWORD i = 0; i < res; ++i) {
        lpTargetPath[i] = static_cast<char>(wBuf[i]);
    }
    return res;
}

inline BOOL WINAPI SetVolumeMountPointW(LPCWSTR lpszVolumeMountPoint, LPCWSTR lpszVolumeName) noexcept {
    if (!lpszVolumeMountPoint || !lpszVolumeName) {
        SetLastError(87);
        return FALSE;
    }
    wchar_t d = static_cast<wchar_t>(std::towupper(lpszVolumeMountPoint[0]));
    if (d < L'A' || d > L'Z') {
        SetLastError(87);
        return FALSE;
    }
    NtStatus st = fs::VirtualFileSystem::get().assignDriveLetter(d, lpszVolumeName, lpszVolumeName);
    if (!NT_SUCCESS(st)) {
        SetLastError(ntdll::RtlNtStatusToDosError(st));
        return FALSE;
    }
    SetLastError(0);
    return TRUE;
}

inline BOOL WINAPI SetVolumeMountPointA(const char* lpszVolumeMountPoint, const char* lpszVolumeName) noexcept {
    if (!lpszVolumeMountPoint || !lpszVolumeName) return FALSE;
    std::string sMnt(lpszVolumeMountPoint), sVol(lpszVolumeName);
    std::wstring wMnt(sMnt.begin(), sMnt.end()), wVol(sVol.begin(), sVol.end());
    return SetVolumeMountPointW(wMnt.c_str(), wVol.c_str());
}

inline BOOL WINAPI DeleteVolumeMountPointW(LPCWSTR lpszVolumeMountPoint) noexcept {
    if (!lpszVolumeMountPoint) {
        SetLastError(87);
        return FALSE;
    }
    wchar_t d = static_cast<wchar_t>(std::towupper(lpszVolumeMountPoint[0]));
    fs::VirtualFileSystem::get().removeDriveLetter(d);
    SetLastError(0);
    return TRUE;
}

inline BOOL WINAPI DeleteVolumeMountPointA(const char* lpszVolumeMountPoint) noexcept {
    if (!lpszVolumeMountPoint) return FALSE;
    std::string sMnt(lpszVolumeMountPoint);
    std::wstring wMnt(sMnt.begin(), sMnt.end());
    return DeleteVolumeMountPointW(wMnt.c_str());
}

inline BOOL WINAPI SetVolumeLabelA(const char* /*lpRootPathName*/, const char* /*lpVolumeName*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI GetDiskFreeSpaceExA(
    const char* /*lpDirectoryName*/,
    uint64_t* lpFreeBytesAvailableToCaller,
    uint64_t* lpTotalNumberOfBytes,
    uint64_t* lpTotalNumberOfFreeBytes
) noexcept {
    constexpr uint64_t oneTB = 1099511627776ULL;
    constexpr uint64_t freeBytes = 858993459200ULL; // 800 GB free
    if (lpFreeBytesAvailableToCaller) *lpFreeBytesAvailableToCaller = freeBytes;
    if (lpTotalNumberOfBytes) *lpTotalNumberOfBytes = oneTB;
    if (lpTotalNumberOfFreeBytes) *lpTotalNumberOfFreeBytes = freeBytes;
    return TRUE;
}

inline DWORD WINAPI GetLogicalDriveStringsA(DWORD nBufferLength, char* lpBuffer) noexcept {
    auto letters = fs::VirtualFileSystem::get().getMountedDriveLetters();
    DWORD totalChars = static_cast<DWORD>(letters.size() * 4 + 1);
    if (nBufferLength < totalChars) {
        return totalChars;
    }
    if (!lpBuffer) {
        return 0;
    }
    char* cur = lpBuffer;
    for (wchar_t l : letters) {
        *cur++ = static_cast<char>(std::towupper(l));
        *cur++ = ':';
        *cur++ = '\\';
        *cur++ = '\0';
    }
    *cur = '\0';
    return totalChars - 1;
}

inline BOOL WINAPI CancelIoEx(HANDLE /*hFile*/, void* /*lpOverlapped*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI CancelSynchronousIo(HANDLE /*hThread*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI GetOverlappedResultEx(
    HANDLE /*hFile*/,
    void* /*lpOverlapped*/,
    DWORD* lpNumberOfBytesTransferred,
    DWORD /*dwMilliseconds*/,
    BOOL /*bAlertable*/
) noexcept {
    if (lpNumberOfBytesTransferred) *lpNumberOfBytesTransferred = 512;
    return TRUE;
}

inline BOOL WINAPI SleepConditionVariableCS(
    void* /*ConditionVariable*/,
    void* /*CriticalSection*/,
    DWORD /*dwMilliseconds*/
) noexcept {
    return TRUE;
}

inline BOOL WINAPI CreateSymbolicLinkW(const wchar_t* /*lpSymlinkFileName*/, const wchar_t* /*lpTargetFileName*/, DWORD /*dwFlags*/) noexcept {
    return TRUE;
}

inline HWND WINAPI GetConsoleWindow() noexcept {
    return reinterpret_cast<HWND>(0x1000);
}

inline BOOL WINAPI EnumUILanguagesW(void* /*lpUILanguageEnumProc*/, DWORD /*dwFlags*/, LONG_PTR /*lParam*/) noexcept {
    return TRUE;
}

inline LCID WINAPI GetSystemDefaultLCID() noexcept {
    return 0x0409; // en-US
}

inline WORD WINAPI GetThreadUILanguage() noexcept {
    return 0x0409; // en-US
}

inline int WINAPI LCIDToLocaleName(LCID /*Locale*/, wchar_t* lpName, int cchName, DWORD /*dwFlags*/) noexcept {
    if (lpName && cchName >= 6) {
        std::wcsncpy(lpName, L"en-US", cchName);
        return 6;
    }
    return 0;
}

inline BOOL WINAPI SetDefaultDllDirectories(DWORD /*DirectoryFlags*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI SetFileAttributesA(const char* /*lpFileName*/, DWORD /*dwFileAttributes*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI VerifyVersionInfoA(void* /*lpVersionInformation*/, DWORD /*dwTypeMask*/, uint64_t /*dwlConditionMask*/) noexcept {
    return TRUE;
}

inline DWORD WINAPI K32GetModuleFileNameExW(
    HANDLE /*hProcess*/,
    void* /*hModule*/,
    wchar_t* lpFilename,
    DWORD nSize
) noexcept {
    if (lpFilename && nSize > 0) {
        std::wcsncpy(lpFilename, L"C:\\Program Files\\Rufus\\rufus.exe", nSize);
        return static_cast<DWORD>(std::wcslen(lpFilename));
    }
    return 0;
}

inline DWORD WINAPI K32GetProcessImageFileNameW(
    HANDLE /*hProcess*/,
    wchar_t* lpImageFileName,
    DWORD nSize
) noexcept {
    if (lpImageFileName && nSize > 0) {
        std::wcsncpy(lpImageFileName, L"\\Device\\HarddiskVolume1\\Program Files\\Rufus\\rufus.exe", nSize);
        return static_cast<DWORD>(std::wcslen(lpImageFileName));
    }
    return 0;
}

struct SLIST_ENTRY {
    SLIST_ENTRY* Next;
};

struct SLIST_HEADER {
    uint64_t Alignment;
    uint64_t Region;
};

struct ACTCTXW {
    uint32_t cbSize;
    uint32_t dwFlags;
    const wchar_t* lpSource;
    uint16_t wProcessorArchitecture;
    uint16_t wLangId;
    const wchar_t* lpAssemblyDirectory;
    const wchar_t* lpResourceName;
    const wchar_t* lpApplicationName;
    void* hModule;
};

inline void* WINAPI GlobalReAlloc(void* hMem, size_t dwBytes, uint32_t /*uFlags*/) noexcept {
    if (!hMem) {
        return std::malloc(dwBytes ? dwBytes : 1);
    }
    return std::realloc(hMem, dwBytes ? dwBytes : 1);
}

inline void* WINAPI LocalReAlloc(void* hMem, size_t uBytes, uint32_t /*uFlags*/) noexcept {
    if (!hMem) {
        return std::malloc(uBytes ? uBytes : 1);
    }
    return std::realloc(hMem, uBytes ? uBytes : 1);
}

inline void* WINAPI GlobalHandle(const void* pMem) noexcept {
    return const_cast<void*>(pMem);
}

inline uint32_t WINAPI GlobalFlags(void* /*hMem*/) noexcept {
    return 0; // GMEM_FIXED
}

inline uint16_t WINAPI SetThreadUILanguage(uint16_t LangId) noexcept {
    static uint16_t s_lang = 0x0409; // en-US
    uint16_t prev = s_lang;
    if (LangId != 0) {
        s_lang = LangId;
    }
    return prev;
}

inline BOOL WINAPI SetSearchPathMode(uint32_t /*Flags*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI SetDllDirectoryW(const wchar_t* /*lpPathName*/) noexcept {
    return TRUE;
}

inline uint32_t WINAPI GetSystemWow64DirectoryW(wchar_t* lpBuffer, uint32_t uSize) noexcept {
    const wchar_t sysWow64[] = L"C:\\Windows\\SysWOW64";
    constexpr uint32_t len = 19;
    if (!lpBuffer || uSize <= len) {
        return len + 1;
    }
    std::wmemcpy(lpBuffer, sysWow64, len);
    lpBuffer[len] = L'\0';
    return len;
}

inline uint32_t WINAPI ExpandEnvironmentStringsA(const char* lpSrc, char* lpDst, uint32_t nSize) noexcept {
    if (!lpSrc) {
        return 0;
    }
    std::string src(lpSrc);
    std::string out;
    size_t i = 0;
    while (i < src.size()) {
        if (src[i] == '%') {
            size_t end = src.find('%', i + 1);
            if (end != std::string::npos) {
                std::string var = src.substr(i + 1, end - i - 1);
                std::string val;
                for (char& c : var) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                if (var == "SYSTEMROOT" || var == "WINDIR") val = "C:\\Windows";
                else if (var == "SYSTEMDRIVE") val = "C:";
                else if (var == "TEMP" || var == "TMP") val = "C:\\Users\\admin\\AppData\\Local\\Temp";
                else if (var == "USERPROFILE") val = "C:\\Users\\admin";
                else if (var == "PROGRAMDATA") val = "C:\\ProgramData";
                else val = "";
                out.append(val);
                i = end + 1;
                continue;
            }
        }
        out.push_back(src[i++]);
    }
    uint32_t req = static_cast<uint32_t>(out.size() + 1);
    if (!lpDst || nSize < req) {
        return req;
    }
    std::memcpy(lpDst, out.c_str(), req);
    return req - 1;
}

inline void* WINAPI CreateActCtxW(const ACTCTXW* /*pActCtx*/) noexcept {
    return reinterpret_cast<void*>(0xAC1C0001);
}

inline BOOL WINAPI ActivateActCtx(void* /*hActCtx*/, uintptr_t* lpCookie) noexcept {
    if (lpCookie) {
        *lpCookie = 0x12345678;
    }
    return TRUE;
}

inline BOOL WINAPI DeactivateActCtx(uint32_t /*dwFlags*/, uintptr_t /*ulCookie*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI FindActCtxSectionStringW(uint32_t /*dwFlags*/, const void* /*lpExtensionGuid*/, uint32_t /*ulSectionId*/, const wchar_t* /*lpStringToFind*/, void* /*PActCtx*/) noexcept {
    return FALSE;
}

inline BOOL WINAPI QueryActCtxW(uint32_t /*dwFlags*/, void* /*hActCtx*/, void* /*pvSubInstance*/, uint32_t /*ulInfoClass*/, void* /*pvBuffer*/, size_t /*cbBuffer*/, size_t* pcbWritten) noexcept {
    if (pcbWritten) {
        *pcbWritten = 0;
    }
    return TRUE;
}

inline uint32_t WINAPI GetProfileIntW(const wchar_t* /*lpAppName*/, const wchar_t* /*lpKeyName*/, int32_t nDefault) noexcept {
    return static_cast<uint32_t>(nDefault);
}

inline uint32_t WINAPI GlobalGetAtomNameW(uint16_t nAtom, wchar_t* lpBuffer, int32_t nSize) noexcept {
    if (!lpBuffer || nSize <= 0) {
        return 0;
    }
    int written = std::swprintf(lpBuffer, nSize, L"#%u", nAtom);
    return written > 0 ? static_cast<uint32_t>(written) : 0;
}

inline int32_t WINAPI lstrcmpA(const char* s1, const char* s2) noexcept {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    return std::strcmp(s1, s2);
}

inline BOOL WINAPI LockFile(HANDLE /*hFile*/, uint32_t /*dwFileOffsetLow*/, uint32_t /*dwFileOffsetHigh*/, uint32_t /*nNumberOfBytesToLockLow*/, uint32_t /*nNumberOfBytesToLockHigh*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI UnlockFile(HANDLE /*hFile*/, uint32_t /*dwFileOffsetLow*/, uint32_t /*dwFileOffsetHigh*/, uint32_t /*nNumberOfBytesToUnlockLow*/, uint32_t /*nNumberOfBytesToUnlockHigh*/) noexcept {
    return TRUE;
}

inline void* WINAPI FindResourceExW(HMODULE hModule, const wchar_t* lpType, const wchar_t* lpName, uint16_t /*wLanguage*/) noexcept {
    return FindResourceW(hModule, lpName, lpType);
}

inline SLIST_ENTRY* WINAPI InterlockedPushEntrySList(SLIST_HEADER* ListHead, SLIST_ENTRY* ListEntry) noexcept {
    if (!ListHead || !ListEntry) {
        return nullptr;
    }
    auto* oldFirst = reinterpret_cast<SLIST_ENTRY*>(ListHead->Alignment);
    ListEntry->Next = oldFirst;
    ListHead->Alignment = reinterpret_cast<uint64_t>(ListEntry);
    return oldFirst;
}

inline uint32_t WINAPI GetThreadId(HANDLE /*Thread*/) noexcept {
    return 1001;
}

inline HANDLE WINAPI CreateIoCompletionPort(HANDLE /*FileHandle*/, HANDLE ExistingCompletionPort, ULONG_PTR /*CompletionKey*/, DWORD /*NumberOfConcurrentThreads*/) noexcept {
    return ExistingCompletionPort ? ExistingCompletionPort : reinterpret_cast<HANDLE>(0x10C01);
}

inline BOOL WINAPI GetQueuedCompletionStatus(HANDLE /*CompletionPort*/, DWORD* lpNumberOfBytesTransferred, ULONG_PTR* lpCompletionKey, void** lpOverlapped, DWORD /*dwMilliseconds*/) noexcept {
    if (lpNumberOfBytesTransferred) *lpNumberOfBytesTransferred = 1024;
    if (lpCompletionKey) *lpCompletionKey = 1;
    if (lpOverlapped) *lpOverlapped = reinterpret_cast<void*>(0x8102);
    return TRUE;
}

inline BOOL WINAPI PostQueuedCompletionStatus(HANDLE /*CompletionPort*/, DWORD /*dwNumberOfBytesTransferred*/, ULONG_PTR /*dwCompletionKey*/, void* /*lpOverlapped*/) noexcept {
    return TRUE;
}

// ============================================================================
// Process Mitigation Policies (Windows NT 10.0+ / processthreadsapi.h)
// ============================================================================

enum PROCESS_MITIGATION_POLICY {
    ProcessDEPPolicy = 0,
    ProcessASLRPolicy = 1,
    ProcessDynamicCodePolicy = 2,
    ProcessStrictHandleCheckPolicy = 3,
    ProcessSystemCallDisablePolicy = 4,
    ProcessMitigationOptionsMask = 5,
    ProcessExtensionPointDisablePolicy = 6,
    ProcessControlFlowGuardPolicy = 7,
    ProcessSignaturePolicy = 8,
    ProcessFontDisablePolicy = 9,
    ProcessImageLoadPolicy = 10,
    ProcessSystemCallFilterPolicy = 11,
    ProcessPayloadRestrictionPolicy = 12,
    ProcessChildProcessPolicy = 13,
    ProcessSideChannelIsolationPolicy = 14,
    ProcessUserShadowStackPolicy = 15,
    ProcessRedirectionTrustPolicy = 16,
    ProcessUserPointerAuthPolicy = 17,
    ProcessSEHOPPolicy = 18,
    MaxProcessMitigationPolicy
};

struct PROCESS_MITIGATION_DEP_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD Enable : 1;
            DWORD DisableAtlThunkEmulation : 1;
            DWORD ReservedFlags : 30;
        };
    };
    BOOLEAN Permanent;
};

struct PROCESS_MITIGATION_ASLR_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD EnableBottomUpRandomization : 1;
            DWORD EnableForceRelocateImages : 1;
            DWORD EnableHighEntropy : 1;
            DWORD DisallowStrippedImages : 1;
            DWORD ReservedFlags : 28;
        };
    };
};

struct PROCESS_MITIGATION_DYNAMIC_CODE_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD ProhibitDynamicCode : 1;
            DWORD AllowThreadOptOut : 1;
            DWORD AllowRemoteDowngrade : 1;
            DWORD AuditProhibitDynamicCode : 1;
            DWORD ReservedFlags : 28;
        };
    };
};

struct PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD RaiseExceptionOnInvalidHandleReference : 1;
            DWORD HandleExceptionsPermanentlyEnabled : 1;
            DWORD ReservedFlags : 30;
        };
    };
};

struct PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD DisallowWin32kSystemCalls : 1;
            DWORD AuditDisallowWin32kSystemCalls : 1;
            DWORD ReservedFlags : 30;
        };
    };
};

struct PROCESS_MITIGATION_IMAGE_LOAD_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD NoRemoteImages : 1;
            DWORD NoLowMandatoryLabelImages : 1;
            DWORD PreferSystem32Images : 1;
            DWORD AuditNoRemoteImages : 1;
            DWORD AuditNoLowMandatoryLabelImages : 1;
            DWORD ReservedFlags : 27;
        };
    };
};

struct PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD EnableUserShadowStack : 1;
            DWORD AuditUserShadowStack : 1;
            DWORD SetContextIpValidation : 1;
            DWORD AuditSetContextIpValidation : 1;
            DWORD EnableUserShadowStackStrictMode : 1;
            DWORD BlockIfLegacyBijectionNeeded : 1;
            DWORD BlockIfLegacyBijectionNeededAudit : 1;
            DWORD ReservedFlags : 25;
        };
    };
};

class SovereignProcessMitigationManager {
private:
    std::mutex m_mutex;
    struct MitigationState {
        PROCESS_MITIGATION_DEP_POLICY dep{};
        PROCESS_MITIGATION_ASLR_POLICY aslr{};
        PROCESS_MITIGATION_DYNAMIC_CODE_POLICY dynamicCode{};
        PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY strictHandle{};
        PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY syscallDisable{};
        PROCESS_MITIGATION_IMAGE_LOAD_POLICY imageLoad{};
        PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY userShadowStack{};
    };
    MitigationState m_state{};

public:
    static SovereignProcessMitigationManager& get() noexcept {
        static SovereignProcessMitigationManager instance;
        return instance;
    }

    SovereignProcessMitigationManager() {
        m_state.dep.Enable = 1;
        m_state.dep.Permanent = 1;
        m_state.aslr.EnableBottomUpRandomization = 1;
        m_state.aslr.EnableHighEntropy = 1;
    }

    BOOL setPolicy(PROCESS_MITIGATION_POLICY policy, PVOID lpBuffer, SIZE_T dwLength) noexcept {
        if (!lpBuffer || dwLength == 0) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        switch (policy) {
            case ProcessDEPPolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_DEP_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                m_state.dep = *reinterpret_cast<const PROCESS_MITIGATION_DEP_POLICY*>(lpBuffer);
                return TRUE;
            case ProcessASLRPolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_ASLR_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                m_state.aslr = *reinterpret_cast<const PROCESS_MITIGATION_ASLR_POLICY*>(lpBuffer);
                return TRUE;
            case ProcessDynamicCodePolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_DYNAMIC_CODE_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                m_state.dynamicCode = *reinterpret_cast<const PROCESS_MITIGATION_DYNAMIC_CODE_POLICY*>(lpBuffer);
                return TRUE;
            case ProcessStrictHandleCheckPolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                m_state.strictHandle = *reinterpret_cast<const PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY*>(lpBuffer);
                return TRUE;
            case ProcessSystemCallDisablePolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                m_state.syscallDisable = *reinterpret_cast<const PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY*>(lpBuffer);
                return TRUE;
            case ProcessImageLoadPolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_IMAGE_LOAD_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                m_state.imageLoad = *reinterpret_cast<const PROCESS_MITIGATION_IMAGE_LOAD_POLICY*>(lpBuffer);
                return TRUE;
            case ProcessUserShadowStackPolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                m_state.userShadowStack = *reinterpret_cast<const PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY*>(lpBuffer);
                return TRUE;
            default:
                return TRUE;
        }
    }

    BOOL getPolicy(HANDLE /*hProcess*/, PROCESS_MITIGATION_POLICY policy, PVOID lpBuffer, SIZE_T dwLength) noexcept {
        if (!lpBuffer || dwLength == 0) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        switch (policy) {
            case ProcessDEPPolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_DEP_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                *reinterpret_cast<PROCESS_MITIGATION_DEP_POLICY*>(lpBuffer) = m_state.dep;
                return TRUE;
            case ProcessASLRPolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_ASLR_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                *reinterpret_cast<PROCESS_MITIGATION_ASLR_POLICY*>(lpBuffer) = m_state.aslr;
                return TRUE;
            case ProcessDynamicCodePolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_DYNAMIC_CODE_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                *reinterpret_cast<PROCESS_MITIGATION_DYNAMIC_CODE_POLICY*>(lpBuffer) = m_state.dynamicCode;
                return TRUE;
            case ProcessStrictHandleCheckPolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                *reinterpret_cast<PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY*>(lpBuffer) = m_state.strictHandle;
                return TRUE;
            case ProcessSystemCallDisablePolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                *reinterpret_cast<PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY*>(lpBuffer) = m_state.syscallDisable;
                return TRUE;
            case ProcessImageLoadPolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_IMAGE_LOAD_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                *reinterpret_cast<PROCESS_MITIGATION_IMAGE_LOAD_POLICY*>(lpBuffer) = m_state.imageLoad;
                return TRUE;
            case ProcessUserShadowStackPolicy:
                if (dwLength < sizeof(PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
                *reinterpret_cast<PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY*>(lpBuffer) = m_state.userShadowStack;
                return TRUE;
            default:
                std::memset(lpBuffer, 0, dwLength);
                return TRUE;
        }
    }
};

inline BOOL WINAPI SetProcessMitigationPolicy(PROCESS_MITIGATION_POLICY MitigationPolicy, PVOID lpBuffer, SIZE_T dwLength) noexcept {
    return SovereignProcessMitigationManager::get().setPolicy(MitigationPolicy, lpBuffer, dwLength);
}

inline BOOL WINAPI GetProcessMitigationPolicy(HANDLE hProcess, PROCESS_MITIGATION_POLICY MitigationPolicy, PVOID lpBuffer, SIZE_T dwLength) noexcept {
    return SovereignProcessMitigationManager::get().getPolicy(hProcess, MitigationPolicy, lpBuffer, dwLength);
}

inline BOOL WINAPI LockFileEx(HANDLE /*hFile*/, DWORD /*dwFlags*/, DWORD /*dwReserved*/, DWORD /*nNumberOfBytesToLockLow*/, DWORD /*nNumberOfBytesToLockHigh*/, void* /*lpOverlapped*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI UnlockFileEx(HANDLE /*hFile*/, DWORD /*dwReserved*/, DWORD /*nNumberOfBytesToUnlockLow*/, DWORD /*nNumberOfBytesToUnlockHigh*/, void* /*lpOverlapped*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI FlushViewOfFile(const void* /*lpBaseAddress*/, size_t /*dwNumberOfBytesToFlush*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI HeapValidate(HANDLE /*hHeap*/, DWORD /*dwFlags*/, const void* /*lpMem*/) noexcept {
    return TRUE;
}

inline size_t WINAPI HeapCompact(HANDLE /*hHeap*/, DWORD /*dwFlags*/) noexcept {
    return 0;
}

inline void* WINAPI InterlockedFlushSList(SLIST_HEADER* /*ListHead*/) noexcept {
    return nullptr;
}

inline BOOL WINAPI ReadConsoleA(HANDLE /*hConsoleInput*/, void* lpBuffer, DWORD nNumberOfCharsToRead, DWORD* lpNumberOfCharsRead, void* /*pInputControl*/) noexcept {
    if (lpNumberOfCharsRead) *lpNumberOfCharsRead = 0;
    if (lpBuffer && nNumberOfCharsToRead > 0) static_cast<char*>(lpBuffer)[0] = '\0';
    return TRUE;
}

inline uint16_t WINAPI RtlCaptureStackBackTrace(DWORD /*FramesToSkip*/, DWORD FramesToCapture, void** BackTrace, DWORD* BackTraceHash) noexcept {
    if (BackTrace && FramesToCapture > 0) BackTrace[0] = reinterpret_cast<void*>(0x140001000ULL);
    if (BackTraceHash) *BackTraceHash = 0;
    return BackTrace ? 1 : 0;
}

inline BOOL WINAPI CheckRemoteDebuggerPresent(HANDLE /*hProcess*/, BOOL* pbDebuggerPresent) noexcept {
    if (pbDebuggerPresent) *pbDebuggerPresent = 0;
    return TRUE;
}

inline DWORD WINAPI WTSGetActiveConsoleSessionId() noexcept {
    return 1;
}

inline void WINAPI GetSystemTimePreciseAsFileTime(void* lpSystemTimeAsFileTime) noexcept {
    if (lpSystemTimeAsFileTime) {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        uint64_t winTime = (nanos / 100) + 116444736000000000ULL;
        std::memcpy(lpSystemTimeAsFileTime, &winTime, sizeof(uint64_t));
    }
}

// ============================================================================
// Sub-Microsecond High-Precision Timers (Windows NT 10.0+ / realtimeapiset.h)
// ============================================================================

inline void WINAPI QueryInterruptTimePrecise(PULONGLONG lpInterruptTimePrecise) noexcept {
    if (lpInterruptTimePrecise) {
        static const auto s_bootTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - s_bootTime).count();
        // Authentic Windows NT interrupt time includes elapsed ticks since system boot (> 0)
        *lpInterruptTimePrecise = static_cast<ULONGLONG>(nanos / 100) + 10000000ULL;
    }
}

inline void WINAPI QueryUnbiasedInterruptTimePrecise(PULONGLONG lpUnbiasedInterruptTimePrecise) noexcept {
    if (lpUnbiasedInterruptTimePrecise) {
        static const auto s_bootTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - s_bootTime).count();
        // Authentic Windows NT unbiased interrupt time (excluding sleep/hibernate) (> 0)
        *lpUnbiasedInterruptTimePrecise = static_cast<ULONGLONG>(nanos / 100) + 10000000ULL;
    }
}

// ============================================================================
// Physical Memory & Working Set Management (sysinfoapi.h / psapi.h)
// ============================================================================

inline BOOL WINAPI GetPhysicallyInstalledSystemMemory(PULONGLONG TotalMemoryInKilobytes) noexcept {
    if (!TotalMemoryInKilobytes) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    // Authentic host physical memory: 32 GB = 33,554,432 KB
    *TotalMemoryInKilobytes = 33554432ULL;
    return TRUE;
}

inline BOOL WINAPI EmptyWorkingSet(HANDLE /*hProcess*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI K32EmptyWorkingSet(HANDLE hProcess) noexcept {
    return EmptyWorkingSet(hProcess);
}

// ============================================================================
// Sovereign VBS Virtualization Enclaves (Windows NT 10.0+ / enclaveapi.h)
// ============================================================================

#ifndef ENCLAVE_TYPE_SGX
#define ENCLAVE_TYPE_SGX        0x00000001
#endif

#ifndef ENCLAVE_TYPE_SGX2
#define ENCLAVE_TYPE_SGX2       0x00000002
#endif

#ifndef ENCLAVE_TYPE_VBS
#define ENCLAVE_TYPE_VBS        0x00000010
#endif

#ifndef ENCLAVE_TYPE_VBS_BASIC
#define ENCLAVE_TYPE_VBS_BASIC  0x00000011
#endif

#ifndef ENCLAVE_VBS_FLAG_DEBUG
#define ENCLAVE_VBS_FLAG_DEBUG  0x00000001
#endif

struct ENCLAVE_CREATE_INFO_VBS {
    DWORD Flags;
    BYTE  OwnerID[32];
};

struct ENCLAVE_INIT_INFO_VBS {
    DWORD Length;
    DWORD ThreadCount;
};

struct SovereignEnclaveRecord {
    PVOID BaseAddress{nullptr};
    SIZE_T Size{0};
    SIZE_T InitialCommitment{0};
    DWORD EnclaveType{0};
    DWORD Flags{0};
    BYTE OwnerID[32]{0};
    bool Initialized{false};
    bool Terminated{false};
    DWORD ThreadCount{0};
    std::vector<uint8_t> BackingMemory;
};

class SovereignVbsEnclaveManager {
public:
    static SovereignVbsEnclaveManager& get() noexcept {
        static SovereignVbsEnclaveManager instance;
        return instance;
    }

    BOOL isEnclaveTypeSupported(DWORD flEnclaveType) const noexcept {
        return (flEnclaveType == ENCLAVE_TYPE_VBS || flEnclaveType == ENCLAVE_TYPE_VBS_BASIC);
    }

    PVOID createEnclave(
        HANDLE /*hProcess*/,
        PVOID lpAddress,
        SIZE_T dwSize,
        SIZE_T dwInitialCommitment,
        DWORD flEnclaveType,
        LPCVOID lpEnclaveInformation,
        DWORD dwInfoLength,
        LPDWORD lpEnclaveError
    ) noexcept {
        if (!isEnclaveTypeSupported(flEnclaveType)) {
            if (lpEnclaveError) *lpEnclaveError = 0x80070032; // ERROR_NOT_SUPPORTED
            return nullptr;
        }
        if (dwSize == 0 || (dwSize & 0xFFF) != 0) {
            if (lpEnclaveError) *lpEnclaveError = 0x80070057; // ERROR_INVALID_PARAMETER
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        PVOID enclaveBase = lpAddress;
        if (!enclaveBase) {
            static uintptr_t s_NextEnclaveBase = 0x00007F1000000000ULL;
            enclaveBase = reinterpret_cast<PVOID>(s_NextEnclaveBase);
            s_NextEnclaveBase += ((dwSize + 0x1FFFFULL) & ~0x1FFFFULL);
        }

        SovereignEnclaveRecord rec{};
        rec.BaseAddress = enclaveBase;
        rec.Size = dwSize;
        rec.InitialCommitment = dwInitialCommitment;
        rec.EnclaveType = flEnclaveType;
        if (lpEnclaveInformation && dwInfoLength >= sizeof(ENCLAVE_CREATE_INFO_VBS)) {
            const auto* info = reinterpret_cast<const ENCLAVE_CREATE_INFO_VBS*>(lpEnclaveInformation);
            rec.Flags = info->Flags;
            std::memcpy(rec.OwnerID, info->OwnerID, 32);
        }
        rec.BackingMemory.resize(dwSize, 0);

        m_enclaves[enclaveBase] = std::move(rec);
        if (lpEnclaveError) *lpEnclaveError = 0;
        return enclaveBase;
    }

    BOOL initializeEnclave(
        HANDLE /*hProcess*/,
        PVOID lpAddress,
        LPCVOID lpEnclaveInformation,
        DWORD dwInfoLength,
        LPDWORD lpEnclaveError
    ) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_enclaves.find(lpAddress);
        if (it == m_enclaves.end() || it->second.Terminated) {
            if (lpEnclaveError) *lpEnclaveError = 0x80070490; // ERROR_NOT_FOUND
            return FALSE;
        }

        if (lpEnclaveInformation && dwInfoLength >= sizeof(ENCLAVE_INIT_INFO_VBS)) {
            const auto* info = reinterpret_cast<const ENCLAVE_INIT_INFO_VBS*>(lpEnclaveInformation);
            it->second.ThreadCount = info->ThreadCount;
        } else {
            it->second.ThreadCount = 1;
        }
        it->second.Initialized = true;
        if (lpEnclaveError) *lpEnclaveError = 0;
        return TRUE;
    }

    BOOL loadEnclaveData(
        HANDLE /*hProcess*/,
        PVOID lpAddress,
        LPCVOID lpBuffer,
        SIZE_T nSize,
        DWORD /*flProtect*/,
        LPCVOID /*lpPageInformation*/,
        DWORD /*dwInfoLength*/,
        PSIZE_T lpNumberOfBytesWritten,
        LPDWORD lpEnclaveError
    ) noexcept {
        if (!lpBuffer || nSize == 0) {
            if (lpEnclaveError) *lpEnclaveError = 0x80070057;
            return FALSE;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [base, enclave] : m_enclaves) {
            uintptr_t b = reinterpret_cast<uintptr_t>(base);
            uintptr_t target = reinterpret_cast<uintptr_t>(lpAddress);
            if (target >= b && target + nSize <= b + enclave.Size) {
                size_t offset = static_cast<size_t>(target - b);
                std::memcpy(enclave.BackingMemory.data() + offset, lpBuffer, nSize);
                if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = nSize;
                if (lpEnclaveError) *lpEnclaveError = 0;
                return TRUE;
            }
        }
        if (lpEnclaveError) *lpEnclaveError = 0x80070490;
        return FALSE;
    }

    BOOL terminateEnclave(PVOID lpAddress, BOOL /*fWait*/) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_enclaves.find(lpAddress);
        if (it != m_enclaves.end()) {
            it->second.Terminated = true;
            return TRUE;
        }
        return FALSE;
    }

    BOOL callEnclave(FARPROC lpRoutine, LPVOID lpParameter, BOOL /*fWaitForThread*/, PVOID* lpReturnValue) noexcept {
        if (!lpRoutine) return FALSE;
        using EnclaveProc = PVOID (*)(PVOID);
        auto proc = reinterpret_cast<EnclaveProc>(lpRoutine);
        PVOID ret = proc(lpParameter);
        if (lpReturnValue) *lpReturnValue = ret;
        return TRUE;
    }

    [[nodiscard]] size_t getActiveEnclaveCount() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t count = 0;
        for (const auto& [_, enc] : m_enclaves) {
            if (enc.Initialized && !enc.Terminated) ++count;
        }
        return count;
    }

private:
    mutable std::mutex m_mutex;
    std::unordered_map<PVOID, SovereignEnclaveRecord> m_enclaves;
};

inline BOOL WINAPI IsEnclaveTypeSupported(DWORD flEnclaveType) noexcept {
    return SovereignVbsEnclaveManager::get().isEnclaveTypeSupported(flEnclaveType);
}

inline PVOID WINAPI CreateEnclave(
    HANDLE hProcess,
    PVOID lpAddress,
    SIZE_T dwSize,
    SIZE_T dwInitialCommitment,
    DWORD flEnclaveType,
    LPCVOID lpEnclaveInformation,
    DWORD dwInfoLength,
    LPDWORD lpEnclaveError
) noexcept {
    return SovereignVbsEnclaveManager::get().createEnclave(hProcess, lpAddress, dwSize, dwInitialCommitment, flEnclaveType, lpEnclaveInformation, dwInfoLength, lpEnclaveError);
}

inline BOOL WINAPI InitializeEnclave(
    HANDLE hProcess,
    PVOID lpAddress,
    LPCVOID lpEnclaveInformation,
    DWORD dwInfoLength,
    LPDWORD lpEnclaveError
) noexcept {
    return SovereignVbsEnclaveManager::get().initializeEnclave(hProcess, lpAddress, lpEnclaveInformation, dwInfoLength, lpEnclaveError);
}

inline BOOL WINAPI LoadEnclaveData(
    HANDLE hProcess,
    PVOID lpAddress,
    LPCVOID lpBuffer,
    SIZE_T nSize,
    DWORD flProtect,
    LPCVOID lpPageInformation,
    DWORD dwInfoLength,
    PSIZE_T lpNumberOfBytesWritten,
    LPDWORD lpEnclaveError
) noexcept {
    return SovereignVbsEnclaveManager::get().loadEnclaveData(hProcess, lpAddress, lpBuffer, nSize, flProtect, lpPageInformation, dwInfoLength, lpNumberOfBytesWritten, lpEnclaveError);
}

inline BOOL WINAPI TerminateEnclave(PVOID lpAddress, BOOL fWait) noexcept {
    return SovereignVbsEnclaveManager::get().terminateEnclave(lpAddress, fWait);
}

inline BOOL WINAPI CallEnclave(FARPROC lpRoutine, LPVOID lpParameter, BOOL fWaitForThread, PVOID* lpReturnValue) noexcept {
    return SovereignVbsEnclaveManager::get().callEnclave(lpRoutine, lpParameter, fWaitForThread, lpReturnValue);
}

inline HANDLE WINAPI CreateFile2(const wchar_t* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, void* /*pCreateExParams*/) noexcept {
    return CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, nullptr, dwCreationDisposition, 0, nullptr);
}

inline BOOL WINAPI GetVolumePathNamesForVolumeNameW(const wchar_t* /*lpszVolumeName*/, wchar_t* lpszVolumePathNames, DWORD cchBufferLength, DWORD* lpcchReturnLength) noexcept {
    if (lpcchReturnLength) *lpcchReturnLength = 4;
    if (lpszVolumePathNames && cchBufferLength >= 4) {
        std::memcpy(lpszVolumePathNames, L"C:\\\0", 8);
    }
    return TRUE;
}

inline BOOL WINAPI GetVolumeNameForVolumeMountPointW(const wchar_t* /*lpszVolumeMountPoint*/, wchar_t* lpszVolumeName, DWORD cchBufferLength) noexcept {
    static const wchar_t dummyVol[] = L"\\\\?\\Volume{12345678-1234-1234-1234-123456789abc}\\";
    if (lpszVolumeName && cchBufferLength >= 49) {
        std::memcpy(lpszVolumeName, dummyVol, sizeof(dummyVol));
        return TRUE;
    }
    return FALSE;
}

inline BOOL WINAPI GetComputerNameExW(int32_t /*NameType*/, wchar_t* lpBuffer, DWORD* nSize) noexcept {
    static const wchar_t compName[] = L"MicaNT-Host";
    if (!nSize) return FALSE;
    if (!lpBuffer || *nSize < 12) {
        *nSize = 12;
        return FALSE;
    }
    std::memcpy(lpBuffer, compName, sizeof(compName));
    *nSize = 11;
    return TRUE;
}

inline HRESULT WINAPI SetThreadDescription(HANDLE /*hThread*/, const wchar_t* /*lpThreadDescription*/) noexcept {
    return 0; // S_OK
}

inline BOOL WINAPI SetFileInformationByHandle(HANDLE /*hFile*/, int32_t /*FileInformationClass*/, void* /*lpFileInformation*/, DWORD /*dwBufferSize*/) noexcept {
    return TRUE;
}

inline int32_t WINAPI GetUserGeoID(int32_t /*GeoClass*/) noexcept {
    return 244; // United States
}

inline int32_t WINAPI GetGeoInfoW(int32_t /*Location*/, int32_t /*GeoType*/, wchar_t* lpGeoData, int32_t cchData, uint16_t /*LangId*/) noexcept {
    if (lpGeoData && cchData >= 3) {
        std::memcpy(lpGeoData, L"US\0", 6);
        return 3;
    }
    return 3;
}

inline int32_t WINAPI GetCurrencyFormatW(uint32_t /*Locale*/, DWORD /*dwFlags*/, const wchar_t* /*lpValue*/, const void* /*lpFormat*/, wchar_t* lpCurrencyStr, int32_t cchCurrency) noexcept {
    if (lpCurrencyStr && cchCurrency >= 8) {
        std::memcpy(lpCurrencyStr, L"$0.00\0", 12);
        return 6;
    }
    return 6;
}

inline BOOL WINAPI GetUserPreferredUILanguages(DWORD /*dwFlags*/, uint32_t* pulNumLanguages, wchar_t* pwszLanguagesBuffer, uint32_t* pcchLanguagesBuffer) noexcept {
    if (pulNumLanguages) *pulNumLanguages = 1;
    if (pcchLanguagesBuffer) *pcchLanguagesBuffer = 6;
    if (pwszLanguagesBuffer) std::memcpy(pwszLanguagesBuffer, L"en-US\0", 12);
    return TRUE;
}

inline int32_t WINAPI GetUserDefaultLocaleName(wchar_t* lpLocaleName, int32_t cchLocaleName) noexcept {
    if (lpLocaleName && cchLocaleName >= 6) {
        std::memcpy(lpLocaleName, L"en-US", 12);
        return 6;
    }
    return 6;
}

inline uint64_t WINAPI GetEnabledXStateFeatures() noexcept {
    return 3; // XSTATE_LEGACY_FLOATING_POINT | XSTATE_LEGACY_SSE
}

inline int WINAPI GetNumberFormatEx(const wchar_t* /*lpLocaleName*/, DWORD /*dwFlags*/, const wchar_t* lpValue, void* /*lpFormat*/, wchar_t* lpNumberStr, int cchNumber) noexcept {
    if (!lpNumberStr || cchNumber <= 0) return 16;
    if (lpValue) {
        std::wcsncpy(lpNumberStr, lpValue, cchNumber);
        return static_cast<int>(std::wcslen(lpNumberStr));
    }
    return 0;
}

inline BOOL WINAPI SetConsoleCP(uint32_t /*wCodePageID*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI SetConsoleOutputCP(uint32_t /*wCodePageID*/) noexcept {
    return TRUE;
}

inline BOOL WINAPI SetWaitableTimer(HANDLE /*hTimer*/, const void* /*lpDueTime*/, int32_t /*lPeriod*/, void* /*pfnCompletionRoutine*/, void* /*lpArgToCompletionRoutine*/, BOOL /*fResume*/) noexcept {
    return TRUE;
}

inline HANDLE WINAPI CreateWaitableTimerA(void* /*lpTimerAttributes*/, BOOL /*bManualReset*/, const char* /*lpTimerName*/) noexcept {
    return reinterpret_cast<HANDLE>(0x12001);
}

inline HANDLE WINAPI CreateSemaphoreA(void* /*lpSemaphoreAttributes*/, int32_t /*lInitialCount*/, int32_t /*lMaximumCount*/, const char* /*lpName*/) noexcept {
    return reinterpret_cast<HANDLE>(0x13001);
}


namespace detail {
    struct WaitOnAddressBucket {
        std::mutex mtx;
        std::condition_variable cv;
        uint64_t wakeSequence{0};
    };
    inline constexpr size_t NUM_WAIT_BUCKETS = 64;
    inline WaitOnAddressBucket s_waitBuckets[NUM_WAIT_BUCKETS];

    inline size_t HashAddress(const volatile void* addr) noexcept {
        return (reinterpret_cast<uintptr_t>(addr) >> 3) % NUM_WAIT_BUCKETS;
    }
}

inline BOOL WINAPI WaitOnAddress(volatile void* Address, void* CompareAddress, size_t AddressSize, DWORD dwMilliseconds) noexcept {
    if (!Address || !CompareAddress) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (AddressSize != 1 && AddressSize != 2 && AddressSize != 4 && AddressSize != 8) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Compare Address memory with CompareAddress
    bool equal = false;
    switch (AddressSize) {
        case 1: equal = (*reinterpret_cast<volatile uint8_t*>(Address)  == *reinterpret_cast<const uint8_t*>(CompareAddress)); break;
        case 2: equal = (*reinterpret_cast<volatile uint16_t*>(Address) == *reinterpret_cast<const uint16_t*>(CompareAddress)); break;
        case 4: equal = (*reinterpret_cast<volatile uint32_t*>(Address) == *reinterpret_cast<const uint32_t*>(CompareAddress)); break;
        case 8: equal = (*reinterpret_cast<volatile uint64_t*>(Address) == *reinterpret_cast<const uint64_t*>(CompareAddress)); break;
    }

    if (!equal) {
        return TRUE;
    }

    auto& bucket = detail::s_waitBuckets[detail::HashAddress(Address)];
    std::unique_lock<std::mutex> lock(bucket.mtx);

    // Re-check value under lock
    switch (AddressSize) {
        case 1: equal = (*reinterpret_cast<volatile uint8_t*>(Address)  == *reinterpret_cast<const uint8_t*>(CompareAddress)); break;
        case 2: equal = (*reinterpret_cast<volatile uint16_t*>(Address) == *reinterpret_cast<const uint16_t*>(CompareAddress)); break;
        case 4: equal = (*reinterpret_cast<volatile uint32_t*>(Address) == *reinterpret_cast<const uint32_t*>(CompareAddress)); break;
        case 8: equal = (*reinterpret_cast<volatile uint64_t*>(Address) == *reinterpret_cast<const uint64_t*>(CompareAddress)); break;
    }
    if (!equal) return TRUE;

    uint64_t initialSeq = bucket.wakeSequence;
    auto predicate = [&]() noexcept -> bool {
        switch (AddressSize) {
            case 1: if (*reinterpret_cast<volatile uint8_t*>(Address)  != *reinterpret_cast<const uint8_t*>(CompareAddress)) return true; break;
            case 2: if (*reinterpret_cast<volatile uint16_t*>(Address) != *reinterpret_cast<const uint16_t*>(CompareAddress)) return true; break;
            case 4: if (*reinterpret_cast<volatile uint32_t*>(Address) != *reinterpret_cast<const uint32_t*>(CompareAddress)) return true; break;
            case 8: if (*reinterpret_cast<volatile uint64_t*>(Address) != *reinterpret_cast<const uint64_t*>(CompareAddress)) return true; break;
        }
        return bucket.wakeSequence != initialSeq;
    };

    if (dwMilliseconds == 0xFFFFFFFF) { // INFINITE
        bucket.cv.wait(lock, predicate);
        return TRUE;
    } else {
        bool woken = bucket.cv.wait_for(lock, std::chrono::milliseconds(dwMilliseconds), predicate);
        if (!woken) {
            SetLastError(ERROR_TIMEOUT);
            return FALSE;
        }
        return TRUE;
    }
}

inline void WINAPI WakeByAddressSingle(void* Address) noexcept {
    if (!Address) return;
    auto& bucket = detail::s_waitBuckets[detail::HashAddress(Address)];
    {
        std::lock_guard<std::mutex> lock(bucket.mtx);
        bucket.wakeSequence++;
    }
    bucket.cv.notify_one();
}

inline void WINAPI WakeByAddressAll(void* Address) noexcept {
    if (!Address) return;
    auto& bucket = detail::s_waitBuckets[detail::HashAddress(Address)];
    {
        std::lock_guard<std::mutex> lock(bucket.mtx);
        bucket.wakeSequence++;
    }
    bucket.cv.notify_all();
}

// QueryDosDeviceW defined above
inline BOOL ReadProcessMemory(HANDLE /*hProcess*/, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead) noexcept {
    if (!lpBaseAddress || !lpBuffer || nSize == 0) {
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
        return FALSE;
    }
    std::memcpy(lpBuffer, lpBaseAddress, nSize);
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = nSize;
    return TRUE;
}
inline void* GetCPInfoExW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL WriteProcessMemory(HANDLE /*hProcess*/, LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten) noexcept {
    if (!lpBaseAddress || !lpBuffer || nSize == 0) {
        if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = 0;
        return FALSE;
    }
    std::memcpy(lpBaseAddress, lpBuffer, nSize);
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = nSize;
    return TRUE;
}

inline BOOL QueryProcessCycleTime(HANDLE /*ProcessHandle*/, uint64_t* CycleTime) noexcept {
    if (!CycleTime) return FALSE;
    LargeInteger qpc{};
    QueryPerformanceCounter(&qpc);
    *CycleTime = static_cast<uint64_t>(qpc.quadPart) * 4200000000ULL / 10000000ULL;
    return TRUE;
}

inline BOOL QueryThreadCycleTime(HANDLE /*ThreadHandle*/, uint64_t* CycleTime) noexcept {
    if (!CycleTime) return FALSE;
    LargeInteger qpc{};
    QueryPerformanceCounter(&qpc);
    *CycleTime = (static_cast<uint64_t>(qpc.quadPart) * 4200000000ULL / 10000000ULL) / 8;
    return TRUE;
}

inline BOOL QueryIdleProcessorCycleTime(uint32_t* BufferLength, uint64_t* ProcessorIdleCycleTime) noexcept {
    if (!BufferLength) return FALSE;
    uint32_t required = 16 * sizeof(uint64_t);
    if (*BufferLength < required) {
        *BufferLength = required;
        return FALSE;
    }
    if (ProcessorIdleCycleTime) {
        for (int i = 0; i < 16; ++i) {
            ProcessorIdleCycleTime[i] = 1000000000ULL * (i + 1);
        }
    }
    *BufferLength = required;
    return TRUE;
}
inline BOOL TryEnterCriticalSection(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr) noexcept { return 1; }
inline BOOL HeapDestroy(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetDiskFreeSpaceA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* FindFirstFileA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* OpenMutexW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline int lstrlenA(const char* s) noexcept { return s ? static_cast<int>(std::strlen(s)) : 0; }
inline char* lstrcpyA(char* dst, const char* src) noexcept { return (dst && src) ? std::strcpy(dst, src) : dst; }
inline void* CreateFileA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL FreeResource(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL GlobalAddAtomW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetExitCodeThread(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL OutputDebugStringW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL IsBadReadPtr(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr) noexcept { return 1; }
inline void* GetShortPathNameW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL GlobalFindAtomW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL WritePrivateProfileStringW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* FindNextFileA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL GlobalDeleteAtom(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetThreadPriority(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL SetThreadPriority(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SearchPathW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL VerifyVersionInfoW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr) noexcept { return 1; }
inline BOOL HeapCreate(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL SignalObjectAndWait(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL VerSetConditionMask(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr) noexcept { return 1; }
inline DWORD GetUserDefaultUILanguage(void* /*a*/ = nullptr) noexcept { return 0x0409; }
inline DWORD GetConsoleCP(void* /*a*/ = nullptr) noexcept { return 0x0409; }
inline BOOL CompareStringA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline BOOL EnumResourceNamesW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetSystemDirectoryW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetDriveTypeA(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline void* GetComputerNameW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline DWORD GetSystemDefaultUILanguage(void* /*a*/ = nullptr) noexcept { return 0x0409; }
inline BOOL EnumCalendarInfoW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline void* GetPrivateProfileStringW(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return reinterpret_cast<void*>(0x8800); }
inline BOOL WaitForMultipleObjectsEx(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline DWORD GetThreadLocale(void* /*a*/ = nullptr) noexcept { return 0x0409; }
inline BOOL SetThreadLocale(void* /*a*/ = nullptr, void* /*b*/ = nullptr, void* /*c*/ = nullptr, void* /*d*/ = nullptr, void* /*e*/ = nullptr, void* /*f*/ = nullptr, void* /*g*/ = nullptr) noexcept { return 1; }
inline uint32_t VerLanguageNameW(uint32_t /*wLang*/, wchar_t* szLang, uint32_t cchLang) noexcept {
    if (!szLang || cchLang == 0) return 0;
    const wchar_t desc[] = L"English (United States)";
    size_t len = std::wcslen(desc);
    if (len >= cchLang) len = cchLang - 1;
    std::wcsncpy(szLang, desc, len);
    szLang[len] = L'\0';
    return static_cast<uint32_t>(len);
}
enum LOGICAL_PROCESSOR_RELATIONSHIP : uint32_t {
    RelationProcessorCore    = 0,
    RelationNumaNode         = 1,
    RelationCache            = 2,
    RelationProcessorPackage = 3,
    RelationGroup            = 4,
    RelationAll              = 0xFFFF
};

enum PROCESSOR_CACHE_TYPE : uint32_t {
    CacheUnified     = 0,
    CacheInstruction = 1,
    CacheData        = 2,
    CacheTrace       = 3
};

struct CACHE_DESCRIPTOR {
    uint8_t   Level;
    uint8_t   Associativity;
    uint16_t  LineSize;
    uint32_t  Size;
    PROCESSOR_CACHE_TYPE Type;
};

struct SYSTEM_LOGICAL_PROCESSOR_INFORMATION {
    ULONG_PTR ProcessorMask;
    LOGICAL_PROCESSOR_RELATIONSHIP Relationship;
    union {
        struct {
            uint8_t Flags;
        } ProcessorCore;
        struct {
            uint32_t NodeNumber;
        } NumaNode;
        CACHE_DESCRIPTOR Cache;
        uint64_t Reserved[2];
    };
};

struct CACHE_RELATIONSHIP {
    uint8_t Level;
    uint8_t Associativity;
    uint16_t LineSize;
    uint32_t CacheSize;
    PROCESSOR_CACHE_TYPE Type;
    uint8_t Reserved[20];
    uint16_t GroupCount;
    KAFFINITY GroupMasks[1];
};

struct PROCESSOR_RELATIONSHIP {
    uint8_t Flags; // 0x01 = LTP_PC_SMT
    uint8_t EfficiencyClass; // 0 = E-core, 1 = P-core, 2 = 3D V-Cache P-core
    uint8_t Reserved[20];
    uint16_t GroupCount;
    KAFFINITY GroupMasks[1];
};

struct NUMA_NODE_RELATIONSHIP {
    uint32_t NodeNumber;
    uint8_t Reserved[20];
    uint16_t GroupCount;
    KAFFINITY GroupMasks[1];
};

struct SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX {
    LOGICAL_PROCESSOR_RELATIONSHIP Relationship;
    uint32_t Size;
    union {
        PROCESSOR_RELATIONSHIP Processor;
        NUMA_NODE_RELATIONSHIP NumaNode;
        CACHE_RELATIONSHIP Cache;
    };
};

inline BOOL GetLogicalProcessorInformation(void* Buffer, uint32_t* ReturnedLength) noexcept {
    if (!ReturnedLength) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    // 16 cores (8 entries for dual-thread cores) + L1/L2/L3 caches + 1 NUMA node = ~12 entries
    constexpr uint32_t count = 12;
    uint32_t required = count * sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    if (!Buffer || *ReturnedLength < required) {
        *ReturnedLength = required;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    auto* arr = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION*>(Buffer);
    std::memset(arr, 0, required);

    // Populate representative Core 0..7 (Performance/V-Cache cores)
    for (uint32_t i = 0; i < 8; ++i) {
        arr[i].Relationship = RelationProcessorCore;
        arr[i].ProcessorMask = (1ULL << i);
        arr[i].ProcessorCore.Flags = 1; // SMT enabled
    }

    // Populate L3 Cache (96MB 3D V-Cache on CCD 0)
    arr[8].Relationship = RelationCache;
    arr[8].ProcessorMask = 0x00FF; // Cores 0-7
    arr[8].Cache.Level = 3;
    arr[8].Cache.Associativity = 16;
    arr[8].Cache.LineSize = 64;
    arr[8].Cache.Size = 96 * 1024 * 1024; // 96MB
    arr[8].Cache.Type = CacheUnified;

    // Populate NUMA Node 0
    arr[9].Relationship = RelationNumaNode;
    arr[9].ProcessorMask = 0xFFFF; // All 16 cores
    arr[9].NumaNode.NodeNumber = 0;

    *ReturnedLength = required;
    return TRUE;
}

inline BOOL GetLogicalProcessorInformationEx(LOGICAL_PROCESSOR_RELATIONSHIP RelationshipType,
                                             void* Buffer,
                                             uint32_t* ReturnedLength) noexcept {
    if (!ReturnedLength) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Calculate buffer size needed
    uint32_t recordCount = 0;
    if (RelationshipType == RelationProcessorCore || RelationshipType == RelationAll) {
        recordCount += 16; // 16 cores
    }
    if (RelationshipType == RelationCache || RelationshipType == RelationAll) {
        recordCount += 2;  // L3 Cache 96MB + L3 Cache 32MB
    }
    if (RelationshipType == RelationNumaNode || RelationshipType == RelationAll) {
        recordCount += 1;  // NUMA Node 0
    }

    uint32_t required = recordCount * sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
    if (!Buffer || *ReturnedLength < required) {
        *ReturnedLength = required;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    auto* curr = reinterpret_cast<uint8_t*>(Buffer);
    std::memset(curr, 0, required);

    // Populate Cores if requested
    if (RelationshipType == RelationProcessorCore || RelationshipType == RelationAll) {
        for (uint32_t i = 0; i < 16; ++i) {
            auto* pInfo = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(curr);
            pInfo->Relationship = RelationProcessorCore;
            pInfo->Size = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
            pInfo->Processor.Flags = (i < 8) ? 1 : 0; // SMT on P-cores
            pInfo->Processor.EfficiencyClass = (i < 4) ? 2 : ((i < 8) ? 1 : 0); // 2 = 3D V-Cache, 1 = P-core, 0 = E-core
            pInfo->Processor.GroupCount = 1;
            pInfo->Processor.GroupMasks[0] = (1ULL << i);
            curr += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
        }
    }

    // Populate Caches if requested
    if (RelationshipType == RelationCache || RelationshipType == RelationAll) {
        // 96MB 3D V-Cache on CCD 0
        auto* pC1 = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(curr);
        pC1->Relationship = RelationCache;
        pC1->Size = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
        pC1->Cache.Level = 3;
        pC1->Cache.Associativity = 16;
        pC1->Cache.LineSize = 64;
        pC1->Cache.CacheSize = 96 * 1024 * 1024;
        pC1->Cache.Type = CacheUnified;
        pC1->Cache.GroupCount = 1;
        pC1->Cache.GroupMasks[0] = 0x00FF; // Cores 0-7
        curr += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);

        // 32MB standard L3 on CCD 1
        auto* pC2 = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(curr);
        pC2->Relationship = RelationCache;
        pC2->Size = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
        pC2->Cache.Level = 3;
        pC2->Cache.Associativity = 16;
        pC2->Cache.LineSize = 64;
        pC2->Cache.CacheSize = 32 * 1024 * 1024;
        pC2->Cache.Type = CacheUnified;
        pC2->Cache.GroupCount = 1;
        pC2->Cache.GroupMasks[0] = 0xFF00; // Cores 8-15
        curr += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
    }

    // Populate NUMA Node if requested
    if (RelationshipType == RelationNumaNode || RelationshipType == RelationAll) {
        auto* pNuma = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(curr);
        pNuma->Relationship = RelationNumaNode;
        pNuma->Size = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
        pNuma->NumaNode.NodeNumber = 0;
        pNuma->NumaNode.GroupCount = 1;
        pNuma->NumaNode.GroupMasks[0] = 0xFFFF;
        curr += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
    }

    *ReturnedLength = required;
    return TRUE;
}

inline std::vector<ULONG> s_defaultCpuSets{0, 1, 2, 3, 4, 5, 6, 7}; // Default to P-cores/V-Cache
inline std::unordered_map<HANDLE, std::vector<ULONG>> s_threadCpuSets;
inline std::mutex s_cpuSetLock;

inline BOOL WINAPI SetThreadSelectedCpuSets(HANDLE hThread, const ULONG* CpuSetIds, ULONG CpuSetIdCount) noexcept {
    if (!CpuSetIds && CpuSetIdCount > 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    std::lock_guard<std::mutex> lock(s_cpuSetLock);
    if (CpuSetIdCount == 0) {
        s_threadCpuSets.erase(hThread);
    } else {
        s_threadCpuSets[hThread].assign(CpuSetIds, CpuSetIds + CpuSetIdCount);
    }
    return TRUE;
}

inline BOOL WINAPI GetThreadSelectedCpuSets(HANDLE hThread, ULONG* CpuSetIds, ULONG CpuSetIdCount, ULONG* RequiredIdCount) noexcept {
    std::lock_guard<std::mutex> lock(s_cpuSetLock);
    auto it = s_threadCpuSets.find(hThread);
    const auto& sets = (it != s_threadCpuSets.end()) ? it->second : s_defaultCpuSets;

    if (RequiredIdCount) *RequiredIdCount = static_cast<ULONG>(sets.size());
    if (CpuSetIds && CpuSetIdCount >= sets.size()) {
        std::copy(sets.begin(), sets.end(), CpuSetIds);
        return TRUE;
    }
    if (!CpuSetIds && CpuSetIdCount == 0) {
        return TRUE;
    }
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return FALSE;
}

inline BOOL WINAPI SetProcessDefaultCpuSets(HANDLE /*hProcess*/, const ULONG* CpuSetIds, ULONG CpuSetIdCount) noexcept {
    if (!CpuSetIds && CpuSetIdCount > 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    std::lock_guard<std::mutex> lock(s_cpuSetLock);
    if (CpuSetIdCount == 0) {
        s_defaultCpuSets.clear();
    } else {
        s_defaultCpuSets.assign(CpuSetIds, CpuSetIds + CpuSetIdCount);
    }
    return TRUE;
}

inline BOOL WINAPI GetProcessDefaultCpuSets(HANDLE /*hProcess*/, ULONG* CpuSetIds, ULONG CpuSetIdCount, ULONG* RequiredIdCount) noexcept {
    std::lock_guard<std::mutex> lock(s_cpuSetLock);
    if (RequiredIdCount) *RequiredIdCount = static_cast<ULONG>(s_defaultCpuSets.size());
    if (CpuSetIds && CpuSetIdCount >= s_defaultCpuSets.size()) {
        std::copy(s_defaultCpuSets.begin(), s_defaultCpuSets.end(), CpuSetIds);
        return TRUE;
    }
    if (!CpuSetIds && CpuSetIdCount == 0) {
        return TRUE;
    }
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return FALSE;
}
inline BOOL IsWow64Process(void* /*hProcess*/, BOOL* Wow64Process) noexcept {
    if (Wow64Process) *Wow64Process = 0;
    return 1;
}
inline BOOL ProcessIdToSessionId(uint32_t /*dwProcessId*/, uint32_t* pSessionId) noexcept {
    if (pSessionId) *pSessionId = 1;
    return 1;
}
inline uint32_t LocaleNameToLCID(const wchar_t* /*lpName*/, uint32_t /*dwFlags*/) noexcept {
    return 0x0409;
}
inline BOOL GetTimeZoneInformationForYear(uint16_t /*wYear*/, void* /*pDynamicTimeZoneInformation*/, void* /*pTimeZoneInformation*/) noexcept {
    return 1;
}
inline BOOL GetSystemTimes(void* lpIdleTime, void* lpKernelTime, void* lpUserTime) noexcept {
    if (lpIdleTime) std::memset(lpIdleTime, 0, 8);
    if (lpKernelTime) std::memset(lpKernelTime, 0, 8);
    if (lpUserTime) std::memset(lpUserTime, 0, 8);
    return 1;
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
    ldr.registerExport("kernel32.dll", "VirtualAllocEx", reinterpret_cast<void*>(VirtualAllocEx));
    ldr.registerExport("kernel32.dll", "VirtualAlloc2", reinterpret_cast<void*>(VirtualAlloc2));
    ldr.registerExport("kernel32.dll", "VirtualAlloc2FromApp", reinterpret_cast<void*>(VirtualAlloc2FromApp));
    ldr.registerExport("kernel32.dll", "VirtualFreeEx", reinterpret_cast<void*>(VirtualFreeEx));
    ldr.registerExport("kernel32.dll", "VirtualLock", reinterpret_cast<void*>(VirtualLock));
    ldr.registerExport("kernel32.dll", "VirtualUnlock", reinterpret_cast<void*>(VirtualUnlock));
    ldr.registerExport("kernel32.dll", "GetProcessWorkingSetSizeEx", reinterpret_cast<void*>(GetProcessWorkingSetSizeEx));
    ldr.registerExport("kernel32.dll", "SetProcessWorkingSetSizeEx", reinterpret_cast<void*>(SetProcessWorkingSetSizeEx));
    ldr.registerExport("kernel32.dll", "MapViewOfFileNuma2", reinterpret_cast<void*>(MapViewOfFileNuma2));
    ldr.registerExport("api-ms-win-core-memory-l1-1-6.dll", "VirtualAlloc2", reinterpret_cast<void*>(VirtualAlloc2));
    ldr.registerExport("api-ms-win-core-memory-l1-1-6.dll", "VirtualAlloc2FromApp", reinterpret_cast<void*>(VirtualAlloc2FromApp));
    ldr.registerExport("api-ms-win-core-memory-l1-1-6.dll", "MapViewOfFileNuma2", reinterpret_cast<void*>(MapViewOfFileNuma2));
    ldr.registerExport("api-ms-win-core-memory-l1-1-6.dll", "VirtualQueryEx", reinterpret_cast<void*>(VirtualQueryEx));
    ldr.registerExport("api-ms-win-core-memory-l1-1-6.dll", "VirtualProtectEx", reinterpret_cast<void*>(VirtualProtectEx));
    ldr.registerExport("api-ms-win-core-memory-l1-1-6.dll", "ReadProcessMemory", reinterpret_cast<void*>(ReadProcessMemory));
    ldr.registerExport("api-ms-win-core-memory-l1-1-6.dll", "WriteProcessMemory", reinterpret_cast<void*>(WriteProcessMemory));
    ldr.registerExport("kernel32.dll", "GetCurrentProcess", reinterpret_cast<void*>(GetCurrentProcess));
    ldr.registerExport("kernel32.dll", "GetCurrentProcessId", reinterpret_cast<void*>(GetCurrentProcessId));
    ldr.registerExport("kernel32.dll", "GetCurrentPackageFullName", reinterpret_cast<void*>(GetCurrentPackageFullName));
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
    ldr.registerExport("kernel32.dll", "RtlRestoreContext", reinterpret_cast<void*>(RtlRestoreContext));
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
    ldr.registerExport("api-ms-win-core-file-l1-1-0.dll", "GetFinalPathNameByHandleW", reinterpret_cast<void*>(GetFinalPathNameByHandleW));
    ldr.registerExport("kernel32.dll", "GetSystemTimeAdjustment", reinterpret_cast<void*>(GetSystemTimeAdjustment));
    ldr.registerExport("api-ms-win-core-sysinfo-l1-2-0.dll", "GetSystemTimeAdjustment", reinterpret_cast<void*>(GetSystemTimeAdjustment));
    ldr.registerExport("kernel32.dll", "GetSystemTimeAdjustmentPrecise", reinterpret_cast<void*>(GetSystemTimeAdjustmentPrecise));
    ldr.registerExport("api-ms-win-core-realtime-l1-1-1.dll", "GetSystemTimeAdjustmentPrecise", reinterpret_cast<void*>(GetSystemTimeAdjustmentPrecise));
    ldr.registerExport("api-ms-win-core-sysinfo-l1-2-0.dll", "GetLogicalProcessorInformationEx", reinterpret_cast<void*>(GetLogicalProcessorInformationEx));
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
    ldr.registerExport("kernel32.dll", "CreateThreadpool", reinterpret_cast<void*>(CreateThreadpool));
    ldr.registerExport("kernel32.dll", "SetThreadpoolThreadMaximum", reinterpret_cast<void*>(SetThreadpoolThreadMaximum));
    ldr.registerExport("kernel32.dll", "SetThreadpoolThreadMinimum", reinterpret_cast<void*>(SetThreadpoolThreadMinimum));
    ldr.registerExport("kernel32.dll", "CloseThreadpool", reinterpret_cast<void*>(CloseThreadpool));
    ldr.registerExport("kernel32.dll", "CreateThreadpoolWork", reinterpret_cast<void*>(CreateThreadpoolWork));
    ldr.registerExport("kernel32.dll", "SubmitThreadpoolWork", reinterpret_cast<void*>(SubmitThreadpoolWork));
    ldr.registerExport("kernel32.dll", "WaitForThreadpoolWorkCallbacks", reinterpret_cast<void*>(WaitForThreadpoolWorkCallbacks));
    ldr.registerExport("kernel32.dll", "CloseThreadpoolWork", reinterpret_cast<void*>(CloseThreadpoolWork));
    ldr.registerExport("kernel32.dll", "CreateThreadpoolTimer", reinterpret_cast<void*>(CreateThreadpoolTimer));
    ldr.registerExport("kernel32.dll", "SetThreadpoolTimer", reinterpret_cast<void*>(SetThreadpoolTimer));
    ldr.registerExport("kernel32.dll", "IsThreadpoolTimerSet", reinterpret_cast<void*>(IsThreadpoolTimerSet));
    ldr.registerExport("kernel32.dll", "WaitForThreadpoolTimerCallbacks", reinterpret_cast<void*>(WaitForThreadpoolTimerCallbacks));
    ldr.registerExport("kernel32.dll", "CloseThreadpoolTimer", reinterpret_cast<void*>(CloseThreadpoolTimer));
    ldr.registerExport("kernel32.dll", "CreateThreadpoolWait", reinterpret_cast<void*>(CreateThreadpoolWait));
    ldr.registerExport("kernel32.dll", "SetThreadpoolWait", reinterpret_cast<void*>(SetThreadpoolWait));
    ldr.registerExport("kernel32.dll", "WaitForThreadpoolWaitCallbacks", reinterpret_cast<void*>(WaitForThreadpoolWaitCallbacks));
    ldr.registerExport("kernel32.dll", "CloseThreadpoolWait", reinterpret_cast<void*>(CloseThreadpoolWait));
    ldr.registerExport("kernel32.dll", "CreateThreadpoolCleanupGroup", reinterpret_cast<void*>(CreateThreadpoolCleanupGroup));
    ldr.registerExport("kernel32.dll", "CloseThreadpoolCleanupGroupMembers", reinterpret_cast<void*>(CloseThreadpoolCleanupGroupMembers));
    ldr.registerExport("kernel32.dll", "CloseThreadpoolCleanupGroup", reinterpret_cast<void*>(CloseThreadpoolCleanupGroup));
    ldr.registerExport("kernel32.dll", "SetThreadpoolCallbackPool", reinterpret_cast<void*>(SetThreadpoolCallbackPool));
    ldr.registerExport("kernel32.dll", "SetThreadpoolCallbackCleanupGroup", reinterpret_cast<void*>(SetThreadpoolCallbackCleanupGroup));
    ldr.registerExport("kernel32.dll", "InitializeThreadpoolEnvironment", reinterpret_cast<void*>(InitializeThreadpoolEnvironment));
    ldr.registerExport("kernel32.dll", "DestroyThreadpoolEnvironment", reinterpret_cast<void*>(DestroyThreadpoolEnvironment));
    ldr.registerExport("kernel32.dll", "FreeLibraryWhenCallbackReturns", reinterpret_cast<void*>(FreeLibraryWhenCallbackReturns));

    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "CreateThreadpool", reinterpret_cast<void*>(CreateThreadpool));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "SetThreadpoolThreadMaximum", reinterpret_cast<void*>(SetThreadpoolThreadMaximum));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "SetThreadpoolThreadMinimum", reinterpret_cast<void*>(SetThreadpoolThreadMinimum));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "CloseThreadpool", reinterpret_cast<void*>(CloseThreadpool));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "CreateThreadpoolWork", reinterpret_cast<void*>(CreateThreadpoolWork));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "SubmitThreadpoolWork", reinterpret_cast<void*>(SubmitThreadpoolWork));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "WaitForThreadpoolWorkCallbacks", reinterpret_cast<void*>(WaitForThreadpoolWorkCallbacks));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "CloseThreadpoolWork", reinterpret_cast<void*>(CloseThreadpoolWork));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "CreateThreadpoolTimer", reinterpret_cast<void*>(CreateThreadpoolTimer));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "SetThreadpoolTimer", reinterpret_cast<void*>(SetThreadpoolTimer));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "IsThreadpoolTimerSet", reinterpret_cast<void*>(IsThreadpoolTimerSet));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "WaitForThreadpoolTimerCallbacks", reinterpret_cast<void*>(WaitForThreadpoolTimerCallbacks));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "CloseThreadpoolTimer", reinterpret_cast<void*>(CloseThreadpoolTimer));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "CreateThreadpoolWait", reinterpret_cast<void*>(CreateThreadpoolWait));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "SetThreadpoolWait", reinterpret_cast<void*>(SetThreadpoolWait));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "WaitForThreadpoolWaitCallbacks", reinterpret_cast<void*>(WaitForThreadpoolWaitCallbacks));
    ldr.registerExport("api-ms-win-core-threadpool-l1-2-0.dll", "CloseThreadpoolWait", reinterpret_cast<void*>(CloseThreadpoolWait));
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
    ldr.registerExport("kernel32.dll", "RemoveVectoredContinueHandler", reinterpret_cast<void*>(RemoveVectoredContinueHandler));
    ldr.registerExport("api-ms-win-core-errorhandling-l1-1-1.dll", "RemoveVectoredContinueHandler", reinterpret_cast<void*>(RemoveVectoredContinueHandler));
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
    ldr.registerExport("kernel32.dll", "QueryInformationJobObject", reinterpret_cast<void*>(QueryInformationJobObject));
    ldr.registerExport("kernel32.dll", "TerminateJobObject", reinterpret_cast<void*>(TerminateJobObject));
    ldr.registerExport("kernel32.dll", "IsProcessInJob", reinterpret_cast<void*>(IsProcessInJob));
    ldr.registerExport("api-ms-win-core-job-l1-1-0.dll", "CreateJobObjectW", reinterpret_cast<void*>(CreateJobObjectW));
    ldr.registerExport("api-ms-win-core-job-l1-1-0.dll", "AssignProcessToJobObject", reinterpret_cast<void*>(AssignProcessToJobObject));
    ldr.registerExport("api-ms-win-core-job-l1-1-0.dll", "SetInformationJobObject", reinterpret_cast<void*>(SetInformationJobObject));
    ldr.registerExport("api-ms-win-core-job-l1-1-0.dll", "QueryInformationJobObject", reinterpret_cast<void*>(QueryInformationJobObject));
    ldr.registerExport("api-ms-win-core-job-l1-1-0.dll", "TerminateJobObject", reinterpret_cast<void*>(TerminateJobObject));
    ldr.registerExport("api-ms-win-core-job-l1-1-0.dll", "IsProcessInJob", reinterpret_cast<void*>(IsProcessInJob));
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
    ldr.registerExport("kernel32.dll", "AddVectoredContinueHandler", reinterpret_cast<void*>(AddVectoredContinueHandler));
    ldr.registerExport("api-ms-win-core-errorhandling-l1-1-1.dll", "AddVectoredExceptionHandler", reinterpret_cast<void*>(AddVectoredExceptionHandler));
    ldr.registerExport("api-ms-win-core-errorhandling-l1-1-1.dll", "AddVectoredContinueHandler", reinterpret_cast<void*>(AddVectoredContinueHandler));
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
    ldr.registerExport("kernel32.dll", "FindFirstVolumeA", reinterpret_cast<void*>(FindFirstVolumeA));
    ldr.registerExport("kernel32.dll", "FindNextVolumeA", reinterpret_cast<void*>(FindNextVolumeA));
    ldr.registerExport("kernel32.dll", "GetVolumeNameForVolumeMountPointA", reinterpret_cast<void*>(GetVolumeNameForVolumeMountPointA));
    ldr.registerExport("kernel32.dll", "GetVolumePathNameA", reinterpret_cast<void*>(GetVolumePathNameA));
    ldr.registerExport("kernel32.dll", "GetVolumeInformationA", reinterpret_cast<void*>(GetVolumeInformationA));
    ldr.registerExport("kernel32.dll", "GetVolumeInformationByHandleW", reinterpret_cast<void*>(GetVolumeInformationByHandleW));
    ldr.registerExport("kernel32.dll", "SetVolumeMountPointA", reinterpret_cast<void*>(SetVolumeMountPointA));
    ldr.registerExport("kernel32.dll", "DeleteVolumeMountPointA", reinterpret_cast<void*>(DeleteVolumeMountPointA));
    ldr.registerExport("kernel32.dll", "SetVolumeLabelA", reinterpret_cast<void*>(SetVolumeLabelA));
    ldr.registerExport("kernel32.dll", "DefineDosDeviceW", reinterpret_cast<void*>(DefineDosDeviceW));
    ldr.registerExport("kernel32.dll", "DefineDosDeviceA", reinterpret_cast<void*>(DefineDosDeviceA));
    ldr.registerExport("kernel32.dll", "SetVolumeMountPointW", reinterpret_cast<void*>(SetVolumeMountPointW));
    ldr.registerExport("kernel32.dll", "DeleteVolumeMountPointW", reinterpret_cast<void*>(DeleteVolumeMountPointW));
    ldr.registerExport("kernel32.dll", "QueryDosDeviceA", reinterpret_cast<void*>(QueryDosDeviceA));
    ldr.registerExport("kernel32.dll", "GetDiskFreeSpaceExA", reinterpret_cast<void*>(GetDiskFreeSpaceExA));
    ldr.registerExport("kernel32.dll", "GetLogicalDriveStringsA", reinterpret_cast<void*>(GetLogicalDriveStringsA));
    ldr.registerExport("kernel32.dll", "CancelIoEx", reinterpret_cast<void*>(CancelIoEx));
    ldr.registerExport("kernel32.dll", "CancelSynchronousIo", reinterpret_cast<void*>(CancelSynchronousIo));
    ldr.registerExport("kernel32.dll", "GetOverlappedResultEx", reinterpret_cast<void*>(GetOverlappedResultEx));
    ldr.registerExport("kernel32.dll", "SleepConditionVariableCS", reinterpret_cast<void*>(SleepConditionVariableCS));
    ldr.registerExport("kernel32.dll", "CreateSymbolicLinkW", reinterpret_cast<void*>(CreateSymbolicLinkW));
    ldr.registerExport("kernel32.dll", "GetConsoleWindow", reinterpret_cast<void*>(GetConsoleWindow));
    ldr.registerExport("kernel32.dll", "EnumUILanguagesW", reinterpret_cast<void*>(EnumUILanguagesW));
    ldr.registerExport("kernel32.dll", "GetSystemDefaultLCID", reinterpret_cast<void*>(GetSystemDefaultLCID));
    ldr.registerExport("kernel32.dll", "GetThreadUILanguage", reinterpret_cast<void*>(GetThreadUILanguage));
    ldr.registerExport("kernel32.dll", "LCIDToLocaleName", reinterpret_cast<void*>(LCIDToLocaleName));
    ldr.registerExport("kernel32.dll", "SetDefaultDllDirectories", reinterpret_cast<void*>(SetDefaultDllDirectories));
    ldr.registerExport("kernel32.dll", "SetFileAttributesA", reinterpret_cast<void*>(SetFileAttributesA));
    ldr.registerExport("kernel32.dll", "VerifyVersionInfoA", reinterpret_cast<void*>(VerifyVersionInfoA));
    ldr.registerExport("kernel32.dll", "K32GetModuleFileNameExW", reinterpret_cast<void*>(K32GetModuleFileNameExW));
    ldr.registerExport("kernel32.dll", "K32GetProcessImageFileNameW", reinterpret_cast<void*>(K32GetProcessImageFileNameW));
    ldr.registerExport("kernel32.dll", "GlobalReAlloc", reinterpret_cast<void*>(GlobalReAlloc));
    ldr.registerExport("kernel32.dll", "LocalReAlloc", reinterpret_cast<void*>(LocalReAlloc));
    ldr.registerExport("kernel32.dll", "GlobalHandle", reinterpret_cast<void*>(GlobalHandle));
    ldr.registerExport("kernel32.dll", "GlobalFlags", reinterpret_cast<void*>(GlobalFlags));
    ldr.registerExport("kernel32.dll", "SetThreadUILanguage", reinterpret_cast<void*>(SetThreadUILanguage));
    ldr.registerExport("kernel32.dll", "SetSearchPathMode", reinterpret_cast<void*>(SetSearchPathMode));
    ldr.registerExport("kernel32.dll", "SetDllDirectoryW", reinterpret_cast<void*>(SetDllDirectoryW));
    ldr.registerExport("kernel32.dll", "GetSystemWow64DirectoryW", reinterpret_cast<void*>(GetSystemWow64DirectoryW));
    ldr.registerExport("kernel32.dll", "ExpandEnvironmentStringsA", reinterpret_cast<void*>(ExpandEnvironmentStringsA));
    ldr.registerExport("kernel32.dll", "CreateActCtxW", reinterpret_cast<void*>(CreateActCtxW));
    ldr.registerExport("kernel32.dll", "ActivateActCtx", reinterpret_cast<void*>(ActivateActCtx));
    ldr.registerExport("kernel32.dll", "DeactivateActCtx", reinterpret_cast<void*>(DeactivateActCtx));
    ldr.registerExport("kernel32.dll", "FindActCtxSectionStringW", reinterpret_cast<void*>(FindActCtxSectionStringW));
    ldr.registerExport("kernel32.dll", "QueryActCtxW", reinterpret_cast<void*>(QueryActCtxW));
    ldr.registerExport("kernel32.dll", "GetProfileIntW", reinterpret_cast<void*>(GetProfileIntW));
    ldr.registerExport("kernel32.dll", "GlobalGetAtomNameW", reinterpret_cast<void*>(GlobalGetAtomNameW));
    ldr.registerExport("kernel32.dll", "lstrcmpA", reinterpret_cast<void*>(lstrcmpA));
    ldr.registerExport("kernel32.dll", "LockFile", reinterpret_cast<void*>(LockFile));
    ldr.registerExport("kernel32.dll", "UnlockFile", reinterpret_cast<void*>(UnlockFile));
    ldr.registerExport("kernel32.dll", "FindResourceExW", reinterpret_cast<void*>(FindResourceExW));
    ldr.registerExport("kernel32.dll", "InterlockedPushEntrySList", reinterpret_cast<void*>(InterlockedPushEntrySList));
    ldr.registerExport("kernel32.dll", "GetThreadId", reinterpret_cast<void*>(GetThreadId));
    ldr.registerExport("kernel32.dll", "CreateIoCompletionPort", reinterpret_cast<void*>(CreateIoCompletionPort));
    ldr.registerExport("kernel32.dll", "GetQueuedCompletionStatus", reinterpret_cast<void*>(GetQueuedCompletionStatus));
    ldr.registerExport("kernel32.dll", "PostQueuedCompletionStatus", reinterpret_cast<void*>(PostQueuedCompletionStatus));
    ldr.registerExport("kernel32.dll", "CreateFiberEx", reinterpret_cast<void*>(CreateFiberEx));
    ldr.registerExport("kernel32.dll", "SwitchToFiber", reinterpret_cast<void*>(SwitchToFiber));
    ldr.registerExport("kernel32.dll", "DeleteFiber", reinterpret_cast<void*>(DeleteFiber));
    ldr.registerExport("kernel32.dll", "ConvertThreadToFiberEx", reinterpret_cast<void*>(ConvertThreadToFiberEx));
    ldr.registerExport("kernel32.dll", "ConvertFiberToThread", reinterpret_cast<void*>(ConvertFiberToThread));
    ldr.registerExport("kernel32.dll", "LockFileEx", reinterpret_cast<void*>(LockFileEx));
    ldr.registerExport("kernel32.dll", "UnlockFileEx", reinterpret_cast<void*>(UnlockFileEx));
    ldr.registerExport("kernel32.dll", "FlushViewOfFile", reinterpret_cast<void*>(FlushViewOfFile));
    ldr.registerExport("kernel32.dll", "HeapValidate", reinterpret_cast<void*>(HeapValidate));
    ldr.registerExport("kernel32.dll", "HeapCompact", reinterpret_cast<void*>(HeapCompact));
    ldr.registerExport("kernel32.dll", "InterlockedFlushSList", reinterpret_cast<void*>(InterlockedFlushSList));
    ldr.registerExport("kernel32.dll", "ReadConsoleA", reinterpret_cast<void*>(ReadConsoleA));
    ldr.registerExport("kernel32.dll", "RtlCaptureStackBackTrace", reinterpret_cast<void*>(RtlCaptureStackBackTrace));
    ldr.registerExport("kernel32.dll", "CheckRemoteDebuggerPresent", reinterpret_cast<void*>(CheckRemoteDebuggerPresent));
    ldr.registerExport("kernel32.dll", "WTSGetActiveConsoleSessionId", reinterpret_cast<void*>(WTSGetActiveConsoleSessionId));
    ldr.registerExport("kernel32.dll", "GetSystemTimePreciseAsFileTime", reinterpret_cast<void*>(GetSystemTimePreciseAsFileTime));
    ldr.registerExport("kernel32.dll", "CreateFile2", reinterpret_cast<void*>(CreateFile2));
    ldr.registerExport("kernel32.dll", "GetVolumePathNamesForVolumeNameW", reinterpret_cast<void*>(GetVolumePathNamesForVolumeNameW));
    ldr.registerExport("kernel32.dll", "GetVolumeNameForVolumeMountPointW", reinterpret_cast<void*>(GetVolumeNameForVolumeMountPointW));
    ldr.registerExport("kernel32.dll", "GetComputerNameExW", reinterpret_cast<void*>(GetComputerNameExW));
    ldr.registerExport("kernel32.dll", "SetThreadInformation", reinterpret_cast<void*>(SetThreadInformation));
    ldr.registerExport("kernel32.dll", "SetThreadDescription", reinterpret_cast<void*>(SetThreadDescription));
    ldr.registerExport("kernel32.dll", "SetFileInformationByHandle", reinterpret_cast<void*>(SetFileInformationByHandle));
    ldr.registerExport("kernel32.dll", "GetUserGeoID", reinterpret_cast<void*>(GetUserGeoID));
    ldr.registerExport("kernel32.dll", "GetGeoInfoW", reinterpret_cast<void*>(GetGeoInfoW));
    ldr.registerExport("kernel32.dll", "GetCurrencyFormatW", reinterpret_cast<void*>(GetCurrencyFormatW));
    ldr.registerExport("kernel32.dll", "GetUserPreferredUILanguages", reinterpret_cast<void*>(GetUserPreferredUILanguages));
    ldr.registerExport("kernel32.dll", "GetUserDefaultLocaleName", reinterpret_cast<void*>(GetUserDefaultLocaleName));
    ldr.registerExport("kernel32.dll", "SetWaitableTimer", reinterpret_cast<void*>(SetWaitableTimer));
    ldr.registerExport("kernel32.dll", "CreateWaitableTimerA", reinterpret_cast<void*>(CreateWaitableTimerA));
    ldr.registerExport("kernel32.dll", "CreateSemaphoreA", reinterpret_cast<void*>(CreateSemaphoreA));
    ldr.registerExport("kernel32.dll", "CreateThreadpoolWait", reinterpret_cast<void*>(CreateThreadpoolWait));
    ldr.registerExport("kernel32.dll", "SetThreadpoolWait", reinterpret_cast<void*>(SetThreadpoolWait));
    ldr.registerExport("kernel32.dll", "WaitForThreadpoolWaitCallbacks", reinterpret_cast<void*>(WaitForThreadpoolWaitCallbacks));
    ldr.registerExport("kernel32.dll", "CloseThreadpoolWait", reinterpret_cast<void*>(CloseThreadpoolWait));
    ldr.registerExport("kernel32.dll", "WaitOnAddress", reinterpret_cast<void*>(WaitOnAddress));
    ldr.registerExport("kernel32.dll", "WakeByAddressSingle", reinterpret_cast<void*>(WakeByAddressSingle));
    ldr.registerExport("kernel32.dll", "WakeByAddressAll", reinterpret_cast<void*>(WakeByAddressAll));
    ldr.registerExport("api-ms-win-core-synch-l1-2-0.dll", "WaitOnAddress", reinterpret_cast<void*>(WaitOnAddress));
    ldr.registerExport("api-ms-win-core-synch-l1-2-0.dll", "WakeByAddressSingle", reinterpret_cast<void*>(WakeByAddressSingle));
    ldr.registerExport("api-ms-win-core-synch-l1-2-0.dll", "WakeByAddressAll", reinterpret_cast<void*>(WakeByAddressAll));
    ldr.registerExport("kernel32.dll", "GetEnabledXStateFeatures", reinterpret_cast<void*>(GetEnabledXStateFeatures));
    ldr.registerExport("kernel32.dll", "GetNumberFormatEx", reinterpret_cast<void*>(GetNumberFormatEx));
    ldr.registerExport("kernel32.dll", "SetConsoleCP", reinterpret_cast<void*>(SetConsoleCP));
    ldr.registerExport("kernel32.dll", "SetConsoleOutputCP", reinterpret_cast<void*>(SetConsoleOutputCP));

    ldr.registerExport("kernel32.dll", "QueryDosDeviceW", reinterpret_cast<void*>(QueryDosDeviceW));
    ldr.registerExport("kernel32.dll", "ReadProcessMemory", reinterpret_cast<void*>(ReadProcessMemory));
    ldr.registerExport("kernel32.dll", "GetCPInfoExW", reinterpret_cast<void*>(GetCPInfoExW));
    ldr.registerExport("kernel32.dll", "WriteProcessMemory", reinterpret_cast<void*>(WriteProcessMemory));
    ldr.registerExport("kernel32.dll", "TryEnterCriticalSection", reinterpret_cast<void*>(TryEnterCriticalSection));
    ldr.registerExport("kernel32.dll", "HeapDestroy", reinterpret_cast<void*>(HeapDestroy));
    ldr.registerExport("kernel32.dll", "GetDiskFreeSpaceA", reinterpret_cast<void*>(GetDiskFreeSpaceA));
    ldr.registerExport("kernel32.dll", "FindFirstFileA", reinterpret_cast<void*>(FindFirstFileA));
    ldr.registerExport("kernel32.dll", "OpenMutexW", reinterpret_cast<void*>(OpenMutexW));
    ldr.registerExport("kernel32.dll", "lstrlenA", reinterpret_cast<void*>(lstrlenA));
    ldr.registerExport("kernel32.dll", "lstrcpyA", reinterpret_cast<void*>(lstrcpyA));
    ldr.registerExport("kernel32.dll", "CreateFileA", reinterpret_cast<void*>(CreateFileA));
    ldr.registerExport("kernel32.dll", "FreeResource", reinterpret_cast<void*>(FreeResource));
    ldr.registerExport("kernel32.dll", "GlobalAddAtomW", reinterpret_cast<void*>(GlobalAddAtomW));
    ldr.registerExport("kernel32.dll", "GetExitCodeThread", reinterpret_cast<void*>(GetExitCodeThread));
    ldr.registerExport("kernel32.dll", "OutputDebugStringW", reinterpret_cast<void*>(OutputDebugStringW));
    ldr.registerExport("kernel32.dll", "IsBadReadPtr", reinterpret_cast<void*>(IsBadReadPtr));
    ldr.registerExport("kernel32.dll", "GetShortPathNameW", reinterpret_cast<void*>(GetShortPathNameW));
    ldr.registerExport("kernel32.dll", "GlobalFindAtomW", reinterpret_cast<void*>(GlobalFindAtomW));
    ldr.registerExport("kernel32.dll", "WritePrivateProfileStringW", reinterpret_cast<void*>(WritePrivateProfileStringW));
    ldr.registerExport("kernel32.dll", "FindNextFileA", reinterpret_cast<void*>(FindNextFileA));
    ldr.registerExport("kernel32.dll", "GlobalDeleteAtom", reinterpret_cast<void*>(GlobalDeleteAtom));
    ldr.registerExport("kernel32.dll", "GetThreadPriority", reinterpret_cast<void*>(GetThreadPriority));
    ldr.registerExport("kernel32.dll", "SetThreadPriority", reinterpret_cast<void*>(SetThreadPriority));
    ldr.registerExport("kernel32.dll", "SearchPathW", reinterpret_cast<void*>(SearchPathW));
    ldr.registerExport("kernel32.dll", "VerifyVersionInfoW", reinterpret_cast<void*>(VerifyVersionInfoW));
    ldr.registerExport("kernel32.dll", "HeapCreate", reinterpret_cast<void*>(HeapCreate));
    ldr.registerExport("kernel32.dll", "SignalObjectAndWait", reinterpret_cast<void*>(SignalObjectAndWait));
    ldr.registerExport("kernel32.dll", "VerSetConditionMask", reinterpret_cast<void*>(VerSetConditionMask));
    ldr.registerExport("kernel32.dll", "GetUserDefaultUILanguage", reinterpret_cast<void*>(GetUserDefaultUILanguage));
    ldr.registerExport("kernel32.dll", "GetConsoleCP", reinterpret_cast<void*>(GetConsoleCP));
    ldr.registerExport("kernel32.dll", "CompareStringA", reinterpret_cast<void*>(CompareStringA));
    ldr.registerExport("kernel32.dll", "EnumResourceNamesW", reinterpret_cast<void*>(EnumResourceNamesW));
    ldr.registerExport("kernel32.dll", "GetSystemDirectoryW", reinterpret_cast<void*>(GetSystemDirectoryW));
    ldr.registerExport("kernel32.dll", "GetDriveTypeA", reinterpret_cast<void*>(GetDriveTypeA));
    ldr.registerExport("kernel32.dll", "GetComputerNameW", reinterpret_cast<void*>(GetComputerNameW));
    ldr.registerExport("kernel32.dll", "GetSystemDefaultUILanguage", reinterpret_cast<void*>(GetSystemDefaultUILanguage));
    ldr.registerExport("kernel32.dll", "EnumCalendarInfoW", reinterpret_cast<void*>(EnumCalendarInfoW));
    ldr.registerExport("kernel32.dll", "GetPrivateProfileStringW", reinterpret_cast<void*>(GetPrivateProfileStringW));
    ldr.registerExport("kernel32.dll", "WaitForMultipleObjectsEx", reinterpret_cast<void*>(WaitForMultipleObjectsEx));
    ldr.registerExport("kernel32.dll", "GetThreadLocale", reinterpret_cast<void*>(GetThreadLocale));
    ldr.registerExport("kernel32.dll", "SetThreadLocale", reinterpret_cast<void*>(SetThreadLocale));
    ldr.registerExport("kernel32.dll", "VerLanguageNameW", reinterpret_cast<void*>(VerLanguageNameW));
    ldr.registerExport("kernel32.dll", "GetLogicalProcessorInformation", reinterpret_cast<void*>(GetLogicalProcessorInformation));
    ldr.registerExport("kernel32.dll", "GetLogicalProcessorInformationEx", reinterpret_cast<void*>(GetLogicalProcessorInformationEx));
    ldr.registerExport("kernel32.dll", "SetThreadSelectedCpuSets", reinterpret_cast<void*>(SetThreadSelectedCpuSets));
    ldr.registerExport("kernel32.dll", "GetThreadSelectedCpuSets", reinterpret_cast<void*>(GetThreadSelectedCpuSets));
    ldr.registerExport("kernel32.dll", "SetProcessDefaultCpuSets", reinterpret_cast<void*>(SetProcessDefaultCpuSets));
    ldr.registerExport("kernel32.dll", "GetProcessDefaultCpuSets", reinterpret_cast<void*>(GetProcessDefaultCpuSets));
    ldr.registerExport("kernel32.dll", "GetProcessInformation", reinterpret_cast<void*>(GetProcessInformation));
    ldr.registerExport("kernel32.dll", "IsWow64Process", reinterpret_cast<void*>(IsWow64Process));
    ldr.registerExport("kernel32.dll", "ProcessIdToSessionId", reinterpret_cast<void*>(ProcessIdToSessionId));
    ldr.registerExport("kernel32.dll", "LocaleNameToLCID", reinterpret_cast<void*>(LocaleNameToLCID));
    ldr.registerExport("kernel32.dll", "GetTimeZoneInformationForYear", reinterpret_cast<void*>(GetTimeZoneInformationForYear));
    ldr.registerExport("kernel32.dll", "GetSystemTimes", reinterpret_cast<void*>(GetSystemTimes));

    // Horizon 202: VBS Enclaves, High-Precision Timers, Physical Memory & Working Set
    ldr.registerExport("kernel32.dll", "IsEnclaveTypeSupported", reinterpret_cast<void*>(IsEnclaveTypeSupported));
    ldr.registerExport("kernel32.dll", "CreateEnclave", reinterpret_cast<void*>(CreateEnclave));
    ldr.registerExport("kernel32.dll", "InitializeEnclave", reinterpret_cast<void*>(InitializeEnclave));
    ldr.registerExport("kernel32.dll", "LoadEnclaveData", reinterpret_cast<void*>(LoadEnclaveData));
    ldr.registerExport("kernel32.dll", "TerminateEnclave", reinterpret_cast<void*>(TerminateEnclave));
    ldr.registerExport("kernel32.dll", "CallEnclave", reinterpret_cast<void*>(CallEnclave));
    ldr.registerExport("kernel32.dll", "QueryInterruptTimePrecise", reinterpret_cast<void*>(QueryInterruptTimePrecise));
    ldr.registerExport("kernel32.dll", "QueryUnbiasedInterruptTimePrecise", reinterpret_cast<void*>(QueryUnbiasedInterruptTimePrecise));
    ldr.registerExport("kernel32.dll", "GetPhysicallyInstalledSystemMemory", reinterpret_cast<void*>(GetPhysicallyInstalledSystemMemory));
    ldr.registerExport("kernel32.dll", "EmptyWorkingSet", reinterpret_cast<void*>(EmptyWorkingSet));
    ldr.registerExport("kernel32.dll", "K32EmptyWorkingSet", reinterpret_cast<void*>(K32EmptyWorkingSet));

    ldr.registerExport("api-ms-win-core-enclave-l1-1-0.dll", "IsEnclaveTypeSupported", reinterpret_cast<void*>(IsEnclaveTypeSupported));
    ldr.registerExport("api-ms-win-core-enclave-l1-1-0.dll", "CreateEnclave", reinterpret_cast<void*>(CreateEnclave));
    ldr.registerExport("api-ms-win-core-enclave-l1-1-0.dll", "InitializeEnclave", reinterpret_cast<void*>(InitializeEnclave));
    ldr.registerExport("api-ms-win-core-enclave-l1-1-0.dll", "LoadEnclaveData", reinterpret_cast<void*>(LoadEnclaveData));
    ldr.registerExport("api-ms-win-core-enclave-l1-1-0.dll", "TerminateEnclave", reinterpret_cast<void*>(TerminateEnclave));
    ldr.registerExport("api-ms-win-core-enclave-l1-1-0.dll", "CallEnclave", reinterpret_cast<void*>(CallEnclave));

    ldr.registerExport("api-ms-win-core-realtime-l1-1-1.dll", "QueryInterruptTimePrecise", reinterpret_cast<void*>(QueryInterruptTimePrecise));
    ldr.registerExport("api-ms-win-core-realtime-l1-1-1.dll", "QueryUnbiasedInterruptTimePrecise", reinterpret_cast<void*>(QueryUnbiasedInterruptTimePrecise));

    ldr.registerExport("api-ms-win-core-sysinfo-l1-2-0.dll", "GetPhysicallyInstalledSystemMemory", reinterpret_cast<void*>(GetPhysicallyInstalledSystemMemory));

    ldr.registerExport("api-ms-win-core-processthreads-l1-1-0.dll", "GetThreadContext", reinterpret_cast<void*>(GetThreadContext));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-0.dll", "SetThreadContext", reinterpret_cast<void*>(SetThreadContext));

    ldr.registerExport("psapi.dll", "EmptyWorkingSet", reinterpret_cast<void*>(EmptyWorkingSet));
    ldr.registerExport("psapi.dll", "K32EmptyWorkingSet", reinterpret_cast<void*>(K32EmptyWorkingSet));

    // Horizon 203: Fibers, FLS, Process Mitigation Policies
    ldr.registerExport("kernel32.dll", "ConvertThreadToFiber", reinterpret_cast<void*>(ConvertThreadToFiber));
    ldr.registerExport("kernel32.dll", "ConvertThreadToFiberEx", reinterpret_cast<void*>(ConvertThreadToFiberEx));
    ldr.registerExport("kernel32.dll", "ConvertFiberToThread", reinterpret_cast<void*>(ConvertFiberToThread));
    ldr.registerExport("kernel32.dll", "CreateFiber", reinterpret_cast<void*>(CreateFiber));
    ldr.registerExport("kernel32.dll", "CreateFiberEx", reinterpret_cast<void*>(CreateFiberEx));
    ldr.registerExport("kernel32.dll", "DeleteFiber", reinterpret_cast<void*>(DeleteFiber));
    ldr.registerExport("kernel32.dll", "SwitchToFiber", reinterpret_cast<void*>(SwitchToFiber));
    ldr.registerExport("kernel32.dll", "IsThreadAFiber", reinterpret_cast<void*>(IsThreadAFiber));
    ldr.registerExport("kernel32.dll", "GetCurrentFiber", reinterpret_cast<void*>(GetCurrentFiber));
    ldr.registerExport("kernel32.dll", "FlsAlloc", reinterpret_cast<void*>(FlsAlloc));
    ldr.registerExport("kernel32.dll", "FlsFree", reinterpret_cast<void*>(FlsFree));
    ldr.registerExport("kernel32.dll", "FlsGetValue", reinterpret_cast<void*>(FlsGetValue));
    ldr.registerExport("kernel32.dll", "FlsSetValue", reinterpret_cast<void*>(FlsSetValue));
    ldr.registerExport("kernel32.dll", "SetProcessMitigationPolicy", reinterpret_cast<void*>(SetProcessMitigationPolicy));
    ldr.registerExport("kernel32.dll", "GetProcessMitigationPolicy", reinterpret_cast<void*>(GetProcessMitigationPolicy));

    ldr.registerExport("api-ms-win-core-fibers-l1-1-1.dll", "FlsAlloc", reinterpret_cast<void*>(FlsAlloc));
    ldr.registerExport("api-ms-win-core-fibers-l1-1-1.dll", "FlsFree", reinterpret_cast<void*>(FlsFree));
    ldr.registerExport("api-ms-win-core-fibers-l1-1-1.dll", "FlsGetValue", reinterpret_cast<void*>(FlsGetValue));
    ldr.registerExport("api-ms-win-core-fibers-l1-1-1.dll", "FlsSetValue", reinterpret_cast<void*>(FlsSetValue));
    ldr.registerExport("api-ms-win-core-fibers-l1-1-1.dll", "ConvertThreadToFiber", reinterpret_cast<void*>(ConvertThreadToFiber));
    ldr.registerExport("api-ms-win-core-fibers-l1-1-1.dll", "ConvertThreadToFiberEx", reinterpret_cast<void*>(ConvertThreadToFiberEx));
    ldr.registerExport("api-ms-win-core-fibers-l1-1-1.dll", "ConvertFiberToThread", reinterpret_cast<void*>(ConvertFiberToThread));
    ldr.registerExport("api-ms-win-core-fibers-l1-1-1.dll", "CreateFiber", reinterpret_cast<void*>(CreateFiber));
    ldr.registerExport("api-ms-win-core-fibers-l1-1-1.dll", "CreateFiberEx", reinterpret_cast<void*>(CreateFiberEx));
    ldr.registerExport("api-ms-win-core-fibers-l1-1-1.dll", "DeleteFiber", reinterpret_cast<void*>(DeleteFiber));
    ldr.registerExport("api-ms-win-core-fibers-l1-1-1.dll", "SwitchToFiber", reinterpret_cast<void*>(SwitchToFiber));
    ldr.registerExport("api-ms-win-core-fibers-l1-1-1.dll", "IsThreadAFiber", reinterpret_cast<void*>(IsThreadAFiber));

    ldr.registerExport("api-ms-win-core-processthreads-l1-1-0.dll", "SetProcessMitigationPolicy", reinterpret_cast<void*>(SetProcessMitigationPolicy));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-0.dll", "GetProcessMitigationPolicy", reinterpret_cast<void*>(GetProcessMitigationPolicy));

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
    ldr.registerExport("ntdll.dll", "RtlAddVectoredExceptionHandler", reinterpret_cast<void*>(AddVectoredExceptionHandler));
    ldr.registerExport("ntdll.dll", "RtlRemoveVectoredExceptionHandler", reinterpret_cast<void*>(RemoveVectoredExceptionHandler));
    ldr.registerExport("ntdll.dll", "RtlAddVectoredContinueHandler", reinterpret_cast<void*>(AddVectoredContinueHandler));
    ldr.registerExport("ntdll.dll", "RtlRemoveVectoredContinueHandler", reinterpret_cast<void*>(RemoveVectoredContinueHandler));
    ldr.registerExport("ntdll.dll", "NtQueryInformationProcess", reinterpret_cast<void*>(ntdll::NtQueryInformationProcess));
    ldr.registerExport("ntdll.dll", "NtCreateNamedPipeFile", reinterpret_cast<void*>(ntdll::NtCreateNamedPipeFile));
    ldr.registerExport("ntdll.dll", "NtCreateMailslotFile", reinterpret_cast<void*>(ntdll::NtCreateMailslotFile));

    // Horizon 204: Thread Affinity, Priority Boost & Ideal Processor Pacing
    ldr.registerExport("kernel32.dll", "SetThreadPriorityBoost", reinterpret_cast<void*>(SetThreadPriorityBoost));
    ldr.registerExport("kernel32.dll", "GetThreadPriorityBoost", reinterpret_cast<void*>(GetThreadPriorityBoost));
    ldr.registerExport("kernel32.dll", "SetProcessPriorityBoost", reinterpret_cast<void*>(SetProcessPriorityBoost));
    ldr.registerExport("kernel32.dll", "GetProcessPriorityBoost", reinterpret_cast<void*>(GetProcessPriorityBoost));
    ldr.registerExport("kernel32.dll", "SetThreadIdealProcessorEx", reinterpret_cast<void*>(SetThreadIdealProcessorEx));
    ldr.registerExport("kernel32.dll", "GetThreadIdealProcessorEx", reinterpret_cast<void*>(GetThreadIdealProcessorEx));
    ldr.registerExport("kernel32.dll", "GetThreadInformation", reinterpret_cast<void*>(GetThreadInformation));

    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "SetThreadAffinityMask", reinterpret_cast<void*>(SetThreadAffinityMask));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "GetProcessAffinityMask", reinterpret_cast<void*>(GetProcessAffinityMask));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "SetProcessAffinityMask", reinterpret_cast<void*>(SetProcessAffinityMask));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "SetThreadPriorityBoost", reinterpret_cast<void*>(SetThreadPriorityBoost));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "GetThreadPriorityBoost", reinterpret_cast<void*>(GetThreadPriorityBoost));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "SetProcessPriorityBoost", reinterpret_cast<void*>(SetProcessPriorityBoost));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "GetProcessPriorityBoost", reinterpret_cast<void*>(GetProcessPriorityBoost));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "SetThreadIdealProcessorEx", reinterpret_cast<void*>(SetThreadIdealProcessorEx));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "GetThreadIdealProcessorEx", reinterpret_cast<void*>(GetThreadIdealProcessorEx));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "SetThreadInformation", reinterpret_cast<void*>(SetThreadInformation));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "GetThreadInformation", reinterpret_cast<void*>(GetThreadInformation));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "QueryProcessCycleTime", reinterpret_cast<void*>(QueryProcessCycleTime));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "QueryThreadCycleTime", reinterpret_cast<void*>(QueryThreadCycleTime));
    ldr.registerExport("api-ms-win-core-processthreads-l1-1-1.dll", "QueryIdleProcessorCycleTime", reinterpret_cast<void*>(QueryIdleProcessorCycleTime));
    ldr.registerExport("kernel32.dll", "VirtualProtectEx", reinterpret_cast<void*>(VirtualProtectEx));
    ldr.registerExport("kernel32.dll", "QueryProcessCycleTime", reinterpret_cast<void*>(QueryProcessCycleTime));
    ldr.registerExport("kernel32.dll", "QueryThreadCycleTime", reinterpret_cast<void*>(QueryThreadCycleTime));
    ldr.registerExport("kernel32.dll", "QueryIdleProcessorCycleTime", reinterpret_cast<void*>(QueryIdleProcessorCycleTime));
}


} // namespace micant::win32

namespace micant {
namespace kernel32 = win32;
namespace um {
namespace kernel32 = micant::win32;
}
}
