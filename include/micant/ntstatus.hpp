#pragma once

#include <cstdint>
#include <string_view>

namespace micant {

/**
 * @brief Standard Windows NT Status Code (NTSTATUS) representation.
 * 32-bit value structured as:
 *   Bits 31-30: Severity (00 = Success, 01 = Info, 10 = Warning, 11 = Error)
 *   Bit 29: Customer / Reserved
 *   Bit 28: Reserved
 *   Bits 27-16: Facility code
 *   Bits 15-0: Status code
 */
enum class NtStatus : uint32_t {
    // 0x00000000 - Success
    Success                          = 0x00000000,
    Wait0                            = 0x00000000,
    Wait1                            = 0x00000001,
    Abandoned                        = 0x00000080,
    UserApc                          = 0x000000C0,
    Timeout                          = 0x00000102,
    Pending                          = 0x00000103,

    // 0x40000000 - Informational
    BufferOverflow                   = 0x80000005,
    NoMoreFiles                      = 0x80000006,
    HandlesClosed                    = 0x8000000A,

    // 0xC0000000 - Errors
    Unsuccessful                     = 0xC0000001,
    NotImplemented                   = 0xC0000002,
    InvalidInfoClass                 = 0xC0000003,
    InfoLengthMismatch               = 0xC0000004,
    AccessViolation                  = 0xC0000005,
    InPageError                      = 0xC0000006,
    PagefileQuota                    = 0xC0000007,
    InvalidHandle                    = 0xC0000008,
    BadInitialStack                  = 0xC0000009,
    BadInitialPc                     = 0xC000000A,
    InvalidCid                       = 0xC000000B,
    InvalidParameter                 = 0xC000000D,
    NoSuchDevice                     = 0xC000000E,
    NoSuchFile                       = 0xC000000F,
    InvalidDeviceRequest             = 0xC0000010,
    EndOfFile                        = 0xC0000011,
    NoMemory                         = 0xC0000017,
    Conflict                         = 0xC0000018,
    AccessDenied                     = 0xC0000022,
    BufferTooSmall                   = 0xC0000023,
    ObjectNameNotFound               = 0xC0000034,
    ObjectNameCollision              = 0xC0000035,
    ObjectPathNotFound               = 0xC000003A,
    ObjectPathSyntaxBad              = 0xC000003B,
    SectionTooBig                    = 0xC0000040,
    PortConnectionRefused            = 0xC0000041,
    ProcessIsTerminating             = 0xC000010A,
    PrivilegeNotHeld                 = 0xC0000061
};

// Standard NT macro semantics evaluated constexpr
[[nodiscard]] constexpr bool NT_SUCCESS(NtStatus status) noexcept {
    return static_cast<int32_t>(status) >= 0;
}

[[nodiscard]] constexpr bool NT_INFORMATION(NtStatus status) noexcept {
    return (static_cast<uint32_t>(status) >> 30) == 1;
}

[[nodiscard]] constexpr bool NT_WARNING(NtStatus status) noexcept {
    return (static_cast<uint32_t>(status) >> 30) == 2;
}

[[nodiscard]] constexpr bool NT_ERROR(NtStatus status) noexcept {
    return (static_cast<uint32_t>(status) >> 30) == 3;
}

[[nodiscard]] constexpr std::string_view NtStatusToString(NtStatus status) noexcept {
    switch (status) {
        case NtStatus::Success: return "STATUS_SUCCESS";
        case NtStatus::Timeout: return "STATUS_TIMEOUT";
        case NtStatus::Pending: return "STATUS_PENDING";
        case NtStatus::BufferOverflow: return "STATUS_BUFFER_OVERFLOW";
        case NtStatus::NoMoreFiles: return "STATUS_NO_MORE_FILES";
        case NtStatus::Unsuccessful: return "STATUS_UNSUCCESSFUL";
        case NtStatus::NotImplemented: return "STATUS_NOT_IMPLEMENTED";
        case NtStatus::AccessViolation: return "STATUS_ACCESS_VIOLATION";
        case NtStatus::InvalidHandle: return "STATUS_INVALID_HANDLE";
        case NtStatus::InvalidParameter: return "STATUS_INVALID_PARAMETER";
        case NtStatus::NoSuchFile: return "STATUS_NO_SUCH_FILE";
        case NtStatus::NoMemory: return "STATUS_INSUFFICIENT_RESOURCES";
        case NtStatus::AccessDenied: return "STATUS_ACCESS_DENIED";
        case NtStatus::BufferTooSmall: return "STATUS_BUFFER_TOO_SMALL";
        case NtStatus::ObjectNameNotFound: return "STATUS_OBJECT_NAME_NOT_FOUND";
        case NtStatus::ObjectNameCollision: return "STATUS_OBJECT_NAME_COLLISION";
        case NtStatus::ObjectPathNotFound: return "STATUS_OBJECT_PATH_NOT_FOUND";
        case NtStatus::ProcessIsTerminating: return "STATUS_PROCESS_IS_TERMINATING";
        default: return "STATUS_UNKNOWN";
    }
}

} // namespace micant
