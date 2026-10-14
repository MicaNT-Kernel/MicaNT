/**
 * @file srm_ci.cpp
 * @brief Horizon 58: Sovereign Ring 0 Security Reference Monitor (Se), Mandatory
 *        Integrity Control (MIC / UIPI), AppContainer Token Sandboxing & Zero-Lag
 *        ci.dll Code Integrity Compilation Unit (base/ntos/se/srm_token_ci).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/srm_token_ci.hpp"

namespace micant::sys::internal {

void SrmCiInitializeExecutiveSubsystem() noexcept {
    micant::km::srmci::InitializeSrmTokenCiExports();
}

} // namespace micant::sys::internal
