#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/ex.hpp"
#include "micant/km/executive/ex_work.hpp"
#include "micant/km/executive/lookaside.hpp"

namespace micant::sys::internal {

void ExInitializeExecutiveSubsystem() noexcept {
    auto& poolMgr = micant::ex::ExecutivePool::get();
    (void)poolMgr;
}

} // namespace micant::sys::internal
