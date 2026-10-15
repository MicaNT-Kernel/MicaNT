/**
 * @file prismx_directstorage_gdeflate.cpp
 * @brief Horizon 84 Kernel Compilation Unit (#56):
 *        PrismX + Official Microsoft DirectStorage GDeflate::TileStream GPU Decompression.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directstorage_gdeflate_gpu.hpp"

namespace micant::sys::internal {

void PrismXGDeflateGpuInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_gdeflate_gpu::InitializePrismXGDeflateGpuExports();
}

} // namespace micant::sys::internal
