#pragma once

#include <cstdint>

namespace micant::hal::amd64::detail {

// CR8 exposes only sixteen interrupt priority classes. Assign a distinct
// DIRQL to each device class instead of pretending 24 levels can be ordered
// independently by this controller. Device vectors occupy classes 2..11;
// class 13 is the existing LAPIC clock and class 15 covers HIGH_LEVEL.
inline constexpr uint8_t DeviceIrqls[]{3, 5, 8, 10, 13, 15, 18, 20, 23, 26};
inline constexpr uint8_t DeviceClassFirst = 2;
inline constexpr uint8_t DeviceClassLast = 11;

// Map every IRQL to the greatest class at or below it. A device source can
// be connected only at one of DeviceIrqls, so an intermediate IRQL never
// masks a source with a higher assigned IRQL.
[[nodiscard]] constexpr uint8_t bootstrapHardwarePriority(uint8_t irql) noexcept {
    if (irql >= 30) return 15;
    if (irql == 29) return 14;
    if (irql == 28) return 13;
    if (irql == 27) return 12;
    uint8_t priority = 0;
    for (uint8_t i = 0; i < sizeof(DeviceIrqls); ++i) {
        if (DeviceIrqls[i] > irql) break;
        priority = static_cast<uint8_t>(DeviceClassFirst + i);
    }
    return priority;
}

// Only classes represented by a distinct DIRQL may be used for devices.
// The caller owns vector allocation and must install a gate before unmasking.
[[nodiscard]] constexpr bool bootstrapDeviceVectorIrql(uint8_t vector, uint8_t& irql) noexcept {
    const uint8_t priority = vector >> 4;
    if (priority < DeviceClassFirst || priority > DeviceClassLast) return false;
    irql = DeviceIrqls[priority - DeviceClassFirst];
    return true;
}

[[nodiscard]] constexpr bool bootstrapDeviceVector(uint8_t irql, uint8_t slot,
                                                    uint8_t& vector) noexcept {
    if (slot >= 16) return false;
    for (uint8_t i = 0; i < sizeof(DeviceIrqls); ++i) {
        if (DeviceIrqls[i] != irql) continue;
        vector = static_cast<uint8_t>(((DeviceClassFirst + i) << 4) | slot);
        return true;
    }
    return false;
}
} // namespace micant::hal::amd64::detail
