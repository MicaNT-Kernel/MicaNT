/**
 * @file prismx_wavert_xaudio2_wddm32.cpp
 * @brief Horizon 314 (Kernel Unit #90): Sovereign MicaNT Real-Time Audio Subsystem
 *        (WaveRT & XAudio2 2.9 Low-Latency DMA Engine) & GameInput 8000Hz Spatial Audio Synchronization
 *        - WaveRT (Wave Real-Time) Circular DMA Audio Buffer (<1.5ms Period @ 48kHz)
 *        - Event-Driven Buffer Notifications (KSRTAUDIO_NOTIFICATION_EVENT)
 *        - XAudio2 2.9 7.1.4 Dolby Atmos / Windows Sonic 3D Spatial HRTF Panning Engine
 *        - GameInput 8000Hz Direct Audio Trigger Interlock (<1.5ms Trigger-to-Ear Latency)
 *        - Zero-Copy Cyclic Aperture Mapping for Intel HDA & USB Audio Class 2.0 Controllers.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_wavert_xaudio2_wddm32.hpp"

namespace micant::um::prismx_audio {

bool VerifyHorizon314WaveRtXAudio2Subsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: WaveRT Circular DMA Buffer Geometry & Low-Latency Timing
    // ------------------------------------------------------------------------
    WaveRtBufferDescriptor desc{};
    desc.SampleRate = WAVERT_SAMPLERATE_48KHZ;
    desc.Channels = 2;
    desc.BitsPerSample = 32;
    desc.PeriodSamples = WAVERT_PERIOD_SAMPLES_64;
    desc.PeriodCount = WAVERT_DEFAULT_PERIODS;
    desc.PhysicalBaseAddress = 0x00000000FE800000ULL;

    MicaNtWaveRtAudioEngine engine(desc);

    if (engine.GetDescriptor().SampleRate != 48000u) return false;
    if (engine.GetDescriptor().PeriodSamples != 64u) return false;
    if (engine.GetDmaRingBuffer().size() != 2048u) return false; // 64 * 2 * 4 * 4

    const auto& initTel = engine.GetTelemetry();
    if (!initTel.WaveRtBufferValid) return false;
    if (initTel.BufferPeriodLatencyMs < 1.33f || initTel.BufferPeriodLatencyMs > 1.34f) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: 7.1.4 Dolby Atmos / Windows Sonic 3D Spatial Audio Panning
    // ------------------------------------------------------------------------
    SpatialListener listener{};
    listener.Position = { 0.0f, 0.0f, 0.0f };

    SpatialEmitter frontEmitter{};
    frontEmitter.Position = { 0.0f, 0.0f, 10.0f }; // Directly in front
    frontEmitter.OuterRadius = 50.0f;

    auto pansFront = engine.ComputeSpatialPans(listener, frontEmitter);
    // Center & front channels must carry predominant energy
    if (pansFront[SPEAKER_FRONT_CENTER] <= 0.0f) return false;
    if (pansFront[SPEAKER_FRONT_LEFT] <= 0.0f) return false;
    if (pansFront[SPEAKER_FRONT_RIGHT] <= 0.0f) return false;

    SpatialEmitter leftTopEmitter{};
    leftTopEmitter.Position = { -10.0f, 10.0f, 0.0f }; // High left
    leftTopEmitter.OuterRadius = 50.0f;

    auto pansTopLeft = engine.ComputeSpatialPans(listener, leftTopEmitter);
    if (pansTopLeft[SPEAKER_TOP_FRONT_LEFT] <= 0.0f) return false;
    if (pansTopLeft[SPEAKER_SIDE_LEFT] <= 0.0f) return false;

    // Verify energy normalization (sum of squares is ~1.0)
    float sumSq = 0.0f;
    for (float g : pansTopLeft) sumSq += g * g;
    if (std::abs(sumSq - 1.0f) >= 0.05f) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: GameInput 8000Hz Direct Audio Trigger Interlock
    // ------------------------------------------------------------------------
    NTSTATUS triggerStatus = engine.EnqueueGameInputTriggerAudio(frontEmitter, listener, 0.9f);
    if (!NT_SUCCESS(triggerStatus)) return false;

    bool callbackFired = false;
    uint32_t signaledPeriod = 999;
    engine.RegisterNotificationCallback([&](uint32_t period) {
        callbackFired = true;
        signaledPeriod = period;
    });

    NTSTATUS dmaStatus = engine.ProcessDmaPeriod();
    if (!NT_SUCCESS(dmaStatus)) return false;
    if (!callbackFired || signaledPeriod != 1u) return false;

    if (engine.GetHwClock().SampleOffset != 64u) return false;
    if (engine.GetHwClock().CycleCounter != 1u) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: Telemetry Verification
    // ------------------------------------------------------------------------
    const auto& tel = engine.GetTelemetry();
    if (tel.TotalDmaCyclesCompleted != 1u) return false;
    if (tel.TotalAudioFramesRendered != 64u) return false;
    if (tel.DmaUnderflowCount != 0u) return false;
    if (tel.TotalSpatialPansCalculated < 2u) return false;
    if (tel.GameInputAudioTriggersInterlocked != 1u) return false;
    if (!tel.SpatialPanningValid) return false;
    if (!tel.GameInputInterlockValid) return false;
    if (!tel.HdaDmaEngineActive) return false;
    if (tel.TriggerToEarLatencyMs > 1.5f) return false;

    return true;
}

} // namespace micant::um::prismx_audio
