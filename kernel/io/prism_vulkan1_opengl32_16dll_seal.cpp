/**
 * @file prism_vulkan1_opengl32_16dll_seal.cpp
 * @brief Horizon 109 (Kernel Unit #81): Sovereign PrismX Official Khronos Vulkan 1.3 Loader
 *        (vulkan-1.dll) & OpenGL 4.6 WGL Runtime (opengl32.dll) + 16-DLL System32 Seal.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_vulkan1_opengl32_16dll_seal.hpp"

namespace micant::sys::internal {

void PrismXVulkan1OpenGL32SixteenDllSealInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_vulkan1_opengl32_seal::InitializePrismSystem32SixteenDllSealExports();
}

} // namespace micant::sys::internal
