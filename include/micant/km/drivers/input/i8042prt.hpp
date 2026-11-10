#pragma once

#include <cstdint>
#include <mutex>

#include "micant/km/drivers/input/kbdclass.hpp"
#include "micant/km/drivers/input/mouclass.hpp"
#include "micant/km/executive/io.hpp"

namespace micant::km::hid {

inline constexpr uint32_t IOCTL_INTERNAL_KEYBOARD_CONNECT = 0x000B0203;
inline constexpr uint32_t IOCTL_INTERNAL_MOUSE_CONNECT = 0x000F0203;
struct CONNECT_DATA { io::DeviceObject* ClassDeviceObject{nullptr}; void* ClassService{nullptr}; };
inline uint8_t I8042KeyboardPortTag = 0;
inline uint8_t I8042PointerPortTag = 0;
using KeyboardClassServiceRoutine = void (WINAPI *)(io::DeviceObject*, KEYBOARD_INPUT_DATA*, KEYBOARD_INPUT_DATA*, uint32_t*);
using MouseClassServiceRoutine = void (WINAPI *)(io::DeviceObject*, MOUSE_INPUT_DATA*, MOUSE_INPUT_DATA*, uint32_t*);

inline constexpr uint16_t I8042_DATA_PORT = 0x0060;
inline constexpr uint16_t I8042_STATUS_PORT = 0x0064;
inline constexpr uint16_t I8042_COMMAND_PORT = 0x0064;
inline constexpr uint8_t I8042_STS_OBF = 0x01, I8042_STS_IBF = 0x02, I8042_STS_SYS = 0x04;
inline constexpr uint8_t I8042_STS_A2 = 0x08, I8042_STS_AUXOBF = 0x20;
inline constexpr uint8_t I8042_CMD_READ_CFG = 0x20, I8042_CMD_WRITE_CFG = 0x60;
inline constexpr uint8_t I8042_CMD_DIS_MOUSE = 0xA7, I8042_CMD_EN_MOUSE = 0xA8;
inline constexpr uint8_t I8042_CMD_SELF_TEST = 0xAA, I8042_CMD_KBD_TEST = 0xAB;
inline constexpr uint8_t I8042_CMD_EN_KBD = 0xAE, I8042_CMD_WRITE_AUX = 0xD4;

class I8042PortDriver {
public:
    static I8042PortDriver& Instance() { static I8042PortDriver instance; return instance; }

    uint8_t readPort(uint16_t port) {
        std::lock_guard lock(m_lock);
        if (port == I8042_STATUS_PORT) return m_status;
        if (port == I8042_DATA_PORT) {
            m_status &= static_cast<uint8_t>(~(I8042_STS_OBF | I8042_STS_AUXOBF));
            return m_output;
        }
        return 0xFF;
    }
    void writePort(uint16_t port, uint8_t value) {
        std::lock_guard lock(m_lock);
        if (port == I8042_COMMAND_PORT) {
            m_lastCommand = value;
            switch (value) {
            case I8042_CMD_SELF_TEST: m_output = 0x55; m_status |= I8042_STS_OBF | I8042_STS_SYS; break;
            case I8042_CMD_KBD_TEST: m_output = 0; m_status |= I8042_STS_OBF; break;
            case I8042_CMD_READ_CFG: m_output = m_config; m_status |= I8042_STS_OBF; break;
            case I8042_CMD_EN_MOUSE: m_config |= 0x02; break;
            case I8042_CMD_DIS_MOUSE: m_config &= static_cast<uint8_t>(~0x02); break;
            case I8042_CMD_EN_KBD: m_config |= 0x01; break;
            default: break;
            }
        } else if (port == I8042_DATA_PORT) {
            if (m_lastCommand == I8042_CMD_WRITE_CFG) { m_config = value; m_lastCommand = 0; }
            else if (m_lastCommand == I8042_CMD_WRITE_AUX) {
                m_output = 0xFA; m_status |= I8042_STS_OBF | I8042_STS_AUXOBF; m_lastCommand = 0;
            } else { m_output = 0xFA; m_status |= I8042_STS_OBF; }
        }
    }

