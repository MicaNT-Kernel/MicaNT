/**
 * @file prismx_spirv16_dxil_cross_translator_seal.cpp
 * @brief Horizon 97 (Kernel Unit #69): Sovereign PrismX Bidirectional SPIR-V 1.6 <-> DXIL SM 6.9 /
 *        DXBC SM 5.1 Cross-Translator, Vulkan DescriptorSet -> D3D12 RootSignature Remapper & Seal.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_spirv16_dxil_cross_translator_seal.hpp"

namespace micant::sys::internal {

void PrismXSpirv16DxilCrossTranslatorSealInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_spirv_seal::InitializePrismXSpirv16DxilCrossTranslatorExports();
}

} // namespace micant::sys::internal
