// ============================================================================
// MicaNT: Sovereign ACPI 6.5 Platform Subsystem & AML Interpreter
// File: include/micant/km/hal/acpi.hpp
// Sovereign System Domain: TitanACPI / AegisACPI
//
// Description:
//   Clean-room implementation of the Advanced Configuration and Power Interface
//   (ACPI 6.5) Specification, System Description Table (SDT) architecture,
//   Multiple APIC Description Table (MADT), PCI Express Memory Mapped Configuration
//   (MCFG), Fixed ACPI Description Table (FADT), DMA Remapping Reporting (DMAR),
//   System Resource Affinity Table (SRAT), High Precision Event Timer (HPET),
//   Boot Graphics Resource Table (BGRT), ACPI Machine Language (AML) Bytecode
//   Interpreter, Small & Large Resource Item Decoders/Encoders, PCI Interrupt
//   Routing Table (_PRT), General Purpose Events (GPE/SCI), Power Management FSM,
//   Secondary System Description Table (SSDT) dynamic loading/unloading,
//   and Windows ACPI/HAL Driver C ABI Exports (acpi.sys and hal.dll).
//
// Clean-Room Engineering Reference & Standards:
//   - UEFI Forum: Advanced Configuration and Power Interface (ACPI) Specification 6.5 (2022)
//   - Intel Corporation: ACPI Component Architecture (ACPICA) Reference Manual
//   - Microsoft Open win32metadata repository: Windows.Win32.System.Power
//   - PCI-SIG: PCI Firmware Specification Revision 3.3
//   - ISO/IEC 14882:2023 C++ Standard
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
#include <span>
#include <functional>

#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "janusldr.hpp"
#include "scm.hpp"
#include "version.hpp"
#include "boot.hpp"
#include "pci.hpp"

#ifndef _BUS_DATA_TYPE_DEFINED
#define _BUS_DATA_TYPE_DEFINED
typedef enum _BUS_DATA_TYPE {
    ConfigurationSpaceUndefined = -1,
    CbusConfiguration,
    MasterAdapterConfiguration,
    SlotConfiguration,
    PCIConfiguration,
    PCMCIABusConfiguration,
    MaximumBusDataType
} BUS_DATA_TYPE, *PBUS_DATA_TYPE;
#endif

#ifndef _PVOID_DEFINED
#define _PVOID_DEFINED
using PVOID = void*;
#endif

