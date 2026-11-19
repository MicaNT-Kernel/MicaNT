// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 342 (Suite 558)
// DirectComposition Dynamic Holographic Lighting, Normal-Mapped Specular
//   Highlights & Directional Light Visual Pipeline (IDCompositionLightVisual)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>

namespace micant::um::prismx_holographic_lighting {

enum class KernelLightType : uint32_t {
    Ambient = 0,
    Distant = 1,
    Point   = 2,
    Spot    = 3
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

    [[nodiscard]] static float dot(const KernelVector3& a, const KernelVector3& b) noexcept {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
};

struct KernelLightProperties {
    KernelLightType type{KernelLightType::Spot};
    uint32_t        argbColor{0xFFFFFFFF}; // White light
    float           intensity{1.2f};
    KernelVector3   position{960.0f, 540.0f, 50.0f};
    KernelVector3   direction{0.0f, 0.0f, -1.0f};
    float           innerConeAngleDeg{15.0f};
    float           outerConeAngleDeg{35.0f};
    float           attConstant{1.0f};
    float           attLinear{0.005f};
    float           attQuadratic{0.0001f};
};

struct KernelMaterialProperties {
    float ambientCoeff{0.15f};
    float diffuseCoeff{0.65f};
    float specularCoeff{0.45f};
    float specularShininess{32.0f};
};

struct KernelShadedPixel {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float diffuseIntensity{0.0f};
    float specularIntensity{0.0f};
};

struct LightingSubsystemTelemetry {
    uint64_t totalLightingPasses{0};
    uint64_t pointerLightUpdates{0};
    uint64_t activeLightSources{0};
    double   avgLightingDispatchLatencyUs{0.0};
};

bool InitializeHorizon342DirectCompositionHolographicLightingSubsystem();
bool VerifyHorizon342DirectCompositionHolographicLightingSubsystem();

} // namespace micant::um::prismx_holographic_lighting
