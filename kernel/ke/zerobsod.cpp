/**
 * @file zerobsod.cpp
 * @brief Horizon 53: Sovereign NT Zero-BSOD Self-Healing Kernel Guard —
 *        DPC Watchdog Threaded-DPC Demotion (0x133), Power IRP Timeout Auto-Completion (0x9F),
 *        High-IRQL Page-Fault Shadow Healer (0xD1 / 0x0A), and Idempotent Pool / LIST_ENTRY
 *        Self-Repair (0xC2 / 0x139 — base/ntos/ke/zerobsod).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/core/zero_bsod_guard.hpp"

namespace micant::sys::internal {

void ZeroBsodInitializeExecutiveSubsystem() noexcept {
    micant::km::zerobsod::InitializeZeroBsodGuardExports();
    (void)micant::km::zerobsod::TitanZeroBsodKernelGuard::get();
}

} // namespace micant::sys::internal