namespace micant::acpi {

using BOOL = int32_t;
inline constexpr BOOL TRUE = 1;
inline constexpr BOOL FALSE = 0;

// ============================================================================
// 1. ACPI 6.5 Table Signatures & Magic Constants
// ============================================================================

inline constexpr char ACPI_SIG_RSDP[8] = {'R', 'S', 'D', ' ', 'P', 'T', 'R', ' '};
inline constexpr uint32_t ACPI_SIG_RSDT = 0x54445352; // "RSDT"
inline constexpr uint32_t ACPI_SIG_XSDT = 0x54445358; // "XSDT"
inline constexpr uint32_t ACPI_SIG_FADT = 0x50434146; // "FACP"
inline constexpr uint32_t ACPI_SIG_DSDT = 0x54445344; // "DSDT"
inline constexpr uint32_t ACPI_SIG_SSDT = 0x54445353; // "SSDT"
inline constexpr uint32_t ACPI_SIG_MADT = 0x43495041; // "APIC"
inline constexpr uint32_t ACPI_SIG_MCFG = 0x4746434D; // "MCFG"
inline constexpr uint32_t ACPI_SIG_HPET = 0x54455048; // "HPET"
inline constexpr uint32_t ACPI_SIG_SRAT = 0x54415253; // "SRAT"
inline constexpr uint32_t ACPI_SIG_SLIT = 0x54494C53; // "SLIT"
inline constexpr uint32_t ACPI_SIG_DMAR = 0x52414D44; // "DMAR"
inline constexpr uint32_t ACPI_SIG_WAET = 0x54454157; // "WAET"
inline constexpr uint32_t ACPI_SIG_BGRT = 0x54524742; // "BGRT"

// Power Management Profiles (FADT preferredPmProfile)
inline constexpr uint8_t PM_PROFILE_UNSPECIFIED         = 0;
inline constexpr uint8_t PM_PROFILE_DESKTOP             = 1;
inline constexpr uint8_t PM_PROFILE_MOBILE              = 2;
inline constexpr uint8_t PM_PROFILE_WORKSTATION         = 3;
inline constexpr uint8_t PM_PROFILE_ENTERPRISE_SERVER   = 4;
inline constexpr uint8_t PM_PROFILE_SOHO_SERVER         = 5;
inline constexpr uint8_t PM_PROFILE_APPLIANCE_PC        = 6;
inline constexpr uint8_t PM_PROFILE_PERFORMANCE_SERVER  = 7;

// System Sleep States
enum class AcpiSleepState : uint8_t {
    S0_Working    = 0,
    S1_CpuStop    = 1,
    S2_LowPower   = 2,
    S3_SuspendRam = 3,
    S4_Hibernate  = 4,
    S5_SoftOff    = 5
};

// Device Power States (ACPI 6.5 Section 2.3)
enum class AcpiDevicePowerState : uint8_t {
    D0 = 0,    // Fully On
    D1 = 1,    // Intermediate Power
    D2 = 2,    // Low Power / Clock Gated
    D3hot = 3, // Soft Off / Context Preserved
    D3cold = 4 // Fully Powered Down
};

// MADT Structure Types
inline constexpr uint8_t MADT_TYPE_LOCAL_APIC           = 0;
inline constexpr uint8_t MADT_TYPE_IO_APIC              = 1;
inline constexpr uint8_t MADT_TYPE_INTERRUPT_OVERRIDE   = 2;
inline constexpr uint8_t MADT_TYPE_NMI_SOURCE           = 3;
inline constexpr uint8_t MADT_TYPE_LOCAL_APIC_NMI       = 4;
inline constexpr uint8_t MADT_TYPE_LOCAL_APIC_OVERRIDE  = 5;
inline constexpr uint8_t MADT_TYPE_LOCAL_X2APIC         = 9;
inline constexpr uint8_t MADT_TYPE_GICC                 = 11;
inline constexpr uint8_t MADT_TYPE_GICD                 = 12;

// AML Opcode Tokens (ACPI 6.5 Chapter 20 AML Specification)
inline constexpr uint8_t AML_ZERO_OP            = 0x00;
inline constexpr uint8_t AML_ONE_OP             = 0x01;
inline constexpr uint8_t AML_ONES_OP            = 0xFF;
inline constexpr uint8_t AML_NAME_OP            = 0x08;
inline constexpr uint8_t AML_BYTE_PREFIX        = 0x0A;
inline constexpr uint8_t AML_WORD_PREFIX        = 0x0B;
inline constexpr uint8_t AML_DWORD_PREFIX       = 0x0C;
inline constexpr uint8_t AML_STRING_PREFIX      = 0x0D;
inline constexpr uint8_t AML_QWORD_PREFIX       = 0x0E;
inline constexpr uint8_t AML_SCOPE_OP           = 0x10;
inline constexpr uint8_t AML_BUFFER_OP          = 0x11;
inline constexpr uint8_t AML_PACKAGE_OP         = 0x12;
inline constexpr uint8_t AML_METHOD_OP          = 0x14;
inline constexpr uint8_t AML_LOCAL0_OP          = 0x60;
inline constexpr uint8_t AML_LOCAL1_OP          = 0x61;
inline constexpr uint8_t AML_LOCAL2_OP          = 0x62;
inline constexpr uint8_t AML_LOCAL3_OP          = 0x63;
inline constexpr uint8_t AML_LOCAL4_OP          = 0x64;
inline constexpr uint8_t AML_LOCAL5_OP          = 0x65;
inline constexpr uint8_t AML_LOCAL6_OP          = 0x66;
inline constexpr uint8_t AML_LOCAL7_OP          = 0x67;
inline constexpr uint8_t AML_ARG0_OP            = 0x68;
inline constexpr uint8_t AML_ARG1_OP            = 0x69;
inline constexpr uint8_t AML_ARG2_OP            = 0x6A;
inline constexpr uint8_t AML_ARG3_OP            = 0x6B;
inline constexpr uint8_t AML_ARG4_OP            = 0x6C;
inline constexpr uint8_t AML_ARG5_OP            = 0x6D;
inline constexpr uint8_t AML_ARG6_OP            = 0x6E;
inline constexpr uint8_t AML_STORE_OP           = 0x70;
inline constexpr uint8_t AML_ADD_OP             = 0x72;
inline constexpr uint8_t AML_SUBTRACT_OP        = 0x74;
inline constexpr uint8_t AML_MULTIPLY_OP        = 0x77;
inline constexpr uint8_t AML_DIVIDE_OP          = 0x78;
inline constexpr uint8_t AML_SHIFT_LEFT_OP      = 0x79;
inline constexpr uint8_t AML_SHIFT_RIGHT_OP     = 0x7A;
inline constexpr uint8_t AML_AND_OP             = 0x7B;
inline constexpr uint8_t AML_OR_OP              = 0x7D;
inline constexpr uint8_t AML_XOR_OP             = 0x7F;
inline constexpr uint8_t AML_NOT_OP             = 0x80;
inline constexpr uint8_t AML_DEVICE_OP          = 0x82;
inline constexpr uint8_t AML_LAND_OP            = 0x90;
inline constexpr uint8_t AML_LOR_OP             = 0x91;
inline constexpr uint8_t AML_LNOT_OP            = 0x92;
inline constexpr uint8_t AML_LEQUAL_OP          = 0x93;
inline constexpr uint8_t AML_LGREATER_OP        = 0x94;
inline constexpr uint8_t AML_LLESS_OP           = 0x95;
inline constexpr uint8_t AML_IF_OP              = 0xA0;
inline constexpr uint8_t AML_ELSE_OP            = 0xA1;
inline constexpr uint8_t AML_WHILE_OP           = 0xA2;
inline constexpr uint8_t AML_RETURN_OP          = 0xA4;

#pragma pack(push, 1)

// ============================================================================
// 2. ACPI Physical Table Headers & Descriptors
// ============================================================================

// ACPI 2.0+ Root System Description Pointer (RSDP)
struct AcpiRsdp {
    char     signature[8];     // "RSD PTR "
    uint8_t  checksum;         // ACPI 1.0 checksum
    char     oemId[6];         // OEM ID string
    uint8_t  revision;         // 0 = ACPI 1.0, 2 = ACPI 2.0+
    uint32_t rsdtAddress;      // Physical 32-bit address of RSDT
    // ACPI 2.0+ Extended Fields
    uint32_t length;           // Length of RSDP structure (36 bytes)
    uint64_t xsdtAddress;      // Physical 64-bit address of XSDT
    uint8_t  extendedChecksum; // Entire table checksum
    uint8_t  reserved[3];
};

// Universal System Description Table Header (SDT)
struct AcpiTableHeader {
    char     signature[4];     // Table Identifier (e.g. "FACP", "APIC", "MCFG")
    uint32_t length;           // Total table length including header
    uint8_t  revision;         // Specification revision
    uint8_t  checksum;         // Entire table byte-sum must equal 0 (mod 256)
    char     oemId[6];         // OEM string
    char     oemTableId[8];    // OEM Table ID
    uint32_t oemRevision;      // OEM Revision number
    char     aslCompilerId[4]; // Creator / ASL compiler ID
    uint32_t aslCompilerRev;   // Creator Revision number
};

// Generic Address Structure (GAS)
struct AcpiGenericAddress {
    uint8_t  addressSpaceId;    // 0 = System Memory, 1 = System I/O, 2 = PCI Config Space
    uint8_t  registerBitWidth;
    uint8_t  registerBitOffset;
    uint8_t  accessSize;        // 0 = Undefined, 1 = Byte, 2 = Word, 3 = DWord, 4 = QWord
    uint64_t address;           // 64-bit physical address or I/O port
};

// Fixed ACPI Description Table (FADT / FACP)
struct AcpiFadt {
    AcpiTableHeader header;
    uint32_t firmwareCtrl;     // Physical address of FACS
    uint32_t dsdtAddress;      // 32-bit physical address of DSDT
    uint8_t  reserved1;
    uint8_t  preferredPmProfile;
    uint16_t sciInt;           // System Control Interrupt vector
    uint32_t smiCmd;           // Port for SMI command
    uint8_t  acpiEnable;       // Value to write to smiCmd to enable ACPI
    uint8_t  acpiDisable;      // Value to write to smiCmd to disable ACPI
    uint8_t  s4BiosReq;
    uint8_t  pstateCnt;
    uint32_t pm1aEvtBlk;
    uint32_t pm1bEvtBlk;
    uint32_t pm1aCntBlk;
    uint32_t pm1bCntBlk;
    uint32_t pm2CntBlk;
    uint32_t pmTmrBlk;         // Power Management Timer Port (24/32-bit 3.579545 MHz)
    uint32_t gpe0Blk;
    uint32_t gpe1Blk;
    uint8_t  pm1EvtLen;
    uint8_t  pm1CntLen;
    uint8_t  pm2CntLen;
    uint8_t  pmTmrLen;
    uint8_t  gpe0Len;
    uint8_t  gpe1Len;
    uint8_t  gpe1Base;
    uint8_t  cstCnt;
    uint16_t pLevel2Lat;
    uint16_t pLevel3Lat;
    uint16_t flushSize;
    uint16_t flushStride;
    uint8_t  dutyOffset;
    uint8_t  dutyWidth;
    uint8_t  dayAlarm;
    uint8_t  monthAlarm;
    uint8_t  century;
    uint16_t bootArchFlags;
    uint8_t  reserved2;
    uint32_t flags;
    AcpiGenericAddress resetReg;
    uint8_t  resetValue;
    uint16_t armBootFlags;
    uint8_t  minorVersion;
    uint64_t xFirmwareCtrl;
    uint64_t xDsdtAddress;     // 64-bit physical address of DSDT
    AcpiGenericAddress xPm1aEvtBlk;
    AcpiGenericAddress xPm1bEvtBlk;
    AcpiGenericAddress xPm1aCntBlk;
    AcpiGenericAddress xPm1bCntBlk;
    AcpiGenericAddress xPm2CntBlk;
    AcpiGenericAddress xPmTmrBlk;
    AcpiGenericAddress xGpe0Blk;
    AcpiGenericAddress xGpe1Blk;
    AcpiGenericAddress sleepControlReg;
    AcpiGenericAddress sleepStatusReg;
    uint64_t hypervisorVendorId;
};

// Multiple APIC Description Table (MADT) Header
struct AcpiMadtHeader {
    AcpiTableHeader header;
    uint32_t localApicAddress; // Physical address of Local APIC MMIO registers (default 0xFEE00000)
    uint32_t flags;            // 1 = Dual 8259 Legacy PICs installed
};

// MADT Record Header
struct AcpiMadtRecordHeader {
    uint8_t type;
    uint8_t length;
};

// MADT Type 0: Processor Local APIC
struct AcpiMadtLocalApic {
    AcpiMadtRecordHeader header; // type = 0, length = 8
    uint8_t  processorId;        // ACPI Processor ID
    uint8_t  apicId;             // Local APIC ID
    uint32_t flags;              // Bit 0 = Enabled, Bit 1 = Online capable
};

// MADT Type 1: I/O APIC
struct AcpiMadtIoApic {
    AcpiMadtRecordHeader header; // type = 1, length = 12
    uint8_t  ioApicId;           // I/O APIC ID
    uint8_t  reserved;
    uint32_t ioApicAddress;      // Physical MMIO address (e.g. 0xFEC00000)
    uint32_t gsiBase;            // Global System Interrupt Base
};

// MADT Type 2: Interrupt Source Override
struct AcpiMadtIntrOverride {
    AcpiMadtRecordHeader header; // type = 2, length = 10
    uint8_t  bus;                // 0 = ISA
    uint8_t  source;             // Bus-relative IRQ
    uint32_t gsi;                // Global System Interrupt mapping
    uint16_t flags;              // Polarity and Trigger Mode
};

// PCI Express Memory Mapped Configuration Table (MCFG) Allocation Record
struct AcpiMcfgAllocation {
    uint64_t baseAddress;        // Base MMIO address of PCIe configuration space
    uint16_t pciSegmentGroup;    // PCI Segment (Domain 0)
    uint8_t  startBusNumber;     // Start Bus (0)
    uint8_t  endBusNumber;       // End Bus (255)
    uint32_t reserved;
};

struct AcpiMcfgHeader {
    AcpiTableHeader header;
    uint64_t reserved;
    AcpiMcfgAllocation allocations[1]; // Flexible array
};

// System Resource Affinity Table (SRAT) Header
struct AcpiSratHeader {
    AcpiTableHeader header;
    uint32_t tableRevision;      // 1
    uint64_t reserved;
};

// DMA Remapping Reporting Table (DMAR) Header
struct AcpiDmarHeader {
    AcpiTableHeader header;
    uint8_t  hostAddressWidth;   // Max physical address width (e.g. 39 or 48)
    uint8_t  flags;              // INTR_REMAP, X2APIC_OPT_OUT
    uint8_t  reserved[10];
};

// High Precision Event Timer (HPET) Table
struct AcpiHpetTable {
    AcpiTableHeader header;
    uint32_t eventTimerBlockId; // Hardware ID, revision, comparators count
    AcpiGenericAddress baseAddress; // System Memory GAS (address: 0xFED00000)
    uint8_t  hpetNumber;
    uint16_t mainCounterMinClockTick;
    uint8_t  pageProtectionAndOemAttribute;
};

// Boot Graphics Resource Table (BGRT)
struct AcpiBgrtTable {
    AcpiTableHeader header;
    uint16_t version;     // 1
    uint8_t  status;      // Bit 0 = Displayed (1)
    uint8_t  imageType;   // 0 = Bitmap
    uint64_t imageAddress;// Physical memory address of boot logo bitmap
    uint32_t imageOffsetX;
    uint32_t imageOffsetY;
};

#pragma pack(pop)

// ============================================================================
// 3. ACPI Table Checksum & Parsing Helper
// ============================================================================

inline uint8_t CalculateAcpiChecksum(const void* data, size_t length) {
    if (!data || length == 0) return 0;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
    uint8_t sum = 0;
    for (size_t i = 0; i < length; ++i) {
        sum += bytes[i];
    }
    return sum;
}

inline bool VerifyAcpiChecksum(const void* data, size_t length) {
    return CalculateAcpiChecksum(data, length) == 0;
}

// ============================================================================
// 4. ACPI Small & Large Resource Item Decoders / Encoders
// ============================================================================

enum class AcpiResourceType : uint32_t {
    IoPort,
    FixedIoPort,
    Memory32,
    FixedMemory32,
    Irq,
    ExtendedIrq,
    Dma,
    EndTag
};

struct ACPI_RESOURCE {
    AcpiResourceType type{AcpiResourceType::EndTag};
    union {
        struct {
            uint16_t baseAddress;
            uint16_t length;
            uint8_t  alignment;
        } io;
        struct {
            uint16_t baseAddress;
            uint8_t  length;
        } fixedIo;
        struct {
            uint32_t baseAddress;
            uint32_t length;
            uint8_t  writeProtect;
        } fixedMemory32;
        struct {
            uint32_t minimumAddress;
            uint32_t maximumAddress;
            uint32_t alignment;
            uint32_t length;
            uint8_t  writeProtect;
        } memory32;
        struct {
            uint16_t irqMask;
            uint8_t  triggering; // 0 = Edge, 1 = Level
            uint8_t  polarity;   // 0 = Active High, 1 = Active Low
            uint8_t  sharable;   // 0 = Exclusive, 1 = Shared
        } irq;
        struct {
            uint32_t gsi;
            uint8_t  triggering;
            uint8_t  polarity;
            uint8_t  sharable;
        } extendedIrq;
        struct {
            uint8_t  channelMask;
            uint8_t  transferType;
        } dma;
    };
};

inline std::vector<uint8_t> EncodeAcpiResources(const std::vector<ACPI_RESOURCE>& resources) {
    std::vector<uint8_t> buf;
    for (const auto& r : resources) {
        switch (r.type) {
        case AcpiResourceType::FixedIoPort: {
            buf.push_back(0x4B); // Small item 9, length 3
            buf.push_back(static_cast<uint8_t>(r.fixedIo.baseAddress & 0xFF));
            buf.push_back(static_cast<uint8_t>((r.fixedIo.baseAddress >> 8) & 0xFF));
            buf.push_back(r.fixedIo.length);
            break;
        }
        case AcpiResourceType::IoPort: {
            buf.push_back(0x47); // Small item 8, length 7
            buf.push_back(1);    // 16-bit decode
            buf.push_back(static_cast<uint8_t>(r.io.baseAddress & 0xFF));
            buf.push_back(static_cast<uint8_t>((r.io.baseAddress >> 8) & 0xFF));
            uint16_t maxAddr = static_cast<uint16_t>(r.io.baseAddress + r.io.length - 1);
            buf.push_back(static_cast<uint8_t>(maxAddr & 0xFF));
            buf.push_back(static_cast<uint8_t>((maxAddr >> 8) & 0xFF));
            buf.push_back(r.io.alignment);
            buf.push_back(static_cast<uint8_t>(r.io.length & 0xFF));
            break;
        }
        case AcpiResourceType::Irq: {
            buf.push_back(0x23); // Small item 4, length 3
            buf.push_back(static_cast<uint8_t>(r.irq.irqMask & 0xFF));
            buf.push_back(static_cast<uint8_t>((r.irq.irqMask >> 8) & 0xFF));
            uint8_t flags = static_cast<uint8_t>((r.irq.triggering & 0x01) | ((r.irq.polarity & 0x01) << 3) | ((r.irq.sharable & 0x01) << 4));
            buf.push_back(flags);
            break;
        }
        case AcpiResourceType::Dma: {
            buf.push_back(0x2A); // Small item 5, length 2
            buf.push_back(r.dma.channelMask);
            buf.push_back(r.dma.transferType);
            break;
        }
        case AcpiResourceType::FixedMemory32: {
            buf.push_back(0x86); // Large item 6
            buf.push_back(9);    // Length low
            buf.push_back(0);    // Length high
            buf.push_back(r.fixedMemory32.writeProtect);
            uint32_t base = r.fixedMemory32.baseAddress;
            buf.push_back(static_cast<uint8_t>(base & 0xFF));
            buf.push_back(static_cast<uint8_t>((base >> 8) & 0xFF));
            buf.push_back(static_cast<uint8_t>((base >> 16) & 0xFF));
            buf.push_back(static_cast<uint8_t>((base >> 24) & 0xFF));
            uint32_t len = r.fixedMemory32.length;
            buf.push_back(static_cast<uint8_t>(len & 0xFF));
            buf.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
            buf.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
            buf.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
            break;
        }
        case AcpiResourceType::Memory32: {
            buf.push_back(0x85); // Large item 5
            buf.push_back(17);
            buf.push_back(0);
            buf.push_back(r.memory32.writeProtect);
            auto pushU32 = [&](uint32_t v) {
                buf.push_back(static_cast<uint8_t>(v & 0xFF));
                buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
                buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
                buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
            };
            pushU32(r.memory32.minimumAddress);
            pushU32(r.memory32.maximumAddress);
            pushU32(r.memory32.alignment);
            pushU32(r.memory32.length);
            break;
        }
        case AcpiResourceType::ExtendedIrq: {
            buf.push_back(0x89); // Large item 9
            buf.push_back(6);
            buf.push_back(0);
            uint8_t flags = static_cast<uint8_t>(1 | ((r.extendedIrq.triggering & 0x01) << 1) | ((r.extendedIrq.polarity & 0x01) << 2) | ((r.extendedIrq.sharable & 0x01) << 3));
            buf.push_back(flags);
            buf.push_back(1); // 1 interrupt in table
            uint32_t gsi = r.extendedIrq.gsi;
            buf.push_back(static_cast<uint8_t>(gsi & 0xFF));
            buf.push_back(static_cast<uint8_t>((gsi >> 8) & 0xFF));
            buf.push_back(static_cast<uint8_t>((gsi >> 16) & 0xFF));
            buf.push_back(static_cast<uint8_t>((gsi >> 24) & 0xFF));
            break;
        }
        case AcpiResourceType::EndTag:
            break;
        }
    }
    // Append End Tag (Small Item 15, length 1)
    buf.push_back(0x79);
    buf.push_back(0x00);
    return buf;
}

inline std::vector<ACPI_RESOURCE> DecodeAcpiResources(std::span<const uint8_t> buffer) {
    std::vector<ACPI_RESOURCE> res;
    size_t offset = 0;
    while (offset < buffer.size()) {
        uint8_t b0 = buffer[offset];
        if (b0 & 0x80) {
            // Large Item
            uint8_t itemType = b0 & 0x7F;
            if (offset + 3 > buffer.size()) break;
            uint16_t length = static_cast<uint16_t>(buffer[offset + 1] | (buffer[offset + 2] << 8));
            size_t itemData = offset + 3;
            if (itemData + length > buffer.size()) break;

            if (itemType == 0x06 && length >= 9) {
                ACPI_RESOURCE r{};
                r.type = AcpiResourceType::FixedMemory32;
                r.fixedMemory32.writeProtect = buffer[itemData];
                r.fixedMemory32.baseAddress = static_cast<uint32_t>(buffer[itemData + 1] | (buffer[itemData + 2] << 8) | (buffer[itemData + 3] << 16) | (buffer[itemData + 4] << 24));
                r.fixedMemory32.length = static_cast<uint32_t>(buffer[itemData + 5] | (buffer[itemData + 6] << 8) | (buffer[itemData + 7] << 16) | (buffer[itemData + 8] << 24));
                res.push_back(r);
            } else if (itemType == 0x05 && length >= 17) {
                ACPI_RESOURCE r{};
                r.type = AcpiResourceType::Memory32;
                r.memory32.writeProtect = buffer[itemData];
                auto readU32 = [&](size_t idx) -> uint32_t {
                    return static_cast<uint32_t>(buffer[idx] | (buffer[idx + 1] << 8) | (buffer[idx + 2] << 16) | (buffer[idx + 3] << 24));
                };
                r.memory32.minimumAddress = readU32(itemData + 1);
                r.memory32.maximumAddress = readU32(itemData + 5);
                r.memory32.alignment = readU32(itemData + 9);
                r.memory32.length = readU32(itemData + 13);
                res.push_back(r);
            } else if (itemType == 0x09 && length >= 6) {
                ACPI_RESOURCE r{};
                r.type = AcpiResourceType::ExtendedIrq;
                uint8_t flags = buffer[itemData];
                r.extendedIrq.triggering = (flags >> 1) & 0x01;
                r.extendedIrq.polarity = (flags >> 2) & 0x01;
                r.extendedIrq.sharable = (flags >> 3) & 0x01;
                r.extendedIrq.gsi = static_cast<uint32_t>(buffer[itemData + 2] | (buffer[itemData + 3] << 8) | (buffer[itemData + 4] << 16) | (buffer[itemData + 5] << 24));
                res.push_back(r);
            }
            offset += 3 + length;
        } else {
            // Small Item
            uint8_t itemType = (b0 >> 3) & 0x0F;
            uint8_t length = b0 & 0x07;
            size_t itemData = offset + 1;
            if (itemData + length > buffer.size()) break;

            if (itemType == 0x0F) {
                // End Tag marks the end of the resource buffer
                break;
            } else if (itemType == 0x09 && length >= 3) {
                ACPI_RESOURCE r{};
                r.type = AcpiResourceType::FixedIoPort;
                r.fixedIo.baseAddress = static_cast<uint16_t>(buffer[itemData] | (buffer[itemData + 1] << 8));
                r.fixedIo.length = buffer[itemData + 2];
                res.push_back(r);
            } else if (itemType == 0x08 && length >= 7) {
                ACPI_RESOURCE r{};
                r.type = AcpiResourceType::IoPort;
                r.io.baseAddress = static_cast<uint16_t>(buffer[itemData + 1] | (buffer[itemData + 2] << 8));
                r.io.alignment = buffer[itemData + 5];
                r.io.length = buffer[itemData + 6];
                res.push_back(r);
            } else if (itemType == 0x04 && length >= 2) {
                ACPI_RESOURCE r{};
                r.type = AcpiResourceType::Irq;
                r.irq.irqMask = static_cast<uint16_t>(buffer[itemData] | (buffer[itemData + 1] << 8));
                if (length >= 3) {
                    uint8_t flags = buffer[itemData + 2];
                    r.irq.triggering = flags & 0x01;
                    r.irq.polarity = (flags >> 3) & 0x01;
                    r.irq.sharable = (flags >> 4) & 0x01;
                }
                res.push_back(r);
            } else if (itemType == 0x05 && length >= 2) {
                ACPI_RESOURCE r{};
                r.type = AcpiResourceType::Dma;
                r.dma.channelMask = buffer[itemData];
                r.dma.transferType = buffer[itemData + 1];
                res.push_back(r);
            }
            offset += 1 + length;
        }
    }
    return res;
}

// ============================================================================
// 5. PCI Interrupt Routing Table (_PRT)
// ============================================================================

struct AcpiPrtEntry {
    uint32_t address; // Device # in high word, Function (0xFFFF for all) in low word
    uint8_t  pin;     // 0 = INTA, 1 = INTB, 2 = INTC, 3 = INTD
    uint32_t gsi;     // Direct Global System Interrupt number
};

// ============================================================================
// 6. GPE (General Purpose Event) & SCI Controller
// ============================================================================

class GpeController {
    mutable std::mutex m_mutex;
    uint64_t m_status{0};
    uint64_t m_enable{0};
    uint64_t m_levelTriggered{0};
    std::unordered_map<uint32_t, std::function<void(uint32_t)>> m_handlers;
    std::atomic<uint64_t> m_dispatchedCount{0};

public:
    void registerHandler(uint32_t gpeNumber, std::function<void(uint32_t)> handler, bool levelTriggered = false) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (gpeNumber < 64) {
            m_handlers[gpeNumber] = std::move(handler);
            m_enable |= (1ULL << gpeNumber);
            if (levelTriggered) {
                m_levelTriggered |= (1ULL << gpeNumber);
            } else {
                m_levelTriggered &= ~(1ULL << gpeNumber);
            }
        }
    }

