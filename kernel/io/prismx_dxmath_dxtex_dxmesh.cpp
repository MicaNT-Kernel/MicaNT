/**
 * @file prismx_dxmath_dxtex_dxmesh.cpp
 * @brief Horizon 81 (Unit #53): PrismX + Official Microsoft DirectXMath (SIMD 4x4),
 *        DirectXTex (BC7/DDS), DirectXMesh (D3D12 Meshlets) & DirectXTK12 Roll-In.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_dxmath_dxtex_dxmesh_tk12.hpp"

namespace micant::sys::internal {

void PrismXDxMathTexMeshInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_dx_assets::InitializePrismXDxMathTexMeshTk12Exports();
}

} // namespace micant::sys::internal
