// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 345 (Suite 561)
// DirectComposition Dynamic Holographic Motion Blur, Velocity Vector
//   Reconstruction & Directional Object Smear Pipeline (IDCompositionMotionBlurEffect)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>

namespace micant::um::prismx_motion_blur {

enum class KernelMotionBlurQuality : uint32_t {
    Low    = 0,
    Medium = 1,
    High   = 2,
    Ultra  = 3
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

struct KernelVelocityVector {
    float vx{0.0f};
    float vy{0.0f};

    [[nodiscard]] float magnitude() const noexcept {
        return std::sqrt(vx * vx + vy * vy);
    }
};

struct KernelMotionBlurParams {
    float                   shutterAngleDeg{180.0f};
    uint32_t                sampleCount{8};
    float                   maxBlurRadius{32.0f};
    float                   velocityScale{1.0f};
    KernelMotionBlurQuality quality{KernelMotionBlurQuality::Ultra};
};

struct KernelVelocityTile {
    uint32_t tileX{0};
    uint32_t tileY{0};
    float    maxVelocity{0.0f};
    bool     isStatic{true};
};

struct MotionBlurSubsystemTelemetry {
    uint64_t totalBlurPasses{0};
    uint64_t velocityReconstructions{0};
    uint64_t staticTilesCulled{0};
    uint64_t activeBlurSamplesComputed{0};
    double   avgMotionBlurDispatchLatencyUs{0.0};
};

bool InitializeHorizon345DirectCompositionMotionBlurSubsystem();
bool VerifyHorizon345DirectCompositionMotionBlurSubsystem();

} // namespace micant::um::prismx_motion_blur
