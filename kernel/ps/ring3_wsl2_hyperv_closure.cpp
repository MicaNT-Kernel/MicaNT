/**
 * @file ring3_wsl2_hyperv_closure.cpp
 * @brief Horizon 79 (Unit #51): Sovereign Ring 3 WSL2/LXSS, Hyper-V WHP, Enterprise RDP &
 *        100% 4-Ring OS Closure.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/servers/ring3_wsl2_hyperv_enterprise_closure.hpp"

namespace micant::sys::internal {

void Ring3Wsl2HyperVClosureInitializeExecutiveSubsystem() noexcept {
    micant::um::enterprise_closure::InitializeRing3Wsl2HyperVEnterpriseClosureExports();
}

} // namespace micant::sys::internal