    void unregisterHandler(uint32_t gpeNumber) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (gpeNumber < 64) {
            m_handlers.erase(gpeNumber);
            m_enable &= ~(1ULL << gpeNumber);
        }
    }

    void setGpeStatus(uint32_t gpeNumber) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (gpeNumber < 64) {
            m_status |= (1ULL << gpeNumber);
        }
    }

    void clearGpeStatus(uint32_t gpeNumber) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (gpeNumber < 64) {
            m_status &= ~(1ULL << gpeNumber);
        }
    }

    bool isGpePending(uint32_t gpeNumber) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (gpeNumber < 64) {
            return (m_status & m_enable & (1ULL << gpeNumber)) != 0;
        }
        return false;
    }

    bool dispatchGpe(uint32_t gpeNumber) {
        std::function<void(uint32_t)> fn;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (gpeNumber >= 64) return false;
            if (!(m_levelTriggered & (1ULL << gpeNumber))) {
                m_status &= ~(1ULL << gpeNumber);
            }
            auto it = m_handlers.find(gpeNumber);
            if (it != m_handlers.end()) {
                fn = it->second;
            }
        }
        if (fn) {
            fn(gpeNumber);
            m_dispatchedCount.fetch_add(1);
            return true;
        }
        return false;
    }

    uint32_t dispatchPendingGpes() {
        uint32_t dispatched = 0;
        for (uint32_t i = 0; i < 64; ++i) {
            if (isGpePending(i)) {
                if (dispatchGpe(i)) {
                    dispatched++;
                }
            }
        }
        return dispatched;
    }

    uint64_t getDispatchedCount() const { return m_dispatchedCount.load(); }
    void resetTelemetry() { m_dispatchedCount.store(0); }
};

// ============================================================================
// 7. AML (ACPI Machine Language) Object Model & AST Nodes
// ============================================================================

enum class AmlObjectType {
    Integer,
    String,
    Buffer,
    Package,
    Device,
    Scope,
    Method,
    ThermalZone,
    Processor
};

struct AmlValue {
    AmlObjectType type{AmlObjectType::Integer};
    uint64_t integerVal{0};
    std::string stringVal;
    std::vector<uint8_t> bufferVal;
    std::vector<AmlValue> packageElements;

    static AmlValue MakeInteger(uint64_t v) {
        AmlValue val{};
        val.type = AmlObjectType::Integer;
        val.integerVal = v;
        return val;
    }

    static AmlValue MakeString(std::string_view s) {
        AmlValue val{};
        val.type = AmlObjectType::String;
        val.stringVal = s;
        return val;
    }

    static AmlValue MakeBuffer(std::vector<uint8_t> buf) {
        AmlValue val{};
        val.type = AmlObjectType::Buffer;
        val.bufferVal = std::move(buf);
        return val;
    }

    static AmlValue MakePackage(std::vector<AmlValue> elements) {
        AmlValue val{};
        val.type = AmlObjectType::Package;
        val.packageElements = std::move(elements);
        return val;
    }
};

class AmlNode {
public:
    std::string name;
    AmlObjectType type{AmlObjectType::Device};
    AmlValue value;
    std::map<std::string, std::shared_ptr<AmlNode>> children;
    std::function<AmlValue(const std::vector<AmlValue>&)> methodHandler;

    AmlNode(std::string_view n, AmlObjectType t) : name(n), type(t) {}

    void addChild(std::shared_ptr<AmlNode> child) {
        if (child) children[child->name] = child;
    }

    std::shared_ptr<AmlNode> findChild(std::string_view childName) const {
        auto it = children.find(std::string(childName));
        if (it != children.end()) return it->second;
        return nullptr;
    }
};

// Forward declaration of TitanAcpiSubsystem for AmlBytecodeParser telemetry
class TitanAcpiSubsystem;

// ============================================================================
// 8. AML Bytecode Parser & Runtime Execution Engine
// ============================================================================

class AmlBytecodeParser {
    std::span<const uint8_t> m_bytecode;
    size_t m_pc{0};
    std::array<uint64_t, 8> m_locals{};
    std::array<uint64_t, 7> m_args{};
    uint64_t m_returnValue{0};
    bool m_hasReturned{false};
    uint64_t m_opsExecuted{0};

public:
    AmlBytecodeParser() = default;

    void reset() {
        m_pc = 0;
        m_locals.fill(0);
        m_args.fill(0);
        m_returnValue = 0;
        m_hasReturned = false;
        m_opsExecuted = 0;
    }

    uint64_t getLocal(size_t index) const {
        return (index < 8) ? m_locals[index] : 0;
    }

    void setLocal(size_t index, uint64_t val) {
        if (index < 8) m_locals[index] = val;
    }

    uint64_t getArg(size_t index) const {
        return (index < 7) ? m_args[index] : 0;
    }

    void setArg(size_t index, uint64_t val) {
        if (index < 7) m_args[index] = val;
    }

    bool hasReturned() const { return m_hasReturned; }
    uint64_t getReturnValue() const { return m_returnValue; }
    uint64_t getOpsExecuted() const { return m_opsExecuted; }

    uint64_t executeBytecode(std::span<const uint8_t> bytecode, const std::vector<AmlValue>& args = {});

private:
    bool hasMore() const { return m_pc < m_bytecode.size(); }
    uint8_t peek() const { return (m_pc < m_bytecode.size()) ? m_bytecode[m_pc] : 0; }
    uint8_t consume() { return (m_pc < m_bytecode.size()) ? m_bytecode[m_pc++] : 0; }

    void recordOp();

    uint16_t consumeWord() {
        uint16_t b0 = consume();
        uint16_t b1 = consume();
        return static_cast<uint16_t>(b0 | (b1 << 8));
    }

