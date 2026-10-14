/**
 * @file sysinfo_taskmgr.cpp
 * @brief Horizon 55: Sovereign NT Kernel Telemetry (NtQuerySystemInformation),
 *        "Show Kernel Times" (Taskmgr) Performance Engine & Lock-Free SMP / MMCSS
 *        Priority Scheduler Compilation Unit (base/ntos/ex/sysinfo_taskmgr).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/sysinfo_taskmgr.hpp"

namespace micant::sys::internal {

void SysInfoTaskmgrInitializeExecutiveSubsystem() noexcept {
    micant::km::sysinfo::InitializeSysInfoTaskmgrExports();
}

} // namespace micant::sys::internal
