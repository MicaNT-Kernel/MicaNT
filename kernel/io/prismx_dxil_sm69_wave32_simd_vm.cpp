/**
 * @file prismx_dxil_sm69_wave32_simd_vm.cpp
 * @brief Horizon 96 (Kernel Unit #68): Sovereign PrismX + Microsoft DirectXShaderCompiler
 *        (DxilContainer.h & HlslIntrinsicOp.h) SM 6.0–6.9 DXIL Container Parser &
 *        PrismWave32ShaderVM 32-Lane SIMD Engine.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_dxil_sm69_wave32_simd_vm.hpp"

namespace micant::sys::internal {

void PrismXDxilSm69Wave32SimdVmInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_dxil_wave32::InitializePrismXDxilSm69Wave32SimdVmExports();
}

} // namespace micant::sys::internal
