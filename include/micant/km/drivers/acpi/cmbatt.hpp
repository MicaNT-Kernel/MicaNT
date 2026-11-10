#pragma once

// ACPI control-method battery miniport. This clean-room implementation reads
// the standard _STA, _BIF and _BST objects and feeds the existing battery
// class device; it does not synthesize pack measurements.
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "micant/km/executive/bms.hpp"
#include "micant/km/drivers/bus/battclass.hpp"
#include "micant/km/executive/pnp.hpp"
#include "micant/km/hal/acpi.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::km::acpi::cmbatt {

struct FirmwareBatterySnapshot {
    bool present{};
    po::bms::BatteryInformation information{};
    po::bms::BatteryStatus status{};
    std::array<po::bms::BatteryReportingScale, 4> reportingScales{};
    uint32_t reportingScaleCount{};
    std::string deviceName;
    std::string serialNumber;
    std::string manufacturer;
};

struct MiniportExtension {
    io::DeviceObject* pdo{};
    void* classData{};
    bool started{};
};

class ControlMethodBatteryMiniport {
public:
    static bool QueryFirmware(FirmwareBatterySnapshot& snapshot) {
        auto& subsystem = TitanAcpiSubsystem::Instance();
        if (!subsystem.isInitialized()) subsystem.initialize();
        const auto& ns = subsystem.getNamespace();

        const bool acOnline = queryAcOnline(ns);

        const auto sta = ns.evaluate("\\_SB.BAT0._STA");
        snapshot.present = sta.type == AmlObjectType::Integer && (sta.integerVal & 0x01U) != 0;
        if (!snapshot.present) return true;

        // ACPI 4.0 _BIX extends and supersedes _BIF. Prefer it when present,
        // while retaining support for firmware that only provides _BIF.
        const bool hasBix = static_cast<bool>(ns.resolvePath("\\_SB.BAT0._BIX"));
        const auto bif = ns.evaluate(hasBix ? "\\_SB.BAT0._BIX" : "\\_SB.BAT0._BIF");
        const auto bst = ns.evaluate("\\_SB.BAT0._BST");
        const size_t base = hasBix ? 1 : 0;
        if (bif.type != AmlObjectType::Package ||
            bif.packageElements.size() < (hasBix ? 20U : 13U) ||
            bst.type != AmlObjectType::Package || bst.packageElements.size() < 4) return false;
        for (size_t i = 0; i < (hasBix ? 16U : 9U); ++i) {
            if (bif.packageElements[i].type != AmlObjectType::Integer) return false;
        }
        for (size_t i = 0; i < 4; ++i) {
            if (bst.packageElements[i].type != AmlObjectType::Integer) return false;
        }

        const uint32_t unit = static_cast<uint32_t>(bif.packageElements[base].integerVal);
        if (unit > 1) return false;
        const uint32_t designVoltage = clamp32(bif.packageElements[base + 4].integerVal);
        auto& info = snapshot.information;
        info.capabilities = po::bms::BATTERY_SYSTEM_BATTERY;
        info.technology = bif.packageElements[base + 3].integerVal ? 1 : 0;
        info.designedCapacity = toMilliwattHours(bif.packageElements[base + 1].integerVal, unit, designVoltage);
        info.fullChargedCapacity = toMilliwattHours(bif.packageElements[base + 2].integerVal, unit, designVoltage);
        info.defaultAlert1 = toMilliwattHours(bif.packageElements[base + 5].integerVal, unit, designVoltage);
        info.defaultAlert2 = toMilliwattHours(bif.packageElements[base + 6].integerVal, unit, designVoltage);
        info.criticalBias = 0;
        info.cycleCount = hasBix ? clamp32(bif.packageElements[8].integerVal) : 0;
        const size_t granularity1Index = hasBix ? 14 : 7;
        const size_t granularity2Index = hasBix ? 15 : 8;
        const size_t modelIndex = hasBix ? 16 : 9;
        const size_t serialIndex = hasBix ? 17 : 10;
        const size_t chemistryIndex = hasBix ? 18 : 11;
        const size_t manufacturerIndex = hasBix ? 19 : 12;
        if (bif.packageElements[modelIndex].type != AmlObjectType::String ||
            bif.packageElements[serialIndex].type != AmlObjectType::String ||
            bif.packageElements[chemistryIndex].type != AmlObjectType::String ||
            bif.packageElements[manufacturerIndex].type != AmlObjectType::String) return false;
        setChemistry(info.chemistry, bif.packageElements[chemistryIndex]);
        snapshot.deviceName = stringOrEmpty(bif.packageElements[modelIndex]);
        snapshot.serialNumber = stringOrEmpty(bif.packageElements[serialIndex]);
        snapshot.manufacturer = stringOrEmpty(bif.packageElements[manufacturerIndex]);

        const auto granularity1 = bif.packageElements[granularity1Index].integerVal;
        const auto granularity2 = bif.packageElements[granularity2Index].integerVal;
        if (granularity1 != 0 && granularity1 != UINT32_MAX) {
            snapshot.reportingScales[0] = {
                toMilliwattHours(granularity1, unit, designVoltage), info.defaultAlert1};
            snapshot.reportingScaleCount = 1;
        }
        if (granularity2 != 0 && granularity2 != UINT32_MAX) {
            const uint32_t scaleIndex = snapshot.reportingScaleCount;
            snapshot.reportingScales[scaleIndex] = {
                toMilliwattHours(granularity2, unit, designVoltage), info.fullChargedCapacity};
            ++snapshot.reportingScaleCount;
        }

        const uint32_t state = clamp32(bst.packageElements[0].integerVal);
        const uint32_t rawRate = clamp32(bst.packageElements[1].integerVal);
        const uint32_t capacity = toMilliwattHours(bst.packageElements[2].integerVal, unit, designVoltage);
        const uint32_t voltage = clamp32(bst.packageElements[3].integerVal);
        auto& status = snapshot.status;
        status = {};
        status.powerState = acOnline ? po::bms::BATTERY_POWER_ON_LINE : 0;
        status.capacity = capacity;
        status.voltage = voltage;
        if (unit == 1 && voltage != 0) {
            const uint64_t converted = static_cast<uint64_t>(rawRate) * voltage / 1000U;
            status.rate = clampSigned32(converted);
        } else {
            status.rate = clampSigned32(rawRate);
        }
        if (state & 0x01U) {
            status.powerState |= po::bms::BATTERY_DISCHARGING;
            status.rate = -status.rate;
        }
        if (state & 0x02U) status.powerState |= po::bms::BATTERY_CHARGING;
        if (state & 0x04U) status.powerState |= po::bms::BATTERY_CRITICAL;
        return true;
    }

