#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <cstring>
#include <atomic>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/executive/ob.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/km/executive/mm.hpp"

namespace micant::io {

using micant::IoStatusBlock;
using micant::LargeInteger;
using micant::NtStatus;

// ============================================================================
// 1. Canonical Windows NT IRP Major Function Codes (0x00 - 0x1B)
// ============================================================================
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

// Device Object Flags (Buffering Models)
inline constexpr uint32_t DO_BUFFERED_IO                 = 0x00000004;
inline constexpr uint32_t DO_DIRECT_IO                   = 0x00000010;
inline constexpr uint32_t DO_NEITHER_IO                  = 0x00000000;
inline constexpr uint32_t DO_DEVICE_INITIALIZING         = 0x00000080;
inline constexpr uint32_t DO_POWER_PAGABLE               = 0x00000020;

// Device Characteristics Flags
inline constexpr uint32_t FILE_REMOVABLE_MEDIA           = 0x00000001;
inline constexpr uint32_t FILE_READ_ONLY_DEVICE          = 0x00000002;
inline constexpr uint32_t FILE_FLOPPY_DISKETTE           = 0x00000004;
inline constexpr uint32_t FILE_WRITE_ONCE_MEDIA          = 0x00000008;
inline constexpr uint32_t FILE_REMOTE_DEVICE             = 0x00000010;
inline constexpr uint32_t FILE_DEVICE_IS_MOUNTED         = 0x00000020;
inline constexpr uint32_t FILE_VIRTUAL_VOLUME            = 0x00000040;
inline constexpr uint32_t FILE_DEVICE_SECURE_OPEN        = 0x00000100;

// Device Types
enum class DeviceType : uint32_t {
    CdRom           = 0x00000002,
    CdRomFileSystem = 0x00000003,
    Disk            = 0x00000007,
    FileSystem      = 0x00000009,
    Null            = 0x00000015,
    Console         = 0x00000050,
    Battery         = 0x00000029,
    Unknown         = 0x00000022
};

// ============================================================================
// 2. I/O Subsystem Diagnostic Telemetry
// ============================================================================
struct IO_TELEMETRY {
    size_t totalDevicesCreated{0};
    size_t totalDevicesDeleted{0};
    size_t totalIrpsAllocated{0};
    size_t totalIrpsFreed{0};
    size_t totalIrpsDispatched{0};
    size_t totalIrpsCompleted{0};
    size_t totalIrpsCancelled{0};
    size_t totalCancelRoutinesInvoked{0};
    size_t totalFastIoReads{0};
    size_t totalFastIoWrites{0};
    size_t totalFastIoDeviceControls{0};
    size_t totalFastIoSuccesses{0};
    size_t totalFastIoBypasses{0};
    size_t totalBufferedIoBytes{0};
    size_t totalDirectIoBytes{0};
    size_t totalNeitherIoBytes{0};
    size_t totalDeviceQueuePacketsStarted{0};
    size_t totalDeviceQueuePacketsQueued{0};
};

inline IO_TELEMETRY g_IoTelemetry{};
inline std::mutex g_IoTelemetryMutex{};

inline IO_TELEMETRY IoQueryTelemetry() {
    std::lock_guard<std::mutex> lock(g_IoTelemetryMutex);
    return g_IoTelemetry;
}

inline void IoResetTelemetryForTesting() {
    std::lock_guard<std::mutex> lock(g_IoTelemetryMutex);
    g_IoTelemetry = IO_TELEMETRY{};
}

struct DeviceObject;
struct DriverObject;
struct Irp;
struct Vpb;

// Cancel Routine Prototype
using PDRIVER_CANCEL = void (*)(DeviceObject* device, Irp* irp);

/**
 * @brief Canonical Windows NT Volume Parameter Block (VPB).
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

        struct {
            uint32_t type;
            void* argument;
        } pnp;
    } parameters{};

    DeviceObject* deviceObject{nullptr};
    void* fileObject{nullptr};
    NtStatus (*completionRoutine)(DeviceObject* device, Irp* irp, void* context){nullptr};
    void* context{nullptr};
};

inline constexpr int8_t IRP_MAX_STACK_LOCATIONS = 8;

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
    void* mdlAddress{nullptr};
    uint32_t length{0};
    LargeInteger byteOffset{};

    // Cancellation Subsystem
    bool cancel{false};
    ke::KIRQL cancelIrql{ke::PASSIVE_LEVEL};
    PDRIVER_CANCEL cancelRoutine{nullptr};

    // Layered Device Stack Tracking
    int8_t stackCount{IRP_MAX_STACK_LOCATIONS};
    int8_t currentStackLocation{1};
    IoStackLocation stackLocations[IRP_MAX_STACK_LOCATIONS]{};

    [[nodiscard]] IoStackLocation* getCurrentStack() noexcept {
        int idx = currentStackLocation - 1;
        if (idx >= 0 && idx < stackCount) {
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
    bool (*fastIoCheckIfPossible)(void* fileObject, LargeInteger* offset, uint32_t length, bool wait, uint32_t lockKey, bool checkForRead, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*fastIoRead)(void* fileObject, LargeInteger* offset, uint32_t length, bool wait, void* buffer, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*fastIoReadEx)(void* fileObject, LargeInteger* offset, uint32_t length, bool wait, uint32_t lockKey, void* buffer, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*fastIoWrite)(void* fileObject, LargeInteger* offset, uint32_t length, bool wait, const void* buffer, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*fastIoWriteEx)(void* fileObject, LargeInteger* offset, uint32_t length, bool wait, uint32_t lockKey, const void* buffer, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*fastIoDeviceControl)(void* fileObject, bool wait, void* inBuf, uint32_t inLen, void* outBuf, uint32_t outLen, uint32_t ioctl, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*fastIoQueryBasicInfo)(void* fileObject, bool wait, void* basicInfo, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*fastIoQueryStandardInfo)(void* fileObject, bool wait, void* standardInfo, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*fastIoLock)(void* fileObject, LargeInteger* offset, LargeInteger* length, void* processId, uint32_t key, bool failImmediately, bool exclusiveLock, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*fastIoUnlockSingle)(void* fileObject, LargeInteger* offset, LargeInteger* length, void* processId, uint32_t key, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    void (*fastIoDetachDevice)(DeviceObject* sourceDevice, DeviceObject* targetDevice){nullptr};
    bool (*fastIoQueryNetworkOpenInfo)(void* fileObject, bool wait, void* networkOpenInfo, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*mdlRead)(void* fileObject, LargeInteger* offset, uint32_t length, uint32_t lockKey, void** mdlChain, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*mdlReadComplete)(void* fileObject, void* mdlChain, DeviceObject* dev){nullptr};
    bool (*prepareMdlWrite)(void* fileObject, LargeInteger* offset, uint32_t length, uint32_t lockKey, void** mdlChain, IoStatusBlock* ioStatus, DeviceObject* dev){nullptr};
    bool (*mdlWriteComplete)(void* fileObject, LargeInteger* offset, void* mdlChain, DeviceObject* dev){nullptr};
};

/**
 * @brief Canonical Windows NT Device Queue (KDEVICE_QUEUE) for IRP serialization.
 */
struct KDeviceQueue {
    bool busy{false};
    std::vector<Irp*> queue;
    ke::SpinLock spinLock;
};

/**
 * @brief Driver Object representing loaded kernel device driver logic.
 */
struct DriverObject {
    std::wstring driverName;
    DriverDispatchRoutine majorFunction[32]{nullptr};
    FastIoDispatch* fastIoDispatch{nullptr};
    void (*driverStartIo)(DeviceObject* device, Irp* irp){nullptr};
    void (*driverUnload)(DriverObject* driverObject){nullptr};
    NtStatus (*driverAddDevice)(DriverObject* driverObject, DeviceObject* physicalDeviceObject){nullptr};

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
    int8_t stackSize{1};
    DeviceObject* attachedDevice{nullptr};
    void* deviceExtension{nullptr};
    Vpb* vpb{nullptr};
    KDeviceQueue deviceQueue{};
    Irp* currentIrp{nullptr};
};

// ============================================================================
// 3. IRP Cancellation Race Condition Prevention Engine (Cancel SpinLock)
// ============================================================================
inline ke::SpinLock g_IoCancelSpinLock;

inline void IoAcquireCancelSpinLock(ke::KIRQL* oldIrql) noexcept {
    if (oldIrql) {
        *oldIrql = g_IoCancelSpinLock.acquire();
    } else {
        g_IoCancelSpinLock.acquire();
    }
}

inline void IoReleaseCancelSpinLock(ke::KIRQL oldIrql) noexcept {
    g_IoCancelSpinLock.release(oldIrql);
}

inline PDRIVER_CANCEL IoSetCancelRoutine(Irp* irp, PDRIVER_CANCEL routine) noexcept {
    if (!irp) return nullptr;
    PDRIVER_CANCEL oldRoutine = irp->cancelRoutine;
    irp->cancelRoutine = routine;
    return oldRoutine;
}

inline bool IoCancelIrp(Irp* irp) noexcept {
    if (!irp) return false;
    ke::KIRQL oldIrql = ke::PASSIVE_LEVEL;
    IoAcquireCancelSpinLock(&oldIrql);

    irp->cancel = true;
    PDRIVER_CANCEL cancelRoutine = irp->cancelRoutine;
    irp->cancelRoutine = nullptr;

    {
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        g_IoTelemetry.totalIrpsCancelled++;
    }

    if (cancelRoutine) {
        irp->cancelIrql = oldIrql;
        {
            std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
            g_IoTelemetry.totalCancelRoutinesInvoked++;
        }
        cancelRoutine(irp->deviceObject, irp);
        if (g_IoCancelSpinLock.isLocked()) {
            IoReleaseCancelSpinLock(oldIrql);
        }
        return true;
    }

    IoReleaseCancelSpinLock(oldIrql);
    return false;
}

// ============================================================================
// 4. I/O Completion Port (IOCP) Subsystem
// ============================================================================
struct CompletionPacket {
    uint64_t completionKey{0};
    uintptr_t overlapped{0};
    IoStatusBlock ioStatus{};
    uint32_t bytesTransferred{0};
};

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

