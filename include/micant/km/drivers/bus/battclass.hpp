#pragma once

// Public battery class/miniport contracts from batclass.h. Keep field order,
// fixed-width members, callback signatures and version constants ABI-exact.
#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "micant/km/executive/io.hpp"
#include "micant/km/executive/bms.hpp"
#include "micant/shared/ntdef.hpp"

namespace micant::po::bms {

using BatteryClassQueryTag = NtStatus (*)(void* context, uint32_t* batteryTag);
using BatteryClassQueryInformation = NtStatus (*)(void* context, uint32_t batteryTag,
    uint32_t level, int32_t atRate, void* buffer, uint32_t bufferLength, uint32_t* returnedLength);
using BatteryClassSetInformation = NtStatus (*)(void* context, uint32_t batteryTag,
    uint32_t level, void* buffer);
using BatteryClassQueryStatus = NtStatus (*)(void* context, uint32_t batteryTag, void* batteryStatus);

struct BATTERY_NOTIFY {
    uint32_t PowerState;
    uint32_t LowCapacity;
    uint32_t HighCapacity;
};
static_assert(sizeof(BATTERY_NOTIFY) == 12);

struct BATTERY_WMI_STATUS {
    uint32_t Tag;
    uint32_t RemainingCapacity;
    int32_t ChargeRate;
    int32_t DischargeRate;
    uint32_t Voltage;
    uint8_t PowerOnline;
    uint8_t Charging;
    uint8_t Discharging;
    uint8_t Critical;
};
struct BATTERY_WMI_RUNTIME { uint32_t Tag; uint32_t EstimatedRuntime; };
struct BATTERY_WMI_TEMPERATURE { uint32_t Tag; uint32_t Temperature; };
struct BATTERY_WMI_FULL_CHARGED_CAPACITY { uint32_t Tag; uint32_t FullChargedCapacity; };
struct BATTERY_WMI_CYCLE_COUNT { uint32_t Tag; uint32_t CycleCount; };
struct BATTERY_WMI_STATIC_DATA {
    uint32_t Tag;
    uint16_t ManufactureDate[25];
    BatteryReportingScale Granularity[4];
    uint32_t Capabilities;
    uint8_t Technology;
    uint32_t Chemistry;
    uint32_t DesignedCapacity;
    uint32_t DefaultAlert1;
    uint32_t DefaultAlert2;
    uint32_t CriticalBias;
    uint16_t Strings[1];
};
static_assert(sizeof(BATTERY_WMI_STATUS) == 24);
static_assert(sizeof(BATTERY_WMI_RUNTIME) == 8);
static_assert(offsetof(BATTERY_WMI_STATIC_DATA, Strings) == 116);

inline constexpr uint32_t BATTERY_WMI_STATUS_GUID_INDEX = 0;
inline constexpr uint32_t BATTERY_WMI_RUNTIME_GUID_INDEX = 1;
inline constexpr uint32_t BATTERY_WMI_TEMPERATURE_GUID_INDEX = 2;
inline constexpr uint32_t BATTERY_WMI_FULL_CHARGED_CAPACITY_GUID_INDEX = 3;
inline constexpr uint32_t BATTERY_WMI_CYCLE_COUNT_GUID_INDEX = 4;
inline constexpr uint32_t BATTERY_WMI_STATIC_DATA_GUID_INDEX = 5;
using BatteryClassSetStatusNotify = NtStatus (*)(void* context, uint32_t batteryTag, BATTERY_NOTIFY* notify);
using BatteryClassDisableStatusNotify = NtStatus (*)(void* context);

struct BATTERY_MINIPORT_INFO {
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    void* Context;
    BatteryClassQueryTag QueryTag;
    BatteryClassQueryInformation QueryInformation;
    BatteryClassSetInformation SetInformation;
    BatteryClassQueryStatus QueryStatus;
    BatteryClassSetStatusNotify SetStatusNotify;
    BatteryClassDisableStatusNotify DisableStatusNotify;
    io::DeviceObject* Pdo;
    PUNICODE_STRING DeviceName;
};
using PBATTERY_MINIPORT_INFO = BATTERY_MINIPORT_INFO*;
inline constexpr uint16_t BATTERY_CLASS_MAJOR_VERSION = 1;
inline constexpr uint16_t BATTERY_CLASS_MINOR_VERSION = 0;
inline constexpr GUID GUID_DEVICE_BATTERY = {
    0x72631E54, 0x78A4, 0x11D0, {0xBC, 0xF7, 0x00, 0xAA, 0x00, 0xB7, 0xB3, 0x2A}};
static_assert(offsetof(BATTERY_MINIPORT_INFO, MajorVersion) == 0);
static_assert(offsetof(BATTERY_MINIPORT_INFO, MinorVersion) == 2);
static_assert(offsetof(BATTERY_MINIPORT_INFO, Context) ==
    ((sizeof(void*) == 8) ? 8U : 4U));
static_assert(offsetof(BATTERY_MINIPORT_INFO, Pdo) ==
    ((sizeof(void*) == 8) ? 64U : 32U));
static_assert(sizeof(BATTERY_MINIPORT_INFO) == ((sizeof(void*) == 8) ? 80U : 40U));

struct BatteryClassDevice {
    BATTERY_MINIPORT_INFO Miniport{};
    std::mutex waitMutex;
    std::condition_variable waitCondition;
    uint64_t statusSequence{};
    bool unloaded{};
};

class BatteryClassRegistry {
public:
    static BatteryClassRegistry& Instance() {
        static BatteryClassRegistry registry;
        return registry;
    }

