/**
 * @file prism_xaudio2_dinput8_com_abi.cpp
 * @brief Horizon 105 (Kernel Unit #77): Sovereign PrismX Official Microsoft XAudio2 2.9
 *        (xaudio2_9.dll) & DirectInput8 (dinput8.dll) COM VTable ABI & Binary IIDs.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_xaudio2_dinput8_com_abi.hpp"

namespace micant::sys::internal {

void PrismXXAudio2AndDirectInput8ComAbiInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_xaudio2_dinput8_abi::InitializePrismXAudio2AndDirectInput8Exports();
}

} // namespace micant::sys::internal
