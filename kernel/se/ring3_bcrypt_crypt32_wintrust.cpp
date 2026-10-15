/**
 * @file ring3_bcrypt_crypt32_wintrust.cpp
 * @brief Horizon 76 Kernel Compilation Unit (#48): Ring 3 BCrypt CNG, Crypt32 DPAPI &
 *        WinTrust Authenticode Tunneling Through Ring 2 LsaIso/KeyIso & Ring 0 ci.dll.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/security/ring3_bcrypt_crypt32_wintrust_tunnel.hpp"

namespace micant::sys::internal {

void Ring3BcryptCrypt32WinTrustInitializeExecutiveSubsystem() noexcept {
    micant::um::crypto_tunnel::InitializeRing3BcryptCrypt32WinTrustTunnelExports();
}

} // namespace micant::sys::internal
