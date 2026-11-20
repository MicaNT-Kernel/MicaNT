// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 353 (Suite 569)
// DirectComposition Dynamic GPU Particle System, Curl Noise Fluid Flow &
//   Holographic Energy Field Subsystem (IDCompositionParticleEmitterEffect,
//   IDCompositionParticleSystemVisual, DirectCompositionParticlePipeline)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>
#include <algorithm>

namespace micant::um::prismx_particles {

enum class KernelParticleEmitterShape : uint32_t {
    Point  = 0,
    Box    = 1,
    Sphere = 2,
    Disc   = 3,
    Line   = 4
};

enum class KernelParticleBlendMode : uint32_t {
    Additive      = 0,
    AlphaBlend    = 1,
    Premultiplied = 2,
    Subtractive   = 3
};

enum class KernelParticleSimulationSpace : uint32_t {
    Local = 0,
    World = 1
};

struct KernelParticleVector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    [[nodiscard]] constexpr float lengthSquared() const noexcept {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(lengthSquared());
    }

    [[nodiscard]] KernelParticleVector3 normalize() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f, 1.0f};
        return {x / len, y / len, z / len};
    }

    [[nodiscard]] constexpr float dot(const KernelParticleVector3& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }

    [[nodiscard]] constexpr KernelParticleVector3 cross(const KernelParticleVector3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] constexpr KernelParticleVector3 operator+(const KernelParticleVector3& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }

    [[nodiscard]] constexpr KernelParticleVector3 operator-(const KernelParticleVector3& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }

    [[nodiscard]] constexpr KernelParticleVector3 operator-() const noexcept {
        return {-x, -y, -z};
    }

    [[nodiscard]] constexpr KernelParticleVector3 operator*(float s) const noexcept {
        return {x * s, y * s, z * s};
    }

    [[nodiscard]] constexpr KernelParticleVector3 operator/(float s) const noexcept {
        float inv = (s != 0.0f) ? (1.0f / s) : 0.0f;
        return {x * inv, y * inv, z * inv};
    }
};

struct KernelParticleColorRGBA {
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};
    float a{1.0f};

    [[nodiscard]] constexpr KernelParticleColorRGBA operator+(const KernelParticleColorRGBA& o) const noexcept {
        return {r + o.r, g + o.g, b + o.b, a + o.a};
    }

    [[nodiscard]] constexpr KernelParticleColorRGBA operator*(float s) const noexcept {
        return {r * s, g * s, b * s, a * s};
    }

    [[nodiscard]] constexpr KernelParticleColorRGBA clamp(float minVal = 0.0f, float maxVal = 1.0f) const noexcept {
        return {
            std::clamp(r, minVal, maxVal),
            std::clamp(g, minVal, maxVal),
            std::clamp(b, minVal, maxVal),
            std::clamp(a, minVal, maxVal)
        };
    }

    [[nodiscard]] static constexpr KernelParticleColorRGBA Lerp(const KernelParticleColorRGBA& c1, const KernelParticleColorRGBA& c2, float t) noexcept {
        float factor = std::clamp(t, 0.0f, 1.0f);
        return {
            c1.r + (c2.r - c1.r) * factor,
            c1.g + (c2.g - c1.g) * factor,
            c1.b + (c2.b - c1.b) * factor,
            c1.a + (c2.a - c1.a) * factor
        };
    }
};

struct KernelParticle {
    KernelParticleVector3   position{0.0f, 0.0f, 0.0f};
    KernelParticleVector3   velocity{0.0f, 0.0f, 0.0f};
    KernelParticleVector3   acceleration{0.0f, 0.0f, 0.0f};
    KernelParticleColorRGBA color{1.0f, 1.0f, 1.0f, 1.0f};
    KernelParticleColorRGBA startColor{1.0f, 1.0f, 1.0f, 1.0f};
    KernelParticleColorRGBA endColor{1.0f, 1.0f, 1.0f, 0.0f};
    float                   size{4.0f};
    float                   startSize{4.0f};
    float                   endSize{0.5f};
    float                   age{0.0f};
    float                   lifespan{2.0f};
    float                   mass{1.0f};
    bool                    alive{false};
};

struct KernelParticleEmitterParameters {
    KernelParticleEmitterShape    shape{KernelParticleEmitterShape::Sphere};
    KernelParticleBlendMode       blendMode{KernelParticleBlendMode::Additive};
    KernelParticleSimulationSpace simulationSpace{KernelParticleSimulationSpace::World};
    float                         emissionRate{500.0f};
    float                         particleLifespanSec{2.0f};
    KernelParticleVector3         emitterPosition{0.0f, 0.0f, 100.0f};
    KernelParticleVector3         emitterBoxExtents{50.0f, 50.0f, 20.0f};
    float                         emitterSphereRadius{30.0f};
    KernelParticleVector3         initialVelocity{0.0f, 0.0f, 60.0f};
    float                         velocitySpreadAngleDeg{35.0f};
    float                         speedMin{40.0f};
    float                         speedMax{80.0f};
    KernelParticleVector3         gravity{0.0f, 0.0f, -98.1f};
    float                         dragCoefficient{0.04f};
    float                         curlNoiseStrength{25.0f};
    float                         curlNoiseFrequency{0.015f};
    bool                          floorCollisionEnabled{true};
    float                         floorHeight{0.0f};
    float                         collisionRestitution{0.65f};
    float                         collisionFriction{0.90f};
    KernelParticleColorRGBA       startColor{0.2f, 0.6f, 1.0f, 1.0f};
    KernelParticleColorRGBA       endColor{0.9f, 0.1f, 0.5f, 0.0f};
    float                         startSize{6.0f};
    float                         endSize{1.0f};
    uint32_t                      maxActiveParticles{2048};
};

struct ParticleSubsystemTelemetry {
    uint64_t totalSimulationSteps{0};
    uint64_t activeParticleCount{0};
    uint64_t particlesSpawned{0};
    uint64_t particlesDied{0};
    uint64_t floorCollisions{0};
    uint64_t curlEvaluations{0};
    double   avgParticleDispatchLatencyUs{0.0};
};

// Subsystem Control & Diagnostics API
bool InitializeParticleSubsystem(const KernelParticleEmitterParameters& params = {}) noexcept;
void ShutdownParticleSubsystem() noexcept;
bool SetParticleParameters(const KernelParticleEmitterParameters& params) noexcept;
bool GetParticleParameters(KernelParticleEmitterParameters* outParams) noexcept;

void TriggerParticleBurst(uint32_t count) noexcept;
bool UpdateParticleSimulation(float deltaTimeSec) noexcept;
uint32_t GetActiveParticleCount() noexcept;
bool GetParticle(size_t index, KernelParticle* outParticle) noexcept;
void ResetParticleSimulation() noexcept;

bool ExecuteParticleStressPass(uint32_t iterations = 200) noexcept;
bool GetParticleTelemetry(ParticleSubsystemTelemetry* outTelemetry) noexcept;
bool VerifyHorizon353ParticleSubsystem() noexcept;

} // namespace micant::um::prismx_particles
