/**
 * @file prism_dxgi16_com_hierarchy.cpp
 * @brief Horizon 101 (Kernel Unit #73): Sovereign PrismX Official Microsoft DXGI 1.0 - 1.6
 *        COM Hierarchy, Exact Binary IID GUIDs & 540Hz Tearing Support.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_dxgi16_com_hierarchy.hpp"

namespace micant::sys::internal {

void PrismXDxgi16ComHierarchyInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_dxgi16::InitializePrismDxgi16ComHierarchyExports();
}

} // namespace micant::sys::internal
