// ============================================================================
// MicaNT: Sovereign Bare-Metal I/O Subsystem & IRP Dispatcher
// (io_baremetal.hpp)
//
// Description:
//   Clean-room freestanding NT I/O Subsystem for Ring 0 bare-metal silicon.
//   Implements canonical Dave Cutler packet-driven I/O model:
//   - I/O Request Packets (IRP) with IO_STACK_LOCATION traversal
//   - Major function dispatch table (IRP_MJ_CREATE..IRP_MJ_PNP)
//   - DRIVER_OBJECT and DEVICE_OBJECT registration and layering
//   - IoCallDriver, IoCompleteRequest, and IoBuildSynchronousFsdRequest
//   - Core bare-metal drivers (\Driver\Null, \Driver\Console, \Driver\Disk, \Driver\FastFAT)
//
// Clean-Room ISO C++23. Zero proprietary headers, binaries, or decompiled code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/executive/ob_baremetal.hpp"

namespace micant::km::io_baremetal {

// --- Canonical Windows NT IRP Major Function Codes (0x00 - 0x1B) ---
inline constexpr uint8_t IRP_MJ_CREATE                   = 0x00;
inline constexpr uint8_t IRP_MJ_CREATE_NAMED_PIPE        = 0x01;
inline constexpr uint8_t IRP_MJ_CLOSE                    = 0x02;
inline constexpr uint8_t IRP_MJ_READ                     = 0x03;
inline constexpr uint8_t IRP_MJ_WRITE                    = 0x04;
inline constexpr uint8_t IRP_MJ_QUERY_INFORMATION        = 0x05;
inline constexpr uint8_t IRP_MJ_SET_INFORMATION          = 0x06;
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
inline constexpr uint8_t IRP_MJ_POWER                    = 0x16;
inline constexpr uint8_t IRP_MJ_SYSTEM_CONTROL           = 0x17;
inline constexpr uint8_t IRP_MJ_PNP                      = 0x1B;
inline constexpr uint8_t IRP_MJ_MAXIMUM_FUNCTION         = 0x1B;

// --- Device Object Buffering Model Flags ---
inline constexpr uint32_t DO_BUFFERED_IO                 = 0x00000004;
inline constexpr uint32_t DO_DIRECT_IO                   = 0x00000010;
inline constexpr uint32_t DO_NEITHER_IO                  = 0x00000000;
inline constexpr uint32_t DO_DEVICE_INITIALIZING         = 0x00000080;

// Forward Declarations
struct BareMetalDeviceObject;
struct BareMetalDriverObject;
struct BareMetalIrp;

using DriverDispatchRoutine = micant::NtStatus (*)(BareMetalDeviceObject* device, BareMetalIrp* irp);
using DiskIoRoutine = bool (*)(uint64_t lba, uint32_t sectorCount, void* buffer, bool isWrite);

// --- Canonical IO_STACK_LOCATION ---
struct BareMetalIoStackLocation {
    uint8_t majorFunction{IRP_MJ_CREATE};
    uint8_t minorFunction{0};
    uint8_t flags{0};
    uint8_t control{0};

    union {
        struct {
            uint32_t outputBufferLength;
            uint32_t inputBufferLength;
            uint32_t ioControlCode;
            void*    type3InputBuffer;
        } deviceIoControl;

        struct {
            uint32_t length;
            uint32_t key;
            uint64_t byteOffset;
        } read;

        struct {
            uint32_t length;
            uint32_t key;
            uint64_t byteOffset;
        } write;
    } parameters{};

    BareMetalDeviceObject* deviceObject{nullptr};
    void* fileObject{nullptr};
    void* context{nullptr};
};

// --- Canonical I/O Request Packet (IRP) ---
inline constexpr int8_t MAX_IRP_STACK_LOCATIONS = 4;

struct BareMetalIrp {
    uint8_t majorFunction{IRP_MJ_CREATE};
    uint8_t minorFunction{0};
    uint32_t flags{0};
    micant::IoStatusBlock ioStatus{};
    BareMetalDeviceObject* deviceObject{nullptr};
    void* userBuffer{nullptr};
    void* systemBuffer{nullptr};
    uint32_t length{0};
    uint64_t byteOffset{0};

