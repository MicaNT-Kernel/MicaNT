// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 335 (Suite 551)
// DirectComposition Multi-Queue Hardware Synchronization, Asynchronous Compute
//   Backdrop Blur & Work Graph Surface Handover Pipeline
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace micant::um::prismx_multiqueue {

struct MultiQueueSubsystemMetrics {
    uint64_t directSubmissions{0};
    uint64_t computeSubmissions{0};
    uint64_t copySubmissions{0};
    uint64_t crossQueueFencesSynchronized{0};
    uint64_t workGraphNodesDispatched{0};
    double   avgDispatchLatencyUs{0.0};
};

struct KernelBackdropBlurJob {
    uint32_t jobId{0};
    uint32_t width{1920};
    uint32_t height{1080};
    float    blurRadius{30.0f};
    uint64_t inputVramAddress{0};
    uint64_t outputVramAddress{0};
    uint64_t computeFenceValue{0};
};

struct KernelWorkGraphBinding {
    uint64_t surfaceVramAddress{0};
    uint32_t nodeId{0};
    uint32_t threadGroupsX{0};
    uint32_t threadGroupsY{0};
    bool     active{false};
};

bool InitializeHorizon335DirectCompositionMultiQueueSubsystem();
bool VerifyHorizon335DirectCompositionMultiQueueSubsystem();

} // namespace micant::um::prismx_multiqueue
