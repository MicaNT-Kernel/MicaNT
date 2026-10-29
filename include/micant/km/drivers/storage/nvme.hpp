// ============================================================================
// MicaNT: Sovereign NVM Express (NVMe 1.0-2.0) & Universal Flash Storage Stack
// File: include/micant/nvme.hpp
// Sovereign System Domain: TitanNVMe / EmeraldNVMe / TitanFlash
//
// Description:
//   Clean-room implementation of the NVM Express (NVMe 1.0e through 2.0d)
//   Controller Architecture, Queuing Engine, Multi-Namespace Flash Storage,
//   Universal Flash Storage (UFS 3.1/4.0), eMMC 5.1 / SDHCI 4.2, Serial ATA
//   AHCI 1.3.1 with Native Command Queuing (NCQ), Windows StorPort Miniport
//   Driver architecture (stornvme.sys, storahci.sys, storufs.sys), and block
//   device filesystem interoperability.
//
// Clean-Room Engineering Reference & Standards:
//   - NVM Express Organization: NVM Express Base Specification Revision 2.0d (2024)
//   - NVM Express Organization: NVM Express Base Specification Revision 1.4b (2020)
//   - NVM Express Organization: NVM Express Base Specification Revision 1.3d (2019)
//   - JEDEC Solid State Technology Association: Universal Flash Storage (JESD220E/F)
//   - JEDEC Solid State Technology Association: Embedded MultiMediaCard (JESD84-B51)
//   - Serial ATA International Organization: Serial ATA AHCI Specification 1.3.1
//   - Microsoft Open win32metadata repository: Windows.Win32.Storage.Nvme
//   - ISO/IEC 14882:2023 C++ Standard
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <queue>
#include <mutex>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/native/janusldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"
#include "micant/km/drivers/storage/storage.hpp"
#include "micant/km/hal/pci.hpp"

