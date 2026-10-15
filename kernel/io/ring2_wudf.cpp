/**
 * @file ring2_wudf.cpp
 * @brief Horizon 65: Sovereign Ring 2 Isolated Device & Parser Host
 *        (WUDFHost.exe & fontdrvhost.exe) Compilation Unit
 *        (base/ntos/io/ring2_wudf_fontdrv_host).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/ring2_wudf_fontdrv_host.hpp"

namespace micant::sys::internal {

void Ring2WudfInitializeExecutiveSubsystem() noexcept {
    micant::km::ring2wudf::InitializeRing2WudfFontDrvExports();
}

} // namespace micant::sys::internal
