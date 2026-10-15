#pragma once
/**
 * @file winnt_bcrypt_crypt32_secur32_wintrust_abi.hpp
 * @brief Horizon 114 (Kernel Unit #86): Sovereign WinNT Core User-Mode System32 DLLs
 *        (bcrypt.dll, crypt32.dll, secur32.dll & wintrust.dll) Official PE32+ Exports,
 *        CNG Entropy & SHA-256 / AES-GCM Primitives, X.509 Certificate Store & DPAPI,
 *        SSPI SChannel TLS 1.3 Security Contexts, and Authenticode PE Signature Verification.
 */

#include <cstdint>
#include <cstring>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::um::winnt_bcrypt_crypt32_secur32_wintrust_abi {

inline constexpr uint32_t BCRYPT_USE_SYSTEM_PREFERRED_RNG_OFFICIAL = 0x00000002U;
inline constexpr uint32_t CERT_STORE_PROV_SYSTEM_W_OFFICIAL        = 10U;

struct WinNtDataBlob {
    uint32_t cbData{0};
    uint8_t* pbData{nullptr};
};

struct WinNtSecHandle {
    uintptr_t dwLower{0};
    uintptr_t dwUpper{0};
};

// --- 1. bcrypt.dll official exports (10 functions) ---
NTSTATUS BCrypt_BCryptOpenAlgorithmProvider(void** phAlgorithm, const wchar_t* pszAlgId, const wchar_t* pszImplementation, uint32_t dwFlags) noexcept;
NTSTATUS BCrypt_BCryptCloseAlgorithmProvider(void* hAlgorithm, uint32_t dwFlags) noexcept;
NTSTATUS BCrypt_BCryptGenRandom(void* hAlgorithm, uint8_t* pbBuffer, uint32_t cbBuffer, uint32_t dwFlags) noexcept;
NTSTATUS BCrypt_BCryptCreateHash(void* hAlgorithm, void** phHash, uint8_t* pbHashObject, uint32_t cbHashObject, const uint8_t* pbSecret, uint32_t cbSecret, uint32_t dwFlags) noexcept;
NTSTATUS BCrypt_BCryptHashData(void* hHash, const uint8_t* pbInput, uint32_t cbInput, uint32_t dwFlags) noexcept;
NTSTATUS BCrypt_BCryptFinishHash(void* hHash, uint8_t* pbOutput, uint32_t cbOutput, uint32_t dwFlags) noexcept;
NTSTATUS BCrypt_BCryptDestroyHash(void* hHash) noexcept;
NTSTATUS BCrypt_BCryptEncrypt(void* hKey, const uint8_t* pbInput, uint32_t cbInput, void* pPaddingInfo, uint8_t* pbIV, uint32_t cbIV, uint8_t* pbOutput, uint32_t cbOutput, uint32_t* pcbResult, uint32_t dwFlags) noexcept;
NTSTATUS BCrypt_BCryptDecrypt(void* hKey, const uint8_t* pbInput, uint32_t cbInput, void* pPaddingInfo, uint8_t* pbIV, uint32_t cbIV, uint8_t* pbOutput, uint32_t cbOutput, uint32_t* pcbResult, uint32_t dwFlags) noexcept;
NTSTATUS BCrypt_BCryptDeriveKeyPBKDF2(void* hPrf, const uint8_t* pbPassword, uint32_t cbPassword, const uint8_t* pbSalt, uint32_t cbSalt, uint64_t cIterations, uint8_t* pbDerivedKey, uint32_t cbDerivedKey, uint32_t dwFlags) noexcept;

// --- 2. crypt32.dll official exports (10 functions) ---
void*   Crypt32_CertOpenSystemStoreW(uintptr_t hProv, const wchar_t* szSubsystemProtocol) noexcept;
void*   Crypt32_CertOpenStore(const char* lpszStoreProvider, uint32_t dwEncodingType, uintptr_t hCryptProv, uint32_t dwFlags, const void* pvPara) noexcept;
int32_t Crypt32_CertCloseStore(void* hCertStore, uint32_t dwFlags) noexcept;
const void* Crypt32_CertFindCertificateInStore(void* hCertStore, uint32_t dwCertEncodingType, uint32_t dwFindFlags, uint32_t dwFindType, const void* pvFindPara, const void* pPrevCertContext) noexcept;
int32_t Crypt32_CertFreeCertificateContext(const void* pCertContext) noexcept;
int32_t Crypt32_CertGetCertificateChain(void* hChainEngine, const void* pCertContext, const void* pTime, void* hAdditionalStore, const void* pChainPara, uint32_t dwFlags, void* pvReserved, const void** ppChainContext) noexcept;
int32_t Crypt32_CertVerifyCertificateChainPolicy(const char* pszPolicyOID, const void* pChainContext, const void* pPolicyPara, void* pPolicyStatus) noexcept;
void    Crypt32_CertFreeCertificateChain(const void* pChainContext) noexcept;
int32_t Crypt32_CryptProtectData(WinNtDataBlob* pDataIn, const wchar_t* szDataDescr, WinNtDataBlob* pOptionalEntropy, void* pvReserved, void* pPromptStruct, uint32_t dwFlags, WinNtDataBlob* pDataOut) noexcept;
int32_t Crypt32_CryptUnprotectData(WinNtDataBlob* pDataIn, wchar_t** ppszDataDescr, WinNtDataBlob* pOptionalEntropy, void* pvReserved, void* pPromptStruct, uint32_t dwFlags, WinNtDataBlob* pDataOut) noexcept;