    [[nodiscard]] uint32_t getMaxConcurrentThreads() const noexcept {
        return maxConcurrentThreads_;
    }

private:
    uint32_t maxConcurrentThreads_{0};
    std::queue<CompletionPacket> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

// ============================================================================
// 5. I/O Manager Central Engine
// ============================================================================
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
        dev->stackSize = 1;
        dev->flags = DO_DEVICE_INITIALIZING;

        devices_[dev->deviceName] = dev;

        {
            std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
            g_IoTelemetry.totalDevicesCreated++;
        }

        return dev;
    }

    NtStatus createDeviceEx(
        DriverObject* driver,
        size_t extensionSize,
        std::wstring_view deviceName,
        DeviceType type,
        uint32_t characteristics,
        bool exclusive,
        std::shared_ptr<DeviceObject>& outDev
    ) {
        void* ext = nullptr;
        if (extensionSize > 0) {
            ext = std::malloc(extensionSize);
            if (!ext) return NtStatus::NoMemory;
            std::memset(ext, 0, extensionSize);
        }

        outDev = createDevice(driver, deviceName, type, ext);
        if (!outDev) {
            if (ext) std::free(ext);
            return NtStatus::InsufficientResources;
        }

        outDev->characteristics = characteristics;
        if (exclusive) {
            outDev->characteristics |= 0x00000200; // Exclusive attribute
        }
        return NtStatus::Success;
    }

    void deleteDevice(DeviceObject* device) {
        if (!device) return;
        if (device->deviceExtension) {
            std::free(device->deviceExtension);
            device->deviceExtension = nullptr;
        }
        devices_.erase(device->deviceName);

        {
            std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
            g_IoTelemetry.totalDevicesDeleted++;
        }
    }

    [[nodiscard]] std::shared_ptr<DeviceObject> lookupDevice(std::wstring_view name) const {
        auto it = devices_.find(std::wstring(name));
        if (it != devices_.end()) return it->second;
        return nullptr;
    }

    [[nodiscard]] size_t getDeviceCount() const noexcept { return devices_.size(); }

    void resetForTesting() {
        devices_.clear();
    }

