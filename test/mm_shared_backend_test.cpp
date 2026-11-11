#include "micant/km/executive/section.hpp"
#include <cassert>

using namespace micant;
using namespace micant::mm;
using namespace micant::section;
namespace {
constexpr uintptr_t Base = 0x100000000ULL;
Mmpfn metadata[9];
struct Mapper { uint64_t frames[8]{}; int fail{-1}; int failUnmap{-1}; };
void zero(uint64_t) noexcept {}
bool map(void* context, uintptr_t va, uint64_t pfn, uint32_t) noexcept {
    auto& m = *static_cast<Mapper*>(context);
    auto i = (va - Base) / PageSize4KB;
    if (i >= 8 || static_cast<int>(i) == m.fail || m.frames[i]) return false;
    m.frames[i] = pfn;
    return true;
}
bool unmap(void* context, uintptr_t va) noexcept {
    auto& m = *static_cast<Mapper*>(context);
    auto i = (va - Base) / PageSize4KB;
    if (i >= 8 || static_cast<int>(i) == m.failUnmap || !m.frames[i]) return false;
    m.frames[i] = 0;
    return true;
}
}
int main() {
    auto& db = PfnDatabase::get();
    PhysicalPageRange range{1, 8};
    assert(db.initializeFromBootMemory(metadata, {&range, 1}, zero) == NtStatus::Success);
    Mapper mapper;
    ProcessAddressSpace space;
    assert(space.configurePagingBackend({&mapper, map, unmap}, Base, 8 * PageSize4KB));
    uintptr_t a = Base, b = Base + 2 * PageSize4KB;
    {
        SectionObject section(2 * PageSize4KB, SEC_COMMIT, PAGE_READWRITE);
        assert(section.getInitializationStatus() == NtStatus::Success);
        auto ref = section.getBackingReference();
        assert(space.mapSharedPages(a, 2 * PageSize4KB, PAGE_READWRITE, ref.pfns, ref.pageCount,
                                    ref.context, ref.acquire, ref.release) == NtStatus::Success);
        assert(space.mapSharedPages(b, 2 * PageSize4KB, PAGE_READONLY, ref.pfns, ref.pageCount,
                                    ref.context, ref.acquire, ref.release) == NtStatus::Success);
        assert(mapper.frames[0] == mapper.frames[2] && mapper.frames[1] == mapper.frames[3]);
        uintptr_t fail = Base + 4 * PageSize4KB;
        mapper.fail = 5;
        assert(space.mapSharedPages(fail, 2 * PageSize4KB, PAGE_READWRITE, ref.pfns, ref.pageCount,
                                    ref.context, ref.acquire, ref.release) == NtStatus::Unsuccessful);
        assert(mapper.frames[4] == 0 && space.getRegionCount() == 2);
        mapper.failUnmap = 4;
        assert(space.mapSharedPages(fail, 2 * PageSize4KB, PAGE_READWRITE, ref.pfns, ref.pageCount,
                                    ref.context, ref.acquire, ref.release) == NtStatus::Unsuccessful);
        assert(mapper.frames[4] && space.getRegionCount() == 3);
        mapper.failUnmap = -1;
        assert(space.unmapSharedPages(fail) == NtStatus::Success);
    }
    assert(db.getActiveCount() == 2);
    assert(space.free(a, 0, MEM_RELEASE) == NtStatus::Conflict);
    assert(space.unmapSharedPages(a + 1) == NtStatus::Success);
    assert(mapper.frames[2] && db.getActiveCount() == 2);
    assert(space.unmapSharedPages(b) == NtStatus::Success);
    assert(db.getActiveCount() == 0 && space.getRegionCount() == 0);
    db.resetForTesting();
}
