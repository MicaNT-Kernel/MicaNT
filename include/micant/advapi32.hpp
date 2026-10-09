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
#include <atomic>

#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "kernel32.hpp"
#include "ldr.hpp"
#include "scm.hpp"
#include "sam.hpp"
#include "lsass.hpp"
#include "cipherksp.hpp"

namespace micant::advapi32 {

using SC_HANDLE   = void*;
using HCRYPTPROV  = uintptr_t;
using HCRYPTHASH  = uintptr_t;
using HCRYPTKEY   = uintptr_t;
using LPCWSTR     = const wchar_t*;

inline constexpr uint32_t PROV_RSA_FULL        = 1;
inline constexpr uint32_t PROV_RSA_AES         = 24;

inline constexpr uint32_t CRYPT_VERIFYCONTEXT  = 0xF0000000;
inline constexpr uint32_t CRYPT_NEWKEYSET      = 0x00000008;
inline constexpr uint32_t CRYPT_DELETEKEYSET   = 0x00000010;

inline constexpr uint32_t CALG_MD5             = 0x8003;
inline constexpr uint32_t CALG_SHA1            = 0x8004;
inline constexpr uint32_t CALG_SHA_256         = 0x800c;
inline constexpr uint32_t CALG_AES_128         = 0x660e;
inline constexpr uint32_t CALG_AES_256         = 0x6610;

inline constexpr uint32_t HP_ALGID             = 0x0001;
inline constexpr uint32_t HP_HASHVAL           = 0x0002;
inline constexpr uint32_t HP_HASHSIZE          = 0x0004;

inline constexpr uint32_t PLAINTEXTKEYBLOB     = 0x8;
inline constexpr uint32_t SIMPLEBLOB           = 0x1;

struct CryptoApiContext {
    std::string container;
    std::string provider;
    uint32_t provType{PROV_RSA_FULL};
};

struct CryptoApiHash {
    uint32_t algId{CALG_SHA_256};
    std::vector<uint8_t> buffer;
    bool finished{false};
    std::vector<uint8_t> digest;

    void update(const uint8_t* data, size_t len) {
        if (!finished) buffer.insert(buffer.end(), data, data + len);
    }

    const std::vector<uint8_t>& getDigest() {
        if (!finished) {
            finished = true;
            if (algId == CALG_MD5) {
                digest = crypto::Md5::hash(buffer);
            } else if (algId == CALG_SHA1) {
                digest = crypto::Sha1::hash(buffer);
            } else {
                digest = crypto::Sha256::hash(buffer);
            }
        }
        return digest;
    }
};

struct CryptoApiKey {
    uint32_t algId{CALG_AES_256};
    std::vector<uint8_t> secret;
};

class CryptoApiManager {
private:
    std::mutex m_mutex;
    uintptr_t m_nextId{0x1000};
    std::unordered_map<HCRYPTPROV, std::shared_ptr<CryptoApiContext>> m_provs;
    std::unordered_map<HCRYPTHASH, std::shared_ptr<CryptoApiHash>>    m_hashes;
    std::unordered_map<HCRYPTKEY,  std::shared_ptr<CryptoApiKey>>     m_keys;

public:
    static CryptoApiManager& Instance() {
        static CryptoApiManager s_inst;
        return s_inst;
    }

    HCRYPTPROV registerProv(const std::string& cont, const std::string& prov, uint32_t type) {
        std::lock_guard<std::mutex> lock(m_mutex);
        uintptr_t id = m_nextId++;
        auto ctx = std::make_shared<CryptoApiContext>();
        ctx->container = cont;
        ctx->provider = prov;
        ctx->provType = type;
        m_provs[id] = ctx;
        return id;
    }

    bool releaseProv(HCRYPTPROV hProv) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_provs.erase(hProv) > 0;
    }

    HCRYPTHASH registerHash(uint32_t algId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        uintptr_t id = m_nextId++;
        auto h = std::make_shared<CryptoApiHash>();
        h->algId = algId;
        m_hashes[id] = h;
        return id;
    }

    std::shared_ptr<CryptoApiHash> getHash(HCRYPTHASH hHash) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_hashes.find(hHash);
        return (it != m_hashes.end()) ? it->second : nullptr;
    }

    bool releaseHash(HCRYPTHASH hHash) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_hashes.erase(hHash) > 0;
    }

    HCRYPTKEY registerKey(uint32_t algId, std::span<const uint8_t> secret) {
        std::lock_guard<std::mutex> lock(m_mutex);
        uintptr_t id = m_nextId++;
        auto k = std::make_shared<CryptoApiKey>();
        k->algId = algId;
        k->secret.assign(secret.begin(), secret.end());
        m_keys[id] = k;
        return id;
    }

    std::shared_ptr<CryptoApiKey> getKey(HCRYPTKEY hKey) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_keys.find(hKey);
        return (it != m_keys.end()) ? it->second : nullptr;
    }

    bool releaseKey(HCRYPTKEY hKey) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_keys.erase(hKey) > 0;
    }
};

inline win32::BOOL CryptAcquireContextA(
    HCRYPTPROV* phProv,
    const char* szContainer,
    const char* szProvider,
    uint32_t dwProvType,
    [[maybe_unused]] uint32_t dwFlags
) noexcept {
    if (!phProv) {
        win32::SetLastError(87);
        return win32::FALSE;
    }
    std::string cont = szContainer ? szContainer : "";
    std::string prov = szProvider ? szProvider : "MicaNT RSA/AES Provider";
    *phProv = CryptoApiManager::Instance().registerProv(cont, prov, dwProvType);
    return win32::TRUE;
}

