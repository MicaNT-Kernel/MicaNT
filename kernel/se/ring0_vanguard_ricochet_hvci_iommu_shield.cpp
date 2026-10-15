/**
 * @file ring0_vanguard_ricochet_hvci_iommu_shield.cpp
 * @brief Horizon 99 (Kernel Unit #71): Sovereign Boot-Start Anti-Cheat Container
 *        (vgk.sys / randgrid.sys), HVCI / TPM 2.0 Attestation & IOMMU PCIe DMA VRAM Shield.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/drivers/security/ring0_vanguard_ricochet_hvci_iommu_shield.hpp"

namespace micant::sys::internal {

void Ring0VanguardRicochetHvciIommuShieldInitializeExecutiveSubsystem() noexcept {
    micant::km::vanguard_iommu::InitializeRing0VanguardRicochetIommuExports();
}

} // namespace micant::sys::internal
