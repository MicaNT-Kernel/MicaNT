/**
 * @file prism_dxcore_adapter_factory_com_abi.cpp
 * @brief Horizon 108 (Kernel Unit #80): Sovereign PrismX Official Microsoft DXCore
 *        (<dxcore_interface.h> / dxcore.dll) IDXCoreAdapterFactory1 COM VTable & ML/Compute Discovery.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_dxcore_adapter_factory_com_abi.hpp"

namespace micant::sys::internal {

void PrismXDXCoreComAbiInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_dxcore_abi::InitializePrismDXCoreComExports();
}

} // namespace micant::sys::internal