    uint32_t consumeDword() {
        uint32_t b0 = consume();
        uint32_t b1 = consume();
        uint32_t b2 = consume();
        uint32_t b3 = consume();
        return static_cast<uint32_t>(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
    }

    uint64_t consumeQword() {
        uint64_t lo = consumeDword();
        uint64_t hi = consumeDword();
        return lo | (hi << 32);
    }

    size_t consumePkgLength() {
        uint8_t lead = consume();
        uint8_t byteCount = (lead >> 6) & 0x03;
        if (byteCount == 0) {
            return lead & 0x3F;
        }
        size_t length = (lead & 0x0F);
        for (uint8_t i = 0; i < byteCount; ++i) {
            uint8_t b = consume();
            length |= (static_cast<size_t>(b) << (4 + 8 * i));
        }
        return length;
    }

    void storeResult(uint64_t value) {
        if (!hasMore()) return;
        uint8_t target = peek();
        if (target >= AML_LOCAL0_OP && target <= AML_LOCAL7_OP) {
            consume();
            m_locals[target - AML_LOCAL0_OP] = value;
        } else if (target >= AML_ARG0_OP && target <= AML_ARG6_OP) {
            consume();
            m_args[target - AML_ARG0_OP] = value;
        } else if (target == AML_ZERO_OP) {
            consume(); // Discarded target
        }
    }

    uint64_t parseOperand() {
        if (!hasMore()) return 0;
        uint8_t op = peek();

        if (op == AML_ZERO_OP) {
            consume();
            return 0;
        }
        if (op == AML_ONE_OP) {
            consume();
            return 1;
        }
        if (op == AML_ONES_OP) {
            consume();
            return 0xFFFFFFFFFFFFFFFFULL;
        }
        if (op == AML_BYTE_PREFIX) {
            consume();
            return consume();
        }
        if (op == AML_WORD_PREFIX) {
            consume();
            return consumeWord();
        }
        if (op == AML_DWORD_PREFIX) {
            consume();
            return consumeDword();
        }
        if (op == AML_QWORD_PREFIX) {
            consume();
            return consumeQword();
        }
        if (op >= AML_LOCAL0_OP && op <= AML_LOCAL7_OP) {
            consume();
            return m_locals[op - AML_LOCAL0_OP];
        }
        if (op >= AML_ARG0_OP && op <= AML_ARG6_OP) {
            consume();
            return m_args[op - AML_ARG0_OP];
        }

        // If it's an operator expression, execute it
        return executeStatement();
    }

    uint64_t executeStatement() {
        if (!hasMore()) return 0;
        uint8_t op = consume();
        recordOp();

        switch (op) {
        case AML_STORE_OP: {
            uint64_t val = parseOperand();
            storeResult(val);
            return val;
        }
        case AML_ADD_OP: {
            uint64_t a = parseOperand();
            uint64_t b = parseOperand();
            uint64_t res = a + b;
            storeResult(res);
            return res;
        }
        case AML_SUBTRACT_OP: {
            uint64_t a = parseOperand();
            uint64_t b = parseOperand();
            uint64_t res = a - b;
            storeResult(res);
            return res;
        }
        case AML_MULTIPLY_OP: {
            uint64_t a = parseOperand();
            uint64_t b = parseOperand();
            uint64_t res = a * b;
            storeResult(res);
            return res;
        }
        case AML_DIVIDE_OP: {
            uint64_t dividend = parseOperand();
            uint64_t divisor = parseOperand();
            uint64_t rem = (divisor != 0) ? (dividend % divisor) : 0;
            uint64_t quot = (divisor != 0) ? (dividend / divisor) : 0;
            storeResult(rem);  // Remainder target
            storeResult(quot); // Quotient target
            return quot;
        }
        case AML_SHIFT_LEFT_OP: {
            uint64_t a = parseOperand();
            uint64_t b = parseOperand();
            uint64_t res = (b < 64) ? (a << b) : 0;
            storeResult(res);
            return res;
        }
        case AML_SHIFT_RIGHT_OP: {
            uint64_t a = parseOperand();
            uint64_t b = parseOperand();
            uint64_t res = (b < 64) ? (a >> b) : 0;
            storeResult(res);
            return res;
        }
        case AML_AND_OP: {
            uint64_t a = parseOperand();
            uint64_t b = parseOperand();
            uint64_t res = a & b;
            storeResult(res);
            return res;
        }
        case AML_OR_OP: {
            uint64_t a = parseOperand();
            uint64_t b = parseOperand();
            uint64_t res = a | b;
            storeResult(res);
            return res;
        }
        case AML_XOR_OP: {
            uint64_t a = parseOperand();
            uint64_t b = parseOperand();
            uint64_t res = a ^ b;
            storeResult(res);
            return res;
        }
        case AML_NOT_OP: {
            uint64_t a = parseOperand();
            uint64_t res = ~a;
            storeResult(res);
            return res;
        }
        case AML_LAND_OP: {
            uint64_t a = parseOperand();
            uint64_t b = parseOperand();
            return (a && b) ? 1 : 0;
        }
        case AML_LOR_OP: {
            uint64_t a = parseOperand();
            uint64_t b = parseOperand();
            return (a || b) ? 1 : 0;
        }
        case AML_LNOT_OP: {
            uint64_t a = parseOperand();
            return (!a) ? 1 : 0;
        }
        case AML_LEQUAL_OP: {
            uint64_t a = parseOperand();
            uint64_t b = parseOperand();
            return (a == b) ? 1 : 0;
        }
        case AML_LGREATER_OP: {
            uint64_t a = parseOperand();
            uint64_t b = parseOperand();
            return (a > b) ? 1 : 0;
        }
        case AML_LLESS_OP: {
            uint64_t a = parseOperand();
            uint64_t b = parseOperand();
            return (a < b) ? 1 : 0;
        }
        case AML_RETURN_OP: {
            uint64_t res = parseOperand();
            m_returnValue = res;
            m_hasReturned = true;
            return res;
        }
        case AML_IF_OP: {
            size_t pkgStart = m_pc;
            size_t pkgLen = consumePkgLength();
            size_t endOffset = pkgStart + pkgLen;
            uint64_t cond = parseOperand();
            if (cond != 0) {
                while (m_pc < endOffset && !m_hasReturned) {
                    executeStatement();
                }
                if (hasMore() && peek() == AML_ELSE_OP) {
                    consume(); recordOp();
                    size_t elseStart = m_pc;
                    size_t elseLen = consumePkgLength();
                    m_pc = elseStart + elseLen;
                }
            } else {
                m_pc = endOffset;
                if (hasMore() && peek() == AML_ELSE_OP) {
                    consume(); recordOp();
                    size_t elseStart = m_pc;
                    size_t elseLen = consumePkgLength();
                    size_t elseEnd = elseStart + elseLen;
                    while (m_pc < elseEnd && !m_hasReturned) {
                        executeStatement();
                    }
                }
            }
            return 0;
        }
        case AML_WHILE_OP: {
            size_t pkgStart = m_pc;
            size_t pkgLen = consumePkgLength();
            size_t endOffset = pkgStart + pkgLen;
            size_t condStart = m_pc;
            while (!m_hasReturned) {
                m_pc = condStart;
                uint64_t cond = parseOperand();
                if (!cond) {
                    m_pc = endOffset;
                    break;
                }
                while (m_pc < endOffset && !m_hasReturned) {
                    executeStatement();
                }
            }
            return 0;
        }
        default:
            return 0;
        }
    }
};

// ============================================================================
// 9. ACPI Diagnostic Telemetry
// ============================================================================

struct ACPI_TELEMETRY {
    uint64_t tablesDiscovered;
    uint64_t amlBytecodeOpsExecuted;
    uint64_t gpeEventsDispatched;
    uint64_t powerTransitions;
    uint64_t devicePowerTransitions;
    uint64_t dynamicTablesLoaded;
    uint64_t dynamicTablesUnloaded;
    uint64_t resourceDecodes;
    uint64_t pciRoutingsResolved;
};

// ============================================================================
// 10. ACPI Namespace Tree & High-Level Hardware Device Model
// ============================================================================

class AcpiNamespace {
    std::shared_ptr<AmlNode> m_root;
    mutable std::mutex m_mutex;

public:
    AcpiNamespace() {
        m_root = std::make_shared<AmlNode>("\\", AmlObjectType::Scope);
        populatePredefinedTree();
    }

    std::shared_ptr<AmlNode> getRoot() const { return m_root; }

    std::shared_ptr<AmlNode> resolvePath(std::string_view path) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (path.empty() || path == "\\") return m_root;

        std::string p(path);
        if (p.front() == '\\') p.erase(0, 1);

        auto curr = m_root;
        std::stringstream ss(p);
        std::string segment;
        while (std::getline(ss, segment, '.')) {
            if (segment.empty()) continue;
            auto next = curr->findChild(segment);
            if (!next) return nullptr;
            curr = next;
        }
        return curr;
    }

    AmlValue evaluate(std::string_view path, const std::vector<AmlValue>& args = {}) const {
        auto node = resolvePath(path);
        if (!node) return AmlValue::MakeInteger(0);
        if (node->methodHandler) {
            return node->methodHandler(args);
        }
        return node->value;
    }

private:
    void populatePredefinedTree() {
        // Root Scopes: \_SB (System Bus), \_PR (Processors), \_TZ (Thermal Zones), \_SI (System Indicators)
        auto sb = std::make_shared<AmlNode>("_SB", AmlObjectType::Scope);
        auto pr = std::make_shared<AmlNode>("_PR", AmlObjectType::Scope);
        auto tz = std::make_shared<AmlNode>("_TZ", AmlObjectType::Scope);
        auto si = std::make_shared<AmlNode>("_SI", AmlObjectType::Scope);

        m_root->addChild(sb);
        m_root->addChild(pr);
        m_root->addChild(tz);
        m_root->addChild(si);

        // Sleep State Packages: \_S0 .. \_S5
        auto s0 = std::make_shared<AmlNode>("_S0", AmlObjectType::Package);
        s0->value = AmlValue::MakePackage({ AmlValue::MakeInteger(0), AmlValue::MakeInteger(0) });
        m_root->addChild(s0);

        auto s1 = std::make_shared<AmlNode>("_S1", AmlObjectType::Package);
        s1->value = AmlValue::MakePackage({ AmlValue::MakeInteger(1), AmlValue::MakeInteger(1) });
        m_root->addChild(s1);

        auto s2 = std::make_shared<AmlNode>("_S2", AmlObjectType::Package);
        s2->value = AmlValue::MakePackage({ AmlValue::MakeInteger(2), AmlValue::MakeInteger(2) });
        m_root->addChild(s2);

        auto s3 = std::make_shared<AmlNode>("_S3", AmlObjectType::Package);
        s3->value = AmlValue::MakePackage({ AmlValue::MakeInteger(3), AmlValue::MakeInteger(3) });
        m_root->addChild(s3);

        auto s4 = std::make_shared<AmlNode>("_S4", AmlObjectType::Package);
        s4->value = AmlValue::MakePackage({ AmlValue::MakeInteger(4), AmlValue::MakeInteger(4) });
        m_root->addChild(s4);

        auto s5 = std::make_shared<AmlNode>("_S5", AmlObjectType::Package);
        s5->value = AmlValue::MakePackage({ AmlValue::MakeInteger(5), AmlValue::MakeInteger(5) });
        m_root->addChild(s5);

        // 1. Operating System Interface: \_OSI
        auto osi = std::make_shared<AmlNode>("_OSI", AmlObjectType::Method);
        osi->methodHandler = [](const std::vector<AmlValue>& args) -> AmlValue {
            if (args.empty()) return AmlValue::MakeInteger(0);
            const std::string& os = args[0].stringVal;
            if (os == "Windows 2004" || os == "Windows 2009" || os == "Windows 2015" ||
                os == "Windows 2016" || os == "Windows 2017" || os == "Windows 2018" ||
                os == "Windows 2019" || os == "Windows 2020" || os == "Windows 2021" ||
                os == "Windows 2022" || os == "MicaNT")
            {
                return AmlValue::MakeInteger(0xFFFFFFFFULL);
            }
            return AmlValue::MakeInteger(0);
        };
        m_root->addChild(osi);

        // 2. Prepare To Sleep: \_PTS
        auto pts = std::make_shared<AmlNode>("_PTS", AmlObjectType::Method);
        pts->methodHandler = [](const std::vector<AmlValue>&) -> AmlValue {
            return AmlValue::MakeInteger(0);
        };
        m_root->addChild(pts);

        // 3. System Wake: \_WAK
        auto wak = std::make_shared<AmlNode>("_WAK", AmlObjectType::Method);
        wak->methodHandler = [](const std::vector<AmlValue>&) -> AmlValue {
            return AmlValue::MakePackage({ AmlValue::MakeInteger(0), AmlValue::MakeInteger(0) });
        };
        m_root->addChild(wak);

        // 4. Processors: \_PR.CPU0 .. \_PR.CPU3 (SMP 4-core topology)
        for (uint32_t i = 0; i < 4; ++i) {
            std::string cpuName = "CPU" + std::to_string(i);
            auto cpu = std::make_shared<AmlNode>(cpuName, AmlObjectType::Processor);

            auto hid = std::make_shared<AmlNode>("_HID", AmlObjectType::String);
            hid->value = AmlValue::MakeString("ACPI0007");
            cpu->addChild(hid);

            auto uid = std::make_shared<AmlNode>("_UID", AmlObjectType::Integer);
            uid->value = AmlValue::MakeInteger(i);
            cpu->addChild(uid);

            auto sta = std::make_shared<AmlNode>("_STA", AmlObjectType::Method);
            sta->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(0x0F); };
            cpu->addChild(sta);

            auto pss = std::make_shared<AmlNode>("_PSS", AmlObjectType::Method);
            pss->methodHandler = [](const std::vector<AmlValue>&) {
                return AmlValue::MakePackage({
                    AmlValue::MakePackage({ AmlValue::MakeInteger(3800), AmlValue::MakeInteger(65000), AmlValue::MakeInteger(10), AmlValue::MakeInteger(10), AmlValue::MakeInteger(0), AmlValue::MakeInteger(0) }),
                    AmlValue::MakePackage({ AmlValue::MakeInteger(3200), AmlValue::MakeInteger(45000), AmlValue::MakeInteger(10), AmlValue::MakeInteger(10), AmlValue::MakeInteger(1), AmlValue::MakeInteger(1) }),
                    AmlValue::MakePackage({ AmlValue::MakeInteger(2400), AmlValue::MakeInteger(25000), AmlValue::MakeInteger(10), AmlValue::MakeInteger(10), AmlValue::MakeInteger(2), AmlValue::MakeInteger(2) })
                });
            };
            cpu->addChild(pss);

            pr->addChild(cpu);
        }

