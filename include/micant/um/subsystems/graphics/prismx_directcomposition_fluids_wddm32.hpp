// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 356 (Suite 572)
// DirectComposition Dynamic Real-Time Sub-Surface Fluid Simulation, Navier-Stokes
//   Eulerian Grid & Shallow Water Equations (SWE) Ripple Subsystem
//   (IDCompositionFluidSimulationEffect, IDCompositionFluidSurfaceVisual,
//   DirectCompositionFluidPipeline)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>
#include <algorithm>

namespace micant::um::prismx_fluids {

enum class KernelFluidModel : uint32_t {
    ShallowWaterHeightfield = 0, // 2D hyperbolic wave equation with damping
    EulerianAdvection2D     = 1, // Incompressible Navier-Stokes advection-projection
    HybridCapillaryRipple   = 2  // Coupled shallow water + surface tension ripples
};

enum class KernelFluidBoundary : uint32_t {
    ReflectiveNeumann  = 0, // Zero normal derivative (rigid wall reflection)
    AbsorbingDirichlet = 1, // Dampened borders (open water radiation)
    PeriodicToroidal   = 2  // Wrap-around toroidal boundaries
};

constexpr float PI = 3.14159265358979323846f;

struct KernelFluidVector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    [[nodiscard]] constexpr float lengthSquared() const noexcept {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(lengthSquared());
    }

    [[nodiscard]] KernelFluidVector3 normalize() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f, 1.0f};
        return {x / len, y / len, z / len};
    }

    [[nodiscard]] constexpr float dot(const KernelFluidVector3& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }

    [[nodiscard]] constexpr KernelFluidVector3 cross(const KernelFluidVector3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] constexpr KernelFluidVector3 operator+(const KernelFluidVector3& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }

    [[nodiscard]] constexpr KernelFluidVector3 operator-(const KernelFluidVector3& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }

    [[nodiscard]] constexpr KernelFluidVector3 operator-() const noexcept {
        return {-x, -y, -z};
    }

    [[nodiscard]] constexpr KernelFluidVector3 operator*(float s) const noexcept {
        return {x * s, y * s, z * s};
    }

    [[nodiscard]] constexpr KernelFluidVector3 operator/(float s) const noexcept {
        float inv = (s != 0.0f) ? (1.0f / s) : 0.0f;
        return {x * inv, y * inv, z * inv};
    }
};

struct KernelFluidSimulationParameters {
    KernelFluidModel    model{KernelFluidModel::ShallowWaterHeightfield};
    KernelFluidBoundary boundary{KernelFluidBoundary::ReflectiveNeumann};
    uint32_t gridWidth{32};             // Simulation grid horizontal resolution
    uint32_t gridHeight{32};            // Simulation grid vertical resolution
    float    cellSizeM{0.02f};          // Spatial distance between adjacent grid nodes (meters)
    float    waveSpeedMPerSec{1.2f};    // Wave phase propagation speed c (m/s)
    float    dampingFactor{0.015f};     // Wave energy dissipation factor (gamma)
    float    viscosity{0.001f};         // Kinematic fluid viscosity (nu)
    float    surfaceTension{0.0728f};   // Capillary surface tension coeff (N/m, water ~0.073)
    float    gravityMPerSec2{9.80665f}; // Gravitational acceleration
    float    impulseRadiusM{0.08f};     // Radius of pointer/droplet impulse disturbances
    float    impulseStrength{1.5f};     // Magnitude of impulse displacement
};

struct FluidSubsystemTelemetry {
    uint64_t totalSimulationSteps{0};
    uint64_t totalImpulsesInjected{0};
    uint64_t totalLaplacianEvaluations{0};
    double   maxSurfaceDisplacementM{0.0};
    double   totalKineticEnergy{0.0};
    double   avgDispatchLatencyUs{0.0};
};

// Subsystem Control & Diagnostics API
bool InitializeFluidSubsystem(const KernelFluidSimulationParameters& params = {}) noexcept;
void ShutdownFluidSubsystem() noexcept;
bool SetFluidParameters(const KernelFluidSimulationParameters& params) noexcept;
bool GetFluidParameters(KernelFluidSimulationParameters* outParams) noexcept;
bool ResetFluidState() noexcept;

bool InjectFluidImpulse(float normX, float normY, float amplitude) noexcept;
bool StepFluidSimulation(float dtSec) noexcept;
float SampleFluidHeight(float normX, float normY) noexcept;
KernelFluidVector3 SampleFluidNormal(float normX, float normY) noexcept;

bool ExecuteFluidStressPass(uint32_t iterations = 200) noexcept;
bool GetFluidTelemetry(FluidSubsystemTelemetry* outTelemetry) noexcept;
bool VerifyHorizon356FluidSubsystem() noexcept;

} // namespace micant::um::prismx_fluids