namespace micant::nvme {

using micant::BOOL;
using micant::TRUE;
using micant::FALSE;
using micant::ULONG;

// ============================================================================
// 1. NVMe Multi-Generation Specification Versions & Constants
// ============================================================================

enum class NvmeVersion : uint32_t {
    Version_1_0 = 0x00010000, // NVMe 1.0e (2012)
    Version_1_1 = 0x00010100, // NVMe 1.1b (2014)
    Version_1_2 = 0x00010200, // NVMe 1.2.1 (2016)
    Version_1_3 = 0x00010300, // NVMe 1.3d (2019)
    Version_1_4 = 0x00010400, // NVMe 1.4b (2020)
    Version_2_0 = 0x00020000  // NVMe 2.0d (2024)
};

inline std::string NvmeVersionToString(NvmeVersion v) {
    switch (v) {
        case NvmeVersion::Version_1_0: return "NVMe 1.0e";
        case NvmeVersion::Version_1_1: return "NVMe 1.1b";
        case NvmeVersion::Version_1_2: return "NVMe 1.2.1";
        case NvmeVersion::Version_1_3: return "NVMe 1.3d";
        case NvmeVersion::Version_1_4: return "NVMe 1.4b";
        case NvmeVersion::Version_2_0: return "NVMe 2.0d";
        default: return "NVMe Unknown";
    }
}

// Controller Register Offsets (BAR0 MMIO)
inline constexpr uint32_t NVME_REG_CAP      = 0x0000; // Controller Capabilities (64-bit)
inline constexpr uint32_t NVME_REG_VS       = 0x0008; // Version (32-bit)
inline constexpr uint32_t NVME_REG_INTMS    = 0x000C; // Interrupt Mask Set (32-bit)
inline constexpr uint32_t NVME_REG_INTMC    = 0x0010; // Interrupt Mask Clear (32-bit)
inline constexpr uint32_t NVME_REG_CC       = 0x0014; // Controller Configuration (32-bit)
inline constexpr uint32_t NVME_REG_CSTS     = 0x001C; // Controller Status (32-bit)
inline constexpr uint32_t NVME_REG_NSSR     = 0x0020; // NVM Subsystem Reset (32-bit)
inline constexpr uint32_t NVME_REG_AQA      = 0x0024; // Admin Queue Attributes (32-bit)
inline constexpr uint32_t NVME_REG_ASQ      = 0x0028; // Admin Submission Queue Base (64-bit)
inline constexpr uint32_t NVME_REG_ACQ      = 0x0030; // Admin Completion Queue Base (64-bit)
inline constexpr uint32_t NVME_REG_CMBLOC   = 0x0038; // Controller Memory Buffer Location
inline constexpr uint32_t NVME_REG_CMBSZ    = 0x003C; // Controller Memory Buffer Size
inline constexpr uint32_t NVME_REG_BPINFO   = 0x0040; // Boot Partition Information
inline constexpr uint32_t NVME_REG_BPRSEL   = 0x0044; // Boot Partition Read Select
inline constexpr uint32_t NVME_REG_BPMBL    = 0x0048; // Boot Partition Memory Buffer Base
inline constexpr uint32_t NVME_REG_DBS      = 0x1000; // Doorbell Registers start

// Controller Configuration (CC) Bits
inline constexpr uint32_t NVME_CC_EN        = 0x00000001; // Enable
inline constexpr uint32_t NVME_CC_CSS_NVM   = 0x00000000; // NVM Command Set
inline constexpr uint32_t NVME_CC_MPS_4K    = 0x00000000; // 4KB Page Size
inline constexpr uint32_t NVME_CC_AMS_RR    = 0x00000000; // Round Robin Arbitration
inline constexpr uint32_t NVME_CC_SHN_NONE  = 0x00000000; // No shutdown
inline constexpr uint32_t NVME_CC_SHN_NORM  = 0x00004000; // Normal shutdown
inline constexpr uint32_t NVME_CC_IOSQES_64 = 0x00060000; // 64 bytes SQE (2^6)
inline constexpr uint32_t NVME_CC_IOCQES_16 = 0x00400000; // 16 bytes CQE (2^4)

// Controller Status (CSTS) Bits
inline constexpr uint32_t NVME_CSTS_RDY     = 0x00000001; // Ready
inline constexpr uint32_t NVME_CSTS_CFS     = 0x00000002; // Controller Fatal Status
inline constexpr uint32_t NVME_CSTS_SHST_OK = 0x00000008; // Shutdown complete

// Admin Command Opcodes
inline constexpr uint8_t NVME_ADMIN_DELETE_IO_SQ   = 0x00;
inline constexpr uint8_t NVME_ADMIN_CREATE_IO_SQ   = 0x01;
inline constexpr uint8_t NVME_ADMIN_GET_LOG_PAGE   = 0x02;
inline constexpr uint8_t NVME_ADMIN_DELETE_IO_CQ   = 0x04;
inline constexpr uint8_t NVME_ADMIN_CREATE_IO_CQ   = 0x05;
inline constexpr uint8_t NVME_ADMIN_IDENTIFY       = 0x06;
inline constexpr uint8_t NVME_ADMIN_ABORT          = 0x08;
inline constexpr uint8_t NVME_ADMIN_SET_FEATURES   = 0x09;
inline constexpr uint8_t NVME_ADMIN_GET_FEATURES   = 0x0A;
inline constexpr uint8_t NVME_ADMIN_ASYNC_EVENT    = 0x0C;
inline constexpr uint8_t NVME_ADMIN_NS_MANAGEMENT  = 0x0D;
inline constexpr uint8_t NVME_ADMIN_FW_COMMIT      = 0x10;
inline constexpr uint8_t NVME_ADMIN_FW_DOWNLOAD    = 0x11;
inline constexpr uint8_t NVME_ADMIN_FORMAT_NVM     = 0x80;
inline constexpr uint8_t NVME_ADMIN_SECURITY_SEND  = 0x81;
inline constexpr uint8_t NVME_ADMIN_SECURITY_RECV  = 0x82;
inline constexpr uint8_t NVME_ADMIN_SANITIZE       = 0x84;

// NVM I/O Command Opcodes
inline constexpr uint8_t NVME_NVM_FLUSH            = 0x00;
inline constexpr uint8_t NVME_NVM_WRITE            = 0x01;
inline constexpr uint8_t NVME_NVM_READ             = 0x02;
inline constexpr uint8_t NVME_NVM_WRITE_UNCORR     = 0x04;
inline constexpr uint8_t NVME_NVM_COMPARE          = 0x05;
inline constexpr uint8_t NVME_NVM_WRITE_ZEROES     = 0x08;
inline constexpr uint8_t NVME_NVM_DATASET_MGMT     = 0x09; // TRIM / Deallocate
inline constexpr uint8_t NVME_NVM_VERIFY           = 0x0C;

// Identify CNS (Controller or Namespace Structure)
inline constexpr uint8_t NVME_IDENTIFY_CNS_NS       = 0x00; // Identify Namespace
inline constexpr uint8_t NVME_IDENTIFY_CNS_CTRL     = 0x01; // Identify Controller
inline constexpr uint8_t NVME_IDENTIFY_CNS_ACTIVE_NS= 0x02; // Active NSID list

// Log Page Identifiers
inline constexpr uint8_t NVME_LOG_PAGE_ERROR_INFO   = 0x01;
inline constexpr uint8_t NVME_LOG_PAGE_SMART_HEALTH = 0x02;
inline constexpr uint8_t NVME_LOG_PAGE_FW_SLOT      = 0x03;

// Status Code Types & Status Codes
inline constexpr uint16_t NVME_SC_SUCCESS                   = 0x0000;
inline constexpr uint16_t NVME_SC_INVALID_OPCODE            = 0x0001;
inline constexpr uint16_t NVME_SC_INVALID_FIELD             = 0x0002;
inline constexpr uint16_t NVME_SC_COMMAND_ID_CONFLICT       = 0x0003;
inline constexpr uint16_t NVME_SC_DATA_TRANSFER_ERROR       = 0x0004;
inline constexpr uint16_t NVME_SC_INTERNAL_DEVICE_ERROR     = 0x0006;
inline constexpr uint16_t NVME_SC_ABORTED_BY_REQUEST        = 0x0007;
inline constexpr uint16_t NVME_SC_LBA_OUT_OF_RANGE          = 0x0080;
inline constexpr uint16_t NVME_SC_CAPACITY_EXCEEDED         = 0x0081;
inline constexpr uint16_t NVME_SC_NAMESPACE_NOT_READY       = 0x0082;

// NVMe 1.4/2.0 SGL (Scatter-Gather List) Descriptors & Flags
inline constexpr uint8_t NVME_SGL_DESCRIPTOR_TYPE_DATA_BLOCK = 0x00;
inline constexpr uint8_t NVME_SGL_DESCRIPTOR_TYPE_BIT_BUCKET = 0x01;
inline constexpr uint8_t NVME_SGL_DESCRIPTOR_TYPE_SEGMENT    = 0x02;
inline constexpr uint8_t NVME_SGL_DESCRIPTOR_TYPE_LAST_SEG   = 0x03;
inline constexpr uint8_t NVME_SGL_SUBTYPE_ADDRESS            = 0x00;
inline constexpr uint8_t NVME_SGL_SUBTYPE_OFFSET             = 0x01;

// NVMe 2.0 Command Set Identifier (CSI)
inline constexpr uint8_t NVME_CSI_NVM                        = 0x00;
inline constexpr uint8_t NVME_CSI_KV                         = 0x01;
inline constexpr uint8_t NVME_CSI_ZNS                        = 0x02;

// NVMe 2.0 ZNS (Zoned Namespaces) Opcodes & Actions
inline constexpr uint8_t NVME_ZNS_ZONE_MANAGEMENT_SEND       = 0x79;
inline constexpr uint8_t NVME_ZNS_ZONE_MANAGEMENT_RECEIVE    = 0x7A;
inline constexpr uint8_t NVME_ZNS_ZONE_APPEND                = 0x7D;

inline constexpr uint8_t NVME_ZNS_ACTION_CLOSE               = 0x01;
inline constexpr uint8_t NVME_ZNS_ACTION_FINISH              = 0x02;
inline constexpr uint8_t NVME_ZNS_ACTION_OPEN                = 0x03;
inline constexpr uint8_t NVME_ZNS_ACTION_RESET               = 0x04;

inline constexpr uint8_t NVME_ZONE_STATE_EMPTY               = 0x01;
inline constexpr uint8_t NVME_ZONE_STATE_IMP_OPEN            = 0x02;
inline constexpr uint8_t NVME_ZONE_STATE_EXP_OPEN            = 0x03;
inline constexpr uint8_t NVME_ZONE_STATE_CLOSED              = 0x04;
inline constexpr uint8_t NVME_ZONE_STATE_READONLY            = 0x0D;
inline constexpr uint8_t NVME_ZONE_STATE_FULL                = 0x0E;
inline constexpr uint8_t NVME_ZONE_STATE_OFFLINE             = 0x0F;

// End-to-End Data Protection (T10 DIF/DIX) Status Codes
inline constexpr uint16_t NVME_SC_GUARD_CHECK_ERROR          = 0x0280;
inline constexpr uint16_t NVME_SC_APP_TAG_CHECK_ERROR        = 0x0281;
inline constexpr uint16_t NVME_SC_REF_TAG_CHECK_ERROR        = 0x0282;

// Asynchronous Event Request (AER) Constants
inline constexpr uint8_t NVME_AER_TYPE_ERROR                 = 0x00;
inline constexpr uint8_t NVME_AER_TYPE_SMART                 = 0x01;
inline constexpr uint8_t NVME_AER_TYPE_NOTICE                = 0x02;
inline constexpr uint8_t NVME_AER_SMART_TEMP_EXCEEDED        = 0x01;
inline constexpr uint8_t NVME_AER_SMART_SPARE_BELOW_THRESH   = 0x02;
inline constexpr uint8_t NVME_AER_SMART_RELIABILITY_DEGRADED = 0x03;
inline constexpr uint8_t NVME_AER_SMART_READ_ONLY            = 0x04;

// Standard Windows NT Storage IOCTL Codes (BusTypeNvme)
inline constexpr uint32_t IOCTL_STORAGE_QUERY_PROPERTY_CODE =
    driver::CTL_CODE(0x0000002D, 0x0500, driver::METHOD_BUFFERED, driver::FILE_ANY_ACCESS); // 0x002D1400
inline constexpr uint32_t IOCTL_STORAGE_MANAGE_DATA_SET_ATTRIBUTES_CODE =
    driver::CTL_CODE(0x0000002D, 0x0501, driver::METHOD_BUFFERED, driver::FILE_WRITE_ACCESS); // 0x002D5404

// Clean-Room T10 DIF CRC-16 Calculation
inline uint16_t ComputeT10Crc16(const void* data, size_t length) {
    if (!data || length == 0) return 0;
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(p[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x8BB7;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// ============================================================================
// 2. NVMe Structures (SQE, CQE, Identify, SMART)
// ============================================================================

#pragma pack(push, 1)

// 64-byte Submission Queue Entry (SQE)
struct NvmeSqe {
    uint8_t  opcode{0};
    uint8_t  flags{0};      // FUSE (bits 0:1), PSDT (bits 6:7)
    uint16_t commandId{0};
    uint32_t nsid{0};       // Namespace Identifier
    uint64_t reserved{0};
    uint64_t metadataPtr{0};
    uint64_t prp1{0};       // Physical Region Page 1 (or SGL 1)
    uint64_t prp2{0};       // Physical Region Page 2 (or SGL 2)
    uint32_t cdw10{0};      // Command-specific
    uint32_t cdw11{0};
    uint32_t cdw12{0};
    uint32_t cdw13{0};
    uint32_t cdw14{0};
    uint32_t cdw15{0};
};

// 16-byte Completion Queue Entry (CQE)
struct NvmeCqe {
    uint32_t commandSpecific{0};
    uint32_t reserved{0};
    uint16_t sqHead{0};
    uint16_t sqId{0};
    uint16_t commandId{0};
    uint16_t status{0};     // Phase Tag (bit 0), Status Code (bits 1:15)

    constexpr bool getPhase() const { return (status & 0x01) != 0; }
    void setPhase(bool p) {
        if (p) status |= 0x01;
        else status &= ~0x01u;
    }

    constexpr uint16_t getStatusCode() const { return (status >> 1) & 0x7FFF; }
    void setStatusCode(uint16_t sc) {
        status = (status & 0x01) | ((sc & 0x7FFF) << 1);
    }
};

// 16-byte Scatter-Gather List (SGL) Descriptor
struct NvmeSglDescriptor {
    uint64_t address{0};
    uint32_t length{0};
    uint8_t  reserved[3]{0};
    uint8_t  sglIdentifier{0}; // bits 7:4 = type, bits 3:0 = sub-type

    uint8_t getType() const noexcept { return (sglIdentifier >> 4) & 0x0F; }
    uint8_t getSubType() const noexcept { return sglIdentifier & 0x0F; }
    void setType(uint8_t type, uint8_t subtype = 0) noexcept {
        sglIdentifier = static_cast<uint8_t>(((type & 0x0F) << 4) | (subtype & 0x0F));
    }
};

// 8-byte T10 Protection Information (DIF/DIX)
struct NvmeProtectionInformation {
    uint16_t guardTag{0};      // CRC-16 T10 DIF
    uint16_t applicationTag{0};
    uint32_t referenceTag{0};  // Seeded by lower 32-bits of LBA in Type 1
};

// 64-byte ZNS Zone Descriptor
struct NvmeZoneDescriptor {
    uint8_t  zoneType{0x02}; // Sequential Write Required
    uint8_t  zoneState{NVME_ZONE_STATE_EMPTY};
    uint8_t  zoneAttributes{0};
    uint8_t  reserved[5]{0};
    uint64_t zoneCapacity{512};
    uint64_t zoneStartLba{0};
    uint64_t writePointer{0};
    uint8_t  reserved2[32]{0};
};

enum STORAGE_BUS_TYPE : uint32_t {
    BusTypeUnknown = 0x00,
    BusTypeScsi    = 0x01,
    BusTypeAtapi   = 0x02,
    BusTypeAta     = 0x03,
    BusType1394    = 0x04,
    BusTypeSsa     = 0x05,
    BusTypeFibre   = 0x06,
    BusTypeUsb     = 0x07,
    BusTypeRAID    = 0x08,
    BusTypeiScsi   = 0x09,
    BusTypeSas     = 0x0A,
    BusTypeSata    = 0x0B,
    BusTypeSd      = 0x0C,
    BusTypeMmc     = 0x0D,
    BusTypeVirtual = 0x0E,
    BusTypeFileBackedVirtual = 0x0F,
    BusTypeSpaces  = 0x10,
    BusTypeNvme    = 0x11, // 17 (0x11)
    BusTypeScm     = 0x12,
    BusTypeUfs     = 0x13,
    BusTypeMax     = 0x14
};

enum STORAGE_PROPERTY_ID : uint32_t {
    StorageDeviceProperty = 0,
    StorageAdapterProperty = 1,
    StorageDeviceIdProperty = 2,
    StorageDeviceUniqueIdProperty = 3,
    StorageDeviceWriteCacheProperty = 4,
    StorageMiniportProperty = 5,
    StorageAccessAlignmentProperty = 6,
    StorageDeviceSeekPenaltyProperty = 7,
    StorageDeviceTrimProperty = 8,
    StorageDeviceWriteAggregationProperty = 9,
    StorageDeviceDeviceTelemetryProperty = 10,
    StorageDeviceLBPOptionsProperty = 11
};

enum STORAGE_QUERY_TYPE : uint32_t {
    PropertyStandardQuery = 0,
    PropertyExistsQuery = 1,
    PropertyMaskQuery = 2,
    PropertyQueryMaxDefined = 3
};

struct STORAGE_PROPERTY_QUERY {
    STORAGE_PROPERTY_ID PropertyId{StorageDeviceProperty};
    STORAGE_QUERY_TYPE  QueryType{PropertyStandardQuery};
    uint8_t             AdditionalParameters[1]{0};
};

struct STORAGE_DEVICE_DESCRIPTOR {
    uint32_t Version{sizeof(STORAGE_DEVICE_DESCRIPTOR)};
    uint32_t Size{sizeof(STORAGE_DEVICE_DESCRIPTOR)};
    uint8_t  DeviceType{0};
    uint8_t  DeviceTypeModifier{0};
    uint8_t  RemovableMedia{0};
    uint8_t  CommandQueueing{1};
    uint32_t VendorIdOffset{0};
    uint32_t ProductIdOffset{0};
    uint32_t ProductRevisionOffset{0};
    uint32_t SerialNumberOffset{0};
    STORAGE_BUS_TYPE BusType{BusTypeNvme};
    uint32_t RawPropertiesLength{0};
    uint8_t  RawDeviceProperties[1]{0};
};

// LBA Format Descriptor (16 entries in Identify Namespace)
struct NvmeLbaFormat {
    uint16_t metadataSize{0};
    uint8_t  lbaDataSizeShift{9}; // 9 = 512 bytes, 12 = 4096 bytes
    uint8_t  relativePerformance{0}; // 0 = Best, 1 = Better, 2 = Good, 3 = Degraded
};

// 4096-byte Identify Controller Structure
struct NvmeIdentifyController {
    uint16_t vid{0x144D};             // PCI Vendor ID (Samsung / Sovereign)
    uint16_t ssvid{0x144D};           // Subsystem Vendor ID
    char     sn[20]{"MICA-NVME-2026001"}; // Serial Number
    char     mn[40]{"TitanNVMe Sovereign Flash SSD 2TB"}; // Model Number
    char     fr[8]{"2.0.0M"};       // Firmware Revision
    uint8_t  rab{3};                  // Recommended Arbitration Burst
    uint8_t  ieee[3]{0x00, 0x25, 0x38}; // IEEE OUI
    uint8_t  cmic{0};                 // Controller Multi-Path I/O
    uint8_t  mdts{5};                 // Max Data Transfer Size (2^mdts * min page size)
    uint16_t cntlid{1};               // Controller ID
    uint32_t ver{0x00020000};         // Version 2.0
    uint32_t rtd3r{100000};           // RTD3 Resume Latency (us)
    uint32_t rtd3e{5000000};          // RTD3 Entry Latency (us)
    uint32_t oaes{0};                 // Optional Async Events
    uint32_t ctratt{0};               // Controller Attributes
    uint16_t rrls{0};
    uint8_t  reserved1[140]{};
    uint16_t oacs{0x0017};            // Optional Admin Command Support (Format, Security, FW)
    uint8_t  acl{3};                  // Abort Command Limit
    uint8_t  aerl{3};                 // Asynchronous Event Request Limit
    uint8_t  frmw{0x06};              // Firmware Updates (Slots, Activation)
    uint8_t  lpa{0x07};               // Log Page Attributes (SMART, Commands)
    uint8_t  elpe{63};                // Error Log Page Entries
    uint8_t  npss{4};                 // Number of Power States
    uint8_t  avscc{1};                // Admin Vendor Specific Command Config
    uint8_t  apsta{1};                // Autonomous Power State Transition
    uint16_t wctemp{348};             // Warning Composite Temp (Kelvin: 75 C)
    uint16_t cctemp{358};             // Critical Composite Temp (Kelvin: 85 C)
    uint16_t mtfa{0};
    uint32_t hmpre{0};
    uint32_t hmmin{0};
    uint64_t tnvmcap[2]{2000398934016ULL, 0}; // Total NVM Capacity (2TB)
    uint64_t unvmcap[2]{2000398934016ULL, 0}; // Unallocated NVM Capacity
    uint32_t rpmbs{0};
    uint16_t edstt{0};
    uint8_t  dsto{0};
    uint8_t  fwug{0};
    uint16_t kas{0};
    uint16_t hcs{0};
    uint8_t  reserved2[200]{};
    uint8_t  sqes{0x66};              // SQ Entry Size: Max 64B, Min 64B (2^6)
    uint8_t  cqes{0x44};              // CQ Entry Size: Max 16B, Min 16B (2^4)
    uint16_t maxcmd{1024};
    uint32_t nn{4};                   // Number of Namespaces (4 active namespaces)
    uint16_t oncs{0x005F};            // Optional NVM Commands: Compare, Write Zeroes, Dataset Mgmt
    uint16_t fuses{0};
    uint8_t  fna{0x04};               // Format NVM Attributes
    uint8_t  vwc{0x01};               // Volatile Write Cache Present
    uint16_t awun{0};                 // Atomic Write Unit Normal
    uint16_t awupf{0};                // Atomic Write Unit Power Fail
    uint8_t  nvscc{1};
    uint8_t  nwpc{0};
    uint16_t acwu{0};
    uint8_t  reserved3[3072]{};
};

// 4096-byte Identify Namespace Structure
struct NvmeIdentifyNamespace {
    uint64_t nsze{0};                 // Namespace Size (in logical blocks)
    uint64_t ncap{0};                 // Namespace Capacity (in logical blocks)
    uint64_t nuse{0};                 // Namespace Utilization (in logical blocks)
    uint8_t  nsfeat{0x04};            // Namespace Features (Deallocated/Unwritten error)
    uint8_t  nlbaf{1};                // Number of LBA Formats (0-based: 2 formats: 512B & 4KB)
    uint8_t  flbas{0};                // Formatted LBA Size (index into lbaf)
    uint8_t  mc{0};                   // Metadata Capabilities
    uint8_t  dpc{0};                  // End-to-end Data Protection Capabilities
    uint8_t  dps{0};                  // End-to-end Data Protection Settings
    uint8_t  nmic{0};                 // Namespace Multi-path I/O
    uint8_t  rescap{0};               // Reservation Capabilities
    uint8_t  fpi{0};
    uint8_t  dlfeat{0x09};            // Deallocate Logical Block Features (Read zeroes)
    uint16_t nawun{0};
    uint16_t nawupf{0};
    uint16_t nacwu{0};
    uint16_t nabsn{0};
    uint16_t nabo{0};
    uint16_t nabspf{0};
    uint16_t noiob{0};
    uint64_t nvmcap[2]{};             // NVM Capacity in bytes
    uint8_t  reserved1[40]{};
    uint8_t  nguid[16]{};             // 128-bit Globally Unique Identifier
    uint64_t eui64{0};                // 64-bit IEEE Extended Unique Identifier
    NvmeLbaFormat lbaf[16]{};         // LBA Format list
    uint8_t  reserved2[3904]{};
};

// 512-byte SMART / Health Information Log Page
struct NvmeSmartLog {
    uint8_t  criticalWarning{0};      // Spare, Temp, Reliability, ReadOnly, VolatileMem
    uint16_t compositeTemp{318};      // Current Temp in Kelvin (318K = 45 C)
    uint8_t  availableSpare{100};     // Percentage available spare remaining
    uint8_t  availableSpareThreshold{10};
    uint8_t  percentageUsed{1};       // Endurance wear indicator (1% used)
    uint8_t  enduSummary{0};
    uint8_t  reserved1[26]{};
    uint64_t dataUnitsRead[2]{1048576, 0};    // 1000 units * 512B
    uint64_t dataUnitsWritten[2]{524288, 0};
    uint64_t hostReadCommands[2]{40960, 0};
    uint64_t hostWriteCommands[2]{20480, 0};
    uint64_t controllerBusyTime[2]{120, 0};   // Minutes
    uint64_t powerCycles[2]{15, 0};
    uint64_t powerOnHours[2]{48, 0};
    uint64_t unsafeShutdowns[2]{0, 0};
    uint64_t mediaAndDataIntegrityErrors[2]{0, 0};
    uint64_t numErrorLogEntries[2]{0, 0};
    uint32_t warningCompositeTempTime{0};
    uint32_t criticalCompositeTempTime{0};
    uint16_t tempSensor[8]{318, 316, 320, 0, 0, 0, 0, 0};
    uint8_t  reserved2[296]{};
};

#pragma pack(pop)

// ============================================================================
// 3. Flash Namespace & Virtual Flash Media Engine
// ============================================================================

class FlashDiskDevice : public storage::IBlockDevice {
    std::wstring m_name;
    uint64_t m_totalBlocks{0};
    uint32_t m_blockSize{512};
    std::vector<uint8_t> m_storage;
    mutable std::mutex m_mutex;

public:
    FlashDiskDevice(std::wstring_view name, uint64_t totalBytes, uint32_t blockSize = 512)
        : m_name(name), m_blockSize(blockSize ? blockSize : 512)
    {
        m_totalBlocks = (totalBytes + m_blockSize - 1) / m_blockSize;
        size_t allocBytes = std::min<size_t>(static_cast<size_t>(totalBytes), 16 * 1024 * 1024);
        m_storage.resize(allocBytes, 0);
    }

    NtStatus readBlocks(uint64_t lba, uint32_t count, void* buffer) override {
        if (!buffer || count == 0) return NtStatus::InvalidParameter;
        if (lba + count > m_totalBlocks) return NtStatus::EndOfFile;
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t byteCount = static_cast<size_t>(count) * m_blockSize;
        size_t offset = (static_cast<size_t>(lba * m_blockSize)) % m_storage.size();
        if (offset + byteCount <= m_storage.size()) {
            std::memcpy(buffer, m_storage.data() + offset, byteCount);
        } else {
            std::memset(buffer, 0, byteCount);
        }
        return NtStatus::Success;
    }

    NtStatus writeBlocks(uint64_t lba, uint32_t count, const void* buffer) override {
        if (!buffer || count == 0) return NtStatus::InvalidParameter;
        if (lba + count > m_totalBlocks) return NtStatus::DiskFull;
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t byteCount = static_cast<size_t>(count) * m_blockSize;
        size_t offset = (static_cast<size_t>(lba * m_blockSize)) % m_storage.size();
        if (offset + byteCount <= m_storage.size()) {
            std::memcpy(m_storage.data() + offset, buffer, byteCount);
        }
        return NtStatus::Success;
    }

    uint32_t getBlockSize() const noexcept override { return m_blockSize; }
    uint64_t getTotalBlocks() const noexcept override { return m_totalBlocks; }
    const std::wstring& getDeviceName() const noexcept override { return m_name; }
};

class NvmeNamespace : public storage::IBlockDevice {
    uint32_t m_nsid{1};
    uint64_t m_totalBlocks{0};
    uint32_t m_blockSize{512};
    std::wstring m_name;
    std::vector<uint8_t> m_flashStorage;
    std::unordered_set<uint64_t> m_trimmedBlocks;
    bool m_piEnabled{false};
    uint8_t m_piType{1}; // Type 1: Guard + App + Ref
    std::unordered_map<uint64_t, NvmeProtectionInformation> m_protectionMap;
    bool m_isZns{false};
    uint32_t m_zoneCount{16};
    uint64_t m_zoneSize{512};
    std::vector<NvmeZoneDescriptor> m_zones;
    mutable std::recursive_mutex m_mutex;

public:
    NvmeNamespace(uint32_t nsid, uint64_t totalBlocks, uint32_t blockSize = 512)
        : m_nsid(nsid), m_totalBlocks(totalBlocks), m_blockSize(blockSize)
    {
        m_name = L"\\Device\\Harddisk1\\DR" + std::to_wstring(nsid);
        size_t totalBytes = static_cast<size_t>(totalBlocks * blockSize);
        // Cap in-memory allocation for simulator testing to 64MB if blocks represent terabytes
        size_t allocBytes = std::min<size_t>(totalBytes, 64 * 1024 * 1024);
        m_flashStorage.resize(allocBytes, 0);
    }

    uint32_t getNsid() const { return m_nsid; }

    // IBlockDevice overrides
    NtStatus readBlocks(uint64_t lba, uint32_t count, void* buffer) override {
        if (!buffer || count == 0) return NtStatus::InvalidParameter;
        if (lba + count > m_totalBlocks) return NtStatus::EndOfFile;

        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        uint8_t* dst = reinterpret_cast<uint8_t*>(buffer);
        size_t bytesToRead = static_cast<size_t>(count) * m_blockSize;
        size_t offset = static_cast<size_t>(lba * m_blockSize) % m_flashStorage.size();

        if (offset + bytesToRead <= m_flashStorage.size()) {
            std::memcpy(dst, m_flashStorage.data() + offset, bytesToRead);
        } else {
            std::memset(dst, 0, bytesToRead);
        }
        return NtStatus::Success;
    }

    NtStatus writeBlocks(uint64_t lba, uint32_t count, const void* buffer) override {
        if (!buffer || count == 0) return NtStatus::InvalidParameter;
        if (lba + count > m_totalBlocks) return NtStatus::EndOfFile;

        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        const uint8_t* src = reinterpret_cast<const uint8_t*>(buffer);
        size_t bytesToWrite = static_cast<size_t>(count) * m_blockSize;
        size_t offset = static_cast<size_t>(lba * m_blockSize) % m_flashStorage.size();

        if (offset + bytesToWrite <= m_flashStorage.size()) {
            std::memcpy(m_flashStorage.data() + offset, src, bytesToWrite);
        }
        for (uint64_t i = 0; i < count; ++i) {
            m_trimmedBlocks.erase(lba + i);
        }
        return NtStatus::Success;
    }

    NtStatus writeZeroes(uint64_t lba, uint32_t count) {
        if (count == 0) return NtStatus::InvalidParameter;
        if (lba + count > m_totalBlocks) return NtStatus::EndOfFile;

        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        size_t bytesToClear = static_cast<size_t>(count) * m_blockSize;
        size_t offset = static_cast<size_t>(lba * m_blockSize) % m_flashStorage.size();

        if (offset + bytesToClear <= m_flashStorage.size()) {
            std::memset(m_flashStorage.data() + offset, 0, bytesToClear);
        }
        return NtStatus::Success;
    }

    NtStatus trimBlocks(uint64_t lba, uint32_t count) {
        if (count == 0) return NtStatus::InvalidParameter;
        if (lba + count > m_totalBlocks) return NtStatus::EndOfFile;

        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (uint64_t i = 0; i < count; ++i) {
            m_trimmedBlocks.insert(lba + i);
        }
        // Deallocated blocks read back as zero per dlfeat
        size_t bytesToClear = static_cast<size_t>(count) * m_blockSize;
        size_t offset = static_cast<size_t>(lba * m_blockSize) % m_flashStorage.size();
        if (offset + bytesToClear <= m_flashStorage.size()) {
            std::memset(m_flashStorage.data() + offset, 0, bytesToClear);
        }
        return NtStatus::Success;
    }

    bool isTrimmed(uint64_t lba) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_trimmedBlocks.find(lba) != m_trimmedBlocks.end();
    }

    uint32_t getBlockSize() const noexcept override { return m_blockSize; }
    uint64_t getTotalBlocks() const noexcept override { return m_totalBlocks; }
    const std::wstring& getDeviceName() const noexcept override { return m_name; }

    void format(uint32_t newBlockSize) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_blockSize = newBlockSize;
        std::fill(m_flashStorage.begin(), m_flashStorage.end(), 0);
        m_trimmedBlocks.clear();
        m_protectionMap.clear();
    }

    void enableProtectionInformation(uint8_t type = 1) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_piEnabled = true;
        m_piType = type;
    }

    void disableProtectionInformation() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_piEnabled = false;
        m_protectionMap.clear();
    }

