#include "micant/km/executive/mm.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"

extern "C" int MicantTestMmProcessAddressSpace() noexcept {
    using namespace micant;
    using namespace micant::mm;
    using namespace micant::hal::amd64;

    auto& database = PfnDatabase::get();
    const size_t active = database.getActiveCount();
    const size_t available = database.getFreeCount() + database.getZeroedCount() +
                             database.getStandbyCount();
    {
        ProcessAddressSpace space;
        const ProcessAddressSpace::PagingBackend backend{
            nullptr, mapBootstrapManagedPage, unmapBootstrapManagedPage
        };
        if (!space.configurePagingBackend(backend, BootstrapManagedBase, BootstrapManagedSize))
            return 1;
        uintptr_t base = BootstrapManagedBase;
        if (space.allocate(base, 3 * PageSize4KB, MEM_RESERVE, PAGE_READWRITE) != NtStatus::Success ||
            base != BootstrapManagedBase || space.getRegionCount() != 1 ||
            space.getCommitCharge() != 0 ||
            bootstrapUserRange(base, PageSize4KB, false)) return 2;
        uintptr_t overlap = base + PageSize4KB;
        if (space.allocate(overlap, PageSize4KB, MEM_RESERVE, PAGE_READWRITE) != NtStatus::Conflict)
            return 3;
        uintptr_t committed = base + PageSize4KB;
        if (space.allocate(committed, PageSize4KB, MEM_COMMIT, PAGE_READWRITE) != NtStatus::Success ||
            committed != base + PageSize4KB || space.getCommitCharge() != 1 ||
            !bootstrapUserRange(committed, PageSize4KB, true) ||
            bootstrapUserRange(base, PageSize4KB, false)) return 4;
        auto* memory = reinterpret_cast<volatile uint64_t*>(committed);
        if (memory[0] != 0 || memory[PageSize4KB / sizeof(uint64_t) - 1] != 0) return 5;
        memory[0] = 0x123456789abcdef0ULL;
        if (memory[0] != 0x123456789abcdef0ULL ||
            space.allocate(committed, PageSize4KB, MEM_COMMIT, PAGE_READWRITE) != NtStatus::Success ||
            memory[0] != 0x123456789abcdef0ULL || space.getCommitCharge() != 1) return 6;
        if (space.free(committed, 0, MEM_RELEASE) != NtStatus::InvalidParameter ||
            space.free(base, PageSize4KB, MEM_RELEASE) != NtStatus::InvalidParameter ||
            space.free(base, 0, MEM_RELEASE) != NtStatus::Success ||
            space.getRegionCount() != 0 || space.getCommitCharge() != 0 ||
            bootstrapUserRange(committed, PageSize4KB, false)) return 7;
        if (space.allocate(base, PageSize4KB, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) != NtStatus::Success ||
            !bootstrapUserRange(base, PageSize4KB, true) ||
            space.free(base, 0, MEM_RELEASE) != NtStatus::Success) return 8;
        if (space.allocate(base, PageSize4KB, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY) != NtStatus::Success ||
            !bootstrapUserRange(base, PageSize4KB, false) ||
            bootstrapUserRange(base, PageSize4KB, true) ||
            space.free(base, 0, MEM_RELEASE) != NtStatus::Success) return 10;
    }
    return database.getActiveCount() == active &&
           database.getFreeCount() + database.getZeroedCount() +
               database.getStandbyCount() == available ? 0 : 9;
}
