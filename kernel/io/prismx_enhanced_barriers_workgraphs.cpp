/**
 * @file prismx_enhanced_barriers_workgraphs.cpp
 * @brief Horizon 86 Kernel Compilation Unit (#58):
 *        PrismX + D3D12 Enhanced Barriers, GPU Upload Heaps (ReBAR), Work Graphs & SM 6.9 WaveMatrix.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_enhanced_barriers_workgraphs_sm69.hpp"

namespace micant::sys::internal {

void PrismXEnhancedBarriersWorkGraphsInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_agility_specs::InitializePrismXEnhancedBarriersWorkGraphsExports();
}

} // namespace micant::sys::internal
