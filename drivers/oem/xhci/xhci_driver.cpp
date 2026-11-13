// Standalone boot-start xHCI OEM driver entry point.
// Controller startup remains disabled until Io/PnP claim and device-safe DMA
// allocation services are available through KernelServices V2.
#include <cstdint>
#include <atomic>
#include "micant/shared/boot_driver.hpp"
#include "xhci_transport.hpp"
#include "micant/shared/usb_host_abi.hpp"

namespace {
using namespace micant::boot_driver;

constexpr uint8_t kPciHeaderTypeOffset = 0x0C;
constexpr uint8_t kPciClassOffset = 0x08;
constexpr uint8_t kPciBar0Offset = 0x10;
constexpr int32_t kBusy = static_cast<int32_t>(0x80000011u);
micant::oem::xhci::Transport g_transport{};
const KernelServices* g_services{};
uint64_t g_hostToken{};
micant::shared::usb::HostServicesV1 g_host{};
uint16_t g_bus{}, g_device{}, g_function{};
PciBarResourceV1 g_claimResources[2]{};
size_t g_claimResourceCount{};
std::atomic<uint32_t> g_msiInterrupts{};
std::atomic<bool> g_msiReported{};
struct MsiState {
    uint16_t capability{}, msixCapability{};
    uint32_t msiHeader{}, msiAddressLow{}, msiAddressHigh{}, msiData{}, msiMask{};
    uint32_t msixHeader{};
    uint16_t command{};
    uint64_t routeToken{};
    bool is64{}, hasMask{}, pciProgrammed{}, connected{};
    volatile uint32_t* msixTable{};
    uint32_t msixEntry[4]{};
    size_t msixTableBytes{};
    bool msixMapped{}, msixProgrammed{};
    bool msixMode{};
} g_msi{};
void log(const KernelServices& services, const char* message) noexcept;
bool readConfig(const KernelServices& services, uint8_t bus, uint8_t device,
                uint8_t function, uint16_t offset, uint32_t& value) noexcept;
bool configureMsi() noexcept;
bool restoreMsi() noexcept;

void msiInterrupt(void*) noexcept {
    g_msiInterrupts.fetch_add(1, std::memory_order_relaxed);
}

bool findCapability(const KernelServices& services, uint8_t bus, uint8_t device,
                    uint8_t function, uint8_t requested, uint16_t& result) noexcept {
    result = 0;
    uint32_t status{}, pointerWord{};
    if (!readConfig(services, bus, device, function, 4, status) ||
        !(status & (1u << 20)) ||
        !readConfig(services, bus, device, function, 0x34, pointerWord)) return false;
    uint8_t pointer = static_cast<uint8_t>(pointerWord);
    uint64_t visited{};
    for (unsigned count = 0; pointer && count < 48; ++count) {
        if (pointer < 0x40 || pointer > 0xfc || (pointer & 3u)) return false;
        const unsigned slot = pointer >> 2;
        const uint64_t bit = uint64_t{1} << (slot & 63u);
        if (visited & bit) return false;
        visited |= bit;
        uint32_t entry{};
        if (!readConfig(services, bus, device, function, pointer, entry)) return false;
        if (static_cast<uint8_t>(entry) == requested) { result = pointer; return true; }
        pointer = static_cast<uint8_t>(entry >> 8);
    }
    return pointer == 0;
}

bool writeConfig(const KernelServices& services, uint16_t offset, uint32_t value) noexcept {
    return services.PciWriteConfig32 && services.PciWriteConfig32(
        services.Context, g_bus, g_device, g_function, offset, value) == STATUS_SUCCESS;
}

bool restoreMsi() noexcept {
    if (!g_msi.connected && !g_msi.pciProgrammed && !g_msi.msixMapped) { g_msi = {}; return true; }
    if (g_msi.msixMode && g_msi.pciProgrammed && g_services &&
        !writeConfig(*g_services, g_msi.msixCapability, g_msi.msixHeader & ~(1u << 31))) return false;
    if (g_msi.msixProgrammed && g_msi.msixTable) {
        g_msi.msixTable[3] |= 1u;
        for (unsigned i = 0; i < 4; ++i) g_msi.msixTable[i] = g_msi.msixEntry[i];
        g_msi.msixProgrammed = false;
    }
    if (g_msi.pciProgrammed && g_services && g_msi.msixMode) {
        uint32_t currentCommand{};
        if (!readConfig(*g_services, g_bus, g_device, g_function, 4, currentCommand)) return false;
        const uint32_t command = (currentCommand & 0xffffu & ~(1u << 10)) |
            (g_msi.command & (1u << 10));
        if (!writeConfig(*g_services, 4, command) ||
            !writeConfig(*g_services, g_msi.msixCapability, g_msi.msixHeader)) return false;
        g_msi.pciProgrammed = false;
    } else if (g_msi.pciProgrammed && g_services) {
        uint32_t disabled = g_msi.msiHeader & ~(1u << 16);
        if (!writeConfig(*g_services, g_msi.capability, disabled)) return false;
        const uint16_t dataOffset = static_cast<uint16_t>(g_msi.capability + (g_msi.is64 ? 12 : 8));
        if (!writeConfig(*g_services, g_msi.capability + 4, g_msi.msiAddressLow)) return false;
        if (g_msi.is64 && !writeConfig(*g_services, g_msi.capability + 8, g_msi.msiAddressHigh)) return false;
        if (!writeConfig(*g_services, dataOffset, g_msi.msiData)) return false;
        if (g_msi.hasMask && !writeConfig(*g_services,
                g_msi.capability + (g_msi.is64 ? 16 : 12), g_msi.msiMask)) return false;
        if (g_msi.msixCapability && !writeConfig(*g_services, g_msi.msixCapability,
                                                  g_msi.msixHeader)) return false;
        uint32_t currentCommand{};
        if (!readConfig(*g_services, g_bus, g_device, g_function, 4, currentCommand)) return false;
        const uint32_t command = (currentCommand & 0xffffu & ~(1u << 10)) |
            (g_msi.command & (1u << 10));
        if (!writeConfig(*g_services, 4, command)) return false;
        if (!writeConfig(*g_services, g_msi.capability, g_msi.msiHeader)) return false;
        g_msi.pciProgrammed = false;
    }
    if (g_msi.msixMapped) {
        if (!g_services || !g_services->ReleaseDeviceMemory(g_services->Context,
                const_cast<uint32_t*>(g_msi.msixTable), g_msi.msixTableBytes)) return false;
        g_msi.msixMapped = false;
    }
    if (g_msi.connected) {
        if (!g_services || g_services->DisconnectMsiInterrupt(g_services->Context,
                                                               g_msi.routeToken) != STATUS_SUCCESS)
            return false;
        g_msi.connected = false;
    }
    g_msi = {};
    return true;
}

bool configureMsi() noexcept {
    if (!g_services || !g_services->MsiInterruptVersion ||
        g_services->MsiInterruptVersion != MSI_INTERRUPT_SERVICES_VERSION ||
        !g_services->ConnectMsiInterrupt || !g_services->DisconnectMsiInterrupt) return false;
    uint16_t msi{}, msix{};
    const bool hasMsi = findCapability(*g_services, static_cast<uint8_t>(g_bus), static_cast<uint8_t>(g_device),
                        static_cast<uint8_t>(g_function), 0x05, msi) && msi;
    const bool hasMsix = findCapability(*g_services, static_cast<uint8_t>(g_bus), static_cast<uint8_t>(g_device),
                         static_cast<uint8_t>(g_function), 0x11, msix) && msix;
    if (!hasMsi && !hasMsix) return false;

    if (!hasMsi && hasMsix) {
        uint32_t header{}, tableInfo{}, command{};
        if (!readConfig(*g_services, g_bus, g_device, g_function, msix, header) ||
            !readConfig(*g_services, g_bus, g_device, g_function, msix + 4, tableInfo) ||
            !readConfig(*g_services, g_bus, g_device, g_function, 4, command)) return false;
        const uint8_t barIndex = static_cast<uint8_t>(tableInfo & 7u);
        const uint64_t tableOffset = tableInfo & ~uint32_t{7};
        if (tableOffset > UINT64_MAX - 16) return false;
        const PciBarResourceV1* bar{};
        for (size_t i = 0; i < g_claimResourceCount; ++i)
            if (g_claimResources[i].BarIndex == barIndex) bar = &g_claimResources[i];
        if (!bar || tableOffset + 16 > bar->Length ||
            bar->PhysicalAddress > UINT64_MAX - tableOffset) return false;
        auto* table = static_cast<volatile uint32_t*>(g_services->MapDeviceMemory(
            g_services->Context, bar->PhysicalAddress + tableOffset, 16));
        if (!table) return false;
        g_msi.capability = 0; g_msi.msixCapability = msix; g_msi.msixHeader = header;
        g_msi.command = static_cast<uint16_t>(command);
        g_msi.msixTable = table; g_msi.msixTableBytes = 16; g_msi.msixMapped = true;
        g_msi.msixMode = true;
        for (unsigned i = 0; i < 4; ++i) g_msi.msixEntry[i] = table[i];
        MsiInterruptRouteV1 route{};
        if (g_services->ConnectMsiInterrupt(g_services->Context, msiInterrupt,
                &g_msiInterrupts, &route) != STATUS_SUCCESS || route.Size < sizeof(route) ||
            route.Version != MSI_INTERRUPT_SERVICES_VERSION || !route.Token || !route.MessageAddress) {
            (void)restoreMsi();
            return false;
        }
        g_msi.routeToken = route.Token; g_msi.connected = true;
        g_msi.pciProgrammed = true;
        const uint32_t masked = (header | (1u << 30)) & ~(1u << 31);
        if (!writeConfig(*g_services, msix, masked)) { (void)restoreMsi(); return false; }
        table[3] |= 1u;
        table[0] = static_cast<uint32_t>(route.MessageAddress);
        table[1] = static_cast<uint32_t>(route.MessageAddress >> 32);
        table[2] = route.MessageData;
        g_msi.msixProgrammed = true;
        if (!writeConfig(*g_services, 4, (command & 0xffffu) | (1u << 10)) ||
            !writeConfig(*g_services, msix, (header & ~(1u << 30)) | (1u << 31))) {
            (void)restoreMsi(); return false;
        }
        table[3] &= ~1u;
        if (!g_transport.enableInterrupts()) { (void)restoreMsi(); return false; }
        g_msiInterrupts.store(0, std::memory_order_relaxed);
        g_msiReported.store(false, std::memory_order_relaxed);
        log(*g_services, "usbxhci.sys: PCI MSI-X table entry 0 and xHCI interrupter armed");
        return true;
    }

    uint32_t msiHeader{}, command{}, addressLow{}, dataWord{};
    if (!readConfig(*g_services, g_bus, g_device, g_function, msi, msiHeader) ||
        !readConfig(*g_services, g_bus, g_device, g_function, 4, command)) return false;
    const uint16_t control = static_cast<uint16_t>(msiHeader >> 16);
    const bool is64 = (control & (1u << 7)) != 0;
    const bool hasMask = (control & (1u << 8)) != 0;
    const uint16_t dataOffset = static_cast<uint16_t>(msi + (is64 ? 12 : 8));
    if (!readConfig(*g_services, g_bus, g_device, g_function, msi + 4, addressLow) ||
        !readConfig(*g_services, g_bus, g_device, g_function, dataOffset, dataWord)) return false;
    uint32_t addressHigh{}, mask{};
    if (is64 && !readConfig(*g_services, g_bus, g_device, g_function, msi + 8, addressHigh)) return false;
    if (hasMask && !readConfig(*g_services, g_bus, g_device, g_function,
                               msi + (is64 ? 16 : 12), mask)) return false;
    uint32_t msixHeader{};
    if (msix && !readConfig(*g_services, g_bus, g_device, g_function, msix, msixHeader)) return false;

    g_msi.capability = msi; g_msi.msixCapability = msix;
    g_msi.msiHeader = msiHeader; g_msi.msiAddressLow = addressLow;
    g_msi.msiAddressHigh = addressHigh; g_msi.msiData = dataWord; g_msi.msiMask = mask;
    g_msi.msixHeader = msixHeader; g_msi.command = static_cast<uint16_t>(command);
    g_msi.is64 = is64; g_msi.hasMask = hasMask;

    MsiInterruptRouteV1 route{};
    const auto connected = g_services->ConnectMsiInterrupt(g_services->Context,
        msiInterrupt, &g_msiInterrupts, &route);
    if (connected != STATUS_SUCCESS || route.Size < sizeof(route) ||
        route.Version != MSI_INTERRUPT_SERVICES_VERSION || !route.Token || !route.MessageAddress) {
        if (connected == STATUS_SUCCESS && route.Token)
            (void)g_services->DisconnectMsiInterrupt(g_services->Context, route.Token);
        g_msi = {};
        return false;
    }
    g_msi.routeToken = route.Token; g_msi.connected = true;
    g_msi.pciProgrammed = true;
    // Mask MSI-X while conventional MSI is active; preserve its original
    // state for exact rollback during controller stop/unload.
    if (msix && (msixHeader & (1u << 31))) {
        const uint32_t maskedMsix = (msixHeader | (1u << 30)) & ~(1u << 31);
        if (!writeConfig(*g_services, msix, maskedMsix)) { (void)restoreMsi(); return false; }
    }
    if (!writeConfig(*g_services, msi, msiHeader & ~(1u << 16))) { (void)restoreMsi(); return false; }
    if (!writeConfig(*g_services, msi + 4, static_cast<uint32_t>(route.MessageAddress))) {
        (void)restoreMsi(); return false;
    }
    if (is64 && !writeConfig(*g_services, msi + 8,
                              static_cast<uint32_t>(route.MessageAddress >> 32))) {
        (void)restoreMsi(); return false;
    }
    const uint32_t newData = is64 ?
        ((dataWord & 0xffff0000u) | (route.MessageData & 0xffffu)) :
        ((dataWord & 0x0000ffffu) | ((route.MessageData & 0xffffu) << 16));
    if (!writeConfig(*g_services, dataOffset, newData)) { (void)restoreMsi(); return false; }
    if (hasMask && !writeConfig(*g_services, msi + (is64 ? 16 : 12), mask & ~1u)) {
        (void)restoreMsi(); return false;
    }
    if (!writeConfig(*g_services, 4, (command & 0xffffu) | (1u << 10))) {
        (void)restoreMsi(); return false;
    }
    const uint32_t enabled = (msiHeader & 0x0000ffffu) |
        (uint32_t{static_cast<uint16_t>((control & ~0x0070u) | 1u)} << 16);
    if (!writeConfig(*g_services, msi, enabled)) { (void)restoreMsi(); return false; }
    if (!g_transport.enableInterrupts()) { (void)restoreMsi(); return false; }
    g_msiInterrupts.store(0, std::memory_order_relaxed);
    g_msiReported.store(false, std::memory_order_relaxed);
    log(*g_services, "usbxhci.sys: PCI MSI capability and xHCI interrupter armed");
    return true;
}

int32_t startHost(void*) { return g_transport.start() ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY; }
int32_t stopHost(void*) {
    g_transport.disableInterrupts();
    if (!restoreMsi() || !g_transport.stop()) return kBusy;
    return STATUS_SUCCESS;
}
int32_t restartHost(void*) {
    g_transport.disableInterrupts();
    if (!restoreMsi() || !g_services || !g_transport.stop()) return kBusy;
    uint16_t msix{}; uint32_t tableInfo{};
    PciBarRequestV1 requests[2]{{0, 0, 0, 0x1000}, {0, 0, 0, 0x1000}};
    size_t requestCount = 1;
    if (findCapability(*g_services, g_bus, g_device, g_function, 0x11, msix) && msix &&
        readConfig(*g_services, g_bus, g_device, g_function, msix + 4, tableInfo)) {
        const auto tableBar = static_cast<uint8_t>(tableInfo & 7u);
        if (tableBar) { requests[1].BarIndex = tableBar; requestCount = 2; }
    }
    g_claimResourceCount = requestCount;
    uint64_t claim{};
    const auto status = g_services->ClaimPciDevice(g_services->Context, 0, g_bus, g_device,
        g_function, requests, requestCount, &claim, g_claimResources, &g_claimResourceCount);
    if (status != STATUS_SUCCESS || !claim || g_claimResourceCount != requestCount ||
        g_claimResources[0].BarIndex != 0 || !g_claimResources[0].PhysicalAddress ||
        g_claimResources[0].Length < 0x1000 || g_claimResources[0].Length > 16u * 1024u * 1024u) {
        if (claim) (void)g_services->ReleasePciDevice(g_services->Context, claim);
        return STATUS_DEVICE_NOT_READY;
    }
    if (!g_transport.initialize(*g_services, g_bus, g_device, g_function, claim,
                                g_claimResources[0].PhysicalAddress,
                                static_cast<size_t>(g_claimResources[0].Length))) {
        return g_transport.stop() ? STATUS_DEVICE_NOT_READY : kBusy;
    }
    (void)configureMsi(); // Bounded event-ring polling remains the fallback.
    if (!g_transport.start()) return g_transport.stop() ? STATUS_DEVICE_NOT_READY : kBusy;
    return STATUS_SUCCESS;
}
uint8_t portCount(void*) { return g_transport.portCount(); }
int32_t readPort(void*, uint8_t port, uint32_t* value) {
    return value && g_transport.readPort(port, *value) ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
}
int32_t resetPort(void*, uint8_t port, uint8_t* speed) {
    return speed && g_transport.resetPort(port, *speed) ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
}
int32_t acknowledgeRootChange(void*, uint8_t port) {
    return g_transport.acknowledgeConnectionChange(port) ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
}
int32_t beginRootReset(void*, uint8_t port) {
    return g_transport.beginPortReset(port) ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
}
int32_t pollRootReset(void*, uint8_t port, uint8_t* speed) {
    if (!speed) return STATUS_INVALID_PARAMETER;
    const auto result = g_transport.pollPortReset(port, *speed);
    return result > 0 ? STATUS_SUCCESS : result == 0 ? kBusy : STATUS_DEVICE_NOT_READY;
}
uint8_t portSlotType(void*, uint8_t port) { return g_transport.portSlotType(port); }
int32_t prepareAddress(void*, uint8_t slot, uint8_t port, uint8_t speed, micant::shared::usb::Trb* command) {
    if (!command) return STATUS_INVALID_PARAMETER;
    micant::oem::xhci::Trb local{};
    if (!g_transport.prepareAddressDevice(slot, port, speed, local)) return STATUS_DEVICE_NOT_READY;
    *command = {local.parameter, local.status, local.control};
    return STATUS_SUCCESS;
}
int32_t prepareDownstream(void*, uint8_t slot, uint8_t rootPort, uint8_t speed,
                          uint8_t hubSlot, uint8_t hubPort, bool multiTt, micant::shared::usb::Trb* command) {
    if (!command) return STATUS_INVALID_PARAMETER;
    micant::oem::xhci::Trb local{};
    if (!g_transport.prepareAddressDevice(slot, rootPort, speed, local,
                                          speed < 3 ? hubSlot : 0, hubPort, speed < 3 && multiTt))
        return STATUS_DEVICE_NOT_READY;
    *command = {local.parameter, local.status, local.control}; return STATUS_SUCCESS;
}
int32_t prepareHub(void*, uint8_t endpoint, uint16_t packet, uint8_t interval, uint8_t speed,
                   uint8_t ports, uint8_t thinkTime, bool multiTt, micant::shared::usb::Trb* command) {
    if (!command) return STATUS_INVALID_PARAMETER;
    micant::oem::xhci::Trb local{};
    if (!g_transport.prepareHubEndpoint(endpoint, packet, interval, speed, ports, thinkTime, multiTt, local))
        return STATUS_DEVICE_NOT_READY;
    *command = {local.parameter, local.status, local.control}; return STATUS_SUCCESS;
}
int32_t prepareTopology(void*, uint8_t slot, uint8_t rootPort, uint8_t speed, uint32_t route,
                        uint8_t ttHub, uint8_t ttPort, bool multiTt, micant::shared::usb::Trb* command) {
    if (!command) return STATUS_INVALID_PARAMETER;
    micant::oem::xhci::Trb local{};
    if (!g_transport.prepareAddressDevice(slot, rootPort, speed, local, ttHub, ttPort, multiTt, route))
        return STATUS_DEVICE_NOT_READY;
    *command = {local.parameter, local.status, local.control}; return STATUS_SUCCESS;
}
int32_t prepareEp0Packet(void*, uint16_t packet, micant::shared::usb::Trb* command) {
    if (!command) return STATUS_INVALID_PARAMETER;
    micant::oem::xhci::Trb local{};
    if (!g_transport.prepareEp0Packet(packet, local)) return STATUS_DEVICE_NOT_READY;
    *command = {local.parameter, local.status, local.control}; return STATUS_SUCCESS;
}
int32_t prepareSuperSpeedHub(void*, uint8_t endpoint, uint16_t packet, uint8_t interval, uint8_t speed,
                             uint8_t ports, uint8_t burst, uint16_t bytes, micant::shared::usb::Trb* command) {
    if (!command) return STATUS_INVALID_PARAMETER;
    micant::oem::xhci::Trb local{};
    if (!g_transport.prepareHubEndpoint(endpoint, packet, interval, speed, ports, 0, false, local, burst, bytes))
        return STATUS_DEVICE_NOT_READY;
    *command = {local.parameter, local.status, local.control}; return STATUS_SUCCESS;
}
int32_t selectDevice(void*, uint8_t slot) {
    return g_transport.selectDevice(slot) ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
}
int32_t forgetDevice(void*, uint8_t slot) {
    return g_transport.forgetDevice(slot) ? STATUS_SUCCESS : kBusy;
}
int32_t submitCommand(void*, const micant::shared::usb::Trb* command, uint64_t* address) {
    if (!command || !address) return STATUS_INVALID_PARAMETER;
    const micant::oem::xhci::Trb local{command->parameter, command->status, command->control};
    return g_transport.submitCommand(local, *address) ? STATUS_SUCCESS : kBusy;
}
int32_t submitControl(void*, uint64_t setup, uint16_t length, bool input, uint64_t* statusAddress) {
    return statusAddress && g_transport.submitControl(setup, length, input, *statusAddress) ? STATUS_SUCCESS : kBusy;
}
const uint8_t* controlBuffer(void*) { return g_transport.controlBuffer(); }
int32_t pollEvent(void*, micant::shared::usb::Trb* event, bool* available) {
    if (!event || !available) return STATUS_INVALID_PARAMETER;
    if (!g_transport.healthy()) return STATUS_DEVICE_NOT_READY;
    micant::oem::xhci::Trb local{};
    *available = g_transport.pollEvent(local);
    if (g_msiInterrupts.exchange(0, std::memory_order_acq_rel) &&
        !g_msiReported.exchange(true, std::memory_order_acq_rel) && g_services && g_services->Log)
        log(*g_services, "usbxhci.sys: native MSI notification delivered for xHCI events");
    if (*available) *event = {local.parameter, local.status, local.control};
    return STATUS_SUCCESS;
}
int32_t prepareInterrupt(void*, uint8_t endpoint, uint16_t packet, uint8_t interval,
                         uint8_t speed, micant::shared::usb::Trb* command) {
    if (!command) return STATUS_INVALID_PARAMETER;
    micant::oem::xhci::Trb local{};
    if (!g_transport.prepareInterruptEndpoint(endpoint, packet, interval, speed, local)) return STATUS_DEVICE_NOT_READY;
    *command = {local.parameter, local.status, local.control};
    return STATUS_SUCCESS;
}
int32_t submitInterrupt(void*, uint64_t* address) {
    return address && g_transport.submitInterrupt(*address) ? STATUS_SUCCESS : kBusy;
}
const uint8_t* interruptBuffer(void*) { return g_transport.interruptBuffer(); }
int32_t prepareBulk(void*, uint8_t input, uint16_t inPacket, uint8_t output, uint16_t outPacket,
                    uint8_t speed, micant::shared::usb::Trb* command) {
    if (!command) return STATUS_INVALID_PARAMETER;
    micant::oem::xhci::Trb local{};
    if (!g_transport.prepareBulkEndpoints(input, inPacket, output, outPacket, speed, local))
        return STATUS_DEVICE_NOT_READY;
    *command = {local.parameter, local.status, local.control};
    return STATUS_SUCCESS;
}
int32_t prepareSuperSpeedInterrupt(void*, uint8_t endpoint, uint16_t packet, uint8_t interval,
                                   uint8_t speed, uint8_t burst, uint16_t bytes, micant::shared::usb::Trb* command) {
    if (!command) return STATUS_INVALID_PARAMETER;
    micant::oem::xhci::Trb local{};
    if (!g_transport.prepareInterruptEndpoint(endpoint, packet, interval, speed, local, burst, bytes))
        return STATUS_DEVICE_NOT_READY;
    *command = {local.parameter, local.status, local.control}; return STATUS_SUCCESS;
}
int32_t prepareSuperSpeedBulk(void*, uint8_t input, uint16_t inPacket, uint8_t inBurst,
                             uint8_t output, uint16_t outPacket, uint8_t outBurst, uint8_t speed,
                             micant::shared::usb::Trb* command) {
    if (!command) return STATUS_INVALID_PARAMETER;
    micant::oem::xhci::Trb local{};
    if (!g_transport.prepareBulkEndpoints(input, inPacket, output, outPacket, speed, local, inBurst, outBurst))
        return STATUS_DEVICE_NOT_READY;
    *command = {local.parameter, local.status, local.control}; return STATUS_SUCCESS;
}
int32_t bulkTransfer(void*, uint8_t endpoint, void* buffer, size_t length,
                     size_t* actual, uint32_t timeoutMs) {
    if (!actual) return STATUS_INVALID_PARAMETER;
    return g_transport.bulkTransfer(endpoint, buffer, length, *actual, timeoutMs)
        ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
}
int32_t bulkReset(void*, uint8_t interfaceNumber, uint8_t input, uint8_t output) {
    return g_transport.bulkResetRecovery(interfaceNumber, input, output)
        ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
}
void unloadHost(DriverObject* driver) noexcept {
    if (!g_services) return;
    if (g_hostToken) {
        if (g_services->UnregisterUsbHost(g_services->Context, g_hostToken) != STATUS_SUCCESS) return;
        g_hostToken = 0;
    } else if (!restoreMsi() || !g_transport.stop()) return;
    g_services = nullptr;
    if (driver) driver->DriverUnload = nullptr;
}

void log(const KernelServices& services, const char* message) noexcept {
    if (services.Log) services.Log(services.Context, message);
}
void logHost(void*, const char* message) noexcept {
    if (g_services && message) log(*g_services, message);
}

bool readConfig(const KernelServices& services, uint8_t bus, uint8_t device,
                uint8_t function, uint16_t offset, uint32_t& value) noexcept {
    return services.PciReadConfig32 && services.PciReadConfig32(
        services.Context, bus, device, function, offset, &value) == STATUS_SUCCESS;
}

bool findXhci(const KernelServices& services, uint32_t firstBdf, uint8_t& busOut,
              uint8_t& deviceOut, uint8_t& functionOut,
              uint64_t& bar0) noexcept {
    for (uint32_t bus = 0; bus <= UINT8_MAX; ++bus) {
        for (uint32_t device = 0; device < 32; ++device) {
            uint32_t id{};
            if (!readConfig(services, static_cast<uint8_t>(bus),
                    static_cast<uint8_t>(device), 0, 0, id)) return false;
            if (static_cast<uint16_t>(id) == UINT16_MAX) continue;
            uint32_t header{};
            if (!readConfig(services, static_cast<uint8_t>(bus),
                    static_cast<uint8_t>(device), 0, kPciHeaderTypeOffset, header)) return false;
            const uint8_t functionCount = ((header >> 16) & 0x80) ? 8 : 1;
            for (uint8_t function = 0; function < functionCount; ++function) {
                const uint32_t bdf = (bus << 8) | (device << 3) | function;
                if (bdf < firstBdf) continue;
                if (function && !readConfig(services, static_cast<uint8_t>(bus),
                        static_cast<uint8_t>(device), function, 0, id)) return false;
                if (static_cast<uint16_t>(id) == UINT16_MAX) continue;
                uint32_t classCode{};
                if (!readConfig(services, static_cast<uint8_t>(bus),
                        static_cast<uint8_t>(device), function, kPciClassOffset, classCode)) return false;
                if ((classCode >> 24) != 0x0C || ((classCode >> 16) & 0xFF) != 0x03 ||
                    ((classCode >> 8) & 0xFF) != 0x30) continue;

                uint32_t low{};
                if (!readConfig(services, static_cast<uint8_t>(bus),
                        static_cast<uint8_t>(device), function, kPciBar0Offset, low)) return false;
                if (low & 1u) continue;
                const uint32_t type = (low >> 1) & 3u;
                if (type != 0 && type != 2) continue;
                bar0 = static_cast<uint64_t>(low & ~0xFu);
                if (type == 2) {
                    uint32_t high{};
                    if (!readConfig(services, static_cast<uint8_t>(bus), static_cast<uint8_t>(device),
                                    function, kPciBar0Offset + 4, high)) return false;
                    bar0 |= uint64_t{high} << 32;
                }
                if (!bar0) continue;
                busOut = static_cast<uint8_t>(bus);
                deviceOut = static_cast<uint8_t>(device);
                functionOut = function;
                return true;
            }
        }
    }
    return false;
}
} // namespace

