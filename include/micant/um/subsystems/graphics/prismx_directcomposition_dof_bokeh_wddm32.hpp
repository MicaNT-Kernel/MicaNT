// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 346 (Suite 562)
// DirectComposition Dynamic Holographic Depth-of-Field (DoF), Circle of
//   Confusion (CoC) & Bokeh Hexagonal Aperture Pipeline (IDCompositionDepthOfFieldEffect)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>

namespace micant::um::prismx_dof_bokeh {

enum class KernelBokehShape : uint32_t {
    Circular   = 0,
    Hexagonal  = 1,
    Octagonal  = 2,
    Anamorphic = 3
};

struct KernelVector2 {
    float x{0.0f};
    float y{0.0f};

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(x * x + y * y);
    }

    [[nodiscard]] KernelVector2 normalize() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f};
        return {x / len, y / len};
    }
};

struct KernelCameraOpticalParams {
    float            focalDistanceMm{500.0f};
    float            focalLengthMm{50.0f};
    float            fStop{1.8f};
    uint32_t         apertureBlades{6};
    float            maxCocRadiusPx{24.0f};
    KernelBokehShape bokehShape{KernelBokehShape::Hexagonal};
};

struct KernelBokehSample {
    KernelVector2 offset{0.0f, 0.0f};
    float         weight{1.0f};
};

struct DepthOfFieldSubsystemTelemetry {
    uint64_t totalDofPasses{0};
    uint64_t cocEvaluations{0};
    uint64_t nearFieldSeparations{0};
    uint64_t farFieldSeparations{0};
    double   avgDofDispatchLatencyUs{0.0};
};

bool InitializeHorizon346DirectCompositionDepthOfFieldSubsystem();
bool VerifyHorizon346DirectCompositionDepthOfFieldSubsystem();

} // namespace micant::um::prismx_dof_bokeh
