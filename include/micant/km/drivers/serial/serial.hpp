#pragma once

// Clean-room WDM serial function driver. Public data structures and IOCTL
// values here follow ntddser.h; the implementation is limited to the 8250
// register-compatible UART surface supported by the current HAL/arbiter.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "micant/km/executive/io.hpp"
#include "micant/km/executive/pnp.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::km::serial {

inline constexpr uint32_t FILE_DEVICE_SERIAL_PORT = 0x1B;
inline constexpr uint32_t IOCTL_SERIAL_SET_BAUD_RATE = 0x001B0004;
inline constexpr uint32_t IOCTL_SERIAL_SET_LINE_CONTROL = 0x001B000C;
inline constexpr uint32_t IOCTL_SERIAL_GET_BAUD_RATE = 0x001B0050;
inline constexpr uint32_t IOCTL_SERIAL_GET_LINE_CONTROL = 0x001B0054;
inline constexpr uint32_t IOCTL_SERIAL_SET_TIMEOUTS = 0x001B001C;
inline constexpr uint32_t IOCTL_SERIAL_GET_TIMEOUTS = 0x001B0020;

struct SERIAL_BAUD_RATE { uint32_t BaudRate; };
struct SERIAL_LINE_CONTROL { uint8_t StopBits; uint8_t Parity; uint8_t WordLength; };
struct SERIAL_TIMEOUTS {
    uint32_t ReadIntervalTimeout;
    uint32_t ReadTotalTimeoutMultiplier;
    uint32_t ReadTotalTimeoutConstant;
    uint32_t WriteTotalTimeoutMultiplier;
    uint32_t WriteTotalTimeoutConstant;
};
static_assert(sizeof(SERIAL_BAUD_RATE) == 4);
static_assert(sizeof(SERIAL_LINE_CONTROL) == 3);
static_assert(sizeof(SERIAL_TIMEOUTS) == 20);

inline constexpr uint8_t STOP_BIT_1 = 0;
inline constexpr uint8_t STOP_BITS_1_5 = 1;
inline constexpr uint8_t STOP_BITS_2 = 2;
inline constexpr uint8_t NO_PARITY = 0;
inline constexpr uint8_t ODD_PARITY = 1;
inline constexpr uint8_t EVEN_PARITY = 2;
inline constexpr uint8_t MARK_PARITY = 3;
inline constexpr uint8_t SPACE_PARITY = 4;

struct DeviceExtension {
    io::DeviceObject* pdo{};
    uint16_t portBase{};
    uint32_t portLength{};
    uint32_t baudRate{9600};
    SERIAL_LINE_CONTROL lineControl{STOP_BIT_1, NO_PARITY, 8};
    SERIAL_TIMEOUTS timeouts{};
    bool started{};
};

namespace detail {
inline uint8_t readPort(uint16_t port) noexcept {
#if defined(__i386__) || defined(__x86_64__)
    uint8_t value;
#if defined(_MSC_VER)
    __asm { mov dx, port; in al, dx; mov value, al }
#else
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
#endif
    return value;
#else
    (void)port;
    return 0;
#endif
}
inline void writePort(uint16_t port, uint8_t value) noexcept {
#if defined(__i386__) || defined(__x86_64__)
#if defined(_MSC_VER)
    __asm { mov dx, port; mov al, value; out dx, al }
#else
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
#endif
#else
    (void)port; (void)value;
#endif
}
inline void complete(io::Irp* irp, NtStatus status, uint32_t bytes = 0) noexcept {
    if (!irp) return;
    irp->ioStatus.status = status;
    irp->ioStatus.information = bytes;
}
inline void programLine(DeviceExtension& ext) noexcept {
    const uint32_t divisor = 115200U / ext.baudRate;
    const uint16_t base = ext.portBase;
    writePort(base + 3, static_cast<uint8_t>(0x80 | ((ext.lineControl.WordLength - 5) & 3) |
        ((ext.lineControl.StopBits == STOP_BIT_1) ? 0 : 4) |
        ((ext.lineControl.Parity == NO_PARITY) ? 0 : 8) |
        ((ext.lineControl.Parity == EVEN_PARITY || ext.lineControl.Parity == SPACE_PARITY) ? 0x10 : 0) |
        ((ext.lineControl.Parity == MARK_PARITY || ext.lineControl.Parity == SPACE_PARITY) ? 0x20 : 0)));
    writePort(base, static_cast<uint8_t>(divisor & 0xFF));
    writePort(base + 1, static_cast<uint8_t>((divisor >> 8) & 0xFF));
    writePort(base + 3, static_cast<uint8_t>(((ext.lineControl.WordLength - 5) & 3) |
        ((ext.lineControl.StopBits == STOP_BIT_1) ? 0 : 4) |
        ((ext.lineControl.Parity == NO_PARITY) ? 0 : 8) |
        ((ext.lineControl.Parity == EVEN_PARITY || ext.lineControl.Parity == SPACE_PARITY) ? 0x10 : 0) |
        ((ext.lineControl.Parity == MARK_PARITY || ext.lineControl.Parity == SPACE_PARITY) ? 0x20 : 0)));
    writePort(base + 2, 0x07); // Enable and clear the UART FIFOs.
    writePort(base + 1, 0x00); // Interrupts are polled until the kernel IRQ path exists.
}
inline bool validLine(const SERIAL_LINE_CONTROL& line) noexcept {
    return line.WordLength >= 5 && line.WordLength <= 8 && line.StopBits <= STOP_BITS_2 &&
        line.Parity <= SPACE_PARITY && !(line.WordLength == 5 && line.StopBits == STOP_BITS_2);
}
} // namespace detail

