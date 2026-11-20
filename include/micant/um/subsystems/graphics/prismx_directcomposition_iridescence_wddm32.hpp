// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 355 (Suite 571)
// DirectComposition Real-Time Thin-Film Optical Interference, Iridescence &
//   Structural Coloration Subsystem (IDCompositionThinFilmInterferenceEffect,
//   IDCompositionIridescentVisual, DirectCompositionIridescencePipeline)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>
#include <algorithm>

namespace micant::um::prismx_iridescence {

// Win32 / DirectComposition Thin-Film Medium & Substrate Types
enum class KernelThinFilmMedium : uint32_t {
    Air   = 0, // n1 = 1.000
    Water = 1, // n1 = 1.333
    Glass = 2  // n1 = 1.520
};

enum class KernelThinFilmSubstrate : uint32_t {
    Dielectric      = 0, // n3 = 1.500 (e.g. coated optics, oil on glass)
    Metallic        = 1, // n3 = 2.500 (e.g. titanium oxide, chromium)
    WaterSubstrate = 2  // n3 = 1.333 (e.g. soap bubble in air, oil on puddle)
};

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;

// Physical spectral wavelengths in nanometers (RGB primary channels)
constexpr float WAVELENGTH_RED_NM   = 650.0f;
constexpr float WAVELENGTH_GREEN_NM = 532.0f;
constexpr float WAVELENGTH_BLUE_NM  = 450.0f;

struct KernelIridescenceVector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    [[nodiscard]] constexpr float lengthSquared() const noexcept {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(lengthSquared());
    }

    [[nodiscard]] KernelIridescenceVector3 normalize() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f, 1.0f};
        return {x / len, y / len, z / len};
    }

    [[nodiscard]] constexpr float dot(const KernelIridescenceVector3& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }

    [[nodiscard]] constexpr KernelIridescenceVector3 cross(const KernelIridescenceVector3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] constexpr KernelIridescenceVector3 operator+(const KernelIridescenceVector3& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }

    [[nodiscard]] constexpr KernelIridescenceVector3 operator-(const KernelIridescenceVector3& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }

    [[nodiscard]] constexpr KernelIridescenceVector3 operator-() const noexcept {
        return {-x, -y, -z};
    }

    [[nodiscard]] constexpr KernelIridescenceVector3 operator*(float s) const noexcept {
        return {x * s, y * s, z * s};
    }

    [[nodiscard]] constexpr KernelIridescenceVector3 operator/(float s) const noexcept {
        float inv = (s != 0.0f) ? (1.0f / s) : 0.0f;
        return {x * inv, y * inv, z * inv};
    }
};

struct KernelIridescenceColorRGB {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};

    [[nodiscard]] constexpr KernelIridescenceColorRGB operator+(const KernelIridescenceColorRGB& o) const noexcept {
        return {r + o.r, g + o.g, b + o.b};
    }

    [[nodiscard]] constexpr KernelIridescenceColorRGB operator-(const KernelIridescenceColorRGB& o) const noexcept {
        return {
            std::max(0.0f, r - o.r),
            std::max(0.0f, g - o.g),
            std::max(0.0f, b - o.b)
        };
    }

    [[nodiscard]] constexpr KernelIridescenceColorRGB operator*(const KernelIridescenceColorRGB& o) const noexcept {
        return {r * o.r, g * o.g, b * o.b};
    }

    [[nodiscard]] constexpr KernelIridescenceColorRGB operator*(float s) const noexcept {
        return {r * s, g * s, b * s};
    }

    [[nodiscard]] constexpr KernelIridescenceColorRGB clamp(float minVal = 0.0f, float maxVal = 1.0f) const noexcept {
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

struct KernelThinFilmParameters {
    KernelThinFilmMedium    medium{KernelThinFilmMedium::Air};
    KernelThinFilmSubstrate substrate{KernelThinFilmSubstrate::WaterSubstrate};
    float                   filmIOR{1.40f};           // Refractive index of thin film
    float                   mediumIOR{1.0f};          // Refractive index of external medium (air: 1.0)
    float                   substrateIOR{1.333f};     // Refractive index of underlying substrate (water: 1.333)
    float                   baseThicknessNm{450.0f};  // Nominal thickness in nanometers
    float                   thicknessVarianceNm{200.0f}; // Spatial amplitude of thickness variations
    KernelIridescenceVector3 marangoniVelocity{0.05f, 0.02f, 0.0f}; // Fluid current drift velocity
    float                   drainageFactor{0.15f};    // Gravitational vertical drainage thinning
    float                   roughness{0.02f};         // Microfacet roughness for specular dispersion
    float                   fresnelBoost{1.0f};       // Energy conservation scale factor
};

struct IridescenceSubsystemTelemetry {
    uint64_t totalEvaluations{0};
    uint64_t totalFringeCalculations{0};
    uint64_t constructiveInterferenceEvents{0};
    uint64_t destructiveInterferenceEvents{0};
    double   avgDispatchLatencyUs{0.0};
};

// Subsystem Control & Diagnostics API
bool InitializeIridescenceSubsystem(const KernelThinFilmParameters& params = {}) noexcept;
void ShutdownIridescenceSubsystem() noexcept;
bool SetIridescenceParameters(const KernelThinFilmParameters& params) noexcept;
bool GetIridescenceParameters(KernelThinFilmParameters* outParams) noexcept;

float EvaluateThinFilmThickness(float u, float v, float timeSec) noexcept;
KernelIridescenceColorRGB EvaluateSpectralReflectance(float thicknessNm, float cosTheta) noexcept;

bool ShadeThinFilmSurface(
    const KernelIridescenceVector3& normal,
    const KernelIridescenceVector3& viewDir,
    float u,
    float v,
    float timeSec,
    KernelIridescenceColorRGB* outReflectance,
    KernelIridescenceColorRGB* outTransmittance) noexcept;

bool ExecuteIridescenceStressPass(uint32_t iterations = 200) noexcept;
bool GetIridescenceTelemetry(IridescenceSubsystemTelemetry* outTelemetry) noexcept;
bool VerifyHorizon355IridescenceSubsystem() noexcept;

} // namespace micant::um::prismx_iridescence
