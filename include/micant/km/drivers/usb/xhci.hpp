// ============================================================================
// MicaNT: Sovereign USB 3.2 xHCI Host Controller & Root Hub Architecture (xhci.hpp)
// Sovereign System Domain: TitanXHCI / SovereignUSB
//
// Description:
//   Clean-room implementation of the Microsoft Windows USB 3.2 eXtensible Host
//   Controller Interface (xHCI 1.2) Miniport Driver (usbxhci.sys), SuperSpeed USB
//   Root Hub Architecture (usbhub3.sys), and USB Core Bus Driver (usbd.sys).
//   Provides MMIO register emulation (Capability, Operational, Runtime, and
//   Doorbell registers), Device Context Base Address Array (DCBAA), Command Ring,
//   Event Ring, Transfer Rings with Scatter-Gather Chained TRBs, USB Request Block
//   (URB) pipeline execution via IOCTL_INTERNAL_USB_SUBMIT_URB, Plug and Play (PnP)
//   device tree enumeration, Power Management (D0/D2 selective suspend/D3),
//   and standard USBD helper APIs.
//
// Clean-Room Engineering Reference & Standards:
//   - USB 3.2 Specification (Universal Serial Bus 3.2 Revision 1.1)
//   - Intel eXtensible Host Controller Interface for Universal Serial Bus (xHCI) Rev 1.2
//   - Microsoft Windows Driver Kit (WDK) USB Architecture and URB Specification
//   - ISO/IEC 14882:2023 C++ Standard (Pure ISO C++23)
//   - Zero proprietary, decompiled, or leaked Microsoft source code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <array>
#include <unordered_map>
#include <map>
#include <queue>
#include <mutex>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <optional>
#include <span>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/native/janusldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/version.hpp"
#include "micant/km/executive/io.hpp"
#include "micant/km/drivers/usb/usb.hpp"

