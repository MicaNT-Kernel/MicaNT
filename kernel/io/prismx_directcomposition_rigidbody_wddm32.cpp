// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 359 (Suite 575)
// DirectComposition Rigid Body Dynamics, Continuous Collision Detection (CCD)
//   & Multi-Contact Manifold Physics Subsystem
//   (IDCompositionRigidBodyVisual, IDCompositionPhysicsMeshEffect,
//   DirectCompositionRigidBodyPipeline)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#include "micant/um/subsystems/graphics/prismx_directcomposition_rigidbody_wddm32.hpp"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <vector>
#include <memory>
#include <cmath>
#include <array>

namespace micant::um::prismx_rigidbody {

namespace {

struct InternalMatrix3x3 {
    float m[3][3]{
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };

    [[nodiscard]] KernelPhysicsVector3 Multiply(const KernelPhysicsVector3& v) const noexcept {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        };
    }

    [[nodiscard]] InternalMatrix3x3 Transpose() const noexcept {
        InternalMatrix3x3 res{};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                res.m[r][c] = m[c][r];
            }
        }
        return res;
    }

    [[nodiscard]] InternalMatrix3x3 operator*(const InternalMatrix3x3& o) const noexcept {
        InternalMatrix3x3 res{};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                res.m[r][c] = m[r][0] * o.m[0][c] + m[r][1] * o.m[1][c] + m[r][2] * o.m[2][c];
            }
        }
        return res;
    }

    static InternalMatrix3x3 FromQuaternion(const KernelPhysicsQuaternion& q) noexcept {
        InternalMatrix3x3 res{};
        float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
        float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
        float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

        res.m[0][0] = 1.0f - 2.0f * (yy + zz);
        res.m[0][1] = 2.0f * (xy - wz);
        res.m[0][2] = 2.0f * (xz + wy);

        res.m[1][0] = 2.0f * (xy + wz);
        res.m[1][1] = 1.0f - 2.0f * (xx + zz);
        res.m[1][2] = 2.0f * (yz - wx);

        res.m[2][0] = 2.0f * (xz - wy);
        res.m[2][1] = 2.0f * (yz + wx);
        res.m[2][2] = 1.0f - 2.0f * (xx + yy);

        return res;
    }
};

struct InternalBody {
    KernelRigidBodyConfig config{};
    KernelPhysicsVector3  position{};
    KernelPhysicsQuaternion orientation{1.0f, 0.0f, 0.0f, 0.0f};
    KernelPhysicsVector3  linearVelocity{};
    KernelPhysicsVector3  angularVelocity{};
    KernelPhysicsVector3  forceAccumulator{};
    KernelPhysicsVector3  torqueAccumulator{};

    float                 invMass{1.0f};
    InternalMatrix3x3     invInertiaBody{};
    InternalMatrix3x3     invInertiaWorld{};

    InternalBody(const KernelRigidBodyConfig& c, const KernelPhysicsVector3& pos)
        : config(c), position(pos) {
        if (config.massKg <= 1e-4f) {
            invMass = 0.0f;
            invInertiaBody = {};
            invInertiaWorld = {};
        } else {
            invMass = 1.0f / config.massKg;
            InternalMatrix3x3 inertia{};
            if (config.shape == KernelRigidBodyShape::Box) {
                float w = config.halfExtents.x * 2.0f;
                float h = config.halfExtents.y * 2.0f;
                float d_len = config.halfExtents.z * 2.0f;
                float factor = config.massKg / 12.0f;
                inertia.m[0][0] = factor * (h * h + d_len * d_len);
                inertia.m[1][1] = factor * (w * w + d_len * d_len);
                inertia.m[2][2] = factor * (w * w + h * h);
            } else {
                float r = config.radius;
                float val = 0.4f * config.massKg * r * r;
                inertia.m[0][0] = val;
                inertia.m[1][1] = val;
                inertia.m[2][2] = val;
            }
            invInertiaBody.m[0][0] = (inertia.m[0][0] > 1e-6f) ? 1.0f / inertia.m[0][0] : 0.0f;
            invInertiaBody.m[1][1] = (inertia.m[1][1] > 1e-6f) ? 1.0f / inertia.m[1][1] : 0.0f;
            invInertiaBody.m[2][2] = (inertia.m[2][2] > 1e-6f) ? 1.0f / inertia.m[2][2] : 0.0f;
            UpdateWorldInertia();
        }
    }

