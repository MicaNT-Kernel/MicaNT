#include "micant/km/executive/section.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

using namespace micant;
using namespace micant::mm;
using namespace micant::section;

namespace {
std::array<Mmpfn, 3> metadata{};
std::array<std::array<uint8_t, PageSize4KB>, 3> pages{};
size_t zeroCount = 0;

void zeroPage(uint64_t pfn) noexcept {
    pages[pfn].fill(0);
    ++zeroCount;
}
}

int main() {
    auto& db = PfnDatabase::get();
    const std::array<PhysicalPageRange, 1> available{{{1, 2}}};
    for (auto& page : pages) page.fill(0xa5);
    assert(db.initializeFromBootMemory(metadata, available, zeroPage) == NtStatus::Success);

    for (uint32_t protect : {PAGE_READONLY, PAGE_READWRITE}) {
        {
            SectionObject section(2 * PageSize4KB, SEC_COMMIT, protect);
            assert(section.getInitializationStatus() == NtStatus::Success);
            assert(db.getActiveCount() == 2);
            assert(section.getData() == nullptr);
            for (const auto& pte : section.getSegment()->prototypePtes) {
                assert(pte.valid && pte.pfn != 0 && pte.protection == protect);
                for (uint8_t byte : pages[pte.pfn]) assert(byte == 0);
                pages[pte.pfn].fill(0x5a);
            }
            char byte = 0;
            assert(section.loadData(&byte, 1) == NtStatus::NotImplemented);
            assert(section.read(0, &byte, 1) == NtStatus::NotImplemented);
            assert(section.write(0, &byte, 1) == NtStatus::NotImplemented);
            SECTION_VIEW view{};
            view.baseAddress = 0x10000;
            section.addView(view);
            assert(section.getMappedViewCount() == 1);
            assert(section.removeView(nullptr, view.baseAddress));
            assert(section.getMappedViewCount() == 0);
        }
        assert(db.getActiveCount() == 0);
    }
    assert(zeroCount == 4);

    const uint64_t held = db.allocatePage();
    assert(held != 0);
    {
        SectionObject failed(2 * PageSize4KB, SEC_COMMIT, PAGE_READWRITE);
        assert(failed.getInitializationStatus() == NtStatus::NoMemory);
        assert(failed.getControlArea()->sharedPfnMap.empty());
        for (const auto& pte : failed.getSegment()->prototypePtes) assert(!pte.valid);
        assert(db.getActiveCount() == 1);
    }
    db.freePage(held);
    assert(db.getActiveCount() == 0);

    {
        SectionObject image(PageSize4KB, SEC_COMMIT | SEC_IMAGE, PAGE_READONLY);
        SectionObject reserve(PageSize4KB, SEC_RESERVE, PAGE_READWRITE);
        SectionObject cow(PageSize4KB, SEC_COMMIT, PAGE_WRITECOPY);
        SectionObject file(PageSize4KB, SEC_COMMIT, PAGE_READWRITE, &db);
        SectionObject empty(0, SEC_COMMIT, PAGE_READWRITE);
        assert(image.getInitializationStatus() == NtStatus::NotImplemented);
        assert(reserve.getInitializationStatus() == NtStatus::NotImplemented);
        assert(cow.getInitializationStatus() == NtStatus::NotImplemented);
        assert(file.getInitializationStatus() == NtStatus::NotImplemented);
        assert(empty.getInitializationStatus() == NtStatus::InvalidParameter);
        assert(db.getActiveCount() == 0);
    }
}
