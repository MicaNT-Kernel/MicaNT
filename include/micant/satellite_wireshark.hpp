// ============================================================================
// MicaNT: Wireshark 4.x / Universal C Runtime & MSVC STL Satellite Subsystems
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Canonical functionality graduated directly to MicaNT Core (msvcrt.hpp,
// kernel32.hpp, advapi32.hpp, ws2_32.hpp, iphlpapi.hpp).
// This header serves as a thin forwarding bridge for test suites and subsystems.
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
#include "advapi32.hpp"
#include "ws2_32.hpp"
#include "iphlpapi.hpp"
#include "msvcrt.hpp"
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

// 1. KERNEL32 Forwarders
inline BOOL WINAPI K32_DisableThreadLibraryCalls(HMODULE h) noexcept { return win32::DisableThreadLibraryCalls(h); }
inline BOOL WINAPI K32_SetProcessDEPPolicy(DWORD d) noexcept { return win32::SetProcessDEPPolicy(d); }
inline BOOL WINAPI K32_SetDllDirectoryA(LPCSTR s) noexcept { return win32::SetDllDirectoryA(s); }

// 2. IPHLPAPI Forwarders
using NET_LUID = iphlpapi::NET_LUID;
using PNET_LUID = iphlpapi::PNET_LUID;
using NETIO_STATUS = iphlpapi::NETIO_STATUS;

inline NETIO_STATUS WINAPI Iphlp_ConvertInterfaceGuidToLuid(const GUID* g, PNET_LUID l) noexcept {
    return iphlpapi::ConvertInterfaceGuidToLuid(g, l);
}
inline NETIO_STATUS WINAPI Iphlp_ConvertInterfaceLuidToAlias(const NET_LUID* l, LPWSTR a, SIZE_T s) noexcept {
    return iphlpapi::ConvertInterfaceLuidToAlias(l, a, s);
}

// 3. Winsock Forwarders
inline uint16_t WINAPI WS2_ntohs(uint16_t s) noexcept { return ws2_32::ntohs(s); }

// 4. ADVAPI32 Forwarders
inline BOOL WINAPI Advapi_CreateWellKnownSid(DWORD t, void* c, void* s, DWORD* cb) noexcept {
    return advapi32::CreateWellKnownSid(t, c, s, cb);
}

// 5. Universal C Runtime Forwarders
using timespec64 = msvcrt::timespec64;

inline double CRT_sin(double x) noexcept { return msvcrt::CRT_sin(x); }
inline double CRT_cos(double x) noexcept { return msvcrt::CRT_cos(x); }
inline double CRT_tan(double x) noexcept { return msvcrt::CRT_tan(x); }
inline double CRT_asin(double x) noexcept { return msvcrt::CRT_asin(x); }
inline double CRT_acos(double x) noexcept { return msvcrt::CRT_acos(x); }
inline double CRT_atan(double x) noexcept { return msvcrt::CRT_atan(x); }
inline double CRT_atan2(double y, double x) noexcept { return msvcrt::CRT_atan2(y, x); }
inline double CRT_exp(double x) noexcept { return msvcrt::CRT_exp(x); }
inline double CRT_log(double x) noexcept { return msvcrt::CRT_log(x); }
inline double CRT_log10(double x) noexcept { return msvcrt::CRT_log10(x); }
inline double CRT_log2(double x) noexcept { return msvcrt::CRT_log2(x); }
inline double CRT_pow(double x, double y) noexcept { return msvcrt::CRT_pow(x, y); }
inline double CRT_sqrt(double x) noexcept { return msvcrt::CRT_sqrt(x); }
inline double CRT_cbrt(double x) noexcept { return msvcrt::CRT_cbrt(x); }
inline double CRT_ceil(double x) noexcept { return msvcrt::CRT_ceil(x); }
inline double CRT_floor(double x) noexcept { return msvcrt::CRT_floor(x); }
inline double CRT_round(double x) noexcept { return msvcrt::CRT_round(x); }
inline long CRT_lround(double x) noexcept { return msvcrt::CRT_lround(x); }
inline double CRT_fmod(double x, double y) noexcept { return msvcrt::CRT_fmod(x, y); }
inline double CRT_modf(double x, double* iptr) noexcept { return msvcrt::CRT_modf(x, iptr); }
inline double CRT_ldexp(double x, int exp) noexcept { return msvcrt::CRT_ldexp(x, exp); }
inline double CRT_copysign(double x, double y) noexcept { return msvcrt::CRT_copysign(x, y); }
inline int CRT_dclass(double x) noexcept { return msvcrt::CRT_dclass(x); }
inline int CRT_dpcomp(double x, double y) noexcept { return msvcrt::CRT_dpcomp(x, y); }

inline int CRT_tolower(int c) noexcept { return msvcrt::CRT_tolower(c); }
inline int CRT_toupper(int c) noexcept { return msvcrt::CRT_toupper(c); }
inline int CRT_isdigit(int c) noexcept { return msvcrt::CRT_isdigit(c); }
inline int CRT_islower(int c) noexcept { return msvcrt::CRT_islower(c); }
inline int CRT_isupper(int c) noexcept { return msvcrt::CRT_isupper(c); }
inline int CRT_isxdigit(int c) noexcept { return msvcrt::CRT_isxdigit(c); }
inline int CRT_iswspace(wint_t wc) noexcept { return msvcrt::CRT_iswspace(wc); }

inline size_t CRT_strspn(const char* s, const char* accept) noexcept { return msvcrt::CRT_strspn(s, accept); }
inline size_t CRT_strcspn(const char* s, const char* reject) noexcept { return msvcrt::CRT_strcspn(s, reject); }
inline char* CRT_strpbrk(const char* s, const char* accept) noexcept { return msvcrt::CRT_strpbrk(s, accept); }
inline char* CRT_strtok(char* str, const char* delim) noexcept { return msvcrt::CRT_strtok(str, delim); }
inline size_t CRT_strnlen(const char* s, size_t maxlen) noexcept { return msvcrt::CRT_strnlen(s, maxlen); }
inline char* CRT_strncat(char* dest, const char* src, size_t n) noexcept { return msvcrt::CRT_strncat(dest, src, n); }
inline int CRT_mblen(const char* s, size_t n) noexcept { return msvcrt::CRT_mblen(s, n); }
inline int CRT_mbtowc(wchar_t* pwc, const char* s, size_t n) noexcept { return msvcrt::CRT_mbtowc(pwc, s, n); }
inline int CRT_wcscat_s(wchar_t* dest, size_t destSz, const wchar_t* src) noexcept { return msvcrt::CRT_wcscat_s(dest, destSz, src); }
inline long CRT_strtol(const char* nptr, char** endptr, int base) noexcept { return msvcrt::CRT_strtol(nptr, endptr, base); }
inline int CRT_atoi(const char* nptr) noexcept { return msvcrt::CRT_atoi(nptr); }

inline void* CRT_bsearch(const void* k, const void* b, size_t num, size_t sz, int (*cmp)(const void*, const void*)) noexcept {
    return msvcrt::CRT_bsearch(k, b, num, sz, cmp);
}
inline void CRT_qsort(void* b, size_t num, size_t sz, int (*cmp)(const void*, const void*)) noexcept {
    msvcrt::CRT_qsort(b, num, sz, cmp);
}

inline int CRT_localtime64_s(struct tm* tmDest, const time_t* sourceTime) noexcept { return msvcrt::CRT_localtime64_s(tmDest, sourceTime); }
inline int CRT_gmtime64_s(struct tm* tmDest, const time_t* sourceTime) noexcept { return msvcrt::CRT_gmtime64_s(tmDest, sourceTime); }
inline int CRT_timespec64_get(timespec64* ts, int base) noexcept { return msvcrt::CRT_timespec64_get(ts, base); }

// 6. VCRuntime 140 Forwarders
inline void* VCRT_memcpy(void* dest, const void* src, size_t count) noexcept { return msvcrt::VCRT_memcpy(dest, src, count); }
inline void* VCRT_memset(void* dest, int c, size_t count) noexcept { return msvcrt::VCRT_memset(dest, c, count); }
inline void* VCRT_memmove(void* dest, const void* src, size_t count) noexcept { return msvcrt::VCRT_memmove(dest, src, count); }
inline int VCRT_memcmp(const void* buf1, const void* buf2, size_t count) noexcept { return msvcrt::VCRT_memcmp(buf1, buf2, count); }
inline void* VCRT_memchr(const void* buf, int c, size_t count) noexcept { return msvcrt::VCRT_memchr(buf, c, count); }
inline char* VCRT_strchr(const char* str, int c) noexcept { return msvcrt::VCRT_strchr(str, c); }
inline char* VCRT_strrchr(const char* str, int c) noexcept { return msvcrt::VCRT_strrchr(str, c); }
inline char* VCRT_strstr(const char* str, const char* strSearch) noexcept { return msvcrt::VCRT_strstr(str, strSearch); }

// 7. MSVCP140 Forwarders
inline void MSVC_Mtx_lock(void* mtx) noexcept { msvcrt::MSVC_Mtx_lock(mtx); }
inline int MSVC_Mtx_trylock(void* mtx) noexcept { return msvcrt::MSVC_Mtx_trylock(mtx); }
inline void MSVC_Mtx_unlock(void* mtx) noexcept { msvcrt::MSVC_Mtx_unlock(mtx); }
inline void MSVC_Cnd_wait(void* cnd, void* mtx) noexcept { msvcrt::MSVC_Cnd_wait(cnd, mtx); }
inline void MSVC_Cnd_broadcast(void* cnd) noexcept { msvcrt::MSVC_Cnd_broadcast(cnd); }
inline unsigned int MSVC_Random_device() noexcept { return msvcrt::MSVC_Random_device(); }
inline const wchar_t* MSVC_W_Getdays() noexcept { return msvcrt::MSVC_W_Getdays(); }
inline const wchar_t* MSVC_W_Getmonths() noexcept { return msvcrt::MSVC_W_Getmonths(); }

// 8. Initialization Forwarder
inline void InitializeWiresharkExports() noexcept {
    // Delegated to canonical core initializers
    msvcrt::InitializeMsvcrtSubsystemExports();
    win32::InitializeWin32SubsystemExports();
    advapi32::InitializeAdvapi32SubsystemExports();
    iphlpapi::InitializeIpHlpApiSubsystemExports();
    ws2_32::InitializeWs2_32SubsystemExports();
}

} // namespace micant::satellite::wireshark
