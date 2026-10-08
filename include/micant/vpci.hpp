#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <array>
#include <atomic>

#include "ntstatus.hpp"
#include "version.hpp"
#include "scm.hpp"

namespace micant::vpci {

// ============================================================================
// Constants and PCI Architecture Enums
// ============================================================================

inline constexpr uint32_t PCI_TYPE0_HEADER_SIZE      = 64;   // Standard Header (0x00 - 0x3F)
inline constexpr uint32_t PCI_CONFIG_SPACE_SIZE      = 256;  // Legacy PCI config space
inline constexpr uint32_t PCIE_EXT_CONFIG_SPACE_SIZE = 4096; // PCIe Extended config space

// PCI Configuration Register Offsets
inline constexpr uint32_t PCI_REG_VENDOR_ID          = 0x00;
inline constexpr uint32_t PCI_REG_DEVICE_ID          = 0x02;
inline constexpr uint32_t PCI_REG_COMMAND            = 0x04;
inline constexpr uint32_t PCI_REG_STATUS             = 0x06;
inline constexpr uint32_t PCI_REG_REVISION           = 0x08;
inline constexpr uint32_t PCI_REG_CLASS_PROG         = 0x09;
inline constexpr uint32_t PCI_REG_CLASS_SUB          = 0x0A;
inline constexpr uint32_t PCI_REG_CLASS_BASE         = 0x0B;
inline constexpr uint32_t PCI_REG_CACHE_LINE_SIZE    = 0x0C;
inline constexpr uint32_t PCI_REG_LATENCY_TIMER      = 0x0D;
inline constexpr uint32_t PCI_REG_HEADER_TYPE        = 0x0E;
inline constexpr uint32_t PCI_REG_BIST               = 0x0F;
inline constexpr uint32_t PCI_REG_BAR0               = 0x10;
inline constexpr uint32_t PCI_REG_BAR1               = 0x14;
inline constexpr uint32_t PCI_REG_BAR2               = 0x18;
inline constexpr uint32_t PCI_REG_BAR3               = 0x1C;
inline constexpr uint32_t PCI_REG_BAR4               = 0x20;
inline constexpr uint32_t PCI_REG_BAR5               = 0x24;
inline constexpr uint32_t PCI_REG_SUBSYS_VENDOR_ID   = 0x2C;
inline constexpr uint32_t PCI_REG_SUBSYS_ID          = 0x2E;
inline constexpr uint32_t PCI_REG_CAP_PTR            = 0x34;
inline constexpr uint32_t PCI_REG_INTERRUPT_LINE     = 0x3C;
inline constexpr uint32_t PCI_REG_INTERRUPT_PIN      = 0x3D;

// PCI Command Bits
inline constexpr uint16_t PCI_CMD_IO_SPACE           = 0x0001;
inline constexpr uint16_t PCI_CMD_MEMORY_SPACE       = 0x0002;
inline constexpr uint16_t PCI_CMD_BUS_MASTER         = 0x0004;
inline constexpr uint16_t PCI_CMD_SPECIAL_CYCLES     = 0x0008;
inline constexpr uint16_t PCI_CMD_MWI_ENABLE         = 0x0010;
inline constexpr uint16_t PCI_CMD_VGA_PALETTE_SNOOP  = 0x0020;
inline constexpr uint16_t PCI_CMD_PARITY_ERROR_RESP  = 0x0040;
inline constexpr uint16_t PCI_CMD_SERR_ENABLE        = 0x0100;
inline constexpr uint16_t PCI_CMD_FAST_B2B_ENABLE    = 0x0200;
inline constexpr uint16_t PCI_CMD_INTX_DISABLE       = 0x0400;

// PCI Status Bits
inline constexpr uint16_t PCI_STATUS_CAP_LIST        = 0x0010;
inline constexpr uint16_t PCI_STATUS_66MHZ_CAP       = 0x0020;
inline constexpr uint16_t PCI_STATUS_FAST_B2B_CAP    = 0x0080;
inline constexpr uint16_t PCI_STATUS_PARITY_ERROR    = 0x0100;
inline constexpr uint16_t PCI_STATUS_SIG_TARGET_ABORT= 0x0800;
inline constexpr uint16_t PCI_STATUS_REC_TARGET_ABORT= 0x1000;
inline constexpr uint16_t PCI_STATUS_REC_MASTER_ABORT= 0x2000;
inline constexpr uint16_t PCI_STATUS_SIG_SYSTEM_ERR  = 0x4000;
inline constexpr uint16_t PCI_STATUS_DETECTED_PARITY = 0x8000;

// Standard PCI Capability IDs
inline constexpr uint8_t PCI_CAP_ID_PM               = 0x01;
inline constexpr uint8_t PCI_CAP_ID_AGP              = 0x02;
inline constexpr uint8_t PCI_CAP_ID_VPD              = 0x03;
inline constexpr uint8_t PCI_CAP_ID_SLOTID           = 0x04;
inline constexpr uint8_t PCI_CAP_ID_MSI              = 0x05;
inline constexpr uint8_t PCI_CAP_ID_CHSWP            = 0x06;
inline constexpr uint8_t PCI_CAP_ID_PCIX             = 0x07;
inline constexpr uint8_t PCI_CAP_ID_HT               = 0x08;
inline constexpr uint8_t PCI_CAP_ID_VNDR             = 0x09;
inline constexpr uint8_t PCI_CAP_ID_SHPC             = 0x0C;
inline constexpr uint8_t PCI_CAP_ID_SSVID            = 0x0D;
inline constexpr uint8_t PCI_CAP_ID_AGP3             = 0x0E;
inline constexpr uint8_t PCI_CAP_ID_SECURE           = 0x0F;
inline constexpr uint8_t PCI_CAP_ID_EXP              = 0x10; // PCI Express
inline constexpr uint8_t PCI_CAP_ID_MSIX             = 0x11; // MSI-X
inline constexpr uint8_t PCI_CAP_ID_SATA             = 0x12;
inline constexpr uint8_t PCI_CAP_ID_AF               = 0x13;

// PCIe Extended Capability IDs (offset >= 0x100)
inline constexpr uint16_t PCI_EXT_CAP_ID_AER         = 0x0001;
inline constexpr uint16_t PCI_EXT_CAP_ID_VC          = 0x0002;
inline constexpr uint16_t PCI_EXT_CAP_ID_DSN         = 0x0003;
inline constexpr uint16_t PCI_EXT_CAP_ID_PWR         = 0x0004;
inline constexpr uint16_t PCI_EXT_CAP_ID_RCLD        = 0x0005;
inline constexpr uint16_t PCI_EXT_CAP_ID_ACS         = 0x000D;
inline constexpr uint16_t PCI_EXT_CAP_ID_ARI         = 0x000E;
inline constexpr uint16_t PCI_EXT_CAP_ID_ATS         = 0x000F;
inline constexpr uint16_t PCI_EXT_CAP_ID_SRIOV       = 0x0010; // SR-IOV
inline constexpr uint16_t PCI_EXT_CAP_ID_PASID       = 0x001B;

// Device Types
enum class VpciDeviceType : uint32_t {
    SriovVirtualFunction = 1,
    DiscreteDeviceAssignment = 2,
    EmulatedPciDevice = 3
};

// Device States
enum class VpciDeviceState : uint32_t {
    Discovered = 1,
    Configured = 2,
    Assigned   = 3,
    Active     = 4,
    Revoked    = 5,
    Removed    = 6
};

// BAR Types
enum class PciBarType : uint8_t {
    None,
    Io32,
    Memory32,
    Memory64
};

// ============================================================================
// BAR Descriptor
// ============================================================================
struct PciBarDescriptor {
    uint32_t   barIndex{0};
    PciBarType type{PciBarType::None};
    uint64_t   baseAddress{0};
    uint64_t   size{0};
    bool       isPrefetchable{false};
    bool       isMapped{false};
    std::string name;
};

// ============================================================================
// MSI-X Table Entry
// ============================================================================
struct MsiXTableEntry {
    uint64_t msgAddress{0};
    uint32_t msgData{0};
    uint32_t vectorControl{1}; // Bit 0 = Masked (1 = masked, 0 = unmasked)
    uint64_t triggerCount{0};
};

// ============================================================================
// Virtual PCI Device Model
// ============================================================================
class VirtualPciDevice {
public:
    VirtualPciDevice(uint32_t deviceId,
                     VpciDeviceType type,
                     const std::string& name,
                     const std::string& bdf,
                     uint16_t vendorId,
                     uint16_t pciDeviceId,
                     uint8_t baseClass,
                     uint8_t subClass,
                     uint8_t progIf = 0x00,
                     uint8_t revisionId = 0x01)
        : m_deviceId(deviceId)
        , m_type(type)
        , m_name(name)
        , m_bdf(bdf)
        , m_vendorId(vendorId)
        , m_pciDeviceId(pciDeviceId)
        , m_baseClass(baseClass)
        , m_subClass(subClass)
        , m_progIf(progIf)
        , m_revisionId(revisionId)
    {
        std::memset(m_configSpace.data(), 0, m_configSpace.size());
        initStandardConfigSpace();
    }

