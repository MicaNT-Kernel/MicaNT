#include "micant/km/drivers/usb/usb_host_baremetal.hpp"
#include "micant/km/drivers/usb/usb_baremetal.hpp"
#include "micant/km/drivers/usb/usbstor_baremetal.hpp"
#include "micant/km/drivers/input/hid_boot_baremetal.hpp"
#include "micant/km/core/passive_work.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/fsrtl/fat_boot.hpp"
#include <atomic>

namespace micant::usb::baremetal {
namespace {
using shared::usb::Trb;
using Host = shared::usb::HostServicesV1;
constexpr int32_t success = 0;
constexpr int32_t busy = static_cast<int32_t>(0x80000011u);
enum class Phase : uint8_t {
    Empty, NeedEnable, WaitEnable, NeedAddress, WaitAddress, DeviceDescriptor,
    ConfigurationHeader, ConfigurationBody, SetConfiguration, ReportDescriptor,
    NeedEndpoint, WaitEndpoint, SetProtocol, HidRunning, StorageInitialize,
    StorageReady, Disconnect, NeedDisable, WaitDisable, DrainDisabled, Quarantined
};
struct Device {
    Phase phase{Phase::Empty};
    uint8_t port{}, speed{}, slot{};
    bool removing{}, storageDevice{};
    uint16_t controlLength{};
    uint64_t deadline{}, commandAddress{}, transferAddress{}, mountToken{};
    Handle blockHandle{};
    Trb prepared{};
    HidInterface hid{};
    StorageInterface storageInterface{};
    km::hid::BootHidCollection collection{};
    MassStorage storage{};
    Trb events[16]{};
    uint8_t eventHead{}, eventCount{};
};
struct Bus {
    const void* owner{};
    uint64_t token{};
    Host host{};
    ke::passive_work::PeriodicItem poll{};
    std::atomic_flag hostGate = ATOMIC_FLAG_INIT;
    Device devices[4]{};
};
Bus bus{};
uint64_t nextToken{1};
struct HostGuard {
    bool held{!bus.hostGate.test_and_set(std::memory_order_acquire)};
    ~HostGuard() { if (held) bus.hostGate.clear(std::memory_order_release); }
};
bool armDeadline(Device& d, uint64_t duration = 20'000'000) noexcept {
    uint64_t now{};
    if (!ke::KeQueryUptime100ns(now) || now > UINT64_MAX - duration) return false;
    d.deadline = now + duration;
    return true;
}
uint64_t setup(uint8_t type, uint8_t request, uint16_t value, uint16_t index, uint16_t length) noexcept {
    return uint64_t{type} | (uint64_t{request} << 8) | (uint64_t{value} << 16) |
           (uint64_t{index} << 32) | (uint64_t{length} << 48);
}
bool select(const Device& d) noexcept {
    return d.slot && bus.host.SelectDevice(bus.host.Context, d.slot) == success;
}
bool beginControl(Device& d, uint8_t type, uint8_t request, uint16_t value,
                  uint16_t index, uint16_t length, Phase next) noexcept {
    if (!select(d) || !armDeadline(d) || bus.host.SubmitControl(bus.host.Context,
        setup(type, request, value, index, length), length, length != 0, &d.transferAddress) != success)
        return false;
    d.controlLength = length;
    d.phase = next;
    return true;
}
NtStatus bulkTransfer(void* context, uint8_t endpoint, void* buffer, size_t length,
                      size_t& actual, uint32_t timeout) noexcept {
    (void)context; // The BOT transaction holds hostGate and selected its slot.
    return static_cast<NtStatus>(bus.host.BulkTransfer(bus.host.Context, endpoint, buffer,
                                                       length, &actual, timeout));
}
NtStatus bulkReset(void* context, uint8_t interfaceNumber, uint8_t input, uint8_t output) noexcept {
    (void)context;
    return static_cast<NtStatus>(bus.host.BulkResetRecovery(bus.host.Context, interfaceNumber, input, output));
}
bool beginBulk(void* context) noexcept {
    auto& d = *static_cast<Device*>(context);
    if (bus.hostGate.test_and_set(std::memory_order_acquire)) return false;
    if (!select(d)) { bus.hostGate.clear(std::memory_order_release); return false; }
    return true;
}
void endBulk(void*) noexcept { bus.hostGate.clear(std::memory_order_release); }
bool enqueue(Device& d, const Trb& event) noexcept {
    if (d.eventCount == 16) { d.phase = Phase::Disconnect; return false; }
    d.events[(d.eventHead + d.eventCount++) % 16] = event;
    return true;
}
bool pop(Device& d, Trb& event) noexcept {
    if (!d.eventCount) return false;
    event = d.events[d.eventHead];
    d.eventHead = static_cast<uint8_t>((d.eventHead + 1) % 16);
    --d.eventCount;
    return true;
}
bool commandWaitPhase(Phase phase) noexcept {
    return phase == Phase::WaitEnable || phase == Phase::WaitAddress ||
           phase == Phase::WaitEndpoint || phase == Phase::WaitDisable;
}
void routeEvents() noexcept {
    for (unsigned budget = 0; budget < 128; ++budget) {
        Trb event{}; bool available{};
        if (bus.host.PollEvent(bus.host.Context, &event, &available) != success) {
            for (auto& d : bus.devices) if (d.phase != Phase::Empty) d.phase = Phase::Disconnect;
            return;
        }
        if (!available) return;
        const uint32_t type = (event.control >> 10) & 63u;
        if (type == 33) {
            const uint64_t address = event.parameter & ~uint64_t{15};
            for (auto& d : bus.devices) if (commandWaitPhase(d.phase) && d.commandAddress == address) {
                (void)enqueue(d, event); break;
            }
        } else if (type == 32) {
            const uint8_t slot = static_cast<uint8_t>(event.control >> 24);
            for (auto& d : bus.devices) if (d.slot == slot && slot && d.phase != Phase::Empty) {
                (void)enqueue(d, event); break;
            }
        }
    }
}
void resetDevice(Device& d) noexcept {
    d.phase = Phase::Empty; d.port = d.speed = d.slot = 0; d.removing = d.storageDevice = false;
    d.controlLength = 0; d.deadline = d.commandAddress = d.transferAddress = d.mountToken = 0;
    d.blockHandle = 0; d.prepared = {}; d.hid = {}; d.storageInterface = {};
    d.eventHead = d.eventCount = 0;
}
void process(Device& d) noexcept {
    if (d.phase == Phase::Empty || d.phase == Phase::Quarantined ||
        d.phase == Phase::StorageInitialize || d.phase == Phase::StorageReady) return;
    if (d.phase == Phase::Disconnect) {
        if (d.storageDevice) {
            d.storage.detach();
            if (d.mountToken) {
                if (fsrtl::fat::unmount(&d, d.mountToken) != NtStatus::Success) return;
                d.mountToken = 0;
            }
            if (d.blockHandle) {
                if (sys::NtClose(d.blockHandle) != NtStatus::Success) return;
                d.blockHandle = 0;
            }
            if (!d.storage.canDestroy()) return;
        }
        if (!d.collection.disconnect()) return;
        if (!d.slot) { resetDevice(d); return; }
        d.phase = Phase::NeedDisable;
        if (!armDeadline(d)) d.phase = Phase::Quarantined;
    }
    if (d.phase == Phase::NeedEnable || d.phase == Phase::NeedAddress ||
        d.phase == Phase::NeedEndpoint || d.phase == Phase::NeedDisable) {
        Trb command = d.prepared;
        if (d.phase == Phase::NeedEnable) command = {0, 0, (9u << 10) |
            (uint32_t{bus.host.PortSlotType(bus.host.Context, d.port)} << 16)};
        if (d.phase == Phase::NeedDisable) command = {0, 0, (10u << 10) | (uint32_t{d.slot} << 24)};
        const auto result = bus.host.SubmitCommand(bus.host.Context, &command, &d.commandAddress);
        if (result == busy) return;
        if (result != success || !armDeadline(d)) { d.phase = Phase::Quarantined; return; }
        d.phase = d.phase == Phase::NeedEnable ? Phase::WaitEnable :
                  d.phase == Phase::NeedAddress ? Phase::WaitAddress :
                  d.phase == Phase::NeedEndpoint ? Phase::WaitEndpoint : Phase::WaitDisable;
        return;
    }
    if (d.phase == Phase::DrainDisabled) {
        if (bus.host.ForgetDevice(bus.host.Context, d.slot) == success) resetDevice(d);
        return;
    }
    Trb event{};
    while (pop(d, event)) {
        const uint32_t type = (event.control >> 10) & 63u;
        if (commandWaitPhase(d.phase)) {
            if (type != 33 || (event.parameter & ~uint64_t{15}) != d.commandAddress) continue;
            if ((event.status >> 24) != 1) { d.phase = Phase::Disconnect; return; }
            if (d.phase == Phase::WaitEnable) {
                d.slot = static_cast<uint8_t>(event.control >> 24);
                if (!d.slot) { d.phase = Phase::Quarantined; return; }
                if (d.removing) { d.phase = Phase::Disconnect; return; }
                if (bus.host.PrepareAddressDevice(bus.host.Context, d.slot, d.port, d.speed, &d.prepared) != success) {
                    d.phase = Phase::Disconnect; return;
                }
                d.phase = armDeadline(d) ? Phase::NeedAddress : Phase::Quarantined;
            } else if (d.phase == Phase::WaitAddress) {
                if (!beginControl(d, 0x80, 6, 0x0100, 0, 18, Phase::DeviceDescriptor)) d.phase = Phase::Disconnect;
            } else if (d.phase == Phase::WaitDisable) {
                d.phase = Phase::DrainDisabled;
                if (!armDeadline(d)) d.phase = Phase::Quarantined;
            } else if (d.storageDevice) {
                d.phase = Phase::StorageInitialize;
            } else if (!beginControl(d, 0x21, 0x0b, 0, d.hid.interfaceNumber, 0, Phase::SetProtocol)) {
                d.phase = Phase::Disconnect;
            }
            return;
        }
        if (type != 32 || (event.parameter & ~uint64_t{15}) != d.transferAddress ||
            static_cast<uint8_t>(event.control >> 24) != d.slot) continue;
        const uint32_t dci = (event.control >> 16) & 31u;
        if (d.phase == Phase::HidRunning) {
            if (dci != (d.hid.endpoint & 15u) * 2u + 1u) continue;
            const uint32_t completion = event.status >> 24, remaining = event.status & 0xffffffu;
            if ((completion != 1 && completion != 13) || remaining > d.hid.maxPacket || !select(d)) {
                d.phase = Phase::Disconnect; return;
            }
            const auto* report = bus.host.InterruptBuffer(bus.host.Context);
            const size_t length = d.hid.maxPacket - remaining;
            const bool delivered = d.hid.protocol == 1 ? d.collection.keyboard(report, length)
                                                       : d.collection.mouse(report, length);
            if (!delivered || bus.host.SubmitInterrupt(bus.host.Context, &d.transferAddress) != success)
                d.phase = Phase::Disconnect;
            return;
        }
        if (dci != 1) continue;
        if ((event.status >> 24) != 1 || (event.status & 0xffffffu) || !select(d)) {
            d.phase = Phase::Disconnect; return;
        }
        const auto* bytes = bus.host.ControlBuffer(bus.host.Context);
        if (!bytes) { d.phase = Phase::Disconnect; return; }
        if (d.phase == Phase::DeviceDescriptor) {
            if (bytes[0] < 18 || bytes[1] != 1 || bytes[7] != (d.speed >= 4 ? 9 : d.speed == 3 ? 64 : 8) ||
                !beginControl(d, 0x80, 6, 0x0200, 0, 9, Phase::ConfigurationHeader)) d.phase = Phase::Disconnect;
        } else if (d.phase == Phase::ConfigurationHeader) {
            const uint16_t length = static_cast<uint16_t>(bytes[2] | (uint16_t{bytes[3]} << 8));
            if (bytes[0] < 9 || bytes[1] != 2 || length < 9 || length > 4096 ||
                !beginControl(d, 0x80, 6, 0x0200, 0, length, Phase::ConfigurationBody)) d.phase = Phase::Disconnect;
        } else if (d.phase == Phase::ConfigurationBody) {
            if (findBootHid(bytes, d.controlLength, d.hid) && d.hid.alternate == 0 && d.hid.reportDescriptorLength <= 4096) {
                d.storageDevice = false;
                if (!beginControl(d, 0x00, 9, d.hid.configuration, 0, 0, Phase::SetConfiguration)) d.phase = Phase::Disconnect;
            } else if (findBulkStorage(bytes, d.controlLength, d.storageInterface) && d.storageInterface.alternate == 0) {
                d.storageDevice = true;
                if (!beginControl(d, 0x00, 9, d.storageInterface.configuration, 0, 0, Phase::SetConfiguration)) d.phase = Phase::Disconnect;
            } else d.phase = Phase::Disconnect;
        } else if (d.phase == Phase::SetConfiguration) {
            if (d.storageDevice) {
                if ((d.speed >= 4 ?
                    (!d.storageInterface.inCompanion || !d.storageInterface.outCompanion || !bus.host.PrepareSuperSpeedBulk ?
                        int32_t{-1} : bus.host.PrepareSuperSpeedBulk(bus.host.Context, d.storageInterface.bulkIn,
                        d.storageInterface.inPacket, d.storageInterface.inBurst, d.storageInterface.bulkOut,
                        d.storageInterface.outPacket, d.storageInterface.outBurst, d.speed, &d.prepared)) :
                    bus.host.PrepareBulkEndpoints(bus.host.Context, d.storageInterface.bulkIn,
                        d.storageInterface.inPacket, d.storageInterface.bulkOut, d.storageInterface.outPacket,
                        d.speed, &d.prepared)) != success) d.phase = Phase::Disconnect;
                else d.phase = armDeadline(d) ? Phase::NeedEndpoint : Phase::Quarantined;
            } else if (!beginControl(d, 0x81, 6, 0x2200, d.hid.interfaceNumber,
                                     d.hid.reportDescriptorLength, Phase::ReportDescriptor)) d.phase = Phase::Disconnect;
        } else if (d.phase == Phase::ReportDescriptor) {
            if (!validateHidReportDescriptor(bytes, d.controlLength) ||
                (d.speed >= 4 ?
                    (!d.hid.superspeedCompanion || !bus.host.PrepareSuperSpeedInterrupt ? int32_t{-1} :
                        bus.host.PrepareSuperSpeedInterrupt(bus.host.Context, d.hid.endpoint, d.hid.maxPacket,
                            d.hid.interval, d.speed, d.hid.maxBurst, d.hid.bytesPerInterval, &d.prepared)) :
                    bus.host.PrepareInterruptEndpoint(bus.host.Context, d.hid.endpoint, d.hid.maxPacket,
                        d.hid.interval, d.speed, &d.prepared)) != success) d.phase = Phase::Disconnect;
            else d.phase = armDeadline(d) ? Phase::NeedEndpoint : Phase::Quarantined;
        } else if (d.phase == Phase::SetProtocol) {
            if (bus.host.SubmitInterrupt(bus.host.Context, &d.transferAddress) != success) d.phase = Phase::Disconnect;
            else d.phase = Phase::HidRunning;
        }
        return;
    }
}
void service(void*) noexcept {
    if (!bus.token) return;
    {
        HostGuard guard;
        if (!guard.held) return;
        routeEvents();
        uint64_t now{};
        if (!ke::KeQueryUptime100ns(now)) return;
        for (auto& d : bus.devices) {
            if (d.phase == Phase::Empty || d.phase == Phase::Quarantined) continue;
            if (d.port && d.phase != Phase::Disconnect && d.phase != Phase::NeedDisable &&
                d.phase != Phase::WaitDisable && d.phase != Phase::DrainDisabled) {
                uint32_t portStatus{};
                if (bus.host.ReadPort(bus.host.Context, d.port, &portStatus) != success || !(portStatus & 1u)) {
                    if (d.phase == Phase::WaitEnable) d.removing = true;
                    else d.phase = Phase::Disconnect;
                }
            }
            if (d.phase != Phase::HidRunning && d.phase != Phase::StorageReady &&
                d.phase != Phase::StorageInitialize && d.phase != Phase::Disconnect &&
                d.deadline && now >= d.deadline) {
                d.phase = d.phase == Phase::WaitDisable || d.phase == Phase::DrainDisabled || d.phase == Phase::WaitEnable
                    ? Phase::Quarantined : Phase::Disconnect;
            }
            process(d);
        }
        Device* free{};
        for (auto& d : bus.devices) if (d.phase == Phase::Empty) { free = &d; break; }
        if (free) {
            const uint8_t count = bus.host.PortCount(bus.host.Context);
            for (uint16_t port = 1; port <= count; ++port) {
                bool owned = false;
                for (const auto& d : bus.devices) if (d.phase != Phase::Empty && d.port == port) { owned = true; break; }
                if (owned) continue;
                uint32_t state{}; uint8_t speed{};
                if (bus.host.ReadPort(bus.host.Context, static_cast<uint8_t>(port), &state) != success || !(state & 1u) ||
                    bus.host.ResetPort(bus.host.Context, static_cast<uint8_t>(port), &speed) != success) continue;
                free->port = static_cast<uint8_t>(port); free->speed = speed;
                free->collection.setUnitId(static_cast<uint16_t>(free - bus.devices));
                free->phase = armDeadline(*free) ? Phase::NeedEnable : Phase::Quarantined;
                break;
            }
        }
    }
    // Storage callbacks acquire hostGate themselves; do not hold it across
    // synchronous SCSI/block operations or FsRtl mount reads.
    for (auto& d : bus.devices) if (d.phase == Phase::StorageInitialize) {
        BulkTransport bulk{&d, bulkTransfer, bulkReset, 4096, beginBulk, endBulk};
        const auto initialized = d.storage.initialize(bulk, d.storageInterface.interfaceNumber,
                                                       d.storageInterface.bulkIn, d.storageInterface.bulkOut);
        if (initialized == NtStatus::DeviceBusy) continue;
        if (initialized != NtStatus::Success) { d.phase = Phase::Disconnect; continue; }
        auto backend = d.storage.blockBackend();
        if (!backend.context || io::IoCreateBlockDevice(&d.blockHandle, 0x00120089, backend) != NtStatus::Success ||
            fsrtl::fat::mount(&d, d.blockHandle, d.mountToken) != NtStatus::Success || !d.mountToken) {
            d.storage.detach(); d.phase = Phase::Disconnect; continue;
        }
        d.phase = Phase::StorageReady;
    }
}
}

NtStatus registerHost(const void* owner, const Host& host, uint64_t& token) noexcept {
    token = 0;
    if (!owner || host.Size < offsetof(Host, PrepareSuperSpeedInterrupt) || host.Version != shared::usb::HostServicesVersion ||
        !host.Start || !host.Stop || !host.Restart || !host.PortCount || !host.ReadPort || !host.ResetPort || !host.PortSlotType ||
        !host.PrepareAddressDevice || !host.SelectDevice || !host.ForgetDevice || !host.SubmitCommand ||
        !host.SubmitControl || !host.ControlBuffer || !host.PollEvent || !host.PrepareInterruptEndpoint ||
        !host.SubmitInterrupt || !host.InterruptBuffer || !host.PrepareBulkEndpoints || !host.BulkTransfer ||
        !host.BulkResetRecovery || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL) return NtStatus::InvalidParameter;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (bus.token || !nextToken) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return NtStatus::DeviceBusy;
    }
    bus.owner = owner; bus.host = {};
    const size_t copied = host.Size >= sizeof(Host) ? sizeof(Host) :
        host.Size >= offsetof(Host, PrepareHubEndpoint) ? offsetof(Host, PrepareHubEndpoint) :
        offsetof(Host, PrepareSuperSpeedInterrupt);
    std::memcpy(&bus.host, &host, copied);
    bus.token = nextToken++; token = bus.token;
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    if (host.Start(host.Context) != success || !ke::passive_work::startPeriodic(bus.poll, service, nullptr, 200'000)) {
        if (host.Stop(host.Context) != success) return NtStatus::DeviceBusy;
        bus.owner = nullptr; bus.token = 0; bus.host = {}; token = 0;
        return NtStatus::DeviceNotReady;
    }
    return NtStatus::Success;
}
NtStatus unregisterHost(const void* owner, uint64_t token) noexcept {
    if (!owner || !token || bus.owner != owner || bus.token != token) return NtStatus::InvalidHandle;
    if (!ke::passive_work::stopPeriodic(bus.poll)) return NtStatus::DeviceBusy;
    for (auto& d : bus.devices) {
        if (d.storageDevice) {
            d.storage.detach();
            if (d.mountToken) {
                if (fsrtl::fat::unmount(&d, d.mountToken) != NtStatus::Success) return NtStatus::DeviceBusy;
                d.mountToken = 0;
            }
            if (d.blockHandle) {
                if (sys::NtClose(d.blockHandle) != NtStatus::Success) return NtStatus::DeviceBusy;
                d.blockHandle = 0;
            }
            if (!d.storage.canDestroy()) return NtStatus::DeviceBusy;
        }
        if (!d.collection.disconnect()) return NtStatus::DeviceBusy;
    }
    HostGuard guard;
    if (!guard.held || bus.host.Stop(bus.host.Context) != success) return NtStatus::DeviceBusy;
    bus.owner = nullptr; bus.token = 0; bus.host = {};
    for (auto& d : bus.devices) resetDevice(d);
    return NtStatus::Success;
}
}
