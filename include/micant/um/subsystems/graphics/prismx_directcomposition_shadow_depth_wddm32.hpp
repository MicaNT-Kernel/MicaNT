// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 343 (Suite 559)
// DirectComposition Dynamic Holographic Depth Perception, Parallax Occlusion
//   Mapping & Per-Layer Z-Depth Shadow Casting Pipeline (IDCompositionShadowVisual)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>

namespace micant::um::prismx_shadow_depth {

enum class KernelShadowQuality : uint32_t {
    Low    = 0, // 1-tap hard
    Medium = 1, // 4-tap PCF
    High   = 2, // 16-tap PCF
    Ultra  = 3  // Contact-Hardening PCSS Soft Penumbra
};

namespace ElevationPreset {
    constexpr float Card    = 2.0f;
    constexpr float Button  = 4.0f;
    constexpr float Flyout  = 8.0f;
    constexpr float Drawer  = 16.0f;
    constexpr float Dialog  = 32.0f;
    constexpr float Modal   = 64.0f;
}

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

struct KernelShadowParams {
    float               elevationDp{ElevationPreset::Flyout};
    uint32_t            shadowColor{0x40000000};
    float               blurRadius{6.0f};
    KernelVector2       offset{0.0f, 4.0f};
    float               spread{0.0f};
    KernelShadowQuality quality{KernelShadowQuality::Ultra};
};

struct KernelAtlasTile {
    uint32_t tileId{0};
    bool     allocated{false};
    float    cachedElevation{0.0f};
    bool     isDirty{false};
};

struct ShadowSubsystemTelemetry {
    uint64_t totalShadowCastingPasses{0};
    uint64_t elevationTransitions{0};
    uint64_t shadowAtlasEvictions{0};
    uint64_t parallaxComputations{0};
    double   avgShadowDispatchLatencyUs{0.0};
};

bool InitializeHorizon343DirectCompositionShadowDepthSubsystem();
bool VerifyHorizon343DirectCompositionShadowDepthSubsystem();

} // namespace micant::um::prismx_shadow_depth