    void injectKeyboardIrq1(uint8_t scancode) {
        KEYBOARD_INPUT_DATA packet{};
        {
            std::lock_guard lock(m_lock);
            ++m_interrupts; m_output = scancode; m_status |= I8042_STS_OBF;
            if (scancode == 0xE0) { m_extendedE0 = true; return; }
            if (scancode == 0xE1) { m_extendedE1 = true; return; }
            packet.MakeCode = static_cast<uint16_t>(scancode & 0x7F);
            packet.Flags = (scancode & 0x80) ? KEY_BREAK : KEY_MAKE;
            if (m_extendedE0) { packet.Flags |= KEY_E0; m_extendedE0 = false; }
            if (m_extendedE1) { packet.Flags |= KEY_E1; m_extendedE1 = false; }
        }
        io::DeviceObject* device = nullptr; KeyboardClassServiceRoutine service = nullptr;
        { std::lock_guard lock(m_lock); device = m_keyboardDevice; service = m_keyboardService; }
        if (service) { uint32_t consumed = 0; service(device, &packet, &packet + 1, &consumed); }
    }

    void injectMouseIrq12(uint8_t status, int8_t dx, int8_t dy, int8_t wheel = 0) {
        MOUSE_INPUT_DATA packet{};
        {
            std::lock_guard lock(m_lock);
            ++m_interrupts; packet.LastX = dx; packet.LastY = -dy; packet.RawButtons = status & 7;
            const bool left = (status & 1) != 0, right = (status & 2) != 0, middle = (status & 4) != 0;
            if (left != m_left) packet.ButtonFlags |= left ? MOUSE_LEFT_BUTTON_DOWN : MOUSE_LEFT_BUTTON_UP;
            if (right != m_right) packet.ButtonFlags |= right ? MOUSE_RIGHT_BUTTON_DOWN : MOUSE_RIGHT_BUTTON_UP;
            if (middle != m_middle) packet.ButtonFlags |= middle ? MOUSE_MIDDLE_BUTTON_DOWN : MOUSE_MIDDLE_BUTTON_UP;
            m_left = left; m_right = right; m_middle = middle;
            if (wheel) { packet.ButtonFlags |= MOUSE_WHEEL; packet.ButtonData = static_cast<uint16_t>(static_cast<int16_t>(wheel) * 120); }
        }
        io::DeviceObject* device = nullptr; MouseClassServiceRoutine service = nullptr;
        { std::lock_guard lock(m_lock); device = m_mouseDevice; service = m_mouseService; }
        if (service) { uint32_t consumed = 0; service(device, &packet, &packet + 1, &consumed); }
    }
    uint64_t interruptsServiced() { std::lock_guard lock(m_lock); return m_interrupts; }

