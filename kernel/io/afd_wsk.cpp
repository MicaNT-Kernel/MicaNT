/**
 * @file afd_wsk.cpp
 * @brief Horizon 59: Sovereign Ring 0 Ancillary Function Driver (afd.sys),
 *        Winsock Kernel (WSK) Direct Transport & Zero-Copy Registered I/O (RIO)
 *        Compilation Unit (base/ntos/net/afd_wsk_rio).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/drivers/net/afd_wsk_rio.hpp"

namespace micant::sys::internal {

void AfdWskInitializeExecutiveSubsystem() noexcept {
    micant::km::afdwsk::InitializeAfdWskRioExports();
}

} // namespace micant::sys::internal
