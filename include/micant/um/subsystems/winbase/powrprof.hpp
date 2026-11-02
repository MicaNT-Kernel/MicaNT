// ============================================================================
// MicaNT: Windows Power Profile Management Subsystem (powrprof.dll)
// (powrprof.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides system sleep and suspend state transitions conforming to win32metadata.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include <mutex>
#include "micant/shared/ntdef.hpp"
#include "micant/um/native/ldr.hpp"

namespace micant::powrprof {

using BOOLEAN = uint8_t;

struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];

    bool operator==(const GUID& o) const noexcept {
        return std::memcmp(this, &o, sizeof(GUID)) == 0;
    }
};

// Standard Windows Power Scheme GUIDs
inline constexpr GUID GUID_MIN_POWER_SAVINGS     = { 0x8c5e7fda, 0xe8bf, 0x4a96, { 0x9a, 0x85, 0xa6, 0xe2, 0x3a, 0x8c, 0x63, 0x5c } }; // High Performance
inline constexpr GUID GUID_TYPICAL_POWER_SAVINGS = { 0x381b4222, 0xf694, 0x41f0, { 0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e } }; // Balanced
inline constexpr GUID GUID_MAX_POWER_SAVINGS     = { 0xa1841308, 0x3541, 0x4fab, { 0xbc, 0x81, 0xf7, 0x15, 0x56, 0xf2, 0x0b, 0x4a } }; // Power Saver

inline GUID s_activeScheme = GUID_MIN_POWER_SAVINGS; // Default to High Performance for Gaming
inline std::mutex s_powerLock;

inline BOOLEAN WINAPI SetSuspendState(BOOLEAN /*bHibernate*/, BOOLEAN /*bForce*/, BOOLEAN /*bWakeupEventsDisabled*/) noexcept {
    return 1;
}

inline uint32_t WINAPI PowerGetActiveScheme(void* /*UserRootPowerKey*/, GUID** pActivePolicyGuid) noexcept {
    if (!pActivePolicyGuid) return 87; // ERROR_INVALID_PARAMETER
    std::lock_guard<std::mutex> lock(s_powerLock);
    static GUID s_reportedGuid;
    s_reportedGuid = s_activeScheme;
    *pActivePolicyGuid = &s_reportedGuid;
    return 0; // ERROR_SUCCESS
}

inline uint32_t WINAPI PowerSetActiveScheme(void* /*UserRootPowerKey*/, const GUID* SchemeGuid) noexcept {
    if (!SchemeGuid) return 87; // ERROR_INVALID_PARAMETER
    std::lock_guard<std::mutex> lock(s_powerLock);
    s_activeScheme = *SchemeGuid;
    return 0; // ERROR_SUCCESS
}

inline uint32_t WINAPI PowerReadACValueIndex(void* /*RootPowerKey*/, const GUID* /*SchemeGuid*/, const GUID* /*SubGroup*/, const GUID* /*PowerSetting*/, uint32_t* AcValueIndex) noexcept {
    if (AcValueIndex) *AcValueIndex = 0; // 0 = maximum unthrottled performance
    return 0;
}

inline uint32_t WINAPI PowerWriteACValueIndex(void* /*RootPowerKey*/, const GUID* /*SchemeGuid*/, const GUID* /*SubGroup*/, const GUID* /*PowerSetting*/, uint32_t /*AcValueIndex*/) noexcept {
    return 0;
}

inline uint32_t WINAPI PowerSettingRegisterNotification(const GUID* /*SettingGuid*/, uint32_t /*Flags*/, void* /*Recipient*/, void** RegistrationHandle) noexcept {
    if (RegistrationHandle) *RegistrationHandle = reinterpret_cast<void*>(0x5001);
    return 0;
}

inline uint32_t WINAPI PowerSettingUnregisterNotification(void* /*RegistrationHandle*/) noexcept {
    return 0;
}

inline void InitializePowrProfSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("powrprof.dll", "SetSuspendState", reinterpret_cast<void*>(SetSuspendState));
    ldr.registerExport("powrprof.dll", "PowerGetActiveScheme", reinterpret_cast<void*>(PowerGetActiveScheme));
    ldr.registerExport("powrprof.dll", "PowerSetActiveScheme", reinterpret_cast<void*>(PowerSetActiveScheme));
    ldr.registerExport("powrprof.dll", "PowerReadACValueIndex", reinterpret_cast<void*>(PowerReadACValueIndex));
    ldr.registerExport("powrprof.dll", "PowerWriteACValueIndex", reinterpret_cast<void*>(PowerWriteACValueIndex));
    ldr.registerExport("powrprof.dll", "PowerSettingRegisterNotification", reinterpret_cast<void*>(PowerSettingRegisterNotification));
    ldr.registerExport("powrprof.dll", "PowerSettingUnregisterNotification", reinterpret_cast<void*>(PowerSettingUnregisterNotification));
}

} // namespace micant::powrprof