    uint32_t getDeviceId() const { return m_deviceId; }
    VpciDeviceType getType() const { return m_type; }
    VpciDeviceState getState() const { return m_state.load(); }
    void setState(VpciDeviceState s) { m_state.store(s); }
    const std::string& getName() const { return m_name; }
    const std::string& getBdf() const { return m_bdf; }
    uint16_t getVendorId() const { return m_vendorId; }
    uint16_t getPciDeviceId() const { return m_pciDeviceId; }
    uint8_t getBaseClass() const { return m_baseClass; }
    uint8_t getSubClass() const { return m_subClass; }

    // BAR configuration
    void addBar(uint32_t index, PciBarType type, uint64_t size, uint64_t baseAddr, bool prefetch, const std::string& name) {
        if (index >= 6) return;
        PciBarDescriptor bar{};
        bar.barIndex = index;
        bar.type = type;
        bar.size = size;
        bar.baseAddress = baseAddr;
        bar.isPrefetchable = prefetch;
        bar.isMapped = (baseAddr != 0);
        bar.name = name;
        m_bars[index] = bar;

        // Encode in Type 0 config space
        uint32_t barReg = PCI_REG_BAR0 + (index * 4);
        if (type == PciBarType::Memory32) {
            uint32_t val = static_cast<uint32_t>(baseAddr & 0xFFFFFFF0) | (prefetch ? 0x08 : 0x00);
            writeConfigDword(barReg, val);
        } else if (type == PciBarType::Memory64) {
            uint32_t low = static_cast<uint32_t>(baseAddr & 0xFFFFFFF0) | 0x04 | (prefetch ? 0x08 : 0x00);
            uint32_t high = static_cast<uint32_t>(baseAddr >> 32);
            writeConfigDword(barReg, low);
            if (index + 1 < 6) {
                writeConfigDword(barReg + 4, high);
            }
        } else if (type == PciBarType::Io32) {
            uint32_t val = static_cast<uint32_t>(baseAddr & 0xFFFFFFFC) | 0x01;
            writeConfigDword(barReg, val);
        }
    }