// --- 3. secur32.dll official exports (8 functions) ---
int32_t Secur32_AcquireCredentialsHandleW(const wchar_t* pszPrincipal, const wchar_t* pszPackage, uint32_t fCredentialUse, void* pvLogonId, void* pAuthData, void* pGetKeyFn, void* pvGetKeyArgument, WinNtSecHandle* phCredential, void* ptsExpiry) noexcept;
int32_t Secur32_FreeCredentialsHandle(WinNtSecHandle* phCredential) noexcept;
int32_t Secur32_InitializeSecurityContextW(WinNtSecHandle* phCredential, WinNtSecHandle* phContext, const wchar_t* pszTargetName, uint32_t fContextReq, uint32_t Reserved1, uint32_t TargetDataRep, void* pInput, uint32_t Reserved2, WinNtSecHandle* phNewContext, void* pOutput, uint32_t* pfContextAttr, void* ptsExpiry) noexcept;
int32_t Secur32_DeleteSecurityContext(WinNtSecHandle* phContext) noexcept;
int32_t Secur32_QueryContextAttributesW(WinNtSecHandle* phContext, uint32_t ulAttribute, void* pBuffer) noexcept;
int32_t Secur32_EncryptMessage(WinNtSecHandle* phContext, uint32_t fQOP, void* pMessage, uint32_t MessageSeqNo) noexcept;
int32_t Secur32_DecryptMessage(WinNtSecHandle* phContext, void* pMessage, uint32_t MessageSeqNo, uint32_t* pfQOP) noexcept;
uint8_t Secur32_GetUserNameExW(uint32_t NameFormat, wchar_t* lpNameBuffer, uint32_t* nSize) noexcept;

// --- 4. wintrust.dll official exports (4 functions) ---
int32_t Wintrust_WinVerifyTrust(void* hwnd, const void* pgActionID, void* pWVTData) noexcept;
int32_t Wintrust_WinVerifyTrustEx(void* hwnd, const void* pgActionID, void* pWinTrustData) noexcept;
void*   Wintrust_WTHelperProvDataFromStateData(void* hStateData) noexcept;
void*   Wintrust_WTHelperGetProvSignerFromChain(void* pProvData, uint32_t idxSigner, int32_t fCounterSigner, uint32_t idxCounterSigner) noexcept;

struct Horizon114BCryptCrypt32Secur32WintrustAuditReport {
    uint32_t bcryptExportCount{0};
    uint32_t crypt32ExportCount{0};
    uint32_t secur32ExportCount{0};
    uint32_t wintrustExportCount{0};
    bool     bcryptRngAndHashVerified{false};
    bool     crypt32CertStoreAndDpapiVerified{false};
    bool     secur32SchannelContextVerified{false};
    bool     wintrustAuthenticodeVerified{false};
};

class WinNtBCryptCrypt32Secur32WintrustCoordinator {
public:
    static WinNtBCryptCrypt32Secur32WintrustCoordinator& instance() noexcept {
        static WinNtBCryptCrypt32Secur32WintrustCoordinator s_inst;
        return s_inst;
    }

    void initialize() noexcept;
    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }

    [[nodiscard]] Horizon114BCryptCrypt32Secur32WintrustAuditReport runCryptoSecurityAudit() noexcept;

private:
    WinNtBCryptCrypt32Secur32WintrustCoordinator() = default;
    bool m_initialized{false};
    Horizon114BCryptCrypt32Secur32WintrustAuditReport m_cachedReport{};
};

void InitializeWinNtBCryptCrypt32Secur32WintrustSubsystem() noexcept;

} // namespace micant::um::winnt_bcrypt_crypt32_secur32_wintrust_abi
