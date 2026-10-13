#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/dbgk.hpp"

namespace micant::sys::internal {

void DbgkInitializeExecutiveSubsystem() noexcept {
    micant::km::dbgk::InitializeDbgkSubsystemExports();
}

} // namespace micant::sys::internal
