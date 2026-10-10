// ============================================================================
// MicaNT: System Informer 4.x / Process Hacker 3 Satellite (satellite_system_informer.hpp)
//
// Modern Clean-Room Forwarding Bridge in pure ISO C++23.
// All 274 exports have graduated directly to Canonical Core MicaNT Subsystems:
//   - aclui.dll    -> include/micant/aclui.hpp
//   - winsta.dll   -> include/micant/winsta.hpp
//   - ntdll.dll    -> include/micant/ntdll.hpp
//   - advapi32.dll -> include/micant/advapi32.hpp
//   - user32.dll   -> include/micant/user32.hpp
//   - kernel32.dll -> include/micant/kernel32.hpp
//   - gdi32.dll    -> include/micant/gdi32.hpp
//   - comctl32.dll -> include/micant/comctl32.hpp
//   - comdlg32.dll -> include/micant/comdlg32.hpp
//   - setupapi.dll -> include/micant/setupapi.hpp
//   - ole32.dll    -> include/micant/ole32.hpp
//   - oleaut32.dll -> include/micant/oleaut32.hpp
//   - winhttp.dll  -> include/micant/winhttp.hpp
//
// Strict clean-room implementation referencing Microsoft win32metadata.
// Zero proprietary, leaked, or decompiled code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <cwchar>
#include <cstring>
#include <vector>
#include <mutex>
#include <atomic>

#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "kernel32.hpp"
#include "user32.hpp"
#include "gdi32.hpp"
#include "advapi32.hpp"
#include "comctl32.hpp"
#include "comdlg32.hpp"
#include "setupapi.hpp"
#include "ole32.hpp"
#include "oleaut32.hpp"
#include "winhttp.hpp"
#include "winsta.hpp"
#include "aclui.hpp"
#include "ntdll.hpp"
#include "ldr.hpp"

