// End-to-end verification of the file-backed SEC_IMAGE path: the image is
// opened through the mounted FAT32 volume, turned into a SEC_FILE|SEC_IMAGE
// section by the canonical Mm path, and used to initialize a real process.
#include "micant/km/executive/ps.hpp"
#include "micant/km/executive/ps_objects.hpp"
#include "micant/km/executive/io_file.hpp"
#include "micant/km/executive/section.hpp"
#include "micant/km/executive/usermode.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"

namespace micant::sys {
NtStatus referenceProcessImageSection(Handle handle, ob::ObjectHeader*& reference) noexcept;
}

namespace {

struct ScopedHandle {
    micant::Handle value{};
    ~ScopedHandle() {
        if (value && micant::sys::NtClose(value) != micant::NtStatus::Success) __builtin_trap();
    }
};

struct ScopedReference {
    micant::ob::ObjectHeader* value{};
    ~ScopedReference() {
        if (value) micant::ob::ObDereferenceObject(value);
    }
};

// Build the system DLL path the same way the Ps loader does, so the test
// exercises the real mounted-volume lookup instead of a hardcoded path.
bool buildSystemPath(const char* leaf, char16_t* output, size_t capacity,
                     size_t& length) noexcept {
    using namespace micant;
    char16_t directory[256]{};
    size_t directoryLength{};
    if (io::IoQuerySystemDirectory(directory, sizeof(directory) / sizeof(directory[0]),
                                   directoryLength) != NtStatus::Success)
        return false;
    if (!directoryLength || directoryLength >= sizeof(directory) / sizeof(directory[0]))
        return false;
    size_t written{};
    for (size_t index = 0; index < directoryLength; ++index) {
        if (!directory[index]) break;
        if (written + 1 >= capacity) return false;
        output[written++] = directory[index];
    }
    if (!written || output[written - 1] != u'\\') {
        if (written + 1 >= capacity) return false;
        output[written++] = u'\\';
    }
    for (size_t index = 0; leaf[index]; ++index) {
        if (written + 1 >= capacity) return false;
        output[written++] = static_cast<char16_t>(static_cast<uint8_t>(leaf[index]));
    }
    output[written] = 0;
    length = written;
    return true;
}

// Section deletion is deferred to a passive worker once the clock runs.
void drainDeferredDeletes() noexcept {
    using namespace micant;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; sti" : "=r"(flags) : : "memory");
    const uint64_t start = hal::amd64::bootstrapSchedulerClockTicks();
    for (size_t spin = 0; spin < 50000000 && io::IoDeferredFileDeletesPending() &&
         hal::amd64::bootstrapSchedulerClockTicks() - start < 100; ++spin)
        __asm__ __volatile__("pause");
    __asm__ __volatile__("cli" : : : "memory");
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
}

} // namespace

// Verifies that a real PE file on the mounted FAT32 volume becomes a
// validated SEC_IMAGE section and initializes a process image.
extern "C" bool MicantTestPsFileBackedImage() noexcept {
    using namespace micant;
    if (ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL) return false;

    const size_t activeBefore = mm::PfnDatabase::get().getActiveCount();
    char16_t imagePath[256]{};
    size_t imagePathLength{};
    if (!buildSystemPath("fileimage.exe", imagePath, sizeof(imagePath) / sizeof(imagePath[0]),
                         imagePathLength))
        return false;

    {
        // The file must be reachable through the canonical Io backend, which
        // is what proves the volume is mounted and the stream identity
        // resolves.
        ScopedHandle fileHandle;
        if (io::IoOpenBackendFile(&fileHandle.value, 0x0021,
                                  {imagePath, imagePathLength}) != NtStatus::Success)
            return false;

        ScopedHandle sectionHandle;
        auto status = sys::NtCreateSectionFromKernel(&sectionHandle.value,
            section::SECTION_ALL_ACCESS, nullptr, nullptr, mm::PAGE_EXECUTE_READ,
            section::SEC_FILE | section::SEC_IMAGE, fileHandle.value);
        if (status != NtStatus::Success) return false;

        ScopedReference sectionReference;
        status = sys::referenceProcessImageSection(sectionHandle.value, sectionReference.value);
        if (status != NtStatus::Success || !sectionReference.value) return false;
        auto* image = sectionReference.value->as<section::SectionObject>();
        if (!image || !image->getFileSize() || !image->getSize() || !image->getImageBase() ||
            image->getFileName().empty())
            return false;
        // The section must have been built from the file bytes, not from a
        // synthetic in-memory PE: the recorded name is the opened path.
        if (image->getFileName().size() != imagePathLength) return false;
        for (size_t index = 0; index < imagePathLength; ++index)
            if (image->getFileName()[index] != imagePath[index]) return false;

        ps::EProcess process;
        if (process.initializeAddressSpace() != NtStatus::Success) return false;
        uintptr_t previous{};
        __asm__ __volatile__("movq %%cr3, %0" : "=r"(previous));
        if (!hal::amd64::switchProcessPagingRootByPhysical(process.getDirectoryTableBase()))
            return false;
        const auto imageStatus = process.initializeMainImageFromSection(*image,
                                                                        sectionReference.value);
        const bool restored = hal::amd64::switchProcessPagingRootByPhysical(previous);
        if (!restored) return false;
        if (imageStatus != NtStatus::Success) {
            (void)process.closeAddressSpace();
            return false;
        }
        // The loader must have produced a finalized native image set whose
        // main module is the file we opened.
        if (!process.nativeImagesFinalized() || !process.nativeImages() ||
            !process.nativeImages()->size() || !process.userPebAddress()) {
            (void)process.closeAddressSpace();
            return false;
        }
        const auto* module = process.nativeImages()->module(0);
        if (!module || !module->base || module->name != "fileimage.exe") {
            (void)process.closeAddressSpace();
            return false;
        }
        if (process.closeAddressSpace() != NtStatus::Success) return false;
    }

    drainDeferredDeletes();
    // Every PFN the image path acquired must be released again.
    return mm::PfnDatabase::get().getActiveCount() == activeBefore;
}
