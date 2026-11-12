#pragma once

#include "micant/km/core/ke.hpp"
#include "micant/shared/boot_driver.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::km::arb {
// Probe must restore all PCI state on every failure and serialize the whole
// config transaction against all other HAL config accessors.
using ProbeBar = NtStatus (*)(uint16_t, uint16_t, uint16_t, uint8_t,
                              boot_driver::PciBarResourceV1&) noexcept;
struct Claim {
    uint64_t token{};
    const void* owner{};
    uint16_t bus{}, device{}, function{};
    uint8_t count{};
    uint32_t mappings{};
    uint32_t dmaAllocations{};
    boot_driver::PciBarResourceV1 bars[6]{};
};
inline ke::SpinLock claimLock{};
inline Claim claims[32]{};
inline uint64_t nextToken{1};
struct ReservedRange { uint64_t first{}, last{}; };
inline ReservedRange reserved[1025]{};
inline size_t reservedCount{};
// A live device whose BAR cannot be sized without changing its decoding makes
// the firmware inventory incomplete. In that case PCI claims must fail closed.
inline bool pciInventoryComplete{};
struct FirmwareBar {
    uint64_t address{};
    uint64_t length{};
    uint16_t bus{}, device{}, function{};
    uint8_t index{};
    bool ioSpace{};
    bool decodingActive{};
};
inline FirmwareBar firmwareBars[1024]{};
inline size_t firmwareBarCount{};

// Used during single-BSP boot enumeration before publishing the inventory.
// An address without a verified length cannot establish non-overlap.
inline bool recordUnmeasuredBar(const FirmwareBar& bar) noexcept {
    ke::SpinLockGuard guard(claimLock);
    pciInventoryComplete = false;
    if (firmwareBarCount == 1024) return false;
    firmwareBars[firmwareBarCount++] = bar;
    return true;
}

inline bool recordMeasuredBar(const FirmwareBar& bar) noexcept {
    if (!bar.length || bar.address > UINT64_MAX - (bar.length - 1)) return false;
    ke::SpinLockGuard guard(claimLock);
    if (firmwareBarCount == 1024) return false;
    firmwareBars[firmwareBarCount++] = bar;
    return true;
}

inline void setPciInventoryComplete(bool complete) noexcept {
    ke::SpinLockGuard guard(claimLock);
    pciInventoryComplete = complete;
}

// Firmware and PnP register occupied physical ranges with the root arbiter
// before drivers are started. Ranges remain reserved for system lifetime.
inline bool reservePhysicalRange(uint64_t start, uint64_t length) noexcept {
    if (!length || start > UINT64_MAX - (length - 1)) return false;
    ke::SpinLockGuard guard(claimLock);
    if (reservedCount == 1025) return false;
    const auto end = start + length - 1;
    for (size_t i = 0; i < firmwareBarCount; ++i) {
        const auto& bar = firmwareBars[i];
        if (bar.ioSpace) continue;
        if (!bar.length || (start <= bar.address + bar.length - 1 && bar.address <= end))
            return false;
    }
    for (const auto& existing : claims) {
        if (!existing.token) continue;
        for (size_t i = 0; i < existing.count; ++i) {
            const auto& bar = existing.bars[i];
            if (start <= bar.PhysicalAddress + bar.Length - 1 && bar.PhysicalAddress <= end)
                return false;
        }
    }
    reserved[reservedCount++] = {start, end};
    return true;
}

