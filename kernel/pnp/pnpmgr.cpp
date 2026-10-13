#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/pnp.hpp"

namespace micant::sys::internal {

void PnpInitializeExecutiveSubsystem() noexcept {
    auto& pnp = micant::pnp::PnpManager::get();
    (void)pnp;
}

} // namespace micant::sys::internal
