#pragma once

#include <cstdint>
#include <string_view>
#include <span>
#include <vector>
#include <optional>
#include "ntstatus.hpp"
#include "mm.hpp"

namespace micant::pe {

inline constexpr uint16_t DOS_MAGIC = 0x5A4D; // 'MZ'
inline constexpr uint32_t NT_SIGNATURE = 0x00004550; // 'PE\0\0'
inline constexpr uint16_t MACHINE_AMD64 = 0x8664;
inline constexpr uint16_t MACHINE_I386  = 0x014C;
inline constexpr uint16_t PE32PLUS_MAGIC = 0x020B; // 64-bit Optional Header
inline constexpr uint16_t PE32_MAGIC     = 0x010B; // 32-bit Optional Header

#pragma pack(push, 1)

struct ImageDosHeader {
    uint16_t e_magic;      // Magic number ('MZ')
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    int32_t  e_lfanew;     // Offset to NT headers
};

struct ImageFileHeader {
    uint16_t machine;
    uint16_t numberOfSections;
    uint32_t timeDateStamp;
    uint32_t pointerToSymbolTable;
    uint32_t numberOfSymbols;
    uint16_t sizeOfOptionalHeader;
    uint16_t characteristics;
};

struct ImageDataDirectory {
    uint32_t virtualAddress;
    uint32_t size;
};

struct ImageOptionalHeader64 {
    uint16_t magic;
    uint8_t  majorLinkerVersion;
    uint8_t  minorLinkerVersion;
    uint32_t sizeOfCode;
    uint32_t sizeOfInitializedData;
    uint32_t sizeOfUninitializedData;
    uint32_t addressOfEntryPoint;
    uint32_t baseOfCode;
    uint64_t imageBase;
    uint32_t sectionAlignment;
    uint32_t fileAlignment;
    uint16_t majorOperatingSystemVersion;
    uint16_t minorOperatingSystemVersion;
    uint16_t majorImageVersion;
    uint16_t minorImageVersion;
    uint16_t majorSubsystemVersion;
    uint16_t minorSubsystemVersion;
    uint32_t win32VersionValue;
    uint32_t sizeOfImage;
    uint32_t sizeOfHeaders;
    uint32_t checkSum;
    uint16_t subsystem;
    uint16_t dllCharacteristics;
    uint64_t sizeOfStackReserve;
    uint64_t sizeOfStackCommit;
    uint64_t sizeOfHeapReserve;
    uint64_t sizeOfHeapCommit;
    uint32_t loaderFlags;
    uint32_t numberOfRvaAndSizes;
    ImageDataDirectory dataDirectory[16];
};

struct ImageNtHeaders64 {
    uint32_t signature;
    ImageFileHeader fileHeader;
    ImageOptionalHeader64 optionalHeader;
};

struct ImageOptionalHeader32 {
    uint16_t magic;
    uint8_t  majorLinkerVersion;
    uint8_t  minorLinkerVersion;
    uint32_t sizeOfCode;
    uint32_t sizeOfInitializedData;
    uint32_t sizeOfUninitializedData;
    uint32_t addressOfEntryPoint;
    uint32_t baseOfCode;
    uint32_t baseOfData;
    uint32_t imageBase;
    uint32_t sectionAlignment;
    uint32_t fileAlignment;
    uint16_t majorOperatingSystemVersion;
    uint16_t minorOperatingSystemVersion;
    uint16_t majorImageVersion;
    uint16_t minorImageVersion;
    uint16_t majorSubsystemVersion;
    uint16_t minorSubsystemVersion;
    uint32_t win32VersionValue;
    uint32_t sizeOfImage;
    uint32_t sizeOfHeaders;
    uint32_t checkSum;
    uint16_t subsystem;
    uint16_t dllCharacteristics;
    uint32_t sizeOfStackReserve;
    uint32_t sizeOfStackCommit;
    uint32_t sizeOfHeapReserve;
    uint32_t sizeOfHeapCommit;
    uint32_t loaderFlags;
    uint32_t numberOfRvaAndSizes;
    ImageDataDirectory dataDirectory[16];
};

struct ImageNtHeaders32 {
    uint32_t signature;
    ImageFileHeader fileHeader;
    ImageOptionalHeader32 optionalHeader;
};

struct ImageSectionHeader {
    uint8_t  name[8];
    union {
        uint32_t physicalAddress;
        uint32_t virtualSize;
    } misc;
    uint32_t virtualAddress;
    uint32_t sizeOfRawData;
    uint32_t pointerToRawData;
    uint32_t pointerToRelocations;
    uint32_t pointerToLinenumbers;
    uint16_t numberOfRelocations;
    uint16_t numberOfLinenumbers;
    uint32_t characteristics;

