// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 353 (Suite 569)
// DirectComposition Dynamic GPU Particle System, Curl Noise Fluid Flow &
//   Holographic Energy Field Subsystem (IDCompositionParticleEmitterEffect,
//   IDCompositionParticleSystemVisual, DirectCompositionParticlePipeline)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#include "micant/um/subsystems/graphics/prismx_directcomposition_particles_wddm32.hpp"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <vector>
#include <memory>
#include <cmath>

namespace micant::um::prismx_particles {

namespace {

class KernelCurlNoise3D {
private:
    [[nodiscard]] static KernelParticleVector3 EvaluatePotential(const KernelParticleVector3& p, float freq) noexcept {
        float x = p.x * freq;
        float y = p.y * freq;
        float z = p.z * freq;

        float psiX = std::sin(y * 1.3f + z * 0.7f) + 0.5f * std::cos(z * 2.1f + x * 0.9f);
        float psiY = std::sin(z * 1.1f + x * 1.5f) + 0.5f * std::cos(x * 1.7f + y * 1.1f);
        float psiZ = std::sin(x * 0.9f + y * 1.2f) + 0.5f * std::cos(y * 1.9f + z * 1.4f);

        return {psiX, psiY, psiZ};
    }

public:
    [[nodiscard]] static KernelParticleVector3 SampleCurl(const KernelParticleVector3& pos, float frequency, float strength) noexcept {
        constexpr float eps = 0.05f;
        constexpr float inv2Eps = 1.0f / (2.0f * eps);

        KernelParticleVector3 pXp = EvaluatePotential(pos + KernelParticleVector3{eps, 0.0f, 0.0f}, frequency);
        KernelParticleVector3 pXm = EvaluatePotential(pos - KernelParticleVector3{eps, 0.0f, 0.0f}, frequency);
        KernelParticleVector3 pYp = EvaluatePotential(pos + KernelParticleVector3{0.0f, eps, 0.0f}, frequency);
        KernelParticleVector3 pYm = EvaluatePotential(pos - KernelParticleVector3{0.0f, eps, 0.0f}, frequency);
        KernelParticleVector3 pZp = EvaluatePotential(pos + KernelParticleVector3{0.0f, 0.0f, eps}, frequency);
        KernelParticleVector3 pZm = EvaluatePotential(pos - KernelParticleVector3{0.0f, 0.0f, eps}, frequency);

        float dPsiZ_dy = (pYp.z - pYm.z) * inv2Eps;
        float dPsiY_dz = (pZp.y - pZm.y) * inv2Eps;

        float dPsiX_dz = (pZp.x - pZm.x) * inv2Eps;
        float dPsiZ_dx = (pXp.z - pXm.z) * inv2Eps;

        float dPsiY_dx = (pXp.y - pXm.y) * inv2Eps;
        float dPsiX_dy = (pYp.x - pYm.x) * inv2Eps;

        KernelParticleVector3 curl{
            dPsiZ_dy - dPsiY_dz,
            dPsiX_dz - dPsiZ_dx,
            dPsiY_dx - dPsiX_dy
        };

        return curl * strength;
    }

