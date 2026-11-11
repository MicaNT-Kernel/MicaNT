#include "micant/km/executive/mm.hpp"
#include "micant/km/executive/section.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"

namespace {
struct FailingSectionMap { size_t successfulMaps{}; bool blockUnmap{}; };
bool mapUntilSecond(void* context, uintptr_t address, uint64_t pfn, uint32_t protection) noexcept {
    auto& state = *static_cast<FailingSectionMap*>(context);
    if (state.successfulMaps == 1) return false;
    if (!micant::hal::amd64::mapBootstrapManagedPage(nullptr, address, pfn, protection)) return false;
    ++state.successfulMaps;
    return true;
}
bool unmapSectionPage(void* context, uintptr_t address) noexcept {
    if (static_cast<FailingSectionMap*>(context)->blockUnmap) return false;
    return micant::hal::amd64::unmapBootstrapManagedPage(nullptr, address);
}
}

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
            nullptr, mapBootstrapManagedPage, unmapBootstrapManagedPage,
            mapBootstrapProtectManagedPage
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
        uintptr_t first = BootstrapManagedBase;
        uintptr_t second = first + PageSize4KB;
        {
            section::SectionObject section(PageSize4KB, section::SEC_COMMIT, PAGE_READWRITE);
            if (section.getInitializationStatus() != NtStatus::Success) return 11;
            const auto backing = section.getBackingReference();
            if (space.mapSharedPages(first, PageSize4KB, PAGE_READWRITE, backing.pfns,
                                     backing.pageCount, backing.context, backing.acquire, backing.release) != NtStatus::Success ||
                space.mapSharedPages(second, PageSize4KB, PAGE_READONLY, backing.pfns,
                                     backing.pageCount, backing.context, backing.acquire, backing.release,
                                     PAGE_READONLY) != NtStatus::Success) return 12;
        }
        auto* writable = reinterpret_cast<volatile uint64_t*>(first);
        auto* readonly = reinterpret_cast<volatile uint64_t*>(second);
        *writable = 0xabcdef1234567890ULL;
        if (*readonly != *writable || bootstrapUserRange(second, PageSize4KB, true) ||
            !bootstrapUserRange(first, PageSize4KB, true) ||
            (bootstrapQemuFaultTestsAvailable() && !verifyBootstrapManagedPageFault(second, true, true))) return 13;
        uint32_t oldProtection{};
        if (space.protect(second, PageSize4KB, PAGE_READWRITE, &oldProtection) != NtStatus::SectionProtection ||
            bootstrapUserRange(second, PageSize4KB, true) ||
            space.protect(first, PageSize4KB, PAGE_READONLY, &oldProtection) != NtStatus::Success ||
            oldProtection != PAGE_READWRITE || bootstrapUserRange(first, PageSize4KB, true) ||
            space.protect(first, PageSize4KB, PAGE_READWRITE, &oldProtection) != NtStatus::Success ||
            oldProtection != PAGE_READONLY || !bootstrapUserRange(first, PageSize4KB, true)) return 15;
        if (space.unmapSharedPages(first + 1) != NtStatus::Success ||
            *readonly != 0xabcdef1234567890ULL ||
            space.unmapSharedPages(second) != NtStatus::Success) return 14;
    }
    {
        ProcessAddressSpace space;
        FailingSectionMap fault{};
        if (!space.configurePagingBackend({&fault, mapUntilSecond, unmapSectionPage},
                                          BootstrapManagedBase, BootstrapManagedSize)) return 16;
        section::SectionObject section(2 * PageSize4KB, section::SEC_COMMIT, PAGE_READWRITE);
        if (section.getInitializationStatus() != NtStatus::Success) return 17;
        const auto backing = section.getBackingReference();
        uintptr_t address = BootstrapManagedBase;
        if (space.mapSharedPages(address, 2 * PageSize4KB, PAGE_READWRITE,
                                 backing.pfns, backing.pageCount, backing.context,
                                 backing.acquire, backing.release) != NtStatus::Unsuccessful ||
            fault.successfulMaps != 1 || space.getRegionCount() != 0 ||
            bootstrapUserRange(address, PageSize4KB, false)) return 18;
        fault.successfulMaps = 0;
        fault.blockUnmap = true;
        address = 0;
        if (space.mapSharedPages(address, 2 * PageSize4KB, PAGE_READWRITE,
                                 backing.pfns, backing.pageCount, backing.context,
                                 backing.acquire, backing.release) != NtStatus::Unsuccessful ||
            address != 0 || space.getRegionCount() != 1 ||
            !bootstrapUserRange(BootstrapManagedBase, PageSize4KB, true)) return 19;
        fault.blockUnmap = false;
        fault.successfulMaps = 0;
        if (space.mapSharedPages(address, PageSize4KB, PAGE_READWRITE,
                                 backing.pfns, backing.pageCount, backing.context,
                                 backing.acquire, backing.release) != NtStatus::Success ||
            address != BootstrapManagedBase || space.getRegionCount() != 1 ||
            space.unmapSharedPages(address) != NtStatus::Success) return 20;
    }

    return database.getActiveCount() == active &&
           database.getFreeCount() + database.getZeroedCount() +
               database.getStandbyCount() == available ? 0 : 9;
}

