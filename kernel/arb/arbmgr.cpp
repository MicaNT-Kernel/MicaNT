#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/arbiter.hpp"

namespace micant::sys::internal {

void ArbInitializeExecutiveSubsystem() noexcept {
    micant::km::arb::InitializeArbiterSubsystemExports();
}

} // namespace micant::sys::internal
