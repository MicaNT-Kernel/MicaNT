#include <cassert>

#include "micant/km/drivers/system/beep.hpp"

int main() {
    using namespace micant;
    using namespace micant::km::system;

    io::DriverObject driver{};
    InitializeBeepDriver(driver);
    assert(driver.driverName == L"\\Driver\\Beep");
    io::DeviceObject device{};
    device.driverObject = &driver;

    io::Irp create{};
    create.majorFunction = io::IRP_MJ_CREATE;
    assert(BeepDriver::Instance().dispatch(&device, &create) == NtStatus::Success);

    BEEP_SET_PARAMETERS parameters{BEEP_FREQUENCY_MINIMUM - 1, 50};
    io::Irp invalid{};
    invalid.majorFunction = io::IRP_MJ_DEVICE_CONTROL;
    invalid.deviceObject = &device;
    invalid.userBuffer = &parameters;
    auto* stack = invalid.getCurrentStack();
    stack->parameters.deviceIoControl.ioControlCode = IOCTL_BEEP_SET;
    stack->parameters.deviceIoControl.inputBufferLength = sizeof(parameters);
    assert(BeepDriver::Instance().dispatch(&device, &invalid) == NtStatus::InvalidParameter);

    parameters.Frequency = BEEP_FREQUENCY_MAXIMUM + 1;
    assert(BeepDriver::Instance().dispatch(&device, &invalid) == NtStatus::InvalidParameter);

    stack->parameters.deviceIoControl.ioControlCode = 0xFFFFFFFFu;
    assert(BeepDriver::Instance().dispatch(&device, &invalid) == NtStatus::NotImplemented);
    assert(invalid.ioStatus.information == 0);

    stack->parameters.deviceIoControl.ioControlCode = IOCTL_BEEP_SET;
    stack->parameters.deviceIoControl.inputBufferLength = sizeof(parameters) - 1;
    assert(BeepDriver::Instance().dispatch(&device, &invalid) == NtStatus::InvalidParameter);
    return 0;
}