inline win32::BOOL CryptAcquireContextW(
    HCRYPTPROV* phProv,
    const wchar_t* szContainer,
    const wchar_t* szProvider,
    uint32_t dwProvType,
    uint32_t dwFlags
) noexcept {
    std::string cont;
    if (szContainer) {
        while (*szContainer) cont.push_back(static_cast<char>(*szContainer++));
    }
    std::string prov;
    if (szProvider) {
        while (*szProvider) prov.push_back(static_cast<char>(*szProvider++));
    }
    return CryptAcquireContextA(phProv, cont.empty() ? nullptr : cont.c_str(), prov.empty() ? nullptr : prov.c_str(), dwProvType, dwFlags);
}

inline win32::BOOL CryptReleaseContext(HCRYPTPROV hProv, [[maybe_unused]] uint32_t dwFlags) noexcept {
    return CryptoApiManager::Instance().releaseProv(hProv) ? win32::TRUE : win32::FALSE;
}

inline win32::BOOL CryptGenRandom([[maybe_unused]] HCRYPTPROV hProv, uint32_t dwLen, uint8_t* pbBuffer) noexcept {
    if (!pbBuffer) {
        win32::SetLastError(87);
        return win32::FALSE;
    }
    crypto::Csprng::get().getBytes(std::span<uint8_t>(pbBuffer, dwLen));
    return win32::TRUE;
}

inline win32::BOOL CryptCreateHash(
    [[maybe_unused]] HCRYPTPROV hProv,
    uint32_t Algid,
    [[maybe_unused]] HCRYPTKEY hKey,
    [[maybe_unused]] uint32_t dwFlags,
    HCRYPTHASH* phHash
) noexcept {
    if (!phHash) {
        win32::SetLastError(87);
        return win32::FALSE;
    }
    *phHash = CryptoApiManager::Instance().registerHash(Algid);
    return win32::TRUE;
}

inline win32::BOOL CryptHashData(
    HCRYPTHASH hHash,
    const uint8_t* pbData,
    uint32_t dwDataLen,
    [[maybe_unused]] uint32_t dwFlags
) noexcept {
    if (!pbData && dwDataLen > 0) {
        win32::SetLastError(87);
        return win32::FALSE;
    }
    auto hash = CryptoApiManager::Instance().getHash(hHash);
    if (!hash) {
        win32::SetLastError(6); // ERROR_INVALID_HANDLE
        return win32::FALSE;
    }
    hash->update(pbData, dwDataLen);
    return win32::TRUE;
}

inline win32::BOOL CryptGetHashParam(
    HCRYPTHASH hHash,
    uint32_t dwParam,
    uint8_t* pbData,
    uint32_t* pdwDataLen,
    [[maybe_unused]] uint32_t dwFlags
) noexcept {
    if (!pdwDataLen) {
        win32::SetLastError(87);
        return win32::FALSE;
    }
    auto hash = CryptoApiManager::Instance().getHash(hHash);
    if (!hash) {
        win32::SetLastError(6);
        return win32::FALSE;
    }

    if (dwParam == HP_HASHSIZE) {
        uint32_t sz = (hash->algId == CALG_MD5) ? 16 : ((hash->algId == CALG_SHA1) ? 20 : 32);
        if (!pbData) {
            *pdwDataLen = sizeof(uint32_t);
            return win32::TRUE;
        }
        if (*pdwDataLen < sizeof(uint32_t)) {
            *pdwDataLen = sizeof(uint32_t);
            win32::SetLastError(122);
            return win32::FALSE;
        }
        std::memcpy(pbData, &sz, sizeof(uint32_t));
        return win32::TRUE;
    }

    if (dwParam == HP_HASHVAL) {
        const auto& d = hash->getDigest();
        uint32_t req = static_cast<uint32_t>(d.size());
        if (!pbData) {
            *pdwDataLen = req;
            return win32::TRUE;
        }
        if (*pdwDataLen < req) {
            *pdwDataLen = req;
            win32::SetLastError(122);
            return win32::FALSE;
        }
        std::memcpy(pbData, d.data(), req);
        *pdwDataLen = req;
        return win32::TRUE;
    }

    win32::SetLastError(87);
    return win32::FALSE;
}

inline win32::BOOL CryptDestroyHash(HCRYPTHASH hHash) noexcept {
    return CryptoApiManager::Instance().releaseHash(hHash) ? win32::TRUE : win32::FALSE;
}

inline win32::BOOL CryptDeriveKey(
    [[maybe_unused]] HCRYPTPROV hProv,
    uint32_t Algid,
    HCRYPTHASH hBaseData,
    [[maybe_unused]] uint32_t dwFlags,
    HCRYPTKEY* phKey
) noexcept {
    if (!phKey) {
        win32::SetLastError(87);
        return win32::FALSE;
    }
    auto hash = CryptoApiManager::Instance().getHash(hBaseData);
    if (!hash) {
        win32::SetLastError(6);
        return win32::FALSE;
    }
    const auto& d = hash->getDigest();
    uint32_t keyLen = (Algid == CALG_AES_128) ? 16 : 32;
    std::vector<uint8_t> secret(keyLen);
    for (size_t i = 0; i < keyLen; ++i) {
        secret[i] = d[i % d.size()];
    }
    *phKey = CryptoApiManager::Instance().registerKey(Algid, secret);
    return win32::TRUE;
}

inline win32::BOOL CryptDestroyKey(HCRYPTKEY hKey) noexcept {
    return CryptoApiManager::Instance().releaseKey(hKey) ? win32::TRUE : win32::FALSE;
}

