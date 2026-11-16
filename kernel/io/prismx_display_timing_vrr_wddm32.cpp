/**
 * @file prismx_display_timing_vrr_wddm32.cpp
 * @brief Horizon 310 (Kernel Unit #86): Sovereign MicaNT Kernel WDDM 3.2 Display Timing Engine,
 *        Variable Refresh Rate (VRR) & Multi-Plane Overlay (MPO) DirectFlip Architecture
 *        - Clean-Room Native DisplayPort / HDMI CRTC Timing Controller
 *        - VESA Adaptive-Sync, G-Sync & FreeSync VRR Kernel Cadence Governor
 *        - Tier-3 Hardware Multi-Plane Overlay (MPO) with DirectFlip Zero-Copy Scanout
 *        - Low Framerate Compensation (LFC) Kernel Cadence Doubler below VRR Floor
 *        - Microsecond-Accurate Scanline Beam Tracking & Sub-1.5ms Click-to-Scanout Latency.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_display_timing_vrr_wddm32.hpp"

namespace micant::um::prismx_display {

bool VerifyHorizon310DisplayTimingVrrSubsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: Display Timing & VRR ABI Constants & Enums
    // ------------------------------------------------------------------------
    if (VRR_MODE_DISABLED != 0u || VRR_MODE_ADAPTIVE_SYNC != 1u ||
        VRR_MODE_GSYNC_COMPATIBLE != 2u || VRR_MODE_FREESYNC_PREMIUM_PRO != 3u) {
        return false;
    }
    if (MPO_PLANE_DESKTOP_UI != 0u || MPO_PLANE_3D_VIEWPORT != 1u || MPO_PLANE_HARDWARE_CURSOR != 2u) {
        return false;
    }

    // ------------------------------------------------------------------------
    // Pillar 2: Sovereign Kernel Display Timing Controller Engine Initialization
    // ------------------------------------------------------------------------
    MicaNtDisplayTimingDescriptor timing{};
    timing.HorizontalActive = 3840;
    timing.VerticalActive = 2160;
    timing.PixelClockMHz = 1188.0f; // 4K 144Hz
    timing.MinRefreshHz = 48;       // VRR floor
    timing.MaxRefreshHz = 540;      // 540Hz ultra-refresh ceiling
    timing.VrrMode = VRR_MODE_ADAPTIVE_SYNC;

    MicaNtDisplayTimingEngine engine;
    NTSTATUS status = engine.Initialize(timing);
    if (!NT_SUCCESS(status)) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Microsecond Scanline Raster Beam Prediction
    // ------------------------------------------------------------------------
    uint32_t scanline = 0;
    bool inVBlank = false;

    // Mid-frame at 50%
    status = engine.QueryScanline(0.5f, scanline, inVBlank);
    if (!NT_SUCCESS(status) || scanline < 1000 || scanline > 1200 || inVBlank) {
        return false;
    }

    // End-of-frame inside VBlank at 99%
    status = engine.QueryScanline(0.99f, scanline, inVBlank);
    if (!NT_SUCCESS(status) || scanline < 2160 || !inVBlank) {
        return false;
    }

    // ------------------------------------------------------------------------
    // Pillar 4: Variable Refresh Rate (VRR) & Low Framerate Compensation (LFC)
    // ------------------------------------------------------------------------
    bool directFlipActive = false;
    float observedHz = 0.0f;

    // Present 1: 30 FPS rendering (33.33ms) - triggers LFC cadence doubling
    status = engine.DispatchPresent(33.33f, directFlipActive, observedHz);
    if (!NT_SUCCESS(status) || !directFlipActive || observedHz < 55.0f || engine.GetLfcEngagements() != 1u) {
        return false;
    }

    // Present 2: 144 FPS rendering (6.94ms) - native VRR pacing
    status = engine.DispatchPresent(6.94f, directFlipActive, observedHz);
    if (!NT_SUCCESS(status) || !directFlipActive || observedHz < 140.0f || observedHz > 145.0f) {
        return false;
    }

    // ------------------------------------------------------------------------
    // Pillar 5: Multi-Plane Overlay (MPO) Windowed Fallback to Compositor
    // ------------------------------------------------------------------------
    MicaNtMpoPlaneConfig windowedPlane{};
    windowedPlane.PlaneId = MPO_PLANE_3D_VIEWPORT;
    windowedPlane.Width = 1920;
    windowedPlane.Height = 1080;
    windowedPlane.DirectFlipEnabled = false;

    status = engine.ConfigurePlane(windowedPlane);
    if (!NT_SUCCESS(status)) return false;

    // Present 3: Windowed mode disables DirectFlip
    status = engine.DispatchPresent(6.94f, directFlipActive, observedHz);
    if (!NT_SUCCESS(status) || directFlipActive) {
        return false; // DirectFlip must be inactive for windowed viewport
    }

    // ------------------------------------------------------------------------
    // Pillar 6: Telemetry & Quality Counters
    // ------------------------------------------------------------------------
    if (engine.GetTotalPresents() != 3u) return false;
    if (engine.GetDirectFlipPresents() != 2u) return false;
    if (engine.GetCompositedPresents() != 1u) return false;
    if (engine.GetLfcEngagements() != 1u) return false;

    return true;
}

} // namespace micant::um::prismx_display