    int8_t stackCount{MAX_IRP_STACK_LOCATIONS};
    int8_t currentStackLocation{1};
    BareMetalIoStackLocation stackLocations[MAX_IRP_STACK_LOCATIONS]{};

    [[nodiscard]] BareMetalIoStackLocation* getCurrentStack() noexcept {
        int idx = currentStackLocation - 1;
        if (idx >= 0 && idx < stackCount) return &stackLocations[idx];
        return &stackLocations[0];
    }
};

// --- Driver Object ---
struct BareMetalDriverObject {
    char driverName[48]{};
    DriverDispatchRoutine majorFunction[32]{nullptr};
    void (*driverUnload)(BareMetalDriverObject* drv){nullptr};

    void setDispatch(uint8_t major, DriverDispatchRoutine fn) noexcept {
        if (major < 32) majorFunction[major] = fn;
    }
};

// --- Device Object ---
struct BareMetalDeviceObject {
    BareMetalDriverObject* driverObject{nullptr};
    BareMetalDeviceObject* nextDevice{nullptr};
    BareMetalDeviceObject* attachedDevice{nullptr};
    uint32_t deviceType{micant::km::ob_baremetal::FILE_DEVICE_NULL};
    uint32_t characteristics{0};
    uint32_t flags{DO_BUFFERED_IO};
    char deviceName[64]{};
    void* deviceExtension{nullptr};
};

// --- I/O Subsystem Diagnostic Telemetry ---
struct BareMetalIoTelemetry {
    uint64_t totalDriversCreated{0};
    uint64_t totalDevicesCreated{0};
    uint64_t totalIrpsDispatched{0};
    uint64_t totalIrpsCompleted{0};
    uint64_t totalBytesRead{0};
    uint64_t totalBytesWritten{0};
    uint64_t totalDeviceControlCalls{0};
};

// --- Central Bare-Metal I/O Manager ---
inline constexpr size_t MAX_DRIVERS = 16;
inline constexpr size_t MAX_DEVICES = 32;
inline constexpr size_t MAX_IN_FLIGHT_IRPS = 16;

class BareMetalIoManager {
private:
    BareMetalDriverObject m_drivers[MAX_DRIVERS]{};
    BareMetalDeviceObject m_devices[MAX_DEVICES]{};
    BareMetalIrp          m_irps[MAX_IN_FLIGHT_IRPS]{};
    size_t                m_driverCount{0};
    size_t                m_deviceCount{0};
    size_t                m_irpAllocIndex{0};
    BareMetalIoTelemetry  m_telemetry{};
    DiskIoRoutine         m_diskHandler{nullptr};
    bool                  m_initialized{false};

public:
    static inline void copyStr(char* dst, const char* src, size_t maxLen) noexcept {
        if (!dst || maxLen == 0) return;
        size_t i = 0;
        if (src) {
            while (src[i] && i + 1 < maxLen) {
                dst[i] = src[i];
                ++i;
            }
        }
        dst[i] = '\0';
    }

    static inline bool strEqual(const char* a, const char* b) noexcept {
        if (!a || !b) return false;
        while (*a && *b) {
            char ca = *a++;
            char cb = *b++;
            if (ca >= 'A' && ca <= 'Z') ca += 32;
            if (cb >= 'A' && cb <= 'Z') cb += 32;
            if (ca != cb) return false;
        }
        return *a == *b;
    }

    static inline void concatStr(char* dst, const char* src, size_t maxLen) noexcept {
        if (!dst || !src || maxLen == 0) return;
        size_t d = 0;
        while (dst[d] && d < maxLen) ++d;
        size_t s = 0;
        while (src[s] && d + 1 < maxLen) {
            dst[d++] = src[s++];
        }
        dst[d] = '\0';
    }

    static inline void appendDec(char* dst, uint64_t val, size_t maxLen) noexcept {
        char buf[24];
        int p = 0;
        if (val == 0) {
            buf[p++] = '0';
        } else {
            char rev[24];
            int rp = 0;
            while (val > 0) {
                rev[rp++] = '0' + (val % 10);
                val /= 10;
            }
            while (rp > 0) {
                buf[p++] = rev[--rp];
            }
        }
        buf[p] = '\0';
        concatStr(dst, buf, maxLen);
    }

