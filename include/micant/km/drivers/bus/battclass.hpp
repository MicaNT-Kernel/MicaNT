#pragma once

// Public battery class/miniport contracts from batclass.h. Keep field order,
// fixed-width members, callback signatures and version constants ABI-exact.
#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
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
