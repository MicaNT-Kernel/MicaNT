#include <array>
#include <cassert>
#include <cstring>

#include "micant/km/drivers/input/mouclass.hpp"

int main() {
    using namespace micant;
    using namespace micant::km::hid;

    MouClassResetState();
    InitializeMouClassExports();

    io::DriverObject driver{};
    InitializeMouClassDriver(driver);
    assert(driver.driverName == L"\\Driver\\Mouclass");
    io::DeviceObject device{};
    device.driverObject = &driver;

    std::array<MOUSE_INPUT_DATA, 1> buffer{};
    io::Irp pending{};
    pending.majorFunction = io::IRP_MJ_READ;
    pending.deviceObject = &device;
    pending.userBuffer = buffer.data();
    pending.length = sizeof(buffer);
    assert(MouClassDriver::Instance().dispatch(&device, &pending) == NtStatus::Pending);

    MOUSE_INPUT_DATA source{};
    source.Flags = MOUSE_MOVE_RELATIVE;
    source.LastX = 37;
    source.LastY = -12;
    source.ButtonFlags = MOUSE_LEFT_BUTTON_DOWN;
    ULONG consumed = 0;
    MouseClassServiceCallback(&device, &source, &source + 1, &consumed);

    assert(consumed == 1);
    assert(pending.ioStatus.status == NtStatus::Success);
    assert(pending.ioStatus.information == sizeof(MOUSE_INPUT_DATA));
    assert(buffer[0].LastX == 37 && buffer[0].LastY == -12);
    assert(buffer[0].ButtonFlags == MOUSE_LEFT_BUTTON_DOWN);

    MOUSE_ATTRIBUTES attributes{};
    io::Irp query{};
    query.majorFunction = io::IRP_MJ_DEVICE_CONTROL;
    query.byteOffset.lowPart = IOCTL_MOUSE_QUERY_ATTRIBUTES;
    query.userBuffer = &attributes;
    query.length = sizeof(attributes);
    assert(MouClassDriver::Instance().dispatch(&device, &query) == NtStatus::Success);
    assert(query.ioStatus.information == sizeof(attributes));
    assert(attributes.InputDataQueueLength == 256 * sizeof(MOUSE_INPUT_DATA));

    std::array<uint8_t, sizeof(MOUSE_ATTRIBUTES)> unitBuffer{};
    MOUSE_UNIT_ID_PARAMETER unit{1};
    std::memcpy(unitBuffer.data(), &unit, sizeof(unit));
    query.userBuffer = unitBuffer.data();
    query.byteOffset.highPart = sizeof(unit);
    assert(MouClassDriver::Instance().dispatch(&device, &query) == NtStatus::NoSuchDevice);
    assert(query.ioStatus.information == 0);

    return 0;
}