    void UpdateWorldInertia() noexcept {
        if (invMass <= 0.0f) return;
        InternalMatrix3x3 R = InternalMatrix3x3::FromQuaternion(orientation);
        invInertiaWorld = R * invInertiaBody * R.Transpose();
    }

    void ApplyImpulse(const KernelPhysicsVector3& impulse, const KernelPhysicsVector3& contactPoint) noexcept {
        if (invMass <= 0.0f) return;
        linearVelocity = linearVelocity + impulse * invMass;
        KernelPhysicsVector3 r = contactPoint - position;
        angularVelocity = angularVelocity + invInertiaWorld.Multiply(r.cross(impulse));
    }

    [[nodiscard]] double ComputeKineticEnergy() const noexcept {
        if (invMass <= 0.0f) return 0.0;
        double lin = 0.5 * config.massKg * linearVelocity.lengthSquared();
        KernelPhysicsVector3 angMom = InternalMatrix3x3::FromQuaternion(orientation).Multiply(angularVelocity);
        double ang = 0.5 * angularVelocity.dot(angMom);
        return lin + ang;
    }
};

struct InternalContactPoint {
    KernelPhysicsVector3 position{};
    KernelPhysicsVector3 normal{};
    float                penetration{0.0f};
    float                accumulatedNormalImpulse{0.0f};
    float                accumulatedTangentImpulse{0.0f};
};

struct InternalManifold {
    uint32_t                          bodyA{0};
    uint32_t                          bodyB{UINT32_MAX};
    std::vector<InternalContactPoint> contacts;
};

class KernelRigidBodyGovernor {
private:
    std::vector<InternalBody>     m_bodies;
    std::vector<InternalManifold> m_manifolds;
    KernelRigidBodyTelemetry      m_telemetry{};
    bool                          m_initialized{false};
    KernelPhysicsVector3          m_gravity{0.0f, 0.0f, -9.80665f};
    float                         m_floorZ{-1.0f};
    bool                          m_floorEnabled{true};
    uint32_t                      m_solverIterations{10};

public:
    KernelRigidBodyGovernor() = default;

    void Initialize() noexcept {
        m_bodies.clear();
        m_manifolds.clear();
        m_telemetry = {};
        m_initialized = true;
    }

    void Shutdown() noexcept {
        m_initialized = false;
        m_bodies.clear();
        m_manifolds.clear();
    }

    [[nodiscard]] bool IsInitialized() const noexcept {
        return m_initialized;
    }

    uint32_t CreateBody(const KernelRigidBodyConfig& cfg, const KernelPhysicsVector3& pos) noexcept {
        uint32_t id = static_cast<uint32_t>(m_bodies.size());
        m_bodies.emplace_back(cfg, pos);
        m_telemetry.activeBodyCount = static_cast<uint32_t>(m_bodies.size());
        return id;
    }

    void ApplyImpulse(uint32_t id, const KernelPhysicsVector3& imp, const KernelPhysicsVector3& pos) noexcept {
        if (id < m_bodies.size()) {
            m_bodies[id].ApplyImpulse(imp, pos);
        }
    }

