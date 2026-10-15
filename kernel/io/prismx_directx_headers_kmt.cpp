/**
 * @file prismx_directx_headers_kmt.cpp
 * @brief Horizon 80 (Unit #52): PrismX + Microsoft DirectX-Headers (CD3DX12 / DXCore)
 *        & WDDM 3.2 D3DKMT* Kernel-Mode Thunk Scanout Runtime.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directx_headers_kmt_runtime.hpp"

namespace micant::sys::internal {

void PrismXDirectXHeadersKmtInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_kmt_runtime::InitializePrismXDirectXHeadersKmtRuntimeExports();
}

} // namespace micant::sys::internal
