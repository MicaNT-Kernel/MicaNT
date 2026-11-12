#include "micant/km/hal/amd64_firmware_interrupts.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"
#include "micant/km/executive/arbiter.hpp"
#include <cstring>

namespace micant::hal::amd64 {
namespace {
constexpr size_t maxTableSize = 1024 * 1024;
constexpr size_t headerSize = 36;
BootstrapInterruptTopology g_topology{};
bool g_topologyAvailable{};
alignas(16) uint8_t g_tableBuffer[maxTableSize]{};
uint8_t g_rsdpBuffer[4096]{};

bool firmwareRange(const boot::UefiKernelHandoffV3& handoff, uint64_t address, size_t size) noexcept {
    if (!address || !size || size - 1 > UINT64_MAX - address) return false;
    uint64_t cursor = address;
    const uint64_t last = address + size - 1;
    while (cursor <= last) {
        bool found = false;
        for (uint64_t offset = 0; offset < handoff.memoryMapSize; offset += handoff.memoryMapDescriptorSize) {
            uefi::EfiMemoryDescriptor descriptor{};
            std::memcpy(&descriptor, reinterpret_cast<const void*>(handoff.memoryMapAddress + offset), sizeof(descriptor));
            const auto type = static_cast<uefi::EfiMemoryType>(descriptor.type);
            if (type != uefi::EfiMemoryType::EfiACPIReclaimMemory &&
                type != uefi::EfiMemoryType::EfiACPIMemoryNVS &&
                type != uefi::EfiMemoryType::EfiRuntimeServicesData &&
                type != uefi::EfiMemoryType::EfiBootServicesData &&
                type != uefi::EfiMemoryType::EfiLoaderData &&
                type != uefi::EfiMemoryType::EfiReservedMemoryType) continue;
            // The scratch page is mapped write-back; reject memory which the
            // firmware does not describe as capable of that cache policy.
            if (!(descriptor.attribute & uint64_t{8}) || !descriptor.numberOfPages ||
                descriptor.numberOfPages > (UINT64_MAX - descriptor.physicalStart) / 4096)
                continue;
            const uint64_t end = descriptor.physicalStart + descriptor.numberOfPages * 4096;
            if (cursor < descriptor.physicalStart || cursor >= end) continue;
            if (last < end) return true;
            cursor = end;
            found = true;
            break;
        }
        if (!found) return false;
    }
    return true;
}

bool copyFirmware(const boot::UefiKernelHandoffV3& handoff, uint64_t physical,
                  void* destination, size_t size) noexcept {
    return firmwareRange(handoff, physical, size) &&
        copyBootstrapFirmwareMemory(physical, destination, size);
}

template<typename T> T read(const uint8_t* p) noexcept {
    T value{};
    std::memcpy(&value, p, sizeof(value));
    return value;
}

bool checksum(const uint8_t* bytes, size_t size) noexcept {
    uint8_t sum = 0;
    for (size_t i = 0; i < size; ++i) sum = static_cast<uint8_t>(sum + bytes[i]);
    return sum == 0;
}

const uint8_t* table(const boot::UefiKernelHandoffV3& handoff, uint64_t physical,
                     const char* signature, size_t& length) noexcept {
    uint8_t header[headerSize]{};
    if (!copyFirmware(handoff, physical, header, sizeof(header))) return nullptr;
    auto* p = header;
    length = read<uint32_t>(p + 4);
    if (length < headerSize || length > maxTableSize ||
        std::memcmp(p, signature, 4) ||
        !copyFirmware(handoff, physical, g_tableBuffer, length) ||
        !checksum(g_tableBuffer, length)) return nullptr;
    return g_tableBuffer;
}

bool parseMadt(const uint8_t* madt, size_t size, BootstrapInterruptTopology& topology) noexcept {
    if (size < 44) return false;
    for (size_t offset = 44; offset < size;) {
        if (size - offset < 2) return false;
        const uint8_t* p = madt + offset;
        const size_t length = p[1];
        if (length < 2 || length > size - offset) return false;
        if (p[0] == 1) {
            if (length < 12 || topology.controllerCount == 8) return false;
            const auto physical = read<uint32_t>(p + 4);
            if (!physical || (physical & 4095)) return false;
            auto& item = topology.controllers[topology.controllerCount++];
            item = {physical, read<uint32_t>(p + 8), p[2]};
            for (size_t i = 0; i + 1 < topology.controllerCount; ++i)
                if (topology.controllers[i].id == item.id ||
                    topology.controllers[i].physicalAddress == item.physicalAddress ||
                    topology.controllers[i].globalInterruptBase == item.globalInterruptBase) return false;
        } else if (p[0] == 2) {
            if (length < 10 || p[2] != 0 || topology.overrideCount == 16) return false;
            const uint16_t flags = read<uint16_t>(p + 8);
            if ((flags & ~uint16_t{15}) || (flags & 3) == 2 || ((flags >> 2) & 3) == 2)
                return false;
            auto& item = topology.overrides[topology.overrideCount++];
            item = {read<uint32_t>(p + 4), flags, p[3]};
            if (item.source >= 16) return false;
            for (size_t i = 0; i + 1 < topology.overrideCount; ++i)
                if (topology.overrides[i].source == item.source) return false;
        }
        offset += length;
    }
    return topology.controllerCount != 0;
}
}

bool discoverBootstrapInterruptTopology(const boot::UefiKernelHandoffV3& handoff,
                                        BootstrapInterruptTopology& output) noexcept {
    // Both the scratch mapping and table buffers are single-BSP resources.
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0" : "=r"(flags) : : "memory");
    if ((flags & (uint64_t{1} << 9)) ||
        handoff.memoryMapDescriptorSize < sizeof(uefi::EfiMemoryDescriptor) ||
        !handoff.memoryMapSize || handoff.memoryMapSize % handoff.memoryMapDescriptorSize ||
        handoff.memoryMapSize > UINT64_MAX - handoff.memoryMapAddress ||
        !bootstrapKernelRange(handoff.memoryMapAddress, handoff.memoryMapSize, false)) return false;
    const uint64_t rsdpPhysical = handoff.acpiTablePhysicalAddress;
    if (!copyFirmware(handoff, rsdpPhysical, g_rsdpBuffer, 20)) return false;
    const auto* rsdp = g_rsdpBuffer;
    if (std::memcmp(rsdp, "RSD PTR ", 8) || !checksum(rsdp, 20)) return false;
    bool xsdt = false;
    uint64_t root = read<uint32_t>(rsdp + 16);
    if (rsdp[15] >= 2) {
        if (!copyFirmware(handoff, rsdpPhysical, g_rsdpBuffer, 36)) return false;
        const size_t length = read<uint32_t>(rsdp + 20);
        if (length < 36 || length > sizeof(g_rsdpBuffer) ||
            !copyFirmware(handoff, rsdpPhysical, g_rsdpBuffer, length) ||
            !checksum(rsdp, length)) return false;
        const uint64_t candidate = read<uint64_t>(rsdp + 24);
        if (candidate) { root = candidate; xsdt = true; }
    }
    size_t rootSize{};
    const auto* entries = table(handoff, root, xsdt ? "XSDT" : "RSDT", rootSize);
    const size_t stride = xsdt ? 8 : 4;
    if (!entries || (rootSize - headerSize) % stride) return false;
    for (size_t offset = headerSize; offset < rootSize; offset += stride) {
        const uint64_t physical = xsdt ? read<uint64_t>(entries + offset) : read<uint32_t>(entries + offset);
        uint8_t signature[4]{};
        if (!copyFirmware(handoff, physical, signature, sizeof(signature))) return false;
        if (std::memcmp(signature, "APIC", 4)) continue;
        size_t length{};
        const auto* madt = table(handoff, physical, "APIC", length);
        BootstrapInterruptTopology topology{};
        if (!madt || !parseMadt(madt, length, topology)) return false;
        output = topology;
        g_topology = topology;
        g_topologyAvailable = true;
        return true;
    }
    return false;
}
const BootstrapInterruptTopology* bootstrapInterruptTopology() noexcept {
    return g_topologyAvailable ? &g_topology : nullptr;
}

namespace {
volatile uint32_t* g_dmaRemappingUnits[16]{};
size_t g_dmaRemappingUnitCount{};
bool g_directDmaInventoryReady{};
uint32_t g_directDmaLeases{};
bool g_iommuTransitionPending{};
}

bool initializeBootstrapDmaTranslationState(const boot::UefiKernelHandoffV3& handoff) noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    bool success = false;
    do {
        if (g_directDmaInventoryReady || g_dmaRemappingUnitCount ||
            handoff.memoryMapDescriptorSize < sizeof(uefi::EfiMemoryDescriptor) ||
            !handoff.memoryMapSize || handoff.memoryMapSize % handoff.memoryMapDescriptorSize ||
            handoff.memoryMapSize > UINT64_MAX - handoff.memoryMapAddress ||
            !bootstrapKernelRange(handoff.memoryMapAddress, handoff.memoryMapSize, false)) break;
        uint32_t eax{}, ebx{}, ecx{}, edx{};
        __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0), "c"(0));
        if (ebx != 0x756e6547u || edx != 0x49656e69u || ecx != 0x6c65746eu) break;
        const uint64_t rsdpPhysical = handoff.acpiTablePhysicalAddress;
        if (!copyFirmware(handoff, rsdpPhysical, g_rsdpBuffer, 20) ||
            std::memcmp(g_rsdpBuffer, "RSD PTR ", 8) || !checksum(g_rsdpBuffer, 20)) break;
        uint64_t root = read<uint32_t>(g_rsdpBuffer + 16);
        bool xsdt = false;
        if (g_rsdpBuffer[15] >= 2) {
            if (!copyFirmware(handoff, rsdpPhysical, g_rsdpBuffer, 36)) break;
            const size_t length = read<uint32_t>(g_rsdpBuffer + 20);
            if (length < 36 || length > sizeof(g_rsdpBuffer) ||
                !copyFirmware(handoff, rsdpPhysical, g_rsdpBuffer, length) ||
                !checksum(g_rsdpBuffer, length)) break;
            const auto candidate = read<uint64_t>(g_rsdpBuffer + 24);
            if (candidate) { root = candidate; xsdt = true; }
        }
        size_t rootSize{};
        const auto* entries = table(handoff, root, xsdt ? "XSDT" : "RSDT", rootSize);
        const size_t stride = xsdt ? 8 : 4;
        if (!entries || (rootSize - headerSize) % stride) break;
        uint64_t dmarPhysical{};
        bool invalid = false;
        for (size_t offset = headerSize; offset < rootSize; offset += stride) {
            const auto physical = xsdt ? read<uint64_t>(entries + offset) : read<uint32_t>(entries + offset);
            uint8_t signature[4]{};
            if (!copyFirmware(handoff, physical, signature, sizeof(signature))) { invalid = true; break; }
            if (!std::memcmp(signature, "IVRS", 4)) { invalid = true; break; }
            if (!std::memcmp(signature, "DMAR", 4)) {
                if (dmarPhysical) { invalid = true; break; }
                dmarPhysical = physical;
            }
        }
        if (invalid || !dmarPhysical) break;
        size_t length{};
        const auto* dmar = table(handoff, dmarPhysical, "DMAR", length);
        if (!dmar || length < 48) break;
        bool includeAll = false;
        for (size_t offset = 48; offset < length;) {
            if (length - offset < 4) { invalid = true; break; }
            const uint16_t kind = read<uint16_t>(dmar + offset);
            const uint16_t size = read<uint16_t>(dmar + offset + 2);
            if (size < 4 || size > length - offset) { invalid = true; break; }
            // Other DMAR structures can impose reserved ranges or remapping
            // semantics not yet represented by the bootstrap DMA owner.
            if (kind != 0) { invalid = true; break; }
            if (kind == 0) {
                if (size < 16 || read<uint16_t>(dmar + offset + 6) != 0 ||
                    g_dmaRemappingUnitCount == 16) { invalid = true; break; }
                const auto physical = read<uint64_t>(dmar + offset + 8);
                if (!physical || (physical & 4095)) { invalid = true; break; }
                // The root arbiter retains each remapping register range for
                // the kernel IOMMU owner throughout the direct DMA lifetime.
                if (!km::arb::reservePhysicalRange(physical, 4096)) { invalid = true; break; }
                auto* unit = static_cast<volatile uint32_t*>(mapBootstrapDeviceMemory(physical, 4096));
                if (!unit) { invalid = true; break; }
                g_dmaRemappingUnits[g_dmaRemappingUnitCount++] = unit;
                if (!protectBootstrapIommuRegisters(physical, 4096, const_cast<uint32_t*>(unit))) { invalid = true; break; }
                if (!unit[0] || unit[0] == UINT32_MAX || (unit[0x1c / 4] & (1u << 31))) {
                    invalid = true; break;
                }
                includeAll |= (dmar[offset + 4] & 1u) != 0;
            }
            offset += size;
        }
        if (invalid || !includeAll || !g_dmaRemappingUnitCount) break;
        commitBootstrapIommuProtection();
        g_directDmaInventoryReady = true;
        success = true;
    } while (false);
    if (!success) {
        while (g_dmaRemappingUnitCount) {
            auto* unit = g_dmaRemappingUnits[--g_dmaRemappingUnitCount];
            if (!releaseBootstrapDeviceMemory(const_cast<uint32_t*>(unit), 4096)) __builtin_trap();
            g_dmaRemappingUnits[g_dmaRemappingUnitCount] = nullptr;
        }
    }
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return success;
}

