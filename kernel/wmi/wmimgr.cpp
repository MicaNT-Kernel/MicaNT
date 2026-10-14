#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/wmi_driver_compat.hpp"

namespace micant::sys::internal {

void WmiInitializeExecutiveSubsystem() noexcept {
    micant::km::wmi::InitializeWmiAndDriverCompatExports();
}

} // namespace micant::sys::internal
