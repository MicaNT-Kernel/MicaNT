#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/mm.hpp"
#include "micant/km/executive/section.hpp"
#include "micant/km/drivers/fs/fs.hpp"
#include <unordered_map>
#include <memory>
#include <limits>
#include <mutex>

namespace micant::sys {

static mm::ProcessAddressSpace g_KernelAddressSpace;
static std::mutex g_AddressSpaceServiceMutex;
static std::unordered_map<Handle, std::shared_ptr<section::SectionObject>> g_KernelSections;
static Handle g_NextSectionHandle = 0x300;

namespace {
bool pageRange(uintptr_t address, size_t size, uintptr_t& start, size_t& length) {
    if (!size) return false;
    const size_t offset = address & mm::PageMask4KB;
    if (size > std::numeric_limits<size_t>::max() - offset ||
        size + offset > std::numeric_limits<size_t>::max() - mm::PageMask4KB)
        return false;
    start = address & ~static_cast<uintptr_t>(mm::PageMask4KB);
    length = (size + offset + mm::PageMask4KB) & ~static_cast<size_t>(mm::PageMask4KB);
    return length && start <= std::numeric_limits<uintptr_t>::max() - (length - 1);
}
NtStatus validateProcessHandle(Handle processHandle) {
    if (processHandle == InvalidHandleValue) return NtStatus::Success;
    return processHandle == 0 ? NtStatus::InvalidHandle : NtStatus::NotImplemented;
}
}

namespace internal {

bool MmCloseSectionHandle(Handle h) {
    return g_KernelSections.erase(h) > 0;
}

} // namespace internal

NtStatus NtAllocateVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    uintptr_t zeroBits,
    size_t* regionSize,
    uint32_t allocationType,
    uint32_t protect
) {
    if (!baseAddress || !regionSize || *regionSize == 0) {
        return NtStatus::InvalidParameter;
    }
    if (const auto status = validateProcessHandle(processHandle); !NT_SUCCESS(status)) return status;
    if (zeroBits != 0) return NtStatus::NotImplemented;
    if (protect != mm::PAGE_READONLY && protect != mm::PAGE_READWRITE)
        return NtStatus::NotImplemented;

    uintptr_t addr;
    size_t size;
    if (!pageRange(*baseAddress, *regionSize, addr, size)) return NtStatus::InvalidParameter;
    std::lock_guard lock(g_AddressSpaceServiceMutex);
    NtStatus status = g_KernelAddressSpace.allocate(addr, size, allocationType, protect);
    if (NT_SUCCESS(status)) {
        *baseAddress = addr;
        *regionSize = size;
    }
    return status;
}

NtStatus NtFreeVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    size_t* regionSize,
    uint32_t freeType
) {
    if (!baseAddress || !regionSize || *baseAddress == 0) {
        return NtStatus::InvalidParameter;
    }
    if (const auto status = validateProcessHandle(processHandle); !NT_SUCCESS(status)) return status;
    if (freeType != mm::MEM_RELEASE && freeType != mm::MEM_DECOMMIT)
        return NtStatus::NotImplemented;
    std::lock_guard lock(g_AddressSpaceServiceMutex);
    const auto* vad = g_KernelAddressSpace.findVad(*baseAddress);
    if (!vad) return NtStatus::InvalidParameter;
    uintptr_t addr = *baseAddress;
    size_t size = *regionSize;
    if (freeType == mm::MEM_RELEASE || size == 0) {
        if (addr != vad->startingAddress || (freeType == mm::MEM_RELEASE && size != 0))
            return NtStatus::InvalidParameter;
        size = vad->endingAddress - vad->startingAddress + 1;
    } else if (!pageRange(addr, size, addr, size)) {
        return NtStatus::InvalidParameter;
    }
    // The hosted fallback currently decommits an entire VAD, never a subrange.
    if (freeType == mm::MEM_DECOMMIT &&
        (addr != vad->startingAddress || size != vad->endingAddress - vad->startingAddress + 1))
        return NtStatus::NotImplemented;
    const NtStatus status = g_KernelAddressSpace.free(addr, freeType == mm::MEM_RELEASE ? 0 : size, freeType);
    if (NT_SUCCESS(status)) {
        *baseAddress = addr;
        *regionSize = size;
    }
    return status;
}

