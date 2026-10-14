/**
 * @file ring1_smss.cpp
 * @brief Horizon 64: Sovereign Native NT Session Bootstrap (smss.exe, \KnownDlls
 *        Pre-Mapper & csrss.exe Super-Server) Compilation Unit
 *        (base/ntos/ps/ring1_smss_csrss_bootstrap).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/ring1_smss_csrss_bootstrap.hpp"

namespace micant::sys::internal {

void Ring1SmssInitializeExecutiveSubsystem() noexcept {
    micant::km::ring1smss::InitializeRing1SmssCsrssExports();
}

} // namespace micant::sys::internal
