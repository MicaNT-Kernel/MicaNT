// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 351 (Suite 567)
// DirectComposition Dynamic Screen-Space Global Illumination (SSGI), Radiosity &
//   Multi-Bounce Indirect Diffuse Pipeline (IDCompositionGlobalIlluminationEffect,
//   IDCompositionIndirectVisual)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>
#include <algorithm>

namespace micant::um::prismx_ssgi {

enum class KernelSsgiQuality : uint32_t {
    Low    = 0,
    Medium = 1,
    High   = 2,
    Ultra  = 3
};

enum class KernelBounceCount : uint32_t {
    SingleBounce = 1,
    DualBounce   = 2,
    TripleBounce = 3
};

enum class KernelDenoiserPasses : uint32_t {
    Passthrough       = 0,
    TwoPassBilateral  = 2,
    ThreePassAtrous   = 3,
    FivePassAtrous    = 5
};

struct KernelSsgiVector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    [[nodiscard]] constexpr float lengthSquared() const noexcept {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(lengthSquared());
    }

    [[nodiscard]] KernelSsgiVector3 normalize() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f, 1.0f};
        return {x / len, y / len, z / len};
    }

    [[nodiscard]] constexpr float dot(const KernelSsgiVector3& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }

    [[nodiscard]] constexpr KernelSsgiVector3 cross(const KernelSsgiVector3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] constexpr KernelSsgiVector3 operator+(const KernelSsgiVector3& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }

    [[nodiscard]] constexpr KernelSsgiVector3 operator-(const KernelSsgiVector3& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }

    [[nodiscard]] constexpr KernelSsgiVector3 operator-() const noexcept {
        return {-x, -y, -z};
    }

    [[nodiscard]] constexpr KernelSsgiVector3 operator*(float s) const noexcept {
        return {x * s, y * s, z * s};
    }

    [[nodiscard]] constexpr KernelSsgiVector3 operator/(float s) const noexcept {
        float inv = (s != 0.0f) ? (1.0f / s) : 0.0f;
        return {x * inv, y * inv, z * inv};
    }
};

struct KernelSsgiColorRGB {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};

    [[nodiscard]] constexpr KernelSsgiColorRGB operator+(const KernelSsgiColorRGB& o) const noexcept {
        return {r + o.r, g + o.g, b + o.b};
    }

    [[nodiscard]] constexpr KernelSsgiColorRGB operator-(const KernelSsgiColorRGB& o) const noexcept {
        return {
            std::max(0.0f, r - o.r),
            std::max(0.0f, g - o.g),
            std::max(0.0f, b - o.b)
        };
    }

    [[nodiscard]] constexpr KernelSsgiColorRGB operator*(const KernelSsgiColorRGB& o) const noexcept {
        return {r * o.r, g * o.g, b * o.b};
    }

    [[nodiscard]] constexpr KernelSsgiColorRGB operator*(float s) const noexcept {
        return {r * s, g * s, b * s};
    }

    [[nodiscard]] constexpr KernelSsgiColorRGB clamp(float minVal = 0.0f, float maxVal = 100.0f) const noexcept {
        return {
            std::clamp(r, minVal, maxVal),
            std::clamp(g, minVal, maxVal),
            std::clamp(b, minVal, maxVal)
        };
    }

    [[nodiscard]] constexpr float luminance() const noexcept {
        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }
};

struct KernelSsgiParameters {
    KernelSsgiQuality quality{KernelSsgiQuality::Medium};
    KernelBounceCount bounceCount{KernelBounceCount::DualBounce};
    KernelDenoiserPasses denoiserPasses{KernelDenoiserPasses::ThreePassAtrous};
    uint32_t rayCount{16};
    uint32_t maxRaySteps{48};
    float maxRayDistanceMm{2500.0f};
    float indirectIntensity{1.0f};
    float normalBiasMm{1.5f};
    float temporalBlendFactor{0.85f};
    float multiBounceAlbedoScale{0.75f};
    float spatialFilterRadiusMm{16.0f};
};

struct SsgiSubsystemTelemetry {
    uint64_t totalSsgiPasses{0};
    uint64_t totalIndirectRaysCast{0};
    uint64_t indirectHitsEvaluated{0};
    uint64_t multiBounceEvaluations{0};
    uint64_t temporalReprojections{0};
    uint64_t denoiserFilterPasses{0};
    double avgSsgiDispatchLatencyUs{0.0};
};

// Subsystem Control & Diagnostics API
bool InitializeSsgiSubsystem(const KernelSsgiParameters& params = {}) noexcept;
void ShutdownSsgiSubsystem() noexcept;
bool SetSsgiParameters(const KernelSsgiParameters& params) noexcept;
bool GetSsgiParameters(KernelSsgiParameters* outParams) noexcept;
bool AddSsgiVisual(
    const KernelSsgiVector3& pos,
    const KernelSsgiVector3& normal,
    float widthMm,
    float heightMm,
    const KernelSsgiColorRGB& albedo,
    const KernelSsgiColorRGB& emissive = {0.0f, 0.0f, 0.0f},
    float roughness = 0.5f) noexcept;
void ClearSsgiVisuals() noexcept;
KernelSsgiColorRGB EvaluateSsgiSurfaceRadiance(
    const KernelSsgiVector3& surfacePos,
    const KernelSsgiVector3& surfaceNormal,
    const KernelSsgiColorRGB& surfaceAlbedo,
    uint64_t frameSeed = 0) noexcept;
KernelSsgiColorRGB DenoiseSsgiSurfaceRadiance(
    const KernelSsgiColorRGB& center,
    const KernelSsgiVector3& centerNormal,
    float centerDepthMm,
    const std::vector<KernelSsgiColorRGB>& neighbors,
    const std::vector<KernelSsgiVector3>& neighborNormals,
    const std::vector<float>& neighborDepths) noexcept;
bool ExecuteSsgiStressPass(uint32_t iterations = 200) noexcept;
bool GetSsgiTelemetry(SsgiSubsystemTelemetry* outTelemetry) noexcept;
bool VerifyHorizon351SsgiSubsystem() noexcept;

} // namespace micant::um::prismx_ssgi
