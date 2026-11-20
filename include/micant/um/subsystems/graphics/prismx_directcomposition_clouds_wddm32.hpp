// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 354 (Suite 570)
// DirectComposition Volumetric Cloud Deck, Atmospheric Nebula & Multi-Scattering
//   Celestial Raymarching Subsystem (IDCompositionVolumetricCloudEffect,
//   IDCompositionAtmosphericNebulaVisual, DirectCompositionCloudPipeline)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>
#include <algorithm>

namespace micant::um::prismx_clouds {

enum class KernelCloudModel : uint32_t {
    Cumulus         = 0, // Low-altitude fluffy cumulus cloud deck
    Stratocumulus   = 1, // Dense, low overcast layered blanket
    Cirrus          = 2, // High-altitude fibrous ice crystal filaments
    NebulaCelestial = 3  // Interstellar cosmic emission nebula & stellar gas
};

enum class KernelCloudQuality : uint32_t {
    Low    = 0, // 16 raymarch steps
    Medium = 1, // 32 raymarch steps
    High   = 2, // 64 raymarch steps
    Ultra  = 3  // 128 raymarch steps
};

enum class KernelCloudScatteringMode : uint32_t {
    SingleScattering = 0, // Single direct sun extinction
    DualLobeHG       = 1, // Forward silver-lining + backward Henyey-Greenstein
    MultiOctaveCone  = 2  // Multi-scattering octaves with cone forward transport
};

struct KernelCloudVector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    [[nodiscard]] constexpr float lengthSquared() const noexcept {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(lengthSquared());
    }

    [[nodiscard]] KernelCloudVector3 normalize() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f, 1.0f};
        return {x / len, y / len, z / len};
    }

    [[nodiscard]] constexpr float dot(const KernelCloudVector3& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }

    [[nodiscard]] constexpr KernelCloudVector3 cross(const KernelCloudVector3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] constexpr KernelCloudVector3 operator+(const KernelCloudVector3& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }

    [[nodiscard]] constexpr KernelCloudVector3 operator-(const KernelCloudVector3& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }

    [[nodiscard]] constexpr KernelCloudVector3 operator-() const noexcept {
        return {-x, -y, -z};
    }

    [[nodiscard]] constexpr KernelCloudVector3 operator*(float s) const noexcept {
        return {x * s, y * s, z * s};
    }

    [[nodiscard]] constexpr KernelCloudVector3 operator/(float s) const noexcept {
        float inv = (s != 0.0f) ? (1.0f / s) : 0.0f;
        return {x * inv, y * inv, z * inv};
    }
};

struct KernelCloudColorRGB {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};

    [[nodiscard]] constexpr KernelCloudColorRGB operator+(const KernelCloudColorRGB& o) const noexcept {
        return {r + o.r, g + o.g, b + o.b};
    }

    [[nodiscard]] constexpr KernelCloudColorRGB operator-(const KernelCloudColorRGB& o) const noexcept {
        return {
            std::max(0.0f, r - o.r),
            std::max(0.0f, g - o.g),
            std::max(0.0f, b - o.b)
        };
    }

    [[nodiscard]] constexpr KernelCloudColorRGB operator*(const KernelCloudColorRGB& o) const noexcept {
        return {r * o.r, g * o.g, b * o.b};
    }

    [[nodiscard]] constexpr KernelCloudColorRGB operator*(float s) const noexcept {
        return {r * s, g * s, b * s};
    }

    [[nodiscard]] constexpr KernelCloudColorRGB clamp(float minVal = 0.0f, float maxVal = 100.0f) const noexcept {
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

struct KernelVolumetricCloudParameters {
    KernelCloudModel          model{KernelCloudModel::Cumulus};
    KernelCloudQuality        quality{KernelCloudQuality::Medium};
    KernelCloudScatteringMode scatteringMode{KernelCloudScatteringMode::DualLobeHG};
    float                     cloudBottomAltitudeM{1500.0f};
    float                     cloudTopAltitudeM{4000.0f};
    float                     cloudCoverage{0.55f};
    float                     cloudDensityMultiplier{0.008f};
    KernelCloudVector3        sunDirection{0.4f, 0.2f, 0.9f};
    KernelCloudColorRGB       sunColor{1.0f, 0.95f, 0.88f};
    KernelCloudColorRGB       ambientSkyColor{0.35f, 0.55f, 0.85f};
    KernelCloudColorRGB       ambientGroundColor{0.15f, 0.12f, 0.10f};
    KernelCloudVector3        windVelocity{15.0f, 5.0f, 0.0f};
    float                     forwardScatteringG1{0.80f};
    float                     backwardScatteringG2{-0.20f};
    float                     silverLiningBlendWeight{0.70f};
    uint32_t                  multiScatteringOctaves{3};
    float                     extinctionCoeff{0.025f};
};

struct CloudSubsystemTelemetry {
    uint64_t totalRaymarchPasses{0};
    uint64_t totalRaymarchSteps{0};
    uint64_t lightRaySteps{0};
    uint64_t cloudHits{0};
    uint64_t multiScatteringEvaluations{0};
    double   avgCloudDispatchLatencyUs{0.0};
};

// Subsystem Control & Diagnostics API
bool InitializeCloudSubsystem(const KernelVolumetricCloudParameters& params = {}) noexcept;
void ShutdownCloudSubsystem() noexcept;
bool SetCloudParameters(const KernelVolumetricCloudParameters& params) noexcept;
bool GetCloudParameters(KernelVolumetricCloudParameters* outParams) noexcept;

float SampleCloudDensity(const KernelCloudVector3& posM, float timeSec) noexcept;
float EvaluateCloudPhaseFunction(float cosTheta) noexcept;

bool RaymarchCloudViewRay(
    const KernelCloudVector3& rayOriginM,
    const KernelCloudVector3& rayDir,
    float maxDistM,
    float timeSec,
    KernelCloudColorRGB* outRadiance,
    float* outTransmittance) noexcept;

bool ExecuteCloudStressPass(uint32_t iterations = 200) noexcept;
bool GetCloudTelemetry(CloudSubsystemTelemetry* outTelemetry) noexcept;
bool VerifyHorizon354CloudSubsystem() noexcept;

} // namespace micant::um::prismx_clouds
