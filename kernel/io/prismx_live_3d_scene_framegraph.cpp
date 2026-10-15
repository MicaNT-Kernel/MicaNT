/**
 * @file prismx_live_3d_scene_framegraph.cpp
 * @brief Horizon 89 Kernel Compilation Unit (#61):
 *        PrismX Live 3D Mesh Shader & DXR 1.1 Inline Raytracing Frame Graph.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_live_3d_scene_framegraph.hpp"

namespace micant::sys::internal {

void PrismXLive3DSceneFrameGraphInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_live_scene::InitializePrismXLive3DSceneFrameGraphExports();
}

} // namespace micant::sys::internal