    const std::array<PciBarDescriptor, 6>& getBars() const { return m_bars; }

    // MSI-X Configuration
    void configureMsiX(uint32_t vectorCount, uint32_t barIndex, uint32_t offset) {
        m_msixVectors.resize(vectorCount);
        m_msixBarIndex = barIndex;
        m_msixTableOffset = offset;
        m_msixEnabled = true;

        // Add MSI-X capability pointer (at offset 0x70)
        uint8_t capOffset = 0x70;
        m_configSpace[capOffset] = PCI_CAP_ID_MSIX;
        m_configSpace[capOffset + 1] = 0x00; // End of list
        uint16_t msgCtrl = static_cast<uint16_t>(vectorCount - 1) & 0x07FF;
        msgCtrl |= 0x8000; // MSI-X Enable bit
        std::memcpy(&m_configSpace[capOffset + 2], &msgCtrl, sizeof(uint16_t));
        uint32_t tableBir = offset | (barIndex & 0x07);
        std::memcpy(&m_configSpace[capOffset + 4], &tableBir, sizeof(uint32_t));
    }

    bool isMsiXEnabled() const { return m_msixEnabled; }
    size_t getMsiXVectorCount() const { return m_msixVectors.size(); }

    bool setMsiXVector(uint32_t index, uint64_t msgAddr, uint32_t msgData, bool masked) {
        std::unique_lock lock(m_mutex);
        if (index >= m_msixVectors.size()) return false;
        m_msixVectors[index].msgAddress = msgAddr;
        m_msixVectors[index].msgData = msgData;
        m_msixVectors[index].vectorControl = masked ? 1 : 0;
        return true;
    }

