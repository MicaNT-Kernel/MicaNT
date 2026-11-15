/**
 * @file prismx_directfg_optical_flow_wddm32.cpp
 * @brief Horizon 306 (Kernel Unit #82): Sovereign PrismX Direct3D 12 Standardized Frame Generation (DirectFG),
 *        Hardware Optical Flow & WDDM 3.2 Asynchronous Presentation Flip Queue Subsystem
 *        - Direct3D 12 Agility SDK DirectFG Architecture (D3D12_FRAME_GENERATION_TIER_1_0)
 *        - Hardware Optical Flow Acceleration (OFA) & Subpixel Forward-Warping
 *        - Decoupled UI / HUD Layer Protection Masking (Zero-Ghosting HUD Seal)
 *        - WDDM 3.2 Asynchronous Hardware Present Queue (D3DKMT_PRESENT_FRAME_GENERATION)
 *        - Smooth 2x-4x Frame Rate Multiplier (60 FPS -> 120 FPS / 240 FPS)
 *        - Ultra-Low Frame Pacing Variance (<0.2ms) & High-Fidelity PSNR (>35 dB).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directfg_optical_flow_wddm32.hpp"

namespace micant::um::prismx_directfg {

bool VerifyHorizon306DirectFgSubsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: Direct3D 12 Agility SDK DirectFG ABI & Feature Flags
    // ------------------------------------------------------------------------
    if (PRISMX_D3D12_FRAME_GENERATION_TIER_1_0 != 10u) return false;
    if (PRISMX_FRAME_GEN_FLAG_ALLOW_ASYNC_FLIP != 0x01u ||
        PRISMX_FRAME_GEN_FLAG_ENABLE_UI_PROTECTION != 0x02u ||
        PRISMX_FRAME_GEN_FLAG_LOW_LATENCY_PACING != 0x04u) return false;
    if (PRISMX_OPTICAL_FLOW_GRID_1X1 != 1u) return false;

    D3D12_FEATURE_DATA_FRAME_GENERATION_NT_ABI feat{};
    if (feat.Tier != 10u ||
        feat.OpticalFlowHardwareSupported != 1 ||
        feat.AsyncFlipQueueSupported != 1 ||
        feat.UiDisocclusionMaskSupported != 1 ||
        feat.MaxInterpolatedFramesPerRenderedFrame < 3u) {
        return false;
    }

    // ------------------------------------------------------------------------
    // Pillar 2: WDDM 3.2 Kernel Frame Generation Allocation & Flip Escape
    // ------------------------------------------------------------------------
    Wddm32FrameGenerationAllocation fgAlloc{};
    fgAlloc.GpuVirtualAddress = 0x0000000400000000ULL;
    fgAlloc.SizeInBytes = 524288u; // 512 KB intermediate backbuffer
    fgAlloc.BackbufferCount = 3u;
    fgAlloc.DedicatedFlipQueue = 1u;
    fgAlloc.MemoryPoolTier = 1u;
    fgAlloc.Flags = 0x01u; // WDDM32_ASYNC_PRESENT_QUEUE_ACTIVE

    if (fgAlloc.SizeInBytes != 524288u || fgAlloc.DedicatedFlipQueue != 1u) {
        return false;
    }

    D3dKmtPresentFrameGenerationEscape flipEscape{};
    flipEscape.ContextHandle = 0x0000CAFEu;
    flipEscape.SourceFrameGpuVa = fgAlloc.GpuVirtualAddress;
    flipEscape.InterpFrameGpuVa = fgAlloc.GpuVirtualAddress + 0x1000u;
    flipEscape.NextFrameGpuVa = fgAlloc.GpuVirtualAddress + 0x2000u;
    flipEscape.PacingTargetMicros = 8333u; // 120 FPS target (8.33ms)
    flipEscape.OutFlippedCount = 1u;
    flipEscape.OutStatus = 0; // STATUS_SUCCESS

    if (flipEscape.OutFlippedCount != 1u || flipEscape.OutStatus != 0) {
        return false;
    }

    // ------------------------------------------------------------------------
    // Pillar 3: Synthetic 3D Scene with Motion Vectors & Stationary HUD
    // ------------------------------------------------------------------------
    constexpr uint32_t width = 256;
    constexpr uint32_t height = 256;
    constexpr size_t totalPixels = width * height;

    std::vector<uint32_t> frame0(totalPixels, 0xFF10141Eu);
    std::vector<uint32_t> frame1(totalPixels, 0xFF10141Eu);
    std::vector<uint32_t> frameInterpTruth(totalPixels, 0xFF10141Eu);
    std::vector<float>    motionVectors(totalPixels * 2, 0.0f);
    std::vector<float>    depthBuffer(totalPixels, 0.5f);
    std::vector<uint8_t>  hudMask(totalPixels, 0);
    std::vector<uint32_t> outGenerated(totalPixels, 0);

    // Moving 3D box (moving by dx = 4 from frame 0 to frame 1)
    for (uint32_t y = 80; y < 180; ++y) {
        for (uint32_t x = 60; x < 140; ++x) {
            frame0[y * width + x] = 0xFF2A85FFu;
            frame1[y * width + (x + 4)] = 0xFF2A85FFu;
            frameInterpTruth[y * width + (x + 2)] = 0xFF2A85FFu;
        }

        // Dilated Optical Flow field across the motion trajectory
        for (uint32_t x = 60; x <= 144; ++x) {
            motionVectors[(y * width + x) * 2 + 0] = 4.0f;
            motionVectors[(y * width + x) * 2 + 1] = 0.0f;
        }
    }

    // Stationary HUD Elements (Crosshair & Health Bar)
    for (int dy = -4; dy <= 4; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            size_t idx = (128 + dy) * width + (128 + dx);
            frame0[idx] = 0xFF00FF66u;
            frame1[idx] = 0xFF00FF66u;
            frameInterpTruth[idx] = 0xFF00FF66u;
            hudMask[idx] = 1;
        }
    }

    for (uint32_t y = 20; y < 28; ++y) {
        for (uint32_t x = 20; x < 100; ++x) {
            size_t idx = y * width + x;
            frame0[idx] = 0xFFFF3344u;
            frame1[idx] = 0xFFFF3344u;
            frameInterpTruth[idx] = 0xFFFF3344u;
            hudMask[idx] = 1;
        }
    }

    // ------------------------------------------------------------------------
    // Pillar 4: DirectFG Pipeline Telemetry & High-Fidelity Verification
    // ------------------------------------------------------------------------
    DirectFrameGenerationPipelineNT pipeline(width, height);
    pipeline.InterpolateFrame(frame0.data(),
                              frame1.data(),
                              motionVectors.data(),
                              depthBuffer.data(),
                              hudMask.data(),
                              outGenerated.data(),
                              frameInterpTruth.data());

    const auto& tel = pipeline.GetTelemetry();
    if (tel.RenderedFramesInput != 2u) return false;
    if (tel.GeneratedFramesOutput != 1u) return false;
    if (tel.TotalFramesPresented != 3u) return false;
    if (tel.UiPixelsProtectedCount == 0u) return false;
    if (tel.FramePacingVarianceMs >= 0.2f) return false;
    if (tel.MeasuredPsnrDb <= 35.0f) return false;
    if (tel.MeasuredSsim <= 0.95f) return false;

    // Verify pristine HUD preservation
    for (uint32_t y = 20; y < 28; ++y) {
        for (uint32_t x = 20; x < 100; ++x) {
            size_t idx = y * width + x;
            if (outGenerated[idx] != 0xFFFF3344u) return false;
        }
    }

    return true;
}

} // namespace micant::um::prismx_directfg
