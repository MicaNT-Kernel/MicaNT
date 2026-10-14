/**
 * @file ring1_audio.cpp
 * @brief Horizon 63: Sovereign Ring 1 Real-Time WaveRT Audio & Multimedia DMA
 *        Compilation Unit (base/ntos/io/ring1_wavert_audio_dma).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/drivers/audio/ring1_wavert_audio_dma.hpp"

namespace micant::sys::internal {

void Ring1AudioInitializeExecutiveSubsystem() noexcept {
    micant::km::ring1audio::InitializeRing1WaveRtAudioExports();
}

} // namespace micant::sys::internal