namespace micant::satellite::system_informer {

using BOOL = int32_t;
using BOOLEAN = uint8_t;
using DWORD = uint32_t;
using ULONG = uint32_t;
using USHORT = uint16_t;
using UCHAR = uint8_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using HANDLE = void*;
using HWND = void*;
using HDC = void*;
using HICON = void*;
using HMENU = void*;
using HKEY = void*;
using HDEVINFO = void*;
using HPROPSHEETPAGE = void*;
using LSTATUS = int32_t;
using HRESULT = int32_t;
using LONG = int32_t;
using LONG_PTR = intptr_t;
using ULONG_PTR = uintptr_t;
using LPARAM = intptr_t;
using LCID = uint32_t;

inline constexpr BOOL TRUE_VAL = 1;
inline constexpr BOOL FALSE_VAL = 0;
inline constexpr HRESULT S_OK_VAL = 0;
inline constexpr DWORD ERROR_SUCCESS_VAL = 0;

// ============================================================================
// Canonical Core NT Forwarding Bridges for Test Suite & Internal Callers
// ============================================================================

// 1. aclui.dll
inline HPROPSHEETPAGE WINAPI Aclui_CreateSecurityPage_Ordinal1(void* psi) noexcept {
    return micant::aclui::CreateSecurityPage(psi);
}

inline BOOL WINAPI Aclui_EditSecurity_Ordinal2(HWND hwndOwner, void* psi) noexcept {
    return micant::aclui::EditSecurity(hwndOwner, psi);
}

inline HRESULT WINAPI Aclui_EditSecurityAdvanced_Ordinal3(HWND hwndOwner, void* psi, DWORD dwFlags) noexcept {
    return micant::aclui::EditSecurityAdvanced(hwndOwner, psi, dwFlags);
}

// 2. advapi32.dll
inline DWORD WINAPI GetEffectiveRightsFromAclW(void* pAcl, void* pTrustee, uint32_t* pAccessRights) noexcept {
    return micant::advapi32::GetEffectiveRightsFromAclW(pAcl, pTrustee, pAccessRights);
}

inline DWORD WINAPI GetSecurityInfo(HANDLE handle, uint32_t ObjectType, uint32_t SecurityInfo, void** ppsidOwner, void** ppsidGroup, void** ppDacl, void** ppSacl, void** ppSecurityDescriptor) noexcept {
    return micant::advapi32::GetSecurityInfo(handle, ObjectType, SecurityInfo, ppsidOwner, ppsidGroup, ppDacl, ppSacl, ppSecurityDescriptor);
}

inline DWORD WINAPI LsaEnumerateAccounts(HANDLE PolicyHandle, void* EnumerationContext, void** Buffer, uint32_t PreferredMaximumLength, uint32_t* CountReturned) noexcept {
    return micant::advapi32::LsaEnumerateAccounts(PolicyHandle, EnumerationContext, Buffer, PreferredMaximumLength, CountReturned);
}

inline DWORD WINAPI LsaLookupNames2(HANDLE PolicyHandle, uint32_t Flags, uint32_t Count, void* Names, void** ReferencedDomains, void** Sids) noexcept {
    return micant::advapi32::LsaLookupNames2(PolicyHandle, Flags, Count, Names, ReferencedDomains, Sids);
}

inline DWORD WINAPI LsaFreeMemory(void* Buffer) noexcept {
    return micant::advapi32::LsaFreeMemory(Buffer);
}

// 3. ntdll.dll
inline micant::NtStatus WINAPI NtCreateJobObject(HANDLE* JobHandle, uint32_t DesiredAccess, void* ObjectAttributes) noexcept {
    return micant::ntdll::NtCreateJobObject(JobHandle, DesiredAccess, ObjectAttributes);
}

inline micant::NtStatus WINAPI NtCreateKey(HANDLE* KeyHandle, uint32_t DesiredAccess, void* ObjectAttributes, uint32_t TitleIndex, void* Class, uint32_t CreateOptions, uint32_t* Disposition) noexcept {
    return micant::ntdll::NtCreateKey(KeyHandle, DesiredAccess, ObjectAttributes, TitleIndex, Class, CreateOptions, Disposition);
}

inline micant::NtStatus WINAPI NtOpenSection(HANDLE* SectionHandle, uint32_t DesiredAccess, void* ObjectAttributes) noexcept {
    return micant::ntdll::NtOpenSection(SectionHandle, DesiredAccess, ObjectAttributes);
}

inline micant::NtStatus WINAPI NtQueryVirtualMemory(HANDLE ProcessHandle, void* BaseAddress, uint32_t MemoryInformationClass, void* MemoryInformation, size_t MemoryInformationLength, size_t* ReturnLength) noexcept {
    return micant::ntdll::NtQueryVirtualMemory(ProcessHandle, BaseAddress, MemoryInformationClass, MemoryInformation, MemoryInformationLength, ReturnLength);
}

inline micant::NtStatus WINAPI NtQueryTimerResolution(uint32_t* MaximumTime, uint32_t* MinimumTime, uint32_t* CurrentTime) noexcept {
    return micant::ntdll::NtQueryTimerResolution(MaximumTime, MinimumTime, CurrentTime);
}

inline micant::NtStatus WINAPI NtConnectPort(HANDLE* PortHandle, void* PortName, void* SecurityQos, void* ClientView, void* ServerView, uint32_t* MaxMessageLength, void* ConnectionInformation, uint32_t* ConnectionInformationLength) noexcept {
    return micant::ntdll::NtConnectPort_Export(PortHandle, PortName, SecurityQos, ClientView, ServerView, MaxMessageLength, ConnectionInformation, ConnectionInformationLength);
}

inline micant::NtStatus WINAPI RtlGetVersion(void* lpVersionInformation) noexcept {
    return micant::ntdll::RtlGetVersion(lpVersionInformation);
}

inline micant::NtStatus WINAPI RtlIpv4AddressToStringExW(const void* Address, uint16_t Port, wchar_t* AddressString, uint32_t* AddressStringLength) noexcept {
    return micant::ntdll::RtlIpv4AddressToStringExW(Address, Port, AddressString, AddressStringLength);
}

inline uint32_t WINAPI RtlRandomEx(uint32_t* Seed) noexcept {
    return micant::ntdll::RtlRandomEx(Seed);
}

// 4. user32.dll
inline HANDLE WINAPI OpenWindowStationW(const wchar_t* lpwinsta, BOOL fInherit, DWORD dwDesiredAccess) noexcept {
    return micant::user32::OpenWindowStationW(lpwinsta, fInherit, dwDesiredAccess);
}

inline HANDLE WINAPI GetProcessWindowStation() noexcept {
    return micant::user32::GetProcessWindowStation();
}

inline BOOL WINAPI CloseWindowStation(HANDLE hWinSta) noexcept {
    return micant::user32::CloseWindowStation(hWinSta);
}

inline BOOL WINAPI EnumDesktopsW(HANDLE hwinsta, void* lpEnumFunc, LPARAM lParam) noexcept {
    return user32::EnumDesktopsW(hwinsta, lpEnumFunc, lParam);
}

inline HWND WINAPI GetShellWindow() noexcept {
    return user32::GetShellWindow();
}

inline DWORD WINAPI GetGuiResources(HANDLE hProcess, DWORD uiFlags) noexcept {
    return user32::GetGuiResources(hProcess, uiFlags);
}

// 5. winsta.dll
inline BOOL WINAPI WinStationConnectW(HANDLE hServer, DWORD SessionId, DWORD TargetSessionId, const wchar_t* pPassword, BOOL bWait) noexcept {
    return micant::winsta::WinStationConnectW(hServer, SessionId, TargetSessionId, pPassword, bWait);
}

inline BOOL WINAPI WinStationQueryInformationW(HANDLE hServer, DWORD SessionId, DWORD WinStationInformationClass, void* pWinStationInformation, DWORD WinStationInformationLength, DWORD* pReturnLength) noexcept {
    return micant::winsta::WinStationQueryInformationW(hServer, SessionId, WinStationInformationClass, pWinStationInformation, WinStationInformationLength, pReturnLength);
}

inline BOOL WINAPI WinStationSendMessageW(HANDLE hServer, DWORD SessionId, const wchar_t* pTitle, DWORD TitleLength, const wchar_t* pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait) noexcept {
    return micant::winsta::WinStationSendMessageW(hServer, SessionId, pTitle, TitleLength, pMessage, MessageLength, Style, Timeout, pResponse, bWait);
}

// ============================================================================
// Master Dynamic Loader Registration Bridge (0 registered satellite exports)
// ============================================================================

inline void InitializeSystemInformerExports() noexcept {
    // 0 exports defined in satellite header.
    // All 274 System Informer exports have graduated into canonical Core NT headers!
}

} // namespace micant::satellite::system_informer
