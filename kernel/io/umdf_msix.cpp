/**
 * @file umdf_msix.cpp
 * @brief Horizon 52: Sovereign NT User-Mode Driver Framework 2.0 (UMDF / Wudfrd.sys),
 *        Anti-CrowdStrike Kernel Channel & Definition Guard (SafeChannel*), and
 *        Live Hardware MSI-X Multi-Core Interrupt Affinity Router (base/ntos/io/umdf_msix).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/umdf_msix_guard.hpp"

namespace micant::sys::internal {

void UmdfMsixInitializeExecutiveSubsystem() noexcept {
    micant::km::umdf_msix::InitializeUmdfMsixExports();
    (void)micant::km::umdf_msix::TitanUmdfSafeChannelMsixManager::get();
}

} // namespace micant::sys::internal
