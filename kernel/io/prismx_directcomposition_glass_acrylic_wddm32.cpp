/**
 * @file prismx_directcomposition_glass_acrylic_wddm32.cpp
 * @brief Horizon 325 (Kernel Unit #101): Sovereign MicaNT DirectComposition Glass / Acrylic
 *        Backdrop Brush Convolution Filters, Tint Opacity & Hardware Blur Acceleration Pipeline
 *        - Multi-layer effect graph composition (IDCompositionEffectGroup, IDCompositionGaussianBlurEffect)
 *        - Separable Gaussian convolution filter kernels (9-tap, 13-tap, 17-tap) with dual-pass downsampling
 *        - Perceptual backdrop tint luminosity blending and procedural noise/grain dithering on HDR swapchains
 *        - Dirty rect invalidation and sub-120us effect graph dispatch latency
 *        - Pure ISO C++23. Zero External Dependencies.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directcomposition_glass_acrylic_wddm32.hpp"

namespace micant::um::prismx_glass {

bool VerifyHorizon325DirectCompositionGlassAcrylicSubsystem() noexcept {
    MicaNtDirectCompositionGlassAcrylicEngine engine;

    // ------------------------------------------------------------------------
    // Pillar 1: Analytical Gaussian Kernel Generation & Normalization
    // ------------------------------------------------------------------------
    auto k9 = MicaNtDirectCompositionGlassAcrylicEngine::ComputeGaussianKernel(MicaNtBlurTapMode::Tap9, 2.5f);
    if (k9.weights.size() != 9) return false;
    if (k9.radius != 4) return false;
    float sum9 = 0.0f;
    for (float w : k9.weights) sum9 += w;
    if (std::abs(sum9 - 1.0f) > 1e-5f) return false;

    auto k13 = MicaNtDirectCompositionGlassAcrylicEngine::ComputeGaussianKernel(MicaNtBlurTapMode::Tap13, 3.8f);
    if (k13.weights.size() != 13) return false;
    float sum13 = 0.0f;
    for (float w : k13.weights) sum13 += w;
    if (std::abs(sum13 - 1.0f) > 1e-5f) return false;

    auto k17 = MicaNtDirectCompositionGlassAcrylicEngine::ComputeGaussianKernel(MicaNtBlurTapMode::Tap17, 5.0f);
    if (k17.weights.size() != 17) return false;
    float sum17 = 0.0f;
    for (float w : k17.weights) sum17 += w;
    if (std::abs(sum17 - 1.0f) > 1e-5f) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: Separable 2-Pass Convolution Execution
    // ------------------------------------------------------------------------
    MicaNtBackdropSurface input;
    input.Resize(64, 64, { 0.0f, 0.0f, 0.0f, 1.0f });
    input.Set(32, 32, { 1.0f, 1.0f, 1.0f, 1.0f });

    MicaNtBackdropSurface blurred;
    engine.ApplySeparableBlur(input, blurred, k9);

    const auto& centerPix = blurred.Get(32, 32);
    const auto& neighborPix = blurred.Get(33, 32);
    if (centerPix.r <= 0.0f || centerPix.r >= 1.0f) return false;
    if (neighborPix.r <= 0.0f || neighborPix.r >= centerPix.r) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Perceptual Tint & Luminosity Blending
    // ------------------------------------------------------------------------
    MicaNtBackdropSurface tintTest;
    tintTest.Resize(16, 16, { 0.5f, 0.5f, 0.5f, 1.0f });
    MicaNtColorRgba darkTint{ 0.1f, 0.1f, 0.1f, 0.8f };
    engine.ApplyTintAndLuminosity(tintTest, darkTint, 0.9f);

    const auto& tintedPix = tintTest.Get(8, 8);
    if (tintedPix.r >= 0.5f || tintedPix.g >= 0.5f || tintedPix.b >= 0.5f) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: Procedural Noise Grain Dithering
    // ------------------------------------------------------------------------
    MicaNtBackdropSurface grainTest;
    grainTest.Resize(16, 16, { 0.5f, 0.5f, 0.5f, 1.0f });
    engine.ApplyNoiseGrain(grainTest, 0.03f, 0x12345678);

    const auto& pA = grainTest.Get(4, 4);
    const auto& pB = grainTest.Get(4, 5);
    if (std::abs(pA.r - pB.r) < 1e-6f) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Full Acrylic Recipe Execution, Cache & Telemetry Verification
    // ------------------------------------------------------------------------
    MicaNtAcrylicRecipe recipe{
        .blurRadius = 25.0f,
        .tapMode = MicaNtBlurTapMode::Tap13,
        .tintColor = { 0.15f, 0.15f, 0.18f, 0.75f },
        .tintOpacity = 0.75f,
        .luminosityFactor = 0.88f,
        .noiseGrainAmount = 0.015f
    };

    MicaNtBackdropSurface finalAcrylic;
    engine.RenderAcrylicBackdrop(input, recipe, finalAcrylic);

    if (!engine.IsCacheValid()) return false;
    engine.InvalidateCache();
    if (engine.IsCacheValid()) return false;

    const auto& tel = engine.GetTelemetry();
    if (tel.BackdropEffectsCreated < 1u) return false;
    if (tel.SeparablePassesProcessed < 4u) return false;
    if (tel.BlurKernelsComputed < 1u) return false;
    if (tel.GrainDitherSamplesGenerated < 256u) return false;
    if (tel.AverageEffectDispatchMicroseconds >= 120.0f) return false;

    return true;
}

} // namespace micant::um::prismx_glass