        // 5. System Bus Devices: \_SB.PCI0 (PCI Express Root Complex)
        auto pci0 = std::make_shared<AmlNode>("PCI0", AmlObjectType::Device);
        auto pciHid = std::make_shared<AmlNode>("_HID", AmlObjectType::String);
        pciHid->value = AmlValue::MakeString("PNP0A08");
        auto pciCid = std::make_shared<AmlNode>("_CID", AmlObjectType::String);
        pciCid->value = AmlValue::MakeString("PNP0A03");
        auto pciSta = std::make_shared<AmlNode>("_STA", AmlObjectType::Method);
        pciSta->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(0x0F); };

        pci0->addChild(pciHid);
        pci0->addChild(pciCid);
        pci0->addChild(pciSta);

        // PCI Root Bridge _CRS Resources
        std::vector<ACPI_RESOURCE> pciResources;
        ACPI_RESOURCE pciMem{};
        pciMem.type = AcpiResourceType::Memory32;
        pciMem.memory32.minimumAddress = 0x80000000;
        pciMem.memory32.maximumAddress = 0xDFFFFFFF;
        pciMem.memory32.alignment = 0x10000000;
        pciMem.memory32.length = 0x60000000;
        pciMem.memory32.writeProtect = 1;
        pciResources.push_back(pciMem);

        ACPI_RESOURCE pciIo{};
        pciIo.type = AcpiResourceType::FixedIoPort;
        pciIo.fixedIo.baseAddress = 0x0CF8;
        pciIo.fixedIo.length = 8;
        pciResources.push_back(pciIo);

        auto pciCrs = std::make_shared<AmlNode>("_CRS", AmlObjectType::Buffer);
        pciCrs->value = AmlValue::MakeBuffer(EncodeAcpiResources(pciResources));
        pci0->addChild(pciCrs);

        // PCI Interrupt Routing Table (_PRT)
        auto prt = std::make_shared<AmlNode>("_PRT", AmlObjectType::Package);
        prt->value = AmlValue::MakePackage({
            AmlValue::MakePackage({ AmlValue::MakeInteger(0x0001FFFF), AmlValue::MakeInteger(0), AmlValue::MakeInteger(0), AmlValue::MakeInteger(16) }),
            AmlValue::MakePackage({ AmlValue::MakeInteger(0x0002FFFF), AmlValue::MakeInteger(0), AmlValue::MakeInteger(0), AmlValue::MakeInteger(17) }),
            AmlValue::MakePackage({ AmlValue::MakeInteger(0x0003FFFF), AmlValue::MakeInteger(0), AmlValue::MakeInteger(0), AmlValue::MakeInteger(18) }),
            AmlValue::MakePackage({ AmlValue::MakeInteger(0x0004FFFF), AmlValue::MakeInteger(0), AmlValue::MakeInteger(0), AmlValue::MakeInteger(19) }),
            AmlValue::MakePackage({ AmlValue::MakeInteger(0x0005FFFF), AmlValue::MakeInteger(0), AmlValue::MakeInteger(0), AmlValue::MakeInteger(20) })
        });
        pci0->addChild(prt);

        // Helper to attach device power methods (_PS0.._PS3, _PSC) and _CRS
        auto setupDevice = [](std::shared_ptr<AmlNode> dev, uint32_t adr, const std::vector<ACPI_RESOURCE>& devRes) {
            auto devAdr = std::make_shared<AmlNode>("_ADR", AmlObjectType::Integer);
            devAdr->value = AmlValue::MakeInteger(adr);
            dev->addChild(devAdr);

            auto devSta = std::make_shared<AmlNode>("_STA", AmlObjectType::Method);
            devSta->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(0x0F); };
            dev->addChild(devSta);

            auto psc = std::make_shared<AmlNode>("_PSC", AmlObjectType::Method);
            psc->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(0); };
            dev->addChild(psc);

            auto ps0 = std::make_shared<AmlNode>("_PS0", AmlObjectType::Method);
            ps0->methodHandler = [psc](const std::vector<AmlValue>&) {
                psc->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(0); };
                return AmlValue::MakeInteger(0);
            };
            dev->addChild(ps0);

            auto ps1 = std::make_shared<AmlNode>("_PS1", AmlObjectType::Method);
            ps1->methodHandler = [psc](const std::vector<AmlValue>&) {
                psc->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(1); };
                return AmlValue::MakeInteger(0);
            };
            dev->addChild(ps1);

            auto ps2 = std::make_shared<AmlNode>("_PS2", AmlObjectType::Method);
            ps2->methodHandler = [psc](const std::vector<AmlValue>&) {
                psc->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(2); };
                return AmlValue::MakeInteger(0);
            };
            dev->addChild(ps2);

            auto ps3 = std::make_shared<AmlNode>("_PS3", AmlObjectType::Method);
            ps3->methodHandler = [psc](const std::vector<AmlValue>&) {
                psc->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(3); };
                return AmlValue::MakeInteger(0);
            };
            dev->addChild(ps3);

            if (!devRes.empty()) {
                auto crs = std::make_shared<AmlNode>("_CRS", AmlObjectType::Buffer);
                crs->value = AmlValue::MakeBuffer(EncodeAcpiResources(devRes));
                dev->addChild(crs);
            }
        };

        // PrismX 3D Discrete GPU at 01:00.0 (ADR 0x00010000)
        auto gpuDev = std::make_shared<AmlNode>("GFX0", AmlObjectType::Device);
        std::vector<ACPI_RESOURCE> gpuRes;
        ACPI_RESOURCE gpuMem{};
        gpuMem.type = AcpiResourceType::FixedMemory32;
        gpuMem.fixedMemory32.baseAddress = 0xE1000000;
        gpuMem.fixedMemory32.length = 0x01000000; // 16MB
        gpuMem.fixedMemory32.writeProtect = 1;
        gpuRes.push_back(gpuMem);
        ACPI_RESOURCE gpuIrq{};
        gpuIrq.type = AcpiResourceType::ExtendedIrq;
        gpuIrq.extendedIrq.gsi = 16;
        gpuIrq.extendedIrq.triggering = 1; // Level
        gpuIrq.extendedIrq.polarity = 0;   // Active High
        gpuIrq.extendedIrq.sharable = 1;   // Shared
        gpuRes.push_back(gpuIrq);
        setupDevice(gpuDev, 0x00010000, gpuRes);
        pci0->addChild(gpuDev);

        // NVMe SSD Controller at 02:00.0 (ADR 0x00020000)
        auto nvmeDev = std::make_shared<AmlNode>("NVME", AmlObjectType::Device);
        std::vector<ACPI_RESOURCE> nvmeRes;
        ACPI_RESOURCE nvmeMem{};
        nvmeMem.type = AcpiResourceType::FixedMemory32;
        nvmeMem.fixedMemory32.baseAddress = 0xE2000000;
        nvmeMem.fixedMemory32.length = 0x00004000; // 16KB
        nvmeMem.fixedMemory32.writeProtect = 1;
        nvmeRes.push_back(nvmeMem);
        ACPI_RESOURCE nvmeIrq{};
        nvmeIrq.type = AcpiResourceType::ExtendedIrq;
        nvmeIrq.extendedIrq.gsi = 17;
        nvmeIrq.extendedIrq.triggering = 1;
        nvmeIrq.extendedIrq.polarity = 0;
        nvmeIrq.extendedIrq.sharable = 1;
        nvmeRes.push_back(nvmeIrq);
        setupDevice(nvmeDev, 0x00020000, nvmeRes);
        pci0->addChild(nvmeDev);

        // TitanUSB xHCI Controller at 03:00.0 (ADR 0x00030000)
        auto usbDev = std::make_shared<AmlNode>("XUSB", AmlObjectType::Device);
        std::vector<ACPI_RESOURCE> usbRes;
        ACPI_RESOURCE usbMem{};
        usbMem.type = AcpiResourceType::FixedMemory32;
        usbMem.fixedMemory32.baseAddress = 0xE3000000;
        usbMem.fixedMemory32.length = 0x00010000; // 64KB
        usbMem.fixedMemory32.writeProtect = 1;
        usbRes.push_back(usbMem);
        ACPI_RESOURCE usbIrq{};
        usbIrq.type = AcpiResourceType::ExtendedIrq;
        usbIrq.extendedIrq.gsi = 18;
        usbIrq.extendedIrq.triggering = 1;
        usbIrq.extendedIrq.polarity = 0;
        usbIrq.extendedIrq.sharable = 1;
        usbRes.push_back(usbIrq);
        setupDevice(usbDev, 0x00030000, usbRes);
        pci0->addChild(usbDev);

        sb->addChild(pci0);

        // 6. Power Button: \_SB.PWRB
        auto pwrb = std::make_shared<AmlNode>("PWRB", AmlObjectType::Device);
        auto pwrbHid = std::make_shared<AmlNode>("_HID", AmlObjectType::String);
        pwrbHid->value = AmlValue::MakeString("PNP0C0C");
        auto pwrbSta = std::make_shared<AmlNode>("_STA", AmlObjectType::Method);
        pwrbSta->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(0x0B); };
        pwrb->addChild(pwrbHid);
        pwrb->addChild(pwrbSta);

        std::vector<ACPI_RESOURCE> pwrbRes;
        ACPI_RESOURCE pwrIo{};
        pwrIo.type = AcpiResourceType::FixedIoPort;
        pwrIo.fixedIo.baseAddress = 0x0400;
        pwrIo.fixedIo.length = 4;
        pwrbRes.push_back(pwrIo);
        ACPI_RESOURCE pwrIrq{};
        pwrIrq.type = AcpiResourceType::Irq;
        pwrIrq.irq.irqMask = 0x0200; // IRQ 9
        pwrIrq.irq.triggering = 1;
        pwrIrq.irq.polarity = 0;
        pwrIrq.irq.sharable = 1;
        pwrbRes.push_back(pwrIrq);
        auto pwrbCrs = std::make_shared<AmlNode>("_CRS", AmlObjectType::Buffer);
        pwrbCrs->value = AmlValue::MakeBuffer(EncodeAcpiResources(pwrbRes));
        pwrb->addChild(pwrbCrs);

        sb->addChild(pwrb);

        // 7. Smart Battery: \_SB.BAT0
        auto bat0 = std::make_shared<AmlNode>("BAT0", AmlObjectType::Device);
        auto batHid = std::make_shared<AmlNode>("_HID", AmlObjectType::String);
        batHid->value = AmlValue::MakeString("PNP0C0A");
        auto batSta = std::make_shared<AmlNode>("_STA", AmlObjectType::Method);
        batSta->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(0x0F); };

        auto bst = std::make_shared<AmlNode>("_BST", AmlObjectType::Method);
        bst->methodHandler = [](const std::vector<AmlValue>&) {
            return AmlValue::MakePackage({
                AmlValue::MakeInteger(0),     // State: Idle
                AmlValue::MakeInteger(0),     // Present Rate (mW)
                AmlValue::MakeInteger(75000), // Remaining Capacity (mWh)
                AmlValue::MakeInteger(15400)  // Present Voltage (mV)
            });
        };

        auto bif = std::make_shared<AmlNode>("_BIF", AmlObjectType::Method);
        bif->methodHandler = [](const std::vector<AmlValue>&) {
            return AmlValue::MakePackage({
                AmlValue::MakeInteger(0),     // Power Unit: mWh
                AmlValue::MakeInteger(80000), // Design Capacity
                AmlValue::MakeInteger(78000), // Last Full Charge Capacity
                AmlValue::MakeInteger(1),     // Battery Technology: Secondary
                AmlValue::MakeInteger(15400), // Design Voltage
                AmlValue::MakeInteger(5000),  // Design Capacity Warning
                AmlValue::MakeInteger(1000),  // Design Capacity Low
                AmlValue::MakeString("MicaNT Smart Battery"),
                AmlValue::MakeString("BAT-2026-X1"),
                AmlValue::MakeString("Li-Ion"),
                AmlValue::MakeString("Sovereign Power Corp")
            });
        };

        bat0->addChild(batHid);
        bat0->addChild(batSta);
        bat0->addChild(bst);
        bat0->addChild(bif);
        sb->addChild(bat0);

        // 8. Thermal Zone: \_TZ.TZ00
        auto tz00 = std::make_shared<AmlNode>("TZ00", AmlObjectType::ThermalZone);
        auto tmp = std::make_shared<AmlNode>("_TMP", AmlObjectType::Method);
        tmp->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(3182); }; // 45.0 C

        auto crt = std::make_shared<AmlNode>("_CRT", AmlObjectType::Method);
        crt->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(3732); }; // 100.0 C

        auto ac0 = std::make_shared<AmlNode>("_AC0", AmlObjectType::Method);
        ac0->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(3282); }; // 55.0 C

        tz00->addChild(tmp);
        tz00->addChild(crt);
        tz00->addChild(ac0);
        tz->addChild(tz00);
    }
};