    bool isProtectionEnabled() const noexcept { return m_piEnabled; }
    uint8_t getProtectionType() const noexcept { return m_piType; }

    uint16_t writeBlocksWithPi(uint64_t lba, uint32_t count, const void* buffer, const NvmeProtectionInformation* piArray) {
        if (!buffer || count == 0 || !piArray) return NVME_SC_INVALID_FIELD;
        if (lba + count > m_totalBlocks) return NVME_SC_LBA_OUT_OF_RANGE;

        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        const uint8_t* pBuf = static_cast<const uint8_t*>(buffer);

        for (uint32_t i = 0; i < count; ++i) {
            uint64_t currLba = lba + i;
            const auto& pi = piArray[i];

            // 1. Guard Tag verification (CRC-16)
            uint16_t calcCrc = ComputeT10Crc16(pBuf + (i * m_blockSize), m_blockSize);
            if (pi.guardTag != 0 && pi.guardTag != calcCrc) {
                return NVME_SC_GUARD_CHECK_ERROR;
            }

            // 2. Reference Tag verification (Type 1 checks against lower 32 bits of LBA)
            if (m_piType == 1 && pi.referenceTag != 0) {
                uint32_t expRef = static_cast<uint32_t>(currLba & 0xFFFFFFFF);
                if (pi.referenceTag != expRef) {
                    return NVME_SC_REF_TAG_CHECK_ERROR;
                }
            }

            m_protectionMap[currLba] = pi;
        }

        return (writeBlocks(lba, count, buffer) == NtStatus::Success) ? NVME_SC_SUCCESS : NVME_SC_INTERNAL_DEVICE_ERROR;
    }

    uint16_t readBlocksWithPi(uint64_t lba, uint32_t count, void* buffer, NvmeProtectionInformation* piArray) {
        if (!buffer || count == 0) return NVME_SC_INVALID_FIELD;
        if (lba + count > m_totalBlocks) return NVME_SC_LBA_OUT_OF_RANGE;

        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        NtStatus st = readBlocks(lba, count, buffer);
        if (st != NtStatus::Success) return NVME_SC_INTERNAL_DEVICE_ERROR;

        uint8_t* pBuf = static_cast<uint8_t*>(buffer);
        for (uint32_t i = 0; i < count; ++i) {
            uint64_t currLba = lba + i;
            auto it = m_protectionMap.find(currLba);
            NvmeProtectionInformation pi{};
            if (it != m_protectionMap.end()) {
                pi = it->second;
            } else {
                pi.guardTag = ComputeT10Crc16(pBuf + (i * m_blockSize), m_blockSize);
                pi.referenceTag = static_cast<uint32_t>(currLba & 0xFFFFFFFF);
                pi.applicationTag = 0x5EED;
            }

            uint16_t calcCrc = ComputeT10Crc16(pBuf + (i * m_blockSize), m_blockSize);
            if (pi.guardTag != 0 && pi.guardTag != calcCrc) {
                return NVME_SC_GUARD_CHECK_ERROR;
            }
            if (m_piType == 1 && pi.referenceTag != 0 && pi.referenceTag != static_cast<uint32_t>(currLba & 0xFFFFFFFF)) {
                return NVME_SC_REF_TAG_CHECK_ERROR;
            }

            if (piArray) {
                piArray[i] = pi;
            }
        }
        return NVME_SC_SUCCESS;
    }

    void enableZns(uint32_t numZones = 16, uint64_t zoneCapacity = 512) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_isZns = true;
        m_zoneCount = numZones;
        m_zoneSize = zoneCapacity;
        m_zones.clear();
        m_zones.resize(numZones);
        for (uint32_t i = 0; i < numZones; ++i) {
            m_zones[i].zoneType = 0x02; // Sequential Write Required
            m_zones[i].zoneState = NVME_ZONE_STATE_EMPTY;
            m_zones[i].zoneAttributes = 0;
            m_zones[i].zoneCapacity = zoneCapacity;
            m_zones[i].zoneStartLba = i * zoneCapacity;
            m_zones[i].writePointer = m_zones[i].zoneStartLba;
        }
    }

    bool isZnsEnabled() const noexcept { return m_isZns; }
    uint32_t getZoneCount() const noexcept { return m_zoneCount; }
    uint64_t getZoneSize() const noexcept { return m_zoneSize; }

    uint16_t zoneManagementSend(uint8_t action, uint64_t slba) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!m_isZns || m_zones.empty()) return NVME_SC_INVALID_FIELD;
        uint32_t zoneIdx = static_cast<uint32_t>(slba / m_zoneSize);
        if (zoneIdx >= m_zones.size()) return NVME_SC_LBA_OUT_OF_RANGE;
        auto& z = m_zones[zoneIdx];

        switch (action) {
            case NVME_ZNS_ACTION_OPEN:
                z.zoneState = NVME_ZONE_STATE_EXP_OPEN;
                break;
            case NVME_ZNS_ACTION_CLOSE:
                z.zoneState = NVME_ZONE_STATE_CLOSED;
                break;
            case NVME_ZNS_ACTION_FINISH:
                z.zoneState = NVME_ZONE_STATE_FULL;
                z.writePointer = z.zoneStartLba + z.zoneCapacity;
                break;
            case NVME_ZNS_ACTION_RESET:
                z.zoneState = NVME_ZONE_STATE_EMPTY;
                z.writePointer = z.zoneStartLba;
                writeZeroes(z.zoneStartLba, static_cast<uint32_t>(z.zoneCapacity));
                break;
            default:
                return NVME_SC_INVALID_FIELD;
        }
        return NVME_SC_SUCCESS;
    }

    uint16_t zoneManagementReceive(uint64_t slba, std::vector<NvmeZoneDescriptor>& outDescs) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!m_isZns || m_zones.empty()) return NVME_SC_INVALID_FIELD;
        uint32_t startIdx = static_cast<uint32_t>(slba / m_zoneSize);
        outDescs.clear();
        for (size_t i = startIdx; i < m_zones.size(); ++i) {
            outDescs.push_back(m_zones[i]);
        }
        return NVME_SC_SUCCESS;
    }

    uint16_t zoneAppend(uint64_t zslba, uint16_t nlb, const void* buffer, uint64_t& outAllocatedLba) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!m_isZns || m_zones.empty()) return NVME_SC_INVALID_FIELD;
        uint32_t zoneIdx = static_cast<uint32_t>(zslba / m_zoneSize);
        if (zoneIdx >= m_zones.size()) return NVME_SC_LBA_OUT_OF_RANGE;
        auto& z = m_zones[zoneIdx];

        if (z.zoneState == NVME_ZONE_STATE_FULL || z.zoneState == NVME_ZONE_STATE_READONLY) {
            return NVME_SC_CAPACITY_EXCEEDED;
        }
        if (z.writePointer + nlb > z.zoneStartLba + z.zoneCapacity) {
            return NVME_SC_CAPACITY_EXCEEDED;
        }

        outAllocatedLba = z.writePointer;
        NtStatus st = writeBlocks(outAllocatedLba, nlb, buffer);
        if (st != NtStatus::Success) return NVME_SC_DATA_TRANSFER_ERROR;

        z.writePointer += nlb;
        if (z.writePointer >= z.zoneStartLba + z.zoneCapacity) {
            z.zoneState = NVME_ZONE_STATE_FULL;
        } else {
            z.zoneState = NVME_ZONE_STATE_IMP_OPEN;
        }
        return NVME_SC_SUCCESS;
    }
};

// ============================================================================
// 3.5. SCSI & StorPort Miniport Protocol Definitions
// ============================================================================

enum SRB_FUNCTION : uint8_t {
    SRB_FUNCTION_EXECUTE_SCSI          = 0x00,
    SRB_FUNCTION_CLAIM_DEVICE          = 0x01,
    SRB_FUNCTION_ABORT_COMMAND         = 0x02,
    SRB_FUNCTION_RELEASE_DEVICE        = 0x04,
    SRB_FUNCTION_RESET_BUS             = 0x05,
    SRB_FUNCTION_RESET_DEVICE          = 0x06,
    SRB_FUNCTION_TERMINATE_IO          = 0x14,
    SRB_FUNCTION_FLUSH_QUEUE           = 0x15,
    SRB_FUNCTION_ATTACH_DEVICE         = 0x16,
    SRB_FUNCTION_RELEASE_QUEUE         = 0x17,
    SRB_FUNCTION_RESET_LOGICAL_UNIT    = 0x20,
    SRB_FUNCTION_STORAGE_REQUEST_BLOCK = 0x30,
    SRB_FUNCTION_PNP                   = 0x25,
    SRB_FUNCTION_POWER                 = 0x24
};

enum SRB_STATUS : uint8_t {
    SRB_STATUS_PENDING                 = 0x00,
    SRB_STATUS_SUCCESS                 = 0x01,
    SRB_STATUS_ABORTED                 = 0x02,
    SRB_STATUS_ABORT_FAILED            = 0x03,
    SRB_STATUS_ERROR                   = 0x04,
    SRB_STATUS_BUSY                    = 0x05,
    SRB_STATUS_INVALID_REQUEST         = 0x06,
    SRB_STATUS_INVALID_PATH_ID         = 0x07,
    SRB_STATUS_NO_DEVICE               = 0x08,
    SRB_STATUS_TIMEOUT                 = 0x09,
    SRB_STATUS_SELECTION_TIMEOUT       = 0x0A,
    SRB_STATUS_COMMAND_TIMEOUT         = 0x0B,
    SRB_STATUS_DATA_OVERRUN            = 0x12,
    SRB_STATUS_UNEXPECTED_BUS_FREE     = 0x13,
    SRB_STATUS_PHASE_SEQUENCE_FAILURE  = 0x14,
    SRB_STATUS_BAD_SRB_BLOCK_LENGTH    = 0x15,
    SRB_STATUS_REQUEST_FLUSHED         = 0x16
};

enum SCSI_STATUS : uint8_t {
    SCSISTAT_GOOD                      = 0x00,
    SCSISTAT_CHECK_CONDITION           = 0x02,
    SCSISTAT_CONDITION_MET             = 0x04,
    SCSISTAT_BUSY                      = 0x08,
    SCSISTAT_INTERMEDIATE              = 0x10,
    SCSISTAT_RESERVATION_CONFLICT      = 0x18,
    SCSISTAT_COMMAND_TERMINATED        = 0x22,
    SCSISTAT_QUEUE_FULL                = 0x28
};

enum SCSI_OPCODE : uint8_t {
    SCSIOP_TEST_UNIT_READY             = 0x00,
    SCSIOP_REQUEST_SENSE               = 0x03,
    SCSIOP_INQUIRY                     = 0x12,
    SCSIOP_MODE_SELECT                 = 0x15,
    SCSIOP_RESERVE_UNIT                = 0x16,
    SCSIOP_RELEASE_UNIT                = 0x17,
    SCSIOP_MODE_SENSE                  = 0x1A,
    SCSIOP_START_STOP_UNIT             = 0x1B,
    SCSIOP_READ_CAPACITY10             = 0x25,
    SCSIOP_READ10                      = 0x28,
    SCSIOP_WRITE10                     = 0x2A,
    SCSIOP_SEEK10                      = 0x2B,
    SCSIOP_WRITE_VERIFY10              = 0x2E,
    SCSIOP_VERIFY10                    = 0x2F,
    SCSIOP_SYNCHRONIZE_CACHE10         = 0x35,
    SCSIOP_UNMAP                       = 0x42,
    SCSIOP_MODE_SENSE10                = 0x5A,
    SCSIOP_READ16                      = 0x88,
    SCSIOP_WRITE16                     = 0x8A,
    SCSIOP_SYNCHRONIZE_CACHE16         = 0x91,
    SCSIOP_READ_CAPACITY16             = 0x9E,
    SCSIOP_REPORT_LUNS                 = 0xA0
};

struct SCSI_REQUEST_BLOCK {
    uint16_t            Length{sizeof(SCSI_REQUEST_BLOCK)};
    uint8_t             Function{SRB_FUNCTION_EXECUTE_SCSI};
    uint8_t             SrbStatus{SRB_STATUS_PENDING};
    uint8_t             ScsiStatus{SCSISTAT_GOOD};
    uint8_t             PathId{0};
    uint8_t             TargetId{0};
    uint8_t             Lun{0};
    uint8_t             QueueTag{0};
    uint8_t             QueueAction{0};
    uint8_t             CdbLength{16};
    uint8_t             SenseInfoBufferLength{0};
    uint32_t            SrbFlags{0};
    uint32_t            DataTransferLength{0};
    uint32_t            TimeOutValue{30};
    void*               DataBuffer{nullptr};
    void*               SenseInfoBuffer{nullptr};
    SCSI_REQUEST_BLOCK* NextSrb{nullptr};
    void*               OriginalRequest{nullptr};
    void*               SrbExtension{nullptr};
    uint32_t            InternalStatus{0};
    uint32_t            Reserved{0};
    uint8_t             Cdb[16]{};
};
using PSCSI_REQUEST_BLOCK = SCSI_REQUEST_BLOCK*;

struct STOR_ADDR_BTL8 {
    uint16_t Type{0};
    uint16_t Port{0};
    uint32_t AddressLength{sizeof(STOR_ADDR_BTL8)};
    uint8_t  Path{0};
    uint8_t  Target{0};
    uint8_t  Lun{0};
    uint8_t  Reserved{0};
};