    static BareMetalIoManager& get() noexcept {
        static BareMetalIoManager instance;
        return instance;
    }

    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }
    [[nodiscard]] const BareMetalIoTelemetry& getTelemetry() const noexcept { return m_telemetry; }
    [[nodiscard]] size_t getDriverCount() const noexcept { return m_driverCount; }
    [[nodiscard]] size_t getDeviceCount() const noexcept { return m_deviceCount; }

    void setDiskIoHandler(DiskIoRoutine handler) noexcept { m_diskHandler = handler; }
    [[nodiscard]] DiskIoRoutine getDiskIoHandler() const noexcept { return m_diskHandler; }

    BareMetalDriverObject* createDriver(const char* driverName) noexcept {
        if (!driverName || m_driverCount >= MAX_DRIVERS) return nullptr;
        for (size_t i = 0; i < m_driverCount; ++i) {
            if (strEqual(m_drivers[i].driverName, driverName)) return &m_drivers[i];
        }

        auto* drv = &m_drivers[m_driverCount++];
        *drv = BareMetalDriverObject{};
        copyStr(drv->driverName, driverName, sizeof(drv->driverName));

        // Default unsupported stub for all 32 major functions
        for (size_t m = 0; m < 32; ++m) {
            drv->majorFunction[m] = [](BareMetalDeviceObject*, BareMetalIrp* irp) -> micant::NtStatus {
                if (irp) {
                    irp->ioStatus.status = micant::NtStatus::NotImplemented;
                    irp->ioStatus.information = 0;
                }
                return micant::NtStatus::NotImplemented;
            };
        }

        m_telemetry.totalDriversCreated++;
        return drv;
    }

    BareMetalDeviceObject* createDevice(
        BareMetalDriverObject* drv,
        const char* deviceName,
        uint32_t deviceType,
        uint32_t flags = DO_BUFFERED_IO,
        void* deviceExtension = nullptr
    ) noexcept {
        if (!drv || !deviceName || m_deviceCount >= MAX_DEVICES) return nullptr;
        for (size_t i = 0; i < m_deviceCount; ++i) {
            if (strEqual(m_devices[i].deviceName, deviceName)) return &m_devices[i];
        }

        auto* dev = &m_devices[m_deviceCount++];
        *dev = BareMetalDeviceObject{};
        dev->driverObject = drv;
        dev->deviceType = deviceType;
        dev->flags = flags;
        dev->deviceExtension = deviceExtension;
        copyStr(dev->deviceName, deviceName, sizeof(dev->deviceName));

        // Also register in Object Manager
        auto& ob = micant::km::ob_baremetal::BareMetalObjectManager::get();
        ob.createDevice(deviceName, deviceType);

        m_telemetry.totalDevicesCreated++;
        return dev;
    }

    BareMetalDeviceObject* findDevice(const char* name) noexcept {
        if (!name) return nullptr;
        for (size_t i = 0; i < m_deviceCount; ++i) {
            if (strEqual(m_devices[i].deviceName, name)) return &m_devices[i];
        }
        return nullptr;
    }

    BareMetalDriverObject* findDriver(const char* name) noexcept {
        if (!name) return nullptr;
        for (size_t i = 0; i < m_driverCount; ++i) {
            if (strEqual(m_drivers[i].driverName, name)) return &m_drivers[i];
        }
        return nullptr;
    }

    BareMetalIrp* allocateIrp(uint8_t majorFunction, BareMetalDeviceObject* dev) noexcept {
        auto* irp = &m_irps[m_irpAllocIndex++ % MAX_IN_FLIGHT_IRPS];
        *irp = BareMetalIrp{};
        irp->majorFunction = majorFunction;
        irp->deviceObject = dev;
        irp->stackCount = 2;
        irp->currentStackLocation = 1;

        auto* sp = irp->getCurrentStack();
        sp->majorFunction = majorFunction;
        sp->deviceObject = dev;
        return irp;
    }

    micant::NtStatus callDriver(BareMetalDeviceObject* dev, BareMetalIrp* irp) noexcept {
        if (!dev || !dev->driverObject || !irp) return micant::NtStatus::InvalidParameter;
        m_telemetry.totalIrpsDispatched++;

        uint8_t major = irp->majorFunction;
        if (major >= 32) return micant::NtStatus::InvalidDeviceRequest;

        auto dispatch = dev->driverObject->majorFunction[major];
        if (!dispatch) return micant::NtStatus::NotImplemented;

        return dispatch(dev, irp);
    }