bool bootstrapDirectDmaAvailable() noexcept {
    if (!g_directDmaInventoryReady || !g_dmaRemappingUnitCount) return false;
    for (size_t i = 0; i < g_dmaRemappingUnitCount; ++i)
        if (!g_dmaRemappingUnits[i] || (g_dmaRemappingUnits[i][0x1c / 4] & (1u << 31)))
            return false;
    return true;
}

bool acquireBootstrapDirectDmaLease() noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    const bool ok = !g_iommuTransitionPending && bootstrapDirectDmaAvailable() &&
        g_directDmaLeases != UINT32_MAX;
    if (ok) ++g_directDmaLeases;
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return ok;
}
void releaseBootstrapDirectDmaLease() noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (!g_directDmaLeases) __builtin_trap();
    --g_directDmaLeases;
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
}
bool beginBootstrapIommuTransition() noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    const bool ok = !g_iommuTransitionPending && !g_directDmaLeases;
    if (ok) g_iommuTransitionPending = true;
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return ok;
}
void endBootstrapIommuTransition() noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (!g_iommuTransitionPending) __builtin_trap();
    g_iommuTransitionPending = false;
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
}
} // namespace micant::hal::amd64

namespace micant::hal::amd64 {
namespace {
struct IoApicController {
    volatile uint32_t* registers{};
    uint32_t globalBase{};
    uint32_t inputCount{};
};
IoApicController g_ioControllers[8]{};
size_t g_ioControllerCount{};
bool g_ioInitializationAttempted{};

bool interruptsDisabled() noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0" : "=r"(flags) : : "memory");
    return (flags & (uint64_t{1} << 9)) == 0;
}

uint32_t readIoRegister(volatile uint32_t* mmio, uint8_t reg) noexcept {
    mmio[0] = reg;
    __asm__ __volatile__("" : : : "memory");
    return mmio[4];
}

void writeIoRegister(volatile uint32_t* mmio, uint8_t reg, uint32_t value) noexcept {
    mmio[0] = reg;
    __asm__ __volatile__("" : : : "memory");
    mmio[4] = value;
    __asm__ __volatile__("" : : : "memory");
}
}

