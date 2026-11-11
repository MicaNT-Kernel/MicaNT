#include "micant/km/executive/mm.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

using namespace micant::mm;
using micant::NtStatus;

namespace {
constexpr uintptr_t Base = (128ULL << 39) + 0x20000000;
constexpr size_t Pages = 64;
Mmpfn entries[Pages + 1];
unsigned char physical[Pages + 1][PageSize4KB];

void zero(uint64_t pfn) noexcept {
    for (auto& byte : physical[pfn]) byte = 0;
}

struct Mapper {
    uint64_t frames[Pages]{};
    uint32_t protection[Pages]{};
    int failMap{-1};
    int failUnmap{-1};
    int failProtect{-1};
    int failProtectRollback{-1};
    size_t maps{0};
    size_t unmaps{0};
};

bool map(void* context, uintptr_t va, uint64_t pfn, uint32_t protect) noexcept {
    auto& m = *static_cast<Mapper*>(context);
    if (va < Base || va >= Base + Pages * PageSize4KB || (va & PageMask4KB)) return false;
    size_t index = (va - Base) / PageSize4KB;
    if (static_cast<int>(index) == m.failMap || m.frames[index]) return false;
    for (auto byte : physical[pfn]) if (byte != 0) return false;
    m.frames[index] = pfn;
    m.protection[index] = protect;
    ++m.maps;
    return true;
}

bool unmap(void* context, uintptr_t va) noexcept {
    auto& m = *static_cast<Mapper*>(context);
    size_t index = (va - Base) / PageSize4KB;
    if (index >= Pages || static_cast<int>(index) == m.failUnmap || !m.frames[index]) return false;
    m.frames[index] = 0;
    ++m.unmaps;
    return true;
}

bool protect(void* context, uintptr_t va, uint32_t protection) noexcept {
    auto& m = *static_cast<Mapper*>(context);
    if (va < Base || va >= Base + Pages * PageSize4KB || (va & PageMask4KB)) return false;
    const size_t index = (va - Base) / PageSize4KB;
    if (static_cast<int>(index) == m.failProtect) {
        m.failProtect = m.failProtectRollback;
        m.failProtectRollback = -1;
        return false;
    }
    if (!m.frames[index]) return false;
    m.protection[index] = protection;
    return true;
}
}

