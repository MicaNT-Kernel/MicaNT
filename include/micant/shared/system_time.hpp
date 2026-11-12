#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace micant::shared::time {

// Mode-neutral civil time used by the UEFI handoff and the AMD64 HAL.
// System time is represented as NT/FILETIME 100-ns intervals since 1601-01-01.
struct SystemTimeFields {
    uint16_t year{};
    uint16_t month{};
    uint16_t day{};
    uint16_t hour{};
    uint16_t minute{};
    uint16_t second{};
    uint16_t milliseconds{};
    uint16_t weekday{}; // Sunday = 0
    uint16_t subMillisecond100ns{};
};

inline constexpr uint64_t TicksPerMillisecond = 10'000;
inline constexpr uint64_t TicksPerSecond = 10'000'000;
inline constexpr uint64_t TicksPerMinute = 60 * TicksPerSecond;
inline constexpr uint64_t TicksPerDay = 24 * 60 * TicksPerMinute;
inline constexpr uint16_t EfiUnspecifiedTimeZone = 0x07ff;
inline constexpr uint8_t EfiAdjustForDaylight = 0x01;
inline constexpr uint8_t EfiInDaylight = 0x02;

[[nodiscard]] inline constexpr bool isLeapYear(uint32_t year) noexcept {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

[[nodiscard]] inline constexpr uint8_t daysInMonth(uint32_t year,
                                                    uint32_t month) noexcept {
    constexpr uint8_t days[]{0, 31, 28, 31, 30, 31, 30,
                             31, 31, 30, 31, 30, 31};
    if (month == 0 || month > 12) return 0;
    if (month == 2 && isLeapYear(year)) return 29;
    return days[month];
}

[[nodiscard]] inline constexpr bool isValid(const SystemTimeFields& value) noexcept {
    return value.year >= 1601 && value.year <= 9999 &&
        value.month >= 1 && value.month <= 12 && value.day >= 1 &&
        value.day <= daysInMonth(value.year, value.month) &&
        value.hour < 24 && value.minute < 60 && value.second < 60 &&
        value.milliseconds < 1000 && value.subMillisecond100ns < 10'000;
}

// Days since 1601-01-01, using a 400-year Gregorian cycle. Intermediate
// values are bounded by the accepted year range above.
[[nodiscard]] inline constexpr uint64_t daysSince1601(uint32_t year,
                                                       uint32_t month,
                                                       uint32_t day) noexcept {
    int64_t y = static_cast<int64_t>(year);
    y -= month <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const uint32_t yearOfEra = static_cast<uint32_t>(y - era * 400);
    const uint32_t shiftedMonth = month + (month > 2 ? uint32_t(-3) : 9u);
    const uint32_t dayOfYear = (153 * shiftedMonth + 2) / 5 + day - 1;
    const uint32_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 -
        yearOfEra / 100 + dayOfYear;
    // 719468 days are 1970-01-01 from the civil algorithm's epoch.
    constexpr int64_t UnixDaysFrom1601 = 134'774;
    const int64_t unixDays = era * 146097 + dayOfEra - 719468;
    return static_cast<uint64_t>(unixDays + UnixDaysFrom1601);
}

[[nodiscard]] inline constexpr bool toSystemTime100ns(
    const SystemTimeFields& value, uint64_t& result) noexcept {
    if (!isValid(value)) return false;
    const uint64_t days = daysSince1601(value.year, value.month, value.day);
    if (days > UINT64_MAX / TicksPerDay) return false;
    uint64_t ticks = days * TicksPerDay;
    const uint64_t withinDay = static_cast<uint64_t>(value.hour) * 3600 * TicksPerSecond +
        static_cast<uint64_t>(value.minute) * 60 * TicksPerSecond +
        static_cast<uint64_t>(value.second) * TicksPerSecond +
        static_cast<uint64_t>(value.milliseconds) * TicksPerMillisecond +
        value.subMillisecond100ns;
    if (withinDay > UINT64_MAX - ticks) return false;
    result = ticks + withinDay;
    return true;
}

[[nodiscard]] inline constexpr bool toUtc100ns(
    const SystemTimeFields& local, int16_t timezoneMinutes,
    uint64_t& utc) noexcept {
    if (timezoneMinutes < -1440 || timezoneMinutes > 1440) return false;
    uint64_t localTicks{};
    if (!toSystemTime100ns(local, localTicks)) return false;
    const int64_t offset = static_cast<int64_t>(timezoneMinutes) *
        static_cast<int64_t>(TicksPerMinute);
    if (offset >= 0) {
        const uint64_t add = static_cast<uint64_t>(offset);
        if (localTicks > UINT64_MAX - add) return false;
        utc = localTicks + add;
    } else {
        const uint64_t subtract = static_cast<uint64_t>(-offset);
        if (localTicks < subtract) return false;
        utc = localTicks - subtract;
    }
    return true;
}

[[nodiscard]] inline constexpr bool fromSystemTime100ns(
    uint64_t ticks, SystemTimeFields& value) noexcept {
    const uint64_t days = ticks / TicksPerDay;
    if (days > daysSince1601(9999, 12, 31)) return false;

    // Convert the absolute day to Gregorian civil year/month/day. The 1601
    // epoch shift is applied before using the same 400-year-cycle algorithm.
    int64_t z = static_cast<int64_t>(days) - 134'774 + 719'468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const uint32_t dayOfEra = static_cast<uint32_t>(z - era * 146097);
    const uint32_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 -
        dayOfEra / 146096) / 365;
    int64_t year = static_cast<int64_t>(yearOfEra) + era * 400;
    const uint32_t dayOfYear = dayOfEra -
        (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const uint32_t monthPrime = (5 * dayOfYear + 2) / 153;
    const uint32_t day = dayOfYear - (153 * monthPrime + 2) / 5 + 1;
    const uint32_t month = monthPrime + (monthPrime < 10 ? 3 : uint32_t(-9));
    year += month <= 2;
    if (year < 1601 || year > 9999) return false;

    uint64_t remainder = ticks % TicksPerDay;
    value.year = static_cast<uint16_t>(year);
    value.month = static_cast<uint16_t>(month);
    value.day = static_cast<uint16_t>(day);
    value.hour = static_cast<uint16_t>(remainder / (3600 * TicksPerSecond));
    remainder %= 3600 * TicksPerSecond;
    value.minute = static_cast<uint16_t>(remainder / (60 * TicksPerSecond));
    remainder %= 60 * TicksPerSecond;
    value.second = static_cast<uint16_t>(remainder / TicksPerSecond);
    value.milliseconds = static_cast<uint16_t>((remainder % TicksPerSecond) /
                                               TicksPerMillisecond);
    value.subMillisecond100ns = static_cast<uint16_t>(remainder % TicksPerMillisecond);
    value.weekday = static_cast<uint16_t>((days + 1) % 7); // 1601-01-01 Monday
    return true;
}

static_assert([] {
    uint64_t ticks{};
    SystemTimeFields epoch{1601, 1, 1, 0, 0, 0, 0, 1};
    if (!toSystemTime100ns(epoch, ticks) || ticks != 0) return false;
    SystemTimeFields unixEpoch{1970, 1, 1, 0, 0, 0, 0, 4};
    if (!toSystemTime100ns(unixEpoch, ticks) ||
        ticks != 116'444'736'000'000'000ULL) return false;
    SystemTimeFields roundTrip{};
    if (!fromSystemTime100ns(ticks, roundTrip) ||
        roundTrip.year != unixEpoch.year || roundTrip.month != unixEpoch.month ||
        roundTrip.day != unixEpoch.day || roundTrip.weekday != unixEpoch.weekday)
        return false;
    SystemTimeFields pacific{2025, 1, 1, 12, 0, 0, 0, 3};
    uint64_t utc{};
    if (!toUtc100ns(pacific, 480, utc) ||
        !fromSystemTime100ns(utc, roundTrip) || roundTrip.year != 2025 ||
        roundTrip.month != 1 || roundTrip.day != 1 || roundTrip.hour != 20)
        return false;
    SystemTimeFields yearEnd{2024, 12, 31, 23, 59, 59, 0, 2};
    uint64_t yearEndTicks{};
    return toSystemTime100ns(yearEnd, yearEndTicks) &&
        yearEndTicks <= UINT64_MAX - TicksPerSecond &&
        fromSystemTime100ns(yearEndTicks + TicksPerSecond, roundTrip) &&
        roundTrip.year == 2025 && roundTrip.month == 1 &&
        roundTrip.day == 1 && roundTrip.hour == 0 && roundTrip.second == 0;
}());

} // namespace micant::shared::time

namespace micant::shared::boot_time {

enum class SampleStatus : uint32_t {
    NotProvided = 0,
    UtcValid = 1,
    LocalTimeZoneUnspecified = 2,
    InvalidFirmwareData = 3,
};

enum class SampleSource : uint16_t {
    None = 0,
    UefiRuntimeGetTime = 1,
    CmosRtc = 2,
};

struct BootTimeSampleV1 {
    uint32_t size{sizeof(BootTimeSampleV1)};
    uint16_t version{1};
    SampleSource source{SampleSource::None};
    SampleStatus status{SampleStatus::NotProvided};
    int16_t timeZoneMinutes{
        static_cast<int16_t>(micant::shared::time::EfiUnspecifiedTimeZone)};
    uint8_t daylight{};
    uint8_t reserved{};
    uint64_t utcTime100ns{};
};

enum class BootClockFlagsV1 : uint16_t {
    None = 0,
    InvariantTsc = 1,
};

// Optional firmware-measured TSC scale. It is only a calibration input to
// HAL; the kernel still checks the architectural invariant-TSC capability.
struct BootClockCalibrationV1 {
    uint32_t size{sizeof(BootClockCalibrationV1)};
    uint16_t version{1};
    BootClockFlagsV1 flags{BootClockFlagsV1::None};
    uint64_t tscFrequencyHz{};
};

static_assert(std::is_standard_layout_v<BootTimeSampleV1>);
static_assert(std::is_trivially_copyable_v<BootTimeSampleV1>);
static_assert(sizeof(BootTimeSampleV1) == 24);
static_assert(offsetof(BootTimeSampleV1, utcTime100ns) == 16);
static_assert(std::is_standard_layout_v<BootClockCalibrationV1>);
static_assert(std::is_trivially_copyable_v<BootClockCalibrationV1>);
static_assert(sizeof(BootClockCalibrationV1) == 16);
static_assert(offsetof(BootClockCalibrationV1, tscFrequencyHz) == 8);

} // namespace micant::shared::boot_time
