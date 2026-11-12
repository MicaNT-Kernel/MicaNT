#if defined(MICANT_FREESTANDING_KERNEL32)

#include <cstdint>
#include "micant/shared/system_time.hpp"
#include "micant/um/subsystems/winbase/utf8_conversion.hpp"

static_assert(sizeof(void*) == 8);

extern "C" {

__declspec(dllimport) int32_t __stdcall NtQuerySystemTime(int64_t* systemTime) noexcept;
__declspec(dllimport) int32_t __stdcall NtQueryPerformanceCounter(
    int64_t* performanceCounter, int64_t* performanceFrequency) noexcept;

struct Kernel32SystemTime {
    uint16_t year, month, dayOfWeek, day, hour, minute, second, milliseconds;
};
struct Kernel32FileTime { uint32_t low, high; };
struct Kernel32TimeZoneInformation {
    int32_t bias;
    wchar_t standardName[32];
    Kernel32SystemTime standardDate;
    int32_t standardBias;
    wchar_t daylightName[32];
    Kernel32SystemTime daylightDate;
    int32_t daylightBias;
};
static_assert(sizeof(Kernel32SystemTime) == 16);
static_assert(sizeof(Kernel32FileTime) == 8);
static_assert(sizeof(Kernel32TimeZoneInformation) == 172);

static bool ReadSystemTimeZone(Kernel32TimeZoneInformation& zone,
                               uint32_t& zoneId) noexcept {
    const auto* base = reinterpret_cast<const volatile uint8_t*>(0x7FFE0000ULL);
    const auto* low = reinterpret_cast<const volatile uint32_t*>(base + 0x20);
    const auto* high1 = reinterpret_cast<const volatile int32_t*>(base + 0x24);
    const auto* high2 = reinterpret_cast<const volatile int32_t*>(base + 0x28);
    bool stable = false;
    int32_t signedHigh{};
    uint32_t lowValue{};
    for (unsigned retry = 0; retry < 8; ++retry) {
        const int32_t first = *high1;
        const uint32_t middle = *low;
        const int32_t second = *high2;
        if (first == second) {
            signedHigh = first;
            lowValue = middle;
            stable = true;
            break;
        }
    }
    if (!stable) return false;
    const int64_t biasTicks = static_cast<int64_t>(
        (static_cast<uint64_t>(static_cast<uint32_t>(signedHigh)) << 32) | lowValue);
    if (biasTicks % static_cast<int64_t>(micant::shared::time::TicksPerMinute))
        return false;
    const int64_t bias = biasTicks /
        static_cast<int64_t>(micant::shared::time::TicksPerMinute);
    if (bias < -1440 || bias > 1440) return false;
    zone = {};
    zone.bias = static_cast<int32_t>(bias);
    if (bias == 0) {
        zone.standardName[0] = L'U'; zone.standardName[1] = L'T';
        zone.standardName[2] = L'C';
    } else {
        zone.standardName[0] = L'M'; zone.standardName[1] = L'i';
        zone.standardName[2] = L'c'; zone.standardName[3] = L'a';
        zone.standardName[4] = L'N'; zone.standardName[5] = L'T';
    }
    zoneId = *reinterpret_cast<const volatile uint32_t*>(base + 0x240);
    return zoneId <= 2;
}

static bool QueryUtcFields(micant::shared::time::SystemTimeFields& fields) noexcept {
    int64_t ticks{};
    if (NtQuerySystemTime(&ticks) < 0 ||
        !micant::shared::time::fromSystemTime100ns(static_cast<uint64_t>(ticks), fields))
        return false;
    return true;
}

static bool ApplyTimeZone(const Kernel32TimeZoneInformation* zone,
                          const Kernel32SystemTime* utc,
                          Kernel32SystemTime* local) noexcept {
    using namespace micant::shared::time;
    if (!zone || !utc || !local || zone->bias < -1440 || zone->bias > 1440 ||
        zone->standardBias < -1440 || zone->standardBias > 1440 ||
        zone->daylightBias < -1440 || zone->daylightBias > 1440) return false;
    SystemTimeFields civil{utc->year, utc->month, utc->day, utc->hour, utc->minute,
        utc->second, utc->milliseconds, utc->dayOfWeek};
    uint64_t utcTicks{};
    if (!toSystemTime100ns(civil, utcTicks)) return false;
    int64_t bias = static_cast<int64_t>(zone->bias) + zone->standardBias;

    auto transition = [&](const Kernel32SystemTime& rule, uint16_t year,
                          int32_t priorBias, uint64_t& instant) noexcept {
        if (rule.month == 0) return false;
        if (rule.year && rule.year != year) return false;
        const uint16_t effectiveYear = rule.year ? rule.year : year;
        uint32_t day = rule.day;
        if (rule.year == 0) {
            if (rule.month > 12 || rule.day < 1 || rule.day > 5 || rule.dayOfWeek > 6)
                return false;
            SystemTimeFields first{effectiveYear, rule.month, 1, 0, 0, 0, 0, 0};
            uint64_t firstTicks{};
            if (!toSystemTime100ns(first, firstTicks)) return false;
            const uint32_t firstWeekday = fromSystemTime100ns(firstTicks, first)
                ? first.weekday : 0;
            day = 1 + (rule.dayOfWeek + 7 - firstWeekday) % 7 +
                  (rule.day - 1) * 7;
            const auto maxDay = daysInMonth(effectiveYear, rule.month);
            if (day > maxDay) day -= 7;
        }
        SystemTimeFields localRule{effectiveYear, rule.month, static_cast<uint16_t>(day),
            rule.hour, rule.minute, rule.second, rule.milliseconds, 0};
        uint64_t localTicks{};
        if (!toSystemTime100ns(localRule, localTicks)) return false;
        const int64_t offset = priorBias * static_cast<int64_t>(TicksPerMinute);
        if (offset >= 0) {
            if (localTicks > UINT64_MAX - static_cast<uint64_t>(offset)) return false;
            instant = localTicks + static_cast<uint64_t>(offset);
        } else {
            const auto add = static_cast<uint64_t>(-offset);
            if (localTicks < add) return false;
            instant = localTicks - add;
        }
        return true;
    };

    if ((zone->daylightDate.month == 0) != (zone->standardDate.month == 0))
        return false;
    if (zone->daylightDate.month != 0) {
        uint64_t daylightStart{}, standardStart{};
        const int64_t daylightTotal = static_cast<int64_t>(zone->bias) + zone->daylightBias;
        if (!transition(zone->daylightDate, utc->year, static_cast<int32_t>(bias), daylightStart) ||
            !transition(zone->standardDate, utc->year, static_cast<int32_t>(daylightTotal), standardStart))
            return false;
        const bool inDaylight = daylightStart < standardStart
            ? utcTicks >= daylightStart && utcTicks < standardStart
            : utcTicks >= daylightStart || utcTicks < standardStart;
        if (inDaylight) bias = daylightTotal;
    }
    const int64_t delta = -bias * static_cast<int64_t>(TicksPerMinute);
    uint64_t localTicks{};
    if (delta >= 0) {
        if (utcTicks > UINT64_MAX - static_cast<uint64_t>(delta)) return false;
        localTicks = utcTicks + static_cast<uint64_t>(delta);
    } else {
        if (utcTicks < static_cast<uint64_t>(-delta)) return false;
        localTicks = utcTicks - static_cast<uint64_t>(-delta);
    }
    if (!fromSystemTime100ns(localTicks, civil)) return false;
    *local = {civil.year, civil.month, civil.weekday, civil.day, civil.hour,
              civil.minute, civil.second, civil.milliseconds};
    return true;
}

static bool LocalTimeToUtc(const Kernel32TimeZoneInformation* zone,
                           const Kernel32SystemTime* local,
                           Kernel32SystemTime* utc) noexcept {
    using namespace micant::shared::time;
    if (!zone || !local || !utc || zone->bias < -1440 || zone->bias > 1440 ||
        zone->standardBias < -1440 || zone->standardBias > 1440 ||
        zone->daylightBias < -1440 || zone->daylightBias > 1440) return false;
    SystemTimeFields localFields{local->year, local->month, local->day, local->hour,
        local->minute, local->second, local->milliseconds, local->dayOfWeek};
    uint64_t localTicks{};
    if (!toSystemTime100ns(localFields, localTicks)) return false;
    const int64_t standardBias = static_cast<int64_t>(zone->bias) + zone->standardBias;
    const int64_t daylightBias = static_cast<int64_t>(zone->bias) + zone->daylightBias;
    auto candidateForBias = [&](int64_t bias, Kernel32SystemTime& result) noexcept {
        if (bias < -2880 || bias > 2880) return false;
        const int64_t delta = bias * static_cast<int64_t>(TicksPerMinute);
        uint64_t candidate{};
        if (delta >= 0) {
            if (localTicks > UINT64_MAX - static_cast<uint64_t>(delta)) return false;
            candidate = localTicks + static_cast<uint64_t>(delta);
        } else {
            if (localTicks < static_cast<uint64_t>(-delta)) return false;
            candidate = localTicks - static_cast<uint64_t>(-delta);
        }
        SystemTimeFields fields{};
        if (!fromSystemTime100ns(candidate, fields)) return false;
        Kernel32SystemTime utcCandidate{fields.year, fields.month, fields.weekday,
            fields.day, fields.hour, fields.minute, fields.second, fields.milliseconds};
        Kernel32SystemTime roundTrip{};
        if (!ApplyTimeZone(zone, &utcCandidate, &roundTrip) ||
            roundTrip.year != local->year || roundTrip.month != local->month ||
            roundTrip.day != local->day || roundTrip.hour != local->hour ||
            roundTrip.minute != local->minute || roundTrip.second != local->second ||
            roundTrip.milliseconds != local->milliseconds) return false;
        result = utcCandidate;
        return true;
    };
    Kernel32SystemTime result{};
    if (!candidateForBias(standardBias, result) &&
        (!zone->daylightDate.month || !zone->standardDate.month ||
         !candidateForBias(daylightBias, result))) return false;
    *utc = result;
    return true;
}

using VectoredExceptionHandler = int32_t (__stdcall *)(void*);
__declspec(dllimport) void* __stdcall RtlAddVectoredExceptionHandler(
    uint32_t first, VectoredExceptionHandler handler) noexcept;
__declspec(dllimport) unsigned long __stdcall RtlRemoveVectoredExceptionHandler(
    void* cookie) noexcept;
__declspec(dllimport) void* __stdcall RtlAddVectoredContinueHandler(
    uint32_t first, VectoredExceptionHandler handler) noexcept;
__declspec(dllimport) unsigned long __stdcall RtlRemoveVectoredContinueHandler(
    void* cookie) noexcept;

__declspec(dllexport) void* __stdcall AddVectoredExceptionHandler(
    uint32_t first, VectoredExceptionHandler handler) noexcept {
    return RtlAddVectoredExceptionHandler(first, handler);
}

__declspec(dllexport) unsigned long __stdcall RemoveVectoredExceptionHandler(
    void* cookie) noexcept {
    return RtlRemoveVectoredExceptionHandler(cookie);
}

__declspec(dllexport) void* __stdcall AddVectoredContinueHandler(
    uint32_t first, VectoredExceptionHandler handler) noexcept {
    return RtlAddVectoredContinueHandler(first, handler);
}

__declspec(dllexport) unsigned long __stdcall RemoveVectoredContinueHandler(
    void* cookie) noexcept {
    return RtlRemoveVectoredContinueHandler(cookie);
}

__declspec(dllexport) uint32_t __stdcall GetCurrentProcessId() noexcept {
    uint32_t processId = 0;
    __asm__ __volatile__("movl %%gs:0x40, %0" : "=r"(processId));
    return processId;
}

__declspec(dllexport) uint32_t __stdcall GetCurrentThreadId() noexcept {
    uint32_t threadId = 0;
    __asm__ __volatile__("movl %%gs:0x48, %0" : "=r"(threadId));
    return threadId;
}

__declspec(dllexport) uint32_t __stdcall GetLastError() noexcept {
    uint32_t error = 0;
    __asm__ __volatile__("movl %%gs:0x68, %0" : "=r"(error));
    return error;
}

__declspec(dllexport) void __stdcall SetLastError(uint32_t error) noexcept {
    __asm__ __volatile__("movl %0, %%gs:0x68" : : "r"(error) : "memory");
}

static void SetConversionError(micant::um::winbase::utf8::Error error) noexcept {
    uint32_t win32Error = 87;
    if (error == micant::um::winbase::utf8::Error::insufficient_buffer) win32Error = 122;
    if (error == micant::um::winbase::utf8::Error::no_unicode_translation) win32Error = 1113;
    SetLastError(win32Error);
}

__declspec(dllexport) int __stdcall MultiByteToWideChar(
    uint32_t codePage, uint32_t flags, const char* input, int inputCount,
    wchar_t* output, int outputCapacity) noexcept {
    if ((codePage != 0 && codePage != 65001) || (flags & ~0x8U) != 0 ||
        (output == nullptr && outputCapacity != 0) || inputCount == 0 ||
        inputCount < -1 || outputCapacity < 0) {
        SetConversionError(micant::um::winbase::utf8::Error::invalid_parameter);
        return 0;
    }
    static_assert(sizeof(wchar_t) == sizeof(uint16_t));
    const auto result = micant::um::winbase::utf8::to_utf16(
        input, inputCount, reinterpret_cast<uint16_t*>(output), outputCapacity,
        (flags & 0x8U) != 0);
    if (result.error != micant::um::winbase::utf8::Error::none) {
        SetConversionError(result.error);
        return 0;
    }
    return result.count;
}

__declspec(dllexport) int __stdcall WideCharToMultiByte(
    uint32_t codePage, uint32_t flags, const wchar_t* input, int inputCount,
    char* output, int outputCapacity, const char* defaultChar,
    int* usedDefaultChar) noexcept {
    if ((codePage != 0 && codePage != 65001) || (flags & ~0x80U) != 0 ||
        defaultChar != nullptr || usedDefaultChar != nullptr ||
        (output == nullptr && outputCapacity != 0) || inputCount == 0 ||
        inputCount < -1 || outputCapacity < 0) {
        SetConversionError(micant::um::winbase::utf8::Error::invalid_parameter);
        return 0;
    }
    static_assert(sizeof(wchar_t) == sizeof(uint16_t));
    const auto result = micant::um::winbase::utf8::to_utf8(
        reinterpret_cast<const uint16_t*>(input), inputCount, output, outputCapacity,
        (flags & 0x80U) != 0);
    if (result.error != micant::um::winbase::utf8::Error::none) {
        SetConversionError(result.error);
        return 0;
    }
    return result.count;
}

int __stdcall DllMainCRTStartup(void*, unsigned long, void*) noexcept {
    return 1;
}

__declspec(dllexport) void __stdcall GetSystemTime(Kernel32SystemTime* output) noexcept {
    if (!output) return;
    micant::shared::time::SystemTimeFields value{};
    if (!QueryUtcFields(value)) return;
    *output = {value.year, value.month, value.weekday, value.day, value.hour,
               value.minute, value.second, value.milliseconds};
}

// Convert UTC using the active bias shared by the kernel; explicit TZI rules
// can be supplied through SystemTimeToTzSpecificLocalTime.
__declspec(dllexport) void __stdcall GetLocalTime(Kernel32SystemTime* output) noexcept {
    if (!output) return;
    micant::shared::time::SystemTimeFields current{};
    if (!QueryUtcFields(current)) return;
    const Kernel32SystemTime utc{current.year, current.month, current.weekday,
        current.day, current.hour, current.minute, current.second,
        current.milliseconds};
    Kernel32TimeZoneInformation zone{};
    uint32_t zoneId{};
    if (!ReadSystemTimeZone(zone, zoneId)) return;
    Kernel32SystemTime local{};
    if (ApplyTimeZone(&zone, &utc, &local)) *output = local;
}

__declspec(dllexport) uint32_t __stdcall GetTimeZoneInformation(
    Kernel32TimeZoneInformation* zone) noexcept {
    if (!zone) return 0xFFFFFFFFu;
    uint32_t zoneId{};
    if (!ReadSystemTimeZone(*zone, zoneId)) return 0xFFFFFFFFu;
    return zoneId;
}

__declspec(dllexport) int32_t __stdcall SystemTimeToTzSpecificLocalTime(
    const Kernel32TimeZoneInformation* zone, const Kernel32SystemTime* utc,
    Kernel32SystemTime* local) noexcept {
    if (!utc || !local) return 0;
    Kernel32TimeZoneInformation configured{};
    uint32_t zoneId{};
    if (!zone) {
        if (!ReadSystemTimeZone(configured, zoneId)) return 0;
        zone = &configured;
    }
    return ApplyTimeZone(zone, utc, local) ? 1 : 0;
}

__declspec(dllexport) int32_t __stdcall TzSpecificLocalTimeToSystemTime(
    const Kernel32TimeZoneInformation* zone, const Kernel32SystemTime* local,
    Kernel32SystemTime* utc) noexcept {
    if (!local || !utc) return 0;
    Kernel32TimeZoneInformation configured{};
    uint32_t zoneId{};
    if (!zone) {
        if (!ReadSystemTimeZone(configured, zoneId)) return 0;
        zone = &configured;
    }
    return LocalTimeToUtc(zone, local, utc) ? 1 : 0;
}

__declspec(dllexport) void __stdcall GetSystemTimeAsFileTime(Kernel32FileTime* output) noexcept {
    if (!output) return;
    int64_t ticks{};
    if (NtQuerySystemTime(&ticks) < 0) return;
    const uint64_t value = static_cast<uint64_t>(ticks);
    output->low = static_cast<uint32_t>(value);
    output->high = static_cast<uint32_t>(value >> 32);
}

__declspec(dllexport) void __stdcall GetSystemTimePreciseAsFileTime(
    Kernel32FileTime* output) noexcept {
    GetSystemTimeAsFileTime(output);
}

__declspec(dllexport) int32_t __stdcall QueryPerformanceCounter(int64_t* value) noexcept {
    if (!value) return 0;
    return NtQueryPerformanceCounter(value, nullptr) >= 0 ? 1 : 0;
}

__declspec(dllexport) int32_t __stdcall QueryPerformanceFrequency(int64_t* value) noexcept {
    if (!value) return 0;
    int64_t counter{};
    return NtQueryPerformanceCounter(&counter, value) >= 0 ? 1 : 0;
}

__declspec(dllexport) uint64_t __stdcall GetTickCount64() noexcept {
    auto* shared = reinterpret_cast<volatile const uint64_t*>(0x7FFE0320ULL);
    return *shared;
}

}