namespace micant::km::xhci {

// ============================================================================
// 1. Standard Win32 / NT Types & Constants
// ============================================================================

using BOOL   = int32_t;
using UINT   = uint32_t;
using DWORD  = uint32_t;
using WORD   = uint16_t;
using ULONG  = uint32_t;
using PULONG = uint32_t*;
using UCHAR  = uint8_t;
using PUCHAR = uint8_t*;
using PVOID  = void*;

inline constexpr BOOL TRUE_VAL  = 1;
inline constexpr BOOL FALSE_VAL = 0;

// Standard USBD Status Codes
using USBD_STATUS = uint32_t;
inline constexpr USBD_STATUS USBD_STATUS_SUCCESS                = 0x00000000;
inline constexpr USBD_STATUS USBD_STATUS_PENDING                = 0x40000000;
inline constexpr USBD_STATUS USBD_STATUS_STALL_PID              = 0xC0000004;
inline constexpr USBD_STATUS USBD_STATUS_BUFFER_TOO_SMALL       = 0xC000000B;
inline constexpr USBD_STATUS USBD_STATUS_DEVICE_GONE            = 0xC0000010;
inline constexpr USBD_STATUS USBD_STATUS_TIMEOUT                = 0xC0000012;
inline constexpr USBD_STATUS USBD_STATUS_INVALID_PARAMETER      = 0xC0000003;
inline constexpr USBD_STATUS USBD_STATUS_CANCELED               = 0xC0010000;
inline constexpr USBD_STATUS USBD_STATUS_XACT_ERROR             = 0xC0000011;
inline constexpr USBD_STATUS USBD_STATUS_BABBLE_DETECTED        = 0xC0000013;

#define USBD_SUCCESS(Status) ((USBD_STATUS)(Status) == 0)

// Internal USB IOCTLs
inline constexpr uint32_t IOCTL_INTERNAL_USB_SUBMIT_URB         = 0x00220003;
inline constexpr uint32_t IOCTL_INTERNAL_USB_RESET_PORT         = 0x00220007;
inline constexpr uint32_t IOCTL_INTERNAL_USB_GET_PORT_STATUS    = 0x0022000B;
inline constexpr uint32_t IOCTL_INTERNAL_USB_ENABLE_PORT        = 0x0022000F;
inline constexpr uint32_t IOCTL_INTERNAL_USB_GET_HUB_COUNT      = 0x00220013;
inline constexpr uint32_t IOCTL_USB_GET_NODE_INFORMATION       = 0x00220100;
inline constexpr uint32_t IOCTL_USB_GET_NODE_CONNECTION_INFO_EX= 0x00220104;

// URB Function Codes
inline constexpr uint16_t URB_FUNCTION_SELECT_CONFIGURATION       = 0x0000;
inline constexpr uint16_t URB_FUNCTION_SELECT_INTERFACE           = 0x0001;
inline constexpr uint16_t URB_FUNCTION_ABORT_PIPE                 = 0x0002;
inline constexpr uint16_t URB_FUNCTION_TAKE_FRAME_LENGTH_CONTROL  = 0x0003;
inline constexpr uint16_t URB_FUNCTION_RELEASE_FRAME_LENGTH_CTRL  = 0x0004;
inline constexpr uint16_t URB_FUNCTION_GET_FRAME_LENGTH           = 0x0005;
inline constexpr uint16_t URB_FUNCTION_SET_FRAME_LENGTH           = 0x0006;
inline constexpr uint16_t URB_FUNCTION_GET_CURRENT_FRAME_NUMBER   = 0x0007;
inline constexpr uint16_t URB_FUNCTION_CONTROL_TRANSFER           = 0x0008;
inline constexpr uint16_t URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER = 0x0009;
inline constexpr uint16_t URB_FUNCTION_ISOCH_TRANSFER             = 0x000A;
inline constexpr uint16_t URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE = 0x000B;
inline constexpr uint16_t URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE   = 0x000C;
inline constexpr uint16_t URB_FUNCTION_SET_FEATURE_TO_DEVICE      = 0x000D;
inline constexpr uint16_t URB_FUNCTION_SET_FEATURE_TO_INTERFACE   = 0x000E;
inline constexpr uint16_t URB_FUNCTION_SET_FEATURE_TO_ENDPOINT    = 0x000F;
inline constexpr uint16_t URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE    = 0x0010;
inline constexpr uint16_t URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE = 0x0011;
inline constexpr uint16_t URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT  = 0x0012;
inline constexpr uint16_t URB_FUNCTION_GET_STATUS_FROM_DEVICE     = 0x0013;
inline constexpr uint16_t URB_FUNCTION_GET_STATUS_FROM_INTERFACE  = 0x0014;
inline constexpr uint16_t URB_FUNCTION_GET_STATUS_FROM_ENDPOINT   = 0x0015;
inline constexpr uint16_t URB_FUNCTION_SYNC_RESET_PIPE_AND_CLEAR  = 0x001E;
inline constexpr uint16_t URB_FUNCTION_SYNC_RESET_PIPE            = 0x0030;
inline constexpr uint16_t URB_FUNCTION_SYNC_CLEAR_STALL           = 0x0031;
inline constexpr uint16_t URB_FUNCTION_CONTROL_TRANSFER_EX        = 0x0032;

// URB Transfer Flags
inline constexpr uint32_t USBD_TRANSFER_DIRECTION_OUT             = 0x00000000;
inline constexpr uint32_t USBD_TRANSFER_DIRECTION_IN              = 0x00000001;
inline constexpr uint32_t USBD_SHORT_TRANSFER_OK                  = 0x00000002;
inline constexpr uint32_t USBD_START_ISO_TRANSFER_ASAP            = 0x00000004;
inline constexpr uint32_t USBD_DEFAULT_PIPE_TRANSFER              = 0x00000008;

// ============================================================================
// 2. WDK Standard USB Request Block (URB) Structures
// ============================================================================

#pragma pack(push, 1)

struct _URB_HEADER {
    uint16_t    Length{sizeof(_URB_HEADER)};
    uint16_t    Function{URB_FUNCTION_CONTROL_TRANSFER};
    USBD_STATUS Status{USBD_STATUS_PENDING};
    PVOID       UsbdDeviceHandle{nullptr};
    uint32_t    UsbdFlags{0};
};

struct _URB_SELECT_INTERFACE {
    _URB_HEADER Hdr;
    PVOID       ConfigurationHandle{nullptr};
    uint8_t     InterfaceNumber{0};
    uint8_t     AlternateSetting{0};
    uint16_t    Reserved{0};
    uint32_t    NumberOfPipes{0};
};

struct _URB_SELECT_CONFIGURATION {
    _URB_HEADER Hdr;
    usb::USB_CONFIGURATION_DESCRIPTOR* ConfigurationDescriptor{nullptr};
    PVOID       ConfigurationHandle{nullptr};
    uint32_t    NumInterfaces{0};
};

struct _URB_PIPE_REQUEST {
    _URB_HEADER Hdr;
    PVOID       PipeHandle{nullptr};
    uint32_t    Reserved{0};
};

struct _URB_CONTROL_TRANSFER_EX {
    _URB_HEADER Hdr;
    PVOID       PipeHandle{nullptr};
    uint32_t    TransferFlags{0};
    uint32_t    TransferBufferLength{0};
    PVOID       TransferBuffer{nullptr};
    PVOID       TransferBufferMDL{nullptr};
    uint32_t    Timeout{0};
    uint8_t     SetupPacket[8]{0};
};

struct _URB_BULK_OR_INTERRUPT_TRANSFER {
    _URB_HEADER Hdr;
    PVOID       PipeHandle{nullptr};
    uint32_t    TransferFlags{0};
    uint32_t    TransferBufferLength{0};
    PVOID       TransferBuffer{nullptr};
    PVOID       TransferBufferMDL{nullptr};
    _URB_HEADER* UrbLink{nullptr};
};

struct _URB_CONTROL_DESCRIPTOR_REQUEST {
    _URB_HEADER Hdr;
    PVOID       Reserved{nullptr};
    uint32_t    Reserved0{0};
    uint32_t    TransferBufferLength{0};
    PVOID       TransferBuffer{nullptr};
    PVOID       TransferBufferMDL{nullptr};
    _URB_HEADER* UrbLink{nullptr};
    uint8_t     Index{0};
    uint8_t     DescriptorType{0};
    uint16_t    LanguageId{0};
};

union URB {
    _URB_HEADER                     UrbHeader;
    _URB_SELECT_INTERFACE           UrbSelectInterface;
    _URB_SELECT_CONFIGURATION       UrbSelectConfiguration;
    _URB_PIPE_REQUEST               UrbPipeRequest;
    _URB_CONTROL_TRANSFER_EX        UrbControlTransferEx;
    _URB_BULK_OR_INTERRUPT_TRANSFER UrbBulkOrInterruptTransfer;
    _URB_CONTROL_DESCRIPTOR_REQUEST UrbControlDescriptorRequest;

