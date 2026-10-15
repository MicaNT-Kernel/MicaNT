/**
 * @file prismx_d3d12_translation_on12.cpp
 * @brief Horizon 82 (Unit #54): PrismX + Microsoft D3D12TranslationLayer,
 *        D3D11On12 & D3D9On12 Unified Graphics Funnel.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_d3d12_translation_layer_on12.hpp"

namespace micant::sys::internal {

void PrismXD3D12TranslationOn12InitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_on12_funnel::InitializePrismXD3D12TranslationLayerOn12Exports();
}

} // namespace micant::sys::internal