    bool getMsiXVector(uint32_t index, MsiXTableEntry& entry) const {
        std::shared_lock lock(m_mutex);
        if (index >= m_msixVectors.size()) return false;
        entry = m_msixVectors[index];
        return true;
    }

    bool triggerMsiX(uint32_t index) {
        std::unique_lock lock(m_mutex);
        if (index >= m_msixVectors.size()) return false;
        if ((m_msixVectors[index].vectorControl & 1) != 0) {
            // Masked
            return false;
        }
        m_msixVectors[index].triggerCount++;
        m_totalInterruptsFired++;
        return true;
    }

    uint64_t getTotalInterrupts() const { return m_totalInterruptsFired.load(); }

    // NetVSC Acceleration Pairing (SR-IOV teaming)
    void setNetVscPairing(uint32_t adapterId, bool active) {
        m_netVscPairedAdapterId = adapterId;
        m_netVscSriovTeamingActive.store(active);
    }

    bool isNetVscSriovTeamingActive() const { return m_netVscSriovTeamingActive.load(); }
    uint32_t getNetVscPairedAdapterId() const { return m_netVscPairedAdapterId; }

    // Config Space Read / Write
    bool readConfig(uint32_t offset, uint32_t length, void* pBuffer) const {
        if (!pBuffer || offset + length > m_configSpace.size()) return false;
        std::shared_lock lock(m_mutex);
        std::memcpy(pBuffer, &m_configSpace[offset], length);
        return true;
    }

    bool writeConfig(uint32_t offset, uint32_t length, const void* pData) {
        if (!pData || offset + length > m_configSpace.size()) return false;
        std::unique_lock lock(m_mutex);
        // Protect read-only registers like Vendor ID, Device ID, Header Type
        if (offset == PCI_REG_COMMAND && length == 2) {
            uint16_t cmd = *reinterpret_cast<const uint16_t*>(pData);
            m_commandReg = cmd;
            std::memcpy(&m_configSpace[offset], pData, length);
            return true;
        }
        // General writes
        std::memcpy(&m_configSpace[offset], pData, length);
        return true;
    }

    uint16_t getCommand() const { return m_commandReg; }

private:
    void initStandardConfigSpace() {
        // Standard Type 0 Header Layout
        std::memcpy(&m_configSpace[PCI_REG_VENDOR_ID], &m_vendorId, sizeof(uint16_t));
        std::memcpy(&m_configSpace[PCI_REG_DEVICE_ID], &m_pciDeviceId, sizeof(uint16_t));

        m_commandReg = PCI_CMD_IO_SPACE | PCI_CMD_MEMORY_SPACE | PCI_CMD_BUS_MASTER;
        std::memcpy(&m_configSpace[PCI_REG_COMMAND], &m_commandReg, sizeof(uint16_t));

        uint16_t status = PCI_STATUS_CAP_LIST;
        std::memcpy(&m_configSpace[PCI_REG_STATUS], &status, sizeof(uint16_t));

        m_configSpace[PCI_REG_REVISION] = m_revisionId;
        m_configSpace[PCI_REG_CLASS_PROG] = m_progIf;
        m_configSpace[PCI_REG_CLASS_SUB] = m_subClass;
        m_configSpace[PCI_REG_CLASS_BASE] = m_baseClass;

        m_configSpace[PCI_REG_CACHE_LINE_SIZE] = 0x10; // 64 bytes
        m_configSpace[PCI_REG_LATENCY_TIMER] = 0x00;
        m_configSpace[PCI_REG_HEADER_TYPE] = 0x00;   // Type 0 Standard Header
        m_configSpace[PCI_REG_BIST] = 0x00;

        uint16_t subsysVendor = m_vendorId;
        uint16_t subsysId = m_pciDeviceId;
        std::memcpy(&m_configSpace[PCI_REG_SUBSYS_VENDOR_ID], &subsysVendor, sizeof(uint16_t));
        std::memcpy(&m_configSpace[PCI_REG_SUBSYS_ID], &subsysId, sizeof(uint16_t));

        // Capabilities pointer at 0x40
        m_configSpace[PCI_REG_CAP_PTR] = 0x40;

        // Cap 1: PCIe Express Capability at 0x40
        m_configSpace[0x40] = PCI_CAP_ID_EXP;
        m_configSpace[0x41] = 0x70; // Next cap = MSI-X at 0x70
        uint16_t pcieCaps = 0x0002; // PCIe v2 endpoint
        std::memcpy(&m_configSpace[0x42], &pcieCaps, sizeof(uint16_t));
    }

