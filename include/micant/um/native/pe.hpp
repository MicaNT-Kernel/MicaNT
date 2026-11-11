#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <span>
#include <vector>
#include <optional>
#include <array>
#include "micant/shared/ntstatus.hpp"

namespace micant::pe {

inline constexpr uint16_t DOS_MAGIC = 0x5A4D; // 'MZ'
inline constexpr uint32_t NT_SIGNATURE = 0x00004550; // 'PE\0\0'
inline constexpr uint16_t MACHINE_AMD64 = 0x8664;
inline constexpr uint16_t MACHINE_I386  = 0x014C;
inline constexpr uint16_t PE32PLUS_MAGIC = 0x020B; // 64-bit Optional Header
inline constexpr uint16_t PE32_MAGIC     = 0x010B; // 32-bit Optional Header

// Standard PE Signatures & File Header Characteristics
inline constexpr uint16_t IMAGE_DOS_SIGNATURE           = DOS_MAGIC;
inline constexpr uint32_t IMAGE_NT_SIGNATURE            = NT_SIGNATURE;
inline constexpr uint16_t IMAGE_FILE_MACHINE_AMD64      = MACHINE_AMD64;
inline constexpr uint16_t IMAGE_FILE_MACHINE_I386       = MACHINE_I386;
inline constexpr uint16_t IMAGE_NT_OPTIONAL_HDR64_MAGIC = PE32PLUS_MAGIC;
inline constexpr uint16_t IMAGE_NT_OPTIONAL_HDR32_MAGIC = PE32_MAGIC;
inline constexpr uint16_t IMAGE_FILE_EXECUTABLE_IMAGE   = 0x0002;
inline constexpr uint16_t IMAGE_FILE_LARGE_ADDRESS_AWARE = 0x0020;
inline constexpr uint16_t IMAGE_SUBSYSTEM_WINDOWS_GUI   = 2;
inline constexpr uint16_t IMAGE_SUBSYSTEM_WINDOWS_CUI   = 3;
inline constexpr uint32_t IMAGE_NUMBEROF_DIRECTORY_ENTRIES = 16;

// Standard Data Directory Indices
inline constexpr uint32_t IMAGE_DIRECTORY_ENTRY_EXPORT    = 0;
inline constexpr uint32_t IMAGE_DIRECTORY_ENTRY_IMPORT    = 1;
inline constexpr uint32_t IMAGE_DIRECTORY_ENTRY_RESOURCE  = 2;
inline constexpr uint32_t IMAGE_DIRECTORY_ENTRY_EXCEPTION = 3;
inline constexpr uint32_t IMAGE_DIRECTORY_ENTRY_SECURITY  = 4;
inline constexpr uint32_t IMAGE_DIRECTORY_ENTRY_BASERELOC = 5;
inline constexpr uint32_t IMAGE_DIRECTORY_ENTRY_DEBUG     = 6;
#ifndef IMAGE_DIRECTORY_ENTRY_TLS
inline constexpr uint32_t IMAGE_DIRECTORY_ENTRY_TLS       = 9;
#endif
inline constexpr uint32_t IMAGE_DIRECTORY_ENTRY_IAT       = 12;

// Import Ordinal Flags
inline constexpr uint64_t IMAGE_ORDINAL_FLAG64 = 0x8000000000000000ULL;
inline constexpr uint32_t IMAGE_ORDINAL_FLAG32 = 0x80000000U;

// Base Relocation Types
inline constexpr uint16_t IMAGE_REL_BASED_ABSOLUTE = 0;
inline constexpr uint16_t IMAGE_REL_BASED_HIGH     = 1;
inline constexpr uint16_t IMAGE_REL_BASED_LOW      = 2;
inline constexpr uint16_t IMAGE_REL_BASED_HIGHLOW  = 3;
inline constexpr uint16_t IMAGE_REL_BASED_HIGHADJ  = 4;
inline constexpr uint16_t IMAGE_REL_BASED_DIR64    = 10;

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

struct ImageImportDescriptor {
    union {
        uint32_t characteristics;
        uint32_t originalFirstThunk; // RVA to Import Lookup Table (INT)
    };
    uint32_t timeDateStamp;
    uint32_t forwarderChain;
    uint32_t name; // RVA to null-terminated DLL name
    uint32_t firstThunk; // RVA to Import Address Table (IAT)
};

struct ImageImportByName {
    uint16_t hint;
    char name[1]; // Null-terminated ASCII symbol name
};

struct ImageBaseRelocation {
    uint32_t virtualAddress;
    uint32_t sizeOfBlock;
};

struct ImageTlsDirectory64 {
    uint64_t startAddressOfRawData;
    uint64_t endAddressOfRawData;
    uint64_t addressOfIndex;
    uint64_t addressOfCallBacks;
    uint32_t sizeOfZeroFill;
    uint32_t characteristics;
};

struct ImageExportDirectory {
    uint32_t characteristics;
    uint32_t timeDateStamp;
    uint16_t majorVersion;
    uint16_t minorVersion;
    uint32_t name;
    uint32_t base;
    uint32_t numberOfFunctions;
    uint32_t numberOfNames;
    uint32_t addressOfFunctions;
    uint32_t addressOfNames;
    uint32_t addressOfNameOrdinals;
};

#pragma pack(pop)

struct ImportedSymbol {
    std::string name;
    uint16_t ordinal{0};
    bool isOrdinal{false};
    uint32_t iatRva{0}; // RVA in loaded image where function pointer is written
};

struct ImportedLibrary {
    std::string libraryName;
    std::vector<ImportedSymbol> symbols;
};

// Section Characteristics
inline constexpr uint32_t IMAGE_SCN_MEM_EXECUTE   = 0x20000000;
inline constexpr uint32_t IMAGE_SCN_MEM_READ      = 0x40000000;
inline constexpr uint32_t IMAGE_SCN_MEM_WRITE     = 0x80000000;
inline constexpr uint32_t IMAGE_SCN_MEM_DISCARDABLE = 0x02000000;

// Allocation-free PE32+ view used before a process runtime or heap exists.
class PeImageView64 {
public:
    struct ExportTarget {
        uint64_t address{};
        std::string_view forwarder{};
    };
    struct TlsLayout {
        std::span<const uint8_t> initialData{};
        uint32_t zeroFill{};
        uint32_t indexRva{};
        size_t alignment{1};
        std::array<uint64_t, 64> callbacks{};
        size_t callbackCount{};
        bool present{};
    };
    NtStatus inspect(std::span<const uint8_t> file) noexcept {
        *this = {};
        ImageDosHeader dos{};
        if (!read(file, 0, dos) || dos.e_magic != DOS_MAGIC || dos.e_lfanew < 0) {
            return NtStatus::InvalidImageFormat;
        }
        const size_t ntOffset = static_cast<uint32_t>(dos.e_lfanew);
        uint32_t signature{};
        ImageFileHeader coff{};
        if (!read(file, ntOffset, signature) || signature != NT_SIGNATURE ||
            !read(file, ntOffset + 4, coff) || coff.machine != MACHINE_AMD64 ||
            !coff.numberOfSections || coff.numberOfSections > 96 ||
            coff.sizeOfOptionalHeader < 112) {
            return NtStatus::InvalidImageFormat;
        }
        const size_t optionalOffset = ntOffset + 4 + sizeof(coff);
        if (!fits(file.size(), optionalOffset, coff.sizeOfOptionalHeader)) {
            return NtStatus::InvalidImageFormat;
        }
        headers_.signature = signature;
        headers_.fileHeader = coff;
        const size_t optionalBytes = coff.sizeOfOptionalHeader < sizeof(headers_.optionalHeader)
            ? coff.sizeOfOptionalHeader : sizeof(headers_.optionalHeader);
        copy(&headers_.optionalHeader, file.data() + optionalOffset, optionalBytes);
        const auto& opt = headers_.optionalHeader;
        if (opt.magic != PE32PLUS_MAGIC || opt.numberOfRvaAndSizes > 16 ||
            112 + opt.numberOfRvaAndSizes * 8 > coff.sizeOfOptionalHeader ||
            !powerOfTwo(opt.fileAlignment) || opt.fileAlignment < 512 ||
            opt.fileAlignment > 65536 || !powerOfTwo(opt.sectionAlignment) ||
            opt.sectionAlignment < opt.fileAlignment || !opt.sizeOfImage ||
            opt.sizeOfImage % opt.sectionAlignment ||
            !opt.sizeOfHeaders || opt.sizeOfHeaders > file.size() ||
            opt.sizeOfHeaders > opt.sizeOfImage ||
            opt.sizeOfHeaders % opt.fileAlignment) {
            return NtStatus::InvalidImageFormat;
        }
        if (opt.sectionAlignment < 4096) {
            return NtStatus::NotSupported;
        }
        sectionsOffset_ = optionalOffset + coff.sizeOfOptionalHeader;
        if (!fits(opt.sizeOfHeaders, sectionsOffset_,
                  static_cast<size_t>(coff.numberOfSections) * sizeof(ImageSectionHeader))) {
            return NtStatus::InvalidImageFormat;
        }
        file_ = file;
        for (size_t index = 0; index < coff.numberOfSections; ++index) {
            const auto current = section(index);
            const size_t size = sectionSize(current);
            if (current.virtualAddress < opt.sizeOfHeaders ||
                current.virtualAddress % opt.sectionAlignment ||
                !fits(opt.sizeOfImage, current.virtualAddress, size) ||
                (current.sizeOfRawData &&
                    (current.pointerToRawData < opt.sizeOfHeaders ||
                     current.pointerToRawData % opt.fileAlignment ||
                     !fits(file.size(), current.pointerToRawData, current.sizeOfRawData)))) {
                file_ = {};
                return NtStatus::InvalidImageFormat;
            }
            for (size_t previous = 0; previous < index; ++previous) {
                const auto other = section(previous);
                if (size && sectionSize(other) &&
                    current.virtualAddress < other.virtualAddress + sectionSize(other) &&
                    other.virtualAddress < current.virtualAddress + size) {
                    file_ = {};
                    return NtStatus::InvalidImageFormat;
                }
            }
        }
        if (opt.addressOfEntryPoint && !executableRva(opt.addressOfEntryPoint)) {
            file_ = {};
            return NtStatus::InvalidImageFormat;
        }
        for (uint32_t index = 0; index < opt.numberOfRvaAndSizes; ++index) {
            const auto dir = opt.dataDirectory[index];
            if (!dir.virtualAddress && !dir.size) continue;
            if (!dir.virtualAddress || !dir.size ||
                (index == IMAGE_DIRECTORY_ENTRY_SECURITY
                    ? !fits(file.size(), dir.virtualAddress, dir.size)
                    : !mappedRange(dir.virtualAddress, dir.size))) {
                file_ = {};
                return NtStatus::InvalidImageFormat;
            }
        }
        return NtStatus::Success;
    }

