#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/ktm.hpp"

namespace micant::sys::internal {

void KtmInitializeExecutiveSubsystem() noexcept {
    micant::km::ktm::InitializeKtmSubsystemExports();
}

} // namespace micant::sys::internal
