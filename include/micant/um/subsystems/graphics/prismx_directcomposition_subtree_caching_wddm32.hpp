// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 337 (Suite 553)
// DirectComposition Dynamic Subtree Visual Caching, Pre-Rasterized Layer
//   Isolation & GPU Tile Eviction Pipeline (IDCompositionVisual2)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace micant::um::prismx_subtree_caching {

enum class KernelOpacityMode : uint32_t {
    Layer    = 0,
    Multiply = 1
};

enum class KernelBackfaceVisibility : uint32_t {
    Visible = 0,
    Hidden  = 1
};

struct SubtreeTileInfo {
    uint32_t visualId{0};
    uint64_t vramAddress{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t bytesAllocated{0};
    bool     isCached{false};
    bool     isDirty{true};
};

struct SubtreeSubsystemTelemetry {
    uint64_t compositionPasses{0};
    uint64_t cacheHits{0};
    uint64_t cacheMisses{0};
    uint64_t tilesEvicted{0};
    uint64_t currentVramUsageBytes{0};
    double   avgCompositionLatencyUs{0.0};
};

bool InitializeHorizon337DirectCompositionSubtreeCachingSubsystem();
bool VerifyHorizon337DirectCompositionSubtreeCachingSubsystem();

} // namespace micant::um::prismx_subtree_caching
