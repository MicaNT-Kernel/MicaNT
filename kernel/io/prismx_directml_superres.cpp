/**
 * @file prismx_directml_superres.cpp
 * @brief Horizon 85 Kernel Compilation Unit (#57):
 *        PrismX + Official Microsoft DirectML (DirectMLX.h & DirectMLSuperResolution).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_directml_superres_graph.hpp"

namespace micant::sys::internal {

void PrismXDirectMLSuperResInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_directml_sr::InitializePrismXDirectMLSuperResExports();
}

} // namespace micant::sys::internal
