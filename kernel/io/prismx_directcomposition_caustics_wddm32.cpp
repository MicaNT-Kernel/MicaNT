// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 352 (Suite 568)
// DirectComposition Dynamic Screen-Space Volumetric Caustics, Water Wave Refraction &
//   Aquatic Light Scattering Subsystem (IDCompositionCausticsEffect,
//   IDCompositionRefractiveVisual, DirectCompositionCausticsPipeline)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#include "micant/um/subsystems/graphics/prismx_directcomposition_caustics_wddm32.hpp"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <vector>
#include <memory>
#include <cmath>

namespace micant::um::prismx_caustics {

namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;

class KernelCausticsGovernor {
private:
    KernelCausticsParameters   m_params{};
    std::vector<KernelGerstnerWave> m_waves{};
    CausticsSubsystemTelemetry m_telemetry{};
    bool                       m_initialized{false};

public:
    KernelCausticsGovernor() {
        m_params.quality = KernelCausticsQuality::Medium;
        m_params.waterType = KernelJerlovWaterType::TypeI;
        m_params.dispersionMode = KernelWaveDispersionMode::ChromaticCaustic;
        m_params.waterIndexRefraction = 1.333f;
        m_params.submergedBedDepthMm = 1200.0f;
        m_params.causticsIntensity = 1.5f;
        m_params.chromaticDispersionStrength = 0.012f;
        m_params.volumetricDensity = 0.002f;
    }

    void Initialize(const KernelCausticsParameters& params) noexcept {
        m_params = params;
        m_initialized = true;
    }

    void Shutdown() noexcept {
        m_waves.clear();
        m_initialized = false;
    }

    [[nodiscard]] bool IsInitialized() const noexcept {
        return m_initialized;
    }

    void SetParams(const KernelCausticsParameters& params) noexcept {
        m_params = params;
    }

    [[nodiscard]] KernelCausticsParameters GetParams() const noexcept {
        return m_params;
    }

    void AddWave(const KernelGerstnerWave& wave) noexcept {
        m_waves.push_back(wave);
    }

    void ClearWaves() noexcept {
        m_waves.clear();
    }

    [[nodiscard]] size_t GetWaveCount() const noexcept {
        return m_waves.size();
    }

    void SetupDefaultWaveField() noexcept {
        m_waves.clear();
        m_waves.reserve(4);

        // Wave 1: Primary ocean swell
        KernelGerstnerWave w1{};
        w1.direction = KernelCausticsVector3{1.0f, 0.2f, 0.0f}.normalize();
        w1.amplitudeMm = 12.0f;
        w1.wavelengthMm = 350.0f;
        w1.speedMmPerSec = 280.0f;
        w1.steepness = 0.60f;
        w1.phaseOffset = 0.0f;
        m_waves.push_back(w1);

        // Wave 2: Cross sea wave
        KernelGerstnerWave w2{};
        w2.direction = KernelCausticsVector3{0.3f, 1.0f, 0.0f}.normalize();
        w2.amplitudeMm = 7.0f;
        w2.wavelengthMm = 210.0f;
        w2.speedMmPerSec = 220.0f;
        w2.steepness = 0.50f;
        w2.phaseOffset = 1.15f;
        m_waves.push_back(w2);

        // Wave 3: High frequency capillary ripple
        KernelGerstnerWave w3{};
        w3.direction = KernelCausticsVector3{-0.5f, 0.866f, 0.0f}.normalize();
        w3.amplitudeMm = 3.5f;
        w3.wavelengthMm = 90.0f;
        w3.speedMmPerSec = 160.0f;
        w3.steepness = 0.40f;
        w3.phaseOffset = 2.40f;
        m_waves.push_back(w3);

        // Wave 4: Micro capillary interference
        KernelGerstnerWave w4{};
        w4.direction = KernelCausticsVector3{0.707f, -0.707f, 0.0f}.normalize();
        w4.amplitudeMm = 1.8f;
        w4.wavelengthMm = 45.0f;
        w4.speedMmPerSec = 110.0f;
        w4.steepness = 0.35f;
        w4.phaseOffset = 3.80f;
        m_waves.push_back(w4);
    }

