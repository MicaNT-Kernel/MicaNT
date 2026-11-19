// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 349 (Suite 565)
// DirectComposition Subsurface Scattering (SSS), Translucency & Skin Absorption
//   Pipeline (IDCompositionSubsurfaceScatteringEffect, IDCompositionTranslucentVisual)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>

namespace micant::um::prismx_sss {

enum class KernelSSSProfile : uint32_t {
    Skin   = 0,
    Marble = 1,
    Jade   = 2,
    Wax    = 3
};

enum class KernelSSSQuality : uint32_t {
    Low    = 0,
    Medium = 1,
    High   = 2,
    Ultra  = 3
};

struct KernelVector3 {
    float x{0.0f};
    float y{0.0f};
    float z{1.0f};

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(x * x + y * y + z * z);
    }

    [[nodiscard]] KernelVector3 normalize() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f, 1.0f};
        return {x / len, y / len, z / len};
    }

    [[nodiscard]] float dot(const KernelVector3& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }
};

struct KernelColorRGB {
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};
};

struct KernelGaussianDipoleSample {
    float offsetMm{0.0f};
    float weight{1.0f};
    float variance{1.0f};
};

struct KernelSubsurfaceScatteringParams {
    float            scatterRadiusMm{14.0f};
    float            translucencyFactor{0.70f};
    KernelColorRGB   absorptionColor{0.85f, 0.35f, 0.25f};
    float            extinctionCoeff{0.15f};
    uint32_t         sampleCount{6};
    KernelSSSProfile profile{KernelSSSProfile::Skin};
    KernelSSSQuality quality{KernelSSSQuality::Medium};
};

struct SSSSubsystemTelemetry {
    uint64_t totalSssPasses{0};
    uint64_t diffusionDipoleEvaluations{0};
    uint64_t translucencyAbsorptionEvaluations{0};
    uint64_t screenSpaceBlurPasses{0};
    double   avgSssDispatchLatencyUs{0.0};
};

bool InitializeHorizon349DirectCompositionSubsurfaceScatteringSubsystem();
bool VerifyHorizon349DirectCompositionSubsurfaceScatteringSubsystem();

} // namespace micant::um::prismx_sss