    [[nodiscard]] const ImageNtHeaders64& headers() const noexcept { return headers_; }
    [[nodiscard]] ImageSectionHeader section(size_t index) const noexcept {
        ImageSectionHeader result{};
        if (index < headers_.fileHeader.numberOfSections) {
            read(file_, sectionsOffset_ + index * sizeof(result), result);
        }
        return result;
    }
    [[nodiscard]] bool executableRva(uint32_t rva) const noexcept {
        for (size_t index = 0; index < headers_.fileHeader.numberOfSections; ++index) {
            const auto sec = section(index);
            if ((sec.characteristics & IMAGE_SCN_MEM_EXECUTE) &&
                rva >= sec.virtualAddress && rva - sec.virtualAddress < sectionSize(sec)) {
                return true;
            }
        }
        return false;
    }
    [[nodiscard]] bool mappedRange(uint32_t rva, size_t count) const noexcept {
        return count && count <= mappedRemaining(rva);
    }

    NtStatus map(std::span<uint8_t> destination, uint64_t actualBase) const noexcept {
        if (file_.empty() || destination.size() < headers_.optionalHeader.sizeOfImage) {
            return NtStatus::BufferTooSmall;
        }
        if (actualBase > UINT64_MAX - headers_.optionalHeader.sizeOfImage) {
            return NtStatus::InvalidParameter;
        }
        const uintptr_t source = reinterpret_cast<uintptr_t>(file_.data());
        const uintptr_t target = reinterpret_cast<uintptr_t>(destination.data());
        if (target >= source ? target - source < file_.size()
                             : source - target < headers_.optionalHeader.sizeOfImage) {
            return NtStatus::InvalidParameter;
        }
        destination = destination.first(headers_.optionalHeader.sizeOfImage);
        for (auto& byte : destination) byte = 0;
        copy(destination.data(), file_.data(), headers_.optionalHeader.sizeOfHeaders);
        for (size_t index = 0; index < headers_.fileHeader.numberOfSections; ++index) {
            const auto sec = section(index);
            if (sec.sizeOfRawData) {
                copy(destination.data() + sec.virtualAddress,
                     file_.data() + sec.pointerToRawData, sec.sizeOfRawData);
            }
        }
        const uint64_t delta = actualBase - headers_.optionalHeader.imageBase;
        if (!delta) return NtStatus::Success;
        const auto dir = directory(IMAGE_DIRECTORY_ENTRY_BASERELOC);
        if (!dir.size || (headers_.fileHeader.characteristics & 1)) {
            return NtStatus::Conflict;
        }
        size_t cursor = dir.virtualAddress;
        const size_t end = cursor + dir.size;
        while (cursor < end) {
            ImageBaseRelocation block{};
            if (!fits(end, cursor, sizeof(block)) || !read(destination, cursor, block) ||
                block.sizeOfBlock < sizeof(block) ||
                (block.sizeOfBlock - sizeof(block)) % 2 ||
                !fits(end, cursor, block.sizeOfBlock) || block.virtualAddress % 4096) {
                return NtStatus::InvalidImageFormat;
            }
            for (size_t offset = sizeof(block); offset < block.sizeOfBlock; offset += 2) {
                uint16_t relocation{};
                read(destination, cursor + offset, relocation);
                const uint16_t type = relocation >> 12;
                if (type == IMAGE_REL_BASED_ABSOLUTE) continue;
                if (type != IMAGE_REL_BASED_DIR64 ||
                    block.virtualAddress > UINT32_MAX - (relocation & 4095)) {
                    return NtStatus::InvalidImageFormat;
                }
                const uint32_t rva = block.virtualAddress + (relocation & 4095);
                uint64_t value{};
                if (!mappedRange(rva, sizeof(value)) || !read(destination, rva, value)) {
                    return NtStatus::InvalidImageFormat;
                }
                value += delta;
                copy(destination.data() + rva, &value, sizeof(value));
            }
            cursor += block.sizeOfBlock;
        }
        return NtStatus::Success;
    }

