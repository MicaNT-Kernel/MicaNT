/**
 * @file prism_d2d1_dwrite_dcomp_11dll_seal.cpp
 * @brief Horizon 106 (Kernel Unit #78): Sovereign PrismX Official Microsoft Direct2D 1.3 (d2d1.dll),
 *        DirectWrite (dwrite.dll), DirectComposition (dcomp.dll) COM ABI & 11-DLL System32 Seal.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_d2d1_dwrite_dcomp_11dll_seal.hpp"

namespace micant::sys::internal {

void PrismXD2D1DWriteDCompElevenDllSealInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_d2d1_dwrite_dcomp_seal::InitializePrismSystem32ElevenDllSealExports();
}

} // namespace micant::sys::internal
