// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 347 (Suite 563)
// DirectComposition Screen-Space Ambient Occlusion (SSAO), Ground-Truth
//   Horizon-Based AO (HBAO+) & Contact Crevice Shadowing Subsystem
//   (IDCompositionAmbientOcclusionEffect, IDCompositionAmbientVisual)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>

namespace micant::um::prismx_ssao_hbao {

enum class KernelAOMode : uint32_t {
    SSAO      = 0,
    HBAO_PLUS = 1,
    GTAO      = 2
};

enum class KernelAOQuality : uint32_t {
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

    [[nodiscard]] KernelVector3 cross(const KernelVector3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }
};

struct KernelAmbientOcclusionParams {
    float           radiusMm{50.0f};
    float           biasAngleDeg{15.0f};
    float           powerExponent{1.5f};
    uint32_t        numDirections{8};
    uint32_t        numSteps{4};
    float           blurRadiusPx{4.0f};
    KernelAOMode    mode{KernelAOMode::HBAO_PLUS};
    KernelAOQuality quality{KernelAOQuality::Medium};
};

struct AmbientOcclusionSubsystemTelemetry {
    uint64_t totalAoPasses{0};
    uint64_t horizonAngleEvaluations{0};
    uint64_t creviceShadowEvaluations{0};
    uint64_t bilateralFilterPasses{0};
    double   avgAoDispatchLatencyUs{0.0};
};

bool InitializeHorizon347DirectCompositionAmbientOcclusionSubsystem();
bool VerifyHorizon347DirectCompositionAmbientOcclusionSubsystem();

} // namespace micant::um::prismx_ssao_hbao
