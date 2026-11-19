// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 339 (Suite 555)
// DirectComposition Variable Refresh Rate (VRR) Dynamic Cadence Flipping,
//   Present Duration Pacing & Scanline Beam Synchronization Pipeline
//   (IDXGISwapChain3::SetPresentDuration, DwmGetCompositionTimingInfoEx)
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace micant::um::prismx_vrr_cadence {

constexpr uint64_t KERNEL_HNS_PER_SECOND = 10000000ULL;

struct KernelVrrCapabilities {
    uint32_t minRefreshHz{48};
    uint32_t maxRefreshHz{540};
    uint32_t totalScanlines{1125};
    uint32_t activeScanlines{1080};
    bool     supportsPresentDuration{true};
    bool     supportsLfc{true};
};

struct KernelBeamState {
    uint32_t currentScanline{0};
    uint32_t predictedScanlineAtFlip{0};
    bool     isInVBlank{false};
    double   timeToNextVBlankUs{0.0};
};

struct VrrSubsystemTelemetry {
    uint64_t totalPresentFlips{0};
    uint64_t lfcDuplicatedFrames{0};
    uint64_t vrrDurationChanges{0};
    double   avgCadenceDispatchLatencyUs{0.0};
};

bool InitializeHorizon339DirectCompositionVrrCadenceSubsystem();
bool VerifyHorizon339DirectCompositionVrrCadenceSubsystem();

} // namespace micant::um::prismx_vrr_cadence