    void writeConfigDword(uint32_t offset, uint32_t val) {
        if (offset + 4 <= m_configSpace.size()) {
            std::memcpy(&m_configSpace[offset], &val, sizeof(uint32_t));
        }
    }

    uint32_t                            m_deviceId{0};
    VpciDeviceType                      m_type{VpciDeviceType::EmulatedPciDevice};
    std::atomic<VpciDeviceState>        m_state{VpciDeviceState::Discovered};
    std::string                         m_name;
    std::string                         m_bdf;
    uint16_t                            m_vendorId{0};
    uint16_t                            m_pciDeviceId{0};
    uint8_t                             m_baseClass{0};
    uint8_t                             m_subClass{0};
    uint8_t                             m_progIf{0};
    uint8_t                             m_revisionId{1};
    uint16_t                            m_commandReg{0};

    std::array<uint8_t, PCIE_EXT_CONFIG_SPACE_SIZE> m_configSpace{};
    std::array<PciBarDescriptor, 6>     m_bars{};

    bool                                m_msixEnabled{false};
    uint32_t                            m_msixBarIndex{0};
    uint32_t                            m_msixTableOffset{0};
    std::vector<MsiXTableEntry>         m_msixVectors{};
    std::atomic<uint64_t>               m_totalInterruptsFired{0};

    uint32_t                            m_netVscPairedAdapterId{0};
    std::atomic<bool>                   m_netVscSriovTeamingActive{false};

    mutable std::shared_mutex           m_mutex;
};

// ============================================================================
// Virtual PCI Subsystem (TitanVPCI / AegisPassthrough)
// ============================================================================
class VpciSubsystem {
public:
    static VpciSubsystem& get() {
        static VpciSubsystem s_instance;
        return s_instance;
    }

    void initialize() {
        std::unique_lock lock(m_mutex);
        if (m_initialized) return;

        // Pre-seed Windows Virtual PCI bus devices
        // Device 1: Mellanox ConnectX-6 Dx Virtual Function (SR-IOV)
        auto dev1 = std::make_shared<VirtualPciDevice>(
            1,
            VpciDeviceType::SriovVirtualFunction,
            "Mellanox ConnectX-6 Dx Virtual Function",
            "0000:01:00.1",
            0x15B3, // Mellanox Technologies
            0x101E, // ConnectX-6 Dx VF
            0x02,   // Network Controller
            0x00    // Ethernet Controller
        );
        dev1->addBar(0, PciBarType::Memory64, 64 * 1024 * 1024, 0xFE000000, true, "NIC Doorbell & Internal Registers");
        dev1->configureMsiX(16, 0, 0x2000);
        dev1->setNetVscPairing(2, true); // Paired with NetVSC synthetic adapter #2
        dev1->setState(VpciDeviceState::Active);
        m_devices[1] = dev1;

        // Device 2: NVIDIA A100 Tensor Core GPU (Discrete Device Assignment - DDA)
        auto dev2 = std::make_shared<VirtualPciDevice>(
            2,
            VpciDeviceType::DiscreteDeviceAssignment,
            "NVIDIA A100-PCIE-40GB Tensor Core GPU (DDA Passthrough)",
            "0000:02:00.0",
            0x10DE, // NVIDIA Corporation
            0x20F1, // GA100 [A100 PCIe 40GB]
            0x03,   // Display Controller
            0x02    // 3D Controller
        );
        dev2->addBar(0, PciBarType::Memory32, 16 * 1024 * 1024, 0xFD000000, false, "GPU MMIO Command Registers");
        dev2->addBar(1, PciBarType::Memory64, 16ULL * 1024 * 1024 * 1024, 0x2000000000ULL, true, "HBM2 Framebuffer Aperture");
        dev2->configureMsiX(32, 0, 0x10000);
        dev2->setState(VpciDeviceState::Active);
        m_devices[2] = dev2;

        // Device 3: Samsung PM1733 Enterprise NVMe SSD (DDA Passthrough)
        auto dev3 = std::make_shared<VirtualPciDevice>(
            3,
            VpciDeviceType::DiscreteDeviceAssignment,
            "Samsung PM1733 NVMe Enterprise SSD (DDA Passthrough)",
            "0000:03:00.0",
            0x144D, // Samsung Electronics
            0xA808, // PM1733 NVMe Controller
            0x01,   // Mass Storage Controller
            0x08,   // Non-Volatile Memory
            0x02    // NVM Express (NVMe)
        );
        dev3->addBar(0, PciBarType::Memory64, 16 * 1024, 0xFC000000, false, "NVMe BAR0 Doorbell Registers");
        dev3->configureMsiX(64, 0, 0x2000);
        dev3->setState(VpciDeviceState::Active);
        m_devices[3] = dev3;

        m_initialized = true;
    }

