#pragma once

#include <cstdint>
#include <string_view>
#include <concepts>
#include "ntstatus.hpp"

namespace micant {

using Handle = intptr_t;
inline constexpr Handle InvalidHandleValue = -1;

/**
 * @brief Standard NT counted Unicode string.
 * Length and MaximumLength are stored in bytes, not wchar_t count.
 */
struct UnicodeString {
    uint16_t length{0};         // Length in bytes, excluding null terminator
    uint16_t maximumLength{0};  // Total buffer allocation in bytes
    const wchar_t* buffer{nullptr};

    [[nodiscard]] constexpr std::wstring_view view() const noexcept {
        if (!buffer || length == 0) return {};
        return std::wstring_view(buffer, length / sizeof(wchar_t));
    }
};

/**
 * @brief 64-bit QuadPart integer representation.
 */
union LargeInteger {
    struct {
        uint32_t lowPart;
        int32_t highPart;
    };
    struct {
        uint32_t lowPart;
        int32_t highPart;
    } u;
    int64_t quadPart{0};
};

/**
 * @brief Client identifier (Unique Process ID and Thread ID).
 */
struct ClientId {
    Handle uniqueProcess{0};
    Handle uniqueThread{0};
};

/**
 * @brief Standard NT Object Attributes for kernel object instantiation.
 */
struct ObjectAttributes {
    uint32_t length{sizeof(ObjectAttributes)};
    Handle rootDirectory{0};
    const UnicodeString* objectName{nullptr};
    uint32_t attributes{0};
    void* securityDescriptor{nullptr};
    void* securityQualityOfService{nullptr};
};

// Object attribute flags
inline constexpr uint32_t OBJ_INHERIT             = 0x00000002;
inline constexpr uint32_t OBJ_PERMANENT           = 0x00000010;
inline constexpr uint32_t OBJ_EXCLUSIVE           = 0x00000020;
inline constexpr uint32_t OBJ_CASE_INSENSITIVE     = 0x00000040;
inline constexpr uint32_t OBJ_OPENIF              = 0x00000080;
inline constexpr uint32_t OBJ_OPENLINK            = 0x00000100;
inline constexpr uint32_t OBJ_KERNEL_HANDLE       = 0x00000200;
inline constexpr uint32_t OBJ_FORCE_ACCESS_CHECK   = 0x00000400;

/**
 * @brief I/O Status Block for asynchronous I/O completion.
 */
struct IoStatusBlock {
    union {
        NtStatus status;
        void* pointer;
    };
    uintptr_t information{0};
};

/**
 * @brief Standard KUSER_SHARED_DATA memory region.
 * On 64-bit Windows, mapped at fixed virtual address 0x000000007FFE0000.
 * Read-only in userland, read/write in kernel.
 */
struct KUserSharedData {
    uint32_t tickCountLowDeprecated;
    uint32_t tickCountMultiplier;
    volatile uint32_t interruptTimeLow;
    volatile int32_t interruptTimeHigh1;
    volatile int32_t interruptTimeHigh2;
    volatile uint32_t systemTimeLow;
    volatile int32_t systemTimeHigh1;
    volatile int32_t systemTimeHigh2;
    volatile uint32_t timeZoneBiasLow;
    volatile int32_t timeZoneBiasHigh1;
    volatile int32_t timeZoneBiasHigh2;
    uint16_t imageNumberLow;
    uint16_t imageNumberHigh;
    wchar_t ntSystemRoot[260];
    uint32_t maxStackTraceDepth;
    uint32_t cryptoExponent;
    uint32_t timeZoneId;
    uint32_t largePageMinimum;
    uint32_t aitSamplingValue;
    uint32_t appCompatFlag;
    uint64_t rngSeedVersion;
    uint32_t globalValidationRunlevel;
    volatile int32_t timeZoneBiasStamp;
    uint32_t ntBuildNumber;
    uint32_t ntProductType;
    uint8_t productType;
    uint8_t nativeProcessorArchitecture;
    uint16_t ntMajorVersion;
    uint16_t ntMinorVersion;
    uint8_t processorFeatures[64];
};

inline constexpr uintptr_t UserSharedDataAddress = 0x7FFE0000ULL;

} // namespace micant
