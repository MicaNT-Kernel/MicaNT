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
#include "micant/km/hal/acpi.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::km::acpi::cmbatt {

struct FirmwareBatterySnapshot {
    bool present{};
    po::bms::BatteryInformation information{};
    po::bms::BatteryStatus status{};
    std::string deviceName;
    std::string serialNumber;
    std::string manufacturer;
};

class ControlMethodBatteryMiniport {
public:
    static bool QueryFirmware(FirmwareBatterySnapshot& snapshot) {
        auto& subsystem = TitanAcpiSubsystem::Instance();
        if (!subsystem.isInitialized()) subsystem.initialize();
        const auto& ns = subsystem.getNamespace();

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
            bif.packageElements.size() < (hasBix ? 20U : 11U) ||
            bst.type != AmlObjectType::Package || bst.packageElements.size() < 4) return false;
        for (size_t i = 0; i < (hasBix ? 16U : 7U); ++i) {
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
        const size_t modelIndex = hasBix ? 16 : 7;
        const size_t serialIndex = hasBix ? 17 : 8;
        const size_t chemistryIndex = hasBix ? 18 : 9;
        const size_t manufacturerIndex = hasBix ? 19 : 10;
        setChemistry(info.chemistry, bif.packageElements[chemistryIndex]);
        snapshot.deviceName = stringOrEmpty(bif.packageElements[modelIndex]);
        snapshot.serialNumber = stringOrEmpty(bif.packageElements[serialIndex]);
        snapshot.manufacturer = stringOrEmpty(bif.packageElements[manufacturerIndex]);

        const uint32_t state = clamp32(bst.packageElements[0].integerVal);
        const uint32_t rawRate = clamp32(bst.packageElements[1].integerVal);
        const uint32_t capacity = toMilliwattHours(bst.packageElements[2].integerVal, unit, designVoltage);
        const uint32_t voltage = clamp32(bst.packageElements[3].integerVal);
        auto& status = snapshot.status;
        status = {};
        status.powerState = 0;
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
            snapshot.deviceName, snapshot.serialNumber, snapshot.manufacturer);
        return queried;
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
            destination[0] = 'L'; destination[1] = 'I'; destination[2] = 'o'; destination[3] = 'n';
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
    (void)ControlMethodBatteryMiniport::RefreshClassDevice();
}

} // namespace micant::km::acpi::cmbatt
