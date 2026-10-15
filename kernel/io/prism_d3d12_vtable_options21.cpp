/**
 * @file prism_d3d12_vtable_options21.cpp
 * @brief Horizon 102 (Kernel Unit #74): Sovereign PrismX Official 44-Slot ID3D12Device14
 *        COM VTable Layout, CheckFeatureSupport OPTIONS..OPTIONS21 & Agility SDK D3D12GetInterface.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_d3d12_vtable_options21.hpp"

namespace micant::sys::internal {

void PrismXD3D12VTableOptions21InitializeExecutiveSubsystem() noexcept {
    micant::um::prism_d3d12_abi::InitializePrismD3D12VTableOptions21Exports();
}

} // namespace micant::sys::internal
