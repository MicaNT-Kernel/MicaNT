/**
 * @file kuser_seal.cpp
 * @brief Horizon 60: Sovereign Ring 0 <-> Ring 3 Hardware ABI Seal
 *        (KUSER_SHARED_DATA 0x7FFE0000, SYSCALL/SYSRET MSRs & KiUser*
 *        Upcall Trampolines) Compilation Unit (base/ntos/ke/kuser_upcall_seal).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/core/kuser_upcall_seal.hpp"

namespace micant::sys::internal {

void KuserSealInitializeExecutiveSubsystem() noexcept {
    micant::km::kuserseal::InitializeKuserUpcallSealExports();
}

} // namespace micant::sys::internal