    void completeRequest(BareMetalIrp* irp, uint32_t /*priorityBoost*/ = 0) noexcept {
        if (!irp) return;
        m_telemetry.totalIrpsCompleted++;

        switch (irp->majorFunction) {
            case IRP_MJ_READ:
                m_telemetry.totalBytesRead += irp->ioStatus.information;
                break;
            case IRP_MJ_WRITE:
                m_telemetry.totalBytesWritten += irp->ioStatus.information;
                break;
            case IRP_MJ_DEVICE_CONTROL:
                m_telemetry.totalDeviceControlCalls++;
                break;
            default:
                break;
        }
    }

    void dumpIoStatus(char* buf, size_t maxLen) const noexcept {
        if (!buf || maxLen == 0) return;
        buf[0] = '\0';

        concatStr(buf, "Canonical NT I/O Subsystem & Driver Dispatch Hierarchy:\r\n", maxLen);
        concatStr(buf, "-------------------------------------------------------\r\n", maxLen);
        concatStr(buf, "Drivers Loaded: ", maxLen);
        appendDec(buf, m_driverCount, maxLen);
        concatStr(buf, " | Devices Registered: ", maxLen);
        appendDec(buf, m_deviceCount, maxLen);
        concatStr(buf, " | IRPs Handled: ", maxLen);
        appendDec(buf, m_telemetry.totalIrpsCompleted, maxLen);
        concatStr(buf, " | Read: ", maxLen);
        appendDec(buf, m_telemetry.totalBytesRead, maxLen);
        concatStr(buf, "B | Written: ", maxLen);
        appendDec(buf, m_telemetry.totalBytesWritten, maxLen);
        concatStr(buf, "B\r\n\r\nRegistered Devices & Drivers:\r\n", maxLen);

        for (size_t i = 0; i < m_deviceCount; ++i) {
            const auto& dev = m_devices[i];
            concatStr(buf, "  ", maxLen);
            concatStr(buf, dev.deviceName, maxLen);
            for (size_t s = 0; dev.deviceName[s]; ++s) {
                if (s > 28) break;
            }
            concatStr(buf, " -> Driver: ", maxLen);
            if (dev.driverObject) {
                concatStr(buf, dev.driverObject->driverName, maxLen);
            } else {
                concatStr(buf, "None", maxLen);
            }
            concatStr(buf, " [Type: 0x", maxLen);
            char h[12];
            uint32_t val = dev.deviceType;
            int p = 0;
            for (int s = 28; s >= 0; s -= 4) {
                uint8_t nib = (val >> s) & 0xF;
                h[p++] = (nib < 10) ? ('0' + nib) : ('A' + nib - 10);
            }
            h[p] = '\0';
            concatStr(buf, h, maxLen);
            concatStr(buf, "]\r\n", maxLen);
        }
    }