#if defined(MICANT_FREESTANDING)
extern "C"
#endif
int main() {
    for (auto& page : physical) for (auto& byte : page) byte = 0xa5;
    auto& db = PfnDatabase::get();
    PhysicalPageRange range{1, Pages};
    assert(db.initializeFromBootMemory(entries, {&range, 1}, zero) == NtStatus::Success);
    Mapper mapper;
    const ProcessAddressSpace::PagingBackend backend{&mapper, map, unmap, protect};
    {
        ProcessAddressSpace space;
        assert(!space.configurePagingBackend(backend, Base + 1, Pages * PageSize4KB));
        assert(space.configurePagingBackend(backend, Base, Pages * PageSize4KB));
        assert(!space.configurePagingBackend(backend, Base, Pages * PageSize4KB));
        uintptr_t automatic = 0;
        uintptr_t secondAutomatic = 0;
        assert(space.allocate(automatic, PageSize4KB, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success);
        assert(space.allocate(secondAutomatic, PageSize4KB, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success);
        assert(automatic == Base && secondAutomatic == Base + ProcessAddressSpace::AllocationGranularity);
        assert(space.free(automatic, 0, MEM_RELEASE) == NtStatus::Success);
        assert(space.free(secondAutomatic, 0, MEM_RELEASE) == NtStatus::Success);
        uintptr_t native = Base + 3 * PageSize4KB + 7;
        assert(space.allocate(native, PageSize4KB, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success);
        assert(native == Base + 3 * PageSize4KB);
        assert(space.findVad(native)->endingAddress == Base + 5 * PageSize4KB - 1);
        assert(space.free(native, 0, MEM_RELEASE) == NtStatus::Success);

        uintptr_t bad = Base;
        assert(space.allocate(bad, 0, MEM_RESERVE, PAGE_READWRITE) == NtStatus::InvalidParameter);
        assert(space.allocate(bad, std::numeric_limits<size_t>::max(), MEM_RESERVE, PAGE_READWRITE) == NtStatus::InvalidParameter);
        assert(space.allocate(bad, PageSize4KB, MEM_RESERVE, PAGE_EXECUTE_READ) == NtStatus::NotImplemented);
        uintptr_t outside = Base + Pages * PageSize4KB;
        assert(space.allocate(outside, PageSize4KB, MEM_RESERVE, PAGE_READWRITE) == NtStatus::InvalidParameter);

        uintptr_t base = 0;
        assert(space.allocate(base, 4 * PageSize4KB, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success);
        assert(base == Base && space.getRegionCount() == 1 && db.getActiveCount() == 0);
        uintptr_t overlap = Base + PageSize4KB;
        assert(space.allocate(overlap, PageSize4KB, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Conflict);
        uintptr_t partial = Base + PageSize4KB + 7;
        assert(space.allocate(partial, PageSize4KB, MEM_COMMIT, PAGE_READONLY) == NtStatus::Success);
        assert(partial == Base + PageSize4KB && mapper.maps == 2);
        assert(mapper.protection[1] == PAGE_READONLY && mapper.protection[2] == PAGE_READONLY);
        assert(space.getRegionCount() == 1 && space.getCommitCharge() == 2);
        auto firstPfn = mapper.frames[1];
        uint32_t oldProtect = 0;
        assert(space.protect(Base + PageSize4KB + 1, PageSize4KB, PAGE_READWRITE, &oldProtect)
               == NtStatus::Success);
        assert(oldProtect == PAGE_READONLY && mapper.protection[1] == PAGE_READWRITE &&
               mapper.protection[2] == PAGE_READWRITE);
        mapper.failProtect = 1;
        oldProtect = 123;
        assert(space.protect(Base + PageSize4KB, 2 * PageSize4KB, PAGE_READONLY, &oldProtect)
               == NtStatus::Unsuccessful);
        assert(oldProtect == 123 && mapper.protection[1] == PAGE_READWRITE &&
               mapper.protection[2] == PAGE_READWRITE);
        mapper.failProtect = 1;
        mapper.failProtectRollback = 2;
        assert(space.protect(Base + PageSize4KB, 2 * PageSize4KB, PAGE_READONLY, &oldProtect)
               == NtStatus::Unsuccessful);
        assert(oldProtect == 123 && mapper.protection[1] == PAGE_READWRITE &&
               mapper.protection[2] == PAGE_READONLY);
        mapper.failProtect = -1;
        assert(space.protect(Base + 2 * PageSize4KB, PageSize4KB, PAGE_READWRITE, &oldProtect)
               == NtStatus::Success);
        assert(oldProtect == PAGE_READONLY && mapper.protection[2] == PAGE_READWRITE);
        oldProtect = 123;
        assert(space.protect(Base, PageSize4KB, PAGE_READONLY, &oldProtect) == NtStatus::Conflict);
        assert(oldProtect == 123);
        assert(space.protect(Base + PageSize4KB, std::numeric_limits<size_t>::max(),
                             PAGE_READONLY, &oldProtect) == NtStatus::InvalidParameter);
        assert(space.allocate(partial, PageSize4KB, MEM_COMMIT, PAGE_READWRITE) == NtStatus::Success);
        assert(mapper.frames[1] == firstPfn && mapper.maps == 2 && space.getCommitCharge() == 2);
        mapper.failMap = 3;
        uintptr_t extension = base;
        assert(space.allocate(extension, 4 * PageSize4KB, MEM_COMMIT, PAGE_READWRITE) == NtStatus::Unsuccessful);
        assert(mapper.frames[0] == 0 && mapper.frames[1] == firstPfn && mapper.frames[2] != 0);
        assert(db.getActiveCount() == 2 && space.getCommitCharge() == 2);
        mapper.failMap = -1;
        uintptr_t crosses = Base + 3 * PageSize4KB;
        assert(space.allocate(crosses, 2 * PageSize4KB, MEM_COMMIT, PAGE_READWRITE) == NtStatus::Conflict);

        assert(space.free(base + PageSize4KB, 0, MEM_RELEASE) == NtStatus::InvalidParameter);
        assert(space.free(base, PageSize4KB, MEM_RELEASE) == NtStatus::InvalidParameter);
        assert(space.free(base + PageSize4KB + 7, 1, MEM_DECOMMIT) == NtStatus::Success);
        assert(mapper.frames[1] == 0 && mapper.frames[2] != 0 && space.getCommitCharge() == 1);
        assert(space.free(base + PageSize4KB, PageSize4KB, MEM_DECOMMIT) == NtStatus::Success);
        assert(space.free(base + PageSize4KB, 0, MEM_DECOMMIT) == NtStatus::InvalidParameter);
        assert(space.free(base + 3 * PageSize4KB, 2 * PageSize4KB, MEM_DECOMMIT) == NtStatus::Conflict);
        partial = base + PageSize4KB;
        assert(space.allocate(partial, PageSize4KB, MEM_COMMIT, PAGE_READWRITE) == NtStatus::Success);
        firstPfn = mapper.frames[1];
        mapper.failUnmap = 1;
        assert(space.free(base, 0, MEM_RELEASE) == NtStatus::Unsuccessful);
        assert(mapper.frames[1] == firstPfn && db.getActiveCount() == 1 && space.getCommitCharge() == 1);
        mapper.failUnmap = -1;
        assert(space.free(base, 0, MEM_RELEASE) == NtStatus::Success);
        assert(space.getRegionCount() == 0 && db.getActiveCount() == 0);

        mapper.failMap = 2;
        uintptr_t failed = Base;
        assert(space.allocate(failed, 4 * PageSize4KB, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) == NtStatus::Unsuccessful);
        assert(space.getRegionCount() == 0 && db.getActiveCount() == 0);
        mapper.failMap = -1;

        uintptr_t held = 0;
        mapper.failMap = 2;
        mapper.failUnmap = 1;
        assert(space.allocate(held, 4 * PageSize4KB, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) == NtStatus::Unsuccessful);
        assert(held == 0 && space.getRegionCount() == 1 && mapper.frames[1] && db.getActiveCount() == 1);
        assert(space.getCommitCharge() == 1);
        mapper.failMap = mapper.failUnmap = -1;
        assert(space.close() == NtStatus::Success);
        assert(db.getActiveCount() == 0 && space.getRegionCount() == 0);

        uintptr_t larger = Base;
        assert(space.allocate(larger, Pages * PageSize4KB, MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success);
        auto exhausted = db.allocatePage();
        assert(exhausted);
        assert(space.allocate(larger, Pages * PageSize4KB, MEM_COMMIT, PAGE_READWRITE) == NtStatus::NoMemory);
        assert(db.getActiveCount() == 1 && space.getCommitCharge() == 0);
        db.freePage(exhausted);
        assert(space.free(larger, 0, MEM_RELEASE) == NtStatus::Success);

        uintptr_t teardown = 0;
        assert(space.allocate(teardown, PageSize4KB, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) == NtStatus::Success);
        assert(db.getActiveCount() == 1);
        mapper.failUnmap = 0;
        assert(space.close() == NtStatus::Unsuccessful);
        assert(space.getRegionCount() == 1 && mapper.frames[0] != 0 && db.getActiveCount() == 1);
        mapper.failUnmap = -1;
        assert(space.close() == NtStatus::Success);
        assert(space.close() == NtStatus::Success);
        uintptr_t all = Base;
        assert(space.allocate(all, 3 * PageSize4KB, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == NtStatus::Success);
        assert(space.free(all, 0, MEM_DECOMMIT) == NtStatus::Success);
        assert(space.getRegionCount() == 1 && space.getCommitCharge() == 0 && db.getActiveCount() == 0);
        assert(space.free(all, 0, MEM_RELEASE) == NtStatus::Success);
    }
    assert(db.getActiveCount() == 0 && mapper.frames[0] == 0);

    return 0;
}
