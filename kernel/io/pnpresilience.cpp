#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/driver_resilience.hpp"

namespace micant::sys::internal {

void PnpResilienceInitializeExecutiveSubsystem() noexcept {
    micant::km::resilience::InitializeDriverResilienceExports();
}

} // namespace micant::sys::internal
