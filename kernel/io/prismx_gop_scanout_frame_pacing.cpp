/**
 * @file prismx_gop_scanout_frame_pacing.cpp
 * @brief Horizon 90 Kernel Compilation Unit (#62):
 *        PrismX WDDM 3.2 GOP / Primary Surface Zero-Copy Scanout & 540Hz Frame Pacing Engine.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_gop_scanout_frame_pacing.hpp"

namespace micant::sys::internal {

void PrismXGopScanoutFramePacingInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_gop_scanout::InitializePrismXGopScanoutFramePacingExports();
}

} // namespace micant::sys::internal
