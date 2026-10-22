/**
 * @file bms.cpp
 * @brief Sovereign Clean-Room Windows NT Battery Management System (BMS) Executive Implementation
 *
 * Connects the smart battery class driver to the NT Object Manager namespace (\Device\Battery0),
 * I/O Manager IRP packet hierarchy, and the Executive Power Manager (micant::po::PowerManager).
 */

#include "micant/km/executive/bms.hpp"
#include "micant/km/executive/ob.hpp"
#include "micant/km/executive/io.hpp"
#include "micant/km/executive/po.hpp"

namespace micant::po::bms {

static NtStatus BattcDispatch(micant::io::DeviceObject* device, micant::io::Irp* irp) {
    (void)device;
    if (!irp) return NtStatus::InvalidParameter;

    auto* stack = micant::io::IoGetCurrentIrpStackLocation(irp);
    if (!stack) {
        irp->ioStatus.status = NtStatus::InvalidParameter;
        irp->ioStatus.information = 0;
        micant::io::IoCompleteRequest(irp, 0);
        return NtStatus::InvalidParameter;
    }

    switch (stack->majorFunction) {
        case micant::io::IRP_MJ_CREATE:
        case micant::io::IRP_MJ_CLOSE:
        case micant::io::IRP_MJ_CLEANUP: {
            irp->ioStatus.status = NtStatus::Success;
            irp->ioStatus.information = 0;
            micant::io::IoCompleteRequest(irp, 0);
            return NtStatus::Success;
        }

        case micant::io::IRP_MJ_DEVICE_CONTROL: {
            auto& bms = SmartBatterySubsystem::get();
            size_t bytesRet = 0;
            void* inBuf = irp->systemBuffer;
            void* outBuf = irp->userBuffer ? irp->userBuffer : irp->systemBuffer;

            NtStatus status = bms.handleIoctl(
                stack->parameters.deviceIoControl.ioControlCode,
                inBuf,
                stack->parameters.deviceIoControl.inputBufferLength,
                outBuf,
                stack->parameters.deviceIoControl.outputBufferLength,
                bytesRet
            );

            irp->ioStatus.status = status;
            irp->ioStatus.information = bytesRet;
            micant::io::IoCompleteRequest(irp, 0);
            return status;
        }

        default: {
            irp->ioStatus.status = NtStatus::NotImplemented;
            irp->ioStatus.information = 0;
            micant::io::IoCompleteRequest(irp, 0);
            return NtStatus::NotImplemented;
        }
    }
}

static micant::io::DriverObject s_battcDriver;
static micant::io::DeviceObject s_batteryDevice;
static micant::ob::ObjectHeader s_batteryHeader;

void InitializeBatterySubsystem() noexcept {
    auto& ons = micant::ob::ObjectNamespace::get();
    ons.initializeRoot();

    // 1. Setup Battery Class Driver (battc.sys)
    s_battcDriver.driverName = L"\\Driver\\BattC";
    s_battcDriver.setDispatch(micant::io::IRP_MJ_CREATE, BattcDispatch);
    s_battcDriver.setDispatch(micant::io::IRP_MJ_CLOSE, BattcDispatch);
    s_battcDriver.setDispatch(micant::io::IRP_MJ_CLEANUP, BattcDispatch);
    s_battcDriver.setDispatch(micant::io::IRP_MJ_DEVICE_CONTROL, BattcDispatch);

    // 2. Setup Device Object (\Device\Battery0)
    s_batteryDevice.driverObject = &s_battcDriver;
    s_batteryDevice.deviceType = micant::io::DeviceType::Battery;
    s_batteryDevice.deviceName = L"\\Device\\Battery0";
    s_batteryDevice.flags = micant::io::DO_BUFFERED_IO | micant::io::DO_POWER_PAGABLE;

    auto devDir = ons.getDirectory(L"\\Device");
    if (!devDir) {
        ons.createDirectory(L"\\Device");
        devDir = ons.getDirectory(L"\\Device");
    }

    const auto* devType = ons.getType(L"Device");
    s_batteryHeader.type = devType;
    s_batteryHeader.objectName = L"Battery0";
    if (devDir) {
        devDir->insertObject(L"Battery0", &s_batteryHeader);
    }

    // 3. Create Symbolic Link \DosDevices\Battery0 -> \Device\Battery0
    ons.createSymbolicLink(L"\\DosDevices\\Battery0", L"\\Device\\Battery0");

    // 4. Synchronize Battery State into Executive PowerManager
    auto& po = micant::po::PowerManager::get();
    auto& bms = SmartBatterySubsystem::get();
    const auto& bStatus = bms.getStatus();
    const auto& bInfo = bms.getInformation();

    SystemBatteryState sysBat{};
    sysBat.acOnLine = (bStatus.powerState & BATTERY_POWER_ON_LINE) != 0;
    sysBat.batteryPresent = bms.isBatteryPresent();
    sysBat.charging = (bStatus.powerState & BATTERY_CHARGING) != 0;
    sysBat.discharging = (bStatus.powerState & BATTERY_DISCHARGING) != 0;
    sysBat.maxCapacity = bInfo.fullChargedCapacity;
    sysBat.remainingCapacity = bStatus.capacity;
    sysBat.rate = bStatus.rate;
    sysBat.estimatedTime = (bStatus.rate < 0) ? (bStatus.capacity * 3600) / static_cast<uint32_t>(std::abs(bStatus.rate)) : 0xFFFFFFFF;

    po.updateBatteryState(sysBat);
}

} // namespace micant::po::bms
