#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/pnp_boot_scheduler.hpp"

namespace micant::sys::internal {

void PnpBootInitializeExecutiveSubsystem() noexcept {
    micant::km::pnpboot::InitializePnpBootSchedulerExports();
}

} // namespace micant::sys::internal
