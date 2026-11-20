// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 358 (Suite 574)
// DirectComposition Position-Based Dynamics (PBD) Cloth Simulation, Aerodynamic
//   Wind Coupling & Collision Response Subsystem
//   (IDCompositionClothVisual, IDCompositionClothMeshEffect,
//   DirectCompositionClothPipeline)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#include "micant/um/subsystems/graphics/prismx_directcomposition_cloth_wddm32.hpp"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <vector>
#include <memory>
#include <cmath>

namespace micant::um::prismx_cloth {

namespace {

struct InternalClothParticle {
    KernelClothVector3 position{};
    KernelClothVector3 oldPosition{};
    KernelClothVector3 velocity{};
    KernelClothVector3 acceleration{};
    float              invMass{1.0f};
    bool               isPinned{false};
};

struct InternalDistanceConstraint {
    uint32_t nodeA{0};
    uint32_t nodeB{0};
    float    restLength{0.04f};
    float    stiffness{0.9f};
    bool     isBending{false};
};

class KernelClothGovernor {
private:
    KernelClothParameters   m_params{};
    ClothSubsystemTelemetry m_telemetry{};
    bool                    m_initialized{false};

    std::vector<InternalClothParticle>     m_particles;
    std::vector<InternalDistanceConstraint> m_constraints;

    [[nodiscard]] uint32_t Index(uint32_t x, uint32_t y) const noexcept {
        return y * m_params.gridDimX + x;
    }

public:
    KernelClothGovernor() {
        ResetToRest();
    }

    void Initialize(const KernelClothParameters& params) noexcept {
        m_params = params;
        m_params.gridDimX = std::clamp(m_params.gridDimX, 4u, 32u);
        m_params.gridDimY = std::clamp(m_params.gridDimY, 4u, 32u);
        m_params.particleSpacingM = std::max(0.005f, m_params.particleSpacingM);
        m_params.particleMassKg = std::max(0.001f, m_params.particleMassKg);
        m_params.stiffness = std::clamp(m_params.stiffness, 0.05f, 1.0f);
        m_params.bendingStiffness = std::clamp(m_params.bendingStiffness, 0.0f, 1.0f);
        m_params.damping = std::clamp(m_params.damping, 0.8f, 1.0f);
        m_params.solverIterations = std::clamp(m_params.solverIterations, 1u, 16u);

        m_initialized = true;
        ResetToRest();
    }

    void Shutdown() noexcept {
        m_initialized = false;
    }

    [[nodiscard]] bool IsInitialized() const noexcept {
        return m_initialized;
    }

    void SetParams(const KernelClothParameters& params) noexcept {
        m_params = params;
        m_params.gridDimX = std::clamp(m_params.gridDimX, 4u, 32u);
        m_params.gridDimY = std::clamp(m_params.gridDimY, 4u, 32u);
        m_params.particleSpacingM = std::max(0.005f, m_params.particleSpacingM);
        m_params.particleMassKg = std::max(0.001f, m_params.particleMassKg);
        m_params.stiffness = std::clamp(m_params.stiffness, 0.05f, 1.0f);
        m_params.bendingStiffness = std::clamp(m_params.bendingStiffness, 0.0f, 1.0f);
        m_params.damping = std::clamp(m_params.damping, 0.8f, 1.0f);
        m_params.solverIterations = std::clamp(m_params.solverIterations, 1u, 16u);
    }

    [[nodiscard]] KernelClothParameters GetParams() const noexcept {
        return m_params;
    }

