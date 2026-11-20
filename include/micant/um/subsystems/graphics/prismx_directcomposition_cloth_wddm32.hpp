// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 358 (Suite 574)
// DirectComposition Position-Based Dynamics (PBD) Cloth Simulation, Aerodynamic
//   Wind Coupling & Collision Response Subsystem
//   (IDCompositionClothVisual, IDCompositionClothMeshEffect,
//   DirectCompositionClothPipeline)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>
#include <algorithm>

namespace micant::um::prismx_cloth {

enum class KernelClothModel : uint32_t {
    PBDCloth           = 0, // Position-Based Dynamics distance and bending constraints
    MassSpringCloth    = 1, // Traditional mass-spring-damper cloth lattice
    AerodynamicBanner  = 2  // Coupled aero-drag and vortex-induced fluttering sheet
};

enum class KernelClothMaterial : uint32_t {
    Silk        = 0, // Ultralight, flexible, low bending resistance, high flutter
    Cotton      = 1, // Medium weight, moderate stretch resistance, gentle drape
    HeavyCanvas = 2, // Heavy mass, rigid bending constraints, high wind drag
    Kevlar      = 3  // Inextensible, extreme structural stiffness, minimal elasticity
};

enum class KernelClothPinMode : uint32_t {
    TopCorners  = 0, // Pin top-left and top-right vertices (curtain / banner)
    FourCorners = 1, // Pin all four corner vertices (trampoline / canopy)
    LeftEdge    = 2, // Pin entire left edge (flag on pole)
    NoPins      = 3  // Free-falling cloth dropped on an obstacle
};

constexpr float PI = 3.14159265358979323846f;

struct KernelClothVector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    [[nodiscard]] constexpr float lengthSquared() const noexcept {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(lengthSquared());
    }

    [[nodiscard]] KernelClothVector3 normalize() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f, 1.0f};
        return {x / len, y / len, z / len};
    }

    [[nodiscard]] constexpr float dot(const KernelClothVector3& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }

    [[nodiscard]] constexpr KernelClothVector3 cross(const KernelClothVector3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] constexpr KernelClothVector3 operator+(const KernelClothVector3& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }

    [[nodiscard]] constexpr KernelClothVector3 operator-(const KernelClothVector3& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }

    [[nodiscard]] constexpr KernelClothVector3 operator-() const noexcept {
        return {-x, -y, -z};
    }

    [[nodiscard]] constexpr KernelClothVector3 operator*(float s) const noexcept {
        return {x * s, y * s, z * s};
    }

    [[nodiscard]] constexpr KernelClothVector3 operator/(float s) const noexcept {
        float inv = (s != 0.0f) ? (1.0f / s) : 0.0f;
        return {x * inv, y * inv, z * inv};
    }
};

struct KernelClothSphereObstacle {
    KernelClothVector3 center{0.0f, 0.0f, -0.3f};
    float              radius{0.18f};
    float              friction{0.25f};
    bool               enabled{true};
};

struct KernelClothParameters {
    KernelClothModel          model{KernelClothModel::PBDCloth};
    KernelClothMaterial       material{KernelClothMaterial::Cotton};
    KernelClothPinMode        pinMode{KernelClothPinMode::TopCorners};
    uint32_t                  gridDimX{16};              // Grid nodes along X
    uint32_t                  gridDimY{16};              // Grid nodes along Y
    float                     particleSpacingM{0.04f};   // Spacing between nodes (meters)
    float                     particleMassKg{0.02f};     // Mass per node (kg)
    float                     stiffness{0.92f};          // PBD distance compliance [0, 1]
    float                     bendingStiffness{0.45f};   // PBD bending resistance [0, 1]
    float                     damping{0.985f};           // Velocity air drag retention factor
    KernelClothVector3        windVelocity{0.0f, 2.5f, 0.0f}; // Ambient wind vector (m/s)
    float                     gravityMPerSec2{-9.80665f}; // Gravity along Z-axis (m/s^2)
    uint32_t                  solverIterations{4};       // PBD constraint projection iterations
    KernelClothSphereObstacle obstacle{};                // Collision obstacle
};

struct ClothSubsystemTelemetry {
    uint64_t totalSimulationSteps{0};
    uint64_t totalConstraintsSolved{0};
    uint64_t totalCollisionsDetected{0};
    double   maxStrain{0.0};
    double   avgDispatchLatencyUs{0.0};
};

// Subsystem Control & Diagnostics API
bool InitializeClothSubsystem(const KernelClothParameters& params = {}) noexcept;
void ShutdownClothSubsystem() noexcept;
bool SetClothParameters(const KernelClothParameters& params) noexcept;
bool GetClothParameters(KernelClothParameters* outParams) noexcept;
bool ResetClothToRest() noexcept;

bool ApplyClothImpulse(const KernelClothVector3& origin, float radiusM, const KernelClothVector3& forceN) noexcept;
bool StepClothSimulation(float dtSec) noexcept;
KernelClothVector3 GetClothParticlePosition(uint32_t x, uint32_t y) noexcept;
KernelClothVector3 ComputeClothSurfaceNormal(uint32_t x, uint32_t y) noexcept;
KernelClothVector3 ComputeClothAerodynamicLift(const KernelClothVector3& windVec) noexcept;

bool ExecuteClothStressPass(uint32_t iterations = 200) noexcept;
bool GetClothTelemetry(ClothSubsystemTelemetry* outTelemetry) noexcept;
bool VerifyHorizon358ClothSubsystem() noexcept;

} // namespace micant::um::prismx_cloth