// ============================================================================
// 11. ACPI Master Subsystem & Table Repository (TitanACPI)
// ============================================================================

class TitanAcpiSubsystem {
    AcpiRsdp m_rsdp{};
    AcpiTableHeader m_rsdtHeader{};
    std::vector<uint32_t> m_rsdtEntries;
    AcpiTableHeader m_xsdtHeader{};
    std::vector<uint64_t> m_xsdtEntries;
    AcpiFadt m_fadt{};
    AcpiMadtHeader m_madtHeader{};
    std::vector<AcpiMadtLocalApic> m_localApics;
    std::vector<AcpiMadtIoApic> m_ioApics;
    std::vector<AcpiMadtIntrOverride> m_intrOverrides;
    AcpiMcfgHeader m_mcfg{};
    AcpiDmarHeader m_dmar{};
    AcpiSratHeader m_srat{};
    AcpiHpetTable m_hpet{};
    AcpiBgrtTable m_bgrt{};

    std::unordered_map<std::string, std::vector<uint8_t>> m_tableStorage;
    std::unordered_map<std::string, uint64_t> m_tablePhysicalAddresses;

    // Dynamic SSDT Management
    std::unordered_map<uint32_t, std::vector<uint8_t>> m_ssdtTables;
    std::unordered_map<uint32_t, std::string> m_ssdtDevNames;
    uint32_t m_nextSsdtHandle{1};

    AcpiNamespace m_namespace;
    GpeController m_gpeController;
    AmlBytecodeParser m_amlParser;
    AcpiSleepState m_currentSleepState{AcpiSleepState::S0_Working};
    std::unordered_map<std::string, AcpiDevicePowerState> m_devicePowerStates;
    std::vector<AcpiPrtEntry> m_prtEntries;

    ACPI_TELEMETRY m_telemetry{};
    std::atomic<bool> m_initialized{false};
    mutable std::mutex m_mutex;

    TitanAcpiSubsystem() = default;

public:
    static TitanAcpiSubsystem& Instance() {
        static TitanAcpiSubsystem s_inst;
        return s_inst;
    }

