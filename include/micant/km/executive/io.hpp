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

// Canonical Windows NT IRP Major Function Codes (0x00 - 0x1B)
inline constexpr uint8_t IRP_MJ_CREATE                   = 0x00;
inline constexpr uint8_t IRP_MJ_CREATE_NAMED_PIPE        = 0x01;
inline constexpr uint8_t IRP_MJ_CLOSE                    = 0x02;
inline constexpr uint8_t IRP_MJ_READ                     = 0x03;
inline constexpr uint8_t IRP_MJ_WRITE                    = 0x04;
inline constexpr uint8_t IRP_MJ_QUERY_INFORMATION        = 0x05;
inline constexpr uint8_t IRP_MJ_QUERY_INFO               = 0x05;
inline constexpr uint8_t IRP_MJ_SET_INFORMATION          = 0x06;
inline constexpr uint8_t IRP_MJ_SET_INFO                 = 0x06;
inline constexpr uint8_t IRP_MJ_QUERY_EA                 = 0x07;
inline constexpr uint8_t IRP_MJ_SET_EA                   = 0x08;
inline constexpr uint8_t IRP_MJ_FLUSH_BUFFERS            = 0x09;
inline constexpr uint8_t IRP_MJ_QUERY_VOLUME_INFORMATION = 0x0A;
inline constexpr uint8_t IRP_MJ_SET_VOLUME_INFORMATION   = 0x0B;
inline constexpr uint8_t IRP_MJ_DIRECTORY_CONTROL        = 0x0C;
inline constexpr uint8_t IRP_MJ_FILE_SYSTEM_CONTROL      = 0x0D;
inline constexpr uint8_t IRP_MJ_DEVICE_CONTROL           = 0x0E;
inline constexpr uint8_t IRP_MJ_INTERNAL_DEVICE_CONTROL  = 0x0F;
inline constexpr uint8_t IRP_MJ_SHUTDOWN                 = 0x10;
inline constexpr uint8_t IRP_MJ_LOCK_CONTROL             = 0x11;
inline constexpr uint8_t IRP_MJ_CLEANUP                  = 0x12;
inline constexpr uint8_t IRP_MJ_CREATE_MAILSLOT          = 0x13;
inline constexpr uint8_t IRP_MJ_QUERY_SECURITY           = 0x14;
inline constexpr uint8_t IRP_MJ_SET_SECURITY             = 0x15;
inline constexpr uint8_t IRP_MJ_POWER                    = 0x16;
inline constexpr uint8_t IRP_MJ_SYSTEM_CONTROL           = 0x17;
inline constexpr uint8_t IRP_MJ_DEVICE_CHANGE            = 0x18;
inline constexpr uint8_t IRP_MJ_QUERY_QUOTA              = 0x19;
inline constexpr uint8_t IRP_MJ_SET_QUOTA                = 0x1A;
inline constexpr uint8_t IRP_MJ_PNP                      = 0x1B;
inline constexpr uint8_t IRP_MJ_MAXIMUM_FUNCTION         = 0x1B;

// File System Control Minor Function Codes
inline constexpr uint8_t IRP_MN_USER_FS_REQUEST          = 0x00;
inline constexpr uint8_t IRP_MN_MOUNT_VOLUME             = 0x01;
inline constexpr uint8_t IRP_MN_VERIFY_VOLUME            = 0x02;
inline constexpr uint8_t IRP_MN_LOAD_FILE_SYSTEM         = 0x03;

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
struct Irp;
struct Vpb;

/**
 * @brief Canonical Windows NT Volume Parameter Block (VPB).
 * Connects a physical/partition device object to the mounted volume device object.
 */
struct Vpb {
    int16_t type{0};
    int16_t size{sizeof(Vpb)};
    uint16_t flags{0};
    uint16_t volumeLabelLength{0};
    DeviceObject* deviceObject{nullptr};      // Target volume device object created by FSD
    DeviceObject* realDevice{nullptr};        // Physical disk/partition device object
    uint32_t serialNumber{0};
    uint32_t referenceCount{0};
    wchar_t volumeLabel[32]{0};
};

inline constexpr uint16_t VPB_MOUNTED    = 0x0001;
inline constexpr uint16_t VPB_LOCKED     = 0x0002;
inline constexpr uint16_t VPB_PERSISTENT = 0x0004;