inline win32::BOOL CryptEncrypt(
    HCRYPTKEY hKey,
    [[maybe_unused]] HCRYPTHASH hHash,
    [[maybe_unused]] win32::BOOL Final,
    [[maybe_unused]] uint32_t dwFlags,
    uint8_t* pbData,
    uint32_t* pdwDataLen,
    uint32_t dwBufLen
) noexcept {
    if (!pbData || !pdwDataLen) {
        win32::SetLastError(87);
        return win32::FALSE;
    }
    auto key = CryptoApiManager::Instance().getKey(hKey);
    if (!key) {
        win32::SetLastError(6);
        return win32::FALSE;
    }

    crypto::Aes aes(key->secret);
    uint8_t iv[16]{};
    auto enc = aes.encrypt(std::span<const uint8_t>(pbData, *pdwDataLen), crypto::Aes::Mode::CBC, iv, true);

    if (dwBufLen < enc.size()) {
        *pdwDataLen = static_cast<uint32_t>(enc.size());
        win32::SetLastError(122);
        return win32::FALSE;
    }

    std::memcpy(pbData, enc.data(), enc.size());
    *pdwDataLen = static_cast<uint32_t>(enc.size());
    return win32::TRUE;
}

inline win32::BOOL CryptDecrypt(
    HCRYPTKEY hKey,
    [[maybe_unused]] HCRYPTHASH hHash,
    [[maybe_unused]] win32::BOOL Final,
    [[maybe_unused]] uint32_t dwFlags,
    uint8_t* pbData,
    uint32_t* pdwDataLen
) noexcept {
    if (!pbData || !pdwDataLen) {
        win32::SetLastError(87);
        return win32::FALSE;
    }
    auto key = CryptoApiManager::Instance().getKey(hKey);
    if (!key) {
        win32::SetLastError(6);
        return win32::FALSE;
    }

    crypto::Aes aes(key->secret);
    uint8_t iv[16]{};
    bool ok = false;
    auto dec = aes.decrypt(std::span<const uint8_t>(pbData, *pdwDataLen), crypto::Aes::Mode::CBC, iv, true, &ok);
    if (!ok) {
        win32::SetLastError(0x80090005); // NTE_BAD_DATA
        return win32::FALSE;
    }

    std::memcpy(pbData, dec.data(), dec.size());
    *pdwDataLen = static_cast<uint32_t>(dec.size());
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

inline win32::BOOL AdjustTokenPrivileges(
    win32::HANDLE /*TokenHandle*/,
    win32::BOOL /*DisableAllPrivileges*/,
    void* /*NewState*/,
    uint32_t /*BufferLength*/,
    void* /*PreviousState*/,
    uint32_t* /*ReturnLength*/
) noexcept {
    return win32::TRUE;
}

inline win32::BOOL GetFileSecurityW(
    const wchar_t* /*lpFileName*/,
    uint32_t /*RequestedInformation*/,
    void* /*pSecurityDescriptor*/,
    uint32_t /*nLength*/,
    uint32_t* lpcbLengthNeeded
) noexcept {
    if (lpcbLengthNeeded) *lpcbLengthNeeded = 64;
    return win32::TRUE;
}

inline win32::BOOL SetFileSecurityW(
    const wchar_t* /*lpFileName*/,
    uint32_t /*SecurityInformation*/,
    void* /*pSecurityDescriptor*/
) noexcept {
    return win32::TRUE;
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

// ============================================================================
// Clean-Room Win32 Registry APIs (advapi32.dll)
// ============================================================================

using HKEY = win32::HKEY;
using REGSAM = uint32_t;
using PHKEY = HKEY*;

inline const HKEY HKEY_CLASSES_ROOT   = reinterpret_cast<HKEY>(static_cast<uintptr_t>(0x80000000UL));
inline const HKEY HKEY_CURRENT_USER   = reinterpret_cast<HKEY>(static_cast<uintptr_t>(0x80000001UL));
inline const HKEY HKEY_LOCAL_MACHINE  = reinterpret_cast<HKEY>(static_cast<uintptr_t>(0x80000002UL));
inline const HKEY HKEY_USERS          = reinterpret_cast<HKEY>(static_cast<uintptr_t>(0x80000003UL));
inline const HKEY HKEY_CURRENT_CONFIG = reinterpret_cast<HKEY>(static_cast<uintptr_t>(0x80000005UL));

inline constexpr uint32_t REG_NONE      = 0;
inline constexpr uint32_t REG_SZ        = 1;
inline constexpr uint32_t REG_EXPAND_SZ = 2;
inline constexpr uint32_t REG_BINARY    = 3;
inline constexpr uint32_t REG_DWORD     = 4;
inline constexpr uint32_t REG_MULTI_SZ  = 7;
inline constexpr uint32_t REG_QWORD     = 11;

inline constexpr int32_t ERROR_SUCCESS = 0;
inline constexpr int32_t ERROR_FILE_NOT_FOUND = 2;
inline constexpr int32_t ERROR_MORE_DATA = 234;

struct RegValue {
    uint32_t type{REG_SZ};
    std::vector<uint8_t> data;
};

struct RegNode {
    std::wstring name;
    std::unordered_map<std::wstring, RegValue> values;
    std::unordered_map<std::wstring, std::shared_ptr<RegNode>> subkeys;
};

class SovereignRegistryDatabase {
public:
    static SovereignRegistryDatabase& Instance() {
        static SovereignRegistryDatabase s_Inst;
        return s_Inst;
    }

    SovereignRegistryDatabase() {
        m_hklm = std::make_shared<RegNode>();
        m_hkcu = std::make_shared<RegNode>();
        m_hkcr = std::make_shared<RegNode>();
        m_hku  = std::make_shared<RegNode>();

        auto cv = getOrCreatePath(m_hklm, L"Software\\Microsoft\\Windows NT\\CurrentVersion");
        setValue(cv, L"ProductName", REG_SZ, L"MicaNT Cutler Edition");
        setValue(cv, L"CurrentBuild", REG_SZ, L"26100");
        setValue(cv, L"ReleaseId", REG_SZ, L"2026");

        auto shell = getOrCreatePath(m_hkcu, L"Software\\MicaNT\\SurShell");
        setValue(shell, L"Theme", REG_SZ, L"DarkMica");
    }

    std::shared_ptr<RegNode> getRoot(HKEY hKey) {
        uintptr_t v = reinterpret_cast<uintptr_t>(hKey);
        if (v == 0x80000000UL) return m_hkcr;
        if (v == 0x80000001UL) return m_hkcu;
        if (v == 0x80000002UL) return m_hklm;
        if (v == 0x80000003UL) return m_hku;

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_openHandles.find(v);
        if (it != m_openHandles.end()) return it->second;
        return m_hklm;
    }

    HKEY allocateHandle(std::shared_ptr<RegNode> node) {
        std::lock_guard<std::mutex> lock(m_mutex);
        uintptr_t h = ++m_nextHandle;
        m_openHandles[h] = node;
        return reinterpret_cast<HKEY>(h);
    }

    void closeHandle(HKEY hKey) {
        uintptr_t v = reinterpret_cast<uintptr_t>(hKey);
        if (v >= 0x80000000UL && v <= 0x80000005UL) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_openHandles.erase(v);
    }

    std::shared_ptr<RegNode> getOrCreatePath(std::shared_ptr<RegNode> root, const std::wstring& path) {
        auto cur = root;
        size_t start = 0;
        while (start < path.size()) {
            size_t slash = path.find(L'\\', start);
            std::wstring part = (slash == std::wstring::npos) ? path.substr(start) : path.substr(start, slash - start);
            if (!part.empty()) {
                auto& child = cur->subkeys[part];
                if (!child) {
                    child = std::make_shared<RegNode>();
                    child->name = part;
                }
                cur = child;
            }
            if (slash == std::wstring::npos) break;
            start = slash + 1;
        }
        return cur;
    }

    void setValue(std::shared_ptr<RegNode> node, const std::wstring& name, uint32_t type, const std::wstring& strVal) {
        RegValue rv;
        rv.type = type;
        size_t byteLen = (strVal.size() + 1) * sizeof(wchar_t);
        rv.data.resize(byteLen);
        std::memcpy(rv.data.data(), strVal.c_str(), byteLen);
        node->values[name] = rv;
    }

private:
    std::mutex m_mutex;
    std::shared_ptr<RegNode> m_hklm;
    std::shared_ptr<RegNode> m_hkcu;
    std::shared_ptr<RegNode> m_hkcr;
    std::shared_ptr<RegNode> m_hku;
    std::unordered_map<uintptr_t, std::shared_ptr<RegNode>> m_openHandles;
    uintptr_t m_nextHandle{0x1000};
};

inline int32_t RegOpenKeyExW(HKEY hKey, const wchar_t* lpSubKey, uint32_t /*ulOptions*/, REGSAM /*samDesired*/, PHKEY phkResult) noexcept {
    if (!phkResult) return 87; // ERROR_INVALID_PARAMETER
    auto root = SovereignRegistryDatabase::Instance().getRoot(hKey);
    if (!root) return ERROR_FILE_NOT_FOUND;

    if (!lpSubKey || !*lpSubKey) {
        *phkResult = hKey;
        return ERROR_SUCCESS;
    }

    auto target = SovereignRegistryDatabase::Instance().getOrCreatePath(root, lpSubKey);
    *phkResult = SovereignRegistryDatabase::Instance().allocateHandle(target);
    return ERROR_SUCCESS;
}

inline int32_t RegOpenKeyExA(HKEY hKey, const char* lpSubKey, uint32_t ulOptions, REGSAM samDesired, PHKEY phkResult) noexcept {
    std::wstring subKeyW;
    if (lpSubKey) {
        while (*lpSubKey) subKeyW.push_back(static_cast<wchar_t>(*lpSubKey++));
    }
    return RegOpenKeyExW(hKey, subKeyW.c_str(), ulOptions, samDesired, phkResult);
}

inline int32_t RegQueryValueExW(HKEY hKey, const wchar_t* lpValueName, uint32_t* /*lpReserved*/, uint32_t* lpType, uint8_t* lpData, uint32_t* lpcbData) noexcept {
    auto node = SovereignRegistryDatabase::Instance().getRoot(hKey);
    if (!node) return ERROR_FILE_NOT_FOUND;

    std::wstring vName = lpValueName ? lpValueName : L"";
    auto it = node->values.find(vName);
    if (it == node->values.end()) {
        if (lpcbData) *lpcbData = 0;
        return ERROR_FILE_NOT_FOUND;
    }

    if (lpType) *lpType = it->second.type;
    uint32_t dataSize = static_cast<uint32_t>(it->second.data.size());

    if (!lpData) {
        if (lpcbData) *lpcbData = dataSize;
        return ERROR_SUCCESS;
    }

    if (lpcbData && *lpcbData < dataSize) {
        *lpcbData = dataSize;
        return ERROR_MORE_DATA;
    }

    std::memcpy(lpData, it->second.data.data(), dataSize);
    if (lpcbData) *lpcbData = dataSize;
    return ERROR_SUCCESS;
}

inline int32_t RegQueryValueExA(HKEY hKey, const char* lpValueName, uint32_t* lpReserved, uint32_t* lpType, uint8_t* lpData, uint32_t* lpcbData) noexcept {
    std::wstring vNameW;
    if (lpValueName) {
        while (*lpValueName) vNameW.push_back(static_cast<wchar_t>(*lpValueName++));
    }
    return RegQueryValueExW(hKey, vNameW.c_str(), lpReserved, lpType, lpData, lpcbData);
}

inline int32_t RegCloseKey(HKEY hKey) noexcept {
    SovereignRegistryDatabase::Instance().closeHandle(hKey);
    return ERROR_SUCCESS;
}

inline int32_t RegCreateKeyExW(HKEY hKey, const wchar_t* lpSubKey, uint32_t ulOptions, const wchar_t* /*lpClass*/, uint32_t /*dwOptions*/,
                               REGSAM samDesired, void* /*lpSecurityAttributes*/, PHKEY phkResult, uint32_t* lpdwDisposition) noexcept {
    if (lpdwDisposition) *lpdwDisposition = 1; // REG_CREATED_NEW_KEY
    return RegOpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

inline int32_t RegCreateKeyExA(HKEY hKey, const char* lpSubKey, uint32_t ulOptions, const char* lpClass, uint32_t dwOptions,
                               REGSAM samDesired, void* lpSecurityAttributes, PHKEY phkResult, uint32_t* lpdwDisposition) noexcept {
    (void)lpClass; (void)dwOptions; (void)lpSecurityAttributes;
    return RegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

inline int32_t RegSetValueExW(HKEY hKey, const wchar_t* lpValueName, uint32_t /*Reserved*/, uint32_t dwType, const uint8_t* lpData, uint32_t cbData) noexcept {
    auto node = SovereignRegistryDatabase::Instance().getRoot(hKey);
    if (!node) return ERROR_FILE_NOT_FOUND;
    std::wstring vName = lpValueName ? lpValueName : L"";
    RegValue rv;
    rv.type = dwType;
    if (lpData && cbData > 0) {
        rv.data.assign(lpData, lpData + cbData);
    }
    node->values[vName] = rv;
    return ERROR_SUCCESS;
}

inline int32_t RegSetValueExA(HKEY hKey, const char* lpValueName, uint32_t Reserved, uint32_t dwType, const uint8_t* lpData, uint32_t cbData) noexcept {
    std::wstring vNameW;
    if (lpValueName) {
        while (*lpValueName) vNameW.push_back(static_cast<wchar_t>(*lpValueName++));
    }
    return RegSetValueExW(hKey, vNameW.c_str(), Reserved, dwType, lpData, cbData);
}

inline int32_t RegDeleteKeyW(HKEY hKey, const wchar_t* lpSubKey) noexcept {
    auto node = SovereignRegistryDatabase::Instance().getRoot(hKey);
    if (node && lpSubKey) node->subkeys.erase(lpSubKey);
    return ERROR_SUCCESS;
}

inline int32_t RegDeleteValueW(HKEY hKey, const wchar_t* lpValueName) noexcept {
    auto node = SovereignRegistryDatabase::Instance().getRoot(hKey);
    if (node && lpValueName) node->values.erase(lpValueName);
    return ERROR_SUCCESS;
}

inline int32_t RegEnumKeyExW(HKEY hKey, uint32_t dwIndex, wchar_t* lpName, uint32_t* lpcchName, uint32_t* /*lpReserved*/,
                             wchar_t* /*lpClass*/, uint32_t* /*lpcchClass*/, void* /*lpftLastWriteTime*/) noexcept {
    auto node = SovereignRegistryDatabase::Instance().getRoot(hKey);
    if (!node) return ERROR_FILE_NOT_FOUND;
    if (dwIndex >= node->subkeys.size()) return 259; // ERROR_NO_MORE_ITEMS
    auto it = node->subkeys.begin();
    std::advance(it, dwIndex);
    if (lpName && lpcchName && *lpcchName > it->first.size()) {
        wcscpy_s(lpName, *lpcchName, it->first.c_str());
        *lpcchName = static_cast<uint32_t>(it->first.size());
    }
    return ERROR_SUCCESS;
}

inline int32_t RegEnumValueW(HKEY hKey, uint32_t dwIndex, wchar_t* lpValueName, uint32_t* lpcchValueName, uint32_t* /*lpReserved*/,
                             uint32_t* lpType, uint8_t* lpData, uint32_t* lpcbData) noexcept {
    auto node = SovereignRegistryDatabase::Instance().getRoot(hKey);
    if (!node) return ERROR_FILE_NOT_FOUND;
    if (dwIndex >= node->values.size()) return 259; // ERROR_NO_MORE_ITEMS
    auto it = node->values.begin();
    std::advance(it, dwIndex);
    if (lpValueName && lpcchValueName && *lpcchValueName > it->first.size()) {
        wcscpy_s(lpValueName, *lpcchValueName, it->first.c_str());
        *lpcchValueName = static_cast<uint32_t>(it->first.size());
    }
    if (lpType) *lpType = it->second.type;
    if (lpData && lpcbData && *lpcbData >= it->second.data.size()) {
        std::memcpy(lpData, it->second.data.data(), it->second.data.size());
        *lpcbData = static_cast<uint32_t>(it->second.data.size());
    }
    return ERROR_SUCCESS;
}

inline int32_t RegQueryInfoKeyW(HKEY hKey, wchar_t* /*lpClass*/, uint32_t* /*lpcchClass*/, uint32_t* /*lpReserved*/,
                                uint32_t* lpcSubKeys, uint32_t* /*lpcbMaxSubKeyLen*/, uint32_t* /*lpcbMaxClassLen*/,
                                uint32_t* lpcValues, uint32_t* /*lpcbMaxValueNameLen*/, uint32_t* /*lpcbMaxValueLen*/,
                                void* /*lpcbSecurityDescriptor*/, void* /*lpftLastWriteTime*/) noexcept {
    auto node = SovereignRegistryDatabase::Instance().getRoot(hKey);
    if (!node) return ERROR_FILE_NOT_FOUND;
    if (lpcSubKeys) *lpcSubKeys = static_cast<uint32_t>(node->subkeys.size());
    if (lpcValues) *lpcValues = static_cast<uint32_t>(node->values.size());
    return ERROR_SUCCESS;
}

inline int32_t RegGetValueW(
    HKEY hkey,
    LPCWSTR lpSubKey,
    LPCWSTR lpValue,
    uint32_t /*dwFlags*/,
    uint32_t* pdwType,
    void* pvData,
    uint32_t* pcbData
) noexcept {
    HKEY hSub = hkey;
    bool opened = false;
    if (lpSubKey && lpSubKey[0]) {
        if (RegOpenKeyExW(hkey, lpSubKey, 0, 0x20019, &hSub) != ERROR_SUCCESS) {
            return ERROR_FILE_NOT_FOUND;
        }
        opened = true;
    }
    int32_t res = RegQueryValueExW(hSub, lpValue, nullptr, pdwType, reinterpret_cast<uint8_t*>(pvData), pcbData);
    if (opened) RegCloseKey(hSub);
    return res;
}

inline win32::BOOL AllocateAndInitializeSid(
    void* /*pIdentifierAuthority*/,
    uint8_t /*nSubAuthorityCount*/,
    uint32_t /*dwSubAuthority0*/,
    uint32_t /*dwSubAuthority1*/,
    uint32_t /*dwSubAuthority2*/,
    uint32_t /*dwSubAuthority3*/,
    uint32_t /*dwSubAuthority4*/,
    uint32_t /*dwSubAuthority5*/,
    uint32_t /*dwSubAuthority6*/,
    uint32_t /*dwSubAuthority7*/,
    void** pSid
) noexcept {
    static uint32_t dummySid[8] = { 0x00000101, 0x05000000, 0x00000020, 0x00000220 }; // Administrators SID
    if (pSid) *pSid = dummySid;
    return win32::TRUE;
}

inline void* FreeSid(void* /*pSid*/) noexcept {
    return nullptr;
}

inline win32::BOOL CheckTokenMembership(win32::HANDLE /*TokenHandle*/, void* /*SidToCheck*/, win32::BOOL* IsMember) noexcept {
    if (IsMember) *IsMember = win32::TRUE; // Sovereign admin
    return win32::TRUE;
}

inline uint32_t* GetSidSubAuthority(void* pSid, uint32_t nSubAuthority) noexcept {
    if (!pSid) return nullptr;
    auto* sub = reinterpret_cast<uint32_t*>(pSid);
    return &sub[2 + nSubAuthority];
}

inline uint8_t* GetSidSubAuthorityCount(void* pSid) noexcept {
    static uint8_t count = 2;
    if (!pSid) return &count;
    auto* b = reinterpret_cast<uint8_t*>(pSid);
    return &b[1];
}

inline int32_t IsTextUnicode(const void* lpv, int iSize, int* lpiResult) noexcept {
    if (!lpv || iSize <= 1) {
        if (lpiResult) *lpiResult = 0;
        return 0;
    }
    const auto* bytes = reinterpret_cast<const uint8_t*>(lpv);
    bool hasZero = false;
    for (int i = 1; i < iSize; i += 2) {
        if (bytes[i] == 0) { hasZero = true; break; }
    }
    if (lpiResult) *lpiResult = hasZero ? 1 : 0;
    return hasZero ? 1 : 0;
}

inline win32::BOOL CreateWellKnownSid(win32::DWORD /*WellKnownSidType*/, void* /*pContextSid*/, void* pSid, win32::DWORD* cbSid) noexcept {
    if (!cbSid) return win32::FALSE;
    if (!pSid || *cbSid < 28) {
        *cbSid = 28;
        return win32::FALSE;
    }
    // Create standard Local System SID S-1-5-18
    uint8_t* b = reinterpret_cast<uint8_t*>(pSid);
    std::memset(b, 0, 28);
    b[0] = 1; // Revision
    b[1] = 1; // SubAuthorityCount
    b[7] = 5; // IdentifierAuthority (NT Authority)
    *reinterpret_cast<uint32_t*>(&b[8]) = 18; // SECURITY_LOCAL_SYSTEM_RID
    *cbSid = 28;
    return win32::TRUE;
}

inline std::atomic<uint32_t> g_luidCounter{1000};

inline win32::BOOL AllocateLocallyUniqueId(void* Luid) noexcept {
    if (!Luid) return win32::FALSE;
    uint32_t* p = reinterpret_cast<uint32_t*>(Luid);
    p[0] = g_luidCounter.fetch_add(1);
    p[1] = 0;
    return win32::TRUE;
}

inline win32::BOOL CredReadW([[maybe_unused]] LPCWSTR TargetName,
                             [[maybe_unused]] win32::DWORD Type,
                             [[maybe_unused]] win32::DWORD Flags,
                             void** Credential) noexcept {
    if (Credential) *Credential = nullptr;
    win32::SetLastError(1168); // ERROR_NOT_FOUND
    return win32::FALSE;
}

inline void CredFree([[maybe_unused]] void* Buffer) noexcept {
}

inline win32::BOOL CredWriteW([[maybe_unused]] void* Credential,
                              [[maybe_unused]] win32::DWORD Flags) noexcept {
    return win32::TRUE;
}

inline win32::BOOL CredDeleteW([[maybe_unused]] LPCWSTR TargetName,
                               [[maybe_unused]] win32::DWORD Type,
                               [[maybe_unused]] win32::DWORD Flags) noexcept {
    return win32::TRUE;
}

inline win32::BOOL DuplicateTokenEx(win32::HANDLE hExistingToken,
                                    [[maybe_unused]] win32::DWORD dwDesiredAccess,
                                    [[maybe_unused]] void* lpTokenAttributes,
                                    [[maybe_unused]] int ImpersonationLevel,
                                    [[maybe_unused]] int TokenType,
                                    win32::HANDLE* phNewToken) noexcept {
    if (phNewToken) {
        *phNewToken = hExistingToken ? hExistingToken : reinterpret_cast<win32::HANDLE>(0x00040001);
    }
    return win32::TRUE;
}

inline win32::BOOL ImpersonateLoggedOnUser([[maybe_unused]] win32::HANDLE hToken) noexcept {
    return win32::TRUE;
}

inline win32::BOOL RevertToSelf() noexcept {
    return win32::TRUE;
}

inline win32::BOOL LogonUserExW([[maybe_unused]] LPCWSTR lpszUsername,
                                [[maybe_unused]] LPCWSTR lpszDomain,
                                [[maybe_unused]] LPCWSTR lpszPassword,
                                [[maybe_unused]] win32::DWORD dwLogonType,
                                [[maybe_unused]] win32::DWORD dwLogonProvider,
                                win32::HANDLE* phToken,
                                [[maybe_unused]] void** ppLogonSid,
                                [[maybe_unused]] void** ppProfileBuffer,
                                [[maybe_unused]] win32::DWORD* pdwProfileLength,
                                [[maybe_unused]] void* pQuotaLimits) noexcept {
    if (phToken) {
        *phToken = reinterpret_cast<win32::HANDLE>(0x00040002);
    }
    return win32::TRUE;
}

inline win32::DWORD SetEntriesInAclW([[maybe_unused]] uint32_t cCountOfExplicitEntries,
                                     [[maybe_unused]] void* pListOfExplicitEntries,
                                     void* OldAcl,
                                     void** NewAcl) noexcept {
    if (NewAcl) {
        *NewAcl = OldAcl;
    }
    return 0; // ERROR_SUCCESS
}

inline win32::BOOL SetSecurityDescriptorSacl([[maybe_unused]] void* pSecurityDescriptor,
                                             [[maybe_unused]] win32::BOOL bSaclPresent,
                                             [[maybe_unused]] void* pSacl,
                                             [[maybe_unused]] win32::BOOL bSaclDefaulted) noexcept {
    return win32::TRUE;
}

inline win32::BOOL SetTokenInformation([[maybe_unused]] win32::HANDLE TokenHandle,
                                        [[maybe_unused]] int TokenInformationClass,
                                        [[maybe_unused]] void* TokenInformation,
                                        [[maybe_unused]] win32::DWORD TokenInformationLength) noexcept {
    return win32::TRUE;
}

inline win32::BOOL CryptSetProvParam([[maybe_unused]] uintptr_t hProv,
                                     [[maybe_unused]] win32::DWORD dwParam,
                                     [[maybe_unused]] const uint8_t* pbData,
                                     [[maybe_unused]] win32::DWORD dwFlags) noexcept {
    return win32::TRUE;
}

inline win32::BOOL CryptSignHashA([[maybe_unused]] uintptr_t hHash,
                                  [[maybe_unused]] win32::DWORD dwKeySpec,
                                  [[maybe_unused]] const char* sDescription,
                                  [[maybe_unused]] win32::DWORD dwFlags,
                                  uint8_t* pbSignature,
                                  win32::DWORD* pdwSigLen) noexcept {
    if (!pdwSigLen) return win32::FALSE;
    if (!pbSignature) {
        *pdwSigLen = 64;
        return win32::TRUE;
    }
    std::memset(pbSignature, 0xAA, *pdwSigLen);
    return win32::TRUE;
}

inline void InitializeAdvapi32SubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("advapi32.dll", "CryptAcquireContextA", reinterpret_cast<void*>(CryptAcquireContextA));
    ldr.registerExport("advapi32.dll", "CryptAcquireContextW", reinterpret_cast<void*>(CryptAcquireContextW));
    ldr.registerExport("advapi32.dll", "CryptReleaseContext", reinterpret_cast<void*>(CryptReleaseContext));
    ldr.registerExport("advapi32.dll", "CryptGenRandom", reinterpret_cast<void*>(CryptGenRandom));
    ldr.registerExport("advapi32.dll", "CryptCreateHash", reinterpret_cast<void*>(CryptCreateHash));
    ldr.registerExport("advapi32.dll", "CryptHashData", reinterpret_cast<void*>(CryptHashData));
    ldr.registerExport("advapi32.dll", "CryptGetHashParam", reinterpret_cast<void*>(CryptGetHashParam));
    ldr.registerExport("advapi32.dll", "CryptDestroyHash", reinterpret_cast<void*>(CryptDestroyHash));
    ldr.registerExport("advapi32.dll", "CryptDeriveKey", reinterpret_cast<void*>(CryptDeriveKey));
    ldr.registerExport("advapi32.dll", "CryptDestroyKey", reinterpret_cast<void*>(CryptDestroyKey));
    ldr.registerExport("advapi32.dll", "CryptEncrypt", reinterpret_cast<void*>(CryptEncrypt));
    ldr.registerExport("advapi32.dll", "CryptDecrypt", reinterpret_cast<void*>(CryptDecrypt));
    ldr.registerExport("advapi32.dll", "OpenProcessToken", reinterpret_cast<void*>(OpenProcessToken));
    ldr.registerExport("advapi32.dll", "GetTokenInformation", reinterpret_cast<void*>(GetTokenInformation));
    ldr.registerExport("advapi32.dll", "AllocateAndInitializeSid", reinterpret_cast<void*>(AllocateAndInitializeSid));
    ldr.registerExport("advapi32.dll", "FreeSid", reinterpret_cast<void*>(FreeSid));
    ldr.registerExport("advapi32.dll", "CheckTokenMembership", reinterpret_cast<void*>(CheckTokenMembership));
    ldr.registerExport("advapi32.dll", "GetSidSubAuthority", reinterpret_cast<void*>(GetSidSubAuthority));
    ldr.registerExport("advapi32.dll", "GetSidSubAuthorityCount", reinterpret_cast<void*>(GetSidSubAuthorityCount));
    ldr.registerExport("advapi32.dll", "IsTextUnicode", reinterpret_cast<void*>(IsTextUnicode));

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
    ldr.registerExport("advapi32.dll", "AdjustTokenPrivileges", reinterpret_cast<void*>(AdjustTokenPrivileges));
    ldr.registerExport("advapi32.dll", "GetFileSecurityW", reinterpret_cast<void*>(GetFileSecurityW));
    ldr.registerExport("advapi32.dll", "SetFileSecurityW", reinterpret_cast<void*>(SetFileSecurityW));

    // Win32 Registry Exports
    ldr.registerExport("advapi32.dll", "RegOpenKeyExW", reinterpret_cast<void*>(RegOpenKeyExW));
    ldr.registerExport("advapi32.dll", "RegOpenKeyExA", reinterpret_cast<void*>(RegOpenKeyExA));
    ldr.registerExport("advapi32.dll", "RegQueryValueExW", reinterpret_cast<void*>(RegQueryValueExW));
    ldr.registerExport("advapi32.dll", "RegQueryValueExA", reinterpret_cast<void*>(RegQueryValueExA));
    ldr.registerExport("advapi32.dll", "RegCloseKey", reinterpret_cast<void*>(RegCloseKey));
    ldr.registerExport("advapi32.dll", "RegCreateKeyExW", reinterpret_cast<void*>(RegCreateKeyExW));
    ldr.registerExport("advapi32.dll", "RegCreateKeyExA", reinterpret_cast<void*>(RegCreateKeyExA));
    ldr.registerExport("advapi32.dll", "RegSetValueExW", reinterpret_cast<void*>(RegSetValueExW));
    ldr.registerExport("advapi32.dll", "RegSetValueExA", reinterpret_cast<void*>(RegSetValueExA));
    ldr.registerExport("advapi32.dll", "RegDeleteKeyW", reinterpret_cast<void*>(RegDeleteKeyW));
    ldr.registerExport("advapi32.dll", "RegDeleteValueW", reinterpret_cast<void*>(RegDeleteValueW));
    ldr.registerExport("advapi32.dll", "RegEnumKeyExW", reinterpret_cast<void*>(RegEnumKeyExW));
    ldr.registerExport("advapi32.dll", "RegEnumValueW", reinterpret_cast<void*>(RegEnumValueW));
    ldr.registerExport("advapi32.dll", "RegQueryInfoKeyW", reinterpret_cast<void*>(RegQueryInfoKeyW));
    ldr.registerExport("advapi32.dll", "RegGetValueW", reinterpret_cast<void*>(RegGetValueW));

    // Graduate Security and Crypto APIs
    ldr.registerExport("advapi32.dll", "CreateWellKnownSid", reinterpret_cast<void*>(CreateWellKnownSid));
    ldr.registerExport("api-ms-win-security-base-l1-1-0.dll", "CreateWellKnownSid", reinterpret_cast<void*>(CreateWellKnownSid));
    ldr.registerExport("advapi32.dll", "AllocateLocallyUniqueId", reinterpret_cast<void*>(AllocateLocallyUniqueId));
    ldr.registerExport("advapi32.dll", "CredReadW", reinterpret_cast<void*>(CredReadW));
    ldr.registerExport("advapi32.dll", "CredFree", reinterpret_cast<void*>(CredFree));
    ldr.registerExport("advapi32.dll", "CredWriteW", reinterpret_cast<void*>(CredWriteW));
    ldr.registerExport("advapi32.dll", "CredDeleteW", reinterpret_cast<void*>(CredDeleteW));
    ldr.registerExport("advapi32.dll", "DuplicateTokenEx", reinterpret_cast<void*>(DuplicateTokenEx));
    ldr.registerExport("advapi32.dll", "ImpersonateLoggedOnUser", reinterpret_cast<void*>(ImpersonateLoggedOnUser));
    ldr.registerExport("advapi32.dll", "RevertToSelf", reinterpret_cast<void*>(RevertToSelf));
    ldr.registerExport("advapi32.dll", "LogonUserExW", reinterpret_cast<void*>(LogonUserExW));
    ldr.registerExport("advapi32.dll", "SetEntriesInAclW", reinterpret_cast<void*>(SetEntriesInAclW));
    ldr.registerExport("advapi32.dll", "SetSecurityDescriptorSacl", reinterpret_cast<void*>(SetSecurityDescriptorSacl));
    ldr.registerExport("advapi32.dll", "SetTokenInformation", reinterpret_cast<void*>(SetTokenInformation));
    ldr.registerExport("advapi32.dll", "CryptSetProvParam", reinterpret_cast<void*>(CryptSetProvParam));
    ldr.registerExport("advapi32.dll", "CryptSignHashA", reinterpret_cast<void*>(CryptSignHashA));

    // Initialize SCM daemon
    scm::ServiceControlManager::get().initialize();
}

} // namespace micant::advapi32