extern "C" int MicantTestMmManagedProtection() noexcept {
    using namespace micant;
    using namespace micant::mm;
    using namespace micant::hal::amd64;

    auto& database = PfnDatabase::get();
    const size_t active = database.getActiveCount();
    const size_t available = database.getFreeCount() + database.getZeroedCount() +
                             database.getStandbyCount();
    const bool faultTests = bootstrapQemuFaultTestsAvailable();
    for (unsigned iteration = 0; iteration < 4; ++iteration) {
        ProcessAddressSpace space;
        const ProcessAddressSpace::PagingBackend backend{
            nullptr, mapBootstrapManagedPage, unmapBootstrapManagedPage,
            mapBootstrapProtectManagedPage
        };
        if (!space.configurePagingBackend(backend, BootstrapManagedBase, BootstrapManagedSize))
            return 1;
        uintptr_t base = BootstrapManagedBase;
        if (space.allocate(base, 128 * 1024, MEM_RESERVE, PAGE_READWRITE) != NtStatus::Success ||
            space.getCommitCharge() != 0) return 2;
        uintptr_t committed = base + PageSize4KB;
        if (space.allocate(committed, PageSize4KB, MEM_COMMIT, PAGE_READWRITE) != NtStatus::Success ||
            space.getCommitCharge() != 1 ||
            bootstrapUserRange(committed + PageSize4KB, PageSize4KB, false)) return 3;
        auto* memory = reinterpret_cast<volatile uint64_t*>(committed);
        if (*memory != 0) return 4;
        *memory = 0x123456789abcdef0ULL;
        const uint64_t firstPhysical = translateBootstrapAddress(reinterpret_cast<void*>(committed));
        if (!firstPhysical || *memory != 0x123456789abcdef0ULL ||
            (faultTests && !verifyBootstrapManagedPageFault(committed + PageSize4KB, false, false)))
            return 5;
        uintptr_t secondAddress = committed + PageSize4KB;
        if (space.allocate(secondAddress, PageSize4KB, MEM_COMMIT, PAGE_READWRITE) != NtStatus::Success ||
            space.getCommitCharge() != 2) return 3;
        auto* second = reinterpret_cast<volatile uint64_t*>(committed + PageSize4KB);
        if (*second != 0) return 4;
        *second = 0xfedcba9876543210ULL;
        const uint64_t secondPhysical = translateBootstrapAddress(
            reinterpret_cast<void*>(committed + PageSize4KB));
        if (!secondPhysical ||
            translateBootstrapAddress(reinterpret_cast<void*>(committed)) != firstPhysical ||
            *memory != 0x123456789abcdef0ULL || *second != 0xfedcba9876543210ULL ||
            bootstrapUserRange(base, PageSize4KB, false)) return 5;
        if (faultTests && !verifyBootstrapManagedPageFault(base, false, false)) return 6;

        uint32_t oldProtect{};
        if (space.protect(committed, PageSize4KB, PAGE_READONLY, &oldProtect) != NtStatus::Success ||
            oldProtect != PAGE_READWRITE || !bootstrapUserRange(committed, PageSize4KB, false) ||
            bootstrapUserRange(committed, PageSize4KB, true) ||
            !bootstrapUserRange(committed + PageSize4KB, PageSize4KB, true) ||
            translateBootstrapAddress(reinterpret_cast<void*>(committed)) != firstPhysical ||
            *memory != 0x123456789abcdef0ULL) return 7;
        if (faultTests && !verifyBootstrapManagedPageFault(committed, true, true)) return 8;
        if (*memory != 0x123456789abcdef0ULL ||
            space.protect(committed, PageSize4KB, PAGE_READWRITE, &oldProtect) != NtStatus::Success ||
            oldProtect != PAGE_READONLY ||
            translateBootstrapAddress(reinterpret_cast<void*>(committed)) != firstPhysical) return 9;
        *memory = 0x1111222233334444ULL;
        if (*memory != 0x1111222233334444ULL ||
            space.free(committed, PageSize4KB, MEM_DECOMMIT) != NtStatus::Success ||
            space.getCommitCharge() != 1 ||
            bootstrapUserRange(committed, PageSize4KB, false) ||
            translateBootstrapAddress(reinterpret_cast<void*>(committed + PageSize4KB)) != secondPhysical ||
            *second != 0xfedcba9876543210ULL) return 10;
        if (faultTests && !verifyBootstrapManagedPageFault(committed, false, false)) return 11;
        if (space.allocate(committed, PageSize4KB, MEM_COMMIT, PAGE_READWRITE) != NtStatus::Success ||
            *memory != 0 || space.getCommitCharge() != 2 ||
            space.free(base, 0, MEM_RELEASE) != NtStatus::Success ||
            space.getRegionCount() != 0 || space.getCommitCharge() != 0 ||
            space.close() != NtStatus::Success) return 12;
        if (database.getActiveCount() != active ||
            database.getFreeCount() + database.getZeroedCount() + database.getStandbyCount() != available)
            return 13;
    }
    return 0;
}