struct PORT_CONFIGURATION_INFORMATION {
    uint32_t Length{sizeof(PORT_CONFIGURATION_INFORMATION)};
    uint32_t SystemIoBusNumber{0};
    uint32_t AdapterInterfaceType{0}; // PCI
    uint32_t BusInterruptLevel{0};
    uint32_t BusInterruptVector{0};
    uint32_t InterruptMode{0};
    uint32_t MaximumTransferLength{2 * 1024 * 1024}; // 2MB
    uint32_t NumberOfPhysicalBreaks{512};
    uint32_t Dma64BitAddresses{1};
    uint32_t BufferAlignmentMask{0x03};
    uint32_t NumberOfBuses{1};
    uint32_t MaximumNumberOfTargets{4};
    uint32_t MaximumNumberOfLogicalUnits{8};
    uint32_t SynchronizationModel{1};
};

enum STORPORT_NOTIFICATION_TYPE {
    RequestComplete           = 0,
    NextRequest               = 1,
    NextLuRequest             = 2,
    ResetDetected             = 3,
    CallDisableInterrupts     = 4,
    CallEnableInterrupts      = 5,
    RequestTimerCall          = 6,
    BusChangeDetected         = 7,
    WmiEvent                  = 8,
    WmiLogEvent               = 9,
    LinkDown                  = 10,
    LinkUp                    = 11
};

enum SC_CONTROL_TYPE {
    ScsiQuerySupportedControlTypes = 0,
    ScsiStopAdapter                = 1,
    ScsiRestartAdapter             = 2,
    ScsiSetBootConfig              = 3,
    ScsiSetRunningConfig           = 4,
    ScsiPowerSettingNotification   = 5
};

struct HW_INITIALIZATION_DATA {
    uint32_t HwInitializationDataSize{sizeof(HW_INITIALIZATION_DATA)};
    uint32_t AdapterInterfaceType{0};
};

// ============================================================================
// 3.6. Physical Region Page (PRP) & Scatter-Gather DMA Engine
// ============================================================================

class PrpDmaEngine {
private:
    std::unordered_map<uint64_t, std::vector<uint8_t>> m_physicalPages;
    uint64_t m_nextPhysAddr{0x10000000ULL}; // 256MB base
    mutable std::mutex m_mutex;

public:
    static constexpr size_t PAGE_SIZE = 4096;

    uint64_t allocatePhysicalPage() {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t addr = m_nextPhysAddr;
        m_nextPhysAddr += PAGE_SIZE;
        m_physicalPages[addr].assign(PAGE_SIZE, 0);
        return addr;
    }

    void writePhysicalMemory(uint64_t physAddr, const void* src, size_t length) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const uint8_t* pSrc = static_cast<const uint8_t*>(src);
        size_t written = 0;
        while (written < length) {
            uint64_t pageBase = physAddr & ~(static_cast<uint64_t>(PAGE_SIZE - 1));
            size_t pageOffset = physAddr & (PAGE_SIZE - 1);
            size_t chunk = std::min(length - written, PAGE_SIZE - pageOffset);
            auto& page = m_physicalPages[pageBase];
            if (page.size() < PAGE_SIZE) page.resize(PAGE_SIZE, 0);
            std::memcpy(page.data() + pageOffset, pSrc + written, chunk);
            physAddr += chunk;
            written += chunk;
        }
    }

    void readPhysicalMemory(uint64_t physAddr, void* dst, size_t length) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint8_t* pDst = static_cast<uint8_t*>(dst);
        size_t readBytes = 0;
        while (readBytes < length) {
            uint64_t pageBase = physAddr & ~(static_cast<uint64_t>(PAGE_SIZE - 1));
            size_t pageOffset = physAddr & (PAGE_SIZE - 1);
            size_t chunk = std::min(length - readBytes, PAGE_SIZE - pageOffset);
            auto it = m_physicalPages.find(pageBase);
            if (it != m_physicalPages.end()) {
                std::memcpy(pDst + readBytes, it->second.data() + pageOffset, chunk);
            } else {
                std::memset(pDst + readBytes, 0, chunk);
            }
            physAddr += chunk;
            readBytes += chunk;
        }
    }

    void buildPrpChain(
        const void* hostData,
        size_t length,
        uint64_t& outPrp1,
        uint64_t& outPrp2,
        std::vector<uint64_t>& outPrpList
    ) {
        if (length == 0) {
            outPrp1 = 0;
            outPrp2 = 0;
            return;
        }

        size_t numPages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
        std::vector<uint64_t> pages;
        pages.reserve(numPages);

        const uint8_t* pData = static_cast<const uint8_t*>(hostData);
        for (size_t i = 0; i < numPages; ++i) {
            uint64_t pageAddr = allocatePhysicalPage();
            pages.push_back(pageAddr);
            if (pData) {
                size_t offset = i * PAGE_SIZE;
                size_t chunk = std::min(length - offset, PAGE_SIZE);
                writePhysicalMemory(pageAddr, pData + offset, chunk);
            }
        }

        outPrp1 = pages[0];
        if (numPages == 1) {
            outPrp2 = 0;
        } else if (numPages == 2) {
            outPrp2 = pages[1];
        } else {
            uint64_t prpListPageAddr = allocatePhysicalPage();
            outPrpList.clear();
            for (size_t i = 1; i < numPages; ++i) {
                outPrpList.push_back(pages[i]);
            }
            writePhysicalMemory(prpListPageAddr, outPrpList.data(), outPrpList.size() * sizeof(uint64_t));
            outPrp2 = prpListPageAddr;
        }
    }

    std::vector<uint64_t> resolvePrpPages(uint64_t prp1, uint64_t prp2, size_t length) const {
        std::vector<uint64_t> pages;
        if (length == 0 || prp1 == 0) return pages;

        size_t numPages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
        pages.push_back(prp1);

        if (numPages == 2) {
            pages.push_back(prp2);
        } else if (numPages > 2) {
            size_t remainingPages = numPages - 1;
            std::vector<uint64_t> listEntries(remainingPages);
            readPhysicalMemory(prp2, listEntries.data(), remainingPages * sizeof(uint64_t));
            for (uint64_t addr : listEntries) {
                pages.push_back(addr);
            }
        }
        return pages;
    }

    void transferToStorage(
        uint64_t prp1,
        uint64_t prp2,
        size_t length,
        NvmeNamespace& targetNs,
        uint64_t startLba,
        uint32_t blockCount
    ) {
        std::vector<uint8_t> buffer(length);
        auto pages = resolvePrpPages(prp1, prp2, length);
        size_t copied = 0;
        for (uint64_t pAddr : pages) {
            size_t chunk = std::min(length - copied, PAGE_SIZE);
            readPhysicalMemory(pAddr, buffer.data() + copied, chunk);
            copied += chunk;
        }
        targetNs.writeBlocks(startLba, blockCount, buffer.data());
    }

    void transferFromStorage(
        uint64_t prp1,
        uint64_t prp2,
        size_t length,
        NvmeNamespace& sourceNs,
        uint64_t startLba,
        uint32_t blockCount
    ) {
        std::vector<uint8_t> buffer(length);
        sourceNs.readBlocks(startLba, blockCount, buffer.data());
        auto pages = resolvePrpPages(prp1, prp2, length);
        size_t copied = 0;
        for (uint64_t pAddr : pages) {
            size_t chunk = std::min(length - copied, PAGE_SIZE);
            writePhysicalMemory(pAddr, buffer.data() + copied, chunk);
            copied += chunk;
        }
    }

    void buildSglChain(
        const void* hostData,
        size_t length,
        std::vector<NvmeSglDescriptor>& outSgl
    ) {
        outSgl.clear();
        if (length == 0) return;
        const uint8_t* pData = static_cast<const uint8_t*>(hostData);
        size_t offset = 0;
        while (offset < length) {
            size_t chunk = std::min<size_t>(length - offset, PAGE_SIZE);
            uint64_t physAddr = allocatePhysicalPage();
            if (pData) {
                writePhysicalMemory(physAddr, pData + offset, chunk);
            }
            NvmeSglDescriptor desc{};
            desc.address = physAddr;
            desc.length = static_cast<uint32_t>(chunk);
            desc.setType(NVME_SGL_DESCRIPTOR_TYPE_DATA_BLOCK, NVME_SGL_SUBTYPE_ADDRESS);
            outSgl.push_back(desc);
            offset += chunk;
        }
    }

    void readSglMemory(const std::vector<NvmeSglDescriptor>& sgl, void* dst, size_t length) const {
        uint8_t* pDst = static_cast<uint8_t*>(dst);
        size_t transferred = 0;
        for (const auto& desc : sgl) {
            if (transferred >= length) break;
            size_t chunk = std::min<size_t>(length - transferred, desc.length);
            readPhysicalMemory(desc.address, pDst + transferred, chunk);
            transferred += chunk;
        }
    }

    void writeSglMemory(const std::vector<NvmeSglDescriptor>& sgl, const void* src, size_t length) {
        const uint8_t* pSrc = static_cast<const uint8_t*>(src);
        size_t transferred = 0;
        for (const auto& desc : sgl) {
            if (transferred >= length) break;
            size_t chunk = std::min<size_t>(length - transferred, desc.length);
            writePhysicalMemory(desc.address, pSrc + transferred, chunk);
            transferred += chunk;
        }
    }
};

using SglDmaEngine = PrpDmaEngine;

// ============================================================================
// 4. NVMe Controller Core Engine
// ============================================================================

struct NVME_TELEMETRY {
    uint64_t controllerResets{0};
    uint64_t doorbellWrites{0};
    uint64_t adminCommandsExecuted{0};
    uint64_t ioCommandsExecuted{0};
    uint64_t prpTransfersExecuted{0};
    uint64_t dmaBytesTransferred{0};
    uint64_t abortedCommands{0};
    uint64_t msixInterruptsGenerated{0};
    uint64_t fatalErrorsInjected{0};
    uint64_t recoveriesCompleted{0};
    uint64_t aerEventsFired{0};
};

class NvmeController {
public:
    // Queue pair mappings
    struct QueuePair {
        uint16_t qid{0};
        uint16_t sqSize{64};
        uint16_t cqSize{64};
        uint64_t sqBase{0};
        uint64_t cqBase{0};
        uint16_t sqTail{0};
        uint16_t sqHead{0};
        uint16_t cqHead{0};
        uint16_t cqTail{0};
        bool     cqPhase{true};
        std::vector<NvmeSqe> sqRing;
        std::vector<NvmeCqe> cqRing;
        uint64_t msixCount{0};
    };

private:
    NvmeVersion m_version{NvmeVersion::Version_2_0};
    uint64_t m_cap{0};
    uint32_t m_cc{0};
    uint32_t m_csts{0};
    uint32_t m_aqa{0};
    uint64_t m_asq{0};
    uint64_t m_acq{0};
    uint32_t m_intms{0};
    uint32_t m_intmc{0};
    uint8_t  m_dstrd{0}; // Doorbell stride (0 = 4 bytes, 1 = 8 bytes)

    QueuePair m_adminQueue;
    std::unordered_map<uint16_t, QueuePair> m_ioQueues;
    std::unordered_map<uint32_t, uint16_t> m_cpuToQueueId;
    std::map<uint32_t, std::shared_ptr<NvmeNamespace>> m_namespaces;
    std::queue<uint16_t> m_pendingAers;

    NvmeIdentifyController m_identCtrl{};
    NvmeSmartLog m_smartLog{};
    NVME_TELEMETRY m_telemetry{};

    mutable std::recursive_mutex m_mutex;

public:
    explicit NvmeController(NvmeVersion ver = NvmeVersion::Version_2_0)
        : m_version(ver)
    {
        // CAP: MQES=1023 (1024 entries), CQC=1 (contiguous), DSTRD=0 (4 bytes), TO=15 (7.5s), CSS=1 (NVM)
        m_cap = 0x0000003F010303FFULL;
        m_identCtrl.ver = static_cast<uint32_t>(ver);

        // Pre-seed Namespaces
        // NSID 1: Primary System OS Volume (2TB, 512B sectors)
        m_namespaces[1] = std::make_shared<NvmeNamespace>(1, 3907029168ULL, 512);

        // NSID 2: Ultra High-Speed DirectStorage Scratch Volume (512GB, 4096B sectors 4Kn)
        m_namespaces[2] = std::make_shared<NvmeNamespace>(2, 125026372ULL, 4096);

        // Setup Admin Queue default sizes
        m_adminQueue.qid = 0;
        m_adminQueue.sqSize = 64;
        m_adminQueue.cqSize = 64;
        m_adminQueue.sqRing.resize(64);
        m_adminQueue.cqRing.resize(64);
    }

    NvmeVersion getVersion() const { return m_version; }
    void setVersion(NvmeVersion v) {
        m_version = v;
        m_identCtrl.ver = static_cast<uint32_t>(v);
    }

    bool isReady() const { return (m_csts & NVME_CSTS_RDY) != 0; }

