// MicaNT boot-start VirtIO-net OEM driver preflight.
//
// This image owns the VirtIO transport; it is deliberately not part of
// ntoskrnl_core_baremetal. Bring-up remains fail-closed until Io/PnP can claim
// the PCI function and the kernel provides DMA allocation plus an NDIS packet
// registration/completion interface. In particular, TranslateDmaRange alone
// does not allocate or pin device-visible queue memory.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <atomic>
#include "micant/shared/boot_driver.hpp"
#include "micant/shared/ndis_adapter_abi.hpp"

namespace {
using namespace micant::boot_driver;

constexpr uint16_t kVirtioVendor = 0x1AF4;
constexpr uint16_t kVirtioNetModern = 0x1041;
constexpr uint8_t kPciCapabilityVendor = 0x09;
constexpr uint8_t kVirtioPciCapCommon = 1;
constexpr uint8_t kVirtioPciCapNotify = 2;
constexpr uint8_t kVirtioPciCapDevice = 4;
constexpr size_t kMaxCapabilities = 48;
constexpr size_t kMaxBars = 6;
constexpr size_t kQueueLimit = 16;
constexpr size_t kFrameCapacity = 2048;
constexpr size_t kNetHeaderSize = 12;
constexpr int32_t kBusy = static_cast<int32_t>(0x80000011u);

struct VirtqDesc { uint64_t address; uint32_t length; uint16_t flags, next; };
struct VirtqUsedElement { uint32_t id, length; };
struct alignas(4096) VirtqMemory {
    VirtqDesc descriptors[kQueueLimit];
    struct { uint16_t flags; volatile uint16_t index; uint16_t ring[kQueueLimit]; uint16_t event; } available;
    struct { uint16_t flags; volatile uint16_t index; VirtqUsedElement ring[kQueueLimit]; uint16_t event; } used;
    alignas(16) uint8_t buffers[kQueueLimit][kFrameCapacity];
};
struct alignas(4096) DmaMemory { VirtqMemory rx, tx; };
static_assert(sizeof(VirtqDesc) == 16);

struct DeviceState {
    const KernelServices* services{};
    uint16_t bus{}, device{}, function{};
    uint64_t claim{}, dmaToken{}, dmaAddress{}, adapter{};
    DmaMemory* dma{};
    void* mappings[kMaxBars]{};
    size_t mappingLengths[kMaxBars]{};
    size_t mappingCount{};
    volatile uint8_t* common{};
    volatile uint8_t* notify{};
    volatile uint8_t* deviceConfig{};
    uint32_t notifyLength{}, notifyMultiplier{};
    uint16_t notifyOffset[2]{}, queueSize[2]{}, rxUsed{}, txUsed{};
    bool txInUse[kQueueLimit]{};
    bool commandEnabled{}, started{}, faulted{};
    uint16_t originalCommand{};
    micant::shared::ndis::ReceivePacketRoutine receive{};
    void* receiveContext{};
    micant::shared::ndis::AdapterServicesV1 adapterServices{};
};
DeviceState g_device{};
std::atomic_flag g_busy = ATOMIC_FLAG_INIT;
struct Guard {
    bool acquired{!g_busy.test_and_set(std::memory_order_acquire)};
    ~Guard() { if (acquired) g_busy.clear(std::memory_order_release); }
};

struct PciLocation {
    uint16_t Bus{}, Device{}, Function{};
};

struct VirtioRegion {
    uint8_t Bar{};
    uint32_t Offset{};
    uint32_t Length{};
    uint32_t NotifyMultiplier{};
    bool Present{};
};

struct VirtioCapabilities {
    VirtioRegion Common{}, Notify{}, Device{};
};

void log(const KernelServices& services, const char* message) noexcept {
    if (services.Log) services.Log(services.Context, message);
}

bool readConfig(const KernelServices& services, PciLocation location,
                uint16_t offset, uint32_t& value) noexcept {
    if (!services.PciReadConfig32 || (offset & 3u) || offset > 0xFC)
        return false;
    value = UINT32_MAX;
    return services.PciReadConfig32(services.Context, location.Bus,
        location.Device, location.Function, offset, &value) == STATUS_SUCCESS;
}

bool readConfigByte(const KernelServices& services, PciLocation location,
                    uint16_t offset, uint8_t& value) noexcept {
    uint32_t word{};
    if (!readConfig(services, location, static_cast<uint16_t>(offset & ~3u), word))
        return false;
    value = static_cast<uint8_t>(word >> ((offset & 3u) * 8));
    return true;
}

bool findVirtioNet(const KernelServices& services, PciLocation& result) noexcept {
    for (uint16_t bus = 0; bus <= UINT8_MAX; ++bus) {
        for (uint16_t device = 0; device < 32; ++device) {
            PciLocation location{bus, device, 0};
            uint32_t id{};
            if (!readConfig(services, location, 0, id)) return false;
            if (static_cast<uint16_t>(id) == UINT16_MAX) continue;

            uint32_t headerWord{};
            if (!readConfig(services, location, 0x0C, headerWord)) return false;
            const bool multiFunction = (headerWord & (1u << 23)) != 0;
            const uint16_t functionCount = multiFunction ? 8 : 1;
            for (uint16_t function = 0; function < functionCount; ++function) {
                location.Function = function;
                if (function && !readConfig(services, location, 0, id)) return false;
                if (static_cast<uint16_t>(id) != kVirtioVendor) continue;
                const uint16_t vendorDevice = static_cast<uint16_t>(id >> 16);
                if (vendorDevice == kVirtioNetModern) {
                    result = location;
                    return true;
                }
            }
        }
    }
    return false;
}

bool recordRegion(VirtioRegion& region, uint8_t bar, uint32_t offset,
                  uint32_t length, uint32_t notifyMultiplier = 0) noexcept {
    if (region.Present || bar >= kMaxBars || !length ||
        offset > UINT32_MAX - length)
        return false;
    region = {bar, offset, length, notifyMultiplier, true};
    return true;
}

bool parseVirtioCapabilities(const KernelServices& services, PciLocation location,
                             VirtioCapabilities& result) noexcept {
    uint32_t statusWord{};
    if (!readConfig(services, location, 0x04, statusWord) ||
        (statusWord & (1u << 20)) == 0)
        return false;
    uint8_t capability{};
    if (!readConfigByte(services, location, 0x34, capability)) return false;

    bool visited[256]{};
    for (size_t step = 0; step < kMaxCapabilities; ++step) {
        if (!capability) break;
        if (capability < 0x40 || capability > 0xFC || (capability & 3u) ||
            visited[capability])
            return false;
        visited[capability] = true;

        uint32_t header{};
        if (!readConfig(services, location, capability, header)) return false;
        const uint8_t id = static_cast<uint8_t>(header);
        const uint8_t next = static_cast<uint8_t>(header >> 8);
        const uint8_t length = static_cast<uint8_t>(header >> 16);
        const uint8_t cfgType = static_cast<uint8_t>(header >> 24);
        if (id == kPciCapabilityVendor) {
            // The complete advertised capability must fit in the 256-byte
            // conventional configuration space, including unknown variants.
            if (length < 16 || length > 256u - capability) return false;
            uint32_t barAndOffset{}, regionOffset{}, regionLength{};
            if (!readConfig(services, location, capability + 4, barAndOffset) ||
                !readConfig(services, location, capability + 8, regionOffset) ||
                !readConfig(services, location, capability + 12, regionLength))
                return false;
            const uint8_t bar = static_cast<uint8_t>(barAndOffset);
            if (cfgType == kVirtioPciCapCommon) {
                if (regionLength < 56) return false;
                if (!recordRegion(result.Common, bar, regionOffset, regionLength)) return false;
            } else if (cfgType == kVirtioPciCapDevice) {
                if (!recordRegion(result.Device, bar, regionOffset, regionLength)) return false;
            } else if (cfgType == kVirtioPciCapNotify) {
                uint32_t notifyMultiplier{};
                if (length < 20 || regionLength < 2 ||
                    !readConfig(services, location, capability + 16,
                                notifyMultiplier) ||
                    !recordRegion(result.Notify, bar, regionOffset, regionLength,
                                  notifyMultiplier))
                    return false;
            }
        }
        if (next && (next < 0x40 || next > 0xFC || (next & 3u))) return false;
        capability = next;
    }
    return !capability && result.Common.Present && result.Notify.Present &&
           result.Device.Present;
}

bool buildBarRequests(const VirtioCapabilities& capabilities,
                      PciBarRequestV1 (&requests)[kMaxBars],
                      size_t& requestCount) noexcept {
    requestCount = 0;
    const VirtioRegion* regions[]{&capabilities.Common, &capabilities.Notify,
                                  &capabilities.Device};
    for (const auto* region : regions) {
        if (!region->Present || region->Bar >= kMaxBars || !region->Length ||
            region->Offset > UINT64_MAX - region->Length)
            return false;
        const uint64_t requiredLength = static_cast<uint64_t>(region->Offset) +
                                        region->Length;
        size_t index = 0;
        while (index < requestCount && requests[index].BarIndex != region->Bar)
            ++index;
        if (index == requestCount) {
            if (requestCount == kMaxBars) return false;
            requests[requestCount++] = {region->Bar, 0, 0, requiredLength};
        } else if (requiredLength > requests[index].MinimumLength) {
            requests[index].MinimumLength = requiredLength;
        }
    }
    return requestCount != 0;
}

bool validateClaimedBars(const VirtioCapabilities& caps,
                         const PciBarRequestV1* requests,
                         const PciBarResourceV1* resources,
                         size_t count) noexcept {
    if (!requests || !resources || !count || count > kMaxBars) return false;
    for (size_t i = 0; i < count; ++i) {
        if (resources[i].BarIndex != requests[i].BarIndex ||
            !resources[i].PhysicalAddress ||
            resources[i].Length < requests[i].MinimumLength ||
            resources[i].PhysicalAddress > UINT64_MAX - resources[i].Length)
            return false;
    }
    const VirtioRegion* regions[]{&caps.Common, &caps.Notify, &caps.Device};
    for (const auto* region : regions) {
        bool found = false;
        for (size_t i = 0; i < count; ++i) {
            if (resources[i].BarIndex != region->Bar) continue;
            if (region->Offset > resources[i].Length ||
                region->Length > resources[i].Length - region->Offset)
                return false;
            found = true;
            break;
        }
        if (!found) return false;
    }
    return true;
}

template<typename T> T mmioRead(volatile uint8_t* base, size_t offset) noexcept {
    return *reinterpret_cast<volatile T*>(base + offset);
}
template<typename T> void mmioWrite(volatile uint8_t* base, size_t offset, T value) noexcept {
    *reinterpret_cast<volatile T*>(base + offset) = value;
}
bool pciWriteCommand(uint16_t command) noexcept {
    return g_device.services->PciWriteConfig32(g_device.services->Context, g_device.bus,
        g_device.device, g_device.function, 4, command) == STATUS_SUCCESS;
}
bool waitReset() noexcept {
    uint64_t start{}, frequency{};
    const auto* s = g_device.services;
    if (!s->QueryMonotonicClock || !s->DelayMicroseconds ||
        s->QueryMonotonicClock(s->Context, &start, &frequency) != STATUS_SUCCESS || !frequency) return false;
    for (unsigned n = 0; n < 10000; ++n) {
        if (mmioRead<uint8_t>(g_device.common, 20) == 0) return true;
        uint64_t now{}, current{};
        if (s->QueryMonotonicClock(s->Context, &now, &current) != STATUS_SUCCESS ||
            current != frequency || now < start || now - start >= frequency / 10 ||
            s->DelayMicroseconds(s->Context, 50) != STATUS_SUCCESS) return false;
    }
    return false;
}
bool waitPciDrain() noexcept {
    const auto* s = g_device.services;
    const PciLocation location{g_device.bus, g_device.device, g_device.function};
    uint32_t command{}, capWord{};
    if (!readConfig(*s, location, 4, command) || !(command & (1u << 20)) ||
        !readConfig(*s, location, 0x34, capWord)) return false;
    uint8_t cap = static_cast<uint8_t>(capWord);
    uint16_t deviceStatus{};
    for (unsigned n = 0; cap && n < 48; ++n) {
        uint32_t entry{};
        if (cap < 0x40 || cap > 0xf4 || (cap & 3) || !readConfig(*s, location, cap, entry)) return false;
        if ((entry & 0xff) == 0x10) { deviceStatus = cap + 8; break; }
        cap = static_cast<uint8_t>(entry >> 8);
    }
    if (!deviceStatus) return false;
    uint64_t start{}, frequency{};
    if (s->QueryMonotonicClock(s->Context, &start, &frequency) != STATUS_SUCCESS || !frequency) return false;
    for (unsigned n = 0; n < 10000; ++n) {
        uint32_t status{};
        if (!readConfig(*s, location, deviceStatus, status)) return false;
        if (!(status & (1u << 21))) { __atomic_thread_fence(__ATOMIC_ACQUIRE); return true; }
        uint64_t now{}, current{};
        if (s->QueryMonotonicClock(s->Context, &now, &current) != STATUS_SUCCESS ||
            current != frequency || now < start || now - start >= frequency / 10 ||
            s->DelayMicroseconds(s->Context, 50) != STATUS_SUCCESS) return false;
    }
    return false;
}
bool stopDevice() noexcept {
    auto* s = g_device.services;
    if (!s) return true;
    g_device.started = false;
    g_device.receive = nullptr;
    if (g_device.commandEnabled) {
        mmioWrite<uint8_t>(g_device.common, 20, 0);
        if (!waitReset() || !pciWriteCommand(g_device.originalCommand & ~uint16_t{7}) || !waitPciDrain()) return false;
        g_device.commandEnabled = false;
    }
    if (g_device.dmaToken) {
        if (s->FreeDmaBuffer(s->Context, g_device.dmaToken) != STATUS_SUCCESS) return false;
        g_device.dmaToken = 0; g_device.dma = nullptr;
    }
    while (g_device.mappingCount) {
        const size_t i = g_device.mappingCount - 1;
        if (!s->ReleaseDeviceMemory(s->Context, g_device.mappings[i], g_device.mappingLengths[i])) return false;
        g_device.mappings[i] = nullptr; --g_device.mappingCount;
    }
    if (g_device.claim) {
        if (s->ReleasePciDevice(s->Context, g_device.claim) != STATUS_SUCCESS) return false;
        g_device.claim = 0;
    }
    return true;
}
bool setupQueue(unsigned queueIndex, VirtqMemory& queue, uint64_t queueAddress) noexcept {
    auto* common = g_device.common;
    mmioWrite<uint16_t>(common, 22, static_cast<uint16_t>(queueIndex));
    const uint16_t offered = mmioRead<uint16_t>(common, 24);
    if (!offered || mmioRead<uint16_t>(common, 28)) return false;
    uint16_t size = 1;
    while (size < kQueueLimit && static_cast<unsigned>(size) * 2 <= offered) size *= 2;
    if (size < 2) return false;
    g_device.queueSize[queueIndex] = size;
    mmioWrite<uint16_t>(common, 24, size);
    mmioWrite<uint16_t>(common, 26, UINT16_MAX);
    g_device.notifyOffset[queueIndex] = mmioRead<uint16_t>(common, 30);
    const uint64_t offset = uint64_t{g_device.notifyOffset[queueIndex]} * g_device.notifyMultiplier;
    if (offset > g_device.notifyLength || g_device.notifyLength - offset < 2 || (offset & 1)) return false;
    // This miniport polls completions from a passive worker; no IRQ route is
    // installed, so request suppression before publishing the queue.
    queue.available.flags = 1;
    mmioWrite<uint64_t>(common, 32, queueAddress + offsetof(VirtqMemory, descriptors));
    mmioWrite<uint64_t>(common, 40, queueAddress + offsetof(VirtqMemory, available));
    mmioWrite<uint64_t>(common, 48, queueAddress + offsetof(VirtqMemory, used));
    if (queueIndex == 0) {
        for (uint16_t i = 0; i < size; ++i) {
            queue.descriptors[i] = {queueAddress + offsetof(VirtqMemory, buffers) + uint64_t{i} * kFrameCapacity,
                                    kFrameCapacity, 2, 0};
            queue.available.ring[i] = i;
        }
        __atomic_thread_fence(__ATOMIC_RELEASE);
        queue.available.index = size;
    }
    __atomic_thread_fence(__ATOMIC_RELEASE);
    mmioWrite<uint16_t>(common, 28, 1);
    return mmioRead<uint16_t>(common, 28) == 1;
}
void notifyQueue(unsigned index) noexcept {
    const auto offset = uint64_t{g_device.notifyOffset[index]} * g_device.notifyMultiplier;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    mmioWrite<uint16_t>(g_device.notify, static_cast<size_t>(offset), static_cast<uint16_t>(index));
}

bool collectTx() noexcept {
    auto& queue = g_device.dma->tx;
    const uint16_t end = queue.used.index;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    const uint16_t size = g_device.queueSize[1];
    if (static_cast<uint16_t>(end - g_device.txUsed) > size) return false;
    while (g_device.txUsed != end) {
        const auto id = queue.used.ring[g_device.txUsed % size].id;
        if (id >= size || !g_device.txInUse[id]) return false;
        g_device.txInUse[id] = false;
        ++g_device.txUsed;
    }
    return true;
}
int32_t sendPacket(void*, const uint8_t* frame, size_t length) {
    if (!frame || !length || length > kFrameCapacity - kNetHeaderSize) return STATUS_INVALID_PARAMETER;
    Guard guard;
    if (!guard.acquired) return kBusy;
    if (!g_device.started || g_device.faulted || (mmioRead<uint8_t>(g_device.common, 20) & 0xc0u) ||
        !collectTx()) { g_device.faulted = true; return STATUS_DEVICE_NOT_READY; }
    auto& queue = g_device.dma->tx;
    const uint16_t size = g_device.queueSize[1];
    uint16_t id = 0;
    while (id < size && g_device.txInUse[id]) ++id;
    if (id == size) return kBusy;
    std::memset(queue.buffers[id], 0, kNetHeaderSize);
    std::memcpy(queue.buffers[id] + kNetHeaderSize, frame, length);
    const uint64_t queueAddress = g_device.dmaAddress + offsetof(DmaMemory, tx);
    queue.descriptors[id] = {queueAddress + offsetof(VirtqMemory, buffers) + uint64_t{id} * kFrameCapacity,
                             static_cast<uint32_t>(length + kNetHeaderSize), 0, 0};
    g_device.txInUse[id] = true;
    const uint16_t next = queue.available.index;
    queue.available.ring[next % size] = id;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    queue.available.index = static_cast<uint16_t>(next + 1);
    notifyQueue(1);
    return STATUS_SUCCESS;
}
int32_t setReceive(void*, micant::shared::ndis::ReceivePacketRoutine routine, void* context) {
    Guard guard;
    if (!guard.acquired) return kBusy;
    g_device.receive = routine; g_device.receiveContext = context;
    return STATUS_SUCCESS;
}
int32_t pollReceive(void*) {
    Guard guard;
    if (!guard.acquired) return kBusy;
    if (!g_device.started || g_device.faulted || (mmioRead<uint8_t>(g_device.common, 20) & 0xc0u))
        return STATUS_DEVICE_NOT_READY;
    auto& queue = g_device.dma->rx;
    const uint16_t size = g_device.queueSize[0], end = queue.used.index;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    if (static_cast<uint16_t>(end - g_device.rxUsed) > size || !collectTx()) {
        g_device.faulted = true; return STATUS_DEVICE_NOT_READY;
    }
    bool rearmed = false;
    uint32_t seen = 0;
    while (g_device.rxUsed != end) {
        const auto entry = queue.used.ring[g_device.rxUsed % size];
        if (entry.id >= size || entry.length < kNetHeaderSize || entry.length > kFrameCapacity ||
            (seen & (1u << entry.id))) {
            g_device.faulted = true; return STATUS_DEVICE_NOT_READY;
        }
        seen |= 1u << entry.id;
        if (g_device.receive && entry.length > kNetHeaderSize)
            g_device.receive(g_device.receiveContext, queue.buffers[entry.id] + kNetHeaderSize,
                             entry.length - kNetHeaderSize);
        const uint16_t next = queue.available.index;
        queue.available.ring[next % size] = static_cast<uint16_t>(entry.id);
        __atomic_thread_fence(__ATOMIC_RELEASE);
        queue.available.index = static_cast<uint16_t>(next + 1);
        ++g_device.rxUsed;
        rearmed = true;
    }
    if (rearmed) notifyQueue(0);
    return STATUS_SUCCESS;
}
int32_t stopAdapter(void*) {
    Guard guard;
    if (!guard.acquired) return kBusy;
    return stopDevice() ? STATUS_SUCCESS : kBusy;
}
void unloadDriver(DriverObject* driver) noexcept {
    if (!g_device.services) return;
    if (g_device.adapter) {
        if (g_device.services->UnregisterNetworkAdapter(g_device.services->Context, g_device.adapter) != STATUS_SUCCESS) return;
        g_device.adapter = 0;
    } else if (!stopDevice()) return;
    g_device.services = nullptr;
    if (driver) driver->DriverUnload = nullptr;
}

} // namespace

