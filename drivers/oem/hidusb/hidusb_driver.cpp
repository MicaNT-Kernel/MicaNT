// USB boot-protocol HID class driver. Hardware access and USB transfer
// ownership remain with USBD/xHCI; this image owns only HID interface state.
#include "micant/shared/boot_driver.hpp"
#include "micant/shared/usb_class_abi.hpp"
#include "../usb_class_descriptors.hpp"
#include "micant/km/drivers/input/hid_scancode.hpp"
#include "micant/km/drivers/input/mouclass_contract.hpp"
#include <atomic>
#include <cstring>

namespace {
using namespace micant;
using namespace micant::boot_driver;
using namespace micant::shared::usb;
using namespace micant::km::hid;
constexpr int32_t kBusy = static_cast<int32_t>(0x80000011u);
struct Collection {
    std::atomic<bool> used{false};
    std::atomic<bool> activityReported{false};
    std::atomic<bool> releaseReported{false};
    std::atomic<bool> active{false};
    TransferServicesV1 transfer{};
    uint8_t protocol{}, keys[8]{}, buttons{};
    uint16_t unit{};
};
Collection collections[4]{};
const KernelServices* services{};
uint64_t registration{};

bool contains(const uint8_t* report, uint8_t key) noexcept {
    for (unsigned i = 2; i < 8; ++i) if (report[i] == key) return true;
    return false;
}
bool keyboard(Collection& d, const uint8_t* report, size_t size) noexcept {
    if (!report || size != 8 || report[1]) return false;
    for (unsigned i = 2; i < 8; ++i) {
        if (report[i] && report[i] < 4) return false;
        if (report[i]) for (unsigned j = 2; j < i; ++j) if (report[i] == report[j]) return false;
    }
    KEYBOARD_INPUT_DATA packets[28]{}; uint32_t count{};
    auto append = [&](uint8_t usage, bool released) noexcept {
        uint16_t code{}, flags{};
        if (!hidUsageToScanCode(usage, code, flags)) return;
        packets[count].UnitId = d.unit; packets[count].MakeCode = code;
        packets[count++].Flags = flags | (released ? KEY_BREAK : KEY_MAKE);
    };
    for (unsigned bit = 0; bit < 8; ++bit)
        if ((d.keys[0] & (1u << bit)) && !(report[0] & (1u << bit))) append(0xe0 + bit, true);
    for (unsigned i = 2; i < 8; ++i)
        if (d.keys[i] && !contains(report, d.keys[i])) append(d.keys[i], true);
    for (unsigned bit = 0; bit < 8; ++bit)
        if (!(d.keys[0] & (1u << bit)) && (report[0] & (1u << bit))) append(0xe0 + bit, false);
    for (unsigned i = 2; i < 8; ++i)
        if (report[i] && !contains(d.keys, report[i])) append(report[i], false);
    if (count && d.transfer.SubmitKeyboard(d.transfer.Context, packets, count) != count) return false;
    std::memcpy(d.keys, report, sizeof(d.keys));
    return true;
}
bool mouse(Collection& d, const uint8_t* report, size_t size) noexcept {
    if (!report || size != 3) return false;
    const uint8_t buttons = report[0] & 7u;
    MOUSE_INPUT_DATA packet{}; packet.UnitId = d.unit;
    static constexpr uint16_t down[]{MOUSE_LEFT_BUTTON_DOWN, MOUSE_RIGHT_BUTTON_DOWN, MOUSE_MIDDLE_BUTTON_DOWN};
    static constexpr uint16_t up[]{MOUSE_LEFT_BUTTON_UP, MOUSE_RIGHT_BUTTON_UP, MOUSE_MIDDLE_BUTTON_UP};
    for (unsigned bit = 0; bit < 3; ++bit) {
        if ((buttons & (1u << bit)) && !(d.buttons & (1u << bit))) packet.ButtonFlags |= down[bit];
        if (!(buttons & (1u << bit)) && (d.buttons & (1u << bit))) packet.ButtonFlags |= up[bit];
    }
    packet.RawButtons = buttons; packet.LastX = static_cast<int8_t>(report[1]);
    packet.LastY = static_cast<int8_t>(report[2]);
    if ((packet.LastX || packet.LastY || packet.ButtonFlags) &&
        d.transfer.SubmitMouse(d.transfer.Context, &packet, 1) != 1) return false;
    d.buttons = buttons;
    return true;
}
int32_t match(void*, const uint8_t* bytes, size_t size, InterfaceV1* output) {
    if (!output) return micant::boot_driver::STATUS_INVALID_PARAMETER;
    micant::usb::baremetal::HidInterface hid{};
    if (!micant::usb::baremetal::findBootHid(bytes, size, hid) || hid.alternate ||
        !hid.reportDescriptorLength || hid.reportDescriptorLength > 4096) {
        if (services && services->Log)
            services->Log(services->Context, "hidusb.sys: configuration has no supported boot HID interface");
        return micant::boot_driver::STATUS_NOT_SUPPORTED;
    }
    *output = {};
    output->Size = sizeof(*output); output->Kind = TransferKind::InterruptIn;
    output->Configuration = hid.configuration; output->InterfaceNumber = hid.interfaceNumber;
    output->Alternate = hid.alternate; output->Protocol = hid.protocol;
    output->InEndpoint = hid.endpoint; output->InPacket = hid.maxPacket; output->Interval = hid.interval;
    output->ReportDescriptorLength = hid.reportDescriptorLength;
    output->DescriptorRequestType = 0x81; output->DescriptorRequest = 6;
    output->DescriptorValue = 0x2200; output->DescriptorIndex = hid.interfaceNumber;
    output->ControlRequestType = 0x21; output->ControlRequest = 0x0b;
    output->ControlValue = 0; output->ControlIndex = hid.interfaceNumber; output->NeedsControl = true;
    output->InBurst = hid.maxBurst; output->BytesPerInterval = hid.bytesPerInterval;
    output->InCompanion = hid.superspeedCompanion;
    return micant::boot_driver::STATUS_SUCCESS;
}
int32_t create(void*, const InterfaceV1* info, const TransferServicesV1* transfer,
               uint16_t unit, void** result) {
    if (!info || !transfer || !result || info->Size < sizeof(*info) ||
        transfer->Size < sizeof(*transfer) || transfer->Version != TransferServicesVersion ||
        !transfer->SubmitKeyboard || !transfer->SubmitMouse ||
        info->Kind != TransferKind::InterruptIn || (info->Protocol != 1 && info->Protocol != 2))
        return micant::boot_driver::STATUS_INVALID_PARAMETER;
    *result = nullptr;
    for (auto& d : collections) {
        bool expected = false;
        if (!d.used.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) continue;
        d.transfer = *transfer; d.protocol = info->Protocol; d.unit = unit;
        d.activityReported.store(false, std::memory_order_relaxed);
        d.releaseReported.store(false, std::memory_order_relaxed);
        d.active.store(false, std::memory_order_relaxed);
        std::memset(d.keys, 0, sizeof(d.keys)); d.buttons = 0; *result = &d;
        return micant::boot_driver::STATUS_SUCCESS;
    }
    return kBusy;
}
int32_t validateReport(void*, const uint8_t* data, size_t size) {
    return micant::usb::baremetal::validateHidReportDescriptor(data, size) ? micant::boot_driver::STATUS_SUCCESS : micant::boot_driver::STATUS_INVALID_PARAMETER;
}
int32_t start(void* context) { return context ? micant::boot_driver::STATUS_SUCCESS : micant::boot_driver::STATUS_INVALID_PARAMETER; }
int32_t report(void* context, const uint8_t* data, size_t size) {
    auto* d = static_cast<Collection*>(context);
    if (!d || !d->used.load(std::memory_order_acquire)) return micant::boot_driver::STATUS_INVALID_PARAMETER;
    if (!(d->protocol == 1 ? keyboard(*d, data, size) : mouse(*d, data, size))) return kBusy;
    if (size >= (d->protocol == 1 ? 8u : 3u)) {
        const bool active = d->protocol == 1
            ? (data[0] != 0 || data[2] != 0 || data[3] != 0 || data[4] != 0 ||
               data[5] != 0 || data[6] != 0 || data[7] != 0)
            : (data[0] != 0 || data[1] != 0 || data[2] != 0);
        if (active) {
            d->active.store(true, std::memory_order_release);
            bool expected = false;
            if (d->activityReported.compare_exchange_strong(expected, true, std::memory_order_acq_rel) &&
                services && services->Log)
                services->Log(services->Context,
                    "hidusb.sys: completed USB HID activity report delivered to input class");
        } else if (d->active.exchange(false, std::memory_order_acq_rel)) {
            bool expected = false;
            if (d->releaseReported.compare_exchange_strong(expected, true, std::memory_order_acq_rel) &&
                services && services->Log)
                services->Log(services->Context,
                    "hidusb.sys: completed USB HID release report delivered to input class");
        }
    }
    return micant::boot_driver::STATUS_SUCCESS;
}
int32_t remove(void* context) {
    auto* d = static_cast<Collection*>(context);
    if (!d || !d->used.load(std::memory_order_acquire)) return micant::boot_driver::STATUS_INVALID_PARAMETER;
    const uint8_t keys[8]{}, buttons[3]{};
    if (!(d->protocol == 1 ? keyboard(*d, keys, sizeof(keys)) : mouse(*d, buttons, sizeof(buttons)))) return kBusy;
    d->transfer = {}; d->protocol = 0; d->used.store(false, std::memory_order_release);
    return micant::boot_driver::STATUS_SUCCESS;
}
void unload(DriverObject* driver) noexcept {
    if (!services) return;
    if (registration && services->UnregisterUsbClass(services->Context, registration) != micant::boot_driver::STATUS_SUCCESS) return;
    registration = 0; services = nullptr;
    if (driver) driver->DriverUnload = nullptr;
}
}
extern "C" int32_t DriverEntry(micant::boot_driver::DriverObject* driver,
                                const micant::boot_driver::UnicodeString*) {
    if (!driver || driver->Size < sizeof(*driver) || driver->Version != DRIVER_ABI_VERSION || !driver->HardwareResources)
        return micant::boot_driver::STATUS_INVALID_PARAMETER;
    const auto& s = *static_cast<const KernelServices*>(driver->HardwareResources);
    if (s.Size < sizeof(KernelServices) || s.Version != KERNEL_SERVICES_VERSION ||
        s.UsbClassVersion != ClassServicesVersion || !s.RegisterUsbClass || !s.UnregisterUsbClass)
        return micant::boot_driver::STATUS_NOT_SUPPORTED;
    if (services || registration) return kBusy;
    static const ClassServicesV1 callbacks{sizeof(ClassServicesV1), ClassServicesVersion, &collections[0],
        match, create, validateReport, start, report, remove};
    services = &s; driver->DriverUnload = unload;
    const auto result = s.RegisterUsbClass(s.Context, &callbacks, &registration);
    if (result != micant::boot_driver::STATUS_SUCCESS || !registration) { unload(driver); return result == micant::boot_driver::STATUS_SUCCESS ? micant::boot_driver::STATUS_DEVICE_NOT_READY : result; }
    return micant::boot_driver::STATUS_SUCCESS;
}
