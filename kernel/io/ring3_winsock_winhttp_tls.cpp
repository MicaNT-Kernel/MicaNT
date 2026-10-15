/**
 * @file ring3_winsock_winhttp_tls.cpp
 * @brief Horizon 74 Kernel Compilation Unit (#46): Ring 3 Winsock2, WinHTTP &
 *        Schannel TLS 1.3 Tunneling Through Ring 0 afd.sys / WSK / RIO.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/net/ring3_winsock_winhttp_tls_tunnel.hpp"

namespace micant::sys::internal {

void Ring3WinsockWinHttpTlsInitializeExecutiveSubsystem() noexcept {
    micant::um::net_tunnel::InitializeRing3WinsockWinHttpTlsTunnelExports();
}

} // namespace micant::sys::internal
