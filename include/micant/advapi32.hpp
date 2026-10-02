#pragma once

/**
 * @file advapi32.hpp
 * @brief MicaNT Clean-Room Advanced Windows 32 Base API (advapi32.dll) Bridge.
 *
 * Implements security, crypto random, token management, and Service Control
 * Manager (SCM) exports.
 */

#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "kernel32.hpp"
#include "ldr.hpp"
#include "scm.hpp"
#include "sam.hpp"
#include "lsass.hpp"

namespace micant::advapi32 {

using SC_HANDLE = void*;

inline win32::BOOL CryptAcquireContextA(
    uintptr_t* phProv,
    const char* /*szContainer*/,
    const char* /*szProvider*/,
    uint32_t /*dwProvType*/,
    uint32_t /*dwFlags*/
) noexcept {
    if (phProv) *phProv = 0xCAFE0001;
    return win32::TRUE;
}

inline win32::BOOL CryptReleaseContext(uintptr_t /*hProv*/, uint32_t /*dwFlags*/) noexcept {
    return win32::TRUE;
}

inline win32::BOOL CryptGenRandom(uintptr_t /*hProv*/, uint32_t dwLen, uint8_t* pbBuffer) noexcept {
    if (!pbBuffer) return win32::FALSE;
    static std::mt19937_64 rng(0x1988DEC);
    for (uint32_t i = 0; i < dwLen; ++i) {
        pbBuffer[i] = static_cast<uint8_t>(rng() & 0xFF);
    }
    return win32::TRUE;
}

inline win32::BOOL OpenProcessToken(
    win32::HANDLE /*ProcessHandle*/,
    uint32_t /*DesiredAccess*/,
    win32::HANDLE* TokenHandle
) noexcept {
    if (TokenHandle) *TokenHandle = reinterpret_cast<win32::HANDLE>(0x0000000000000100ULL);
    return win32::TRUE;
}

inline win32::BOOL GetTokenInformation(
    win32::HANDLE /*TokenHandle*/,
    uint32_t /*TokenInformationClass*/,
    void* /*TokenInformation*/,
    uint32_t /*TokenInformationLength*/,
    uint32_t* ReturnLength
) noexcept {
    if (ReturnLength) *ReturnLength = 64;
    return win32::TRUE;
}

// ============================================================================
// Security, Logon & LSA Win32 APIs
// ============================================================================

inline win32::BOOL LogonUserW(
    const wchar_t* lpszUsername,
    const wchar_t* lpszDomain,
    const wchar_t* lpszPassword,
    uint32_t dwLogonType,
    uint32_t /*dwLogonProvider*/,
    win32::HANDLE* phToken
) noexcept {
    if (!lpszUsername || !phToken) {
        win32::SetLastError(87); // ERROR_INVALID_PARAMETER
        return win32::FALSE;
    }

    std::wstring user(lpszUsername);
    std::wstring dom = lpszDomain ? lpszDomain : L"";
    std::wstring pass = lpszPassword ? lpszPassword : L"";

    std::shared_ptr<se::TokenObject> token;
    Luid logonId{0, 0};
    NTSTATUS st = lsass::LocalSecurityAuthority::get().logonUser(
        dom, user, pass, static_cast<lsass::SecurityLogonType>(dwLogonType), L"MSV1_0", token, logonId
    );

    if (st != STATUS_SUCCESS) {
        win32::SetLastError(1326); // ERROR_LOGON_FAILURE
        return win32::FALSE;
    }

    *phToken = reinterpret_cast<win32::HANDLE>(static_cast<uintptr_t>(logonId.toUint64()));
    return win32::TRUE;
}

inline win32::BOOL LookupAccountSidW(
    const wchar_t* /*lpSystemName*/,
    const se::Sid* lpSid,
    wchar_t* lpName,
    uint32_t* cchName,
    wchar_t* lpReferencedDomainName,
    uint32_t* cchReferencedDomainName,
    uint32_t* peUse
) noexcept {
    if (!lpSid || !cchName || !cchReferencedDomainName) {
        win32::SetLastError(87);
        return win32::FALSE;
    }

    std::wstring name, domain;
    if (!lsass::LocalSecurityAuthority::get().lookupAccountSid(*lpSid, name, domain)) {
        win32::SetLastError(1332); // ERROR_NONE_MAPPED
        return win32::FALSE;
    }

    if (lpName && *cchName > name.size()) {
        wcscpy_s(lpName, *cchName, name.c_str());
    }
    *cchName = static_cast<uint32_t>(name.size());

    if (lpReferencedDomainName && *cchReferencedDomainName > domain.size()) {
        wcscpy_s(lpReferencedDomainName, *cchReferencedDomainName, domain.c_str());
    }
    *cchReferencedDomainName = static_cast<uint32_t>(domain.size());

    if (peUse) *peUse = 1; // SidTypeUser
    return win32::TRUE;
}

inline win32::BOOL LookupAccountNameW(
    const wchar_t* /*lpSystemName*/,
    const wchar_t* lpAccountName,
    se::Sid* Sid,
    uint32_t* cbSid,
    wchar_t* ReferencedDomainName,
    uint32_t* cchReferencedDomainName,
    uint32_t* peUse
) noexcept {
    if (!lpAccountName || !cbSid || !cchReferencedDomainName) {
        win32::SetLastError(87);
        return win32::FALSE;
    }

    se::Sid resolvedSid;
    std::wstring domain;
    if (!lsass::LocalSecurityAuthority::get().lookupAccountName(lpAccountName, resolvedSid, domain)) {
        win32::SetLastError(1332);
        return win32::FALSE;
    }

    if (Sid) *Sid = resolvedSid;
    *cbSid = sizeof(se::Sid);

    if (ReferencedDomainName && *cchReferencedDomainName > domain.size()) {
        wcscpy_s(ReferencedDomainName, *cchReferencedDomainName, domain.c_str());
    }
    *cchReferencedDomainName = static_cast<uint32_t>(domain.size());

    if (peUse) *peUse = 1;
    return win32::TRUE;
}

inline win32::BOOL LookupPrivilegeValueW(
    const wchar_t* /*lpSystemName*/,
    const wchar_t* lpName,
    Luid* lpLuid
) noexcept {
    if (!lpName || !lpLuid) {
        win32::SetLastError(87);
        return win32::FALSE;
    }

    static const std::unordered_map<std::wstring, uint32_t> privMap = {
        {L"SeCreateTokenPrivilege", 2},
        {L"SeAssignPrimaryTokenPrivilege", 3},
        {L"SeLockMemoryPrivilege", 4},
        {L"SeIncreaseQuotaPrivilege", 5},
        {L"SeTcbPrivilege", 7},
        {L"SeSecurityPrivilege", 8},
        {L"SeTakeOwnershipPrivilege", 9},
        {L"SeLoadDriverPrivilege", 10},
        {L"SeSystemtimePrivilege", 12},
        {L"SeBackupPrivilege", 17},
        {L"SeRestorePrivilege", 18},
        {L"SeShutdownPrivilege", 19},
        {L"SeDebugPrivilege", 20},
        {L"SeSystemEnvironmentPrivilege", 22},
        {L"SeChangeNotifyPrivilege", 23},
        {L"SeImpersonatePrivilege", 29}
    };

    auto it = privMap.find(lpName);
    if (it != privMap.end()) {
        lpLuid->lowPart = it->second;
        lpLuid->highPart = 0;
        return win32::TRUE;
    }

    win32::SetLastError(1313); // ERROR_NO_SUCH_PRIVILEGE
    return win32::FALSE;
}

inline win32::BOOL LookupPrivilegeNameW(
    const wchar_t* /*lpSystemName*/,
    const Luid* lpLuid,
    wchar_t* lpName,
    uint32_t* cchName
) noexcept {
    if (!lpLuid || !cchName) {
        win32::SetLastError(87);
        return win32::FALSE;
    }

    static const std::unordered_map<uint32_t, std::wstring> luidMap = {
        {2, L"SeCreateTokenPrivilege"},
        {3, L"SeAssignPrimaryTokenPrivilege"},
        {4, L"SeLockMemoryPrivilege"},
        {5, L"SeIncreaseQuotaPrivilege"},
        {7, L"SeTcbPrivilege"},
        {8, L"SeSecurityPrivilege"},
        {9, L"SeTakeOwnershipPrivilege"},
        {10, L"SeLoadDriverPrivilege"},
        {12, L"SeSystemtimePrivilege"},
        {17, L"SeBackupPrivilege"},
        {18, L"SeRestorePrivilege"},
        {19, L"SeShutdownPrivilege"},
        {20, L"SeDebugPrivilege"},
        {22, L"SeSystemEnvironmentPrivilege"},
        {23, L"SeChangeNotifyPrivilege"},
        {29, L"SeImpersonatePrivilege"}
    };

    auto it = luidMap.find(lpLuid->lowPart);
    if (it != luidMap.end()) {
        if (lpName && *cchName > it->second.size()) {
            wcscpy_s(lpName, *cchName, it->second.c_str());
        }
        *cchName = static_cast<uint32_t>(it->second.size());
        return win32::TRUE;
    }

    win32::SetLastError(1313);
    return win32::FALSE;
}

inline NTSTATUS LsaOpenPolicy(
    const void* /*SystemName*/,
    const void* /*ObjectAttributes*/,
    uint32_t /*DesiredAccess*/,
    uintptr_t* PolicyHandle
) noexcept {
    if (PolicyHandle) *PolicyHandle = 0xCAFE0002;
    return STATUS_SUCCESS;
}

inline NTSTATUS LsaClose(uintptr_t /*ObjectHandle*/) noexcept {
    return STATUS_SUCCESS;
}

// ============================================================================
// Service Control Manager (SCM) Win32 APIs
// ============================================================================

inline SC_HANDLE OpenSCManagerW(
    const wchar_t* lpMachineName,
    const wchar_t* lpDatabaseName,
    uint32_t dwDesiredAccess
) noexcept {
    uintptr_t handle = 0;
    std::wstring machine = lpMachineName ? lpMachineName : L"";
    std::wstring db = lpDatabaseName ? lpDatabaseName : L"";
    uint32_t err = scm::ServiceControlManager::get().openSCManager(machine, db, dwDesiredAccess, handle);
    if (err != scm::ERROR_SUCCESS) {
        win32::SetLastError(err);
        return nullptr;
    }
    return reinterpret_cast<SC_HANDLE>(handle);
}

inline SC_HANDLE CreateServiceW(
    SC_HANDLE hSCManager,
    const wchar_t* lpServiceName,
    const wchar_t* lpDisplayName,
    uint32_t dwDesiredAccess,
    uint32_t dwServiceType,
    uint32_t dwStartType,
    uint32_t dwErrorControl,
    const wchar_t* lpBinaryPathName,
    const wchar_t* lpLoadOrderGroup,
    uint32_t* lpdwTagId,
    const wchar_t* lpDependencies,
    const wchar_t* lpServiceStartName,
    const wchar_t* lpPassword
) noexcept {
    (void)lpdwTagId;
    (void)lpPassword;
    uintptr_t hManager = reinterpret_cast<uintptr_t>(hSCManager);
    uintptr_t hService = 0;
    std::wstring name = lpServiceName ? lpServiceName : L"";
    std::wstring disp = lpDisplayName ? lpDisplayName : L"";
    std::wstring bin = lpBinaryPathName ? lpBinaryPathName : L"";
    std::wstring grp = lpLoadOrderGroup ? lpLoadOrderGroup : L"";
    std::wstring startName = lpServiceStartName ? lpServiceStartName : L"LocalSystem";
    std::vector<std::wstring> deps;
    if (lpDependencies) {
        const wchar_t* p = lpDependencies;
        while (*p) {
            deps.emplace_back(p);
            p += wcslen(p) + 1;
        }
    }
    uint32_t err = scm::ServiceControlManager::get().createService(
        hManager, name, disp, dwDesiredAccess, dwServiceType, dwStartType,
        dwErrorControl, bin, grp, deps, startName, hService
    );
    if (err != scm::ERROR_SUCCESS) {
        win32::SetLastError(err);
        return nullptr;
    }
    return reinterpret_cast<SC_HANDLE>(hService);
}

inline SC_HANDLE OpenServiceW(
    SC_HANDLE hSCManager,
    const wchar_t* lpServiceName,
    uint32_t dwDesiredAccess
) noexcept {
    uintptr_t hManager = reinterpret_cast<uintptr_t>(hSCManager);
    uintptr_t hService = 0;
    std::wstring name = lpServiceName ? lpServiceName : L"";
    uint32_t err = scm::ServiceControlManager::get().openService(hManager, name, dwDesiredAccess, hService);
    if (err != scm::ERROR_SUCCESS) {
        win32::SetLastError(err);
        return nullptr;
    }
    return reinterpret_cast<SC_HANDLE>(hService);
}

inline win32::BOOL StartServiceW(
    SC_HANDLE hService,
    uint32_t dwNumServiceArgs,
    const wchar_t** lpServiceArgVectors
) noexcept {
    uintptr_t h = reinterpret_cast<uintptr_t>(hService);
    std::vector<std::wstring> args;
    if (lpServiceArgVectors && dwNumServiceArgs > 0) {
        for (uint32_t i = 0; i < dwNumServiceArgs; ++i) {
            if (lpServiceArgVectors[i]) args.emplace_back(lpServiceArgVectors[i]);
        }
    }
    uint32_t err = scm::ServiceControlManager::get().startService(h, args);
    if (err != scm::ERROR_SUCCESS) {
        win32::SetLastError(err);
        return win32::FALSE;
    }
    return win32::TRUE;
}

inline win32::BOOL ControlService(
    SC_HANDLE hService,
    uint32_t dwControl,
    scm::SERVICE_STATUS* lpServiceStatus
) noexcept {
    if (!lpServiceStatus) {
        win32::SetLastError(scm::ERROR_INVALID_PARAMETER);
        return win32::FALSE;
    }
    uintptr_t h = reinterpret_cast<uintptr_t>(hService);
    scm::SERVICE_STATUS st{};
    uint32_t err = scm::ServiceControlManager::get().controlService(h, dwControl, st);
    if (err != scm::ERROR_SUCCESS) {
        win32::SetLastError(err);
        return win32::FALSE;
    }
    *lpServiceStatus = st;
    return win32::TRUE;
}

inline win32::BOOL DeleteService(SC_HANDLE hService) noexcept {
    uintptr_t h = reinterpret_cast<uintptr_t>(hService);
    uint32_t err = scm::ServiceControlManager::get().deleteService(h);
    if (err != scm::ERROR_SUCCESS) {
        win32::SetLastError(err);
        return win32::FALSE;
    }
    return win32::TRUE;
}

inline win32::BOOL QueryServiceStatus(
    SC_HANDLE hService,
    scm::SERVICE_STATUS* lpServiceStatus
) noexcept {
    if (!lpServiceStatus) {
        win32::SetLastError(scm::ERROR_INVALID_PARAMETER);
        return win32::FALSE;
    }
    uintptr_t h = reinterpret_cast<uintptr_t>(hService);
    scm::SERVICE_STATUS_PROCESS ssp{};
    uint32_t err = scm::ServiceControlManager::get().queryServiceStatus(h, ssp);
    if (err != scm::ERROR_SUCCESS) {
        win32::SetLastError(err);
        return win32::FALSE;
    }
    lpServiceStatus->dwServiceType = ssp.dwServiceType;
    lpServiceStatus->dwCurrentState = ssp.dwCurrentState;
    lpServiceStatus->dwControlsAccepted = ssp.dwControlsAccepted;
    lpServiceStatus->dwWin32ExitCode = ssp.dwWin32ExitCode;
    lpServiceStatus->dwServiceSpecificExitCode = ssp.dwServiceSpecificExitCode;
    lpServiceStatus->dwCheckPoint = ssp.dwCheckPoint;
    lpServiceStatus->dwWaitHint = ssp.dwWaitHint;
    return win32::TRUE;
}

inline win32::BOOL QueryServiceStatusEx(
    SC_HANDLE hService,
    uint32_t InfoLevel,
    uint8_t* lpBuffer,
    uint32_t cbBufSize,
    uint32_t* pcbBytesNeeded
) noexcept {
    if (InfoLevel != 0 || !lpBuffer || cbBufSize < sizeof(scm::SERVICE_STATUS_PROCESS)) {
        if (pcbBytesNeeded) *pcbBytesNeeded = sizeof(scm::SERVICE_STATUS_PROCESS);
        win32::SetLastError(scm::ERROR_INSUFFICIENT_BUFFER);
        return win32::FALSE;
    }
    uintptr_t h = reinterpret_cast<uintptr_t>(hService);
    scm::SERVICE_STATUS_PROCESS ssp{};
    uint32_t err = scm::ServiceControlManager::get().queryServiceStatus(h, ssp);
    if (err != scm::ERROR_SUCCESS) {
        win32::SetLastError(err);
        return win32::FALSE;
    }
    std::memcpy(lpBuffer, &ssp, sizeof(scm::SERVICE_STATUS_PROCESS));
    if (pcbBytesNeeded) *pcbBytesNeeded = sizeof(scm::SERVICE_STATUS_PROCESS);
    return win32::TRUE;
}

inline win32::BOOL CloseServiceHandle(SC_HANDLE hSCObject) noexcept {
    uintptr_t h = reinterpret_cast<uintptr_t>(hSCObject);
    uint32_t err = scm::ServiceControlManager::get().closeServiceHandle(h);
    if (err != scm::ERROR_SUCCESS) {
        win32::SetLastError(err);
        return win32::FALSE;
    }
    return win32::TRUE;
}

inline void InitializeAdvapi32SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("advapi32.dll", "CryptAcquireContextA", reinterpret_cast<void*>(CryptAcquireContextA));
    ldr.registerExport("advapi32.dll", "CryptReleaseContext", reinterpret_cast<void*>(CryptReleaseContext));
    ldr.registerExport("advapi32.dll", "CryptGenRandom", reinterpret_cast<void*>(CryptGenRandom));
    ldr.registerExport("advapi32.dll", "OpenProcessToken", reinterpret_cast<void*>(OpenProcessToken));
    ldr.registerExport("advapi32.dll", "GetTokenInformation", reinterpret_cast<void*>(GetTokenInformation));