    static bool RefreshClassDevice() {
        FirmwareBatterySnapshot snapshot{};
        const bool queried = QueryFirmware(snapshot);
        if (!queried) snapshot.present = false;
        po::bms::SmartBatterySubsystem::get().updateFromAcpi(
            snapshot.present, snapshot.information, snapshot.status,
            snapshot.deviceName, snapshot.serialNumber, snapshot.manufacturer,
            snapshot.reportingScales, snapshot.reportingScaleCount);
        return queried;
    }

    static NtStatus QueryTag(void* context, uint32_t* batteryTag) {
        if (!context || !batteryTag) return NtStatus::InvalidParameter;
        (void)RefreshClassDevice();
        *batteryTag = po::bms::SmartBatterySubsystem::get().getBatteryTag();
        return NtStatus::Success;
    }

    static NtStatus QueryInformation(void* context, uint32_t batteryTag, uint32_t level,
                                     int32_t atRate, void* buffer, uint32_t bufferLength,
                                     uint32_t* returnedLength) {
        if (!context || !returnedLength || (!buffer && bufferLength)) return NtStatus::InvalidParameter;
        *returnedLength = 0;
        (void)RefreshClassDevice();
        auto& battery = po::bms::SmartBatterySubsystem::get();
        if (batteryTag == 0 || batteryTag != battery.getBatteryTag()) return NtStatus::DeviceNotReady;
        const po::bms::BatteryQueryInformation query{
            batteryTag, static_cast<po::bms::BatteryQueryInfoLevel>(level), static_cast<uint32_t>(atRate)};
        size_t bytes = 0;
        const auto status = battery.handleIoctl(po::bms::IOCTL_BATTERY_QUERY_INFORMATION,
            &query, sizeof(query), buffer, bufferLength, bytes);
        *returnedLength = static_cast<uint32_t>(std::min<size_t>(bytes, UINT32_MAX));
        return status;
    }

    static NtStatus SetInformation(void* context, uint32_t batteryTag, uint32_t, void*) {
        if (!context) return NtStatus::InvalidParameter;
        (void)RefreshClassDevice();
        const auto currentTag = po::bms::SmartBatterySubsystem::get().getBatteryTag();
        if (!batteryTag || batteryTag != currentTag) return NtStatus::DeviceNotReady;
        // This ACPI device has no _BTP/_BMC implementation, so accepting a
        // firmware trip-point or charge-control write would be false success.
        return NtStatus::InvalidDeviceRequest;
    }

    static NtStatus QueryStatus(void* context, uint32_t batteryTag, void* output) {
        if (!context || !output) return NtStatus::InvalidParameter;
        (void)RefreshClassDevice();
        auto& battery = po::bms::SmartBatterySubsystem::get();
        if (!batteryTag || batteryTag != battery.getBatteryTag()) return NtStatus::DeviceNotReady;
        const auto& status = battery.getStatus();
        std::memcpy(output, &status, sizeof(status));
        return NtStatus::Success;
    }

