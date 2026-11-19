/**
 * @file prismx_directcomposition_occlusion_damage_wddm32.cpp
 * @brief Horizon 329 (Kernel Unit #105): Sovereign MicaNT DirectComposition Dynamic Surface Occlusion
 *        Culling, Dirty-Rect Incremental Blitting & Power-Gated Damage Region Pipeline
 *        - Front-to-back visual subtree occlusion culling & Z-order depth slicing
 *        - Fine-grained damage region tracking (dirty rect lists) & spatial union merging
 *        - Incremental tile blitting & partial swapchain present (DXGI_PRESENT_PARAMETERS)
 *        - Power-gated frame throttling & hardware Panel Self-Refresh (PSR) low-power coordination
 *        - Compositor damage telemetry & sub-80us dispatch latency
 *        - Pure ISO C++23. Zero External Dependencies.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directcomposition_occlusion_damage_wddm32.hpp"

namespace micant::um::prismx_damage_culling {

bool VerifyHorizon329DirectCompositionOcclusionDamageSubsystem() noexcept {
    MicaNtOcclusionDamageCompositor compositor(3840, 2160);

    // ------------------------------------------------------------------------
    // Pillar 1: Front-to-Back Occlusion Culling Traversal & Z-Order Pruning
    // ------------------------------------------------------------------------
    MicaNtVisualNode backgroundNode{
        .nodeId = 1001,
        .zIndex = 0,
        .bounds = MicaNtRectI{ 0, 0, 3840, 2160 },
        .opacity = 1.0f,
        .isOpaque = true,
        .isOccluded = false
    };
    compositor.AddVisual(backgroundNode);

    MicaNtVisualNode foregroundDialog{
        .nodeId = 1002,
        .zIndex = 10,
        .bounds = MicaNtRectI{ 0, 0, 3840, 2160 },
        .opacity = 1.0f,
        .isOpaque = true,
        .isOccluded = false
    };
    compositor.AddVisual(foregroundDialog);

    MicaNtVisualNode tooltipNode{
        .nodeId = 1003,
        .zIndex = 20,
        .bounds = MicaNtRectI{ 100, 100, 500, 300 },
        .opacity = 0.95f,
        .isOpaque = false,
        .isOccluded = false
    };
    compositor.AddVisual(tooltipNode);

    compositor.PerformOcclusionCulling();

    const auto& visuals = compositor.GetVisuals();
    if (visuals[0].nodeId != 1003 || visuals[0].isOccluded) return false;
    if (visuals[1].nodeId != 1002 || visuals[1].isOccluded) return false;
    if (visuals[2].nodeId != 1001 || !visuals[2].isOccluded) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: Fine-Grained Damage Tracking & Spatial Union Coalescing
    // ------------------------------------------------------------------------
    MicaNtVisualNode* dialogPtr = compositor.FindVisual(1002);
    if (!dialogPtr) return false;

    dialogPtr->InvalidateRect(MicaNtRectI{ 500, 500, 700, 700 });
    if (dialogPtr->dirtyRects.size() != 1) return false;
    if (dialogPtr->GetDamagedArea() != 40000) return false;

    dialogPtr->InvalidateRect(MicaNtRectI{ 600, 600, 800, 800 });
    if (dialogPtr->dirtyRects.size() != 2) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Incremental Partial Present Parameters (DXGI_PRESENT_PARAMETERS)
    // ------------------------------------------------------------------------
    MicaNtPresentParameters params = compositor.EvaluatePresentParameters();
    if (!params.isPartialPresent) return false;
    if (params.dirtyRects.size() != 2) return false;
    if (params.bandwidthSavingsRatio < 0.90f) return false;

    compositor.CommitFrame();

    // ------------------------------------------------------------------------
    // Pillar 4: Power-Gated Panel Self-Refresh (PSR) State Transitions
    // ------------------------------------------------------------------------
    MicaNtPresentParameters staticParams = compositor.EvaluatePresentParameters();
    if (staticParams.isPartialPresent) return false;
    if (compositor.GetPsrState() != MicaNtPsrState::Active) return false;

    dialogPtr->InvalidateRect(MicaNtRectI{ 100, 100, 150, 150 });
    MicaNtPresentParameters wakeParams = compositor.EvaluatePresentParameters();
    if (compositor.GetPsrState() != MicaNtPsrState::Exiting) return false;
    if (!wakeParams.isPartialPresent) return false;

    compositor.CommitFrame();
    if (compositor.GetPsrState() != MicaNtPsrState::Disabled) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Compositor Damage Telemetry Verification
    // ------------------------------------------------------------------------
    const auto& tel = compositor.GetTelemetry();
    if (tel.FramesComposed < 3) return false;
    if (tel.TotalVisualsEvaluated < 3) return false;
    if (tel.VisualsCulledByOcclusion < 1) return false;
    if (tel.DirtyRectsProcessed < 2) return false;
    if (tel.FullFramePresentsBypassed < 1) return false;
    if (tel.PsrTransitionsEntered < 1) return false;
    if (tel.PsrTransitionsExited < 1) return false;
    if (tel.AverageDamageEvaluationLatencyMicroseconds > 80.0f) return false;

    return true;
}

} // namespace micant::um::prismx_damage_culling