    [[nodiscard]] std::string_view getName() const noexcept {
        size_t len = 0;
        while (len < 8 && name[len] != 0) len++;
        return std::string_view(reinterpret_cast<const char*>(name), len);
    }
};

#pragma pack(pop)

// Section Characteristics
inline constexpr uint32_t IMAGE_SCN_MEM_EXECUTE = 0x20000000;
inline constexpr uint32_t IMAGE_SCN_MEM_READ    = 0x40000000;
inline constexpr uint32_t IMAGE_SCN_MEM_WRITE   = 0x80000000;

/**
 * @brief Clean-room 64-bit PE Image Parser & Loader.
 */
class PeLoader {
public:
    static NtStatus inspect(std::span<const uint8_t> bytes, ImageNtHeaders64& outHeaders, std::vector<ImageSectionHeader>& outSections) {
        if (bytes.size() < sizeof(ImageDosHeader)) {
            return NtStatus::InvalidParameter;
        }

        const auto* dos = reinterpret_cast<const ImageDosHeader*>(bytes.data());
        if (dos->e_magic != DOS_MAGIC) {
            return NtStatus::InvalidParameter;
        }

        if (dos->e_lfanew <= 0 || static_cast<size_t>(dos->e_lfanew) + sizeof(ImageNtHeaders64) > bytes.size()) {
            return NtStatus::InvalidParameter;
        }

        const auto* nt = reinterpret_cast<const ImageNtHeaders64*>(bytes.data() + dos->e_lfanew);
        if (nt->signature != NT_SIGNATURE || nt->fileHeader.machine != MACHINE_AMD64 || nt->optionalHeader.magic != PE32PLUS_MAGIC) {
            return NtStatus::InvalidParameter;
        }

        outHeaders = *nt;

        const auto* section = reinterpret_cast<const ImageSectionHeader*>(
            bytes.data() + dos->e_lfanew + sizeof(uint32_t) + sizeof(ImageFileHeader) + nt->fileHeader.sizeOfOptionalHeader
        );

        outSections.clear();
        for (uint16_t i = 0; i < nt->fileHeader.numberOfSections; ++i) {
            outSections.push_back(section[i]);
        }

        return NtStatus::Success;
    }

    static NtStatus inspect32(std::span<const uint8_t> bytes, ImageNtHeaders32& outHeaders, std::vector<ImageSectionHeader>& outSections) {
        if (bytes.size() < sizeof(ImageDosHeader)) {
            return NtStatus::InvalidParameter;
        }

        const auto* dos = reinterpret_cast<const ImageDosHeader*>(bytes.data());
        if (dos->e_magic != DOS_MAGIC) {
            return NtStatus::InvalidParameter;
        }

        if (dos->e_lfanew <= 0 || static_cast<size_t>(dos->e_lfanew) + sizeof(ImageNtHeaders32) > bytes.size()) {
            return NtStatus::InvalidParameter;
        }

        const auto* nt = reinterpret_cast<const ImageNtHeaders32*>(bytes.data() + dos->e_lfanew);
        if (nt->signature != NT_SIGNATURE || nt->fileHeader.machine != MACHINE_I386 || nt->optionalHeader.magic != PE32_MAGIC) {
            return NtStatus::InvalidParameter;
        }

        outHeaders = *nt;

        const auto* section = reinterpret_cast<const ImageSectionHeader*>(
            bytes.data() + dos->e_lfanew + sizeof(uint32_t) + sizeof(ImageFileHeader) + nt->fileHeader.sizeOfOptionalHeader
        );

        outSections.clear();
        for (uint16_t i = 0; i < nt->fileHeader.numberOfSections; ++i) {
            outSections.push_back(section[i]);
        }

        return NtStatus::Success;
    }
};

} // namespace micant::pe