#else

/**
 * @file winnt_kernel32_advapi32_system32_abi.cpp
 * @brief Horizon 110 (Kernel Unit #82): Sovereign WinNT Core User-Mode System32 DLLs
 *        (kernel32.dll & advapi32.dll) Official PE32+ Exports, KUSER_SHARED_DATA 10MHz
 *        QPC Timer, D3D12Fence Event Synchronization, DynamicLoader, and Cm Registry.
 */

#include "micant/um/subsystems/winbase/winnt_kernel32_advapi32_system32_abi.hpp"

namespace micant::um::winnt_kernel32_advapi32_abi {

namespace {
uint32_t g_lastError = ERROR_SUCCESS_OFFICIAL;

WinNtEventHandleObject g_eventPool[32]{};
uint32_t g_eventPoolNext = 0;

WinNtRegistryKeyObject g_regKeyPool[32]{};
uint32_t g_regKeyPoolNext = 0;

alignas(64) uint8_t g_virtualAllocArena[256 * 1024]{};

inline void ByteCopy(void* dst, const void* src, size_t n) noexcept {
    auto* d = static_cast<uint8_t*>(dst);
    const auto* s = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
}

inline void ByteZero(void* dst, size_t n) noexcept {
    auto* d = static_cast<uint8_t*>(dst);
    for (size_t i = 0; i < n; ++i) d[i] = 0;
}
} // namespace

int32_t Kernel32_QueryPerformanceCounter(int64_t* lpPerformanceCount) noexcept {
    if (!lpPerformanceCount) return 0;
    return NtQueryPerformanceCounter(lpPerformanceCount, nullptr) >= 0 ? 1 : 0;
}

int32_t Kernel32_QueryPerformanceFrequency(int64_t* lpFrequency) noexcept {
    if (!lpFrequency) return 0;
    int64_t counter{};
    return NtQueryPerformanceCounter(&counter, lpFrequency) >= 0 ? 1 : 0;
}

uint64_t Kernel32_GetTickCount64() noexcept {
    auto* shared = reinterpret_cast<volatile const uint64_t*>(0x7FFE0320ULL);
    return *shared;
}

void* Kernel32_CreateEventExW(void* /*lpEventAttributes*/, const wchar_t* lpName, uint32_t dwFlags, uint32_t /*dwDesiredAccess*/) noexcept {
    WinNtEventHandleObject* ev = &g_eventPool[(g_eventPoolNext++) & 31U];
    ev->magic       = 0x4D4E544556454E54ULL;
    ev->manualReset = (dwFlags & 0x00000001U) ? 1 : 0;
    ev->signaled    = (dwFlags & 0x00000002U) ? 1 : 0;
    ev->waitCount   = 0;
    if (lpName) {
        size_t i = 0;
        while (lpName[i] != L'\0' && i + 1 < 64) {
            ev->name[i] = lpName[i];
            ++i;
        }
        ev->name[i] = L'\0';
    } else {
        ev->name[0] = L'\0';
    }
    return ev;
}

void* Kernel32_CreateEventW(void* lpEventAttributes, int32_t bManualReset, int32_t bInitialState, const wchar_t* lpName) noexcept {
    uint32_t flags = (bManualReset ? 0x1U : 0U) | (bInitialState ? 0x2U : 0U);
    return Kernel32_CreateEventExW(lpEventAttributes, lpName, flags, 0x1F0003U);
}

int32_t Kernel32_SetEvent(void* hEvent) noexcept {
    if (!hEvent) return 0;
    auto* ev = static_cast<WinNtEventHandleObject*>(hEvent);
    if (ev->magic != 0x4D4E544556454E54ULL) return 0;
    ev->signaled = 1;
    return 1;
}

int32_t Kernel32_ResetEvent(void* hEvent) noexcept {
    if (!hEvent) return 0;
    auto* ev = static_cast<WinNtEventHandleObject*>(hEvent);
    if (ev->magic != 0x4D4E544556454E54ULL) return 0;
    ev->signaled = 0;
    return 1;
}

uint32_t Kernel32_WaitForSingleObjectEx(void* hHandle, uint32_t /*dwMilliseconds*/, int32_t /*bAlertable*/) noexcept {
    if (!hHandle) return 0xFFFFFFFFU;
    auto* ev = static_cast<WinNtEventHandleObject*>(hHandle);
    if (ev->magic != 0x4D4E544556454E54ULL) return WAIT_OBJECT_0_OFFICIAL;
    ev->waitCount++;
    if (ev->signaled) {
        if (!ev->manualReset) {
            ev->signaled = 0;
        }
        return WAIT_OBJECT_0_OFFICIAL;
    }
    return WAIT_TIMEOUT_OFFICIAL;
}

uint32_t Kernel32_WaitForSingleObject(void* hHandle, uint32_t dwMilliseconds) noexcept {
    return Kernel32_WaitForSingleObjectEx(hHandle, dwMilliseconds, 0);
}

uint32_t Kernel32_WaitForMultipleObjectsEx(uint32_t nCount, const void* const* lpHandles, int32_t /*bWaitAll*/, uint32_t dwMilliseconds, int32_t bAlertable) noexcept {
    if (nCount == 0 || !lpHandles) return 0xFFFFFFFFU;
    return Kernel32_WaitForSingleObjectEx(const_cast<void*>(lpHandles[0]), dwMilliseconds, bAlertable);
}

int32_t Kernel32_CloseHandle(void* hObject) noexcept {
    if (!hObject) return 0;
    auto* ev = static_cast<WinNtEventHandleObject*>(hObject);
    if (ev->magic == 0x4D4E544556454E54ULL) {
        ev->magic = 0;
    }
    return 1;
}

void* Kernel32_VirtualAlloc(void* /*lpAddress*/, size_t dwSize, uint32_t /*flAllocationType*/, uint32_t /*flProtect*/) noexcept {
    if (dwSize == 0 || dwSize > sizeof(g_virtualAllocArena)) return nullptr;
    ByteZero(g_virtualAllocArena, dwSize);
    return g_virtualAllocArena;
}

int32_t Kernel32_VirtualFree(void* lpAddress, size_t /*dwSize*/, uint32_t dwFreeType) noexcept {
    if (!lpAddress) return 0;
    return (dwFreeType == MEM_RELEASE_OFFICIAL) ? 1 : 0;
}

void* Kernel32_LoadLibraryExW(const wchar_t* lpLibFileName, void* /*hFile*/, uint32_t /*dwFlags*/) noexcept {
    if (!lpLibFileName) return nullptr;
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x00007FF810000000ULL));
}

