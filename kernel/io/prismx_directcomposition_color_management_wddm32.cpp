/**
 * @file prismx_directcomposition_color_management_wddm32.cpp
 * @brief Horizon 330 (Kernel Unit #106): Sovereign MicaNT DirectComposition Hardware Color Management,
 *        ICC Profile 3D LUT Color Transformation & Display Color Volume Mapping Pipeline
 *        - Per-display ICC Profile and 3D LUT (Look-Up Table) trilinear hardware interpolation
 *        - Color gamut transformation matrices (sRGB, Display P3, BT.2020) via analytical matrices
 *        - Dynamic tone mapping operators (Reinhard, ACES Filmic, Peak White Compression)
 *        - Hardware 1D De-gamma / Re-gamma transfer curve LUT pre-linearization and post-re-quantization
 *        - Real-time color composition telemetry & sub-100us latency
 *        - Pure ISO C++23. Zero External Dependencies.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directcomposition_color_management_wddm32.hpp"

namespace micant::um::prismx_colormgmt {

bool VerifyHorizon330DirectCompositionColorManagementSubsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: 1D De-gamma & Re-gamma Exact Round-Trip Verification
    // ------------------------------------------------------------------------
    float midSrgb = 0.5f;
    float midLinear = MicaNtColorTransfer::SrgbToLinear(midSrgb);
    float roundTrip = MicaNtColorTransfer::LinearToSrgb(midLinear);
    if (std::abs(roundTrip - midSrgb) > 0.001f) return false;

    if (std::abs(MicaNtColorTransfer::SrgbToLinear(0.0f)) > 0.0001f) return false;
    if (std::abs(MicaNtColorTransfer::SrgbToLinear(1.0f) - 1.0f) > 0.0001f) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: 3x3 Gamut Matrix Transformation (sRGB to BT.2020)
    // ------------------------------------------------------------------------
    MicaNtMatrix3x3 srgbToBt2020 = MicaNtMatrix3x3::SrgbToBt2020();
    MicaNtColorRgb pureRedSrgb{ 1.0f, 0.0f, 0.0f };
    MicaNtColorRgb bt2020Red = srgbToBt2020.Transform(pureRedSrgb);
    if (std::abs(bt2020Red.r - 0.627404f) > 0.005f) return false;
    if (std::abs(bt2020Red.g - 0.069097f) > 0.005f) return false;
    if (std::abs(bt2020Red.b - 0.016391f) > 0.005f) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Dynamic Tone Mapping Operators (Reinhard & ACES Filmic)
    // ------------------------------------------------------------------------
    MicaNtColorRgb hdrPeak{ 4.0f, 2.0f, 1.0f };
    MicaNtColorRgb reinhard = MicaNtToneMapper::Apply(hdrPeak, MicaNtToneMapOperator::Reinhard);
    if (std::abs(reinhard.r - 0.8f) > 0.01f) return false;
    if (reinhard.r >= 1.0f || reinhard.g >= 1.0f || reinhard.b >= 1.0f) return false;

    MicaNtColorRgb aces = MicaNtToneMapper::Apply(hdrPeak, MicaNtToneMapOperator::AcesFilmic);
    if (aces.r > 1.0f || aces.r < 0.9f) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: 3D LUT Hardware Trilinear Interpolation
    // ------------------------------------------------------------------------
    MicaNtLut3D lut(17);
    MicaNtColorRgb midSample = lut.Sample(MicaNtColorRgb{ 0.5f, 0.5f, 0.5f });
    if (std::abs(midSample.r - 0.5f) > 0.01f) return false;
    if (std::abs(midSample.g - 0.5f) > 0.01f) return false;
    if (std::abs(midSample.b - 0.5f) > 0.01f) return false;

    lut.SetNode(8, 8, 8, MicaNtColorRgb{ 0.55f, 0.50f, 0.45f });
    MicaNtColorRgb warmSample = lut.Sample(MicaNtColorRgb{ 0.5f, 0.5f, 0.5f });
    if (std::abs(warmSample.r - 0.55f) > 0.01f) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Integrated Color Management Engine & Telemetry
    // ------------------------------------------------------------------------
    MicaNtColorManagementEngine engine(17);
    engine.SetGamutMatrix(MicaNtMatrix3x3::SrgbToBt2020());
    engine.SetToneMapper(MicaNtToneMapOperator::AcesFilmic);

    MicaNtColorRgb inColor{ 0.8f, 0.4f, 0.2f };
    MicaNtColorRgb outColor = engine.ProcessPixel(inColor);
    if (outColor.r <= 0.0f || outColor.r > 1.0f) return false;

    const auto& tel = engine.GetTelemetry();
    if (tel.PixelsTransformed != 1) return false;
    if (tel.GamutConversions != 1) return false;
    if (tel.ToneMapConversions != 1) return false;
    if (tel.Lut3DLookups != 1) return false;
    if (tel.AverageColorDispatchLatencyMicroseconds > 100.0f) return false;

    return true;
}

} // namespace micant::um::prismx_colormgmt