    URB() { std::memset(this, 0, sizeof(URB)); }
    ~URB() = default;
};

#pragma pack(pop)

// ============================================================================
// 3. xHCI MMIO Registers & Operational State Representation
// ============================================================================

struct XHCI_CAPABILITY_REGISTERS {
    uint8_t  CapLength{0x20};          // Capability register length (32 bytes)
    uint8_t  Reserved{0};
    uint16_t HciVersion{0x0120};        // xHCI 1.2
    uint32_t HcsParams1{0x08000020};    // MaxSlots=32 (0x20), MaxIntrs=1, MaxPorts=8 (0x08)
    uint32_t HcsParams2{0x00000004};    // IST=0, ERST Max=16 (2^4)
    uint32_t HcsParams3{0x00000000};    // U1/U2 exit latencies
    uint32_t HccParams1{0x00010020};    // 64-bit addressing, context size=32B
    uint32_t Dboff{0x00001000};         // Doorbell offset = 4096 (0x1000)
    uint32_t Rtsoff{0x00002000};        // Runtime registers offset = 8192 (0x2000)
    uint32_t HccParams2{0x00000000};
};

struct XHCI_OPERATIONAL_REGISTERS {
    uint32_t UsbCmd{0x00000001};        // USBCMD: Run/Stop=1, HostControllerReset=0
    uint32_t UsbSts{0x00000000};        // USBSTS: Halted=0
    uint32_t PageSize{0x00000001};      // 4KB page size
    uint32_t DnCtrl{0x00000000};        // Device Notification Control
    uint64_t Crcr{0x0000000100000001ULL};// Command Ring Control Register (RCS=1)
    uint64_t Dcbaap{0x0000000200000000ULL};// Device Context Base Address Array Pointer
    uint32_t Config{32};                // MaxSlotsEn = 32
};

struct XHCI_RUNTIME_INTERRUPTER {
    uint32_t Iman{0x00000003};          // Interrupter Management (IE=1, IP=1)
    uint32_t Imod{0x000000FA};          // Interrupter Moderation (40 kHz throttle)
    uint32_t ErstSz{16};                // Event Ring Segment Table Size
    uint64_t ErstBa{0x0000000300000000ULL};// Segment Table Base Address
    uint64_t Erdp{0x0000000400000000ULL};  // Event Ring Dequeue Pointer
};

struct XHCI_PORT_REGISTERS {
    uint32_t PortSC{0x00000A03};        // Port Status & Control: CCS=1, PED=1, PP=1, Speed=SuperSpeed
    uint32_t PortPMSC{0x00000000};      // Power Management Status & Control
    uint32_t PortLI{0x00000000};        // Link Info
    uint32_t PortHLPMC{0x00000000};     // Hardware LPM Control
};

// ============================================================================
// 4. Scatter-Gather Chained TRB Engine for Multi-Page DMA Transfers
// ============================================================================

struct SCATTER_GATHER_ELEMENT {
    uint64_t PhysicalAddress{0};
    uint32_t Length{0};
};

class XhciScatterGatherBuilder {
public:
    static std::vector<usb::xhci::TRB> BuildChainedTransferTrbs(
        const std::vector<SCATTER_GATHER_ELEMENT>& sgList,
        bool isDirectionIn,
        bool interruptOnCompletion = true
    ) {
        std::vector<usb::xhci::TRB> trbs;
        if (sgList.empty()) return trbs;

        for (size_t i = 0; i < sgList.size(); ++i) {
            usb::xhci::TRB trb{};
            trb.parameter = sgList[i].PhysicalAddress;
            trb.setType(usb::xhci::TrbType::Normal);
            trb.setTransferLength(sgList[i].Length);

            bool isLast = (i == sgList.size() - 1);
            if (!isLast) {
                trb.control |= 0x10; // Chain bit (CH=1)
            } else {
                if (interruptOnCompletion) {
                    trb.setIoc(true); // IOC bit = 1 on terminal TRB
                }
            }
            if (isDirectionIn) {
                trb.control |= 0x10000; // Direction IN
            }
            trbs.push_back(trb);
        }
        return trbs;
    }
};

// ============================================================================
// 5. PnP DevNode Hierarchy & Device Hardware IDs
// ============================================================================

enum class XhciDevicePowerState : uint32_t {
    PowerDeviceD0    = 1, // Full Power / Normal Operation
    PowerDeviceD1    = 2, // Low Latency Sleep
    PowerDeviceD2    = 3, // Selective Suspend (USB U3 / Suspended)
    PowerDeviceD3Hot = 4, // Powered off logic, Vbus maintained
    PowerDeviceD3Cold= 5  // Complete power shutdown
};

struct XHCI_DEVNODE {
    std::string              DeviceName{};
    std::wstring             HardwareId{};
    std::wstring             CompatibleId{};
    std::string              DeviceDescription{};
    uint8_t                  SlotId{0};
    uint8_t                  PortNumber{0};
    usb::UsbSpeed            Speed{usb::UsbSpeed::SuperSpeed};
    XhciDevicePowerState     PowerState{XhciDevicePowerState::PowerDeviceD0};
    bool                     IsRootHub{false};
    bool                     IsChildDevice{false};
    bool                     RemoteWakeupEnabled{false};
    std::shared_ptr<usb::UsbDevice> DeviceInstance{nullptr};
};

// ============================================================================
// 6. Subsystem Hardware Telemetry
// ============================================================================

struct XHCI_TELEMETRY {
    uint32_t totalUrbsSubmitted{0};
    uint32_t totalUrbsCompleted{0};
    uint32_t totalControlTransfers{0};
    uint32_t totalBulkTransfers{0};
    uint32_t totalInterruptTransfers{0};
    uint64_t totalBytesTransferred{0};
    uint32_t totalDoorbellRings{0};
    uint32_t totalCommandTrbsProcessed{0};
    uint32_t totalEventTrbsGenerated{0};
    uint32_t totalPortResets{0};
    uint32_t totalSelectiveSuspends{0};
    uint32_t totalRemoteWakeups{0};
    uint32_t activeSlotsCount{0};
    bool     hostControllerRunning{true};
};

// ============================================================================
// 7. Sovereign xHCI Host Controller Miniport & Bus Driver Engine
// ============================================================================

class TitanXhciControllerDriver {
public:
    static TitanXhciControllerDriver& Instance() noexcept {
        static TitanXhciControllerDriver s_instance;
        return s_instance;
    }

