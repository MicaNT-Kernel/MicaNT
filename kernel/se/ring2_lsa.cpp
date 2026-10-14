/**
 * @file ring2_lsa.cpp
 * @brief Horizon 66: Sovereign Ring 2 Security Authority & Isolated Credential
 *        Vault (lsass.exe, lsaiso.exe & KeyIso) Compilation Unit
 *        (base/ntos/se/ring2_lsa_vault_keyiso).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/drivers/security/ring2_lsa_vault_keyiso.hpp"

namespace micant::sys::internal {

void Ring2LsaInitializeExecutiveSubsystem() noexcept {
    micant::km::ring2lsa::InitializeRing2LsaVaultExports();
}

} // namespace micant::sys::internal
