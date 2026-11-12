#include "micant/shared/boot_driver.hpp"
#include "micant/shared/ndis_adapter_abi.hpp"
#include "i210_controller.hpp"
#include <atomic>

namespace {

using namespace micant::boot_driver;

constexpr uint16_t kIntelVendor = 0x8086;
constexpr uint16_t kI210Device = 0x1533;
constexpr uint64_t kI210RegisterBarLength = 0x20000;
const KernelServices* g_services{};
std::atomic_flag g_controllerBusy = ATOMIC_FLAG_INIT;
micant::shared::ndis::ReceivePacketRoutine g_receive{};
void* g_receiveContext{};
uint64_t g_adapterHandle{};
micant::shared::ndis::AdapterServicesV1 g_adapter{};
constexpr int32_t StatusDeviceBusy = static_cast<int32_t>(0x80000011u);

struct ControllerGuard {
    bool acquired{!g_controllerBusy.test_and_set(std::memory_order_acquire)};
    ~ControllerGuard() { if (acquired) g_controllerBusy.clear(std::memory_order_release); }
};

void indicateFrame(void*, const uint8_t* frame, size_t length) noexcept {
    if (g_receive) g_receive(g_receiveContext, frame, length);
}
int32_t sendFrame(void*, const uint8_t* frame, size_t length) {
    if (!frame || !length || length > UINT16_MAX) return STATUS_INVALID_PARAMETER;
    ControllerGuard guard;
    if (!guard.acquired) return StatusDeviceBusy;
    return micant::oem::i210::BareMetalI210Driver::get().sendPacket(frame, static_cast<uint16_t>(length))
        ? STATUS_SUCCESS : STATUS_DEVICE_NOT_READY;
}
int32_t setReceive(void*, micant::shared::ndis::ReceivePacketRoutine routine, void* context) {
    ControllerGuard guard;
    if (!guard.acquired) return StatusDeviceBusy;
    g_receive = routine;
    g_receiveContext = context;
    micant::oem::i210::BareMetalI210Driver::get().registerReceiveCallback(
        nullptr, routine ? indicateFrame : nullptr);
    return STATUS_SUCCESS;
}
int32_t pollReceive(void*) {
    ControllerGuard guard;
    if (!guard.acquired) return StatusDeviceBusy;
    micant::oem::i210::BareMetalI210Driver::get().pollRx();
    return STATUS_SUCCESS;
}
int32_t stopAdapter(void*) {
    ControllerGuard guard;
    if (!guard.acquired) return StatusDeviceBusy;
    g_receive = nullptr;
    g_receiveContext = nullptr;
    auto& controller = micant::oem::i210::BareMetalI210Driver::get();
    controller.registerReceiveCallback(nullptr, nullptr);
    return controller.shutdown() ? STATUS_SUCCESS : StatusDeviceBusy;
}

uint32_t controllerRead(uint32_t bdf, uint16_t offset) noexcept {
    uint32_t value = UINT32_MAX;
    if (g_services) (void)g_services->PciReadConfig32(g_services->Context, bdf >> 8,
        (bdf >> 3) & 31, bdf & 7, offset, &value);
    return value;
}
bool controllerWrite(uint32_t bdf, uint16_t offset, uint32_t value) noexcept {
    return g_services && g_services->PciWriteConfig32 &&
        g_services->PciWriteConfig32(g_services->Context, bdf >> 8, (bdf >> 3) & 31,
                                     bdf & 7, offset, value) == STATUS_SUCCESS;
}
bool controllerClaim(uint32_t bdf, uint8_t index, uint64_t length, uint64_t* token,
                     uint64_t* address, uint64_t* actualLength) noexcept {
    if (!g_services || !token || !address || !actualLength) return false;
    PciBarRequestV1 request{index, 0, 0, length}; PciBarResourceV1 resource{}; size_t count = 1;
    const auto status = g_services->ClaimPciDevice(g_services->Context, 0, bdf >> 8,
        (bdf >> 3) & 31, bdf & 7, &request, 1, token, &resource, &count);
    if (status != STATUS_SUCCESS || count != 1) return false;
    *address = resource.PhysicalAddress; *actualLength = resource.Length;
    return resource.BarIndex == index;
}
bool controllerRelease(uint64_t token) noexcept {
    return g_services && g_services->ReleasePciDevice(g_services->Context, token) == STATUS_SUCCESS;
}
volatile uint32_t* controllerMap(uint64_t address, size_t length) noexcept {
    return g_services && g_services->MapDeviceMemory
        ? static_cast<volatile uint32_t*>(g_services->MapDeviceMemory(g_services->Context, address, length)) : nullptr;
}
bool controllerUnmap(volatile uint32_t* address, size_t length) noexcept {
    return g_services && g_services->ReleaseDeviceMemory &&
        g_services->ReleaseDeviceMemory(g_services->Context, const_cast<uint32_t*>(address), length);
}
bool controllerAllocate(uint64_t claim, size_t size, size_t alignment, void** cpu,
                        uint64_t* device, uint64_t* token) noexcept {
    return g_services && g_services->Size >= sizeof(KernelServices) &&
        g_services->DmaVersion == DMA_SERVICES_VERSION && g_services->AllocateDmaBuffer &&
        g_services->AllocateDmaBuffer(g_services->Context, claim, size, alignment, UINT64_MAX,
                                      cpu, device, token) == STATUS_SUCCESS;
}
bool controllerFree(uint64_t token) noexcept {
    return g_services && g_services->Size >= sizeof(KernelServices) && g_services->FreeDmaBuffer &&
        g_services->FreeDmaBuffer(g_services->Context, token) == STATUS_SUCCESS;
}
bool controllerClock(uint64_t* counter, uint64_t* frequency) noexcept {
    return g_services && g_services->QueryMonotonicClock &&
        g_services->QueryMonotonicClock(g_services->Context, counter, frequency) == STATUS_SUCCESS;
}
bool controllerDelay(uint32_t microseconds) noexcept {
    return g_services && g_services->DelayMicroseconds &&
        g_services->DelayMicroseconds(g_services->Context, microseconds) == STATUS_SUCCESS;
}

micant::oem::i210::HardwareServices controllerServices() noexcept {
    return {controllerRead, controllerWrite, controllerClaim, controllerRelease,
            controllerMap, controllerUnmap, controllerAllocate, controllerFree,
            controllerClock, controllerDelay};
}

void unloadController(DriverObject* driver) noexcept {
    if (!g_services) return;
    if (g_adapterHandle) {
        if (g_services->UnregisterNetworkAdapter(g_services->Context, g_adapterHandle) != STATUS_SUCCESS)
            return;
        g_adapterHandle = 0;
    } else if (!micant::oem::i210::BareMetalI210Driver::get().shutdown()) {
        return;
    }
    g_services = nullptr;
    if (driver) driver->DriverUnload = nullptr;
}

struct PciLocation {
    uint16_t Bus{};
    uint16_t Device{};
    uint16_t Function{};
};

bool validUnicodeString(const UnicodeString* name) noexcept {
    return !name || (name->Length <= name->MaximumLength &&
        (name->Length == 0 || name->Buffer != nullptr) &&
        (name->Length % sizeof(wchar_t)) == 0 &&
        (name->MaximumLength % sizeof(wchar_t)) == 0);
}

void log(const KernelServices& services, const char* message) noexcept {
    if (services.Log) services.Log(services.Context, message);
}

bool readPci(const KernelServices& services, PciLocation location,
             uint16_t offset, uint32_t& value) noexcept {
    if (!services.PciReadConfig32 || offset > 0xFC || (offset & 3u)) return false;
    value = UINT32_MAX;
    return services.PciReadConfig32(services.Context, location.Bus,
        location.Device, location.Function, offset, &value) == STATUS_SUCCESS;
}

bool findI210(const KernelServices& services, PciLocation& result) noexcept {
    for (uint16_t bus = 0; bus <= UINT8_MAX; ++bus) {
        for (uint16_t device = 0; device < 32; ++device) {
            PciLocation location{bus, device, 0};
            uint32_t identity{};
            if (!readPci(services, location, 0, identity)) return false;
            if (static_cast<uint16_t>(identity) == UINT16_MAX) continue;

            uint32_t header{};
            if (!readPci(services, location, 0x0C, header)) return false;
            const uint16_t functionCount = (header & (1u << 23)) ? 8 : 1;
            for (uint16_t function = 0; function < functionCount; ++function) {
                location.Function = function;
                if (function && !readPci(services, location, 0, identity)) return false;
                if (static_cast<uint16_t>(identity) == kIntelVendor &&
                    static_cast<uint16_t>(identity >> 16) == kI210Device) {
                    result = location;
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

// The OEM image never scans or programs hardware after claim failure. PCI
// discovery is read-only; all BAR ownership must come from Io/PnP.
extern "C" int32_t DriverEntry(DriverObject* driverObject,
                               const UnicodeString* registryPath) noexcept {
    if (!driverObject || driverObject->Size < sizeof(DriverObject) ||
        driverObject->Version != DRIVER_ABI_VERSION ||
        !validUnicodeString(registryPath)) return STATUS_INVALID_PARAMETER;

    const auto* services = static_cast<const KernelServices*>(driverObject->HardwareResources);
    if (!services || services->Size < sizeof(KernelServices) ||
        services->Version != KERNEL_SERVICES_VERSION) return STATUS_INVALID_PARAMETER;
    if (!services->PciReadConfig32 || !services->PciWriteConfig32 || !services->ClaimPciDevice ||
        !services->ReleasePciDevice || !services->AllocateDmaBuffer || !services->FreeDmaBuffer ||
        services->DmaVersion != DMA_SERVICES_VERSION ||
        services->NetworkAdapterVersion != micant::shared::ndis::AdapterServicesVersion ||
        !services->RegisterNetworkAdapter || !services->UnregisterNetworkAdapter) {
        log(*services, "i210.sys: PCI, DMA or NDIS services unavailable");
        return STATUS_NOT_SUPPORTED;
    }
    if (g_adapterHandle) return StatusDeviceBusy;
    if (g_services) {
        if (!micant::oem::i210::BareMetalI210Driver::get().shutdown()) return StatusDeviceBusy;
        g_services = nullptr;
    }
    g_services = services;
    driverObject->DriverUnload = unloadController;
    auto& controller = micant::oem::i210::BareMetalI210Driver::get();
    if (!controller.initialize(controllerServices())) {
        unloadController(driverObject);
        log(*services, "i210.sys: controller startup failed");
        return STATUS_DEVICE_NOT_READY;
    }
    g_adapter = {};
    g_adapter.Size = sizeof(g_adapter);
    g_adapter.Version = micant::shared::ndis::AdapterServicesVersion;
    g_adapter.Context = &controller;
    g_adapter.Mtu = 1500;
    g_adapter.LinkState = controller.isLinkUp() ? 1 : 0;
    g_adapter.SpeedBps = controller.getLinkSpeedBps();
    std::memcpy(g_adapter.MacAddress, controller.getMac(), sizeof(g_adapter.MacAddress));
    g_adapter.SendPacket = sendFrame;
    g_adapter.SetReceiveHandler = setReceive;
    g_adapter.PollReceive = pollReceive;
    g_adapter.Stop = stopAdapter;
    const int32_t result = services->RegisterNetworkAdapter(services->Context, &g_adapter, &g_adapterHandle);
    if (result != STATUS_SUCCESS || !g_adapterHandle) {
        unloadController(driverObject);
        return result == STATUS_SUCCESS ? STATUS_DEVICE_NOT_READY : result;
    }
    return STATUS_SUCCESS;
}
