#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/executive/mm.hpp"

namespace micant::ex {

// ============================================================================
// Executive Synchronization & Buffer Probing Subsystem Telemetry
// ============================================================================
struct EX_SYNC_TELEMETRY {
    uint64_t totalSharedAcquisitions{0};
    uint64_t totalExclusiveAcquisitions{0};
    uint64_t totalResourceConversions{0};
    uint64_t totalResourceTryAcquireFailures{0};
    uint64_t totalPushLocksAcquiredExclusive{0};
    uint64_t totalPushLocksAcquiredShared{0};
    uint64_t totalPushLockConversions{0};
    uint64_t totalFastMutexAcquisitions{0};
    uint64_t totalGuardedMutexAcquisitions{0};
    uint64_t totalProbesValidated{0};
    uint64_t totalProbeViolationsCaught{0};
    uint64_t totalSafeCopiesCompleted{0};
    uint64_t totalSafeCopyFaultsCaught{0};
    uint64_t totalLookasideAllocations{0};
    uint64_t totalLookasideHits{0};
    uint64_t totalLookasideFrees{0};
    uint64_t totalLookasideDynamicAdjustments{0};
};

inline EX_SYNC_TELEMETRY& GetSyncTelemetry() noexcept {
    static EX_SYNC_TELEMETRY telemetry{};
    return telemetry;
}

inline EX_SYNC_TELEMETRY ExQuerySyncTelemetry() noexcept {
    return GetSyncTelemetry();
}

inline void ExResetSyncTelemetryForTesting() noexcept {
    GetSyncTelemetry() = EX_SYNC_TELEMETRY{};
}

} // namespace micant::ex

namespace micant::mm {

// Standard AMD64 Canonical User-Mode Address Limits & Boundaries
inline constexpr uintptr_t MM_HIGHEST_USER_ADDRESS   = 0x00007FFFFFFFFFFFULL;
inline constexpr uintptr_t MM_USER_PROBE_ADDRESS      = 0x00007FFFFFF00000ULL;
inline constexpr uintptr_t MM_LOWEST_USER_ADDRESS     = 0x0000000000010000ULL; // 64KB Null Allocation Trap
inline constexpr size_t    PAGE_SIZE                  = 4096;
inline constexpr uintptr_t PAGE_MASK                  = 0xFFF;

/**
 * @brief Memory page permission tracker for synthetic probe tests and page boundary verification.
 */
struct PROBE_PAGE_ENTRY {
    uintptr_t pageBase{0};
    bool isWritable{true};
    bool isPresent{true};
};

class ProbeMemoryManager {
public:
    static ProbeMemoryManager& get() noexcept {
        static ProbeMemoryManager instance;
        return instance;
    }

    void setPagePermission(uintptr_t pageBase, bool isPresent, bool isWritable) {
        pageBase &= ~PAGE_MASK;
        pages_[pageBase] = {pageBase, isWritable, isPresent};
    }

    void clearPermissions() {
        pages_.clear();
    }

    [[nodiscard]] bool isPageWritable(uintptr_t pageBase) const {
        pageBase &= ~PAGE_MASK;
        auto it = pages_.find(pageBase);
        if (it != pages_.end()) {
            return it->second.isPresent && it->second.isWritable;
        }
        return true;
    }

