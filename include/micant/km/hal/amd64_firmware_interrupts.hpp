#pragma once

#include <cstddef>
#include <cstdint>
#include "micant/km/hal/boot_contract.hpp"

namespace micant::hal::amd64 {

// Firmware-reported topology, not an installed interrupt route. The bootstrap
// kernel runs on one CPU and does not yet consume PCI or hot-plug routing.
struct BootstrapIoApic {
    uint64_t physicalAddress{};
    uint32_t globalInterruptBase{};
    uint8_t id{};
};
struct BootstrapInterruptOverride {
    uint32_t globalInterrupt{};
    uint16_t flags{};
    uint8_t source{};
};
struct BootstrapInterruptTopology {
    BootstrapIoApic controllers[8]{};
    BootstrapInterruptOverride overrides[16]{};
    size_t controllerCount{};
    size_t overrideCount{};
};

// The boot memory map bounds all physical firmware reads. A false result
// leaves output unchanged.
[[nodiscard]] bool discoverBootstrapInterruptTopology(
    const boot::UefiKernelHandoffV3& handoff, BootstrapInterruptTopology& output) noexcept;
[[nodiscard]] const BootstrapInterruptTopology* bootstrapInterruptTopology() noexcept;
// Conservative Intel VT-d inventory. Direct DMA is enabled only if firmware
// describes an include-all DRHD for segment 0 and every unit has translation
// disabled; unknown or unsupported topologies remain unavailable.
[[nodiscard]] bool initializeBootstrapDmaTranslationState(
    const boot::UefiKernelHandoffV3& handoff, uint32_t* failureStage = nullptr,
    uint64_t* failureDetail = nullptr) noexcept;
[[nodiscard]] bool bootstrapDirectDmaAvailable() noexcept;
[[nodiscard]] bool acquireBootstrapDirectDmaLease() noexcept;
void releaseBootstrapDirectDmaLease() noexcept;
[[nodiscard]] bool beginBootstrapIommuTransition() noexcept;
void endBootstrapIommuTransition() noexcept;

} // namespace micant::hal::amd64

namespace micant::hal::amd64 {
// These BSP-only operations require interrupts disabled. Initialization maps
// firmware-described controllers and reads their hardware GSI ranges. A
// prepared entry stays masked: no device source may be unmasked until an IDT
// gate, ISR, EOI and synchronization protocol have been installed.
[[nodiscard]] bool initializeBootstrapIoApics() noexcept;
[[nodiscard]] bool resolveBootstrapIsaInterrupt(uint8_t irq, uint32_t& gsi,
                                                bool& activeLow, bool& levelTriggered) noexcept;
[[nodiscard]] bool prepareBootstrapIoApicRoute(uint32_t globalInterrupt,
                                               uint8_t vector,
                                               uint8_t destinationApicId,
                                               bool activeLow,
                                               bool levelTriggered) noexcept;
[[nodiscard]] bool unmaskBootstrapIoApicRoute(uint32_t globalInterrupt, uint8_t vector) noexcept;
} // namespace micant::hal::amd64

namespace micant::hal::amd64 {
void setBootstrapPs2InterruptMode(bool active) noexcept;
void captureBootstrapPs2Interrupt() noexcept;
[[nodiscard]] bool popBootstrapPs2KeyboardByte(uint8_t& byte, bool& lost) noexcept;
}
