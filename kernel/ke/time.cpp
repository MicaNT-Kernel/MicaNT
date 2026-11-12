#include "micant/km/core/ke.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/executive/probe.hpp"

namespace micant::ke {

bool KeQuerySystemTime100ns(uint64_t& systemTime100ns) noexcept {
#if defined(MICANT_BAREMETAL) && (defined(_M_X64) || defined(__x86_64__))
    return hal::amd64::queryBootstrapSystemTime(systemTime100ns) ==
        hal::amd64::BootstrapClockStatus::Success;
#else
    (void)systemTime100ns;
    return false;
#endif
}

bool KeQueryUptime100ns(uint64_t& uptime100ns) noexcept {
#if defined(MICANT_BAREMETAL) && (defined(_M_X64) || defined(__x86_64__))
    return hal::amd64::queryBootstrapUptime(uptime100ns) ==
        hal::amd64::BootstrapClockStatus::Success;
#else
    (void)uptime100ns;
    return false;
#endif
}

bool KeQueryPerformanceCounterValue(uint64_t& counter,
                                    uint64_t& frequency) noexcept {
#if defined(MICANT_BAREMETAL) && (defined(_M_X64) || defined(__x86_64__))
    return hal::amd64::queryBootstrapPerformanceCounter(counter, frequency) ==
        hal::amd64::BootstrapClockStatus::Success;
#else
    (void)counter;
    (void)frequency;
    return false;
#endif
}

} // namespace micant::ke

namespace micant::sys {

NtStatus NtQuerySystemTime(LargeInteger* systemTime) noexcept {
    if (!systemTime) return NtStatus::AccessViolation;
    if (ex::ProbeForWrite(systemTime, sizeof(*systemTime), alignof(LargeInteger),
                          mm::UserMode) != NtStatus::Success)
        return NtStatus::AccessViolation;
    uint64_t value{};
    if (!ke::KeQuerySystemTime100ns(value) || value > static_cast<uint64_t>(INT64_MAX))
        return NtStatus::DeviceNotReady;
    LargeInteger result{};
    result.quadPart = static_cast<int64_t>(value);
    return ex::ExSafeCopyToUser(systemTime, &result, sizeof(result), mm::UserMode);
}

NtStatus NtQueryPerformanceCounter(LargeInteger* performanceCounter,
                                   LargeInteger* performanceFrequency) {
    if (!performanceCounter) return NtStatus::AccessViolation;
    if (ex::ProbeForWrite(performanceCounter, sizeof(*performanceCounter),
                          alignof(LargeInteger), mm::UserMode) != NtStatus::Success ||
        (performanceFrequency && ex::ProbeForWrite(performanceFrequency,
            sizeof(*performanceFrequency), alignof(LargeInteger), mm::UserMode) != NtStatus::Success))
        return NtStatus::AccessViolation;
    uint64_t counter{}, frequency{};
    if (!ke::KeQueryPerformanceCounterValue(counter, frequency) ||
        counter > static_cast<uint64_t>(INT64_MAX) ||
        frequency > static_cast<uint64_t>(INT64_MAX))
        return NtStatus::DeviceNotReady;
    LargeInteger countResult{}, frequencyResult{};
    countResult.quadPart = static_cast<int64_t>(counter);
    frequencyResult.quadPart = static_cast<int64_t>(frequency);
    NtStatus status = ex::ExSafeCopyToUser(performanceCounter, &countResult,
                                           sizeof(countResult), mm::UserMode);
    if (status != NtStatus::Success || !performanceFrequency) return status;
    return ex::ExSafeCopyToUser(performanceFrequency, &frequencyResult,
                                sizeof(frequencyResult), mm::UserMode);
}

} // namespace micant::sys
