#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/verifier.hpp"

namespace micant::sys::internal {

void VfInitializeExecutiveSubsystem() noexcept {
    micant::km::verifier::InitializeVerifierSubsystemExports();
}

} // namespace micant::sys::internal
