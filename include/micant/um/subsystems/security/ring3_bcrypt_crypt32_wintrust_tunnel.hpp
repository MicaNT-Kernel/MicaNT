#pragma once
/**
 * @file ring3_bcrypt_crypt32_wintrust_tunnel.hpp
 * @brief Horizon 76: Sovereign Ring 3 Cryptography Next Generation (bcrypt.dll / ncrypt.dll),
 *        DPAPI & Certificate Store (crypt32.dll) & Authenticode WinTrust (wintrust.dll)
 *        Tunneling Through Ring 2 LsaIso Credential Guard / KeyIso & Ring 0 3-us ci.dll.
 */

#include <cstdint>
#include <cstring>
#include <vector>
#include <unordered_set>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/subsystems/security/cipherksp.hpp"
#include "micant/um/subsystems/security/crypt32.hpp"
#include "micant/um/subsystems/security/wintrust.hpp"
#include "micant/km/drivers/security/ring2_lsa_vault_keyiso.hpp"
#include "micant/km/executive/srm_token_ci.hpp"

namespace micant::um::crypto_tunnel {

struct BcryptCrypt32WinTrustTunnelTelemetry {
    uint32_t sha256DigestBytes;
    uint8_t  sha256FirstFourBytes[4];
    uint32_t dpapiPlaintextBytes;
    uint32_t dpapiEncryptedBlobBytes;
    bool     dpapiRoundTripVerified;
    uint32_t ring2LsaIsoPid;
    bool     ring2CredentialGuardDumpBlocked;
    bool     ring2KeyIsoZeroRawKeyExposure;
    uint32_t ring2KeyIsoLatencyUs;
    uint32_t ring0CiVerifyLatencyUs;
    bool     ring0CiUsnCacheHit;
    bool     wave3BSecurityCryptoTunnelSealed;
};

class SovereignBcryptCrypt32WinTrustTunnelManager {
public:
    static SovereignBcryptCrypt32WinTrustTunnelManager& get() noexcept {
        static SovereignBcryptCrypt32WinTrustTunnelManager instance;
        return instance;
    }

    NTSTATUS initializeAndVerifyCryptoWinTrustTunnel() noexcept {
        if (m_initialized) {
            return STATUS_SUCCESS;
        }

        // 1. Register Ring 3 bcrypt.dll, ncrypt.dll, crypt32.dll & wintrust.dll exports
        micant::crypto::InitializeBCryptSubsystemExports();
        micant::crypt32::InitializeCrypt32SubsystemExports();
        micant::wintrust::InitializeWinTrustSubsystemExports();

        // 2. Compute FIPS 180-4 SHA-256 Digest via Clean-Room CNG/BCrypt Engine
        const char* message = "MicaNT-Ring3-Wave3B-CNG-Authenticode-Seal";
        auto digest = micant::crypto::Sha256::hash(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(message), std::strlen(message))
        );
        m_telemetry.sha256DigestBytes = static_cast<uint32_t>(digest.size());
        if (digest.size() >= 4) {
            for (size_t i = 0; i < 4; ++i) {
                m_telemetry.sha256FirstFourBytes[i] = digest[i];
            }
        }

        // 3. Execute DPAPI CryptProtectData & CryptUnprotectData Round-Trip (AES-256-CBC + HMAC-SHA256)
        uint8_t secretPayload[32] = "SovereignRing3MasterTokenSecret";
        micant::crypt32::DATA_BLOB inBlob{ 32, secretPayload };
        micant::crypt32::DATA_BLOB encBlob{};
        micant::crypt32::DATA_BLOB decBlob{};

        if (micant::crypt32::CryptProtectData(&inBlob, L"MicaNT_Vault_Secret", nullptr, nullptr, nullptr, 0, &encBlob)) {
            m_telemetry.dpapiPlaintextBytes     = inBlob.cbData;
            m_telemetry.dpapiEncryptedBlobBytes = encBlob.cbData;
            if (micant::crypt32::CryptUnprotectData(&encBlob, nullptr, nullptr, nullptr, nullptr, 0, &decBlob)) {
                m_telemetry.dpapiRoundTripVerified =
                    (decBlob.cbData == inBlob.cbData &&
                     std::memcmp(decBlob.pbData, inBlob.pbData, inBlob.cbData) == 0);
                if (decBlob.pbData) {
                    micant::win32::LocalFree(decBlob.pbData);
                }
            }
            if (encBlob.pbData) {
                micant::win32::LocalFree(encBlob.pbData);
            }
        }

        // 4. Tunnel Through Ring 2 LsaIso Credential Guard & KeyIso Zero-Key-Exposure Broker
        micant::km::ring2lsa::CredentialGuardVaultReport credReport{};
        micant::km::ring2lsa::TitanRing2LsaVaultAndKeyIsoEngine::verifyCredentialGuardVault(
            0x0010u, &credReport
        );

        micant::km::ring2lsa::KeyIsoDpapiOperationReport keyIsoReport{};
        micant::km::ring2lsa::TitanRing2LsaVaultAndKeyIsoEngine::executeKeyIsoAndDpapiProtect(
            512, &keyIsoReport
        );

        // 5. Tunnel WinTrust Authenticode Verification Through Ring 0 3-us USN-Cached ci.dll
        micant::km::srmci::CiZeroLagVerificationReport ciReport{};
        micant::km::srmci::TitanSrmTokenAndCiEngine::verifyImageHeaderZeroLagCi(
            "C:\\Program Files\\7-Zip\\7zFM.exe",
            0x0001000000048A20ULL,
            0x0000000008402190ULL,
            &ciReport
        );

        m_telemetry.ring2LsaIsoPid                  = credReport.LsaIsoProcessId;
        m_telemetry.ring2CredentialGuardDumpBlocked = credReport.CredentialDumpAttackBlocked;
        m_telemetry.ring2KeyIsoZeroRawKeyExposure   = !keyIsoReport.RawPrivateKeyMappedInRing3App;
        m_telemetry.ring2KeyIsoLatencyUs            = keyIsoReport.CryptoSignAndProtectLatencyUs;
        m_telemetry.ring0CiVerifyLatencyUs          = ciReport.VerificationLatencyMicroseconds;
        m_telemetry.ring0CiUsnCacheHit              = ciReport.UsnCacheHitOnColdLaunch;
        m_telemetry.wave3BSecurityCryptoTunnelSealed =
            (m_telemetry.dpapiRoundTripVerified &&
             m_telemetry.ring2CredentialGuardDumpBlocked &&
             m_telemetry.ring2KeyIsoZeroRawKeyExposure &&
             m_telemetry.ring0CiUsnCacheHit);

        m_initialized = true;
        return STATUS_SUCCESS;
    }

    [[nodiscard]] const BcryptCrypt32WinTrustTunnelTelemetry& telemetry() const noexcept {
        return m_telemetry;
    }

private:
    SovereignBcryptCrypt32WinTrustTunnelManager() = default;
    bool m_initialized = false;
    BcryptCrypt32WinTrustTunnelTelemetry m_telemetry{};
};

inline void InitializeRing3BcryptCrypt32WinTrustTunnelExports() noexcept {
    (void)SovereignBcryptCrypt32WinTrustTunnelManager::get().initializeAndVerifyCryptoWinTrustTunnel();
}

} // namespace micant::um::crypto_tunnel
