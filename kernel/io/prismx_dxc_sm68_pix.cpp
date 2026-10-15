/**
 * @file prismx_dxc_sm68_pix.cpp
 * @brief Horizon 83 Kernel Compilation Unit (#55):
 *        PrismX + Official Microsoft DirectXShaderCompiler (IDxcCompiler3 SM 6.8 & PIX Runtime).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_dxc_sm68_pix_runtime.hpp"

namespace micant::sys::internal {

void PrismXDxcSm68PixInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_dxc_sm68::InitializePrismXDxcSm68PixExports();
}

} // namespace micant::sys::internal
