#include "micant/km/executive/io_file.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/km/drivers/storage/ahci_baremetal.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"
#include <new>

namespace micant::io {
namespace {

using micant::km::ahci_baremetal::BareMetalAhciController;
using micant::km::ahci_baremetal::Fat32FileSystem;
using micant::km::ahci_baremetal::Fat32Volume;
using micant::km::ahci_baremetal::Fat32DirEntry;

struct FatFileContext {
    Fat32Volume* volume{};
    Fat32DirEntry entry{};
};

ke::SpinLock g_fatIoLock;
bool g_providerRegistered{};

void* mapAhciRegisters(uint64_t physical, size_t size) noexcept {
    return hal::amd64::mapBootstrapDeviceMemory(physical, size);
}

uint64_t translateDmaAddress(const void* address) noexcept {
    return hal::amd64::translateBootstrapAddress(address);
}

NtStatus capturePath(std::u16string_view name, Fat32Volume*& volume,
                     char (&path)[256]) noexcept {
    volume = nullptr;
    if (name.empty()) return NtStatus::ObjectNameNotFound;
    size_t offset{};

    if (name.size() >= 4 && name[0] == u'\\' && name[1] == u'?' &&
        name[2] == u'?' && name[3] == u'\\') {
        offset = 4;
    } else if (name.size() >= 4 && name[0] == u'\\' && name[1] == u'\\' &&
               name[2] == u'?' && name[3] == u'\\') {
        offset = 4;
    }

    if (name.size() - offset >= 2 && name[offset + 1] == u':') {
        const char16_t drive = name[offset];
        if (drive == u'C' || drive == u'c') volume = &Fat32FileSystem::get('C');
        else if (drive == u'D' || drive == u'd') volume = &Fat32FileSystem::get('D');
        else if (drive == u'S' || drive == u's') volume = &Fat32FileSystem::get('S');
        else return NtStatus::NoSuchDevice;
        offset += 2;
    } else {
        constexpr char16_t devicePrefix[] = u"\\Device\\Harddisk0\\Partition";
        constexpr size_t prefixLength = (sizeof(devicePrefix) / sizeof(devicePrefix[0])) - 1;
        if (name.size() - offset < prefixLength + 1) return NtStatus::ObjectPathNotFound;
        bool prefixMatches = true;
        for (size_t i = 0; i < prefixLength; ++i) {
            char16_t actual = name[offset + i];
            char16_t expected = devicePrefix[i];
            if (actual >= u'A' && actual <= u'Z') actual += u'a' - u'A';
            if (expected >= u'A' && expected <= u'Z') expected += u'a' - u'A';
            if (actual != expected) { prefixMatches = false; break; }
        }
        if (!prefixMatches) return NtStatus::ObjectPathNotFound;
        const size_t numberIndex = offset + prefixLength;
        if (name[numberIndex] < u'1' || name[numberIndex] > u'3')
            return NtStatus::ObjectPathNotFound;
        const char16_t partition = name[numberIndex];
        if (partition == u'1') volume = &Fat32FileSystem::get('S');
        else if (partition == u'2') volume = &Fat32FileSystem::get('C');
        else volume = &Fat32FileSystem::get('D');
        offset = numberIndex + 1;
        if (offset < name.size() && name[offset] != u'\\' && name[offset] != u'/')
            return NtStatus::ObjectPathSyntaxBad;
    }

    while (offset < name.size() && (name[offset] == u'\\' || name[offset] == u'/')) ++offset;
    if (!volume || offset == name.size()) return NtStatus::ObjectNameNotFound;
    if (!volume->isMounted()) return NtStatus::NoSuchDevice;

    size_t length{};
    for (size_t i = offset; i < name.size(); ++i) {
        const char16_t ch = name[i];
        if (ch == 0 || ch > 0x7F || length + 1 >= sizeof(path))
            return ch > 0x7F ? NtStatus::NotSupported : NtStatus::ObjectPathSyntaxBad;
        path[length++] = static_cast<char>(ch);
    }
    path[length] = '\0';
    if (!length) return NtStatus::ObjectNameNotFound;
    size_t componentLength{};
    for (size_t i = 0; i <= length; ++i) {
        const char ch = path[i];
        if (ch == '\0' || ch == '\\' || ch == '/') {
            if (componentLength > 127) return NtStatus::ObjectPathSyntaxBad;
            if ((componentLength == 1 && path[i - 1] == '.') ||
                (componentLength == 2 && path[i - 2] == '.' && path[i - 1] == '.'))
                return NtStatus::ObjectPathSyntaxBad;
            componentLength = 0;
        } else {
            ++componentLength;
        }
    }
    return NtStatus::Success;
}

NtStatus openFatFile(void*, std::u16string_view name, FileBackend& backend) noexcept {
    backend = {};
    Fat32Volume* volume{};
    char path[256]{};
    const NtStatus pathStatus = capturePath(name, volume, path);
    if (pathStatus != NtStatus::Success) return pathStatus;

    Fat32DirEntry entry{};
    {
        ke::SpinLockGuard guard(g_fatIoLock);
        if (!volume->findPath(path, entry)) return NtStatus::ObjectNameNotFound;
    }
    if (entry.isDirectory) return NtStatus::FileIsADirectory;

    auto* context = new (std::nothrow_t{}) FatFileContext{};
    if (!context) return NtStatus::NoMemory;
    context->volume = volume;
    context->entry = entry;

    backend.context = context;
    backend.size = context->entry.fileSize;
    backend.name = name;
    backend.read = [](void* opaque, uint64_t offset, void* buffer, size_t length,
                      size_t* bytesRead) noexcept -> NtStatus {
        if (!opaque || !bytesRead || (!buffer && length)) return NtStatus::InvalidParameter;
        *bytesRead = 0;
        auto& file = *static_cast<FatFileContext*>(opaque);
        if (offset > file.entry.fileSize || length > file.entry.fileSize - offset)
            return NtStatus::EndOfFile;
        ke::SpinLockGuard guard(g_fatIoLock);
        size_t transferred{};
        if (!file.volume->readFileEntryAt(file.entry, offset, buffer, length, transferred))
            return NtStatus::Unsuccessful;
        *bytesRead = transferred;
        return transferred == length ? NtStatus::Success : NtStatus::EndOfFile;
    };
    backend.release = [](void* opaque) noexcept {
        delete static_cast<FatFileContext*>(opaque);
    };
    return NtStatus::Success;
}

} // namespace

NtStatus IoInitializeAhciFileProvider() noexcept {
    if (g_providerRegistered) return NtStatus::Success;
    auto& controller = BareMetalAhciController::get();
    if (!controller.isInitialized() && !controller.initialize(&mapAhciRegisters,
                                                               &translateDmaAddress))
        return NtStatus::NoSuchDevice;
    if (!Fat32FileSystem::mountAll(controller)) return NtStatus::UnrecognizedVolume;
    const NtStatus status = IoRegisterFileOpenRoutine(&openFatFile, nullptr);
    if (status == NtStatus::Success) g_providerRegistered = true;
    return status;
}

} // namespace micant::io
