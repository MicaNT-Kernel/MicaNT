/**
 * @file quotagov.cpp
 * @brief Horizon 56: Sovereign NT Anti-Bloat Process/GDI Leak Governor, Runaway
 *        svchost Service Healer & Native ApiSetSchema / Primes.exe Drag-Race
 *        Compilation Unit (base/ntos/ps/quota_svchost_primes).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/quota_svchost_primes.hpp"

namespace micant::sys::internal {

void QuotaGovInitializeExecutiveSubsystem() noexcept {
    micant::km::quotagov::InitializeQuotaSvchostPrimesExports();
}

} // namespace micant::sys::internal
