/**
 * @file ring3_prismaudio_mf.cpp
 * @brief Horizon 73: Sovereign Ring 3 PrismAudio (XAudio2 3D Spatial Engine), WASAPI &
 *        Media Foundation Tunneling Through Ring 2 AudioDG APO Sandbox, Ring 1 1.33 ms
 *        WaveRT DMA & Ring 0 MMCSS Compilation Unit (base/ntos/io/ring3_prismaudio_wasapi_mf_tunnel).
 */

#include <iostream>
#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/media/ring3_prismaudio_wasapi_mf_tunnel.hpp"

namespace micant::sys::internal {

void Ring3PrismAudioMfInitializeExecutiveSubsystem() noexcept {
    auto& mgr = micant::um::prismaudio_tunnel::SovereignPrismAudioWasapiMfTunnelManager::get();
    mgr.initializeAndVerifyPrismAudioTunnel();
    const auto& t = mgr.telemetry();
    std::cout << "[MicaNT Ring 3 PrismAudio] XAudio2 " << t.sampleRateHz << "Hz ("
              << t.channelCount << "ch, " << t.synthesizedPcmBytes
              << "B PCM, Samples=" << t.samplesPlayed
              << ") | 3D Spatial (Vol=" << t.spatialVolumeFactor
              << ", PanL=" << t.spatialPanLeft << ", PanR=" << t.spatialPanRight
              << ") | R1 WaveRT=" << t.ring1WaveRtLatencyUs << " us | R2 APO="
              << t.ring2ApoBudgetUs << " us | R0 MMCSS Prio=" << t.ring0MmcssPriority
              << " | Underruns=" << t.audioGlitchesOrUnderruns << "\n";
}

} // namespace micant::sys::internal