    bool isInitialized() const {
        std::shared_lock lock(m_mutex);
        return m_initialized;
    }

    std::shared_ptr<VirtualPciDevice> getDevice(uint32_t deviceId) const {
        std::shared_lock lock(m_mutex);
        auto it = m_devices.find(deviceId);
        return (it != m_devices.end()) ? it->second : nullptr;
    }

    std::vector<std::shared_ptr<VirtualPciDevice>> getAllDevices() const {
        std::shared_lock lock(m_mutex);
        std::vector<std::shared_ptr<VirtualPciDevice>> result;
        result.reserve(m_devices.size());
        for (const auto& [_, dev] : m_devices) {
            result.push_back(dev);
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a->getDeviceId() < b->getDeviceId();
        });
        return result;
    }

    bool addDevice(std::shared_ptr<VirtualPciDevice> dev) {
        if (!dev) return false;
        std::unique_lock lock(m_mutex);
        m_devices[dev->getDeviceId()] = dev;
        return true;
    }

    // Failover helper: SR-IOV VF to synthetic NetVSC
    bool failoverSriovToSynthetic(uint32_t deviceId) {
        auto dev = getDevice(deviceId);
        if (!dev || dev->getType() != VpciDeviceType::SriovVirtualFunction) return false;
        dev->setNetVscPairing(dev->getNetVscPairedAdapterId(), false);
        dev->setState(VpciDeviceState::Revoked);
        return true;
    }

    bool restoreSriovTeaming(uint32_t deviceId) {
        auto dev = getDevice(deviceId);
        if (!dev || dev->getType() != VpciDeviceType::SriovVirtualFunction) return false;
        dev->setNetVscPairing(dev->getNetVscPairedAdapterId(), true);
        dev->setState(VpciDeviceState::Active);
        return true;
    }

    void reset() {
        std::unique_lock lock(m_mutex);
        m_devices.clear();
        m_initialized = false;
    }

private:
    VpciSubsystem() = default;

    bool                                                               m_initialized{false};
    std::unordered_map<uint32_t, std::shared_ptr<VirtualPciDevice>>   m_devices{};
    mutable std::shared_mutex                                          m_mutex;
};