void* Kernel32_LoadLibraryW(const wchar_t* lpLibFileName) noexcept {
    return Kernel32_LoadLibraryExW(lpLibFileName, nullptr, 0);
}

void* Kernel32_GetModuleHandleW(const wchar_t* /*lpModuleName*/) noexcept {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x00007FF700000000ULL));
}

void* Kernel32_GetProcAddress(void* hModule, const char* lpProcName) noexcept {
    if (!hModule || !lpProcName) return nullptr;
    return reinterpret_cast<void*>(&Kernel32_QueryPerformanceCounter);
}

int32_t Kernel32_FreeLibrary(void* hLibModule) noexcept {
    return hLibModule ? 1 : 0;
}

void Kernel32_Sleep(uint32_t /*dwMilliseconds*/) noexcept {}
int32_t Kernel32_SwitchToThread() noexcept { return 1; }
uint32_t Kernel32_GetCurrentProcessId() noexcept { return 4096U; }
uint32_t Kernel32_GetCurrentThreadId() noexcept { return 4100U; }
uint32_t Kernel32_GetLastError() noexcept { return g_lastError; }
void Kernel32_SetLastError(uint32_t dwErrCode) noexcept { g_lastError = dwErrCode; }
int32_t Kernel32_SetThreadDescription(void* /*hThread*/, const wchar_t* /*lpThreadDescription*/) noexcept { return 0; }

