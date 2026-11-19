// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 340 (Suite 556)
// DirectComposition Stereo 3D Stereoscopic Display Swapchain, Per-Eye Visual
//   Layer Pairing & Holographic Convergence Pipeline (IDCompositionStereo3DVisual)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace micant::um::prismx_stereo3d {

enum class KernelStereoLayout : uint32_t {
    Mono               = 0,
    FramePacked        = 1,
    SideBySide         = 2,
    TopAndBottom       = 3,
    TemporalSequential = 4
};

struct KernelStereoViewport {
    uint32_t width{1920};
    uint32_t height{1080};
    uint64_t leftEyeVramAddress{0x00000008E0000000ULL};
    uint64_t rightEyeVramAddress{0x00000008F0000000ULL};
    float    interpupillaryDistanceMeters{0.064f};
    float    convergencePlaneMeters{2.0f};
    float    eyeDisparityPixels{12.0f};
};

struct KernelStereoCursor {
    float x{960.0f};
    float y{540.0f};
    float depthZ{1.5f};
    float leftCursorX{0.0f};
    float rightCursorX{0.0f};
};

struct StereoSubsystemTelemetry {
    uint64_t stereoPairsPresented{0};
    uint64_t eyeSeparationsAdjusted{0};
    uint64_t convergenceAdjustments{0};
    double   avgDualEyeDispatchLatencyUs{0.0};
};

bool InitializeHorizon340DirectCompositionStereo3DSubsystem();
bool VerifyHorizon340DirectCompositionStereo3DSubsystem();

} // namespace micant::um::prismx_stereo3d