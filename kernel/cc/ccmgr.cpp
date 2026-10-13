#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/cc.hpp"

namespace micant::sys::internal {

void CcInitializeExecutiveCache() noexcept {
    micant::km::cc::InitializeCcCacheSubsystemExports();
}

} // namespace micant::sys::internal
