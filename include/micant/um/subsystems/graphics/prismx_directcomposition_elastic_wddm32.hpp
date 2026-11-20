// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 357 (Suite 573)
// DirectComposition Dynamic Sub-Surface Elastic Soft-Body Deformation, Mass-Spring
//   Lattice & Corotational Finite Element Method (FEM) Jelly Subsystem
//   (IDCompositionDeformableMeshEffect, IDCompositionSoftBodyVisual,
//   DirectCompositionElasticPipeline)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>
#include <algorithm>

namespace micant::um::prismx_elastic {

enum class KernelElasticModel : uint32_t {
    MassSpringLattice    = 0, // Structural + shear + bending mass-spring lattice
    CorotationalFEM       = 1, // Corotational linear elasticity with polar decomposition
    HydrostaticJellyBulge = 2  // Coupled volume-preserving hydrostatic rubber/jelly
};

enum class KernelElasticMaterial : uint32_t {
    Rubber       = 0, // High stiffness, medium damping, low volume compressibility
    Jelly        = 1, // Low stiffness, low damping, extreme harmonic oscillatory squish
    Foam         = 2, // Low stiffness, high damping, compressible porous structure
    StiffSilicon = 3  // High stiffness, high damping, fast settling
};

constexpr float PI = 3.14159265358979323846f;

struct KernelElasticVector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    [[nodiscard]] constexpr float lengthSquared() const noexcept {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(lengthSquared());
    }

    [[nodiscard]] KernelElasticVector3 normalize() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f, 1.0f};
        return {x / len, y / len, z / len};
    }

    [[nodiscard]] constexpr float dot(const KernelElasticVector3& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }

    [[nodiscard]] constexpr KernelElasticVector3 cross(const KernelElasticVector3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] constexpr KernelElasticVector3 operator+(const KernelElasticVector3& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }

    [[nodiscard]] constexpr KernelElasticVector3 operator-(const KernelElasticVector3& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }

    [[nodiscard]] constexpr KernelElasticVector3 operator-() const noexcept {
        return {-x, -y, -z};
    }

    [[nodiscard]] constexpr KernelElasticVector3 operator*(float s) const noexcept {
        return {x * s, y * s, z * s};
    }

    [[nodiscard]] constexpr KernelElasticVector3 operator/(float s) const noexcept {
        float inv = (s != 0.0f) ? (1.0f / s) : 0.0f;
        return {x * inv, y * inv, z * inv};
    }
};

struct KernelElasticDeformationParameters {
    KernelElasticModel    model{KernelElasticModel::HydrostaticJellyBulge};
    KernelElasticMaterial material{KernelElasticMaterial::Jelly};
    uint32_t gridDimX{3};                // 3x3x3 tetrahedral lattice
    uint32_t gridDimY{3};
    uint32_t gridDimZ{3};
    float    nodeSpacingM{0.05f};        // Rest distance between adjacent nodes (meters)
    float    particleMassKg{0.05f};      // Mass per lattice node (kg)
    float    youngsModulus{500.0f};      // Material stiffness (N/m)
    float    dampingRatio{2.0f};         // Viscoelastic damping coefficient
    float    volumeStiffness{300.0f};    // Hydrostatic pressure volume preservation penalty
    float    gravityMPerSec2{-9.80665f}; // Gravity force along Z-axis
    float    floorZ{-0.25f};             // Rigid collision boundary plane (meters)
    float    restitution{0.35f};         // Elastic collision restitution bounce factor
};

struct ElasticSubsystemTelemetry {
    uint64_t totalSimulationSteps{0};
    uint64_t totalSpringEvaluations{0};
    uint64_t totalFloorCollisions{0};
    double   currentVolumeM3{0.0};
    double   restVolumeM3{0.0};
    double   totalDeformationEnergy{0.0};
    double   avgDispatchLatencyUs{0.0};
};

// Subsystem Control & Diagnostics API
bool InitializeElasticSubsystem(const KernelElasticDeformationParameters& params = {}) noexcept;
void ShutdownElasticSubsystem() noexcept;
bool SetElasticParameters(const KernelElasticDeformationParameters& params) noexcept;
bool GetElasticParameters(KernelElasticDeformationParameters* outParams) noexcept;
bool ResetElasticToRest() noexcept;

bool ApplyElasticImpulse(const KernelElasticVector3& origin, float radiusM, const KernelElasticVector3& forceN) noexcept;
bool StepElasticSimulation(float dtSec) noexcept;
KernelElasticVector3 GetElasticNodePosition(uint32_t index) noexcept;
KernelElasticVector3 ComputeElasticSurfaceNormal(uint32_t node0, uint32_t node1, uint32_t node2) noexcept;
double ComputeElasticVolume() noexcept;

bool ExecuteElasticStressPass(uint32_t iterations = 200) noexcept;
bool GetElasticTelemetry(ElasticSubsystemTelemetry* outTelemetry) noexcept;
bool VerifyHorizon357ElasticSubsystem() noexcept;

} // namespace micant::um::prismx_elastic