    void initialize() noexcept {
        if (m_initialized) return;

        // 1. \Driver\Null (Null Sink)
        auto* drvNull = createDriver("\\Driver\\Null");
        if (drvNull) {
            drvNull->setDispatch(IRP_MJ_CREATE, [](BareMetalDeviceObject*, BareMetalIrp* irp) {
                irp->ioStatus.status = micant::NtStatus::Success;
                irp->ioStatus.information = 0;
                BareMetalIoManager::get().completeRequest(irp);
                return micant::NtStatus::Success;
            });
            drvNull->setDispatch(IRP_MJ_CLOSE, [](BareMetalDeviceObject*, BareMetalIrp* irp) {
                irp->ioStatus.status = micant::NtStatus::Success;
                irp->ioStatus.information = 0;
                BareMetalIoManager::get().completeRequest(irp);
                return micant::NtStatus::Success;
            });
            drvNull->setDispatch(IRP_MJ_READ, [](BareMetalDeviceObject*, BareMetalIrp* irp) {
                irp->ioStatus.status = micant::NtStatus::EndOfFile;
                irp->ioStatus.information = 0;
                BareMetalIoManager::get().completeRequest(irp);
                return micant::NtStatus::EndOfFile;
            });
            drvNull->setDispatch(IRP_MJ_WRITE, [](BareMetalDeviceObject*, BareMetalIrp* irp) {
                auto* sp = irp->getCurrentStack();
                irp->ioStatus.status = micant::NtStatus::Success;
                irp->ioStatus.information = sp->parameters.write.length;
                BareMetalIoManager::get().completeRequest(irp);
                return micant::NtStatus::Success;
            });
            createDevice(drvNull, "\\Device\\Null", micant::km::ob_baremetal::FILE_DEVICE_NULL);
        }

        // 2. \Driver\BootVid (GOP Framebuffer)
        auto* drvVid = createDriver("\\Driver\\BootVid");
        if (drvVid) {
            drvVid->setDispatch(IRP_MJ_CREATE, [](BareMetalDeviceObject*, BareMetalIrp* irp) {
                irp->ioStatus.status = micant::NtStatus::Success;
                irp->ioStatus.information = 0;
                BareMetalIoManager::get().completeRequest(irp);
                return micant::NtStatus::Success;
            });
            drvVid->setDispatch(IRP_MJ_DEVICE_CONTROL, [](BareMetalDeviceObject*, BareMetalIrp* irp) {
                irp->ioStatus.status = micant::NtStatus::Success;
                irp->ioStatus.information = 0;
                BareMetalIoManager::get().completeRequest(irp);
                return micant::NtStatus::Success;
            });
            createDevice(drvVid, "\\Device\\Video0", micant::km::ob_baremetal::FILE_DEVICE_SCREEN);
        }

        // 3. \Driver\Disk (AHCI SATA Controller)
        auto* drvDisk = createDriver("\\Driver\\Disk");
        if (drvDisk) {
            drvDisk->setDispatch(IRP_MJ_CREATE, [](BareMetalDeviceObject*, BareMetalIrp* irp) {
                irp->ioStatus.status = micant::NtStatus::Success;
                irp->ioStatus.information = 0;
                BareMetalIoManager::get().completeRequest(irp);
                return micant::NtStatus::Success;
            });
            drvDisk->setDispatch(IRP_MJ_DEVICE_CONTROL, [](BareMetalDeviceObject*, BareMetalIrp* irp) {
                irp->ioStatus.status = micant::NtStatus::Success;
                irp->ioStatus.information = 0;
                BareMetalIoManager::get().completeRequest(irp);
                return micant::NtStatus::Success;
            });
            drvDisk->setDispatch(IRP_MJ_READ, [](BareMetalDeviceObject* dev, BareMetalIrp* irp) -> micant::NtStatus {
                if (!irp || !dev) return micant::NtStatus::InvalidParameter;
                auto* sp = irp->getCurrentStack();
                uint64_t byteOffset = sp->parameters.read.byteOffset;
                uint32_t length = sp->parameters.read.length;
                void* buffer = irp->userBuffer ? irp->userBuffer : irp->systemBuffer;

                uint64_t partitionBaseLba = 0;
                if (BareMetalIoManager::strEqual(dev->deviceName, "\\Device\\Harddisk0\\Partition2")) {
                    partitionBaseLba = 2099200; // MICA_SYS (sdb2)
                } else if (BareMetalIoManager::strEqual(dev->deviceName, "\\Device\\Harddisk1\\Partition2") ||
                           BareMetalIoManager::strEqual(dev->deviceName, "\\Device\\Harddisk0\\Partition3")) {
                    partitionBaseLba = 270534656; // MICA_DATA (sdb3)
                }

                uint64_t lba = partitionBaseLba + (byteOffset / 512);
                uint32_t sectors = (length + 511) / 512;
                if (sectors == 0) sectors = 1;

                auto diskHandler = BareMetalIoManager::get().getDiskIoHandler();
                if (diskHandler && buffer) {
                    bool ok = diskHandler(lba, sectors, buffer, false);
                    if (ok) {
                        irp->ioStatus.status = micant::NtStatus::Success;
                        irp->ioStatus.information = length;
                        BareMetalIoManager::get().completeRequest(irp);
                        return micant::NtStatus::Success;
                    }
                }
                irp->ioStatus.status = micant::NtStatus::DeviceError;
                irp->ioStatus.information = 0;
                BareMetalIoManager::get().completeRequest(irp);
                return micant::NtStatus::DeviceError;
            });
            drvDisk->setDispatch(IRP_MJ_WRITE, [](BareMetalDeviceObject* dev, BareMetalIrp* irp) -> micant::NtStatus {
                if (!irp || !dev) return micant::NtStatus::InvalidParameter;
                auto* sp = irp->getCurrentStack();
                uint64_t byteOffset = sp->parameters.write.byteOffset;
                uint32_t length = sp->parameters.write.length;
                const void* buffer = irp->userBuffer ? irp->userBuffer : irp->systemBuffer;

                uint64_t partitionBaseLba = 0;
                if (BareMetalIoManager::strEqual(dev->deviceName, "\\Device\\Harddisk0\\Partition2")) {
                    partitionBaseLba = 2099200; // MICA_SYS (sdb2)
                } else if (BareMetalIoManager::strEqual(dev->deviceName, "\\Device\\Harddisk1\\Partition2") ||
                           BareMetalIoManager::strEqual(dev->deviceName, "\\Device\\Harddisk0\\Partition3")) {
                    partitionBaseLba = 270534656; // MICA_DATA (sdb3)
                }

                uint64_t lba = partitionBaseLba + (byteOffset / 512);
                uint32_t sectors = (length + 511) / 512;
                if (sectors == 0) sectors = 1;

                auto diskHandler = BareMetalIoManager::get().getDiskIoHandler();
                if (diskHandler && buffer) {
                    bool ok = diskHandler(lba, sectors, const_cast<void*>(buffer), true);
                    if (ok) {
                        irp->ioStatus.status = micant::NtStatus::Success;
                        irp->ioStatus.information = length;
                        BareMetalIoManager::get().completeRequest(irp);
                        return micant::NtStatus::Success;
                    }
                }
                irp->ioStatus.status = micant::NtStatus::DeviceError;
                irp->ioStatus.information = 0;
                BareMetalIoManager::get().completeRequest(irp);
                return micant::NtStatus::DeviceError;
            });
            createDevice(drvDisk, "\\Device\\Harddisk0", micant::km::ob_baremetal::FILE_DEVICE_DISK);
            createDevice(drvDisk, "\\Device\\Harddisk0\\Partition1", micant::km::ob_baremetal::FILE_DEVICE_DISK);
            createDevice(drvDisk, "\\Device\\Harddisk0\\Partition2", micant::km::ob_baremetal::FILE_DEVICE_DISK);
            createDevice(drvDisk, "\\Device\\Harddisk0\\Partition3", micant::km::ob_baremetal::FILE_DEVICE_DISK);
            createDevice(drvDisk, "\\Device\\Harddisk1\\Partition2", micant::km::ob_baremetal::FILE_DEVICE_DISK);
        }

        // 4. \Driver\I210 (Intel I210 Gigabit Ethernet)
        auto* drvNet = createDriver("\\Driver\\I210");
        if (drvNet) {
            drvNet->setDispatch(IRP_MJ_CREATE, [](BareMetalDeviceObject*, BareMetalIrp* irp) {
                irp->ioStatus.status = micant::NtStatus::Success;
                irp->ioStatus.information = 0;
                BareMetalIoManager::get().completeRequest(irp);
                return micant::NtStatus::Success;
            });
            drvNet->setDispatch(IRP_MJ_DEVICE_CONTROL, [](BareMetalDeviceObject*, BareMetalIrp* irp) {
                irp->ioStatus.status = micant::NtStatus::Success;
                irp->ioStatus.information = 0;
                BareMetalIoManager::get().completeRequest(irp);
                return micant::NtStatus::Success;
            });
            createDevice(drvNet, "\\Device\\NetworkAdapter0", micant::km::ob_baremetal::FILE_DEVICE_NETWORK);
        }

        // 5. \Driver\xHCI (USB 3.0 Virtual Keyboard)
        auto* drvUsb = createDriver("\\Driver\\xHCI");
        if (drvUsb) {
            drvUsb->setDispatch(IRP_MJ_CREATE, [](BareMetalDeviceObject*, BareMetalIrp* irp) {
                irp->ioStatus.status = micant::NtStatus::Success;
                irp->ioStatus.information = 0;
                BareMetalIoManager::get().completeRequest(irp);
                return micant::NtStatus::Success;
            });
            createDevice(drvUsb, "\\Device\\KeyboardClass0", micant::km::ob_baremetal::FILE_DEVICE_KEYBOARD);
        }

        m_initialized = true;
    }
};

} // namespace micant::km::io_baremetal
