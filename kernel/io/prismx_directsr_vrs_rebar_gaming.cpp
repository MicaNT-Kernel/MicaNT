/**
 * @file prismx_directsr_vrs_rebar_gaming.cpp
 * @brief Horizon 92 (Kernel Unit #64): Sovereign PrismX Gaming Core —
 *        DirectSR, Variable Rate Shading Tier 2, and Resizable BAR GPU Upload Heaps.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directsr_vrs_rebar_gaming.hpp"

namespace micant::sys::internal {

void PrismXDirectSrVrsReBarGamingInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_directsr_vrs_rebar::InitializePrismXDirectSrVrsReBarExports();
}

} // namespace micant::sys::internal
