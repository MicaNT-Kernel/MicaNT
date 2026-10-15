/**
 * @file ring3_com_rpc_shell.cpp
 * @brief Horizon 75 Kernel Compilation Unit (#47): Ring 3 COM/OLE, RPC & Shell32
 *        Tunneling Through Ring 2 RpcSs/DcomLaunch & Ring 1 VFS.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/com/ring3_com_rpc_shell_controls_tunnel.hpp"

namespace micant::sys::internal {

void Ring3ComRpcShellInitializeExecutiveSubsystem() noexcept {
    micant::um::com_shell_tunnel::InitializeRing3ComRpcShellTunnelExports();
}

} // namespace micant::sys::internal
