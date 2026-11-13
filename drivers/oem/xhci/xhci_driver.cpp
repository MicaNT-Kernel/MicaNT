// Standalone boot-start xHCI OEM driver entry point.
// Controller startup remains disabled until Io/PnP claim and device-safe DMA
// allocation services are available through KernelServices V2.
#include <cstdint>
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

int32_t startHost(void*) { return g_transport.start() ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY; }
int32_t stopHost(void*) { return g_transport.stop() ? STATUS_SUCCESS : kBusy; }
int32_t restartHost(void*) {
    if (!g_services || !g_transport.stop()) return kBusy;
    PciBarRequestV1 request{0, 0, 0, 0x1000};
    PciBarResourceV1 resource{};
    size_t count = 1;
    uint64_t claim{};
    const auto status = g_services->ClaimPciDevice(g_services->Context, 0, g_bus, g_device,
        g_function, &request, 1, &claim, &resource, &count);
    if (status != STATUS_SUCCESS || !claim || count != 1 || resource.BarIndex != 0 ||
        !resource.PhysicalAddress || resource.Length < 0x1000 || resource.Length > 16u * 1024u * 1024u) {
        if (claim) (void)g_services->ReleasePciDevice(g_services->Context, claim);
        return STATUS_DEVICE_NOT_READY;
    }
    if (!g_transport.initialize(*g_services, g_bus, g_device, g_function, claim,
                                resource.PhysicalAddress, static_cast<size_t>(resource.Length)) ||
        !g_transport.start()) {
        return g_transport.stop() ? STATUS_DEVICE_NOT_READY : kBusy;
    }
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
    } else if (!g_transport.stop()) return;
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
    const PciBarRequestV1 request{0, 0, 0, 0x1000};
    PciBarResourceV1 resource{};
    bool claimed = false;
    while (findXhci(services, cursor, bus, device, function, bar0)) {
        cursor = (uint32_t{bus} << 8) | (uint32_t{device} << 3) | function;
        ++cursor;
        size_t resourceCount = 1;
        claimToken = 0; resource = {};
        const auto claimStatus = services.ClaimPciDevice(services.Context, 0,
            bus, device, function, &request, 1, &claimToken, &resource, &resourceCount);
        if (claimStatus == STATUS_SUCCESS && claimToken && resourceCount == 1 &&
            resource.BarIndex == 0 && resource.PhysicalAddress == bar0 &&
            resource.Length >= 0x1000 && resource.Length <= 16u * 1024u * 1024u) {
            if (g_transport.initialize(services, bus, device, function, claimToken,
                                       resource.PhysicalAddress, static_cast<size_t>(resource.Length))) {
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
