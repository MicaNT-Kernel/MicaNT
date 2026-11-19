// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 348 (Suite 564)
// DirectComposition Screen-Space Reflections (SSR), Cone-Tracing Glossy Surfaces
//   & Roughness Falloff Pipeline (IDCompositionReflectionEffect, IDCompositionGlossyVisual)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>

namespace micant::um::prismx_ssr {

enum class KernelSSRQuality : uint32_t {
    Low    = 0,
    Medium = 1,
    High   = 2,
    Ultra  = 3
};

enum class KernelSSRRoughnessModel : uint32_t {
    GGX      = 0,
    Beckmann = 1,
    Blinn    = 2
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

    [[nodiscard]] KernelVector3 reflect(const KernelVector3& normal) const noexcept {
        float d = this->dot(normal);
        return {
            x - 2.0f * d * normal.x,
            y - 2.0f * d * normal.y,
            z - 2.0f * d * normal.z
        };
    }
};

struct KernelReflectionParams {
    float                   maxRayDistanceMm{1000.0f};
    float                   roughness{0.15f};
    float                   fresnelF0{0.04f};
    uint32_t                stepCount{32};
    float                   coneTracingExpansion{0.15f};
    KernelSSRQuality        quality{KernelSSRQuality::Medium};
    KernelSSRRoughnessModel roughnessModel{KernelSSRRoughnessModel::GGX};
};

struct SSRSubsystemTelemetry {
    uint64_t totalSsrPasses{0};
    uint64_t raymarchStepEvaluations{0};
    uint64_t coneTracingFootprintEvaluations{0};
    uint64_t fresnelCalculations{0};
    double   avgSsrDispatchLatencyUs{0.0};
};

bool InitializeHorizon348DirectCompositionReflectionSubsystem();
bool VerifyHorizon348DirectCompositionReflectionSubsystem();

} // namespace micant::um::prismx_ssr
