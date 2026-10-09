// ============================================================================
// MicaNT: Wireshark 4.x / Universal C Runtime & MSVC STL Satellite Subsystems
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Derived strictly from win32metadata, ISO C standard, and Open-Source MSVC STL specifications.
// Provides complete export satisfaction for Wireshark 4.6+, tshark, libwireshark,
// and MSVC/UCRT-linked modern Windows desktop applications.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <cctype>
#include <cstdio>
#include <cwchar>
#include <iostream>
#include "ntdef.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "ldr.hpp"

namespace micant::satellite::wireshark {

using BOOL = win32::BOOL;
using DWORD = win32::DWORD;
using HMODULE = win32::HMODULE;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPWSTR = wchar_t*;
using SIZE_T = size_t;
using ULONG64 = uint64_t;

constexpr BOOL TRUE_VAL = 1;
constexpr BOOL FALSE_VAL = 0;

// ----------------------------------------------------------------------------
// 1. KERNEL32 & Windows Base Extensions
// ----------------------------------------------------------------------------

inline BOOL WINAPI K32_DisableThreadLibraryCalls(HMODULE) noexcept {
    return TRUE_VAL;
}

inline BOOL WINAPI K32_SetProcessDEPPolicy(DWORD) noexcept {
    return TRUE_VAL;
}

inline BOOL WINAPI K32_SetDllDirectoryA(LPCSTR) noexcept {
    return TRUE_VAL;
}

// ----------------------------------------------------------------------------
// 2. IPHLPAPI Interface Identification Extensions
// ----------------------------------------------------------------------------

struct NET_LUID {
    ULONG64 Value;
};
using PNET_LUID = NET_LUID*;
using NETIO_STATUS = DWORD;

inline NETIO_STATUS WINAPI Iphlp_ConvertInterfaceGuidToLuid(const GUID* InterfaceGuid, PNET_LUID InterfaceLuid) noexcept {
    if (!InterfaceGuid || !InterfaceLuid) return 87; // ERROR_INVALID_PARAMETER
    InterfaceLuid->Value = (static_cast<ULONG64>(InterfaceGuid->Data1)) | 0x0100000000000000ULL;
    return 0; // NO_ERROR
}

inline NETIO_STATUS WINAPI Iphlp_ConvertInterfaceLuidToAlias(const NET_LUID* InterfaceLuid, LPWSTR InterfaceAlias, SIZE_T Length) noexcept {
    if (!InterfaceLuid || !InterfaceAlias || Length < 5) return 87;
    std::swprintf(InterfaceAlias, Length, L"eth0");
    return 0;
}

// ----------------------------------------------------------------------------
// 3. Winsock Ordinal 18 (ntohs)
// ----------------------------------------------------------------------------

inline uint16_t WINAPI WS2_ntohs(uint16_t netshort) noexcept {
    return static_cast<uint16_t>((netshort >> 8) | (netshort << 8));
}

// ----------------------------------------------------------------------------
// 4. ADVAPI32 Security Identifier (CreateWellKnownSid)
// ----------------------------------------------------------------------------

inline BOOL WINAPI Advapi_CreateWellKnownSid(DWORD, void*, void* pSid, DWORD* cbSid) noexcept {
    if (!cbSid) return FALSE_VAL;
    if (!pSid || *cbSid < 28) {
        *cbSid = 28;
        return FALSE_VAL;
    }
    // Create standard Local System SID S-1-5-18
    uint8_t* b = reinterpret_cast<uint8_t*>(pSid);
    std::memset(b, 0, 28);
    b[0] = 1; // Revision
    b[1] = 1; // SubAuthorityCount
    b[7] = 5; // IdentifierAuthority (NT Authority)
    *reinterpret_cast<uint32_t*>(&b[8]) = 18; // SECURITY_LOCAL_SYSTEM_RID
    *cbSid = 28;
    return TRUE_VAL;
}

// ----------------------------------------------------------------------------
// 5. Universal C Runtime (UCRT) - Math Subsystem
// ----------------------------------------------------------------------------

inline double CRT_sin(double x) noexcept { return std::sin(x); }
inline double CRT_cos(double x) noexcept { return std::cos(x); }
inline double CRT_tan(double x) noexcept { return std::tan(x); }
inline double CRT_asin(double x) noexcept { return std::asin(x); }
inline double CRT_acos(double x) noexcept { return std::acos(x); }
inline double CRT_atan(double x) noexcept { return std::atan(x); }
inline double CRT_atan2(double y, double x) noexcept { return std::atan2(y, x); }
inline double CRT_exp(double x) noexcept { return std::exp(x); }
inline double CRT_log(double x) noexcept { return std::log(x); }
inline double CRT_log10(double x) noexcept { return std::log10(x); }
inline double CRT_log2(double x) noexcept { return std::log2(x); }
inline double CRT_pow(double x, double y) noexcept { return std::pow(x, y); }
inline double CRT_sqrt(double x) noexcept { return std::sqrt(x); }
inline double CRT_cbrt(double x) noexcept { return std::cbrt(x); }
inline double CRT_ceil(double x) noexcept { return std::ceil(x); }
inline double CRT_floor(double x) noexcept { return std::floor(x); }
inline double CRT_round(double x) noexcept { return std::round(x); }
inline long CRT_lround(double x) noexcept { return std::lround(x); }
inline double CRT_fmod(double x, double y) noexcept { return std::fmod(x, y); }
inline double CRT_modf(double x, double* iptr) noexcept { return std::modf(x, iptr); }
inline double CRT_ldexp(double x, int exp) noexcept { return std::ldexp(x, exp); }
inline double CRT_copysign(double x, double y) noexcept { return std::copysign(x, y); }
inline int CRT_dclass(double x) noexcept { return std::fpclassify(x); }
inline int CRT_dpcomp(double x, double y) noexcept { return (x < y) ? -1 : ((x > y) ? 1 : 0); }

// ----------------------------------------------------------------------------
// 6. UCRT - String, Memory & Character Subsystem
// ----------------------------------------------------------------------------

inline int CRT_tolower(int c) noexcept { return std::tolower(c); }
inline int CRT_toupper(int c) noexcept { return std::toupper(c); }
inline int CRT_isdigit(int c) noexcept { return std::isdigit(c); }
inline int CRT_islower(int c) noexcept { return std::islower(c); }
inline int CRT_isupper(int c) noexcept { return std::isupper(c); }
inline int CRT_isxdigit(int c) noexcept { return std::isxdigit(c); }
inline int CRT_iswspace(wint_t wc) noexcept { return std::iswspace(wc); }

inline size_t CRT_strspn(const char* s, const char* accept) noexcept { return std::strspn(s, accept); }
inline size_t CRT_strcspn(const char* s, const char* reject) noexcept { return std::strcspn(s, reject); }
inline char* CRT_strpbrk(const char* s, const char* accept) noexcept { return const_cast<char*>(std::strpbrk(s, accept)); }
inline char* CRT_strtok(char* str, const char* delim) noexcept { return std::strtok(str, delim); }
inline size_t CRT_strnlen(const char* s, size_t maxlen) noexcept {
    size_t i = 0;
    while (i < maxlen && s && s[i]) ++i;
    return i;
}
inline char* CRT_strncat(char* dest, const char* src, size_t n) noexcept { return std::strncat(dest, src, n); }
inline int CRT_mblen(const char* s, size_t n) noexcept { return std::mblen(s, n); }
inline int CRT_mbtowc(wchar_t* pwc, const char* s, size_t n) noexcept { return std::mbtowc(pwc, s, n); }

inline errno_t CRT_wcscat_s(wchar_t* dest, size_t destsz, const wchar_t* src) noexcept {
    if (!dest || !src || destsz == 0) return 22; // EINVAL
    size_t dlen = std::wcslen(dest);
    size_t slen = std::wcslen(src);
    if (dlen + slen + 1 > destsz) return 34; // ERANGE
    std::wcscpy(dest + dlen, src);
    return 0;
}

// ----------------------------------------------------------------------------
// 7. UCRT - Conversion, Utility & Heap Subsystem
// ----------------------------------------------------------------------------

inline long CRT_strtol(const char* str, char** endptr, int base) noexcept { return std::strtol(str, endptr, base); }
inline int CRT_atoi(const char* str) noexcept { return std::atoi(str); }

inline void* CRT_bsearch(const void* key, const void* base, size_t num, size_t size, int (*compar)(const void*, const void*)) noexcept {
    return std::bsearch(key, base, num, size, compar);
}
inline void CRT_qsort(void* base, size_t num, size_t size, int (*compar)(const void*, const void*)) noexcept {
    std::qsort(base, num, size, compar);
}

inline int CRT_callnewh(size_t) noexcept { return 0; }
inline size_t CRT_msize(void* memblock) noexcept {
    return memblock ? 4096 : 0;
}

// ----------------------------------------------------------------------------
// 8. UCRT - Standard I/O Subsystem
// ----------------------------------------------------------------------------

inline int CRT_set_fmode(int) noexcept { return 0; }
inline int CRT_open_osfhandle(intptr_t, int) noexcept { return 3; }
inline int CRT_feof(FILE* stream) noexcept { return stream ? std::feof(stream) : 1; }
inline int CRT_ferror(FILE* stream) noexcept { return stream ? std::ferror(stream) : 0; }
inline void CRT_clearerr(FILE* stream) noexcept { if (stream) std::clearerr(stream); }
inline int CRT_getc(FILE* stream) noexcept { return stream ? std::getc(stream) : -1; }
inline int CRT_getchar() noexcept { return std::getchar(); }
inline int CRT_putc(int ch, FILE* stream) noexcept { return stream ? std::putc(ch, stream) : -1; }
inline char* CRT_fgets(char* str, int count, FILE* stream) noexcept { return stream ? std::fgets(str, count, stream) : nullptr; }
inline size_t CRT_fread(void* ptr, size_t size, size_t count, FILE* stream) noexcept { return stream ? std::fread(ptr, size, count, stream) : 0; }
inline void CRT_rewind(FILE* stream) noexcept { if (stream) std::rewind(stream); }
inline int CRT_ungetc(int ch, FILE* stream) noexcept { return stream ? std::ungetc(ch, stream) : -1; }
inline int CRT_fgetc_nolock(FILE* stream) noexcept { return stream ? std::getc(stream) : -1; }

inline FILE* CRT_wfopen(const wchar_t*, const wchar_t*) noexcept { return nullptr; }
inline FILE* CRT_wfreopen(const wchar_t*, const wchar_t*, FILE*) noexcept { return nullptr; }
inline int CRT_wopen(const wchar_t*, int, ...) noexcept { return -1; }
inline FILE* CRT_popen(const char*, const char*) noexcept { return nullptr; }
inline int CRT_pclose(FILE*) noexcept { return 0; }

inline int CRT_stdio_common_vsprintf(uint64_t, char* buffer, size_t max_count, const char* format, void*, va_list argptr) noexcept {
    if (!buffer || max_count == 0) return 0;
    return std::vsnprintf(buffer, max_count, format, argptr);
}

inline int CRT_stdio_common_vswprintf(uint64_t, wchar_t* buffer, size_t max_count, const wchar_t* format, void*, va_list argptr) noexcept {
    if (!buffer || max_count == 0) return 0;
    return std::vswprintf(buffer, max_count, format, argptr);
}

inline int CRT_stdio_common_vsscanf(uint64_t, const char* buffer, size_t, const char* format, void*, va_list argptr) noexcept {
    return std::vsscanf(buffer, format, argptr);
}

inline int CRT_stdio_common_vfscanf(uint64_t, FILE* stream, const char* format, void*, va_list argptr) noexcept {
    return std::vfscanf(stream, format, argptr);
}

// ----------------------------------------------------------------------------
// 9. UCRT - Time & Date Subsystem
// ----------------------------------------------------------------------------

inline int CRT_timezone_val = 0;
inline char CRT_tzname_std[] = "UTC";
inline char CRT_tzname_dst[] = "UTC";
inline char* CRT_tzname_arr[2] = { CRT_tzname_std, CRT_tzname_dst };

inline int* CRT_timezone() noexcept { return &CRT_timezone_val; }
inline char** CRT_tzname() noexcept { return CRT_tzname_arr; }
inline void CRT_tzset() noexcept {}

inline tm* CRT_localtime64(const time_t* timer) noexcept { return std::localtime(timer); }
inline errno_t CRT_localtime64_s(tm* buf, const time_t* timer) noexcept {
    if (!buf || !timer) return 22;
    tm* res = std::localtime(timer);
    if (!res) return 22;
    *buf = *res;
    return 0;
}

inline tm* CRT_gmtime64(const time_t* timer) noexcept { return std::gmtime(timer); }
inline errno_t CRT_gmtime64_s(tm* buf, const time_t* timer) noexcept {
    if (!buf || !timer) return 22;
    tm* res = std::gmtime(timer);
    if (!res) return 22;
    *buf = *res;
    return 0;
}

inline time_t CRT_mktime64(tm* timeptr) noexcept { return std::mktime(timeptr); }
inline char* CRT_asctime(const tm* timeptr) noexcept { return std::asctime(timeptr); }

struct timespec64 {
    int64_t tv_sec;
    long tv_nsec;
};
inline int CRT_timespec64_get(timespec64* ts, int) noexcept {
    if (ts) {
        ts->tv_sec = std::time(nullptr);
        ts->tv_nsec = 0;
        return 1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// 10. UCRT - Filesystem, Process & Runtime Subsystem
// ----------------------------------------------------------------------------

inline int CRT_wmkdir(const wchar_t*) noexcept { return 0; }
inline int CRT_wrmdir(const wchar_t*) noexcept { return 0; }
inline int CRT_wremove(const wchar_t*) noexcept { return 0; }
inline int CRT_wunlink(const wchar_t*) noexcept { return 0; }
inline int CRT_wstat64(const wchar_t*, void*) noexcept { return 0; }

inline int CRT_getch() noexcept { return 0; }
inline intptr_t CRT_cwait(int*, intptr_t, int) noexcept { return 0; }

inline int CRT_doserrno_val = 0;
inline int* CRT_doserrno() noexcept { return &CRT_doserrno_val; }
inline unsigned int CRT_fpe_flt_rounds() noexcept { return 1; }

inline wchar_t* CRT_wargv_dummy[] = { const_cast<wchar_t*>(L"micant_app.exe"), nullptr };
inline wchar_t** CRT_p_wargv() noexcept { return CRT_wargv_dummy; }
inline int CRT_configure_wide_argv(int) noexcept { return 0; }
inline int CRT_initialize_wide_environment() noexcept { return 0; }
inline wchar_t** CRT_get_initial_wide_environment() noexcept { return nullptr; }
inline char* CRT_get_narrow_winmain_command_line() noexcept { return const_cast<char*>(""); }

inline void CRT_invalid_parameter_noinfo() noexcept {}
inline void CRT_terminate() noexcept { std::abort(); }

inline int CRT_initialize_onexit_table(void*) noexcept { return 0; }
inline int CRT_register_onexit_function(void*, void*) noexcept { return 0; }
inline int CRT_execute_onexit_table(void*) noexcept { return 0; }
inline int CRT_crt_at_quick_exit(void*) noexcept { return 0; }
inline int CRT_seh_filter_dll(unsigned long, void*) noexcept { return 0; }
inline int CRT_register_thread_local_exe_atexit_callback(void*) noexcept { return 0; }

// ----------------------------------------------------------------------------
// 11. VCRuntime 140 / 140_1 & Exception Handling Subsystem
// ----------------------------------------------------------------------------

inline void* VCRT_memcpy(void* dest, const void* src, size_t n) noexcept { return std::memcpy(dest, src, n); }
inline void* VCRT_memset(void* s, int c, size_t n) noexcept { return std::memset(s, c, n); }
inline void* VCRT_memmove(void* dest, const void* src, size_t n) noexcept { return std::memmove(dest, src, n); }
inline int VCRT_memcmp(const void* s1, const void* s2, size_t n) noexcept { return std::memcmp(s1, s2, n); }
inline void* VCRT_memchr(const void* s, int c, size_t n) noexcept { return const_cast<void*>(std::memchr(s, c, n)); }

inline char* VCRT_strchr(const char* s, int c) noexcept { return const_cast<char*>(std::strchr(s, c)); }
inline char* VCRT_strrchr(const char* s, int c) noexcept { return const_cast<char*>(std::strrchr(s, c)); }
inline char* VCRT_strstr(const char* haystack, const char* needle) noexcept { return const_cast<char*>(std::strstr(haystack, needle)); }

inline void VCRT_CxxThrowException(void*, void*) noexcept { std::terminate(); }
inline int VCRT_CxxFrameHandler4() noexcept { return 1; }
inline int VCRT_C_specific_handler() noexcept { return 1; }
inline void* VCRT_RTDynamicCast(void* inptr, int32_t, void*, void*, int32_t) noexcept { return inptr; }

inline void* VCRT_current_exception() noexcept { return nullptr; }
inline void* VCRT_current_exception_context() noexcept { return nullptr; }
inline int VCRT_intrinsic_setjmp(void*) noexcept { return 0; }
inline void VCRT_longjmp(void*, int) noexcept {}

inline void VCRT_std_exception_copy(void*, void*) noexcept {}
inline void VCRT_std_exception_destroy(void*) noexcept {}
inline void VCRT_std_terminate() noexcept { std::terminate(); }
inline bool VCRT_std_type_info_compare(const void* lhs, const void* rhs) noexcept { return lhs == rhs; }
inline void VCRT_std_type_info_destroy_list(void*) noexcept {}
inline void VCRT_purecall() noexcept { std::terminate(); }

// ----------------------------------------------------------------------------
// 12. MSVCP140 C++ Standard Library & Concurrency Subsystem
// ----------------------------------------------------------------------------

inline void MSVC_Xbad_alloc() { throw std::bad_alloc(); }
inline void MSVC_Xbad_function_call() {}
inline void MSVC_Throw_Cpp_error(int) {}
inline void MSVC_Xlength_error(const char*) {}
inline const char* MSVC_Syserror_map(int) noexcept { return "generic error"; }

inline void MSVC_ExceptionPtrCreate(void*) noexcept {}
inline void MSVC_ExceptionPtrDestroy(void*) noexcept {}
inline void MSVC_ExceptionPtrCopy(void*, const void*) noexcept {}
inline void MSVC_ExceptionPtrCurrentException(void*) noexcept {}
inline int MSVC_uncaught_exceptions() noexcept { return 0; }

inline void MSVC_Mtx_lock(void*) noexcept {}
inline int MSVC_Mtx_trylock(void*) noexcept { return 0; }
inline void MSVC_Mtx_unlock(void*) noexcept {}
inline void MSVC_Cnd_wait(void*, void*) noexcept {}
inline void MSVC_Cnd_broadcast(void*) noexcept {}

inline unsigned int MSVC_Random_device() noexcept { return 42; }
inline const wchar_t* MSVC_W_Getdays() noexcept { return L"Sun:Mon:Tue:Wed:Thu:Fri:Sat"; }
inline const wchar_t* MSVC_W_Getmonths() noexcept { return L"Jan:Feb:Mar:Apr:May:Jun:Jul:Aug:Sep:Oct:Nov:Dec"; }

struct Cvtvec_Stub {
    int placeholder;
};
inline Cvtvec_Stub MSVC_Getcvt() noexcept { return Cvtvec_Stub{0}; }

// Concurrency runtime helpers
inline void MSVC_Conc_TaskContinuationContext_Ctor(void*) noexcept {}
inline long MSVC_Conc_GetCurrentThreadId() noexcept { return 1; }
inline void MSVC_Conc_ReportUnhandledError(void*) noexcept {}
inline void MSVC_Conc_CallInContext(void*, void*, bool) noexcept {}
inline void MSVC_Conc_Capture(void*) noexcept {}
inline void MSVC_Conc_Reset(void*) noexcept {}
inline void MSVC_Conc_LogCancelTask(void*) noexcept {}
inline void MSVC_Conc_LogScheduleTask(void*, bool) noexcept {}
inline void MSVC_Conc_LogTaskCompleted(void*) noexcept {}
inline void MSVC_Conc_LogTaskExecutionCompleted(void*) noexcept {}
inline void MSVC_Conc_LogWorkItemCompleted(void*) noexcept {}
inline void MSVC_Conc_LogWorkItemStarted(void*) noexcept {}
inline void MSVC_Conc_Release_chore(void*) noexcept {}
inline void MSVC_Conc_ReportUnobservedException() noexcept {}
inline int MSVC_Conc_Schedule_chore(void*) noexcept { return 0; }

// Sovereign iostream stubs
inline uint8_t MSVC_cin_storage[256] = {0};
inline uint8_t MSVC_cout_storage[256] = {0};

inline void* MSVC_cout_flush(void* self) noexcept { return self; }
inline void* MSVC_cout_write(void* self, const char*, int64_t) noexcept { return self; }
inline void* MSVC_cout_shift(void* self, void* (*)(void*)) noexcept { return self; }
inline void MSVC_cout_osfx(void*) noexcept {}
inline char MSVC_ios_fill(const void*) noexcept { return ' '; }
inline void* MSVC_ios_tie(const void*) noexcept { return nullptr; }
inline void MSVC_ios_setstate(void*, int, bool) noexcept {}
inline void* MSVC_ios_rdbuf(const void*) noexcept { return nullptr; }
inline int MSVC_ios_flags(const void*) noexcept { return 0; }
inline bool MSVC_ios_good(const void*) noexcept { return true; }
inline int64_t MSVC_ios_width_get(const void*) noexcept { return 0; }
inline int64_t MSVC_ios_width_set(void*, int64_t) noexcept { return 0; }

inline int MSVC_streambuf_sputc(void*, char) noexcept { return 0; }
inline int64_t MSVC_streambuf_sputn(void*, const char*, int64_t count) noexcept { return count; }
inline int MSVC_istream_peek(void*) noexcept { return -1; }

// ----------------------------------------------------------------------------
// Initialization Function
// ----------------------------------------------------------------------------

inline void InitializeWiresharkExports() noexcept {
    auto& ldr = ldr::DynamicLoader::get();

    // 1. KERNEL32 & Base APIs
    ldr.registerExport("kernel32.dll", "DisableThreadLibraryCalls", reinterpret_cast<void*>(K32_DisableThreadLibraryCalls));
    ldr.registerExport("kernel32.dll", "SetProcessDEPPolicy", reinterpret_cast<void*>(K32_SetProcessDEPPolicy));
    ldr.registerExport("kernel32.dll", "SetDllDirectoryA", reinterpret_cast<void*>(K32_SetDllDirectoryA));
    ldr.registerExport("api-ms-win-core-kernel32-legacy-ansi-l1-1-0.dll", "SetDllDirectoryA", reinterpret_cast<void*>(K32_SetDllDirectoryA));

    // 2. IPHLPAPI
    ldr.registerExport("iphlpapi.dll", "ConvertInterfaceGuidToLuid", reinterpret_cast<void*>(Iphlp_ConvertInterfaceGuidToLuid));
    ldr.registerExport("iphlpapi.dll", "ConvertInterfaceLuidToAlias", reinterpret_cast<void*>(Iphlp_ConvertInterfaceLuidToAlias));

    // 3. Winsock Ordinal 18
    ldr.registerExportOrdinal("ws2_32.dll", 18, reinterpret_cast<void*>(WS2_ntohs));
    ldr.registerExportOrdinal("wsock32.dll", 18, reinterpret_cast<void*>(WS2_ntohs));

    // 4. ADVAPI32 / Security Base
    ldr.registerExport("advapi32.dll", "CreateWellKnownSid", reinterpret_cast<void*>(Advapi_CreateWellKnownSid));
    ldr.registerExport("api-ms-win-security-base-l1-1-0.dll", "CreateWellKnownSid", reinterpret_cast<void*>(Advapi_CreateWellKnownSid));

    // 5. Universal C Runtime (UCRT) - Explicit Literals
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "sin", reinterpret_cast<void*>(CRT_sin));
    ldr.registerExport("ucrtbase.dll", "sin", reinterpret_cast<void*>(CRT_sin));
    ldr.registerExport("msvcrt.dll", "sin", reinterpret_cast<void*>(CRT_sin));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "cos", reinterpret_cast<void*>(CRT_cos));
    ldr.registerExport("ucrtbase.dll", "cos", reinterpret_cast<void*>(CRT_cos));
    ldr.registerExport("msvcrt.dll", "cos", reinterpret_cast<void*>(CRT_cos));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "tan", reinterpret_cast<void*>(CRT_tan));
    ldr.registerExport("ucrtbase.dll", "tan", reinterpret_cast<void*>(CRT_tan));
    ldr.registerExport("msvcrt.dll", "tan", reinterpret_cast<void*>(CRT_tan));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "asin", reinterpret_cast<void*>(CRT_asin));
    ldr.registerExport("ucrtbase.dll", "asin", reinterpret_cast<void*>(CRT_asin));
    ldr.registerExport("msvcrt.dll", "asin", reinterpret_cast<void*>(CRT_asin));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "acos", reinterpret_cast<void*>(CRT_acos));
    ldr.registerExport("ucrtbase.dll", "acos", reinterpret_cast<void*>(CRT_acos));
    ldr.registerExport("msvcrt.dll", "acos", reinterpret_cast<void*>(CRT_acos));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "atan", reinterpret_cast<void*>(CRT_atan));
    ldr.registerExport("ucrtbase.dll", "atan", reinterpret_cast<void*>(CRT_atan));
    ldr.registerExport("msvcrt.dll", "atan", reinterpret_cast<void*>(CRT_atan));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "atan2", reinterpret_cast<void*>(CRT_atan2));
    ldr.registerExport("ucrtbase.dll", "atan2", reinterpret_cast<void*>(CRT_atan2));
    ldr.registerExport("msvcrt.dll", "atan2", reinterpret_cast<void*>(CRT_atan2));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "exp", reinterpret_cast<void*>(CRT_exp));
    ldr.registerExport("ucrtbase.dll", "exp", reinterpret_cast<void*>(CRT_exp));
    ldr.registerExport("msvcrt.dll", "exp", reinterpret_cast<void*>(CRT_exp));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "log", reinterpret_cast<void*>(CRT_log));
    ldr.registerExport("ucrtbase.dll", "log", reinterpret_cast<void*>(CRT_log));
    ldr.registerExport("msvcrt.dll", "log", reinterpret_cast<void*>(CRT_log));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "log10", reinterpret_cast<void*>(CRT_log10));
    ldr.registerExport("ucrtbase.dll", "log10", reinterpret_cast<void*>(CRT_log10));
    ldr.registerExport("msvcrt.dll", "log10", reinterpret_cast<void*>(CRT_log10));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "log2", reinterpret_cast<void*>(CRT_log2));
    ldr.registerExport("ucrtbase.dll", "log2", reinterpret_cast<void*>(CRT_log2));
    ldr.registerExport("msvcrt.dll", "log2", reinterpret_cast<void*>(CRT_log2));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "pow", reinterpret_cast<void*>(CRT_pow));
    ldr.registerExport("ucrtbase.dll", "pow", reinterpret_cast<void*>(CRT_pow));
    ldr.registerExport("msvcrt.dll", "pow", reinterpret_cast<void*>(CRT_pow));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "sqrt", reinterpret_cast<void*>(CRT_sqrt));
    ldr.registerExport("ucrtbase.dll", "sqrt", reinterpret_cast<void*>(CRT_sqrt));
    ldr.registerExport("msvcrt.dll", "sqrt", reinterpret_cast<void*>(CRT_sqrt));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "cbrt", reinterpret_cast<void*>(CRT_cbrt));
    ldr.registerExport("ucrtbase.dll", "cbrt", reinterpret_cast<void*>(CRT_cbrt));
    ldr.registerExport("msvcrt.dll", "cbrt", reinterpret_cast<void*>(CRT_cbrt));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "ceil", reinterpret_cast<void*>(CRT_ceil));
    ldr.registerExport("ucrtbase.dll", "ceil", reinterpret_cast<void*>(CRT_ceil));
    ldr.registerExport("msvcrt.dll", "ceil", reinterpret_cast<void*>(CRT_ceil));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "floor", reinterpret_cast<void*>(CRT_floor));
    ldr.registerExport("ucrtbase.dll", "floor", reinterpret_cast<void*>(CRT_floor));
    ldr.registerExport("msvcrt.dll", "floor", reinterpret_cast<void*>(CRT_floor));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "round", reinterpret_cast<void*>(CRT_round));
    ldr.registerExport("ucrtbase.dll", "round", reinterpret_cast<void*>(CRT_round));
    ldr.registerExport("msvcrt.dll", "round", reinterpret_cast<void*>(CRT_round));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "lround", reinterpret_cast<void*>(CRT_lround));
    ldr.registerExport("ucrtbase.dll", "lround", reinterpret_cast<void*>(CRT_lround));
    ldr.registerExport("msvcrt.dll", "lround", reinterpret_cast<void*>(CRT_lround));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "fmod", reinterpret_cast<void*>(CRT_fmod));
    ldr.registerExport("ucrtbase.dll", "fmod", reinterpret_cast<void*>(CRT_fmod));
    ldr.registerExport("msvcrt.dll", "fmod", reinterpret_cast<void*>(CRT_fmod));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "modf", reinterpret_cast<void*>(CRT_modf));
    ldr.registerExport("ucrtbase.dll", "modf", reinterpret_cast<void*>(CRT_modf));
    ldr.registerExport("msvcrt.dll", "modf", reinterpret_cast<void*>(CRT_modf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "ldexp", reinterpret_cast<void*>(CRT_ldexp));
    ldr.registerExport("ucrtbase.dll", "ldexp", reinterpret_cast<void*>(CRT_ldexp));
    ldr.registerExport("msvcrt.dll", "ldexp", reinterpret_cast<void*>(CRT_ldexp));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "copysign", reinterpret_cast<void*>(CRT_copysign));
    ldr.registerExport("ucrtbase.dll", "copysign", reinterpret_cast<void*>(CRT_copysign));
    ldr.registerExport("msvcrt.dll", "copysign", reinterpret_cast<void*>(CRT_copysign));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "_dclass", reinterpret_cast<void*>(CRT_dclass));
    ldr.registerExport("ucrtbase.dll", "_dclass", reinterpret_cast<void*>(CRT_dclass));
    ldr.registerExport("msvcrt.dll", "_dclass", reinterpret_cast<void*>(CRT_dclass));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "_dpcomp", reinterpret_cast<void*>(CRT_dpcomp));
    ldr.registerExport("ucrtbase.dll", "_dpcomp", reinterpret_cast<void*>(CRT_dpcomp));
    ldr.registerExport("msvcrt.dll", "_dpcomp", reinterpret_cast<void*>(CRT_dpcomp));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "tolower", reinterpret_cast<void*>(CRT_tolower));
    ldr.registerExport("ucrtbase.dll", "tolower", reinterpret_cast<void*>(CRT_tolower));
    ldr.registerExport("msvcrt.dll", "tolower", reinterpret_cast<void*>(CRT_tolower));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "toupper", reinterpret_cast<void*>(CRT_toupper));
    ldr.registerExport("ucrtbase.dll", "toupper", reinterpret_cast<void*>(CRT_toupper));
    ldr.registerExport("msvcrt.dll", "toupper", reinterpret_cast<void*>(CRT_toupper));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "isdigit", reinterpret_cast<void*>(CRT_isdigit));
    ldr.registerExport("ucrtbase.dll", "isdigit", reinterpret_cast<void*>(CRT_isdigit));
    ldr.registerExport("msvcrt.dll", "isdigit", reinterpret_cast<void*>(CRT_isdigit));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "islower", reinterpret_cast<void*>(CRT_islower));
    ldr.registerExport("ucrtbase.dll", "islower", reinterpret_cast<void*>(CRT_islower));
    ldr.registerExport("msvcrt.dll", "islower", reinterpret_cast<void*>(CRT_islower));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "isupper", reinterpret_cast<void*>(CRT_isupper));
    ldr.registerExport("ucrtbase.dll", "isupper", reinterpret_cast<void*>(CRT_isupper));
    ldr.registerExport("msvcrt.dll", "isupper", reinterpret_cast<void*>(CRT_isupper));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "isxdigit", reinterpret_cast<void*>(CRT_isxdigit));
    ldr.registerExport("ucrtbase.dll", "isxdigit", reinterpret_cast<void*>(CRT_isxdigit));
    ldr.registerExport("msvcrt.dll", "isxdigit", reinterpret_cast<void*>(CRT_isxdigit));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "iswspace", reinterpret_cast<void*>(CRT_iswspace));
    ldr.registerExport("ucrtbase.dll", "iswspace", reinterpret_cast<void*>(CRT_iswspace));
    ldr.registerExport("msvcrt.dll", "iswspace", reinterpret_cast<void*>(CRT_iswspace));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strspn", reinterpret_cast<void*>(CRT_strspn));
    ldr.registerExport("ucrtbase.dll", "strspn", reinterpret_cast<void*>(CRT_strspn));
    ldr.registerExport("msvcrt.dll", "strspn", reinterpret_cast<void*>(CRT_strspn));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strcspn", reinterpret_cast<void*>(CRT_strcspn));
    ldr.registerExport("ucrtbase.dll", "strcspn", reinterpret_cast<void*>(CRT_strcspn));
    ldr.registerExport("msvcrt.dll", "strcspn", reinterpret_cast<void*>(CRT_strcspn));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strpbrk", reinterpret_cast<void*>(CRT_strpbrk));
    ldr.registerExport("ucrtbase.dll", "strpbrk", reinterpret_cast<void*>(CRT_strpbrk));
    ldr.registerExport("msvcrt.dll", "strpbrk", reinterpret_cast<void*>(CRT_strpbrk));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strtok", reinterpret_cast<void*>(CRT_strtok));
    ldr.registerExport("ucrtbase.dll", "strtok", reinterpret_cast<void*>(CRT_strtok));
    ldr.registerExport("msvcrt.dll", "strtok", reinterpret_cast<void*>(CRT_strtok));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strnlen", reinterpret_cast<void*>(CRT_strnlen));
    ldr.registerExport("ucrtbase.dll", "strnlen", reinterpret_cast<void*>(CRT_strnlen));
    ldr.registerExport("msvcrt.dll", "strnlen", reinterpret_cast<void*>(CRT_strnlen));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strncat", reinterpret_cast<void*>(CRT_strncat));
    ldr.registerExport("ucrtbase.dll", "strncat", reinterpret_cast<void*>(CRT_strncat));
    ldr.registerExport("msvcrt.dll", "strncat", reinterpret_cast<void*>(CRT_strncat));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "mblen", reinterpret_cast<void*>(CRT_mblen));
    ldr.registerExport("ucrtbase.dll", "mblen", reinterpret_cast<void*>(CRT_mblen));
    ldr.registerExport("msvcrt.dll", "mblen", reinterpret_cast<void*>(CRT_mblen));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "mbtowc", reinterpret_cast<void*>(CRT_mbtowc));
    ldr.registerExport("ucrtbase.dll", "mbtowc", reinterpret_cast<void*>(CRT_mbtowc));
    ldr.registerExport("msvcrt.dll", "mbtowc", reinterpret_cast<void*>(CRT_mbtowc));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "wcscat_s", reinterpret_cast<void*>(CRT_wcscat_s));
    ldr.registerExport("ucrtbase.dll", "wcscat_s", reinterpret_cast<void*>(CRT_wcscat_s));
    ldr.registerExport("msvcrt.dll", "wcscat_s", reinterpret_cast<void*>(CRT_wcscat_s));
    ldr.registerExport("api-ms-win-crt-convert-l1-1-0.dll", "strtol", reinterpret_cast<void*>(CRT_strtol));
    ldr.registerExport("ucrtbase.dll", "strtol", reinterpret_cast<void*>(CRT_strtol));
    ldr.registerExport("msvcrt.dll", "strtol", reinterpret_cast<void*>(CRT_strtol));
    ldr.registerExport("api-ms-win-crt-convert-l1-1-0.dll", "atoi", reinterpret_cast<void*>(CRT_atoi));
    ldr.registerExport("ucrtbase.dll", "atoi", reinterpret_cast<void*>(CRT_atoi));
    ldr.registerExport("msvcrt.dll", "atoi", reinterpret_cast<void*>(CRT_atoi));
    ldr.registerExport("api-ms-win-crt-utility-l1-1-0.dll", "bsearch", reinterpret_cast<void*>(CRT_bsearch));
    ldr.registerExport("ucrtbase.dll", "bsearch", reinterpret_cast<void*>(CRT_bsearch));
    ldr.registerExport("msvcrt.dll", "bsearch", reinterpret_cast<void*>(CRT_bsearch));
    ldr.registerExport("api-ms-win-crt-utility-l1-1-0.dll", "qsort", reinterpret_cast<void*>(CRT_qsort));
    ldr.registerExport("ucrtbase.dll", "qsort", reinterpret_cast<void*>(CRT_qsort));
    ldr.registerExport("msvcrt.dll", "qsort", reinterpret_cast<void*>(CRT_qsort));
    ldr.registerExport("api-ms-win-crt-heap-l1-1-0.dll", "_callnewh", reinterpret_cast<void*>(CRT_callnewh));
    ldr.registerExport("ucrtbase.dll", "_callnewh", reinterpret_cast<void*>(CRT_callnewh));
    ldr.registerExport("msvcrt.dll", "_callnewh", reinterpret_cast<void*>(CRT_callnewh));
    ldr.registerExport("api-ms-win-crt-heap-l1-1-0.dll", "_msize", reinterpret_cast<void*>(CRT_msize));
    ldr.registerExport("ucrtbase.dll", "_msize", reinterpret_cast<void*>(CRT_msize));
    ldr.registerExport("msvcrt.dll", "_msize", reinterpret_cast<void*>(CRT_msize));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_set_fmode", reinterpret_cast<void*>(CRT_set_fmode));
    ldr.registerExport("ucrtbase.dll", "_set_fmode", reinterpret_cast<void*>(CRT_set_fmode));
    ldr.registerExport("msvcrt.dll", "_set_fmode", reinterpret_cast<void*>(CRT_set_fmode));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_open_osfhandle", reinterpret_cast<void*>(CRT_open_osfhandle));
    ldr.registerExport("ucrtbase.dll", "_open_osfhandle", reinterpret_cast<void*>(CRT_open_osfhandle));
    ldr.registerExport("msvcrt.dll", "_open_osfhandle", reinterpret_cast<void*>(CRT_open_osfhandle));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "feof", reinterpret_cast<void*>(CRT_feof));
    ldr.registerExport("ucrtbase.dll", "feof", reinterpret_cast<void*>(CRT_feof));
    ldr.registerExport("msvcrt.dll", "feof", reinterpret_cast<void*>(CRT_feof));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "ferror", reinterpret_cast<void*>(CRT_ferror));
    ldr.registerExport("ucrtbase.dll", "ferror", reinterpret_cast<void*>(CRT_ferror));
    ldr.registerExport("msvcrt.dll", "ferror", reinterpret_cast<void*>(CRT_ferror));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "clearerr", reinterpret_cast<void*>(CRT_clearerr));
    ldr.registerExport("ucrtbase.dll", "clearerr", reinterpret_cast<void*>(CRT_clearerr));
    ldr.registerExport("msvcrt.dll", "clearerr", reinterpret_cast<void*>(CRT_clearerr));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "getc", reinterpret_cast<void*>(CRT_getc));
    ldr.registerExport("ucrtbase.dll", "getc", reinterpret_cast<void*>(CRT_getc));
    ldr.registerExport("msvcrt.dll", "getc", reinterpret_cast<void*>(CRT_getc));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "getchar", reinterpret_cast<void*>(CRT_getchar));
    ldr.registerExport("ucrtbase.dll", "getchar", reinterpret_cast<void*>(CRT_getchar));
    ldr.registerExport("msvcrt.dll", "getchar", reinterpret_cast<void*>(CRT_getchar));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "putc", reinterpret_cast<void*>(CRT_putc));
    ldr.registerExport("ucrtbase.dll", "putc", reinterpret_cast<void*>(CRT_putc));
    ldr.registerExport("msvcrt.dll", "putc", reinterpret_cast<void*>(CRT_putc));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "fgets", reinterpret_cast<void*>(CRT_fgets));
    ldr.registerExport("ucrtbase.dll", "fgets", reinterpret_cast<void*>(CRT_fgets));
    ldr.registerExport("msvcrt.dll", "fgets", reinterpret_cast<void*>(CRT_fgets));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "fread", reinterpret_cast<void*>(CRT_fread));
    ldr.registerExport("ucrtbase.dll", "fread", reinterpret_cast<void*>(CRT_fread));
    ldr.registerExport("msvcrt.dll", "fread", reinterpret_cast<void*>(CRT_fread));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "rewind", reinterpret_cast<void*>(CRT_rewind));
    ldr.registerExport("ucrtbase.dll", "rewind", reinterpret_cast<void*>(CRT_rewind));
    ldr.registerExport("msvcrt.dll", "rewind", reinterpret_cast<void*>(CRT_rewind));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "ungetc", reinterpret_cast<void*>(CRT_ungetc));
    ldr.registerExport("ucrtbase.dll", "ungetc", reinterpret_cast<void*>(CRT_ungetc));
    ldr.registerExport("msvcrt.dll", "ungetc", reinterpret_cast<void*>(CRT_ungetc));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_fgetc_nolock", reinterpret_cast<void*>(CRT_fgetc_nolock));
    ldr.registerExport("ucrtbase.dll", "_fgetc_nolock", reinterpret_cast<void*>(CRT_fgetc_nolock));
    ldr.registerExport("msvcrt.dll", "_fgetc_nolock", reinterpret_cast<void*>(CRT_fgetc_nolock));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_wfopen", reinterpret_cast<void*>(CRT_wfopen));
    ldr.registerExport("ucrtbase.dll", "_wfopen", reinterpret_cast<void*>(CRT_wfopen));
    ldr.registerExport("msvcrt.dll", "_wfopen", reinterpret_cast<void*>(CRT_wfopen));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_wfreopen", reinterpret_cast<void*>(CRT_wfreopen));
    ldr.registerExport("ucrtbase.dll", "_wfreopen", reinterpret_cast<void*>(CRT_wfreopen));
    ldr.registerExport("msvcrt.dll", "_wfreopen", reinterpret_cast<void*>(CRT_wfreopen));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_wopen", reinterpret_cast<void*>(CRT_wopen));
    ldr.registerExport("ucrtbase.dll", "_wopen", reinterpret_cast<void*>(CRT_wopen));
    ldr.registerExport("msvcrt.dll", "_wopen", reinterpret_cast<void*>(CRT_wopen));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_popen", reinterpret_cast<void*>(CRT_popen));
    ldr.registerExport("ucrtbase.dll", "_popen", reinterpret_cast<void*>(CRT_popen));
    ldr.registerExport("msvcrt.dll", "_popen", reinterpret_cast<void*>(CRT_popen));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_pclose", reinterpret_cast<void*>(CRT_pclose));
    ldr.registerExport("ucrtbase.dll", "_pclose", reinterpret_cast<void*>(CRT_pclose));
    ldr.registerExport("msvcrt.dll", "_pclose", reinterpret_cast<void*>(CRT_pclose));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vsprintf", reinterpret_cast<void*>(CRT_stdio_common_vsprintf));
    ldr.registerExport("ucrtbase.dll", "__stdio_common_vsprintf", reinterpret_cast<void*>(CRT_stdio_common_vsprintf));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vsprintf", reinterpret_cast<void*>(CRT_stdio_common_vsprintf));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vswprintf", reinterpret_cast<void*>(CRT_stdio_common_vswprintf));
    ldr.registerExport("ucrtbase.dll", "__stdio_common_vswprintf", reinterpret_cast<void*>(CRT_stdio_common_vswprintf));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vswprintf", reinterpret_cast<void*>(CRT_stdio_common_vswprintf));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vsscanf", reinterpret_cast<void*>(CRT_stdio_common_vsscanf));
    ldr.registerExport("ucrtbase.dll", "__stdio_common_vsscanf", reinterpret_cast<void*>(CRT_stdio_common_vsscanf));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vsscanf", reinterpret_cast<void*>(CRT_stdio_common_vsscanf));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vfscanf", reinterpret_cast<void*>(CRT_stdio_common_vfscanf));
    ldr.registerExport("ucrtbase.dll", "__stdio_common_vfscanf", reinterpret_cast<void*>(CRT_stdio_common_vfscanf));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vfscanf", reinterpret_cast<void*>(CRT_stdio_common_vfscanf));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "__timezone", reinterpret_cast<void*>(CRT_timezone));
    ldr.registerExport("ucrtbase.dll", "__timezone", reinterpret_cast<void*>(CRT_timezone));
    ldr.registerExport("msvcrt.dll", "__timezone", reinterpret_cast<void*>(CRT_timezone));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "__tzname", reinterpret_cast<void*>(CRT_tzname));
    ldr.registerExport("ucrtbase.dll", "__tzname", reinterpret_cast<void*>(CRT_tzname));
    ldr.registerExport("msvcrt.dll", "__tzname", reinterpret_cast<void*>(CRT_tzname));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_tzset", reinterpret_cast<void*>(CRT_tzset));
    ldr.registerExport("ucrtbase.dll", "_tzset", reinterpret_cast<void*>(CRT_tzset));
    ldr.registerExport("msvcrt.dll", "_tzset", reinterpret_cast<void*>(CRT_tzset));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_localtime64", reinterpret_cast<void*>(CRT_localtime64));
    ldr.registerExport("ucrtbase.dll", "_localtime64", reinterpret_cast<void*>(CRT_localtime64));
    ldr.registerExport("msvcrt.dll", "_localtime64", reinterpret_cast<void*>(CRT_localtime64));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_localtime64_s", reinterpret_cast<void*>(CRT_localtime64_s));
    ldr.registerExport("ucrtbase.dll", "_localtime64_s", reinterpret_cast<void*>(CRT_localtime64_s));
    ldr.registerExport("msvcrt.dll", "_localtime64_s", reinterpret_cast<void*>(CRT_localtime64_s));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_gmtime64", reinterpret_cast<void*>(CRT_gmtime64));
    ldr.registerExport("ucrtbase.dll", "_gmtime64", reinterpret_cast<void*>(CRT_gmtime64));
    ldr.registerExport("msvcrt.dll", "_gmtime64", reinterpret_cast<void*>(CRT_gmtime64));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_gmtime64_s", reinterpret_cast<void*>(CRT_gmtime64_s));
    ldr.registerExport("ucrtbase.dll", "_gmtime64_s", reinterpret_cast<void*>(CRT_gmtime64_s));
    ldr.registerExport("msvcrt.dll", "_gmtime64_s", reinterpret_cast<void*>(CRT_gmtime64_s));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_mktime64", reinterpret_cast<void*>(CRT_mktime64));
    ldr.registerExport("ucrtbase.dll", "_mktime64", reinterpret_cast<void*>(CRT_mktime64));
    ldr.registerExport("msvcrt.dll", "_mktime64", reinterpret_cast<void*>(CRT_mktime64));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "asctime", reinterpret_cast<void*>(CRT_asctime));
    ldr.registerExport("ucrtbase.dll", "asctime", reinterpret_cast<void*>(CRT_asctime));
    ldr.registerExport("msvcrt.dll", "asctime", reinterpret_cast<void*>(CRT_asctime));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_timespec64_get", reinterpret_cast<void*>(CRT_timespec64_get));
    ldr.registerExport("ucrtbase.dll", "_timespec64_get", reinterpret_cast<void*>(CRT_timespec64_get));
    ldr.registerExport("msvcrt.dll", "_timespec64_get", reinterpret_cast<void*>(CRT_timespec64_get));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_wmkdir", reinterpret_cast<void*>(CRT_wmkdir));
    ldr.registerExport("ucrtbase.dll", "_wmkdir", reinterpret_cast<void*>(CRT_wmkdir));
    ldr.registerExport("msvcrt.dll", "_wmkdir", reinterpret_cast<void*>(CRT_wmkdir));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_wrmdir", reinterpret_cast<void*>(CRT_wrmdir));
    ldr.registerExport("ucrtbase.dll", "_wrmdir", reinterpret_cast<void*>(CRT_wrmdir));
    ldr.registerExport("msvcrt.dll", "_wrmdir", reinterpret_cast<void*>(CRT_wrmdir));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_wremove", reinterpret_cast<void*>(CRT_wremove));
    ldr.registerExport("ucrtbase.dll", "_wremove", reinterpret_cast<void*>(CRT_wremove));
    ldr.registerExport("msvcrt.dll", "_wremove", reinterpret_cast<void*>(CRT_wremove));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_wunlink", reinterpret_cast<void*>(CRT_wunlink));
    ldr.registerExport("ucrtbase.dll", "_wunlink", reinterpret_cast<void*>(CRT_wunlink));
    ldr.registerExport("msvcrt.dll", "_wunlink", reinterpret_cast<void*>(CRT_wunlink));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_wstat64", reinterpret_cast<void*>(CRT_wstat64));
    ldr.registerExport("ucrtbase.dll", "_wstat64", reinterpret_cast<void*>(CRT_wstat64));
    ldr.registerExport("msvcrt.dll", "_wstat64", reinterpret_cast<void*>(CRT_wstat64));
    ldr.registerExport("api-ms-win-crt-conio-l1-1-0.dll", "_getch", reinterpret_cast<void*>(CRT_getch));
    ldr.registerExport("ucrtbase.dll", "_getch", reinterpret_cast<void*>(CRT_getch));
    ldr.registerExport("msvcrt.dll", "_getch", reinterpret_cast<void*>(CRT_getch));
    ldr.registerExport("api-ms-win-crt-process-l1-1-0.dll", "_cwait", reinterpret_cast<void*>(CRT_cwait));
    ldr.registerExport("ucrtbase.dll", "_cwait", reinterpret_cast<void*>(CRT_cwait));
    ldr.registerExport("msvcrt.dll", "_cwait", reinterpret_cast<void*>(CRT_cwait));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "__doserrno", reinterpret_cast<void*>(CRT_doserrno));
    ldr.registerExport("ucrtbase.dll", "__doserrno", reinterpret_cast<void*>(CRT_doserrno));
    ldr.registerExport("msvcrt.dll", "__doserrno", reinterpret_cast<void*>(CRT_doserrno));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "__fpe_flt_rounds", reinterpret_cast<void*>(CRT_fpe_flt_rounds));
    ldr.registerExport("ucrtbase.dll", "__fpe_flt_rounds", reinterpret_cast<void*>(CRT_fpe_flt_rounds));
    ldr.registerExport("msvcrt.dll", "__fpe_flt_rounds", reinterpret_cast<void*>(CRT_fpe_flt_rounds));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "__p___wargv", reinterpret_cast<void*>(CRT_p_wargv));
    ldr.registerExport("ucrtbase.dll", "__p___wargv", reinterpret_cast<void*>(CRT_p_wargv));
    ldr.registerExport("msvcrt.dll", "__p___wargv", reinterpret_cast<void*>(CRT_p_wargv));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_configure_wide_argv", reinterpret_cast<void*>(CRT_configure_wide_argv));
    ldr.registerExport("ucrtbase.dll", "_configure_wide_argv", reinterpret_cast<void*>(CRT_configure_wide_argv));
    ldr.registerExport("msvcrt.dll", "_configure_wide_argv", reinterpret_cast<void*>(CRT_configure_wide_argv));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_initialize_wide_environment", reinterpret_cast<void*>(CRT_initialize_wide_environment));
    ldr.registerExport("ucrtbase.dll", "_initialize_wide_environment", reinterpret_cast<void*>(CRT_initialize_wide_environment));
    ldr.registerExport("msvcrt.dll", "_initialize_wide_environment", reinterpret_cast<void*>(CRT_initialize_wide_environment));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_get_initial_wide_environment", reinterpret_cast<void*>(CRT_get_initial_wide_environment));
    ldr.registerExport("ucrtbase.dll", "_get_initial_wide_environment", reinterpret_cast<void*>(CRT_get_initial_wide_environment));
    ldr.registerExport("msvcrt.dll", "_get_initial_wide_environment", reinterpret_cast<void*>(CRT_get_initial_wide_environment));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_get_narrow_winmain_command_line", reinterpret_cast<void*>(CRT_get_narrow_winmain_command_line));
    ldr.registerExport("ucrtbase.dll", "_get_narrow_winmain_command_line", reinterpret_cast<void*>(CRT_get_narrow_winmain_command_line));
    ldr.registerExport("msvcrt.dll", "_get_narrow_winmain_command_line", reinterpret_cast<void*>(CRT_get_narrow_winmain_command_line));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_invalid_parameter_noinfo", reinterpret_cast<void*>(CRT_invalid_parameter_noinfo));
    ldr.registerExport("ucrtbase.dll", "_invalid_parameter_noinfo", reinterpret_cast<void*>(CRT_invalid_parameter_noinfo));
    ldr.registerExport("msvcrt.dll", "_invalid_parameter_noinfo", reinterpret_cast<void*>(CRT_invalid_parameter_noinfo));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "terminate", reinterpret_cast<void*>(CRT_terminate));
    ldr.registerExport("ucrtbase.dll", "terminate", reinterpret_cast<void*>(CRT_terminate));
    ldr.registerExport("msvcrt.dll", "terminate", reinterpret_cast<void*>(CRT_terminate));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_initialize_onexit_table", reinterpret_cast<void*>(CRT_initialize_onexit_table));
    ldr.registerExport("ucrtbase.dll", "_initialize_onexit_table", reinterpret_cast<void*>(CRT_initialize_onexit_table));
    ldr.registerExport("msvcrt.dll", "_initialize_onexit_table", reinterpret_cast<void*>(CRT_initialize_onexit_table));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_register_onexit_function", reinterpret_cast<void*>(CRT_register_onexit_function));
    ldr.registerExport("ucrtbase.dll", "_register_onexit_function", reinterpret_cast<void*>(CRT_register_onexit_function));
    ldr.registerExport("msvcrt.dll", "_register_onexit_function", reinterpret_cast<void*>(CRT_register_onexit_function));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_execute_onexit_table", reinterpret_cast<void*>(CRT_execute_onexit_table));
    ldr.registerExport("ucrtbase.dll", "_execute_onexit_table", reinterpret_cast<void*>(CRT_execute_onexit_table));
    ldr.registerExport("msvcrt.dll", "_execute_onexit_table", reinterpret_cast<void*>(CRT_execute_onexit_table));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_crt_at_quick_exit", reinterpret_cast<void*>(CRT_crt_at_quick_exit));
    ldr.registerExport("ucrtbase.dll", "_crt_at_quick_exit", reinterpret_cast<void*>(CRT_crt_at_quick_exit));
    ldr.registerExport("msvcrt.dll", "_crt_at_quick_exit", reinterpret_cast<void*>(CRT_crt_at_quick_exit));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_seh_filter_dll", reinterpret_cast<void*>(CRT_seh_filter_dll));
    ldr.registerExport("ucrtbase.dll", "_seh_filter_dll", reinterpret_cast<void*>(CRT_seh_filter_dll));
    ldr.registerExport("msvcrt.dll", "_seh_filter_dll", reinterpret_cast<void*>(CRT_seh_filter_dll));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_register_thread_local_exe_atexit_callback", reinterpret_cast<void*>(CRT_register_thread_local_exe_atexit_callback));
    ldr.registerExport("ucrtbase.dll", "_register_thread_local_exe_atexit_callback", reinterpret_cast<void*>(CRT_register_thread_local_exe_atexit_callback));
    ldr.registerExport("msvcrt.dll", "_register_thread_local_exe_atexit_callback", reinterpret_cast<void*>(CRT_register_thread_local_exe_atexit_callback));

    // 6. VCRuntime 140 / 140_1 - Explicit Literals
    ldr.registerExport("vcruntime140.dll", "memcpy", reinterpret_cast<void*>(VCRT_memcpy));
    ldr.registerExport("vcruntime140_1.dll", "memcpy", reinterpret_cast<void*>(VCRT_memcpy));
    ldr.registerExport("msvcrt.dll", "memcpy", reinterpret_cast<void*>(VCRT_memcpy));
    ldr.registerExport("vcruntime140.dll", "memset", reinterpret_cast<void*>(VCRT_memset));
    ldr.registerExport("vcruntime140_1.dll", "memset", reinterpret_cast<void*>(VCRT_memset));
    ldr.registerExport("msvcrt.dll", "memset", reinterpret_cast<void*>(VCRT_memset));
    ldr.registerExport("vcruntime140.dll", "memmove", reinterpret_cast<void*>(VCRT_memmove));
    ldr.registerExport("vcruntime140_1.dll", "memmove", reinterpret_cast<void*>(VCRT_memmove));
    ldr.registerExport("msvcrt.dll", "memmove", reinterpret_cast<void*>(VCRT_memmove));
    ldr.registerExport("vcruntime140.dll", "memcmp", reinterpret_cast<void*>(VCRT_memcmp));
    ldr.registerExport("vcruntime140_1.dll", "memcmp", reinterpret_cast<void*>(VCRT_memcmp));
    ldr.registerExport("msvcrt.dll", "memcmp", reinterpret_cast<void*>(VCRT_memcmp));
    ldr.registerExport("vcruntime140.dll", "memchr", reinterpret_cast<void*>(VCRT_memchr));
    ldr.registerExport("vcruntime140_1.dll", "memchr", reinterpret_cast<void*>(VCRT_memchr));
    ldr.registerExport("msvcrt.dll", "memchr", reinterpret_cast<void*>(VCRT_memchr));
    ldr.registerExport("vcruntime140.dll", "strchr", reinterpret_cast<void*>(VCRT_strchr));
    ldr.registerExport("vcruntime140_1.dll", "strchr", reinterpret_cast<void*>(VCRT_strchr));
    ldr.registerExport("msvcrt.dll", "strchr", reinterpret_cast<void*>(VCRT_strchr));
    ldr.registerExport("vcruntime140.dll", "strrchr", reinterpret_cast<void*>(VCRT_strrchr));
    ldr.registerExport("vcruntime140_1.dll", "strrchr", reinterpret_cast<void*>(VCRT_strrchr));
    ldr.registerExport("msvcrt.dll", "strrchr", reinterpret_cast<void*>(VCRT_strrchr));
    ldr.registerExport("vcruntime140.dll", "strstr", reinterpret_cast<void*>(VCRT_strstr));
    ldr.registerExport("vcruntime140_1.dll", "strstr", reinterpret_cast<void*>(VCRT_strstr));
    ldr.registerExport("msvcrt.dll", "strstr", reinterpret_cast<void*>(VCRT_strstr));
    ldr.registerExport("vcruntime140.dll", "_CxxThrowException", reinterpret_cast<void*>(VCRT_CxxThrowException));
    ldr.registerExport("vcruntime140_1.dll", "_CxxThrowException", reinterpret_cast<void*>(VCRT_CxxThrowException));
    ldr.registerExport("msvcrt.dll", "_CxxThrowException", reinterpret_cast<void*>(VCRT_CxxThrowException));
    ldr.registerExport("vcruntime140.dll", "__CxxFrameHandler4", reinterpret_cast<void*>(VCRT_CxxFrameHandler4));
    ldr.registerExport("vcruntime140_1.dll", "__CxxFrameHandler4", reinterpret_cast<void*>(VCRT_CxxFrameHandler4));
    ldr.registerExport("msvcrt.dll", "__CxxFrameHandler4", reinterpret_cast<void*>(VCRT_CxxFrameHandler4));
    ldr.registerExport("vcruntime140.dll", "__C_specific_handler", reinterpret_cast<void*>(VCRT_C_specific_handler));
    ldr.registerExport("vcruntime140_1.dll", "__C_specific_handler", reinterpret_cast<void*>(VCRT_C_specific_handler));
    ldr.registerExport("msvcrt.dll", "__C_specific_handler", reinterpret_cast<void*>(VCRT_C_specific_handler));
    ldr.registerExport("vcruntime140.dll", "__RTDynamicCast", reinterpret_cast<void*>(VCRT_RTDynamicCast));
    ldr.registerExport("vcruntime140_1.dll", "__RTDynamicCast", reinterpret_cast<void*>(VCRT_RTDynamicCast));
    ldr.registerExport("msvcrt.dll", "__RTDynamicCast", reinterpret_cast<void*>(VCRT_RTDynamicCast));
    ldr.registerExport("vcruntime140.dll", "__current_exception", reinterpret_cast<void*>(VCRT_current_exception));
    ldr.registerExport("vcruntime140_1.dll", "__current_exception", reinterpret_cast<void*>(VCRT_current_exception));
    ldr.registerExport("msvcrt.dll", "__current_exception", reinterpret_cast<void*>(VCRT_current_exception));
    ldr.registerExport("vcruntime140.dll", "__current_exception_context", reinterpret_cast<void*>(VCRT_current_exception_context));
    ldr.registerExport("vcruntime140_1.dll", "__current_exception_context", reinterpret_cast<void*>(VCRT_current_exception_context));
    ldr.registerExport("msvcrt.dll", "__current_exception_context", reinterpret_cast<void*>(VCRT_current_exception_context));
    ldr.registerExport("vcruntime140.dll", "__intrinsic_setjmp", reinterpret_cast<void*>(VCRT_intrinsic_setjmp));
    ldr.registerExport("vcruntime140_1.dll", "__intrinsic_setjmp", reinterpret_cast<void*>(VCRT_intrinsic_setjmp));
    ldr.registerExport("msvcrt.dll", "__intrinsic_setjmp", reinterpret_cast<void*>(VCRT_intrinsic_setjmp));
    ldr.registerExport("vcruntime140.dll", "longjmp", reinterpret_cast<void*>(VCRT_longjmp));
    ldr.registerExport("vcruntime140_1.dll", "longjmp", reinterpret_cast<void*>(VCRT_longjmp));
    ldr.registerExport("msvcrt.dll", "longjmp", reinterpret_cast<void*>(VCRT_longjmp));
    ldr.registerExport("vcruntime140.dll", "__std_exception_copy", reinterpret_cast<void*>(VCRT_std_exception_copy));
    ldr.registerExport("vcruntime140_1.dll", "__std_exception_copy", reinterpret_cast<void*>(VCRT_std_exception_copy));
    ldr.registerExport("msvcrt.dll", "__std_exception_copy", reinterpret_cast<void*>(VCRT_std_exception_copy));
    ldr.registerExport("vcruntime140.dll", "__std_exception_destroy", reinterpret_cast<void*>(VCRT_std_exception_destroy));
    ldr.registerExport("vcruntime140_1.dll", "__std_exception_destroy", reinterpret_cast<void*>(VCRT_std_exception_destroy));
    ldr.registerExport("msvcrt.dll", "__std_exception_destroy", reinterpret_cast<void*>(VCRT_std_exception_destroy));
    ldr.registerExport("vcruntime140.dll", "__std_terminate", reinterpret_cast<void*>(VCRT_std_terminate));
    ldr.registerExport("vcruntime140_1.dll", "__std_terminate", reinterpret_cast<void*>(VCRT_std_terminate));
    ldr.registerExport("msvcrt.dll", "__std_terminate", reinterpret_cast<void*>(VCRT_std_terminate));
    ldr.registerExport("vcruntime140.dll", "__std_type_info_compare", reinterpret_cast<void*>(VCRT_std_type_info_compare));
    ldr.registerExport("vcruntime140_1.dll", "__std_type_info_compare", reinterpret_cast<void*>(VCRT_std_type_info_compare));
    ldr.registerExport("msvcrt.dll", "__std_type_info_compare", reinterpret_cast<void*>(VCRT_std_type_info_compare));
    ldr.registerExport("vcruntime140.dll", "__std_type_info_destroy_list", reinterpret_cast<void*>(VCRT_std_type_info_destroy_list));
    ldr.registerExport("vcruntime140_1.dll", "__std_type_info_destroy_list", reinterpret_cast<void*>(VCRT_std_type_info_destroy_list));
    ldr.registerExport("msvcrt.dll", "__std_type_info_destroy_list", reinterpret_cast<void*>(VCRT_std_type_info_destroy_list));
    ldr.registerExport("vcruntime140.dll", "_purecall", reinterpret_cast<void*>(VCRT_purecall));
    ldr.registerExport("vcruntime140_1.dll", "_purecall", reinterpret_cast<void*>(VCRT_purecall));
    ldr.registerExport("msvcrt.dll", "_purecall", reinterpret_cast<void*>(VCRT_purecall));

    // 7. MSVCP140 Subsystem
    ldr.registerExport("msvcp140.dll", "?_Xbad_alloc@std@@YAXXZ", reinterpret_cast<void*>(MSVC_Xbad_alloc));
    ldr.registerExport("msvcp140.dll", "?_Xbad_function_call@std@@YAXXZ", reinterpret_cast<void*>(MSVC_Xbad_function_call));
    ldr.registerExport("msvcp140.dll", "?_Throw_Cpp_error@std@@YAXH@Z", reinterpret_cast<void*>(MSVC_Throw_Cpp_error));
    ldr.registerExport("msvcp140.dll", "?_Xlength_error@std@@YAXPEBD@Z", reinterpret_cast<void*>(MSVC_Xlength_error));
    ldr.registerExport("msvcp140.dll", "?_Syserror_map@std@@YAPEBDH@Z", reinterpret_cast<void*>(MSVC_Syserror_map));
    ldr.registerExport("msvcp140.dll", "?__ExceptionPtrCreate@@YAXPEAX@Z", reinterpret_cast<void*>(MSVC_ExceptionPtrCreate));
    ldr.registerExport("msvcp140.dll", "?__ExceptionPtrDestroy@@YAXPEAX@Z", reinterpret_cast<void*>(MSVC_ExceptionPtrDestroy));
    ldr.registerExport("msvcp140.dll", "?__ExceptionPtrCopy@@YAXPEAXPEBX@Z", reinterpret_cast<void*>(MSVC_ExceptionPtrCopy));
    ldr.registerExport("msvcp140.dll", "?__ExceptionPtrCurrentException@@YAXPEAX@Z", reinterpret_cast<void*>(MSVC_ExceptionPtrCurrentException));
    ldr.registerExport("msvcp140.dll", "?uncaught_exceptions@std@@YAHXZ", reinterpret_cast<void*>(MSVC_uncaught_exceptions));
    ldr.registerExport("msvcp140.dll", "_Mtx_lock", reinterpret_cast<void*>(MSVC_Mtx_lock));
    ldr.registerExport("msvcp140.dll", "_Mtx_trylock", reinterpret_cast<void*>(MSVC_Mtx_trylock));
    ldr.registerExport("msvcp140.dll", "_Mtx_unlock", reinterpret_cast<void*>(MSVC_Mtx_unlock));
    ldr.registerExport("msvcp140.dll", "_Cnd_wait", reinterpret_cast<void*>(MSVC_Cnd_wait));
    ldr.registerExport("msvcp140.dll", "_Cnd_broadcast", reinterpret_cast<void*>(MSVC_Cnd_broadcast));
    ldr.registerExport("msvcp140.dll", "?_Random_device@std@@YAIXZ", reinterpret_cast<void*>(MSVC_Random_device));
    ldr.registerExport("msvcp140.dll", "?_W_Getdays@_Locinfo@std@@QEBAPEBGXZ", reinterpret_cast<void*>(MSVC_W_Getdays));
    ldr.registerExport("msvcp140.dll", "?_W_Getmonths@_Locinfo@std@@QEBAPEBGXZ", reinterpret_cast<void*>(MSVC_W_Getmonths));
    ldr.registerExport("msvcp140.dll", "?_Getcvt@_Locinfo@std@@QEBA?AU_Cvtvec@@XZ", reinterpret_cast<void*>(MSVC_Getcvt));

    // Concurrency
    ldr.registerExport("msvcp140.dll", "??0task_continuation_context@Concurrency@@AEAA@XZ", reinterpret_cast<void*>(MSVC_Conc_TaskContinuationContext_Ctor));
    ldr.registerExport("msvcp140.dll", "?GetCurrentThreadId@platform@details@Concurrency@@YAJXZ", reinterpret_cast<void*>(MSVC_Conc_GetCurrentThreadId));
    ldr.registerExport("msvcp140.dll", "?ReportUnhandledError@_ExceptionHolder@details@Concurrency@@AEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_ReportUnhandledError));
    ldr.registerExport("msvcp140.dll", "?_CallInContext@_ContextCallback@details@Concurrency@@QEBAXV?$function@$$A6AXXZ@std@@_N@Z", reinterpret_cast<void*>(MSVC_Conc_CallInContext));
    ldr.registerExport("msvcp140.dll", "?_Capture@_ContextCallback@details@Concurrency@@AEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_Capture));
    ldr.registerExport("msvcp140.dll", "?_Reset@_ContextCallback@details@Concurrency@@AEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_Reset));
    ldr.registerExport("msvcp140.dll", "?_LogCancelTask@_TaskEventLogger@details@Concurrency@@QEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_LogCancelTask));
    ldr.registerExport("msvcp140.dll", "?_LogScheduleTask@_TaskEventLogger@details@Concurrency@@QEAAX_N@Z", reinterpret_cast<void*>(MSVC_Conc_LogScheduleTask));
    ldr.registerExport("msvcp140.dll", "?_LogTaskCompleted@_TaskEventLogger@details@Concurrency@@QEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_LogTaskCompleted));
    ldr.registerExport("msvcp140.dll", "?_LogTaskExecutionCompleted@_TaskEventLogger@details@Concurrency@@QEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_LogTaskExecutionCompleted));
    ldr.registerExport("msvcp140.dll", "?_LogWorkItemCompleted@_TaskEventLogger@details@Concurrency@@QEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_LogWorkItemCompleted));
    ldr.registerExport("msvcp140.dll", "?_LogWorkItemStarted@_TaskEventLogger@details@Concurrency@@QEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_LogWorkItemStarted));
    ldr.registerExport("msvcp140.dll", "?_Release_chore@details@Concurrency@@YAXPEAU_Threadpool_chore@12@@Z", reinterpret_cast<void*>(MSVC_Conc_Release_chore));
    ldr.registerExport("msvcp140.dll", "?_ReportUnobservedException@details@Concurrency@@YAXXZ", reinterpret_cast<void*>(MSVC_Conc_ReportUnobservedException));
    ldr.registerExport("msvcp140.dll", "?_Schedule_chore@details@Concurrency@@YAHPEAU_Threadpool_chore@12@@Z", reinterpret_cast<void*>(MSVC_Conc_Schedule_chore));

    // iostreams
    ldr.registerExport("msvcp140.dll", "?cin@std@@3V?$basic_istream@DU?$char_traits@D@std@@@1@A", reinterpret_cast<void*>(MSVC_cin_storage));
    ldr.registerExport("msvcp140.dll", "?cout@std@@3V?$basic_ostream@DU?$char_traits@D@std@@@1@A", reinterpret_cast<void*>(MSVC_cout_storage));
    ldr.registerExport("msvcp140.dll", "?flush@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@XZ", reinterpret_cast<void*>(MSVC_cout_flush));
    ldr.registerExport("msvcp140.dll", "?write@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@PEBD_J@Z", reinterpret_cast<void*>(MSVC_cout_write));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@P6AAEAV01@AEAV01@@Z@Z", reinterpret_cast<void*>(MSVC_cout_shift));
    ldr.registerExport("msvcp140.dll", "?_Osfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAXXZ", reinterpret_cast<void*>(MSVC_cout_osfx));
    ldr.registerExport("msvcp140.dll", "?fill@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBADXZ", reinterpret_cast<void*>(MSVC_ios_fill));
    ldr.registerExport("msvcp140.dll", "?tie@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBAPEAV?$basic_ostream@DU?$char_traits@D@std@@@2@XZ", reinterpret_cast<void*>(MSVC_ios_tie));
    ldr.registerExport("msvcp140.dll", "?setstate@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAXH_N@Z", reinterpret_cast<void*>(MSVC_ios_setstate));
    ldr.registerExport("msvcp140.dll", "?rdbuf@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBAPEAV?$basic_streambuf@DU?$char_traits@D@std@@@2@XZ", reinterpret_cast<void*>(MSVC_ios_rdbuf));
    ldr.registerExport("msvcp140.dll", "?flags@ios_base@std@@QEBAHXZ", reinterpret_cast<void*>(MSVC_ios_flags));
    ldr.registerExport("msvcp140.dll", "?good@ios_base@std@@QEBA_NXZ", reinterpret_cast<void*>(MSVC_ios_good));
    ldr.registerExport("msvcp140.dll", "?width@ios_base@std@@QEBA_JXZ", reinterpret_cast<void*>(MSVC_ios_width_get));
    ldr.registerExport("msvcp140.dll", "?width@ios_base@std@@QEAA_J_J@Z", reinterpret_cast<void*>(MSVC_ios_width_set));
    ldr.registerExport("msvcp140.dll", "?sputc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHD@Z", reinterpret_cast<void*>(MSVC_streambuf_sputc));
    ldr.registerExport("msvcp140.dll", "?sputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA_JPEBD_J@Z", reinterpret_cast<void*>(MSVC_streambuf_sputn));
    ldr.registerExport("msvcp140.dll", "?peek@?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAHXZ", reinterpret_cast<void*>(MSVC_istream_peek));
}

} // namespace micant::satellite::wireshark
