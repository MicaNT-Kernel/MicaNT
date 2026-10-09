// ============================================================================
// MicaNT: FileZilla 3.x / Sovereign Networking & Enterprise FTP Subsystems
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Canonical functionality graduated directly to MicaNT Core (msvcrt.hpp,
// kernel32.hpp, user32.hpp, gdi32.hpp, advapi32.hpp, crypt32.hpp,
// cipherksp.hpp, shell32.hpp, uxtheme.hpp, ws2_32.hpp).
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
#include <atomic>
#include "ntdef.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "gdi32.hpp"
#include "advapi32.hpp"
#include "crypt32.hpp"
#include "cipherksp.hpp"
#include "shell32.hpp"
#include "uxtheme.hpp"
#include "ws2_32.hpp"
#include "msvcrt.hpp"
#include "ldr.hpp"

namespace micant::satellite::filezilla {

using BOOL = win32::BOOL;
using DWORD = win32::DWORD;
using ULONG = uint32_t;
using HMODULE = win32::HMODULE;
using HDC = void*;
using HRGN = void*;
using HBRUSH = void*;
using HCURSOR = void*;
using HWND = win32::HWND;
using HANDLE = win32::HANDLE;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPWSTR = wchar_t*;
using SIZE_T = size_t;
using ULONG64 = uint64_t;
using UINT = unsigned int;
using LONG = long;
using SHORT = short;
using LPARAM = intptr_t;
using WPARAM = uintptr_t;

// 1. KERNEL32 Forwarders
using SYSTEM_POWER_STATUS = win32::SYSTEM_POWER_STATUS;

inline BOOL WINAPI K32_GetSystemPowerStatus(SYSTEM_POWER_STATUS* p) noexcept { return win32::GetSystemPowerStatus(p); }
inline DWORD WINAPI K32_GetActiveProcessorCount(uint16_t g) noexcept { return win32::GetActiveProcessorCount(g); }
inline BOOL WINAPI K32_GetHandleInformation(HANDLE h, DWORD* f) noexcept { return win32::GetHandleInformation(h, f); }
inline DWORD WINAPI K32_GetProfileStringW(LPCWSTR a, LPCWSTR k, LPCWSTR d, LPWSTR r, DWORD s) noexcept {
    return win32::GetProfileStringW(a, k, d, r, s);
}
inline int WINAPI K32_IdnToAscii(DWORD f, LPCWSTR u, int cu, LPWSTR a, int ca) noexcept {
    return win32::IdnToAscii(f, u, cu, a, ca);
}
inline BOOL WINAPI K32_IsBadStringPtrA(LPCSTR l, size_t m) noexcept { return win32::IsBadStringPtrA(l, m); }
inline BOOL WINAPI K32_SetProcessPriorityBoost(HANDLE h, BOOL d) noexcept { return win32::SetProcessPriorityBoost(h, d); }
inline BOOL WINAPI K32_SetThreadContext(HANDLE h, const void* c) noexcept { return win32::SetThreadContext(h, c); }
inline HANDLE WINAPI K32_FindFirstVolumeW(LPWSTR v, DWORD l) noexcept { return win32::FindFirstVolumeW(v, l); }
inline BOOL WINAPI K32_FindNextVolumeW(HANDLE h, LPWSTR v, DWORD l) noexcept { return win32::FindNextVolumeW(h, v, l); }
inline BOOL WINAPI K32_FindVolumeClose(HANDLE h) noexcept { return win32::FindVolumeClose(h); }
inline DWORD WINAPI K32_GetFinalPathNameByHandleA(HANDLE h, char* p, DWORD c, DWORD f) noexcept {
    return win32::GetFinalPathNameByHandleA(h, p, c, f);
}
inline BOOL WINAPI K32_FillConsoleOutputCharacterW(HANDLE h, wchar_t c, DWORD l, void* w, DWORD* o) noexcept {
    return win32::FillConsoleOutputCharacterW(h, c, l, w, o);
}
inline BOOL WINAPI K32_ReadConsoleOutputCharacterA(HANDLE h, char* c, DWORD l, void* r, DWORD* o) noexcept {
    return win32::ReadConsoleOutputCharacterA(h, c, l, r, o);
}

// 2. USER32 Forwarders
inline BOOL WINAPI U32_AnimateWindow(HWND w, DWORD t, DWORD f) noexcept { return user32::AnimateWindow(w, t, f); }
inline LONG WINAPI U32_ChangeDisplaySettingsExW(LPCWSTR d, void* m, HWND w, DWORD f, void* l) noexcept {
    return user32::ChangeDisplaySettingsExW(d, m, w, f, l);
}
inline BOOL WINAPI U32_EnumDisplaySettingsW(LPCWSTR d, DWORD m, void* dm) noexcept { return user32::EnumDisplaySettingsW(d, m, dm); }
inline BOOL WINAPI U32_DrawStateW(HDC h, HBRUSH b, void* c, LPARAM l, WPARAM w, int x, int y, int cx, int cy, UINT f) noexcept {
    return user32::DrawStateW(h, b, c, l, w, x, y, cx, cy, f);
}
inline BOOL WINAPI U32_GetProcessDefaultLayout(DWORD* l) noexcept { return user32::GetProcessDefaultLayout(l); }
inline HCURSOR WINAPI U32_LoadCursorFromFileW(LPCWSTR f) noexcept { return reinterpret_cast<HCURSOR>(user32::LoadCursorFromFileW(f)); }
inline BOOL WINAPI U32_SetCaretBlinkTime(UINT m) noexcept { return user32::SetCaretBlinkTime(m); }
inline BOOL WINAPI U32_ValidateRgn(HWND w, HRGN r) noexcept { return user32::ValidateRgn(w, r); }
inline SHORT WINAPI U32_VkKeyScanW(wchar_t c) noexcept { return user32::VkKeyScanW(c); }
inline DWORD WINAPI U32_WaitForInputIdle(HANDLE p, DWORD m) noexcept { return user32::WaitForInputIdle(p, m); }
inline void WINAPI U32_keybd_event(uint8_t v, uint8_t s, DWORD f, uintptr_t e) noexcept { user32::keybd_event(v, s, f, e); }
inline void* WINAPI U32_DdeCreateDataHandle(DWORD i, uint8_t* p, DWORD cb, DWORD cbo, void* item, UINT fmt, UINT cmd) noexcept {
    return user32::DdeCreateDataHandle(i, p, cb, cbo, item, fmt, cmd);
}
inline DWORD WINAPI U32_DdeGetData(void* h, uint8_t* d, DWORD max, DWORD off) noexcept { return user32::DdeGetData(h, d, max, off); }
inline UINT WINAPI U32_DdeGetLastError(DWORD i) noexcept { return user32::DdeGetLastError(i); }
inline void* WINAPI U32_DdeNameService(DWORD i, void* s1, void* s2, UINT cmd) noexcept { return user32::DdeNameService(i, s1, s2, cmd); }
inline BOOL WINAPI U32_DdePostAdvise(DWORD i, void* top, void* item) noexcept { return user32::DdePostAdvise(i, top, item); }
inline DWORD WINAPI U32_DdeQueryStringW(DWORD i, void* h, LPWSTR p, DWORD max, int cp) noexcept {
    return user32::DdeQueryStringW(i, h, p, max, cp);
}

// 3. GDI32 Forwarders
inline HRGN WINAPI GDI_CreatePolygonRgn(const void* pt, int cp, int m) noexcept { return gdi32::CreatePolygonRgn(pt, cp, m); }
inline BOOL WINAPI GDI_PolyPolygon(HDC h, const void* pt, const int* sz, int c) noexcept { return gdi32::PolyPolygon(h, pt, sz, c); }
inline BOOL WINAPI GDI_EqualRgn(HRGN r1, HRGN r2) noexcept { return gdi32::EqualRgn(r1, r2); }
inline BOOL WINAPI GDI_PtInRegion(HRGN r, int x, int y) noexcept { return gdi32::PtInRegion(r, x, y); }
inline BOOL WINAPI GDI_RectInRegion(HRGN r, const void* rc) noexcept { return gdi32::RectInRegion(r, rc); }
inline void* WINAPI GDI_GetEnhMetaFileW(LPCWSTR n) noexcept { return gdi32::GetEnhMetaFileW(n); }
inline void* WINAPI GDI_SetMetaFileBitsEx(UINT cb, const uint8_t* d) noexcept { return gdi32::SetMetaFileBitsEx(cb, d); }
inline int WINAPI GDI_GetGraphicsMode(HDC h) noexcept { return gdi32::GetGraphicsMode(h); }
inline BOOL WINAPI GDI_GetWorldTransform(HDC h, void* x) noexcept { return gdi32::GetWorldTransform(h, x); }
inline BOOL WINAPI GDI_ModifyWorldTransform(HDC h, const void* x, DWORD m) noexcept { return gdi32::ModifyWorldTransform(h, x, m); }

// 4. ADVAPI32 Forwarders
inline BOOL WINAPI ADV_AllocateLocallyUniqueId(void* l) noexcept { return advapi32::AllocateLocallyUniqueId(l); }
inline BOOL WINAPI ADV_CredReadW(LPCWSTR t, DWORD tp, DWORD f, void** c) noexcept { return advapi32::CredReadW(t, tp, f, c); }
inline void WINAPI ADV_CredFree(void* b) noexcept { advapi32::CredFree(b); }
inline BOOL WINAPI ADV_CredWriteW(void* c, DWORD f) noexcept { return advapi32::CredWriteW(c, f); }
inline BOOL WINAPI ADV_CredDeleteW(LPCWSTR t, DWORD tp, DWORD f) noexcept { return advapi32::CredDeleteW(t, tp, f); }
inline BOOL WINAPI ADV_DuplicateTokenEx(HANDLE h, DWORD a, void* attr, int lvl, int tp, HANDLE* n) noexcept {
    return advapi32::DuplicateTokenEx(h, a, attr, lvl, tp, n);
}
inline BOOL WINAPI ADV_ImpersonateLoggedOnUser(HANDLE h) noexcept { return advapi32::ImpersonateLoggedOnUser(h); }
inline BOOL WINAPI ADV_RevertToSelf() noexcept { return advapi32::RevertToSelf(); }
inline BOOL WINAPI ADV_LogonUserExW(LPCWSTR u, LPCWSTR d, LPCWSTR p, DWORD lt, DWORD lp, HANDLE* t, void** s, void** pr, DWORD* pl, void* q) noexcept {
    return advapi32::LogonUserExW(u, d, p, lt, lp, t, s, pr, pl, q);
}
inline DWORD WINAPI ADV_SetEntriesInAclW(ULONG c, void* p, void* o, void** n) noexcept { return advapi32::SetEntriesInAclW(c, p, o, n); }
inline BOOL WINAPI ADV_SetSecurityDescriptorSacl(void* sd, BOOL sp, void* s, BOOL df) noexcept {
    return advapi32::SetSecurityDescriptorSacl(sd, sp, s, df);
}
inline BOOL WINAPI ADV_SetTokenInformation(HANDLE t, int c, void* i, DWORD l) noexcept { return advapi32::SetTokenInformation(t, c, i, l); }
inline BOOL WINAPI ADV_CryptSetProvParam(uintptr_t h, DWORD p, const uint8_t* d, DWORD f) noexcept { return advapi32::CryptSetProvParam(h, p, d, f); }
inline BOOL WINAPI ADV_CryptSignHashA(uintptr_t h, DWORD k, LPCSTR d, DWORD f, uint8_t* s, DWORD* l) noexcept {
    return advapi32::CryptSignHashA(h, k, d, f, s, l);
}

// 5. CRYPT32 Forwarders
inline BOOL WINAPI CRYPT_CertDeleteCertificateFromStore(void* c) noexcept { return crypt32::CertDeleteCertificateFromStore(c); }
inline void* WINAPI CRYPT_CertEnumCRLsInStore(void* s, void* p) noexcept { return crypt32::CertEnumCRLsInStore(s, p); }
inline BOOL WINAPI CRYPT_CryptProtectMemory(void* d, DWORD cb, DWORD f) noexcept { return crypt32::CryptProtectMemory(d, cb, f); }

// 6. NCRYPT Forwarders
inline NTSTATUS WINAPI NC_NCryptDecrypt(uintptr_t k, const uint8_t* in, DWORD cbi, void* p, uint8_t* out, DWORD cbo, DWORD* res, DWORD f) noexcept {
    return crypto::NCryptDecrypt(k, in, cbi, p, out, cbo, res, f);
}
inline NTSTATUS WINAPI NC_NCryptGetProperty(uintptr_t o, LPCWSTR p, uint8_t* out, DWORD cb, DWORD* res, DWORD f) noexcept {
    return crypto::NCryptGetProperty(o, p, out, cb, res, f);
}
inline NTSTATUS WINAPI NC_NCryptSignHash(uintptr_t k, void* p, const uint8_t* h, DWORD cbh, uint8_t* sig, DWORD cbs, DWORD* res, DWORD f) noexcept {
    return crypto::NCryptSignHash(k, p, h, cbh, sig, cbs, res, f);
}
inline NTSTATUS WINAPI NC_BCryptOpenAlgorithmProvider(crypto::BCRYPT_ALG_HANDLE* a, const wchar_t* id, const wchar_t* impl, uint32_t f) noexcept {
    return crypto::BCryptOpenAlgorithmProvider(a, id, impl, f);
}
inline NTSTATUS WINAPI NC_BCryptCloseAlgorithmProvider(crypto::BCRYPT_ALG_HANDLE a, uint32_t f) noexcept {
    return crypto::BCryptCloseAlgorithmProvider(a, f);
}
inline NTSTATUS WINAPI NC_BCryptGenRandom(crypto::BCRYPT_ALG_HANDLE a, uint8_t* b, uint32_t c, uint32_t f) noexcept {
    return crypto::BCryptGenRandom(a, b, c, f);
}

// 7. SHELL32 Forwarders
inline int32_t WINAPI SHL_SHDefExtractIconW(LPCWSTR f, int i, UINT flg, void** l, void** s, UINT sz) noexcept {
    return shell32::SHDefExtractIconW(f, i, flg, l, s, sz);
}
inline int WINAPI SHL_SHGetIconOverlayIndexW(LPCWSTR p, int i) noexcept { return shell32::SHGetIconOverlayIndexW(p, i); }

// 8. UXTHEME Forwarders
inline int32_t WINAPI UXT_GetThemeBackgroundExtent(void* th, HDC h, int p, int s, const void* cr, void* er) noexcept {
    return uxtheme::GetThemeBackgroundExtent(th, h, p, s, cr, er);
}
inline DWORD WINAPI UXT_GetThemeSysColor(void* th, int c) noexcept { return uxtheme::GetThemeSysColor(th, c); }
inline int32_t WINAPI UXT_GetThemeSysFont(void* th, int f, void* lf) noexcept { return uxtheme::GetThemeSysFont(th, f, lf); }
inline BOOL WINAPI UXT_IsThemePartDefined(void* th, int p, int s) noexcept { return uxtheme::IsThemePartDefined(th, p, s); }

// 9. WS2_32 Forwarders
inline win32::HANDLE WINAPI WS2_WSACreateEvent() noexcept { return ws2_32::WSACreateEvent(); }
inline BOOL WINAPI WS2_WSACloseEvent(win32::HANDLE h) noexcept { return ws2_32::WSACloseEvent(h); }
inline BOOL WINAPI WS2_WSASetEvent(win32::HANDLE h) noexcept { return ws2_32::WSASetEvent(h); }
inline DWORD WINAPI WS2_WSAWaitForMultipleEvents(DWORD c, const win32::HANDLE* e, BOOL a, DWORD t, BOOL al) noexcept {
    return ws2_32::WSAWaitForMultipleEvents(c, e, a, t, al);
}

// 10. MSVCRT Forwarders
inline double CRT_cosh(double x) noexcept { return msvcrt::CRT_cosh(x); }
inline double CRT_sinh(double x) noexcept { return msvcrt::CRT_sinh(x); }
inline double CRT_tanh(double x) noexcept { return msvcrt::CRT_tanh(x); }
inline double CRT_atof(const char* s) noexcept { return msvcrt::CRT_atof(s); }
inline long   CRT_atol(const char* s) noexcept { return msvcrt::CRT_atol(s); }
inline double CRT_difftime(time_t t1, time_t t0) noexcept { return msvcrt::CRT_difftime(t1, t0); }
inline int    CRT_raise(int sig) noexcept { return msvcrt::CRT_raise(sig); }
inline wchar_t* CRT_wcsdup(const wchar_t* s) noexcept { return msvcrt::CRT_wcsdup(s); }
inline wchar_t* CRT_wcsncpy(wchar_t* d, const wchar_t* s, size_t n) noexcept { return msvcrt::CRT_wcsncpy(d, s, n); }
inline wchar_t* CRT_wcscat(wchar_t* d, const wchar_t* s) noexcept { return msvcrt::CRT_wcscat(d, s); }
inline wchar_t* CRT_wcschr(const wchar_t* s, wchar_t c) noexcept { return msvcrt::CRT_wcschr(s, c); }
inline wchar_t* CRT_wcspbrk(const wchar_t* s, const wchar_t* c) noexcept { return msvcrt::CRT_wcspbrk(s, c); }
inline size_t   CRT_wcsspn(const wchar_t* s, const wchar_t* c) noexcept { return msvcrt::CRT_wcsspn(s, c); }
inline size_t   CRT_wcstombs(char* mb, const wchar_t* wc, size_t max) noexcept { return msvcrt::CRT_wcstombs(mb, wc, max); }
inline unsigned long CRT_wcstoul(const wchar_t* s, wchar_t** e, int b) noexcept { return msvcrt::CRT_wcstoul(s, e, b); }
inline int      CRT_wtoi(const wchar_t* s) noexcept { return msvcrt::CRT_wtoi(s); }
inline int      CRT_wcsnicmp(const wchar_t* s1, const wchar_t* s2, size_t c) noexcept { return msvcrt::CRT_wcsnicmp(s1, s2, c); }
inline void*    CRT_aligned_malloc(size_t s, size_t a) noexcept { return msvcrt::CRT_aligned_malloc(s, a); }
inline void     CRT_aligned_free(void* p) noexcept { msvcrt::CRT_aligned_free(p); }
inline int      CRT_waccess(const wchar_t* p, int m) noexcept { return msvcrt::CRT_waccess(p, m); }
inline int      CRT_wchdir(const wchar_t* d) noexcept { return msvcrt::CRT_wchdir(d); }
inline int      CRT_wchmod(const wchar_t* f, int p) noexcept { return msvcrt::CRT_wchmod(f, p); }
inline wchar_t* CRT_wfullpath(wchar_t* r, const wchar_t* p, size_t max) noexcept { return msvcrt::CRT_wfullpath(r, p, max); }
inline wchar_t* CRT_wgetcwd(wchar_t* b, int max) noexcept { return msvcrt::CRT_wgetcwd(b, max); }
inline wchar_t* CRT_wgetenv(const wchar_t* v) noexcept { return msvcrt::CRT_wgetenv(v); }
inline int      CRT_wputenv(const wchar_t* e) noexcept { return msvcrt::CRT_wputenv(e); }
inline int      CRT_wrename(const wchar_t* o, const wchar_t* n) noexcept { return msvcrt::CRT_wrename(o, n); }
inline int      CRT_wutime64(const wchar_t* f, void* t) noexcept { return msvcrt::CRT_wutime64(f, t); }
inline intptr_t CRT_wfindfirst64(const wchar_t* f, void* d) noexcept { return msvcrt::CRT_wfindfirst64(f, d); }
inline int      CRT_wfindnext64(intptr_t h, void* d) noexcept { return msvcrt::CRT_wfindnext64(h, d); }
inline int      CRT_findclose(intptr_t h) noexcept { return msvcrt::CRT_findclose(h); }
inline char*    CRT_getcwd(char* b, int max) noexcept { return msvcrt::CRT_getcwd(b, max); }
inline int      CRT_getdrive() noexcept { return msvcrt::CRT_getdrive(); }
inline int      CRT_chdrive(int d) noexcept { return msvcrt::CRT_chdrive(d); }
inline int      CRT_mkdir(const char* d) noexcept { return msvcrt::CRT_mkdir(d); }
inline int      CRT_commit(int f) noexcept { return msvcrt::CRT_commit(f); }
inline int      CRT_dup2(int f1, int f2) noexcept { return msvcrt::CRT_dup2(f1, f2); }
inline int64_t  CRT_filelengthi64(int f) noexcept { return msvcrt::CRT_filelengthi64(f); }
inline int64_t  CRT_telli64(int f) noexcept { return msvcrt::CRT_telli64(f); }
inline uintptr_t CRT_beginthread(void (*s)(void*), unsigned int st, void* a) noexcept { return msvcrt::CRT_beginthread(s, st, a); }
inline void     CRT_endthreadex(unsigned int r) noexcept { msvcrt::CRT_endthreadex(r); }
inline wint_t   CRT_fgetwc(std::FILE* s) noexcept { return msvcrt::CRT_fgetwc(s); }
inline int      CRT_fputws(const wchar_t* str, std::FILE* s) noexcept { return msvcrt::CRT_fputws(str, s); }
inline wint_t   CRT_getwc(std::FILE* s) noexcept { return msvcrt::CRT_getwc(s); }
inline wint_t   CRT_putwc(wchar_t c, std::FILE* s) noexcept { return msvcrt::CRT_putwc(c, s); }
inline wint_t   CRT_ungetwc(wint_t c, std::FILE* s) noexcept { return msvcrt::CRT_ungetwc(c, s); }
inline int      CRT_putws(const wchar_t* str) noexcept { return msvcrt::CRT_putws(str); }
inline int      CRT_fseek(std::FILE* s, long o, int o2) noexcept { return msvcrt::CRT_fseek(s, o, o2); }
inline long     CRT_ftell(std::FILE* s) noexcept { return msvcrt::CRT_ftell(s); }
inline int      CRT_fgetpos(std::FILE* s, fpos_t* p) noexcept { return msvcrt::CRT_fgetpos(s, p); }
inline int      CRT_fsetpos(std::FILE* s, const fpos_t* p) noexcept { return msvcrt::CRT_fsetpos(s, p); }
inline int      CRT_remove(const char* f) noexcept { return msvcrt::CRT_remove(f); }
inline int      CRT_setmaxstdio(int m) noexcept { return msvcrt::CRT_setmaxstdio(m); }
inline int      CRT_getmaxstdio() noexcept { return msvcrt::CRT_getmaxstdio(); }
inline void     CRT_wperror(const wchar_t* s) noexcept { msvcrt::CRT_wperror(s); }
inline int      CRT_isalnum(int c) noexcept { return msvcrt::CRT_isalnum(c); }
inline int      CRT_isalpha(int c) noexcept { return msvcrt::CRT_isalpha(c); }
inline int      CRT_isspace(int c) noexcept { return msvcrt::CRT_isspace(c); }
inline int      CRT_iswalnum(wint_t c) noexcept { return msvcrt::CRT_iswalnum(c); }
inline int      CRT_iswalpha(wint_t c) noexcept { return msvcrt::CRT_iswalpha(c); }
inline int      CRT_iswdigit(wint_t c) noexcept { return msvcrt::CRT_iswdigit(c); }
inline int      CRT_iswprint(wint_t c) noexcept { return msvcrt::CRT_iswprint(c); }
inline int      CRT_iswpunct(wint_t c) noexcept { return msvcrt::CRT_iswpunct(c); }
inline int      CRT_iswxdigit(wint_t c) noexcept { return msvcrt::CRT_iswxdigit(c); }
inline void     CRT_assert(const char* m, const char* f, unsigned l) noexcept { msvcrt::CRT_assert(m, f, l); }
inline int      CRT_setjmp(void* b) noexcept { return msvcrt::CRT_setjmp(b); }
inline int64_t  CRT_strtoi64(const char* n, char** e, int b) noexcept { return msvcrt::CRT_strtoi64(n, e, b); }
inline uint64_t CRT_strtoui64(const char* n, char** e, int b) noexcept { return msvcrt::CRT_strtoui64(n, e, b); }
inline char*    CRT_ctime64(const int64_t* t) noexcept { return msvcrt::CRT_ctime64(t); }

inline long& CRT_timezone_val = msvcrt::CRT_timezone_val;

// 11. Initialization Forwarder
inline void InitializeFileZillaExports() noexcept {
    // Delegated to canonical core initializers
    win32::InitializeWin32SubsystemExports();
    user32::InitializeUser32SubsystemExports();
    gdi32::InitializeGdi32SubsystemExports();
    advapi32::InitializeAdvapi32SubsystemExports();
    crypt32::InitializeCrypt32SubsystemExports();
    crypto::InitializeBCryptSubsystemExports();
    shell32::InitializeShell32SubsystemExports();
    uxtheme::InitializeUxThemeSubsystemExports();
    ws2_32::InitializeWs2_32SubsystemExports();
    msvcrt::InitializeMsvcrtSubsystemExports();
}

} // namespace micant::satellite::filezilla
