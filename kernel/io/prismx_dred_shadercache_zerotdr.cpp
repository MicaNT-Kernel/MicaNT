/**
 * @file prismx_dred_shadercache_zerotdr.cpp
 * @brief Horizon 88 Kernel Compilation Unit (#60 - 60-Unit Milestone!):
 *        PrismX + DRED 1.2 Auto-Breadcrumbs, Persistent PSO Cache & Ring 1 Zero-TDR GPU Guard.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_dred_shadercache_zerotdr_seal.hpp"

namespace micant::sys::internal {

void PrismXDredShaderCacheZeroTdrInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_dred_zerotdr::InitializePrismXDredShaderCacheZeroTdrExports();
}

} // namespace micant::sys::internal
