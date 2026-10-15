/**
 * @file prismx_execute_indirect_sampler_feedback.cpp
 * @brief Horizon 93 (Kernel Unit #65): Sovereign PrismX GPU-Driven Rendering
 *        (ExecuteIndirect Tier 1.1) & Sampler Feedback 1.0 / Tiled Resources Tier 4.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_execute_indirect_sampler_feedback.hpp"

namespace micant::sys::internal {

void PrismXExecuteIndirectSamplerFeedbackInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_indirect_sfs::InitializePrismXExecuteIndirectSamplerFeedbackExports();
}

} // namespace micant::sys::internal