// I/O Stack Location completion flags
inline constexpr uint8_t SL_PENDING_RETURNED   = 0x01;
inline constexpr uint8_t SL_INVOKE_ON_CANCEL   = 0x20;
inline constexpr uint8_t SL_INVOKE_ON_SUCCESS  = 0x40;
inline constexpr uint8_t SL_INVOKE_ON_ERROR    = 0x80;

/**
 * @brief Canonical Windows NT IO_STACK_LOCATION.
 * Represents driver-specific parameters for a given tier in the device stack.
 */
struct IoStackLocation {
    uint8_t majorFunction{IRP_MJ_CREATE};
    uint8_t minorFunction{0};
    uint8_t flags{0};
    uint8_t control{0};

    union {
        struct {
            uint32_t outputBufferLength;
            uint32_t inputBufferLength;
            uint32_t ioControlCode;
            void* type3InputBuffer;
        } deviceIoControl;

        struct {
            uint32_t outputBufferLength;
            uint32_t inputBufferLength;
            uint32_t fsControlCode;
            void* type3InputBuffer;
        } fileSystemControl;

        struct {
            Vpb* vpb;
            DeviceObject* deviceObject;
        } mountVolume;

        struct {
            uint32_t length;
            uint32_t key;
            LargeInteger byteOffset;
        } read;

        struct {
            uint32_t length;
            uint32_t key;
            LargeInteger byteOffset;
        } write;
    } parameters{};

    DeviceObject* deviceObject{nullptr};
    void* fileObject{nullptr};
    NtStatus (*completionRoutine)(DeviceObject* device, Irp* irp, void* context){nullptr};
    void* context{nullptr};
};

inline constexpr int8_t IRP_MAX_STACK_LOCATIONS = 4;

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

    // Layered Device Stack Tracking
    int8_t stackCount{IRP_MAX_STACK_LOCATIONS};
    int8_t currentStackLocation{1};
    IoStackLocation stackLocations[IRP_MAX_STACK_LOCATIONS]{};

    [[nodiscard]] IoStackLocation* getCurrentStack() noexcept {
        int idx = currentStackLocation - 1;
        if (idx >= 0 && idx < IRP_MAX_STACK_LOCATIONS) {
            return &stackLocations[idx];
        }
        return &stackLocations[0];
    }
};

using DriverDispatchRoutine = NtStatus (*)(DeviceObject* device, Irp* irp);

/**
 * @brief Canonical Windows NT FAST_IO_DISPATCH table.
 * Allows file system and cache operations to bypass IRP allocation for high throughput.
 */
struct FastIoDispatch {
    size_t sizeOfFastIoDispatch{sizeof(FastIoDispatch)};
    bool (*fastIoRead)(void* fileObject, LargeInteger* offset, uint32_t length, bool wait, void* buffer, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*fastIoWrite)(void* fileObject, LargeInteger* offset, uint32_t length, bool wait, const void* buffer, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*fastIoDeviceControl)(void* fileObject, bool wait, void* inBuf, uint32_t inLen, void* outBuf, uint32_t outLen, uint32_t ioctl, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
};

/**
 * @brief Driver Object representing loaded kernel device driver logic.
 */
struct DriverObject {
    std::wstring driverName;
    DriverDispatchRoutine majorFunction[32]{nullptr};
    FastIoDispatch* fastIoDispatch{nullptr};

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
    Vpb* vpb{nullptr};
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
 * @brief Canonical Windows NT WDM: Obtains a pointer to the caller's stack location in the specified IRP.
 */
inline IoStackLocation* IoGetCurrentIrpStackLocation(Irp* irp) noexcept {
    if (!irp) return nullptr;
    return irp->getCurrentStack();
}

/**
 * @brief Canonical Windows NT WDM: Obtains a pointer to the next-lower-level driver's stack location in an IRP.
 */
inline IoStackLocation* IoGetNextIrpStackLocation(Irp* irp) noexcept {
    if (!irp || irp->currentStackLocation <= 1) return nullptr;
    return &irp->stackLocations[irp->currentStackLocation - 2];
}

/**
 * @brief Canonical Windows NT WDM: Advances current stack location pointer down the stack.
 */
inline void IoSetNextIrpStackLocation(Irp* irp) noexcept {
    if (irp && irp->currentStackLocation > 1) {
        irp->currentStackLocation--;
    }
}

/**
 * @brief Canonical Windows NT WDM: Resets current stack location so driver reuses the current location.
 */
inline void IoSkipCurrentIrpStackLocation(Irp* irp) noexcept {
    if (irp && irp->currentStackLocation < irp->stackCount) {
        irp->currentStackLocation++;
    }
}

/**
 * @brief Canonical Windows NT WDM: Copies the IRP stack parameters from the current location to the next location.
 */
inline void IoCopyCurrentIrpStackLocationToNext(Irp* irp) noexcept {
    if (!irp) return;
    auto* current = IoGetCurrentIrpStackLocation(irp);
    auto* next = IoGetNextIrpStackLocation(irp);
    if (current && next) {
        *next = *current;
    }
}

/**
 * @brief Canonical Windows NT WDM: Sets a completion routine to be called when the next-lower-level driver completes request.
 */
inline void IoSetCompletionRoutine(
    Irp* irp,
    NtStatus (*routine)(DeviceObject*, Irp*, void*),
    void* context,
    bool invokeOnSuccess,
    bool invokeOnError,
    bool invokeOnCancel
) noexcept {
    auto* next = IoGetNextIrpStackLocation(irp);
    if (!next) return;
    next->completionRoutine = routine;
    next->context = context;
    next->control = 0;
    if (invokeOnSuccess) next->control |= SL_INVOKE_ON_SUCCESS;
    if (invokeOnError)   next->control |= SL_INVOKE_ON_ERROR;
    if (invokeOnCancel)  next->control |= SL_INVOKE_ON_CANCEL;
}

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

