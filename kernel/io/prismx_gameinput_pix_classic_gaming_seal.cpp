/**
 * @file prismx_gameinput_pix_classic_gaming_seal.cpp
 * @brief Horizon 94 (Kernel Unit #66): Sovereign PrismX Complete Gaming Runtime Seal —
 *        GameInput 8000Hz, ID3D12StateObjectDatabase, PIX GPU Profiler, and DDraw7/D3D8 Bridge.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_gameinput_pix_classic_gaming_seal.hpp"

namespace micant::sys::internal {

void PrismXGameInputPixClassicGamingSealInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_gaming_seal::InitializePrismXCompleteGamingRuntimeExports();
}

} // namespace micant::sys::internal