NtStatus NtProtectVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    size_t* regionSize,
    uint32_t newProtect,
    uint32_t* oldProtect
) {
    if (!baseAddress || !regionSize || !oldProtect) {
        return NtStatus::InvalidParameter;
    }
    if (const auto status = validateProcessHandle(processHandle); !NT_SUCCESS(status)) return status;
    uintptr_t addr;
    size_t size;
    if (!pageRange(*baseAddress, *regionSize, addr, size)) return NtStatus::InvalidParameter;
    std::lock_guard lock(g_AddressSpaceServiceMutex);
    const auto* vad = g_KernelAddressSpace.findVad(addr);
    if (!vad) return NtStatus::InvalidParameter;
    // The hosted fallback only supports changes to an entire committed VAD.
    if (addr != vad->startingAddress || size != vad->endingAddress - vad->startingAddress + 1 ||
        !vad->committed || (newProtect != mm::PAGE_READONLY && newProtect != mm::PAGE_READWRITE))
        return NtStatus::NotImplemented;
    uint32_t previous;
    const NtStatus status = g_KernelAddressSpace.protect(addr, size, newProtect, &previous);
    if (NT_SUCCESS(status)) {
        *baseAddress = addr;
        *regionSize = size;
        *oldProtect = previous;
    }
    return status;
}

NtStatus NtCreateSection(
    Handle* sectionHandle,
    uint32_t /*desiredAccess*/,
    ObjectAttributes* /*objectAttributes*/,
    LargeInteger* maximumSize,
    uint32_t sectionPageProtection,
    uint32_t allocationAttributes,
    Handle fileHandle
) {
    if (!sectionHandle) return NtStatus::InvalidParameter;
    size_t size = maximumSize ? static_cast<size_t>(maximumSize->quadPart) : 4096;

    fs::FileObject* fileObj = nullptr;
    if (fileHandle != 0) {
        fileObj = LookupKernelFileObject(fileHandle);
        if (fileObj) {
            if (size == 0 || (maximumSize && maximumSize->quadPart == 0)) {
                size = fileObj->getFileSize();
            }
        }
    }
    if (size == 0) size = 4096;

    auto sec = std::make_shared<section::SectionObject>(size, allocationAttributes, sectionPageProtection);
    if (!NT_SUCCESS(sec->getInitializationStatus())) return sec->getInitializationStatus();
    if (fileObj && fileObj->getFileSize() > 0) {
        const NtStatus status = sec->loadData(fileObj->getData().data(), fileObj->getFileSize());
        if (!NT_SUCCESS(status)) return status;
    }

    Handle h = g_NextSectionHandle++;
    g_KernelSections[h] = sec;
    *sectionHandle = h;
    return NtStatus::Success;
}

NtStatus NtMapViewOfSection(
    Handle sectionHandle,
    Handle /*processHandle*/,
    uintptr_t* baseAddress,
    uintptr_t /*zeroBits*/,
    size_t /*commitSize*/,
    LargeInteger* /*sectionOffset*/,
    size_t* viewSize,
    uint32_t /*inheritDisposition*/,
    uint32_t /*allocationType*/,
    uint32_t /*win32Protect*/
) {
    if (!baseAddress || !viewSize) return NtStatus::InvalidParameter;
    auto it = g_KernelSections.find(sectionHandle);
    if (it == g_KernelSections.end() || !it->second) return NtStatus::InvalidHandle;
    return NtStatus::NotImplemented;
}

NtStatus NtUnmapViewOfSection(Handle /*processHandle*/, uintptr_t baseAddress) {
    std::lock_guard lock(g_AddressSpaceServiceMutex);
    return g_KernelAddressSpace.free(baseAddress, 0, mm::MEM_RELEASE);
}

} // namespace micant::sys
