/**
 * @file prismx_direct2d_geometry_wddm32.cpp
 * @brief Horizon 319 (Kernel Unit #95): Sovereign MicaNT Direct2D Hardware-Accelerated
 *        Geometry Realization & GPU Path Tessellation Pipeline
 *        - Hardware-Accelerated Path Geometry Sink (Lines, Quadratic & Cubic Béziers)
 *        - Adaptive De Casteljau Recursive Curve Subdivision (<0.25px Chord Tolerance)
 *        - Triangle Strip & Fan Fill Tessellation with Even-Odd / Non-Zero Winding Rules
 *        - Anti-Aliased Stroke Realization with Miter Joins, Line Caps, and Dash Patterns
 *        - Multi-Stop Linear & Radial Gradient Brush Sampler with Sub-Pixel Interpolation
 *        - Sub-Millisecond Path Realization Latency (<420us).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_direct2d_geometry_wddm32.hpp"

namespace micant::um::prismx_geometry {

bool VerifyHorizon319Direct2DGeometrySubsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: Path Figure Construction & Adaptive Bézier Subdivision
    // ------------------------------------------------------------------------
    MicaNtDirect2DGeometryEngine engine(0.25f);

    engine.BeginFigure({ 0.0f, 0.0f });
    engine.AddLine({ 100.0f, 0.0f });
    engine.AddCubicBezier({ 150.0f, 50.0f }, { 150.0f, 100.0f }, { 100.0f, 150.0f });
    engine.AddQuadraticBezier({ 50.0f, 200.0f }, { 0.0f, 150.0f });
    engine.EndFigure(true);

    const auto& pts = engine.GetFlattenedPoints();
    if (pts.size() < 8) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: Fill Geometry Realization & Tessellation
    // ------------------------------------------------------------------------
    prismx_composition::MicaNtCompositionColor fillColor{ 0.0f, 0.8f, 0.9f, 1.0f };
    auto fillMesh = engine.RealizeFill(fillColor, MicaNtFillMode::Alternate);
    if (fillMesh.TriangleCount < 6) return false;
    if (fillMesh.Indices.size() != fillMesh.TriangleCount * 3) return false;
    if (fillMesh.Bounds.Width() <= 0 || fillMesh.Bounds.Height() <= 0) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Stroke Geometry Realization (Offset Quad Strips)
    // ------------------------------------------------------------------------
    MicaNtStrokeStyleDesc strokeStyle{};
    strokeStyle.StrokeWidth = 4.0f;
    strokeStyle.Join = MicaNtLineJoin::Miter;
    strokeStyle.StartCap = MicaNtCapStyle::Flat;
    strokeStyle.EndCap = MicaNtCapStyle::Flat;

    prismx_composition::MicaNtCompositionColor strokeColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    auto strokeMesh = engine.RealizeStroke(strokeStyle, strokeColor);
    if (strokeMesh.TriangleCount < 10) return false;
    if (strokeMesh.Vertices.size() < 20) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: Multi-Stop Linear Gradient Brush Interpolation
    // ------------------------------------------------------------------------
    MicaNtLinearGradientBrushDesc grad{};
    grad.StartPoint = { 0.0f, 0.0f };
    grad.EndPoint = { 100.0f, 0.0f };
    grad.Stops = {
        { 0.0f, { 1.0f, 0.0f, 0.0f, 1.0f } }, // Red
        { 0.5f, { 0.0f, 1.0f, 0.0f, 1.0f } }, // Green
        { 1.0f, { 0.0f, 0.0f, 1.0f, 1.0f } }  // Blue
    };

    auto sample0 = engine.SampleLinearGradient(grad, 0.0f);
    if (sample0.r != 1.0f || sample0.g != 0.0f || sample0.b != 0.0f) return false;

    auto sampleMid = engine.SampleLinearGradient(grad, 0.25f);
    if (std::abs(sampleMid.r - 0.5f) >= 0.01f) return false;
    if (std::abs(sampleMid.g - 0.5f) >= 0.01f) return false;
    if (sampleMid.b != 0.0f) return false;

    auto sampleEnd = engine.SampleLinearGradient(grad, 1.0f);
    if (sampleEnd.r != 0.0f || sampleEnd.g != 0.0f || sampleEnd.b != 1.0f) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Telemetry Verification
    // ------------------------------------------------------------------------
    const auto& tel = engine.GetTelemetry();
    if (tel.TotalPathsRealized < 2u) return false;
    if (tel.TotalTrianglesTessellated < 16u) return false;
    if (tel.BezierSubdivisions < 1u) return false;
    if (tel.StrokesGenerated < 1u) return false;
    if (tel.AverageTessellationLatencyMicroseconds >= 420.0f) return false;
    if (!tel.DeCasteljauToleranceVerified) return false;
    if (!tel.SubPixelAntiAliasingVerified) return false;
    if (!tel.GradientInterpolationVerified) return false;

    return true;
}

} // namespace micant::um::prismx_geometry