    // MMIO Register Access
    uint32_t readReg32(uint32_t offset) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        switch (offset) {
            case NVME_REG_CAP: return static_cast<uint32_t>(m_cap & 0xFFFFFFFF);
            case NVME_REG_CAP + 4: return static_cast<uint32_t>(m_cap >> 32);
            case NVME_REG_VS: return static_cast<uint32_t>(m_version);
            case NVME_REG_INTMS: return m_intms;
            case NVME_REG_INTMC: return m_intmc;
            case NVME_REG_CC: return m_cc;
            case NVME_REG_CSTS: return m_csts;
            case NVME_REG_NSSR: return 0;
            case NVME_REG_AQA: return m_aqa;
            case NVME_REG_ASQ: return static_cast<uint32_t>(m_asq & 0xFFFFFFFF);
            case NVME_REG_ASQ + 4: return static_cast<uint32_t>(m_asq >> 32);
            case NVME_REG_ACQ: return static_cast<uint32_t>(m_acq & 0xFFFFFFFF);
            case NVME_REG_ACQ + 4: return static_cast<uint32_t>(m_acq >> 32);
            default:
                if (offset >= NVME_REG_DBS) {
                    return 0; // Doorbell read
                }
                return 0;
        }
    }

    void writeReg32(uint32_t offset, uint32_t val) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        switch (offset) {
            case NVME_REG_CC: {
                m_cc = val;
                if (val & NVME_CC_EN) {
                    m_csts |= NVME_CSTS_RDY;
                } else {
                    m_csts &= ~NVME_CSTS_RDY;
                }
                if ((val & NVME_CC_SHN_NORM) || (val & 0x00008000)) {
                    m_csts |= NVME_CSTS_SHST_OK;
                }
                break;
            }
            case NVME_REG_NSSR: {
                if (val == 0x4E564D65) { // "NVMe"
                    m_telemetry.controllerResets++;
                    m_csts = 0;
                    m_cc = 0;
                    m_adminQueue.sqTail = 0;
                    m_adminQueue.sqHead = 0;
                    m_adminQueue.cqHead = 0;
                    m_adminQueue.cqTail = 0;
                    m_adminQueue.cqPhase = true;
                    m_adminQueue.msixCount = 0;
                    m_ioQueues.clear();
                }
                break;
            }
            case NVME_REG_AQA: m_aqa = val; break;
            case NVME_REG_ASQ: m_asq = (m_asq & 0xFFFFFFFF00000000ULL) | val; break;
            case NVME_REG_ASQ + 4: m_asq = (static_cast<uint64_t>(val) << 32) | (m_asq & 0xFFFFFFFFULL); break;
            case NVME_REG_ACQ: m_acq = (m_acq & 0xFFFFFFFF00000000ULL) | val; break;
            case NVME_REG_ACQ + 4: m_acq = (static_cast<uint64_t>(val) << 32) | (m_acq & 0xFFFFFFFFULL); break;
            default:
                if (offset >= NVME_REG_DBS) {
                    m_telemetry.doorbellWrites++;
                    uint32_t stride = 4 << m_dstrd;
                    uint32_t dbIdx = (offset - NVME_REG_DBS) / stride;
                    uint16_t qid = static_cast<uint16_t>(dbIdx / 2);
                    bool isCq = (dbIdx % 2) == 1;
                    if (qid == 0) {
                        if (!isCq) {
                            m_adminQueue.sqTail = static_cast<uint16_t>(val);
                            processQueueLocked(m_adminQueue);
                        } else {
                            m_adminQueue.cqHead = static_cast<uint16_t>(val);
                        }
                    } else {
                        auto it = m_ioQueues.find(qid);
                        if (it != m_ioQueues.end()) {
                            if (!isCq) {
                                it->second.sqTail = static_cast<uint16_t>(val);
                                processQueueLocked(it->second);
                            } else {
                                it->second.cqHead = static_cast<uint16_t>(val);
                            }
                        }
                    }
                }
                break;
        }
    }

    // Direct Command Execution Engine
    NvmeCqe executeAdminCommand(const NvmeSqe& sqe, void* dataBuffer = nullptr, uint32_t bufferLength = 0) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_telemetry.adminCommandsExecuted++;
        NvmeCqe cqe{};
        cqe.commandId = sqe.commandId;
        cqe.sqId = 0;
        cqe.setPhase(true);
        cqe.setStatusCode(NVME_SC_SUCCESS);

        switch (sqe.opcode) {
            case NVME_ADMIN_IDENTIFY: {
                uint8_t cns = sqe.cdw10 & 0xFF;
                if (cns == NVME_IDENTIFY_CNS_CTRL) {
                    if (dataBuffer && bufferLength >= sizeof(NvmeIdentifyController)) {
                        std::memcpy(dataBuffer, &m_identCtrl, sizeof(NvmeIdentifyController));
                    }
                } else if (cns == NVME_IDENTIFY_CNS_NS) {
                    auto it = m_namespaces.find(sqe.nsid);
                    if (it != m_namespaces.end() && dataBuffer && bufferLength >= sizeof(NvmeIdentifyNamespace)) {
                        NvmeIdentifyNamespace identNs{};
                        identNs.nsze = it->second->getTotalBlocks();
                        identNs.ncap = it->second->getTotalBlocks();
                        identNs.nuse = it->second->getTotalBlocks() / 4;
                        identNs.nlbaf = 1;
                        identNs.flbas = (it->second->getBlockSize() == 4096) ? 1 : 0;
                        identNs.lbaf[0].lbaDataSizeShift = 9;  // 512B
                        identNs.lbaf[1].lbaDataSizeShift = 12; // 4096B
                        std::memcpy(dataBuffer, &identNs, sizeof(NvmeIdentifyNamespace));
                    } else {
                        cqe.setStatusCode(NVME_SC_INVALID_FIELD);
                    }
                } else if (cns == NVME_IDENTIFY_CNS_ACTIVE_NS) {
                    if (dataBuffer && bufferLength >= 16) {
                        uint32_t* activeList = reinterpret_cast<uint32_t*>(dataBuffer);
                        size_t idx = 0;
                        for (const auto& [nsid, ns] : m_namespaces) {
                            if (idx < (bufferLength / 4)) activeList[idx++] = nsid;
                        }
                    }
                }
                break;
            }
            case NVME_ADMIN_CREATE_IO_CQ: {
                uint16_t qid = sqe.cdw10 & 0xFFFF;
                uint16_t qsize = (sqe.cdw10 >> 16) & 0xFFFF;
                auto& qp = m_ioQueues[qid];
                qp.qid = qid;
                qp.cqSize = qsize + 1;
                qp.cqBase = sqe.prp1;
                qp.cqRing.resize(qp.cqSize);
                qp.cqPhase = true;
                qp.msixCount = 0;
                break;
            }
            case NVME_ADMIN_CREATE_IO_SQ: {
                uint16_t qid = sqe.cdw10 & 0xFFFF;
                uint16_t qsize = (sqe.cdw10 >> 16) & 0xFFFF;
                auto& qp = m_ioQueues[qid];
                qp.qid = qid;
                qp.sqSize = qsize + 1;
                qp.sqBase = sqe.prp1;
                qp.sqRing.resize(qp.sqSize);
                break;
            }
            case NVME_ADMIN_DELETE_IO_SQ: {
                uint16_t qid = sqe.cdw10 & 0xFFFF;
                auto it = m_ioQueues.find(qid);
                if (it != m_ioQueues.end()) {
                    it->second.sqRing.clear();
                    it->second.sqSize = 0;
                }
                break;
            }
            case NVME_ADMIN_DELETE_IO_CQ: {
                uint16_t qid = sqe.cdw10 & 0xFFFF;
                auto it = m_ioQueues.find(qid);
                if (it != m_ioQueues.end()) {
                    it->second.cqRing.clear();
                    it->second.cqSize = 0;
                    m_ioQueues.erase(it);
                }
                break;
            }
            case NVME_ADMIN_ABORT: {
                uint16_t sqid = sqe.cdw10 & 0xFFFF;
                uint16_t cid = (sqe.cdw10 >> 16) & 0xFFFF;
                abortCommand(sqid, cid);
                cqe.commandSpecific = 0;
                break;
            }
            case NVME_ADMIN_GET_LOG_PAGE: {
                uint8_t lid = sqe.cdw10 & 0xFF;
                if (lid == NVME_LOG_PAGE_SMART_HEALTH) {
                    if (dataBuffer && bufferLength >= sizeof(NvmeSmartLog)) {
                        std::memcpy(dataBuffer, &m_smartLog, sizeof(NvmeSmartLog));
                    }
                }
                break;
            }
            case NVME_ADMIN_SET_FEATURES:
            case NVME_ADMIN_GET_FEATURES: {
                // Return feature results in commandSpecific
                cqe.commandSpecific = 0x00080008; // 8 SQs, 8 CQs
                break;
            }
            case NVME_ADMIN_FORMAT_NVM: {
                auto it = m_namespaces.find(sqe.nsid);
                if (it != m_namespaces.end()) {
                    uint8_t lbafIdx = (sqe.cdw10 >> 8) & 0x0F;
                    uint32_t newBsize = (lbafIdx == 1) ? 4096 : 512;
                    it->second->format(newBsize);
                }
                break;
            }
            case NVME_ADMIN_ASYNC_EVENT: {
                m_pendingAers.push(sqe.commandId);
                break;
            }
            default:
                break;
        }

        return cqe;
    }

    NvmeCqe executeIoCommand(const NvmeSqe& sqe, void* dataBuffer = nullptr, uint32_t bufferLength = 0) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_telemetry.ioCommandsExecuted++;
        NvmeCqe cqe{};
        cqe.commandId = sqe.commandId;
        cqe.setPhase(true);
        cqe.setStatusCode(NVME_SC_SUCCESS);

        if (m_csts & NVME_CSTS_CFS) {
            cqe.setStatusCode(NVME_SC_INTERNAL_DEVICE_ERROR);
            return cqe;
        }

        auto it = m_namespaces.find(sqe.nsid);
        if (it == m_namespaces.end()) {
            cqe.setStatusCode(NVME_SC_INVALID_FIELD);
            return cqe;
        }
        auto ns = it->second;

        switch (sqe.opcode) {
            case NVME_NVM_READ: {
                uint64_t slba = (static_cast<uint64_t>(sqe.cdw11) << 32) | sqe.cdw10;
                uint16_t nlb = (sqe.cdw12 & 0xFFFF) + 1; // 0-based
                NtStatus st = ns->readBlocks(slba, nlb, dataBuffer);
                if (!NT_SUCCESS(st)) cqe.setStatusCode(NVME_SC_LBA_OUT_OF_RANGE);
                m_smartLog.hostReadCommands[0]++;
                m_smartLog.dataUnitsRead[0] += (nlb * ns->getBlockSize()) / 512;
                break;
            }
            case NVME_NVM_WRITE: {
                uint64_t slba = (static_cast<uint64_t>(sqe.cdw11) << 32) | sqe.cdw10;
                uint16_t nlb = (sqe.cdw12 & 0xFFFF) + 1;
                NtStatus st = ns->writeBlocks(slba, nlb, dataBuffer);
                if (!NT_SUCCESS(st)) cqe.setStatusCode(NVME_SC_LBA_OUT_OF_RANGE);
                m_smartLog.hostWriteCommands[0]++;
                m_smartLog.dataUnitsWritten[0] += (nlb * ns->getBlockSize()) / 512;
                break;
            }
            case NVME_NVM_WRITE_ZEROES: {
                uint64_t slba = (static_cast<uint64_t>(sqe.cdw11) << 32) | sqe.cdw10;
                uint16_t nlb = (sqe.cdw12 & 0xFFFF) + 1;
                NtStatus st = ns->writeZeroes(slba, nlb);
                if (!NT_SUCCESS(st)) cqe.setStatusCode(NVME_SC_LBA_OUT_OF_RANGE);
                break;
            }
            case NVME_NVM_DATASET_MGMT: {
                // TRIM / Deallocate
                uint8_t nr = (sqe.cdw10 & 0xFF) + 1;
                if (dataBuffer && bufferLength >= (nr * 16)) {
                    const uint8_t* p = reinterpret_cast<const uint8_t*>(dataBuffer);
                    for (uint8_t r = 0; r < nr; ++r) {
                        uint64_t slba = *reinterpret_cast<const uint64_t*>(p + (r * 16) + 8);
                        uint32_t nlb = *reinterpret_cast<const uint32_t*>(p + (r * 16) + 4);
                        ns->trimBlocks(slba, nlb);
                    }
                }
                break;
            }
            case NVME_NVM_FLUSH: {
                // Volatile cache flush
                break;
            }
            case NVME_ZNS_ZONE_MANAGEMENT_SEND: {
                uint8_t action = sqe.cdw10 & 0xFF;
                uint64_t slba = (static_cast<uint64_t>(sqe.cdw11) << 32) | sqe.cdw10;
                uint16_t sc = ns->zoneManagementSend(action, slba);
                cqe.setStatusCode(sc);
                break;
            }
            case NVME_ZNS_ZONE_MANAGEMENT_RECEIVE: {
                uint64_t slba = (static_cast<uint64_t>(sqe.cdw11) << 32) | sqe.cdw10;
                std::vector<NvmeZoneDescriptor> descs;
                uint16_t sc = ns->zoneManagementReceive(slba, descs);
                cqe.setStatusCode(sc);
                if (dataBuffer && bufferLength >= descs.size() * sizeof(NvmeZoneDescriptor)) {
                    std::memcpy(dataBuffer, descs.data(), descs.size() * sizeof(NvmeZoneDescriptor));
                }
                break;
            }
            case NVME_ZNS_ZONE_APPEND: {
                uint64_t zslba = (static_cast<uint64_t>(sqe.cdw11) << 32) | sqe.cdw10;
                uint16_t nlb = (sqe.cdw12 & 0xFFFF) + 1;
                uint64_t allocatedLba = 0;
                uint16_t sc = ns->zoneAppend(zslba, nlb, dataBuffer, allocatedLba);
                cqe.setStatusCode(sc);
                cqe.commandSpecific = static_cast<uint32_t>(allocatedLba & 0xFFFFFFFF);
                break;
            }
            default:
                cqe.setStatusCode(NVME_SC_INVALID_OPCODE);
                break;
        }

        return cqe;
    }

    NvmeCqe executeIoCommandWithPrp(const NvmeSqe& sqe, PrpDmaEngine& dma) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        NvmeCqe cqe{};
        cqe.commandId = sqe.commandId;
        cqe.setPhase(true);
        cqe.setStatusCode(NVME_SC_SUCCESS);

        if (m_csts & NVME_CSTS_CFS) {
            cqe.setStatusCode(NVME_SC_INTERNAL_DEVICE_ERROR);
            return cqe;
        }

        auto it = m_namespaces.find(sqe.nsid);
        if (it == m_namespaces.end()) {
            cqe.setStatusCode(NVME_SC_INVALID_FIELD);
            return cqe;
        }
        auto ns = it->second;

        uint64_t slba = (static_cast<uint64_t>(sqe.cdw11) << 32) | sqe.cdw10;
        uint16_t nlb = (sqe.cdw12 & 0xFFFF) + 1;
        size_t byteLength = static_cast<size_t>(nlb) * ns->getBlockSize();

        if (sqe.opcode == NVME_NVM_WRITE) {
            dma.transferToStorage(sqe.prp1, sqe.prp2, byteLength, *ns, slba, nlb);
            m_telemetry.prpTransfersExecuted++;
            m_telemetry.dmaBytesTransferred += byteLength;
            m_telemetry.ioCommandsExecuted++;
            m_smartLog.hostWriteCommands[0]++;
            m_smartLog.dataUnitsWritten[0] += byteLength / 512;
        } else if (sqe.opcode == NVME_NVM_READ) {
            dma.transferFromStorage(sqe.prp1, sqe.prp2, byteLength, *ns, slba, nlb);
            m_telemetry.prpTransfersExecuted++;
            m_telemetry.dmaBytesTransferred += byteLength;
            m_telemetry.ioCommandsExecuted++;
            m_smartLog.hostReadCommands[0]++;
            m_smartLog.dataUnitsRead[0] += byteLength / 512;
        } else {
            cqe.setStatusCode(NVME_SC_INVALID_OPCODE);
        }

        return cqe;
    }

    NvmeCqe executeIoCommandWithSgl(const NvmeSqe& sqe, const std::vector<NvmeSglDescriptor>& sglDescs, PrpDmaEngine& dma) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        NvmeCqe cqe{};
        cqe.commandId = sqe.commandId;
        cqe.setPhase(true);
        cqe.setStatusCode(NVME_SC_SUCCESS);

        if (m_csts & NVME_CSTS_CFS) {
            cqe.setStatusCode(NVME_SC_INTERNAL_DEVICE_ERROR);
            return cqe;
        }

        auto it = m_namespaces.find(sqe.nsid);
        if (it == m_namespaces.end()) {
            cqe.setStatusCode(NVME_SC_INVALID_FIELD);
            return cqe;
        }
        auto ns = it->second;

        uint64_t slba = (static_cast<uint64_t>(sqe.cdw11) << 32) | sqe.cdw10;
        uint16_t nlb = (sqe.cdw12 & 0xFFFF) + 1;
        size_t byteLength = static_cast<size_t>(nlb) * ns->getBlockSize();

        if (sqe.opcode == NVME_NVM_WRITE) {
            std::vector<uint8_t> tmp(byteLength);
            dma.readSglMemory(sglDescs, tmp.data(), byteLength);
            NtStatus st = ns->writeBlocks(slba, nlb, tmp.data());
            if (!NT_SUCCESS(st)) cqe.setStatusCode(NVME_SC_LBA_OUT_OF_RANGE);
            m_telemetry.ioCommandsExecuted++;
            m_telemetry.dmaBytesTransferred += byteLength;
            m_smartLog.hostWriteCommands[0]++;
            m_smartLog.dataUnitsWritten[0] += byteLength / 512;
        } else if (sqe.opcode == NVME_NVM_READ) {
            std::vector<uint8_t> tmp(byteLength);
            NtStatus st = ns->readBlocks(slba, nlb, tmp.data());
            if (!NT_SUCCESS(st)) cqe.setStatusCode(NVME_SC_LBA_OUT_OF_RANGE);
            dma.writeSglMemory(sglDescs, tmp.data(), byteLength);
            m_telemetry.ioCommandsExecuted++;
            m_telemetry.dmaBytesTransferred += byteLength;
            m_smartLog.hostReadCommands[0]++;
            m_smartLog.dataUnitsRead[0] += byteLength / 512;
        } else {
            cqe.setStatusCode(NVME_SC_INVALID_OPCODE);
        }
        return cqe;
    }

    uint8_t getDoorbellStride() const noexcept { return m_dstrd; }
    void setDoorbellStride(uint8_t dstrd) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_dstrd = dstrd & 0x0F;
        m_cap = (m_cap & ~0x0000000F00000000ULL) | (static_cast<uint64_t>(m_dstrd) << 32);
    }

    uint32_t getDoorbellOffset(uint16_t qid, bool isCq) const {
        uint32_t stride = 4 << m_dstrd;
        return NVME_REG_DBS + ((2 * qid) + (isCq ? 1 : 0)) * stride;
    }

    void createPerCpuQueuePairs(uint32_t numCpus, uint16_t qsize = 256) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_cpuToQueueId.clear();
        for (uint32_t cpu = 0; cpu < numCpus; ++cpu) {
            uint16_t qid = static_cast<uint16_t>(cpu + 1);
            auto& qp = m_ioQueues[qid];
            qp.qid = qid;
            qp.sqSize = qsize;
            qp.cqSize = qsize;
            qp.sqBase = 0x20000000ULL + (static_cast<uint64_t>(qid) * 0x10000);
            qp.cqBase = 0x30000000ULL + (static_cast<uint64_t>(qid) * 0x10000);
            qp.sqTail = 0;
            qp.sqHead = 0;
            qp.cqHead = 0;
            qp.cqTail = 0;
            qp.cqPhase = true;
            qp.msixCount = 0;
            qp.sqRing.assign(qsize, NvmeSqe{});
            qp.cqRing.assign(qsize, NvmeCqe{});
            m_cpuToQueueId[cpu] = qid;
        }
    }

    QueuePair* getPerCpuQueuePair(uint32_t cpuIndex) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_cpuToQueueId.find(cpuIndex);
        if (it != m_cpuToQueueId.end()) {
            auto qIt = m_ioQueues.find(it->second);
            if (qIt != m_ioQueues.end()) return &qIt->second;
        }
        return nullptr;
    }

    uint16_t getQueueIdForCpu(uint32_t cpuIndex) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_cpuToQueueId.find(cpuIndex);
        return (it != m_cpuToQueueId.end()) ? it->second : 0;
    }

    QueuePair* getQueuePair(uint16_t qid) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (qid == 0) return &m_adminQueue;
        auto it = m_ioQueues.find(qid);
        if (it != m_ioQueues.end()) return &it->second;
        return nullptr;
    }

    void submitAer(uint16_t commandId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_pendingAers.push(commandId);
    }

    bool triggerSmartAlert(uint8_t alertType, uint8_t info, uint8_t logPage = NVME_LOG_PAGE_SMART_HEALTH) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_pendingAers.empty()) return false;

        uint16_t cid = m_pendingAers.front();
        m_pendingAers.pop();

        NvmeCqe cqe{};
        cqe.commandId = cid;
        cqe.sqId = 0;
        cqe.commandSpecific = (static_cast<uint32_t>(logPage) << 16) |
                              (static_cast<uint32_t>(info) << 8) |
                              alertType;
        cqe.setPhase(m_adminQueue.cqPhase);
        cqe.setStatusCode(NVME_SC_SUCCESS);

        m_adminQueue.cqRing[m_adminQueue.cqTail] = cqe;
        m_adminQueue.cqTail = (m_adminQueue.cqTail + 1) % m_adminQueue.cqSize;
        if (m_adminQueue.cqTail == 0) m_adminQueue.cqPhase = !m_adminQueue.cqPhase;

        m_adminQueue.msixCount++;
        m_telemetry.msixInterruptsGenerated++;
        m_telemetry.aerEventsFired++;
        return true;
    }

    size_t getPendingAerCount() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_pendingAers.size();
    }

    NTSTATUS createIoQueuePair(uint16_t qid, uint16_t sqSize, uint16_t cqSize, uint64_t sqBase, uint64_t cqBase) {
        if (qid == 0 || sqSize == 0 || cqSize == 0) return STATUS_INVALID_PARAMETER;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto& qp = m_ioQueues[qid];
        qp.qid = qid;
        qp.sqSize = sqSize;
        qp.cqSize = cqSize;
        qp.sqBase = sqBase;
        qp.cqBase = cqBase;
        qp.sqTail = 0;
        qp.sqHead = 0;
        qp.cqHead = 0;
        qp.cqTail = 0;
        qp.cqPhase = true;
        qp.msixCount = 0;
        qp.sqRing.resize(sqSize);
        qp.cqRing.resize(cqSize);
        return STATUS_SUCCESS;
    }

    NTSTATUS deleteIoQueuePair(uint16_t qid) {
        if (qid == 0) return STATUS_INVALID_PARAMETER;
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_ioQueues.find(qid);
        if (it == m_ioQueues.end()) return STATUS_NOT_FOUND;
        m_ioQueues.erase(it);
        return STATUS_SUCCESS;
    }

    NTSTATUS postSqe(uint16_t qid, const NvmeSqe& sqe) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        QueuePair* qp = nullptr;
        if (qid == 0) qp = &m_adminQueue;
        else {
            auto it = m_ioQueues.find(qid);
            if (it != m_ioQueues.end()) qp = &it->second;
        }
        if (!qp) return STATUS_NOT_FOUND;

        qp->sqRing[qp->sqTail] = sqe;
        qp->sqTail = (qp->sqTail + 1) % qp->sqSize;
        return STATUS_SUCCESS;
    }

    void ringDoorbell(uint16_t qid, bool isCq, uint16_t val) {
        uint32_t offset = NVME_REG_DBS + (qid * 8) + (isCq ? 4 : 0);
        writeReg32(offset, val);
    }

    bool abortCommand(uint16_t qid, uint16_t cid) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_telemetry.abortedCommands++;
        QueuePair* qp = nullptr;
        if (qid == 0) qp = &m_adminQueue;
        else {
            auto it = m_ioQueues.find(qid);
            if (it != m_ioQueues.end()) qp = &it->second;
        }
        if (qp && qp->cqSize > 0) {
            NvmeCqe cqe{};
            cqe.commandId = cid;
            cqe.sqId = qid;
            cqe.setPhase(qp->cqPhase);
            cqe.setStatusCode(NVME_SC_ABORTED_BY_REQUEST);
            qp->cqRing[qp->cqTail] = cqe;
            qp->cqTail = (qp->cqTail + 1) % qp->cqSize;
            if (qp->cqTail == 0) qp->cqPhase = !qp->cqPhase;
            qp->msixCount++;
            m_telemetry.msixInterruptsGenerated++;
            return true;
        }
        return false;
    }

    void triggerFatalError() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_csts |= NVME_CSTS_CFS;
        m_telemetry.fatalErrorsInjected++;
    }

    void recoverController() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_csts = 0;
        m_cc = 0;
        m_adminQueue.sqTail = 0;
        m_adminQueue.sqHead = 0;
        m_adminQueue.cqHead = 0;
        m_adminQueue.cqTail = 0;
        m_adminQueue.cqPhase = true;
        m_adminQueue.msixCount = 0;
        m_ioQueues.clear();

        m_cc = NVME_CC_EN | NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16;
        m_csts = NVME_CSTS_RDY;
        m_telemetry.recoveriesCompleted++;
    }

    [[nodiscard]] NVME_TELEMETRY getTelemetry() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_telemetry;
    }

    void resetTelemetry() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_telemetry = NVME_TELEMETRY{};
    }

    void resetForTesting() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_csts = NVME_CSTS_RDY;
        m_cc = NVME_CC_EN | NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16;
        m_adminQueue.sqTail = 0;
        m_adminQueue.sqHead = 0;
        m_adminQueue.cqHead = 0;
        m_adminQueue.cqTail = 0;
        m_adminQueue.cqPhase = true;
        m_adminQueue.msixCount = 0;
        m_ioQueues.clear();
        m_telemetry = NVME_TELEMETRY{};
    }

    size_t getQueueCount() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return 1 + m_ioQueues.size();
    }

    uint64_t getMsixInterrupts(uint16_t qid) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (qid == 0) return m_adminQueue.msixCount;
        auto it = m_ioQueues.find(qid);
        if (it != m_ioQueues.end()) return it->second.msixCount;
        return 0;
    }

    std::shared_ptr<NvmeNamespace> getNamespace(uint32_t nsid) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_namespaces.find(nsid);
        if (it != m_namespaces.end()) return it->second;
        return nullptr;
    }

    std::vector<std::shared_ptr<NvmeNamespace>> getAllNamespaces() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<std::shared_ptr<NvmeNamespace>> res;
        for (const auto& [nsid, ns] : m_namespaces) res.push_back(ns);
        return res;
    }

    const NvmeSmartLog& getSmartLog() const { return m_smartLog; }