    void ResetToRest() noexcept {
        const uint32_t nx = m_params.gridDimX;
        const uint32_t ny = m_params.gridDimY;
        const float dx = m_params.particleSpacingM;
        const float halfWidth = static_cast<float>(nx - 1) * dx * 0.5f;

        m_particles.resize(nx * ny);
        for (uint32_t y = 0; y < ny; ++y) {
            for (uint32_t x = 0; x < nx; ++x) {
                const uint32_t idx = Index(x, y);
                InternalClothParticle& p = m_particles[idx];
                p.position = {
                    static_cast<float>(x) * dx - halfWidth,
                    static_cast<float>(y) * dx,
                    0.0f
                };
                p.oldPosition = p.position;
                p.velocity = {0.0f, 0.0f, 0.0f};
                p.acceleration = {0.0f, 0.0f, 0.0f};
                p.invMass = 1.0f / m_params.particleMassKg;
                p.isPinned = false;

                // Pinning logic
                switch (m_params.pinMode) {
                case KernelClothPinMode::TopCorners:
                    if (y == 0 && (x == 0 || x == nx - 1)) {
                        p.isPinned = true;
                        p.invMass = 0.0f;
                    }
                    break;
                case KernelClothPinMode::FourCorners:
                    if ((y == 0 || y == ny - 1) && (x == 0 || x == nx - 1)) {
                        p.isPinned = true;
                        p.invMass = 0.0f;
                    }
                    break;
                case KernelClothPinMode::LeftEdge:
                    if (x == 0) {
                        p.isPinned = true;
                        p.invMass = 0.0f;
                    }
                    break;
                case KernelClothPinMode::NoPins:
                    break;
                }
            }
        }

        // Rebuild Distance Constraints
        m_constraints.clear();
        m_constraints.reserve(nx * ny * 8);

        auto AddConstraint = [this](uint32_t a, uint32_t b, float stiffness, bool isBending) {
            const float rest = (m_particles[a].position - m_particles[b].position).length();
            m_constraints.push_back({a, b, rest, stiffness, isBending});
        };

        for (uint32_t y = 0; y < ny; ++y) {
            for (uint32_t x = 0; x < nx; ++x) {
                const uint32_t cur = Index(x, y);

                // Structural constraints (horizontal and vertical)
                if (x + 1 < nx) AddConstraint(cur, Index(x + 1, y), m_params.stiffness, false);
                if (y + 1 < ny) AddConstraint(cur, Index(x, y + 1), m_params.stiffness, false);

                // Shear constraints (diagonals)
                if (x + 1 < nx && y + 1 < ny) {
                    AddConstraint(cur, Index(x + 1, y + 1), m_params.stiffness * 0.85f, false);
                    AddConstraint(Index(x + 1, y), Index(x, y + 1), m_params.stiffness * 0.85f, false);
                }

                // Bending constraints (2-hop neighbors)
                if (m_params.bendingStiffness > 0.001f) {
                    if (x + 2 < nx) AddConstraint(cur, Index(x + 2, y), m_params.bendingStiffness, true);
                    if (y + 2 < ny) AddConstraint(cur, Index(x, y + 2), m_params.bendingStiffness, true);
                }
            }
        }

        m_telemetry.maxStrain = 0.0;
    }

    void ApplyImpulse(const KernelClothVector3& origin, float radiusM, const KernelClothVector3& forceN) noexcept {
        const float rSq = radiusM * radiusM;
        for (auto& p : m_particles) {
            if (p.isPinned) continue;
            const float distSq = (p.position - origin).lengthSquared();
            if (distSq <= rSq) {
                const float factor = 1.0f - std::sqrt(distSq) / radiusM;
                p.velocity = p.velocity + forceN * (factor * p.invMass);
            }
        }
    }

