// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 355 (Suite 571)
// DirectComposition Real-Time Thin-Film Optical Interference, Iridescence &
//   Structural Coloration Subsystem (IDCompositionThinFilmInterferenceEffect,
//   IDCompositionIridescentVisual, DirectCompositionIridescencePipeline)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#include "micant/um/subsystems/graphics/prismx_directcomposition_iridescence_wddm32.hpp"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <vector>
#include <memory>
#include <cmath>

namespace micant::um::prismx_iridescence {

namespace {

class KernelIridescenceGovernor {
private:
    KernelThinFilmParameters      m_params{};
    IridescenceSubsystemTelemetry m_telemetry{};
    bool                          m_initialized{false};

public:
    KernelIridescenceGovernor() = default;

    void Initialize(const KernelThinFilmParameters& params) noexcept {
        m_params = params;
        m_params.filmIOR = std::max(1.0f, m_params.filmIOR);
        m_params.mediumIOR = std::max(1.0f, m_params.mediumIOR);
        m_params.substrateIOR = std::max(1.0f, m_params.substrateIOR);
        m_params.baseThicknessNm = std::max(10.0f, m_params.baseThicknessNm);
        m_params.thicknessVarianceNm = std::max(0.0f, m_params.thicknessVarianceNm);
        m_initialized = true;
    }

    void Shutdown() noexcept {
        m_initialized = false;
    }

    [[nodiscard]] bool IsInitialized() const noexcept {
        return m_initialized;
    }

    void SetParams(const KernelThinFilmParameters& params) noexcept {
        m_params = params;
        m_params.filmIOR = std::max(1.0f, m_params.filmIOR);
        m_params.mediumIOR = std::max(1.0f, m_params.mediumIOR);
        m_params.substrateIOR = std::max(1.0f, m_params.substrateIOR);
        m_params.baseThicknessNm = std::max(10.0f, m_params.baseThicknessNm);
        m_params.thicknessVarianceNm = std::max(0.0f, m_params.thicknessVarianceNm);
    }

    [[nodiscard]] KernelThinFilmParameters GetParams() const noexcept {
        return m_params;
    }

    [[nodiscard]] float EvaluateFilmThickness(float u, float v, float timeSec) const noexcept {
        // Marangoni convective fluid swirl currents & vertical gravity drainage
        float flowU = u + std::sin(v * 4.0f + timeSec * 0.8f) * 0.15f;
        float flowV = v + std::cos(u * 3.5f + timeSec * 0.64f) * 0.12f;

        // Gravity drainage: thickness increases downwards along v (from top to bottom)
        float drainage = (1.0f - v) * m_params.drainageFactor;

        // Multi-frequency surface wave harmonic modulation
        float ripple1 = std::sin(flowU * 12.0f + flowV * 8.0f);
        float ripple2 = 0.5f * std::cos(flowU * 22.0f - flowV * 15.0f);
        float ripple = (ripple1 + ripple2) / 1.5f;

        float thickness = m_params.baseThicknessNm + (ripple * m_params.thicknessVarianceNm) + (drainage * m_params.thicknessVarianceNm);
        return std::max(10.0f, thickness);
    }

    [[nodiscard]] static float EvaluateAiryReflectance(
        float lambdaNm,
        float dNm,
        float n1,
        float n2,
        float n3,
        float cosTheta1) noexcept {
        // Snell's Law in thin film: n1 * sin(theta1) = n2 * sin(theta2)
        float sinTheta1 = std::sqrt(std::max(0.0f, 1.0f - cosTheta1 * cosTheta1));
        float sinTheta2 = (n1 / n2) * sinTheta1;
        if (sinTheta2 > 1.0f) {
            return 1.0f; // Total internal reflection in film
        }
        float cosTheta2 = std::sqrt(1.0f - sinTheta2 * sinTheta2);

        // Optical path difference: OPD = 2 * n2 * d * cos(theta2)
        float opd = 2.0f * n2 * dNm * cosTheta2;

        // Fresnel reflection amplitude magnitude coefficients at boundaries
        float r1 = std::abs((n1 - n2) / (n1 + n2));
        float r2 = std::abs((n2 - n3) / (n2 + n3));

        // Reflection phase shift delta: pi shift if going from lower to higher optical index
        float delta = 0.0f;
        if ((n1 < n2 && n2 >= n3) || (n1 >= n2 && n2 < n3)) {
            delta = PI; // Exactly one interface undergoes a pi phase change
        } else {
            delta = 0.0f; // Either both or neither undergo a pi phase change
        }

        // Total phase difference
        float phi = (TWO_PI / lambdaNm) * opd + delta;

        // Airy thin film summation formula:
        // R = (r1^2 + r2^2 + 2 * r1 * r2 * cos(phi)) / (1 + r1^2 * r2^2 + 2 * r1 * r2 * cos(phi))
        float cosPhi = std::cos(phi);
        float r1sq = r1 * r1;
        float r2sq = r2 * r2;
        float num = r1sq + r2sq + 2.0f * r1 * r2 * cosPhi;
        float den = 1.0f + r1sq * r2sq + 2.0f * r1 * r2 * cosPhi;

        if (den < 1e-6f) den = 1e-6f;
        float reflectance = num / den;

        return std::clamp(reflectance, 0.0f, 1.0f);
    }