private:
    IoManager() = default;
    std::unordered_map<std::wstring, std::shared_ptr<DeviceObject>> devices_;
};

// ============================================================================
// 6. Canonical Windows NT WDM I/O Routines & Stack Management
// ============================================================================
inline IoStackLocation* IoGetCurrentIrpStackLocation(Irp* irp) noexcept {
    if (!irp) return nullptr;
    return irp->getCurrentStack();
}

inline IoStackLocation* IoGetNextIrpStackLocation(Irp* irp) noexcept {
    if (!irp || irp->currentStackLocation <= 1) return nullptr;
    return &irp->stackLocations[irp->currentStackLocation - 2];
}

inline void IoSetNextIrpStackLocation(Irp* irp) noexcept {
    if (irp && irp->currentStackLocation > 1) {
        irp->currentStackLocation--;
    }
}

inline void IoSkipCurrentIrpStackLocation(Irp* irp) noexcept {
    if (irp && irp->currentStackLocation < irp->stackCount) {
        irp->currentStackLocation++;
    }
}

inline void IoCopyCurrentIrpStackLocationToNext(Irp* irp) noexcept {
    if (!irp) return;
    auto* current = IoGetCurrentIrpStackLocation(irp);
    auto* next = IoGetNextIrpStackLocation(irp);
    if (current && next) {
        *next = *current;
    }
}

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

