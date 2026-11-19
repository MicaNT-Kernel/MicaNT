// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 338 (Suite 554)
// DirectComposition Alpha-Masked Layered Windows, Per-Pixel Translucency &
//   Hardware Composition Borders Pipeline (IDCompositionVisualAlpha)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace micant::um::prismx_alpha_layered {

enum class KernelAlphaMode : uint32_t {
    Unknown       = 0,
    Premultiplied = 1,
    Straight      = 2,
    Ignore        = 3,
    Inherit       = 4
};

enum class KernelHitTestCode : int32_t {
    TransparentPassthrough = -1,
    Nowhere                = 0,
    Client                 = 1,
    Caption                = 2,
    LeftBorder             = 10,
    RightBorder            = 11,
    TopBorder              = 12,
    BottomBorder           = 15
};

struct KernelWindowInsets {
    uint32_t left{8};
    uint32_t top{32};
    uint32_t right{8};
    uint32_t bottom{8};
};

struct KernelFrameBrushDesc {
    uint32_t argbTintColor{0xCC202228};
    float    backdropBlurRadius{30.0f};
    bool     enablePerPixelAlpha{true};
    float    alphaRejectionThreshold{0.05f};
};

struct KernelBorderStrokeDesc {
    float    thickness{1.5f};
    float    cornerRadius{12.0f};
    uint32_t argbBorderColor{0x806080B0};
    uint32_t argbAccentGlow{0x600078D4};
    float    shadowBlurRadius{24.0f};
    float    shadowOffsetY{8.0f};
    uint32_t argbShadowColor{0x40000000};
};

struct KernelBorderVertex {
    float x{0.0f};
    float y{0.0f};
    uint32_t color{0};
};

struct AlphaLayeredSubsystemTelemetry {
    uint64_t framesRendered{0};
    uint64_t borderTessellations{0};
    uint64_t hitTestsEvaluated{0};
    uint64_t passthroughHitsRejected{0};
    double   avgDispatchLatencyUs{0.0};
};

bool InitializeHorizon338DirectCompositionAlphaLayeredSubsystem();
bool VerifyHorizon338DirectCompositionAlphaLayeredSubsystem();

} // namespace micant::um::prismx_alpha_layered