    NtStatus initializeDevice(const BATTERY_MINIPORT_INFO* info, void** classData) {
        if (!info || !classData || info->MajorVersion != BATTERY_CLASS_MAJOR_VERSION ||
            info->MinorVersion > BATTERY_CLASS_MINOR_VERSION || !info->Context || !info->Pdo ||
            info->DeviceName || !info->QueryTag || !info->QueryInformation || !info->SetInformation ||
            !info->QueryStatus || !info->SetStatusNotify || !info->DisableStatusNotify)
            return NtStatus::InvalidParameter;
        auto device = std::make_shared<BatteryClassDevice>();
        device->Miniport = *info;
        void* handle = device.get();
        std::lock_guard<std::mutex> guard(lock_);
        devices_.push_back(device);
        *classData = handle;
        return NtStatus::Success;
    }

    NtStatus unloadDevice(void* classData) {
        if (!classData) return NtStatus::InvalidParameter;
        std::shared_ptr<BatteryClassDevice> removed;
        {
            std::lock_guard<std::mutex> guard(lock_);
            for (auto it = devices_.begin(); it != devices_.end(); ++it) {
                if (it->get() == classData) {
                    removed = *it;
                    devices_.erase(it);
                    break;
                }
            }
        }
        if (!removed) return NtStatus::InvalidParameter;
        {
            std::lock_guard<std::mutex> guard(removed->waitMutex);
            removed->unloaded = true;
            ++removed->statusSequence;
        }
        removed->waitCondition.notify_all();
        return NtStatus::Success;
    }

    static std::shared_ptr<BatteryClassDevice> resolve(void* classData) noexcept {
        if (!classData) return {};
        auto* device = static_cast<BatteryClassDevice*>(classData);
        // Class handles are opaque. Verify membership before using the pointer.
        auto& registry = Instance();
        std::lock_guard<std::mutex> guard(registry.lock_);
        for (const auto& item : registry.devices_) if (item.get() == device) return item;
        return {};
    }

    NtStatus statusNotify(void* classData) noexcept {
        auto device = resolve(classData);
        if (!device) return NtStatus::InvalidParameter;
        {
            std::lock_guard<std::mutex> guard(device->waitMutex);
            if (device->unloaded) return NtStatus::NoSuchDevice;
            ++device->statusSequence;
        }
        device->waitCondition.notify_all();
        return NtStatus::Success;
    }

    void* firstHandle() noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        return devices_.empty() ? nullptr : devices_.front().get();
    }

    void* handleForPdo(io::DeviceObject* pdo) noexcept {
        if (!pdo) return nullptr;
        std::lock_guard<std::mutex> guard(lock_);
        for (const auto& item : devices_)
            if (item->Miniport.Pdo == pdo) return item.get();
        return nullptr;
    }

private:
    std::mutex lock_;
    std::vector<std::shared_ptr<BatteryClassDevice>> devices_;
};

} // namespace micant::po::bms