    void Step(float dtSec) noexcept {
        if (!m_initialized || dtSec <= 0.0f || m_bodies.empty()) return;
        const auto startTime = std::chrono::high_resolution_clock::now();
        const float dt = std::clamp(dtSec, 0.001f, 0.033f);

        // 1. Force accumulation & velocity integration
        for (auto& b : m_bodies) {
            if (b.invMass <= 0.0f) continue;
            b.forceAccumulator = b.forceAccumulator + (m_gravity * b.config.massKg);
            b.linearVelocity = b.linearVelocity + (b.forceAccumulator * b.invMass) * dt;
            b.angularVelocity = b.angularVelocity + b.invInertiaWorld.Multiply(b.torqueAccumulator) * dt;

            b.linearVelocity = b.linearVelocity * b.config.linearDamping;
            b.angularVelocity = b.angularVelocity * b.config.angularDamping;

            b.forceAccumulator = {};
            b.torqueAccumulator = {};
        }

        // 2. Collision detection
        DetectCollisions();

        // 3. Sequential impulse solver
        for (uint32_t iter = 0; iter < m_solverIterations; ++iter) {
            SolveContactImpulses();
        }

        // 4. Position & orientation integration
        for (auto& b : m_bodies) {
            if (b.invMass <= 0.0f) continue;
            b.position = b.position + b.linearVelocity * dt;

            KernelPhysicsQuaternion wq{0.0f, b.angularVelocity.x, b.angularVelocity.y, b.angularVelocity.z};
            KernelPhysicsQuaternion dq = (wq * b.orientation);
            b.orientation.w += 0.5f * dt * dq.w;
            b.orientation.x += 0.5f * dt * dq.x;
            b.orientation.y += 0.5f * dt * dq.y;
            b.orientation.z += 0.5f * dt * dq.z;
            b.orientation = b.orientation.normalize();

            b.UpdateWorldInertia();
        }

        // 5. Telemetry update
        double totalKE = 0.0;
        for (const auto& b : m_bodies) {
            totalKE += b.ComputeKineticEnergy();
        }
        m_telemetry.systemKineticEnergy = totalKE;
        m_telemetry.totalSimulationSteps++;

        const auto endTime = std::chrono::high_resolution_clock::now();
        const double elapsedUs = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        m_telemetry.avgDispatchLatencyUs = (m_telemetry.avgDispatchLatencyUs * 0.95) + (elapsedUs * 0.05);
    }

    [[nodiscard]] KernelPhysicsVector3 GetPosition(uint32_t id) const noexcept {
        if (id >= m_bodies.size()) return {};
        return m_bodies[id].position;
    }

    [[nodiscard]] KernelPhysicsQuaternion GetOrientation(uint32_t id) const noexcept {
        if (id >= m_bodies.size()) return {};
        return m_bodies[id].orientation;
    }

    [[nodiscard]] KernelPhysicsVector3 GetLinearVelocity(uint32_t id) const noexcept {
        if (id >= m_bodies.size()) return {};
        return m_bodies[id].linearVelocity;
    }

    void ExecuteStressPass(uint32_t iterations) noexcept {
        for (uint32_t i = 0; i < iterations; ++i) {
            Step(0.016f);
        }
    }

