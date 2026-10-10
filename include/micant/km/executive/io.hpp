#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ob.hpp"

namespace micant::io {

// Major IRP Functions
inline constexpr uint8_t IRP_MJ_CREATE         = 0x00;
inline constexpr uint8_t IRP_MJ_CLOSE          = 0x02;
inline constexpr uint8_t IRP_MJ_READ           = 0x03;
inline constexpr uint8_t IRP_MJ_WRITE          = 0x04;
inline constexpr uint8_t IRP_MJ_QUERY_INFO     = 0x05;
inline constexpr uint8_t IRP_MJ_SET_INFO       = 0x06;
inline constexpr uint8_t IRP_MJ_DEVICE_CONTROL = 0x0E;
inline constexpr uint8_t IRP_MJ_CLEANUP        = 0x12;
inline constexpr uint8_t IRP_MJ_POWER          = 0x16;
inline constexpr uint8_t IRP_MJ_SYSTEM_CONTROL = 0x17;
inline constexpr uint8_t IRP_MJ_PNP            = 0x1B;

// Device Types
enum class DeviceType : uint32_t {
    Disk = 0x00000007,
    FileSystem = 0x00000009,
    Null = 0x00000015,
    Console = 0x00000050,
    Unknown = 0x00000022
};

struct DeviceObject;
struct DriverObject;

/**
 * @brief I/O Request Packet (IRP) representing an in-flight I/O operation.
 */
struct Irp {
    uint8_t majorFunction{IRP_MJ_CREATE};
    uint8_t minorFunction{0};
    uint32_t flags{0};
    IoStatusBlock ioStatus{};
    DeviceObject* deviceObject{nullptr};
    void* userBuffer{nullptr};
    void* systemBuffer{nullptr};
    uint32_t length{0};
    LargeInteger byteOffset{};
};

using DriverDispatchRoutine = NtStatus (*)(DeviceObject* device, Irp* irp);

/**
 * @brief Driver Object representing loaded kernel device driver logic.
 */
struct DriverObject {
    std::wstring driverName;
    DriverDispatchRoutine majorFunction[32]{nullptr};

    void setDispatch(uint8_t major, DriverDispatchRoutine fn) noexcept {
        if (major < 32) majorFunction[major] = fn;
    }

    [[nodiscard]] NtStatus dispatch(DeviceObject* dev, Irp* irp) {
        if (!irp || irp->majorFunction >= 32 || !majorFunction[irp->majorFunction]) {
            return NtStatus::InvalidDeviceRequest;
        }
        return majorFunction[irp->majorFunction](dev, irp);
    }
};

/**
 * @brief Device Object representing target hardware or virtual device node in \Device.
 */
struct DeviceObject {
    DriverObject* driverObject{nullptr};
    DeviceType deviceType{DeviceType::Unknown};
    std::wstring deviceName;
    uint32_t characteristics{0};
    uint32_t flags{0};
    DeviceObject* attachedDevice{nullptr};
    void* deviceExtension{nullptr};
};

/**
 * @brief Completion Packet queued to an I/O Completion Port.
 */
struct CompletionPacket {
    uint64_t completionKey{0};
    uintptr_t overlapped{0};
    IoStatusBlock ioStatus{};
    uint32_t bytesTransferred{0};
};

/**
 * @brief I/O Completion Port (IOCP).
 * Dave Cutler's high-concurrency async I/O worker queue.
 */
class IoCompletionPort {
public:
    explicit IoCompletionPort(uint32_t maxConcurrentThreads = 0)
        : maxConcurrentThreads_(maxConcurrentThreads) {}

    NtStatus postCompletion(uint64_t completionKey, uintptr_t overlapped, NtStatus status, uint32_t bytesTransferred) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(CompletionPacket{
            .completionKey = completionKey,
            .overlapped = overlapped,
            .ioStatus = { .status = status, .information = bytesTransferred },
            .bytesTransferred = bytesTransferred
        });
        cv_.notify_one();
        return NtStatus::Success;
    }

