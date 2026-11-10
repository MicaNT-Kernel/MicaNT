#include <array>
#include <cassert>

#include "micant/km/drivers/input/kbdclass.hpp"

int main() {
    using namespace micant;
    using namespace micant::km::hid;

    KbdClassResetState();
    InitializeKbdClassExports();
    io::DriverObject driver{};
    InitializeKbdClassDriver(driver);
    io::DeviceObject device{};
    device.driverObject = &driver;

    KEYBOARD_ATTRIBUTES attributes{};
    assert(KbdClassQueryAttributes(&attributes, sizeof(attributes)) == NtStatus::Success);
    assert(attributes.NumberOfKeysTotal == 104);
    assert(attributes.InputDataQueueLength == 128 * sizeof(KEYBOARD_INPUT_DATA));

    KEYBOARD_INDICATOR_PARAMETERS leds{0, KEYBOARD_CAPS_LOCK_ON};
    assert(KbdClassDeviceControl(IOCTL_KEYBOARD_SET_INDICATORS, &leds, sizeof(leds), 0) == NtStatus::Success);
    leds = {};
    assert(KbdClassDeviceControl(IOCTL_KEYBOARD_QUERY_INDICATORS, &leds, 0, sizeof(leds)) == NtStatus::Success);
    assert(leds.LedFlags == KEYBOARD_CAPS_LOCK_ON);

    std::array<KEYBOARD_INPUT_DATA, 1> packets{};
    io::Irp pending{};
    pending.majorFunction = io::IRP_MJ_READ;
    pending.deviceObject = &device;
    pending.userBuffer = packets.data();
    pending.length = sizeof(packets);
    assert(KbdClassDriver::Instance().dispatch(&device, &pending) == NtStatus::Pending);

    KEYBOARD_INPUT_DATA source{0, 0x1E, KEY_MAKE, 0, 0x1234};
    uint32_t consumed = 0;
    KeyboardClassServiceCallback(&device, &source, &source + 1, &consumed);
    assert(consumed == 1);
    assert(pending.ioStatus.status == NtStatus::Success);
    assert(pending.ioStatus.information == sizeof(KEYBOARD_INPUT_DATA));
    assert(packets[0].MakeCode == source.MakeCode && packets[0].ExtraInformation == source.ExtraInformation);
    return 0;
}
