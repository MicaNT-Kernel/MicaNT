#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include "ntstatus.hpp"
#include "ntdef.hpp"

namespace micant::sys {

// Standard x86-64 NT System Service Numbers (SSNs)
// Aligned with Windows 10/11 x64 dispatch indices
inline constexpr uint32_t SSN_NtCreateFile               = 0x0055;
inline constexpr uint32_t SSN_NtOpenFile                 = 0x0033;
inline constexpr uint32_t SSN_NtReadFile                 = 0x0006;
inline constexpr uint32_t SSN_NtWriteFile                = 0x0008;
inline constexpr uint32_t SSN_NtDeviceIoControlFile       = 0x0007;
inline constexpr uint32_t SSN_NtClose                    = 0x000F;
inline constexpr uint32_t SSN_NtAllocateVirtualMemory    = 0x0018;
inline constexpr uint32_t SSN_NtFreeVirtualMemory        = 0x001E;
inline constexpr uint32_t SSN_NtProtectVirtualMemory     = 0x0050;
inline constexpr uint32_t SSN_NtQueryVirtualMemory       = 0x0023;
inline constexpr uint32_t SSN_NtCreateProcessEx          = 0x00B4;
inline constexpr uint32_t SSN_NtTerminateProcess         = 0x002C;
inline constexpr uint32_t SSN_NtCreateThreadEx           = 0x00BD;
inline constexpr uint32_t SSN_NtTerminateThread          = 0x0053;
inline constexpr uint32_t SSN_NtWaitForSingleObject      = 0x0004;
inline constexpr uint32_t SSN_NtQuerySystemInformation   = 0x0036;

// Synchronization & IOCP SSNs
inline constexpr uint32_t SSN_NtCreateEvent              = 0x0048;
inline constexpr uint32_t SSN_NtSetEvent                 = 0x004E;
inline constexpr uint32_t SSN_NtResetEvent               = 0x004F;
inline constexpr uint32_t SSN_NtCreateMutant             = 0x00B2;
inline constexpr uint32_t SSN_NtReleaseMutant            = 0x001D;
inline constexpr uint32_t SSN_NtCreateIoCompletion       = 0x0164;
inline constexpr uint32_t SSN_NtSetIoCompletion          = 0x0165;
inline constexpr uint32_t SSN_NtRemoveIoCompletion       = 0x0009;

// Configuration Manager (Registry) SSNs
inline constexpr uint32_t SSN_NtCreateKey                = 0x0029;
inline constexpr uint32_t SSN_NtOpenKey                  = 0x0012;
inline constexpr uint32_t SSN_NtQueryValueKey            = 0x0016;
inline constexpr uint32_t SSN_NtSetValueKey              = 0x0060;

// Security Reference Monitor SSNs
inline constexpr uint32_t SSN_NtOpenProcessToken         = 0x00BE;
inline constexpr uint32_t SSN_NtAccessCheck              = 0x0182;

// Advanced Local Procedure Call (ALPC) SSNs
inline constexpr uint32_t SSN_NtCreatePort               = 0x0093;
inline constexpr uint32_t SSN_NtConnectPort              = 0x0096;
inline constexpr uint32_t SSN_NtRequestWaitReplyPort     = 0x0022;

/**
 * @brief Native NT Syscall Signatures in Modern C++23
 */

// Memory Management
NtStatus NtAllocateVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    uintptr_t zeroBits,
    size_t* regionSize,
    uint32_t allocationType,
    uint32_t protect
);

NtStatus NtFreeVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    size_t* regionSize,
    uint32_t freeType
);

NtStatus NtProtectVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    size_t* regionSize,
    uint32_t newProtect,
    uint32_t* oldProtect
);

// File & I/O
NtStatus NtCreateFile(
    Handle* fileHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    IoStatusBlock* ioStatusBlock,
    LargeInteger* allocationSize,
    uint32_t fileAttributes,
    uint32_t shareAccess,
    uint32_t createDisposition,
    uint32_t createOptions,
    void* eaBuffer,
    uint32_t eaLength
);

NtStatus NtOpenFile(
    Handle* fileHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    IoStatusBlock* ioStatusBlock,
    uint32_t shareAccess,
    uint32_t openOptions
);

NtStatus NtReadFile(
    Handle fileHandle,
    Handle event,
    void* apcRoutine,
    void* apcContext,
    IoStatusBlock* ioStatusBlock,
    void* buffer,
    uint32_t length,
    LargeInteger* byteOffset,
    uint32_t* key
);

NtStatus NtWriteFile(
    Handle fileHandle,
    Handle event,
    void* apcRoutine,
    void* apcContext,
    IoStatusBlock* ioStatusBlock,
    const void* buffer,
    uint32_t length,
    LargeInteger* byteOffset,
    uint32_t* key
);

NtStatus NtDeviceIoControlFile(
    Handle fileHandle,
    Handle event,
    void* apcRoutine,
    void* apcContext,
    IoStatusBlock* ioStatusBlock,
    uint32_t ioControlCode,
    const void* inputBuffer,
    uint32_t inputBufferLength,
    void* outputBuffer,
    uint32_t outputBufferLength
);

NtStatus NtClose(Handle handle);

// Process & Thread
NtStatus NtTerminateProcess(
    Handle processHandle,
    NtStatus exitStatus
);

NtStatus NtWaitForSingleObject(
    Handle handle,
    bool alertable,
    LargeInteger* timeout
);

NtStatus NtQuerySystemInformation(
    uint32_t systemInformationClass,
    void* systemInformation,
    uint32_t systemInformationLength,
    uint32_t* returnLength
);

} // namespace micant::sys
