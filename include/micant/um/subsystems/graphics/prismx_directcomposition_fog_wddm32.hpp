// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 350 (Suite 566)
// DirectComposition Dynamic Atmospheric Fog, Volumetric God Rays & Light Shaft
//   Scattering Pipeline (IDCompositionAtmosphericFogEffect, IDCompositionVolumetricVisual)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>

namespace micant::um::prismx_fog {

enum class KernelFogMode : uint32_t {
    Exponential        = 0,
    ExponentialSquared = 1,
    HeightLayered      = 2
};

enum class KernelScatteringModel : uint32_t {
    Rayleigh              = 0,
    MieHenyeyGreenstein   = 1,
    DualLobe              = 2
};

enum class KernelFogQuality : uint32_t {
    Low    = 0,
    Medium = 1,
    High   = 2,
    Ultra  = 3
};

struct KernelVector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

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

    [[nodiscard]] KernelVector3 operator+(const KernelVector3& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }

    [[nodiscard]] KernelVector3 operator-(const KernelVector3& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }

    [[nodiscard]] KernelVector3 operator*(float s) const noexcept {
        return {x * s, y * s, z * s};
    }
};

struct KernelColorRGB {
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};

    [[nodiscard]] KernelColorRGB operator+(const KernelColorRGB& o) const noexcept {
        return {r + o.r, g + o.g, b + o.b};
    }

    [[nodiscard]] KernelColorRGB operator*(float s) const noexcept {
        return {r * s, g * s, b * s};
    }

    [[nodiscard]] KernelColorRGB operator*(const KernelColorRGB& o) const noexcept {
        return {r * o.r, g * o.g, b * o.b};
    }
};

struct KernelBoundingBox {
    KernelVector3 minPt{-100.0f, -100.0f, -100.0f};
    KernelVector3 maxPt{100.0f, 100.0f, 100.0f};

    [[nodiscard]] bool contains(const KernelVector3& pt) const noexcept {
        return pt.x >= minPt.x && pt.x <= maxPt.x &&
               pt.y >= minPt.y && pt.y <= maxPt.y &&
               pt.z >= minPt.z && pt.z <= maxPt.z;
    }
};

struct KernelLightSource {
    KernelVector3  direction{0.0f, -0.7071f, 0.7071f};
    KernelColorRGB color{1.0f, 0.95f, 0.85f};
    float          intensity{1.5f};
};

struct KernelFogParameters {
    float                 fogDensity{0.025f};
    float                 heightFalloff{0.05f};
    float                 groundLevelMm{0.0f};
    KernelColorRGB        inscatteringColor{0.70f, 0.80f, 0.95f};
    float                 mieGParameter{0.76f};
    float                 maxDistanceMm{2000.0f};
    float                 lightShaftIntensity{1.25f};
    uint32_t              stepCount{32};
    KernelFogMode         mode{KernelFogMode::HeightLayered};
    KernelScatteringModel scatteringModel{KernelScatteringModel::MieHenyeyGreenstein};
    KernelFogQuality      quality{KernelFogQuality::Medium};
};

struct FogSubsystemTelemetry {
    uint64_t totalFogPasses{0};
    uint64_t epipolarRaySteps{0};
    uint64_t phaseFunctionEvaluations{0};
    uint64_t transmittanceEvaluations{0};
    uint64_t lightShaftPasses{0};
    double   avgFogDispatchLatencyUs{0.0};
};

bool InitializeHorizon350DirectCompositionFogSubsystem();
bool VerifyHorizon350DirectCompositionFogSubsystem();

} // namespace micant::um::prismx_fog
