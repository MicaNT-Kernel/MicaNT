/**
 * @file prismx_vulkanon12_video_av1.cpp
 * @brief Horizon 87 Kernel Compilation Unit (#59):
 *        PrismX + VulkanOn12 HLK Conformance & D3D12 Video AV1/HEVC Hardware Encoder.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_vulkanon12_video_av1_hevc.hpp"

namespace micant::sys::internal {

void PrismXVulkanOn12VideoAv1InitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_vulkanon12_video::InitializePrismXVulkanOn12AndVideoExports();
}

} // namespace micant::sys::internal