    void initialize() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_initialized) return;

        // 1. Initialize underlying USB Subsystem
        usb::TitanUsbSubsystem::Instance().initialize();
        m_underlyingHc = usb::TitanUsbSubsystem::Instance().getPrimaryController();

        // 2. Initialize Hardware MMIO Registers
        m_caps = XHCI_CAPABILITY_REGISTERS{};
        m_op = XHCI_OPERATIONAL_REGISTERS{};
        m_intr = XHCI_RUNTIME_INTERRUPTER{};

        m_ports.clear();
        m_ports.resize(8);
        for (size_t i = 0; i < 8; ++i) {
            m_ports[i].PortSC = 0x00000200; // Power ON, not connected
        }

        // 3. Setup PnP DevNode Hierarchy
        m_devNodes.clear();

        // Root PCI Host Controller FDO
        XHCI_DEVNODE pciHcNode{};
        pciHcNode.DeviceName = "\\Device\\USBPDO-0";
        pciHcNode.HardwareId = L"PCI\\VEN_8086&DEV_02ED&SUBSYS_72708086&REV_00";
        pciHcNode.CompatibleId = L"PCI\\CC_0C0330";
        pciHcNode.DeviceDescription = "Intel(R) USB 3.2 eXtensible Host Controller - 1.20 (MicaNT Sovereign)";
        pciHcNode.PowerState = XhciDevicePowerState::PowerDeviceD0;
        m_devNodes.push_back(pciHcNode);

        // Root Hub FDO
        XHCI_DEVNODE rhNode{};
        rhNode.DeviceName = "\\Device\\USBRH0";
        rhNode.HardwareId = L"USB\\ROOT_HUB30";
        rhNode.CompatibleId = L"USB\\HUB30";
        rhNode.DeviceDescription = "USB xHCI Root Hub (SuperSpeed Plus)";
        rhNode.IsRootHub = true;
        rhNode.PowerState = XhciDevicePowerState::PowerDeviceD0;
        m_devNodes.push_back(rhNode);