    NtStatus removeCompletion(
        uint64_t& outCompletionKey,
        uintptr_t& outOverlapped,
        IoStatusBlock& outIoStatus,
        uint32_t timeoutMs = 0xFFFFFFFF
    ) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            if (timeoutMs == 0) return NtStatus::Timeout;

            auto pred = [this]() { return !queue_.empty(); };
            if (timeoutMs == 0xFFFFFFFF) {
                cv_.wait(lock, pred);
            } else {
                if (!cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), pred)) {
                    return NtStatus::Timeout;
                }
            }
        }

        if (queue_.empty()) return NtStatus::Timeout;

        CompletionPacket packet = queue_.front();
        queue_.pop();

        outCompletionKey = packet.completionKey;
        outOverlapped = packet.overlapped;
        outIoStatus = packet.ioStatus;
        return packet.ioStatus.status;
    }

    [[nodiscard]] size_t getQueuedCount() const noexcept {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        return queue_.size();
    }

private:
    uint32_t maxConcurrentThreads_{0};
    std::queue<CompletionPacket> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

/**
 * @brief I/O Manager Central Subsystem.
 */
class IoManager {
public:
    static IoManager& get() {
        static IoManager instance;
        return instance;
    }

    std::shared_ptr<DeviceObject> createDevice(
        DriverObject* driver,
        std::wstring_view deviceName,
        DeviceType type,
        void* deviceExtension = nullptr
    ) {
        auto dev = std::make_shared<DeviceObject>();
        dev->driverObject = driver;
        dev->deviceName = std::wstring(deviceName);
        dev->deviceType = type;
        dev->deviceExtension = deviceExtension;
        devices_[dev->deviceName] = dev;
        return dev;
    }

    [[nodiscard]] std::shared_ptr<DeviceObject> lookupDevice(std::wstring_view name) const {
        auto it = devices_.find(std::wstring(name));
        if (it != devices_.end()) return it->second;
        return nullptr;
    }

    [[nodiscard]] size_t getDeviceCount() const noexcept { return devices_.size(); }

private:
    IoManager() = default;
    std::unordered_map<std::wstring, std::shared_ptr<DeviceObject>> devices_;
};

/**
 * @brief Canonical Windows NT WDM: Traverses the attachedDevice chain up to the top of the stack.
 */
inline DeviceObject* IoGetAttachedDevice(DeviceObject* device) noexcept {
    if (!device) return nullptr;
    DeviceObject* current = device;
    while (current->attachedDevice != nullptr) {
        current = current->attachedDevice;
    }
    return current;
}

/**
 * @brief Canonical Windows NT WDM: Forwards an IRP down a layered device stack or directly to target.
 */
inline NtStatus IoCallDriver(DeviceObject* device, Irp* irp) {
    if (!device || !irp) return NtStatus::InvalidParameter;
    DeviceObject* target = IoGetAttachedDevice(device);
    if (!target || !target->driverObject) return NtStatus::InvalidDeviceRequest;
    irp->deviceObject = target;
    return target->driverObject->dispatch(target, irp);
}

/**
 * @brief Canonical Windows NT WDM: Attaches a source device object to the top of target's stack.
 */
inline DeviceObject* IoAttachDeviceToDeviceStack(DeviceObject* sourceDevice, DeviceObject* targetDevice) noexcept {
    if (!sourceDevice || !targetDevice) return nullptr;
    DeviceObject* top = IoGetAttachedDevice(targetDevice);
    top->attachedDevice = sourceDevice;
    return top;
}

/**
 * @brief Canonical Windows NT WDM: Detaches a device object from its attached device stack.
 */
inline void IoDetachDevice(DeviceObject* targetDevice) noexcept {
    if (targetDevice) {
        targetDevice->attachedDevice = nullptr;
    }
}

/**
 * @brief Canonical Windows NT WDM: Signals completion of an in-flight I/O request packet.
 */
inline NtStatus IoCompleteRequest(Irp* irp, int8_t priorityBoost = 0) noexcept {
    (void)priorityBoost;
    if (!irp) return NtStatus::InvalidParameter;
    return irp->ioStatus.status;
}

} // namespace micant::io
