#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/po.hpp"
#include "micant/km/executive/bms.hpp"
#include "micant/km/executive/ppm.hpp"
#include "micant/km/executive/thermal.hpp"

namespace micant::sys::internal {

void PoInitializeExecutiveSubsystem() noexcept {
    auto& po = micant::po::PowerManager::get();
    (void)po;
    micant::po::bms::InitializeBatterySubsystem();
    micant::po::ppm::InitializeProcessorPowerSubsystem();
    micant::po::thermal::InitializeThermalSubsystem();
}

} // namespace micant::sys::internal
