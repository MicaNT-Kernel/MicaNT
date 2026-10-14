/**
 * @file mftzipui.cpp
 * @brief Horizon 57: Sovereign NT Zero-Freeze 2ms Input Hook Guard, Kernel-Native
 *        $MFT / $UsnJrnl Instant File Search & Zero-Copy NtMapViewOfSection
 *        zipfldr.dll Archive Bridge Compilation Unit (base/ntos/fsrtl/mftsearch_uiguard_zipfldr).
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/fs/mftsearch_uiguard_zipfldr.hpp"

namespace micant::sys::internal {

void MftZipUiInitializeExecutiveSubsystem() noexcept {
    micant::km::mftzipui::InitializeMftZipUiExports();
}

} // namespace micant::sys::internal
