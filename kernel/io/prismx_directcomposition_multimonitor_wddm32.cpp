/**
 * @file prismx_directcomposition_multimonitor_wddm32.cpp
 * @brief Horizon 328 (Kernel Unit #104): Sovereign MicaNT DirectComposition Multi-Monitor Virtual Desktop
 *        Topology, Cross-Adapter Display Clone / Extend Engine & Per-Monitor DPI Synchronization Pipeline
 *        - Multi-monitor virtual desktop output topology graph (IDCompositionDesktopDevice, IDCompositionTarget)
 *        - Cross-adapter shared visual surface synchronization (dGPU -> iGPU/secondary scanout) with cross-adapter fences
 *        - Per-monitor independent VSync and asymmetric refresh cadence pacing (540Hz primary + 144Hz secondary)
 *        - Per-monitor fractional DPI scaling (PROCESS_PER_MONITOR_DPI_AWARE_V2) boundary transition matrix interpolation
 *        - Cross-monitor cursor continuous boundary clamping & multi-monitor hit-test routing (<100us latency)
 *        - Pure ISO C++23. Zero External Dependencies.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directcomposition_multimonitor_wddm32.hpp"

namespace micant::um::prismx_multimonitor {

bool VerifyHorizon328DirectCompositionMultiMonitorSubsystem() noexcept {
    MicaNtVirtualDesktopTopologyManager mgr;

    // ------------------------------------------------------------------------
    // Pillar 1: Multi-Monitor Virtual Desktop Topology Graph & Rect Calculation
    // Primary: 3840x2160 @ 540Hz (200% DPI) on dGPU (0x1000)
    // Secondary: 2560x1440 @ 144Hz (100% DPI) on iGPU (0x2000), positioned at (3840, 0)
    // ------------------------------------------------------------------------
    MicaNtDisplayMonitorDesc primary{
        .outputId = 0,
        .friendlyName = "PrismX UltraClear 540Hz OLED",
        .bounds = MicaNtRectI{ 0, 0, 3840, 2160 },
        .nativeWidth = 3840,
        .nativeHeight = 2160,
        .refreshRateHz = 540,
        .dpiScalePercent = 200,
        .adapterLuid = 0x1000,
        .isPrimary = true,
        .mode = MicaNtDisplayMode::Extend
    };
    if (!mgr.AddMonitor(primary)) return false;

    MicaNtDisplayMonitorDesc secondary{
        .outputId = 1,
        .friendlyName = "PrismX ProArt 144Hz IPS",
        .bounds = MicaNtRectI{ 3840, 0, 6400, 1440 },
        .nativeWidth = 2560,
        .nativeHeight = 1440,
        .refreshRateHz = 144,
        .dpiScalePercent = 100,
        .adapterLuid = 0x2000,
        .isPrimary = false,
        .mode = MicaNtDisplayMode::Extend
    };
    if (!mgr.AddMonitor(secondary)) return false;

    MicaNtRectI combined = mgr.GetCombinedDesktopBounds();
    if (combined.left != 0 || combined.top != 0 || combined.right != 6400 || combined.bottom != 2160) {
        return false;
    }
    if (combined.Width() != 6400 || combined.Height() != 2160) {
        return false;
    }

    const auto* mon0 = mgr.FindMonitorAt(100, 100);
    if (!mon0 || mon0->outputId != 0) return false;

    const auto* mon1 = mgr.FindMonitorAt(4000, 500);
    if (!mon1 || mon1->outputId != 1) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: Cross-Adapter Shared Surface Synchronization & Fence Signalling
    // ------------------------------------------------------------------------
    MicaNtCrossAdapterSharedSurface crossSurf;
    if (!crossSurf.Allocate(501, 0x1000, 0x2000, 2560, 1440, 4)) return false;
    if (!mgr.RegisterCrossAdapterSurface(crossSurf)) return false;

    if (!mgr.TransferCrossAdapterFrame(501, 100)) return false;
    if (!mgr.TransferCrossAdapterFrame(501, 101)) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Per-Monitor Independent VSync & Asymmetric Refresh Cadence Pacing
    // ------------------------------------------------------------------------
    mgr.QueueOutputPresent(0, 1001, 0);
    mgr.QueueOutputPresent(0, 1002, 1851);
    mgr.QueueOutputPresent(0, 1003, 3702);

    mgr.QueueOutputPresent(1, 2001, 0);

    if (!mgr.DispatchOutputVBlank(0, 1851)) return false;
    if (!mgr.DispatchOutputVBlank(0, 3702)) return false;

    if (!mgr.DispatchOutputVBlank(1, 6944)) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: Per-Monitor Fractional DPI Scaling & Transition Matrix Interpolation
    // ------------------------------------------------------------------------
    MicaNtRectI winInside0{ 500, 500, 1500, 1500 };
    auto dpi0 = mgr.EvaluateDpiTransition(winInside0);
    if (dpi0.dominantOutputId != 0 || dpi0.isSpanningMultipleMonitors) return false;
    if (std::abs(dpi0.effectiveScale - 2.0f) > 0.001f) return false;

    MicaNtRectI winInside1{ 4000, 200, 5000, 1000 };
    auto dpi1 = mgr.EvaluateDpiTransition(winInside1);
    if (dpi1.dominantOutputId != 1 || dpi1.isSpanningMultipleMonitors) return false;
    if (std::abs(dpi1.effectiveScale - 1.0f) > 0.001f) return false;

    // 50/50 split between 2.0x and 1.0x -> effective scale = 1.5x
    MicaNtRectI winSpanning{ 3340, 200, 4340, 700 };
    auto dpiSpan = mgr.EvaluateDpiTransition(winSpanning);
    if (!dpiSpan.isSpanningMultipleMonitors) return false;
    if (std::abs(dpiSpan.effectiveScale - 1.5f) > 0.01f) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Cross-Monitor Cursor Clamping & Hit-Test Routing Telemetry
    // ------------------------------------------------------------------------
    uint32_t outId = 999;
    int32_t lx = 0, ly = 0;
    if (!mgr.RouteHitTest(200, 300, outId, lx, ly)) return false;
    if (outId != 0 || lx != 200 || ly != 300) return false;

    if (!mgr.RouteHitTest(4000, 100, outId, lx, ly)) return false;
    if (outId != 1 || lx != (4000 - 3840) || ly != 100) return false;

    int32_t deadX = 5000;
    int32_t deadY = 1800;
    if (!mgr.ClampCursorPosition(deadX, deadY)) return false;
    if (deadX != 5000 || deadY != 1439) return false;

    const auto& tel = mgr.GetTelemetry();
    if (tel.MonitorsActive != 2) return false;
    if (tel.CrossAdapterFramesTransferred < 2) return false;
    if (tel.IndependentVBlankEventsDispatched < 3) return false;
    if (tel.DpiTransitionsCalculated < 3) return false;
    if (tel.CursorClampsExecuted < 1) return false;
    if (tel.HitTestsRouted < 2) return false;
    if (tel.AverageTopologyDispatchLatencyMicroseconds > 100.0f) return false;

    return true;
}

} // namespace micant::um::prismx_multimonitor