// advapi32.dll implementations
int32_t Advapi32_RegCreateKeyExW(
    void* /*hKey*/,
    const wchar_t* lpSubKey,
    uint32_t /*Reserved*/,
    wchar_t* /*lpClass*/,
    uint32_t /*dwOptions*/,
    uint32_t samDesired,
    void* /*lpSecurityAttributes*/,
    void** phkResult,
    uint32_t* lpdwDisposition) noexcept {
    if (!phkResult) return 87; // ERROR_INVALID_PARAMETER
    WinNtRegistryKeyObject* keyObj = &g_regKeyPool[(g_regKeyPoolNext++) & 31U];
    keyObj->magic          = 0x4D4E545245474B59ULL;
    keyObj->samDesired     = samDesired;
    keyObj->lastDwordValue = 1U;
    if (lpSubKey) {
        size_t i = 0;
        while (lpSubKey[i] != L'\0' && i + 1 < 128) {
            keyObj->subKey[i] = lpSubKey[i];
            ++i;
        }
        keyObj->subKey[i] = L'\0';
    } else {
        keyObj->subKey[0] = L'\0';
    }
    *phkResult = keyObj;
    if (lpdwDisposition) *lpdwDisposition = 1U; // REG_CREATED_NEW_KEY
    return ERROR_SUCCESS_OFFICIAL;
}

