/**
 * @file prismx_directcomposition_video_hdr10_wddm32.cpp
 * @brief Horizon 327 (Kernel Unit #103): Sovereign MicaNT DirectComposition Hardware Video Surface
 *        Swapchain Present Synchronization, HDR10 PQ Metadata & YUV420 Planar Scanout Pipeline
 *        - DirectComposition video visual binding (IDCompositionVisual::SetContent)
 *        - YUV420 semi-planar surface formats (DXGI_FORMAT_NV12, DXGI_FORMAT_P010)
 *        - SMPTE ST 2086 mastering display metadata & CTA-861.3 MaxCLL/MaxFALL
 *        - SMPTE ST 2084 Perceptual Quantizer (PQ) EOTF / OETF mathematical transfer functions
 *        - Zero-copy MPO hardware video plane scanout promotion (<60us present latency)
 *        - Pure ISO C++23. Zero External Dependencies.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directcomposition_video_hdr10_wddm32.hpp"

namespace micant::um::prismx_videohdr10 {

bool VerifyHorizon327DirectCompositionVideoHdr10Subsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: SMPTE ST 2084 (PQ) EOTF / OETF Mathematical Inversion Verification
    // ------------------------------------------------------------------------
    float n10k = MicaNtPerceptualQuantizer::InverseEotf(10000.0f);
    if (std::abs(n10k - 1.0f) > 0.001f) return false;
    float l10k = MicaNtPerceptualQuantizer::Eotf(1.0f);
    if (std::abs(l10k - 10000.0f) > 1.0f) return false;

    float n100 = MicaNtPerceptualQuantizer::InverseEotf(100.0f);
    if (std::abs(n100 - 0.508078f) > 0.005f) return false;
    float l100 = MicaNtPerceptualQuantizer::Eotf(n100);
    if (std::abs(l100 - 100.0f) > 0.5f) return false;

    float n1000 = MicaNtPerceptualQuantizer::InverseEotf(1000.0f);
    float l1000 = MicaNtPerceptualQuantizer::Eotf(n1000);
    if (std::abs(l1000 - 1000.0f) > 1.0f) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: 10-Bit P010 HDR10 Video Swapchain Creation & Visual Binding
    // ------------------------------------------------------------------------
    MicaNtVideoSwapchainDesc hdr10Desc{
        .width = 3840,
        .height = 2160,
        .format = MicaNtVideoPixelFormat::P010,
        .colorSpace = MicaNtColorSpaceType::BT2020_HDR10_PQ,
        .bufferCount = 3,
        .mastering = {
            .redPrimary = { 0.708f, 0.292f },
            .greenPrimary = { 0.170f, 0.797f },
            .bluePrimary = { 0.131f, 0.046f },
            .whitePoint = { 0.3127f, 0.3290f },
            .minLuminanceNits = 0.0001f,
            .maxLuminanceNits = 1000.0f
        },
        .lightLevel = {
            .maxContentLightLevel = 1000,
            .maxFrameAverageLightLevel = 400
        }
    };
    MicaNtDirectCompositionVideoSwapchain hdr10Swapchain(hdr10Desc);
    hdr10Swapchain.BindToVisual(4001);
    if (hdr10Swapchain.GetBoundVisualId() != 4001) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Decoder BackBuffer Write & YUV420 Planar Allocation
    // ------------------------------------------------------------------------
    MicaNtVideoFrame* backBuf = hdr10Swapchain.GetBackBuffer();
    if (!backBuf) return false;
    if (backBuf->width != 3840 || backBuf->height != 2160) return false;
    if (backBuf->lumaY.size() != (3840u * 2160u * 2u)) return false;
    if (backBuf->chromaUV.size() != (3840u * 1080u * 2u)) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: Present Synchronization & MPO Plane 1 Promotion
    // ------------------------------------------------------------------------
    bool presOk = hdr10Swapchain.Present(16666666ull);
    if (!presOk) return false;
    if (!hdr10Swapchain.IsMpoPromoted()) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: NV12 8-bit Video Swapchain Verification & Telemetry
    // ------------------------------------------------------------------------
    MicaNtVideoSwapchainDesc sdrDesc{
        .width = 1920,
        .height = 1080,
        .format = MicaNtVideoPixelFormat::NV12,
        .colorSpace = MicaNtColorSpaceType::BT709_SDR,
        .bufferCount = 2
    };
    MicaNtDirectCompositionVideoSwapchain sdrSwapchain(sdrDesc);
    sdrSwapchain.BindToVisual(4002);
    MicaNtVideoFrame* sdrBuf = sdrSwapchain.GetBackBuffer();
    if (!sdrBuf) return false;
    if (sdrBuf->lumaY.size() != (1920u * 1080u)) return false;
    if (!sdrSwapchain.Present(33333333ull)) return false;
    if (!sdrSwapchain.IsMpoPromoted()) return false;

    const auto& tel = hdr10Swapchain.GetTelemetry();
    if (tel.VideoSurfacesCreated < 1u) return false;
    if (tel.FramesPresented < 1u) return false;
    if (tel.MpoVideoPlanesPromoted < 1u) return false;
    if (tel.Hdr10MetadataPacketsSent < 1u) return false;
    if (tel.AverageVideoPresentLatencyMicroseconds >= 60.0f) return false;

    return true;
}

} // namespace micant::um::prismx_videohdr10