inline DeviceObject* IoGetAttachedDevice(DeviceObject* device) noexcept {
    if (!device) return nullptr;
    DeviceObject* current = device;
    while (current->attachedDevice != nullptr) {
        current = current->attachedDevice;
    }
    return current;
}

inline Irp* IoAllocateIrp(int8_t stackSize = IRP_MAX_STACK_LOCATIONS, bool chargeQuota = false) {
    (void)chargeQuota;
    auto* irp = new (std::nothrow) Irp();
    if (irp) {
        irp->stackCount = (stackSize > 0 && stackSize <= IRP_MAX_STACK_LOCATIONS) ? stackSize : IRP_MAX_STACK_LOCATIONS;
        irp->currentStackLocation = irp->stackCount;
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        g_IoTelemetry.totalIrpsAllocated++;
    }
    return irp;
}

inline void IoFreeIrp(Irp* irp) noexcept {
    if (!irp) return;
    delete irp;
    std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
    g_IoTelemetry.totalIrpsFreed++;
}

/**
 * @brief Completes an I/O request packet, executing layered stack completion routines,
 * copying buffered I/O data back to user buffers, and unlocking/freeing direct I/O MDLs.
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

    // Handle Buffering Teardown
    if (irp->deviceObject) {
        if (irp->deviceObject->flags & DO_BUFFERED_IO) {
            if ((irp->majorFunction == IRP_MJ_READ ||
                 irp->majorFunction == IRP_MJ_DEVICE_CONTROL ||
                 irp->majorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
                irp->systemBuffer && irp->userBuffer) {
                size_t copyBytes = irp->ioStatus.information > 0 ? irp->ioStatus.information : irp->length;
                std::memcpy(irp->userBuffer, irp->systemBuffer, copyBytes);
            }
            if (irp->systemBuffer) {
                std::free(irp->systemBuffer);
                irp->systemBuffer = nullptr;
            }
            std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
            g_IoTelemetry.totalBufferedIoBytes += irp->ioStatus.information;
        } else if (irp->deviceObject->flags & DO_DIRECT_IO) {
            if (irp->mdlAddress) {
                auto* mdl = reinterpret_cast<mm::PMDL>(irp->mdlAddress);
                mm::MmUnlockPages(mdl);
                mm::IoFreeMdl(mdl);
                irp->mdlAddress = nullptr;
            }
            std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
            g_IoTelemetry.totalDirectIoBytes += irp->ioStatus.information;
        } else {
            std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
            g_IoTelemetry.totalNeitherIoBytes += irp->ioStatus.information;
        }
    }

    {
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        g_IoTelemetry.totalIrpsCompleted++;
    }

    return irp->ioStatus.status;
}

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

    {
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        g_IoTelemetry.totalIrpsDispatched++;
    }

    return target->driverObject->dispatch(target, irp);
}

inline DeviceObject* IoAttachDeviceToDeviceStack(DeviceObject* sourceDevice, DeviceObject* targetDevice) noexcept {
    if (!sourceDevice || !targetDevice) return nullptr;
    DeviceObject* top = IoGetAttachedDevice(targetDevice);
    top->attachedDevice = sourceDevice;
    sourceDevice->stackSize = static_cast<int8_t>(top->stackSize + 1);
    return top;
}

inline void IoDetachDevice(DeviceObject* targetDevice) noexcept {
    if (targetDevice) {
        targetDevice->attachedDevice = nullptr;
    }
}

// ============================================================================
// 7. Canonical Windows NT Request Building Helpers (Synchronous & Asynchronous)
// ============================================================================
inline Irp* IoBuildSynchronousFsdRequest(
    uint8_t majorFunction,
    DeviceObject* deviceObject,
    void* buffer,
    uint32_t length,
    LargeInteger* startingOffset,
    void* /*event*/ = nullptr,
    IoStatusBlock* /*ioStatusBlock*/ = nullptr
) {
    if (!deviceObject) return nullptr;
    int8_t stackCount = deviceObject->stackSize > 0 ? deviceObject->stackSize : 1;
    auto* irp = IoAllocateIrp(stackCount);
    if (!irp) return nullptr;

    irp->majorFunction = majorFunction;
    irp->length = length;
    if (startingOffset) {
        irp->byteOffset = *startingOffset;
    }
    irp->deviceObject = deviceObject;

    if (deviceObject->flags & DO_BUFFERED_IO) {
        void* sysBuf = nullptr;
        if (length > 0) {
            sysBuf = std::malloc(length);
            if (!sysBuf) {
                IoFreeIrp(irp);
                return nullptr;
            }
            if (majorFunction == IRP_MJ_WRITE && buffer) {
                std::memcpy(sysBuf, buffer, length);
            } else {
                std::memset(sysBuf, 0, length);
            }
        }
        irp->systemBuffer = sysBuf;
        irp->userBuffer = buffer;
    } else if (deviceObject->flags & DO_DIRECT_IO) {
        if (buffer && length > 0) {
            auto* mdl = mm::IoAllocateMdl(buffer, length, false, false, irp);
            if (!mdl) {
                IoFreeIrp(irp);
                return nullptr;
            }
            auto lockOp = (majorFunction == IRP_MJ_READ) ? mm::LOCK_OPERATION::IoWriteAccess : mm::LOCK_OPERATION::IoReadAccess;
            mm::MmProbeAndLockPages(mdl, mm::KernelMode, lockOp);
            irp->mdlAddress = mdl;
            irp->userBuffer = buffer;
        }
    } else {
        // Neither I/O
        irp->userBuffer = buffer;
    }

    auto* stack = IoGetCurrentIrpStackLocation(irp);
    if (stack) {
        stack->deviceObject = deviceObject;
        stack->majorFunction = majorFunction;
        if (majorFunction == IRP_MJ_READ) {
            stack->parameters.read.length = length;
            if (startingOffset) stack->parameters.read.byteOffset = *startingOffset;
        } else if (majorFunction == IRP_MJ_WRITE) {
            stack->parameters.write.length = length;
            if (startingOffset) stack->parameters.write.byteOffset = *startingOffset;
        }
    }

    return irp;
}

