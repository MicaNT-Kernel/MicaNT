/**
 * @file ring3_fiber_net_apps.cpp
 * @brief Horizon 77 (Unit #49): Sovereign Ring 3 High-Speed Fiber Networking & Sysadmin Apps
 *        Roll-In (PuTTY SSH-2.0, Wireshark NDIS 6.88, qBittorrent RIO/IOCP, WinSCP & FileZilla).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/net/ring3_fiber_net_apps_tunnel.hpp"

namespace micant::sys::internal {

void Ring3FiberNetAppsInitializeExecutiveSubsystem() noexcept {
    micant::um::fiber_net_apps::InitializeRing3FiberNetAppsTunnelExports();
}

} // namespace micant::sys::internal
