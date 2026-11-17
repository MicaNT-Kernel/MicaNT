/**
 * @file prismx_directwrite_cleartype_wddm32.cpp
 * @brief Horizon 318 (Kernel Unit #94): Sovereign MicaNT DirectWrite Hardware-Accelerated Typography
 *        & Sub-Pixel ClearType GPU Compute Pipeline
 *        - OpenType / TrueType SFNT Table Metrics Parsing (cmap, hmtx, hhea, head, kern)
 *        - Sub-Pixel RGB LCD ClearType 5-Tap FIR Filtering with Gamma 2.2 Correction
 *        - GPU Dynamic Glyph Atlas Cache Manager (2048x2048 Shelf Packer with 1px Border)
 *        - Pairwise Kerning Evaluation, Baseline Alignment, and Text Run Layout Engine
 *        - Sub-Millisecond Font Rasterization & Vector Glyph Layout (<450us).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directwrite_cleartype_wddm32.hpp"

namespace micant::um::prismx_typography {

bool VerifyHorizon318DirectWriteTypographySubsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: Font Design Metrics
    // ------------------------------------------------------------------------
    MicaNtDirectWriteEngine engine(2048, 2048);

    const auto& metrics = engine.GetFontDesignMetrics();
    if (metrics.UnitsPerEm != 2048) return false;
    if (metrics.Ascender != 1900) return false;
    if (metrics.Descender != -500) return false;
    if (metrics.CapHeight != 1450) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: OpenType Pairwise Kerning Tables
    // ------------------------------------------------------------------------
    if (engine.GetKerning(65, 86) != -150) return false;  // 'A' and 'V'
    if (engine.GetKerning(84, 111) != -120) return false; // 'T' and 'o'
    if (engine.GetKerning(88, 90) != 0) return false;     // 'X' and 'Z'

    // ------------------------------------------------------------------------
    // Pillar 3: Dynamic GPU Glyph Atlas Shelf Packing
    // ------------------------------------------------------------------------
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    if (!engine.AllocateAtlasSlot(32, 48, u0, v0, u1, v1)) return false;
    if (u0 < 0.0f || u1 > 1.0f || u1 <= u0) return false;
    if (v0 < 0.0f || v1 > 1.0f || v1 <= v0) return false;

    float u0_b = 0.0f, v0_b = 0.0f, u1_b = 0.0f, v1_b = 0.0f;
    if (!engine.AllocateAtlasSlot(64, 48, u0_b, v0_b, u1_b, v1_b)) return false;
    if (u0_b <= u0) return false; // Shelf advancing along X axis

    // ------------------------------------------------------------------------
    // Pillar 4: ClearType 5-Tap Symmetric FIR Sub-Pixel Filtering
    // ------------------------------------------------------------------------
    std::array<float, 7> edgeSamples = { 0.0f, 0.0f, 0.4f, 0.8f, 1.0f, 1.0f, 1.0f };
    auto subSample = engine.FilterClearTypeSubPixel(edgeSamples);
    if (subSample.r > subSample.g || subSample.g > subSample.b) return false;
    if (subSample.r < 0.0f || subSample.b > 1.0f) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Gamma 2.2 Color Correction & Sub-Pixel Blending
    // ------------------------------------------------------------------------
    prismx_composition::MicaNtCompositionColor bg{ 1.0f, 1.0f, 1.0f, 1.0f }; // White background
    prismx_composition::MicaNtCompositionColor fg{ 0.0f, 0.0f, 0.0f, 1.0f }; // Black text
    MicaNtClearTypeSubPixelSample subCoverage{ 0.25f, 0.60f, 0.90f };
    auto blended = engine.BlendClearTypeSubPixel(bg, fg, subCoverage, 2.2f);
    if (blended.r <= blended.g || blended.g <= blended.b) return false; // Sub-pixel RGB differentiation
    if (blended.a != 1.0f) return false;

    // ------------------------------------------------------------------------
    // Pillar 6: Formatted Text Run Layout
    // ------------------------------------------------------------------------
    MicaNtTextLayoutRunDesc run{};
    run.Text = "AV Top Wave";
    run.FontSize = 24.0f;
    run.EnableKerning = true;
    run.EnableClearType = true;

    auto layout = engine.ComputeTextLayout(run);
    if (layout.Glyphs.size() != 11) return false;
    if (layout.TotalWidth <= 0.0f) return false;
    if (layout.BaselineY <= 0.0f) return false;
    if (layout.Glyphs[1].X <= layout.Glyphs[0].X) return false;

    // ------------------------------------------------------------------------
    // Pillar 7: Telemetry Verification
    // ------------------------------------------------------------------------
    const auto& tel = engine.GetTelemetry();
    if (tel.TotalGlyphsCached < 2u) return false;
    if (tel.AtlasAllocations < 2u) return false;
    if (tel.ClearTypeFilterPasses < 1u) return false;
    if (tel.TextLayoutsComputed < 1u) return false;
    if (tel.AverageRasterizationLatencyMicroseconds >= 450.0f) return false;
    if (!tel.SubPixelRgbFilteringVerified) return false;
    if (!tel.GammaCorrection22Verified) return false;
    if (!tel.AtlasShelfPackingValid) return false;

    return true;
}

} // namespace micant::um::prismx_typography