    [[nodiscard]] static float ComputeDivergence(const KernelParticleVector3& pos, float frequency, float strength) noexcept {
        constexpr float eps = 0.05f;
        constexpr float inv2Eps = 1.0f / (2.0f * eps);

        KernelParticleVector3 vXp = SampleCurl(pos + KernelParticleVector3{eps, 0.0f, 0.0f}, frequency, strength);
        KernelParticleVector3 vXm = SampleCurl(pos - KernelParticleVector3{eps, 0.0f, 0.0f}, frequency, strength);
        KernelParticleVector3 vYp = SampleCurl(pos + KernelParticleVector3{0.0f, eps, 0.0f}, frequency, strength);
        KernelParticleVector3 vYm = SampleCurl(pos - KernelParticleVector3{0.0f, eps, 0.0f}, frequency, strength);
        KernelParticleVector3 vZp = SampleCurl(pos + KernelParticleVector3{0.0f, 0.0f, eps}, frequency, strength);
        KernelParticleVector3 vZm = SampleCurl(pos - KernelParticleVector3{0.0f, 0.0f, eps}, frequency, strength);

        float dVx_dx = (vXp.x - vXm.x) * inv2Eps;
        float dVy_dy = (vYp.y - vYm.y) * inv2Eps;
        float dVz_dz = (vZp.z - vZm.z) * inv2Eps;

        return dVx_dx + dVy_dy + dVz_dz;
    }
};

class KernelParticleGovernor {
private:
    KernelParticleEmitterParameters m_params{};
    std::vector<KernelParticle>     m_particles{};
    ParticleSubsystemTelemetry      m_telemetry{};
    float                           m_spawnAccumulator{0.0f};
    uint32_t                        m_pendingBurst{0};
    uint64_t                        m_prngState{987654321ULL};
    bool                            m_initialized{false};

    [[nodiscard]] float Random01() noexcept {
        m_prngState ^= (m_prngState << 13);
        m_prngState ^= (m_prngState >> 7);
        m_prngState ^= (m_prngState << 17);
        return static_cast<float>(m_prngState & 0x00FFFFFFULL) / 16777215.0f;
    }

    [[nodiscard]] float RandomRange(float minVal, float maxVal) noexcept {
        return minVal + (maxVal - minVal) * Random01();
    }

    void SpawnParticle() noexcept {
        size_t targetIndex = m_particles.size();
        for (size_t i = 0; i < m_particles.size(); ++i) {
            if (!m_particles[i].alive) {
                targetIndex = i;
                break;
            }
        }

        if (targetIndex >= m_params.maxActiveParticles) return;

        if (targetIndex == m_particles.size()) {
            m_particles.emplace_back();
        }

        KernelParticle& pt = m_particles[targetIndex];
        pt.alive = true;
        pt.age = 0.0f;
        pt.lifespan = m_params.particleLifespanSec * RandomRange(0.8f, 1.2f);
        pt.mass = 1.0f;
        pt.startColor = m_params.startColor;
        pt.endColor = m_params.endColor;
        pt.color = m_params.startColor;
        pt.startSize = m_params.startSize;
        pt.endSize = m_params.endSize;
        pt.size = m_params.startSize;

        switch (m_params.shape) {
            case KernelParticleEmitterShape::Point:
                pt.position = m_params.emitterPosition;
                break;
            case KernelParticleEmitterShape::Box:
                pt.position = m_params.emitterPosition + KernelParticleVector3{
                    RandomRange(-m_params.emitterBoxExtents.x, m_params.emitterBoxExtents.x),
                    RandomRange(-m_params.emitterBoxExtents.y, m_params.emitterBoxExtents.y),
                    RandomRange(-m_params.emitterBoxExtents.z, m_params.emitterBoxExtents.z)
                };
                break;
            case KernelParticleEmitterShape::Sphere: {
                float theta = RandomRange(0.0f, 6.2831853f);
                float phi = std::acos(RandomRange(-1.0f, 1.0f));
                float r = m_params.emitterSphereRadius * std::cbrt(Random01());
                pt.position = m_params.emitterPosition + KernelParticleVector3{
                    r * std::sin(phi) * std::cos(theta),
                    r * std::sin(phi) * std::sin(theta),
                    r * std::cos(phi)
                };
                break;
            }
            case KernelParticleEmitterShape::Disc: {
                float theta = RandomRange(0.0f, 6.2831853f);
                float r = m_params.emitterSphereRadius * std::sqrt(Random01());
                pt.position = m_params.emitterPosition + KernelParticleVector3{
                    r * std::cos(theta),
                    r * std::sin(theta),
                    0.0f
                };
                break;
            }
            case KernelParticleEmitterShape::Line: {
                float t = RandomRange(-1.0f, 1.0f);
                pt.position = m_params.emitterPosition + (m_params.emitterBoxExtents * t);
                break;
            }
        }

        float speed = RandomRange(m_params.speedMin, m_params.speedMax);
        KernelParticleVector3 baseDir = m_params.initialVelocity.normalize();
        float spreadRad = m_params.velocitySpreadAngleDeg * (3.14159265f / 180.0f);

        float angleX = RandomRange(-spreadRad, spreadRad);
        float angleY = RandomRange(-spreadRad, spreadRad);
        KernelParticleVector3 perturbedDir{
            baseDir.x + angleX,
            baseDir.y + angleY,
            baseDir.z
        };
        pt.velocity = perturbedDir.normalize() * speed;
        pt.acceleration = m_params.gravity;

        m_telemetry.particlesSpawned++;
    }

public:
    KernelParticleGovernor() {
        m_particles.reserve(2048);
    }

