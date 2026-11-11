#include "micant/km/executive/mm.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

#if !defined(_WIN32)
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if !defined(_WIN32) && defined(MICANT_FREESTANDING)
void onTrap(int) {
    _exit(79);
}
#endif

using namespace micant::mm;
using micant::NtStatus;

namespace {
constexpr uintptr_t Base = (128ULL << 39) + 0x20000000;
constexpr size_t Pages = 16;
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
    size_t maps{0};
    size_t unmaps{0};
};

#if !defined(_WIN32) && !defined(MICANT_FREESTANDING)
Mapper* terminatingMapper = nullptr;

void onTerminate() noexcept {
    const bool intact = terminatingMapper && terminatingMapper->frames[0] != 0 &&
        PfnDatabase::get().getActiveCount() == 1;
    _exit(intact ? 77 : 78);
}
#endif

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
    const ProcessAddressSpace::PagingBackend backend{&mapper, map, unmap};
    {
        ProcessAddressSpace space;
        assert(!space.configurePagingBackend(backend, Base + 1, Pages * PageSize4KB));
        assert(space.configurePagingBackend(backend, Base, Pages * PageSize4KB));
        assert(!space.configurePagingBackend(backend, Base, Pages * PageSize4KB));

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

        uintptr_t held = Base;
        mapper.failMap = 2;
        mapper.failUnmap = 1;
        assert(space.allocate(held, 4 * PageSize4KB, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) == NtStatus::Unsuccessful);
        assert(space.getRegionCount() == 1 && mapper.frames[1] && db.getActiveCount() == 1);
        assert(space.getCommitCharge() == 1);
        mapper.failMap = mapper.failUnmap = -1;
        assert(space.free(held, 0, MEM_RELEASE) == NtStatus::Success);
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
    }
    assert(db.getActiveCount() == 0 && mapper.frames[0] == 0);

#if !defined(_WIN32)
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        const rlimit noCore{0, 0};
        if (setrlimit(RLIMIT_CORE, &noCore) != 0) _exit(90);
        Mapper failing;
#if !defined(MICANT_FREESTANDING)
        terminatingMapper = &failing;
        std::set_terminate(onTerminate);
#else
        std::signal(SIGILL, onTrap);
        std::signal(SIGTRAP, onTrap);
#endif
        {
            ProcessAddressSpace space;
            if (!space.configurePagingBackend({&failing, map, unmap}, Base, Pages * PageSize4KB))
                _exit(91);
            uintptr_t address = Base;
            if (space.allocate(address, PageSize4KB, MEM_COMMIT | MEM_RESERVE,
                               PAGE_READWRITE) != NtStatus::Success) _exit(92);
            failing.failUnmap = 0;
        }
        _exit(93);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
#if defined(MICANT_FREESTANDING)
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 79);
#else
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 77);
#endif
#endif
    return 0;
}
