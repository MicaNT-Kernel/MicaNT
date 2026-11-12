#include "micant/km/executive/mm.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>

using namespace micant;
using namespace micant::mm;

namespace {
constexpr size_t Pages = 16;
Mmpfn metadata[Pages + 1]{};
uint8_t physical[Pages + 1][PageSize4KB]{};
uint8_t* window{};
struct Mapper {
    uint64_t frames[Pages]{};
    uint32_t protections[Pages]{};
    int failPage{-1};
    bool failGuard{false};
};
Mapper source, target;
Mapper* current{};
struct Owner { size_t references{1}; uint64_t pfns[2]{}; ProcessAddressSpace* reenter{}; bool reentered{}; };
bool acquire(void* context) noexcept {
    ++static_cast<Owner*>(context)->references;
    return true;
}
void release(void* context) noexcept {
    auto& owner = *static_cast<Owner*>(context);
    --owner.references;
    if (owner.reenter && owner.references == 2) {
        uintptr_t address = reinterpret_cast<uintptr_t>(window) + 3 * PageSize4KB;
        owner.reentered = owner.reenter->allocate(address, PageSize4KB, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success &&
                          owner.reenter->free(address, 0, MEM_RELEASE) == NtStatus::Success;
    }
    if (owner.references == 0) for (const auto pfn : owner.pfns) PfnDatabase::get().freePage(pfn);
}

void zero(uint64_t pfn) noexcept { std::memset(physical[pfn], 0, PageSize4KB); }
bool switchRoot(uintptr_t root) noexcept {
    auto* next = reinterpret_cast<Mapper*>(root);
    if (next != &source && next != &target) return false;
    if (current) {
        for (size_t i = 0; i < Pages; ++i)
            if (current->frames[i])
                std::memcpy(physical[current->frames[i]], window + i * PageSize4KB, PageSize4KB);
    }
    current = next;
    for (size_t i = 0; i < Pages; ++i) {
        if (current->frames[i]) std::memcpy(window + i * PageSize4KB, physical[current->frames[i]], PageSize4KB);
        else std::memset(window + i * PageSize4KB, 0, PageSize4KB);
    }
    return true;
}
bool map(void* context, uintptr_t va, uint64_t pfn, uint32_t protection) noexcept {
    auto& mapper = *static_cast<Mapper*>(context);
    const size_t index = (va - reinterpret_cast<uintptr_t>(window)) / PageSize4KB;
    if (&mapper != current || index >= Pages || mapper.frames[index] || static_cast<int>(index) == mapper.failPage)
        return false;
    mapper.frames[index] = pfn;
    mapper.protections[index] = protection;
    std::memcpy(window + index * PageSize4KB, physical[pfn], PageSize4KB);
    return true;
}
bool unmap(void* context, uintptr_t va) noexcept {
    auto& mapper = *static_cast<Mapper*>(context);
    const size_t index = (va - reinterpret_cast<uintptr_t>(window)) / PageSize4KB;
    if (&mapper != current || index >= Pages || !mapper.frames[index]) return false;
    mapper.frames[index] = 0;
    mapper.protections[index] = 0;
    return true;
}
bool protect(void* context, uintptr_t va, uint32_t protection) noexcept {
    auto& mapper = *static_cast<Mapper*>(context);
    const size_t index = (va - reinterpret_cast<uintptr_t>(window)) / PageSize4KB;
    if (&mapper != current || index >= Pages || !mapper.frames[index]) return false;
    mapper.protections[index] = protection;
    return true;
}
bool guard(void* context, uintptr_t va, bool guarded) noexcept {
    auto& mapper = *static_cast<Mapper*>(context);
    const size_t index = (va - reinterpret_cast<uintptr_t>(window)) / PageSize4KB;
    if (&mapper != current || index >= Pages || !mapper.frames[index] || mapper.failGuard ||
        mapper.protections[index] != (PAGE_READWRITE | (guarded ? 0 : PAGE_GUARD))) return false;
    mapper.protections[index] = PAGE_READWRITE | (guarded ? PAGE_GUARD : 0);
    return true;
}
}

int main() {
    window = static_cast<uint8_t*>(std::aligned_alloc(65536, Pages * PageSize4KB));
    assert(window);
    auto& db = PfnDatabase::get();
    PhysicalPageRange range{1, Pages};
    assert(db.initializeFromBootMemory(metadata, {&range, 1}, zero) == NtStatus::Success);
    const uintptr_t base = reinterpret_cast<uintptr_t>(window);
    {
        ProcessAddressSpace parent, child;
        assert(parent.configurePagingBackend({&source, map, unmap, protect, nullptr, nullptr, guard}, base, Pages * PageSize4KB));
        assert(child.configurePagingBackend({&target, map, unmap, protect, nullptr, nullptr, guard}, base, Pages * PageSize4KB));
        assert(switchRoot(reinterpret_cast<uintptr_t>(&source)));
        uintptr_t address = base;
        assert(parent.allocate(address, 3 * PageSize4KB, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success);
        assert(parent.allocate(address, PageSize4KB, MEM_COMMIT, PAGE_READWRITE) == NtStatus::Success);
        uintptr_t last = base + 2 * PageSize4KB;
        assert(parent.allocate(last, PageSize4KB, MEM_COMMIT, PAGE_READWRITE) == NtStatus::Success);
        window[0] = 0x5a;
        window[2 * PageSize4KB] = 0xa5;
        uint32_t old{};
        assert(parent.protect(last, PageSize4KB, PAGE_READONLY, &old) == NtStatus::Success);
        assert(parent.clonePrivateTo(child, reinterpret_cast<uintptr_t>(&source),
                                     reinterpret_cast<uintptr_t>(&target), switchRoot) == NtStatus::Success);
        assert(current == &target && window[0] == 0x5a && window[2 * PageSize4KB] == 0xa5);
        assert(!target.frames[1] && target.protections[2] == PAGE_READONLY);
        window[0] = 0x77;
        assert(switchRoot(reinterpret_cast<uintptr_t>(&source)) && window[0] == 0x5a);
        assert(switchRoot(reinterpret_cast<uintptr_t>(&target)) && child.close() == NtStatus::Success);
        assert(switchRoot(reinterpret_cast<uintptr_t>(&source)) && parent.setStackGuard(base, true) == NtStatus::Success);
        target.failGuard = true;
        assert(parent.clonePrivateTo(child, reinterpret_cast<uintptr_t>(&source),
                                     reinterpret_cast<uintptr_t>(&target), switchRoot) == NtStatus::Unsuccessful);
        assert(source.protections[0] == (PAGE_READWRITE | PAGE_GUARD));
        target.failGuard = false;
        assert(child.close() == NtStatus::Success);
        assert(parent.clonePrivateTo(child, reinterpret_cast<uintptr_t>(&source),
                                     reinterpret_cast<uintptr_t>(&target), switchRoot) == NtStatus::Success);
        assert(window[0] == 0x5a && source.protections[0] == (PAGE_READWRITE | PAGE_GUARD) &&
               target.protections[0] == (PAGE_READWRITE | PAGE_GUARD));
        assert(child.close() == NtStatus::Success);
        assert(switchRoot(reinterpret_cast<uintptr_t>(&source)) && parent.setStackGuard(base, false) == NtStatus::Success);
        auto* image = parent.findVad(base);
        assert(image);
        image->isSectionView = image->isImageView = image->ownsImagePages = true;
        assert(parent.clonePrivateTo(child, reinterpret_cast<uintptr_t>(&source),
                                     reinterpret_cast<uintptr_t>(&target), switchRoot) == NtStatus::Success);
        const auto* copiedImage = child.findVad(base);
        assert(copiedImage && copiedImage->isSectionView && copiedImage->isImageView && copiedImage->ownsImagePages);
        assert(child.unmapSharedPages(base) == NtStatus::Success);
        image->isSectionView = image->isImageView = image->ownsImagePages = false;
        target.failPage = 2;
        assert(parent.clonePrivateTo(child, reinterpret_cast<uintptr_t>(&source),
                                     reinterpret_cast<uintptr_t>(&target), switchRoot) == NtStatus::Unsuccessful);
        target.failPage = -1;
        assert(switchRoot(reinterpret_cast<uintptr_t>(&target)) && child.close() == NtStatus::Success);
        assert(switchRoot(reinterpret_cast<uintptr_t>(&source)) && window[0] == 0x5a);
        assert(parent.close() == NtStatus::Success);
    }
    assert(db.getActiveCount() == 0);
    source = {};
    target = {};
    current = nullptr;
    {
        ProcessAddressSpace parent, child;
        assert(parent.configurePagingBackend({&source, map, unmap, protect}, base, Pages * PageSize4KB));
        assert(child.configurePagingBackend({&target, map, unmap, protect}, base, Pages * PageSize4KB));
        Owner owner{1, {db.allocatePage(), db.allocatePage()}};
        Owner unshared{1, {db.allocatePage(), 0}};
        assert(owner.pfns[0] && owner.pfns[1] && db.zeroAllocatedPage(owner.pfns[0]) && db.zeroAllocatedPage(owner.pfns[1]));
        assert(unshared.pfns[0] && db.zeroAllocatedPage(unshared.pfns[0]));
        assert(switchRoot(reinterpret_cast<uintptr_t>(&source)));
        uintptr_t view = base;
        assert(parent.mapSharedPages(view, 2 * PageSize4KB, PAGE_READWRITE, owner.pfns, 2,
            &owner, acquire, release, PAGE_READWRITE, false, true, 0x10000) == NtStatus::Success);
        uintptr_t privateView = base + 2 * PageSize4KB;
        assert(parent.mapSharedPages(privateView, PageSize4KB, PAGE_READWRITE, unshared.pfns, 1,
            &unshared, acquire, release, PAGE_READWRITE, false, false) == NtStatus::Success);
        uint32_t old{};
        assert(parent.protect(base + PageSize4KB, PageSize4KB, PAGE_READONLY, &old) == NtStatus::Success);
        window[0] = 0x42;
        target.failPage = 1;
        assert(parent.clonePrivateTo(child, reinterpret_cast<uintptr_t>(&source),
                                     reinterpret_cast<uintptr_t>(&target), switchRoot) == NtStatus::Unsuccessful);
        assert(owner.references == 2 && !target.frames[0] && !target.frames[1] && child.getRegionCount() == 0);
        target.failPage = -1;
        assert(parent.clonePrivateTo(child, reinterpret_cast<uintptr_t>(&source),
                                     reinterpret_cast<uintptr_t>(&target), switchRoot) == NtStatus::Success);
        const auto* copiedView = child.findVad(base);
        assert(owner.references == 3 && window[0] == 0x42 && target.frames[0] == owner.pfns[0] &&
            !target.frames[2] && copiedView && copiedView->sectionOffset == 0x10000 &&
            target.protections[1] == PAGE_READONLY);
        window[0] = 0x43;
        assert(switchRoot(reinterpret_cast<uintptr_t>(&source)) && window[0] == 0x43);
        owner.reenter = &parent;
        assert(parent.unmapSharedPages(base) == NtStatus::Success && owner.references == 2 && owner.reentered);
        owner.reenter = nullptr;
        assert(parent.unmapSharedPages(privateView) == NtStatus::Success && unshared.references == 1);
        release(&unshared);
        release(&owner);
        assert(switchRoot(reinterpret_cast<uintptr_t>(&target)) && window[0] == 0x43);
        assert(child.close() == NtStatus::Success && owner.references == 0);
    }
    assert(db.getActiveCount() == 0);
    std::free(window);
}