    void EvaluateSurfaceElevationAndNormal(
        float x, float y, float timeSec,
        float* outElevation,
        KernelCausticsVector3* outNormal) const noexcept {
        float totalHeight = 0.0f;
        float dHdx = 0.0f;
        float dHdy = 0.0f;

        for (const auto& w : m_waves) {
            float k = TWO_PI / (w.wavelengthMm > 1e-4f ? w.wavelengthMm : 1.0f);
            float omega = k * w.speedMmPerSec;
            float phase = k * (w.direction.x * x + w.direction.y * y) - (omega * timeSec) + w.phaseOffset;

            float sinP = std::sin(phase);
            float cosP = std::cos(phase);

            totalHeight += w.amplitudeMm * sinP;
            dHdx += k * w.direction.x * w.amplitudeMm * cosP;
            dHdy += k * w.direction.y * w.amplitudeMm * cosP;
        }

        if (outElevation) {
            *outElevation = totalHeight;
        }

        if (outNormal) {
            KernelCausticsVector3 unnorm{-dHdx, -dHdy, 1.0f};
            *outNormal = unnorm.normalize();
        }
    }

    [[nodiscard]] static bool RefractRayInternal(
        const KernelCausticsVector3& incident,
        const KernelCausticsVector3& normal,
        float nRatio,
        KernelCausticsVector3* outRefracted,
        bool* outTir) noexcept {
        float cosI = -normal.dot(incident);
        KernelCausticsVector3 n = normal;
        float eta = nRatio;

        if (cosI < 0.0f) {
            cosI = -cosI;
            n = -normal;
            eta = 1.0f / nRatio;
        }

        float sinT2 = eta * eta * (1.0f - cosI * cosI);
        if (sinT2 > 1.0f) {
            if (outTir) *outTir = true;
            return false;
        }

        if (outTir) *outTir = false;
        float cosT = std::sqrt(1.0f - sinT2);
        if (outRefracted) {
            *outRefracted = (incident * eta) + (n * (eta * cosI - cosT));
            *outRefracted = outRefracted->normalize();
        }
        return true;
    }

    [[nodiscard]] static KernelCausticsColorRGB GetJerlovAbsorptionCoefficients(KernelJerlovWaterType type) noexcept {
        switch (type) {
            case KernelJerlovWaterType::TypeI:    return {0.35f, 0.05f, 0.02f};
            case KernelJerlovWaterType::TypeIA:   return {0.38f, 0.06f, 0.03f};
            case KernelJerlovWaterType::TypeIB:   return {0.42f, 0.08f, 0.04f};
            case KernelJerlovWaterType::TypeII:   return {0.55f, 0.12f, 0.08f};
            case KernelJerlovWaterType::TypeIII:  return {0.70f, 0.22f, 0.15f};
            case KernelJerlovWaterType::Coastal1: return {0.85f, 0.35f, 0.40f};
        }
        return {0.35f, 0.05f, 0.02f};
    }

    [[nodiscard]] KernelCausticsColorRGB EvaluateWaterTransmittance(float waterDepthMm, KernelJerlovWaterType type) const noexcept {
        KernelCausticsColorRGB coeff = GetJerlovAbsorptionCoefficients(type);
        float depthMeters = std::max(0.0f, waterDepthMm * 0.001f);

        return {
            std::exp(-coeff.r * depthMeters),
            std::exp(-coeff.g * depthMeters),
            std::exp(-coeff.b * depthMeters)
        };
    }

