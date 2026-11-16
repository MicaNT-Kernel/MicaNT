/**
 * @file prismx_mica_acrylic_composition_wddm32.cpp
 * @brief Horizon 316 (Kernel Unit #92): Sovereign MicaNT DirectComposition Visual Tree Node Graph
 *        & Hardware Mica / Acrylic Shader Synthesis Engine with Independent Flip Promotion
 *        - DirectComposition Visual Tree Node Graph & Multi-Window Z-Order Governor
 *        - Hardware-Accelerated Mica (Luminosity & Wallpaper Texture Synthesis) Engine
 *        - Hardware-Accelerated Acrylic (Dual-Pass Gaussian Blur & Exclusion Blend) Engine
 *        - DirectComposition Independent Flip Promotion for Borderless Fullscreen Applications
 *        - Sub-Millisecond Hit-Testing, Dynamic Clipping & Zero-DWM Buffer Latency Architecture.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_mica_acrylic_composition_wddm32.hpp"

namespace micant::um::prismx_composition {

bool VerifyHorizon316MicaAcrylicCompositionSubsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: DirectComposition Visual Tree Node Graph & Multi-Window Z-Order
    // ------------------------------------------------------------------------
    MicaNtMicaAcrylicEngine engine(3840, 2160);

    void* hwnd1 = reinterpret_cast<void*>(0x10001);
    void* hwnd2 = reinterpret_cast<void*>(0x10002);

    MicaNtWindowVisualDesc win1{};
    win1.Hwnd = hwnd1;
    win1.Bounds = { 100, 100, 900, 700 };
    win1.Material = MicaNtCompositionMaterial::MicaDark;
    win1.TintColor = { 0.12f, 0.12f, 0.14f, 0.85f };
    win1.ZOrder = 10;
    engine.AddWindow(win1);

    MicaNtWindowVisualDesc win2{};
    win2.Hwnd = hwnd2;
    win2.Bounds = { 400, 300, 1200, 900 };
    win2.Material = MicaNtCompositionMaterial::AcrylicThin;
    win2.TintColor = { 0.2f, 0.2f, 0.25f, 0.6f };
    win2.ZOrder = 20;
    engine.AddWindow(win2);

    if (engine.GetWindows().size() != 2) return false;
    if (engine.GetWindows()[0].ZOrder > engine.GetWindows()[1].ZOrder) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: Sub-Millisecond Hit-Testing & Dynamic Z-Order Elevation
    // ------------------------------------------------------------------------
    // Point (500, 400) is inside both win1 and win2; topmost (win2) must receive input
    void* hit = engine.HitTest(500, 400);
    if (hit != hwnd2) return false;

    // Point (200, 200) is only inside win1
    hit = engine.HitTest(200, 200);
    if (hit != hwnd1) return false;

    // Point (2000, 2000) is outside both windows
    hit = engine.HitTest(2000, 2000);
    if (hit != nullptr) return false;

    // Elevate win1 to front; now point (500, 400) hits win1
    engine.BringToFront(hwnd1);
    hit = engine.HitTest(500, 400);
    if (hit != hwnd1) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Hardware-Accelerated Mica & Acrylic Shader Synthesis
    // ------------------------------------------------------------------------
    MicaNtCompositionColor desktopBg{ 0.10f, 0.20f, 0.30f, 1.0f };
    MicaNtCompositionColor micaTint{ 0.15f, 0.15f, 0.18f, 0.80f };
    auto micaSample = engine.SynthesizeMica(desktopBg, micaTint);
    if (micaSample.a != 1.0f) return false;
    if (micaSample.r <= 0.0f || micaSample.g <= 0.0f || micaSample.b <= 0.0f) return false;

    MicaNtCompositionColor acrylicBackdrop{ 0.40f, 0.50f, 0.60f, 1.0f };
    MicaNtCompositionColor acrylicTint{ 0.20f, 0.20f, 0.25f, 0.60f };
    auto acrylicSample = engine.SynthesizeAcrylic(acrylicBackdrop, acrylicTint);
    if (acrylicSample.a != 0.60f) return false;
    if (acrylicSample.r <= 0.0f || acrylicSample.g <= 0.0f || acrylicSample.b <= 0.0f) return false;

    // Verify Gaussian kernel normalization
    float kernelSum = 0.0f;
    for (float w : engine.GetGaussianKernel()) kernelSum += w;
    if (std::abs(kernelSum - 1.0f) >= 0.001f) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: DirectComposition Independent Flip Promotion
    // ------------------------------------------------------------------------
    // Resize Window 1 to full 4K display bounds (3840x2160)
    bool updated = engine.UpdateWindowBounds(hwnd1, { 0, 0, 3840, 2160 });
    if (!updated) return false;
    if (engine.GetTelemetry().IndependentFlipPromotions != 1u) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Telemetry Validation
    // ------------------------------------------------------------------------
    const auto& tel = engine.GetTelemetry();
    if (tel.TotalWindowsManaged != 2u) return false;
    if (tel.MicaSurfacesRendered < 1u) return false;
    if (tel.AcrylicSurfacesRendered < 1u) return false;
    if (tel.IndependentFlipPromotions != 1u) return false;
    if (tel.HitTestsPerformed < 4u) return false;
    if (tel.AverageCompositorLatencyMicroseconds >= 650.0f) return false;
    if (!tel.ZeroDwmQueueLatencyVerified) return false;
    if (!tel.HardwareBackdropShadersVerified) return false;

    return true;
}

} // namespace micant::um::prismx_composition