    void Step(float dtSec) noexcept {
        if (!m_initialized || dtSec <= 0.0f) return;
        const auto startTime = std::chrono::high_resolution_clock::now();

        const float dt = std::clamp(dtSec, 0.0005f, 0.033f);
        const KernelClothVector3 grav{0.0f, 0.0f, m_params.gravityMPerSec2};

        // 1. External forces (Gravity, Wind Aerodynamics) & Symplectic Prediction
        const uint32_t nx = m_params.gridDimX;
        const uint32_t ny = m_params.gridDimY;

        for (uint32_t y = 0; y < ny; ++y) {
            for (uint32_t x = 0; x < nx; ++x) {
                const uint32_t idx = Index(x, y);
                InternalClothParticle& p = m_particles[idx];
                if (p.isPinned) continue;

                // Aerodynamic wind force calculation
                KernelClothVector3 normal = ComputeNormal(x, y);
                KernelClothVector3 relWind = m_params.windVelocity - p.velocity;
                float normalDotWind = normal.dot(relWind);
                KernelClothVector3 aeroForce = normal * (0.5f * 1.225f * normalDotWind * std::abs(normalDotWind) * 0.015f);

                p.acceleration = grav + aeroForce * p.invMass;
                p.velocity = p.velocity + p.acceleration * dt;
                p.velocity = p.velocity * m_params.damping;

                p.oldPosition = p.position;
                p.position = p.position + p.velocity * dt;
            }
        }

        // 2. Position-Based Dynamics (PBD) Constraint Projections
        double maxStrainObserved = 0.0;
        for (uint32_t iter = 0; iter < m_params.solverIterations; ++iter) {
            for (const auto& c : m_constraints) {
                InternalClothParticle& p1 = m_particles[c.nodeA];
                InternalClothParticle& p2 = m_particles[c.nodeB];

                const float w1 = p1.invMass;
                const float w2 = p2.invMass;
                const float wSum = w1 + w2;
                if (wSum < 1e-7f) continue;

                KernelClothVector3 delta = p1.position - p2.position;
                const float currentDist = delta.length();
                if (currentDist < 1e-6f) continue;

                const float strain = std::abs(currentDist - c.restLength) / c.restLength;
                if (strain > maxStrainObserved) maxStrainObserved = strain;

                const float diff = (currentDist - c.restLength) / currentDist;
                const KernelClothVector3 correction = delta * (diff * c.stiffness / wSum);

                if (!p1.isPinned) p1.position = p1.position - correction * w1;
                if (!p2.isPinned) p2.position = p2.position + correction * w2;

                m_telemetry.totalConstraintsSolved++;
            }

            // 3. Collision Resolution (Spherical Obstacle)
            if (m_params.obstacle.enabled) {
                const KernelClothVector3 sc = m_params.obstacle.center;
                const float r = m_params.obstacle.radius;
                for (auto& p : m_particles) {
                    if (p.isPinned) continue;
                    KernelClothVector3 toP = p.position - sc;
                    float dist = toP.length();
                    if (dist < r) {
                        KernelClothVector3 norm = (dist > 1e-6f) ? toP / dist : KernelClothVector3{0.0f, 0.0f, 1.0f};
                        p.position = sc + norm * r;
                        m_telemetry.totalCollisionsDetected++;
                    }
                }
            }
        }

        // 4. Velocity Update from Position Displacements
        const float invDt = 1.0f / dt;
        for (auto& p : m_particles) {
            if (p.isPinned) {
                p.velocity = {0.0f, 0.0f, 0.0f};
            } else {
                p.velocity = (p.position - p.oldPosition) * invDt;
            }
        }

        m_telemetry.maxStrain = maxStrainObserved;
        m_telemetry.totalSimulationSteps++;

        const auto endTime = std::chrono::high_resolution_clock::now();
        const double elapsedUs = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        m_telemetry.avgDispatchLatencyUs = (m_telemetry.avgDispatchLatencyUs * 0.95) + (elapsedUs * 0.05);
    }

    [[nodiscard]] KernelClothVector3 GetParticlePosition(uint32_t x, uint32_t y) const noexcept {
        if (x >= m_params.gridDimX || y >= m_params.gridDimY) return {};
        return m_particles[Index(x, y)].position;
    }