    [[nodiscard]] bool isPagePresent(uintptr_t pageBase) const {
        pageBase &= ~PAGE_MASK;
        auto it = pages_.find(pageBase);
        if (it != pages_.end()) {
            return it->second.isPresent;
        }
        return true;
    }

private:
    ProbeMemoryManager() = default;
    std::unordered_map<uintptr_t, PROBE_PAGE_ENTRY> pages_;
};

/**
 * @brief Validates that a buffer is within permissible address space boundaries,
 * not overflowing, and properly aligned.
 */
inline NtStatus ProbeForRead(
    const void* address,
    size_t length,
    uint32_t alignment,
    KPROCESSOR_MODE mode = UserMode
) noexcept {
    if (length == 0) return NtStatus::Success;
    if (!address) {
        ex::GetSyncTelemetry().totalProbeViolationsCaught++;
        return NtStatus::AccessViolation;
    }

    uintptr_t addr = reinterpret_cast<uintptr_t>(address);

    // Alignment check (alignment must be a power of 2)
    if (alignment > 1 && (addr & (alignment - 1)) != 0) {
        ex::GetSyncTelemetry().totalProbeViolationsCaught++;
        return NtStatus::DatatypeMisalignment;
    }

    // Range overflow check
    if (addr + length < addr) {
        ex::GetSyncTelemetry().totalProbeViolationsCaught++;
        return NtStatus::AccessViolation;
    }

    // Kernel-mode requests bypass user-space ceiling and null-trap boundaries
    if (mode == KernelMode) {
        ex::GetSyncTelemetry().totalProbesValidated++;
        return NtStatus::Success;
    }

    // Null page / 64KB trap boundary check
    if (addr < MM_LOWEST_USER_ADDRESS) {
        ex::GetSyncTelemetry().totalProbeViolationsCaught++;
        return NtStatus::AccessViolation;
    }

    // Must be strictly within user-mode address space
    if (addr + length > MM_HIGHEST_USER_ADDRESS) {
        ex::GetSyncTelemetry().totalProbeViolationsCaught++;
        return NtStatus::AccessViolation;
    }

    ex::GetSyncTelemetry().totalProbesValidated++;
    return NtStatus::Success;
}

/**
 * @brief Validates that a user-mode buffer is within user space, aligned, and writable
 * across all spanned page boundaries.
 */
inline NtStatus ProbeForWrite(
    void* address,
    size_t length,
    uint32_t alignment,
    KPROCESSOR_MODE mode = UserMode
) noexcept {
    NtStatus status = ProbeForRead(address, length, alignment, mode);
    if (status != NtStatus::Success) {
        return status;
    }
    if (length == 0) return NtStatus::Success;

    uintptr_t addr = reinterpret_cast<uintptr_t>(address);
    uintptr_t startPage = addr & ~PAGE_MASK;
    uintptr_t endPage = (addr + length - 1) & ~PAGE_MASK;

    for (uintptr_t page = startPage; page <= endPage; page += PAGE_SIZE) {
        if (!ProbeMemoryManager::get().isPageWritable(page) || !ProbeMemoryManager::get().isPagePresent(page)) {
            ex::GetSyncTelemetry().totalProbeViolationsCaught++;
            return NtStatus::AccessViolation;
        }
    }

    return NtStatus::Success;
}

/**
 * @brief Safely copies memory from user space into a kernel-mode buffer with full probe validation.
 */
inline NtStatus ExSafeCopyFromUser(
    void* kernelDest,
    const void* userSrc,
    size_t length,
    KPROCESSOR_MODE mode = UserMode
) noexcept {
    if (length == 0) return NtStatus::Success;
    if (!kernelDest) return NtStatus::InvalidParameter;

    NtStatus st = ProbeForRead(userSrc, length, 1, mode);
    if (st != NtStatus::Success) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return st;
    }

    try {
        std::memcpy(kernelDest, userSrc, length);
        ex::GetSyncTelemetry().totalSafeCopiesCompleted++;
        return NtStatus::Success;
    } catch (...) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return NtStatus::AccessViolation;
    }
}

/**
 * @brief Safely copies memory from kernel space into a user-mode buffer with full probe validation.
 */
inline NtStatus ExSafeCopyToUser(
    void* userDest,
    const void* kernelSrc,
    size_t length,
    KPROCESSOR_MODE mode = UserMode
) noexcept {
    if (length == 0) return NtStatus::Success;
    if (!kernelSrc) return NtStatus::InvalidParameter;

    NtStatus st = ProbeForWrite(userDest, length, 1, mode);
    if (st != NtStatus::Success) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return st;
    }

    try {
        std::memcpy(userDest, kernelSrc, length);
        ex::GetSyncTelemetry().totalSafeCopiesCompleted++;
        return NtStatus::Success;
    } catch (...) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return NtStatus::AccessViolation;
    }
}

/**
 * @brief Safely probes and reads a scalar or POD struct from user space.
 */