bool initializeBootstrapIoApics() noexcept {
    if (!interruptsDisabled() || g_ioInitializationAttempted) return false;
    const auto* topology = bootstrapInterruptTopology();
    if (!topology) return false;
    IoApicController controllers[8]{};
    size_t mappedCount{};
    const auto rollback = [&]() noexcept {
        while (mappedCount) {
            auto* mapping = controllers[--mappedCount].registers;
            if (!releaseBootstrapDeviceMemory(const_cast<uint32_t*>(mapping), 4096)) __builtin_trap();
        }
    };
    for (size_t i = 0; i < topology->controllerCount; ++i) {
        const auto& descriptor = topology->controllers[i];
        auto* mmio = static_cast<volatile uint32_t*>(
            mapBootstrapDeviceMemory(descriptor.physicalAddress, 4096));
        if (!mmio) { rollback(); return false; }
        controllers[i].registers = mmio;
        ++mappedCount;
        const uint32_t id = readIoRegister(mmio, 0) >> 24;
        const uint32_t version = readIoRegister(mmio, 1);
        const uint32_t count = ((version >> 16) & 0xff) + 1;
        // IOREGSEL is eight bits and redirection registers begin at 0x10.
        if (id != descriptor.id || count > 120 ||
            descriptor.globalInterruptBase > UINT32_MAX - (count - 1)) { rollback(); return false; }
        const uint32_t end = descriptor.globalInterruptBase + count - 1;
        for (size_t j = 0; j < i; ++j) {
            const auto& other = controllers[j];
            const uint32_t otherEnd = other.globalBase + other.inputCount - 1;
            if (descriptor.globalInterruptBase <= otherEnd && other.globalBase <= end) { rollback(); return false; }
        }
        controllers[i] = {mmio, descriptor.globalInterruptBase, count};
    }
    for (size_t i = 0; i < topology->controllerCount; ++i) g_ioControllers[i] = controllers[i];
    g_ioControllerCount = topology->controllerCount;
    g_ioInitializationAttempted = true;
    return g_ioControllerCount != 0;
}