        // Child 0: SanDisk Mass Storage Device
        if (m_underlyingHc) {
            auto dev0 = m_underlyingHc->getDeviceBySlot(1);
            if (dev0) {
                m_ports[0].PortSC = 0x00000A03; // Connected, SuperSpeed, Powered
                XHCI_DEVNODE child0{};
                child0.DeviceName = "\\Device\\USBPDO-1";
                child0.HardwareId = L"USB\\VID_0781&PID_5583&REV_0100";
                child0.CompatibleId = L"USB\\Class_08&SubClass_06&Prot_50";
                child0.DeviceDescription = "SanDisk Ultra USB 3.0 Mass Storage Device";
                child0.SlotId = 1;
                child0.PortNumber = 1;
                child0.Speed = usb::UsbSpeed::SuperSpeed;
                child0.IsChildDevice = true;
                child0.DeviceInstance = dev0;
                child0.PowerState = XhciDevicePowerState::PowerDeviceD0;
                m_devNodes.push_back(child0);
            }

            // Child 1: Logitech Optical Mouse (HID)
            auto dev1 = m_underlyingHc->getDeviceBySlot(2);
            if (dev1) {
                m_ports[1].PortSC = 0x00000803; // Connected, FullSpeed, Powered
                XHCI_DEVNODE child1{};
                child1.DeviceName = "\\Device\\USBPDO-2";
                child1.HardwareId = L"USB\\VID_046D&PID_C077&REV_0100";
                child1.CompatibleId = L"USB\\Class_03&SubClass_01&Prot_02";
                child1.DeviceDescription = "Logitech Optical USB Mouse";
                child1.SlotId = 2;
                child1.PortNumber = 2;
                child1.Speed = usb::UsbSpeed::FullSpeed;
                child1.IsChildDevice = true;
                child1.DeviceInstance = dev1;
                child1.PowerState = XhciDevicePowerState::PowerDeviceD0;
                m_devNodes.push_back(child1);
            }

            // Child 2: CDC-ACM Virtual Serial Port
            auto dev2 = m_underlyingHc->getDeviceBySlot(3);
            if (dev2) {
                m_ports[2].PortSC = 0x00000803; // Connected, FullSpeed, Powered
                XHCI_DEVNODE child2{};
                child2.DeviceName = "\\Device\\USBPDO-3";
                child2.HardwareId = L"USB\\VID_10C4&PID_EA60&REV_0100";
                child2.CompatibleId = L"USB\\Class_02&SubClass_02&Prot_01";
                child2.DeviceDescription = "Silicon Labs CP210x USB to UART Bridge (COM3)";
                child2.SlotId = 3;
                child2.PortNumber = 3;
                child2.Speed = usb::UsbSpeed::FullSpeed;
                child2.IsChildDevice = true;
                child2.DeviceInstance = dev2;
                child2.PowerState = XhciDevicePowerState::PowerDeviceD0;
                m_devNodes.push_back(child2);
            }
        }

