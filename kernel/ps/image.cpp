#include "micant/km/executive/ps.hpp"
#include "micant/km/executive/usermode.hpp"
#include <new>

namespace micant::sys {
NtStatus mapBootstrapImageView(mm::ProcessAddressSpace& space, uintptr_t& base, size_t size) noexcept;
}

namespace micant::ps {
namespace {
bool active(const EProcess& process) noexcept {
    uintptr_t root{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(root));
    return root && root == process.getDirectoryTableBase();
}

// Limit the bootstrap loader to page-aligned, contiguous RO/RW/RX sections.
// Unrepresented gaps and W+X pages require protection semantics not yet
// provided by the native image-section backend.
NtStatus validateProtectionLayout(const pe::PeImageView64& view) noexcept {
    const auto& header = view.headers().optionalHeader;
    size_t end = (static_cast<size_t>(header.sizeOfHeaders) + mm::PageMask4KB) & ~mm::PageMask4KB;
    for (size_t i = 0; i < view.headers().fileHeader.numberOfSections; ++i) {
        const auto section = view.section(i);
        const size_t size = section.misc.virtualSize > section.sizeOfRawData ? section.misc.virtualSize : section.sizeOfRawData;
        if (!size) continue;
        if (section.virtualAddress != end || size > SIZE_MAX - mm::PageMask4KB ||
            !(section.characteristics & pe::IMAGE_SCN_MEM_READ) ||
            ((section.characteristics & pe::IMAGE_SCN_MEM_EXECUTE) &&
             (section.characteristics & pe::IMAGE_SCN_MEM_WRITE))) return NtStatus::NotSupported;
        const size_t rounded = (size + mm::PageMask4KB) & ~mm::PageMask4KB;
        if (end > header.sizeOfImage || rounded > header.sizeOfImage - end) return NtStatus::InvalidImageFormat;
        end += rounded;
    }
    return end == header.sizeOfImage ? NtStatus::Success : NtStatus::NotSupported;
}
}

NtStatus EProcess::loadNativeImage(std::string_view name, std::span<const uint8_t> file) noexcept {
    if (!active(*this) || terminating_ || imagesFinalized_) return NtStatus::InvalidParameter;
    pe::PeImageView64 view;
    auto status = view.inspect(file);
    if (status != NtStatus::Success) return status;
    status = validateProtectionLayout(view);
    if (status != NtStatus::Success) return status;
    const uintptr_t base = view.headers().optionalHeader.imageBase;
    const size_t size = view.headers().optionalHeader.sizeOfImage;
    if (!images_) {
        images_.reset(new (std::nothrow_t{}) um::NativeUserImageSet64{});
        if (!images_) return NtStatus::NoMemory;
    }
    uintptr_t target = base;
    status = sys::mapBootstrapImageView(addressSpace_, target, size);
    if (status != NtStatus::Success) return status;
    status = images_->addModule(name, file, {reinterpret_cast<uint8_t*>(target), size}, target);
    if (status != NtStatus::Success) {
        if (addressSpace_.unmapSharedPages(target) != NtStatus::Success) __builtin_trap();
        return status;
    }
    return NtStatus::Success;
}

NtStatus EProcess::finalizeNativeImages() noexcept {
    if (!active(*this) || terminating_ || !images_) return NtStatus::InvalidParameter;
    if (imagesFinalized_) return NtStatus::Success;
    const auto binding = images_->bindAll();
    if (binding != NtStatus::Success) return binding;
    for (size_t i = 0; i < images_->size(); ++i) {
        const auto* module = images_->module(i);
        if (!module) __builtin_trap();
        uint32_t old{};
        const auto headerSize = (static_cast<size_t>(module->view.headers().optionalHeader.sizeOfHeaders) + mm::PageMask4KB) & ~mm::PageMask4KB;
        auto status = addressSpace_.protect(module->base, headerSize, mm::PAGE_READONLY, &old);
        if (status != NtStatus::Success) return status;
        for (size_t j = 0; j < module->view.headers().fileHeader.numberOfSections; ++j) {
            const auto section = module->view.section(j);
            const size_t size = section.misc.virtualSize > section.sizeOfRawData ? section.misc.virtualSize : section.sizeOfRawData;
            if (!size) continue;
            const auto protection = (section.characteristics & pe::IMAGE_SCN_MEM_EXECUTE) ? mm::PAGE_EXECUTE_READ :
                (section.characteristics & pe::IMAGE_SCN_MEM_WRITE) ? mm::PAGE_READWRITE : mm::PAGE_READONLY;
            status = addressSpace_.protect(module->base + section.virtualAddress,
                (size + mm::PageMask4KB) & ~mm::PageMask4KB, protection, &old);
            if (status != NtStatus::Success) return status;
        }
    }
    // A zeroed page provides the documented initial PEB fields. LoaderData,
    // process parameters and other startup state are not initialized yet.
    if (!userPeb_) {
        auto status = addressSpace_.allocate(userPeb_, mm::PageSize4KB,
            mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE);
        if (status != NtStatus::Success) return status;
    }
    imagesFinalized_ = true;
    return NtStatus::Success;
}
}