    template <typename Resolver>
    NtStatus bindImports(std::span<uint8_t> image, Resolver&& resolve,
                         bool writeBindings = true) const {
        if (file_.empty() || image.size() < headers_.optionalHeader.sizeOfImage) {
            return NtStatus::InvalidParameter;
        }
        const auto dir = directory(IMAGE_DIRECTORY_ENTRY_IMPORT);
        if (!dir.size) return NtStatus::Success;
        size_t cursor = dir.virtualAddress;
        const size_t end = cursor + dir.size;
        while (fits(end, cursor, sizeof(ImageImportDescriptor))) {
            ImageImportDescriptor descriptor{};
            read(image, cursor, descriptor);
            if (!descriptor.originalFirstThunk && !descriptor.timeDateStamp &&
                !descriptor.forwarderChain && !descriptor.name && !descriptor.firstThunk) {
                return NtStatus::Success;
            }
            std::string_view library;
            if (!descriptor.firstThunk || !stringAt(image, descriptor.name, library) ||
                library.empty()) return NtStatus::InvalidImageFormat;
            size_t lookup = descriptor.originalFirstThunk
                ? descriptor.originalFirstThunk : descriptor.firstThunk;
            size_t slot = descriptor.firstThunk;
            for (;;) {
                uint64_t entry{};
                if (lookup > UINT32_MAX || slot > UINT32_MAX ||
                    !mappedRange(static_cast<uint32_t>(lookup), sizeof(entry)) ||
                    !mappedRange(static_cast<uint32_t>(slot), sizeof(entry)) ||
                    !read(image, lookup, entry)) {
                    return NtStatus::InvalidImageFormat;
                }
                if (!entry) break;
                uint16_t ordinal{};
                std::string_view name;
                if (entry & IMAGE_ORDINAL_FLAG64) {
                    if (entry & ~(IMAGE_ORDINAL_FLAG64 | uint64_t{0xFFFF})) {
                        return NtStatus::InvalidImageFormat;
                    }
                    ordinal = static_cast<uint16_t>(entry);
                } else {
                    if (entry > UINT32_MAX - 2 || !mappedRange(static_cast<uint32_t>(entry), 3) ||
                        !stringAt(image, static_cast<uint32_t>(entry) + 2, name) || name.empty()) {
                        return NtStatus::InvalidImageFormat;
                    }
                }
                uint64_t address{};
                const NtStatus status = resolve(library, name, ordinal, address);
                if (status != NtStatus::Success) return status;
                if (!address) return NtStatus::ProcedureNotFound;
                if (writeBindings) copy(image.data() + slot, &address, sizeof(address));
                lookup += sizeof(entry);
                slot += sizeof(entry);
            }
            cursor += sizeof(descriptor);
        }
        return NtStatus::InvalidImageFormat;
    }

