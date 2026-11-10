#include <array>
#include <cassert>

#include "micant/km/drivers/system/null.hpp"

int main() {
    using namespace micant;

    io::DriverObject driver{};
    null_driver::Initialize(driver);
    assert(driver.driverName == L"\\Driver\\Null");

    io::DeviceObject device{};
    std::array<uint8_t, 32> buffer{};
    buffer.fill(0xA5);

    io::Irp read{};
    read.majorFunction = io::IRP_MJ_READ;
    read.userBuffer = buffer.data();
    read.length = buffer.size();
    assert(null_driver::Dispatch(&device, &read) == NtStatus::Success);
    assert(read.ioStatus.information == buffer.size());
    for (uint8_t value : buffer) assert(value == 0);

    io::Irp write{};
    write.majorFunction = io::IRP_MJ_WRITE;
    write.userBuffer = buffer.data();
    write.length = buffer.size();
    assert(null_driver::Dispatch(&device, &write) == NtStatus::Success);
    assert(write.ioStatus.information == buffer.size());

    io::Irp unsupported{};
    unsupported.majorFunction = io::IRP_MJ_DEVICE_CONTROL;
    assert(null_driver::Dispatch(&device, &unsupported) == NtStatus::InvalidDeviceRequest);
    assert(unsupported.ioStatus.information == 0);

    return 0;
}
