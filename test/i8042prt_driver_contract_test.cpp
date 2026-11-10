#include <array>
#include <cassert>

#include "micant/km/drivers/input/i8042prt.hpp"

int main() {
    using namespace micant;
    using namespace micant::km::hid;
    io::DriverObject portDriver{}, keyboardDriver{}, mouseDriver{};
    InitializeI8042PortDriver(portDriver);
    InitializeKbdClassDriver(keyboardDriver);
    InitializeMouClassDriver(mouseDriver);
    io::DeviceObject keyboardPort{}, pointerPort{}, keyboardClass{}, mouseClass{};
    keyboardPort.driverObject = &portDriver;
    keyboardPort.deviceExtension = &I8042KeyboardPortTag;
    pointerPort.driverObject = &portDriver;
    pointerPort.deviceExtension = &I8042PointerPortTag;
    keyboardClass.driverObject = &keyboardDriver;
    mouseClass.driverObject = &mouseDriver;

    assert(I8042ConnectClassDevice(keyboardPort, keyboardClass, IOCTL_INTERNAL_KEYBOARD_CONNECT,
        reinterpret_cast<void*>(KeyboardClassServiceCallback)) == NtStatus::Success);
    assert(I8042ConnectClassDevice(pointerPort, mouseClass, IOCTL_INTERNAL_MOUSE_CONNECT,
        reinterpret_cast<void*>(MouseClassServiceCallback)) == NtStatus::Success);
    assert(I8042ConnectClassDevice(keyboardPort, keyboardClass, IOCTL_INTERNAL_KEYBOARD_CONNECT,
        reinterpret_cast<void*>(KeyboardClassServiceCallback)) == NtStatus::SharingViolation);

    I8042WritePortUchar(I8042_COMMAND_PORT, I8042_CMD_SELF_TEST);
    assert((I8042ReadPortUchar(I8042_STATUS_PORT) & I8042_STS_OBF) != 0);
    assert(I8042ReadPortUchar(I8042_DATA_PORT) == 0x55);

    KbdClassResetState();
    MouClassResetState();
    I8042InjectKeyboardIrq1(0x1E);
    I8042InjectMouseIrq12Packet(1, 5, -3);
    assert(KbdClassQueueDepth() == 1);
    assert(MouClassQueueDepth() == 1);
    KEYBOARD_INPUT_DATA keyboardPacket{};
    MOUSE_INPUT_DATA mousePacket{};
    assert(KbdClassPopInputData(keyboardPacket) && keyboardPacket.MakeCode == 0x1E);
    assert(MouClassPopInputData(mousePacket) && mousePacket.LastX == 5 && mousePacket.LastY == 3);
    assert(I8042InterruptsServiced() == 2);
    return 0;
}
