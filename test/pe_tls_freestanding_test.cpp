#include "micant/um/native/pe.hpp"

using namespace micant;
using namespace micant::pe;

namespace {
template <typename T>
void write(uint8_t* bytes, size_t offset, const T& value) {
    const auto* source = reinterpret_cast<const uint8_t*>(&value);
    for (size_t i = 0; i < sizeof(value); ++i) bytes[offset + i] = source[i];
}

#define CHECK(expression) do { if (!(expression)) return __LINE__; } while (false)
}

extern "C" int pe_tls_freestanding_test() {
    constexpr uint64_t base = 0x140000000;
    static std::array<uint8_t, 0x400> file{};
    static std::array<uint8_t, 0x2000> image{};
    alignas(16) std::array<uint8_t, 32> first{};
    alignas(16) std::array<uint8_t, 32> second{};
    for (auto& byte : file) byte = 0;
    for (auto& byte : image) byte = 0;

    ImageDosHeader dos{};
    dos.e_magic = DOS_MAGIC;
    dos.e_lfanew = 0x80;
    write(file.data(), 0, dos);
    ImageNtHeaders64 nt{};
    nt.signature = NT_SIGNATURE;
    nt.fileHeader.machine = MACHINE_AMD64;
    nt.fileHeader.numberOfSections = 1;
    nt.fileHeader.sizeOfOptionalHeader = sizeof(ImageOptionalHeader64);
    nt.optionalHeader.magic = PE32PLUS_MAGIC;
    nt.optionalHeader.imageBase = base;
    nt.optionalHeader.sectionAlignment = 4096;
    nt.optionalHeader.fileAlignment = 512;
    nt.optionalHeader.sizeOfHeaders = 512;
    nt.optionalHeader.sizeOfImage = 0x2000;
    nt.optionalHeader.numberOfRvaAndSizes = 16;
    nt.optionalHeader.dataDirectory[IMAGE_DIRECTORY_ENTRY_TLS] = {0x1000, sizeof(ImageTlsDirectory64)};
    write(file.data(), 0x80, nt);
    ImageSectionHeader section{};
    section.virtualAddress = 0x1000;
    section.misc.virtualSize = 512;
    section.pointerToRawData = 512;
    section.sizeOfRawData = 512;
    section.characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
    write(file.data(), 0x80 + sizeof(nt), section);
    ImageTlsDirectory64 tls{};
    tls.startAddressOfRawData = base + 0x1080;
    tls.endAddressOfRawData = base + 0x1084;
    tls.addressOfIndex = base + 0x1090;
    tls.sizeOfZeroFill = 5;
    tls.characteristics = 5u << 20;
    write(file.data(), 0x200, tls);
    write(file.data(), 0x280, uint32_t{0x12345678});

    PeImageView64 view;
    CHECK(view.inspect(file) == NtStatus::Success);
    CHECK(view.map(image, base) == NtStatus::Success);
    PeImageView64::TlsLayout layout{};
    CHECK(view.tlsLayout(image, base, layout) == NtStatus::Success);
    CHECK(layout.present && layout.alignment == 16 && layout.indexRva == 0x1090);
    for (auto& byte : first) byte = 0xA5;
    for (auto& byte : second) byte = 0xA5;
    CHECK(PeImageView64::initializeTlsData(layout, first) == NtStatus::Success);
    CHECK(PeImageView64::initializeTlsData(layout, second) == NtStatus::Success);
    CHECK(first[0] == 0x78 && first[1] == 0x56 && first[2] == 0x34 && first[3] == 0x12);
    for (size_t i = 4; i < 9; ++i) CHECK(first[i] == 0 && second[i] == 0);
    CHECK(first[9] == 0xA5 && second[9] == 0xA5);
    first[0] = 0;
    CHECK(second[0] == 0x78 && image[0x1080] == 0x78);
    CHECK(PeImageView64::initializeTlsData(layout, std::span(second).first(8)) == NtStatus::BufferTooSmall);
    CHECK(PeImageView64::initializeTlsData(layout, std::span(second).subspan(1)) == NtStatus::DatatypeMisalignment);
    CHECK(second[0] == 0x78 && second[9] == 0xA5);

    auto alias = layout;
    alias.alignment = 1;
    CHECK(PeImageView64::initializeTlsData(alias, std::span(image).subspan(0x1080, 9)) == NtStatus::InvalidParameter);
    CHECK(PeImageView64::initializeTlsData(alias, std::span(image).subspan(0x107F, 9)) == NtStatus::InvalidParameter);
    CHECK(PeImageView64::initializeTlsData(alias, std::span(image).subspan(0x1081, 9)) == NtStatus::InvalidParameter);
    CHECK(image[0x1080] == 0x78);
    alias.present = false;
    CHECK(PeImageView64::initializeTlsData(alias, second) == NtStatus::InvalidParameter);
    alias.present = true;
    alias.alignment = 3;
    CHECK(PeImageView64::initializeTlsData(alias, second) == NtStatus::InvalidParameter);
    alias.alignment = 1;
    alias.initialData = {};
    for (auto& byte : second) byte = 0xA5;
    alias.zeroFill = UINT32_MAX;
    CHECK(PeImageView64::initializeTlsData(alias, second) == NtStatus::BufferTooSmall);
    CHECK(second[0] == 0xA5);
    alias.zeroFill = 5;
    CHECK(PeImageView64::initializeTlsData(alias, second) == NtStatus::Success);
    for (size_t i = 0; i < 5; ++i) CHECK(second[i] == 0);
    CHECK(second[5] == 0xA5 && second[9] == 0xA5);
    alias.zeroFill = 0;
    CHECK(PeImageView64::initializeTlsData(alias, {}) == NtStatus::Success);
    return 0;
}

#if !defined(MICANT_FREESTANDING)
int main() {
    return pe_tls_freestanding_test();
}
#endif
