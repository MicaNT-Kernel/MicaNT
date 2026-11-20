// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 352 (Suite 568)
// DirectComposition Dynamic Screen-Space Volumetric Caustics, Water Wave Refraction &
//   Aquatic Light Scattering Subsystem (IDCompositionCausticsEffect,
//   IDCompositionRefractiveVisual, DirectCompositionCausticsPipeline)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>
#include <algorithm>

namespace micant::um::prismx_caustics {

enum class KernelCausticsQuality : uint32_t {
    Low     = 0, // 8 rays
    Medium  = 1, // 16 rays
    High    = 2, // 32 rays
    Ultra   = 3  // 64 rays
};

enum class KernelJerlovWaterType : uint32_t {
    TypeI    = 0, // Extremely clear open ocean (deep blue/cyan)
    TypeIA   = 1, // Clear oceanic
    TypeIB   = 2, // Moderately clear
    TypeII   = 3, // Coastal oceanic
    TypeIII  = 4, // Turbid coastal waters
    Coastal1 = 5  // Green coastal / estuarine
};

enum class KernelWaveDispersionMode : uint32_t {
    Monochromatic    = 0, // Single optical index (n = 1.333)
    ChromaticCaustic = 1  // Spectral dispersion (R, G, B refractive split)
};

struct KernelCausticsVector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    [[nodiscard]] constexpr float lengthSquared() const noexcept {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(lengthSquared());
    }

    [[nodiscard]] KernelCausticsVector3 normalize() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f, 1.0f};
        return {x / len, y / len, z / len};
    }

    [[nodiscard]] constexpr float dot(const KernelCausticsVector3& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }

    [[nodiscard]] constexpr KernelCausticsVector3 cross(const KernelCausticsVector3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] constexpr KernelCausticsVector3 operator+(const KernelCausticsVector3& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }

    [[nodiscard]] constexpr KernelCausticsVector3 operator-(const KernelCausticsVector3& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }

    [[nodiscard]] constexpr KernelCausticsVector3 operator-() const noexcept {
        return {-x, -y, -z};
    }

    [[nodiscard]] constexpr KernelCausticsVector3 operator*(float s) const noexcept {
        return {x * s, y * s, z * s};
    }

    [[nodiscard]] constexpr KernelCausticsVector3 operator/(float s) const noexcept {
        float inv = (s != 0.0f) ? (1.0f / s) : 0.0f;
        return {x * inv, y * inv, z * inv};
    }
};

struct KernelCausticsColorRGB {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};

    [[nodiscard]] constexpr KernelCausticsColorRGB operator+(const KernelCausticsColorRGB& o) const noexcept {
        return {r + o.r, g + o.g, b + o.b};
    }

    [[nodiscard]] constexpr KernelCausticsColorRGB operator-(const KernelCausticsColorRGB& o) const noexcept {
        return {
            std::max(0.0f, r - o.r),
            std::max(0.0f, g - o.g),
            std::max(0.0f, b - o.b)
        };
    }

    [[nodiscard]] constexpr KernelCausticsColorRGB operator*(const KernelCausticsColorRGB& o) const noexcept {
        return {r * o.r, g * o.g, b * o.b};
    }

    [[nodiscard]] constexpr KernelCausticsColorRGB operator*(float s) const noexcept {
        return {r * s, g * s, b * s};
    }

    [[nodiscard]] constexpr KernelCausticsColorRGB clamp(float minVal = 0.0f, float maxVal = 100.0f) const noexcept {
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

struct KernelGerstnerWave {
    KernelCausticsVector3 direction{1.0f, 0.0f, 0.0f}; // Unit direction vector (d_x, d_y, 0)
    float                 amplitudeMm{8.0f};           // Peak wave height A
    float                 wavelengthMm{250.0f};        // Spatial wave cycle length lambda
    float                 speedMmPerSec{200.0f};       // Propagation velocity c
    float                 steepness{0.65f};            // Trochoidal sharpness Q in [0, 1]
    float                 phaseOffset{0.0f};           // Initial phi
};

struct KernelCausticsParameters {
    KernelCausticsQuality    quality{KernelCausticsQuality::Medium};
    KernelJerlovWaterType    waterType{KernelJerlovWaterType::TypeI};
    KernelWaveDispersionMode dispersionMode{KernelWaveDispersionMode::ChromaticCaustic};
    float                    waterIndexRefraction{1.333f};
    float                    submergedBedDepthMm{1200.0f};
    float                    causticsIntensity{1.5f};
    float                    chromaticDispersionStrength{0.012f};
    float                    volumetricDensity{0.002f};
};

struct CausticsSubsystemTelemetry {
    uint64_t totalCausticsPasses{0};
    uint64_t causticPhotonsTraced{0};
    uint64_t totalInternalReflections{0};
    uint64_t chromaticDispersionsCalculated{0};
    uint64_t refractiveBouncesEvaluated{0};
    double   avgCausticsDispatchLatencyUs{0.0};
};

// Subsystem Control & Diagnostics API
bool InitializeCausticsSubsystem(const KernelCausticsParameters& params = {}) noexcept;
void ShutdownCausticsSubsystem() noexcept;
bool SetCausticsParameters(const KernelCausticsParameters& params) noexcept;
bool GetCausticsParameters(KernelCausticsParameters* outParams) noexcept;

bool AddGerstnerWave(const KernelGerstnerWave& wave) noexcept;
void ClearGerstnerWaves() noexcept;
size_t GetGerstnerWaveCount() noexcept;

void EvaluateSurfaceElevationAndNormal(
    float x, float y, float timeSec,
    float* outElevation,
    KernelCausticsVector3* outNormal) noexcept;

bool RefractRay(
    const KernelCausticsVector3& incident,
    const KernelCausticsVector3& normal,
    float nRatio,
    KernelCausticsVector3* outRefracted,
    bool* outTir) noexcept;

KernelCausticsColorRGB EvaluateWaterTransmittance(float depthMm, KernelJerlovWaterType type) noexcept;

KernelCausticsColorRGB EvaluateSubmergedBedCausticIrradiance(
    float bedX, float bedY, float timeSec) noexcept;

bool ExecuteCausticsStressPass(uint32_t iterations = 200) noexcept;
bool GetCausticsTelemetry(CausticsSubsystemTelemetry* outTelemetry) noexcept;
bool VerifyHorizon352CausticsSubsystem() noexcept;

} // namespace micant::um::prismx_caustics
