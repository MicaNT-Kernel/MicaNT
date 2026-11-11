#include "micant/km/executive/probe.hpp"
#include <cassert>
#include <cstdio>

namespace {

alignas(4096) unsigned char userMemory[8192]{};
unsigned char kernelMemory[128]{};

#if defined(MICANT_FREESTANDING)
bool present[2]{true, true};
bool writable[2]{true, true};
bool kernelWritable{true};
bool stableMappings{true};

bool fits(uintptr_t address, size_t size, const void* base, size_t capacity) noexcept {
    const uintptr_t start = reinterpret_cast<uintptr_t>(base);
    return size && address >= start && address - start < capacity &&
           size <= capacity - (address - start);
}

bool probe(uintptr_t address, size_t size, bool write,
           micant::mm::KPROCESSOR_MODE mode) noexcept {
    if (!stableMappings) return false;
    if (mode == micant::mm::KernelMode) {
        return fits(address, size, kernelMemory, sizeof(kernelMemory)) &&
               (!write || kernelWritable);
    }
    if (mode != micant::mm::UserMode ||
        !fits(address, size, userMemory, sizeof(userMemory))) return false;
    const size_t first = (address - reinterpret_cast<uintptr_t>(userMemory)) / 4096;
    const size_t last = (address + size - 1 -
                         reinterpret_cast<uintptr_t>(userMemory)) / 4096;
    for (size_t page = first; page <= last; ++page) {
        if (!present[page] || (write && !writable[page])) return false;
    }
    return true;
}
#endif

} // namespace

int main() {
    using namespace micant;
    using namespace micant::mm;
    ex::ExResetSyncTelemetryForTesting();
    assert(ProbeForRead(nullptr, 0, 0) == NtStatus::Success);
    assert(ProbeForWrite(nullptr, 0, 3) == NtStatus::Success);
    assert(ExSafeCopyFromUser(nullptr, nullptr, 0) == NtStatus::Success);
    assert(ExSafeCopyToUser(nullptr, nullptr, 0) == NtStatus::Success);

#if defined(MICANT_FREESTANDING)
    assert(ProbeForRead(userMemory, 1, 1) == NtStatus::AccessViolation);
    assert(!detail::initializeBootstrapProbes(nullptr));
    assert(detail::initializeBootstrapProbes(probe));
    assert(!detail::initializeBootstrapProbes(probe));
    assert(ProbeForRead(userMemory, 1, 0) == NtStatus::InvalidParameter);
    assert(ProbeForRead(userMemory, 1, 3) == NtStatus::InvalidParameter);
    assert(ProbeForRead(userMemory, 1, 1, 2) == NtStatus::AccessViolation);
    assert(ProbeForRead(kernelMemory, 1, 1) == NtStatus::AccessViolation);
#endif

    assert(ProbeForRead(nullptr, 1, 1) == NtStatus::AccessViolation);
    assert(ProbeForRead(userMemory + 1, 4, 4) == NtStatus::DatatypeMisalignment);
    assert(ProbeForRead(reinterpret_cast<void*>(UINTPTR_MAX - 3), 8, 1) ==
           NtStatus::AccessViolation);
    assert(ProbeForRead(reinterpret_cast<void*>(0xFFFF800000000000ULL), 1, 1) ==
           NtStatus::AccessViolation);
    assert(ProbeForRead(userMemory, sizeof(userMemory), 16) == NtStatus::Success);
    assert(ProbeForWrite(userMemory, sizeof(userMemory), 16) == NtStatus::Success);

    for (size_t index = 0; index < sizeof(kernelMemory); ++index) {
        userMemory[index] = static_cast<unsigned char>(index ^ 0xA5);
    }
    assert(ExSafeCopyFromUser(kernelMemory, userMemory, sizeof(kernelMemory)) ==
           NtStatus::Success);
    assert(std::memcmp(kernelMemory, userMemory, sizeof(kernelMemory)) == 0);
    assert(ExSafeCopyToUser(userMemory + 4096, kernelMemory, sizeof(kernelMemory)) ==
           NtStatus::Success);
    assert(std::memcmp(kernelMemory, userMemory + 4096, sizeof(kernelMemory)) == 0);

#if defined(MICANT_FREESTANDING)
    writable[1] = false;
    assert(ProbeForRead(userMemory + 4096, 1, 1) == NtStatus::Success);
    assert(ProbeForWrite(userMemory + 4095, 2, 1) == NtStatus::AccessViolation);
    const auto firstByte = userMemory[4095];
    const auto secondByte = userMemory[4096];
    assert(ExSafeCopyToUser(userMemory + 4095, kernelMemory, 2) ==
           NtStatus::AccessViolation);
    assert(userMemory[4095] == firstByte && userMemory[4096] == secondByte);
    writable[1] = true;
    present[1] = false;
    assert(ProbeForRead(userMemory + 4095, 2, 1) == NtStatus::AccessViolation);
    std::memset(kernelMemory, 0x6B, sizeof(kernelMemory));
    assert(ExSafeCopyFromUser(kernelMemory, userMemory + 4095, 2) ==
           NtStatus::AccessViolation);
    for (auto byte : kernelMemory) assert(byte == 0x6B);
    present[1] = true;

    kernelWritable = false;
    assert(ExSafeCopyFromUser(kernelMemory, userMemory, 1) == NtStatus::AccessViolation);
    kernelWritable = true;
    assert(ExSafeCopyToUser(userMemory, userMemory + 1, 1) == NtStatus::AccessViolation);
    assert(ExSafeCopyFromUser(userMemory + 1, userMemory, 1) == NtStatus::AccessViolation);
    stableMappings = false;
    assert(ExSafeCopyFromUser(kernelMemory, userMemory, 1) == NtStatus::AccessViolation);
    assert(ExSafeCopyToUser(userMemory, kernelMemory, 1) == NtStatus::AccessViolation);
    stableMappings = true;
    assert(ProbeForRead(userMemory + sizeof(userMemory) - 1, 2, 1) ==
           NtStatus::AccessViolation);
#else
    ProbeMemoryManager::get().setPagePermission(
        reinterpret_cast<uintptr_t>(userMemory + 4096), true, false);
    assert(ProbeForWrite(userMemory + 4095, 2, 1) == NtStatus::AccessViolation);
    ProbeMemoryManager::get().clearPermissions();
    assert(ProbeForWrite(userMemory + 4095, 2, 1) == NtStatus::Success);
#endif

    assert(ex::ExQuerySyncTelemetry().totalSafeCopiesCompleted == 2);
    assert(ex::ExQuerySyncTelemetry().totalProbeViolationsCaught > 0);
    std::puts("Bootstrap probe tests passed");
}