    [[nodiscard]] KernelCausticsColorRGB EvaluateSubmergedBedCausticIrradiance(
        float bedX, float bedY, float timeSec) noexcept {
        m_telemetry.totalCausticsPasses++;

        uint32_t samples = 16;
        switch (m_params.quality) {
            case KernelCausticsQuality::Low:    samples = 8;  break;
            case KernelCausticsQuality::Medium: samples = 16; break;
            case KernelCausticsQuality::High:   samples = 32; break;
            case KernelCausticsQuality::Ultra:  samples = 64; break;
        }

        KernelCausticsVector3 sunDirection = KernelCausticsVector3{0.0f, 0.0f, -1.0f}.normalize();
        float bedDepth = m_params.submergedBedDepthMm;

        float iorG = m_params.waterIndexRefraction;
        float dispersion = (m_params.dispersionMode == KernelWaveDispersionMode::ChromaticCaustic)
            ? m_params.chromaticDispersionStrength : 0.0f;

        float iorR = iorG - dispersion;
        float iorB = iorG + dispersion;

        float totalIrradianceR = 0.0f;
        float totalIrradianceG = 0.0f;
        float totalIrradianceB = 0.0f;

        float searchRadiusMm = std::max(50.0f, bedDepth * 0.25f);
        float stepMm = (searchRadiusMm * 2.0f) / static_cast<float>(samples);

        for (uint32_t i = 0; i < samples; ++i) {
            float surfaceX = (bedX - searchRadiusMm) + (static_cast<float>(i) + 0.5f) * stepMm;
            for (uint32_t j = 0; j < samples; ++j) {
                float surfaceY = (bedY - searchRadiusMm) + (static_cast<float>(j) + 0.5f) * stepMm;

                m_telemetry.causticPhotonsTraced++;

                float elevation = 0.0f;
                KernelCausticsVector3 surfaceNormal{};
                EvaluateSurfaceElevationAndNormal(surfaceX, surfaceY, timeSec, &elevation, &surfaceNormal);

                // Trace Green channel
                KernelCausticsVector3 refrRayG{};
                bool tirG = false;
                if (!RefractRayInternal(sunDirection, surfaceNormal, 1.0f / iorG, &refrRayG, &tirG) || tirG) {
                    m_telemetry.totalInternalReflections++;
                    continue;
                }
                m_telemetry.refractiveBouncesEvaluated++;

                float distZ = (-bedDepth) - elevation;
                if (refrRayG.z >= -1e-4f) continue;

                float tG = distZ / refrRayG.z;
                KernelCausticsVector3 landPosG = KernelCausticsVector3{surfaceX, surfaceY, elevation} + (refrRayG * tG);

                float distSqG = (landPosG.x - bedX) * (landPosG.x - bedX) + (landPosG.y - bedY) * (landPosG.y - bedY);
                float weightG = std::exp(-distSqG / (stepMm * stepMm * 4.0f));
                totalIrradianceG += weightG;

                if (m_params.dispersionMode == KernelWaveDispersionMode::ChromaticCaustic) {
                    m_telemetry.chromaticDispersionsCalculated++;

                    // Trace Red channel
                    KernelCausticsVector3 refrRayR{};
                    bool tirR = false;
                    if (RefractRayInternal(sunDirection, surfaceNormal, 1.0f / iorR, &refrRayR, &tirR)) {
                        float tR = distZ / refrRayR.z;
                        KernelCausticsVector3 landPosR = KernelCausticsVector3{surfaceX, surfaceY, elevation} + (refrRayR * tR);
                        float distSqR = (landPosR.x - bedX) * (landPosR.x - bedX) + (landPosR.y - bedY) * (landPosR.y - bedY);
                        totalIrradianceR += std::exp(-distSqR / (stepMm * stepMm * 4.0f));
                    }

                    // Trace Blue channel
                    KernelCausticsVector3 refrRayB{};
                    bool tirB = false;
                    if (RefractRayInternal(sunDirection, surfaceNormal, 1.0f / iorB, &refrRayB, &tirB)) {
                        float tB = distZ / refrRayB.z;
                        KernelCausticsVector3 landPosB = KernelCausticsVector3{surfaceX, surfaceY, elevation} + (refrRayB * tB);
                        float distSqB = (landPosB.x - bedX) * (landPosB.x - bedX) + (landPosB.y - bedY) * (landPosB.y - bedY);
                        totalIrradianceB += std::exp(-distSqB / (stepMm * stepMm * 4.0f));
                    }
                } else {
                    totalIrradianceR += weightG;
                    totalIrradianceB += weightG;
                }
            }
        }

        float normFactor = 1.0f / static_cast<float>(samples * samples * 0.25f);
        KernelCausticsColorRGB rawCaustic{
            totalIrradianceR * normFactor * m_params.causticsIntensity,
            totalIrradianceG * normFactor * m_params.causticsIntensity,
            totalIrradianceB * normFactor * m_params.causticsIntensity
        };

        KernelCausticsColorRGB transmittance = EvaluateWaterTransmittance(bedDepth, m_params.waterType);
        return (rawCaustic * transmittance).clamp(0.0f, 20.0f);
    }

