#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/po.hpp"

namespace micant::sys::internal {

void PoInitializeExecutiveSubsystem() noexcept {
    auto& po = micant::po::PowerManager::get();
    (void)po;
}

} // namespace micant::sys::internal