    NtStatus findExport(std::span<const uint8_t> image, uint64_t actualBase,
                        std::string_view name, uint16_t ordinal,
                        ExportTarget& result) const noexcept {
        result = {};
        if (file_.empty() || image.size() < headers_.optionalHeader.sizeOfImage ||
            actualBase > UINT64_MAX - headers_.optionalHeader.sizeOfImage) {
            return NtStatus::InvalidParameter;
        }
        const auto dir = directory(IMAGE_DIRECTORY_ENTRY_EXPORT);
        if (!dir.size) return NtStatus::ProcedureNotFound;
        ImageExportDirectory exports{};
        if (dir.size < sizeof(exports) || !read(image, dir.virtualAddress, exports) ||
            !exports.numberOfFunctions ||
            !mappedRange(exports.addressOfFunctions,
                         static_cast<size_t>(exports.numberOfFunctions) * 4)) {
            return NtStatus::InvalidImageFormat;
        }
        uint32_t index{};
        if (!name.empty()) {
            if (!exports.numberOfNames) return NtStatus::ProcedureNotFound;
            if (!mappedRange(exports.addressOfNames, static_cast<size_t>(exports.numberOfNames) * 4) ||
                !mappedRange(exports.addressOfNameOrdinals, static_cast<size_t>(exports.numberOfNames) * 2)) {
                return NtStatus::InvalidImageFormat;
            }
            bool found{};
            for (size_t entry = 0; entry < exports.numberOfNames; ++entry) {
                uint32_t rva{};
                uint16_t namedIndex{};
                read(image, exports.addressOfNames + entry * 4, rva);
                read(image, exports.addressOfNameOrdinals + entry * 2, namedIndex);
                std::string_view candidate;
                if (namedIndex >= exports.numberOfFunctions || !stringAt(image, rva, candidate)) {
                    return NtStatus::InvalidImageFormat;
                }
                if (candidate == name) {
                    index = namedIndex;
                    found = true;
                    break;
                }
            }
            if (!found) return NtStatus::ProcedureNotFound;
        } else {
            if (ordinal < exports.base || ordinal - exports.base >= exports.numberOfFunctions) {
                return NtStatus::ProcedureNotFound;
            }
            index = ordinal - exports.base;
        }
        uint32_t rva{};
        read(image, exports.addressOfFunctions + static_cast<size_t>(index) * 4, rva);
        if (!rva) return NtStatus::ProcedureNotFound;
        if (!mappedRange(rva, 1)) return NtStatus::InvalidImageFormat;
        if (rva >= dir.virtualAddress && rva - dir.virtualAddress < dir.size) {
            if (!stringAt(image, rva, result.forwarder) || result.forwarder.empty() ||
                result.forwarder.size() + 1 > dir.size - (rva - dir.virtualAddress)) {
                result = {};
                return NtStatus::InvalidImageFormat;
            }
        } else {
            result.address = actualBase + rva;
        }
        return NtStatus::Success;
    }