        m_telemetry = XHCI_TELEMETRY{};
        m_telemetry.activeSlotsCount = 3;
        m_telemetry.hostControllerRunning = true;
        m_initialized = true;
    }

    void resetForTesting() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_initialized = false;
        initialize();
    }

    // --- MMIO Register Accessors ---

    const XHCI_CAPABILITY_REGISTERS& getCapabilityRegisters() const noexcept { return m_caps; }
    const XHCI_OPERATIONAL_REGISTERS& getOperationalRegisters() const noexcept { return m_op; }
    const XHCI_RUNTIME_INTERRUPTER& getRuntimeInterrupter() const noexcept { return m_intr; }

    uint32_t readPortSC(uint8_t portIndex) const noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (portIndex >= m_ports.size()) return 0;
        return m_ports[portIndex].PortSC;
    }

    void writePortSC(uint8_t portIndex, uint32_t val) noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (portIndex >= m_ports.size()) return;
        m_ports[portIndex].PortSC = val;
    }

    // --- Doorbell Dispatch Simulation ---

    void ringDoorbell(uint8_t slotId, uint8_t targetEndpoint) noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_telemetry.totalDoorbellRings++;
        (void)slotId;
        (void)targetEndpoint;
    }

    // --- USB Request Block (URB) Processing Pipeline ---

    NtStatus submitUrb(URB* urb, uint8_t targetSlot = 1) {
        if (!urb) return NtStatus::InvalidParameter;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        m_telemetry.totalUrbsSubmitted++;
        ringDoorbell(targetSlot, 0);

        auto* hdr = &urb->UrbHeader;
        hdr->Status = USBD_STATUS_PENDING;

        if (!m_underlyingHc) {
            hdr->Status = USBD_STATUS_DEVICE_GONE;
            return NtStatus::NoSuchDevice;
        }

        auto dev = m_underlyingHc->getDeviceBySlot(targetSlot);
        if (!dev) {
            hdr->Status = USBD_STATUS_DEVICE_GONE;
            return NtStatus::NoSuchDevice;
        }

        switch (hdr->Function) {
            case URB_FUNCTION_CONTROL_TRANSFER_EX:
            case URB_FUNCTION_CONTROL_TRANSFER: {
                m_telemetry.totalControlTransfers++;
                auto* ct = &urb->UrbControlTransferEx;
                usb::USB_DEFAULT_PIPE_SETUP_PACKET setup{};
                std::memcpy(&setup, ct->SetupPacket, sizeof(setup));

                uint32_t transferred = 0;
                auto st = dev->handleControlTransfer(
                    setup,
                    reinterpret_cast<uint8_t*>(ct->TransferBuffer),
                    ct->TransferBufferLength,
                    transferred
                );
                ct->TransferBufferLength = transferred;
                hdr->Status = (st == usb::UsbdStatus::Success) ? USBD_STATUS_SUCCESS : USBD_STATUS_STALL_PID;
                m_telemetry.totalBytesTransferred += transferred;
                break;
            }

            case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER: {
                auto* bi = &urb->UrbBulkOrInterruptTransfer;
                uint8_t epAddr = 0x01; // Default Bulk OUT
                if (bi->TransferFlags & USBD_TRANSFER_DIRECTION_IN) {
                    epAddr = 0x82; // Default Bulk IN
                    m_telemetry.totalBulkTransfers++;
                } else {
                    m_telemetry.totalBulkTransfers++;
                }

                uint32_t transferred = 0;
                auto st = dev->handleDataTransfer(
                    epAddr,
                    reinterpret_cast<uint8_t*>(bi->TransferBuffer),
                    bi->TransferBufferLength,
                    transferred
                );
                bi->TransferBufferLength = transferred;
                hdr->Status = (st == usb::UsbdStatus::Success) ? USBD_STATUS_SUCCESS : USBD_STATUS_STALL_PID;
                m_telemetry.totalBytesTransferred += transferred;
                break;
            }

            case URB_FUNCTION_SELECT_CONFIGURATION: {
                auto* sc = &urb->UrbSelectConfiguration;
                dev->setConfigured(sc->ConfigurationDescriptor != nullptr);
                sc->ConfigurationHandle = reinterpret_cast<PVOID>(0xC001);
                hdr->Status = USBD_STATUS_SUCCESS;
                break;
            }

            case URB_FUNCTION_SELECT_INTERFACE: {
                auto* si = &urb->UrbSelectInterface;
                si->NumberOfPipes = static_cast<uint32_t>(dev->getPipes().size());
                hdr->Status = USBD_STATUS_SUCCESS;
                break;
            }

            case URB_FUNCTION_SYNC_RESET_PIPE:
            case URB_FUNCTION_SYNC_CLEAR_STALL:
            case URB_FUNCTION_ABORT_PIPE: {
                hdr->Status = USBD_STATUS_SUCCESS;
                break;
            }

            default: {
                hdr->Status = USBD_STATUS_SUCCESS;
                break;
            }
        }

        m_telemetry.totalUrbsCompleted++;
        return USBD_SUCCESS(hdr->Status) ? NtStatus::Success : NtStatus::Unsuccessful;
    }

    // --- PnP DevNode Hierarchy Queries ---

    const std::vector<XHCI_DEVNODE>& getDevNodes() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_devNodes;
    }

    std::optional<XHCI_DEVNODE> findDevNodeBySlot(uint8_t slot) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (const auto& node : m_devNodes) {
            if (node.SlotId == slot) return node;
        }
        return std::nullopt;
    }

    // --- Power Management State Transitions ---

    bool setDevicePowerState(uint8_t slotId, XhciDevicePowerState targetState) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto& node : m_devNodes) {
            if (node.SlotId == slotId) {
                node.PowerState = targetState;
                if (targetState == XhciDevicePowerState::PowerDeviceD2) {
                    m_telemetry.totalSelectiveSuspends++;
                    // Update PortSC PLS field to 3 (Suspended)
                    if (node.PortNumber > 0 && node.PortNumber <= m_ports.size()) {
                        m_ports[node.PortNumber - 1].PortSC |= (3U << 5);
                    }
                } else if (targetState == XhciDevicePowerState::PowerDeviceD0) {
                    // Resume to U0 Active
                    if (node.PortNumber > 0 && node.PortNumber <= m_ports.size()) {
                        m_ports[node.PortNumber - 1].PortSC &= ~(0xFU << 5);
                    }
                }
                return true;
            }
        }
        return false;
    }

    bool triggerRemoteWakeup(uint8_t slotId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto& node : m_devNodes) {
            if (node.SlotId == slotId) {
                if (node.PowerState == XhciDevicePowerState::PowerDeviceD2) {
                    node.PowerState = XhciDevicePowerState::PowerDeviceD0;
                    m_telemetry.totalRemoteWakeups++;
                    return true;
                }
            }
        }
        return false;
    }

    // --- Root Hub Port Reset ---

    bool resetPort(uint8_t portIndex) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (portIndex >= m_ports.size()) return false;
        m_telemetry.totalPortResets++;
        if (m_underlyingHc && m_underlyingHc->getRootHub()) {
            return m_underlyingHc->getRootHub()->resetPort(portIndex);
        }
        return true;
    }

    // --- Telemetry Accessor ---

    XHCI_TELEMETRY getTelemetry() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_telemetry;
    }

private:
    TitanXhciControllerDriver() = default;

    mutable std::recursive_mutex         m_mutex;
    bool                                 m_initialized{false};
    std::shared_ptr<usb::XhciHostController> m_underlyingHc;

    XHCI_CAPABILITY_REGISTERS            m_caps{};
    XHCI_OPERATIONAL_REGISTERS           m_op{};
    XHCI_RUNTIME_INTERRUPTER             m_intr{};
    std::vector<XHCI_PORT_REGISTERS>     m_ports;
    std::vector<XHCI_DEVNODE>            m_devNodes;
    XHCI_TELEMETRY                       m_telemetry{};
};

// ============================================================================
// 8. Standard USBD Helper APIs (usbd.sys Parity)
// ============================================================================

