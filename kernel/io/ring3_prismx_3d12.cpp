/**
 * @file ring3_prismx_3d12.cpp
 * @brief Horizon 71: Sovereign Ring 3 PrismX Presentation, Prism3D12, PrismShaderVM &
 *        PrismViewer Tunneling Through Ring 1 540 Hz DirectFlip MPO & Ring 0 dxgkrnl
 *        Compilation Unit (base/ntos/io/ring3_prismx_3d12_shader_tunnel).
 */

#include <iostream>
#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/ring3_prismx_3d12_shader_tunnel.hpp"

namespace micant::sys::internal {

void Ring3PrismX3D12InitializeExecutiveSubsystem() noexcept {
    auto& mgr = micant::um::prismx_tunnel::SovereignPrismX3D12ShaderTunnelManager::get();
    mgr.initializeAndVerifyPrismXTunnel();
    const auto& t = mgr.telemetry();
    std::cout << "[MicaNT Ring 3 PrismX] Adapter=\"" << t.primaryAdapterName
              << "\" (" << (t.dedicatedVideoMemoryBytes / (1024 * 1024))
              << " MB VRAM) | D3D12 CmdLists=" << t.executedCommandLists
              << " (Fence=" << t.completedFenceValue
              << ") | ShaderVM=" << t.shaderVmExecutedInstructions
              << " ops | ViewerFrames=" << t.viewerRenderedFrames
              << " (Crystal=" << t.viewerCrystalTriangles
              << " tris) | R1 MPO=" << t.ring1DirectFlipRefreshHz << "Hz ("
              << t.ring1ScanoutLatencyUs << " us, " << t.ring1DwmCopies << " copies)\n";
}

} // namespace micant::sys::internal
