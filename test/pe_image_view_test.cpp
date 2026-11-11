#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include "micant/um/native/pe.hpp"
#include "micant/km/executive/usermode.hpp"
#include "unmodified_fixture.hpp"

using namespace micant;
using namespace micant::pe;

namespace {
constexpr uint64_t kBase = 0x140000000;
using File = std::array<uint8_t, 0x600>;

template <typename T>
void write(File& file, size_t offset, const T& value) {
    assert(offset <= file.size() && sizeof(value) <= file.size() - offset);
    std::memcpy(file.data() + offset, &value, sizeof(value));
}

ImageNtHeaders64 headers(const File& file) {
    ImageNtHeaders64 result{};
    std::memcpy(&result, file.data() + 0x80, sizeof(result));
    return result;
}

File fixture() {
    File file{};
    ImageDosHeader dos{};
    dos.e_magic = DOS_MAGIC;
    dos.e_lfanew = 0x80;
    write(file, 0, dos);
    ImageNtHeaders64 nt{};
    nt.signature = NT_SIGNATURE;
    nt.fileHeader.machine = MACHINE_AMD64;
    nt.fileHeader.numberOfSections = 2;
    nt.fileHeader.sizeOfOptionalHeader = sizeof(ImageOptionalHeader64);
    nt.optionalHeader.magic = PE32PLUS_MAGIC;
    nt.optionalHeader.imageBase = kBase;
    nt.optionalHeader.addressOfEntryPoint = 0x1000;
    nt.optionalHeader.sectionAlignment = 4096;
    nt.optionalHeader.fileAlignment = 512;
    nt.optionalHeader.sizeOfHeaders = 512;
    nt.optionalHeader.sizeOfImage = 0x3000;
    nt.optionalHeader.numberOfRvaAndSizes = 16;
    nt.optionalHeader.dataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC] = {0x2000, 12};
    nt.optionalHeader.dataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT] = {0x1040, 40};
    write(file, 0x80, nt);
    ImageSectionHeader text{};
    text.virtualAddress = 0x1000;
    text.misc.virtualSize = 512;
    text.sizeOfRawData = 512;
    text.pointerToRawData = 512;
    text.characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
    write(file, 0x80 + sizeof(nt), text);
    text.virtualAddress = 0x2000;
    text.pointerToRawData = 1024;
    text.characteristics = IMAGE_SCN_MEM_READ;
    write(file, 0x80 + sizeof(nt) + sizeof(text), text);
    file[0x200] = 0xC3;
    write(file, 0x210, kBase + 0x1234);
    write(file, 0x400, ImageBaseRelocation{0x1000, 12});
    write(file, 0x408, uint16_t{0xA010});
    ImageImportDescriptor descriptor{};
    descriptor.originalFirstThunk = 0x10B0;
    descriptor.name = 0x10A0;
    descriptor.firstThunk = 0x10D0;
    write(file, 0x240, descriptor);
    std::memcpy(file.data() + 0x2A0, "kernel32.dll", 13);
    write(file, 0x2B0, uint64_t{0x10F0});
    write(file, 0x2B8, IMAGE_ORDINAL_FLAG64 | uint64_t{42});
    std::memcpy(file.data() + 0x2F2, "Example", 8);
    return file;
}

void checkMapping() {
    const auto file = fixture();
    PeImageView64 view;
    assert(view.inspect(file) == NtStatus::Success);
    ImageNtHeaders64 inspected{};
    std::vector<ImageSectionHeader> sections;
    assert(PeLoader::inspect(file, inspected, sections) == NtStatus::Success);
    assert(inspected.optionalHeader.sizeOfImage ==
           view.headers().optionalHeader.sizeOfImage);
    assert(sections.size() == 2 && sections[0].virtualAddress == 0x1000);
    assert(view.executableRva(0x1000) && !view.executableRva(0x2000));
    assert(!view.mappedRange(0x1FFF, 2));
    std::array<uint8_t, 0x3002> mapped{};
    mapped.front() = mapped.back() = 0xA5;
    auto image = std::span(mapped).subspan(1, 0x3000);
    assert(view.map(image, kBase + 0x10000) == NtStatus::Success);
    uint64_t relocated{};
    std::memcpy(&relocated, image.data() + 0x1010, sizeof(relocated));
    assert(relocated == kBase + 0x11234);
    assert(image[0x1000] == 0xC3 && image[0x1800] == 0);
    size_t calls{};
    auto resolve = [&](std::string_view module, std::string_view name,
                       uint16_t ordinal, uint64_t& address) {
        assert(module == "kernel32.dll");
        assert((calls == 0 && name == "Example" && ordinal == 0) ||
               (calls == 1 && name.empty() && ordinal == 42));
        address = 0x400000010000 + calls++ * 16;
        return NtStatus::Success;
    };
    assert(view.bindImports(image, resolve) == NtStatus::Success && calls == 2);
    uint64_t binding{};
    std::memcpy(&binding, image.data() + 0x10D8, sizeof(binding));
    assert(binding == 0x400000010010);
    assert(mapped.front() == 0xA5 && mapped.back() == 0xA5);
}

