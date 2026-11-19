// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 334 (Suite 550)
// DirectComposition Visual Layer Debugging, Frame Profiling, ETW Trace Event
//   Instrumentation & GPU Timing Query Telemetry Pipeline
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace micant::um::prismx_debug_telemetry {

constexpr uint32_t DCOMP_DEBUG_FLAG_NONE                 = 0x00000000;
constexpr uint32_t DCOMP_DEBUG_FLAG_HIGHLIGHT_DIRTY      = 0x00000001;
constexpr uint32_t DCOMP_DEBUG_FLAG_SHOW_BOUNDING_BOXES  = 0x00000002;
constexpr uint32_t DCOMP_DEBUG_FLAG_HEATMAP_OVERDRAW     = 0x00000004;
constexpr uint32_t DCOMP_DEBUG_FLAG_FLASH_UPDATES        = 0x00000008;

struct DwmCompositionTimingInfo {
    uint32_t cbSize{sizeof(DwmCompositionTimingInfo)};
    uint64_t qpcRefreshPeriod{0};
    uint64_t qpcCompose{0};
    uint64_t qpcPresentTime{0};
    uint64_t cFrameSequence{0};
    uint32_t cFramesPending{0};
    uint32_t cFramesDisplayed{0};
    uint32_t cFramesDropped{0};
    uint32_t cFramesMissed{0};
    double   compositionLatencyUs{0.0};
    double   scanoutDurationUs{0.0};
};

struct VisualDebugElement {
    uint32_t visualId{0};
    int32_t  x{0};
    int32_t  y{0};
    int32_t  width{0};
    int32_t  height{0};
    uint32_t overdrawPasses{1};
    uint32_t debugColorArgb{0xFF00FF00};
};

struct DebugSubsystemTelemetry {
    uint64_t etwEventsEmitted{0};
    uint64_t framesProfiled{0};
    double   avgProfilingOverheadUs{0.0};
    uint32_t activeDebugFlags{0};
};

bool InitializeHorizon334DirectCompositionDebugTelemetrySubsystem();
bool VerifyHorizon334DirectCompositionDebugTelemetrySubsystem();

} // namespace micant::um::prismx_debug_telemetry