    static NtStatus SetStatusNotify(void* context, uint32_t batteryTag, po::bms::BATTERY_NOTIFY*) {
        if (!context) return NtStatus::InvalidParameter;
        if (!batteryTag || batteryTag != po::bms::SmartBatterySubsystem::get().getBatteryTag())
            return NtStatus::DeviceNotReady;
        // The ACPI event/Notify path is not wired yet; do not claim monitoring.
        return NtStatus::NotSupported;
    }

    static NtStatus DisableStatusNotify(void* context) {
        return context ? NtStatus::NotSupported : NtStatus::InvalidParameter;
    }

    static NtStatus DispatchPnp(io::DeviceObject* device, io::Irp* irp) noexcept {
        if (!device || !device->deviceExtension || !irp) return NtStatus::InvalidParameter;
        auto& extension = *static_cast<MiniportExtension*>(device->deviceExtension);
        switch (irp->minorFunction) {
        case pnp::IRP_MN_START_DEVICE:
            extension.started = true;
            (void)RefreshClassDevice();
            if (!InterfaceSymbolicLink().empty())
                (void)pnp::PnpManager::get().setDeviceInterfaceState(InterfaceSymbolicLink(), true);
            device->flags = (device->flags | io::DO_BUFFERED_IO | io::DO_POWER_PAGABLE) &
                ~io::DO_DEVICE_INITIALIZING;
            break;
        case pnp::IRP_MN_STOP_DEVICE:
            extension.started = false;
            if (!InterfaceSymbolicLink().empty())
                (void)pnp::PnpManager::get().setDeviceInterfaceState(InterfaceSymbolicLink(), false);
            break;
        case pnp::IRP_MN_REMOVE_DEVICE:
            extension.started = false;
            if (!InterfaceSymbolicLink().empty())
                (void)pnp::PnpManager::get().setDeviceInterfaceState(InterfaceSymbolicLink(), false);
            if (extension.classData) {
                (void)BatteryClassUnload(extension.classData);
                extension.classData = nullptr;
            }
            io::IoDetachDevice(extension.pdo);
            io::IoManager::get().deleteDevice(device);
            break;
        default:
            irp->ioStatus.status = NtStatus::InvalidDeviceRequest;
            irp->ioStatus.information = 0;
            return NtStatus::InvalidDeviceRequest;
        }
        irp->ioStatus.status = NtStatus::Success;
        irp->ioStatus.information = 0;
        return NtStatus::Success;
    }

    static NtStatus AddDevice(io::DriverObject* driver, io::DeviceObject* pdo) noexcept {
        if (!driver || !pdo) return NtStatus::InvalidParameter;
        std::shared_ptr<io::DeviceObject> fdo;
        const auto status = io::IoManager::get().createDeviceEx(driver, sizeof(MiniportExtension),
            L"\\Device\\Cmbatt0", static_cast<io::DeviceType>(0x29), io::FILE_DEVICE_SECURE_OPEN,
            false, fdo);
        if (!NT_SUCCESS(status)) return status;
        auto& extension = *static_cast<MiniportExtension*>(fdo->deviceExtension);
        extension.pdo = pdo;
        if (!io::IoAttachDeviceToDeviceStack(fdo.get(), pdo)) {
            io::IoManager::get().deleteDevice(fdo.get());
            return NtStatus::NoSuchDevice;
        }
        po::bms::BATTERY_MINIPORT_INFO info{};
        info.MajorVersion = po::bms::BATTERY_CLASS_MAJOR_VERSION;
        info.MinorVersion = po::bms::BATTERY_CLASS_MINOR_VERSION;
        info.Context = &extension;
        info.QueryTag = QueryTag;
        info.QueryInformation = QueryInformation;
        info.SetInformation = SetInformation;
        info.QueryStatus = QueryStatus;
        info.SetStatusNotify = SetStatusNotify;
        info.DisableStatusNotify = DisableStatusNotify;
        info.Pdo = pdo;
        info.DeviceName = nullptr;
        const auto classStatus = BatteryClassInitializeDevice(&info, &extension.classData);
        if (!NT_SUCCESS(classStatus)) {
            io::IoDetachDevice(pdo);
            io::IoManager::get().deleteDevice(fdo.get());
            return classStatus;
        }
        const auto interfaceStatus = pnp::PnpManager::get().registerDeviceInterface(
            pdo, po::bms::GUID_DEVICE_BATTERY, {}, InterfaceSymbolicLink());
        if (!NT_SUCCESS(interfaceStatus)) {
            (void)BatteryClassUnload(extension.classData);
            extension.classData = nullptr;
            io::IoDetachDevice(pdo);
            io::IoManager::get().deleteDevice(fdo.get());
            return interfaceStatus;
        }
        fdo->flags = io::DO_BUFFERED_IO | io::DO_POWER_PAGABLE;
        return NtStatus::Success;
    }