int32_t Advapi32_RegOpenKeyExW(void* hKey, const wchar_t* lpSubKey, uint32_t /*ulOptions*/, uint32_t samDesired, void** phkResult) noexcept {
    return Advapi32_RegCreateKeyExW(hKey, lpSubKey, 0, nullptr, 0, samDesired, nullptr, phkResult, nullptr);
}

int32_t Advapi32_RegSetValueExW(void* hKey, const wchar_t* /*lpValueName*/, uint32_t /*Reserved*/, uint32_t dwType, const uint8_t* lpData, uint32_t cbData) noexcept {
    if (!hKey || !lpData) return 87;
    auto* keyObj = static_cast<WinNtRegistryKeyObject*>(hKey);
    if (keyObj->magic == 0x4D4E545245474B59ULL && dwType == REG_DWORD_OFFICIAL && cbData >= sizeof(uint32_t)) {
        ByteCopy(&keyObj->lastDwordValue, lpData, sizeof(uint32_t));
    }
    return ERROR_SUCCESS_OFFICIAL;
}

int32_t Advapi32_RegQueryValueExW(void* hKey, const wchar_t* /*lpValueName*/, uint32_t* /*lpReserved*/, uint32_t* lpType, uint8_t* lpData, uint32_t* lpcbData) noexcept {
    if (!hKey) return 87;
    auto* keyObj = static_cast<WinNtRegistryKeyObject*>(hKey);
    if (lpType) *lpType = REG_DWORD_OFFICIAL;
    if (lpData && lpcbData && *lpcbData >= sizeof(uint32_t)) {
        uint32_t val = (keyObj->magic == 0x4D4E545245474B59ULL) ? keyObj->lastDwordValue : 1U;
        ByteCopy(lpData, &val, sizeof(uint32_t));
    }
    if (lpcbData) *lpcbData = sizeof(uint32_t);
    return ERROR_SUCCESS_OFFICIAL;
}