    // Populate current stack location for target driver dispatch
    auto* stack = IoGetCurrentIrpStackLocation(irp);
    if (stack) {
        stack->deviceObject = target;
        stack->majorFunction = irp->majorFunction;
        stack->minorFunction = irp->minorFunction;
        if (irp->majorFunction == IRP_MJ_READ) {
            stack->parameters.read.length = irp->length;
            stack->parameters.read.byteOffset = irp->byteOffset;
        } else if (irp->majorFunction == IRP_MJ_WRITE) {
            stack->parameters.write.length = irp->length;
            stack->parameters.write.byteOffset = irp->byteOffset;
        } else if (irp->majorFunction == IRP_MJ_DEVICE_CONTROL) {
            stack->parameters.deviceIoControl.ioControlCode = irp->byteOffset.lowPart;
            stack->parameters.deviceIoControl.inputBufferLength = static_cast<uint32_t>(irp->byteOffset.highPart);
            stack->parameters.deviceIoControl.outputBufferLength = irp->length;
        }
    }
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
 * Invokes completion routines upwards through the driver stack.
 */
inline NtStatus IoCompleteRequest(Irp* irp, int8_t priorityBoost = 0) noexcept {
    (void)priorityBoost;
    if (!irp) return NtStatus::InvalidParameter;

    while (irp->currentStackLocation <= irp->stackCount) {
        auto* stack = IoGetCurrentIrpStackLocation(irp);
        if (stack && stack->completionRoutine) {
            bool success = NT_SUCCESS(irp->ioStatus.status);
            bool isCancel = (irp->ioStatus.status == NtStatus::Cancelled);
            bool invoke = (success && (stack->control & SL_INVOKE_ON_SUCCESS)) ||
                          (!success && !isCancel && (stack->control & SL_INVOKE_ON_ERROR)) ||
                          (isCancel && (stack->control & SL_INVOKE_ON_CANCEL));
            if (invoke) {
                NtStatus st = stack->completionRoutine(stack->deviceObject, irp, stack->context);
                if (st == NtStatus::MoreProcessingRequired) {
                    return NtStatus::MoreProcessingRequired;
                }
            }
        }
        if (irp->currentStackLocation >= irp->stackCount) break;
        irp->currentStackLocation++;
    }

    return irp->ioStatus.status;
}

/**
 * @brief Canonical Windows NT WDM: Allocates an IRP from the executive memory pool.
 */
inline Irp* IoAllocateIrp(int8_t stackSize = IRP_MAX_STACK_LOCATIONS, bool chargeQuota = false) {
    (void)chargeQuota;
    auto* irp = new (std::nothrow) Irp();
    if (irp) {
        irp->stackCount = (stackSize > 0 && stackSize <= IRP_MAX_STACK_LOCATIONS) ? stackSize : IRP_MAX_STACK_LOCATIONS;
        irp->currentStackLocation = irp->stackCount;
    }
    return irp;
}

/**
 * @brief Canonical Windows NT WDM: Frees a previously allocated IRP.
 */
inline void IoFreeIrp(Irp* irp) noexcept {
    delete irp;
}

} // namespace micant::io