private:
    void processQueueLocked(QueuePair& qp) {
        if (m_csts & NVME_CSTS_CFS) return;
        while (qp.sqHead != qp.sqTail) {
            NvmeSqe sqe = qp.sqRing[qp.sqHead];
            qp.sqHead = (qp.sqHead + 1) % qp.sqSize;

            NvmeCqe cqe{};
            if (qp.qid == 0) {
                cqe = executeAdminCommand(sqe, nullptr, 0);
            } else {
                cqe = executeIoCommand(sqe, nullptr, 0);
            }
            cqe.sqId = qp.qid;
            cqe.sqHead = qp.sqHead;
            cqe.setPhase(qp.cqPhase);

            qp.cqRing[qp.cqTail] = cqe;
            qp.cqTail = (qp.cqTail + 1) % qp.cqSize;
            if (qp.cqTail == 0) {
                qp.cqPhase = !qp.cqPhase;
            }
            qp.msixCount++;
            m_telemetry.msixInterruptsGenerated++;
        }
    }
};

// ============================================================================
// 5. Universal Flash Storage (UFS 3.1 & 4.0) Subsystem
// ============================================================================

class UfsHostController {
    uint32_t m_version{0x00040000}; // UFS 4.0
    bool m_enabled{true};
    std::vector<std::shared_ptr<storage::IBlockDevice>> m_luns;

public:
    UfsHostController() {
        // Pre-seed LUN 0: UFS Boot / OS Partition (128GB, 4096B blocks)
        m_luns.push_back(std::make_shared<FlashDiskDevice>(L"\\Device\\Harddisk2\\DR0", 128ULL * 1024 * 1024 * 1024, 4096));
        // LUN 1: UFS Data Partition (128GB, 4096B blocks)
        m_luns.push_back(std::make_shared<FlashDiskDevice>(L"\\Device\\Harddisk2\\DR1", 128ULL * 1024 * 1024 * 1024, 4096));
    }

    uint32_t getVersion() const { return m_version; }
    bool isEnabled() const { return m_enabled; }
    size_t getLunCount() const { return m_luns.size(); }
    std::shared_ptr<storage::IBlockDevice> getLun(size_t index) const {
        if (index < m_luns.size()) return m_luns[index];
        return nullptr;
    }
};

// ============================================================================
// 6. eMMC 5.1 & SD Host Controller (SDHCI 4.2) Subsystem
// ============================================================================

class EmmcHostController {
    uint32_t m_specVersion{0x0501}; // eMMC 5.1
    std::shared_ptr<storage::IBlockDevice> m_userPartition;

public:
    EmmcHostController() {
        // Pre-seed eMMC User Data Area (64GB, 512B sectors)
        m_userPartition = std::make_shared<FlashDiskDevice>(L"\\Device\\Harddisk3\\DR0", 64ULL * 1024 * 1024 * 1024, 512);
    }

    uint32_t getSpecVersion() const { return m_specVersion; }
    std::shared_ptr<storage::IBlockDevice> getUserPartition() const { return m_userPartition; }
};

// ============================================================================
// 7. AHCI 1.3.1 SATA SSD Controller (Native Command Queuing - NCQ)
// ============================================================================

class AhciSataController {
    uint32_t m_version{0x00010301}; // AHCI 1.3.1
    std::array<std::shared_ptr<storage::IBlockDevice>, 4> m_ports;
    uint32_t m_activeTags{0}; // NCQ 32-slot bitmask

public:
    AhciSataController() {
        // Port 0: SATA 6Gbps Solid State Drive (512GB, 512B sectors)
        m_ports[0] = std::make_shared<FlashDiskDevice>(L"\\Device\\Harddisk0\\DR0", 512ULL * 1024 * 1024 * 1024, 512);
    }

    uint32_t getVersion() const { return m_version; }
    std::shared_ptr<storage::IBlockDevice> getPort(size_t index) const {
        if (index < m_ports.size()) return m_ports[index];
        return nullptr;
    }
    uint32_t getActiveTags() const { return m_activeTags; }
    void submitNcqCommand(uint8_t tag) {
        if (tag < 32) m_activeTags |= (1U << tag);
    }
    void completeNcqCommand(uint8_t tag) {
        if (tag < 32) m_activeTags &= ~(1U << tag);
    }
};

// ============================================================================
// 7.5. Windows StorPort NVMe Miniport Engine (stornvme.sys)
// ============================================================================

struct STORPORT_TELEMETRY {
    uint64_t srbTotal{0};
    uint64_t srbDispatched{0};
    uint64_t srbCompleted{0};
    uint64_t scsiReads{0};
    uint64_t scsiWrites{0};
    uint64_t scsiFlushes{0};
    uint64_t scsiUnmaps{0};
    uint64_t scsiInquiries{0};
    uint64_t scsiReadCapacities{0};
    uint64_t busResets{0};
    uint64_t abortedSrbs{0};
    uint64_t notificationsSent{0};
};

class StorNvmeMiniport {
private:
    std::shared_ptr<NvmeController> m_controller;
    std::shared_ptr<PrpDmaEngine>   m_dmaEngine;
    PORT_CONFIGURATION_INFORMATION  m_configInfo{};
    bool                            m_initialized{false};
    mutable std::mutex              m_mutex;

public:
    using STORPORT_TELEMETRY = micant::nvme::STORPORT_TELEMETRY;
    STORPORT_TELEMETRY m_telemetry{};

    StorNvmeMiniport(std::shared_ptr<NvmeController> ctrl, std::shared_ptr<PrpDmaEngine> dma)
        : m_controller(std::move(ctrl)), m_dmaEngine(std::move(dma)) {}

    NTSTATUS findAdapter(PORT_CONFIGURATION_INFORMATION* configInfo) {
        if (!configInfo) return STATUS_INVALID_PARAMETER;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_configInfo = *configInfo;
        m_configInfo.MaximumTransferLength = 2 * 1024 * 1024; // 2MB
        m_configInfo.NumberOfPhysicalBreaks = 512;
        m_configInfo.Dma64BitAddresses = 1;
        m_configInfo.NumberOfBuses = 1;
        m_configInfo.MaximumNumberOfTargets = 4;
        m_configInfo.MaximumNumberOfLogicalUnits = 8;
        *configInfo = m_configInfo;
        return STATUS_SUCCESS;
    }

