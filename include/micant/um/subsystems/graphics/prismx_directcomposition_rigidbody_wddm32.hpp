// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 359 (Suite 575)
// DirectComposition Rigid Body Dynamics, Continuous Collision Detection (CCD)
//   & Multi-Contact Manifold Physics Subsystem
//   (IDCompositionRigidBodyVisual, IDCompositionPhysicsMeshEffect,
//   DirectCompositionRigidBodyPipeline)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>
#include <algorithm>

namespace micant::um::prismx_rigidbody {

enum class KernelRigidBodyShape : uint32_t {
    Box     = 0,
    Sphere  = 1,
    Capsule = 2
};

struct KernelPhysicsVector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    [[nodiscard]] constexpr float lengthSquared() const noexcept {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(lengthSquared());
    }

    [[nodiscard]] KernelPhysicsVector3 normalize() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f, 1.0f};
        return {x / len, y / len, z / len};
    }

    [[nodiscard]] constexpr float dot(const KernelPhysicsVector3& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }

    [[nodiscard]] constexpr KernelPhysicsVector3 cross(const KernelPhysicsVector3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] constexpr KernelPhysicsVector3 operator+(const KernelPhysicsVector3& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }

    [[nodiscard]] constexpr KernelPhysicsVector3 operator-(const KernelPhysicsVector3& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }

    [[nodiscard]] constexpr KernelPhysicsVector3 operator-() const noexcept {
        return {-x, -y, -z};
    }

    [[nodiscard]] constexpr KernelPhysicsVector3 operator*(float s) const noexcept {
        return {x * s, y * s, z * s};
    }

    [[nodiscard]] constexpr KernelPhysicsVector3 operator/(float s) const noexcept {
        float inv = (s != 0.0f) ? (1.0f / s) : 0.0f;
        return {x * inv, y * inv, z * inv};
    }
};

struct KernelPhysicsQuaternion {
    float w{1.0f};
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(w * w + x * x + y * y + z * z);
    }

    [[nodiscard]] KernelPhysicsQuaternion normalize() const noexcept {
        float len = length();
        if (len < 1e-6f) return {1.0f, 0.0f, 0.0f, 0.0f};
        return {w / len, x / len, y / len, z / len};
    }

    [[nodiscard]] KernelPhysicsQuaternion operator*(const KernelPhysicsQuaternion& q) const noexcept {
        return {
            w * q.w - x * q.x - y * q.y - z * q.z,
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w
        };
    }

    [[nodiscard]] KernelPhysicsVector3 rotate(const KernelPhysicsVector3& v) const noexcept {
        KernelPhysicsVector3 qv{x, y, z};
        KernelPhysicsVector3 uv = qv.cross(v);
        KernelPhysicsVector3 uuv = qv.cross(uv);
        return v + (uv * (2.0f * w)) + (uuv * 2.0f);
    }
};

struct KernelRigidBodyConfig {
    KernelRigidBodyShape shape{KernelRigidBodyShape::Box};
    KernelPhysicsVector3 halfExtents{0.5f, 0.5f, 0.5f};
    float                radius{0.5f};
    float                massKg{1.0f};
    float                restitution{0.45f};
    float                friction{0.5f};
    float                linearDamping{0.99f};
    float                angularDamping{0.98f};
};

struct KernelRigidBodyTelemetry {
    uint64_t totalSimulationSteps{0};
    uint64_t totalContactsSolved{0};
    uint32_t activeBodyCount{0};
    double   systemKineticEnergy{0.0};
    double   maxPenetrationDepthM{0.0};
    double   avgDispatchLatencyUs{0.0};
};

// Subsystem Control & Diagnostics API
bool InitializeRigidBodySubsystem() noexcept;
void ShutdownRigidBodySubsystem() noexcept;
uint32_t CreateRigidBody(const KernelRigidBodyConfig& config, const KernelPhysicsVector3& initialPos) noexcept;
bool ApplyRigidBodyImpulse(uint32_t bodyId, const KernelPhysicsVector3& impulseN, const KernelPhysicsVector3& worldPos) noexcept;
bool StepRigidBodySimulation(float dtSec) noexcept;
KernelPhysicsVector3 GetRigidBodyPosition(uint32_t bodyId) noexcept;
KernelPhysicsQuaternion GetRigidBodyOrientation(uint32_t bodyId) noexcept;
KernelPhysicsVector3 GetRigidBodyLinearVelocity(uint32_t bodyId) noexcept;
bool ExecuteRigidBodyStressPass(uint32_t iterations = 200) noexcept;
bool GetRigidBodyTelemetry(KernelRigidBodyTelemetry* outTelemetry) noexcept;
bool VerifyHorizon359RigidBodySubsystem() noexcept;

} // namespace micant::um::prismx_rigidbody