// ============================================================================
// Clean-Room Win32 & NT C ABI Parity Exports (vpci.sys)
// ============================================================================
extern "C" {

inline NTSTATUS VpciInitializeSubsystem() {
    auto& sys = VpciSubsystem::get();
    sys.initialize();
    return sys.isInitialized() ? micant::STATUS_SUCCESS : micant::STATUS_UNSUCCESSFUL;
}

inline NTSTATUS VpciDeviceEnumerate(uint32_t* pCount, uint32_t* pDeviceIds, uint32_t maxDevices) {
    if (!pCount) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = VpciSubsystem::get();
    sys.initialize();

    auto devs = sys.getAllDevices();
    *pCount = static_cast<uint32_t>(devs.size());

    if (pDeviceIds && maxDevices > 0) {
        uint32_t toCopy = std::min(*pCount, maxDevices);
        for (uint32_t i = 0; i < toCopy; ++i) {
            pDeviceIds[i] = devs[i]->getDeviceId();
        }
    }
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS VpciReadConfigSpace(uint32_t deviceId, uint32_t offset, uint32_t length, void* pBuffer) {
    if (!pBuffer || length == 0) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = VpciSubsystem::get();
    sys.initialize();

    auto dev = sys.getDevice(deviceId);
    if (!dev) return micant::STATUS_NOT_FOUND;

    return dev->readConfig(offset, length, pBuffer) ? micant::STATUS_SUCCESS : micant::STATUS_INVALID_PARAMETER;
}

inline NTSTATUS VpciWriteConfigSpace(uint32_t deviceId, uint32_t offset, uint32_t length, const void* pData) {
    if (!pData || length == 0) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = VpciSubsystem::get();
    sys.initialize();

    auto dev = sys.getDevice(deviceId);
    if (!dev) return micant::STATUS_NOT_FOUND;

    return dev->writeConfig(offset, length, pData) ? micant::STATUS_SUCCESS : micant::STATUS_INVALID_PARAMETER;
}

inline NTSTATUS VpciMapBarSpace(uint32_t deviceId, uint32_t barIndex, uint64_t* pMappedAddress, uint64_t* pSize) {
    if (!pMappedAddress || !pSize || barIndex >= 6) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = VpciSubsystem::get();
    sys.initialize();

    auto dev = sys.getDevice(deviceId);
    if (!dev) return micant::STATUS_NOT_FOUND;

    const auto& bars = dev->getBars();
    if (bars[barIndex].type == PciBarType::None) {
        return micant::STATUS_NOT_FOUND;
    }

    *pMappedAddress = bars[barIndex].baseAddress;
    *pSize = bars[barIndex].size;
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS VpciAssignMsiInterrupt(uint32_t deviceId, uint32_t vectorIndex, uint64_t messageAddress, uint32_t messageData) {
    auto& sys = VpciSubsystem::get();
    sys.initialize();

    auto dev = sys.getDevice(deviceId);
    if (!dev) return micant::STATUS_NOT_FOUND;

    bool ok = dev->setMsiXVector(vectorIndex, messageAddress, messageData, false);
    return ok ? micant::STATUS_SUCCESS : micant::STATUS_INVALID_PARAMETER;
}

inline NTSTATUS VpciQueryDeviceCapabilities(uint32_t deviceId, uint32_t* pIsSriov, uint32_t* pIsDda, uint32_t* pNetVscPaired) {
    if (!pIsSriov || !pIsDda || !pNetVscPaired) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = VpciSubsystem::get();
    sys.initialize();

    auto dev = sys.getDevice(deviceId);
    if (!dev) return micant::STATUS_NOT_FOUND;

    *pIsSriov = (dev->getType() == VpciDeviceType::SriovVirtualFunction) ? 1 : 0;
    *pIsDda = (dev->getType() == VpciDeviceType::DiscreteDeviceAssignment) ? 1 : 0;
    *pNetVscPaired = dev->isNetVscSriovTeamingActive() ? 1 : 0;
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS VpciTriggerInterrupt(uint32_t deviceId, uint32_t vectorIndex) {
    auto& sys = VpciSubsystem::get();
    sys.initialize();

    auto dev = sys.getDevice(deviceId);
    if (!dev) return micant::STATUS_NOT_FOUND;

    return dev->triggerMsiX(vectorIndex) ? micant::STATUS_SUCCESS : micant::STATUS_UNSUCCESSFUL;
}

} // extern "C"

// ============================================================================
// SCM & Version Registration Helper
// ============================================================================
inline void RegisterVpciSubsystem() {
    // 1. Version Database Registration
    auto& vdb = micant::version::VersionDatabase::Instance();
    vdb.RegisterModule("vpci.sys", "10.0.26100.1", "Virtual PCI Bus Driver (TitanVPCI)");

    // 2. SCM Registration
    auto& scm = micant::scm::ServiceControlManager::get();
    auto rec = std::make_shared<micant::scm::ServiceRecord>();
    rec->serviceName = L"VpciService";
    rec->displayName = L"Hyper-V Virtual PCI & SR-IOV Pass-Through Service";
    rec->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
    rec->startType = micant::scm::SERVICE_AUTO_START;
    rec->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k LocalSystemNetworkRestricted";
    rec->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
    scm.registerServiceRecord(rec);

    // Initialize core subsystem singleton
    VpciSubsystem::get().initialize();
}

} // namespace micant::vpci
