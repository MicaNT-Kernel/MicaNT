/**
 * @file ring1_vfs.cpp
 * @brief Horizon 61: Sovereign Ring 1 VFS & Minifilter Fault-Isolated Fast-I/O
 *        Plane Compilation Unit (base/ntos/fsrtl/ring1_vfs_minifilter_plane).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/drivers/fs/ring1_vfs_minifilter_plane.hpp"

namespace micant::sys::internal {

void Ring1VfsInitializeExecutiveSubsystem() noexcept {
    micant::km::ring1vfs::InitializeRing1VfsMinifilterExports();
}

} // namespace micant::sys::internal
