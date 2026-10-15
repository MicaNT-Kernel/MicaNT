/**
 * @file ring3_workstation_apps.cpp
 * @brief Horizon 78 (Unit #50): Sovereign Ring 3 Power-User Workstation Apps Roll-In
 *        (SystemInformer, Everything 6-us $MFT Search, Rufus VHDX, SumatraPDF & WinMerge).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/winuser/ring3_workstation_apps_tunnel.hpp"

namespace micant::sys::internal {

void Ring3WorkstationAppsInitializeExecutiveSubsystem() noexcept {
    micant::um::workstation_apps::InitializeRing3WorkstationAppsTunnelExports();
}

} // namespace micant::sys::internal
