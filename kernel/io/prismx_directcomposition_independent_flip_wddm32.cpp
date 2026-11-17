/**
 * @file prismx_directcomposition_independent_flip_wddm32.cpp
 * @brief Horizon 321 (Kernel Unit #97): Sovereign MicaNT DirectComposition Independent Flip
 *        Visual Surface Swapchain Present Synchronization with VRR MPO Scanout Pipeline
 *        - DirectComposition visual tree binding to IDXGISwapChain4 surfaces
 *        - Dynamic Independent Flip (IFlip) promotion & MPO hardware plane allocation
 *        - Zero-copy display scanout elimination of intermediate composition passes
 *        - Variable Refresh Rate (VRR / G-Sync / Adaptive-Sync) beam-racing presentation
 *        - Robust window occlusion detection & seamless demotion to composed DWM
 *        - Pure ISO C++23. Zero External Dependencies.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directcomposition_independent_flip_wddm32.hpp"

namespace micant::um::prismx_present {

bool VerifyHorizon321DirectCompositionIndependentFlipSubsystem() noexcept {
    MicaNtDirectCompositionIndependentFlipGovernor gov;

    const prismx_composition::MicaNtRect displayViewport{ 0, 0, 1920, 1080 };

    // ------------------------------------------------------------------------
    // Pillar 1: Fullscreen Swapchain Binding & DirectFlip Promotion
    // ------------------------------------------------------------------------
    MicaNtSwapChainVisualBindingDesc fsDesc{};
    fsDesc.SwapChainId = 101;
    fsDesc.Width = 1920;
    fsDesc.Height = 1080;
    fsDesc.SwapEffect = 4; // DXGI_SWAP_EFFECT_FLIP_DISCARD

    MicaNtVisualNodePresenter fsNode{};
    fsNode.VisualNodeId = 1;
    fsNode.VisualRect = { 0, 0, 1920, 1080 };
    fsNode.Opacity = 1.0f;
    fsNode.TransformMatrix[0] = 1.0f; fsNode.TransformMatrix[1] = 0.0f;
    fsNode.TransformMatrix[2] = 0.0f; fsNode.TransformMatrix[3] = 1.0f;

    gov.BindSwapChainToVisual(fsDesc, fsNode);

    std::vector<prismx_composition::MicaNtRect> emptyOccluders{};
    auto mode1 = gov.EvaluatePromotion(0, emptyOccluders, displayViewport, true);
    if (mode1 != MicaNtPresentationMode::DirectFlip) return false;

    auto token1 = gov.Present(0, 0, 1'000'000'000ULL);
    if (token1.Mode != MicaNtPresentationMode::DirectFlip) return false;
    if (!token1.DirectFlipCommitted) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: Windowed Swapchain & Multi-Plane Overlay (MPO) Promotion
    // ------------------------------------------------------------------------
    MicaNtSwapChainVisualBindingDesc winDesc{};
    winDesc.SwapChainId = 102;
    winDesc.Width = 1280;
    winDesc.Height = 720;
    winDesc.SwapEffect = 4; // DXGI_SWAP_EFFECT_FLIP_DISCARD

    MicaNtVisualNodePresenter winNode{};
    winNode.VisualNodeId = 2;
    winNode.VisualRect = { 100, 100, 1380, 820 }; // 1280x720 window
    winNode.Opacity = 1.0f;
    winNode.TransformMatrix[0] = 1.0f; winNode.TransformMatrix[1] = 0.0f;
    winNode.TransformMatrix[2] = 0.0f; winNode.TransformMatrix[3] = 1.0f;

    gov.BindSwapChainToVisual(winDesc, winNode);

    auto mode2 = gov.EvaluatePromotion(1, emptyOccluders, displayViewport, true);
    if (mode2 != MicaNtPresentationMode::MultiPlaneOverlay) return false;

    auto token2 = gov.Present(1, 0, 1'001'851'851ULL);
    if (token2.Mode != MicaNtPresentationMode::MultiPlaneOverlay) return false;
    if (!token2.DirectFlipCommitted) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Dynamic Window Occlusion Demotion to Composed DWM
    // ------------------------------------------------------------------------
    std::vector<prismx_composition::MicaNtRect> occluders = {
        { 200, 200, 600, 500 } // Overlaps winNode (100, 100, 1380, 820)
    };

    auto mode3 = gov.EvaluatePromotion(1, occluders, displayViewport, true);
    if (mode3 != MicaNtPresentationMode::ComposedDWM) return false;

    auto token3 = gov.Present(1, 1, 1'003'703'702ULL);
    if (token3.Mode != MicaNtPresentationMode::ComposedDWM) return false;
    if (token3.DirectFlipCommitted) return false; // Demoted to composed Blt

    // ------------------------------------------------------------------------
    // Pillar 4: VRR Display Timing Synchronization & 540Hz Flip Cadence
    // ------------------------------------------------------------------------
    uint64_t simTimeNs = 1'010'000'000ULL;
    constexpr uint64_t dt540HzNs = 1'851'851ULL; // 1.85ms = 540Hz
    for (int frame = 0; frame < 120; ++frame) {
        simTimeNs += dt540HzNs;
        (void)gov.Present(0, 0, simTimeNs);
    }

    // ------------------------------------------------------------------------
    // Pillar 5: Flip Queue Telemetry & Latency Verification
    // ------------------------------------------------------------------------
    const auto& tel = gov.GetTelemetry();
    if (tel.TotalPresentsProcessed < 120ULL) return false;
    if (tel.DirectFlipPresents < 100ULL) return false;
    if (tel.MultiPlaneOverlayPresents < 1ULL) return false;
    if (tel.ComposedPresents < 1ULL) return false;
    if (tel.ActiveMpoPlanes < 1u) return false;
    if (tel.AverageFlipLatencyMicroseconds >= 80.0f) return false;
    if (!tel.ZeroCopyScanoutVerified) return false;
    if (!tel.VrrCadenceSyncVerified) return false;
    if (!tel.OcclusionDemotionVerified) return false;

    return true;
}

} // namespace micant::um::prismx_present