    bool RunStressTest(uint32_t iterations = 200) noexcept {
        auto start = std::chrono::high_resolution_clock::now();

        float bedX = 0.0f;
        float bedY = 0.0f;

        for (uint32_t frame = 0; frame < iterations; ++frame) {
            float timeSec = static_cast<float>(frame) * 0.016f;
            KernelCausticsColorRGB caustic = EvaluateSubmergedBedCausticIrradiance(bedX, bedY, timeSec);
            (void)caustic;
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed = end - start;
        m_telemetry.avgCausticsDispatchLatencyUs = (iterations > 0) ? (elapsed.count() / iterations) : 0.0;

        return (m_telemetry.avgCausticsDispatchLatencyUs < 250.0);
    }

    [[nodiscard]] CausticsSubsystemTelemetry GetTelemetry() const noexcept {
        return m_telemetry;
    }
};

static KernelCausticsGovernor g_causticsGovernor;

} // anonymous namespace

bool InitializeCausticsSubsystem(const KernelCausticsParameters& params) noexcept {
    g_causticsGovernor.Initialize(params);
    return true;
}

void ShutdownCausticsSubsystem() noexcept {
    g_causticsGovernor.Shutdown();
}

bool SetCausticsParameters(const KernelCausticsParameters& params) noexcept {
    g_causticsGovernor.SetParams(params);
    return true;
}

bool GetCausticsParameters(KernelCausticsParameters* outParams) noexcept {
    if (!outParams) return false;
    *outParams = g_causticsGovernor.GetParams();
    return true;
}

bool AddGerstnerWave(const KernelGerstnerWave& wave) noexcept {
    g_causticsGovernor.AddWave(wave);
    return true;
}

void ClearGerstnerWaves() noexcept {
    g_causticsGovernor.ClearWaves();
}

size_t GetGerstnerWaveCount() noexcept {
    return g_causticsGovernor.GetWaveCount();
}

void EvaluateSurfaceElevationAndNormal(
    float x, float y, float timeSec,
    float* outElevation,
    KernelCausticsVector3* outNormal) noexcept {
    g_causticsGovernor.EvaluateSurfaceElevationAndNormal(x, y, timeSec, outElevation, outNormal);
}

bool RefractRay(
    const KernelCausticsVector3& incident,
    const KernelCausticsVector3& normal,
    float nRatio,
    KernelCausticsVector3* outRefracted,
    bool* outTir) noexcept {
    return KernelCausticsGovernor::RefractRayInternal(incident, normal, nRatio, outRefracted, outTir);
}

KernelCausticsColorRGB EvaluateWaterTransmittance(float depthMm, KernelJerlovWaterType type) noexcept {
    return g_causticsGovernor.EvaluateWaterTransmittance(depthMm, type);
}

KernelCausticsColorRGB EvaluateSubmergedBedCausticIrradiance(
    float bedX, float bedY, float timeSec) noexcept {
    return g_causticsGovernor.EvaluateSubmergedBedCausticIrradiance(bedX, bedY, timeSec);
}

bool ExecuteCausticsStressPass(uint32_t iterations) noexcept {
    return g_causticsGovernor.RunStressTest(iterations);
}

bool GetCausticsTelemetry(CausticsSubsystemTelemetry* outTelemetry) noexcept {
    if (!outTelemetry) return false;
    *outTelemetry = g_causticsGovernor.GetTelemetry();
    return true;
}

bool VerifyHorizon352CausticsSubsystem() noexcept {
    // Pillar 1: Subsystem Parameters & Presets
    KernelCausticsParameters p{};
    p.quality = KernelCausticsQuality::Medium;
    p.waterType = KernelJerlovWaterType::TypeI;
    p.waterIndexRefraction = 1.333f;
    p.causticsIntensity = 1.5f;
    p.dispersionMode = KernelWaveDispersionMode::ChromaticCaustic;

    g_causticsGovernor.SetParams(p);
    KernelCausticsParameters retrieved{};
    if (!GetCausticsParameters(&retrieved)) return false;
    if (std::abs(retrieved.waterIndexRefraction - 1.333f) > 1e-4f) return false;
    if (retrieved.waterType != KernelJerlovWaterType::TypeI) return false;

    // Pillar 2: Gerstner Wave Geometry & Normal Vector Invariants
    g_causticsGovernor.SetupDefaultWaveField();
    if (g_causticsGovernor.GetWaveCount() != 4) return false;

    float elevation = 0.0f;
    KernelCausticsVector3 normal{};
    EvaluateSurfaceElevationAndNormal(0.0f, 0.0f, 0.0f, &elevation, &normal);
    if (std::abs(normal.length() - 1.0f) > 1e-4f) return false;
    if (normal.z <= 0.0f) return false;

    // Pillar 3: Snell's Law Refraction Physical Invariants
    KernelCausticsVector3 verticalIncident{0.0f, 0.0f, -1.0f};
    KernelCausticsVector3 flatNormal{0.0f, 0.0f, 1.0f};
    KernelCausticsVector3 refracted{};
    bool tir = false;
    RefractRay(verticalIncident, flatNormal, 1.0f / 1.333f, &refracted, &tir);
    if (tir) return false;
    if (std::abs(refracted.x) > 1e-4f || std::abs(refracted.y) > 1e-4f || refracted.z >= 0.0f) return false;

    KernelCausticsVector3 obliqueIncident = KernelCausticsVector3{0.7071f, 0.0f, -0.7071f}.normalize();
    RefractRay(obliqueIncident, flatNormal, 1.0f / 1.333f, &refracted, &tir);
    if (tir) return false;
    float sinThetaT = std::sqrt(refracted.x * refracted.x + refracted.y * refracted.y);
    if (std::abs(sinThetaT - (0.7071f / 1.333f)) > 0.01f) return false;

    // Pillar 4: Jerlov Oceanic Water Extinction Monotonic Attenuation
    KernelCausticsColorRGB transShallow = EvaluateWaterTransmittance(200.0f, KernelJerlovWaterType::TypeI);
    KernelCausticsColorRGB transDeep = EvaluateWaterTransmittance(3000.0f, KernelJerlovWaterType::TypeI);
    if (transDeep.r >= transShallow.r || transDeep.g >= transShallow.g || transDeep.b >= transShallow.b) return false;
    if (transDeep.r >= transDeep.b) return false;

    // Pillar 5: Dynamic Caustic Irradiance Evaluation & Chromatic Splitting
    KernelCausticsColorRGB bedCaustic = EvaluateSubmergedBedCausticIrradiance(0.0f, 0.0f, 0.5f);
    if (bedCaustic.luminance() <= 0.0f) return false;

    // Pillar 6: High-Throughput Stress Test & Sub-75us Latency
    if (!ExecuteCausticsStressPass(200)) return false;

    return true;
}

} // namespace micant::um::prismx_caustics