inline Irp* IoBuildDeviceIoControlRequest(
    uint32_t ioControlCode,
    DeviceObject* deviceObject,
    void* inputBuffer,
    uint32_t inputBufferLength,
    void* outputBuffer,
    uint32_t outputBufferLength,
    bool internalDeviceIoControl,
    void* /*event*/ = nullptr,
    IoStatusBlock* /*ioStatusBlock*/ = nullptr
) {
    if (!deviceObject) return nullptr;
    int8_t stackCount = deviceObject->stackSize > 0 ? deviceObject->stackSize : 1;
    auto* irp = IoAllocateIrp(stackCount);
    if (!irp) return nullptr;

    irp->majorFunction = internalDeviceIoControl ? IRP_MJ_INTERNAL_DEVICE_CONTROL : IRP_MJ_DEVICE_CONTROL;
    irp->byteOffset.lowPart = ioControlCode;
    irp->byteOffset.highPart = static_cast<int32_t>(inputBufferLength);
    irp->length = outputBufferLength;
    irp->deviceObject = deviceObject;

    if (deviceObject->flags & DO_BUFFERED_IO) {
        size_t maxLen = std::max(inputBufferLength, outputBufferLength);
        void* sysBuf = nullptr;
        if (maxLen > 0) {
            sysBuf = std::malloc(maxLen);
            if (!sysBuf) {
                IoFreeIrp(irp);
                return nullptr;
            }
            if (inputBuffer && inputBufferLength > 0) {
                std::memcpy(sysBuf, inputBuffer, inputBufferLength);
            }
        }
        irp->systemBuffer = sysBuf;
        irp->userBuffer = outputBuffer;
    } else {
        irp->userBuffer = outputBuffer;
    }

    auto* stack = IoGetCurrentIrpStackLocation(irp);
    if (stack) {
        stack->deviceObject = deviceObject;
        stack->majorFunction = irp->majorFunction;
        stack->parameters.deviceIoControl.ioControlCode = ioControlCode;
        stack->parameters.deviceIoControl.inputBufferLength = inputBufferLength;
        stack->parameters.deviceIoControl.outputBufferLength = outputBufferLength;
    }

    return irp;
}