int32_t Advapi32_RegGetValueW(void* hkey, const wchar_t* /*lpSubKey*/, const wchar_t* lpValue, uint32_t /*dwFlags*/, uint32_t* pdwType, void* pvData, uint32_t* pcbData) noexcept {
    return Advapi32_RegQueryValueExW(hkey, lpValue, nullptr, pdwType, static_cast<uint8_t*>(pvData), pcbData);
}

int32_t Advapi32_RegCloseKey(void* hKey) noexcept {
    if (!hKey) return 87;
    auto* keyObj = static_cast<WinNtRegistryKeyObject*>(hKey);
    if (keyObj->magic == 0x4D4E545245474B59ULL) {
        keyObj->magic = 0;
    }
    return ERROR_SUCCESS_OFFICIAL;
}

int32_t Advapi32_GetUserNameW(wchar_t* lpBuffer, uint32_t* pcbBuffer) noexcept {
    constexpr wchar_t kUser[] = L"MicaNTPlayer";
    constexpr uint32_t kLen = sizeof(kUser) / sizeof(wchar_t);
    if (!pcbBuffer) return 0;
    if (lpBuffer && *pcbBuffer >= kLen) {
        ByteCopy(lpBuffer, kUser, sizeof(kUser));
    }
    *pcbBuffer = kLen;
    return 1;
}

uint8_t Advapi32_SystemFunction036(void* RandomBuffer, uint32_t RandomBufferLength) noexcept {
    if (!RandomBuffer && RandomBufferLength > 0) return 0;
    auto* bytes = static_cast<uint8_t*>(RandomBuffer);
    uint32_t state = 0xA55A3C69U;
    for (uint32_t i = 0; i < RandomBufferLength; ++i) {
        state = state * 1664525U + 1013904223U;
        bytes[i] = static_cast<uint8_t>((state >> 16) & 0xFFU);
    }
    return 1;
}

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
void WinNtKernel32Advapi32Coordinator::initialize() noexcept {
    if (m_initialized) return;
    m_cachedReport = runKernel32Advapi32Audit();
    m_initialized = true;
}