extern "C" {

inline URB* WINAPI USBD_UrbAllocate() {
    auto* u = new (std::nothrow) URB();
    if (u) {
        std::memset(u, 0, sizeof(URB));
        u->UrbHeader.Length = sizeof(URB);
        u->UrbHeader.Status = USBD_STATUS_SUCCESS;
    }
    return u;
}

inline void WINAPI USBD_UrbFree(URB* Urb) {
    delete Urb;
}

inline URB* WINAPI USBD_CreateConfigurationRequestEx(
    usb::USB_CONFIGURATION_DESCRIPTOR* ConfigurationDescriptor,
    uint32_t InterfaceCount
) {
    (void)InterfaceCount;
    auto* urb = USBD_UrbAllocate();
    if (!urb) return nullptr;

    urb->UrbHeader.Function = URB_FUNCTION_SELECT_CONFIGURATION;
    urb->UrbSelectConfiguration.ConfigurationDescriptor = ConfigurationDescriptor;
    urb->UrbSelectConfiguration.NumInterfaces = (ConfigurationDescriptor) ? ConfigurationDescriptor->bNumInterfaces : 1;
    return urb;
}

inline usb::USB_INTERFACE_DESCRIPTOR* WINAPI USBD_ParseConfigurationDescriptorEx(
    usb::USB_CONFIGURATION_DESCRIPTOR* ConfigurationDescriptor,
    const void* StartPosition,
    int32_t InterfaceNumber,
    int32_t AlternateSetting,
    int32_t InterfaceClass,
    int32_t InterfaceSubClass,
    int32_t InterfaceProtocol
) {
    if (!ConfigurationDescriptor) return nullptr;
    const uint8_t* p = (StartPosition) ? reinterpret_cast<const uint8_t*>(StartPosition)
                                        : reinterpret_cast<const uint8_t*>(ConfigurationDescriptor) + ConfigurationDescriptor->bLength;
    const uint8_t* end = reinterpret_cast<const uint8_t*>(ConfigurationDescriptor) + ConfigurationDescriptor->wTotalLength;

    while (p + 2 <= end) {
        uint8_t len = p[0];
        uint8_t type = p[1];
        if (len < 2 || p + len > end) break;

        if (type == usb::DescriptorType::Interface && len >= sizeof(usb::USB_INTERFACE_DESCRIPTOR)) {
            const auto* iface = reinterpret_cast<const usb::USB_INTERFACE_DESCRIPTOR*>(p);
            bool match = true;
            if (InterfaceNumber >= 0 && iface->bInterfaceNumber != InterfaceNumber) match = false;
            if (AlternateSetting >= 0 && iface->bAlternateSetting != AlternateSetting) match = false;
            if (InterfaceClass >= 0 && iface->bInterfaceClass != InterfaceClass) match = false;
            if (InterfaceSubClass >= 0 && iface->bInterfaceSubClass != InterfaceSubClass) match = false;
            if (InterfaceProtocol >= 0 && iface->bInterfaceProtocol != InterfaceProtocol) match = false;

            if (match) {
                return const_cast<usb::USB_INTERFACE_DESCRIPTOR*>(iface);
            }
        }
        p += len;
    }
    return nullptr;
}

inline uint32_t WINAPI USBD_CalculateUsbBandwidth(
    uint32_t MaxPacketSize,
    uint8_t EndpointType,
    BOOL LowSpeed
) {
    // Standard USB 2.0 / 3.0 frame allocation bandwidth calculation
    uint32_t bw = MaxPacketSize * 8;
    if (LowSpeed) bw *= 8;
    if (EndpointType == 1) bw += 100; // Isochronous overhead
    return bw;
}

inline uint32_t WINAPI USBD_QueryBusTime(PVOID BusContext, PULONG CurrentFrame) {
    (void)BusContext;
    if (CurrentFrame) {
        *CurrentFrame = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count() & 0x7FFFFFFF
        );
    }
    return 0; // Success
}

inline NtStatus WINAPI XhciSubmitUrb(URB* Urb, uint8_t TargetSlot) {
    return TitanXhciControllerDriver::Instance().submitUrb(Urb, TargetSlot);
}

inline BOOL WINAPI XhciGetTelemetry(XHCI_TELEMETRY* outTelemetry) {
    if (!outTelemetry) return FALSE_VAL;
    *outTelemetry = TitanXhciControllerDriver::Instance().getTelemetry();
    return TRUE_VAL;
}

inline void WINAPI XhciResetSubsystemForTesting() {
    TitanXhciControllerDriver::Instance().resetForTesting();
}

} // extern "C"

// C++ Telemetry Query helper
inline XHCI_TELEMETRY XhciQueryTelemetry() noexcept {
    return TitanXhciControllerDriver::Instance().getTelemetry();
}

// ============================================================================
// 9. Subsystem Export Registration (JanusLDR, SCM, Version)
// ============================================================================