extern "C" inline micant::NtStatus BatteryClassInitializeDevice(
    micant::po::bms::PBATTERY_MINIPORT_INFO miniportInfo, void** classData) {
    return micant::po::bms::BatteryClassRegistry::Instance().initializeDevice(miniportInfo, classData);
}

extern "C" inline micant::NtStatus BatteryClassUnload(void* classData) {
    return micant::po::bms::BatteryClassRegistry::Instance().unloadDevice(classData);
}

extern "C" inline micant::NtStatus BatteryClassStatusNotify(void* classData) {
    return micant::po::bms::BatteryClassRegistry::Instance().statusNotify(classData);
}

extern "C" inline micant::NtStatus BatteryClassIoctl(void* classData, micant::io::Irp* irp) {
    using namespace micant;
    using namespace micant::po::bms;
    if (!irp) return NtStatus::InvalidParameter;
    if (!classData) {
        irp->ioStatus.status = NtStatus::DeviceNotReady;
        irp->ioStatus.information = 0;
        return NtStatus::DeviceNotReady;
    }
    auto state = BatteryClassRegistry::resolve(classData);
    auto* stack = irp->getCurrentStack();
    if (!state || !stack) return NtStatus::InvalidParameter;
    void* buffer = irp->systemBuffer ? irp->systemBuffer : irp->userBuffer;
    const auto inLength = stack->parameters.deviceIoControl.inputBufferLength;
    const auto outLength = stack->parameters.deviceIoControl.outputBufferLength;
    uint32_t bytesReturned = 0;
    NtStatus status = NtStatus::InvalidDeviceRequest;

    switch (stack->parameters.deviceIoControl.ioControlCode) {
    case IOCTL_BATTERY_QUERY_TAG: {
        if (!buffer || inLength < sizeof(uint32_t)) { status = NtStatus::InvalidParameter; break; }
        if (!buffer || outLength < sizeof(uint32_t)) { status = NtStatus::BufferTooSmall; break; }
        uint32_t timeout = 0;
        std::memcpy(&timeout, buffer, sizeof(timeout));
        const bool infiniteTimeout = timeout == UINT32_MAX;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
        uint32_t tag = 0;
        std::unique_lock<std::mutex> waitLock(state->waitMutex);
        for (;;) {
            const uint64_t observedSequence = state->statusSequence;
            const bool unloaded = state->unloaded;
            waitLock.unlock();
            if (unloaded) {
                status = NtStatus::NoSuchDevice;
                tag = UINT32_MAX;
                waitLock.lock();
                break;
            }
            status = state->Miniport.QueryTag(state->Miniport.Context, &tag);
            if (!NT_SUCCESS(status) || tag != 0) { waitLock.lock(); break; }
            if (timeout == 0 || (!infiniteTimeout && std::chrono::steady_clock::now() >= deadline)) {
                tag = UINT32_MAX; // BATTERY_TAG_INVALID
                status = NtStatus::NoSuchDevice;
                waitLock.lock();
                break;
            }
            waitLock.lock();
            if (infiniteTimeout) {
                state->waitCondition.wait(waitLock, [&] {
                    return state->statusSequence != observedSequence || state->unloaded;
                });
            } else if (!state->waitCondition.wait_until(waitLock, deadline, [&] {
                    return state->statusSequence != observedSequence || state->unloaded;
                })) {
                tag = UINT32_MAX;
                status = NtStatus::NoSuchDevice;
                break;
            }
        }
        std::memcpy(buffer, &tag, sizeof(tag));
        bytesReturned = sizeof(tag);
        break;
    }
    case IOCTL_BATTERY_QUERY_INFORMATION: {
        if (!buffer || inLength < sizeof(BatteryQueryInformation)) { status = NtStatus::InvalidParameter; break; }
        BatteryQueryInformation query{};
        std::memcpy(&query, buffer, sizeof(query));
        status = state->Miniport.QueryInformation(state->Miniport.Context, query.batteryTag,
            static_cast<uint32_t>(query.infoLevel), static_cast<int32_t>(query.atRate),
            buffer, outLength, &bytesReturned);
        break;
    }
    case IOCTL_BATTERY_QUERY_STATUS: {
        if (!buffer || inLength < sizeof(BatteryWaitStatus)) { status = NtStatus::InvalidParameter; break; }
        BatteryWaitStatus wait{};
        std::memcpy(&wait, buffer, sizeof(wait));
        if (outLength < sizeof(BatteryStatus)) { status = NtStatus::BufferTooSmall; break; }
        BatteryStatus batteryStatus{};
        const bool infiniteTimeout = wait.timeout == UINT32_MAX;
        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(wait.timeout);
        std::unique_lock<std::mutex> waitLock(state->waitMutex);
        for (;;) {
            const uint64_t observedSequence = state->statusSequence;
            if (state->unloaded) { status = NtStatus::NoSuchDevice; break; }
            waitLock.unlock();
            status = state->Miniport.QueryStatus(state->Miniport.Context, wait.batteryTag, &batteryStatus);
            if (!NT_SUCCESS(status) || wait.timeout == 0) { waitLock.lock(); break; }
            const bool powerChanged = batteryStatus.powerState != wait.powerState;
            const bool capacityKnown = batteryStatus.capacity != UINT32_MAX;
            const bool belowLow = capacityKnown && wait.lowCapacity != 0 &&
                batteryStatus.capacity < wait.lowCapacity;
            const bool aboveHigh = capacityKnown && wait.highCapacity != 0 &&
                batteryStatus.capacity > wait.highCapacity;
            if (powerChanged || belowLow || aboveHigh) { waitLock.lock(); break; }
            waitLock.lock();
            if (infiniteTimeout) {
                state->waitCondition.wait(waitLock, [&] {
                    return state->statusSequence != observedSequence || state->unloaded;
                });
            } else if (!state->waitCondition.wait_until(waitLock, deadline, [&] {
                    return state->statusSequence != observedSequence || state->unloaded;
                })) {
                waitLock.unlock();
                status = state->Miniport.QueryStatus(state->Miniport.Context, wait.batteryTag, &batteryStatus);
                waitLock.lock();
                break;
            }
        }
        if (NT_SUCCESS(status)) {
            std::memcpy(buffer, &batteryStatus, sizeof(batteryStatus));
            bytesReturned = sizeof(batteryStatus);
        }
        break;
    }
    case IOCTL_BATTERY_SET_INFORMATION: {
        constexpr size_t headerSize = sizeof(uint32_t) + sizeof(BatterySetInfoLevel);
        if (!buffer || inLength < headerSize) { status = NtStatus::InvalidParameter; break; }
        uint32_t tag = 0;
        BatterySetInfoLevel level{};
        std::memcpy(&tag, buffer, sizeof(tag));
        std::memcpy(&level, static_cast<uint8_t*>(buffer) + sizeof(tag), sizeof(level));
        status = state->Miniport.SetInformation(state->Miniport.Context, tag,
            static_cast<uint32_t>(level), static_cast<uint8_t*>(buffer) + headerSize);
        break;
    }
    default:
        break;
    }

    irp->ioStatus.status = status;
    irp->ioStatus.information = bytesReturned;
    return status;
}

