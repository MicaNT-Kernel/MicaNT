/**
 * @file prismx_directcomposition_hardware_cursor_wddm32.cpp
 * @brief Horizon 332 (Kernel Unit #108): Sovereign MicaNT DirectComposition DirectFlip / Independent Flip
 *        Hardware Cursor Overlay, High-Precision Sub-Pixel Pointer Warp & 8000Hz Hardware Plane Composition Pipeline
 *        - Dedicated hardware cursor overlay scanout plane (MPO Plane 2) bypassing desktop recomposition
 *        - High-precision sub-pixel pointer warping and floating-point position interpolation
 *        - 8000Hz polling rate microsecond cadence coalescing (125us interval) with zero frame drops
 *        - Dynamic cursor bitmap shape realization (ARGB 32-bit alpha and hardware inverted color modes)
 *        - Real-time cursor telemetry & sub-20us hardware dispatch latency
 *        - Pure ISO C++23. Zero External Dependencies.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directcomposition_hardware_cursor_wddm32.hpp"

namespace micant::um::prismx_hardware_cursor {

bool VerifyHorizon332DirectCompositionHardwareCursorSubsystem() noexcept {
    MicaNtHardwareCursorOverlayEngine engine;

    // ------------------------------------------------------------------------
    // Pillar 1: Dedicated Hardware Cursor Scanout Plane Initialization
    // ------------------------------------------------------------------------
    if (!engine.InitializeHardwarePlane(64, 64)) return false;
    const auto& initRegs = engine.GetRegisters();
    if (!initRegs.planeEnabled) return false;
    if (initRegs.width != 64 || initRegs.height != 64) return false;
    if (initRegs.surfaceAddress != 0xFED00000ull) return false;

    // ------------------------------------------------------------------------
    // Pillar 2 & 3: 8000Hz Input Ingestion & Sub-Pixel Pointer Warp
    // ------------------------------------------------------------------------
    for (uint32_t i = 0; i < 8; ++i) {
        float x = 500.0f + static_cast<float>(i) * 1.25f;
        float y = 300.0f + static_cast<float>(i) * 0.75f;
        uint64_t t = 1000000ull + i * 125ull;
        MicaNtMouseInputPacket pkt{ .rawX = x, .rawY = y, .timestampUs = t, .buttonState = 0 };
        if (!engine.ProcessInputPacket(pkt)) return false;
    }

    const auto& pos = engine.GetCurrentPosition();
    if (std::abs(pos.x - (500.0f + 7 * 1.25f)) > 0.001f) return false;
    if (std::abs(pos.y - (300.0f + 7 * 0.75f)) > 0.001f) return false;

    const auto& curRegs = engine.GetRegisters();
    if (curRegs.crtcPositionX != static_cast<int32_t>(pos.x)) return false;
    if (curRegs.crtcPositionY != static_cast<int32_t>(pos.y)) return false;
    if (curRegs.subPixelOffsetX <= 0.0f || curRegs.subPixelOffsetY <= 0.0f) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: Dynamic Cursor Shape Realization (ARGB & Invert)
    // ------------------------------------------------------------------------
    MicaNtCursorShapeDesc arrowShape{
        .width = 32,
        .height = 32,
        .hotSpotX = 2,
        .hotSpotY = 2,
        .blendMode = MicaNtCursorBlendMode::AlphaBlend32,
        .shapeType = MicaNtCursorShapeType::Arrow,
        .pixelData = std::vector<uint32_t>(32 * 32, 0xFFFFFFFF)
    };
    if (!engine.SetCursorShape(arrowShape)) return false;

    MicaNtMouseInputPacket pktHot{ .rawX = 1000.5f, .rawY = 500.25f, .timestampUs = 1001000ull, .buttonState = 1 };
    if (!engine.ProcessInputPacket(pktHot)) return false;

    const auto& hotRegs = engine.GetRegisters();
    if (hotRegs.crtcPositionX != 998) return false;
    if (hotRegs.crtcPositionY != 498) return false;

    MicaNtCursorShapeDesc invertShape{
        .width = 32,
        .height = 32,
        .hotSpotX = 16,
        .hotSpotY = 16,
        .blendMode = MicaNtCursorBlendMode::InvertColor,
        .shapeType = MicaNtCursorShapeType::Crosshair,
        .pixelData = std::vector<uint32_t>(32 * 32, 0xFF000000)
    };
    if (!engine.SetCursorShape(invertShape)) return false;
    if (engine.GetRegisters().blendMode != static_cast<uint32_t>(MicaNtCursorBlendMode::InvertColor)) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Real-Time Cursor Telemetry Verification
    // ------------------------------------------------------------------------
    const auto& tel = engine.GetTelemetry();
    if (tel.CursorPacketsProcessed < 9) return false;
    if (tel.HardwarePlaneUpdates < 9) return false;
    if (tel.SubPixelWarpsApplied < 5) return false;
    if (tel.ShapeChangesCommitted < 2) return false;
    if (tel.AverageHardwareCursorLatencyMicroseconds > 20.0f) return false;

    return true;
}

} // namespace micant::um::prismx_hardware_cursor
