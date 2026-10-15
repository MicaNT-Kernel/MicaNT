/**
 * @file horizon100_gaming_drm_hwid_vault_seal.cpp
 * @brief Horizon 100 (Kernel Unit #72 — Historic 100-Horizon Milestone): Sovereign Gaming DRM
 *        Compatibility, Per-Game Salted HWID Privacy Vault & 100-Horizon Architecture Seal.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/drivers/security/horizon100_gaming_drm_hwid_vault_seal.hpp"

namespace micant::sys::internal {

void Horizon100GamingDrmHwidVaultSealInitializeExecutiveSubsystem() noexcept {
    micant::km::horizon100_seal::InitializeHorizon100GamingDrmAndHwidSealExports();
}

} // namespace micant::sys::internal