void checkMalformed() {
    PeImageView64 view;
    std::array<uint8_t, 0x3000> image{};
    auto file = fixture();
    for (size_t size = 0; size < file.size(); ++size) {
        assert(view.inspect(std::span(file).first(size)) != NtStatus::Success);
        ImageNtHeaders64 inspected{};
        std::vector<ImageSectionHeader> sections;
        assert(PeLoader::inspect(std::span(file).first(size), inspected, sections) !=
               NtStatus::Success);
    }
    const auto rejectedHeader = [&](auto mutate) {
        auto candidate = fixture();
        auto nt = headers(candidate);
        mutate(nt);
        write(candidate, 0x80, nt);
        assert(view.inspect(candidate) != NtStatus::Success);
    };
    rejectedHeader([](auto& nt) { nt.fileHeader.numberOfSections = 97; });
    rejectedHeader([](auto& nt) { nt.fileHeader.sizeOfOptionalHeader = 111; });
    rejectedHeader([](auto& nt) { nt.optionalHeader.numberOfRvaAndSizes = 17; });
    rejectedHeader([](auto& nt) { nt.optionalHeader.fileAlignment = 3; });
    rejectedHeader([](auto& nt) { nt.optionalHeader.sizeOfImage = 0x2000; });
    rejectedHeader([](auto& nt) { nt.optionalHeader.sizeOfHeaders = 256; });
    rejectedHeader([](auto& nt) { nt.optionalHeader.addressOfEntryPoint = 0x2000; });
    rejectedHeader([](auto& nt) {
        nt.optionalHeader.dataDirectory[1] = {0x11F0, 64};
    });
    ImageSectionHeader overlapping{};
    std::memcpy(&overlapping, file.data() + 0x80 + sizeof(ImageNtHeaders64), sizeof(overlapping));
    write(file, 0x80 + sizeof(ImageNtHeaders64) + sizeof(overlapping), overlapping);
    assert(view.inspect(file) == NtStatus::InvalidImageFormat);

    file = fixture();
    auto nt = headers(file);
    nt.optionalHeader.dataDirectory[5] = {};
    write(file, 0x80, nt);
    assert(view.inspect(file) == NtStatus::Success);
    assert(view.map(image, kBase + 4096) == NtStatus::Conflict);
    assert(view.map(image, kBase) == NtStatus::Success);
    assert(view.map(std::span(image).first(16), kBase) == NtStatus::BufferTooSmall);
    assert(view.map(image, UINT64_MAX - 100) == NtStatus::InvalidParameter);
    file = fixture();
    write(file, 0x404, uint32_t{0x1000});
    assert(view.inspect(file) == NtStatus::Success);
    assert(view.map(image, kBase + 4096) == NtStatus::InvalidImageFormat);
    file = fixture();
    write(file, 0x408, uint16_t{0x3010});
    assert(view.inspect(file) == NtStatus::Success);
    assert(view.map(image, kBase + 4096) == NtStatus::InvalidImageFormat);
    file = fixture();
    write(file, 0x408, uint16_t{0xAFFC});
    assert(view.inspect(file) == NtStatus::Success);
    assert(view.map(image, kBase + 4096) == NtStatus::InvalidImageFormat);

    file = fixture();
    assert(view.inspect(file) == NtStatus::Success);
    assert(view.map(image, kBase) == NtStatus::Success);
    auto missing = [](auto, auto, auto, uint64_t&) { return NtStatus::ProcedureNotFound; };
    assert(view.bindImports(image, missing) == NtStatus::ProcedureNotFound);
    const auto invalidImport = [&](size_t offset, uint64_t value) {
        assert(view.map(image, kBase) == NtStatus::Success);
        std::memcpy(image.data() + offset, &value, sizeof(value));
        auto resolve = [](auto, auto, auto, uint64_t& address) {
            address = 0x10000;
            return NtStatus::Success;
        };
        assert(view.bindImports(image, resolve) == NtStatus::InvalidImageFormat);
    };
    invalidImport(0x10B0, UINT64_MAX);
    invalidImport(0x10B0, 0x1FFE);
    assert(view.map(image, kBase) == NtStatus::Success);
    std::memset(image.data() + 0x10A0, 'X', 0x160);
    assert(view.bindImports(image, missing) == NtStatus::InvalidImageFormat);
}