    void Initialize(const KernelParticleEmitterParameters& params) noexcept {
        m_params = params;
        m_initialized = true;
    }

    void Shutdown() noexcept {
        m_particles.clear();
        m_initialized = false;
    }

    [[nodiscard]] bool IsInitialized() const noexcept {
        return m_initialized;
    }

    void SetParams(const KernelParticleEmitterParameters& params) noexcept {
        m_params = params;
    }

    [[nodiscard]] KernelParticleEmitterParameters GetParams() const noexcept {
        return m_params;
    }

    void TriggerBurst(uint32_t count) noexcept {
        m_pendingBurst += count;
    }

    bool Update(float deltaTimeSec) noexcept {
        m_telemetry.totalSimulationSteps++;

        m_spawnAccumulator += deltaTimeSec * m_params.emissionRate;
        uint32_t countToSpawn = static_cast<uint32_t>(m_spawnAccumulator);
        m_spawnAccumulator -= static_cast<float>(countToSpawn);

        uint32_t burst = m_pendingBurst;
        m_pendingBurst = 0;
        countToSpawn += burst;

        for (uint32_t i = 0; i < countToSpawn; ++i) {
            SpawnParticle();
        }

        uint32_t activeCount = 0;

        for (auto& pt : m_particles) {
            if (!pt.alive) continue;

            pt.age += deltaTimeSec;
            if (pt.age >= pt.lifespan) {
                pt.alive = false;
                m_telemetry.particlesDied++;
                continue;
            }

            activeCount++;
            float normAge = pt.age / pt.lifespan;

            pt.color = KernelParticleColorRGBA::Lerp(pt.startColor, pt.endColor, normAge);
            pt.size = pt.startSize + (pt.endSize - pt.startSize) * normAge;

            KernelParticleVector3 curlForce{0.0f, 0.0f, 0.0f};
            if (m_params.curlNoiseStrength > 1e-4f) {
                m_telemetry.curlEvaluations++;
                curlForce = KernelCurlNoise3D::SampleCurl(pt.position, m_params.curlNoiseFrequency, m_params.curlNoiseStrength);
            }

            KernelParticleVector3 dragForce = pt.velocity * (-m_params.dragCoefficient);
            pt.acceleration = m_params.gravity + curlForce + dragForce;

            pt.velocity = pt.velocity + (pt.acceleration * deltaTimeSec);
            pt.position = pt.position + (pt.velocity * deltaTimeSec);

            if (m_params.floorCollisionEnabled && pt.position.z <= m_params.floorHeight) {
                pt.position.z = m_params.floorHeight;
                if (pt.velocity.z < 0.0f) {
                    pt.velocity.z = -pt.velocity.z * m_params.collisionRestitution;
                    pt.velocity.x *= m_params.collisionFriction;
                    pt.velocity.y *= m_params.collisionFriction;
                    m_telemetry.floorCollisions++;
                }
            }
        }

        m_telemetry.activeParticleCount = activeCount;
        return true;
    }

