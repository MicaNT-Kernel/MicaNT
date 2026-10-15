/**
 * @file prismx_hlsl_fxc_dxbc_reflection_compiler.cpp
 * @brief Horizon 95 (Kernel Unit #67): Sovereign PrismX Native HLSL Source-to-DXBC Compiler
 *        (D3DCompile2 / fxc.exe SM 2.0–5.1) & ID3D12ShaderReflection Engine.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_hlsl_fxc_dxbc_reflection_compiler.hpp"

namespace micant::sys::internal {

void PrismXHlslFxcDxbcReflectionCompilerInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_fxc_dxbc::InitializePrismXHlslFxcDxbcCompilerExports();
}

} // namespace micant::sys::internal
