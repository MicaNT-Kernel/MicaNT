#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <span>
#include <cwchar>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "dispatcher.hpp"
#include "heap.hpp"
#include "ldr.hpp"

namespace micant::ntdll {

// Re-export Heap API into ntdll namespace
using heap::RtlCreateHeap;
using heap::RtlAllocateHeap;
using heap::RtlFreeHeap;
using heap::RtlDestroyHeap;
using heap::RtlSizeHeap;
using heap::RtlReAllocateHeap;
using heap::HEAP_NO_SERIALIZE;
using heap::HEAP_GROWABLE;
using heap::HEAP_GENERATE_EXCEPTIONS;
using heap::HEAP_ZERO_MEMORY;
using heap::HEAP_REALLOC_IN_PLACE_ONLY;

// Re-export Loader API into ntdll namespace
using ldr::LdrInitializeThunk;
using ldr::LdrLoadDll;
using ldr::LdrGetProcedureAddress;
using ldr::DynamicLoader;

// Thread-local Userland TEB Context Pointer
inline thread_local ps::Teb* g_CurrentTeb = nullptr;

[[nodiscard]] inline ps::Teb* RtlGetCurrentTeb() noexcept {
    return g_CurrentTeb;
}

[[nodiscard]] inline ps::Peb* RtlGetCurrentPeb() noexcept {
    if (!g_CurrentTeb) return nullptr;
    return reinterpret_cast<ps::Peb*>(g_CurrentTeb->processEnvironmentBlock);
}

inline void RtlSetCurrentTeb(ps::Teb* teb) noexcept {
    g_CurrentTeb = teb;
}

// ============================================================================
// NTDLL Clean-Room System Call Thunks (All 33 KiSystemCall64 Services)
// ============================================================================

// 1. NtAllocateVirtualMemory (SSN: 0x0018)
inline NtStatus NtAllocateVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    uintptr_t zeroBits,
    size_t* regionSize,
    uint32_t allocationType,
    uint32_t protect
) {
    uint64_t stack[2] = { allocationType, protect };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtAllocateVirtualMemory,
        .arg1 = static_cast<uint64_t>(processHandle),
        .arg2 = reinterpret_cast<uint64_t>(baseAddress),
        .arg3 = zeroBits,
        .arg4 = reinterpret_cast<uint64_t>(regionSize),
        .stackArgs = stack,
        .stackArgCount = 2
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 2. NtFreeVirtualMemory (SSN: 0x001E)
inline NtStatus NtFreeVirtualMemory(
    Handle processHandle,
    uintptr_t* baseAddress,
    size_t* regionSize,
    uint32_t freeType
) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtFreeVirtualMemory,
        .arg1 = static_cast<uint64_t>(processHandle),
        .arg2 = reinterpret_cast<uint64_t>(baseAddress),
        .arg3 = reinterpret_cast<uint64_t>(regionSize),
        .arg4 = freeType
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 3. NtClose (SSN: 0x000F)
inline NtStatus NtClose(Handle handle) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtClose,
        .arg1 = static_cast<uint64_t>(handle)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 4. NtTerminateProcess (SSN: 0x002C)
inline NtStatus NtTerminateProcess(Handle processHandle, NtStatus exitStatus) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtTerminateProcess,
        .arg1 = static_cast<uint64_t>(processHandle),
        .arg2 = static_cast<uint64_t>(exitStatus)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 5. NtWaitForSingleObject (SSN: 0x0004)
inline NtStatus NtWaitForSingleObject(Handle handle, bool alertable, LargeInteger* timeout) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtWaitForSingleObject,
        .arg1 = static_cast<uint64_t>(handle),
        .arg2 = alertable ? 1ULL : 0ULL,
        .arg3 = reinterpret_cast<uint64_t>(timeout)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 6. NtQuerySystemInformation (SSN: 0x0036)
inline NtStatus NtQuerySystemInformation(
    uint32_t systemInformationClass,
    void* systemInformation,
    uint32_t systemInformationLength,
    uint32_t* returnLength
) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtQuerySystemInformation,
        .arg1 = systemInformationClass,
        .arg2 = reinterpret_cast<uint64_t>(systemInformation),
        .arg3 = systemInformationLength,
        .arg4 = reinterpret_cast<uint64_t>(returnLength)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 7. NtCreateEvent (SSN: 0x0048)
inline NtStatus NtCreateEvent(
    Handle* eventHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    uint32_t eventType,
    bool initialState
) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtCreateEvent,
        .arg1 = reinterpret_cast<uint64_t>(eventHandle),
        .arg2 = desiredAccess,
        .arg3 = eventType,
        .arg4 = initialState ? 1ULL : 0ULL
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 8. NtSetEvent (SSN: 0x004E)
inline NtStatus NtSetEvent(Handle eventHandle, uint32_t* previousState = nullptr) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtSetEvent,
        .arg1 = static_cast<uint64_t>(eventHandle),
        .arg2 = reinterpret_cast<uint64_t>(previousState)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 9. NtResetEvent (SSN: 0x004F)
inline NtStatus NtResetEvent(Handle eventHandle, uint32_t* previousState = nullptr) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtResetEvent,
        .arg1 = static_cast<uint64_t>(eventHandle),
        .arg2 = reinterpret_cast<uint64_t>(previousState)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 10. NtCreateMutant (SSN: 0x00B2)
inline NtStatus NtCreateMutant(
    Handle* mutantHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    bool initialOwner
) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtCreateMutant,
        .arg1 = reinterpret_cast<uint64_t>(mutantHandle),
        .arg2 = desiredAccess,
        .arg3 = initialOwner ? 1ULL : 0ULL,
        .arg4 = reinterpret_cast<uint64_t>(objectAttributes)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 11. NtReleaseMutant (SSN: 0x001D)
inline NtStatus NtReleaseMutant(Handle mutantHandle, int32_t* previousCount = nullptr) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtReleaseMutant,
        .arg1 = static_cast<uint64_t>(mutantHandle),
        .arg2 = reinterpret_cast<uint64_t>(previousCount)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 12. NtCreateIoCompletion (SSN: 0x0164)
inline NtStatus NtCreateIoCompletion(
    Handle* ioCompletionHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    uint32_t count
) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtCreateIoCompletion,
        .arg1 = reinterpret_cast<uint64_t>(ioCompletionHandle),
        .arg2 = desiredAccess,
        .arg3 = reinterpret_cast<uint64_t>(objectAttributes),
        .arg4 = count
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 13. NtSetIoCompletion (SSN: 0x0165)
inline NtStatus NtSetIoCompletion(
    Handle ioCompletionHandle,
    uint64_t keyContext,
    uintptr_t apcContext,
    NtStatus ioStatus,
    uint32_t ioStatusInformation
) {
    uint64_t stack[1] = { ioStatusInformation };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtSetIoCompletion,
        .arg1 = static_cast<uint64_t>(ioCompletionHandle),
        .arg2 = keyContext,
        .arg3 = apcContext,
        .arg4 = static_cast<uint64_t>(ioStatus),
        .stackArgs = stack,
        .stackArgCount = 1
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 14. NtRemoveIoCompletion (SSN: 0x0009)
inline NtStatus NtRemoveIoCompletion(
    Handle ioCompletionHandle,
    uint64_t* keyContext,
    uintptr_t* apcContext,
    IoStatusBlock* ioStatusBlock,
    LargeInteger* timeout
) {
    uint64_t stack[1] = { reinterpret_cast<uint64_t>(timeout) };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtRemoveIoCompletion,
        .arg1 = static_cast<uint64_t>(ioCompletionHandle),
        .arg2 = reinterpret_cast<uint64_t>(keyContext),
        .arg3 = reinterpret_cast<uint64_t>(apcContext),
        .arg4 = reinterpret_cast<uint64_t>(ioStatusBlock),
        .stackArgs = stack,
        .stackArgCount = 1
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 15. NtOpenKey (SSN: 0x0012)
inline NtStatus NtOpenKey(
    Handle* keyHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes
) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtOpenKey,
        .arg1 = reinterpret_cast<uint64_t>(keyHandle),
        .arg2 = desiredAccess,
        .arg3 = reinterpret_cast<uint64_t>(objectAttributes)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 16. NtQueryValueKey (SSN: 0x0016)
inline NtStatus NtQueryValueKey(
    Handle keyHandle,
    UnicodeString* valueName,
    uint32_t keyValueInformationClass,
    void* keyValueInformation,
    size_t length,
    size_t* resultLength
) {
    uint64_t stack[2] = { length, reinterpret_cast<uint64_t>(resultLength) };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtQueryValueKey,
        .arg1 = static_cast<uint64_t>(keyHandle),
        .arg2 = reinterpret_cast<uint64_t>(valueName),
        .arg3 = keyValueInformationClass,
        .arg4 = reinterpret_cast<uint64_t>(keyValueInformation),
        .stackArgs = stack,
        .stackArgCount = 2
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 17. NtSetValueKey (SSN: 0x0060)
inline NtStatus NtSetValueKey(
    Handle keyHandle,
    UnicodeString* valueName,
    uint32_t titleIndex,
    uint32_t type,
    const void* data,
    uint32_t dataSize
) {
    uint64_t stack[2] = { reinterpret_cast<uint64_t>(data), dataSize };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtSetValueKey,
        .arg1 = static_cast<uint64_t>(keyHandle),
        .arg2 = reinterpret_cast<uint64_t>(valueName),
        .arg3 = titleIndex,
        .arg4 = type,
        .stackArgs = stack,
        .stackArgCount = 2
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 18. NtOpenProcessToken (SSN: 0x00BE)
inline NtStatus NtOpenProcessToken(
    Handle processHandle,
    uint32_t desiredAccess,
    Handle* tokenHandle
) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtOpenProcessToken,
        .arg1 = static_cast<uint64_t>(processHandle),
        .arg2 = desiredAccess,
        .arg3 = reinterpret_cast<uint64_t>(tokenHandle)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 19. NtAccessCheck (SSN: 0x0182)
inline NtStatus NtAccessCheck(
    void* securityDescriptor,
    Handle clientToken,
    uint32_t desiredAccess,
    uint32_t* grantedAccess,
    NtStatus* accessStatus = nullptr
) {
    uint64_t stack[1] = { reinterpret_cast<uint64_t>(accessStatus) };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtAccessCheck,
        .arg1 = reinterpret_cast<uint64_t>(securityDescriptor),
        .arg2 = static_cast<uint64_t>(clientToken),
        .arg3 = desiredAccess,
        .arg4 = reinterpret_cast<uint64_t>(grantedAccess),
        .stackArgs = stack,
        .stackArgCount = 1
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 20. NtCreatePort (SSN: 0x0093)
inline NtStatus NtCreatePort(
    Handle* portHandle,
    ObjectAttributes* objectAttributes,
    uint32_t maxConnectionInfoLength = 0,
    uint32_t maxMessageLength = 256
) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtCreatePort,
        .arg1 = reinterpret_cast<uint64_t>(portHandle),
        .arg2 = reinterpret_cast<uint64_t>(objectAttributes),
        .arg3 = maxConnectionInfoLength,
        .arg4 = maxMessageLength
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 21. NtConnectPort (SSN: 0x0096)
inline NtStatus NtConnectPort(
    Handle* portHandle,
    UnicodeString* portName,
    void* securityQos = nullptr,
    void* clientView = nullptr
) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtConnectPort,
        .arg1 = reinterpret_cast<uint64_t>(portHandle),
        .arg2 = reinterpret_cast<uint64_t>(portName),
        .arg3 = reinterpret_cast<uint64_t>(securityQos),
        .arg4 = reinterpret_cast<uint64_t>(clientView)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 22. NtRequestWaitReplyPort (SSN: 0x0022)
inline NtStatus NtRequestWaitReplyPort(
    Handle portHandle,
    void* requestMessage,
    void* replyMessage
) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtRequestWaitReplyPort,
        .arg1 = static_cast<uint64_t>(portHandle),
        .arg2 = reinterpret_cast<uint64_t>(requestMessage),
        .arg3 = reinterpret_cast<uint64_t>(replyMessage)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 23. NtCreateFile (SSN: 0x0055)
inline NtStatus NtCreateFile(
    Handle* fileHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    IoStatusBlock* ioStatusBlock,
    LargeInteger* allocationSize = nullptr,
    uint32_t fileAttributes = 0,
    uint32_t shareAccess = 0,
    uint32_t createDisposition = 1,
    uint32_t createOptions = 0,
    void* eaBuffer = nullptr,
    uint32_t eaLength = 0
) {
    uint64_t stack[7] = {
        reinterpret_cast<uint64_t>(allocationSize),
        fileAttributes,
        shareAccess,
        createDisposition,
        createOptions,
        reinterpret_cast<uint64_t>(eaBuffer),
        eaLength
    };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtCreateFile,
        .arg1 = reinterpret_cast<uint64_t>(fileHandle),
        .arg2 = desiredAccess,
        .arg3 = reinterpret_cast<uint64_t>(objectAttributes),
        .arg4 = reinterpret_cast<uint64_t>(ioStatusBlock),
        .stackArgs = stack,
        .stackArgCount = 7
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 24. NtOpenFile (SSN: 0x0033)
inline NtStatus NtOpenFile(
    Handle* fileHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    IoStatusBlock* ioStatusBlock,
    uint32_t shareAccess = 0,
    uint32_t openOptions = 0
) {
    uint64_t stack[2] = { shareAccess, openOptions };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtOpenFile,
        .arg1 = reinterpret_cast<uint64_t>(fileHandle),
        .arg2 = desiredAccess,
        .arg3 = reinterpret_cast<uint64_t>(objectAttributes),
        .arg4 = reinterpret_cast<uint64_t>(ioStatusBlock),
        .stackArgs = stack,
        .stackArgCount = 2
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 25. NtReadFile (SSN: 0x0006)
inline NtStatus NtReadFile(
    Handle fileHandle,
    Handle event,
    void* apcRoutine,
    void* apcContext,
    IoStatusBlock* ioStatusBlock,
    void* buffer,
    uint32_t length,
    LargeInteger* byteOffset = nullptr,
    uint32_t* key = nullptr
) {
    uint64_t stack[5] = {
        reinterpret_cast<uint64_t>(ioStatusBlock),
        reinterpret_cast<uint64_t>(buffer),
        length,
        reinterpret_cast<uint64_t>(byteOffset),
        reinterpret_cast<uint64_t>(key)
    };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtReadFile,
        .arg1 = static_cast<uint64_t>(fileHandle),
        .arg2 = static_cast<uint64_t>(event),
        .arg3 = reinterpret_cast<uint64_t>(apcRoutine),
        .arg4 = reinterpret_cast<uint64_t>(apcContext),
        .stackArgs = stack,
        .stackArgCount = 5
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 26. NtWriteFile (SSN: 0x0008)
inline NtStatus NtWriteFile(
    Handle fileHandle,
    Handle event,
    void* apcRoutine,
    void* apcContext,
    IoStatusBlock* ioStatusBlock,
    const void* buffer,
    uint32_t length,
    LargeInteger* byteOffset = nullptr,
    uint32_t* key = nullptr
) {
    uint64_t stack[5] = {
        reinterpret_cast<uint64_t>(ioStatusBlock),
        reinterpret_cast<uint64_t>(buffer),
        length,
        reinterpret_cast<uint64_t>(byteOffset),
        reinterpret_cast<uint64_t>(key)
    };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtWriteFile,
        .arg1 = static_cast<uint64_t>(fileHandle),
        .arg2 = static_cast<uint64_t>(event),
        .arg3 = reinterpret_cast<uint64_t>(apcRoutine),
        .arg4 = reinterpret_cast<uint64_t>(apcContext),
        .stackArgs = stack,
        .stackArgCount = 5
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 27. NtDeviceIoControlFile (SSN: 0x0007)
inline NtStatus NtDeviceIoControlFile(
    Handle fileHandle,
    Handle event,
    void* apcRoutine,
    void* apcContext,
    IoStatusBlock* ioStatusBlock,
    uint32_t ioControlCode,
    const void* inputBuffer = nullptr,
    uint32_t inputBufferLength = 0,
    void* outputBuffer = nullptr,
    uint32_t outputBufferLength = 0
) {
    uint64_t stack[6] = {
        reinterpret_cast<uint64_t>(ioStatusBlock),
        ioControlCode,
        reinterpret_cast<uint64_t>(inputBuffer),
        inputBufferLength,
        reinterpret_cast<uint64_t>(outputBuffer),
        outputBufferLength
    };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtDeviceIoControlFile,
        .arg1 = static_cast<uint64_t>(fileHandle),
        .arg2 = static_cast<uint64_t>(event),
        .arg3 = reinterpret_cast<uint64_t>(apcRoutine),
        .arg4 = reinterpret_cast<uint64_t>(apcContext),
        .stackArgs = stack,
        .stackArgCount = 6
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 28. NtWaitForMultipleObjects (SSN: 0x005A)
inline NtStatus NtWaitForMultipleObjects(
    uint32_t count,
    const Handle* handles,
    WaitType waitType,
    bool alertable,
    LargeInteger* timeout = nullptr
) {
    uint64_t stack[1] = { reinterpret_cast<uint64_t>(timeout) };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtWaitForMultipleObjects,
        .arg1 = count,
        .arg2 = reinterpret_cast<uint64_t>(handles),
        .arg3 = static_cast<uint64_t>(waitType),
        .arg4 = alertable ? 1ULL : 0ULL,
        .stackArgs = stack,
        .stackArgCount = 1
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 29. NtDelayExecution (SSN: 0x0034)
inline NtStatus NtDelayExecution(bool alertable, const LargeInteger* interval) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtDelayExecution,
        .arg1 = alertable ? 1ULL : 0ULL,
        .arg2 = reinterpret_cast<uint64_t>(interval)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 30. NtCreateTimer (SSN: 0x0057)
inline NtStatus NtCreateTimer(
    Handle* timerHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    uint32_t timerType
) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtCreateTimer,
        .arg1 = reinterpret_cast<uint64_t>(timerHandle),
        .arg2 = desiredAccess,
        .arg3 = reinterpret_cast<uint64_t>(objectAttributes),
        .arg4 = timerType
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 31. NtSetTimer (SSN: 0x0078)
inline NtStatus NtSetTimer(
    Handle timerHandle,
    LargeInteger* dueTime,
    void* timerApcRoutine = nullptr,
    void* timerContext = nullptr,
    bool resumeTimer = false,
    uint32_t period = 0,
    bool* previousState = nullptr
) {
    uint64_t stack[3] = {
        resumeTimer ? 1ULL : 0ULL,
        period,
        reinterpret_cast<uint64_t>(previousState)
    };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtSetTimer,
        .arg1 = static_cast<uint64_t>(timerHandle),
        .arg2 = reinterpret_cast<uint64_t>(dueTime),
        .arg3 = reinterpret_cast<uint64_t>(timerApcRoutine),
        .arg4 = reinterpret_cast<uint64_t>(timerContext),
        .stackArgs = stack,
        .stackArgCount = 3
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 32. NtCancelTimer (SSN: 0x0077)
inline NtStatus NtCancelTimer(Handle timerHandle, bool* currentSignaledState = nullptr) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtCancelTimer,
        .arg1 = static_cast<uint64_t>(timerHandle),
        .arg2 = reinterpret_cast<uint64_t>(currentSignaledState)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 33. NtShutdownSystem (SSN: 0x0118)
inline NtStatus NtShutdownSystem(uint32_t action) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtShutdownSystem,
        .arg1 = action
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 34. NtCreateSection (SSN: 0x004A)
inline NtStatus NtCreateSection(
    Handle* sectionHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    LargeInteger* maximumSize,
    uint32_t sectionPageProtection,
    uint32_t allocationAttributes,
    Handle fileHandle
) {
    uint64_t stack[3] = {
        sectionPageProtection,
        allocationAttributes,
        static_cast<uint64_t>(fileHandle)
    };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtCreateSection,
        .arg1 = reinterpret_cast<uint64_t>(sectionHandle),
        .arg2 = desiredAccess,
        .arg3 = reinterpret_cast<uint64_t>(objectAttributes),
        .arg4 = reinterpret_cast<uint64_t>(maximumSize),
        .stackArgs = stack,
        .stackArgCount = 3
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 35. NtMapViewOfSection (SSN: 0x0028)
inline NtStatus NtMapViewOfSection(
    Handle sectionHandle,
    Handle processHandle,
    uintptr_t* baseAddress,
    uintptr_t zeroBits,
    size_t commitSize,
    LargeInteger* sectionOffset,
    size_t* viewSize,
    uint32_t inheritDisposition,
    uint32_t allocationType,
    uint32_t win32Protect
) {
    uint64_t stack[6] = {
        commitSize,
        reinterpret_cast<uint64_t>(sectionOffset),
        reinterpret_cast<uint64_t>(viewSize),
        inheritDisposition,
        allocationType,
        win32Protect
    };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtMapViewOfSection,
        .arg1 = static_cast<uint64_t>(sectionHandle),
        .arg2 = static_cast<uint64_t>(processHandle),
        .arg3 = reinterpret_cast<uint64_t>(baseAddress),
        .arg4 = zeroBits,
        .stackArgs = stack,
        .stackArgCount = 6
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 36. NtUnmapViewOfSection (SSN: 0x002A)
inline NtStatus NtUnmapViewOfSection(Handle processHandle, uintptr_t baseAddress) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtUnmapViewOfSection,
        .arg1 = static_cast<uint64_t>(processHandle),
        .arg2 = baseAddress
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 37. NtQueryInformationFile (SSN: 0x0011)
inline NtStatus NtQueryInformationFile(
    Handle fileHandle,
    IoStatusBlock* ioStatusBlock,
    void* fileInformation,
    uint32_t length,
    FileInformationClass fileInformationClass
) {
    uint64_t stack[1] = { static_cast<uint64_t>(fileInformationClass) };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtQueryInformationFile,
        .arg1 = static_cast<uint64_t>(fileHandle),
        .arg2 = reinterpret_cast<uint64_t>(ioStatusBlock),
        .arg3 = reinterpret_cast<uint64_t>(fileInformation),
        .arg4 = length,
        .stackArgs = stack,
        .stackArgCount = 1
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 38. NtSetInformationFile (SSN: 0x0027)
inline NtStatus NtSetInformationFile(
    Handle fileHandle,
    IoStatusBlock* ioStatusBlock,
    const void* fileInformation,
    uint32_t length,
    FileInformationClass fileInformationClass
) {
    uint64_t stack[1] = { static_cast<uint64_t>(fileInformationClass) };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtSetInformationFile,
        .arg1 = static_cast<uint64_t>(fileHandle),
        .arg2 = reinterpret_cast<uint64_t>(ioStatusBlock),
        .arg3 = reinterpret_cast<uint64_t>(fileInformation),
        .arg4 = length,
        .stackArgs = stack,
        .stackArgCount = 1
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 39. NtQueryDirectoryFile (SSN: 0x0035)
inline NtStatus NtQueryDirectoryFile(
    Handle fileHandle,
    Handle event,
    void* apcRoutine,
    void* apcContext,
    IoStatusBlock* ioStatusBlock,
    void* fileInformation,
    uint32_t length,
    FileInformationClass fileInformationClass,
    bool returnSingleEntry,
    UnicodeString* fileName = nullptr,
    bool restartScan = false
) {
    uint64_t stack[7] = {
        reinterpret_cast<uint64_t>(ioStatusBlock),
        reinterpret_cast<uint64_t>(fileInformation),
        length,
        static_cast<uint64_t>(fileInformationClass),
        returnSingleEntry ? 1ULL : 0ULL,
        reinterpret_cast<uint64_t>(fileName),
        restartScan ? 1ULL : 0ULL
    };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtQueryDirectoryFile,
        .arg1 = static_cast<uint64_t>(fileHandle),
        .arg2 = static_cast<uint64_t>(event),
        .arg3 = reinterpret_cast<uint64_t>(apcRoutine),
        .arg4 = reinterpret_cast<uint64_t>(apcContext),
        .stackArgs = stack,
        .stackArgCount = 7
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 40. NtQueryPerformanceCounter (SSN: 0x0031)
inline NtStatus NtQueryPerformanceCounter(
    LargeInteger* performanceCounter,
    LargeInteger* performanceFrequency = nullptr
) {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtQueryPerformanceCounter,
        .arg1 = reinterpret_cast<uint64_t>(performanceCounter),
        .arg2 = reinterpret_cast<uint64_t>(performanceFrequency)
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 41. NtYieldExecution (SSN: 0x0046)
inline NtStatus NtYieldExecution() {
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtYieldExecution,
        .arg1 = 0
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 42. NtQueryInformationProcess (SSN: 0x0019)
inline NtStatus NtQueryInformationProcess(
    Handle processHandle,
    ProcessInformationClass processInformationClass,
    void* processInformation,
    uint32_t processInformationLength,
    uint32_t* returnLength = nullptr
) {
    uint64_t stack[1] = { reinterpret_cast<uint64_t>(returnLength) };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtQueryInformationProcess,
        .arg1 = static_cast<uint64_t>(processHandle),
        .arg2 = static_cast<uint64_t>(processInformationClass),
        .arg3 = reinterpret_cast<uint64_t>(processInformation),
        .arg4 = processInformationLength,
        .stackArgs = stack,
        .stackArgCount = 1
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 43. NtCreateNamedPipeFile (SSN: 0x0091)
inline NtStatus NtCreateNamedPipeFile(
    Handle* fileHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    IoStatusBlock* ioStatusBlock,
    uint32_t shareAccess = 0,
    uint32_t createDisposition = 1,
    uint32_t createOptions = 0,
    uint32_t namedPipeType = 0,
    uint32_t readMode = 0,
    uint32_t completionMode = 0,
    uint32_t maximumInstances = 0,
    uint32_t inboundQuota = 0,
    uint32_t outboundQuota = 0,
    LargeInteger* defaultTimeout = nullptr
) {
    uint64_t stack[10] = {
        shareAccess,
        createDisposition,
        createOptions,
        namedPipeType,
        readMode,
        completionMode,
        maximumInstances,
        inboundQuota,
        outboundQuota,
        reinterpret_cast<uint64_t>(defaultTimeout)
    };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtCreateNamedPipeFile,
        .arg1 = reinterpret_cast<uint64_t>(fileHandle),
        .arg2 = desiredAccess,
        .arg3 = reinterpret_cast<uint64_t>(objectAttributes),
        .arg4 = reinterpret_cast<uint64_t>(ioStatusBlock),
        .stackArgs = stack,
        .stackArgCount = 10
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// 44. NtCreateMailslotFile (SSN: 0x0092)
inline NtStatus NtCreateMailslotFile(
    Handle* fileHandle,
    uint32_t desiredAccess,
    ObjectAttributes* objectAttributes,
    IoStatusBlock* ioStatusBlock,
    uint32_t createOptions = 0,
    uint32_t mailslotQuota = 0,
    uint32_t maxMessageSize = 0,
    LargeInteger* readTimeout = nullptr
) {
    uint64_t stack[4] = {
        createOptions,
        mailslotQuota,
        maxMessageSize,
        reinterpret_cast<uint64_t>(readTimeout)
    };
    sys::SyscallFrame frame{
        .ssn = sys::SSN_NtCreateMailslotFile,
        .arg1 = reinterpret_cast<uint64_t>(fileHandle),
        .arg2 = desiredAccess,
        .arg3 = reinterpret_cast<uint64_t>(objectAttributes),
        .arg4 = reinterpret_cast<uint64_t>(ioStatusBlock),
        .stackArgs = stack,
        .stackArgCount = 4
    };
    return sys::SyscallDispatcher::get().dispatch(frame);
}

// ============================================================================
// NTDLL Runtime Library (Rtl) Helpers
// ============================================================================

inline thread_local uint32_t g_FallbackLastError = 0;

inline void RtlSetLastWin32Error(uint32_t errCode) noexcept {
    if (g_CurrentTeb) {
        g_CurrentTeb->lastErrorValue = errCode;
    } else {
        g_FallbackLastError = errCode;
    }
}

inline uint32_t RtlGetLastWin32Error() noexcept {
    if (g_CurrentTeb) {
        return g_CurrentTeb->lastErrorValue;
    }
    return g_FallbackLastError;
}

inline uint32_t RtlNtStatusToDosError(NtStatus status) noexcept {
    if (status == NtStatus::Success) return 0; // ERROR_SUCCESS
    switch (status) {
        case NtStatus::NoSuchFile:              return 2;   // ERROR_FILE_NOT_FOUND
        case NtStatus::ObjectPathNotFound:      return 3;   // ERROR_PATH_NOT_FOUND
        case NtStatus::AccessDenied:            return 5;   // ERROR_ACCESS_DENIED
        case NtStatus::InvalidHandle:           return 6;   // ERROR_INVALID_HANDLE
        case NtStatus::NoMemory:                return 14;  // ERROR_OUTOFMEMORY
        case NtStatus::SharingViolation:        return 32;  // ERROR_SHARING_VIOLATION
        case NtStatus::ObjectNameCollision:     return 80;  // ERROR_FILE_EXISTS
        case NtStatus::InvalidParameter:        return 87;  // ERROR_INVALID_PARAMETER
        case NtStatus::EndOfFile:               return 38;  // ERROR_HANDLE_EOF
        case NtStatus::NotImplemented:          return 120; // ERROR_CALL_NOT_IMPLEMENTED
        case NtStatus::InfoLengthMismatch:      return 24;  // ERROR_BAD_LENGTH
        case NtStatus::NotADirectory:           return 267; // ERROR_DIRECTORY
        case NtStatus::DirectoryNotEmpty:       return 145; // ERROR_DIR_NOT_EMPTY
        case NtStatus::FileIsADirectory:        return 5;   // ERROR_ACCESS_DENIED
        case NtStatus::NoMoreFiles:             return 18;  // ERROR_NO_MORE_FILES
        case NtStatus::BufferOverflow:          return 234; // ERROR_MORE_DATA
        case NtStatus::PipeBroken:              return 109; // ERROR_BROKEN_PIPE
        case NtStatus::PipeBusy:                return 231; // ERROR_PIPE_BUSY
        case NtStatus::PipeClosing:             return 232; // ERROR_NO_DATA
        case NtStatus::PipeDisconnected:        return 233; // ERROR_PIPE_NOT_CONNECTED
        case NtStatus::PipeNotAvailable:        return 233; // ERROR_PIPE_NOT_CONNECTED
        case NtStatus::PipeConnected:           return 535; // ERROR_PIPE_CONNECTED
        case NtStatus::PipeListening:           return 536; // ERROR_PIPE_LISTENING
        case NtStatus::MailslotNotFound:        return 2;   // ERROR_FILE_NOT_FOUND
        case NtStatus::Timeout:                 return 121; // ERROR_SEM_TIMEOUT
        default:                                return NT_SUCCESS(status) ? 0 : 31;  // ERROR_GEN_FAILURE
    }
}

// ============================================================================
// NTDLL Zw* Aliases (Identical Entry Points to Nt* Stubs in Userland)
// ============================================================================
inline NtStatus ZwAllocateVirtualMemory(Handle p, uintptr_t* b, uintptr_t z, size_t* r, uint32_t a, uint32_t pr) {
    return NtAllocateVirtualMemory(p, b, z, r, a, pr);
}
inline NtStatus ZwFreeVirtualMemory(Handle p, uintptr_t* b, size_t* r, uint32_t f) {
    return NtFreeVirtualMemory(p, b, r, f);
}
inline NtStatus ZwClose(Handle h) { return NtClose(h); }
inline NtStatus ZwTerminateProcess(Handle p, NtStatus s) { return NtTerminateProcess(p, s); }
inline NtStatus ZwWaitForSingleObject(Handle h, bool a, LargeInteger* t = nullptr) { return NtWaitForSingleObject(h, a, t); }
inline NtStatus ZwQuerySystemInformation(uint32_t c, void* i, uint32_t l, uint32_t* r) { return NtQuerySystemInformation(c, i, l, r); }
inline NtStatus ZwCreateEvent(Handle* h, uint32_t a, ObjectAttributes* o, uint32_t t, bool s) { return NtCreateEvent(h, a, o, t, s); }
inline NtStatus ZwSetEvent(Handle h, uint32_t* p = nullptr) { return NtSetEvent(h, p); }
inline NtStatus ZwResetEvent(Handle h, uint32_t* p = nullptr) { return NtResetEvent(h, p); }
inline NtStatus ZwCreateMutant(Handle* h, uint32_t a, ObjectAttributes* o, bool i) { return NtCreateMutant(h, a, o, i); }
inline NtStatus ZwReleaseMutant(Handle h, int32_t* p = nullptr) { return NtReleaseMutant(h, p); }
inline NtStatus ZwCreateIoCompletion(Handle* h, uint32_t a, ObjectAttributes* o, uint32_t c) { return NtCreateIoCompletion(h, a, o, c); }
inline NtStatus ZwSetIoCompletion(Handle h, uint64_t k, uintptr_t a, NtStatus s, uint32_t i) { return NtSetIoCompletion(h, k, a, s, i); }
inline NtStatus ZwRemoveIoCompletion(Handle h, uint64_t* k, uintptr_t* a, IoStatusBlock* i, LargeInteger* t = nullptr) { return NtRemoveIoCompletion(h, k, a, i, t); }
inline NtStatus ZwOpenKey(Handle* h, uint32_t a, ObjectAttributes* o) { return NtOpenKey(h, a, o); }
inline NtStatus ZwQueryValueKey(Handle h, UnicodeString* v, uint32_t c, void* i, size_t l, size_t* r) { return NtQueryValueKey(h, v, c, i, l, r); }
inline NtStatus ZwSetValueKey(Handle h, UnicodeString* v, uint32_t t, uint32_t ty, const void* d, uint32_t s) { return NtSetValueKey(h, v, t, ty, d, s); }
inline NtStatus ZwOpenProcessToken(Handle p, uint32_t a, Handle* t) { return NtOpenProcessToken(p, a, t); }
inline NtStatus ZwAccessCheck(void* s, Handle c, uint32_t d, uint32_t* g, NtStatus* a = nullptr) { return NtAccessCheck(s, c, d, g, a); }
inline NtStatus ZwCreatePort(Handle* p, ObjectAttributes* o, uint32_t c = 0, uint32_t m = 256) { return NtCreatePort(p, o, c, m); }
inline NtStatus ZwConnectPort(Handle* p, UnicodeString* n, void* s = nullptr, void* c = nullptr) { return NtConnectPort(p, n, s, c); }
inline NtStatus ZwRequestWaitReplyPort(Handle p, void* rq, void* rp) { return NtRequestWaitReplyPort(p, rq, rp); }
inline NtStatus ZwCreateFile(Handle* f, uint32_t d, ObjectAttributes* o, IoStatusBlock* i, LargeInteger* a = nullptr, uint32_t fa = 0, uint32_t s = 0, uint32_t cd = 1, uint32_t op = 0, void* e = nullptr, uint32_t el = 0) {
    return NtCreateFile(f, d, o, i, a, fa, s, cd, op, e, el);
}
inline NtStatus ZwOpenFile(Handle* f, uint32_t d, ObjectAttributes* o, IoStatusBlock* i, uint32_t s = 0, uint32_t op = 0) {
    return NtOpenFile(f, d, o, i, s, op);
}
inline NtStatus ZwReadFile(Handle f, Handle e, void* a, void* c, IoStatusBlock* i, void* b, uint32_t l, LargeInteger* o = nullptr, uint32_t* k = nullptr) {
    return NtReadFile(f, e, a, c, i, b, l, o, k);
}
inline NtStatus ZwWriteFile(Handle f, Handle e, void* a, void* c, IoStatusBlock* i, const void* b, uint32_t l, LargeInteger* o = nullptr, uint32_t* k = nullptr) {
    return NtWriteFile(f, e, a, c, i, b, l, o, k);
}
inline NtStatus ZwDeviceIoControlFile(Handle f, Handle e, void* a, void* c, IoStatusBlock* i, uint32_t io, const void* in = nullptr, uint32_t inl = 0, void* out = nullptr, uint32_t outl = 0) {
    return NtDeviceIoControlFile(f, e, a, c, i, io, in, inl, out, outl);
}
inline NtStatus ZwWaitForMultipleObjects(uint32_t c, const Handle* h, WaitType w, bool a, LargeInteger* t = nullptr) {
    return NtWaitForMultipleObjects(c, h, w, a, t);
}
inline NtStatus ZwDelayExecution(bool a, const LargeInteger* i) { return NtDelayExecution(a, i); }
inline NtStatus ZwCreateTimer(Handle* h, uint32_t d, ObjectAttributes* o, uint32_t t) { return NtCreateTimer(h, d, o, t); }
inline NtStatus ZwSetTimer(Handle h, LargeInteger* d, void* a = nullptr, void* c = nullptr, bool r = false, uint32_t p = 0, bool* pr = nullptr) {
    return NtSetTimer(h, d, a, c, r, p, pr);
}
inline NtStatus ZwCancelTimer(Handle h, bool* c = nullptr) { return NtCancelTimer(h, c); }
inline NtStatus ZwShutdownSystem(uint32_t a) { return NtShutdownSystem(a); }

inline NtStatus ZwCreateSection(Handle* s, uint32_t a, ObjectAttributes* o, LargeInteger* m, uint32_t p, uint32_t al, Handle f) {
    return NtCreateSection(s, a, o, m, p, al, f);
}
inline NtStatus ZwMapViewOfSection(Handle s, Handle p, uintptr_t* b, uintptr_t z, size_t c, LargeInteger* so, size_t* v, uint32_t i, uint32_t at, uint32_t wp) {
    return NtMapViewOfSection(s, p, b, z, c, so, v, i, at, wp);
}
inline NtStatus ZwUnmapViewOfSection(Handle p, uintptr_t b) { return NtUnmapViewOfSection(p, b); }
inline NtStatus ZwQueryInformationFile(Handle f, IoStatusBlock* i, void* fi, uint32_t l, FileInformationClass c) {
    return NtQueryInformationFile(f, i, fi, l, c);
}
inline NtStatus ZwSetInformationFile(Handle f, IoStatusBlock* i, const void* fi, uint32_t l, FileInformationClass c) {
    return NtSetInformationFile(f, i, fi, l, c);
}
inline NtStatus ZwQueryDirectoryFile(Handle f, Handle e, void* a, void* c, IoStatusBlock* i, void* fi, uint32_t l, FileInformationClass fc, bool s, UnicodeString* fn = nullptr, bool r = false) {
    return NtQueryDirectoryFile(f, e, a, c, i, fi, l, fc, s, fn, r);
}
inline NtStatus ZwQueryPerformanceCounter(LargeInteger* c, LargeInteger* f = nullptr) { return NtQueryPerformanceCounter(c, f); }
inline NtStatus ZwYieldExecution() { return NtYieldExecution(); }
inline NtStatus ZwQueryInformationProcess(Handle p, ProcessInformationClass c, void* i, uint32_t l, uint32_t* r = nullptr) {
    return NtQueryInformationProcess(p, c, i, l, r);
}

// ============================================================================
// Userland Win32 ABI Export Implementations & Loader Registration
// ============================================================================

inline NtStatus WINAPI NtAdjustPrivilegesToken(
    void* /*TokenHandle*/,
    BOOLEAN /*DisableAllPrivileges*/,
    void* /*NewState*/,
    uint32_t /*BufferLength*/,
    void* /*PreviousState*/,
    uint32_t* ReturnLength
) noexcept {
    if (ReturnLength) *ReturnLength = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtCreateFile_Export(
    void** fileHandle,
    uint32_t /*desiredAccess*/,
    void* /*objectAttributes*/,
    void* /*ioStatusBlock*/,
    void* /*allocationSize*/,
    uint32_t /*fileAttributes*/,
    uint32_t /*shareAccess*/,
    uint32_t /*createDisposition*/,
    uint32_t /*createOptions*/,
    void* /*eaBuffer*/,
    uint32_t /*eaLength*/
) noexcept {
    if (fileHandle) *fileHandle = reinterpret_cast<void*>(0x4001);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtDelayExecution_Export(BOOLEAN /*Alertable*/, const int64_t* /*DelayInterval*/) noexcept {
    return NtStatus::Success;
}

inline NtStatus WINAPI NtDeviceIoControlFile_Export(
    void* /*FileHandle*/,
    void* /*Event*/,
    void* /*ApcRoutine*/,
    void* /*ApcContext*/,
    void* /*IoStatusBlock*/,
    uint32_t /*IoControlCode*/,
    const void* /*InputBuffer*/,
    uint32_t /*InputBufferLength*/,
    void* /*OutputBuffer*/,
    uint32_t /*OutputBufferLength*/
) noexcept {
    return NtStatus::Success;
}

inline NtStatus WINAPI NtDuplicateObject(
    void* /*SourceProcessHandle*/,
    void* SourceHandle,
    void* /*TargetProcessHandle*/,
    void** TargetHandle,
    uint32_t /*DesiredAccess*/,
    uint32_t /*HandleAttributes*/,
    uint32_t /*Options*/
) noexcept {
    if (TargetHandle) *TargetHandle = SourceHandle;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtFlushBuffersFile(void* /*FileHandle*/, void* /*IoStatusBlock*/) noexcept {
    return NtStatus::Success;
}

inline NtStatus WINAPI NtFsControlFile(
    void* /*FileHandle*/,
    void* /*Event*/,
    void* /*ApcRoutine*/,
    void* /*ApcContext*/,
    void* /*IoStatusBlock*/,
    uint32_t /*FsControlCode*/,
    const void* /*InputBuffer*/,
    uint32_t /*InputBufferLength*/,
    void* /*OutputBuffer*/,
    uint32_t /*OutputBufferLength*/
) noexcept {
    return NtStatus::Success;
}

inline NtStatus WINAPI NtOpenProcess(
    void** ProcessHandle,
    uint32_t /*DesiredAccess*/,
    void* /*ObjectAttributes*/,
    void* /*ClientId*/
) noexcept {
    if (ProcessHandle) *ProcessHandle = reinterpret_cast<void*>(0x1000);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtOpenProcessToken_Export(
    void* /*ProcessHandle*/,
    uint32_t /*DesiredAccess*/,
    void** TokenHandle
) noexcept {
    if (TokenHandle) *TokenHandle = reinterpret_cast<void*>(0x2000);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtOpenSymbolicLinkObject(
    void** LinkHandle,
    uint32_t /*DesiredAccess*/,
    void* /*ObjectAttributes*/
) noexcept {
    if (LinkHandle) *LinkHandle = reinterpret_cast<void*>(0x3000);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryEaFile(
    void* /*FileHandle*/,
    void* /*IoStatusBlock*/,
    void* /*Buffer*/,
    uint32_t /*Length*/,
    BOOLEAN /*ReturnSingleEntry*/,
    void* /*EaList*/,
    uint32_t /*EaListLength*/,
    uint32_t* /*EaIndex*/,
    BOOLEAN /*RestartScan*/
) noexcept {
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryObject(
    void* /*Handle*/,
    uint32_t /*ObjectInformationClass*/,
    void* /*ObjectInformation*/,
    uint32_t /*Length*/,
    uint32_t* ReturnLength
) noexcept {
    if (ReturnLength) *ReturnLength = 64;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQuerySecurityObject(
    void* /*Handle*/,
    uint32_t /*SecurityInformation*/,
    void* /*SecurityDescriptor*/,
    uint32_t /*Length*/,
    uint32_t* LengthNeeded
) noexcept {
    if (LengthNeeded) *LengthNeeded = 32;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQuerySystemInformation_Export(
    uint32_t /*SystemInformationClass*/,
    void* /*SystemInformation*/,
    uint32_t /*SystemInformationLength*/,
    uint32_t* ReturnLength
) noexcept {
    if (ReturnLength) *ReturnLength = 64;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryVolumeInformationFile(
    void* /*FileHandle*/,
    void* /*IoStatusBlock*/,
    void* /*FsInformation*/,
    uint32_t /*Length*/,
    uint32_t /*FsInformationClass*/
) noexcept {
    return NtStatus::Success;
}

inline NtStatus WINAPI NtSetEaFile(
    void* /*FileHandle*/,
    void* /*IoStatusBlock*/,
    void* /*Buffer*/,
    uint32_t /*Length*/
) noexcept {
    return NtStatus::Success;
}

inline NtStatus WINAPI NtSetSecurityObject(
    void* /*Handle*/,
    uint32_t /*SecurityInformation*/,
    void* /*SecurityDescriptor*/
) noexcept {
    return NtStatus::Success;
}

inline void WINAPI RtlCaptureContext(void* /*ContextRecord*/) noexcept {
}

inline void* WINAPI RtlLookupFunctionEntry(uint64_t /*ControlPc*/, uint64_t* /*ImageBase*/, void* /*HistoryTable*/) noexcept {
    return nullptr;
}

inline void* WINAPI RtlPcToFileHeader(void* /*PcValue*/, void** BaseOfImage) noexcept {
    if (BaseOfImage) *BaseOfImage = reinterpret_cast<void*>(0x140000000ULL);
    return reinterpret_cast<void*>(0x140000000ULL);
}

inline void WINAPI RtlUnwind(void* /*TargetFrame*/, void* /*TargetIp*/, void* /*ExceptionRecord*/, void* /*ReturnValue*/) noexcept {
}

inline void WINAPI RtlUnwindEx(void* /*TargetFrame*/, void* /*TargetIp*/, void* /*ExceptionRecord*/, void* /*ReturnValue*/, void* /*ContextRecord*/, void* /*HistoryTable*/) noexcept {
}

inline uint64_t WINAPI RtlVirtualUnwind(
    uint32_t /*HandlerType*/,
    uint64_t /*ImageBase*/,
    uint64_t /*ControlPc*/,
    void* /*FunctionEntry*/,
    void* /*ContextRecord*/,
    void** /*HandlerData*/,
    uint64_t* /*EstablisherFrame*/,
    void* /*ContextPointers*/
) noexcept {
    return 0;
}

inline uint64_t WINAPI VerSetConditionMask(uint64_t ConditionMask, uint32_t TypeMask, uint8_t Condition) noexcept {
    if (TypeMask == 0) return ConditionMask;
    return ConditionMask | (static_cast<uint64_t>(Condition) << (TypeMask * 3));
}

// 13. ntdll.dll (Clean-Room Native NT Kernel Syscalls & RTL Utility Library)
// ============================================================================

using BOOL = int32_t;
using BOOLEAN = uint8_t;
using DWORD = uint32_t;
using ULONG = uint32_t;
using USHORT = uint16_t;
using UCHAR = uint8_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using HANDLE = void*;
using HWND = void*;
using HDC = void*;
using HICON = void*;
using HMENU = void*;
using HKEY = void*;
using HDEVINFO = void*;
using HPROPSHEETPAGE = void*;
using LSTATUS = int32_t;
using HRESULT = int32_t;
using LONG = int32_t;
using LONG_PTR = intptr_t;
using ULONG_PTR = uintptr_t;
using LCID = uint32_t;
using PVOID = void*;

inline constexpr BOOL TRUE_VAL = 1;
inline constexpr BOOL FALSE_VAL = 0;
inline constexpr HRESULT S_OK_VAL = 0;
inline constexpr DWORD ERROR_SUCCESS_VAL = 0;

inline NtStatus WINAPI Generic_Nt_Success() noexcept { return NtStatus::Success; }
inline void* WINAPI Generic_Nt_NullPtr() noexcept { return nullptr; }

inline NtStatus WINAPI NtAcceptConnectPort(HANDLE* PortHandle, void* /*PortContext*/, void* /*ConnectionRequest*/, BOOLEAN /*AcceptConnection*/, void* /*ServerView*/, void* /*ClientView*/) noexcept {
    if (PortHandle) *PortHandle = reinterpret_cast<HANDLE>(0x9001);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtAdjustGroupsToken(HANDLE /*TokenHandle*/, BOOLEAN /*ResetToDefault*/, void* /*NewState*/, ULONG /*BufferLength*/, void* /*PreviousState*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtAlertThread(HANDLE /*ThreadHandle*/) noexcept { return NtStatus::Success; }

inline NtStatus WINAPI NtAlpcQueryInformation(HANDLE /*PortHandle*/, ULONG /*PortInformationClass*/, void* /*PortInformation*/, ULONG /*Length*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 32;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtAssignProcessToJobObject(HANDLE /*JobHandle*/, HANDLE /*ProcessHandle*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtCancelTimer_Export(HANDLE /*TimerHandle*/, BOOLEAN* CurrentState) noexcept {
    if (CurrentState) *CurrentState = 0;
    return NtStatus::Success;
}
inline NtStatus WINAPI NtClearEvent(HANDLE /*EventHandle*/) noexcept { return NtStatus::Success; }

inline NtStatus WINAPI NtConnectPort_Export(HANDLE* PortHandle, void* /*PortName*/, void* /*SecurityQos*/, void* /*ClientView*/, void* /*ServerView*/, ULONG* /*MaxMessageLength*/, void* /*ConnectionInformation*/, ULONG* /*ConnectionInformationLength*/) noexcept {
    if (PortHandle) *PortHandle = reinterpret_cast<HANDLE>(0x9002);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtCreateDirectoryObject(HANDLE* DirectoryHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/) noexcept {
    if (DirectoryHandle) *DirectoryHandle = reinterpret_cast<HANDLE>(0x9003);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtCreateEvent_Export(HANDLE* EventHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, uint32_t /*EventType*/, BOOLEAN /*InitialState*/) noexcept {
    if (EventHandle) *EventHandle = reinterpret_cast<HANDLE>(0x9004);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtCreateIoCompletion_Export(HANDLE* IoCompletionHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, uint32_t /*Count*/) noexcept {
    if (IoCompletionHandle) *IoCompletionHandle = reinterpret_cast<HANDLE>(0x9005);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtCreateJobObject(HANDLE* JobHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/) noexcept {
    if (JobHandle) *JobHandle = reinterpret_cast<HANDLE>(0x9006);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtCreateKey(HANDLE* KeyHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, ULONG /*TitleIndex*/, void* /*Class*/, ULONG /*CreateOptions*/, ULONG* Disposition) noexcept {
    if (KeyHandle) *KeyHandle = reinterpret_cast<HANDLE>(0x9007);
    if (Disposition) *Disposition = 1; // REG_CREATED_NEW_KEY
    return NtStatus::Success;
}

inline NtStatus WINAPI NtCreateKeyedEvent(HANDLE* KeyedEventHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, ULONG /*Flags*/) noexcept {
    if (KeyedEventHandle) *KeyedEventHandle = reinterpret_cast<HANDLE>(0x9008);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtCreateMutant_Export(HANDLE* MutantHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, BOOLEAN /*InitialOwner*/) noexcept {
    if (MutantHandle) *MutantHandle = reinterpret_cast<HANDLE>(0x9009);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtCreatePort_Export(HANDLE* PortHandle, void* /*ObjectAttributes*/, ULONG /*MaxConnectionInfoLength*/, ULONG /*MaxDataLength*/, ULONG /*Reserved*/) noexcept {
    if (PortHandle) *PortHandle = reinterpret_cast<HANDLE>(0x900A);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtCreateProcessEx(HANDLE* ProcessHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, HANDLE /*ParentProcess*/, ULONG /*Flags*/, HANDLE /*SectionHandle*/, HANDLE /*DebugPort*/, HANDLE /*TokenHandle*/, ULONG /*Reserved*/) noexcept {
    if (ProcessHandle) *ProcessHandle = reinterpret_cast<HANDLE>(0x900B);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtCreateSemaphore(HANDLE* SemaphoreHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, int32_t /*InitialCount*/, int32_t /*MaximumCount*/) noexcept {
    if (SemaphoreHandle) *SemaphoreHandle = reinterpret_cast<HANDLE>(0x900C);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtCreateThreadEx(HANDLE* ThreadHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, HANDLE /*ProcessHandle*/, void* /*StartRoutine*/, void* /*Argument*/, ULONG /*CreateFlags*/, ULONG_PTR /*ZeroBits*/, size_t /*StackSize*/, size_t /*MaximumStackSize*/, void* /*AttributeList*/) noexcept {
    if (ThreadHandle) *ThreadHandle = reinterpret_cast<HANDLE>(0x900D);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtCreateTimer_Export(HANDLE* TimerHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, uint32_t /*TimerType*/) noexcept {
    if (TimerHandle) *TimerHandle = reinterpret_cast<HANDLE>(0x900E);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtDeleteKey(HANDLE /*KeyHandle*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtDeleteValueKey(HANDLE /*KeyHandle*/, void* /*ValueName*/) noexcept { return NtStatus::Success; }

inline NtStatus WINAPI NtDuplicateToken(HANDLE /*ExistingTokenHandle*/, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, BOOLEAN /*EffectiveOnly*/, uint32_t /*TokenType*/, HANDLE* NewTokenHandle) noexcept {
    if (NewTokenHandle) *NewTokenHandle = reinterpret_cast<HANDLE>(0x900F);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtEnumerateKey(HANDLE /*KeyHandle*/, ULONG /*Index*/, uint32_t /*KeyInformationClass*/, void* /*KeyInformation*/, ULONG /*Length*/, ULONG* ResultLength) noexcept {
    if (ResultLength) *ResultLength = 0;
    return NtStatus::NoMoreEntries;
}

inline NtStatus WINAPI NtEnumerateSystemEnvironmentValuesEx(ULONG /*InformationClass*/, void* /*Buffer*/, ULONG* BufferLength) noexcept {
    if (BufferLength) *BufferLength = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtEnumerateValueKey(HANDLE /*KeyHandle*/, ULONG /*Index*/, uint32_t /*KeyValueInformationClass*/, void* /*KeyValueInformation*/, ULONG /*Length*/, ULONG* ResultLength) noexcept {
    if (ResultLength) *ResultLength = 0;
    return NtStatus::NoMoreEntries;
}

inline NtStatus WINAPI NtFlushInstructionCache(HANDLE /*ProcessHandle*/, void* /*BaseAddress*/, size_t /*NumberOfBytesToFlush*/) noexcept { return NtStatus::Success; }

inline NtStatus WINAPI NtGetContextThread(HANDLE /*ThreadHandle*/, void* /*ThreadContext*/) noexcept { return NtStatus::Success; }

inline NtStatus WINAPI NtGetNextProcess(HANDLE /*ProcessHandle*/, uint32_t /*DesiredAccess*/, ULONG /*HandleAttributes*/, ULONG /*Flags*/, HANDLE* NewProcessHandle) noexcept {
    if (NewProcessHandle) *NewProcessHandle = nullptr;
    return NtStatus::NoMoreEntries;
}

inline NtStatus WINAPI NtGetNextThread(HANDLE /*ProcessHandle*/, HANDLE /*ThreadHandle*/, uint32_t /*DesiredAccess*/, ULONG /*HandleAttributes*/, ULONG /*Flags*/, HANDLE* NewThreadHandle) noexcept {
    if (NewThreadHandle) *NewThreadHandle = nullptr;
    return NtStatus::NoMoreEntries;
}

inline NtStatus WINAPI NtInitiatePowerAction(uint32_t /*SystemAction*/, uint32_t /*LightestSystemState*/, ULONG /*Flags*/, BOOLEAN /*Asynchronous*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtIsProcessInJob(HANDLE /*ProcessHandle*/, HANDLE /*JobHandle*/) noexcept { return NtStatus::ProcessNotInJob; }
inline NtStatus WINAPI NtLoadDriver(void* /*DriverServiceName*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtLoadKeyEx(void* /*TargetKey*/, void* /*SourceFile*/, ULONG /*Flags*/, HANDLE /*TrustClassKey*/, HANDLE /*Event*/, uint32_t /*DesiredAccess*/, HANDLE* RootHandle, void* /*IoStatusBlock*/) noexcept {
    if (RootHandle) *RootHandle = reinterpret_cast<HANDLE>(0x9010);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtLockFile(HANDLE /*FileHandle*/, HANDLE /*Event*/, void* /*ApcRoutine*/, void* /*ApcContext*/, void* /*IoStatusBlock*/, void* /*ByteOffset*/, void* /*Length*/, ULONG /*Key*/, BOOLEAN /*FailImmediately*/, BOOLEAN /*ExclusiveLock*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtOpenDirectoryObject(HANDLE* DirectoryHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/) noexcept {
    if (DirectoryHandle) *DirectoryHandle = reinterpret_cast<HANDLE>(0x9011);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtOpenJobObject(HANDLE* JobHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/) noexcept {
    if (JobHandle) *JobHandle = reinterpret_cast<HANDLE>(0x9012);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtOpenKey_Export(HANDLE* KeyHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/) noexcept {
    if (KeyHandle) *KeyHandle = reinterpret_cast<HANDLE>(0x9013);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtOpenMutant(HANDLE* MutantHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/) noexcept {
    if (MutantHandle) *MutantHandle = reinterpret_cast<HANDLE>(0x9014);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtOpenSection(HANDLE* SectionHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/) noexcept {
    if (SectionHandle) *SectionHandle = reinterpret_cast<HANDLE>(0x9015);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtOpenThread(HANDLE* ThreadHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, void* /*ClientId*/) noexcept {
    if (ThreadHandle) *ThreadHandle = reinterpret_cast<HANDLE>(0x9016);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtOpenThreadToken(HANDLE /*ThreadHandle*/, uint32_t /*DesiredAccess*/, BOOLEAN /*OpenAsSelf*/, HANDLE* TokenHandle) noexcept {
    if (TokenHandle) *TokenHandle = reinterpret_cast<HANDLE>(0x9017);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtPowerInformation(uint32_t /*InformationLevel*/, void* /*InputBuffer*/, ULONG /*InputBufferLength*/, void* /*OutputBuffer*/, ULONG OutputBufferLength) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtProtectVirtualMemory(HANDLE /*ProcessHandle*/, void** /*BaseAddress*/, size_t* /*RegionSize*/, ULONG /*NewProtect*/, ULONG* OldProtect) noexcept {
    if (OldProtect) *OldProtect = 0x40; // PAGE_EXECUTE_READWRITE
    return NtStatus::Success;
}
inline NtStatus WINAPI NtPulseEvent(HANDLE /*EventHandle*/, int32_t* PreviousState) noexcept {
    if (PreviousState) *PreviousState = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryAttributesFile(void* /*ObjectAttributes*/, void* /*FileInformation*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtQueryDefaultLocale(BOOLEAN /*UserProfile*/, LCID* DefaultLocaleId) noexcept {
    if (DefaultLocaleId) *DefaultLocaleId = 0x0409;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryDirectoryObject(HANDLE /*DirectoryHandle*/, void* /*Buffer*/, ULONG /*Length*/, BOOLEAN /*ReturnSingleEntry*/, BOOLEAN /*RestartScan*/, ULONG* Context, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 0;
    return NtStatus::NoMoreEntries;
}

inline NtStatus WINAPI NtQueryEvent(HANDLE /*EventHandle*/, uint32_t /*EventInformationClass*/, void* /*EventInformation*/, ULONG /*EventInformationLength*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 16;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryFullAttributesFile(void* /*ObjectAttributes*/, void* /*FileInformation*/) noexcept { return NtStatus::Success; }

inline NtStatus WINAPI NtQueryInformationJobObject(HANDLE /*JobHandle*/, uint32_t /*JobObjectInformationClass*/, void* /*JobObjectInformation*/, ULONG /*JobObjectInformationLength*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 32;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryInformationThread(HANDLE /*ThreadHandle*/, uint32_t /*ThreadInformationClass*/, void* /*ThreadInformation*/, ULONG /*ThreadInformationLength*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 32;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryInformationToken(HANDLE /*TokenHandle*/, uint32_t /*TokenInformationClass*/, void* /*TokenInformation*/, ULONG /*TokenInformationLength*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 32;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryKey(HANDLE /*KeyHandle*/, uint32_t /*KeyInformationClass*/, void* /*KeyInformation*/, ULONG /*Length*/, ULONG* ResultLength) noexcept {
    if (ResultLength) *ResultLength = 32;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryMutant(HANDLE /*MutantHandle*/, uint32_t /*MutantInformationClass*/, void* /*MutantInformation*/, ULONG /*MutantInformationLength*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 16;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryOpenSubKeysEx(void* /*TargetKey*/, ULONG /*BufferLength*/, void* /*Buffer*/, ULONG* RequiredSize) noexcept {
    if (RequiredSize) *RequiredSize = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQuerySection(HANDLE /*SectionHandle*/, uint32_t /*SectionInformationClass*/, void* /*SectionInformation*/, size_t /*SectionInformationLength*/, size_t* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 32;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQuerySecurityAttributesToken(HANDLE /*TokenHandle*/, void* /*Attributes*/, ULONG /*NumberOfAttributes*/, void* /*Buffer*/, ULONG /*Length*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQuerySemaphore(HANDLE /*SemaphoreHandle*/, uint32_t /*SemaphoreInformationClass*/, void* /*SemaphoreInformation*/, ULONG /*SemaphoreInformationLength*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 16;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQuerySymbolicLinkObject(HANDLE /*LinkHandle*/, void* /*LinkTarget*/, ULONG* ReturnedLength) noexcept {
    if (ReturnedLength) *ReturnedLength = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQuerySystemEnvironmentValueEx(void* /*VariableName*/, void* /*VendorGuid*/, void* /*Value*/, ULONG* ValueLength, ULONG* Attributes) noexcept {
    if (ValueLength) *ValueLength = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQuerySystemInformationEx(uint32_t /*SystemInformationClass*/, void* /*InputBuffer*/, ULONG /*InputBufferLength*/, void* /*SystemInformation*/, ULONG /*SystemInformationLength*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 64;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQuerySystemTime(int64_t* SystemTime) noexcept {
    if (SystemTime) *SystemTime = 133500000000000000LL;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryTimer(HANDLE /*TimerHandle*/, uint32_t /*TimerInformationClass*/, void* /*TimerInformation*/, ULONG /*TimerInformationLength*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 16;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryTimerResolution(ULONG* MaximumTime, ULONG* MinimumTime, ULONG* CurrentTime) noexcept {
    if (MaximumTime) *MaximumTime = 156250;
    if (MinimumTime) *MinimumTime = 5000;
    if (CurrentTime) *CurrentTime = 10000;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryValueKey_Export(HANDLE /*KeyHandle*/, void* /*ValueName*/, uint32_t /*KeyValueInformationClass*/, void* /*KeyValueInformation*/, ULONG /*Length*/, ULONG* ResultLength) noexcept {
    if (ResultLength) *ResultLength = 32;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryVirtualMemory(HANDLE /*ProcessHandle*/, void* /*BaseAddress*/, uint32_t /*MemoryInformationClass*/, void* MemoryInformation, size_t MemoryInformationLength, size_t* ReturnLength) noexcept {
    if (MemoryInformation && MemoryInformationLength >= 48) {
        std::memset(MemoryInformation, 0, MemoryInformationLength);
    }
    if (ReturnLength) *ReturnLength = MemoryInformationLength;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueueApcThread(HANDLE /*ThreadHandle*/, void* /*ApcRoutine*/, void* /*ApcRoutineContext*/, void* /*ApcStatusBlock*/, void* /*ApcReserved*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtQueueApcThreadEx(HANDLE /*ThreadHandle*/, HANDLE /*UserApcReserveHandle*/, void* /*ApcRoutine*/, void* /*ApcRoutineContext*/, void* /*ApcStatusBlock*/, void* /*ApcReserved*/) noexcept { return NtStatus::Success; }

inline NtStatus WINAPI NtReadVirtualMemory(HANDLE /*ProcessHandle*/, void* /*BaseAddress*/, void* Buffer, size_t NumberOfBytesToRead, size_t* NumberOfBytesRead) noexcept {
    if (Buffer && NumberOfBytesToRead > 0) std::memset(Buffer, 0, NumberOfBytesToRead);
    if (NumberOfBytesRead) *NumberOfBytesRead = NumberOfBytesToRead;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtReleaseKeyedEvent(HANDLE /*KeyedEventHandle*/, void* /*KeyValue*/, BOOLEAN /*Alertable*/, const int64_t* /*Timeout*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtReleaseSemaphore(HANDLE /*SemaphoreHandle*/, int32_t /*ReleaseCount*/, int32_t* PreviousCount) noexcept {
    if (PreviousCount) *PreviousCount = 1;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtRemoveIoCompletion_Export(HANDLE /*IoCompletionHandle*/, void** KeyContext, void** ApcContext, void* /*IoStatusBlock*/, const int64_t* /*Timeout*/) noexcept {
    if (KeyContext) *KeyContext = nullptr;
    if (ApcContext) *ApcContext = nullptr;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtRemoveIoCompletionEx(HANDLE /*IoCompletionHandle*/, void* /*IoCompletionInformation*/, ULONG /*Count*/, ULONG* NumEntriesRemoved, const int64_t* /*Timeout*/, BOOLEAN /*Alertable*/) noexcept {
    if (NumEntriesRemoved) *NumEntriesRemoved = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtRemoveProcessDebug(HANDLE /*ProcessHandle*/, HANDLE /*DebugObjectHandle*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtReplyWaitReceivePort(HANDLE /*PortHandle*/, void** /*PortContext*/, void* /*ReplyMessage*/, void* /*ReceiveMessage*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtRequestWaitReplyPort_Export(HANDLE /*PortHandle*/, void* /*RequestMessage*/, void* /*ReplyMessage*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtResetEvent_Export(HANDLE /*EventHandle*/, int32_t* PreviousState) noexcept {
    if (PreviousState) *PreviousState = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtResumeProcess(HANDLE /*ProcessHandle*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtResumeThread(HANDLE /*ThreadHandle*/, ULONG* PreviousSuspendCount) noexcept {
    if (PreviousSuspendCount) *PreviousSuspendCount = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtSetEvent_Export(HANDLE /*EventHandle*/, int32_t* PreviousState) noexcept {
    if (PreviousState) *PreviousState = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtSetEventBoostPriority(HANDLE /*EventHandle*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtSetHighEventPair(HANDLE /*EventPairHandle*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtSetInformationDebugObject(HANDLE /*DebugObjectHandle*/, uint32_t /*DebugObjectInformationClass*/, void* /*DebugInformation*/, ULONG /*DebugInformationLength*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI NtSetInformationObject(HANDLE /*Handle*/, uint32_t /*ObjectInformationClass*/, void* /*ObjectInformation*/, ULONG /*ObjectInformationLength*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtSetInformationProcess(HANDLE /*ProcessHandle*/, uint32_t /*ProcessInformationClass*/, void* /*ProcessInformation*/, ULONG /*ProcessInformationLength*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtSetInformationThread(HANDLE /*ThreadHandle*/, uint32_t /*ThreadInformationClass*/, void* /*ThreadInformation*/, ULONG /*ThreadInformationLength*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtSetInformationToken(HANDLE /*TokenHandle*/, uint32_t /*TokenInformationClass*/, void* /*TokenInformation*/, ULONG /*TokenInformationLength*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtSetLowEventPair(HANDLE /*EventPairHandle*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtSetSystemEnvironmentValueEx(void* /*VariableName*/, void* /*VendorGuid*/, void* /*Value*/, ULONG /*ValueLength*/, ULONG /*Attributes*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtSetSystemInformation(uint32_t /*SystemInformationClass*/, void* /*SystemInformation*/, ULONG /*SystemInformationLength*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtSetSystemPowerState(uint32_t /*SystemAction*/, uint32_t /*LightestSystemState*/, ULONG /*Flags*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtSetTimer_Export(HANDLE /*TimerHandle*/, const int64_t* /*DueTime*/, void* /*TimerApcRoutine*/, void* /*TimerContext*/, BOOLEAN /*ResumeTimer*/, LONG /*Period*/, BOOLEAN* PreviousState) noexcept {
    if (PreviousState) *PreviousState = 0;
    return NtStatus::Success;
}
inline NtStatus WINAPI NtSetTimerEx(HANDLE /*TimerHandle*/, uint32_t /*TimerSetInformationClass*/, void* /*TimerSetInformation*/, ULONG /*TimerSetInformationLength*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtSetValueKey_Export(HANDLE /*KeyHandle*/, void* /*ValueName*/, ULONG /*TitleIndex*/, ULONG /*Type*/, void* /*Data*/, ULONG /*DataSize*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtShutdownSystem_Export(uint32_t /*Action*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtSuspendProcess(HANDLE /*ProcessHandle*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtSuspendThread(HANDLE /*ThreadHandle*/, ULONG* PreviousSuspendCount) noexcept {
    if (PreviousSuspendCount) *PreviousSuspendCount = 0;
    return NtStatus::Success;
}
inline NtStatus WINAPI NtSystemDebugControl(uint32_t /*Command*/, void* /*InputBuffer*/, ULONG /*InputBufferLength*/, void* /*OutputBuffer*/, ULONG /*OutputBufferLength*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 0;
    return NtStatus::Success;
}
inline NtStatus WINAPI NtTerminateJobObject(HANDLE /*JobHandle*/, NtStatus /*ExitStatus*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtTerminateProcess_Export(HANDLE /*ProcessHandle*/, NtStatus /*ExitStatus*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtTerminateThread(HANDLE /*ThreadHandle*/, NtStatus /*ExitStatus*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtTestAlert() noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtTraceControl(ULONG /*FunctionCode*/, void* /*InBuffer*/, ULONG /*InBufferLen*/, void* /*OutBuffer*/, ULONG /*OutBufferLen*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 0;
    return NtStatus::Success;
}
inline NtStatus WINAPI NtUnloadDriver(void* /*DriverServiceName*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtUnlockFile(HANDLE /*FileHandle*/, void* /*IoStatusBlock*/, void* /*ByteOffset*/, void* /*Length*/, ULONG /*Key*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtUnlockVirtualMemory(HANDLE /*ProcessHandle*/, void** /*BaseAddress*/, size_t* /*RegionSize*/, ULONG /*MapType*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtWaitForKeyedEvent(HANDLE /*KeyedEventHandle*/, void* /*KeyValue*/, BOOLEAN /*Alertable*/, const int64_t* /*Timeout*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtWaitForMultipleObjects_Export(ULONG /*Count*/, const HANDLE* /*Handles*/, uint32_t /*WaitType*/, BOOLEAN /*Alertable*/, const int64_t* /*Timeout*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI NtWriteVirtualMemory(HANDLE /*ProcessHandle*/, void* /*BaseAddress*/, void* /*Buffer*/, size_t NumberOfBytesToWrite, size_t* NumberOfBytesWritten) noexcept {
    if (NumberOfBytesWritten) *NumberOfBytesWritten = NumberOfBytesToWrite;
    return NtStatus::Success;
}

// ----------------------------------------------------------------------------
// RTL Support Routines
// ----------------------------------------------------------------------------
inline NtStatus WINAPI RtlAbsoluteToSelfRelativeSD(void* /*AbsoluteSecurityDescriptor*/, void* /*SelfRelativeSecurityDescriptor*/, ULONG* BufferLength) noexcept {
    if (BufferLength) *BufferLength = 32;
    return NtStatus::Success;
}
inline void WINAPI RtlAcquireSRWLockShared(void* /*SRWLock*/) noexcept {}
inline void WINAPI RtlReleaseSRWLockShared(void* /*SRWLock*/) noexcept {}
inline BOOLEAN WINAPI RtlAreBitsSet(void* /*BitMapHeader*/, ULONG /*StartingIndex*/, ULONG /*Length*/) noexcept { return 1; }
inline void WINAPI RtlClearBits(void* /*BitMapHeader*/, ULONG /*StartingIndex*/, ULONG /*NumberToClear*/) noexcept {}
inline void WINAPI RtlSetBits(void* /*BitMapHeader*/, ULONG /*StartingIndex*/, ULONG /*NumberToSet*/) noexcept {}
inline ULONG WINAPI RtlNumberOfSetBits(void* /*BitMapHeader*/) noexcept { return 0; }
inline ULONG WINAPI RtlFindClearBitsAndSet(void* /*BitMapHeader*/, ULONG /*NumberToFind*/, ULONG /*HintIndex*/) noexcept { return 0; }

inline NtStatus WINAPI RtlConvertSidToUnicodeString(void* /*UnicodeString*/, void* /*Sid*/, BOOLEAN /*AllocateDestinationString*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI RtlCreateProcessParameters(void** pProcessParameters, void* /*ImagePathName*/, void* /*DllPath*/, void* /*CurrentDirectory*/, void* /*CommandLine*/, void* /*Environment*/, void* /*WindowTitle*/, void* /*DesktopInfo*/, void* /*ShellInfo*/, void* /*RuntimeData*/) noexcept {
    if (pProcessParameters) *pProcessParameters = reinterpret_cast<void*>(0x9101);
    return NtStatus::Success;
}
inline NtStatus WINAPI RtlCreateProcessReflection(HANDLE /*ProcessHandle*/, ULONG /*Flags*/, void* /*StartRoutine*/, void* /*StartContext*/, HANDLE /*EventHandle*/, void* /*ReflectionInformation*/) noexcept { return NtStatus::Success; }
inline void* WINAPI RtlCreateQueryDebugBuffer(ULONG /*Size*/, BOOLEAN /*EventPair*/) noexcept { return reinterpret_cast<void*>(0x9102); }
inline NtStatus WINAPI RtlDestroyQueryDebugBuffer(void* /*DebugBuffer*/) noexcept { return NtStatus::Success; }

inline NtStatus WINAPI RtlCreateUserProcess(void* /*ImagePath*/, ULONG /*Attributes*/, void* /*ProcessParameters*/, void* /*ProcessSecurityDescriptor*/, void* /*ThreadSecurityDescriptor*/, HANDLE /*ParentProcess*/, BOOLEAN /*InheritHandles*/, HANDLE /*DebugPort*/, HANDLE /*ExceptionPort*/, void* /*ProcessInformation*/) noexcept { return NtStatus::Success; }
inline void WINAPI RtlExitUserProcess(NtStatus /*ExitStatus*/) noexcept {}

inline void WINAPI RtlDeleteCriticalSection(void* /*CriticalSection*/) noexcept {}
inline void WINAPI RtlInitializeCriticalSection(void* /*CriticalSection*/) noexcept {}
inline void WINAPI RtlEnterCriticalSection(void* /*CriticalSection*/) noexcept {}
inline void WINAPI RtlLeaveCriticalSection(void* /*CriticalSection*/) noexcept {}

inline NtStatus WINAPI RtlDeregisterWaitEx(HANDLE /*WaitHandle*/, HANDLE /*CompletionEvent*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI RtlRegisterWait(HANDLE* WaitHandle, HANDLE /*Handle*/, void* /*Function*/, void* /*Context*/, ULONG /*Milliseconds*/, ULONG /*Flags*/) noexcept {
    if (WaitHandle) *WaitHandle = reinterpret_cast<HANDLE>(0x9103);
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlDosPathNameToNtPathName_U_WithStatus(const wchar_t* /*DosFileName*/, void* /*NtFileName*/, wchar_t** FilePart, void* /*Reserved*/) noexcept {
    if (FilePart) *FilePart = nullptr;
    return NtStatus::Success;
}

inline wchar_t WINAPI RtlDowncaseUnicodeChar(wchar_t SourceCharacter) noexcept {
    if (SourceCharacter >= L'A' && SourceCharacter <= L'Z') return static_cast<wchar_t>(SourceCharacter + (L'a' - L'A'));
    return SourceCharacter;
}
inline wchar_t WINAPI RtlUpcaseUnicodeChar(wchar_t SourceCharacter) noexcept {
    if (SourceCharacter >= L'a' && SourceCharacter <= L'z') return static_cast<wchar_t>(SourceCharacter - (L'a' - L'A'));
    return SourceCharacter;
}

inline void* WINAPI RtlEncodePointer(void* Ptr) noexcept { return Ptr; }

inline NtStatus WINAPI RtlExpandEnvironmentStrings_U(void* /*Environment*/, void* /*Source*/, void* /*Destination*/, ULONG* ReturnedLength) noexcept {
    if (ReturnedLength) *ReturnedLength = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlFindMessage(void* /*DllHandle*/, ULONG /*MessageTableId*/, ULONG /*MessageLanguageId*/, ULONG /*MessageId*/, void** MessageEntry) noexcept {
    if (MessageEntry) *MessageEntry = nullptr;
    return NtStatus::Success;
}

inline void* WINAPI RtlFirstEntrySList(void* /*SListHead*/) noexcept { return nullptr; }
inline void* WINAPI RtlInterlockedFlushSList(void* /*SListHead*/) noexcept { return nullptr; }
inline void* WINAPI RtlInterlockedPopEntrySList(void* /*SListHead*/) noexcept { return nullptr; }
inline void* WINAPI RtlInterlockedPushEntrySList(void* /*SListHead*/, void* ListEntry) noexcept { return ListEntry; }

inline NtStatus WINAPI RtlFormatMessage(const wchar_t* /*MessageFormat*/, ULONG /*MaximumWidth*/, BOOLEAN /*IgnoreInserts*/, BOOLEAN /*ArgumentsAreAnsi*/, BOOLEAN /*ArgumentsAreAnArray*/, void* /*Arguments*/, wchar_t* Length, ULONG /*Size*/, ULONG* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlGUIDFromString(void* /*GuidString*/, GUID* Guid) noexcept {
    if (Guid) std::memset(Guid, 0, sizeof(GUID));
    return NtStatus::Success;
}
inline NtStatus WINAPI RtlStringFromGUID(const GUID* /*Guid*/, void* /*GuidString*/) noexcept { return NtStatus::Success; }

inline ULONG WINAPI RtlGetFullPathName_UEx(const wchar_t* FileName, ULONG BufferLength, wchar_t* Buffer, wchar_t** FilePart, ULONG* BytesRequired) noexcept {
    if (FilePart) *FilePart = nullptr;
    if (BytesRequired) *BytesRequired = 64;
    if (Buffer && BufferLength > 0 && FileName) {
        std::wcsncpy(Buffer, FileName, BufferLength);
        return static_cast<ULONG>(std::wcslen(Buffer));
    }
    return 0;
}

inline NtStatus WINAPI RtlGetUnloadEventTraceEx(ULONG** ElementSize, ULONG** ElementCount, void** EventTrace) noexcept {
    static ULONG elSize = 32;
    static ULONG elCount = 0;
    if (ElementSize) *ElementSize = &elSize;
    if (ElementCount) *ElementCount = &elCount;
    if (EventTrace) *EventTrace = nullptr;
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlGetVersion(void* lpVersionInformation) noexcept {
    if (lpVersionInformation) {
        auto* p = reinterpret_cast<uint32_t*>(lpVersionInformation);
        p[1] = 10; // Major = 10
        p[2] = 0;  // Minor = 0
        p[3] = 22631; // Build = 22631 (Windows 11 23H2)
        p[4] = 2;     // PlatformId = VER_PLATFORM_WIN32_NT
    }
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlIpv4AddressToStringExW(const void* /*Address*/, USHORT /*Port*/, wchar_t* AddressString, ULONG* AddressStringLength) noexcept {
    if (AddressString && AddressStringLength && *AddressStringLength >= 22) {
        std::wcsncpy(AddressString, L"127.0.0.1:8080", *AddressStringLength);
        *AddressStringLength = 14;
    }
    return NtStatus::Success;
}
inline NtStatus WINAPI RtlIpv4StringToAddressExW(const wchar_t* /*AddressString*/, BOOLEAN /*Strict*/, void* /*Address*/, USHORT* Port) noexcept {
    if (Port) *Port = 8080;
    return NtStatus::Success;
}
inline NtStatus WINAPI RtlIpv6AddressToStringExW(const void* /*Address*/, ULONG /*ScopeId*/, USHORT /*Port*/, wchar_t* AddressString, ULONG* AddressStringLength) noexcept {
    if (AddressString && AddressStringLength && *AddressStringLength >= 46) {
        std::wcsncpy(AddressString, L"::1", *AddressStringLength);
        *AddressStringLength = 3;
    }
    return NtStatus::Success;
}
inline NtStatus WINAPI RtlIpv6StringToAddressExW(const wchar_t* /*AddressString*/, void* /*Address*/, ULONG* ScopeId, USHORT* Port) noexcept {
    if (ScopeId) *ScopeId = 0;
    if (Port) *Port = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlMultiByteToUnicodeN(wchar_t* UnicodeString, ULONG MaxBytesInUnicodeString, ULONG* BytesInUnicodeString, const char* CustomString, ULONG BytesInCustomString) noexcept {
    ULONG count = (BytesInCustomString < (MaxBytesInUnicodeString / sizeof(wchar_t))) ? BytesInCustomString : (MaxBytesInUnicodeString / sizeof(wchar_t));
    if (UnicodeString && CustomString) {
        for (ULONG i = 0; i < count; ++i) UnicodeString[i] = static_cast<wchar_t>(static_cast<unsigned char>(CustomString[i]));
    }
    if (BytesInUnicodeString) *BytesInUnicodeString = count * sizeof(wchar_t);
    return NtStatus::Success;
}
inline NtStatus WINAPI RtlMultiByteToUnicodeSize(ULONG* BytesInUnicodeString, const char* /*CustomString*/, ULONG BytesInCustomString) noexcept {
    if (BytesInUnicodeString) *BytesInUnicodeString = BytesInCustomString * sizeof(wchar_t);
    return NtStatus::Success;
}
inline NtStatus WINAPI RtlUnicodeToMultiByteN(char* MultiByteString, ULONG MaxBytesInMultiByteString, ULONG* BytesInMultiByteString, const wchar_t* UnicodeString, ULONG BytesInUnicodeString) noexcept {
    ULONG wchars = BytesInUnicodeString / sizeof(wchar_t);
    ULONG count = (wchars < MaxBytesInMultiByteString) ? wchars : MaxBytesInMultiByteString;
    if (MultiByteString && UnicodeString) {
        for (ULONG i = 0; i < count; ++i) MultiByteString[i] = static_cast<char>(UnicodeString[i] & 0xFF);
    }
    if (BytesInMultiByteString) *BytesInMultiByteString = count;
    return NtStatus::Success;
}
inline NtStatus WINAPI RtlUnicodeToMultiByteSize(ULONG* BytesInMultiByteString, const wchar_t* /*UnicodeString*/, ULONG BytesInUnicodeString) noexcept {
    if (BytesInMultiByteString) *BytesInMultiByteString = BytesInUnicodeString / sizeof(wchar_t);
    return NtStatus::Success;
}
inline NtStatus WINAPI RtlUnicodeToUTF8N(char* UTF8StringDestination, ULONG UTF8StringMaxByteCount, ULONG* UTF8StringActualByteCount, const wchar_t* UnicodeStringSource, ULONG UnicodeStringByteCount) noexcept {
    return RtlUnicodeToMultiByteN(UTF8StringDestination, UTF8StringMaxByteCount, UTF8StringActualByteCount, UnicodeStringSource, UnicodeStringByteCount);
}
inline NtStatus WINAPI RtlUTF8ToUnicodeN(wchar_t* UnicodeStringDestination, ULONG UnicodeStringMaxByteCount, ULONG* UnicodeStringActualByteCount, const char* UTF8StringSource, ULONG UTF8StringByteCount) noexcept {
    return RtlMultiByteToUnicodeN(UnicodeStringDestination, UnicodeStringMaxByteCount, UnicodeStringActualByteCount, UTF8StringSource, UTF8StringByteCount);
}

inline ULONG WINAPI RtlNtStatusToDosErrorNoTeb(NtStatus Status) noexcept {
    return (Status == NtStatus::Success) ? 0 : 31;
}

inline NtStatus WINAPI RtlQueryElevationFlags(ULONG* Flags) noexcept {
    if (Flags) *Flags = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlQueryEnvironmentVariable(void* /*Environment*/, const wchar_t* /*Name*/, size_t /*NameLength*/, wchar_t* /*Value*/, size_t /*ValueLength*/, size_t* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 0;
    return NtStatus::VariableNotFound;
}

inline NtStatus WINAPI RtlQueryHeapInformation(HANDLE /*HeapHandle*/, uint32_t /*HeapInformationClass*/, void* /*HeapInformation*/, size_t /*HeapInformationLength*/, size_t* ReturnLength) noexcept {
    if (ReturnLength) *ReturnLength = 0;
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlSetHeapInformation(HANDLE /*HeapHandle*/, uint32_t /*HeapInformationClass*/, void* /*HeapInformation*/, size_t /*HeapInformationLength*/) noexcept { return NtStatus::Success; }

inline NtStatus WINAPI RtlQueryPerformanceCounter(int64_t* PerformanceCounter) noexcept {
    if (PerformanceCounter) *PerformanceCounter = 1000000;
    return NtStatus::Success;
}
inline NtStatus WINAPI RtlQueryPerformanceFrequency(int64_t* PerformanceFrequency) noexcept {
    if (PerformanceFrequency) *PerformanceFrequency = 10000000;
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlQueryProcessDebugInformation(ULONG /*ProcessId*/, ULONG /*DebugInfoClassMask*/, void* /*DebugBuffer*/) noexcept { return NtStatus::Success; }
inline NtStatus WINAPI RtlQueueApcWow64Thread(HANDLE /*ThreadHandle*/, void* /*ApcRoutine*/, void* /*ApcRoutineContext*/, void* /*ApcStatusBlock*/, void* /*ApcReserved*/) noexcept { return NtStatus::Success; }

inline void WINAPI RtlRaiseStatus(NtStatus /*Status*/) noexcept {}
inline ULONG WINAPI RtlRandomEx(ULONG* Seed) noexcept {
    if (Seed) {
        *Seed = (*Seed * 214013L + 2531011L);
        return (*Seed >> 16) & 0x7FFF;
    }
    return 42;
}

inline void* WINAPI RtlReAllocateHeap_Export(HANDLE /*HeapHandle*/, ULONG /*Flags*/, void* BaseAddress, size_t /*Size*/) noexcept { return BaseAddress; }

inline NtStatus WINAPI RtlSelfRelativeToAbsoluteSD2(void* /*SelfRelativeSecurityDescriptor*/, ULONG* BufferLength) noexcept {
    if (BufferLength) *BufferLength = 32;
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlSetCurrentDirectory_U(void* /*Path*/) noexcept { return NtStatus::Success; }

inline void WINAPI RtlTimeToTimeFields(const int64_t* /*Time*/, void* TimeFields) noexcept {
    if (TimeFields) {
        auto* tf = reinterpret_cast<int16_t*>(TimeFields);
        tf[0] = 2026; // Year
        tf[1] = 10;   // Month
        tf[2] = 9;    // Day
    }
}
inline BOOLEAN WINAPI RtlTimeFieldsToTime(void* /*TimeFields*/, int64_t* Time) noexcept {
    if (Time) *Time = 133500000000000000LL;
    return 1;
}

inline BOOLEAN WINAPI RtlValidAcl(void* /*Acl*/) noexcept { return 1; }
inline BOOLEAN WINAPI RtlValidRelativeSecurityDescriptor(void* /*SecurityDescriptorInput*/, ULONG /*SecurityDescriptorLength*/, uint32_t /*RequiredInformation*/) noexcept { return 1; }

// Threadpool (TP)
inline NtStatus WINAPI TpAllocIoCompletion(void** IoCompletion, HANDLE /*FileHandle*/, void* /*Callback*/, void* /*Context*/, void* /*Environment*/) noexcept {
    if (IoCompletion) *IoCompletion = reinterpret_cast<void*>(0x9201);
    return NtStatus::Success;
}
inline void WINAPI TpReleaseIoCompletion(void* /*IoCompletion*/) noexcept {}
inline NtStatus WINAPI TpWaitForIoCompletion(void* /*IoCompletion*/, void* /*IoStatusBlock*/) noexcept { return NtStatus::Success; }
inline void WINAPI TpStartAsyncIoOperation(void* /*IoCompletion*/) noexcept {}
inline void WINAPI TpCancelAsyncIoOperation(void* /*IoCompletion*/) noexcept {}

inline NtStatus WINAPI TpAllocPool(void** Pool, void* /*Reserved*/) noexcept {
    if (Pool) *Pool = reinterpret_cast<void*>(0x9202);
    return NtStatus::Success;
}
inline void WINAPI TpReleasePool(void* /*Pool*/) noexcept {}
inline void WINAPI TpSetPoolMaxThreads(void* /*Pool*/, ULONG /*MaxThreads*/) noexcept {}
inline BOOL WINAPI TpSetPoolMinThreads(void* /*Pool*/, ULONG /*MinThreads*/) noexcept { return TRUE_VAL; }
inline NtStatus WINAPI TpSimpleTryPost(void* /*Callback*/, void* /*Context*/, void* /*Environment*/) noexcept { return NtStatus::Success; }

// Loader (Ldr)
inline NtStatus WINAPI LdrAccessResource(void* /*DllHandle*/, void* /*ResourceInfo*/, void** ResourceData, ULONG* ResourceSize) noexcept {
    if (ResourceData) *ResourceData = nullptr;
    if (ResourceSize) *ResourceSize = 0;
    return NtStatus::Success;
}
inline NtStatus WINAPI LdrFindResource_U(void* /*DllHandle*/, void* /*ResourceInfo*/, ULONG /*Level*/, void** ResourceData) noexcept {
    if (ResourceData) *ResourceData = nullptr;
    return NtStatus::Success;
}
inline NtStatus WINAPI LdrGetProcedureAddress_Export(void* /*DllHandle*/, void* /*ProcedureName*/, ULONG /*ProcedureNumber*/, void** ProcedureAddress) noexcept {
    if (ProcedureAddress) *ProcedureAddress = reinterpret_cast<void*>(0x9301);
    return NtStatus::Success;
}
inline NtStatus WINAPI LdrLoadAlternateResourceModule(void* /*DllHandle*/, void** AlternateModule) noexcept {
    if (AlternateModule) *AlternateModule = reinterpret_cast<void*>(0x9302);
    return NtStatus::Success;
}
inline NtStatus WINAPI LdrLoadDll_Export(const wchar_t* /*DllPath*/, ULONG* /*DllCharacteristics*/, void* /*DllName*/, void** DllHandle) noexcept {
    if (DllHandle) *DllHandle = reinterpret_cast<void*>(0x9303);
    return NtStatus::Success;
}
inline BOOLEAN WINAPI LdrUnloadAlternateResourceModule(void* /*AlternateModule*/) noexcept { return 1; }
inline NtStatus WINAPI LdrUnloadDll(void* /*DllHandle*/) noexcept { return NtStatus::Success; }

inline NtStatus WINAPI NtOpenFile_Export(HANDLE* FileHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, void* /*IoStatusBlock*/, uint32_t /*ShareAccess*/, uint32_t /*OpenOptions*/) noexcept {
    if (FileHandle) *FileHandle = reinterpret_cast<HANDLE>(0x9005);
    return NtStatus::Success;
}

inline void WINAPI RtlInitUnicodeString(void* DestinationString, const wchar_t* SourceString) noexcept {
    if (!DestinationString) return;
    struct UNICODE_STRING {
        uint16_t Length;
        uint16_t MaximumLength;
        wchar_t* Buffer;
    };
    auto* us = reinterpret_cast<UNICODE_STRING*>(DestinationString);
    if (SourceString) {
        size_t len = std::wcslen(SourceString);
        us->Length = static_cast<uint16_t>(len * sizeof(wchar_t));
        us->MaximumLength = static_cast<uint16_t>((len + 1) * sizeof(wchar_t));
        us->Buffer = const_cast<wchar_t*>(SourceString);
    } else {
        us->Length = 0;
        us->MaximumLength = 0;
        us->Buffer = nullptr;
    }
}

inline void InitializeNtdllSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    ldr.registerExport("ntdll.dll", "NtOpenFile", reinterpret_cast<void*>(NtOpenFile_Export));
    ldr.registerExport("ntdll.dll", "RtlInitUnicodeString", reinterpret_cast<void*>(RtlInitUnicodeString));
    ldr.registerExport("ntdll.dll", "NtAdjustPrivilegesToken", reinterpret_cast<void*>(NtAdjustPrivilegesToken));
    ldr.registerExport("ntdll.dll", "NtCreateFile", reinterpret_cast<void*>(NtCreateFile_Export));
    ldr.registerExport("ntdll.dll", "NtDelayExecution", reinterpret_cast<void*>(NtDelayExecution_Export));
    ldr.registerExport("ntdll.dll", "NtDeviceIoControlFile", reinterpret_cast<void*>(NtDeviceIoControlFile_Export));
    ldr.registerExport("ntdll.dll", "NtDuplicateObject", reinterpret_cast<void*>(NtDuplicateObject));
    ldr.registerExport("ntdll.dll", "NtFlushBuffersFile", reinterpret_cast<void*>(NtFlushBuffersFile));
    ldr.registerExport("ntdll.dll", "NtFsControlFile", reinterpret_cast<void*>(NtFsControlFile));
    ldr.registerExport("ntdll.dll", "NtOpenProcess", reinterpret_cast<void*>(NtOpenProcess));
    ldr.registerExport("ntdll.dll", "NtOpenProcessToken", reinterpret_cast<void*>(NtOpenProcessToken_Export));
    ldr.registerExport("ntdll.dll", "NtOpenSymbolicLinkObject", reinterpret_cast<void*>(NtOpenSymbolicLinkObject));
    ldr.registerExport("ntdll.dll", "NtQueryEaFile", reinterpret_cast<void*>(NtQueryEaFile));
    ldr.registerExport("ntdll.dll", "NtQueryObject", reinterpret_cast<void*>(NtQueryObject));
    ldr.registerExport("ntdll.dll", "NtQuerySecurityObject", reinterpret_cast<void*>(NtQuerySecurityObject));
    ldr.registerExport("ntdll.dll", "NtQuerySystemInformation", reinterpret_cast<void*>(NtQuerySystemInformation_Export));
    ldr.registerExport("ntdll.dll", "NtQueryVolumeInformationFile", reinterpret_cast<void*>(NtQueryVolumeInformationFile));
    ldr.registerExport("ntdll.dll", "NtSetEaFile", reinterpret_cast<void*>(NtSetEaFile));
    ldr.registerExport("ntdll.dll", "NtSetSecurityObject", reinterpret_cast<void*>(NtSetSecurityObject));
    ldr.registerExport("ntdll.dll", "RtlCaptureContext", reinterpret_cast<void*>(RtlCaptureContext));
    ldr.registerExport("ntdll.dll", "RtlLookupFunctionEntry", reinterpret_cast<void*>(RtlLookupFunctionEntry));
    ldr.registerExport("ntdll.dll", "RtlPcToFileHeader", reinterpret_cast<void*>(RtlPcToFileHeader));
    ldr.registerExport("ntdll.dll", "RtlUnwind", reinterpret_cast<void*>(RtlUnwind));
    ldr.registerExport("ntdll.dll", "RtlUnwindEx", reinterpret_cast<void*>(RtlUnwindEx));
    ldr.registerExport("ntdll.dll", "RtlVirtualUnwind", reinterpret_cast<void*>(RtlVirtualUnwind));
    ldr.registerExport("ntdll.dll", "VerSetConditionMask", reinterpret_cast<void*>(VerSetConditionMask));

// 14. ntdll.dll (Syscalls & RTL)
    ldr.registerExport("ntdll.dll", "NtAcceptConnectPort", reinterpret_cast<void*>(NtAcceptConnectPort));
    ldr.registerExport("ntdll.dll", "NtAdjustGroupsToken", reinterpret_cast<void*>(NtAdjustGroupsToken));
    ldr.registerExport("ntdll.dll", "NtAlertThread", reinterpret_cast<void*>(NtAlertThread));
    ldr.registerExport("ntdll.dll", "NtAlpcQueryInformation", reinterpret_cast<void*>(NtAlpcQueryInformation));
    ldr.registerExport("ntdll.dll", "NtAssignProcessToJobObject", reinterpret_cast<void*>(NtAssignProcessToJobObject));
    ldr.registerExport("ntdll.dll", "NtCancelTimer", reinterpret_cast<void*>(NtCancelTimer_Export));
    ldr.registerExport("ntdll.dll", "NtClearEvent", reinterpret_cast<void*>(NtClearEvent));
    ldr.registerExport("ntdll.dll", "NtConnectPort", reinterpret_cast<void*>(NtConnectPort_Export));
    ldr.registerExport("ntdll.dll", "NtCreateDirectoryObject", reinterpret_cast<void*>(NtCreateDirectoryObject));
    ldr.registerExport("ntdll.dll", "NtCreateEvent", reinterpret_cast<void*>(NtCreateEvent_Export));
    ldr.registerExport("ntdll.dll", "NtCreateIoCompletion", reinterpret_cast<void*>(NtCreateIoCompletion_Export));
    ldr.registerExport("ntdll.dll", "NtCreateJobObject", reinterpret_cast<void*>(NtCreateJobObject));
    ldr.registerExport("ntdll.dll", "NtCreateKey", reinterpret_cast<void*>(NtCreateKey));
    ldr.registerExport("ntdll.dll", "NtCreateKeyedEvent", reinterpret_cast<void*>(NtCreateKeyedEvent));
    ldr.registerExport("ntdll.dll", "NtCreateMutant", reinterpret_cast<void*>(NtCreateMutant_Export));
    ldr.registerExport("ntdll.dll", "NtCreatePort", reinterpret_cast<void*>(NtCreatePort_Export));
    ldr.registerExport("ntdll.dll", "NtCreateProcessEx", reinterpret_cast<void*>(NtCreateProcessEx));
    ldr.registerExport("ntdll.dll", "NtCreateSemaphore", reinterpret_cast<void*>(NtCreateSemaphore));
    ldr.registerExport("ntdll.dll", "NtCreateThreadEx", reinterpret_cast<void*>(NtCreateThreadEx));
    ldr.registerExport("ntdll.dll", "NtCreateTimer", reinterpret_cast<void*>(NtCreateTimer_Export));
    ldr.registerExport("ntdll.dll", "NtDeleteKey", reinterpret_cast<void*>(NtDeleteKey));
    ldr.registerExport("ntdll.dll", "NtDeleteValueKey", reinterpret_cast<void*>(NtDeleteValueKey));
    ldr.registerExport("ntdll.dll", "NtDuplicateToken", reinterpret_cast<void*>(NtDuplicateToken));
    ldr.registerExport("ntdll.dll", "NtEnumerateKey", reinterpret_cast<void*>(NtEnumerateKey));
    ldr.registerExport("ntdll.dll", "NtEnumerateSystemEnvironmentValuesEx", reinterpret_cast<void*>(NtEnumerateSystemEnvironmentValuesEx));
    ldr.registerExport("ntdll.dll", "NtEnumerateValueKey", reinterpret_cast<void*>(NtEnumerateValueKey));
    ldr.registerExport("ntdll.dll", "NtFlushInstructionCache", reinterpret_cast<void*>(NtFlushInstructionCache));
    ldr.registerExport("ntdll.dll", "NtGetContextThread", reinterpret_cast<void*>(NtGetContextThread));
    ldr.registerExport("ntdll.dll", "NtGetNextProcess", reinterpret_cast<void*>(NtGetNextProcess));
    ldr.registerExport("ntdll.dll", "NtGetNextThread", reinterpret_cast<void*>(NtGetNextThread));
    ldr.registerExport("ntdll.dll", "NtInitiatePowerAction", reinterpret_cast<void*>(NtInitiatePowerAction));
    ldr.registerExport("ntdll.dll", "NtIsProcessInJob", reinterpret_cast<void*>(NtIsProcessInJob));
    ldr.registerExport("ntdll.dll", "NtLoadDriver", reinterpret_cast<void*>(NtLoadDriver));
    ldr.registerExport("ntdll.dll", "NtLoadKeyEx", reinterpret_cast<void*>(NtLoadKeyEx));
    ldr.registerExport("ntdll.dll", "NtLockFile", reinterpret_cast<void*>(NtLockFile));
    ldr.registerExport("ntdll.dll", "NtOpenDirectoryObject", reinterpret_cast<void*>(NtOpenDirectoryObject));
    ldr.registerExport("ntdll.dll", "NtOpenJobObject", reinterpret_cast<void*>(NtOpenJobObject));
    ldr.registerExport("ntdll.dll", "NtOpenKey", reinterpret_cast<void*>(NtOpenKey_Export));
    ldr.registerExport("ntdll.dll", "NtOpenMutant", reinterpret_cast<void*>(NtOpenMutant));
    ldr.registerExport("ntdll.dll", "NtOpenSection", reinterpret_cast<void*>(NtOpenSection));
    ldr.registerExport("ntdll.dll", "NtOpenThread", reinterpret_cast<void*>(NtOpenThread));
    ldr.registerExport("ntdll.dll", "NtOpenThreadToken", reinterpret_cast<void*>(NtOpenThreadToken));
    ldr.registerExport("ntdll.dll", "NtPowerInformation", reinterpret_cast<void*>(NtPowerInformation));
    ldr.registerExport("ntdll.dll", "NtProtectVirtualMemory", reinterpret_cast<void*>(NtProtectVirtualMemory));
    ldr.registerExport("ntdll.dll", "NtPulseEvent", reinterpret_cast<void*>(NtPulseEvent));
    ldr.registerExport("ntdll.dll", "NtQueryAttributesFile", reinterpret_cast<void*>(NtQueryAttributesFile));
    ldr.registerExport("ntdll.dll", "NtQueryDefaultLocale", reinterpret_cast<void*>(NtQueryDefaultLocale));
    ldr.registerExport("ntdll.dll", "NtQueryDirectoryObject", reinterpret_cast<void*>(NtQueryDirectoryObject));
    ldr.registerExport("ntdll.dll", "NtQueryEvent", reinterpret_cast<void*>(NtQueryEvent));
    ldr.registerExport("ntdll.dll", "NtQueryFullAttributesFile", reinterpret_cast<void*>(NtQueryFullAttributesFile));
    ldr.registerExport("ntdll.dll", "NtQueryInformationJobObject", reinterpret_cast<void*>(NtQueryInformationJobObject));
    ldr.registerExport("ntdll.dll", "NtQueryInformationThread", reinterpret_cast<void*>(NtQueryInformationThread));
    ldr.registerExport("ntdll.dll", "NtQueryInformationToken", reinterpret_cast<void*>(NtQueryInformationToken));
    ldr.registerExport("ntdll.dll", "NtQueryKey", reinterpret_cast<void*>(NtQueryKey));
    ldr.registerExport("ntdll.dll", "NtQueryMutant", reinterpret_cast<void*>(NtQueryMutant));
    ldr.registerExport("ntdll.dll", "NtQueryOpenSubKeysEx", reinterpret_cast<void*>(NtQueryOpenSubKeysEx));
    ldr.registerExport("ntdll.dll", "NtQuerySection", reinterpret_cast<void*>(NtQuerySection));
    ldr.registerExport("ntdll.dll", "NtQuerySecurityAttributesToken", reinterpret_cast<void*>(NtQuerySecurityAttributesToken));
    ldr.registerExport("ntdll.dll", "NtQuerySemaphore", reinterpret_cast<void*>(NtQuerySemaphore));
    ldr.registerExport("ntdll.dll", "NtQuerySymbolicLinkObject", reinterpret_cast<void*>(NtQuerySymbolicLinkObject));
    ldr.registerExport("ntdll.dll", "NtQuerySystemEnvironmentValueEx", reinterpret_cast<void*>(NtQuerySystemEnvironmentValueEx));
    ldr.registerExport("ntdll.dll", "NtQuerySystemInformationEx", reinterpret_cast<void*>(NtQuerySystemInformationEx));
    ldr.registerExport("ntdll.dll", "NtQuerySystemTime", reinterpret_cast<void*>(NtQuerySystemTime));
    ldr.registerExport("ntdll.dll", "NtQueryTimer", reinterpret_cast<void*>(NtQueryTimer));
    ldr.registerExport("ntdll.dll", "NtQueryTimerResolution", reinterpret_cast<void*>(NtQueryTimerResolution));
    ldr.registerExport("ntdll.dll", "NtQueryValueKey", reinterpret_cast<void*>(NtQueryValueKey_Export));
    ldr.registerExport("ntdll.dll", "NtQueryVirtualMemory", reinterpret_cast<void*>(NtQueryVirtualMemory));
    ldr.registerExport("ntdll.dll", "NtQueueApcThread", reinterpret_cast<void*>(NtQueueApcThread));
    ldr.registerExport("ntdll.dll", "NtQueueApcThreadEx", reinterpret_cast<void*>(NtQueueApcThreadEx));
    ldr.registerExport("ntdll.dll", "NtReadVirtualMemory", reinterpret_cast<void*>(NtReadVirtualMemory));
    ldr.registerExport("ntdll.dll", "NtReleaseKeyedEvent", reinterpret_cast<void*>(NtReleaseKeyedEvent));
    ldr.registerExport("ntdll.dll", "NtReleaseSemaphore", reinterpret_cast<void*>(NtReleaseSemaphore));
    ldr.registerExport("ntdll.dll", "NtRemoveIoCompletion", reinterpret_cast<void*>(NtRemoveIoCompletion_Export));
    ldr.registerExport("ntdll.dll", "NtRemoveIoCompletionEx", reinterpret_cast<void*>(NtRemoveIoCompletionEx));
    ldr.registerExport("ntdll.dll", "NtRemoveProcessDebug", reinterpret_cast<void*>(NtRemoveProcessDebug));
    ldr.registerExport("ntdll.dll", "NtReplyWaitReceivePort", reinterpret_cast<void*>(NtReplyWaitReceivePort));
    ldr.registerExport("ntdll.dll", "NtRequestWaitReplyPort", reinterpret_cast<void*>(NtRequestWaitReplyPort_Export));
    ldr.registerExport("ntdll.dll", "NtResetEvent", reinterpret_cast<void*>(NtResetEvent_Export));
    ldr.registerExport("ntdll.dll", "NtResumeProcess", reinterpret_cast<void*>(NtResumeProcess));
    ldr.registerExport("ntdll.dll", "NtResumeThread", reinterpret_cast<void*>(NtResumeThread));
    ldr.registerExport("ntdll.dll", "NtSetEvent", reinterpret_cast<void*>(NtSetEvent_Export));
    ldr.registerExport("ntdll.dll", "NtSetEventBoostPriority", reinterpret_cast<void*>(NtSetEventBoostPriority));
    ldr.registerExport("ntdll.dll", "NtSetHighEventPair", reinterpret_cast<void*>(NtSetHighEventPair));
    ldr.registerExport("ntdll.dll", "NtSetInformationDebugObject", reinterpret_cast<void*>(NtSetInformationDebugObject));
    ldr.registerExport("ntdll.dll", "NtSetInformationObject", reinterpret_cast<void*>(NtSetInformationObject));
    ldr.registerExport("ntdll.dll", "NtSetInformationProcess", reinterpret_cast<void*>(NtSetInformationProcess));
    ldr.registerExport("ntdll.dll", "NtSetInformationThread", reinterpret_cast<void*>(NtSetInformationThread));
    ldr.registerExport("ntdll.dll", "NtSetInformationToken", reinterpret_cast<void*>(NtSetInformationToken));
    ldr.registerExport("ntdll.dll", "NtSetLowEventPair", reinterpret_cast<void*>(NtSetLowEventPair));
    ldr.registerExport("ntdll.dll", "NtSetSystemEnvironmentValueEx", reinterpret_cast<void*>(NtSetSystemEnvironmentValueEx));
    ldr.registerExport("ntdll.dll", "NtSetSystemInformation", reinterpret_cast<void*>(NtSetSystemInformation));
    ldr.registerExport("ntdll.dll", "NtSetSystemPowerState", reinterpret_cast<void*>(NtSetSystemPowerState));
    ldr.registerExport("ntdll.dll", "NtSetTimer", reinterpret_cast<void*>(NtSetTimer_Export));
    ldr.registerExport("ntdll.dll", "NtSetTimerEx", reinterpret_cast<void*>(NtSetTimerEx));
    ldr.registerExport("ntdll.dll", "NtSetValueKey", reinterpret_cast<void*>(NtSetValueKey_Export));
    ldr.registerExport("ntdll.dll", "NtShutdownSystem", reinterpret_cast<void*>(NtShutdownSystem_Export));
    ldr.registerExport("ntdll.dll", "NtSuspendProcess", reinterpret_cast<void*>(NtSuspendProcess));
    ldr.registerExport("ntdll.dll", "NtSuspendThread", reinterpret_cast<void*>(NtSuspendThread));
    ldr.registerExport("ntdll.dll", "NtSystemDebugControl", reinterpret_cast<void*>(NtSystemDebugControl));
    ldr.registerExport("ntdll.dll", "NtTerminateJobObject", reinterpret_cast<void*>(NtTerminateJobObject));
    ldr.registerExport("ntdll.dll", "NtTerminateProcess", reinterpret_cast<void*>(NtTerminateProcess_Export));
    ldr.registerExport("ntdll.dll", "NtTerminateThread", reinterpret_cast<void*>(NtTerminateThread));
    ldr.registerExport("ntdll.dll", "NtTestAlert", reinterpret_cast<void*>(NtTestAlert));
    ldr.registerExport("ntdll.dll", "NtTraceControl", reinterpret_cast<void*>(NtTraceControl));
    ldr.registerExport("ntdll.dll", "NtUnloadDriver", reinterpret_cast<void*>(NtUnloadDriver));
    ldr.registerExport("ntdll.dll", "NtUnlockFile", reinterpret_cast<void*>(NtUnlockFile));
    ldr.registerExport("ntdll.dll", "NtUnlockVirtualMemory", reinterpret_cast<void*>(NtUnlockVirtualMemory));
    ldr.registerExport("ntdll.dll", "NtWaitForKeyedEvent", reinterpret_cast<void*>(NtWaitForKeyedEvent));
    ldr.registerExport("ntdll.dll", "NtWaitForMultipleObjects", reinterpret_cast<void*>(NtWaitForMultipleObjects_Export));
    ldr.registerExport("ntdll.dll", "NtWriteVirtualMemory", reinterpret_cast<void*>(NtWriteVirtualMemory));

    // RTL
    ldr.registerExport("ntdll.dll", "RtlAbsoluteToSelfRelativeSD", reinterpret_cast<void*>(RtlAbsoluteToSelfRelativeSD));
    ldr.registerExport("ntdll.dll", "RtlAcquireSRWLockShared", reinterpret_cast<void*>(RtlAcquireSRWLockShared));
    ldr.registerExport("ntdll.dll", "RtlReleaseSRWLockShared", reinterpret_cast<void*>(RtlReleaseSRWLockShared));
    ldr.registerExport("ntdll.dll", "RtlAreBitsSet", reinterpret_cast<void*>(RtlAreBitsSet));
    ldr.registerExport("ntdll.dll", "RtlClearBits", reinterpret_cast<void*>(RtlClearBits));
    ldr.registerExport("ntdll.dll", "RtlSetBits", reinterpret_cast<void*>(RtlSetBits));
    ldr.registerExport("ntdll.dll", "RtlNumberOfSetBits", reinterpret_cast<void*>(RtlNumberOfSetBits));
    ldr.registerExport("ntdll.dll", "RtlFindClearBitsAndSet", reinterpret_cast<void*>(RtlFindClearBitsAndSet));
    ldr.registerExport("ntdll.dll", "RtlConvertSidToUnicodeString", reinterpret_cast<void*>(RtlConvertSidToUnicodeString));
    ldr.registerExport("ntdll.dll", "RtlCreateProcessParameters", reinterpret_cast<void*>(RtlCreateProcessParameters));
    ldr.registerExport("ntdll.dll", "RtlCreateProcessReflection", reinterpret_cast<void*>(RtlCreateProcessReflection));
    ldr.registerExport("ntdll.dll", "RtlCreateQueryDebugBuffer", reinterpret_cast<void*>(RtlCreateQueryDebugBuffer));
    ldr.registerExport("ntdll.dll", "RtlDestroyQueryDebugBuffer", reinterpret_cast<void*>(RtlDestroyQueryDebugBuffer));
    ldr.registerExport("ntdll.dll", "RtlCreateUserProcess", reinterpret_cast<void*>(RtlCreateUserProcess));
    ldr.registerExport("ntdll.dll", "RtlExitUserProcess", reinterpret_cast<void*>(RtlExitUserProcess));
    ldr.registerExport("ntdll.dll", "RtlDeleteCriticalSection", reinterpret_cast<void*>(RtlDeleteCriticalSection));
    ldr.registerExport("ntdll.dll", "RtlInitializeCriticalSection", reinterpret_cast<void*>(RtlInitializeCriticalSection));
    ldr.registerExport("ntdll.dll", "RtlEnterCriticalSection", reinterpret_cast<void*>(RtlEnterCriticalSection));
    ldr.registerExport("ntdll.dll", "RtlLeaveCriticalSection", reinterpret_cast<void*>(RtlLeaveCriticalSection));
    ldr.registerExport("ntdll.dll", "RtlDeregisterWaitEx", reinterpret_cast<void*>(RtlDeregisterWaitEx));
    ldr.registerExport("ntdll.dll", "RtlRegisterWait", reinterpret_cast<void*>(RtlRegisterWait));
    ldr.registerExport("ntdll.dll", "RtlDosPathNameToNtPathName_U_WithStatus", reinterpret_cast<void*>(RtlDosPathNameToNtPathName_U_WithStatus));
    ldr.registerExport("ntdll.dll", "RtlDowncaseUnicodeChar", reinterpret_cast<void*>(RtlDowncaseUnicodeChar));
    ldr.registerExport("ntdll.dll", "RtlUpcaseUnicodeChar", reinterpret_cast<void*>(RtlUpcaseUnicodeChar));
    ldr.registerExport("ntdll.dll", "RtlEncodePointer", reinterpret_cast<void*>(RtlEncodePointer));
    ldr.registerExport("ntdll.dll", "RtlExpandEnvironmentStrings_U", reinterpret_cast<void*>(RtlExpandEnvironmentStrings_U));
    ldr.registerExport("ntdll.dll", "RtlFindMessage", reinterpret_cast<void*>(RtlFindMessage));
    ldr.registerExport("ntdll.dll", "RtlFirstEntrySList", reinterpret_cast<void*>(RtlFirstEntrySList));
    ldr.registerExport("ntdll.dll", "RtlInterlockedFlushSList", reinterpret_cast<void*>(RtlInterlockedFlushSList));
    ldr.registerExport("ntdll.dll", "RtlInterlockedPopEntrySList", reinterpret_cast<void*>(RtlInterlockedPopEntrySList));
    ldr.registerExport("ntdll.dll", "RtlInterlockedPushEntrySList", reinterpret_cast<void*>(RtlInterlockedPushEntrySList));
    ldr.registerExport("ntdll.dll", "RtlFormatMessage", reinterpret_cast<void*>(RtlFormatMessage));
    ldr.registerExport("ntdll.dll", "RtlGUIDFromString", reinterpret_cast<void*>(RtlGUIDFromString));
    ldr.registerExport("ntdll.dll", "RtlStringFromGUID", reinterpret_cast<void*>(RtlStringFromGUID));
    ldr.registerExport("ntdll.dll", "RtlGetFullPathName_UEx", reinterpret_cast<void*>(RtlGetFullPathName_UEx));
    ldr.registerExport("ntdll.dll", "RtlGetUnloadEventTraceEx", reinterpret_cast<void*>(RtlGetUnloadEventTraceEx));
    ldr.registerExport("ntdll.dll", "RtlGetVersion", reinterpret_cast<void*>(RtlGetVersion));
    ldr.registerExport("ntdll.dll", "RtlIpv4AddressToStringExW", reinterpret_cast<void*>(RtlIpv4AddressToStringExW));
    ldr.registerExport("ntdll.dll", "RtlIpv4StringToAddressExW", reinterpret_cast<void*>(RtlIpv4StringToAddressExW));
    ldr.registerExport("ntdll.dll", "RtlIpv6AddressToStringExW", reinterpret_cast<void*>(RtlIpv6AddressToStringExW));
    ldr.registerExport("ntdll.dll", "RtlIpv6StringToAddressExW", reinterpret_cast<void*>(RtlIpv6StringToAddressExW));
    ldr.registerExport("ntdll.dll", "RtlMultiByteToUnicodeN", reinterpret_cast<void*>(RtlMultiByteToUnicodeN));
    ldr.registerExport("ntdll.dll", "RtlMultiByteToUnicodeSize", reinterpret_cast<void*>(RtlMultiByteToUnicodeSize));
    ldr.registerExport("ntdll.dll", "RtlUnicodeToMultiByteN", reinterpret_cast<void*>(RtlUnicodeToMultiByteN));
    ldr.registerExport("ntdll.dll", "RtlUnicodeToMultiByteSize", reinterpret_cast<void*>(RtlUnicodeToMultiByteSize));
    ldr.registerExport("ntdll.dll", "RtlUnicodeToUTF8N", reinterpret_cast<void*>(RtlUnicodeToUTF8N));
    ldr.registerExport("ntdll.dll", "RtlUTF8ToUnicodeN", reinterpret_cast<void*>(RtlUTF8ToUnicodeN));
    ldr.registerExport("ntdll.dll", "RtlNtStatusToDosErrorNoTeb", reinterpret_cast<void*>(RtlNtStatusToDosErrorNoTeb));
    ldr.registerExport("ntdll.dll", "RtlQueryElevationFlags", reinterpret_cast<void*>(RtlQueryElevationFlags));
    ldr.registerExport("ntdll.dll", "RtlQueryEnvironmentVariable", reinterpret_cast<void*>(RtlQueryEnvironmentVariable));
    ldr.registerExport("ntdll.dll", "RtlQueryHeapInformation", reinterpret_cast<void*>(RtlQueryHeapInformation));
    ldr.registerExport("ntdll.dll", "RtlSetHeapInformation", reinterpret_cast<void*>(RtlSetHeapInformation));
    ldr.registerExport("ntdll.dll", "RtlQueryPerformanceCounter", reinterpret_cast<void*>(RtlQueryPerformanceCounter));
    ldr.registerExport("ntdll.dll", "RtlQueryPerformanceFrequency", reinterpret_cast<void*>(RtlQueryPerformanceFrequency));
    ldr.registerExport("ntdll.dll", "RtlQueryProcessDebugInformation", reinterpret_cast<void*>(RtlQueryProcessDebugInformation));
    ldr.registerExport("ntdll.dll", "RtlQueueApcWow64Thread", reinterpret_cast<void*>(RtlQueueApcWow64Thread));
    ldr.registerExport("ntdll.dll", "RtlRaiseStatus", reinterpret_cast<void*>(RtlRaiseStatus));
    ldr.registerExport("ntdll.dll", "RtlRandomEx", reinterpret_cast<void*>(RtlRandomEx));
    ldr.registerExport("ntdll.dll", "RtlReAllocateHeap", reinterpret_cast<void*>(RtlReAllocateHeap_Export));
    ldr.registerExport("ntdll.dll", "RtlSelfRelativeToAbsoluteSD2", reinterpret_cast<void*>(RtlSelfRelativeToAbsoluteSD2));
    ldr.registerExport("ntdll.dll", "RtlSetCurrentDirectory_U", reinterpret_cast<void*>(RtlSetCurrentDirectory_U));
    ldr.registerExport("ntdll.dll", "RtlTimeToTimeFields", reinterpret_cast<void*>(RtlTimeToTimeFields));
    ldr.registerExport("ntdll.dll", "RtlTimeFieldsToTime", reinterpret_cast<void*>(RtlTimeFieldsToTime));
    ldr.registerExport("ntdll.dll", "RtlValidAcl", reinterpret_cast<void*>(RtlValidAcl));
    ldr.registerExport("ntdll.dll", "RtlValidRelativeSecurityDescriptor", reinterpret_cast<void*>(RtlValidRelativeSecurityDescriptor));

    // Threadpool
    ldr.registerExport("ntdll.dll", "TpAllocIoCompletion", reinterpret_cast<void*>(TpAllocIoCompletion));
    ldr.registerExport("ntdll.dll", "TpReleaseIoCompletion", reinterpret_cast<void*>(TpReleaseIoCompletion));
    ldr.registerExport("ntdll.dll", "TpWaitForIoCompletion", reinterpret_cast<void*>(TpWaitForIoCompletion));
    ldr.registerExport("ntdll.dll", "TpStartAsyncIoOperation", reinterpret_cast<void*>(TpStartAsyncIoOperation));
    ldr.registerExport("ntdll.dll", "TpCancelAsyncIoOperation", reinterpret_cast<void*>(TpCancelAsyncIoOperation));
    ldr.registerExport("ntdll.dll", "TpAllocPool", reinterpret_cast<void*>(TpAllocPool));
    ldr.registerExport("ntdll.dll", "TpReleasePool", reinterpret_cast<void*>(TpReleasePool));
    ldr.registerExport("ntdll.dll", "TpSetPoolMaxThreads", reinterpret_cast<void*>(TpSetPoolMaxThreads));
    ldr.registerExport("ntdll.dll", "TpSetPoolMinThreads", reinterpret_cast<void*>(TpSetPoolMinThreads));
    ldr.registerExport("ntdll.dll", "TpSimpleTryPost", reinterpret_cast<void*>(TpSimpleTryPost));

    // Loader
    ldr.registerExport("ntdll.dll", "LdrAccessResource", reinterpret_cast<void*>(LdrAccessResource));
    ldr.registerExport("ntdll.dll", "LdrFindResource_U", reinterpret_cast<void*>(LdrFindResource_U));
    ldr.registerExport("ntdll.dll", "LdrGetProcedureAddress", reinterpret_cast<void*>(LdrGetProcedureAddress_Export));
    ldr.registerExport("ntdll.dll", "LdrLoadAlternateResourceModule", reinterpret_cast<void*>(LdrLoadAlternateResourceModule));
    ldr.registerExport("ntdll.dll", "LdrLoadDll", reinterpret_cast<void*>(LdrLoadDll_Export));
    ldr.registerExport("ntdll.dll", "LdrUnloadAlternateResourceModule", reinterpret_cast<void*>(LdrUnloadAlternateResourceModule));
    ldr.registerExport("ntdll.dll", "LdrUnloadDll", reinterpret_cast<void*>(LdrUnloadDll));
}

} // namespace micant::ntdll