extern "C" int32_t DriverEntry(DriverObject* driver,
                                const UnicodeString*) noexcept {
    if (!driver || driver->Size < sizeof(DriverObject) ||
        driver->Version != DRIVER_ABI_VERSION || !driver->HardwareResources)
        return STATUS_INVALID_PARAMETER;
    const auto& services = *static_cast<const KernelServices*>(driver->HardwareResources);
    if (services.Size < sizeof(KernelServices) ||
        services.Version != KERNEL_SERVICES_VERSION || !services.PciReadConfig32 ||
        !services.PciWriteConfig32 || !services.ClaimPciDevice || !services.ReleasePciDevice ||
        !services.MapDeviceMemory || !services.ReleaseDeviceMemory ||
        services.DmaVersion != DMA_SERVICES_VERSION || !services.AllocateDmaBuffer || !services.FreeDmaBuffer ||
        services.NetworkAdapterVersion != micant::shared::ndis::AdapterServicesVersion ||
        !services.RegisterNetworkAdapter || !services.UnregisterNetworkAdapter ||
        !services.QueryMonotonicClock || !services.DelayMicroseconds) {
        log(services, "virtio_net.sys: KernelServices V2 PCI/PnP services unavailable");
        return STATUS_NOT_SUPPORTED;
    }

    PciLocation location{};
    if (!findVirtioNet(services, location)) {
        log(services, "virtio_net.sys: no VirtIO-net PCI function found");
        return STATUS_NOT_SUPPORTED;
    }
    VirtioCapabilities capabilities{};
    if (!parseVirtioCapabilities(services, location, capabilities)) {
        log(services, "virtio_net.sys: malformed or incomplete modern PCI capabilities");
        return STATUS_NOT_SUPPORTED;
    }

    PciBarRequestV1 requests[kMaxBars]{};
    size_t requestCount{};
    if (!buildBarRequests(capabilities, requests, requestCount))
        return STATUS_INVALID_PARAMETER;
    PciBarResourceV1 resources[kMaxBars]{};
    size_t resourceCount = requestCount;
    uint64_t claimToken{};
    const int32_t claimStatus = services.ClaimPciDevice(services.Context,
        0, location.Bus, location.Device, location.Function, requests,
        requestCount, &claimToken, resources, &resourceCount);
    if (claimStatus != STATUS_SUCCESS || !claimToken ||
        resourceCount != requestCount ||
        !validateClaimedBars(capabilities, requests, resources, resourceCount)) {
        if (claimToken) (void)services.ReleasePciDevice(services.Context, claimToken);
        log(services, "virtio_net.sys: Io/PnP denied or returned invalid BAR resources");
        return claimStatus == STATUS_SUCCESS ? STATUS_NOT_SUPPORTED : claimStatus;
    }

    if (g_device.services) {
        (void)services.ReleasePciDevice(services.Context, claimToken);
        return kBusy;
    }
    g_device = {};
    g_device.services = &services;
    g_device.bus = location.Bus; g_device.device = location.Device; g_device.function = location.Function;
    g_device.claim = claimToken;
    driver->DriverUnload = unloadDriver;
    const auto fail = [&]() noexcept {
        unloadDriver(driver);
        return STATUS_DEVICE_NOT_READY;
    };
    volatile uint8_t* bars[kMaxBars]{};
    for (size_t i = 0; i < requestCount; ++i) {
        auto* mapping = services.MapDeviceMemory(services.Context, resources[i].PhysicalAddress,
                                               static_cast<size_t>(requests[i].MinimumLength));
        if (!mapping) return fail();
        g_device.mappings[g_device.mappingCount] = mapping;
        g_device.mappingLengths[g_device.mappingCount++] = static_cast<size_t>(requests[i].MinimumLength);
        bars[resources[i].BarIndex] = static_cast<volatile uint8_t*>(mapping);
    }
    if ((capabilities.Common.Offset & 7u) || (capabilities.Notify.Offset & 1u) ||
        capabilities.Device.Length < 6) return fail();
    g_device.common = bars[capabilities.Common.Bar] + capabilities.Common.Offset;
    g_device.notify = bars[capabilities.Notify.Bar] + capabilities.Notify.Offset;
    g_device.deviceConfig = bars[capabilities.Device.Bar] + capabilities.Device.Offset;
    g_device.notifyLength = capabilities.Notify.Length;
    g_device.notifyMultiplier = capabilities.Notify.NotifyMultiplier;
    void* memory{};
    if (services.AllocateDmaBuffer(services.Context, claimToken, sizeof(DmaMemory), alignof(DmaMemory),
                                   UINT64_MAX, &memory, &g_device.dmaAddress,
                                   &g_device.dmaToken) != STATUS_SUCCESS || !memory ||
        !g_device.dmaToken || !g_device.dmaAddress ||
        g_device.dmaAddress > UINT64_MAX - sizeof(DmaMemory)) return fail();
    g_device.dma = static_cast<DmaMemory*>(memory);
    uint32_t command{};
    if (!readConfig(services, location, 4, command) || (command & 7u)) return fail();
    g_device.originalCommand = static_cast<uint16_t>(command);
    if (!pciWriteCommand(g_device.originalCommand | 6u)) return fail();
    g_device.commandEnabled = true;
    mmioWrite<uint8_t>(g_device.common, 20, 0);
    if (!waitReset()) return fail();
    mmioWrite<uint8_t>(g_device.common, 20, 1);
    mmioWrite<uint8_t>(g_device.common, 20, 3);
    mmioWrite<uint32_t>(g_device.common, 0, 0);
    const uint32_t featuresLow = mmioRead<uint32_t>(g_device.common, 4);
    mmioWrite<uint32_t>(g_device.common, 0, 1);
    const uint32_t featuresHigh = mmioRead<uint32_t>(g_device.common, 4);
    if (!(featuresLow & (1u << 5)) || !(featuresHigh & 1u)) return fail();
    mmioWrite<uint32_t>(g_device.common, 8, 0);
    mmioWrite<uint32_t>(g_device.common, 12, 1u << 5);
    mmioWrite<uint32_t>(g_device.common, 8, 1);
    mmioWrite<uint32_t>(g_device.common, 12, 1u);
    mmioWrite<uint8_t>(g_device.common, 20, 11);
    if ((mmioRead<uint8_t>(g_device.common, 20) & 0xc8u) != 8u) return fail();
    if (!setupQueue(0, g_device.dma->rx, g_device.dmaAddress + offsetof(DmaMemory, rx)) ||
        !setupQueue(1, g_device.dma->tx, g_device.dmaAddress + offsetof(DmaMemory, tx))) return fail();
    g_device.adapterServices = {};
    auto& adapter = g_device.adapterServices;
    adapter.Size = sizeof(adapter); adapter.Version = micant::shared::ndis::AdapterServicesVersion;
    adapter.Context = &g_device; adapter.Mtu = 1500; adapter.LinkState = 1;
    bool stable = false;
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        const auto generation = mmioRead<uint8_t>(g_device.common, 21);
        for (size_t i = 0; i < 6; ++i) adapter.MacAddress[i] = mmioRead<uint8_t>(g_device.deviceConfig, i);
        if (generation == mmioRead<uint8_t>(g_device.common, 21)) { stable = true; break; }
    }
    if (!stable || (adapter.MacAddress[0] & 1u)) return fail();
    bool macNonzero = false;
    for (auto byte : adapter.MacAddress) macNonzero |= byte != 0;
    if (!macNonzero) return fail();
    adapter.SendPacket = sendPacket; adapter.SetReceiveHandler = setReceive;
    adapter.PollReceive = pollReceive; adapter.Stop = stopAdapter;
    mmioWrite<uint8_t>(g_device.common, 20, 15);
    if ((mmioRead<uint8_t>(g_device.common, 20) & 0xcfu) != 15) return fail();
    g_device.started = true;
    notifyQueue(0);
    const auto registered = services.RegisterNetworkAdapter(services.Context, &adapter, &g_device.adapter);
    if (registered != STATUS_SUCCESS || !g_device.adapter) return fail();
    return STATUS_SUCCESS;
}
