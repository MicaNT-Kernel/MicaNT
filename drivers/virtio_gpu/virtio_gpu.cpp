// MicaNT boot-start VirtIO-GPU PCI display driver.
// Implements the VirtIO 1.0 modern PCI transport and 2D scanout protocol.
// The kernel owns window composition; this driver owns the device transport,
// guest-backed scanout resource, and presentation to the host display.

#include <cstddef>
#include <cstdint>
#include "micant/shared/boot_driver.hpp"

extern "C" void* memset(void* destination, int value, size_t count) {
    auto* bytes = static_cast<uint8_t*>(destination);
    for (size_t i = 0; i < count; ++i) bytes[i] = static_cast<uint8_t>(value);
    return destination;
}

namespace {

using namespace micant::boot_driver;

constexpr uint16_t kPciConfigAddress = 0x0CF8;
constexpr uint16_t kPciConfigData = 0x0CFC;
constexpr uint16_t kVirtioVendor = 0x1AF4;
constexpr uint16_t kVirtioGpuDevice = 0x1050;
constexpr uint8_t kPciCapabilityVendor = 0x09;
constexpr uint8_t kVirtioPciCapCommon = 1;
constexpr uint8_t kVirtioPciCapNotify = 2;
constexpr uint8_t kVirtioPciCapDevice = 4;
constexpr uint32_t kVirtioFeatureVersion1 = 1u;
constexpr uint32_t kVirtioStatusAcknowledge = 1;
constexpr uint32_t kVirtioStatusDriver = 2;
constexpr uint32_t kVirtioStatusDriverOk = 4;
constexpr uint32_t kVirtioStatusFeaturesOk = 8;
constexpr uint16_t kQueueSize = 128;
constexpr uint32_t kMaxWidth = 1920;
constexpr uint32_t kMaxHeight = 1080;
constexpr uint32_t kMaxBackingPages = (kMaxWidth * kMaxHeight * 4 + 4095) / 4096;
constexpr uint32_t kResourceId = 1;
constexpr uint32_t kFormatB8G8R8A8Unorm = 1;
constexpr uint16_t kDescNext = 1;
constexpr uint16_t kDescWrite = 2;

constexpr uint32_t kCmdGetDisplayInfo = 0x0100;
constexpr uint32_t kCmdResourceCreate2D = 0x0101;
constexpr uint32_t kCmdResourceUnref = 0x0102;
constexpr uint32_t kCmdSetScanout = 0x0103;
constexpr uint32_t kCmdResourceFlush = 0x0104;
constexpr uint32_t kCmdTransferToHost2D = 0x0105;
constexpr uint32_t kCmdResourceAttachBacking = 0x0106;
constexpr uint32_t kRespOkNoData = 0x1100;
constexpr uint32_t kRespOkDisplayInfo = 0x1101;

struct PciLocation { uint8_t bus, device, function; };
struct PciRegion { uint8_t bar; uint32_t offset, length, notifyMultiplier; };
struct PciCaps { PciRegion common{}, notify{}, device{}; bool hasCommon{}, hasNotify{}, hasDevice{}; };

static inline void out32(uint16_t port, uint32_t value) {
    __asm__ __volatile__("outl %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint32_t in32(uint16_t port) {
    uint32_t value;
    __asm__ __volatile__("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static inline void ioBarrier() { __asm__ __volatile__("mfence" : : : "memory"); }
static inline void cpuRelax() { __asm__ __volatile__("pause"); }

uint32_t pciRead(const PciLocation& loc, uint8_t offset) {
    const uint32_t address = 0x80000000u |
        (static_cast<uint32_t>(loc.bus) << 16) |
        (static_cast<uint32_t>(loc.device) << 11) |
        (static_cast<uint32_t>(loc.function) << 8) |
        (offset & 0xFCu);
    out32(kPciConfigAddress, address);
    return in32(kPciConfigData);
}

void pciWrite(const PciLocation& loc, uint8_t offset, uint32_t value) {
    const uint32_t address = 0x80000000u |
        (static_cast<uint32_t>(loc.bus) << 16) |
        (static_cast<uint32_t>(loc.device) << 11) |
        (static_cast<uint32_t>(loc.function) << 8) |
        (offset & 0xFCu);
    out32(kPciConfigAddress, address);
    out32(kPciConfigData, value);
}

void logHex(const KernelServices& services, const char* label, uint32_t value) {
    if (!services.Log) return;
    static constexpr char digits[] = "0123456789ABCDEF";
    char message[64]{};
    size_t out = 0;
    while (label[out] && out < sizeof(message) - 12) { message[out] = label[out]; ++out; }
    for (int shift = 28; shift >= 0; shift -= 4) message[out++] = digits[(value >> shift) & 0xFu];
    message[out] = '\0';
    services.Log(services.Context, message);
}

uint64_t pciBar(const PciLocation& loc, uint8_t barIndex) {
    const uint8_t offset = static_cast<uint8_t>(0x10 + barIndex * 4);
    const uint32_t low = pciRead(loc, offset);
    if (low & 1u) return 0;
    uint64_t base = low & ~0xFu;
    if (((low >> 1) & 3u) == 2u && barIndex < 5) {
        base |= static_cast<uint64_t>(pciRead(loc, static_cast<uint8_t>(offset + 4))) << 32;
    }
    return base;
}

bool findGpu(PciLocation& result, PciCaps& caps, const KernelServices& services) {
    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t dev = 0; dev < 32; ++dev) {
            PciLocation loc{static_cast<uint8_t>(bus), static_cast<uint8_t>(dev), 0};
            if ((pciRead(loc, 0) & 0xFFFFu) != kVirtioVendor) continue;
            if ((pciRead(loc, 0) >> 16) != kVirtioGpuDevice) continue;
            if (services.Log) services.Log(services.Context, "virtio_gpu.sys: matched PCI vendor/device 1AF4:1050");

            uint16_t command = static_cast<uint16_t>(pciRead(loc, 4));
            command = static_cast<uint16_t>(command | 0x0006u); // memory decode + bus master
            uint32_t commandStatus = pciRead(loc, 4);
            commandStatus = (commandStatus & 0xFFFF0000u) | command;
            pciWrite(loc, 4, commandStatus);

            result = loc;
            if ((pciRead(loc, 6) & (1u << 20)) == 0) {
                if (services.Log) services.Log(services.Context, "virtio_gpu.sys: PCI status reports no capability list");
                return true;
            }
            uint8_t cap = static_cast<uint8_t>(pciRead(loc, 0x34));
            for (uint32_t step = 0; cap >= 0x40 && cap <= 0xFC && step < 48; ++step) {
                const uint32_t capWord = pciRead(loc, cap);
                const uint8_t id = static_cast<uint8_t>(capWord & 0xFFu);
                const uint8_t next = static_cast<uint8_t>((capWord >> 8) & 0xFFu);
                const uint8_t capLength = static_cast<uint8_t>((capWord >> 16) & 0xFFu);
                const uint8_t type = static_cast<uint8_t>((capWord >> 24) & 0xFFu);
                if (id == kPciCapabilityVendor && capLength >= 16) {
                    const uint32_t barAndType = pciRead(loc, static_cast<uint8_t>(cap + 4));
                    const uint8_t bar = static_cast<uint8_t>(barAndType & 0xFFu);
                    const uint8_t cfgType = type; // cfg_type occupies byte 3 of the vendor capability header.
                    PciRegion region{bar, pciRead(loc, static_cast<uint8_t>(cap + 8)),
                                     pciRead(loc, static_cast<uint8_t>(cap + 12)), 0};
                    if (bar < 6 && pciBar(loc, bar) != 0) {
                        if (cfgType == kVirtioPciCapCommon) { caps.common = region; caps.hasCommon = true; }
                        else if (cfgType == kVirtioPciCapNotify && capLength >= 20) {
                            region.notifyMultiplier = pciRead(loc, static_cast<uint8_t>(cap + 16));
                            caps.notify = region; caps.hasNotify = true;
                        } else if (cfgType == kVirtioPciCapDevice) { caps.device = region; caps.hasDevice = true; }
                    } else if (services.Log) {
                        services.Log(services.Context, "virtio_gpu.sys: vendor capability references invalid BAR");
                    }
                } else if (id == kPciCapabilityVendor && services.Log) {
                    services.Log(services.Context, "virtio_gpu.sys: short vendor capability rejected");
                }
                if (next == 0 || next == cap) break;
                cap = next;
                (void)type;
            }
            return true;
        }
    }
    return false;
}

struct VirtqDesc { uint64_t address; uint32_t length; uint16_t flags; uint16_t next; };
struct VirtqAvail { volatile uint16_t flags, index; uint16_t ring[kQueueSize]; uint16_t usedEvent; };
struct VirtqUsedElement { uint32_t id, length; };
struct VirtqUsed { volatile uint16_t flags, index; VirtqUsedElement ring[kQueueSize]; uint16_t availEvent; };

struct CommandHeader { uint32_t type, flags; uint64_t fenceId; uint32_t contextId; uint8_t ringIndex, padding[3]; };
struct Rect { uint32_t x, y, width, height; };
struct CommandResourceCreate2D { CommandHeader header; uint32_t resourceId, format, width, height; };
struct CommandSetScanout { CommandHeader header; Rect rect; uint32_t scanoutId, resourceId; };
struct CommandTransfer2D { CommandHeader header; Rect rect; uint64_t offset; uint32_t resourceId, padding; };
struct CommandFlush { CommandHeader header; Rect rect; uint32_t resourceId, padding; };
struct CommandUnref { CommandHeader header; uint32_t resourceId, padding; };
struct CommandAttachPrefix { CommandHeader header; uint32_t resourceId, entries; };
struct MemoryEntry { uint64_t address; uint32_t length, padding; };
struct ResponseHeader { uint32_t type, flags; uint64_t fenceId; uint32_t contextId; uint8_t ringIndex, padding[3]; };
struct DisplayInfo { Rect rect; uint32_t enabled, flags; };
struct ResponseDisplayInfo { ResponseHeader header; DisplayInfo scanouts[16]; };

struct alignas(4096) DeviceState {
    VirtqDesc descriptors[kQueueSize]{};
    alignas(4096) VirtqAvail available{};
    alignas(4096) VirtqUsed used{};
    alignas(4096) uint8_t frame[kMaxWidth * kMaxHeight * 4]{};
    alignas(16) uint8_t command[sizeof(CommandAttachPrefix) + kMaxBackingPages * sizeof(MemoryEntry)]{};
    alignas(16) ResponseHeader response{};
    uint32_t width{}, height{}, pitch{}, pages{};
    uint64_t framePhysicalPages[kMaxBackingPages]{};
    volatile uint8_t* common{};
    volatile uint8_t* notify{};
    uint16_t queueSize{};
    uint16_t lastUsed{};
    uint16_t notifyOffset{};
    uint32_t notifyMultiplier{};
    const KernelServices* services{};
    volatile uint32_t queueLock{};
    bool initialized{};
};

DeviceState g_device{};

template <typename T> T mmioRead(volatile uint8_t* base, size_t offset) {
    return *reinterpret_cast<volatile T*>(base + offset);
}
template <typename T> void mmioWrite(volatile uint8_t* base, size_t offset, T value) {
    *reinterpret_cast<volatile T*>(base + offset) = value;
}

uint64_t physicalAddress(const void* address) {
    return g_device.services && g_device.services->TranslateVirtualAddress
        ? g_device.services->TranslateVirtualAddress(g_device.services->Context, address) : 0;
}

bool runCommand(const void* request, uint32_t requestBytes, ResponseHeader* response,
                uint32_t responseBytes, uint32_t expectedResponse) {
    if (!g_device.initialized && !g_device.common) return false;
    while (__atomic_exchange_n(&g_device.queueLock, 1u, __ATOMIC_ACQUIRE) != 0) cpuRelax();
    const uint64_t requestPa = physicalAddress(request);
    const uint64_t responsePa = physicalAddress(response);
    if (!requestPa || !responsePa || requestBytes == 0 || responseBytes == 0) {
        __atomic_store_n(&g_device.queueLock, 0u, __ATOMIC_RELEASE);
        return false;
    }
    g_device.response.type = 0;

    const uint16_t first = 0;
    const uint16_t second = 1;
    g_device.descriptors[first] = {requestPa, requestBytes, kDescNext, second};
    g_device.descriptors[second] = {responsePa, responseBytes, kDescWrite, 0};
    const uint64_t descPa = physicalAddress(g_device.descriptors);
    const uint64_t availPa = physicalAddress(&g_device.available);
    const uint64_t usedPa = physicalAddress(&g_device.used);
    if (!descPa || !availPa || !usedPa) {
        __atomic_store_n(&g_device.queueLock, 0u, __ATOMIC_RELEASE);
        return false;
    }

    const uint16_t slot = static_cast<uint16_t>(g_device.available.index % g_device.queueSize);
    g_device.available.ring[slot] = first;
    ioBarrier();
    g_device.available.index = static_cast<uint16_t>(g_device.available.index + 1);
    ioBarrier();
    mmioWrite<uint16_t>(g_device.notify,
        static_cast<size_t>(g_device.notifyOffset) * g_device.notifyMultiplier, 0);

    for (uint32_t spin = 0; spin < 20000000u; ++spin) {
        ioBarrier();
        if (g_device.used.index != g_device.lastUsed) {
            g_device.lastUsed = g_device.used.index;
            ioBarrier();
            const bool succeeded = response->type == expectedResponse;
            __atomic_store_n(&g_device.queueLock, 0u, __ATOMIC_RELEASE);
            return succeeded;
        }
        cpuRelax();
    }
    (void)descPa; (void)availPa; (void)usedPa;
    __atomic_store_n(&g_device.queueLock, 0u, __ATOMIC_RELEASE);
    return false;
}

void initHeader(CommandHeader& header, uint32_t type) {
    header = {};
    header.type = type;
}

bool sendNoData(const void* request, uint32_t requestSize) {
    return runCommand(request, requestSize, &g_device.response,
                      sizeof(g_device.response), kRespOkNoData);
}

bool initializeQueue(const PciLocation& location, const PciCaps& caps,
                     const KernelServices& services) {
    const uint64_t commonPhysical = pciBar(location, caps.common.bar) + caps.common.offset;
    const uint64_t notifyPhysical = pciBar(location, caps.notify.bar) + caps.notify.offset;
    g_device.common = static_cast<volatile uint8_t*>(services.MapDeviceMemory(
        services.Context, commonPhysical, caps.common.length));
    g_device.notify = static_cast<volatile uint8_t*>(services.MapDeviceMemory(
        services.Context, notifyPhysical, caps.notify.length));
    // The device-specific PCI configuration BAR may be a 64-bit VRAM aperture
    // above the kernel's current direct MMIO window; 2D command transport does
    // not access it, so only map common configuration and queue notifications.
    if (!g_device.common || !g_device.notify) {
        if (services.Log) {
            services.Log(services.Context, "virtio_gpu.sys: PCI BAR MMIO mapping failed");
        }
        return false;
    }

    mmioWrite<uint8_t>(g_device.common, 20, 0); // reset device status
    for (uint32_t spin = 0; spin < 1000000u && mmioRead<uint8_t>(g_device.common, 20) != 0; ++spin) cpuRelax();
    mmioWrite<uint8_t>(g_device.common, 20, kVirtioStatusAcknowledge);
    mmioWrite<uint8_t>(g_device.common, 20, kVirtioStatusAcknowledge | kVirtioStatusDriver);

    mmioWrite<uint32_t>(g_device.common, 0, 0);
    const uint32_t featuresLow = mmioRead<uint32_t>(g_device.common, 4);
    mmioWrite<uint32_t>(g_device.common, 0, 1);
    const uint32_t featuresHigh = mmioRead<uint32_t>(g_device.common, 4);
    if ((featuresHigh & kVirtioFeatureVersion1) == 0) {
        if (services.Log) logHex(services, "virtio_gpu.sys: offered high feature bits ", featuresHigh);
        return false;
    }

    // Negotiate only VERSION_1. The host therefore uses its standard 2D scanout
    // path and remains compatible with QEMU builds that do not ship virglrenderer.
    (void)featuresLow;
    mmioWrite<uint32_t>(g_device.common, 8, 0);
    mmioWrite<uint32_t>(g_device.common, 12, 0);
    mmioWrite<uint32_t>(g_device.common, 8, 1);
    mmioWrite<uint32_t>(g_device.common, 12, kVirtioFeatureVersion1);
    mmioWrite<uint8_t>(g_device.common, 20,
        kVirtioStatusAcknowledge | kVirtioStatusDriver | kVirtioStatusFeaturesOk);
    if ((mmioRead<uint8_t>(g_device.common, 20) & kVirtioStatusFeaturesOk) == 0) {
        if (services.Log) services.Log(services.Context, "virtio_gpu.sys: device rejected negotiated feature set");
        return false;
    }

    mmioWrite<uint16_t>(g_device.common, 22, 0); // control queue
    const uint16_t offeredSize = mmioRead<uint16_t>(g_device.common, 24);
    if (offeredSize < 2) {
        if (services.Log) logHex(services, "virtio_gpu.sys: control queue size ", offeredSize);
        return false;
    }
    g_device.queueSize = offeredSize < kQueueSize ? offeredSize : kQueueSize;
    mmioWrite<uint16_t>(g_device.common, 24, g_device.queueSize);
    g_device.notifyOffset = mmioRead<uint16_t>(g_device.common, 30);
    g_device.notifyMultiplier = caps.notify.notifyMultiplier;

    const uint64_t descPa = physicalAddress(g_device.descriptors);
    const uint64_t availPa = physicalAddress(&g_device.available);
    const uint64_t usedPa = physicalAddress(&g_device.used);
    if (!descPa || !availPa || !usedPa) {
        if (services.Log) services.Log(services.Context, "virtio_gpu.sys: queue ring address translation failed");
        return false;
    }
    mmioWrite<uint64_t>(g_device.common, 32, descPa);
    mmioWrite<uint64_t>(g_device.common, 40, availPa);
    mmioWrite<uint64_t>(g_device.common, 48, usedPa);
    ioBarrier();
    mmioWrite<uint16_t>(g_device.common, 28, 1);
    mmioWrite<uint8_t>(g_device.common, 20,
        kVirtioStatusAcknowledge | kVirtioStatusDriver | kVirtioStatusFeaturesOk | kVirtioStatusDriverOk);
    g_device.services = &services;
    g_device.initialized = true;
    if (services.Log) services.Log(services.Context, "virtio_gpu.sys: control queue ready");
    return true;
}

bool createScanout(uint32_t width, uint32_t height) {
    ResponseDisplayInfo displayInfo{};
    CommandHeader displayRequest{};
    initHeader(displayRequest, kCmdGetDisplayInfo);
    if (!runCommand(&displayRequest, sizeof(displayRequest), &displayInfo.header,
                    sizeof(displayInfo), kRespOkDisplayInfo)) return false;
    if (displayInfo.scanouts[0].enabled == 0 || displayInfo.scanouts[0].rect.width == 0 ||
        displayInfo.scanouts[0].rect.height == 0) return false;

    const uint32_t maxWidth = displayInfo.scanouts[0].rect.width < kMaxWidth
        ? displayInfo.scanouts[0].rect.width : kMaxWidth;
    const uint32_t maxHeight = displayInfo.scanouts[0].rect.height < kMaxHeight
        ? displayInfo.scanouts[0].rect.height : kMaxHeight;
    g_device.width = width > 0 && width < maxWidth ? width : maxWidth;
    g_device.height = height > 0 && height < maxHeight ? height : maxHeight;
    if (g_device.width == 0 || g_device.height == 0) return false;
    g_device.pitch = g_device.width;
    const uint32_t frameBytes = g_device.width * g_device.height * 4;
    g_device.pages = (frameBytes + 4095) / 4096;
    if (g_device.pages > kMaxBackingPages) return false;

    for (uint32_t page = 0; page < g_device.pages; ++page) {
        const uint64_t pa = physicalAddress(g_device.frame + static_cast<size_t>(page) * 4096);
        if (!pa) return false;
        g_device.framePhysicalPages[page] = pa & ~0xFFFULL;
    }

    CommandResourceCreate2D create{};
    initHeader(create.header, kCmdResourceCreate2D);
    create.resourceId = kResourceId;
    create.format = kFormatB8G8R8A8Unorm;
    create.width = g_device.width;
    create.height = g_device.height;
    if (!sendNoData(&create, sizeof(create))) return false;

    auto* attach = reinterpret_cast<CommandAttachPrefix*>(g_device.command);
    initHeader(attach->header, kCmdResourceAttachBacking);
    attach->resourceId = kResourceId;
    attach->entries = g_device.pages;
    auto* entries = reinterpret_cast<MemoryEntry*>(g_device.command + sizeof(CommandAttachPrefix));
    uint32_t remaining = frameBytes;
    for (uint32_t page = 0; page < g_device.pages; ++page) {
        entries[page] = {g_device.framePhysicalPages[page], 4096, 0};
        remaining = remaining > 4096 ? remaining - 4096 : 0;
    }
    if (!sendNoData(g_device.command, sizeof(CommandAttachPrefix) + g_device.pages * sizeof(MemoryEntry))) {
        CommandUnref unref{};
        initHeader(unref.header, kCmdResourceUnref);
        unref.resourceId = kResourceId;
        sendNoData(&unref, sizeof(unref));
        return false;
    }

    CommandSetScanout scanout{};
    initHeader(scanout.header, kCmdSetScanout);
    scanout.rect = {0, 0, g_device.width, g_device.height};
    scanout.scanoutId = 0;
    scanout.resourceId = kResourceId;
    if (!sendNoData(&scanout, sizeof(scanout))) return false;

    CommandTransfer2D transfer{};
    initHeader(transfer.header, kCmdTransferToHost2D);
    transfer.rect = {0, 0, g_device.width, g_device.height};
    transfer.resourceId = kResourceId;
    if (!sendNoData(&transfer, sizeof(transfer))) return false;
    CommandFlush flush{};
    initHeader(flush.header, kCmdResourceFlush);
    flush.rect = {0, 0, g_device.width, g_device.height};
    flush.resourceId = kResourceId;
    return sendNoData(&flush, sizeof(flush));
}

bool presentFrame(void*, const uint32_t* pixels, uint32_t width, uint32_t height,
                  uint32_t pitch, int32_t dstX, int32_t dstY) {
    if (!g_device.initialized || !pixels || width == 0 || height == 0 || pitch < width) return false;
    const int64_t srcLeft = dstX < 0 ? -static_cast<int64_t>(dstX) : 0;
    const int64_t srcTop = dstY < 0 ? -static_cast<int64_t>(dstY) : 0;
    const int64_t left = dstX > 0 ? dstX : 0;
    const int64_t top = dstY > 0 ? dstY : 0;
    int64_t copyWidth = static_cast<int64_t>(width) - srcLeft;
    int64_t copyHeight = static_cast<int64_t>(height) - srcTop;
    if (left + copyWidth > g_device.width) copyWidth = g_device.width - left;
    if (top + copyHeight > g_device.height) copyHeight = g_device.height - top;
    if (copyWidth <= 0 || copyHeight <= 0) return false;
    for (int64_t row = 0; row < copyHeight; ++row) {
        const uint32_t* src = pixels + (srcTop + row) * pitch + srcLeft;
        uint32_t* dst = reinterpret_cast<uint32_t*>(g_device.frame) + (top + row) * g_device.pitch + left;
        for (int64_t col = 0; col < copyWidth; ++col) dst[col] = src[col];
    }
    ioBarrier();
    CommandTransfer2D transfer{};
    initHeader(transfer.header, kCmdTransferToHost2D);
    transfer.rect = {0, 0, g_device.width, g_device.height};
    transfer.resourceId = kResourceId;
    if (!sendNoData(&transfer, sizeof(transfer))) return false;
    CommandFlush flush{};
    initHeader(flush.header, kCmdResourceFlush);
    flush.rect = {0, 0, g_device.width, g_device.height};
    flush.resourceId = kResourceId;
    return sendNoData(&flush, sizeof(flush));
}

} // namespace

extern "C" int32_t DriverEntry(micant::boot_driver::DriverObject* driver,
                                const micant::boot_driver::UnicodeString*) {
    using namespace micant::boot_driver;
    if (!driver || driver->Size < sizeof(DriverObject) ||
        driver->Version != DRIVER_ABI_VERSION || !driver->HardwareResources || !driver->Display) {
        return STATUS_INVALID_PARAMETER;
    }
    const auto& services = *static_cast<const KernelServices*>(driver->HardwareResources);
    if (services.Size < sizeof(KernelServices) || services.Version != DRIVER_ABI_VERSION ||
        !services.TranslateVirtualAddress || !services.MapDeviceMemory) return STATUS_INVALID_PARAMETER;
    g_device.services = &services;
    PciLocation location{};
    PciCaps caps{};
    if (!findGpu(location, caps, services)) {
        if (services.Log) services.Log(services.Context, "virtio_gpu.sys: modern VirtIO-GPU PCI device not found");
        return STATUS_NOT_SUPPORTED;
    }
    if (!caps.hasCommon || !caps.hasNotify || !caps.hasDevice || !services.MapDeviceMemory) {
        if (services.Log) {
            if (!caps.hasCommon) services.Log(services.Context, "virtio_gpu.sys: PCI common configuration capability missing");
            if (!caps.hasNotify) services.Log(services.Context, "virtio_gpu.sys: PCI queue notification capability missing");
            if (!caps.hasDevice) services.Log(services.Context, "virtio_gpu.sys: PCI device configuration capability missing");
            if (!services.MapDeviceMemory) services.Log(services.Context, "virtio_gpu.sys: kernel MMIO mapping service missing");
        }
        return STATUS_NOT_SUPPORTED;
    }
    if (!initializeQueue(location, caps, services)) {
        if (services.Log) services.Log(services.Context, "virtio_gpu.sys: modern PCI transport initialization failed");
        return STATUS_DEVICE_NOT_READY;
    }
    const uint32_t requestedWidth = services.BootWidth;
    const uint32_t requestedHeight = services.BootHeight;
    if (!createScanout(requestedWidth, requestedHeight)) {
        g_device.initialized = false;
        if (services.Log) services.Log(services.Context, "virtio_gpu.sys: 2D resource or scanout setup failed");
        return STATUS_DEVICE_NOT_READY;
    }

    driver->Flags |= DRIVER_FLAG_DISPLAY;
    driver->DeviceExtension = &g_device;
    driver->Display->Size = sizeof(DisplayDevice);
    driver->Display->Version = DRIVER_ABI_VERSION;
    driver->Display->Width = g_device.width;
    driver->Display->Height = g_device.height;
    driver->Display->Pitch = g_device.pitch;
    driver->Display->PixelFormat = kFormatB8G8R8A8Unorm;
    driver->Display->Context = &g_device;
    driver->Display->PresentFrame = presentFrame;
    if (services.Log) services.Log(services.Context, "virtio_gpu.sys: VirtIO 1.0 2D scanout initialized");
    return STATUS_SUCCESS;
}