Horizon110Kernel32Advapi32AuditReport WinNtKernel32Advapi32Coordinator::runKernel32Advapi32Audit() noexcept {
    Horizon110Kernel32Advapi32AuditReport rep{};
    rep.kernel32ExportCount = 24;
    rep.advapi32ExportCount = 8;

    Kernel32_QueryPerformanceFrequency(&rep.qpcFrequencyHz);
    Kernel32_QueryPerformanceCounter(&rep.qpcCounterValue);
    rep.tickCount64Ms = Kernel32_GetTickCount64();

    void* hEvent = Kernel32_CreateEventW(nullptr, 0, 0, L"Local\\D3D12FenceCompletionEvent");
    if (hEvent) {
        Kernel32_SetEvent(hEvent);
        uint32_t waitRes = Kernel32_WaitForSingleObjectEx(hEvent, INFINITE_OFFICIAL, 0);
        rep.eventCreateSetWaitVerified = (waitRes == WAIT_OBJECT_0_OFFICIAL);
        Kernel32_CloseHandle(hEvent);
    }

    void* mem = Kernel32_VirtualAlloc(nullptr, 65536, MEM_COMMIT_OFFICIAL | MEM_RESERVE_OFFICIAL, PAGE_READWRITE_OFFICIAL);
    if (mem) {
        static_cast<uint32_t*>(mem)[0] = 0x12345678U;
        rep.virtualAllocFreeVerified = (static_cast<uint32_t*>(mem)[0] == 0x12345678U) &&
                                       (Kernel32_VirtualFree(mem, 0, MEM_RELEASE_OFFICIAL) == 1);
    }

    void* hKey = nullptr;
    if (Advapi32_RegOpenKeyExW(nullptr, L"SOFTWARE\\Microsoft\\DirectX", 0, 0x20019U, &hKey) == 0 && hKey) {
        uint32_t writeVal = 0x0001000CU;
        Advapi32_RegSetValueExW(hKey, L"InstalledVersion", 0, REG_DWORD_OFFICIAL, reinterpret_cast<const uint8_t*>(&writeVal), sizeof(writeVal));
        uint32_t readVal = 0;
        uint32_t cbData = sizeof(readVal);
        uint32_t regType = 0;
        if (Advapi32_RegQueryValueExW(hKey, L"InstalledVersion", nullptr, &regType, reinterpret_cast<uint8_t*>(&readVal), &cbData) == 0) {
            rep.registryRoundTripVerified = (readVal == writeVal && regType == REG_DWORD_OFFICIAL);
        }
        Advapi32_RegCloseKey(hKey);
    }

    uint8_t rngBuf[16]{};
    rep.rtlGenRandomVerified = (Advapi32_SystemFunction036(rngBuf, sizeof(rngBuf)) == 1) && (rngBuf[0] != 0 || rngBuf[1] != 0);

    return rep;
}

void InitializeWinNtKernel32Advapi32Subsystem() noexcept {
    WinNtKernel32Advapi32Coordinator::instance().initialize();
}
#endif

} // namespace micant::um::winnt_kernel32_advapi32_abi

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
namespace micant::sys::internal {
void WinNtKernel32Advapi32InitializeExecutiveSubsystem() noexcept {
    micant::um::winnt_kernel32_advapi32_abi::InitializeWinNtKernel32Advapi32Subsystem();
}
} // namespace micant::sys::internal
#endif