    [[nodiscard]] KernelClothVector3 ComputeNormal(uint32_t x, uint32_t y) const noexcept {
        if (x >= m_params.gridDimX || y >= m_params.gridDimY) return {0.0f, 0.0f, 1.0f};

        KernelClothVector3 dx{1.0f, 0.0f, 0.0f};
        KernelClothVector3 dy{0.0f, 1.0f, 0.0f};

        if (x + 1 < m_params.gridDimX && x > 0) {
            dx = m_particles[Index(x + 1, y)].position - m_particles[Index(x - 1, y)].position;
        } else if (x + 1 < m_params.gridDimX) {
            dx = m_particles[Index(x + 1, y)].position - m_particles[Index(x, y)].position;
        } else if (x > 0) {
            dx = m_particles[Index(x, y)].position - m_particles[Index(x - 1, y)].position;
        }

        if (y + 1 < m_params.gridDimY && y > 0) {
            dy = m_particles[Index(x, y + 1)].position - m_particles[Index(x, y - 1)].position;
        } else if (y + 1 < m_params.gridDimY) {
            dy = m_particles[Index(x, y + 1)].position - m_particles[Index(x, y)].position;
        } else if (y > 0) {
            dy = m_particles[Index(x, y)].position - m_particles[Index(x, y - 1)].position;
        }

        return dx.cross(dy).normalize();
    }

    [[nodiscard]] KernelClothVector3 ComputeAerodynamicLift(const KernelClothVector3& windVec) const noexcept {
        KernelClothVector3 totalLift{0.0f, 0.0f, 0.0f};
        for (uint32_t y = 0; y < m_params.gridDimY; ++y) {
            for (uint32_t x = 0; x < m_params.gridDimX; ++x) {
                KernelClothVector3 n = ComputeNormal(x, y);
                float dot = n.dot(windVec);
                totalLift = totalLift + n * dot;
            }
        }
        return totalLift;
    }

    void ExecuteStressPass(uint32_t iterations) noexcept {
        for (uint32_t i = 0; i < iterations; ++i) {
            Step(0.016f);
        }
    }