extern "C" int32_t DriverEntry(micant::boot_driver::DriverObject* driver,
                                const micant::boot_driver::UnicodeString*) {
    using namespace micant::boot_driver;
    if (!driver || driver->Size < sizeof(DriverObject) ||
        driver->Version != DRIVER_ABI_VERSION || !driver->HardwareResources)
        return STATUS_INVALID_PARAMETER;
    const auto& services = *static_cast<const KernelServices*>(driver->HardwareResources);
    if (services.Size < sizeof(KernelServices) ||
        services.Version != KERNEL_SERVICES_VERSION || !services.PciReadConfig32 ||
        !services.PciWriteConfig32 || !services.ClaimPciDevice || !services.ReleasePciDevice ||
        services.DmaVersion != DMA_SERVICES_VERSION || !services.AllocateDmaBuffer ||
        !services.FreeDmaBuffer || services.UsbHostVersion != micant::shared::usb::HostServicesVersion ||
        !services.RegisterUsbHost || !services.UnregisterUsbHost || !services.MapDeviceMemory ||
        !services.ReleaseDeviceMemory || !services.QueryMonotonicClock || !services.DelayMicroseconds) {
        log(services, "usbxhci.sys: KernelServices V2 PCI/PnP/DMA contract unavailable");
        return STATUS_NOT_SUPPORTED;
    }
    if (g_services || g_hostToken) return kBusy;

    uint8_t bus{}, device{}, function{};
    uint64_t bar0{}, claimToken{};
    uint32_t cursor{};
    int32_t lastStatus = STATUS_NOT_SUPPORTED;
    bool claimed = false;
    while (findXhci(services, cursor, bus, device, function, bar0)) {
        cursor = (uint32_t{bus} << 8) | (uint32_t{device} << 3) | function;
        ++cursor;
        uint16_t msix{}; uint32_t tableInfo{};
        PciBarRequestV1 requests[2]{{0, 0, 0, 0x1000}, {0, 0, 0, 0x1000}};
        size_t resourceCount = 1;
        if (findCapability(services, bus, device, function, 0x11, msix) && msix &&
            readConfig(services, bus, device, function, msix + 4, tableInfo)) {
            const auto tableBar = static_cast<uint8_t>(tableInfo & 7u);
            if (tableBar) { requests[1].BarIndex = tableBar; resourceCount = 2; }
        }
        g_claimResourceCount = resourceCount;
        claimToken = 0; g_claimResources[0] = {}; g_claimResources[1] = {};
        const auto claimStatus = services.ClaimPciDevice(services.Context, 0,
            bus, device, function, requests, resourceCount, &claimToken,
            g_claimResources, &g_claimResourceCount);
        if (claimStatus == STATUS_SUCCESS && claimToken && g_claimResourceCount == resourceCount &&
            g_claimResources[0].BarIndex == 0 && g_claimResources[0].PhysicalAddress == bar0 &&
            g_claimResources[0].Length >= 0x1000 && g_claimResources[0].Length <= 16u * 1024u * 1024u) {
            if (g_transport.initialize(services, bus, device, function, claimToken,
                                       g_claimResources[0].PhysicalAddress,
                                       static_cast<size_t>(g_claimResources[0].Length))) {
                claimed = true; break;
            }
            if (!g_transport.stop()) {
                g_services = &services; driver->DriverUnload = unloadHost;
                log(services, "usbxhci.sys: controller rollback remains busy");
                return kBusy;
            }
            claimToken = 0; lastStatus = STATUS_DEVICE_NOT_READY;
            continue;
        }
        lastStatus = claimStatus == STATUS_SUCCESS ? STATUS_NOT_SUPPORTED : claimStatus;
        if (claimToken && services.ReleasePciDevice(services.Context, claimToken) != STATUS_SUCCESS) {
            log(services, "usbxhci.sys: failed to roll back PCI claim");
            return kBusy;
        }
        claimToken = 0;
    }
    if (!claimed) {
        log(services, "usbxhci.sys: no available xHCI controller with safe PCI resources");
        return lastStatus;
    }
    g_services = &services;
    g_bus = bus; g_device = device; g_function = function;
    driver->DriverUnload = unloadHost;
    if (!configureMsi())
        log(services, "usbxhci.sys: PCI MSI/MSI-X unavailable; using bounded xHCI event polling");
    g_host = {};
    g_host.Size = sizeof(g_host); g_host.Version = micant::shared::usb::HostServicesVersion;
    g_host.Context = &g_transport; g_host.Start = startHost; g_host.Stop = stopHost; g_host.Restart = restartHost;
    g_host.PortCount = portCount; g_host.ReadPort = readPort; g_host.ResetPort = resetPort;
    g_host.PortSlotType = portSlotType;
    g_host.PrepareAddressDevice = prepareAddress; g_host.SelectDevice = selectDevice;
    g_host.ForgetDevice = forgetDevice; g_host.SubmitCommand = submitCommand;
    g_host.SubmitControl = submitControl; g_host.ControlBuffer = controlBuffer; g_host.PollEvent = pollEvent;
    g_host.PrepareInterruptEndpoint = prepareInterrupt; g_host.SubmitInterrupt = submitInterrupt;
    g_host.InterruptBuffer = interruptBuffer;
    g_host.PrepareBulkEndpoints = prepareBulk; g_host.BulkTransfer = bulkTransfer;
    g_host.BulkResetRecovery = bulkReset;
    g_host.PrepareSuperSpeedInterrupt = prepareSuperSpeedInterrupt;
    g_host.PrepareSuperSpeedBulk = prepareSuperSpeedBulk;
    g_host.PrepareHubEndpoint = prepareHub; g_host.PrepareAddressDownstream = prepareDownstream;
    g_host.PrepareAddressTopology = prepareTopology; g_host.PrepareEp0Packet = prepareEp0Packet;
    g_host.PrepareSuperSpeedHubEndpoint = prepareSuperSpeedHub;
    g_host.BeginRootPortReset = beginRootReset; g_host.PollRootPortReset = pollRootReset;
    g_host.AcknowledgeRootConnectionChange = acknowledgeRootChange;
    g_host.Log = logHost;
    const auto result = services.RegisterUsbHost(services.Context, &g_host, &g_hostToken);
    if (result != STATUS_SUCCESS || !g_hostToken) {
        unloadHost(driver);
        return result == STATUS_SUCCESS ? STATUS_DEVICE_NOT_READY : result;
    }
    log(services, "usbxhci.sys: native host registered with PCI claimed controller");
    return STATUS_SUCCESS;
}
