/**
 * @file ring1_dxgk.cpp
 * @brief Horizon 62: Sovereign Ring 1 Graphics & Display Scanout Bridge
 *        Compilation Unit (base/ntos/win32k/ring1_dxgkrnl_flip_plane).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/win32k/ring1_dxgkrnl_flip_plane.hpp"

namespace micant::sys::internal {

void Ring1DxgkInitializeExecutiveSubsystem() noexcept {
    micant::km::ring1dxgk::InitializeRing1DxgkrnlFlipExports();
}

} // namespace micant::sys::internal
