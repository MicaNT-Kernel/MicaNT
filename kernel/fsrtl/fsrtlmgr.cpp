#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/fsrtl.hpp"

namespace micant::sys::internal {

void FsRtlInitializeExecutiveSubsystem() noexcept {
    micant::km::fsrtl::InitializeFsRtlSubsystemExports();
}

} // namespace micant::sys::internal