// ============================================================================
// 8. Device Queue Serialization (IoStartPacket & IoStartNextPacket)
// ============================================================================
inline void IoStartPacket(
    DeviceObject* device,
    Irp* irp,
    uint32_t* /*key*/ = nullptr,
    PDRIVER_CANCEL cancelFunction = nullptr
) {
    if (!device || !irp) return;
    ke::KIRQL oldIrql = ke::PASSIVE_LEVEL;
    IoAcquireCancelSpinLock(&oldIrql);

    if (cancelFunction) {
        irp->cancelRoutine = cancelFunction;
    }

    if (!device->deviceQueue.busy) {
        device->deviceQueue.busy = true;
        device->currentIrp = irp;
        IoReleaseCancelSpinLock(oldIrql);

        {
            std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
            g_IoTelemetry.totalDeviceQueuePacketsStarted++;
        }

        if (device->driverObject && device->driverObject->driverStartIo) {
            device->driverObject->driverStartIo(device, irp);
        }
    } else {
        device->deviceQueue.queue.push_back(irp);
        IoReleaseCancelSpinLock(oldIrql);

        {
            std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
            g_IoTelemetry.totalDeviceQueuePacketsQueued++;
        }
    }
}

inline void IoStartNextPacket(DeviceObject* device, bool cancelable = false) {
    if (!device) return;
    ke::KIRQL oldIrql = ke::PASSIVE_LEVEL;
    IoAcquireCancelSpinLock(&oldIrql);

    if (device->deviceQueue.queue.empty()) {
        device->deviceQueue.busy = false;
        device->currentIrp = nullptr;
        IoReleaseCancelSpinLock(oldIrql);
        return;
    }

    Irp* nextIrp = device->deviceQueue.queue.front();
    device->deviceQueue.queue.erase(device->deviceQueue.queue.begin());
    device->currentIrp = nextIrp;

    if (cancelable) {
        nextIrp->cancelRoutine = nullptr;
    }

    IoReleaseCancelSpinLock(oldIrql);

    {
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        g_IoTelemetry.totalDeviceQueuePacketsStarted++;
    }

    if (device->driverObject && device->driverObject->driverStartIo) {
        device->driverObject->driverStartIo(device, nextIrp);
    }
}

// ============================================================================
// 9. Fast I/O Dispatch Invocations & Bypass Mechanics
// ============================================================================
inline bool IoFastRead(
    DeviceObject* device,
    void* fileObject,
    LargeInteger* offset,
    uint32_t length,
    bool wait,
    void* buffer,
    IoStatusBlock* ioStatus
) {
    if (!device || !device->driverObject || !device->driverObject->fastIoDispatch) {
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        g_IoTelemetry.totalFastIoBypasses++;
        return false;
    }
    auto* fastIo = device->driverObject->fastIoDispatch;
    {
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        g_IoTelemetry.totalFastIoReads++;
    }
    if (fastIo->fastIoReadEx) {
        bool res = fastIo->fastIoReadEx(fileObject, offset, length, wait, 0, buffer, ioStatus, device);
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        if (res) g_IoTelemetry.totalFastIoSuccesses++; else g_IoTelemetry.totalFastIoBypasses++;
        return res;
    }
    if (fastIo->fastIoRead) {
        bool res = fastIo->fastIoRead(fileObject, offset, length, wait, buffer, ioStatus, device);
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        if (res) g_IoTelemetry.totalFastIoSuccesses++; else g_IoTelemetry.totalFastIoBypasses++;
        return res;
    }
    std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
    g_IoTelemetry.totalFastIoBypasses++;
    return false;
}