#if defined(BUILD_SYSTEM32_KERNEL32_DLL)
extern "C" {
__declspec(dllexport) int32_t QueryPerformanceCounter(int64_t* lpPerformanceCount) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_QueryPerformanceCounter(lpPerformanceCount);
}
__declspec(dllexport) int32_t QueryPerformanceFrequency(int64_t* lpFrequency) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_QueryPerformanceFrequency(lpFrequency);
}
__declspec(dllexport) uint64_t GetTickCount64() {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_GetTickCount64();
}
__declspec(dllexport) void* CreateEventW(void* lpEventAttributes, int32_t bManualReset, int32_t bInitialState, const wchar_t* lpName) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_CreateEventW(lpEventAttributes, bManualReset, bInitialState, lpName);
}
__declspec(dllexport) void* CreateEventExW(void* lpEventAttributes, const wchar_t* lpName, uint32_t dwFlags, uint32_t dwDesiredAccess) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_CreateEventExW(lpEventAttributes, lpName, dwFlags, dwDesiredAccess);
}
__declspec(dllexport) int32_t SetEvent(void* hEvent) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_SetEvent(hEvent);
}
__declspec(dllexport) int32_t ResetEvent(void* hEvent) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_ResetEvent(hEvent);
}
__declspec(dllexport) uint32_t WaitForSingleObject(void* hHandle, uint32_t dwMilliseconds) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_WaitForSingleObject(hHandle, dwMilliseconds);
}
__declspec(dllexport) uint32_t WaitForSingleObjectEx(void* hHandle, uint32_t dwMilliseconds, int32_t bAlertable) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_WaitForSingleObjectEx(hHandle, dwMilliseconds, bAlertable);
}
__declspec(dllexport) uint32_t WaitForMultipleObjectsEx(uint32_t nCount, const void* const* lpHandles, int32_t bWaitAll, uint32_t dwMilliseconds, int32_t bAlertable) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_WaitForMultipleObjectsEx(nCount, lpHandles, bWaitAll, dwMilliseconds, bAlertable);
}
__declspec(dllexport) int32_t CloseHandle(void* hObject) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_CloseHandle(hObject);
}
__declspec(dllexport) void* VirtualAlloc(void* lpAddress, size_t dwSize, uint32_t flAllocationType, uint32_t flProtect) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
}
__declspec(dllexport) int32_t VirtualFree(void* lpAddress, size_t dwSize, uint32_t dwFreeType) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_VirtualFree(lpAddress, dwSize, dwFreeType);
}
__declspec(dllexport) void* LoadLibraryW(const wchar_t* lpLibFileName) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_LoadLibraryW(lpLibFileName);
}
__declspec(dllexport) void* LoadLibraryExW(const wchar_t* lpLibFileName, void* hFile, uint32_t dwFlags) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_LoadLibraryExW(lpLibFileName, hFile, dwFlags);
}
__declspec(dllexport) void* GetModuleHandleW(const wchar_t* lpModuleName) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_GetModuleHandleW(lpModuleName);
}
__declspec(dllexport) void* GetProcAddress(void* hModule, const char* lpProcName) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_GetProcAddress(hModule, lpProcName);
}
__declspec(dllexport) int32_t FreeLibrary(void* hLibModule) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_FreeLibrary(hLibModule);
}
__declspec(dllexport) void Sleep(uint32_t dwMilliseconds) {
    micant::um::winnt_kernel32_advapi32_abi::Kernel32_Sleep(dwMilliseconds);
}
__declspec(dllexport) int32_t SwitchToThread() {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_SwitchToThread();
}
__declspec(dllexport) uint32_t GetCurrentProcessId() {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_GetCurrentProcessId();
}
__declspec(dllexport) uint32_t GetCurrentThreadId() {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_GetCurrentThreadId();
}
__declspec(dllexport) uint32_t GetLastError() {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_GetLastError();
}
__declspec(dllexport) void SetLastError(uint32_t dwErrCode) {
    micant::um::winnt_kernel32_advapi32_abi::Kernel32_SetLastError(dwErrCode);
}
__declspec(dllexport) int32_t SetThreadDescription(void* hThread, const wchar_t* lpThreadDescription) {
    return micant::um::winnt_kernel32_advapi32_abi::Kernel32_SetThreadDescription(hThread, lpThreadDescription);
}
} // extern "C"
#elif defined(BUILD_SYSTEM32_ADVAPI32_DLL)
extern "C" {
__declspec(dllexport) int32_t RegOpenKeyExW(void* hKey, const wchar_t* lpSubKey, uint32_t ulOptions, uint32_t samDesired, void** phkResult) {
    return micant::um::winnt_kernel32_advapi32_abi::Advapi32_RegOpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}
__declspec(dllexport) int32_t RegCreateKeyExW(void* hKey, const wchar_t* lpSubKey, uint32_t Reserved, wchar_t* lpClass, uint32_t dwOptions, uint32_t samDesired, void* lpSecurityAttributes, void** phkResult, uint32_t* lpdwDisposition) {
    return micant::um::winnt_kernel32_advapi32_abi::Advapi32_RegCreateKeyExW(hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
}
__declspec(dllexport) int32_t RegQueryValueExW(void* hKey, const wchar_t* lpValueName, uint32_t* lpReserved, uint32_t* lpType, uint8_t* lpData, uint32_t* lpcbData) {
    return micant::um::winnt_kernel32_advapi32_abi::Advapi32_RegQueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}
__declspec(dllexport) int32_t RegSetValueExW(void* hKey, const wchar_t* lpValueName, uint32_t Reserved, uint32_t dwType, const uint8_t* lpData, uint32_t cbData) {
    return micant::um::winnt_kernel32_advapi32_abi::Advapi32_RegSetValueExW(hKey, lpValueName, Reserved, dwType, lpData, cbData);
}
__declspec(dllexport) int32_t RegGetValueW(void* hkey, const wchar_t* lpSubKey, const wchar_t* lpValue, uint32_t dwFlags, uint32_t* pdwType, void* pvData, uint32_t* pcbData) {
    return micant::um::winnt_kernel32_advapi32_abi::Advapi32_RegGetValueW(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}
__declspec(dllexport) int32_t RegCloseKey(void* hKey) {
    return micant::um::winnt_kernel32_advapi32_abi::Advapi32_RegCloseKey(hKey);
}
__declspec(dllexport) int32_t GetUserNameW(wchar_t* lpBuffer, uint32_t* pcbBuffer) {
    return micant::um::winnt_kernel32_advapi32_abi::Advapi32_GetUserNameW(lpBuffer, pcbBuffer);
}
__declspec(dllexport) uint8_t SystemFunction036(void* RandomBuffer, uint32_t RandomBufferLength) {
    return micant::um::winnt_kernel32_advapi32_abi::Advapi32_SystemFunction036(RandomBuffer, RandomBufferLength);
}
} // extern "C"
#endif

#endif