    // SCM Exports
    ldr.registerExport("advapi32.dll", "OpenSCManagerW", reinterpret_cast<void*>(OpenSCManagerW));
    ldr.registerExport("advapi32.dll", "CreateServiceW", reinterpret_cast<void*>(CreateServiceW));
    ldr.registerExport("advapi32.dll", "OpenServiceW", reinterpret_cast<void*>(OpenServiceW));
    ldr.registerExport("advapi32.dll", "StartServiceW", reinterpret_cast<void*>(StartServiceW));
    ldr.registerExport("advapi32.dll", "ControlService", reinterpret_cast<void*>(ControlService));
    ldr.registerExport("advapi32.dll", "DeleteService", reinterpret_cast<void*>(DeleteService));
    ldr.registerExport("advapi32.dll", "QueryServiceStatus", reinterpret_cast<void*>(QueryServiceStatus));
    ldr.registerExport("advapi32.dll", "QueryServiceStatusEx", reinterpret_cast<void*>(QueryServiceStatusEx));
    ldr.registerExport("advapi32.dll", "CloseServiceHandle", reinterpret_cast<void*>(CloseServiceHandle));

    // Security & Logon Exports
    ldr.registerExport("advapi32.dll", "LogonUserW", reinterpret_cast<void*>(LogonUserW));
    ldr.registerExport("advapi32.dll", "LookupAccountSidW", reinterpret_cast<void*>(LookupAccountSidW));
    ldr.registerExport("advapi32.dll", "LookupAccountNameW", reinterpret_cast<void*>(LookupAccountNameW));
    ldr.registerExport("advapi32.dll", "LookupPrivilegeValueW", reinterpret_cast<void*>(LookupPrivilegeValueW));
    ldr.registerExport("advapi32.dll", "LookupPrivilegeNameW", reinterpret_cast<void*>(LookupPrivilegeNameW));
    ldr.registerExport("advapi32.dll", "LsaOpenPolicy", reinterpret_cast<void*>(LsaOpenPolicy));
    ldr.registerExport("advapi32.dll", "LsaClose", reinterpret_cast<void*>(LsaClose));

    // Initialize SCM daemon
    scm::ServiceControlManager::get().initialize();
}

} // namespace micant::advapi32
