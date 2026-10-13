#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/hal/hal.hpp"
#include "micant/km/hal/acpi.hpp"
#include "micant/km/hal/pci.hpp"
#include "micant/km/hal/bootvid.hpp"

namespace micant::sys::internal {

void HalInitializeExecutiveSubsystem() noexcept {
    auto& hal = micant::hal::HardwareAbstractionLayer::get();
    if (hal.getProcessorCount() == 0) {
        hal.initialize(4);
    }
}

} // namespace micant::sys::internal