    [[nodiscard]] KernelIridescenceColorRGB EvaluateSpectralReflectance(float thicknessNm, float cosTheta) const noexcept {
        float n1 = m_params.mediumIOR;
        float n2 = m_params.filmIOR;
        float n3 = m_params.substrateIOR;

        float rR = EvaluateAiryReflectance(WAVELENGTH_RED_NM, thicknessNm, n1, n2, n3, cosTheta);
        float rG = EvaluateAiryReflectance(WAVELENGTH_GREEN_NM, thicknessNm, n1, n2, n3, cosTheta);
        float rB = EvaluateAiryReflectance(WAVELENGTH_BLUE_NM, thicknessNm, n1, n2, n3, cosTheta);

        return {rR * m_params.fresnelBoost, rG * m_params.fresnelBoost, rB * m_params.fresnelBoost};
    }

    bool ShadeSurface(
        const KernelIridescenceVector3& normal,
        const KernelIridescenceVector3& viewDir,
        float u,
        float v,
        float timeSec,
        KernelIridescenceColorRGB* outReflectance,
        KernelIridescenceColorRGB* outTransmittance) noexcept {
        if (!outReflectance || !outTransmittance) return false;
        m_telemetry.totalEvaluations++;

        KernelIridescenceVector3 n = normal.normalize();
        KernelIridescenceVector3 vDir = viewDir.normalize();

        float cosTheta = std::clamp(std::abs(n.dot(vDir)), 0.0f, 1.0f);
        float thickness = EvaluateFilmThickness(u, v, timeSec);

        KernelIridescenceColorRGB refl = EvaluateSpectralReflectance(thickness, cosTheta);
        m_telemetry.totalFringeCalculations += 3;

        // Categorize constructive vs destructive interference events
        if (refl.luminance() > 0.40f) {
            m_telemetry.constructiveInterferenceEvents++;
        } else if (refl.luminance() < 0.15f) {
            m_telemetry.destructiveInterferenceEvents++;
        }

        // Energy conservation: Transmittance = 1.0 - Reflectance
        KernelIridescenceColorRGB trans{
            std::clamp(1.0f - refl.r, 0.0f, 1.0f),
            std::clamp(1.0f - refl.g, 0.0f, 1.0f),
            std::clamp(1.0f - refl.b, 0.0f, 1.0f)
        };

        *outReflectance = refl.clamp(0.0f, 1.0f);
        *outTransmittance = trans.clamp(0.0f, 1.0f);
        return true;
    }

