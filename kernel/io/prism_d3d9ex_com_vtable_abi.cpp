/**
 * @file prism_d3d9ex_com_vtable_abi.cpp
 * @brief Horizon 104 (Kernel Unit #76): Sovereign PrismX Official Microsoft Direct3D 9 / 9Ex
 *        (<d3d9.h> / d3d9.dll) 21-Slot IDirect3D9Ex & IDirect3DDevice9Ex COM VTable ABI.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_d3d9ex_com_vtable_abi.hpp"

namespace micant::sys::internal {

void PrismXD3D9ExComVTableAbiInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_d3d9_abi::InitializePrismD3D9ExComAbiExports();
}

} // namespace micant::sys::internal
