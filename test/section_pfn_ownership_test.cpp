#include "micant/km/executive/section.hpp"

#include <array>
#include <cassert>
#include <memory>

using namespace micant;
using namespace micant::mm;
using namespace micant::section;

namespace {
void zeroPage(uint64_t) noexcept {}
bool mapPage(void*, uintptr_t, uint64_t, uint32_t) noexcept { return true; }
bool unmapPage(void*, uintptr_t) noexcept { return true; }
}

int main() {
    auto& db = PfnDatabase::get();
    std::array<Mmpfn, 3> metadata{};
    const std::array<PhysicalPageRange, 1> ranges{{{1, 2}}};
    assert(db.initializeFromBootMemory(metadata, ranges, zeroPage) == NtStatus::Success);

    std::shared_ptr<CONTROL_AREA> control;
    std::shared_ptr<SEGMENT> segment;
    uint64_t referencedPfn = 0;
    {
        auto section = std::make_shared<SectionObject>(PageSize4KB + 1, SEC_COMMIT, PAGE_READWRITE);
        assert(section->getInitializationStatus() == NtStatus::Success);
        control = section->getControlArea();
        segment = section->getSegment();
        assert(control->sharedPfnMap.size() == 2 && db.getActiveCount() == 2);
        assert(segment->prototypePtes.size() == 2);
        for (const auto& pte : segment->prototypePtes)
            assert(pte.valid && pte.pfn != 0);
        referencedPfn = segment->prototypePtes[0].pfn;
        db.referencePage(referencedPfn);

        char byte = 0;
        assert(section->read(UINT64_MAX, &byte, 2) == NtStatus::InvalidParameter);
        assert(section->write(UINT64_MAX, &byte, 2) == NtStatus::InvalidParameter);
    }
    assert(db.getActiveCount() == 2);
    assert(segment->controlArea == control.get());
    control.reset();
    assert(db.getActiveCount() == 1);
    assert(segment->controlArea == nullptr);
    db.freePage(referencedPfn);
    assert(db.getActiveCount() == 0);
    segment.reset();

    const uint64_t held = db.allocatePage();
    assert(held != 0);
    {
        SectionObject failed(2 * PageSize4KB, SEC_COMMIT, PAGE_READWRITE);
        assert(failed.getInitializationStatus() == NtStatus::NoMemory);
        assert(failed.getControlArea()->sharedPfnMap.empty());
        for (const auto& pte : failed.getSegment()->prototypePtes)
            assert(!pte.valid && pte.pfn == 0);
        assert(db.getActiveCount() == 1);
    }
    auto& manager = SectionManager::get();
    ob::HandleTable handles;
    Handle handle = 0;
    std::shared_ptr<SectionObject> result;
    assert(manager.createSection(handle, handles, 2 * PageSize4KB, SEC_COMMIT,
                                 PAGE_READWRITE, result) == NtStatus::NoMemory);
    assert(!result && db.getActiveCount() == 1);
    assert(manager.createFileBackedSection(handle, handles, nullptr, L"file",
                                           2 * PageSize4KB, SEC_COMMIT,
                                           PAGE_READWRITE, result) == NtStatus::NoMemory);
    assert(!result && db.getActiveCount() == 1);
    {
        auto failed = std::make_shared<SectionObject>(2 * PageSize4KB, SEC_COMMIT, PAGE_READWRITE);
        ps::EProcess process(1, L"failed");
        uintptr_t address = 0;
        size_t length = PageSize4KB;
        assert(manager.mapViewOfSection(failed, process, address, length, 0,
                                        PAGE_READWRITE) == NtStatus::NoMemory);
        assert(address == 0 && length == PageSize4KB);
        assert(process.getAddressSpace().getRegionCount() == 0);
    }
    db.freePage(held);

    {
        ProcessAddressSpace first;
        ProcessAddressSpace second;
        auto section = std::make_shared<SectionObject>(PageSize4KB, SEC_COMMIT, PAGE_READWRITE);
        const uint64_t pfn = section->getSegment()->prototypePtes[0].pfn;
        std::weak_ptr<SectionObject> sectionWeak = section;
        std::weak_ptr<CONTROL_AREA> control = section->getControlArea();
        uintptr_t firstView = 0;
        uintptr_t secondView = 0;
        assert(first.mapSectionView(firstView, PageSize4KB, PAGE_READWRITE,
                                    section->getControlArea()->sharedPfnMap,
                                    section.get(), 0, section) == NtStatus::Success);
        assert(second.mapSectionView(secondView, PageSize4KB, PAGE_READWRITE,
                                     section->getControlArea()->sharedPfnMap,
                                     section.get(), 0, section) == NtStatus::Success);
        {
            auto conflicting = std::make_shared<SectionObject>(PageSize4KB, SEC_COMMIT, PAGE_READWRITE);
            std::weak_ptr<SectionObject> conflictingWeak = conflicting;
            uintptr_t occupied = firstView;
            assert(first.mapSectionView(occupied, PageSize4KB, PAGE_READWRITE,
                                        conflicting->getControlArea()->sharedPfnMap,
                                        conflicting.get(), 0, conflicting) == NtStatus::Conflict);
            conflicting.reset();
            assert(conflictingWeak.expired());
        }
        assert(first.findVad(firstView)->sectionContext == section.get());
        assert(db.getActiveCount() == 1);
        assert(metadata[pfn].referenceCount == 3);
        section.reset();
        assert(!sectionWeak.expired() && !control.expired());
        assert(db.getActiveCount() == 1);
        assert(metadata[pfn].referenceCount == 3);
        assert(first.free(firstView, 0, MEM_RELEASE) == NtStatus::Success);
        assert(!sectionWeak.expired() && !control.expired());
        assert(db.getActiveCount() == 1);
        assert(metadata[pfn].referenceCount == 2);
        assert(second.free(secondView, 0, MEM_RELEASE) == NtStatus::Success);
        assert(sectionWeak.expired() && control.expired());
        assert(db.getActiveCount() == 0);
        assert(metadata[pfn].referenceCount == 0);
    }

    {
        ps::EProcess process(1, L"view-lifetime");
        auto section = std::make_shared<SectionObject>(PageSize4KB, SEC_COMMIT, PAGE_READWRITE);
        const uint64_t pfn = section->getSegment()->prototypePtes[0].pfn;
        std::weak_ptr<SectionObject> sectionWeak = section;
        std::weak_ptr<CONTROL_AREA> control = section->getControlArea();
        uintptr_t address = 0;
        size_t length = PageSize4KB;
        assert(manager.mapViewOfSection(section, process, address, length, 0,
                                        PAGE_READWRITE) == NtStatus::Success);
        assert(section->getMappedViewCount() == 1 && metadata[pfn].referenceCount == 2);
        section.reset();
        assert(!sectionWeak.expired() && !control.expired());
        assert(metadata[pfn].referenceCount == 2);
        {
            auto owner = sectionWeak.lock();
            assert(owner && process.getAddressSpace().findVad(address)->sectionContext == owner.get());
        }
        assert(manager.unmapViewOfSection(process, address) == NtStatus::Success);
        assert(sectionWeak.expired() && control.expired());
        assert(metadata[pfn].referenceCount == 0);
    }

    {
        auto section = std::make_shared<SectionObject>(PageSize4KB, SEC_COMMIT, PAGE_READWRITE);
        ps::EProcess process(1, L"backend");
        auto& space = process.getAddressSpace();
        assert(space.configurePagingBackend({nullptr, mapPage, unmapPage},
                                            0x100000000ULL, 2 * PageSize4KB));
        uintptr_t address = 0;
        size_t length = PageSize4KB;
        assert(manager.mapViewOfSection(section, process, address, length, 0,
                                        PAGE_READWRITE) == NtStatus::NotImplemented);
        assert(address == 0 && length == PageSize4KB);
        assert(space.getRegionCount() == 0 && section->getMappedViewCount() == 0);
        std::weak_ptr<SectionObject> sectionWeak = section;
        std::weak_ptr<CONTROL_AREA> control = section->getControlArea();
        section.reset();
        assert(sectionWeak.expired() && control.expired());
    }
    assert(db.getActiveCount() == 0);
    manager.resetForTesting();
    db.resetForTesting();
}
