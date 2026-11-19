// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 341 (Suite 557)
// DirectComposition Hardware Super-Resolution Spatial Upscaling, Sharpening
//   & FSR/XeSS Surface Integration Pipeline (IDCompositionSuperResolutionVisual)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace micant::um::prismx_super_resolution {

enum class KernelScalingMode : uint32_t {
    NearestNeighbor          = 0,
    Bilinear                 = 1,
    Bicubic                  = 2,
    Lanczos3                 = 3,
    EdgeAdaptiveSpatial      = 4,
    ContrastAdaptiveSharpen  = 5
};

struct KernelSuperResolutionDesc {
    uint32_t nativeTargetWidth{3840};
    uint32_t nativeTargetHeight{2160};
    float    renderScale{0.67f};
    float    sharpness{0.75f};
    KernelScalingMode scalingMode{KernelScalingMode::EdgeAdaptiveSpatial};
    bool     enableRingingSuppression{true};
};

struct SuperResolutionSubsystemTelemetry {
    uint64_t framesUpscaled{0};
    uint64_t scalingModeChanges{0};
    uint64_t dynamicScaleAdjustments{0};
    double   estimatedVramBandwidthSavingsPct{0.0};
    double   avgUpscaleDispatchLatencyUs{0.0};
};

bool InitializeHorizon341DirectCompositionSuperResolutionSubsystem();
bool VerifyHorizon341DirectCompositionSuperResolutionSubsystem();

} // namespace micant::um::prismx_super_resolution