bool resolveBootstrapIsaInterrupt(uint8_t irq, uint32_t& gsi, bool& activeLow,
                                  bool& levelTriggered) noexcept {
    const auto* topology = bootstrapInterruptTopology();
    if (!topology || irq >= 16) return false;
    uint32_t candidate = irq;
    bool low = false, level = false;
    for (size_t i = 0; i < topology->overrideCount; ++i) {
        const auto& entry = topology->overrides[i];
        if (entry.source != irq) continue;
        candidate = entry.globalInterrupt;
        low = (entry.flags & 3) == 3;
        level = ((entry.flags >> 2) & 3) == 3;
        break;
    }
    for (size_t i = 0; i < g_ioControllerCount; ++i) {
        const auto& controller = g_ioControllers[i];
        if (candidate >= controller.globalBase &&
            candidate - controller.globalBase < controller.inputCount) {
            gsi = candidate;
            activeLow = low;
            levelTriggered = level;
            return true;
        }
    }
    return false;
}

bool prepareBootstrapIoApicRoute(uint32_t globalInterrupt, uint8_t vector,
                                 uint8_t destinationApicId, bool activeLow,
                                 bool levelTriggered) noexcept {
    if (!interruptsDisabled() || vector < 0x20 || vector == 0xd1 || vector == 0xff) return false;
    for (size_t i = 0; i < g_ioControllerCount; ++i) {
        auto& controller = g_ioControllers[i];
        if (globalInterrupt < controller.globalBase ||
            globalInterrupt - controller.globalBase >= controller.inputCount) continue;
        const auto pin = globalInterrupt - controller.globalBase;
        const auto lowRegister = static_cast<uint8_t>(0x10 + pin * 2);
        constexpr uint32_t mask = 1u << 16;
        const uint32_t oldLow = readIoRegister(controller.registers, lowRegister);
        // Do not silently disconnect a route owned by firmware or another driver.
        if (!(oldLow & mask)) return false;
        const uint32_t low = mask | vector | (activeLow ? 1u << 13 : 0) |
            (levelTriggered ? 1u << 15 : 0);
        writeIoRegister(controller.registers, static_cast<uint8_t>(lowRegister + 1),
                        uint32_t{destinationApicId} << 24);
        writeIoRegister(controller.registers, lowRegister, low);
        return true;
    }
    return false;
}

bool unmaskBootstrapIoApicRoute(uint32_t globalInterrupt, uint8_t vector) noexcept {
    if (!interruptsDisabled()) return false;
    for (size_t i = 0; i < g_ioControllerCount; ++i) {
        auto& controller = g_ioControllers[i];
        if (globalInterrupt < controller.globalBase ||
            globalInterrupt - controller.globalBase >= controller.inputCount) continue;
        const auto reg = static_cast<uint8_t>(0x10 + (globalInterrupt - controller.globalBase) * 2);
        const uint32_t value = readIoRegister(controller.registers, reg);
        if (!(value & (1u << 16)) || (value & 0xff) != vector) return false;
        writeIoRegister(controller.registers, reg, value & ~(1u << 16));
        return true;
    }
    return false;
}
} // namespace micant::hal::amd64