inline NtStatus claim(const void* owner, uint16_t segment, uint16_t bus, uint16_t device,
                      uint16_t function, const boot_driver::PciBarRequestV1* requests,
                      size_t count, uint64_t* token,
                      boot_driver::PciBarResourceV1* output, size_t* capacity,
                      ProbeBar probe) noexcept {
    if (!owner || !token || !capacity || !probe || !count || count > 6 || !requests || !output ||
        segment || bus > 255 || device >= 32 || function >= 8)
        return NtStatus::InvalidParameter;
    const size_t available = *capacity;
    *token = 0;
    *capacity = 0;
    if (available < count) { *capacity = count; return NtStatus::BufferTooSmall; }
    for (size_t i = 0; i < count; ++i) {
        if (requests[i].BarIndex >= 6 || requests[i].Flags || requests[i].Reserved ||
            !requests[i].MinimumLength) return NtStatus::InvalidParameter;
        for (size_t j = 0; j < i; ++j)
            if (requests[i].BarIndex == requests[j].BarIndex) return NtStatus::InvalidParameter;
    }
    ke::SpinLockGuard guard(claimLock);
    if (!pciInventoryComplete) return NtStatus::DeviceNotReady;
    Claim* freeClaim = nullptr;
    for (auto& existing : claims) {
        if (!existing.token) { if (!freeClaim) freeClaim = &existing; continue; }
        if (existing.bus == bus && existing.device == device && existing.function == function)
            return NtStatus::DeviceBusy;
    }
    if (!freeClaim || !nextToken) return NtStatus::InsufficientResources;
    Claim candidate{};
    candidate.owner = owner;
    candidate.bus = bus; candidate.device = device; candidate.function = function;
    candidate.count = static_cast<uint8_t>(count);
    for (size_t i = 0; i < count; ++i) {
        auto& bar = candidate.bars[i];
        const auto status = probe(bus, device, function, requests[i].BarIndex, bar);
        if (status != NtStatus::Success) return status;
        if (!bar.PhysicalAddress || bar.Length < requests[i].MinimumLength ||
            bar.PhysicalAddress > UINT64_MAX - (bar.Length - 1)) return NtStatus::DeviceNotReady;
        for (size_t j = 0; j < reservedCount; ++j)
            if (bar.PhysicalAddress <= reserved[j].last &&
                reserved[j].first <= bar.PhysicalAddress + bar.Length - 1)
                return NtStatus::DeviceBusy;
        for (size_t j = 0; j < firmwareBarCount; ++j) {
            const auto& other = firmwareBars[j];
            if (other.ioSpace || (other.bus == bus && other.device == device &&
                                  other.function == function && other.index == bar.BarIndex)) continue;
            if (!other.length || (bar.PhysicalAddress <= other.address + other.length - 1 &&
                                  other.address <= bar.PhysicalAddress + bar.Length - 1))
                return NtStatus::DeviceBusy;
        }
        for (size_t j = 0; j < i; ++j) {
            const auto& other = candidate.bars[j];
            if (bar.PhysicalAddress <= other.PhysicalAddress + other.Length - 1 &&
                other.PhysicalAddress <= bar.PhysicalAddress + bar.Length - 1)
                return NtStatus::DeviceBusy;
        }
        for (const auto& existing : claims) {
            if (!existing.token) continue;
            for (size_t j = 0; j < existing.count; ++j) {
                const auto& other = existing.bars[j];
                if (bar.PhysicalAddress <= other.PhysicalAddress + other.Length - 1 &&
                    other.PhysicalAddress <= bar.PhysicalAddress + bar.Length - 1)
                    return NtStatus::DeviceBusy;
            }
        }
    }
    candidate.token = nextToken++;
    *freeClaim = candidate;
    for (size_t i = 0; i < count; ++i) output[i] = candidate.bars[i];
    *capacity = count;
    *token = candidate.token;
    return NtStatus::Success;
}

inline bool ownsDevice(const void* owner, uint16_t bus, uint16_t device, uint16_t function) noexcept {
    ke::SpinLockGuard guard(claimLock);
    for (const auto& claim : claims)
        if (claim.token && claim.owner == owner && claim.bus == bus &&
            claim.device == device && claim.function == function) return true;
    return false;
}
inline bool hasDma(const void* owner, uint16_t bus, uint16_t device, uint16_t function) noexcept {
    ke::SpinLockGuard guard(claimLock);
    for (const auto& claim : claims)
        if (claim.token && claim.owner == owner && claim.bus == bus && claim.device == device &&
            claim.function == function) return claim.dmaAllocations != 0;
    return false;
}

[[nodiscard]] inline uint64_t retainMapping(const void* owner, uint64_t address, size_t length) noexcept {
    if (!owner || !length || address > UINT64_MAX - (length - 1)) return 0;
    ke::SpinLockGuard guard(claimLock);
    for (auto& claim : claims) {
        if (!claim.token || claim.owner != owner) continue;
        for (size_t i = 0; i < claim.count; ++i) {
            const auto& bar = claim.bars[i];
            if (address >= bar.PhysicalAddress && address + length - 1 <=
                bar.PhysicalAddress + bar.Length - 1) {
                if (claim.mappings == UINT32_MAX) return 0;
                ++claim.mappings;
                return claim.token;
            }
        }
    }
    return 0;
}

inline bool releaseMapping(const void* owner, uint64_t token) noexcept {
    ke::SpinLockGuard guard(claimLock);
    for (auto& claim : claims) if (claim.token == token && claim.owner == owner) {
        if (!claim.mappings) return false;
        --claim.mappings;
        return true;
    }
    return false;
}

inline bool retainDma(const void* owner, uint64_t token) noexcept {
    ke::SpinLockGuard guard(claimLock);
    for (auto& claim : claims) if (claim.token == token && claim.owner == owner) {
        if (claim.dmaAllocations == UINT32_MAX) return false;
        ++claim.dmaAllocations;
        return true;
    }
    return false;
}
inline bool releaseDma(const void* owner, uint64_t token) noexcept {
    ke::SpinLockGuard guard(claimLock);
    for (auto& claim : claims) if (claim.token == token && claim.owner == owner) {
        if (!claim.dmaAllocations) return false;
        --claim.dmaAllocations;
        return true;
    }
    return false;
}

using ReleaseCheck = bool (*)(uint16_t, uint16_t, uint16_t) noexcept;
inline bool dmaStopped(const void* owner, uint64_t token, ReleaseCheck check) noexcept {
    ke::SpinLockGuard guard(claimLock);
    for (const auto& claim : claims) if (claim.token == token && claim.owner == owner)
        return check && check(claim.bus, claim.device, claim.function);
    return false;
}
inline NtStatus release(const void* owner, uint64_t token, ReleaseCheck check) noexcept {
    if (!owner || !token) return NtStatus::InvalidParameter;
    ke::SpinLockGuard guard(claimLock);
    for (auto& claim : claims) if (claim.token == token && claim.owner == owner) {
        if (claim.mappings || claim.dmaAllocations || !check || !check(claim.bus, claim.device, claim.function))
            return NtStatus::DeviceBusy;
        claim = {};
        return NtStatus::Success;
    }
    return NtStatus::InvalidHandle;
}
}
