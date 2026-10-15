/**
 * @file ring0_anticheat_eac_battleye_callbacks.cpp
 * @brief Horizon 98 (Kernel Unit #70): Sovereign Ring 0 Kernel-Mode Anti-Cheat Callbacks
 *        (EasyAntiCheat_EOS.sys, BEDaisy.sys & EAAntiCheat.sys) & SwapChain Overlay Guard.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/drivers/security/ring0_anticheat_eac_battleye_callbacks.hpp"

namespace micant::sys::internal {

void Ring0AntiCheatEacBattlEyeCallbacksInitializeExecutiveSubsystem() noexcept {
    micant::km::anticheat_eac_be::InitializeRing0AntiCheatCallbacksExports();
}

} // namespace micant::sys::internal