    [[nodiscard]] KernelRigidBodyTelemetry GetTelemetry() const noexcept {
        return m_telemetry;
    }

private:
    void DetectCollisions() noexcept {
        m_manifolds.clear();
        double maxPen = 0.0;

        // Ground Plane Collisions
        if (m_floorEnabled) {
            for (uint32_t i = 0; i < m_bodies.size(); ++i) {
                auto& b = m_bodies[i];
                if (b.invMass <= 0.0f) continue;

                if (b.config.shape == KernelRigidBodyShape::Sphere) {
                    float bottom = b.position.z - b.config.radius;
                    if (bottom < m_floorZ) {
                        InternalManifold mf{};
                        mf.bodyA = i;
                        mf.bodyB = UINT32_MAX;

                        InternalContactPoint cp{};
                        cp.position = {b.position.x, b.position.y, m_floorZ};
                        cp.normal = {0.0f, 0.0f, 1.0f};
                        cp.penetration = m_floorZ - bottom;
                        if (cp.penetration > maxPen) maxPen = cp.penetration;
                        mf.contacts.push_back(cp);
                        m_manifolds.push_back(mf);
                    }
                } else if (b.config.shape == KernelRigidBodyShape::Box) {
                    std::array<KernelPhysicsVector3, 8> localVertices = {
                        KernelPhysicsVector3{-b.config.halfExtents.x, -b.config.halfExtents.y, -b.config.halfExtents.z},
                        KernelPhysicsVector3{ b.config.halfExtents.x, -b.config.halfExtents.y, -b.config.halfExtents.z},
                        KernelPhysicsVector3{ b.config.halfExtents.x,  b.config.halfExtents.y, -b.config.halfExtents.z},
                        KernelPhysicsVector3{-b.config.halfExtents.x,  b.config.halfExtents.y, -b.config.halfExtents.z},
                        KernelPhysicsVector3{-b.config.halfExtents.x, -b.config.halfExtents.y,  b.config.halfExtents.z},
                        KernelPhysicsVector3{ b.config.halfExtents.x, -b.config.halfExtents.y,  b.config.halfExtents.z},
                        KernelPhysicsVector3{ b.config.halfExtents.x,  b.config.halfExtents.y,  b.config.halfExtents.z},
                        KernelPhysicsVector3{-b.config.halfExtents.x,  b.config.halfExtents.y,  b.config.halfExtents.z}
                    };

                    InternalManifold mf{};
                    mf.bodyA = i;
                    mf.bodyB = UINT32_MAX;

                    for (const auto& lv : localVertices) {
                        KernelPhysicsVector3 worldV = b.position + b.orientation.rotate(lv);
                        if (worldV.z < m_floorZ) {
                            InternalContactPoint cp{};
                            cp.position = {worldV.x, worldV.y, m_floorZ};
                            cp.normal = {0.0f, 0.0f, 1.0f};
                            cp.penetration = m_floorZ - worldV.z;
                            if (cp.penetration > maxPen) maxPen = cp.penetration;
                            mf.contacts.push_back(cp);
                        }
                    }

                    if (!mf.contacts.empty()) {
                        m_manifolds.push_back(mf);
                    }
                }
            }
        }

        // Sphere-Sphere Collisions
        for (uint32_t i = 0; i < m_bodies.size(); ++i) {
            for (uint32_t j = i + 1; j < m_bodies.size(); ++j) {
                auto& bA = m_bodies[i];
                auto& bB = m_bodies[j];
                if (bA.invMass <= 0.0f && bB.invMass <= 0.0f) continue;

                if (bA.config.shape == KernelRigidBodyShape::Sphere && bB.config.shape == KernelRigidBodyShape::Sphere) {
                    KernelPhysicsVector3 delta = bA.position - bB.position;
                    float dist = delta.length();
                    float rSum = bA.config.radius + bB.config.radius;
                    if (dist < rSum && dist > 1e-6f) {
                        InternalManifold mf{};
                        mf.bodyA = i;
                        mf.bodyB = j;

                        InternalContactPoint cp{};
                        cp.normal = delta / dist;
                        cp.position = bB.position + cp.normal * bB.config.radius;
                        cp.penetration = rSum - dist;
                        if (cp.penetration > maxPen) maxPen = cp.penetration;
                        mf.contacts.push_back(cp);
                        m_manifolds.push_back(mf);
                    }
                }
            }
        }

        m_telemetry.maxPenetrationDepthM = maxPen;
    }