    NtStatus dispatch(io::DeviceObject* device, io::Irp* irp) {
        if (!irp) return NtStatus::InvalidParameter;
        if (irp->majorFunction == io::IRP_MJ_INTERNAL_DEVICE_CONTROL) {
            auto* stack = irp->getCurrentStack();
            const uint32_t code = stack ? stack->parameters.deviceIoControl.ioControlCode : irp->byteOffset.lowPart;
            const size_t length = stack ? stack->parameters.deviceIoControl.inputBufferLength
                                        : static_cast<size_t>(irp->byteOffset.highPart);
            const auto* connection = stack
                ? static_cast<const CONNECT_DATA*>(stack->parameters.deviceIoControl.type3InputBuffer)
                : static_cast<const CONNECT_DATA*>(irp->userBuffer);
            NtStatus status = NtStatus::Success;
            {
                std::lock_guard lock(m_lock);
                if (!connection || length < sizeof(CONNECT_DATA) || !connection->ClassDeviceObject || !connection->ClassService)
                    status = NtStatus::InvalidParameter;
                else if (code == IOCTL_INTERNAL_KEYBOARD_CONNECT) {
                    if (!device || device->deviceExtension != &I8042KeyboardPortTag) status = NtStatus::InvalidParameter;
                    else if (m_keyboardService) status = NtStatus::SharingViolation;
                    else { m_keyboardDevice = connection->ClassDeviceObject;
                        m_keyboardService = reinterpret_cast<KeyboardClassServiceRoutine>(connection->ClassService); }
                } else if (code == IOCTL_INTERNAL_MOUSE_CONNECT) {
                    if (!device || device->deviceExtension != &I8042PointerPortTag) status = NtStatus::InvalidParameter;
                    else if (m_mouseService) status = NtStatus::SharingViolation;
                    else { m_mouseDevice = connection->ClassDeviceObject;
                        m_mouseService = reinterpret_cast<MouseClassServiceRoutine>(connection->ClassService); }
                } else status = NtStatus::InvalidDeviceRequest;
            }
            irp->ioStatus.information = 0; irp->ioStatus.status = status; return status;
        }
        irp->ioStatus.information = 0;
        irp->ioStatus.status = (irp->majorFunction == io::IRP_MJ_CREATE ||
            irp->majorFunction == io::IRP_MJ_CLOSE || irp->majorFunction == io::IRP_MJ_CLEANUP)
            ? NtStatus::Success : NtStatus::InvalidDeviceRequest;
        return irp->ioStatus.status;
    }

private:
    std::mutex m_lock;
    uint8_t m_status{I8042_STS_SYS}, m_output{0x55}, m_config{0x47}, m_lastCommand{0};
    bool m_extendedE0{false}, m_extendedE1{false}, m_left{false}, m_right{false}, m_middle{false};
    uint64_t m_interrupts{0};
    io::DeviceObject* m_keyboardDevice{nullptr};
    io::DeviceObject* m_mouseDevice{nullptr};
    KeyboardClassServiceRoutine m_keyboardService{nullptr};
    MouseClassServiceRoutine m_mouseService{nullptr};
};

inline uint8_t WINAPI I8042ReadPortUchar(uint16_t port) { return I8042PortDriver::Instance().readPort(port); }
inline void WINAPI I8042WritePortUchar(uint16_t port, uint8_t value) { I8042PortDriver::Instance().writePort(port, value); }
inline void I8042InjectKeyboardIrq1(uint8_t scanCode) { I8042PortDriver::Instance().injectKeyboardIrq1(scanCode); }
inline void I8042InjectMouseIrq12Packet(uint8_t status, int8_t x, int8_t y, int8_t wheel = 0) {
    I8042PortDriver::Instance().injectMouseIrq12(status, x, y, wheel);
}
inline uint64_t I8042InterruptsServiced() { return I8042PortDriver::Instance().interruptsServiced(); }
inline void InitializeI8042PortDriver(io::DriverObject& driver) {
    driver.driverName = L"\\Driver\\i8042prt";
    for (uint8_t major : {io::IRP_MJ_CREATE, io::IRP_MJ_CLOSE, io::IRP_MJ_CLEANUP,
            io::IRP_MJ_DEVICE_CONTROL, io::IRP_MJ_INTERNAL_DEVICE_CONTROL}) {
        driver.setDispatch(major, [](io::DeviceObject* dev, io::Irp* irp) {
            return I8042PortDriver::Instance().dispatch(dev, irp);
        });
    }
}
inline NtStatus I8042ConnectClassDevice(io::DeviceObject& port, io::DeviceObject& classDevice,
    uint32_t controlCode, void* classService) {
    CONNECT_DATA connection{&classDevice, classService};
    io::Irp irp{};
    irp.majorFunction = io::IRP_MJ_INTERNAL_DEVICE_CONTROL;
    irp.deviceObject = &port;
    irp.byteOffset.lowPart = controlCode;
    irp.userBuffer = &connection;
    irp.byteOffset.highPart = sizeof(connection);
    auto* stack = irp.getCurrentStack();
    stack->parameters.deviceIoControl.ioControlCode = controlCode;
    stack->parameters.deviceIoControl.inputBufferLength = sizeof(connection);
    stack->parameters.deviceIoControl.type3InputBuffer = &connection;
    return I8042PortDriver::Instance().dispatch(&port, &irp);
}

} // namespace micant::km::hid
