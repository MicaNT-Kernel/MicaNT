/**
 * @file livehotswap.cpp
 * @brief Horizon 54: Sovereign NT Zero-Downtime Live Driver Hot-Swap (IoHotSwap*),
 *        Self-Healing Paging & Storage I/O (0x7A / 0xF4 Killer), and Hardware Intel
 *        VT-d IOMMU Per-Device DMA Firewall (base/ntos/io/livepatch_storheal).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/live_hotswap_storheal.hpp"

namespace micant::sys::internal {

void LiveHotSwapInitializeExecutiveSubsystem() noexcept {
    micant::km::hotswap::InitializeLiveHotSwapStorHealExports();
    (void)micant::km::hotswap::TitanLiveHotSwapStorHealManager::get();
}

} // namespace micant::sys::internal
