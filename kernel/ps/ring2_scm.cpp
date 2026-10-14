/**
 * @file ring2_scm.cpp
 * @brief Horizon 67: Sovereign Ring 2 Service Control Manager (services.exe),
 *        RpcSs/DcomLaunch & Ring 3 Handover Gate Compilation Unit
 *        (base/ntos/ps/ring2_scm_services_broker).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/ring2_scm_services_broker.hpp"

namespace micant::sys::internal {

void Ring2ScmInitializeExecutiveSubsystem() noexcept {
    micant::km::ring2scm::InitializeRing2ScmBrokerExports();
}

} // namespace micant::sys::internal
