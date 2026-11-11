#if defined(MICANT_FREESTANDING)
#include "micant/km/core/syscalls.hpp"
#include "micant/km/executive/mm.hpp"
#include "micant/km/executive/section.hpp"
#include "micant/km/executive/ob.hpp"
#include "micant/km/executive/ps.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"
#include <new>

namespace micant::sys {
namespace {
mm::ProcessAddressSpace addressSpace;
ob::ObjectType sectionType;
mm::ProcessAddressSpace& currentAddressSpace() noexcept {
    const auto* thread = ke::KeGetCurrentThread();
    return thread && thread->owningProcess ? thread->owningProcess->getAddressSpace() : addressSpace;
}
void deleteSection(ob::ObjectHeader* header) {
    header->as<section::SectionObject>()->~SectionObject();
}
bool acquireSectionView(void* owner) noexcept {
    static_cast<ob::ObjectHeader*>(owner)->addReference();
    return true;
}
void releaseSectionView(void* owner) noexcept {
    ob::ObDereferenceObject(static_cast<ob::ObjectHeader*>(owner));
}
}

bool initializeBootstrapSectionServices() noexcept {
    sectionType.typeId = ob::ObjectTypeId::Section;
    sectionType.deleteProcedure = deleteSection;
    return addressSpace.configurePagingBackend(
        {nullptr, hal::amd64::mapBootstrapManagedPage, hal::amd64::unmapBootstrapManagedPage,
         hal::amd64::mapBootstrapProtectManagedPage, hal::amd64::mapBootstrapManagedView,
         hal::amd64::unmapBootstrapManagedView},
        hal::amd64::BootstrapManagedBase, hal::amd64::BootstrapManagedSize);
}

NtStatus NtCreateSection(Handle* out, uint32_t access, ObjectAttributes* attributes,
                         LargeInteger* maximum, uint32_t protect, uint32_t allocation,
                         Handle file) {
    if (!out || !maximum || maximum->quadPart <= 0) return NtStatus::InvalidParameter;
    if (attributes) {
        if (attributes->length != sizeof(ObjectAttributes)) return NtStatus::InvalidParameter;
        if (attributes->rootDirectory || attributes->objectName || attributes->attributes ||
            attributes->securityDescriptor || attributes->securityQualityOfService)
            return NtStatus::NotImplemented;
    }
    if (file) return NtStatus::NotImplemented;
    if (access & ~section::SECTION_ALL_ACCESS) return NtStatus::NotImplemented;
    void* body{};
    ob::ObjectHeader* header{};
    NtStatus status = ob::ObCreateObject(ob::KernelMode, &sectionType, nullptr,
                                         sizeof(section::SectionObject), &body, &header);
    if (status != NtStatus::Success) return status;
    auto* object = new (body) section::SectionObject(static_cast<size_t>(maximum->quadPart), allocation, protect);
    status = object->getInitializationStatus();
    if (status == NtStatus::Success)
        status = ob::detail::currentHandleTable().createHandle(header, *out, access);
    ob::ObDereferenceObject(header);
    return status;
}

NtStatus NtMapViewOfSection(Handle handle, Handle process, uintptr_t* base, uintptr_t zeroBits,
                            size_t commitSize, LargeInteger* offset, size_t* viewSize,
                            uint32_t inherit, uint32_t allocation, uint32_t protection) {
    if (!base || !viewSize) return NtStatus::InvalidParameter;
    if (process != InvalidHandleValue) return NtStatus::NotImplemented;
    if (inherit != 1 && inherit != 2) return NtStatus::InvalidParameter;
    // ViewShare requires inheritance into future child address spaces, which
    // the bootstrap process model does not yet provide.
    if (inherit == 1 || zeroBits || commitSize || allocation)
        return NtStatus::NotImplemented;
    if (protection != mm::PAGE_READONLY && protection != mm::PAGE_READWRITE)
        return NtStatus::NotImplemented;
    uint32_t desired = protection == mm::PAGE_READWRITE ? section::SECTION_MAP_WRITE : section::SECTION_MAP_READ;
    ob::ObjectHeader* header{};
    NtStatus status = ob::ObReferenceObjectByHandle(handle, ob::detail::currentHandleTable(),
                                                     desired, &sectionType, &header);
    if (status != NtStatus::Success) return status;
    auto* object = header->as<section::SectionObject>();
    if (offset && offset->quadPart < 0) {
        ob::ObDereferenceObject(header);
        return NtStatus::InvalidParameter;
    }
    const uint64_t sectionOffset = offset ? static_cast<uint64_t>(offset->quadPart) &
        ~(static_cast<uint64_t>(mm::ProcessAddressSpace::AllocationGranularity) - 1) : 0;
    if (protection == mm::PAGE_READWRITE && object->getPageProtection() != mm::PAGE_READWRITE) {
        ob::ObDereferenceObject(header);
        return NtStatus::SectionProtection;
    }
    if (object->getSize() > SIZE_MAX - mm::PageMask4KB) {
        ob::ObDereferenceObject(header);
        return NtStatus::InvalidViewSize;
    }
    const size_t sectionSize = (object->getSize() + mm::PageMask4KB) & ~mm::PageMask4KB;
    if (sectionOffset >= sectionSize) {
        ob::ObDereferenceObject(header);
        return NtStatus::InvalidViewSize;
    }
    const size_t remaining = sectionSize - static_cast<size_t>(sectionOffset);
    size_t length = *viewSize ? *viewSize : remaining;
    if (!length || length > remaining || length > SIZE_MAX - mm::PageMask4KB) {
        const auto failure = length > remaining ? NtStatus::InvalidViewSize : NtStatus::InvalidParameter;
        ob::ObDereferenceObject(header);
        return failure;
    }
    length = (length + mm::PageMask4KB) & ~mm::PageMask4KB;
    uintptr_t target = *base ? *base & ~(static_cast<uintptr_t>(mm::ProcessAddressSpace::AllocationGranularity) - 1) : 0;
    const auto backing = object->getBackingReference();
    const size_t firstPage = static_cast<size_t>(sectionOffset) >> mm::PageShift4KB;
    // Later protection changes must remain compatible with this view's
    // original mapping protection, even if the section handle grants more.
    const uint32_t maximumProtection = protection;
    status = currentAddressSpace().mapSharedPages(target, length, protection, backing.pfns + firstPage,
                                         backing.pageCount - firstPage,
                                         header, acquireSectionView, releaseSectionView, maximumProtection);
    if (status == NtStatus::Success) {
        *base = target;
        *viewSize = length;
        if (offset) offset->quadPart = static_cast<int64_t>(sectionOffset);
    }
    ob::ObDereferenceObject(header);
    return status;
}

NtStatus NtUnmapViewOfSection(Handle process, uintptr_t base) {
    if (process != InvalidHandleValue) return NtStatus::NotImplemented;
    const auto result = currentAddressSpace().unmapSharedPages(base);
    return result == NtStatus::InvalidParameter ? NtStatus::NotMappedView : result;
}

bool verifyBootstrapSectionOwnership() noexcept {
    const size_t activeBefore = mm::PfnDatabase::get().getActiveCount();
    const auto objectsBefore = sectionType.totalNumberOfObjects.load(std::memory_order_relaxed);
    LargeInteger maximum{};
    maximum.quadPart = mm::PageSize4KB;
    Handle handle{};
    if (NtCreateSection(&handle, section::SECTION_MAP_READ, nullptr, &maximum,
                        mm::PAGE_READONLY, section::SEC_COMMIT, 0) != NtStatus::Success) return false;
    uintptr_t base{};
    size_t size = mm::PageSize4KB;
    const auto mapStatus = NtMapViewOfSection(handle, InvalidHandleValue, &base, 0, 0,
                                               nullptr, &size, 2, 0, mm::PAGE_READONLY);
    if (mapStatus != NtStatus::Success) { NtClose(handle); return false; }
    if (NtClose(handle) != NtStatus::Success ||
        sectionType.totalNumberOfObjects.load(std::memory_order_relaxed) != objectsBefore + 1) return false;
    if (NtUnmapViewOfSection(InvalidHandleValue, base) != NtStatus::Success) return false;
    return sectionType.totalNumberOfObjects.load(std::memory_order_relaxed) == objectsBefore &&
           mm::PfnDatabase::get().getActiveCount() == activeBefore;
}
}
#else
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
#endif