    static io::DriverObject& DriverObject() {
        static io::DriverObject driver;
        return driver;
    }

    static void InitializeDriverObject() {
        auto& driver = DriverObject();
        driver.driverName = L"\\Driver\\Cmbatt";
        driver.driverAddDevice = AddDevice;
        driver.setDispatch(io::IRP_MJ_PNP, DispatchPnp);
    }

    static void RegisterService() {
        auto service = std::make_shared<scm::ServiceRecord>();
        service->serviceName = L"cmbatt";
        service->displayName = L"ACPI Control Method Battery Driver";
        service->serviceType = scm::SERVICE_KERNEL_DRIVER;
        service->startType = scm::SERVICE_SYSTEM_START;
        service->errorControl = scm::SERVICE_ERROR_NORMAL;
        service->binaryPath = L"\\SystemRoot\\System32\\drivers\\cmbatt.sys";
        service->status.dwCurrentState = scm::SERVICE_RUNNING;
        scm::ServiceControlManager::get().registerServiceRecord(service);
        version::VersionDatabase::Instance().RegisterModule(
            "cmbatt.sys", "10.0.26100.1", "ACPI Control Method Battery Driver");
    }

private:
    static std::wstring& InterfaceSymbolicLink() {
        static std::wstring link;
        return link;
    }
    static bool queryAcOnline(const AcpiNamespace& ns) {
        const auto root = ns.getRoot();
        if (!root) return false;
        return queryAcOnlineNode(ns, root, "", 0);
    }
    static bool queryAcOnlineNode(const AcpiNamespace& ns,
                                  const std::shared_ptr<AmlNode>& node,
                                  const std::string& path, uint32_t depth) {
        if (!node || depth > 64) return false;
        const auto hid = node->children.find("_HID");
        if (hid != node->children.end() && hid->second &&
            hid->second->value.type == AmlObjectType::String &&
            hid->second->value.stringVal == "ACPI0003") {
            const auto psr = ns.evaluate(path + "._PSR");
            return psr.type == AmlObjectType::Integer && psr.integerVal != 0;
        }
        for (const auto& [name, child] : node->children) {
            if (name == "_HID") continue;
            const std::string childPath = path.empty() ? "\\" + name : path + "." + name;
            if (queryAcOnlineNode(ns, child, childPath, depth + 1)) return true;
        }
        return false;
    }
    static uint32_t clamp32(uint64_t value) noexcept {
        return static_cast<uint32_t>(std::min<uint64_t>(value, std::numeric_limits<uint32_t>::max()));
    }
    static int32_t clampSigned32(uint64_t value) noexcept {
        return static_cast<int32_t>(std::min<uint64_t>(value, std::numeric_limits<int32_t>::max()));
    }
    static uint32_t toMilliwattHours(uint64_t value, uint32_t unit, uint32_t voltageMv) noexcept {
        if (unit == 0) return clamp32(value);
        if (voltageMv == 0) return 0;
        if (value > std::numeric_limits<uint64_t>::max() / voltageMv)
            return std::numeric_limits<uint32_t>::max();
        return clamp32(value * static_cast<uint64_t>(voltageMv) / 1000U);
    }
    static std::string stringOrEmpty(const AmlValue& value) {
        return value.type == AmlObjectType::String ? value.stringVal : std::string{};
    }
    static void setChemistry(uint8_t (&destination)[4], const AmlValue& value) noexcept {
        destination[0] = '?'; destination[1] = '?'; destination[2] = '?'; destination[3] = '?';
        if (value.type != AmlObjectType::String) return;
        const auto& chemistry = value.stringVal;
        if (chemistry.find("Li") != std::string::npos || chemistry.find("li") != std::string::npos) {
            destination[0] = 'L'; destination[1] = 'I'; destination[2] = 'O'; destination[3] = 'N';
        } else if (chemistry.find("NiMH") != std::string::npos) {
            destination[0] = 'N'; destination[1] = 'i'; destination[2] = 'M'; destination[3] = 'H';
        } else if (chemistry.find("NiCd") != std::string::npos) {
            destination[0] = 'N'; destination[1] = 'i'; destination[2] = 'C'; destination[3] = 'd';
        } else if (chemistry.find("Pb") != std::string::npos) {
            destination[0] = 'P'; destination[1] = 'b'; destination[2] = 'A'; destination[3] = 'c';
        }
    }
};

inline void InitializeCmbattDriver() {
    ControlMethodBatteryMiniport::RegisterService();
    ControlMethodBatteryMiniport::InitializeDriverObject();
}

} // namespace micant::km::acpi::cmbatt