inline NtStatus DispatchCreateClose(io::DeviceObject*, io::Irp* irp) noexcept {
    detail::complete(irp, NtStatus::Success);
    return NtStatus::Success;
}

inline NtStatus DispatchDeviceControl(io::DeviceObject* device, io::Irp* irp) noexcept {
    if (!device || !device->deviceExtension || !irp) return NtStatus::InvalidParameter;
    auto& ext = *static_cast<DeviceExtension*>(device->deviceExtension);
    auto* stack = irp->getCurrentStack();
    if (!stack) return NtStatus::InvalidParameter;
    const uint32_t code = stack->parameters.deviceIoControl.ioControlCode;
    void* buffer = irp->systemBuffer ? irp->systemBuffer : irp->userBuffer;
    const uint32_t inLength = stack->parameters.deviceIoControl.inputBufferLength;
    const uint32_t outLength = stack->parameters.deviceIoControl.outputBufferLength;
    switch (code) {
    case IOCTL_SERIAL_GET_BAUD_RATE:
        if (!buffer || outLength < sizeof(SERIAL_BAUD_RATE)) break;
        static_cast<SERIAL_BAUD_RATE*>(buffer)->BaudRate = ext.baudRate;
        detail::complete(irp, NtStatus::Success, sizeof(SERIAL_BAUD_RATE)); return NtStatus::Success;
    case IOCTL_SERIAL_SET_BAUD_RATE: {
        if (!buffer || inLength < sizeof(SERIAL_BAUD_RATE)) break;
        const auto baud = static_cast<SERIAL_BAUD_RATE*>(buffer)->BaudRate;
        if (!baud || baud > 115200 || (115200U / baud) == 0) break;
        ext.baudRate = baud;
        if (ext.started) detail::programLine(ext);
        detail::complete(irp, NtStatus::Success); return NtStatus::Success;
    }
    case IOCTL_SERIAL_GET_LINE_CONTROL:
        if (!buffer || outLength < sizeof(SERIAL_LINE_CONTROL)) break;
        std::memcpy(buffer, &ext.lineControl, sizeof(ext.lineControl));
        detail::complete(irp, NtStatus::Success, sizeof(ext.lineControl)); return NtStatus::Success;
    case IOCTL_SERIAL_SET_LINE_CONTROL:
        if (!buffer || inLength < sizeof(SERIAL_LINE_CONTROL)) break;
        if (!detail::validLine(*static_cast<SERIAL_LINE_CONTROL*>(buffer))) break;
        std::memcpy(&ext.lineControl, buffer, sizeof(ext.lineControl));
        if (ext.started) detail::programLine(ext);
        detail::complete(irp, NtStatus::Success); return NtStatus::Success;
    case IOCTL_SERIAL_GET_TIMEOUTS:
        if (!buffer || outLength < sizeof(SERIAL_TIMEOUTS)) break;
        std::memcpy(buffer, &ext.timeouts, sizeof(ext.timeouts));
        detail::complete(irp, NtStatus::Success, sizeof(ext.timeouts)); return NtStatus::Success;
    case IOCTL_SERIAL_SET_TIMEOUTS:
        if (!buffer || inLength < sizeof(SERIAL_TIMEOUTS)) break;
        std::memcpy(&ext.timeouts, buffer, sizeof(ext.timeouts));
        detail::complete(irp, NtStatus::Success); return NtStatus::Success;
    default:
        detail::complete(irp, NtStatus::InvalidDeviceRequest); return NtStatus::InvalidDeviceRequest;
    }
    detail::complete(irp, NtStatus::InvalidParameter);
    return NtStatus::InvalidParameter;
}

inline NtStatus DispatchReadWrite(io::DeviceObject* device, io::Irp* irp) noexcept {
    if (!device || !device->deviceExtension || !irp) return NtStatus::InvalidParameter;
    auto& ext = *static_cast<DeviceExtension*>(device->deviceExtension);
    if (!ext.started) { detail::complete(irp, NtStatus::DeviceNotReady); return NtStatus::DeviceNotReady; }
    auto* buffer = static_cast<uint8_t*>(irp->systemBuffer ? irp->systemBuffer : irp->userBuffer);
    if (!buffer && irp->length) { detail::complete(irp, NtStatus::InvalidParameter); return NtStatus::InvalidParameter; }
    uint32_t done = 0;
    for (; done < irp->length; ++done) {
        uint32_t spins = 1000000;
        const bool write = irp->majorFunction == io::IRP_MJ_WRITE;
        while (spins-- && ((detail::readPort(ext.portBase + 5) & (write ? 0x20 : 0x01)) == 0)) {}
        if (!spins) break;
        if (write) detail::writePort(ext.portBase, buffer[done]);
        else buffer[done] = detail::readPort(ext.portBase);
    }
    const NtStatus status = done == irp->length ? NtStatus::Success : NtStatus::Timeout;
    detail::complete(irp, status, done);
    return status;
}