extern "C" inline micant::NtStatus BatteryClassQueryWmiDataBlock(
    void* classData, micant::io::DeviceObject* deviceObject, micant::io::Irp* irp,
    uint32_t guidIndex, uint32_t* instanceLengthArray, uint32_t outBufferSize,
    uint8_t* buffer) {
    using namespace micant;
    using namespace micant::po::bms;
    if (!irp || !deviceObject) return NtStatus::InvalidParameter;
    auto state = BatteryClassRegistry::resolve(classData);
    if (!state || state->Miniport.Pdo != deviceObject) return NtStatus::InvalidParameter;
    if (guidIndex > BATTERY_WMI_STATIC_DATA_GUID_INDEX) {
        irp->ioStatus.status = NtStatus::WmiGuidNotFound;
        irp->ioStatus.information = 0;
        return NtStatus::WmiGuidNotFound;
    }

    uint32_t tag = 0;
    NtStatus status = state->Miniport.QueryTag(state->Miniport.Context, &tag);
    std::vector<uint8_t> data;
    const auto appendString = [&data](const uint16_t* text, size_t capacity) {
        size_t chars = 0;
        while (chars < capacity && text[chars] != 0) ++chars;
        const uint16_t byteLength = static_cast<uint16_t>(chars * sizeof(uint16_t));
        const auto* lengthBytes = reinterpret_cast<const uint8_t*>(&byteLength);
        data.insert(data.end(), lengthBytes, lengthBytes + sizeof(byteLength));
        const auto* stringBytes = reinterpret_cast<const uint8_t*>(text);
        data.insert(data.end(), stringBytes, stringBytes + byteLength);
    };
    const auto queryInformation = [&](BatteryQueryInfoLevel level, void* output, uint32_t size,
                                      uint32_t* returned) {
        return state->Miniport.QueryInformation(state->Miniport.Context, tag,
            static_cast<uint32_t>(level), 0, output, size, returned);
    };

    if (NT_SUCCESS(status)) {
        switch (guidIndex) {
        case BATTERY_WMI_STATUS_GUID_INDEX: {
            BatteryStatus battery{};
            status = state->Miniport.QueryStatus(state->Miniport.Context, tag, &battery);
            if (NT_SUCCESS(status)) {
                BATTERY_WMI_STATUS value{};
                value.Tag = tag;
                value.RemainingCapacity = battery.capacity;
                value.Voltage = battery.voltage;
                value.PowerOnline = (battery.powerState & BATTERY_POWER_ON_LINE) != 0;
                value.Charging = (battery.powerState & BATTERY_CHARGING) != 0;
                value.Discharging = (battery.powerState & BATTERY_DISCHARGING) != 0;
                value.Critical = (battery.powerState & BATTERY_CRITICAL) != 0;
                if (battery.rate != BATTERY_UNKNOWN_RATE) {
                    if (value.Charging && battery.rate > 0) value.ChargeRate = battery.rate;
                    if (value.Discharging && battery.rate < 0)
                        value.DischargeRate = static_cast<int32_t>(-
                            static_cast<int64_t>(battery.rate));
                }
                data.resize(sizeof(value));
                std::memcpy(data.data(), &value, sizeof(value));
            }
            break;
        }
        case BATTERY_WMI_RUNTIME_GUID_INDEX: {
            uint32_t seconds = UINT32_MAX;
            uint32_t returned = 0;
            status = queryInformation(BatteryQueryInfoLevel::BatteryEstimatedTime,
                &seconds, sizeof(seconds), &returned);
            if (NT_SUCCESS(status) && returned == sizeof(seconds)) {
                const BATTERY_WMI_RUNTIME value{tag, seconds};
                data.resize(sizeof(value));
                std::memcpy(data.data(), &value, sizeof(value));
            } else if (NT_SUCCESS(status)) status = NtStatus::InfoLengthMismatch;
            break;
        }
        case BATTERY_WMI_TEMPERATURE_GUID_INDEX: {
            uint32_t temperature = 0;
            uint32_t returned = 0;
            status = queryInformation(BatteryQueryInfoLevel::BatteryTemperature,
                &temperature, sizeof(temperature), &returned);
            if (NT_SUCCESS(status) && returned == sizeof(temperature)) {
                const BATTERY_WMI_TEMPERATURE value{tag, temperature};
                data.resize(sizeof(value));
                std::memcpy(data.data(), &value, sizeof(value));
            } else if (NT_SUCCESS(status)) status = NtStatus::InfoLengthMismatch;
            break;
        }
        case BATTERY_WMI_FULL_CHARGED_CAPACITY_GUID_INDEX: {
            BatteryInformation information{};
            uint32_t returned = 0;
            status = queryInformation(BatteryQueryInfoLevel::BatteryInformation,
                &information, sizeof(information), &returned);
            if (NT_SUCCESS(status) && returned == sizeof(information)) {
                const BATTERY_WMI_FULL_CHARGED_CAPACITY value{tag,
                    information.fullChargedCapacity};
                data.resize(sizeof(value));
                std::memcpy(data.data(), &value, sizeof(value));
            } else if (NT_SUCCESS(status)) status = NtStatus::InfoLengthMismatch;
            break;
        }
        case BATTERY_WMI_CYCLE_COUNT_GUID_INDEX: {
            BatteryInformation information{};
            uint32_t returned = 0;
            status = queryInformation(BatteryQueryInfoLevel::BatteryInformation,
                &information, sizeof(information), &returned);
            if (NT_SUCCESS(status) && returned == sizeof(information)) {
                const BATTERY_WMI_CYCLE_COUNT value{tag, information.cycleCount};
                data.resize(sizeof(value));
                std::memcpy(data.data(), &value, sizeof(value));
            } else if (NT_SUCCESS(status)) status = NtStatus::InfoLengthMismatch;
            break;
        }
        case BATTERY_WMI_STATIC_DATA_GUID_INDEX: {
            BatteryInformation information{};
            uint32_t returned = 0;
            status = queryInformation(BatteryQueryInfoLevel::BatteryInformation,
                &information, sizeof(information), &returned);
            if (!NT_SUCCESS(status)) break;
            if (returned != sizeof(information)) { status = NtStatus::InfoLengthMismatch; break; }

            std::array<BatteryReportingScale, 4> scales{};
            uint32_t scaleBytes = 0;
            const auto scaleStatus = queryInformation(BatteryQueryInfoLevel::BatteryGranularityInformation,
                scales.data(), static_cast<uint32_t>(sizeof(scales)), &scaleBytes);
            if (!NT_SUCCESS(scaleStatus) && scaleStatus != NtStatus::InvalidDeviceRequest) {
                status = scaleStatus;
                break;
            }
            scaleBytes = std::min<uint32_t>(scaleBytes, sizeof(scales));
            const size_t scaleCount = scaleBytes / sizeof(BatteryReportingScale);

            std::array<uint16_t, 128> manufacturer{};
            std::array<uint16_t, 128> deviceName{};
            std::array<uint16_t, 128> serial{};
            uint32_t ignored = 0;
            (void)queryInformation(BatteryQueryInfoLevel::BatteryManufactureName,
                manufacturer.data(), static_cast<uint32_t>(sizeof(manufacturer)), &ignored);
            (void)queryInformation(BatteryQueryInfoLevel::BatteryDeviceName,
                deviceName.data(), static_cast<uint32_t>(sizeof(deviceName)), &ignored);
            (void)queryInformation(BatteryQueryInfoLevel::BatterySerialNumber,
                serial.data(), static_cast<uint32_t>(sizeof(serial)), &ignored);

            data.resize(offsetof(BATTERY_WMI_STATIC_DATA, Strings));
            BATTERY_WMI_STATIC_DATA value{};
            value.Tag = tag;
            std::copy_n(scales.begin(), scaleCount, value.Granularity);
            value.Capabilities = information.capabilities;
            value.Technology = information.technology;
            value.Chemistry = static_cast<uint32_t>(information.chemistry[0]) |
                (static_cast<uint32_t>(information.chemistry[1]) << 8) |
                (static_cast<uint32_t>(information.chemistry[2]) << 16) |
                (static_cast<uint32_t>(information.chemistry[3]) << 24);
            value.DesignedCapacity = information.designedCapacity;
            value.DefaultAlert1 = information.defaultAlert1;
            value.DefaultAlert2 = information.defaultAlert2;
            value.CriticalBias = information.criticalBias;
            std::memcpy(data.data(), &value, data.size());
            appendString(manufacturer.data(), manufacturer.size());
            appendString(deviceName.data(), deviceName.size());
            appendString(serial.data(), serial.size());
            // The ACPI battery interface does not provide manufacture date,
            // so it cannot form the documented composite unique ID.
            std::array<uint16_t, 1> uniqueId{};
            appendString(uniqueId.data(), uniqueId.size());
            break;
        }
        default:
            status = NtStatus::WmiGuidNotFound;
            break;
        }
    }

    const uint32_t required = static_cast<uint32_t>(std::min<size_t>(data.size(), UINT32_MAX));
    if (instanceLengthArray) instanceLengthArray[0] = required;
    if (NT_SUCCESS(status) && (!buffer || outBufferSize < required)) status = NtStatus::BufferTooSmall;
    if (NT_SUCCESS(status) && required) std::memcpy(buffer, data.data(), required);
    irp->ioStatus.status = status;
    irp->ioStatus.information = NT_SUCCESS(status) ? required : 0;
    return status;
}
