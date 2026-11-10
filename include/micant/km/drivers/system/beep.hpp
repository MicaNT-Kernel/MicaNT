#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "micant/km/core/ke.hpp"
#include "micant/km/core/timer.hpp"
#include "micant/km/executive/io.hpp"
#include "micant/km/hal/hal.hpp"

namespace micant::km::system {

inline constexpr uint32_t FILE_DEVICE_BEEP = 0x00000001;
inline constexpr uint32_t IOCTL_BEEP_SET = 0x00010000;
inline constexpr uint32_t BEEP_FREQUENCY_MINIMUM = 0x25;
inline constexpr uint32_t BEEP_FREQUENCY_MAXIMUM = 0x7FFF;
inline constexpr wchar_t DD_BEEP_DEVICE_NAME_U[] = L"\\Device\\Beep";

struct BEEP_SET_PARAMETERS {
    uint32_t Frequency{0};
    uint32_t Duration{0};
};
static_assert(sizeof(BEEP_SET_PARAMETERS) == 8);

class BeepDriver {
public:
    static BeepDriver& Instance() { static BeepDriver instance; return instance; }

    NtStatus dispatch(io::DeviceObject*, io::Irp* irp) {
        if (!irp) return NtStatus::InvalidParameter;
        NtStatus status;
        uintptr_t information = 0;
        switch (irp->majorFunction) {
        case io::IRP_MJ_CREATE:
        case io::IRP_MJ_CLOSE:
        case io::IRP_MJ_CLEANUP:
            status = NtStatus::Success;
            break;
        case io::IRP_MJ_DEVICE_CONTROL:
            status = setTone(irp);
            break;
        default:
            status = NtStatus::InvalidDeviceRequest;
            break;
        }
        irp->ioStatus.status = status;
        irp->ioStatus.information = information;
        return status;
    }

    NtStatus setTone(const BEEP_SET_PARAMETERS& parameters) {
        if (parameters.Frequency != 0 &&
            (parameters.Frequency < BEEP_FREQUENCY_MINIMUM || parameters.Frequency > BEEP_FREQUENCY_MAXIMUM))
            return NtStatus::InvalidParameter;

        if (parameters.Frequency == 0 || parameters.Duration == 0) {
            (void)timer::KeCancelTimer(&m_timer);
            return hal::SetPcSpeakerTone(0) ? NtStatus::Success : NtStatus::DeviceNotReady;
        }
        if (!hal::SetPcSpeakerTone(parameters.Frequency)) return NtStatus::DeviceNotReady;

        if (parameters.Duration != UINT32_MAX) {
            (void)timer::KeCancelTimer(&m_timer);
            LargeInteger due{};
            due.quadPart = -static_cast<int64_t>(parameters.Duration) * 10000;
            (void)timer::KeSetTimer(&m_timer, due, &m_stopDpc);
        } else {
            (void)timer::KeCancelTimer(&m_timer);
        }
        return NtStatus::Success;
    }

private:
    BeepDriver() {
        timer::KeInitializeTimer(&m_timer);
        KeInitializeDpc(&m_stopDpc, [](ke::KDPC*, void*, void*, void*) {
            (void)hal::SetPcSpeakerTone(0);
        }, nullptr);
    }

    NtStatus setTone(io::Irp* irp) {
        auto* stack = irp->getCurrentStack();
        uint32_t code = stack ? stack->parameters.deviceIoControl.ioControlCode : 0;
        if (!code) code = irp->byteOffset.lowPart;
        if (code != IOCTL_BEEP_SET) return NtStatus::NotImplemented;
        const uint32_t length = stack ? stack->parameters.deviceIoControl.inputBufferLength
                                      : static_cast<uint32_t>(irp->byteOffset.highPart);
        void* buffer = irp->systemBuffer ? irp->systemBuffer : irp->userBuffer;
        if (!buffer || length < sizeof(BEEP_SET_PARAMETERS)) return NtStatus::InvalidParameter;
        BEEP_SET_PARAMETERS parameters{};
        std::memcpy(&parameters, buffer, sizeof(parameters));
        return setTone(parameters);
    }

    timer::KTIMER m_timer{};
    ke::KDPC m_stopDpc{};
};

inline void InitializeBeepDriver(io::DriverObject& driver) {
    driver.driverName = L"\\Driver\\Beep";
    for (uint8_t major : {io::IRP_MJ_CREATE, io::IRP_MJ_CLOSE, io::IRP_MJ_CLEANUP,
                          io::IRP_MJ_DEVICE_CONTROL}) {
        driver.setDispatch(major, [](io::DeviceObject* device, io::Irp* irp) {
            return BeepDriver::Instance().dispatch(device, irp);
        });
    }
}

inline std::shared_ptr<io::DeviceObject> InitializeBeepDevice(io::IoManager& ioManager,
    io::DriverObject& driver) {
    auto device = ioManager.createDevice(&driver, DD_BEEP_DEVICE_NAME_U,
        static_cast<io::DeviceType>(FILE_DEVICE_BEEP));
    if (device) device->flags = (device->flags | io::DO_BUFFERED_IO) & ~io::DO_DEVICE_INITIALIZING;
    return device;
}

} // namespace micant::km::system
