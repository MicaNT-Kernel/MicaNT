/**
 * @file prism_mfplat_mfreadwrite_cutscene_abi.cpp
 * @brief Horizon 107 (Kernel Unit #79): Sovereign PrismX Official Microsoft Media Foundation
 *        (mfplat.dll & mfreadwrite.dll) COM VTables & Zero-Copy D3D11Texture2D Cutscene Decoder.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_mfplat_mfreadwrite_cutscene_abi.hpp"

namespace micant::sys::internal {

void PrismXMediaFoundationCutsceneAbiInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_mfplat_cutscene_abi::InitializePrismMediaFoundationCutsceneExports();
}

} // namespace micant::sys::internal