    [[nodiscard]] uint32_t GetActiveParticleCount() const noexcept {
        return static_cast<uint32_t>(m_telemetry.activeParticleCount);
    }

    bool GetParticle(size_t index, KernelParticle* outParticle) const noexcept {
        if (!outParticle || index >= m_particles.size()) return false;
        *outParticle = m_particles[index];
        return true;
    }

    void Reset() noexcept {
        for (auto& pt : m_particles) {
            pt.alive = false;
        }
        m_telemetry.activeParticleCount = 0;
        m_spawnAccumulator = 0.0f;
    }

    bool RunStressTest(uint32_t iterations = 200) noexcept {
        auto start = std::chrono::high_resolution_clock::now();

        for (uint32_t frame = 0; frame < iterations; ++frame) {
            Update(0.016f);
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed = end - start;
        m_telemetry.avgParticleDispatchLatencyUs = (iterations > 0) ? (elapsed.count() / iterations) : 0.0;

        return (m_telemetry.avgParticleDispatchLatencyUs < 75.0);
    }

    [[nodiscard]] ParticleSubsystemTelemetry GetTelemetry() const noexcept {
        return m_telemetry;
    }
};

static KernelParticleGovernor g_particleGovernor;

} // anonymous namespace

bool InitializeParticleSubsystem(const KernelParticleEmitterParameters& params) noexcept {
    g_particleGovernor.Initialize(params);
    return true;
}

void ShutdownParticleSubsystem() noexcept {
    g_particleGovernor.Shutdown();
}

bool SetParticleParameters(const KernelParticleEmitterParameters& params) noexcept {
    g_particleGovernor.SetParams(params);
    return true;
}

bool GetParticleParameters(KernelParticleEmitterParameters* outParams) noexcept {
    if (!outParams) return false;
    *outParams = g_particleGovernor.GetParams();
    return true;
}

void TriggerParticleBurst(uint32_t count) noexcept {
    g_particleGovernor.TriggerBurst(count);
}

bool UpdateParticleSimulation(float deltaTimeSec) noexcept {
    return g_particleGovernor.Update(deltaTimeSec);
}

uint32_t GetActiveParticleCount() noexcept {
    return g_particleGovernor.GetActiveParticleCount();
}

bool GetParticle(size_t index, KernelParticle* outParticle) noexcept {
    return g_particleGovernor.GetParticle(index, outParticle);
}

void ResetParticleSimulation() noexcept {
    g_particleGovernor.Reset();
}

bool ExecuteParticleStressPass(uint32_t iterations) noexcept {
    return g_particleGovernor.RunStressTest(iterations);
}

bool GetParticleTelemetry(ParticleSubsystemTelemetry* outTelemetry) noexcept {
    if (!outTelemetry) return false;
    *outTelemetry = g_particleGovernor.GetTelemetry();
    return true;
}

bool VerifyHorizon353ParticleSubsystem() noexcept {
    // Pillar 1: Subsystem Parameters & Presets
    KernelParticleEmitterParameters p{};
    p.shape = KernelParticleEmitterShape::Sphere;
    p.blendMode = KernelParticleBlendMode::Additive;
    p.emissionRate = 600.0f;
    p.particleLifespanSec = 2.0f;
    p.emitterPosition = {0.0f, 0.0f, 100.0f};
    p.emitterSphereRadius = 25.0f;
    p.initialVelocity = {0.0f, 0.0f, 80.0f};
    p.floorCollisionEnabled = true;
    p.floorHeight = 0.0f;
    p.curlNoiseStrength = 20.0f;
    p.curlNoiseFrequency = 0.02f;

    g_particleGovernor.SetParams(p);
    KernelParticleEmitterParameters retrieved{};
    if (!GetParticleParameters(&retrieved)) return false;
    if (retrieved.shape != KernelParticleEmitterShape::Sphere) return false;
    if (std::abs(retrieved.emissionRate - 600.0f) > 1e-4f) return false;

    // Pillar 2: Emitter Geometry Sampling (Sphere bounds)
    g_particleGovernor.Reset();
    g_particleGovernor.TriggerBurst(64);
    g_particleGovernor.Update(0.001f);
    if (g_particleGovernor.GetActiveParticleCount() < 64) return false;

    for (size_t i = 0; i < 64; ++i) {
        KernelParticle pt{};
        if (g_particleGovernor.GetParticle(i, &pt) && pt.alive) {
            KernelParticleVector3 offset = pt.position - p.emitterPosition;
            if (offset.length() > p.emitterSphereRadius + 0.1f) return false;
        }
    }

    // Pillar 3: Divergence-Free Curl Noise Invariants (Div(v) = 0)
    KernelParticleVector3 probePoint{15.0f, -25.0f, 50.0f};
    float divergence = KernelCurlNoise3D::ComputeDivergence(probePoint, p.curlNoiseFrequency, p.curlNoiseStrength);
    if (std::abs(divergence) > 0.05f) return false;

    // Pillar 4: Particle Lifecycle & Normalized Aging
    g_particleGovernor.Reset();
    g_particleGovernor.TriggerBurst(10);
    g_particleGovernor.Update(0.016f);

    KernelParticle initialPt{};
    g_particleGovernor.GetParticle(0, &initialPt);
    if (!initialPt.alive || initialPt.age <= 0.0f) return false;

    // Advance past lifespan without continuous emission -> particles must die
    p.emissionRate = 0.0f;
    g_particleGovernor.SetParams(p);
    g_particleGovernor.Reset();
    g_particleGovernor.TriggerBurst(5);
    g_particleGovernor.Update(0.016f);
    if (g_particleGovernor.GetActiveParticleCount() != 5) return false;

    g_particleGovernor.Update(3.5f);
    if (g_particleGovernor.GetActiveParticleCount() != 0) return false;

    // Pillar 5: Floor Collision Response
    g_particleGovernor.Reset();
    p.shape = KernelParticleEmitterShape::Point;
    p.emissionRate = 0.0f;
    p.floorCollisionEnabled = true;
    p.floorHeight = 0.0f;
    p.emitterPosition = {0.0f, 0.0f, 20.0f};
    p.initialVelocity = {0.0f, 0.0f, -50.0f};
    p.velocitySpreadAngleDeg = 0.0f;
    p.speedMin = 50.0f;
    p.speedMax = 50.0f;
    p.gravity = {0.0f, 0.0f, -50.0f};
    p.curlNoiseStrength = 0.0f;
    p.collisionRestitution = 0.65f;
    g_particleGovernor.SetParams(p);

    g_particleGovernor.TriggerBurst(1);
    g_particleGovernor.Update(0.001f);

    bool collisionObserved = false;
    for (int i = 0; i < 60; ++i) {
        g_particleGovernor.Update(0.02f);
        KernelParticle falling{};
        if (g_particleGovernor.GetParticle(0, &falling)) {
            if (falling.position.z <= p.floorHeight + 0.1f && falling.velocity.z >= 0.0f) {
                collisionObserved = true;
                break;
            }
        }
    }
    if (!collisionObserved && g_particleGovernor.GetTelemetry().floorCollisions == 0) return false;

    // Pillar 6: High-Throughput Stress Test & Sub-75us Latency
    p.shape = KernelParticleEmitterShape::Sphere;
    p.emissionRate = 500.0f;
    p.particleLifespanSec = 1.5f;
    p.curlNoiseStrength = 20.0f;
    g_particleGovernor.SetParams(p);
    g_particleGovernor.Reset();

    if (!g_particleGovernor.RunStressTest(200)) return false;

    return true;
}

} // namespace micant::um::prismx_particles