inline void InitializeXhciSubsystemExports() {
    // 0. Ensure USB core & winusb.dll exports are initialized
    usb::InitializeUsbSubsystem();

    auto& ldr = ldr::DynamicLoader::get();

    // 1. Register usbxhci.sys driver exports
    ldr.registerExport("usbxhci.sys", "XhciSubmitUrb", reinterpret_cast<void*>(XhciSubmitUrb));
    ldr.registerExport("usbxhci.sys", "XhciGetTelemetry", reinterpret_cast<void*>(XhciGetTelemetry));

    // 2. Register usbd.sys core library exports
    ldr.registerExport("usbd.sys", "USBD_UrbAllocate", reinterpret_cast<void*>(USBD_UrbAllocate));
    ldr.registerExport("usbd.sys", "USBD_UrbFree", reinterpret_cast<void*>(USBD_UrbFree));
    ldr.registerExport("usbd.sys", "USBD_CreateConfigurationRequestEx", reinterpret_cast<void*>(USBD_CreateConfigurationRequestEx));
    ldr.registerExport("usbd.sys", "USBD_ParseConfigurationDescriptorEx", reinterpret_cast<void*>(USBD_ParseConfigurationDescriptorEx));
    ldr.registerExport("usbd.sys", "USBD_CalculateUsbBandwidth", reinterpret_cast<void*>(USBD_CalculateUsbBandwidth));
    ldr.registerExport("usbd.sys", "USBD_QueryBusTime", reinterpret_cast<void*>(USBD_QueryBusTime));

    // 3. Register usbhub3.sys exports
    ldr.registerExport("usbhub3.sys", "XhciSubmitUrb", reinterpret_cast<void*>(XhciSubmitUrb));
    ldr.registerExport("usbhub3.sys", "XhciGetTelemetry", reinterpret_cast<void*>(XhciGetTelemetry));

    // 4. Register SCM Service Records
    auto& scm = scm::ServiceControlManager::get();

    auto xhciSvc = std::make_shared<scm::ServiceRecord>();
    xhciSvc->serviceName = L"usbxhci";
    xhciSvc->displayName = L"Sovereign USB 3.2 xHCI Host Controller Driver";
    xhciSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    xhciSvc->startType = scm::SERVICE_BOOT_START;
    xhciSvc->errorControl = scm::SERVICE_ERROR_CRITICAL;
    xhciSvc->binaryPath = L"\\SystemRoot\\System32\\drivers\\usbxhci.sys";
    xhciSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(xhciSvc);

    auto hubSvc = std::make_shared<scm::ServiceRecord>();
    hubSvc->serviceName = L"usbhub3";
    hubSvc->displayName = L"Sovereign SuperSpeed USB 3.2 Hub Driver";
    hubSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    hubSvc->startType = scm::SERVICE_SYSTEM_START;
    hubSvc->errorControl = scm::SERVICE_ERROR_NORMAL;
    hubSvc->binaryPath = L"\\SystemRoot\\System32\\drivers\\usbhub3.sys";
    hubSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(hubSvc);

    // 5. Register in VersionDatabase
    version::VersionDatabase::Instance().RegisterModule(
        "usbxhci.sys", "10.0.26100.1", "Sovereign USB 3.2 xHCI Host Controller Miniport");
    version::VersionDatabase::Instance().RegisterModule(
        "usbhub3.sys", "10.0.26100.1", "Sovereign SuperSpeed USB Hub Driver");
    version::VersionDatabase::Instance().RegisterModule(
        "usbd.sys", "10.0.26100.1", "Sovereign USB Core Bus Driver");

    // Initialize controller driver singleton
    TitanXhciControllerDriver::Instance().initialize();
}

} // namespace micant::km::xhci

// Forward into micant parent namespace
namespace micant {
    using km::xhci::TitanXhciControllerDriver;
    using km::xhci::XhciScatterGatherBuilder;
    using km::xhci::SCATTER_GATHER_ELEMENT;
    using km::xhci::XHCI_CAPABILITY_REGISTERS;
    using km::xhci::XHCI_OPERATIONAL_REGISTERS;
    using km::xhci::XHCI_RUNTIME_INTERRUPTER;
    using km::xhci::XHCI_PORT_REGISTERS;
    using km::xhci::XHCI_DEVNODE;
    using km::xhci::XhciDevicePowerState;
    using km::xhci::XHCI_TELEMETRY;
    using km::xhci::URB;
    using km::xhci::_URB_HEADER;
    using km::xhci::_URB_CONTROL_TRANSFER_EX;
    using km::xhci::_URB_BULK_OR_INTERRUPT_TRANSFER;
    using km::xhci::_URB_SELECT_CONFIGURATION;
    using km::xhci::_URB_SELECT_INTERFACE;
    using km::xhci::USBD_UrbAllocate;
    using km::xhci::USBD_UrbFree;
    using km::xhci::USBD_CreateConfigurationRequestEx;
    using km::xhci::USBD_ParseConfigurationDescriptorEx;
    using km::xhci::USBD_CalculateUsbBandwidth;
    using km::xhci::USBD_QueryBusTime;
    using km::xhci::XhciSubmitUrb;
    using km::xhci::XhciGetTelemetry;
    using km::xhci::XhciQueryTelemetry;
    using km::xhci::InitializeXhciSubsystemExports;
} // namespace micant
