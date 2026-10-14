/**
 * @file pofx_whea.cpp
 * @brief Horizon 51: Sovereign NT Runtime Power Management Framework (PoFx*),
 *        Live PnP Hot-Plug / Surprise Removal & Dynamic PCIe BAR Rebalancing,
 *        and Windows Hardware Error Architecture (WHEA — base/ntos/po/pofx_whea).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/pofx_whea.hpp"

namespace micant::sys::internal {

void PoFxWheaInitializeExecutiveSubsystem() noexcept {
    micant::km::pofx::InitializePoFxWheaExports();
    (void)micant::km::pofx::TitanPoFxHotplugWheaManager::get();
}

} // namespace micant::sys::internal