    NTSTATUS initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_controller) return STATUS_DEVICE_NOT_READY;
        m_controller->writeReg32(NVME_REG_CC, NVME_CC_EN | NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16);
        m_initialized = true;
        return STATUS_SUCCESS;
    }

    NTSTATUS startIo(SCSI_REQUEST_BLOCK* srb) {
        if (!srb) return STATUS_INVALID_PARAMETER;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.srbTotal++;
        m_telemetry.srbDispatched++;

        if (srb->Function == SRB_FUNCTION_RESET_LOGICAL_UNIT || srb->Function == SRB_FUNCTION_RESET_DEVICE || srb->Function == SRB_FUNCTION_RESET_BUS) {
            if (m_controller) {
                m_controller->recoverController();
            }
            m_telemetry.busResets++;
            srb->SrbStatus = SRB_STATUS_SUCCESS;
            srb->ScsiStatus = SCSISTAT_GOOD;
            m_telemetry.srbCompleted++;
            m_telemetry.notificationsSent += 2;
            return STATUS_SUCCESS;
        }

        if (srb->Function == SRB_FUNCTION_ABORT_COMMAND) {
            m_telemetry.abortedSrbs++;
            srb->SrbStatus = SRB_STATUS_ABORTED;
            m_telemetry.srbCompleted++;
            m_telemetry.notificationsSent += 2;
            return STATUS_SUCCESS;
        }

        if (!m_initialized || !m_controller || !m_controller->isReady()) {
            srb->SrbStatus = SRB_STATUS_BUSY;
            srb->ScsiStatus = SCSISTAT_BUSY;
            return STATUS_DEVICE_NOT_READY;
        }

        if (srb->Function != SRB_FUNCTION_EXECUTE_SCSI) {
            srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
            return STATUS_NOT_SUPPORTED;
        }

        uint8_t opcode = srb->Cdb[0];
        uint32_t nsid = static_cast<uint32_t>(srb->Lun + 1);
        if (nsid == 0) nsid = 1;
        auto ns = m_controller->getNamespace(nsid);

        switch (opcode) {
            case SCSIOP_INQUIRY: {
                m_telemetry.scsiInquiries++;
                if (!srb->DataBuffer || srb->DataTransferLength < 36) {
                    srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;
                    srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
                    return STATUS_BUFFER_TOO_SMALL;
                }
                uint8_t* inq = static_cast<uint8_t*>(srb->DataBuffer);
                std::memset(inq, 0, srb->DataTransferLength);
                inq[0] = 0x00; // Direct Access Block Device
                inq[1] = 0x00; // Non-removable
                inq[2] = 0x06; // SPC-4 / SCSI compliance
                inq[3] = 0x02; // Response data format
                inq[4] = 31;   // Additional length
                std::memcpy(inq + 8, "MicaNT  ", 8);
                std::memcpy(inq + 16, "TitanNVMe Flash ", 16);
                std::memcpy(inq + 32, "2.00", 4);
                srb->SrbStatus = SRB_STATUS_SUCCESS;
                srb->ScsiStatus = SCSISTAT_GOOD;
                break;
            }

            case SCSIOP_TEST_UNIT_READY: {
                srb->SrbStatus = SRB_STATUS_SUCCESS;
                srb->ScsiStatus = SCSISTAT_GOOD;
                break;
            }

            case SCSIOP_READ_CAPACITY16: {
                m_telemetry.scsiReadCapacities++;
                if (!ns || !srb->DataBuffer || srb->DataTransferLength < 32) {
                    srb->SrbStatus = SRB_STATUS_NO_DEVICE;
                    srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
                    return STATUS_NO_SUCH_DEVICE;
                }
                uint8_t* cap = static_cast<uint8_t*>(srb->DataBuffer);
                std::memset(cap, 0, srb->DataTransferLength);
                uint64_t lastLba = ns->getTotalBlocks() - 1;
                uint32_t blkSize = ns->getBlockSize();
                cap[0] = static_cast<uint8_t>((lastLba >> 56) & 0xFF);
                cap[1] = static_cast<uint8_t>((lastLba >> 48) & 0xFF);
                cap[2] = static_cast<uint8_t>((lastLba >> 40) & 0xFF);
                cap[3] = static_cast<uint8_t>((lastLba >> 32) & 0xFF);
                cap[4] = static_cast<uint8_t>((lastLba >> 24) & 0xFF);
                cap[5] = static_cast<uint8_t>((lastLba >> 16) & 0xFF);
                cap[6] = static_cast<uint8_t>((lastLba >> 8) & 0xFF);
                cap[7] = static_cast<uint8_t>(lastLba & 0xFF);
                cap[8] = static_cast<uint8_t>((blkSize >> 24) & 0xFF);
                cap[9] = static_cast<uint8_t>((blkSize >> 16) & 0xFF);
                cap[10] = static_cast<uint8_t>((blkSize >> 8) & 0xFF);
                cap[11] = static_cast<uint8_t>(blkSize & 0xFF);
                srb->SrbStatus = SRB_STATUS_SUCCESS;
                srb->ScsiStatus = SCSISTAT_GOOD;
                break;
            }

            case SCSIOP_READ10: {
                m_telemetry.scsiReads++;
                if (!ns || !srb->DataBuffer) {
                    srb->SrbStatus = SRB_STATUS_NO_DEVICE;
                    return STATUS_NO_SUCH_DEVICE;
                }
                uint64_t lba = (static_cast<uint64_t>(srb->Cdb[2]) << 24) |
                               (static_cast<uint64_t>(srb->Cdb[3]) << 16) |
                               (static_cast<uint64_t>(srb->Cdb[4]) << 8)  |
                               static_cast<uint64_t>(srb->Cdb[5]);
                uint32_t count = (static_cast<uint32_t>(srb->Cdb[7]) << 8) | static_cast<uint32_t>(srb->Cdb[8]);
                NtStatus st = ns->readBlocks(lba, count, srb->DataBuffer);
                srb->SrbStatus = NT_SUCCESS(st) ? SRB_STATUS_SUCCESS : SRB_STATUS_ERROR;
                srb->ScsiStatus = NT_SUCCESS(st) ? SCSISTAT_GOOD : SCSISTAT_CHECK_CONDITION;
                break;
            }

            case SCSIOP_READ16: {
                m_telemetry.scsiReads++;
                if (!ns || !srb->DataBuffer) {
                    srb->SrbStatus = SRB_STATUS_NO_DEVICE;
                    return STATUS_NO_SUCH_DEVICE;
                }
                uint64_t lba = 0;
                for (int i = 0; i < 8; ++i) {
                    lba = (lba << 8) | srb->Cdb[2 + i];
                }
                uint32_t count = (static_cast<uint32_t>(srb->Cdb[10]) << 24) |
                                 (static_cast<uint32_t>(srb->Cdb[11]) << 16) |
                                 (static_cast<uint32_t>(srb->Cdb[12]) << 8)  |
                                 static_cast<uint32_t>(srb->Cdb[13]);
                NtStatus st = ns->readBlocks(lba, count, srb->DataBuffer);
                srb->SrbStatus = NT_SUCCESS(st) ? SRB_STATUS_SUCCESS : SRB_STATUS_ERROR;
                srb->ScsiStatus = NT_SUCCESS(st) ? SCSISTAT_GOOD : SCSISTAT_CHECK_CONDITION;
                break;
            }

            case SCSIOP_WRITE10: {
                m_telemetry.scsiWrites++;
                if (!ns || !srb->DataBuffer) {
                    srb->SrbStatus = SRB_STATUS_NO_DEVICE;
                    return STATUS_NO_SUCH_DEVICE;
                }
                uint64_t lba = (static_cast<uint64_t>(srb->Cdb[2]) << 24) |
                               (static_cast<uint64_t>(srb->Cdb[3]) << 16) |
                               (static_cast<uint64_t>(srb->Cdb[4]) << 8)  |
                               static_cast<uint64_t>(srb->Cdb[5]);
                uint32_t count = (static_cast<uint32_t>(srb->Cdb[7]) << 8) | static_cast<uint32_t>(srb->Cdb[8]);
                NtStatus st = ns->writeBlocks(lba, count, srb->DataBuffer);
                srb->SrbStatus = NT_SUCCESS(st) ? SRB_STATUS_SUCCESS : SRB_STATUS_ERROR;
                srb->ScsiStatus = NT_SUCCESS(st) ? SCSISTAT_GOOD : SCSISTAT_CHECK_CONDITION;
                break;
            }

            case SCSIOP_WRITE16: {
                m_telemetry.scsiWrites++;
                if (!ns || !srb->DataBuffer) {
                    srb->SrbStatus = SRB_STATUS_NO_DEVICE;
                    return STATUS_NO_SUCH_DEVICE;
                }
                uint64_t lba = 0;
                for (int i = 0; i < 8; ++i) {
                    lba = (lba << 8) | srb->Cdb[2 + i];
                }
                uint32_t count = (static_cast<uint32_t>(srb->Cdb[10]) << 24) |
                                 (static_cast<uint32_t>(srb->Cdb[11]) << 16) |
                                 (static_cast<uint32_t>(srb->Cdb[12]) << 8)  |
                                 static_cast<uint32_t>(srb->Cdb[13]);
                NtStatus st = ns->writeBlocks(lba, count, srb->DataBuffer);
                srb->SrbStatus = NT_SUCCESS(st) ? SRB_STATUS_SUCCESS : SRB_STATUS_ERROR;
                srb->ScsiStatus = NT_SUCCESS(st) ? SCSISTAT_GOOD : SCSISTAT_CHECK_CONDITION;
                break;
            }

            case SCSIOP_SYNCHRONIZE_CACHE10:
            case SCSIOP_SYNCHRONIZE_CACHE16: {
                m_telemetry.scsiFlushes++;
                srb->SrbStatus = SRB_STATUS_SUCCESS;
                srb->ScsiStatus = SCSISTAT_GOOD;
                break;
            }

            case SCSIOP_UNMAP: {
                m_telemetry.scsiUnmaps++;
                if (ns && srb->DataBuffer && srb->DataTransferLength >= 24) {
                    const uint8_t* p = static_cast<const uint8_t*>(srb->DataBuffer);
                    uint16_t bdl = (static_cast<uint16_t>(p[2]) << 8) | p[3];
                    size_t numDescs = bdl / 16;
                    for (size_t d = 0; d < numDescs; ++d) {
                        const uint8_t* desc = p + 8 + (d * 16);
                        uint64_t slba = 0;
                        for (int i = 0; i < 8; ++i) slba = (slba << 8) | desc[i];
                        uint32_t nlb = (static_cast<uint32_t>(desc[8]) << 24) |
                                       (static_cast<uint32_t>(desc[9]) << 16) |
                                       (static_cast<uint32_t>(desc[10]) << 8) |
                                       static_cast<uint32_t>(desc[11]);
                        ns->trimBlocks(slba, nlb);
                    }
                }
                srb->SrbStatus = SRB_STATUS_SUCCESS;
                srb->ScsiStatus = SCSISTAT_GOOD;
                break;
            }

            case SCSIOP_REPORT_LUNS: {
                if (srb->DataBuffer && srb->DataTransferLength >= 16) {
                    uint8_t* p = static_cast<uint8_t*>(srb->DataBuffer);
                    std::memset(p, 0, srb->DataTransferLength);
                    auto allNs = m_controller->getAllNamespaces();
                    uint32_t lunDataLen = static_cast<uint32_t>(allNs.size() * 8);
                    p[0] = static_cast<uint8_t>((lunDataLen >> 24) & 0xFF);
                    p[1] = static_cast<uint8_t>((lunDataLen >> 16) & 0xFF);
                    p[2] = static_cast<uint8_t>((lunDataLen >> 8) & 0xFF);
                    p[3] = static_cast<uint8_t>(lunDataLen & 0xFF);
                    for (size_t i = 0; i < allNs.size() && (8 + (i + 1) * 8) <= srb->DataTransferLength; ++i) {
                        p[8 + i * 8] = static_cast<uint8_t>(i);
                    }
                }
                srb->SrbStatus = SRB_STATUS_SUCCESS;
                srb->ScsiStatus = SCSISTAT_GOOD;
                break;
            }

            default:
                srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
                srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
                break;
        }

        m_telemetry.srbCompleted++;
        m_telemetry.notificationsSent += 2;
        return STATUS_SUCCESS;
    }

    NTSTATUS resetBus(uint32_t pathId) {
        (void)pathId;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.busResets++;
        if (m_controller) {
            m_controller->recoverController();
        }
        return STATUS_SUCCESS;
    }

    [[nodiscard]] STORPORT_TELEMETRY getTelemetry() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_telemetry;
    }

    void resetTelemetry() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry = STORPORT_TELEMETRY{};
    }

    void resetForTesting() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_initialized = true;
        m_telemetry = STORPORT_TELEMETRY{};
    }
};

// ============================================================================
// 7.5. Native Windows NT NVMe Class Driver (nvme.sys)
// ============================================================================

struct BatchIoRequest {
    uint8_t  opcode{NVME_NVM_READ};
    uint32_t nsid{1};
    uint64_t slba{0};
    uint16_t nlb{1};
    void*    buffer{nullptr};
    NtStatus status{NtStatus::Success};
};

class NvmeBlockDriver {
    std::shared_ptr<NvmeController> m_controller;
    std::shared_ptr<PrpDmaEngine>   m_dma;
    std::atomic<bool>               m_initialized{false};
    std::wstring                    m_controllerName{L"\\Device\\NvmeController0"};
    std::wstring                    m_diskName{L"\\Device\\Harddisk0\\DR0"};
    std::wstring                    m_symlink{L"\\\\.\\PhysicalDrive0"};

    std::atomic<uint64_t> m_totalReads{0};
    std::atomic<uint64_t> m_totalWrites{0};
    std::atomic<uint64_t> m_totalBytesRead{0};
    std::atomic<uint64_t> m_totalBytesWritten{0};
    std::atomic<uint64_t> m_totalIoctls{0};

    NvmeBlockDriver() = default;

public:
    static NvmeBlockDriver& Instance() {
        static NvmeBlockDriver s_instance;
        return s_instance;
    }

    void initialize(std::shared_ptr<NvmeController> ctrl = nullptr, std::shared_ptr<PrpDmaEngine> dma = nullptr) {
        if (ctrl) m_controller = ctrl;
        else m_controller = std::make_shared<NvmeController>(NvmeVersion::Version_2_0);

        if (dma) m_dma = dma;
        else m_dma = std::make_shared<PrpDmaEngine>();

        // Pre-create 8 per-CPU core queue pairs for SMP affinity
        m_controller->createPerCpuQueuePairs(8, 256);

        m_initialized.store(true);
    }

    bool isInitialized() const noexcept { return m_initialized.load(); }

    std::shared_ptr<NvmeController> getController() const { return m_controller; }
    std::shared_ptr<PrpDmaEngine> getDma() const { return m_dma; }

    const std::wstring& getControllerDeviceName() const noexcept { return m_controllerName; }
    const std::wstring& getDiskDeviceName() const noexcept { return m_diskName; }
    const std::wstring& getDosDeviceSymlink() const noexcept { return m_symlink; }

    NtStatus dispatchRead(uint32_t nsid, uint64_t lba, uint32_t count, void* buffer) {
        if (!m_controller) return NtStatus::DeviceNotReady;
        auto ns = m_controller->getNamespace(nsid);
        if (!ns) return NtStatus::NoSuchDevice;

        NtStatus st = ns->readBlocks(lba, count, buffer);
        if (NT_SUCCESS(st)) {
            m_totalReads++;
            m_totalBytesRead += static_cast<uint64_t>(count) * ns->getBlockSize();
        }
        return st;
    }

    NtStatus dispatchWrite(uint32_t nsid, uint64_t lba, uint32_t count, const void* buffer) {
        if (!m_controller) return NtStatus::DeviceNotReady;
        auto ns = m_controller->getNamespace(nsid);
        if (!ns) return NtStatus::NoSuchDevice;

        NtStatus st = ns->writeBlocks(lba, count, buffer);
        if (NT_SUCCESS(st)) {
            m_totalWrites++;
            m_totalBytesWritten += static_cast<uint64_t>(count) * ns->getBlockSize();
        }
        return st;
    }

    NtStatus dispatchDeviceControl(
        uint32_t ioControlCode,
        const void* inBuffer,
        size_t inBufferSize,
        void* outBuffer,
        size_t outBufferSize,
        size_t& bytesReturned
    ) {
        bytesReturned = 0;
        if (!m_controller) return NtStatus::DeviceNotReady;
        m_totalIoctls++;

        switch (ioControlCode) {
            case IOCTL_STORAGE_QUERY_PROPERTY_CODE: {
                if (!outBuffer || outBufferSize < sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
                    return NtStatus::BufferTooSmall;
                }
                auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(outBuffer);
                desc->Version = sizeof(STORAGE_DEVICE_DESCRIPTOR);
                desc->Size = static_cast<uint32_t>(std::min<size_t>(outBufferSize, sizeof(STORAGE_DEVICE_DESCRIPTOR) + 128));
                desc->DeviceType = 0; // Direct Access Block Device
                desc->DeviceTypeModifier = 0;
                desc->RemovableMedia = 0;
                desc->CommandQueueing = 1;
                desc->BusType = BusTypeNvme;
                desc->RawPropertiesLength = 0;

                if (outBufferSize >= sizeof(STORAGE_DEVICE_DESCRIPTOR) + 64) {
                    char* rawStr = reinterpret_cast<char*>(desc) + sizeof(STORAGE_DEVICE_DESCRIPTOR);
                    desc->VendorIdOffset = sizeof(STORAGE_DEVICE_DESCRIPTOR);
                    std::strcpy(rawStr, "MicaNT");
                    desc->ProductIdOffset = desc->VendorIdOffset + 8;
                    std::strcpy(rawStr + 8, "Enterprise NVMe SSD PCIe 4.0 4TB");
                    desc->ProductRevisionOffset = desc->ProductIdOffset + 36;
                    std::strcpy(rawStr + 44, "2.0d");
                    desc->SerialNumberOffset = desc->ProductRevisionOffset + 6;
                    std::strcpy(rawStr + 50, "MICA-NVME-0001");
                }
                bytesReturned = desc->Size;
                return NtStatus::Success;
            }

            case 0x002D1080: /* IOCTL_STORAGE_GET_DEVICE_NUMBER */ {
                if (!outBuffer || outBufferSize < sizeof(storage::STORAGE_DEVICE_NUMBER)) {
                    return NtStatus::BufferTooSmall;
                }
                auto* sdn = reinterpret_cast<storage::STORAGE_DEVICE_NUMBER*>(outBuffer);
                sdn->DeviceType = storage::IOCTL_DISK_BASE;
                sdn->DeviceNumber = 0;
                sdn->PartitionNumber = 0;
                bytesReturned = sizeof(storage::STORAGE_DEVICE_NUMBER);
                return NtStatus::Success;
            }

            case 0x000700A0: /* IOCTL_DISK_GET_DRIVE_GEOMETRY_EX */ {
                if (!outBuffer || outBufferSize < sizeof(storage::DISK_GEOMETRY_EX)) {
                    return NtStatus::BufferTooSmall;
                }
                auto ns = m_controller->getNamespace(1);
                if (!ns) return NtStatus::NoSuchDevice;

                auto* geomEx = reinterpret_cast<storage::DISK_GEOMETRY_EX*>(outBuffer);
                geomEx->Geometry = ns->getGeometry();
                geomEx->DiskSize.quadPart = static_cast<int64_t>(ns->getTotalBytes());
                bytesReturned = sizeof(storage::DISK_GEOMETRY_EX);
                return NtStatus::Success;
            }

            case IOCTL_STORAGE_MANAGE_DATA_SET_ATTRIBUTES_CODE: {
                auto ns = m_controller->getNamespace(1);
                if (!ns) return NtStatus::NoSuchDevice;
                ns->trimBlocks(0, 100);
                bytesReturned = 0;
                return NtStatus::Success;
            }

            default:
                return NtStatus::NotImplemented;
        }
    }