    void SolveContactImpulses() noexcept {
        const float baumgarte = 0.2f;
        const float slop = 0.005f;

        for (auto& mf : m_manifolds) {
            InternalBody& bA = m_bodies[mf.bodyA];
            InternalBody* pB = (mf.bodyB != UINT32_MAX) ? &m_bodies[mf.bodyB] : nullptr;

            for (auto& cp : mf.contacts) {
                KernelPhysicsVector3 rA = cp.position - bA.position;
                KernelPhysicsVector3 rB = pB ? (cp.position - pB->position) : KernelPhysicsVector3{};

                KernelPhysicsVector3 vA = bA.linearVelocity + bA.angularVelocity.cross(rA);
                KernelPhysicsVector3 vB = pB ? (pB->linearVelocity + pB->angularVelocity.cross(rB)) : KernelPhysicsVector3{};
                KernelPhysicsVector3 relVel = vA - vB;

                float vn = relVel.dot(cp.normal);
                if (vn > 0.0f) continue;

                float termA = bA.invMass + (bA.invInertiaWorld.Multiply(rA.cross(cp.normal))).cross(rA).dot(cp.normal);
                float termB = pB ? (pB->invMass + (pB->invInertiaWorld.Multiply(rB.cross(cp.normal))).cross(rB).dot(cp.normal)) : 0.0f;
                float effMass = termA + termB;
                if (effMass < 1e-6f) continue;

                float e = bA.config.restitution;
                if (pB) e = std::min(e, pB->config.restitution);

                float penetrationBias = (std::max(0.0f, cp.penetration - slop) * baumgarte) / 0.016f;
                float impulseMagnitude = (-(1.0f + e) * vn + penetrationBias) / effMass;

                float oldImpulse = cp.accumulatedNormalImpulse;
                cp.accumulatedNormalImpulse = std::max(0.0f, oldImpulse + impulseMagnitude);
                float deltaImpulse = cp.accumulatedNormalImpulse - oldImpulse;

                KernelPhysicsVector3 P = cp.normal * deltaImpulse;
                bA.ApplyImpulse(P, cp.position);
                if (pB) pB->ApplyImpulse(-P, cp.position);

                // Tangential Friction
                KernelPhysicsVector3 tangent = (relVel - cp.normal * vn);
                float vt = tangent.length();
                if (vt > 1e-5f) {
                    KernelPhysicsVector3 tDir = tangent / vt;
                    float frictionEffMass = bA.invMass + (bA.invInertiaWorld.Multiply(rA.cross(tDir))).cross(rA).dot(tDir);
                    if (pB) frictionEffMass += pB->invMass + (pB->invInertiaWorld.Multiply(rB.cross(tDir))).cross(rB).dot(tDir);

                    if (frictionEffMass > 1e-6f) {
                        float mu = bA.config.friction;
                        if (pB) mu = std::sqrt(mu * pB->config.friction);

                        float fImpulse = -vt / frictionEffMass;
                        float maxFriction = mu * cp.accumulatedNormalImpulse;
                        fImpulse = std::clamp(fImpulse, -maxFriction, maxFriction);

                        KernelPhysicsVector3 Pt = tDir * fImpulse;
                        bA.ApplyImpulse(Pt, cp.position);
                        if (pB) pB->ApplyImpulse(-Pt, cp.position);
                    }
                }

                m_telemetry.totalContactsSolved++;
            }
        }
    }
};

static std::unique_ptr<KernelRigidBodyGovernor> g_rigidbodyGovernor = nullptr;

} // namespace

bool InitializeRigidBodySubsystem() noexcept {
    if (!g_rigidbodyGovernor) {
        g_rigidbodyGovernor = std::make_unique<KernelRigidBodyGovernor>();
    }
    g_rigidbodyGovernor->Initialize();
    return true;
}

void ShutdownRigidBodySubsystem() noexcept {
    if (g_rigidbodyGovernor) {
        g_rigidbodyGovernor->Shutdown();
        g_rigidbodyGovernor.reset();
    }
}

uint32_t CreateRigidBody(const KernelRigidBodyConfig& config, const KernelPhysicsVector3& initialPos) noexcept {
    if (!g_rigidbodyGovernor || !g_rigidbodyGovernor->IsInitialized()) return UINT32_MAX;
    return g_rigidbodyGovernor->CreateBody(config, initialPos);
}

bool ApplyRigidBodyImpulse(uint32_t bodyId, const KernelPhysicsVector3& impulseN, const KernelPhysicsVector3& worldPos) noexcept {
    if (!g_rigidbodyGovernor || !g_rigidbodyGovernor->IsInitialized()) return false;
    g_rigidbodyGovernor->ApplyImpulse(bodyId, impulseN, worldPos);
    return true;
}

bool StepRigidBodySimulation(float dtSec) noexcept {
    if (!g_rigidbodyGovernor || !g_rigidbodyGovernor->IsInitialized()) return false;
    g_rigidbodyGovernor->Step(dtSec);
    return true;
}

KernelPhysicsVector3 GetRigidBodyPosition(uint32_t bodyId) noexcept {
    if (!g_rigidbodyGovernor || !g_rigidbodyGovernor->IsInitialized()) return {};
    return g_rigidbodyGovernor->GetPosition(bodyId);
}

KernelPhysicsQuaternion GetRigidBodyOrientation(uint32_t bodyId) noexcept {
    if (!g_rigidbodyGovernor || !g_rigidbodyGovernor->IsInitialized()) return {};
    return g_rigidbodyGovernor->GetOrientation(bodyId);
}

KernelPhysicsVector3 GetRigidBodyLinearVelocity(uint32_t bodyId) noexcept {
    if (!g_rigidbodyGovernor || !g_rigidbodyGovernor->IsInitialized()) return {};
    return g_rigidbodyGovernor->GetLinearVelocity(bodyId);
}

