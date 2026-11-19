/**
 * @file prismx_directcomposition_surface_factory_wddm32.cpp
 * @brief Horizon 331 (Kernel Unit #107): Sovereign MicaNT DirectComposition Hardware Surface Sharing,
 *        Direct3D 12 Interop (IDCompositionSurfaceFactory) & D3D11On12 Zero-Copy Texture Interlock Pipeline
 *        - DirectComposition surface factory (IDCompositionSurfaceFactory) & NT shared handle binding
 *        - Cross-API D3D12 / D3D11On12 shared texture interlock & synchronization fences
 *        - Multisampled (MSAA) texture resolve on presentation
 *        - Dynamic window resize surface recycling pool (>90% reallocation elimination)
 *        - Compositor interlock telemetry & sub-60us latency
 *        - Pure ISO C++23. Zero External Dependencies.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directcomposition_surface_factory_wddm32.hpp"

namespace micant::um::prismx_surface_factory {

bool VerifyHorizon331DirectCompositionSurfaceFactorySubsystem() noexcept {
    MicaNtDirectCompositionSurfaceFactory factory;

    // ------------------------------------------------------------------------
    // Pillar 1: Surface Creation from NT Shared Handle (D3D12/D3D11On12 Interop)
    // ------------------------------------------------------------------------
    MicaNtSharedTextureDesc desc12{
        .width = 3840,
        .height = 2160,
        .format = 28,
        .sampleCount = 1,
        .sampleQuality = 0,
        .flags = MicaNtResourceFlag_AllowShared | MicaNtResourceFlag_AllowRenderTarget,
        .sharedHandle = 0xCAFE1001
    };
    if (!factory.CreateSurfaceFromHandle(0xCAFE1001, desc12, 5001)) return false;
    if (factory.GetSurfaceCount() != 1) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: Cross-API Fence Synchronization Interlock
    // ------------------------------------------------------------------------
    if (!factory.RegisterSharedFence(0xDEAD5001)) return false;
    if (!factory.SynchronizeInterlock(0xDEAD5001, 1)) return false;
    if (!factory.SynchronizeInterlock(0xDEAD5001, 2)) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Multisampled (4x MSAA) Texture Resolve on Presentation
    // ------------------------------------------------------------------------
    MicaNtSharedTextureDesc msaaDesc{
        .width = 2560,
        .height = 1440,
        .format = 28,
        .sampleCount = 4,
        .sampleQuality = 0,
        .flags = MicaNtResourceFlag_AllowShared | MicaNtResourceFlag_AllowRenderTarget,
        .sharedHandle = 0xCAFE1002
    };
    if (!factory.CreateSurfaceFromHandle(0xCAFE1002, msaaDesc, 5002)) return false;

    uint32_t msaaSurfId = static_cast<uint32_t>(0xCAFE1002 & 0xFFFFFFFF);
    if (!factory.ResolveAndPresentSurface(msaaSurfId)) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: Window Resize Surface Recycling Pool
    // ------------------------------------------------------------------------
    auto s1 = factory.AcquireRecycledSurface(1920, 1080, 28);
    auto s2 = factory.AcquireRecycledSurface(2560, 1440, 28);

    factory.RecycleSurface(s1);
    auto s1Reused = factory.AcquireRecycledSurface(1920, 1080, 28);
    if (s1Reused.surfaceId != s1.surfaceId) return false;

    factory.RecycleSurface(s2);
    auto s2Reused = factory.AcquireRecycledSurface(2560, 1440, 28);
    if (s2Reused.surfaceId != s2.surfaceId) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Compositor Interlock Telemetry & Sub-60us Latency
    // ------------------------------------------------------------------------
    const auto& tel = factory.GetTelemetry();
    if (tel.SharedSurfacesCreated < 2) return false;
    if (tel.SharedFencesSynchronized < 2) return false;
    if (tel.MsaaResolvesExecuted < 1) return false;
    if (tel.RecycledSurfacesReused < 2) return false;
    if (tel.TotalPresentsSynchronized < 1) return false;
    if (tel.AverageInterlockDispatchLatencyMicroseconds > 60.0f) return false;

    return true;
}

} // namespace micant::um::prismx_surface_factory