    NtStatus tlsLayout(std::span<const uint8_t> image, uint64_t actualBase,
                       TlsLayout& result) const noexcept {
        result = {};
        if (file_.empty() || image.size() < headers_.optionalHeader.sizeOfImage ||
            actualBase > UINT64_MAX - headers_.optionalHeader.sizeOfImage) {
            return NtStatus::InvalidParameter;
        }
        const auto dir = directory(IMAGE_DIRECTORY_ENTRY_TLS);
        if (!dir.size) return NtStatus::Success;
        ImageTlsDirectory64 tls{};
        if (dir.size < sizeof(tls) || !read(image, dir.virtualAddress, tls) ||
            tls.endAddressOfRawData < tls.startAddressOfRawData) {
            return NtStatus::InvalidImageFormat;
        }
        const auto relative = [&](uint64_t va, size_t count, uint32_t& rva) {
            if (va < actualBase || va - actualBase > UINT32_MAX) return false;
            rva = static_cast<uint32_t>(va - actualBase);
            return mappedRange(rva, count);
        };
        const uint64_t size = tls.endAddressOfRawData - tls.startAddressOfRawData;
        uint32_t rva{};
        if (size) {
            if (size > image.size() || !relative(tls.startAddressOfRawData, size, rva)) {
                return NtStatus::InvalidImageFormat;
            }
            result.initialData = image.subspan(rva, static_cast<size_t>(size));
        }
        if (!relative(tls.addressOfIndex, sizeof(uint32_t), result.indexRva)) {
            result = {};
            return NtStatus::InvalidImageFormat;
        }
        const uint32_t alignment = (tls.characteristics >> 20) & 15;
        if (alignment == 15) return NtStatus::InvalidImageFormat;
        result.alignment = alignment ? size_t{1} << (alignment - 1) : 1;
        result.zeroFill = tls.sizeOfZeroFill;
        if (tls.addressOfCallBacks) {
            if (!relative(tls.addressOfCallBacks, 8, rva)) return NtStatus::InvalidImageFormat;
            for (;;) {
                uint64_t callback{};
                if (!mappedRange(rva, 8) || !read(image, rva, callback)) {
                    return NtStatus::InvalidImageFormat;
                }
                if (!callback) break;
                uint32_t callbackRva{};
                if (!relative(callback, 1, callbackRva) || !executableRva(callbackRva)) {
                    return NtStatus::InvalidImageFormat;
                }
                if (result.callbackCount == result.callbacks.size()) {
                    return NtStatus::NotSupported;
                }
                result.callbacks[result.callbackCount++] = callback;
                rva += 8;
            }
        }
        result.present = true;
        return NtStatus::Success;
    }

private:
    static bool powerOfTwo(uint32_t value) noexcept { return value && !(value & (value - 1)); }
    static bool fits(size_t size, size_t start, size_t count) noexcept {
        return start <= size && count <= size - start;
    }
    static size_t sectionSize(const ImageSectionHeader& sec) noexcept {
        return sec.misc.virtualSize > sec.sizeOfRawData
            ? sec.misc.virtualSize : sec.sizeOfRawData;
    }
    static void copy(void* to, const void* from, size_t count) noexcept {
        auto* destination = static_cast<uint8_t*>(to);
        const auto* source = static_cast<const uint8_t*>(from);
        for (size_t byte = 0; byte < count; ++byte) destination[byte] = source[byte];
    }
    template <typename T>
    static bool read(std::span<const uint8_t> bytes, size_t offset, T& result) noexcept {
        if (!fits(bytes.size(), offset, sizeof(T))) return false;
        copy(&result, bytes.data() + offset, sizeof(T));
        return true;
    }
    [[nodiscard]] ImageDataDirectory directory(uint32_t index) const noexcept {
        return index < headers_.optionalHeader.numberOfRvaAndSizes
            ? headers_.optionalHeader.dataDirectory[index] : ImageDataDirectory{};
    }
    [[nodiscard]] size_t mappedRemaining(uint32_t rva) const noexcept {
        if (file_.empty()) return 0;
        if (rva < headers_.optionalHeader.sizeOfHeaders) {
            return headers_.optionalHeader.sizeOfHeaders - rva;
        }
        for (size_t index = 0; index < headers_.fileHeader.numberOfSections; ++index) {
            const auto sec = section(index);
            if (rva >= sec.virtualAddress && rva - sec.virtualAddress < sectionSize(sec)) {
                return sectionSize(sec) - (rva - sec.virtualAddress);
            }
        }
        return 0;
    }
    bool stringAt(std::span<const uint8_t> image, uint32_t rva,
                  std::string_view& text) const noexcept {
        const size_t remaining = mappedRemaining(rva);
        if (!remaining || !fits(image.size(), rva, remaining)) return false;
        for (size_t size = 0; size < remaining; ++size) {
            if (!image[rva + size]) {
                text = {reinterpret_cast<const char*>(image.data() + rva), size};
                return true;
            }
        }
        return false;
    }
    std::span<const uint8_t> file_{};
    ImageNtHeaders64 headers_{};
    size_t sectionsOffset_{};
};

/**
 * @brief Clean-room 64-bit PE Image Parser & Loader.
 */
class PeLoader {
public:
    static NtStatus inspect(std::span<const uint8_t> bytes, ImageNtHeaders64& outHeaders, std::vector<ImageSectionHeader>& outSections) {
        PeImageView64 view;
        const NtStatus status = view.inspect(bytes);
        if (status != NtStatus::Success) return status;
        outHeaders = view.headers();
        outSections.clear();
        for (uint16_t i = 0; i < outHeaders.fileHeader.numberOfSections; ++i) {
            outSections.push_back(view.section(i));
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

    [[nodiscard]] static std::optional<size_t> rvaToOffset(
        uint32_t rva,
        const std::vector<ImageSectionHeader>& sections
    ) noexcept {
        for (const auto& sec : sections) {
            uint32_t secSize = sec.misc.virtualSize ? sec.misc.virtualSize : sec.sizeOfRawData;
            if (rva >= sec.virtualAddress && rva < sec.virtualAddress + secSize) {
                return sec.pointerToRawData + (rva - sec.virtualAddress);
            }
        }
        return std::nullopt;
    }

    static NtStatus mapImage(
        std::span<const uint8_t> fileBytes,
        const ImageNtHeaders64& headers,
        const std::vector<ImageSectionHeader>& sections,
        uint8_t* destinationBuffer,
        size_t destinationSize
    ) {
        if (!destinationBuffer || destinationSize < headers.optionalHeader.sizeOfImage) {
            return NtStatus::InvalidParameter;
        }

        std::memset(destinationBuffer, 0, destinationSize);

        // Copy Headers
        if (headers.optionalHeader.sizeOfHeaders > fileBytes.size() ||
            headers.optionalHeader.sizeOfHeaders > destinationSize) {
            return NtStatus::InvalidParameter;
        }
        std::memcpy(destinationBuffer, fileBytes.data(), headers.optionalHeader.sizeOfHeaders);

        // Copy Sections
        for (const auto& sec : sections) {
            if (sec.virtualAddress + sec.sizeOfRawData > destinationSize) {
                return NtStatus::InvalidParameter;
            }
            if (sec.pointerToRawData + sec.sizeOfRawData > fileBytes.size()) {
                return NtStatus::InvalidParameter;
            }
            if (sec.sizeOfRawData > 0) {
                std::memcpy(destinationBuffer + sec.virtualAddress,
                            fileBytes.data() + sec.pointerToRawData,
                            sec.sizeOfRawData);
            }
        }

        return NtStatus::Success;
    }

    static NtStatus parseImports(
        std::span<const uint8_t> bytes,
        const ImageNtHeaders64& headers,
        const std::vector<ImageSectionHeader>& sections,
        std::vector<ImportedLibrary>& outImports
    ) {
        outImports.clear();
        if (headers.optionalHeader.numberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IMPORT) {
            return NtStatus::Success;
        }

        const auto& dir = headers.optionalHeader.dataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (dir.virtualAddress == 0 || dir.size == 0) {
            return NtStatus::Success; // No imports
        }

        auto offsetOpt = rvaToOffset(dir.virtualAddress, sections);
        if (!offsetOpt || *offsetOpt + sizeof(ImageImportDescriptor) > bytes.size()) {
            return NtStatus::InvalidParameter;
        }

        const auto* desc = reinterpret_cast<const ImageImportDescriptor*>(bytes.data() + *offsetOpt);
        while (desc->name != 0 && desc->firstThunk != 0) {
            auto nameOffset = rvaToOffset(desc->name, sections);
            if (!nameOffset || *nameOffset >= bytes.size()) {
                break;
            }

            const char* dllNameStr = reinterpret_cast<const char*>(bytes.data() + *nameOffset);
            ImportedLibrary lib;
            lib.libraryName = dllNameStr;

            // Use originalFirstThunk (INT) if present, else fallback to firstThunk (IAT)
            uint32_t thunkRva = desc->originalFirstThunk ? desc->originalFirstThunk : desc->firstThunk;
            auto thunkOffset = rvaToOffset(thunkRva, sections);
            if (thunkOffset) {
                const auto* thunkArray = reinterpret_cast<const uint64_t*>(bytes.data() + *thunkOffset);
                size_t idx = 0;
                while (thunkArray[idx] != 0) {
                    uint64_t val = thunkArray[idx];
                    ImportedSymbol sym;
                    sym.iatRva = desc->firstThunk + static_cast<uint32_t>(idx * sizeof(uint64_t));

                    if (val & IMAGE_ORDINAL_FLAG64) {
                        sym.isOrdinal = true;
                        sym.ordinal = static_cast<uint16_t>(val & 0xFFFF);
                    } else {
                        auto byNameOffset = rvaToOffset(static_cast<uint32_t>(val & 0xFFFFFFFF), sections);
                        if (byNameOffset && *byNameOffset + 2 < bytes.size()) {
                            const auto* ibn = reinterpret_cast<const ImageImportByName*>(bytes.data() + *byNameOffset);
                            sym.isOrdinal = false;
                            sym.name = ibn->name;
                        }
                    }
                    lib.symbols.push_back(sym);
                    idx++;
                }
            }

            outImports.push_back(lib);
            desc++;
        }

        return NtStatus::Success;
    }

    template <typename ResolverFunc>
    static size_t bindImports(
        uint8_t* loadedImageBase,
        const std::vector<ImportedLibrary>& imports,
        ResolverFunc&& resolver,
        bool is64Bit = true
    ) {
        if (!loadedImageBase) return 0;
        size_t resolvedCount = 0;

        for (const auto& lib : imports) {
            for (const auto& sym : lib.symbols) {
                void* fnPtr = nullptr;
                if (sym.isOrdinal) {
                    fnPtr = resolver(lib.libraryName, "", sym.ordinal);
                } else {
                    fnPtr = resolver(lib.libraryName, sym.name, 0);
                }

                if (fnPtr) {
                    if (is64Bit) {
                        *reinterpret_cast<uint64_t*>(loadedImageBase + sym.iatRva) = reinterpret_cast<uint64_t>(fnPtr);
                    } else {
                        *reinterpret_cast<uint32_t*>(loadedImageBase + sym.iatRva) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(fnPtr));
                    }
                    resolvedCount++;
                }
            }
        }

        return resolvedCount;
    }

    static NtStatus applyRelocations(
        uint8_t* loadedImageBase,
        size_t imageSize,
        const ImageNtHeaders64& headers,
        uintptr_t actualBaseAddress
    ) {
        if (!loadedImageBase) return NtStatus::InvalidParameter;

        int64_t delta = static_cast<int64_t>(actualBaseAddress) - static_cast<int64_t>(headers.optionalHeader.imageBase);
        if (delta == 0) return NtStatus::Success; // Loaded at preferred base, no relocations needed

        if (headers.optionalHeader.numberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_BASERELOC) {
            return NtStatus::Success;
        }

        const auto& dir = headers.optionalHeader.dataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (dir.virtualAddress == 0 || dir.size == 0) {
            return NtStatus::Success;
        }

        if (dir.virtualAddress + dir.size > imageSize) {
            return NtStatus::InvalidParameter;
        }

        uint32_t currentOffset = dir.virtualAddress;
        uint32_t endOffset = dir.virtualAddress + dir.size;

        while (currentOffset < endOffset) {
            const auto* block = reinterpret_cast<const ImageBaseRelocation*>(loadedImageBase + currentOffset);
            if (block->sizeOfBlock < sizeof(ImageBaseRelocation)) break;

            uint32_t entryCount = (block->sizeOfBlock - sizeof(ImageBaseRelocation)) / sizeof(uint16_t);
            const auto* entries = reinterpret_cast<const uint16_t*>(block + 1);

            for (uint32_t i = 0; i < entryCount; ++i) {
                uint16_t type = entries[i] >> 12;
                uint16_t offset = entries[i] & 0x0FFF;
                uint32_t targetRva = block->virtualAddress + offset;

                if (type == IMAGE_REL_BASED_DIR64 && targetRva + sizeof(uint64_t) <= imageSize) {
                    *reinterpret_cast<uint64_t*>(loadedImageBase + targetRva) += delta;
                } else if (type == IMAGE_REL_BASED_HIGHLOW && targetRva + sizeof(uint32_t) <= imageSize) {
                    *reinterpret_cast<uint32_t*>(loadedImageBase + targetRva) += static_cast<uint32_t>(delta);
                }
            }

            currentOffset += block->sizeOfBlock;
        }

        return NtStatus::Success;
    }
};

} // namespace micant::pe