    void submitBatchIo(uint32_t cpuCore, std::vector<BatchIoRequest>& requests) {
        if (!m_controller) return;
        auto* qp = m_controller->getPerCpuQueuePair(cpuCore);
        if (!qp) return;

        for (auto& req : requests) {
            auto ns = m_controller->getNamespace(req.nsid);
            if (!ns) {
                req.status = NtStatus::NoSuchDevice;
                continue;
            }

            if (req.opcode == NVME_NVM_WRITE) {
                req.status = ns->writeBlocks(req.slba, req.nlb, req.buffer);
                if (NT_SUCCESS(req.status)) {
                    m_totalWrites++;
                    m_totalBytesWritten += static_cast<uint64_t>(req.nlb) * ns->getBlockSize();
                }
            } else if (req.opcode == NVME_NVM_READ) {
                req.status = ns->readBlocks(req.slba, req.nlb, req.buffer);
                if (NT_SUCCESS(req.status)) {
                    m_totalReads++;
                    m_totalBytesRead += static_cast<uint64_t>(req.nlb) * ns->getBlockSize();
                }
            }
            qp->msixCount++;
        }
    }

    uint64_t getTotalReads() const noexcept { return m_totalReads.load(); }
    uint64_t getTotalWrites() const noexcept { return m_totalWrites.load(); }
    uint64_t getTotalBytesRead() const noexcept { return m_totalBytesRead.load(); }
    uint64_t getTotalBytesWritten() const noexcept { return m_totalBytesWritten.load(); }
    uint64_t getTotalIoctls() const noexcept { return m_totalIoctls.load(); }

    void resetForTesting() {
        m_totalReads.store(0);
        m_totalWrites.store(0);
        m_totalBytesRead.store(0);
        m_totalBytesWritten.store(0);
        m_totalIoctls.store(0);
        if (m_controller) m_controller->resetForTesting();
    }
};

// ============================================================================
// 8. Unified Sovereign Flash Storage Manager (TitanFlash)
// ============================================================================

class TitanFlashSubsystem {
    std::shared_ptr<NvmeController>   m_nvme;
    std::shared_ptr<PrpDmaEngine>     m_dma;
    std::shared_ptr<StorNvmeMiniport> m_miniport;
    std::shared_ptr<UfsHostController> m_ufs;
    std::shared_ptr<EmmcHostController> m_emmc;
    std::shared_ptr<AhciSataController> m_ahci;
    std::atomic<bool> m_initialized{false};
    mutable std::mutex m_mutex;

    TitanFlashSubsystem() = default;

public:
    static TitanFlashSubsystem& Instance() {
        static TitanFlashSubsystem s_inst;
        return s_inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized.load()) return;

        m_dma = std::make_shared<PrpDmaEngine>();
        m_nvme = std::make_shared<NvmeController>(NvmeVersion::Version_2_0);
        m_miniport = std::make_shared<StorNvmeMiniport>(m_nvme, m_dma);
        m_miniport->initialize();

        m_ufs = std::make_shared<UfsHostController>();
        m_emmc = std::make_shared<EmmcHostController>();
        m_ahci = std::make_shared<AhciSataController>();

        m_initialized.store(true);
    }

    void resetForTesting() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_nvme) m_nvme->resetForTesting();
        if (m_miniport) m_miniport->resetForTesting();
    }

    std::shared_ptr<NvmeController>   getNvme() const { return m_nvme; }
    std::shared_ptr<PrpDmaEngine>     getDmaEngine() const { return m_dma; }
    std::shared_ptr<StorNvmeMiniport> getMiniport() const { return m_miniport; }
    std::shared_ptr<UfsHostController> getUfs() const { return m_ufs; }
    std::shared_ptr<EmmcHostController> getEmmc() const { return m_emmc; }
    std::shared_ptr<AhciSataController> getAhci() const { return m_ahci; }
};

// ============================================================================
// 9. Windows StorPort Driver C ABI Exports (stornvme.sys)
// ============================================================================

extern "C" {

inline BOOL WINAPI NvmeControllerReset() {
    auto nvme = TitanFlashSubsystem::Instance().getNvme();
    if (!nvme) return FALSE;
    nvme->writeReg32(NVME_REG_CC, 0); // disable
    nvme->writeReg32(NVME_REG_CC, NVME_CC_EN | NVME_CC_IOSQES_64 | NVME_CC_IOCQES_16);
    return nvme->isReady() ? TRUE : FALSE;
}

inline BOOL WINAPI NvmeSubmitAdminCommand(
    const NvmeSqe* sqe,
    NvmeCqe* cqe,
    void* buffer,
    uint32_t bufferLength
) {
    if (!sqe || !cqe) return FALSE;
    auto nvme = TitanFlashSubsystem::Instance().getNvme();
    if (!nvme) return FALSE;

    *cqe = nvme->executeAdminCommand(*sqe, buffer, bufferLength);
    return (cqe->getStatusCode() == NVME_SC_SUCCESS) ? TRUE : FALSE;
}

inline BOOL WINAPI NvmeSubmitIoCommand(
    const NvmeSqe* sqe,
    NvmeCqe* cqe,
    void* buffer,
    uint32_t bufferLength
) {
    if (!sqe || !cqe) return FALSE;
    auto nvme = TitanFlashSubsystem::Instance().getNvme();
    if (!nvme) return FALSE;

    *cqe = nvme->executeIoCommand(*sqe, buffer, bufferLength);
    return (cqe->getStatusCode() == NVME_SC_SUCCESS) ? TRUE : FALSE;
}

inline BOOL WINAPI NvmeGetSmartLog(NvmeSmartLog* outLog) {
    if (!outLog) return FALSE;
    auto nvme = TitanFlashSubsystem::Instance().getNvme();
    if (!nvme) return FALSE;

    NvmeSqe sqe{};
    sqe.opcode = NVME_ADMIN_GET_LOG_PAGE;
    sqe.cdw10 = NVME_LOG_PAGE_SMART_HEALTH;
    NvmeCqe cqe{};
    return NvmeSubmitAdminCommand(&sqe, &cqe, outLog, sizeof(NvmeSmartLog));
}

inline BOOL WINAPI NvmeFormatNamespace(uint32_t nsid, uint32_t blockSize) {
    auto nvme = TitanFlashSubsystem::Instance().getNvme();
    if (!nvme) return FALSE;

    auto ns = nvme->getNamespace(nsid);
    if (!ns) return FALSE;

    ns->format(blockSize);
    return TRUE;
}

inline ULONG WINAPI StorPortInitialize(
    void* Argument1,
    void* Argument2,
    PORT_CONFIGURATION_INFORMATION* ConfigInfo,
    void* HwContext
) {
    (void)Argument1;
    (void)Argument2;
    (void)HwContext;
    if (!ConfigInfo) return 1;
    auto miniport = TitanFlashSubsystem::Instance().getMiniport();
    if (!miniport) return 1;
    return NT_SUCCESS(miniport->findAdapter(ConfigInfo)) ? 0 : 1;
}

inline void WINAPI StorPortNotification(
    STORPORT_NOTIFICATION_TYPE NotificationType,
    void* HwDeviceExtension,
    ...
) {
    (void)NotificationType;
    (void)HwDeviceExtension;
}

inline ULONG WINAPI StorPortGetLogicalUnitAddress(
    void* HwDeviceExtension,
    PSCSI_REQUEST_BLOCK Srb,
    STOR_ADDR_BTL8* Address
) {
    (void)HwDeviceExtension;
    if (!Srb || !Address) return 1;
    Address->Path = Srb->PathId;
    Address->Target = Srb->TargetId;
    Address->Lun = Srb->Lun;
    return 0;
}

inline uint64_t WINAPI StorPortGetPhysicalAddress(
    void* HwDeviceExtension,
    PSCSI_REQUEST_BLOCK Srb,
    void* VirtualAddress,
    ULONG* Length
) {
    (void)HwDeviceExtension;
    (void)Srb;
    if (Length) *Length = 4096;
    return reinterpret_cast<uint64_t>(VirtualAddress);
}

inline BOOL WINAPI StorNvmeSubmitSrb(SCSI_REQUEST_BLOCK* srb) {
    auto miniport = TitanFlashSubsystem::Instance().getMiniport();
    if (!miniport || !srb) return FALSE;
    return NT_SUCCESS(miniport->startIo(srb)) ? TRUE : FALSE;
}

} // extern "C"

inline STORPORT_TELEMETRY StorPortQueryTelemetry() {
    auto miniport = TitanFlashSubsystem::Instance().getMiniport();
    if (!miniport) return STORPORT_TELEMETRY{};
    return miniport->getTelemetry();
}

inline void StorPortResetTelemetry() {
    auto miniport = TitanFlashSubsystem::Instance().getMiniport();
    if (miniport) miniport->resetTelemetry();
}

// ============================================================================
// 10. Subsystem Initialization & SCM Driver Registration
// ============================================================================

inline void InitializeNvmeSubsystem() {
    static bool s_registered = false;
    if (s_registered) return;
    s_registered = true;

    // 1. Initialize Subsystem Singleton & Hardware Media
    pci::InitializePciSubsystem();
    TitanFlashSubsystem::Instance().initialize();

    // 2. Dynamic Loader Exports for stornvme.sys
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("stornvme.sys", "NvmeControllerReset", reinterpret_cast<void*>(NvmeControllerReset));
    ldr.registerExport("stornvme.sys", "NvmeSubmitAdminCommand", reinterpret_cast<void*>(NvmeSubmitAdminCommand));
    ldr.registerExport("stornvme.sys", "NvmeSubmitIoCommand", reinterpret_cast<void*>(NvmeSubmitIoCommand));
    ldr.registerExport("stornvme.sys", "NvmeGetSmartLog", reinterpret_cast<void*>(NvmeGetSmartLog));
    ldr.registerExport("stornvme.sys", "NvmeFormatNamespace", reinterpret_cast<void*>(NvmeFormatNamespace));
    ldr.registerExport("stornvme.sys", "StorPortInitialize", reinterpret_cast<void*>(StorPortInitialize));
    ldr.registerExport("stornvme.sys", "StorPortGetLogicalUnitAddress", reinterpret_cast<void*>(StorPortGetLogicalUnitAddress));
    ldr.registerExport("stornvme.sys", "StorPortGetPhysicalAddress", reinterpret_cast<void*>(StorPortGetPhysicalAddress));
    ldr.registerExport("stornvme.sys", "StorNvmeSubmitSrb", reinterpret_cast<void*>(StorNvmeSubmitSrb));
    ldr.registerExport("stornvme.sys", "StorPortQueryTelemetry", reinterpret_cast<void*>(StorPortQueryTelemetry));
    ldr.registerExport("stornvme.sys", "StorPortResetTelemetry", reinterpret_cast<void*>(StorPortResetTelemetry));

    // 3. Register Storage System Drivers in SCM
    auto nvmeSvc = std::make_shared<scm::ServiceRecord>();
    nvmeSvc->serviceName = L"stornvme";
    nvmeSvc->displayName = L"MicaNT NVM Express Miniport Driver";
    nvmeSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    nvmeSvc->startType = scm::SERVICE_BOOT_START;
    nvmeSvc->errorControl = scm::SERVICE_ERROR_CRITICAL;
    nvmeSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\stornvme.sys";
    nvmeSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(nvmeSvc);

    auto ahciSvc = std::make_shared<scm::ServiceRecord>();
    ahciSvc->serviceName = L"storahci";
    ahciSvc->displayName = L"MicaNT AHCI 1.3 SATA Miniport Driver";
    ahciSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    ahciSvc->startType = scm::SERVICE_BOOT_START;
    ahciSvc->errorControl = scm::SERVICE_ERROR_NORMAL;
    ahciSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\storahci.sys";
    ahciSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(ahciSvc);

    auto ufsSvc = std::make_shared<scm::ServiceRecord>();
    ufsSvc->serviceName = L"storufs";
    ufsSvc->displayName = L"MicaNT Universal Flash Storage Miniport Driver";
    ufsSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    ufsSvc->startType = scm::SERVICE_BOOT_START;
    ufsSvc->errorControl = scm::SERVICE_ERROR_NORMAL;
    ufsSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\storufs.sys";
    ufsSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(ufsSvc);

    auto nvmeBlockSvc = std::make_shared<scm::ServiceRecord>();
    nvmeBlockSvc->serviceName = L"nvme";
    nvmeBlockSvc->displayName = L"MicaNT NVM Express Storage Subsystem Driver";
    nvmeBlockSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    nvmeBlockSvc->startType = scm::SERVICE_BOOT_START;
    nvmeBlockSvc->errorControl = scm::SERVICE_ERROR_CRITICAL;
    nvmeBlockSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\nvme.sys";
    nvmeBlockSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(nvmeBlockSvc);

    // 4. Register Version Database Information
    version::VersionDatabase::Instance().RegisterModule(
        "nvme.sys",
        "10.0.22621.1",
        "MicaNT Sovereign NVM Express Class Driver"
    );
    version::VersionDatabase::Instance().RegisterModule(
        "stornvme.sys",
        "10.0.22621.1",
        "MicaNT Sovereign NVM Express Miniport Driver"
    );
    version::VersionDatabase::Instance().RegisterModule(
        "storahci.sys",
        "10.0.22621.1",
        "MicaNT Sovereign AHCI SATA Miniport Driver"
    );
    version::VersionDatabase::Instance().RegisterModule(
        "storufs.sys",
        "10.0.22621.1",
        "MicaNT Sovereign Universal Flash Storage Miniport Driver"
    );
}

} // namespace micant::nvme

namespace micant {
    using nvme::NvmeController;
    using nvme::NvmeNamespace;
    using nvme::PrpDmaEngine;
    using nvme::StorNvmeMiniport;
    using nvme::SCSI_REQUEST_BLOCK;
    using nvme::PSCSI_REQUEST_BLOCK;
    using nvme::PORT_CONFIGURATION_INFORMATION;
    using nvme::STOR_ADDR_BTL8;
    using nvme::STORPORT_NOTIFICATION_TYPE;
    using nvme::SRB_FUNCTION;
    using nvme::SRB_STATUS;
    using nvme::SCSI_STATUS;
    using nvme::SCSI_OPCODE;
    using nvme::NVME_TELEMETRY;
    using nvme::STORPORT_TELEMETRY;
    using nvme::StorPortQueryTelemetry;
    using nvme::StorPortResetTelemetry;
    using nvme::NvmeBlockDriver;
    using nvme::NvmeSglDescriptor;
    using nvme::NvmeProtectionInformation;
    using nvme::NvmeZoneDescriptor;
    using nvme::STORAGE_DEVICE_DESCRIPTOR;
    using nvme::STORAGE_BUS_TYPE;
}
