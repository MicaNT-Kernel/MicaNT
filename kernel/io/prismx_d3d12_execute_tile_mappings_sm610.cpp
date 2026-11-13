/**
 * @file prismx_d3d12_execute_tile_mappings_sm610.cpp
 * @brief Horizon 301 (Kernel Unit #77): Sovereign PrismX + Microsoft DirectX 2026 Integration
 *        - Direct3D 12 Agility SDK 1.620.2 & ID3D12Device17/18
 *        - ID3D12GraphicsCommandList12 (Async Command Fallback)
 *        - ID3D12TileMappingQueue (D3D12ExecuteTileMappings GPU-Driven Batch Execution)
 *        - Shader Model 6.10 & DXIL 1.10 Container with 18 Official DFCC Parts
 *        - Wave32 32-Lane SIMD LinAlg MMA Dot Product, DebugBreak, TriangleObjectPositions
 *        - DirectX-Headers 1.620.2 Hardware Scheduling Queue Groupings & 256-Byte DX_DUMP_FILE_HEADER.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_d3d12_execute_tile_mappings_sm610.hpp"

namespace micant::sys::internal {

void PrismXD3D12ExecuteTileMappingsSm610InitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_tilemapping_sm610::InitializePrismXD3D12TileMappingSm610Exports();
}

} // namespace micant::sys::internal
