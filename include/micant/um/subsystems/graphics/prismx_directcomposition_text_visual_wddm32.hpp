// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 336 (Suite 552)
// DirectComposition Hardware Accelerated Text Rendering & DirectWrite
//   Typography Visual Pipeline (IDCompositionTextVisual)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace micant::um::prismx_text_visual {

enum class KernelFontWeight : uint32_t {
    Thin       = 100,
    Light      = 300,
    Normal     = 400,
    Medium     = 500,
    SemiBold   = 600,
    Bold       = 700,
    Black      = 900
};

struct TextLayoutMetrics {
    uint32_t glyphCount{0};
    float    layoutWidth{0.0f};
    float    layoutHeight{0.0f};
    uint64_t glyphAtlasAddress{0x00000008A0000000ULL};
    bool     subpixelClearType{true};
    bool     distanceFieldEnabled{true};
};

struct TextSubsystemTelemetry {
    uint64_t layoutsCalculated{0};
    uint64_t glyphsTessellated{0};
    uint64_t fontAnimationsDriven{0};
    double   avgLayoutLatencyUs{0.0};
};

bool InitializeHorizon336DirectCompositionTextVisualSubsystem();
bool VerifyHorizon336DirectCompositionTextVisualSubsystem();

} // namespace micant::um::prismx_text_visual
