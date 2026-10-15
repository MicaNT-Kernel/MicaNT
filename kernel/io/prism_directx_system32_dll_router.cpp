/**
 * @file prism_directx_system32_dll_router.cpp
 * @brief Horizon 103 (Kernel Unit #75): Sovereign PrismX System32 PE32+ DirectX Shared Library
 *        Export Router, Cross-Hierarchy D3D11-to-DXGI COM Bridge & 103-Horizon Seal.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_directx_system32_dll_router.hpp"

namespace micant::sys::internal {

void PrismXDirectXSystem32DllRouterInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_system32_router::InitializePrismDirectXSystem32RouterExports();
}

} // namespace micant::sys::internal
