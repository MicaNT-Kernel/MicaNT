#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "micant/km/drivers/storage/storage.hpp"

int main() {
    using namespace micant;
    using namespace micant::storage;

    static_assert(sizeof(PARTITION_INFORMATION_GPT::Name) == 72);
    static_assert(sizeof(PARTITION_INFORMATION_EX) == 144);

    auto disk = std::make_shared<RamDiskDevice>(L"\\Device\\DiskTest", 4096);
    io::DeviceObject device{};
    device.deviceExtension = disk.get();

    std::array<uint8_t, 512> writeBuffer{};
    writeBuffer[0] = 0x5A;
    io::Irp write{};
    write.majorFunction = io::IRP_MJ_WRITE;
    write.userBuffer = writeBuffer.data();
    write.length = writeBuffer.size();
    assert(DiskDriverDispatch(&device, &write) == NtStatus::Success);
    assert(write.ioStatus.information == writeBuffer.size());

    std::array<uint8_t, 512> readBuffer{};
    io::Irp read{};
    read.majorFunction = io::IRP_MJ_READ;
    read.userBuffer = readBuffer.data();
    read.length = readBuffer.size();
    assert(DiskDriverDispatch(&device, &read) == NtStatus::Success);
    assert(readBuffer[0] == 0x5A);
    assert(read.ioStatus.information == readBuffer.size());

    read.byteOffset.quadPart = 1;
    read.ioStatus.information = 99;
    assert(DiskDriverDispatch(&device, &read) == NtStatus::InvalidParameter);
    assert(read.ioStatus.status == NtStatus::InvalidParameter);
    assert(read.ioStatus.information == 0);

    read.byteOffset.quadPart = 0;
    read.length = 513;
    assert(DiskDriverDispatch(&device, &read) == NtStatus::InvalidParameter);

    read.length = 512;
    read.byteOffset.quadPart = 4096;
    assert(DiskDriverDispatch(&device, &read) == NtStatus::EndOfFile);
    assert(read.ioStatus.information == 0);

    std::array<uint8_t, sizeof(DISK_GEOMETRY) + sizeof(LargeInteger)> geometryBuffer{};
    io::Irp geometry{};
    geometry.majorFunction = io::IRP_MJ_DEVICE_CONTROL;
    geometry.byteOffset.lowPart = IOCTL_DISK_GET_DRIVE_GEOMETRY_EX;
    geometry.userBuffer = geometryBuffer.data();
    geometry.length = geometryBuffer.size();
    assert(DiskDriverDispatch(&device, &geometry) == NtStatus::Success);
    assert(geometry.ioStatus.information == geometryBuffer.size());

    geometry.length--;
    assert(DiskDriverDispatch(&device, &geometry) == NtStatus::BufferTooSmall);
    assert(geometry.ioStatus.information == 0);

    return 0;
}