void checkExportsAndTls() {
    auto file = fixture();
    auto nt = headers(file);
    nt.optionalHeader.dataDirectory[0] = {0x2020, 0x100};
    nt.optionalHeader.dataDirectory[9] = {0x20B0, sizeof(ImageTlsDirectory64)};
    write(file, 0x80, nt);
    ImageExportDirectory exports{};
    exports.base = 42;
    exports.numberOfFunctions = 2;
    exports.numberOfNames = 1;
    exports.addressOfFunctions = 0x2050;
    exports.addressOfNames = 0x2060;
    exports.addressOfNameOrdinals = 0x2070;
    write(file, 0x420, exports);
    write(file, 0x450, uint32_t{0x1000});
    write(file, 0x454, uint32_t{0x2090});
    write(file, 0x460, uint32_t{0x2080});
    write(file, 0x470, uint16_t{0});
    std::memcpy(file.data() + 0x480, "Example", 8);
    std::memcpy(file.data() + 0x490, "k32.Target", 11);
    ImageTlsDirectory64 tls{};
    tls.startAddressOfRawData = kBase + 0x1180;
    tls.endAddressOfRawData = kBase + 0x1184;
    tls.addressOfIndex = kBase + 0x21F0;
    tls.addressOfCallBacks = kBase + 0x20E0;
    tls.sizeOfZeroFill = 16;
    tls.characteristics = 5 << 20;
    write(file, 0x4B0, tls);
    write(file, 0x4E0, kBase + 0x1000);
    write(file, 0x380, uint32_t{0x12345678});
    PeImageView64 view;
    std::array<uint8_t, 0x3000> image{};
    assert(view.inspect(file) == NtStatus::Success);
    assert(view.map(image, kBase) == NtStatus::Success);
    PeImageView64::ExportTarget target{};
    assert(view.findExport(image, kBase, "Example", 0, target) == NtStatus::Success);
    assert(target.address == kBase + 0x1000 && target.forwarder.empty());
    assert(view.findExport(image, kBase, {}, 42, target) == NtStatus::Success);
    assert(target.address == kBase + 0x1000);
    assert(view.findExport(image, kBase, {}, 43, target) == NtStatus::Success);
    assert(target.address == 0 && target.forwarder == "k32.Target");
    assert(view.findExport(image, kBase, "example", 0, target) == NtStatus::ProcedureNotFound);
    assert(view.findExport(image, kBase, {}, 41, target) == NtStatus::ProcedureNotFound);
    PeImageView64::TlsLayout layout;
    assert(view.tlsLayout(image, kBase, layout) == NtStatus::Success);
    assert(layout.present && layout.initialData.size() == 4 && layout.zeroFill == 16);
    assert(layout.initialData[0] == 0x78 && layout.alignment == 16);
    assert(layout.indexRva == 0x21F0 && layout.callbackCount == 1);
    assert(layout.callbacks[0] == kBase + 0x1000);
    write(file, 0x4E0, kBase + 0x2000);
    assert(view.map(image, kBase) == NtStatus::Success);
    assert(view.tlsLayout(image, kBase, layout) == NtStatus::InvalidImageFormat);
    write(file, 0x4E0, kBase + 0x1000);
    tls.addressOfIndex = kBase + 0x2FFF;
    write(file, 0x4B0, tls);
    assert(view.map(image, kBase) == NtStatus::Success);
    assert(view.tlsLayout(image, kBase, layout) == NtStatus::InvalidImageFormat);
    write(file, 0x450, uint32_t{0x5000});
    assert(view.map(image, kBase) == NtStatus::Success);
    assert(view.findExport(image, kBase, "Example", 0, target) == NtStatus::InvalidImageFormat);
    write(file, 0x450, uint32_t{0x1000});
    write(file, 0x470, uint16_t{2});
    assert(view.map(image, kBase) == NtStatus::Success);
    assert(view.findExport(image, kBase, "Example", 0, target) == NtStatus::InvalidImageFormat);
    write(file, 0x470, uint16_t{0});
    std::memset(file.data() + 0x490, 'X', 0x90);
    assert(view.map(image, kBase) == NtStatus::Success);
    assert(view.findExport(image, kBase, {}, 43, target) == NtStatus::InvalidImageFormat);
}

