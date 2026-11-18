/**
 * @file prismx_directcomposition_virtual_surface_wddm32.cpp
 * @brief Horizon 324 (Kernel Unit #100): Sovereign MicaNT DirectComposition Virtual Surface
 *        Tiling, Sparse Allocation & High-DPI Resolution Scaling Engine
 *        - IDCompositionVirtualSurface sparse tiling architecture (arbitrary huge canvases e.g. 65536x65536)
 *        - On-demand physical tile allocation (e.g. 256x256 pixel tiles) and dirty rect invalidation
 *        - Active viewport trimming and LRU memory reclaiming
 *        - High-DPI perceptual resolution scaling (100%, 125%, 150%, 200%, 300%) with pixel snap alignment
 *        - Viewport culling and sparse VRAM memory conservation (>90% savings)
 *        - Pure ISO C++23. Zero External Dependencies.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directcomposition_virtual_surface_wddm32.hpp"

namespace micant::um::prismx_vsurface {

bool VerifyHorizon324DirectCompositionVirtualSurfaceSubsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: Huge Virtual Surface Creation (16,384 x 16,384, 256px Tiles)
    // 64 x 64 = 4,096 total potential tiles (1,073,741,824 bytes = 1 GB VRAM dense)
    // ------------------------------------------------------------------------
    MicaNtVirtualSurfaceDesc desc{
        .width = 16384,
        .height = 16384,
        .tileSize = 256,
        .dpiScale = 1.0f,
        .hasAlpha = true
    };
    MicaNtDirectCompositionVirtualSurface surface(desc);

    if (surface.GetTotalPotentialTiles() != 4096u) return false;
    if (surface.GetAllocatedTileCount() != 0u) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: On-Demand Sparse Tile Allocation (BeginDraw / EndDraw)
    // Draw into a 1920x1080 viewport region at (0, 0)
    // 1920 / 256 = 8 tiles wide; 1080 / 256 = 5 tiles high = 40 tiles
    // ------------------------------------------------------------------------
    MicaNtSurfaceRect view1{ 0, 0, 1920, 1080 };
    int32_t offX{ 0 }, offY{ 0 };
    if (!surface.BeginDraw(view1, offX, offY)) return false;
    if (offX != 0 || offY != 0) return false;
    if (!surface.EndDraw()) return false;

    if (surface.GetAllocatedTileCount() != 40u) return false;

    const auto& tel1 = surface.GetTelemetry();
    if (tel1.VramSavingsRatio < 0.95f) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Pan to New Region & Second On-Demand Draw Pass
    // Draw region at (4000, 4000) to (5000, 5000)
    // ------------------------------------------------------------------------
    MicaNtSurfaceRect view2{ 4000, 4000, 5000, 5000 };
    if (!surface.BeginDraw(view2, offX, offY)) return false;
    if (!surface.EndDraw()) return false;

    size_t allocAfterPan = surface.GetAllocatedTileCount();
    if (allocAfterPan <= 40u) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: Viewport Trimming & Memory Reclaiming
    // ------------------------------------------------------------------------
    uint32_t trimmed = surface.Trim(view2, 0);
    if (trimmed < 40u) return false;
    if (surface.GetAllocatedTileCount() >= allocAfterPan) return false;

    auto visible = surface.QueryVisibleTiles(view2);
    if (visible.empty()) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: High-DPI Resolution Scaling & Subpixel Snap Alignment
    // ------------------------------------------------------------------------
    surface.SetDpiScale(1.5f);
    if (std::abs(surface.LogicalToPhysical(100.0f) - 150.0f) > 0.001f) return false;
    if (std::abs(surface.PhysicalToLogical(150.0f) - 100.0f) > 0.001f) return false;

    float snapped = surface.SnapToDevicePixel(10.333f);
    float physicalPixel = snapped * 1.5f;
    if (std::abs(physicalPixel - std::round(physicalPixel)) > 0.001f) return false;

    const auto& finalTel = surface.GetTelemetry();
    if (!finalTel.SparseAllocationVerified) return false;
    if (!finalTel.HighDpiAlignmentVerified) return false;
    if (!finalTel.ViewportCullingVerified) return false;
    if (!finalTel.TileTrimmingVerified) return false;
    if (finalTel.AverageTileUpdateLatencyMicroseconds >= 100.0f) return false;

    return true;
}

} // namespace micant::um::prismx_vsurface