    void initialize(uint64_t rsdpPhysical = 0x000000007FEF0000ULL) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized.load()) return;

        // 1. Build RSDP
        std::memcpy(m_rsdp.signature, ACPI_SIG_RSDP, 8);
        std::memcpy(m_rsdp.oemId, "MICANT", 6);
        m_rsdp.revision = 2; // ACPI 2.0+
        m_rsdp.rsdtAddress = static_cast<uint32_t>(rsdpPhysical + 0x1000);
        m_rsdp.length = sizeof(AcpiRsdp);
        m_rsdp.xsdtAddress = rsdpPhysical + 0x2000;
        m_rsdp.checksum = 0;
        m_rsdp.checksum = static_cast<uint8_t>(0x100 - CalculateAcpiChecksum(&m_rsdp, 20));
        m_rsdp.extendedChecksum = static_cast<uint8_t>(0x100 - CalculateAcpiChecksum(&m_rsdp, sizeof(AcpiRsdp)));

        m_tablePhysicalAddresses["RSDP"] = rsdpPhysical;

        // 2. Build FADT
        m_fadt = {};
        std::memcpy(m_fadt.header.signature, "FACP", 4);
        m_fadt.header.length = sizeof(AcpiFadt);
        m_fadt.header.revision = 6;
        std::memcpy(m_fadt.header.oemId, "MICANT", 6);
        std::memcpy(m_fadt.header.oemTableId, "MNTFADT ", 8);
        m_fadt.header.oemRevision = 0x00010000;
        std::memcpy(m_fadt.header.aslCompilerId, "MNT ", 4);
        m_fadt.header.aslCompilerRev = 0x00010000;

        m_fadt.preferredPmProfile = PM_PROFILE_ENTERPRISE_SERVER;
        m_fadt.sciInt = 9; // IRQ 9
        m_fadt.smiCmd = 0x000000B2;
        m_fadt.acpiEnable = 0xA0;
        m_fadt.acpiDisable = 0xA1;
        m_fadt.pmTmrBlk = 0x00000408; // PM Timer port
        m_fadt.pmTmrLen = 4;
        m_fadt.pm1aEvtBlk = 0x00000400;
        m_fadt.pm1aCntBlk = 0x00000404;
        m_fadt.pm1CntLen = 2;
        m_fadt.dsdtAddress = static_cast<uint32_t>(rsdpPhysical + 0x3000);
        m_fadt.xDsdtAddress = rsdpPhysical + 0x3000;
        m_fadt.resetReg.addressSpaceId = 1; // I/O
        m_fadt.resetReg.address = 0xCF9;    // PCI reset port
        m_fadt.resetValue = 0x06;           // Full system reset
        m_fadt.flags = 0x00000425;          // WBINVD, PROC_C1, TMR_VAL_EXT, RESET_REG_SUP

        m_fadt.header.checksum = 0;
        m_fadt.header.checksum = static_cast<uint8_t>(0x100 - CalculateAcpiChecksum(&m_fadt, sizeof(AcpiFadt)));
        m_tablePhysicalAddresses["FACP"] = rsdpPhysical + 0x4000;

        // 3. Build MADT (APIC)
        m_madtHeader = {};
        std::memcpy(m_madtHeader.header.signature, "APIC", 4);
        m_madtHeader.header.length = static_cast<uint32_t>(sizeof(AcpiMadtHeader) + (4 * sizeof(AcpiMadtLocalApic)) + sizeof(AcpiMadtIoApic) + (2 * sizeof(AcpiMadtIntrOverride)));
        m_madtHeader.header.revision = 5;
        std::memcpy(m_madtHeader.header.oemId, "MICANT", 6);
        std::memcpy(m_madtHeader.header.oemTableId, "MNTMADT ", 8);
        m_madtHeader.localApicAddress = 0xFEE00000;
        m_madtHeader.flags = 1; // PC-AT dual 8259 installed

        m_localApics.clear();
        for (uint8_t i = 0; i < 4; ++i) {
            AcpiMadtLocalApic lapic{};
            lapic.header.type = MADT_TYPE_LOCAL_APIC;
            lapic.header.length = sizeof(AcpiMadtLocalApic);
            lapic.processorId = i;
            lapic.apicId = i;
            lapic.flags = 1;
            m_localApics.push_back(lapic);
        }

        m_ioApics.clear();
        AcpiMadtIoApic ioapic{};
        ioapic.header.type = MADT_TYPE_IO_APIC;
        ioapic.header.length = sizeof(AcpiMadtIoApic);
        ioapic.ioApicId = 1;
        ioapic.ioApicAddress = 0xFEC00000;
        ioapic.gsiBase = 0;
        m_ioApics.push_back(ioapic);

        m_intrOverrides.clear();
        AcpiMadtIntrOverride ovr0{};
        ovr0.header.type = MADT_TYPE_INTERRUPT_OVERRIDE;
        ovr0.header.length = sizeof(AcpiMadtIntrOverride);
        ovr0.bus = 0;
        ovr0.source = 0;
        ovr0.gsi = 2;
        ovr0.flags = 0;
        m_intrOverrides.push_back(ovr0);

        AcpiMadtIntrOverride ovr9{};
        ovr9.header.type = MADT_TYPE_INTERRUPT_OVERRIDE;
        ovr9.header.length = sizeof(AcpiMadtIntrOverride);
        ovr9.bus = 0;
        ovr9.source = 9;
        ovr9.gsi = 9;
        ovr9.flags = 0x000F; // Level-triggered, Active High
        m_intrOverrides.push_back(ovr9);

        m_tablePhysicalAddresses["APIC"] = rsdpPhysical + 0x5000;

        // 4. Build MCFG (PCIe ECAM Memory Mapped Config)
        m_mcfg = {};
        std::memcpy(m_mcfg.header.signature, "MCFG", 4);
        m_mcfg.header.length = sizeof(AcpiMcfgHeader);
        m_mcfg.header.revision = 1;
        std::memcpy(m_mcfg.header.oemId, "MICANT", 6);
        std::memcpy(m_mcfg.header.oemTableId, "MNTMCFG ", 8);
        m_mcfg.allocations[0].baseAddress = 0xE0000000ULL;
        m_mcfg.allocations[0].pciSegmentGroup = 0;
        m_mcfg.allocations[0].startBusNumber = 0;
        m_mcfg.allocations[0].endBusNumber = 255;
        m_tablePhysicalAddresses["MCFG"] = rsdpPhysical + 0x6000;

        // 5. Build DMAR (DMA Remapping / Intel VT-d)
        m_dmar = {};
        std::memcpy(m_dmar.header.signature, "DMAR", 4);
        m_dmar.header.length = sizeof(AcpiDmarHeader);
        m_dmar.header.revision = 1;
        std::memcpy(m_dmar.header.oemId, "MICANT", 6);
        std::memcpy(m_dmar.header.oemTableId, "MNTDMAR ", 8);
        m_dmar.hostAddressWidth = 39;
        m_dmar.flags = 0x05;
        m_tablePhysicalAddresses["DMAR"] = rsdpPhysical + 0x7000;

        // 6. Build SRAT (NUMA Node Affinity)
        m_srat = {};
        std::memcpy(m_srat.header.signature, "SRAT", 4);
        m_srat.header.length = sizeof(AcpiSratHeader);
        m_srat.header.revision = 3;
        std::memcpy(m_srat.header.oemId, "MICANT", 6);
        std::memcpy(m_srat.header.oemTableId, "MNTSRAT ", 8);
        m_srat.tableRevision = 1;
        m_tablePhysicalAddresses["SRAT"] = rsdpPhysical + 0x8000;

        // 7. Build HPET (High Precision Event Timer)
        m_hpet = {};
        std::memcpy(m_hpet.header.signature, "HPET", 4);
        m_hpet.header.length = sizeof(AcpiHpetTable);
        m_hpet.header.revision = 1;
        std::memcpy(m_hpet.header.oemId, "MICANT", 6);
        std::memcpy(m_hpet.header.oemTableId, "MNTHPET ", 8);
        m_hpet.header.oemRevision = 0x00010000;
        std::memcpy(m_hpet.header.aslCompilerId, "MNT ", 4);
        m_hpet.header.aslCompilerRev = 0x00010000;
        m_hpet.eventTimerBlockId = 0x8086A001;
        m_hpet.baseAddress.addressSpaceId = 0; // System Memory
        m_hpet.baseAddress.registerBitWidth = 64;
        m_hpet.baseAddress.registerBitOffset = 0;
        m_hpet.baseAddress.accessSize = 4; // QWord
        m_hpet.baseAddress.address = 0xFED00000ULL;
        m_hpet.hpetNumber = 0;
        m_hpet.mainCounterMinClockTick = 100;
        m_hpet.pageProtectionAndOemAttribute = 0;
        m_hpet.header.checksum = 0;
        m_hpet.header.checksum = static_cast<uint8_t>(0x100 - CalculateAcpiChecksum(&m_hpet, sizeof(AcpiHpetTable)));
        m_tablePhysicalAddresses["HPET"] = rsdpPhysical + 0x9000;

        // 8. Build BGRT (Boot Graphics Resource Table)
        m_bgrt = {};
        std::memcpy(m_bgrt.header.signature, "BGRT", 4);
        m_bgrt.header.length = sizeof(AcpiBgrtTable);
        m_bgrt.header.revision = 1;
        std::memcpy(m_bgrt.header.oemId, "MICANT", 6);
        std::memcpy(m_bgrt.header.oemTableId, "MNTBGRT ", 8);
        m_bgrt.header.oemRevision = 0x00010000;
        std::memcpy(m_bgrt.header.aslCompilerId, "MNT ", 4);
        m_bgrt.header.aslCompilerRev = 0x00010000;
        m_bgrt.version = 1;
        m_bgrt.status = 1; // Displayed
        m_bgrt.imageType = 0; // Bitmap
        m_bgrt.imageAddress = 0x000000007FE00000ULL;
        m_bgrt.imageOffsetX = 384;
        m_bgrt.imageOffsetY = 284;
        m_bgrt.header.checksum = 0;
        m_bgrt.header.checksum = static_cast<uint8_t>(0x100 - CalculateAcpiChecksum(&m_bgrt, sizeof(AcpiBgrtTable)));
        m_tablePhysicalAddresses["BGRT"] = rsdpPhysical + 0xA000;

        // 9. Build XSDT
        m_xsdtEntries = {
            rsdpPhysical + 0x4000, // FACP
            rsdpPhysical + 0x5000, // APIC
            rsdpPhysical + 0x6000, // MCFG
            rsdpPhysical + 0x7000, // DMAR
            rsdpPhysical + 0x8000, // SRAT
            rsdpPhysical + 0x9000, // HPET
            rsdpPhysical + 0xA000  // BGRT
        };
        m_xsdtHeader = {};
        std::memcpy(m_xsdtHeader.signature, "XSDT", 4);
        m_xsdtHeader.length = static_cast<uint32_t>(sizeof(AcpiTableHeader) + (m_xsdtEntries.size() * sizeof(uint64_t)));
        m_xsdtHeader.revision = 1;
        std::memcpy(m_xsdtHeader.oemId, "MICANT", 6);
        std::memcpy(m_xsdtHeader.oemTableId, "MNTXSDT ", 8);
        m_tablePhysicalAddresses["XSDT"] = m_rsdp.xsdtAddress;

        // 10. Build RSDT
        m_rsdtEntries = {
            static_cast<uint32_t>(rsdpPhysical + 0x4000),
            static_cast<uint32_t>(rsdpPhysical + 0x5000),
            static_cast<uint32_t>(rsdpPhysical + 0x6000),
            static_cast<uint32_t>(rsdpPhysical + 0x7000),
            static_cast<uint32_t>(rsdpPhysical + 0x8000),
            static_cast<uint32_t>(rsdpPhysical + 0x9000),
            static_cast<uint32_t>(rsdpPhysical + 0xA000)
        };
        m_rsdtHeader = {};
        std::memcpy(m_rsdtHeader.signature, "RSDT", 4);
        m_rsdtHeader.length = static_cast<uint32_t>(sizeof(AcpiTableHeader) + (m_rsdtEntries.size() * sizeof(uint32_t)));
        m_rsdtHeader.revision = 1;
        std::memcpy(m_rsdtHeader.oemId, "MICANT", 6);
        std::memcpy(m_rsdtHeader.oemTableId, "MNTRSDT ", 8);
        m_tablePhysicalAddresses["RSDT"] = m_rsdp.rsdtAddress;

        // 11. PCI Interrupt Routing Table Defaults
        m_prtEntries = {
            { 0x0001FFFF, 0, 16 }, // GFX0 Pin INTA -> GSI 16
            { 0x0002FFFF, 0, 17 }, // NVME Pin INTA -> GSI 17
            { 0x0003FFFF, 0, 18 }, // XUSB Pin INTA -> GSI 18
            { 0x0004FFFF, 0, 19 }, // RazzleNet Pin INTA -> GSI 19
            { 0x0005FFFF, 0, 20 }  // VirtIO Pin INTA -> GSI 20
        };

        // 12. Default Device Power States
        m_devicePowerStates["\\_SB.PCI0.GFX0"] = AcpiDevicePowerState::D0;
        m_devicePowerStates["\\_SB.PCI0.NVME"] = AcpiDevicePowerState::D0;
        m_devicePowerStates["\\_SB.PCI0.XUSB"] = AcpiDevicePowerState::D0;

        m_telemetry.tablesDiscovered = m_tablePhysicalAddresses.size();
        m_initialized.store(true);
    }

    bool isInitialized() const { return m_initialized.load(); }
    const AcpiRsdp& getRsdp() const { return m_rsdp; }
    const AcpiTableHeader& getRsdtHeader() const { return m_rsdtHeader; }
    const std::vector<uint32_t>& getRsdtEntries() const { return m_rsdtEntries; }
    const AcpiTableHeader& getXsdtHeader() const { return m_xsdtHeader; }
    const std::vector<uint64_t>& getXsdtEntries() const { return m_xsdtEntries; }
    const AcpiFadt& getFadt() const { return m_fadt; }
    const AcpiMadtHeader& getMadtHeader() const { return m_madtHeader; }
    const std::vector<AcpiMadtLocalApic>& getLocalApics() const { return m_localApics; }
    const std::vector<AcpiMadtIoApic>& getIoApics() const { return m_ioApics; }
    const std::vector<AcpiMadtIntrOverride>& getIntrOverrides() const { return m_intrOverrides; }
    const AcpiMcfgHeader& getMcfg() const { return m_mcfg; }
    const AcpiDmarHeader& getDmar() const { return m_dmar; }
    const AcpiSratHeader& getSrat() const { return m_srat; }
    const AcpiHpetTable& getHpet() const { return m_hpet; }
    const AcpiBgrtTable& getBgrt() const { return m_bgrt; }
    const AcpiNamespace& getNamespace() const { return m_namespace; }
    AcpiNamespace& getNamespace() { return m_namespace; }

    GpeController& getGpeController() { return m_gpeController; }
    const GpeController& getGpeController() const { return m_gpeController; }

    AmlBytecodeParser& getAmlParser() { return m_amlParser; }

    uint64_t executeAmlBytecode(std::span<const uint8_t> bytecode, const std::vector<AmlValue>& args = {}) {
        return m_amlParser.executeBytecode(bytecode, args);
    }

    void recordAmlOp() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.amlBytecodeOpsExecuted++;
    }

    void recordGpeDispatched() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.gpeEventsDispatched++;
    }

    void recordResourceDecode() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.resourceDecodes++;
    }

    uint64_t getTablePhysicalAddress(std::string_view sig) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_tablePhysicalAddresses.find(std::string(sig));
        if (it != m_tablePhysicalAddresses.end()) return it->second;
        return 0;
    }

    std::vector<std::pair<std::string, uint64_t>> getAllTables() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::pair<std::string, uint64_t>> res;
        for (const auto& [sig, addr] : m_tablePhysicalAddresses) {
            res.push_back({sig, addr});
        }
        std::sort(res.begin(), res.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        return res;
    }

    AcpiSleepState getSleepState() const { return m_currentSleepState; }

    bool transitionSleepState(AcpiSleepState target) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_namespace.evaluate("\\_PTS", { AmlValue::MakeInteger(static_cast<uint8_t>(target)) });
        m_currentSleepState = target;
        m_telemetry.powerTransitions++;
        if (target != AcpiSleepState::S0_Working) {
            m_namespace.evaluate("\\_WAK", { AmlValue::MakeInteger(static_cast<uint8_t>(target)) });
            m_currentSleepState = AcpiSleepState::S0_Working;
        }
        return true;
    }

    bool setDevicePowerState(std::string_view devicePath, AcpiDevicePowerState state) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string path(devicePath);
        std::string method;
        switch (state) {
        case AcpiDevicePowerState::D0: method = path + "._PS0"; break;
        case AcpiDevicePowerState::D1: method = path + "._PS1"; break;
        case AcpiDevicePowerState::D2: method = path + "._PS2"; break;
        case AcpiDevicePowerState::D3hot:
        case AcpiDevicePowerState::D3cold: method = path + "._PS3"; break;
        }

        m_namespace.evaluate(method);
        m_devicePowerStates[path] = state;
        m_telemetry.devicePowerTransitions++;
        return true;
    }

    AcpiDevicePowerState getDevicePowerState(std::string_view devicePath) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_devicePowerStates.find(std::string(devicePath));
        if (it != m_devicePowerStates.end()) return it->second;
        return AcpiDevicePowerState::D0;
    }

    bool translatePciRouting(uint32_t pciAddress, uint8_t pin, uint32_t* outGsi) {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint32_t dev = (pciAddress >> 16) & 0xFFFF;
        for (const auto& entry : m_prtEntries) {
            uint32_t entryDev = (entry.address >> 16) & 0xFFFF;
            if (entryDev == dev && entry.pin == pin) {
                if (outGsi) *outGsi = entry.gsi;
                m_telemetry.pciRoutingsResolved++;
                return true;
            }
        }
        return false;
    }

    uint32_t loadSsdtTable(const uint8_t* tableBytes, uint32_t length) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!tableBytes || length < sizeof(AcpiTableHeader)) return 0;
        const auto* hdr = reinterpret_cast<const AcpiTableHeader*>(tableBytes);
        if (std::memcmp(hdr->signature, "SSDT", 4) != 0) return 0;
        if (!VerifyAcpiChecksum(tableBytes, length)) return 0;

        uint32_t handle = m_nextSsdtHandle++;
        std::vector<uint8_t> data(tableBytes, tableBytes + length);
        m_ssdtTables[handle] = data;

        std::string key = "SSDT" + std::to_string(handle);
        uint64_t physAddr = 0x000000007FEA0000ULL + (handle * 0x1000);
        m_tablePhysicalAddresses[key] = physAddr;

        auto sb = m_namespace.resolvePath("\\_SB");
        if (sb) {
            std::string devName = "HOTP" + std::to_string(handle);
            auto dynDev = std::make_shared<AmlNode>(devName, AmlObjectType::Device);
            auto hid = std::make_shared<AmlNode>("_HID", AmlObjectType::String);
            hid->value = AmlValue::MakeString("MNT0001");
            dynDev->addChild(hid);
            auto sta = std::make_shared<AmlNode>("_STA", AmlObjectType::Method);
            sta->methodHandler = [](const std::vector<AmlValue>&) { return AmlValue::MakeInteger(0x0F); };
            dynDev->addChild(sta);
            sb->addChild(dynDev);
            m_ssdtDevNames[handle] = devName;
        }

        m_telemetry.dynamicTablesLoaded++;
        m_telemetry.tablesDiscovered = m_tablePhysicalAddresses.size();
        return handle;
    }

    bool unloadSsdtTable(uint32_t handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_ssdtTables.find(handle);
        if (it == m_ssdtTables.end()) return false;
        m_ssdtTables.erase(it);

        std::string key = "SSDT" + std::to_string(handle);
        m_tablePhysicalAddresses.erase(key);

        auto devIt = m_ssdtDevNames.find(handle);
        if (devIt != m_ssdtDevNames.end()) {
            auto sb = m_namespace.resolvePath("\\_SB");
            if (sb) {
                sb->children.erase(devIt->second);
            }
            m_ssdtDevNames.erase(devIt);
        }

        m_telemetry.dynamicTablesUnloaded++;
        m_telemetry.tablesDiscovered = m_tablePhysicalAddresses.size();
        return true;
    }

    ACPI_TELEMETRY getTelemetry() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        ACPI_TELEMETRY t = m_telemetry;
        t.gpeEventsDispatched = std::max(m_telemetry.gpeEventsDispatched, m_gpeController.getDispatchedCount());
        return t;
    }

    void resetTelemetry() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry = {};
        m_telemetry.tablesDiscovered = m_tablePhysicalAddresses.size();
        m_gpeController.resetTelemetry();
        m_amlParser.reset();
    }
};

inline void AmlBytecodeParser::recordOp() {
    m_opsExecuted++;
    TitanAcpiSubsystem::Instance().recordAmlOp();
}

inline uint64_t AmlBytecodeParser::executeBytecode(std::span<const uint8_t> bytecode, const std::vector<AmlValue>& args) {
    reset();
    m_bytecode = bytecode;
    for (size_t i = 0; i < args.size() && i < 7; ++i) {
        m_args[i] = args[i].integerVal;
    }

    uint64_t lastVal = 0;
    while (m_pc < m_bytecode.size() && !m_hasReturned) {
        lastVal = executeStatement();
    }
    return m_hasReturned ? m_returnValue : lastVal;
}

// ============================================================================
// 12. Windows ACPI Driver & HAL C ABI Exports (acpi.sys and hal.dll)
// ============================================================================