inline NtStatus DispatchPnp(io::DeviceObject* device, io::Irp* irp) noexcept {
    if (!device || !device->deviceExtension || !irp) return NtStatus::InvalidParameter;
    auto& ext = *static_cast<DeviceExtension*>(device->deviceExtension);
    switch (irp->minorFunction) {
    case pnp::IRP_MN_START_DEVICE: {
        auto* stack = irp->getCurrentStack();
        auto* list = stack ? stack->parameters.startDevice.AllocatedResourcesTranslated : nullptr;
        if (!list || list->Count == 0) break;
        const auto& partial = list->List[0].PartialResourceList;
        for (uint32_t i = 0; i < partial.Count; ++i) {
            const auto& descriptor = partial.PartialDescriptors[i];
            if (descriptor.Type == 1 && (descriptor.Flags & 1) && descriptor.u.Port.Length >= 8 &&
                descriptor.u.Port.Start >= 0 && descriptor.u.Port.Start <= 0xFFFF) {
                ext.portBase = static_cast<uint16_t>(descriptor.u.Port.Start);
                ext.portLength = descriptor.u.Port.Length;
                break;
            }
        }
        if (!ext.portBase) break;
        ext.started = true;
        detail::programLine(ext);
        device->flags = (device->flags | io::DO_BUFFERED_IO) & ~io::DO_DEVICE_INITIALIZING;
        detail::complete(irp, NtStatus::Success); return NtStatus::Success;
    }
    case pnp::IRP_MN_STOP_DEVICE:
        ext.started = false;
        detail::complete(irp, NtStatus::Success); return NtStatus::Success;
    case pnp::IRP_MN_REMOVE_DEVICE:
        ext.started = false;
        io::IoDetachDevice(ext.pdo);
        io::IoManager::get().deleteDevice(device);
        detail::complete(irp, NtStatus::Success); return NtStatus::Success;
    default:
        detail::complete(irp, NtStatus::InvalidDeviceRequest); return NtStatus::InvalidDeviceRequest;
    }
    detail::complete(irp, NtStatus::DeviceNotReady);
    return NtStatus::DeviceNotReady;
}

inline NtStatus AddDevice(io::DriverObject* driver, io::DeviceObject* pdo) noexcept {
    if (!driver || !pdo) return NtStatus::InvalidParameter;
    static uint32_t deviceNumber = 0;
    const std::wstring name = L"\\Device\\Serial" + std::to_wstring(deviceNumber++);
    std::shared_ptr<io::DeviceObject> fdo;
    const auto status = io::IoManager::get().createDeviceEx(driver, sizeof(DeviceExtension), name,
        static_cast<io::DeviceType>(FILE_DEVICE_SERIAL_PORT), io::FILE_DEVICE_SECURE_OPEN, false, fdo);
    if (status != NtStatus::Success) return status;
    auto& ext = *static_cast<DeviceExtension*>(fdo->deviceExtension);
    ext.pdo = pdo;
    if (!io::IoAttachDeviceToDeviceStack(fdo.get(), pdo)) {
        io::IoManager::get().deleteDevice(fdo.get());
        return NtStatus::NoSuchDevice;
    }
    fdo->flags |= io::DO_BUFFERED_IO;
    return NtStatus::Success;
}

inline void InitializeSerialDriver(io::DriverObject& driver) {
    driver.driverName = L"\\Driver\\Serial";
    driver.driverAddDevice = AddDevice;
    driver.setDispatch(io::IRP_MJ_CREATE, DispatchCreateClose);
    driver.setDispatch(io::IRP_MJ_CLOSE, DispatchCreateClose);
    driver.setDispatch(io::IRP_MJ_CLEANUP, DispatchCreateClose);
    driver.setDispatch(io::IRP_MJ_DEVICE_CONTROL, DispatchDeviceControl);
    driver.setDispatch(io::IRP_MJ_READ, DispatchReadWrite);
    driver.setDispatch(io::IRP_MJ_WRITE, DispatchReadWrite);
    driver.setDispatch(io::IRP_MJ_PNP, DispatchPnp);
    auto service = std::make_shared<scm::ServiceRecord>();
    service->serviceName = L"serial";
    service->displayName = L"Serial Port Driver";
    service->serviceType = scm::SERVICE_KERNEL_DRIVER;
    service->startType = scm::SERVICE_SYSTEM_START;
    service->errorControl = scm::SERVICE_ERROR_NORMAL;
    service->binaryPath = L"\\SystemRoot\\System32\\drivers\\serial.sys";
    service->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(service);
    version::VersionDatabase::Instance().RegisterModule("serial.sys", "10.0.26100.1", "Serial Port Driver");
}

} // namespace micant::km::serial