File moduleFixture(const char* forwarder = nullptr) {
    auto file = fixture();
    auto nt = headers(file);
    nt.optionalHeader.dataDirectory[1] = {};
    nt.optionalHeader.dataDirectory[0] = {0x2020, 0x100};
    write(file, 0x80, nt);
    ImageExportDirectory exports{};
    exports.base = 42;
    exports.numberOfFunctions = 1;
    exports.numberOfNames = 1;
    exports.addressOfFunctions = 0x2050;
    exports.addressOfNames = 0x2060;
    exports.addressOfNameOrdinals = 0x2070;
    write(file, 0x420, exports);
    write(file, 0x450, uint32_t{forwarder ? 0x2090u : 0x1000u});
    write(file, 0x460, uint32_t{0x2080});
    std::memcpy(file.data() + 0x480, "Example", 8);
    if (forwarder) {
        assert(std::strlen(forwarder) < 0x90);
        std::memcpy(file.data() + 0x490, forwarder, std::strlen(forwarder) + 1);
    }
    return file;
}

void checkNativeImageSet() {
    um::NativeUserImageSet64 modules;
    const auto app = fixture();
    const auto runtime = moduleFixture();
    const auto forwarder = moduleFixture("KeRnEl32.#42");
    const auto cycleA = moduleFixture("b.Example");
    const auto cycleB = moduleFixture("a.Example");
    std::array<std::array<uint8_t, 0x3000>, 6> images{};
    assert(modules.addModule("app.exe", app, images[0], kBase) == NtStatus::Success);
    const auto unbound = images[0];
    assert(modules.bindAll() == NtStatus::NoSuchFile);
    assert(!modules.importsBound() && images[0] == unbound);
    assert(modules.bindingFailure().library == "kernel32.dll");
    assert(modules.addModule("KERNEL32.dll", runtime, images[1], kBase + 0x10000) == NtStatus::Success);
    uint64_t address{};
    assert(modules.addModule("kernel32.DLL", runtime, images[2], kBase + 0x20000) ==
           NtStatus::ObjectNameCollision);
    assert(modules.addModule("other.dll", runtime, images[2], kBase) == NtStatus::Conflict);
    assert(modules.addModule("other.dll", runtime, images[0], kBase + 0x20000) == NtStatus::Conflict);
    assert(modules.addModule("fwd.dll", forwarder, images[2], kBase + 0x20000) == NtStatus::Success);
    assert(modules.resolve("fwd", "Example", 0, address) == NtStatus::Success);
    assert(address == kBase + 0x11000);
    assert(modules.resolve("fwd", "example", 0, address) == NtStatus::ProcedureNotFound);
    assert(modules.addModule("a.dll", cycleA, images[3], kBase + 0x30000) == NtStatus::Success);
    assert(modules.addModule("b.dll", cycleB, images[4], kBase + 0x40000) == NtStatus::Success);
    assert(modules.resolve("a", "Example", 0, address) == NtStatus::CircularReference);
    assert(modules.addModule("ucrtbase.dll", runtime, images[5], kBase + 0x50000) == NtStatus::Success);
    assert(modules.resolve("api-ms-win-crt-heap-l1-1-0.dll", "Example", 0, address) == NtStatus::Success);
    assert(address == kBase + 0x51000);
    assert(modules.resolve("api-ms-win-core-invented.dll", "Example", 0, address) == NtStatus::NoSuchFile);
    assert(modules.bindAll() == NtStatus::Success && modules.importsBound());
    std::memcpy(&address, images[0].data() + 0x10D0, sizeof(address));
    assert(address == kBase + 0x11000);
    assert(modules.bindAll() == NtStatus::Success);
    assert(modules.addModule("extra.dll", runtime, images[5], kBase + 0x60000) ==
           NtStatus::InvalidParameter);
}
} // namespace

int main(int argc, char** argv) {
    checkMapping();
    checkMalformed();
    checkExportsAndTls();
    checkNativeImageSet();
    PeImageView64 embedded;
    assert(embedded.inspect(kUnmodifiedSampleBinary) == NtStatus::Success);
    for (int index = 1; index < argc; ++index) {
        std::ifstream input(argv[index], std::ios::binary);
        assert(input.is_open());
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
        PeImageView64 view;
        assert(view.inspect(bytes) == NtStatus::Success);
        std::vector<uint8_t> image(view.headers().optionalHeader.sizeOfImage);
        assert(view.map(image, view.headers().optionalHeader.imageBase) == NtStatus::Success);
        PeImageView64::TlsLayout tls;
        assert(view.tlsLayout(image, view.headers().optionalHeader.imageBase, tls) == NtStatus::Success);
        std::cout << "Validated and mapped " << argv[index] << '\n';
    }
    std::cout << "Bounded native PE mapping/import tests passed.\n";
}