extern "C" {

inline BOOL WINAPI AcpiFindTable(const char* signature, uint64_t* outPhysicalAddress, uint32_t* outLength) {
    if (!signature || !outPhysicalAddress) return FALSE;
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    uint64_t addr = acpi.getTablePhysicalAddress(signature);
    if (addr == 0) return FALSE;

    *outPhysicalAddress = addr;
    if (outLength) {
        std::string sig(signature);
        if (sig == "FACP") *outLength = sizeof(AcpiFadt);
        else if (sig == "APIC") *outLength = sizeof(AcpiMadtHeader);
        else if (sig == "MCFG") *outLength = sizeof(AcpiMcfgHeader);
        else if (sig == "DMAR") *outLength = sizeof(AcpiDmarHeader);
        else if (sig == "SRAT") *outLength = sizeof(AcpiSratHeader);
        else if (sig == "HPET") *outLength = sizeof(AcpiHpetTable);
        else if (sig == "BGRT") *outLength = sizeof(AcpiBgrtTable);
        else *outLength = sizeof(AcpiTableHeader);
    }
    return TRUE;
}

inline BOOL WINAPI AcpiEvaluateObject(const char* path, uint64_t* outInteger) {
    if (!path || !outInteger) return FALSE;
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    auto val = acpi.getNamespace().evaluate(path);
    *outInteger = val.integerVal;
    return TRUE;
}

inline BOOL WINAPI AcpiGetSystemPowerState(uint8_t* outState) {
    if (!outState) return FALSE;
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    *outState = static_cast<uint8_t>(acpi.getSleepState());
    return TRUE;
}

inline BOOL WINAPI AcpiSetSystemPowerState(uint8_t state) {
    if (state > 5) return FALSE;
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    return acpi.transitionSleepState(static_cast<AcpiSleepState>(state)) ? TRUE : FALSE;
}

inline BOOL WINAPI AcpiGetThermalZoneTemp(uint32_t* outKelvinTenths) {
    if (!outKelvinTenths) return FALSE;
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    auto val = acpi.getNamespace().evaluate("\\_TZ.TZ00._TMP");
    *outKelvinTenths = static_cast<uint32_t>(val.integerVal);
    return TRUE;
}

inline BOOL WINAPI AcpiGetBatteryStatus(
    uint32_t* outState,
    uint32_t* outPresentRate,
    uint32_t* outRemainingCapacity,
    uint32_t* outPresentVoltage
) {
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    auto val = acpi.getNamespace().evaluate("\\_SB.BAT0._BST");
    if (val.type != AmlObjectType::Package || val.packageElements.size() < 4) return FALSE;

    if (outState) *outState = static_cast<uint32_t>(val.packageElements[0].integerVal);
    if (outPresentRate) *outPresentRate = static_cast<uint32_t>(val.packageElements[1].integerVal);
    if (outRemainingCapacity) *outRemainingCapacity = static_cast<uint32_t>(val.packageElements[2].integerVal);
    if (outPresentVoltage) *outPresentVoltage = static_cast<uint32_t>(val.packageElements[3].integerVal);
    return TRUE;
}

inline uint32_t WINAPI AcpiGetProcessorCount() {
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();
    return static_cast<uint32_t>(acpi.getLocalApics().size());
}

inline BOOL WINAPI AcpiRegisterGpeHandler(uint32_t gpeNumber, void (*handler)(uint32_t)) {
    if (gpeNumber >= 64 || !handler) return FALSE;
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    acpi.getGpeController().registerHandler(gpeNumber, [handler](uint32_t g) {
        handler(g);
    });
    return TRUE;
}

inline BOOL WINAPI AcpiDispatchGpe(uint32_t gpeNumber) {
    if (gpeNumber >= 64) return FALSE;
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    bool ok = acpi.getGpeController().dispatchGpe(gpeNumber);
    if (ok) {
        acpi.recordGpeDispatched();
    }
    return ok ? TRUE : FALSE;
}

inline BOOL WINAPI AcpiGetDeviceResources(
    const char* devicePath,
    ACPI_RESOURCE* outResources,
    uint32_t maxResources,
    uint32_t* outCount
) {
    if (!devicePath || !outCount) return FALSE;
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    std::string path(devicePath);
    std::string crsPath = path + "._CRS";
    auto val = acpi.getNamespace().evaluate(crsPath);
    if (val.type != AmlObjectType::Buffer || val.bufferVal.empty()) {
        return FALSE;
    }

    auto decoded = DecodeAcpiResources(val.bufferVal);
    acpi.recordResourceDecode();

    *outCount = static_cast<uint32_t>(decoded.size());
    if (outResources && maxResources > 0) {
        uint32_t copyCount = std::min(maxResources, static_cast<uint32_t>(decoded.size()));
        for (uint32_t i = 0; i < copyCount; ++i) {
            outResources[i] = decoded[i];
        }
    }
    return TRUE;
}

inline BOOL WINAPI AcpiTranslatePciRouting(uint32_t pciAddress, uint8_t pin, uint32_t* outGsi) {
    if (!outGsi) return FALSE;
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    return acpi.translatePciRouting(pciAddress, pin, outGsi) ? TRUE : FALSE;
}

inline BOOL WINAPI AcpiSetDevicePowerState(const char* devicePath, uint8_t state) {
    if (!devicePath || state > 4) return FALSE;
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    return acpi.setDevicePowerState(devicePath, static_cast<AcpiDevicePowerState>(state)) ? TRUE : FALSE;
}

inline BOOL WINAPI AcpiLoadTable(const uint8_t* tableBytes, uint32_t length, uint32_t* outHandle) {
    if (!tableBytes || length == 0 || !outHandle) return FALSE;
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    uint32_t h = acpi.loadSsdtTable(tableBytes, length);
    if (h == 0) return FALSE;
    *outHandle = h;
    return TRUE;
}

inline BOOL WINAPI AcpiUnloadTable(uint32_t handle) {
    if (handle == 0) return FALSE;
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    return acpi.unloadSsdtTable(handle) ? TRUE : FALSE;
}

inline BOOL WINAPI AcpiGetTelemetry(ACPI_TELEMETRY* outTelemetry) {
    if (!outTelemetry) return FALSE;
    auto& acpi = TitanAcpiSubsystem::Instance();
    if (!acpi.isInitialized()) acpi.initialize();

    *outTelemetry = acpi.getTelemetry();
    return TRUE;
}

inline ULONG WINAPI HalGetBusDataByOffset(
    BUS_DATA_TYPE busDataType,
    ULONG busNumber,
    ULONG slotNumber,
    PVOID buffer,
    ULONG offset,
    ULONG length
) {
    if (!buffer || length == 0) return 0;
    if (busDataType != PCIConfiguration) return 0;

    uint8_t dev = static_cast<uint8_t>((slotNumber >> 3) & 0x1F);
    uint8_t fn = static_cast<uint8_t>(slotNumber & 0x07);
    uint32_t bdf = ((busNumber & 0xFF) << 8) | ((dev & 0x1F) << 3) | (fn & 0x07);

    uint8_t* out = reinterpret_cast<uint8_t*>(buffer);
    for (ULONG i = 0; i < length; ++i) {
        out[i] = pci::PciReadConfigByte(bdf, static_cast<uint16_t>(offset + i));
    }
    return length;
}

inline ULONG WINAPI HalSetBusDataByOffset(
    BUS_DATA_TYPE busDataType,
    ULONG busNumber,
    ULONG slotNumber,
    PVOID buffer,
    ULONG offset,
    ULONG length
) {
    if (!buffer || length == 0) return 0;
    if (busDataType != PCIConfiguration) return 0;

    uint8_t dev = static_cast<uint8_t>((slotNumber >> 3) & 0x1F);
    uint8_t fn = static_cast<uint8_t>(slotNumber & 0x07);
    uint32_t bdf = ((busNumber & 0xFF) << 8) | ((dev & 0x1F) << 3) | (fn & 0x07);

    const uint8_t* in = reinterpret_cast<const uint8_t*>(buffer);
    for (ULONG i = 0; i < length; ++i) {
        pci::PciWriteConfigByte(bdf, static_cast<uint16_t>(offset + i), in[i]);
    }
    return length;
}

} // extern "C"

// C++ Telemetry helpers outside extern "C"
inline ACPI_TELEMETRY AcpiQueryTelemetry() {
    return TitanAcpiSubsystem::Instance().getTelemetry();
}

inline void AcpiResetTelemetryForTesting() {
    TitanAcpiSubsystem::Instance().resetTelemetry();
}

// ============================================================================
// 13. Subsystem Initialization & SCM Driver Registration
// ============================================================================

inline void InitializeAcpiSubsystem() {
    static bool s_registered = false;
    if (s_registered) return;
    s_registered = true;

    // 1. Initialize Subsystem Singleton & Table Tree
    TitanAcpiSubsystem::Instance().initialize();

    // 2. Dynamic Loader Exports for acpi.sys
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("acpi.sys", "AcpiFindTable", reinterpret_cast<void*>(AcpiFindTable));
    ldr.registerExport("acpi.sys", "AcpiEvaluateObject", reinterpret_cast<void*>(AcpiEvaluateObject));
    ldr.registerExport("acpi.sys", "AcpiGetSystemPowerState", reinterpret_cast<void*>(AcpiGetSystemPowerState));
    ldr.registerExport("acpi.sys", "AcpiSetSystemPowerState", reinterpret_cast<void*>(AcpiSetSystemPowerState));
    ldr.registerExport("acpi.sys", "AcpiGetThermalZoneTemp", reinterpret_cast<void*>(AcpiGetThermalZoneTemp));
    ldr.registerExport("acpi.sys", "AcpiGetBatteryStatus", reinterpret_cast<void*>(AcpiGetBatteryStatus));
    ldr.registerExport("acpi.sys", "AcpiGetProcessorCount", reinterpret_cast<void*>(AcpiGetProcessorCount));
    ldr.registerExport("acpi.sys", "AcpiRegisterGpeHandler", reinterpret_cast<void*>(AcpiRegisterGpeHandler));
    ldr.registerExport("acpi.sys", "AcpiDispatchGpe", reinterpret_cast<void*>(AcpiDispatchGpe));
    ldr.registerExport("acpi.sys", "AcpiGetDeviceResources", reinterpret_cast<void*>(AcpiGetDeviceResources));
    ldr.registerExport("acpi.sys", "AcpiTranslatePciRouting", reinterpret_cast<void*>(AcpiTranslatePciRouting));
    ldr.registerExport("acpi.sys", "AcpiSetDevicePowerState", reinterpret_cast<void*>(AcpiSetDevicePowerState));
    ldr.registerExport("acpi.sys", "AcpiLoadTable", reinterpret_cast<void*>(AcpiLoadTable));
    ldr.registerExport("acpi.sys", "AcpiUnloadTable", reinterpret_cast<void*>(AcpiUnloadTable));
    ldr.registerExport("acpi.sys", "AcpiGetTelemetry", reinterpret_cast<void*>(AcpiGetTelemetry));

    // 3. Dynamic Loader Exports for hal.dll
    ldr.registerExport("hal.dll", "HalGetBusDataByOffset", reinterpret_cast<void*>(HalGetBusDataByOffset));
    ldr.registerExport("hal.dll", "HalSetBusDataByOffset", reinterpret_cast<void*>(HalSetBusDataByOffset));

    // 4. Register ACPI Platform Driver in SCM
    auto acpiSvc = std::make_shared<scm::ServiceRecord>();
    acpiSvc->serviceName = L"acpi";
    acpiSvc->displayName = L"MicaNT ACPI 6.5 Platform Driver";
    acpiSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    acpiSvc->startType = scm::SERVICE_BOOT_START;
    acpiSvc->errorControl = scm::SERVICE_ERROR_CRITICAL;
    acpiSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\acpi.sys";
    acpiSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(acpiSvc);

    // 5. Register Version Database Information
    version::VersionDatabase::Instance().RegisterModule(
        "acpi.sys",
        "10.0.22621.1",
        "MicaNT Sovereign ACPI 6.5 Platform Driver"
    );
}

} // namespace micant::acpi

namespace micant {
    using acpi::TitanAcpiSubsystem;
    using acpi::AcpiNamespace;
    using acpi::AmlNode;
    using acpi::AmlValue;
    using acpi::AmlObjectType;
    using acpi::AcpiSleepState;
    using acpi::AcpiDevicePowerState;
    using acpi::AcpiResourceType;
    using acpi::ACPI_RESOURCE;
    using acpi::AcpiPrtEntry;
    using acpi::GpeController;
    using acpi::AmlBytecodeParser;
    using acpi::AcpiHpetTable;
    using acpi::AcpiBgrtTable;
    using acpi::ACPI_TELEMETRY;
    using acpi::EncodeAcpiResources;
    using acpi::DecodeAcpiResources;
    using acpi::AcpiQueryTelemetry;
    using acpi::AcpiResetTelemetryForTesting;
}