template <typename T>
inline NtStatus ExSafeProbeAndRead(
    const T* userPtr,
    T& outValue,
    KPROCESSOR_MODE mode = UserMode
) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    if (!userPtr) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return NtStatus::AccessViolation;
    }

    NtStatus st = ProbeForRead(userPtr, sizeof(T), alignof(T), mode);
    if (st != NtStatus::Success) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return st;
    }

    outValue = *userPtr;
    ex::GetSyncTelemetry().totalSafeCopiesCompleted++;
    return NtStatus::Success;
}

/**
 * @brief Safely probes and writes a scalar or POD struct to user space.
 */
template <typename T>
inline NtStatus ExSafeProbeAndWrite(
    T* userPtr,
    const T& inValue,
    KPROCESSOR_MODE mode = UserMode
) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    if (!userPtr) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return NtStatus::AccessViolation;
    }

    NtStatus st = ProbeForWrite(userPtr, sizeof(T), alignof(T), mode);
    if (st != NtStatus::Success) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return st;
    }

    *userPtr = inValue;
    ex::GetSyncTelemetry().totalSafeCopiesCompleted++;
    return NtStatus::Success;
}

/**
 * @brief Safely validates and extracts a UNICODE_STRING from user space into a std::wstring.
 */
inline NtStatus ExSafeReadUnicodeString(
    const UnicodeString* userString,
    std::wstring& outWString,
    size_t maxChars = 1024,
    KPROCESSOR_MODE mode = UserMode
) noexcept {
    if (!userString) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return NtStatus::AccessViolation;
    }

    NtStatus st = ProbeForRead(userString, sizeof(UnicodeString), alignof(UnicodeString), mode);
    if (st != NtStatus::Success) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return st;
    }

    uint16_t length = userString->length;
    uint16_t maxLen = userString->maximumLength;
    if (length > maxLen) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return NtStatus::InvalidParameter;
    }
    if (length > maxChars * sizeof(wchar_t)) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return NtStatus::BufferOverflow;
    }

    if (length == 0) {
        outWString.clear();
        ex::GetSyncTelemetry().totalSafeCopiesCompleted++;
        return NtStatus::Success;
    }

    if (!userString->buffer) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return NtStatus::AccessViolation;
    }

    st = ProbeForRead(userString->buffer, length, alignof(wchar_t), mode);
    if (st != NtStatus::Success) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return st;
    }

    size_t charCount = length / sizeof(wchar_t);
    outWString.assign(userString->buffer, charCount);
    ex::GetSyncTelemetry().totalSafeCopiesCompleted++;
    return NtStatus::Success;
}

/**
 * @brief Safely validates and extracts an ANSI_STRING from user space into a std::string.
 */
inline NtStatus ExSafeReadAnsiString(
    const AnsiString* userString,
    std::string& outString,
    size_t maxChars = 2048,
    KPROCESSOR_MODE mode = UserMode
) noexcept {
    if (!userString) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return NtStatus::AccessViolation;
    }

    NtStatus st = ProbeForRead(userString, sizeof(AnsiString), alignof(AnsiString), mode);
    if (st != NtStatus::Success) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return st;
    }

    uint16_t length = userString->length;
    uint16_t maxLen = userString->maximumLength;
    if (length > maxLen) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return NtStatus::InvalidParameter;
    }
    if (length > maxChars) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return NtStatus::BufferOverflow;
    }

    if (length == 0) {
        outString.clear();
        ex::GetSyncTelemetry().totalSafeCopiesCompleted++;
        return NtStatus::Success;
    }

    if (!userString->buffer) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return NtStatus::AccessViolation;
    }

    st = ProbeForRead(userString->buffer, length, 1, mode);
    if (st != NtStatus::Success) {
        ex::GetSyncTelemetry().totalSafeCopyFaultsCaught++;
        return st;
    }

    outString.assign(userString->buffer, length);
    ex::GetSyncTelemetry().totalSafeCopiesCompleted++;
    return NtStatus::Success;
}

} // namespace micant::mm

namespace micant::ex {
    using mm::ProbeForRead;
    using mm::ProbeForWrite;
    using mm::ExSafeCopyFromUser;
    using mm::ExSafeCopyToUser;
    using mm::ExSafeProbeAndRead;
    using mm::ExSafeProbeAndWrite;
    using mm::ExSafeReadUnicodeString;
    using mm::ExSafeReadAnsiString;
    using mm::ProbeMemoryManager;
} // namespace micant::ex
