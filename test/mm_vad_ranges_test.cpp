#include "micant/km/executive/section.hpp"

#include <cassert>
#include <limits>
#include <memory>

using namespace micant;
using namespace micant::mm;

static std::unique_ptr<VirtualAddressDescriptor> descriptor(uintptr_t start, uintptr_t end) {
    auto vad = std::make_unique<VirtualAddressDescriptor>();
    vad->startingAddress = start;
    vad->endingAddress = end;
    return vad;
}

int main() {
    constexpr size_t page = PageSize4KB;
    constexpr uintptr_t min = ProcessAddressSpace::UserSpaceMin;
    constexpr uintptr_t max = ProcessAddressSpace::UserSpaceMax;
    constexpr uintptr_t hint = 0x100000000ULL;
    const std::unordered_map<size_t, uint64_t> noPages;

    MmAvlTable tree;
    assert(tree.insert(descriptor(0x20000, 0x21fff)));
    assert(tree.insert(descriptor(0x26000, 0x26fff)));
    assert(!tree.insert(descriptor(0x1f000, 0x20fff)));
    assert(!tree.insert(descriptor(0x21000, 0x22fff)));
    assert(!tree.insert(descriptor(0x1f000, 0x27fff)));
    assert(tree.size() == 2);
    assert(tree.findFreeRange(page, page, false, 0x10000, 0x10fff) == 0x10000);
    assert(tree.findFreeRange(page, page, true, 0x10000, 0x10fff) == 0x10000);
    assert(tree.findFreeRange(2 * page, page, false, 0x21000, 0x25fff) == 0x22000);
    assert(tree.findFreeRange(2 * page, page, true, 0x21000, 0x25fff) == 0x24000);
    assert(tree.findFreeRange(page, page, false, 0x30000, 0x30fff) == 0x30000);
    assert(tree.findFreeRange(page, page, false, 0x20000, 0x21fff) == 0);
    assert(tree.findFreeRange(2 * page, page, true, 0x10000, 0x10fff) == 0);
    assert(tree.findFreeRange(page, 6000, false, min, max) == 0);
    assert(tree.findFreeRange(std::numeric_limits<size_t>::max(), page, false, min, max) == 0);
    assert(tree.findFreeRange(page, page, false, max, min) == 0);
    assert(tree.findFreeRange(page, page, true,
                              std::numeric_limits<uintptr_t>::max() - page + 1,
                              std::numeric_limits<uintptr_t>::max()) ==
           std::numeric_limits<uintptr_t>::max() - page + 1);

    ProcessAddressSpace space;
    uintptr_t fixed = hint + 2 * page;
    assert(space.allocate(fixed, 2 * page, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success);
    const auto* original = space.findVad(fixed);
    assert(original && !original->committed);
    for (uintptr_t base : {hint + page, hint + 3 * page, hint}) {
        uintptr_t request = base;
        size_t length = base == hint ? 5 * page : 2 * page;
        assert(space.allocate(request, length, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Conflict);
        assert(request == base && space.getRegionCount() == 1 && space.getCommitCharge() == 0);
        assert(space.findVad(fixed) == original);
        assert(space.mapSectionView(request, length, PAGE_READWRITE, noPages) == NtStatus::Conflict);
        assert(request == base && space.getRegionCount() == 1 && space.getCommitCharge() == 0);
    }

    uintptr_t commit = fixed;
    assert(space.allocate(commit, page, MEM_COMMIT, PAGE_READWRITE) == NtStatus::NotImplemented);
    assert(commit == fixed && !original->committed && space.getRegionCount() == 1);
    uintptr_t automatic = 0;
    assert(space.mapSectionView(automatic, 3 * page, PAGE_READWRITE, noPages) == NtStatus::Success);
    assert(automatic >= fixed + 2 * page && space.getRegionCount() == 2);
    assert(space.findVad(automatic)->isSectionView);
    uintptr_t before = hint + page;
    assert(space.allocate(before, 2 * page, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) == NtStatus::Conflict);
    uintptr_t adjacent = hint + page;
    assert(space.allocate(adjacent, page, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success);

    uintptr_t last = max - page + 1;
    assert(space.allocate(last, page, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success);
    uintptr_t lastOverlap = last;
    assert(space.mapSectionView(lastOverlap, page, PAGE_READWRITE, noPages) == NtStatus::Conflict);
    uintptr_t overflow = last;
    assert(space.allocate(overflow, 2 * page, MEM_RESERVE, PAGE_READWRITE) == NtStatus::InvalidParameter);
    assert(space.mapSectionView(overflow, 2 * page, PAGE_READWRITE, noPages) == NtStatus::InvalidParameter);
    uintptr_t below = min - page;
    assert(space.allocate(below, page, MEM_RESERVE, PAGE_READWRITE) == NtStatus::InvalidParameter);
    assert(space.mapSectionView(below, page, PAGE_READWRITE, noPages) == NtStatus::InvalidParameter);
    uintptr_t zero = 0;
    assert(space.allocate(zero, std::numeric_limits<size_t>::max(), MEM_RESERVE, PAGE_READWRITE) == NtStatus::InvalidParameter);
    assert(space.mapSectionView(zero, std::numeric_limits<size_t>::max(), PAGE_READWRITE, noPages) == NtStatus::InvalidParameter);
    assert(zero == 0);

    ProcessAddressSpace top;
    uintptr_t high = 0;
    assert(top.allocate(high, page, MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE) == NtStatus::Success);
    assert(high == max - page + 1);
    uintptr_t high2 = 0;
    assert(top.allocate(high2, page, MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE) == NtStatus::Success);
    assert(high2 == high - page);

    ProcessAddressSpace reverse;
    uintptr_t mapped = hint;
    assert(reverse.mapSectionView(mapped, 2 * page, PAGE_READWRITE, noPages) == NtStatus::Success);
    uintptr_t reserve = hint - page;
    assert(reverse.allocate(reserve, 2 * page, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Conflict);
    uintptr_t autoReserve = 0;
    assert(reverse.allocate(autoReserve, page, MEM_COMMIT, PAGE_READWRITE) == NtStatus::Success);
    assert(autoReserve == hint + 2 * page);

    ProcessAddressSpace deletion;
    uintptr_t middle = hint + page, left = hint, right = hint + 2 * page;
    assert(deletion.allocate(middle, page, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success);
    assert(deletion.allocate(left, page, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success);
    assert(deletion.mapSectionView(right, page, PAGE_READWRITE, noPages,
                                   &deletion, page) == NtStatus::Success);
    assert(deletion.free(middle, 0, MEM_RELEASE) == NtStatus::Success);
    assert(deletion.findVad(right)->isSectionView);
    assert(deletion.findVad(right)->sectionContext == &deletion);
    assert(deletion.findVad(right)->sectionOffset == page);

    ps::EProcess process(1, L"test");
    auto section = std::make_shared<section::SectionObject>(2 * page, section::SEC_COMMIT, PAGE_READWRITE);
    uintptr_t occupied = hint;
    assert(process.getAddressSpace().allocate(occupied, page, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success);
    uintptr_t view = hint - page;
    size_t viewSize = 2 * page;
    assert(section::SectionManager::get().mapViewOfSection(section, process, view, viewSize,
                                                            MEM_COMMIT, PAGE_READWRITE) == NtStatus::Conflict);
    assert(view == hint - page && viewSize == 2 * page && process.getAddressSpace().getRegionCount() == 1);
    assert(section->getMappedViewCount() == 0 && process.getAddressSpace().getResidentPages() == 0);
    viewSize = 0;
    assert(section::SectionManager::get().mapViewOfSection(section, process, view, viewSize,
                                                            MEM_COMMIT, PAGE_READWRITE, 2 * page + 1) == NtStatus::InvalidParameter);
    assert(viewSize == 0);
}