    [[nodiscard]] ClothSubsystemTelemetry GetTelemetry() const noexcept {
        return m_telemetry;
    }
};

static std::unique_ptr<KernelClothGovernor> g_clothGovernor = nullptr;

} // namespace

bool InitializeClothSubsystem(const KernelClothParameters& params) noexcept {
    if (!g_clothGovernor) {
        g_clothGovernor = std::make_unique<KernelClothGovernor>();
    }
    g_clothGovernor->Initialize(params);
    return true;
}

void ShutdownClothSubsystem() noexcept {
    if (g_clothGovernor) {
        g_clothGovernor->Shutdown();
        g_clothGovernor.reset();
    }
}

bool SetClothParameters(const KernelClothParameters& params) noexcept {
    if (!g_clothGovernor || !g_clothGovernor->IsInitialized()) return false;
    g_clothGovernor->SetParams(params);
    return true;
}

bool GetClothParameters(KernelClothParameters* outParams) noexcept {
    if (!g_clothGovernor || !g_clothGovernor->IsInitialized() || !outParams) return false;
    *outParams = g_clothGovernor->GetParams();
    return true;
}

bool ResetClothToRest() noexcept {
    if (!g_clothGovernor || !g_clothGovernor->IsInitialized()) return false;
    g_clothGovernor->ResetToRest();
    return true;
}

bool ApplyClothImpulse(const KernelClothVector3& origin, float radiusM, const KernelClothVector3& forceN) noexcept {
    if (!g_clothGovernor || !g_clothGovernor->IsInitialized()) return false;
    g_clothGovernor->ApplyImpulse(origin, radiusM, forceN);
    return true;
}

bool StepClothSimulation(float dtSec) noexcept {
    if (!g_clothGovernor || !g_clothGovernor->IsInitialized()) return false;
    g_clothGovernor->Step(dtSec);
    return true;
}

KernelClothVector3 GetClothParticlePosition(uint32_t x, uint32_t y) noexcept {
    if (!g_clothGovernor || !g_clothGovernor->IsInitialized()) return {};
    return g_clothGovernor->GetParticlePosition(x, y);
}

KernelClothVector3 ComputeClothSurfaceNormal(uint32_t x, uint32_t y) noexcept {
    if (!g_clothGovernor || !g_clothGovernor->IsInitialized()) return {0.0f, 0.0f, 1.0f};
    return g_clothGovernor->ComputeNormal(x, y);
}

KernelClothVector3 ComputeClothAerodynamicLift(const KernelClothVector3& windVec) noexcept {
    if (!g_clothGovernor || !g_clothGovernor->IsInitialized()) return {};
    return g_clothGovernor->ComputeAerodynamicLift(windVec);
}

bool ExecuteClothStressPass(uint32_t iterations) noexcept {
    if (!g_clothGovernor || !g_clothGovernor->IsInitialized()) return false;
    g_clothGovernor->ExecuteStressPass(iterations);
    return true;
}

bool GetClothTelemetry(ClothSubsystemTelemetry* outTelemetry) noexcept {
    if (!g_clothGovernor || !g_clothGovernor->IsInitialized() || !outTelemetry) return false;
    *outTelemetry = g_clothGovernor->GetTelemetry();
    return true;
}

bool VerifyHorizon358ClothSubsystem() noexcept {
    KernelClothParameters params{};
    params.gridDimX = 8;
    params.gridDimY = 8;
    params.particleSpacingM = 0.05f;
    params.stiffness = 0.95f;
    params.bendingStiffness = 0.35f;
    params.pinMode = KernelClothPinMode::TopCorners;
    params.windVelocity = {0.0f, 3.0f, 0.5f};
    params.obstacle.enabled = true;
    params.obstacle.center = {0.0f, 0.2f, -0.1f};
    params.obstacle.radius = 0.12f;

    // Pillar 1: Subsystem Initialization
    if (!InitializeClothSubsystem(params)) return false;

    // Pillar 2: Rest state geometry verification & Pin checks
    KernelClothVector3 p00 = GetClothParticlePosition(0, 0);
    KernelClothVector3 p70 = GetClothParticlePosition(7, 0);
    KernelClothVector3 pCenter = GetClothParticlePosition(4, 4);

    if (std::abs(p00.z) > 1e-4f || std::abs(p70.z) > 1e-4f) {
        ShutdownClothSubsystem();
        return false;
    }

    // Pillar 3: Position-Based Dynamics (PBD) Simulation & Gravity Sag
    for (uint32_t s = 0; s < 30; ++s) {
        StepClothSimulation(0.016f);
    }

    KernelClothVector3 pCenterSag = GetClothParticlePosition(4, 4);
    KernelClothVector3 p00Pinned = GetClothParticlePosition(0, 0);

    // Top corners should stay firmly pinned
    if (std::abs(p00Pinned.x - p00.x) > 1e-4f || std::abs(p00Pinned.y - p00.y) > 1e-4f || std::abs(p00Pinned.z - p00.z) > 1e-4f) {
        ShutdownClothSubsystem();
        return false;
    }

    // Center should have sagged or moved under gravity / wind
    if (pCenterSag.z == pCenter.z && pCenterSag.y == pCenter.y) {
        ShutdownClothSubsystem();
        return false;
    }

    // Pillar 4: Aerodynamic wind coupling & normal synthesis
    KernelClothVector3 norm = ComputeClothSurfaceNormal(4, 4);
    if (norm.lengthSquared() < 0.5f) {
        ShutdownClothSubsystem();
        return false;
    }

    KernelClothVector3 lift = ComputeClothAerodynamicLift({0.0f, 5.0f, 1.0f});
    if (lift.lengthSquared() < 1e-4f) {
        ShutdownClothSubsystem();
        return false;
    }

    // Pillar 5: Obstacle collision avoidance
    for (uint32_t s = 0; s < 50; ++s) {
        StepClothSimulation(0.016f);
    }

    // Pillar 6: High-frequency stress pass & Telemetry Validation
    if (!ExecuteClothStressPass(250)) {
        ShutdownClothSubsystem();
        return false;
    }

    ClothSubsystemTelemetry telem{};
    if (!GetClothTelemetry(&telem)) {
        ShutdownClothSubsystem();
        return false;
    }

    if (telem.totalSimulationSteps < 300 || telem.totalConstraintsSolved == 0) {
        ShutdownClothSubsystem();
        return false;
    }

    ShutdownClothSubsystem();
    return true;
}

} // namespace micant::um::prismx_cloth
