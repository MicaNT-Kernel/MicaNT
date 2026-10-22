#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/po.hpp"
#include "micant/km/executive/bms.hpp"

namespace micant::sys::internal {

void PoInitializeExecutiveSubsystem() noexcept {
    auto& po = micant::po::PowerManager::get();
    (void)po;
    micant::po::bms::InitializeBatterySubsystem();
}

} // namespace micant::sys::internal
