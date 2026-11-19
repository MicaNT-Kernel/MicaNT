// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 344 (Suite 560)
// DirectComposition Chromatic Aberration, Spectral Lens Dispersion &
//   Volumetric Holographic Bloom Pipeline (IDCompositionChromaticAberrationEffect)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>

namespace micant::um::prismx_chromatic_bloom {

enum class KernelBloomQuality : uint32_t {
    Low    = 0,
    Medium = 1,
    High   = 2,
    Ultra  = 3
};

struct KernelVector2 {
    float x{0.0f};
    float y{0.0f};

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(x * x + y * y);
    }

    [[nodiscard]] KernelVector2 normalize() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f};
        return {x / len, y / len};
    }
};

struct KernelChromaticParams {
    float         radialIntensity{0.015f};
    float         spectralDispersion{1.2f};
    KernelVector2 focalPoint{0.5f, 0.5f};
};

struct KernelBloomParams {
    float              luminanceThreshold{1.0f};
    float              bloomIntensity{0.8f};
    float              bloomRadius{4.0f};
    uint32_t           pyramidLevels{5};
    KernelBloomQuality quality{KernelBloomQuality::Ultra};
};

struct KernelPyramidLevel {
    uint32_t level{0};
    uint32_t width{0};
    uint32_t height{0};
    bool     allocated{false};
};

struct ChromaticBloomSubsystemTelemetry {
    uint64_t totalPostProcessPasses{0};
    uint64_t pyramidDownsamples{0};
    uint64_t pyramidUpsamples{0};
    uint64_t spectralDispersionCalculations{0};
    double   avgPostProcessDispatchLatencyUs{0.0};
};

bool InitializeHorizon344DirectCompositionChromaticBloomSubsystem();
bool VerifyHorizon344DirectCompositionChromaticBloomSubsystem();

} // namespace micant::um::prismx_chromatic_bloom
