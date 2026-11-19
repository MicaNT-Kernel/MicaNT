// ============================================================================
// Sovereign MicaNT Operating System Kernel - Horizon 333 (Suite 549)
// DirectComposition Hardware Desktop Duplication API (IDXGIOutputDuplication),
//   Dirty/Move Region Tracking & Zero-Copy VRAM Screen Capture Pipeline
//
// 100% Freestanding ISO C++23, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace micant::um::prismx_desktop_duplication {

struct POINT {
    int32_t x;
    int32_t y;
};

struct RECT {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;

    [[nodiscard]] constexpr int32_t Width() const noexcept { return right - left; }
    [[nodiscard]] constexpr int32_t Height() const noexcept { return bottom - top; }
    [[nodiscard]] constexpr bool IsEmpty() const noexcept { return right <= left || bottom <= top; }
};

struct DXGI_OUTDUPL_MOVE_RECT {
    POINT SourcePoint;
    RECT  DestinationRect;
};

struct DXGI_OUTDUPL_POINTER_POSITION {
    POINT Position;
    bool  Visible;
};

struct DXGI_OUTDUPL_POINTER_SHAPE_INFO {
    uint32_t Type;
    uint32_t Width;
    uint32_t Height;
    uint32_t Pitch;
    POINT    HotSpot;
};

struct DXGI_OUTDUPL_FRAME_INFO {
    int64_t                       LastPresentTime;
    int64_t                       LastMouseUpdateTime;
    uint32_t                      AccumulatedFrames;
    bool                          RectsCoalesced;
    bool                          ProtectedContentMaskedOut;
    DXGI_OUTDUPL_POINTER_POSITION PointerPosition;
    uint32_t                      TotalMetadataBufferSize;
    uint32_t                      BufferSizeNeeded;
};

struct WddmDesktopDuplicationDesc {
    uint32_t outputIndex{0};
    uint32_t width{3840};
    uint32_t height{2160};
    uint32_t refreshRateHz{540};
    uint32_t formatBpp{32}; // 32-bit ARGB
    bool     gpuVramResident{true};
};

struct DesktopDuplicationTelemetry {
    uint64_t framesCaptured{0};
    uint64_t dirtyRectsReported{0};
    uint64_t moveRectsReported{0};
    uint64_t pointerUpdatesReported{0};
    double   avgAcquireLatencyUs{0.0};
    uint32_t activeDuplicationSessions{0};
};

bool InitializeHorizon333DirectCompositionDesktopDuplicationSubsystem();
bool VerifyHorizon333DirectCompositionDesktopDuplicationSubsystem();

} // namespace micant::um::prismx_desktop_duplication