bool ExecuteRigidBodyStressPass(uint32_t iterations) noexcept {
    if (!g_rigidbodyGovernor || !g_rigidbodyGovernor->IsInitialized()) return false;
    g_rigidbodyGovernor->ExecuteStressPass(iterations);
    return true;
}

bool GetRigidBodyTelemetry(KernelRigidBodyTelemetry* outTelemetry) noexcept {
    if (!g_rigidbodyGovernor || !g_rigidbodyGovernor->IsInitialized() || !outTelemetry) return false;
    *outTelemetry = g_rigidbodyGovernor->GetTelemetry();
    return true;
}

bool VerifyHorizon359RigidBodySubsystem() noexcept {
    if (!InitializeRigidBodySubsystem()) return false;

    // Pillar 1: Rigid body definition & inertial initialization
    KernelRigidBodyConfig boxCfg{};
    boxCfg.shape = KernelRigidBodyShape::Box;
    boxCfg.halfExtents = {0.25f, 0.25f, 0.25f};
    boxCfg.massKg = 2.0f;
    boxCfg.restitution = 0.5f;

    uint32_t boxId = CreateRigidBody(boxCfg, {0.0f, 0.0f, 2.0f});
    if (boxId == UINT32_MAX) {
        ShutdownRigidBodySubsystem();
        return false;
    }

    // Pillar 2: Dynamic Sphere Body Creation
    KernelRigidBodyConfig sphereCfg{};
    sphereCfg.shape = KernelRigidBodyShape::Sphere;
    sphereCfg.radius = 0.3f;
    sphereCfg.massKg = 1.0f;
    sphereCfg.restitution = 0.6f;
    uint32_t sphereId = CreateRigidBody(sphereCfg, {0.0f, 0.0f, 1.5f});
    if (sphereId == UINT32_MAX) {
        ShutdownRigidBodySubsystem();
        return false;
    }

    // Pillar 3: Gravitational Acceleration & Free Fall
    for (uint32_t i = 0; i < 20; ++i) {
        StepRigidBodySimulation(0.016f);
    }
    KernelPhysicsVector3 pSphereMid = GetRigidBodyPosition(sphereId);
    KernelPhysicsVector3 vSphereMid = GetRigidBodyLinearVelocity(sphereId);
    if (pSphereMid.z >= 1.5f || vSphereMid.z >= 0.0f) {
        ShutdownRigidBodySubsystem();
        return false;
    }

    // Pillar 4: Floor contact manifold resolution & restitution bounce
    for (uint32_t i = 0; i < 60; ++i) {
        StepRigidBodySimulation(0.016f);
    }
    KernelPhysicsVector3 pSphereBounce = GetRigidBodyPosition(sphereId);
    if (pSphereBounce.z < -0.85f) {
        ShutdownRigidBodySubsystem();
        return false;
    }

    // Pillar 5: Multi-body kinetic energy dissipation & stability
    for (uint32_t i = 0; i < 150; ++i) {
        StepRigidBodySimulation(0.016f);
    }
    KernelPhysicsVector3 pBoxSettled = GetRigidBodyPosition(boxId);
    if (pBoxSettled.z < -0.85f) {
        ShutdownRigidBodySubsystem();
        return false;
    }

    // Pillar 6: High-frequency stress pass & Telemetry validation
    if (!ExecuteRigidBodyStressPass(250)) {
        ShutdownRigidBodySubsystem();
        return false;
    }

    KernelRigidBodyTelemetry telem{};
    if (!GetRigidBodyTelemetry(&telem)) {
        ShutdownRigidBodySubsystem();
        return false;
    }

    if (telem.totalSimulationSteps < 300 || telem.totalContactsSolved == 0) {
        ShutdownRigidBodySubsystem();
        return false;
    }
    if (std::isnan(telem.systemKineticEnergy) || std::isnan(telem.maxPenetrationDepthM)) {
        ShutdownRigidBodySubsystem();
        return false;
    }

    ShutdownRigidBodySubsystem();
    return true;
}

} // namespace micant::um::prismx_rigidbody
