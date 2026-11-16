/**
 * @file prismx_uefi_bootloader_iso_wddm32.cpp
 * @brief Horizon 312 (Kernel Unit #88): Sovereign MicaNT Kernel UEFI Bootloader,
 *        GPT Disk Partitioner & Setup ISO Tooling Subsystem (micant_setup.iso)
 *        - El Torito Specification (Platform 0xEF UEFI) & ISO 9660 Bootable Image Generator
 *        - Automated GUID Partition Table (GPT) & Protective MBR (Type 0xEE) Formatter
 *        - EFI System Partition (ESP) & MicaNT System Volume Layout Engine
 *        - Seamless UEFI GOP Linear Framebuffer Handover to WDDM 3.2 DirectFlip Pipeline
 *        - Bootstraps Pure ISO C++23 NT Microkernel Directly on Bare-Metal x86_64 Silicon.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_uefi_bootloader_iso_wddm32.hpp"

namespace micant::um::prismx_boot {

bool VerifyHorizon312UefiBootloaderIsoSubsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: Boot ABI Structures & Specification Alignment Verification
    // ------------------------------------------------------------------------
    if (sizeof(ProtectiveMbrSector) != 512u) return false;
    if (sizeof(GptHeaderDescriptor) != 92u) return false;
    if (sizeof(GptPartitionEntry) != 128u) return false;
    if (sizeof(ElToritoBootCatalogValidationEntry) != 32u) return false;

    if (GPT_HEADER_SIGNATURE != 0x5452415020494645ULL) return false; // "EFI PART"
    if (EL_TORITO_UEFI_PLATFORM_ID != 0xEF) return false;            // UEFI Platform ID

    // ------------------------------------------------------------------------
    // Pillar 2: GUID Partition Table (GPT) & Protective MBR Formatting
    // ------------------------------------------------------------------------
    MicaNtBootSetupEngine engine;

    ProtectiveMbrSector mbr{};
    GptHeaderDescriptor gptHeader{};
    std::vector<GptPartitionEntry> partitionEntries;

    // Reject disks under 32 MB
    if (NT_SUCCESS(engine.FormatGptDisk(32768ULL, mbr, gptHeader, partitionEntries))) {
        return false;
    }

    // Format a 50 GiB NVMe drive (104,857,600 512-byte LBAs)
    const uint64_t targetDiskSectors = 104857600ULL;
    NTSTATUS status = engine.FormatGptDisk(targetDiskSectors, mbr, gptHeader, partitionEntries);
    if (!NT_SUCCESS(status)) return false;

    // Validate Protective MBR
    if (mbr.BootSignature != 0xAA55) return false;
    if (mbr.PartitionRecord[0].PartitionType != 0xEE) return false;
    if (mbr.PartitionRecord[0].StartingLBA != 1) return false;

    // Validate Primary GPT Header
    if (gptHeader.Signature != GPT_HEADER_SIGNATURE) return false;
    if (gptHeader.Revision != GPT_HEADER_REVISION) return false;
    if (gptHeader.HeaderSize != 92u) return false;
    if (gptHeader.HeaderCRC32 == 0) return false;
    if (gptHeader.MyLBA != 1) return false;
    if (gptHeader.AlternateLBA != targetDiskSectors - 1) return false;
    if (gptHeader.FirstUsableLBA != 34) return false;
    if (gptHeader.LastUsableLBA != targetDiskSectors - 34) return false;
    if (gptHeader.NumberOfPartitionEntries != 128u) return false;
    if (gptHeader.SizeOfPartitionEntry != 128u) return false;
    if (gptHeader.PartitionEntryArrayCRC32 == 0) return false;

    // Validate EFI System Partition (ESP)
    if (partitionEntries.size() != 128u) return false;
    if (std::memcmp(partitionEntries[0].PartitionTypeGUID, ESP_PARTITION_GUID.data(), 16) != 0) {
        return false;
    }
    if (partitionEntries[0].StartingLBA != 2048) return false;
    if (partitionEntries[0].EndingLBA != 2048 + 532480 - 1) return false; // 260 MB ESP

    // Validate MicaNT System Partition
    if (std::memcmp(partitionEntries[1].PartitionTypeGUID, MICANT_SYSTEM_PARTITION_GUID.data(), 16) != 0) {
        return false;
    }
    if (partitionEntries[1].StartingLBA != partitionEntries[0].EndingLBA + 1) return false;
    if (partitionEntries[1].EndingLBA != targetDiskSectors - 34) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: El Torito UEFI Boot Catalog Validation
    // ------------------------------------------------------------------------
    ElToritoBootCatalogValidationEntry validEntry{};
    validEntry.HeaderID = 0x01;
    validEntry.PlatformID = EL_TORITO_UEFI_PLATFORM_ID;
    validEntry.KeySignature = 0xAA55;
    status = engine.VerifyElToritoBootCatalog(validEntry);
    if (!NT_SUCCESS(status)) return false;

    // Reject non-UEFI platform ID (e.g. legacy BIOS 0x00)
    ElToritoBootCatalogValidationEntry legacyEntry = validEntry;
    legacyEntry.PlatformID = 0x00;
    if (NT_SUCCESS(engine.VerifyElToritoBootCatalog(legacyEntry))) return false;

    // Reject invalid key signature
    ElToritoBootCatalogValidationEntry corruptEntry = validEntry;
    corruptEntry.KeySignature = 0x1234;
    if (NT_SUCCESS(engine.VerifyElToritoBootCatalog(corruptEntry))) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: Seamless UEFI GOP Linear Framebuffer Handover to WDDM 3.2
    // ------------------------------------------------------------------------
    const uint64_t uefiGopLfb = 0x0000000E00000000ULL;
    const uint32_t screenWidth = 3840;
    const uint32_t screenHeight = 2160;

    status = engine.HandoverUefiGopToWddm32(uefiGopLfb, screenWidth, screenHeight);
    if (!NT_SUCCESS(status)) return false;

    // Verify parameter validation
    if (NT_SUCCESS(engine.HandoverUefiGopToWddm32(0, screenWidth, screenHeight))) return false;
    if (NT_SUCCESS(engine.HandoverUefiGopToWddm32(uefiGopLfb, 0, screenHeight))) return false;
    if (NT_SUCCESS(engine.HandoverUefiGopToWddm32(uefiGopLfb, screenWidth, 0))) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Boot Telemetry & Verification Sign-Off
    // ------------------------------------------------------------------------
    const auto& tel = engine.GetTelemetry();
    if (!tel.ProtectiveMbrValid) return false;
    if (!tel.GptHeaderValid) return false;
    if (!tel.EspPartitionValid) return false;
    if (!tel.ElToritoUefiCatalogValid) return false;
    if (!tel.UefiGopHandoverVerified) return false;
    if (tel.TotalSectorsGenerated != targetDiskSectors) return false;
    if (tel.GptCrc32Calculated != gptHeader.HeaderCRC32) return false;

    return true;
}

} // namespace micant::um::prismx_boot