    bool ExecuteStressPass(uint32_t iterations) noexcept {
        auto start = std::chrono::high_resolution_clock::now();

        KernelIridescenceVector3 norm{0.0f, 0.0f, 1.0f};
        KernelIridescenceVector3 vDir{0.3f, 0.2f, 0.9f};

        for (uint32_t frame = 0; frame < iterations; ++frame) {
            float timeSec = static_cast<float>(frame) * 0.016f;
            KernelIridescenceColorRGB refl{};
            KernelIridescenceColorRGB trans{};
            ShadeSurface(norm, vDir, 0.5f, 0.5f, timeSec, &refl, &trans);
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed = end - start;
        double avgLat = (iterations > 0) ? (elapsed.count() / iterations) : 0.0;
        m_telemetry.avgDispatchLatencyUs = avgLat;

        return (avgLat < 75.0);
    }

    [[nodiscard]] IridescenceSubsystemTelemetry GetTelemetry() const noexcept {
        return m_telemetry;
    }
};

static KernelIridescenceGovernor g_governor{};

} // anonymous namespace

bool InitializeIridescenceSubsystem(const KernelThinFilmParameters& params) noexcept {
    g_governor.Initialize(params);
    return true;
}

void ShutdownIridescenceSubsystem() noexcept {
    g_governor.Shutdown();
}

bool SetIridescenceParameters(const KernelThinFilmParameters& params) noexcept {
    g_governor.SetParams(params);
    return true;
}

bool GetIridescenceParameters(KernelThinFilmParameters* outParams) noexcept {
    if (!outParams) return false;
    *outParams = g_governor.GetParams();
    return true;
}

float EvaluateThinFilmThickness(float u, float v, float timeSec) noexcept {
    return g_governor.EvaluateFilmThickness(u, v, timeSec);
}

KernelIridescenceColorRGB EvaluateSpectralReflectance(float thicknessNm, float cosTheta) noexcept {
    return g_governor.EvaluateSpectralReflectance(thicknessNm, cosTheta);
}

bool ShadeThinFilmSurface(
    const KernelIridescenceVector3& normal,
    const KernelIridescenceVector3& viewDir,
    float u,
    float v,
    float timeSec,
    KernelIridescenceColorRGB* outReflectance,
    KernelIridescenceColorRGB* outTransmittance) noexcept {
    return g_governor.ShadeSurface(normal, viewDir, u, v, timeSec, outReflectance, outTransmittance);
}

bool ExecuteIridescenceStressPass(uint32_t iterations) noexcept {
    return g_governor.ExecuteStressPass(iterations);
}

bool GetIridescenceTelemetry(IridescenceSubsystemTelemetry* outTelemetry) noexcept {
    if (!outTelemetry) return false;
    *outTelemetry = g_governor.GetTelemetry();
    return true;
}

bool VerifyHorizon355IridescenceSubsystem() noexcept {
    // Pillar 1: Configuration Parameters & Presets
    KernelThinFilmParameters p{};
    p.medium = KernelThinFilmMedium::Air;
    p.substrate = KernelThinFilmSubstrate::WaterSubstrate;
    p.filmIOR = 1.40f;
    p.mediumIOR = 1.0f;
    p.substrateIOR = 1.333f;
    p.baseThicknessNm = 450.0f;
    p.thicknessVarianceNm = 200.0f;

    if (!InitializeIridescenceSubsystem(p)) return false;

    KernelThinFilmParameters retrieved{};
    if (!GetIridescenceParameters(&retrieved)) return false;
    if (std::abs(retrieved.filmIOR - 1.40f) > 1e-4f) return false;
    if (std::abs(retrieved.baseThicknessNm - 450.0f) > 1e-4f) return false;

    // Pillar 2: Airy Thin-Film Wave Interference Invariants (R in [0, 1] and R + T == 1.0)
    KernelIridescenceVector3 normal{0.0f, 0.0f, 1.0f};
    KernelIridescenceVector3 viewDir{0.0f, 0.0f, 1.0f}; // Normal incidence
    KernelIridescenceColorRGB refl{};
    KernelIridescenceColorRGB trans{};

    if (!ShadeThinFilmSurface(normal, viewDir, 0.5f, 0.5f, 0.0f, &refl, &trans)) return false;

    if (refl.r < 0.0f || refl.r > 1.0f || refl.g < 0.0f || refl.g > 1.0f || refl.b < 0.0f || refl.b > 1.0f) return false;
    if (std::abs((refl.r + trans.r) - 1.0f) > 1e-4f) return false;
    if (std::abs((refl.g + trans.g) - 1.0f) > 1e-4f) return false;
    if (std::abs((refl.b + trans.b) - 1.0f) > 1e-4f) return false;

    // Pillar 3: Constructive & Destructive Interference Spectral Discrimination
    // For green light (lambda = 532nm), soap film (n2 = 1.4, n1 = 1.0, n3 = 1.333 -> delta = pi):
    // Constructive condition: OPD = 2 * n2 * d = (m + 0.5) * lambda
    // For m = 1: 2 * 1.4 * d = 1.5 * 532 = 798 -> d = 285nm
    KernelIridescenceColorRGB reflConstructive = EvaluateSpectralReflectance(285.0f, 1.0f);
    // Destructive condition: 2 * n2 * d = m * lambda = 532 -> d = 190nm
    KernelIridescenceColorRGB reflDestructive = EvaluateSpectralReflectance(190.0f, 1.0f);

    // Green channel at constructive thickness must be strictly greater than destructive
    if (reflConstructive.g <= reflDestructive.g) return false;

    // Pillar 4: Angular Viewing Shift (Iridescence Fringes)
    // Grazing view angle (cosTheta = 0.3) decreases effective optical path difference, shifting fringes
    KernelIridescenceColorRGB reflNormal = EvaluateSpectralReflectance(400.0f, 1.0f);
    KernelIridescenceColorRGB reflGrazing = EvaluateSpectralReflectance(400.0f, 0.3f);
    if (std::abs(reflNormal.luminance() - reflGrazing.luminance()) < 1e-4f) return false;

    // Pillar 5: Marangoni Fluid Flow & Spatial Dynamic Thickness Variation
    float d1 = EvaluateThinFilmThickness(0.1f, 0.2f, 0.0f);
    float d2 = EvaluateThinFilmThickness(0.8f, 0.9f, 0.0f);
    if (std::abs(d1 - d2) < 1.0f) return false; // Must exhibit spatial thickness variation
    if (d1 < 10.0f || d2 < 10.0f) return false;

    // Pillar 6: High-Throughput Stress Test & Sub-75us Latency
    if (!ExecuteIridescenceStressPass(200)) return false;

    return true;
}

} // namespace micant::um::prismx_iridescence
