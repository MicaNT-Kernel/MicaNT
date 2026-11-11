#include "micant/km/core/syscalls.hpp"
#include "micant/km/executive/mm.hpp"
#include "micant/km/executive/section.hpp"

#include <array>
#include <cassert>
#include <limits>

namespace micant::sys {
fs::FileObject* LookupKernelFileObject(Handle) { return nullptr; }
}

int main() {
    using namespace micant;
    constexpr Handle process = InvalidHandleValue;
    uintptr_t base = 0x210001234ULL;
    size_t size = 0x1000;
    assert(sys::NtAllocateVirtualMemory(process, &base, 0, &size,
        mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE) == NtStatus::Success);
    assert(base == 0x210001000ULL && size == 0x2000);

    uintptr_t failedBase = base + 1;
    size_t failedSize = 1;
    assert(sys::NtAllocateVirtualMemory(process, &failedBase, 0, &failedSize,
        mm::MEM_RESERVE, mm::PAGE_READWRITE) == NtStatus::Conflict);
    assert(failedBase == base + 1 && failedSize == 1);
    assert(sys::NtAllocateVirtualMemory(process, &failedBase, 1, &failedSize,
        mm::MEM_RESERVE, mm::PAGE_READWRITE) == NtStatus::NotImplemented);
    assert(failedBase == base + 1 && failedSize == 1);
    assert(sys::NtAllocateVirtualMemory(0, &failedBase, 0, &failedSize,
        mm::MEM_RESERVE, mm::PAGE_READWRITE) == NtStatus::InvalidHandle);

    uintptr_t protectBase = base + 1;
    size_t protectSize = 0x1fff;
    uint32_t oldProtection = 0;
    assert(sys::NtProtectVirtualMemory(process, &protectBase, &protectSize,
        mm::PAGE_READONLY, &oldProtection) == NtStatus::Success);
    assert(protectBase == base && protectSize == size && oldProtection == mm::PAGE_READWRITE);
    protectBase = base + 1;
    protectSize = 1;
    oldProtection = 0xdeadbeef;
    assert(sys::NtProtectVirtualMemory(process, &protectBase, &protectSize,
        mm::PAGE_READWRITE, &oldProtection) == NtStatus::NotImplemented);
    assert(protectBase == base + 1 && protectSize == 1 && oldProtection == 0xdeadbeef);

    uintptr_t freeBase = base + 1;
    size_t freeSize = 1;
    assert(sys::NtFreeVirtualMemory(process, &freeBase, &freeSize,
        mm::MEM_DECOMMIT) == NtStatus::NotImplemented);
    assert(freeBase == base + 1 && freeSize == 1);
    freeBase = base;
    freeSize = 1;
    assert(sys::NtFreeVirtualMemory(process, &freeBase, &freeSize,
        mm::MEM_RELEASE) == NtStatus::InvalidParameter);
    assert(freeBase == base && freeSize == 1);
    freeSize = 0;
    assert(sys::NtFreeVirtualMemory(process, &freeBase, &freeSize,
        mm::MEM_RELEASE) == NtStatus::Success);
    assert(freeBase == base && freeSize == size);

    base = std::numeric_limits<uintptr_t>::max() - 3;
    size = 8;
    assert(sys::NtAllocateVirtualMemory(process, &base, 0, &size,
        mm::MEM_RESERVE, mm::PAGE_READWRITE) == NtStatus::InvalidParameter);
    assert(base == std::numeric_limits<uintptr_t>::max() - 3 && size == 8);

    std::array<mm::Mmpfn, 2> metadata{};
    const std::array<mm::PhysicalPageRange, 1> ranges{{{1, 1}}};
    auto& db = mm::PfnDatabase::get();
    assert(db.initializeFromBootMemory(metadata, ranges, [](uint64_t) noexcept {}) == NtStatus::Success);
    Handle sectionHandle = 0x555;
    LargeInteger maximum{};
    maximum.quadPart = 2 * mm::PageSize4KB;
    assert(sys::NtCreateSection(&sectionHandle, 0, nullptr, &maximum,
        mm::PAGE_READWRITE, section::SEC_COMMIT, 0) == NtStatus::NoMemory);
    assert(sectionHandle == 0x555 && db.getActiveCount() == 0);
    db.resetForTesting();
}
