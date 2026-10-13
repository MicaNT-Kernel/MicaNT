#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/mm.hpp"
#include "micant/km/executive/section.hpp"
#include "micant/km/drivers/fs.hpp"
#include <unordered_map>
#include <memory>

namespace micant::sys {

static mm::ProcessAddressSpace g_KernelAddressSpace;
static std::unordered_map<Handle, std::shared_ptr<section::SectionObject>> g_KernelSections;
static Handle g_NextSectionHandle = 0x300;

namespace internal {

bool MmCloseSectionHandle(Handle h) {
    return g_KernelSections.erase(h) > 0;
}

} // namespace internal

NtStatus NtAllocateVirtualMemory(
    Handle /*processHandle*/,
    uintptr_t* baseAddress,
    uintptr_t /*zeroBits*/,
    size_t* regionSize,
    uint32_t allocationType,
    uint32_t protect
) {
    if (!baseAddress || !regionSize || *regionSize == 0) {
        return NtStatus::InvalidParameter;
    }

    uintptr_t addr = *baseAddress;
    NtStatus status = g_KernelAddressSpace.allocate(addr, *regionSize, allocationType, protect);
    if (NT_SUCCESS(status)) {
        *baseAddress = addr;
    }
    return status;
}

NtStatus NtFreeVirtualMemory(
    Handle /*processHandle*/,
    uintptr_t* baseAddress,
    size_t* regionSize,
    uint32_t freeType
) {
    if (!baseAddress || *baseAddress == 0) {
        return NtStatus::InvalidParameter;
    }

    size_t sz = regionSize ? *regionSize : 0;
    return g_KernelAddressSpace.free(*baseAddress, sz, freeType);
}

NtStatus NtProtectVirtualMemory(
    Handle /*processHandle*/,
    uintptr_t* baseAddress,
    size_t* regionSize,
    uint32_t /*newProtect*/,
    uint32_t* oldProtect
) {
    if (!baseAddress || !regionSize) {
        return NtStatus::InvalidParameter;
    }
    if (oldProtect) {
        *oldProtect = mm::PAGE_READWRITE;
    }
    return NtStatus::Success;
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
    if (fileObj && fileObj->getFileSize() > 0) {
        sec->loadData(fileObj->getData().data(), fileObj->getFileSize());
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
    uint32_t allocationType,
    uint32_t win32Protect
) {
    if (!baseAddress || !viewSize) return NtStatus::InvalidParameter;
    auto it = g_KernelSections.find(sectionHandle);
    if (it == g_KernelSections.end() || !it->second) return NtStatus::InvalidHandle;

    auto sec = it->second;
    size_t sz = (*viewSize == 0 || *viewSize > sec->getSize()) ? sec->getSize() : *viewSize;

    uintptr_t addr = *baseAddress;
    if (addr == 0) {
        addr = reinterpret_cast<uintptr_t>(sec->getData());
    }

    g_KernelAddressSpace.allocate(
        addr,
        sz,
        allocationType ? allocationType : (mm::MEM_COMMIT | mm::MEM_RESERVE),
        win32Protect ? win32Protect : sec->getPageProtection()
    );

    *baseAddress = addr;
    *viewSize = sz;
    return NtStatus::Success;
}

NtStatus NtUnmapViewOfSection(Handle /*processHandle*/, uintptr_t baseAddress) {
    return g_KernelAddressSpace.free(baseAddress, 0, mm::MEM_RELEASE);
}

} // namespace micant::sys