inline bool IoFastRead(
    DeviceObject* device,
    void* fileObject,
    LargeInteger* offset,
    uint32_t length,
    bool wait,
    uint32_t lockKey,
    void* buffer,
    IoStatusBlock* ioStatus
) {
    if (!device || !device->driverObject || !device->driverObject->fastIoDispatch) {
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        g_IoTelemetry.totalFastIoBypasses++;
        return false;
    }
    auto* fastIo = device->driverObject->fastIoDispatch;
    {
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        g_IoTelemetry.totalFastIoReads++;
    }
    if (fastIo->fastIoReadEx) {
        bool res = fastIo->fastIoReadEx(fileObject, offset, length, wait, lockKey, buffer, ioStatus, device);
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        if (res) g_IoTelemetry.totalFastIoSuccesses++; else g_IoTelemetry.totalFastIoBypasses++;
        return res;
    }
    if (fastIo->fastIoRead) {
        bool res = fastIo->fastIoRead(fileObject, offset, length, wait, buffer, ioStatus, device);
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        if (res) g_IoTelemetry.totalFastIoSuccesses++; else g_IoTelemetry.totalFastIoBypasses++;
        return res;
    }
    std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
    g_IoTelemetry.totalFastIoBypasses++;
    return false;
}

inline bool IoFastWrite(
    DeviceObject* device,
    void* fileObject,
    LargeInteger* offset,
    uint32_t length,
    bool wait,
    const void* buffer,
    IoStatusBlock* ioStatus
) {
    if (!device || !device->driverObject || !device->driverObject->fastIoDispatch) {
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        g_IoTelemetry.totalFastIoBypasses++;
        return false;
    }
    auto* fastIo = device->driverObject->fastIoDispatch;
    {
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        g_IoTelemetry.totalFastIoWrites++;
    }
    if (fastIo->fastIoWriteEx) {
        bool res = fastIo->fastIoWriteEx(fileObject, offset, length, wait, 0, buffer, ioStatus, device);
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        if (res) g_IoTelemetry.totalFastIoSuccesses++; else g_IoTelemetry.totalFastIoBypasses++;
        return res;
    }
    if (fastIo->fastIoWrite) {
        bool res = fastIo->fastIoWrite(fileObject, offset, length, wait, buffer, ioStatus, device);
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        if (res) g_IoTelemetry.totalFastIoSuccesses++; else g_IoTelemetry.totalFastIoBypasses++;
        return res;
    }
    std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
    g_IoTelemetry.totalFastIoBypasses++;
    return false;
}

inline bool IoFastDeviceControl(
    DeviceObject* device,
    void* fileObject,
    bool wait,
    void* inBuf,
    uint32_t inLen,
    void* outBuf,
    uint32_t outLen,
    uint32_t ioctl,
    IoStatusBlock* ioStatus
) {
    if (!device || !device->driverObject || !device->driverObject->fastIoDispatch) {
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        g_IoTelemetry.totalFastIoBypasses++;
        return false;
    }
    auto* fastIo = device->driverObject->fastIoDispatch;
    {
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        g_IoTelemetry.totalFastIoDeviceControls++;
    }
    if (fastIo->fastIoDeviceControl) {
        bool res = fastIo->fastIoDeviceControl(fileObject, wait, inBuf, inLen, outBuf, outLen, ioctl, ioStatus, device);
        std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
        if (res) g_IoTelemetry.totalFastIoSuccesses++; else g_IoTelemetry.totalFastIoBypasses++;
        return res;
    }
    std::lock_guard<std::mutex> lk(g_IoTelemetryMutex);
    g_IoTelemetry.totalFastIoBypasses++;
    return false;
}

inline bool IoFastQueryBasicInfo(
    DeviceObject* device,
    void* fileObject,
    bool wait,
    void* basicInfo,
    IoStatusBlock* ioStatus
) {
    if (!device || !device->driverObject || !device->driverObject->fastIoDispatch) return false;
    auto* fastIo = device->driverObject->fastIoDispatch;
    if (!fastIo->fastIoQueryBasicInfo) return false;
    return fastIo->fastIoQueryBasicInfo(fileObject, wait, basicInfo, ioStatus, device);
}

inline bool IoFastQueryStandardInfo(
    DeviceObject* device,
    void* fileObject,
    bool wait,
    void* standardInfo,
    IoStatusBlock* ioStatus
) {
    if (!device || !device->driverObject || !device->driverObject->fastIoDispatch) return false;
    auto* fastIo = device->driverObject->fastIoDispatch;
    if (!fastIo->fastIoQueryStandardInfo) return false;
    return fastIo->fastIoQueryStandardInfo(fileObject, wait, standardInfo, ioStatus, device);
}

} // namespace micant::io
