#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <span>
#include <cwchar>
#include <mutex>
#include <vector>
#include <bit>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/context.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/core/dispatcher.hpp"
#include "micant/km/executive/heap.hpp"
#include "micant/um/native/ldr.hpp"

namespace micant::ntdll {

// ============================================================================
// Vectored Exception & Continue Handling (VEH / VCH)
// ============================================================================

using micant::CONTEXT;
using micant::PCONTEXT;
using micant::LPCONTEXT;

#ifndef EXCEPTION_MAXIMUM_PARAMETERS
#define EXCEPTION_MAXIMUM_PARAMETERS 15
#endif

#ifndef EXCEPTION_CONTINUE_SEARCH
#define EXCEPTION_CONTINUE_SEARCH 0
#endif

#ifndef EXCEPTION_CONTINUE_EXECUTION
#define EXCEPTION_CONTINUE_EXECUTION (-1)
#endif

inline constexpr uint32_t EXCEPTION_ACCESS_VIOLATION = 0xC0000005;
inline constexpr uint32_t EXCEPTION_BREAKPOINT = 0x80000003;
inline constexpr uint32_t EXCEPTION_SINGLE_STEP = 0x80000004;
inline constexpr uint32_t EXCEPTION_GUARD_PAGE = 0x80000001;
inline constexpr uint32_t EXCEPTION_ILLEGAL_INSTRUCTION = 0xC000001D;
inline constexpr uint32_t EXCEPTION_DATATYPE_MISALIGNMENT = 0x80000002;

struct EXCEPTION_RECORD {
    uint32_t ExceptionCode{0};
    uint32_t ExceptionFlags{0};
    struct EXCEPTION_RECORD* ExceptionRecord{nullptr};
    void* ExceptionAddress{nullptr};
    uint32_t NumberParameters{0};
    uintptr_t ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS]{0};
};
using PEXCEPTION_RECORD = EXCEPTION_RECORD*;

struct EXCEPTION_POINTERS {
    PEXCEPTION_RECORD ExceptionRecord{nullptr};
    PCONTEXT          ContextRecord{nullptr};
};
using PEXCEPTION_POINTERS = EXCEPTION_POINTERS*;

using PVECTORED_EXCEPTION_HANDLER = int32_t (WINAPI *)(PEXCEPTION_POINTERS ExceptionInfo);

class VectoredHandlerManager {
public:
    static VectoredHandlerManager& get() noexcept {
        static VectoredHandlerManager instance;
        return instance;
    }

    struct HandlerEntry {
        uintptr_t id;
        PVECTORED_EXCEPTION_HANDLER handler;
    };

    void* addExceptionHandler(uint32_t First, PVECTORED_EXCEPTION_HANDLER Handler) noexcept {
        if (!Handler) return nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        uintptr_t id = ++m_nextId;
        HandlerEntry entry{ id, Handler };
        if (First) {
            m_exceptionHandlers.insert(m_exceptionHandlers.begin(), entry);
        } else {
            m_exceptionHandlers.push_back(entry);
        }
        return reinterpret_cast<void*>(id);
    }

    uint32_t removeExceptionHandler(void* Handle) noexcept {
        if (!Handle) return 0;
        std::lock_guard<std::mutex> lock(m_mutex);
        uintptr_t id = reinterpret_cast<uintptr_t>(Handle);
        for (auto it = m_exceptionHandlers.begin(); it != m_exceptionHandlers.end(); ++it) {
            if (it->id == id) {
                m_exceptionHandlers.erase(it);
                return 1;
            }
        }
        return 0;
    }

    void* addContinueHandler(uint32_t First, PVECTORED_EXCEPTION_HANDLER Handler) noexcept {
        if (!Handler) return nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        uintptr_t id = ++m_nextId;
        HandlerEntry entry{ id, Handler };
        if (First) {
            m_continueHandlers.insert(m_continueHandlers.begin(), entry);
        } else {
            m_continueHandlers.push_back(entry);
        }
        return reinterpret_cast<void*>(id);
    }

    uint32_t removeContinueHandler(void* Handle) noexcept {
        if (!Handle) return 0;
        std::lock_guard<std::mutex> lock(m_mutex);
        uintptr_t id = reinterpret_cast<uintptr_t>(Handle);
        for (auto it = m_continueHandlers.begin(); it != m_continueHandlers.end(); ++it) {
            if (it->id == id) {
                m_continueHandlers.erase(it);
                return 1;
            }
        }
        return 0;
    }

    int32_t dispatchException(PEXCEPTION_POINTERS ExceptionInfo) noexcept {
        if (!ExceptionInfo) return EXCEPTION_CONTINUE_SEARCH;
        std::vector<PVECTORED_EXCEPTION_HANDLER> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshot.reserve(m_exceptionHandlers.size());
            for (const auto& entry : m_exceptionHandlers) {
                snapshot.push_back(entry.handler);
            }
        }
        for (auto handler : snapshot) {
            int32_t result = handler(ExceptionInfo);
            if (result == EXCEPTION_CONTINUE_EXECUTION) {
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    int32_t dispatchContinue(PEXCEPTION_POINTERS ExceptionInfo) noexcept {
        if (!ExceptionInfo) return EXCEPTION_CONTINUE_SEARCH;
        std::vector<PVECTORED_EXCEPTION_HANDLER> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshot.reserve(m_continueHandlers.size());
            for (const auto& entry : m_continueHandlers) {
                snapshot.push_back(entry.handler);
            }
        }
        for (auto handler : snapshot) {
            int32_t result = handler(ExceptionInfo);
            if (result == EXCEPTION_CONTINUE_EXECUTION) {
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    size_t getExceptionHandlerCount() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_exceptionHandlers.size();
    }

    size_t getContinueHandlerCount() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_continueHandlers.size();
    }

    void clear() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_exceptionHandlers.clear();
        m_continueHandlers.clear();
    }

private:
    mutable std::mutex m_mutex;
    uintptr_t m_nextId{0x2000};
    std::vector<HandlerEntry> m_exceptionHandlers;
    std::vector<HandlerEntry> m_continueHandlers;
};

inline void* WINAPI RtlAddVectoredExceptionHandler(uint32_t First, PVECTORED_EXCEPTION_HANDLER Handler) noexcept {
    return VectoredHandlerManager::get().addExceptionHandler(First, Handler);
}

inline uint32_t WINAPI RtlRemoveVectoredExceptionHandler(void* Handle) noexcept {
    return VectoredHandlerManager::get().removeExceptionHandler(Handle);
}

inline void* WINAPI RtlAddVectoredContinueHandler(uint32_t First, PVECTORED_EXCEPTION_HANDLER Handler) noexcept {
    return VectoredHandlerManager::get().addContinueHandler(First, Handler);
}

inline uint32_t WINAPI RtlRemoveVectoredContinueHandler(void* Handle) noexcept {
    return VectoredHandlerManager::get().removeContinueHandler(Handle);
}

inline int32_t WINAPI RtlDispatchVectoredException(PEXCEPTION_POINTERS ExceptionInfo) noexcept {
    return VectoredHandlerManager::get().dispatchException(ExceptionInfo);
}

inline int32_t WINAPI RtlDispatchVectoredContinueHandler(PEXCEPTION_POINTERS ExceptionInfo) noexcept {
    return VectoredHandlerManager::get().dispatchContinue(ExceptionInfo);
}

inline size_t RtlGetVectoredExceptionHandlerCount() noexcept {
    return VectoredHandlerManager::get().getExceptionHandlerCount();
}

inline size_t RtlGetVectoredContinueHandlerCount() noexcept {
    return VectoredHandlerManager::get().getContinueHandlerCount();
}

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
        case NtStatus::MediaWriteProtected:     return 19;  // ERROR_WRITE_PROTECT
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

#include <cstring>

using ULONG = uint32_t;
using ULONG64 = uint64_t;
using LONG64 = int64_t;
using ULONG_PTR = uintptr_t;
using CCHAR = char;
using CHAR = char;
using BOOLEAN = uint8_t;

#ifndef _MIO_GUID_DEFINED
#define _MIO_GUID_DEFINED
struct GUID {
    uint32_t Data1{0};
    uint16_t Data2{0};
    uint16_t Data3{0};
    uint8_t  Data4[8]{0};
    bool operator==(const GUID& o) const noexcept {
        return std::memcmp(this, &o, sizeof(GUID)) == 0;
    }
};
#endif

// Authentic System Information Structures for Anti-Cheat & Protected Game Engines
struct SYSTEM_BASIC_INFORMATION {
    ULONG Reserved;
    ULONG TimerResolution;
    ULONG PageSize;
    ULONG NumberOfPhysicalPages;
    ULONG LowestPhysicalPageNumber;
    ULONG HighestPhysicalPageNumber;
    ULONG AllocationGranularity;
    ULONG_PTR MinimumUserModeAddress;
    ULONG_PTR MaximumUserModeAddress;
    ULONG_PTR ActiveProcessorsAffinityMask;
    CCHAR NumberOfProcessors;
};

struct SYSTEM_KERNEL_DEBUGGER_INFORMATION {
    BOOLEAN KernelDebuggerEnabled;
    BOOLEAN KernelDebuggerNotPresent;
};

struct SYSTEM_CODEINTEGRITY_INFORMATION {
    ULONG Length;
    ULONG CodeIntegrityOptions;
};

inline constexpr ULONG CODEINTEGRITY_OPTION_ENABLED           = 0x00000001;
inline constexpr ULONG CODEINTEGRITY_OPTION_TESTSIGN          = 0x00000002;
inline constexpr ULONG CODEINTEGRITY_OPTION_UMCI_ENABLED      = 0x00000004;
inline constexpr ULONG CODEINTEGRITY_OPTION_UMCI_AUDIT        = 0x00000008;
inline constexpr ULONG CODEINTEGRITY_OPTION_HVCI_KMCI_ENABLED = 0x00000400;

struct SYSTEM_CODEINTEGRITYPOLICY_INFORMATION {
    ULONG Options;
    ULONG HwId[4];
    ULONG Version;
    ULONG Reserved;
};

// Class 197: SystemHypervisorSharedPageInformation
struct SYSTEM_HYPERVISOR_SHARED_PAGE_INFORMATION {
    void* HypervisorSharedUserVa;
};

// Class 202: SystemDmaGuardPolicyInformation
struct SYSTEM_DMA_GUARD_POLICY_INFORMATION {
    BOOLEAN DmaGuardPolicyEnabled;
};

// Class 206: SystemSpeculationControlInformationEx
struct SYSTEM_SPECULATION_CONTROL_INFORMATION_EX {
    union {
        ULONG Flags;
        struct {
            ULONG BpbEnabled : 1;
            ULONG IbrsPresent : 1;
            ULONG StibpPresent : 1;
            ULONG SmepPresent : 1;
            ULONG SpeculativeStoreBypassDisableAvailable : 1;
            ULONG SpeculativeStoreBypassDisableSupported : 1;
            ULONG IntelRsbFlushAvailable : 1;
            ULONG BhbClearAvailable : 1;
            ULONG BranchHistoryBarrierSupported : 1;
            ULONG Reserved : 23;
        };
    } SpeculationControlFlagsEx;
    union {
        ULONG Flags2;
        struct {
            ULONG SbdrFlushAvailable : 1;
            ULONG FbsdpClearAvailable : 1;
            ULONG PsdpClearAvailable : 1;
            ULONG FbssClearAvailable : 1;
            ULONG Reserved : 28;
        };
    } SpeculationControlFlagsEx2;
};
using PSYSTEM_SPECULATION_CONTROL_INFORMATION_EX = SYSTEM_SPECULATION_CONTROL_INFORMATION_EX*;

// Class 208: SystemProcessorMicrocodeUpdateInformation
struct SYSTEM_PROCESSOR_MICROCODE_UPDATE_INFORMATION {
    ULONG MicrocodeUpdateStatus; // 0 = STATUS_SUCCESS
    ULONG CurrentRevision;       // e.g. 0x000000F4
    ULONG MinimumRequiredRevision; // e.g. 0x000000B0
    ULONG CpuVendor;             // 1 = GenuineIntel, 2 = AuthenticAMD
};
using PSYSTEM_PROCESSOR_MICROCODE_UPDATE_INFORMATION = SYSTEM_PROCESSOR_MICROCODE_UPDATE_INFORMATION*;

// Class 213: SystemSecureSpeculationControlInformation
struct SYSTEM_SECURE_SPECULATION_CONTROL_INFORMATION {
    union {
        ULONG Flags;
        struct {
            ULONG KvaShadowSupported : 1;
            ULONG KvaShadowEnabled : 1;
            ULONG KvaShadowUserGlobal : 1;
            ULONG KvaShadowPcid : 1;
            ULONG KvaShadowInvpcid : 1;
            ULONG KvaShadowRequired : 1;
            ULONG KvaShadowRequiredAvailable : 1;
            ULONG IbrsPresent : 1;
            ULONG IbrsEnabled : 1;
            ULONG IbrsUserEnabled : 1;
            ULONG RetpolinePresent : 1;
            ULONG RetpolineEnabled : 1;
            ULONG Reserved : 20;
        };
    };
};
using PSYSTEM_SECURE_SPECULATION_CONTROL_INFORMATION = SYSTEM_SECURE_SPECULATION_CONTROL_INFORMATION*;

// Class 214: SystemSpacesBootInformation
struct SYSTEM_SPACES_BOOT_INFORMATION {
    ULONG BootFlags;
    ULONG SpacesFlags;
    ULONG Reserved[2];
};
using PSYSTEM_SPACES_BOOT_INFORMATION = SYSTEM_SPACES_BOOT_INFORMATION*;

// Class 215: SystemFwRamdiskInformation
struct SYSTEM_FIRMWARE_RAMDISK_INFORMATION {
    ULONG Version;
    ULONG BlockSize;
    uint64_t BaseAddress;
    uint64_t Size;
};
using PSYSTEM_FIRMWARE_RAMDISK_INFORMATION = SYSTEM_FIRMWARE_RAMDISK_INFORMATION*;

// Class 216: SystemWheaIpmiHardwareInformation
struct SYSTEM_WHEA_IPMI_HARDWARE_INFORMATION {
    ULONG Version;
    ULONG IpmiInterfaceType;
    uint64_t RecordId;
    ULONG EventType;
    ULONG DataLength;
    uint8_t Data[16];
};
using PSYSTEM_WHEA_IPMI_HARDWARE_INFORMATION = SYSTEM_WHEA_IPMI_HARDWARE_INFORMATION*;

// Class 217: SystemCodeIntegrityPolicyInformation
struct SYSTEM_CODE_INTEGRITY_POLICY_INFORMATION {
    ULONG Options;
    uint32_t HwIdCount;
    uint64_t PolicyFlags;
};
using PSYSTEM_CODE_INTEGRITY_POLICY_INFORMATION = SYSTEM_CODE_INTEGRITY_POLICY_INFORMATION*;

// Class 218: SystemNumaProximityInformation
struct SYSTEM_NUMA_PROXIMITY_INFORMATION {
    ULONG NodeNumber;
    ULONG ProximityId;
};
using PSYSTEM_NUMA_PROXIMITY_INFORMATION = SYSTEM_NUMA_PROXIMITY_INFORMATION*;

// Class 219: SystemDefragInformation
struct SYSTEM_DEFRAG_INFORMATION {
    ULONG Flags;
    ULONG DefragPass;
};
using PSYSTEM_DEFRAG_INFORMATION = SYSTEM_DEFRAG_INFORMATION*;

// Class 220: SystemVirtualizationInformation
struct SYSTEM_VIRTUALIZATION_INFORMATION {
    ULONG VirtualizationFlags;
    ULONG HypervisorVendorId;
};
using PSYSTEM_VIRTUALIZATION_INFORMATION = SYSTEM_VIRTUALIZATION_INFORMATION*;

// Class 221: SystemShadowStackInformation
struct SYSTEM_SHADOW_STACK_INFORMATION {
    union {
        ULONG Flags;
        struct {
            ULONG CetCapable : 1;
            ULONG UserCetAllowed : 1;
            ULONG KernelCetEnabled : 1;
            ULONG KernelCetAuditModeEnabled : 1;
            ULONG Reserved : 28;
        };
    };
};
using PSYSTEM_SHADOW_STACK_INFORMATION = SYSTEM_SHADOW_STACK_INFORMATION*;

// Class 222: SystemBuildVersionInformation
struct SYSTEM_BUILD_VERSION_INFORMATION {
    ULONG LayerNumber;
    ULONG LayerCount;
    ULONG OsMajorVersion;
    ULONG OsMinorVersion;
    ULONG NtBuildNumber;
    ULONG NtBuildQfe;
    unsigned char LayerName[128];
    unsigned char NtBuildBranch[128];
    unsigned char NtBuildLab[128];
    unsigned char NtBuildStamp[128];
    unsigned char NtBuildArch[64];
    union {
        ULONG Flags;
        struct {
            ULONG IsTopLevel : 1;
            ULONG IsEmbedded : 1;
            ULONG Reserved : 30;
        };
    };
};
using PSYSTEM_BUILD_VERSION_INFORMATION = SYSTEM_BUILD_VERSION_INFORMATION*;

// Class 223: SystemPoolLimitInformation
struct SYSTEM_POOL_LIMIT_INFORMATION {
    ULONG_PTR PagedPoolLimit;
    ULONG_PTR NonPagedPoolLimit;
    ULONG_PTR PagedPoolSize;
    ULONG_PTR NonPagedPoolSize;
    ULONG     PagedPoolWarningLimit;
    ULONG     NonPagedPoolWarningLimit;
};
using PSYSTEM_POOL_LIMIT_INFORMATION = SYSTEM_POOL_LIMIT_INFORMATION*;

// Class 224: SystemCodeIntegrityAddDynamicStore
struct SYSTEM_CODE_INTEGRITY_DYNAMIC_STORE_INFORMATION {
    ULONG Flags;
    ULONG DynamicStoreId;
    ULONG CatalogCount;
    ULONG HashAlgorithm;
    GUID  StoreGuid;
};
using PSYSTEM_CODE_INTEGRITY_DYNAMIC_STORE_INFORMATION = SYSTEM_CODE_INTEGRITY_DYNAMIC_STORE_INFORMATION*;

// Class 225: SystemCodeIntegrityUnlockInformation
struct SYSTEM_CODE_INTEGRITY_UNLOCK_INFORMATION {
    ULONG UnlockFlags;
    ULONG UnlockVersion;
    GUID  UnlockId;
    ULONG UnlockSequenceNumber;
    ULONG Flags;
};
using PSYSTEM_CODE_INTEGRITY_UNLOCK_INFORMATION = SYSTEM_CODE_INTEGRITY_UNLOCK_INFORMATION*;

// Class 226: SystemLeapSecondInformation
struct SYSTEM_LEAP_SECOND_INFORMATION {
    BOOLEAN      Enabled;
    ULONG        Flags;
    LargeInteger NextLeapSecond;
    LargeInteger PreviousLeapSecond;
};
using PSYSTEM_LEAP_SECOND_INFORMATION = SYSTEM_LEAP_SECOND_INFORMATION*;

// Class 227: SystemMemoryTopologyInformation
struct SYSTEM_MEMORY_TOPOLOGY_INFORMATION {
    ULONG ChannelCount;
    ULONG DimmCount;
    ULONG BusWidthBits;
    ULONG MemorySpeedMTs;
    ULONG BandwidthGBs;
    ULONG AverageLatencyNs;
};
using PSYSTEM_MEMORY_TOPOLOGY_INFORMATION = SYSTEM_MEMORY_TOPOLOGY_INFORMATION*;

// Class 228: SystemDpcWatchdogInformation
struct SYSTEM_DPC_WATCHDOG_INFORMATION {
    ULONG DpcTimeLimitTicks;
    ULONG DpcWatchdogSequence;
    ULONG TotalDpcTime;
    ULONG TotalInterruptTime;
};
using PSYSTEM_DPC_WATCHDOG_INFORMATION = SYSTEM_DPC_WATCHDOG_INFORMATION*;

// Class 229: SystemPoolZeroingInformation
struct SYSTEM_POOL_ZEROING_INFORMATION {
    BOOLEAN PoolZeroingSupported;
    BOOLEAN PoolZeroingEnabled;
    ULONG Reserved;
    uint64_t ZeroedPagesCount;
    uint64_t TotalAllocationsZeroed;
};
using PSYSTEM_POOL_ZEROING_INFORMATION = SYSTEM_POOL_ZEROING_INFORMATION*;

// Class 230: SystemCodeIntegrityPlatformManifestInformation
struct SYSTEM_CODE_INTEGRITY_PLATFORM_MANIFEST_INFORMATION {
    BOOLEAN PlatformManifestPresent;
    uint8_t ManifestHashType;
    uint16_t ManifestCount;
    ULONG PlatformPolicyVersion;
    uint8_t PlatformId[16];
};
using PSYSTEM_CODE_INTEGRITY_PLATFORM_MANIFEST_INFORMATION = SYSTEM_CODE_INTEGRITY_PLATFORM_MANIFEST_INFORMATION*;

// Class 231: SystemInterruptCpuSetsInformation
struct SYSTEM_INTERRUPT_CPU_SET_INFORMATION {
    uint32_t NumberOfCpuSets;
    uint32_t ActiveCpuSetMask;
    uint32_t DpcCpuSetMask;
    uint32_t Flags;
};
using PSYSTEM_INTERRUPT_CPU_SET_INFORMATION = SYSTEM_INTERRUPT_CPU_SET_INFORMATION*;

// Class 232: SystemSecureKernelProfileInformation
struct SYSTEM_SECURE_KERNEL_PROFILE_INFORMATION {
    uint32_t TotalPages;
    uint32_t ActivePages;
    uint32_t VsmEnclaveCount;
    uint32_t Flags;
};
using PSYSTEM_SECURE_KERNEL_PROFILE_INFORMATION = SYSTEM_SECURE_KERNEL_PROFILE_INFORMATION*;

// Class 233: SystemCodeIntegritySyntheticCacheInformation
struct SYSTEM_CODE_INTEGRITY_SYNTHETIC_CACHE_INFORMATION {
    uint32_t SyntheticCacheFlags;
    uint32_t CacheEntryCount;
    uint32_t TotalCachedBytes;
    uint32_t HitCount;
    uint32_t MissCount;
};
using PSYSTEM_CODE_INTEGRITY_SYNTHETIC_CACHE_INFORMATION = SYSTEM_CODE_INTEGRITY_SYNTHETIC_CACHE_INFORMATION*;

// Class 234: SystemFirmwareRamdumpInformation
struct SYSTEM_FIRMWARE_RAMDUMP_INFORMATION {
    uint32_t RamdumpSupported;
    uint32_t RamdumpEnabled;
    uint32_t Reserved;
    uint64_t RamdumpBufferSize;
    uint64_t RamdumpPhysicalAddress;
};
using PSYSTEM_FIRMWARE_RAMDUMP_INFORMATION = SYSTEM_FIRMWARE_RAMDUMP_INFORMATION*;

// Class 235: SystemPhysicalMemoryZeroingInformation
struct SYSTEM_PHYSICAL_MEMORY_ZEROING_INFORMATION {
    uint64_t TotalPhysicalBytesZeroed;
    uint64_t PendingZeroingBytes;
    uint32_t ActiveZeroingWorkerThreads;
    uint32_t ZeroingEfficiencyRating;
    uint32_t Flags;
    uint32_t Reserved;
};
using PSYSTEM_PHYSICAL_MEMORY_ZEROING_INFORMATION = SYSTEM_PHYSICAL_MEMORY_ZEROING_INFORMATION*;

// Class 236: SystemHypervisorProcessorPerformanceInformation
struct SYSTEM_HYPERVISOR_PROCESSOR_PERFORMANCE_INFORMATION {
    uint32_t ProcessorIndex;
    uint32_t HypervisorEnlightenmentFlags;
    uint64_t GuestKernelTime100ns;
    uint64_t GuestUserTime100ns;
    uint64_t HypervisorOverheadTime100ns;
    uint32_t HypercallCount;
    uint32_t Reserved;
};
using PSYSTEM_HYPERVISOR_PROCESSOR_PERFORMANCE_INFORMATION = SYSTEM_HYPERVISOR_PROCESSOR_PERFORMANCE_INFORMATION*;

// Class 237: SystemNumaProximityInformation (Extended)
struct SYSTEM_NUMA_PROXIMITY_INFORMATION_EX {
    ULONG    NodeId;
    ULONG    ProximityId;
    ULONG    Flags;
    ULONG    DistanceRatio;
    ULONG64  TotalMemoryBytes;
    ULONG64  AvailableMemoryBytes;
};
using PSYSTEM_NUMA_PROXIMITY_INFORMATION_EX = SYSTEM_NUMA_PROXIMITY_INFORMATION_EX*;

// Class 238: SystemEntropyPoolInformation
struct SYSTEM_ENTROPY_POOL_INFORMATION {
    ULONG    PoolCapacityBits;
    ULONG    CurrentEntropyBits;
    ULONG    ReseedCount;
    ULONG    JitterEntropyStatus;
    ULONG    HardwareRngHealth;
    ULONG    Reserved;
};
using PSYSTEM_ENTROPY_POOL_INFORMATION = SYSTEM_ENTROPY_POOL_INFORMATION*;

// Class 239: SystemProcessorMicrocodeInformation
struct SYSTEM_PROCESSOR_MICROCODE_INFORMATION {
    ULONG    ProcessorIndex;
    ULONG    CurrentMicrocodeRevision;
    ULONG    MinimumSupportedRevision;
    ULONG    Flags;
    ULONG64  MicrocodeReleaseDate;
};
using PSYSTEM_PROCESSOR_MICROCODE_INFORMATION = SYSTEM_PROCESSOR_MICROCODE_INFORMATION*;

// Class 240: SystemFirmwareSecureBootPolicyInformation
struct SYSTEM_FIRMWARE_SECUREBOOT_POLICY_INFORMATION {
    ULONG    SecureBootEnabled;
    ULONG    SecureBootConfigured;
    ULONG    PolicyPublisher;
    ULONG    PolicyVersion;
    ULONG    Flags;
    ULONG    Reserved;
};
using PSYSTEM_FIRMWARE_SECUREBOOT_POLICY_INFORMATION = SYSTEM_FIRMWARE_SECUREBOOT_POLICY_INFORMATION*;

// Class 241: SystemRootSiloInformation
struct SYSTEM_ROOT_SILO_INFORMATION {
    ULONG NumberOfRootSilos;
    ULONG ActiveSilosCount;
    ULONG SiloFlags;
    ULONG MaxSiloDepth;
};
using PSYSTEM_ROOT_SILO_INFORMATION = SYSTEM_ROOT_SILO_INFORMATION*;

// Class 242: SystemMemoryPartitionInformation
struct SYSTEM_MEMORY_PARTITION_INFORMATION {
    ULONG    PartitionCount;
    ULONG    ActivePartitions;
    ULONG64  TotalPartitionMemoryBytes;
    ULONG64  AvailablePartitionMemoryBytes;
    ULONG    NumaNodesCovered;
    ULONG    Reserved;
};
using PSYSTEM_MEMORY_PARTITION_INFORMATION = SYSTEM_MEMORY_PARTITION_INFORMATION*;

// Class 243: SystemSiloMemoryInformation
struct SYSTEM_SILO_MEMORY_INFORMATION {
    ULONG    SiloId;
    ULONG    SiloFlags;
    ULONG64  CommittedMemoryBytes;
    ULONG64  CommitLimitMemoryBytes;
    ULONG64  WorkingSetBytes;
    ULONG64  PeakWorkingSetBytes;
    ULONG64  SharedMemoryBytes;
    ULONG    ProcessCount;
    ULONG    Reserved;
};
using PSYSTEM_SILO_MEMORY_INFORMATION = SYSTEM_SILO_MEMORY_INFORMATION*;

// Class 244: SystemMemoryTopologyExInformation
struct SYSTEM_MEMORY_TOPOLOGY_EX_INFORMATION {
    ULONG    NumaNodeCount;
    ULONG    ChannelCount;
    ULONG    DimmSlotCount;
    ULONG    PopulatedDimmCount;
    ULONG64  TotalPhysicalBytes;
    ULONG64  AvailablePhysicalBytes;
    ULONG    InterleaveWays;
    ULONG    MemorySpeedMhz;
    ULONG    MemoryType;
    ULONG    Reserved;
};
using PSYSTEM_MEMORY_TOPOLOGY_EX_INFORMATION = SYSTEM_MEMORY_TOPOLOGY_EX_INFORMATION*;

// Class 245: SystemSiloRootDirectoryInformation
struct SYSTEM_SILO_ROOT_DIRECTORY_INFORMATION {
    ULONG    SiloId;
    ULONG    Reserved;
    uint64_t RootDirectoryHandle;
    ULONG    DirectoryPathLength;
    wchar_t  DirectoryPath[260];
    ULONG    Flags;
};
using PSYSTEM_SILO_ROOT_DIRECTORY_INFORMATION = SYSTEM_SILO_ROOT_DIRECTORY_INFORMATION*;

// Class 246: SystemPhysicalMemoryBandwidthInformation
struct SYSTEM_PHYSICAL_MEMORY_BANDWIDTH_INFORMATION {
    ULONG    NumaNode;
    ULONG    ChannelCount;
    ULONG64  TotalReadBandwidthMBs;
    ULONG64  TotalWriteBandwidthMBs;
    ULONG64  PeakBandwidthMBs;
    ULONG    ThrottledChannels;
    ULONG    BandwidthEfficiencyPercent;
    ULONG    Reserved;
};
using PSYSTEM_PHYSICAL_MEMORY_BANDWIDTH_INFORMATION = SYSTEM_PHYSICAL_MEMORY_BANDWIDTH_INFORMATION*;

// Class 247: SystemMemoryPowerStateInformation
struct SYSTEM_MEMORY_POWER_STATE_INFORMATION {
    ULONG    NumaNode;
    ULONG    PowerState;                // 0 = Active/High Performance, 1 = Self-Refresh, 2 = Deep Power Down
    ULONG64  ActivePowerMilliwatts;
    ULONG64  StandbyPowerMilliwatts;
    ULONG    ThermalThrottlingLimitDegreesC;
    ULONG    CurrentTemperatureDegreesC;
    ULONG    LowPowerTransitionsCount;
    ULONG    Reserved;
};
using PSYSTEM_MEMORY_POWER_STATE_INFORMATION = SYSTEM_MEMORY_POWER_STATE_INFORMATION*;

// Class 248: SystemCpuExecutionCharacteristicsInformation
struct SYSTEM_CPU_EXECUTION_CHARACTERISTICS_INFORMATION {
    ULONG    ProcessorIndex;
    ULONG    ExecutionEfficiencyClass;  // Efficiency class (0=Efficiency, 1=Performance)
    ULONG    InstructionIssueWidth;     // 6-wide, 8-wide decode/issue
    ULONG    MicrocodeRevision;
    ULONG64  HardwarePrefetcherFlags;   // Bitmask of enabled L1/L2/L3 streaming prefetchers
    ULONG    BranchPredictorTier;       // Branch prediction capability tier
    ULONG    SpeculativeExecutionFeatures; // CET, PCONFIG, TSX, IBRS, etc.
};
using PSYSTEM_CPU_EXECUTION_CHARACTERISTICS_INFORMATION = SYSTEM_CPU_EXECUTION_CHARACTERISTICS_INFORMATION*;

// Class 249: SystemPagefilePowerEfficiencyInformation
struct SYSTEM_PAGEFILE_POWER_EFFICIENCY_INFORMATION {
    ULONG    PagefileIndex;
    ULONG    Flags;                     // Active, CompressionEnabled, TieredStorage
    ULONG64  TotalAllocatedBytes;
    ULONG64  ActiveWrittenBytes;
    ULONG64  PowerSavedJoules;          // Energy conservation metric
    ULONG    CompressionRatioPercent;  // e.g. 250% (2.5x)
    ULONG    IoPowerState;              // Autonomous device power state
    ULONG    Reserved;
};
using PSYSTEM_PAGEFILE_POWER_EFFICIENCY_INFORMATION = SYSTEM_PAGEFILE_POWER_EFFICIENCY_INFORMATION*;

// Class 250: SystemCpuCooperativeAcceleratorInformation
struct SYSTEM_CPU_COOPERATIVE_ACCELERATOR_INFORMATION {
    ULONG    ProcessorIndex;
    ULONG    AcceleratorType;           // 1 = Matrix/AMX, 2 = NPU, 3 = Crypto, 4 = DSA/IAA
    ULONG    MaxTileDimensions;         // e.g. 2048
    ULONG    SupportedDataTypes;        // Bitmask (FP16, BF16, FP8, INT8, FP4, MXFP)
    ULONG64  OperationsPerSecondPeak;   // Peak TOPS/FLOPS
    ULONG    QueueAffinityMask;
    ULONG    CooperativeDispatchTier;
    ULONG    Reserved;
};
using PSYSTEM_CPU_COOPERATIVE_ACCELERATOR_INFORMATION = SYSTEM_CPU_COOPERATIVE_ACCELERATOR_INFORMATION*;

// Class 251: SystemPagefileCompressionTelemetryInformation
struct SYSTEM_PAGEFILE_COMPRESSION_TELEMETRY_INFORMATION {
    ULONG    PagefileIndex;
    ULONG    Flags;                             // 0x01 = Active, 0x02 = CompressionActive, 0x04 = HardwareAccelerated
    ULONG64  TotalPagesCompressed;              // e.g. 524288 pages
    ULONG64  TotalPagesDecompressed;            // e.g. 262144 pages
    ULONG64  UncompressedBytes;                 // e.g. 2147483648 bytes (2 GB)
    ULONG64  CompressedBytes;                   // e.g. 751619276 bytes (~716 MB)
    ULONG    CompressionRatioPercent;           // e.g. 285% (2.85x)
    ULONG    AverageCompressionLatencyNanos;    // e.g. 1250 ns
    ULONG    CompressionAlgorithm;              // 1 = Xpress, 2 = XpressHuff, 3 = Zstandard-NT, 4 = LZ4-NT
    ULONG    HardwareAcceleratorActive;         // 1
    ULONG64  HardwareAcceleratedPages;          // e.g. 400000 pages
    ULONG64  BypassedPagesDueToIncompressibility; // e.g. 12000 pages
};
using PSYSTEM_PAGEFILE_COMPRESSION_TELEMETRY_INFORMATION = SYSTEM_PAGEFILE_COMPRESSION_TELEMETRY_INFORMATION*;

// Class 252: SystemCpuCooperativeFabricInformation
struct SYSTEM_CPU_COOPERATIVE_FABRIC_INFORMATION {
    ULONG    TotalFabricNodes;                  // e.g. 16
    ULONG    ActiveFabricInterconnects;         // e.g. 32
    ULONG64  FabricBandwidthGBps;               // e.g. 2048 GB/s
    ULONG    CoherencyProtocolVersion;          // e.g. 0x0302
    ULONG    LatencyNanosecondsAverage;         // e.g. 45 ns
    ULONG    CacheCoherentAcceleratorCount;     // e.g. 8
    ULONG64  TotalFabricTransactions;           // e.g. 10000000000ULL
    ULONG64  InterNodeRetries;                  // e.g. 12
    ULONG    ThermalThrottleStatus;             // 0 = Nominal
    ULONG    FabricFlags;                       // 0x07 = DynamicTopology | SubByteQuantRouting | CoherentPipelining
};
using PSYSTEM_CPU_COOPERATIVE_FABRIC_INFORMATION = SYSTEM_CPU_COOPERATIVE_FABRIC_INFORMATION*;

// Class 253: SystemPagefileDecompressionTelemetryInformation
struct SYSTEM_PAGEFILE_DECOMPRESSION_TELEMETRY_INFORMATION {
    ULONG    PagefileIndex;
    ULONG    Flags;                                // 0x01 = Active, 0x02 = DecompressionActive, 0x04 = HardwareDecompressionActive
    ULONG64  TotalPagesDecompressed;               // e.g. 1048576 pages
    ULONG64  HardwareDecompressedPages;            // e.g. 850000 pages
    ULONG64  SoftwareFallbackPages;                // e.g. 198576 pages
    ULONG64  CompressedBytesRead;                  // e.g. 1503238552 bytes (~1.4 GB)
    ULONG64  DecompressedBytesProduced;            // e.g. 4294967296 bytes (4 GB)
    ULONG    AverageDecompressionLatencyNanos;     // e.g. 420 ns
    ULONG    DecompressionThroughputMBps;          // e.g. 12500 MB/s
    ULONG    DecompressionAlgorithm;               // 1 = Xpress, 2 = XpressHuff, 3 = Zstandard-NT, 4 = LZ4-NT
    ULONG64  IntegrityVerificationPasses;          // e.g. 1048576
    ULONG    DecompressedCacheHitRatioPercent;     // e.g. 94%
};
using PSYSTEM_PAGEFILE_DECOMPRESSION_TELEMETRY_INFORMATION = SYSTEM_PAGEFILE_DECOMPRESSION_TELEMETRY_INFORMATION*;

// Class 254: SystemCpuFabricPartitionInformation
struct SYSTEM_CPU_FABRIC_PARTITION_INFORMATION {
    ULONG    TotalFabricPartitions;                // e.g. 4
    ULONG    ActivePartitionId;                    // e.g. 0
    ULONG64  PartitionAffinityMask;                // e.g. 0x00000000FFFFFFFFULL
    ULONG    FabricBandwidthAllocationPercent;     // e.g. 75%
    ULONG    LowPriorityThrottleRate;              // e.g. 10%
    ULONG    FabricCongestionEvents;               // e.g. 5
    ULONG64  CrossPartitionInvalidations;          // e.g. 2500ULL
    ULONG    PartitionIsolationLevel;              // 1 = Shared, 2 = Guarded, 3 = Sovereign Isolated
    ULONG    PartitionFlags;                       // 0x0F = Coherent | QoSGuaranteed | SvmBound | EncryptedInterconnect
};
using PSYSTEM_CPU_FABRIC_PARTITION_INFORMATION = SYSTEM_CPU_FABRIC_PARTITION_INFORMATION*;

// Class 255: SystemPagefileClusterCompactionTelemetryInformation
struct SYSTEM_PAGEFILE_CLUSTER_COMPACTION_TELEMETRY_INFORMATION {
    ULONG    PagefileIndex;
    ULONG    Flags;                                // 0x01 = Active, 0x02 = CompactionActive, 0x04 = HardwareAssisted
    ULONG64  TotalCompactionPasses;                // e.g. 1024 passes
    ULONG64  FragmentedClustersReclaimed;          // e.g. 524288 clusters
    ULONG64  ContiguousClustersCreated;            // e.g. 131072 clusters
    ULONG64  PagefileDiskSpaceFreedBytes;          // e.g. 2147483648 bytes (2 GB)
    ULONG    AverageCompactionLatencyNanos;        // e.g. 850 ns
    ULONG    CompactionThroughputMBps;             // e.g. 15000 MB/s
    ULONG    FragmentationRatioBeforePercent;      // e.g. 68%
    ULONG    FragmentationRatioAfterPercent;       // e.g. 12%
    ULONG    CompactionAlgorithm;                  // 1 = Greedy, 2 = Generational, 3 = Sovereign-Compactor-NT
    ULONG    CompactionEfficiencyRating;           // e.g. 96%
};
using PSYSTEM_PAGEFILE_CLUSTER_COMPACTION_TELEMETRY_INFORMATION = SYSTEM_PAGEFILE_CLUSTER_COMPACTION_TELEMETRY_INFORMATION*;

// Class 256: SystemCpuFabricQosTelemetryInformation
struct SYSTEM_CPU_FABRIC_QOS_TELEMETRY_INFORMATION {
    ULONG    TotalQosQueues;                       // e.g. 16
    ULONG    ActiveQosDomainId;                    // e.g. 0
    ULONG64  HighPriorityBandwidthDeliveredGBps;   // e.g. 1536 GB/s
    ULONG64  BestEffortBandwidthDeliveredGBps;     // e.g. 512 GB/s
    ULONG    HighPriorityP99LatencyNanos;          // e.g. 18 ns
    ULONG    BestEffortP99LatencyNanos;            // e.g. 64 ns
    ULONG    QosSlaViolations;                     // e.g. 0
    ULONG    CongestionMitigationEvents;           // e.g. 3
    ULONG    CreditStarvationEvents;               // e.g. 0
    ULONG    ThermalBudgetUsagePercent;            // e.g. 72%
    ULONG    QosFlags;                             // 0x0F = PriorityQueuing | DynamicCreditReallocation | HardwareRateLimiting | SlaGuaranteed
};
using PSYSTEM_CPU_FABRIC_QOS_TELEMETRY_INFORMATION = SYSTEM_CPU_FABRIC_QOS_TELEMETRY_INFORMATION*;

// Class 257: SystemPagefileClusterDefragmentationTelemetryInformation
struct SYSTEM_PAGEFILE_CLUSTER_DEFRAGMENTATION_TELEMETRY_INFORMATION {
    ULONG    PagefileIndex;
    ULONG    Flags;                                // 0x01 = Active, 0x02 = DefragRunning, 0x04 = ZeroCopyRelocation, 0x08 = HardwareDmaAccelerated
    ULONG64  TotalDefragPasses;                    // e.g. 512 passes
    ULONG64  ClustersRelocated;                    // e.g. 1048576 clusters
    ULONG64  ContinuousFreeExtentsFormed;          // e.g. 65536 extents
    ULONG64  PagefileSparsityFreedBytes;           // e.g. 4294967296 bytes (4 GB)
    ULONG    AverageRelocationLatencyNanos;        // e.g. 420 ns
    ULONG    DefragThroughputMBps;                 // e.g. 24000 MB/s
    ULONG    PreDefragFragmentationIndex;          // e.g. 74%
    ULONG    PostDefragFragmentationIndex;         // e.g. 5%
    ULONG    DefragEngineVersion;                  // e.g. 1 (MicaNT-NativeDefrag-v1)
    ULONG    OptimizationScore;                    // e.g. 98%
};
using PSYSTEM_PAGEFILE_CLUSTER_DEFRAGMENTATION_TELEMETRY_INFORMATION = SYSTEM_PAGEFILE_CLUSTER_DEFRAGMENTATION_TELEMETRY_INFORMATION*;

// Class 258: SystemCpuFabricLatencyTelemetryInformation
struct SYSTEM_CPU_FABRIC_LATENCY_TELEMETRY_INFORMATION {
    ULONG    FabricNodeId;                         // e.g. 0
    ULONG    InterconnectHopCount;                 // e.g. 2
    ULONG64  TotalTransactionsSampled;             // e.g. 1000000000ULL
    ULONG    MinHopLatencyNanos;                   // e.g. 8 ns
    ULONG    AverageHopLatencyNanos;               // e.g. 14 ns
    ULONG    MaxHopLatencyNanos;                   // e.g. 45 ns
    ULONG    P99HopLatencyNanos;                   // e.g. 21 ns
    ULONG    LinkSaturationPercent;                // e.g. 48%
    ULONG    RetransmitRatePPM;                    // e.g. 2 ppm
    ULONG    CrosstalkMitigationEvents;            // e.g. 0
    ULONG    FabricStatusFlags;                    // 0x07 = Operational | LowJitter | DynamicRoutingActive
};
using PSYSTEM_CPU_FABRIC_LATENCY_TELEMETRY_INFORMATION = SYSTEM_CPU_FABRIC_LATENCY_TELEMETRY_INFORMATION*;

// Class 259: SystemPagefileExtentMigrationTelemetryInformation
struct SYSTEM_PAGEFILE_EXTENT_MIGRATION_TELEMETRY_INFORMATION {
    ULONG    PagefileId;                           // e.g. 0
    ULONG64  TotalExtentsMigrated;                 // e.g. 65536
    ULONG64  TotalBytesMigrated;                   // e.g. 17179869184ULL (16 GB)
    ULONG    HotToWarmMigrations;                  // e.g. 40960
    ULONG    WarmToColdMigrations;                 // e.g. 24576
    ULONG    ColdToHotPromotions;                  // e.g. 8192
    ULONG    AverageMigrationLatencyMicros;        // e.g. 125 us
    ULONG    MigrationThroughputMBps;              // e.g. 32000 MB/s
    ULONG    CompressionSavingsPercent;            // e.g. 45%
    ULONG    ExtentIntegrityFailures;              // e.g. 0
    ULONG    StatusFlags;                          // 0x07 = MigrationActive | ZeroCopySupported | DynamicTieringEnabled
};
using PSYSTEM_PAGEFILE_EXTENT_MIGRATION_TELEMETRY_INFORMATION = SYSTEM_PAGEFILE_EXTENT_MIGRATION_TELEMETRY_INFORMATION*;

// Class 260: SystemCpuFabricCongestionTelemetryInformation
struct SYSTEM_CPU_FABRIC_CONGESTION_TELEMETRY_INFORMATION {
    ULONG    FabricId;                             // e.g. 0
    ULONG    CongestionDomainCount;                // e.g. 8
    ULONG64  TotalCreditsIssued;                   // e.g. 5000000000ULL
    ULONG64  TotalCreditsStalled;                  // e.g. 12000ULL
    ULONG    HeadOfLineBlockingEvents;             // e.g. 15
    ULONG    ThrottlingEpisodes;                   // e.g. 4
    ULONG    BufferOverflowDropCount;              // e.g. 0
    ULONG    AverageBufferOccupancyPercent;        // e.g. 35%
    ULONG    PeakBufferOccupancyPercent;           // e.g. 68%
    ULONG    AdaptiveRoutingDiversions;            // e.g. 250
    ULONG    FabricCongestionStatus;               // 0x05 = CongestionManagementActive | FlowControlEnabled
};
using PSYSTEM_CPU_FABRIC_CONGESTION_TELEMETRY_INFORMATION = SYSTEM_CPU_FABRIC_CONGESTION_TELEMETRY_INFORMATION*;

// Class 261: SystemPagefileTierExpulsionTelemetryInformation
struct SYSTEM_PAGEFILE_TIER_EXPULSION_TELEMETRY_INFORMATION {
    ULONG    PagefileId;                           // e.g. 0
    ULONG64  TotalExpulsions;                      // e.g. 16384ULL
    ULONG64  BytesExpelled;                        // e.g. 4294967296ULL (4 GB)
    ULONG64  ColdToPurgeExpulsions;                // e.g. 8192ULL
    ULONG64  AverageExpulsionLatencyMicros;        // e.g. 120ULL
    ULONG64  ExpulsionThroughputMBps;              // e.g. 3200ULL
    ULONG    StatusFlags;                          // 0x07 = ExpulsionActive | AutoTuningEnabled | PurgeCoalescingActive
};
using PSYSTEM_PAGEFILE_TIER_EXPULSION_TELEMETRY_INFORMATION = SYSTEM_PAGEFILE_TIER_EXPULSION_TELEMETRY_INFORMATION*;

// Class 262: SystemCpuFabricBandwidthPartitionTelemetryInformation
struct SYSTEM_CPU_FABRIC_BANDWIDTH_PARTITION_TELEMETRY_INFORMATION {
    ULONG    FabricId;                             // e.g. 0
    ULONG    PartitionCount;                       // e.g. 8
    ULONG64  GuaranteedBandwidthMBps;              // e.g. 128000ULL
    ULONG64  BurstBandwidthMBps;                   // e.g. 256000ULL
    ULONG64  PartitionThrottlingCount;             // e.g. 18ULL
    ULONG    StatusFlags;                          // 0x05 = PartitioningEnforced | QosGuaranteed
};
using PSYSTEM_CPU_FABRIC_BANDWIDTH_PARTITION_TELEMETRY_INFORMATION = SYSTEM_CPU_FABRIC_BANDWIDTH_PARTITION_TELEMETRY_INFORMATION*;

// Class 263: SystemPagefileTierIoQueueTelemetryInformation
struct SYSTEM_PAGEFILE_TIER_IO_QUEUE_TELEMETRY_INFORMATION {
    ULONG    TierId;                               // e.g. 0 (Optane / NVMe fast tier)
    ULONG    QueueDepth;                           // e.g. 12
    ULONG    HighWatermarkQueueDepth;              // e.g. 64
    ULONG    CompletedIoCount;                     // e.g. 1048576
    ULONG    PendingIoCount;                       // e.g. 4
    ULONG    ThrottledIoCount;                     // e.g. 0
    ULONG    ReorderedIoCount;                     // e.g. 128
    ULONG    AverageServiceTimeNs;                 // e.g. 450 ns
    ULONG64  TotalBytesServiced;                   // e.g. 42949672960ULL (40 GB)
    ULONG64  TotalStallDurationNs;                 // e.g. 1200 ns
    ULONG    StatusFlags;                          // 0x07 = QueueActive | AsyncPipeliningEnabled | AutoPriorityCoalescing
};
using PSYSTEM_PAGEFILE_TIER_IO_QUEUE_TELEMETRY_INFORMATION = SYSTEM_PAGEFILE_TIER_IO_QUEUE_TELEMETRY_INFORMATION*;

// Class 264: SystemCpuFabricPowerStateTelemetryInformation
struct SYSTEM_CPU_FABRIC_POWER_STATE_TELEMETRY_INFORMATION {
    ULONG    FabricId;                             // e.g. 0
    ULONG    CurrentPowerState;                    // e.g. 1 (P1 high performance state)
    ULONG    TargetPowerState;                     // e.g. 1
    ULONG    TransitionCount;                      // e.g. 420
    ULONG    TotalActiveDurationUs;                // e.g. 850000 us
    ULONG    TotalIdleDurationUs;                  // e.g. 150000 us
    ULONG    VoltageMv;                            // e.g. 950 mV
    ULONG    FrequencyMhz;                         // e.g. 4200 MHz
    ULONG64  EnergyJoulesMicro;                    // e.g. 55000000ULL (55 Joules)
    ULONG64  ThrottleDurationNs;                   // e.g. 0 ns
    ULONG    StatusFlags;                          // 0x07 = AutonomousPowerManagement | RapidTransitionEnacted | VoltageGatingActive
};
using PSYSTEM_CPU_FABRIC_POWER_STATE_TELEMETRY_INFORMATION = SYSTEM_CPU_FABRIC_POWER_STATE_TELEMETRY_INFORMATION*;

// Class 265: SystemPagefileTierWearLevelingTelemetryInformation
struct SYSTEM_PAGEFILE_TIER_WEAR_LEVELING_TELEMETRY_INFORMATION {
    ULONG    TierId;                               // e.g. 0 (Optane / NVMe fast tier)
    ULONG    TotalBlocks;                          // e.g. 1048576
    ULONG    FreeBlocks;                           // e.g. 524288
    ULONG    EraseCountMin;                        // e.g. 12
    ULONG    EraseCountMax;                        // e.g. 48
    ULONG    EraseCountAverage;                    // e.g. 26
    ULONG    BadBlockCount;                        // e.g. 2
    ULONG    SpareBlocksRemaining;                 // e.g. 4096
    ULONG64  LifetimeBytesWritten;                 // e.g. 1099511627776ULL (1 TB)
    ULONG64  ProjectedRemainingLifespanHours;      // e.g. 87600ULL (10 years)
    ULONG    StatusFlags;                          // 0x07 = WearLevelingActive | DynamicBlockRemapping | EnduranceBudgetOptimal
};
using PSYSTEM_PAGEFILE_TIER_WEAR_LEVELING_TELEMETRY_INFORMATION = SYSTEM_PAGEFILE_TIER_WEAR_LEVELING_TELEMETRY_INFORMATION*;

// Class 266: SystemCpuFabricVoltageDomainTelemetryInformation
struct SYSTEM_CPU_FABRIC_VOLTAGE_DOMAIN_TELEMETRY_INFORMATION {
    ULONG    DomainId;                             // e.g. 0
    ULONG    RailVoltageMv;                        // e.g. 1050 mV
    ULONG    MinimumVoltageMv;                     // e.g. 750 mV
    ULONG    MaximumVoltageMv;                     // e.g. 1350 mV
    ULONG    TargetVoltageMv;                      // e.g. 1050 mV
    ULONG    SlewRateMvPerUs;                      // e.g. 25 mV/us
    ULONG    DroopCompensations;                   // e.g. 128
    ULONG    VoltageRippleUv;                      // e.g. 4500 uV
    ULONG64  VoltageSampleCount;                   // e.g. 1000000ULL
    ULONG64  OutOfToleranceDurationNs;             // e.g. 0 ns
    ULONG    StatusFlags;                          // 0x07 = DomainRegulated | TelemetryOnline | FastTransientResponseActive
};
using PSYSTEM_CPU_FABRIC_VOLTAGE_DOMAIN_TELEMETRY_INFORMATION = SYSTEM_CPU_FABRIC_VOLTAGE_DOMAIN_TELEMETRY_INFORMATION*;

// Class 267: SystemPagefileTierFailoverTelemetryInformation
struct SYSTEM_PAGEFILE_TIER_FAILOVER_TELEMETRY_INFORMATION {
    ULONG    PrimaryTierId;                        // e.g. 0
    ULONG    SecondaryTierId;                      // e.g. 1
    ULONG    FailoverEventsTotal;                  // e.g. 3
    ULONG    FailoverRetries;                      // e.g. 0
    ULONG64  PagesMigratedOnFailover;              // e.g. 65536ULL
    ULONG64  FailoverLatencyMicroseconds;          // e.g. 420ULL
    ULONG    SecondaryTierAvailable;               // e.g. 1 (TRUE)
    ULONG    HighPriorityFailoverPending;          // e.g. 0
    ULONG64  BytesRecoveredFromFaultedTier;        // e.g. 268435456ULL (256 MB)
    ULONG    FailoverTriggerReason;                // e.g. 0x01 (I/O Timeout / Latency Threshold Exceeded)
    ULONG    StatusFlags;                          // 0x07 = FailoverEngineActive | RedundantTierReady | ZeroDataLossGuarantee
};
using PSYSTEM_PAGEFILE_TIER_FAILOVER_TELEMETRY_INFORMATION = SYSTEM_PAGEFILE_TIER_FAILOVER_TELEMETRY_INFORMATION*;

// Class 268: SystemCpuFabricClockStretchingTelemetryInformation
struct SYSTEM_CPU_FABRIC_CLOCK_STRETCHING_TELEMETRY_INFORMATION {
    ULONG    DomainId;                             // e.g. 0
    ULONG    ActiveClockStretchingDomains;         // e.g. 4
    ULONG    TotalStretchingEvents;                // e.g. 256
    ULONG    VoltageDroopCompensatedCount;         // e.g. 240
    ULONG    ThermalThrottleStretchCount;          // e.g. 16
    ULONG    MaxStretchPercentage;                 // e.g. 12 (%)
    ULONG64  CumulativeStretchedDurationPicoSeconds; // e.g. 1500000000ULL (1.5 ms)
    ULONG64  AverageStretchDurationPicoSeconds;    // e.g. 5859375ULL
    ULONG64  ClockCyclesPreserved;                 // e.g. 12000000ULL
    ULONG    StretchHysteresisState;               // e.g. 0 (Normal)
    ULONG    StatusFlags;                          // 0x07 = ClockStretchingEnabled | AutonomousDroopMitigation | PLLSynchronized
};
using PSYSTEM_CPU_FABRIC_CLOCK_STRETCHING_TELEMETRY_INFORMATION = SYSTEM_CPU_FABRIC_CLOCK_STRETCHING_TELEMETRY_INFORMATION*;

// Class 269: SystemPagefileTierMigrationQosInformation
struct SYSTEM_PAGEFILE_TIER_MIGRATION_QOS_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // e.g. 0
    ULONG64  HighPriorityMigrationsTotal;          // e.g. 1024ULL
    ULONG64  StandardPriorityMigrationsTotal;      // e.g. 16384ULL
    ULONG64  BackgroundPriorityMigrationsTotal;    // e.g. 65536ULL
    ULONG64  ThrottledMigrationsTotal;             // e.g. 12ULL
    ULONG64  PreemptedMigrationsTotal;             // e.g. 4ULL
    ULONG64  AverageMigrationLatencyNs;            // e.g. 12500ULL (12.5 µs)
    ULONG64  PeakMigrationBandwidthBytesPerSec;    // e.g. 6442450944ULL (~6 GB/s)
    ULONG    ActiveMigrationQosQueues;             // e.g. 4
    ULONG    TierMigrationFlags;                   // e.g. 0x07 (QoSEnforced | PreemptionEnabled | ZeroCopyActive)
};
using PSYSTEM_PAGEFILE_TIER_MIGRATION_QOS_INFORMATION = SYSTEM_PAGEFILE_TIER_MIGRATION_QOS_INFORMATION*;

// Class 270: SystemCpuFabricThermalThrottleTelemetryInformation
struct SYSTEM_CPU_FABRIC_THERMAL_THROTTLE_TELEMETRY_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // e.g. 0
    ULONG64  TotalThermalThrottleEvents;           // e.g. 42ULL
    ULONG64  ActiveThermalThrottleDurationUs;      // e.g. 150000ULL (150 ms)
    ULONG64  JunctionTemperatureMillicelsius;      // e.g. 65000ULL (65.0 C)
    ULONG64  ThrottleTemperatureThresholdMillicelsius; // e.g. 95000ULL (95.0 C)
    ULONG64  CoreFabricFrequencyReductionMhz;      // e.g. 400ULL
    ULONG64  TjMaxMarginMillicelsius;              // e.g. 30000ULL (30.0 C margin)
    ULONG    ActiveThrottledNodesMask;             // e.g. 0x00000001
    ULONG    ThermalGovernorFlags;                 // e.g. 0x07 (DynamicThermalScaling | TjMaxSupervised | HysteresisActive)
};
using PSYSTEM_CPU_FABRIC_THERMAL_THROTTLE_TELEMETRY_INFORMATION = SYSTEM_CPU_FABRIC_THERMAL_THROTTLE_TELEMETRY_INFORMATION*;

// Class 271: SystemPagefileTierDynamicDrainInformation
struct SYSTEM_PAGEFILE_TIER_DYNAMIC_DRAIN_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // e.g. 0
    ULONG64  DrainedPagesTotal;                    // e.g. 32768ULL
    ULONG64  DrainThroughputBytesPerSec;           // e.g. 4294967296ULL (~4 GB/s)
    ULONG    TargetTierIndex;                      // e.g. 2
    ULONG    SourceTierIndex;                      // e.g. 0
    ULONG    ActiveDrainWorkers;                   // e.g. 8
    ULONG    PendingDrainQueueDepth;               // e.g. 128
    ULONG    EmergencyDrainTriggerCount;           // e.g. 3
    ULONG    DrainCompletionLatencyUs;             // e.g. 450 (450 µs)
    ULONG64  ReclaimedPhysicalMemoryBytes;         // e.g. 134217728ULL (128 MB)
    ULONG    DynamicDrainFlags;                    // e.g. 0x07 (DrainActive | NonBlocking | LowImpact)
    ULONG    Pad;                                  // 0
};
using PSYSTEM_PAGEFILE_TIER_DYNAMIC_DRAIN_INFORMATION = SYSTEM_PAGEFILE_TIER_DYNAMIC_DRAIN_INFORMATION*;

// Class 272: SystemCpuFabricVoltageDroopPredictiveInformation
struct SYSTEM_CPU_FABRIC_VOLTAGE_DROOP_PREDICTIVE_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // e.g. 0
    ULONG64  PredictiveDroopEventsTotal;           // e.g. 180ULL
    ULONG64  MitigatedDroopEventsTotal;            // e.g. 176ULL
    ULONG    PredictedDroopMagnitudeMicrovolts;    // e.g. 45000 (45 mV)
    ULONG    TransientCurrentSlewRateMAperNs;      // e.g. 1250 (1.25 A/ns)
    ULONG    CompensationCapacitanceBoostPf;       // e.g. 3200 (3.2 nF)
    ULONG    PhaseAdvanceLatencyPicoseconds;       // e.g. 250 (250 ps)
    ULONG    VoltageStabilityMarginMillivolts;     // e.g. 85 (85 mV)
    ULONG    FabricPowerSupplyRailIndex;           // e.g. 1
    ULONG    DroopMitigationActionFlags;           // e.g. 0x0F (PredictiveClockStretch | CapBoost | ActivePhaseInject | VddRamp)
    ULONG    VoltageDomainStatus;                  // e.g. 0x01 (NominalRegulated)
};
using PSYSTEM_CPU_FABRIC_VOLTAGE_DROOP_PREDICTIVE_INFORMATION = SYSTEM_CPU_FABRIC_VOLTAGE_DROOP_PREDICTIVE_INFORMATION*;

// Class 273: SystemPagefileTierPressurePredictiveInformation
struct SYSTEM_PAGEFILE_TIER_PRESSURE_PREDICTIVE_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // e.g. 0
    ULONG64  PredictedExhaustionTimestampNs;       // e.g. 1000000000ULL
    ULONG64  TierPressureRateBytesPerSec;          // e.g. 104857600ULL (100 MB/s)
    ULONG    TierIndex;                            // e.g. 1
    ULONG    CurrentPressurePercentage;            // e.g. 68 (68%)
    ULONG    PredictedPressureThresholdPercentage; // e.g. 85 (85%)
    ULONG    TimeToThresholdSeconds;               // e.g. 120 (120 s)
    ULONG    ActiveEvictionWorkers;                // e.g. 4
    ULONG    PendingAllocationBacklogPages;        // e.g. 512
    ULONG64  RecommendedPreemptiveAllocBytes;      // e.g. 67108864ULL (64 MB)
    ULONG    PressureMitigationFlags;              // e.g. 0x07 (PreemptiveExpand | ThrottleBulkIO | PromoteHotPages)
    ULONG    Pad;                                  // 0
};
using PSYSTEM_PAGEFILE_TIER_PRESSURE_PREDICTIVE_INFORMATION = SYSTEM_PAGEFILE_TIER_PRESSURE_PREDICTIVE_INFORMATION*;

// Class 274: SystemCpuFabricVoltageResonanceTelemetryInformation
struct SYSTEM_CPU_FABRIC_VOLTAGE_RESONANCE_TELEMETRY_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // e.g. 0
    ULONG64  ResonanceEventsTotal;                 // e.g. 96ULL
    ULONG64  SuppressionCyclesTotal;               // e.g. 48000ULL
    ULONG    ResonancePeakFrequencyKhz;            // e.g. 12500 (12.5 MHz)
    ULONG    ResonanceHarmonicCount;               // e.g. 3
    ULONG    PeakVoltageOscillationMicrovolts;     // e.g. 38000 (38 mV)
    ULONG    ActiveDampingResistanceMilliOhms;     // e.g. 45 (45 mΩ)
    ULONG    HarmonicSuppressionAttenuationDb;     // e.g. 24 (24 dB)
    ULONG    FabricClockPhaseModulationDegrees;    // e.g. 15 (15 deg)
    ULONG    ResonanceSuppressionFlags;            // e.g. 0x0F (PhaseSpreadSpectrum | ActiveDamping | HarmonicNotch | ClockDither)
    ULONG    VoltageDomainStatus;                  // e.g. 0x01 (NominalRegulated)
};
using PSYSTEM_CPU_FABRIC_VOLTAGE_RESONANCE_TELEMETRY_INFORMATION = SYSTEM_CPU_FABRIC_VOLTAGE_RESONANCE_TELEMETRY_INFORMATION*;

// Class 275: SystemPagefileTierThrottlingTelemetryInformation
struct SYSTEM_PAGEFILE_TIER_THROTTLING_TELEMETRY_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    TierIndex;                            // e.g. 1
    ULONG    ThrottleLevelPercent;                 // e.g. 45 (45%)
    ULONG    QueueBacklogDepth;                    // e.g. 128
    ULONG    ThermalBudgetPercent;                 // e.g. 82 (82%)
    ULONG64  ActiveThrottledCycles;                // e.g. 2500000ULL
    ULONG64  EnforcedLatencyDeltaNs;               // e.g. 15000ULL (15 us)
    ULONG64  ThrottledWriteBytes;                  // e.g. 52428800ULL (50 MB)
    ULONG64  RecoveredCycles;                      // e.g. 1800000ULL
    ULONG    ThrottlingMitigationFlags;            // e.g. 0x05 (DynamicCreditWindow | IOWeightDamping)
    ULONG    Pad;                                  // 0
};
using PSYSTEM_PAGEFILE_TIER_THROTTLING_TELEMETRY_INFORMATION = SYSTEM_PAGEFILE_TIER_THROTTLING_TELEMETRY_INFORMATION*;

// Class 276: SystemCpuFabricVoltagePhaseSpreadSpectrumInformation
struct SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_SPREAD_SPECTRUM_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    SocketId;                             // e.g. 0
    ULONG    PhaseCount;                           // e.g. 8
    ULONG    ModulationFrequencyKhz;               // e.g. 330 (330 kHz spread spectrum modulation)
    ULONG    SpreadSpectrumPercentagePpm;          // e.g. 5000 (0.5% / 5000 ppm)
    ULONG64  PhaseCurrentMicroAmps[8];             // e.g. 18500000ULL per phase (18.5 A)
    ULONG64  VoltageJitterRmsMicroVolts;           // e.g. 4200ULL (4.2 mV RMS jitter)
    ULONG64  ActiveTransientSuppressionCycles;     // e.g. 960000ULL
    ULONG    PhaseInterleavingFlags;               // e.g. 0x03 (QuadratureInterleave | SpreadSpectrumActive)
    ULONG    Pad;                                  // 0
};
using PSYSTEM_CPU_FABRIC_VOLTAGE_PHASE_SPREAD_SPECTRUM_INFORMATION = SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_SPREAD_SPECTRUM_INFORMATION*;

// Class 277: SystemPagefileTierCompactionPressureInformation
struct SYSTEM_PAGEFILE_TIER_COMPACTION_PRESSURE_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    TierIndex;                            // e.g. 1
    ULONG    CompactionPressurePercent;            // e.g. 68 (68% compaction pressure)
    ULONG    FragmentationLevelPercent;            // e.g. 35 (35% fragmentation)
    ULONG    CompressionRatioBasisPoints;          // e.g. 2400 (2.40x compression ratio)
    ULONG64  CompactedPageCount;                   // e.g. 1048576ULL (1M pages)
    ULONG64  ReclaimedMemoryBytes;                 // e.g. 4294967296ULL (4 GB)
    ULONG64  CompactionDurationCycles;             // e.g. 3500000ULL
    ULONG64  DecompressionDurationCycles;           // e.g. 1200000ULL
    ULONG    CompactionEngineFlags;                // e.g. 0x07 (HardwareAccelerated | ZeroPageElimination | DeduplicationActive)
    ULONG    Pad;                                  // 0
};
using PSYSTEM_PAGEFILE_TIER_COMPACTION_PRESSURE_INFORMATION = SYSTEM_PAGEFILE_TIER_COMPACTION_PRESSURE_INFORMATION*;

// Class 278: SystemCpuFabricVoltageSlewRateTelemetryInformation
struct SYSTEM_CPU_FABRIC_VOLTAGE_SLEW_RATE_TELEMETRY_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    SocketId;                             // e.g. 0
    ULONG    SlewRateVPerMicroSec;                 // e.g. 250 (250 V/us)
    ULONG    MaxAllowableSlewRateVPerMicroSec;      // e.g. 500 (500 V/us)
    ULONG    OverShootMicroVolts;                  // e.g. 15000 (15 mV overshoot)
    ULONG64  TransientStepDurationNs;              // e.g. 40ULL (40 ns)
    ULONG64  DrcCompensatedCycles;                 // e.g. 1850000ULL
    ULONG64  SlewRateViolationCount;               // e.g. 0ULL
    ULONG64  ActivePowerRailMicroAmps;              // e.g. 32000000ULL (32 A)
    ULONG    SlewRateControlFlags;                 // e.g. 0x03 (AdaptiveSlewDamping | FastTransientCapacitanceActive)
    ULONG    Pad;                                  // 0
};
using PSYSTEM_CPU_FABRIC_VOLTAGE_SLEW_RATE_TELEMETRY_INFORMATION = SYSTEM_CPU_FABRIC_VOLTAGE_SLEW_RATE_TELEMETRY_INFORMATION*;

// Class 279: SystemPagefileTierExpansionRateTelemetryInformation
struct SYSTEM_PAGEFILE_TIER_EXPANSION_RATE_TELEMETRY_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    TierIndex;                            // e.g. 1
    ULONG    ExpansionAllocationsPerSecond;        // e.g. 1250 (1,250 allocs/sec)
    ULONG64  BytesAllocatedPerSecond;              // e.g. 1073741824ULL (1 GB/s)
    ULONG    SustainedGrowthPressurePercent;       // e.g. 74 (74% growth pressure)
    ULONG    PeakBurstRateMBPerSec;                // e.g. 4096 (4 GB/s)
    ULONG    DynamicAllocationHeadroomPages;       // e.g. 262144 (1 GB headroom)
    ULONG    ThrottleSuppressionFactor;            // e.g. 12
    ULONG    ExpansionLatencyMicroseconds;         // e.g. 85 (85 us)
    ULONG    ExpansionEngineFlags;                 // e.g. 0x07 (BurstOptimized | PredictiveExpansionActive | ZSwapPrewarmEnabled)
    ULONG    Pad;                                  // 0
};
using PSYSTEM_PAGEFILE_TIER_EXPANSION_RATE_TELEMETRY_INFORMATION = SYSTEM_PAGEFILE_TIER_EXPANSION_RATE_TELEMETRY_INFORMATION*;

// Class 280: SystemCpuFabricVoltagePhaseMarginTelemetryInformation
struct SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_MARGIN_TELEMETRY_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    SocketId;                             // e.g. 0
    ULONG    PhaseIndex;                           // e.g. 2
    ULONG    VoltagePhaseMarginMillivolts;         // e.g. 65 (65 mV margin)
    ULONG    PhaseJitterPicoseconds;               // e.g. 18 (18 ps jitter)
    ULONG    PhaseBalanceScorePercent;             // e.g. 96 (96% balance score)
    ULONG    TransientRecoveryMarginMicrovolts;    // e.g. 45000 (45 mV recovery margin)
    ULONG64  PhaseDerateOperatingCycles;           // e.g. 4200000ULL
    ULONG64  PhaseMarginViolationCount;            // e.g. 0ULL
    ULONG    ThermalPhaseDeratePercent;            // e.g. 4 (4% derate)
    ULONG    PhaseMarginControlFlags;              // e.g. 0x05 (DynamicPhaseBalancing | FastMarginTelemetryActive)
};
using PSYSTEM_CPU_FABRIC_VOLTAGE_PHASE_MARGIN_TELEMETRY_INFORMATION = SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_MARGIN_TELEMETRY_INFORMATION*;

// Class 281: SystemPagefileTierCompactDedupEfficiencyInformation
struct SYSTEM_PAGEFILE_TIER_COMPACT_DEDUP_EFFICIENCY_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    TierIndex;                            // e.g. 1
    ULONG    DedupPassCount;                       // e.g. 84
    ULONG64  TotalDeduplicatedBytes;               // e.g. 8589934592ULL (8 GB)
    ULONG64  TotalCompactedPages;                  // e.g. 2097152ULL
    ULONG    EfficiencyRatioBasisPoints;           // e.g. 8450 (84.50%)
    ULONG    DedupHashCollisionsAvoided;           // e.g. 1420
    BOOLEAN  BackgroundDedupActive;                // 1
    BOOLEAN  TierCompactionThrottled;              // 0
    uint8_t  StatusFlags;                          // e.g. 0x07 (DedupActive | HardwareHashAccEn | LosslessCompression)
    uint8_t  Pad;                                  // 0
    ULONG    Reserved2;                            // 0
};
using PSYSTEM_PAGEFILE_TIER_COMPACT_DEDUP_EFFICIENCY_INFORMATION = SYSTEM_PAGEFILE_TIER_COMPACT_DEDUP_EFFICIENCY_INFORMATION*;

// Class 282: SystemCpuFabricVoltageTransitionalDroopIndexInformation
struct SYSTEM_CPU_FABRIC_VOLTAGE_TRANSITIONAL_DROOP_INDEX_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    FabricDomainId;                       // e.g. 0
    ULONG    DroopEventsLogged;                    // e.g. 38
    ULONG    MaxTransitionalDroopMillivolts;       // e.g. 28 (28 mV droop)
    ULONG    AverageDroopRecoveryNanoseconds;      // e.g. 34 (34 ns recovery)
    ULONG    DroopMitigationActivations;           // e.g. 195
    BOOLEAN  AdaptiveLoadlineActive;               // 1
    BOOLEAN  DroopSafetyMarginBreached;            // 0
    uint8_t  DroopIndexStatusFlags;                // e.g. 0x05 (DynamicDroopIndexReady | FastTransientRecovery)
    uint8_t  Pad;                                  // 0
    ULONG    Reserved2;                            // 0
};
using PSYSTEM_CPU_FABRIC_VOLTAGE_TRANSITIONAL_DROOP_INDEX_INFORMATION = SYSTEM_CPU_FABRIC_VOLTAGE_TRANSITIONAL_DROOP_INDEX_INFORMATION*;

// Class 283: SystemPagefileTierEvictionPolicyTelemetryInformation
struct SYSTEM_PAGEFILE_TIER_EVICTION_POLICY_TELEMETRY_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    TierIndex;                            // e.g. 1
    ULONG    EvictionPassCount;                    // e.g. 96
    ULONG64  TotalEvictedBytes;                    // e.g. 17179869184ULL (16 GB)
    ULONG64  TotalEvictedPages;                    // e.g. 4194304ULL
    ULONG    EvictionLatencyNanoseconds;           // e.g. 125
    ULONG    EvictionPressureScore;                // e.g. 7420 (74.20%)
    BOOLEAN  PreemptiveEvictionActive;             // 1
    BOOLEAN  EvictionThrottlingEngaged;            // 0
    uint8_t  PolicyFlags;                          // e.g. 0x07 (LRUOptimized | TierAware | WritebackAccelerated)
    uint8_t  Pad;                                  // 0
    ULONG    Reserved2;                            // 0
};
using PSYSTEM_PAGEFILE_TIER_EVICTION_POLICY_TELEMETRY_INFORMATION = SYSTEM_PAGEFILE_TIER_EVICTION_POLICY_TELEMETRY_INFORMATION*;

// Class 284: SystemCpuFabricVoltageDloadTransientMarginInformation
struct SYSTEM_CPU_FABRIC_VOLTAGE_DLOAD_TRANSIENT_MARGIN_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    FabricDomainId;                       // e.g. 0
    ULONG    DloadTransientEvents;                 // e.g. 52
    ULONG    TransientMarginMicrovolts;             // e.g. 35000 (35 mV margin)
    ULONG    DloadSlewRateMvPerMicrosec;           // e.g. 850
    ULONG    AdaptiveDloadMitigations;             // e.g. 210
    BOOLEAN  DloadRegulationActive;                // 1
    BOOLEAN  TransientMarginWarning;               // 0
    uint8_t  DloadMarginStatusFlags;               // e.g. 0x06 (FastDloadControl | MarginTrackingEn)
    uint8_t  Pad;                                  // 0
    ULONG    Reserved2;                            // 0
};
using PSYSTEM_CPU_FABRIC_VOLTAGE_DLOAD_TRANSIENT_MARGIN_INFORMATION = SYSTEM_CPU_FABRIC_VOLTAGE_DLOAD_TRANSIENT_MARGIN_INFORMATION*;

// Class 285: SystemPagefileTierReclaimEfficiencyInformation
struct SYSTEM_PAGEFILE_TIER_RECLAIM_EFFICIENCY_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    TierIndex;                            // e.g. 1
    ULONG    ReclaimCycleCount;                    // e.g. 128
    ULONG64  TotalPagesScanned;                    // e.g. 8388608ULL (8M pages)
    ULONG64  TotalPagesReclaimed;                  // e.g. 6291456ULL (6M pages)
    ULONG    ReclaimLatencyNanoseconds;            // e.g. 110
    ULONG    EfficiencyRatioPermille;              // e.g. 750 (75.0% reclaim efficiency)
    BOOLEAN  AdaptiveReclaimActive;                // 1
    BOOLEAN  ReclaimStarvationWarning;             // 0
    uint8_t  StatusFlags;                          // e.g. 0x05 (DynamicEvictionCompaction | ZeroCopyReclaim)
    uint8_t  Pad;                                  // 0
    ULONG    Reserved2;                            // 0
};
using PSYSTEM_PAGEFILE_TIER_RECLAIM_EFFICIENCY_INFORMATION = SYSTEM_PAGEFILE_TIER_RECLAIM_EFFICIENCY_INFORMATION*;

// Class 286: SystemCpuCoreVoltageGuardBandHeadroomInformation
struct SYSTEM_CPU_CORE_VOLTAGE_GUARD_BAND_HEADROOM_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    CoreIndex;                            // e.g. 0
    ULONG    GuardBandHeadroomMicrovolts;          // e.g. 42000 (42 mV guard band headroom)
    ULONG    NominalOperatingMicrovolts;           // e.g. 1150000 (1.15 V)
    ULONG    TransientExcursionCounter;            // e.g. 18
    ULONG    AdaptiveGuardBandMitigations;         // e.g. 145
    BOOLEAN  GuardBandTelemetryActive;             // 1
    BOOLEAN  CriticalHeadroomDepletion;            // 0
    uint8_t  GuardBandStatusFlags;                 // e.g. 0x07 (DynamicHeadroomScaling | FastTelemetryEn | VdroopCalibrated)
    uint8_t  Pad;                                  // 0
    ULONG    Reserved2;                            // 0
};
using PSYSTEM_CPU_CORE_VOLTAGE_GUARD_BAND_HEADROOM_INFORMATION = SYSTEM_CPU_CORE_VOLTAGE_GUARD_BAND_HEADROOM_INFORMATION*;

// Class 287: SystemPagefileTierMigrationLatencyInformation
struct SYSTEM_PAGEFILE_TIER_MIGRATION_LATENCY_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    SourceTierIndex;                      // e.g. 0
    ULONG    TargetTierIndex;                      // e.g. 1
    ULONG64  PagesMigrated;                        // e.g. 4194304ULL (4M pages)
    ULONG64  MigrationBytes;                       // e.g. 17179869184ULL (16 GB)
    ULONG    AverageLatencyNanoseconds;            // e.g. 85 ns
    ULONG    PeakLatencyNanoseconds;               // e.g. 240 ns
    BOOLEAN  MigrationThrottleActive;              // 0
    BOOLEAN  DmaEngineAssisted;                    // 1
    uint8_t  MigrationStatusFlags;                 // e.g. 0x07 (TierPipelineSync | NonBlockingAsync | CompressionEn)
    uint8_t  Pad;                                  // 0
    ULONG    Reserved2;                            // 0
};
using PSYSTEM_PAGEFILE_TIER_MIGRATION_LATENCY_INFORMATION = SYSTEM_PAGEFILE_TIER_MIGRATION_LATENCY_INFORMATION*;

// Class 288: SystemCpuCoreVoltageDroopPreventionTelemetryInformation
struct SYSTEM_CPU_CORE_VOLTAGE_DROOP_PREVENTION_TELEMETRY_INFORMATION {
    ULONG    Version;                              // e.g. 1
    ULONG    Reserved;                             // 0
    ULONG    CoreIndex;                            // e.g. 0
    ULONG    DroopMitigationEvents;                // e.g. 312
    ULONG    CurrentTargetMicrovolts;              // e.g. 1180000 (1.18 V)
    ULONG    DroopCompensationMicrovolts;          // e.g. 35000 (35 mV droop compensation)
    ULONG    ActiveVdroopLoadlineResistanceUohm;   // e.g. 1250 (1.25 mOhm)
    BOOLEAN  DroopPreventionActive;                // 1
    BOOLEAN  UnderVoltageDetected;                 // 0
    uint8_t  DroopPreventionStatusFlags;           // e.g. 0x05 (DynamicDroopComp | HighFreqSamplingEn)
    uint8_t  Pad;                                  // 0
    ULONG    Reserved2;                            // 0
};
using PSYSTEM_CPU_CORE_VOLTAGE_DROOP_PREVENTION_TELEMETRY_INFORMATION = SYSTEM_CPU_CORE_VOLTAGE_DROOP_PREVENTION_TELEMETRY_INFORMATION*;

inline constexpr ULONG TPM_FLAG_ENABLED           = 0x00000001;
inline constexpr ULONG TPM_FLAG_ACTIVATED         = 0x00000002;
inline constexpr ULONG TPM_FLAG_OWNED             = 0x00000004;
inline constexpr ULONG TPM_FLAG_SPEC_VERSION_2_0  = 0x00000008;
inline constexpr ULONG TPM_FLAG_READY             = 0x00000010;

struct SYSTEM_TPM_INFORMATION {
    ULONG Flags;
    ULONG TpmId;
};

struct RTL_SYSTEMTIME {
    uint16_t wYear;
    uint16_t wMonth;
    uint16_t wDayOfWeek;
    uint16_t wDay;
    uint16_t wHour;
    uint16_t wMinute;
    uint16_t wSecond;
    uint16_t wMilliseconds;
};

struct RTL_DYNAMIC_TIME_ZONE_INFORMATION {
    int32_t Bias;
    wchar_t StandardName[32];
    RTL_SYSTEMTIME StandardDate;
    int32_t StandardBias;
    wchar_t DaylightName[32];
    RTL_SYSTEMTIME DaylightDate;
    int32_t DaylightBias;
    wchar_t TimeZoneKeyName[128];
    uint8_t DynamicDaylightTimeDisabled;
};

inline constexpr ULONG CODEINTEGRITYPOLICY_OPTION_ENABLED                = 0x00000001;
inline constexpr ULONG CODEINTEGRITYPOLICY_OPTION_HVCI_ENFORCED          = 0x00000002;
inline constexpr ULONG CODEINTEGRITYPOLICY_OPTION_TESTSIGNING_DISABLED   = 0x00000004;
inline constexpr ULONG CODEINTEGRITYPOLICY_OPTION_DEBUGGING_DISABLED     = 0x00000008;

struct SYSTEM_EXCEPTION_INFORMATION {
    ULONG AlignmentFixupCount;
    ULONG ExceptionDispatchCount;
    ULONG FloatingEmulationCount;
    ULONG ByteWordEmulationCount;
};

struct SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION {
    uint64_t CycleTime;
};

struct SYSTEM_NUMA_NODE_PROCESSOR_MAP {
    uint32_t NodeNumber;
    uint32_t Reserved;
    uint64_t ProcessorMask;
};

struct SYSTEM_NUMA_DISTANCE_INFORMATION {
    uint32_t NodeFrom;
    uint32_t NodeTo;
    uint32_t Distance;
};

#ifndef _MICANT_LOGICAL_PROC_DEFINED
#define _MICANT_LOGICAL_PROC_DEFINED
using KAFFINITY = uint64_t;
using ULONG_PTR = uintptr_t;

enum LOGICAL_PROCESSOR_RELATIONSHIP : uint32_t {
    RelationProcessorCore    = 0,
    RelationNumaNode         = 1,
    RelationCache            = 2,
    RelationProcessorPackage = 3,
    RelationGroup            = 4,
    RelationAll              = 0xFFFF
};

enum PROCESSOR_CACHE_TYPE : uint32_t {
    CacheUnified     = 0,
    CacheInstruction = 1,
    CacheData        = 2,
    CacheTrace       = 3
};

struct CACHE_DESCRIPTOR {
    uint8_t   Level;
    uint8_t   Associativity;
    uint16_t  LineSize;
    uint32_t  Size;
    PROCESSOR_CACHE_TYPE Type;
};

struct SYSTEM_LOGICAL_PROCESSOR_INFORMATION {
    ULONG_PTR ProcessorMask;
    LOGICAL_PROCESSOR_RELATIONSHIP Relationship;
    union {
        struct {
            uint8_t Flags;
        } ProcessorCore;
        struct {
            uint32_t NodeNumber;
        } NumaNode;
        CACHE_DESCRIPTOR Cache;
        uint64_t Reserved[2];
    };
};

struct CACHE_RELATIONSHIP {
    uint8_t Level;
    uint8_t Associativity;
    uint16_t LineSize;
    uint32_t CacheSize;
    PROCESSOR_CACHE_TYPE Type;
    uint8_t Reserved[20];
    uint16_t GroupCount;
    KAFFINITY GroupMasks[1];
};

struct PROCESSOR_RELATIONSHIP {
    uint8_t Flags; // 0x01 = LTP_PC_SMT
    uint8_t EfficiencyClass; // 0 = E-core, 1 = P-core, 2 = 3D V-Cache P-core
    uint8_t Reserved[20];
    uint16_t GroupCount;
    KAFFINITY GroupMasks[1];
};

struct NUMA_NODE_RELATIONSHIP {
    uint32_t NodeNumber;
    uint8_t Reserved[20];
    uint16_t GroupCount;
    KAFFINITY GroupMasks[1];
};

struct SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX {
    LOGICAL_PROCESSOR_RELATIONSHIP Relationship;
    uint32_t Size;
    union {
        PROCESSOR_RELATIONSHIP Processor;
        NUMA_NODE_RELATIONSHIP NumaNode;
        CACHE_RELATIONSHIP Cache;
    };
};

enum CPU_SET_INFORMATION_TYPE {
    CpuSetInformation
};

struct SYSTEM_CPU_SET_INFORMATION {
    uint32_t Size;
    CPU_SET_INFORMATION_TYPE Type;
    union {
        struct {
            uint32_t Id;
            uint16_t Group;
            uint8_t  LogicalProcessorIndex;
            uint8_t  CoreIndex;
            uint8_t  LastLevelCacheIndex;
            uint8_t  NumaNodeIndex;
            uint8_t  EfficiencyClass;
            union {
                uint8_t AllFlags;
                struct {
                    uint8_t Parked : 1;
                    uint8_t Allocated : 1;
                    uint8_t AllocatedToTargetProcess : 1;
                    uint8_t RealTime : 1;
                    uint8_t ReservedFlags : 4;
                };
            };
            uint32_t Reserved;
            uint64_t AllocationTag;
        } CpuSet;
    };
};
using PSYSTEM_CPU_SET_INFORMATION = SYSTEM_CPU_SET_INFORMATION*;
#endif

struct SYSTEM_BOOT_ENVIRONMENT_INFORMATION {
    GUID BootIdentifier;
    ULONG FirmwareType; // 2 = FirmwareTypeUefi
    uint64_t BootFlags;
};

struct SYSTEM_KERNEL_DEBUGGER_INFORMATION_EX {
    uint8_t DebuggerAllowed{1};
    uint8_t DebuggerEnabled{0};
    uint8_t DebuggerPresent{0};
};
using PSYSTEM_KERNEL_DEBUGGER_INFORMATION_EX = SYSTEM_KERNEL_DEBUGGER_INFORMATION_EX*;

struct SYSTEM_BOOT_METADATA_INFORMATION {
    uint32_t Size{0};
    uint8_t Data[1]{0};
};
using PSYSTEM_BOOT_METADATA_INFORMATION = SYSTEM_BOOT_METADATA_INFORMATION*;

struct SYSTEM_FIRMWARE_PARTITION_INFORMATION {
    GUID PartitionGuid{0};
    uint32_t PartitionNumber{0};
    uint32_t PartitionFlags{0};
    uint64_t StartingLba{0};
    uint64_t SectorCount{0};
    uint32_t SectorSizeInBytes{0};
    uint32_t Reserved{0};
};
using PSYSTEM_FIRMWARE_PARTITION_INFORMATION = SYSTEM_FIRMWARE_PARTITION_INFORMATION*;

struct SYSTEM_SPECULATION_CONTROL_INFORMATION {
    union {
        ULONG Flags;
        struct {
            ULONG BpbEnabled : 1;
            ULONG BpbDisabledSystemPolicy : 1;
            ULONG BpbDisabledNoHardwareSupport : 1;
            ULONG SpecCtrlEnumerated : 1;
            ULONG SpecCmdEnumerated : 1;
            ULONG IbrsPresent : 1;
            ULONG StibpPresent : 1;
            ULONG SmepPresent : 1;
            ULONG SpeculativeStoreBypassDisableAvailable : 1;
            ULONG SpeculativeStoreBypassDisableSupported : 1;
            ULONG SpeculativeStoreBypassDisabledSystemPolicy : 1;
            ULONG SpeculativeStoreBypassDisabledNoHardwareSupport : 1;
            ULONG BranchLapDisabledSystemPolicy : 1;
            ULONG BranchLapDisabledNoHardwareSupport : 1;
            ULONG MdsCacheFlushSupported : 1;
            ULONG Reserved : 17;
        };
    } SpeculationControlFlags;
};
using PSYSTEM_SPECULATION_CONTROL_INFORMATION = SYSTEM_SPECULATION_CONTROL_INFORMATION*;

struct SYSTEM_KERNEL_VA_SHADOW_INFORMATION {
    union {
        ULONG KvaShadowFlags;
        struct {
            ULONG KvaShadowEnabled : 1;
            ULONG KvaShadowUserGlobal : 1;
            ULONG KvaShadowPcid : 1;
            ULONG KvaShadowInvpcid : 1;
            ULONG KvaShadowRequired : 1;
            ULONG KvaShadowRequiredAvailable : 1;
            ULONG L1DataCacheFlushSupported : 1;
            ULONG L1TerminalFaultMitigationPresent : 1;
            ULONG Reserved : 24;
        };
    };
};
using PSYSTEM_KERNEL_VA_SHADOW_INFORMATION = SYSTEM_KERNEL_VA_SHADOW_INFORMATION*;

struct SYSTEM_ISOLATED_USER_MODE_INFORMATION {
    BOOLEAN SecureKernelRunning : 1;
    BOOLEAN HvciEnabled : 1;
    BOOLEAN HvciStrictMode : 1;
    BOOLEAN DebugEnabled : 1;
    BOOLEAN FirmwarePageProtection : 1;
    BOOLEAN EncryptionKeyAvailable : 1;
    BOOLEAN SpareFlags : 2;
    BOOLEAN TrustletRunning : 1;
    BOOLEAN HvciOptions : 7;
    uint8_t Spare0[6];
    uint64_t Spare1;
};
using PSYSTEM_ISOLATED_USER_MODE_INFORMATION = SYSTEM_ISOLATED_USER_MODE_INFORMATION*;

struct SYSTEM_VSL_PROTECTION_INFORMATION {
    BOOLEAN VslProtectionEnabled : 1;
    BOOLEAN VslHardwareEnforced : 1;
    BOOLEAN VslHypervisorEnforced : 1;
    BOOLEAN Spare0 : 5;
    uint8_t Spare1[3];
};
using PSYSTEM_VSL_PROTECTION_INFORMATION = SYSTEM_VSL_PROTECTION_INFORMATION*;

struct SYSTEM_CONTROL_FLOW_GUARD_INFORMATION {
    ULONG ControlFlowGuardEnabled : 1;
    ULONG ControlFlowGuardExportSuppressionEnabled : 1;
    ULONG ControlFlowGuardStrict : 1;
    ULONG Reserved : 29;
};
using PSYSTEM_CONTROL_FLOW_GUARD_INFORMATION = SYSTEM_CONTROL_FLOW_GUARD_INFORMATION*;

struct SYSTEM_FIRMWARE_TABLE_INFORMATION {
    ULONG ProviderSignature;
    ULONG Action;
    ULONG TableID;
    ULONG TableBufferLength;
    uint8_t TableBuffer[1];
};

struct RTL_PROCESS_MODULE_INFORMATION {
    void* Section{nullptr};
    void* MappedBase{nullptr};
    void* ImageBase{nullptr};
    uint32_t ImageSize{0};
    uint32_t Flags{0};
    uint16_t LoadOrderIndex{0};
    uint16_t InitOrderIndex{0};
    uint16_t LoadCount{1};
    uint16_t OffsetToFileName{0};
    char FullPathName[256]{0};
};

struct RTL_PROCESS_MODULES {
    uint32_t NumberOfModules{4};
    RTL_PROCESS_MODULE_INFORMATION Modules[4]{};
};

struct FILE_INTERNAL_INFORMATION {
    int64_t IndexNumber;
};

struct FILE_STANDARD_INFORMATION {
    LargeInteger AllocationSize;
    LargeInteger EndOfFile;
    uint32_t NumberOfLinks;
    uint8_t DeletePending;
    uint8_t Directory;
};

inline NtStatus WINAPI NtQueryInformationFile_Export(
    void* FileHandle,
    void* /*IoStatusBlock*/,
    void* FileInformation,
    uint32_t Length,
    uint32_t FileInformationClass
) noexcept {
    if (!FileInformation && Length > 0) return NtStatus::InvalidParameter;
    if (FileHandle == nullptr || FileHandle == reinterpret_cast<void*>(-1)) {
        return NtStatus::InvalidHandle;
    }

    switch (FileInformationClass) {
        case 6: { // FileInternalInformation
            if (Length < sizeof(FILE_INTERNAL_INFORMATION)) return NtStatus::InfoLengthMismatch;
            auto* pInfo = reinterpret_cast<FILE_INTERNAL_INFORMATION*>(FileInformation);
            pInfo->IndexNumber = 0x0001000000001234LL; // Authentic 64-bit NTFS file ID
            return NtStatus::Success;
        }
        case 5: { // FileStandardInformation
            if (Length < sizeof(FILE_STANDARD_INFORMATION)) return NtStatus::InfoLengthMismatch;
            auto* pStd = reinterpret_cast<FILE_STANDARD_INFORMATION*>(FileInformation);
            pStd->AllocationSize.quadPart = 1048576;
            pStd->EndOfFile.quadPart = 1048576;
            pStd->NumberOfLinks = 1;
            pStd->DeletePending = 0;
            pStd->Directory = 0;
            return NtStatus::Success;
        }
        default: {
            if (FileInformation && Length > 0) std::memset(FileInformation, 0, Length);
            return NtStatus::Success;
        }
    }
}

struct PROCESS_BASIC_INFORMATION {
    NtStatus ExitStatus{NtStatus::Success};
    void* PebBaseAddress{nullptr};
    uintptr_t AffinityMask{0xFFFF};
    int32_t BasePriority{8};
    uintptr_t UniqueProcessId{1337};
    uintptr_t InheritedFromUniqueProcessId{4};
};

inline NtStatus WINAPI NtQueryInformationProcess_Export(
    void* /*ProcessHandle*/,
    uint32_t ProcessInformationClass,
    void* ProcessInformation,
    uint32_t ProcessInformationLength,
    uint32_t* ReturnLength
) noexcept {
    if (!ProcessInformation && ProcessInformationLength > 0) {
        return NtStatus::InvalidParameter;
    }

    switch (ProcessInformationClass) {
        case 0: { // ProcessBasicInformation
            if (ProcessInformationLength < sizeof(PROCESS_BASIC_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(PROCESS_BASIC_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<PROCESS_BASIC_INFORMATION*>(ProcessInformation);
            pInfo->ExitStatus = NtStatus::Success;
            pInfo->PebBaseAddress = reinterpret_cast<void*>(0x00007FF6FFFF0000ULL);
            pInfo->AffinityMask = 0xFFFF; // 16 cores
            pInfo->BasePriority = 8;      // NORMAL_PRIORITY_CLASS
            pInfo->UniqueProcessId = 1337;
            pInfo->InheritedFromUniqueProcessId = 4; // System PID
            if (ReturnLength) *ReturnLength = sizeof(PROCESS_BASIC_INFORMATION);
            return NtStatus::Success;
        }
        case 7: { // ProcessDebugPort
            if (ProcessInformationLength < sizeof(uintptr_t)) {
                if (ReturnLength) *ReturnLength = sizeof(uintptr_t);
                return NtStatus::InfoLengthMismatch;
            }
            *reinterpret_cast<uintptr_t*>(ProcessInformation) = 0; // No debugger
            if (ReturnLength) *ReturnLength = sizeof(uintptr_t);
            return NtStatus::Success;
        }
        case 30: { // ProcessDebugObjectHandle
            if (ProcessInformationLength < sizeof(void*)) {
                if (ReturnLength) *ReturnLength = sizeof(void*);
                return NtStatus::InfoLengthMismatch;
            }
            *reinterpret_cast<void**>(ProcessInformation) = nullptr;
            if (ReturnLength) *ReturnLength = sizeof(void*);
            return static_cast<NtStatus>(0xC0000353); // STATUS_PORT_NOT_SET
        }
        case 31: { // ProcessDebugFlags
            if (ProcessInformationLength < sizeof(uint32_t)) {
                if (ReturnLength) *ReturnLength = sizeof(uint32_t);
                return NtStatus::InfoLengthMismatch;
            }
            *reinterpret_cast<uint32_t*>(ProcessInformation) = 1; // 1 = PROCESS_DEBUG_INHERIT (No debugger attached)
            if (ReturnLength) *ReturnLength = sizeof(uint32_t);
            return NtStatus::Success;
        }
        default: {
            if (ProcessInformation && ProcessInformationLength > 0) {
                std::memset(ProcessInformation, 0, ProcessInformationLength);
            }
            if (ReturnLength) *ReturnLength = ProcessInformationLength;
            return NtStatus::Success;
        }
    }
}

struct SYSTEM_PERFORMANCE_INFORMATION {
    LargeInteger IdleProcessTime{};
    LargeInteger IoReadTransferCount{};
    LargeInteger IoWriteTransferCount{};
    LargeInteger IoOtherTransferCount{};
    uint32_t IoReadOperationCount{0};
    uint32_t IoWriteOperationCount{0};
    uint32_t IoOtherOperationCount{0};
    uint32_t AvailablePages{6291456};
    uint32_t CommittedPages{2097152};
    uint32_t CommitLimit{8388608};
    uint32_t PeakCommitment{3145728};
    uint32_t PageFaultCount{10240};
    uint32_t CopyOnWriteCount{256};
    uint32_t TransitionCount{128};
    uint32_t CacheTransitionCount{64};
    uint32_t DemandZeroCount{5120};
    uint32_t PageReadCount{1024};
    uint32_t PageReadIocount{512};
    uint32_t ReplaceCounter{0};
    uint32_t PagefilePagesWritten{0};
    uint32_t PagefilePagesTotal{0};
    uint32_t AvailablePagedPoolPages{131072};
    uint32_t ResidentAvailablePages{6291456};
    uint32_t PagedPoolPages{65536};
    uint32_t NonPagedPoolPages{32768};
    uint32_t PagedPoolAllocs{125000};
    uint32_t PagedPoolFrees{100000};
    uint32_t NonPagedPoolAllocs{80000};
    uint32_t NonPagedPoolFrees{70000};
    uint32_t FreeSystemPtes{2000000};
    uint32_t ResidentSystemCodePage{8192};
    uint32_t TotalSystemDriverPages{16384};
    uint32_t TotalSystemCodePages{24576};
    uint32_t NonPagedPoolLookasideHits{10000};
    uint32_t PagedPoolLookasideHits{15000};
    uint32_t Spare3Count{0};
    uint32_t ResidentSystemCachePage{32768};
    uint32_t ResidentPagedPoolPage{65536};
    uint32_t ResidentSystemDriverPage{16384};
    uint32_t CcFastReadNoWait{0};
    uint32_t CcFastReadWait{0};
    uint32_t CcFastReadResourceMiss{0};
    uint32_t CcFastReadNotPossible{0};
    uint32_t CcFastMdlReadNoWait{0};
    uint32_t CcFastMdlReadWait{0};
    uint32_t CcFastMdlReadResourceMiss{0};
    uint32_t CcFastMdlReadNotPossible{0};
    uint32_t CcMapDataNoWait{0};
    uint32_t CcMapDataWait{0};
    uint32_t CcMapDataNoWaitMiss{0};
    uint32_t CcMapDataWaitMiss{0};
    uint32_t CcPinMappedDataCount{0};
    uint32_t CcPinReadNoWait{0};
    uint32_t CcPinReadWait{0};
    uint32_t CcPinReadNoWaitMiss{0};
    uint32_t CcPinReadWaitMiss{0};
    uint32_t CcCopyReadNoWait{0};
    uint32_t CcCopyReadWait{0};
    uint32_t CcCopyReadNoWaitMiss{0};
    uint32_t CcCopyReadWaitMiss{0};
    uint32_t CcMdlReadNoWait{0};
    uint32_t CcMdlReadWait{0};
    uint32_t CcMdlReadNoWaitMiss{0};
    uint32_t CcMdlReadWaitMiss{0};
    uint32_t CcReadAheadIos{0};
    uint32_t CcLazyWriteIos{0};
    uint32_t CcLazyWritePages{0};
    uint32_t CcDataFlushes{0};
    uint32_t CcDataPages{0};
    uint32_t ContextSwitches{500000};
    uint32_t FirstLevelTbFills{0};
    uint32_t SecondLevelTbFills{0};
    uint32_t SystemCalls{1250000};
};

struct SYSTEM_THREAD_INFORMATION {
    LargeInteger KernelTime{};
    LargeInteger UserTime{};
    LargeInteger CreateTime{};
    uint32_t WaitTime{0};
    void* StartAddress{nullptr};
    struct {
        void* UniqueProcess{nullptr};
        void* UniqueThread{nullptr};
    } ClientId{};
    int32_t Priority{8};
    int32_t BasePriority{8};
    uint32_t ContextSwitches{100};
    uint32_t ThreadState{5};
    uint32_t WaitReason{7};
};

struct SYSTEM_PROCESS_INFORMATION {
    uint32_t NextEntryOffset{0};
    uint32_t NumberOfThreads{1};
    LargeInteger WorkingSetPrivateSize{};
    uint32_t HardFaultCount{0};
    uint32_t NumberOfThreadsHighWatermark{1};
    uint64_t CycleTime{0};
    LargeInteger CreateTime{};
    LargeInteger UserTime{};
    LargeInteger KernelTime{};
    UNICODE_STRING ImageName{};
    int32_t BasePriority{8};
    void* UniqueProcessId{nullptr};
    void* InheritedFromUniqueProcessId{nullptr};
    uint32_t HandleCount{100};
    uint32_t SessionId{1};
    uintptr_t UniqueProcessKey{0};
    size_t PeakVirtualSize{0x10000000};
    size_t VirtualSize{0x8000000};
    uint32_t PageFaultCount{1000};
    size_t PeakWorkingSetSize{0x4000000};
    size_t WorkingSetSize{0x2000000};
    size_t QuotaPeakPagedPoolUsage{0x10000};
    size_t QuotaPagedPoolUsage{0x8000};
    size_t QuotaPeakNonPagedPoolUsage{0x10000};
    size_t QuotaNonPagedPoolUsage{0x8000};
    size_t PagefileUsage{0x2000000};
    size_t PeakPagefileUsage{0x4000000};
    size_t PrivatePageCount{0x2000000};
    LargeInteger ReadOperationCount{};
    LargeInteger WriteOperationCount{};
    LargeInteger OtherOperationCount{};
    LargeInteger ReadTransferCount{};
    LargeInteger WriteTransferCount{};
    LargeInteger OtherTransferCount{};
};

struct SYSTEM_TIMEOFDAY_INFORMATION {
    LargeInteger BootTime{};
    LargeInteger CurrentTime{};
    LargeInteger TimeZoneBias{};
    uint32_t TimeZoneId{0};
    uint32_t Reserved{0};
    uint64_t BootTimeBias{0};
    uint64_t SleepTimeBias{0};
};

struct SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
    LargeInteger IdleTime;
    LargeInteger KernelTime;
    LargeInteger UserTime;
    LargeInteger DpcTime;
    LargeInteger InterruptTime;
    uint32_t     InterruptCount{0};
};

struct SYSTEM_LOOKASIDE_INFORMATION {
    uint16_t CurrentDepth{0};
    uint16_t MaximumDepth{0};
    uint32_t TotalAllocates{0};
    uint32_t AllocateMisses{0};
    uint32_t TotalFrees{0};
    uint32_t FreeMisses{0};
    uint32_t Type{0};
    uint32_t Tag{0};
    uint32_t Size{0};
};

struct SYSTEM_HANDLE_TABLE_ENTRY_INFO {
    uint16_t UniqueProcessId{0};
    uint16_t CreatorBackTraceIndex{0};
    uint8_t  ObjectTypeIndex{0};
    uint8_t  HandleAttributes{0};
    uint16_t HandleValue{0};
    void*    Object{nullptr};
    uint32_t GrantedAccess{0};
};

struct SYSTEM_HANDLE_INFORMATION {
    uint32_t NumberOfHandles{0};
    SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];
};

struct SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
    void*    Object{nullptr};
    uint64_t UniqueProcessId{0};
    uint64_t HandleValue{0};
    uint32_t GrantedAccess{0};
    uint16_t CreatorBackTraceIndex{0};
    uint16_t ObjectTypeIndex{0};
    uint32_t HandleAttributes{0};
    uint32_t Reserved{0};
};

struct SYSTEM_HANDLE_INFORMATION_EX {
    uint64_t NumberOfHandles{0};
    uint64_t Reserved{0};
    SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
};

struct SYSTEM_INTERRUPT_INFORMATION {
    uint32_t ContextSwitches{0};
    uint32_t DpcCount{0};
    uint32_t DpcRate{0};
    uint32_t TimeIncrement{156250};
    uint32_t DpcBypassCount{0};
    uint32_t ApcBypassCount{0};
};

struct SYSTEM_SECUREBOOT_INFORMATION {
    uint8_t SecureBootEnabled{1};
    uint8_t SecureBootCapable{1};
};

struct SYSTEM_HYPERVISOR_INFORMATION {
    uint8_t HypervisorPresent{0};
    uint8_t EnclaveFeaturePageAvailable{1};
    uint8_t Reserved[2]{0, 0};
};

inline NtStatus WINAPI NtQuerySystemInformation_Export(
    uint32_t SystemInformationClass,
    void* SystemInformation,
    uint32_t SystemInformationLength,
    uint32_t* ReturnLength
) noexcept {
    if (!SystemInformation && SystemInformationLength > 0) {
        return NtStatus::InvalidParameter;
    }

    switch (SystemInformationClass) {
        case 0: { // SystemBasicInformation
            if (SystemInformationLength < sizeof(SYSTEM_BASIC_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_BASIC_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_BASIC_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(SYSTEM_BASIC_INFORMATION));
            pInfo->PageSize = 4096;
            pInfo->NumberOfPhysicalPages = 8388608; // 32GB / 4KB
            pInfo->AllocationGranularity = 65536;
            pInfo->MinimumUserModeAddress = 0x10000;
            pInfo->MaximumUserModeAddress = 0x7FFFFFFEFFFFULL;
            pInfo->ActiveProcessorsAffinityMask = 0xFFFF; // 16 cores
            pInfo->NumberOfProcessors = 16;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_BASIC_INFORMATION);
            return NtStatus::Success;
        }
        case 3: { // SystemTimeOfDayInformation
            if (SystemInformationLength < sizeof(SYSTEM_TIMEOFDAY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_TIMEOFDAY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pTod = reinterpret_cast<SYSTEM_TIMEOFDAY_INFORMATION*>(SystemInformation);
            std::memset(pTod, 0, sizeof(SYSTEM_TIMEOFDAY_INFORMATION));
            pTod->BootTime.quadPart = 133500000000000000LL;
            pTod->CurrentTime.quadPart = 133500005000000000LL;
            pTod->TimeZoneBias.quadPart = 0;
            pTod->TimeZoneId = 1;
            pTod->Reserved = 0;
            pTod->BootTimeBias = 0;
            pTod->SleepTimeBias = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_TIMEOFDAY_INFORMATION);
            return NtStatus::Success;
        }
        case 8: { // SystemProcessorPerformanceInformation
            constexpr uint32_t kNumProcessors = 16;
            constexpr uint32_t kTotalSize = kNumProcessors * sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION);
            if (SystemInformationLength < kTotalSize) {
                if (ReturnLength) *ReturnLength = kTotalSize;
                return NtStatus::InfoLengthMismatch;
            }
            auto* pPerf = reinterpret_cast<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION*>(SystemInformation);
            for (uint32_t i = 0; i < kNumProcessors; ++i) {
                pPerf[i].IdleTime.quadPart = 10000000000LL + i * 50000000LL;
                pPerf[i].KernelTime.quadPart = 2500000000LL + i * 20000000LL;
                pPerf[i].UserTime.quadPart = 1500000000LL + i * 15000000LL;
                pPerf[i].DpcTime.quadPart = 100000000LL + i * 2000000LL;
                pPerf[i].InterruptTime.quadPart = 50000000LL + i * 1000000LL;
                pPerf[i].InterruptCount = 100000 + i * 1000;
            }
            if (ReturnLength) *ReturnLength = kTotalSize;
            return NtStatus::Success;
        }
        case 45: { // SystemLookasideInformation
            constexpr uint32_t kEntries = 4;
            constexpr uint32_t kTotalSize = kEntries * sizeof(SYSTEM_LOOKASIDE_INFORMATION);
            if (SystemInformationLength < kTotalSize) {
                if (ReturnLength) *ReturnLength = kTotalSize;
                return NtStatus::InfoLengthMismatch;
            }
            auto* pLook = reinterpret_cast<SYSTEM_LOOKASIDE_INFORMATION*>(SystemInformation);
            for (uint32_t i = 0; i < kEntries; ++i) {
                pLook[i].CurrentDepth = 16;
                pLook[i].MaximumDepth = 64;
                pLook[i].TotalAllocates = 50000 + i * 1000;
                pLook[i].AllocateMisses = 200 + i * 10;
                pLook[i].TotalFrees = 49800 + i * 1000;
                pLook[i].FreeMisses = 150 + i * 5;
                pLook[i].Type = 0;
                pLook[i].Tag = 0x4D696361; // 'Mica'
                pLook[i].Size = 64 * (i + 1);
            }
            if (ReturnLength) *ReturnLength = kTotalSize;
            return NtStatus::Success;
        }
        case 23: { // SystemInterruptInformation
            constexpr uint32_t kNumProcessors = 16;
            constexpr uint32_t kTotalSize = kNumProcessors * sizeof(SYSTEM_INTERRUPT_INFORMATION);
            if (SystemInformationLength < kTotalSize) {
                if (ReturnLength) *ReturnLength = kTotalSize;
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInt = reinterpret_cast<SYSTEM_INTERRUPT_INFORMATION*>(SystemInformation);
            for (uint32_t i = 0; i < kNumProcessors; ++i) {
                pInt[i].ContextSwitches = 50000 + i * 250;
                pInt[i].DpcCount = 12000 + i * 100;
                pInt[i].DpcRate = 120;
                pInt[i].TimeIncrement = 156250;
                pInt[i].DpcBypassCount = 0;
                pInt[i].ApcBypassCount = 0;
            }
            if (ReturnLength) *ReturnLength = kTotalSize;
            return NtStatus::Success;
        }
        case 2: { // SystemPerformanceInformation
            if (SystemInformationLength < sizeof(SYSTEM_PERFORMANCE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PERFORMANCE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pPerf = reinterpret_cast<SYSTEM_PERFORMANCE_INFORMATION*>(SystemInformation);
            std::memset(pPerf, 0, sizeof(SYSTEM_PERFORMANCE_INFORMATION));
            pPerf->AvailablePages = 6291456;
            pPerf->CommittedPages = 2097152;
            pPerf->CommitLimit = 8388608;
            pPerf->PeakCommitment = 3145728;
            pPerf->PagedPoolPages = 65536;
            pPerf->NonPagedPoolPages = 32768;
            pPerf->ContextSwitches = 500000;
            pPerf->SystemCalls = 1250000;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PERFORMANCE_INFORMATION);
            return NtStatus::Success;
        }
        case 5: { // SystemProcessInformation
            constexpr uint32_t kEntry1Offset = 0;
            constexpr uint32_t kEntry2Offset = 512;
            constexpr uint32_t kTotalNeeded = 1024;
            if (SystemInformationLength < kTotalNeeded) {
                if (ReturnLength) *ReturnLength = kTotalNeeded;
                return NtStatus::InfoLengthMismatch;
            }
            uint8_t* pBuf = reinterpret_cast<uint8_t*>(SystemInformation);
            std::memset(pBuf, 0, SystemInformationLength);

            // Process 1: System (PID 4)
            auto* pProc1 = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(pBuf + kEntry1Offset);
            pProc1->NextEntryOffset = kEntry2Offset;
            pProc1->NumberOfThreads = 4;
            pProc1->UniqueProcessId = reinterpret_cast<void*>(4);
            pProc1->InheritedFromUniqueProcessId = nullptr;
            pProc1->BasePriority = 8;
            pProc1->HandleCount = 1024;
            pProc1->SessionId = 0;
            static const wchar_t s_systemName[] = L"System";
            pProc1->ImageName.buffer = s_systemName;
            pProc1->ImageName.length = static_cast<uint16_t>(6 * sizeof(wchar_t));
            pProc1->ImageName.maximumLength = static_cast<uint16_t>(7 * sizeof(wchar_t));

            // Process 2: micant.exe (PID 1337)
            auto* pProc2 = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(pBuf + kEntry2Offset);
            pProc2->NextEntryOffset = 0;
            pProc2->NumberOfThreads = 16;
            pProc2->UniqueProcessId = reinterpret_cast<void*>(1337);
            pProc2->InheritedFromUniqueProcessId = reinterpret_cast<void*>(4);
            pProc2->BasePriority = 8;
            pProc2->HandleCount = 256;
            pProc2->SessionId = 1;
            static const wchar_t s_micantName[] = L"micant.exe";
            pProc2->ImageName.buffer = s_micantName;
            pProc2->ImageName.length = static_cast<uint16_t>(10 * sizeof(wchar_t));
            pProc2->ImageName.maximumLength = static_cast<uint16_t>(11 * sizeof(wchar_t));

            if (ReturnLength) *ReturnLength = kTotalNeeded;
            return NtStatus::Success;
        }
        case 11: { // SystemModuleInformation
            if (SystemInformationLength < sizeof(RTL_PROCESS_MODULES)) {
                if (ReturnLength) *ReturnLength = sizeof(RTL_PROCESS_MODULES);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pMods = reinterpret_cast<RTL_PROCESS_MODULES*>(SystemInformation);
            std::memset(pMods, 0, sizeof(RTL_PROCESS_MODULES));
            pMods->NumberOfModules = 4;

            // Module 0: ntoskrnl.exe
            pMods->Modules[0].ImageBase = reinterpret_cast<void*>(0xFFFFF80000000000ULL);
            pMods->Modules[0].ImageSize = 0x01000000;
            pMods->Modules[0].OffsetToFileName = 22;
            std::strncpy(pMods->Modules[0].FullPathName, "\\SystemRoot\\system32\\ntoskrnl.exe", sizeof(pMods->Modules[0].FullPathName) - 1);

            // Module 1: hal.dll
            pMods->Modules[1].ImageBase = reinterpret_cast<void*>(0xFFFFF80001000000ULL);
            pMods->Modules[1].ImageSize = 0x00100000;
            pMods->Modules[1].OffsetToFileName = 22;
            std::strncpy(pMods->Modules[1].FullPathName, "\\SystemRoot\\system32\\hal.dll", sizeof(pMods->Modules[1].FullPathName) - 1);

            // Module 2: dxgkrnl.sys
            pMods->Modules[2].ImageBase = reinterpret_cast<void*>(0xFFFFF80001100000ULL);
            pMods->Modules[2].ImageSize = 0x00400000;
            pMods->Modules[2].OffsetToFileName = 30;
            std::strncpy(pMods->Modules[2].FullPathName, "\\SystemRoot\\system32\\drivers\\dxgkrnl.sys", sizeof(pMods->Modules[2].FullPathName) - 1);

            // Module 3: fltmgr.sys
            pMods->Modules[3].ImageBase = reinterpret_cast<void*>(0xFFFFF80001500000ULL);
            pMods->Modules[3].ImageSize = 0x00080000;
            pMods->Modules[3].OffsetToFileName = 30;
            std::strncpy(pMods->Modules[3].FullPathName, "\\SystemRoot\\system32\\drivers\\fltmgr.sys", sizeof(pMods->Modules[3].FullPathName) - 1);

            if (ReturnLength) *ReturnLength = sizeof(RTL_PROCESS_MODULES);
            return NtStatus::Success;
        }
        case 35: { // SystemKernelDebuggerInformation
            if (SystemInformationLength < sizeof(SYSTEM_KERNEL_DEBUGGER_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_KERNEL_DEBUGGER_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_KERNEL_DEBUGGER_INFORMATION*>(SystemInformation);
            pInfo->KernelDebuggerEnabled = 0;
            pInfo->KernelDebuggerNotPresent = 1;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_KERNEL_DEBUGGER_INFORMATION);
            return NtStatus::Success;
        }
        case 90: { // SystemBootEnvironmentInformation
            if (SystemInformationLength < sizeof(SYSTEM_BOOT_ENVIRONMENT_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_BOOT_ENVIRONMENT_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_BOOT_ENVIRONMENT_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(SYSTEM_BOOT_ENVIRONMENT_INFORMATION));
            pInfo->BootIdentifier = { 0x5a18a93e, 0x8264, 0x47cb, { 0x98, 0x3d, 0x6e, 0x98, 0x3a, 0x76, 0x86, 0x8b } };
            pInfo->FirmwareType = 2; // FirmwareTypeUefi
            pInfo->BootFlags = 0x7;  // SecureBoot (0x1) | MeasuredBoot (0x2) | ElamDriversLoaded (0x4)
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_BOOT_ENVIRONMENT_INFORMATION);
            return NtStatus::Success;
        }
        case 149: { // SystemKernelDebuggerInformationEx
            if (SystemInformationLength < sizeof(SYSTEM_KERNEL_DEBUGGER_INFORMATION_EX)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_KERNEL_DEBUGGER_INFORMATION_EX);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_KERNEL_DEBUGGER_INFORMATION_EX*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(SYSTEM_KERNEL_DEBUGGER_INFORMATION_EX));
            pInfo->DebuggerAllowed = 1;
            pInfo->DebuggerEnabled = 0;
            pInfo->DebuggerPresent = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_KERNEL_DEBUGGER_INFORMATION_EX);
            return NtStatus::Success;
        }
        case 150: { // SystemBootMetadataInformation
            struct SovereignBootMetadataPayload {
                uint32_t Version{1};
                uint32_t Signature{0x4D494341}; // 'MICA'
                uint32_t BootFlags{0x00000007};  // EFI, SecureBoot, TPM-measured
                uint32_t Reserved{0};
                uint64_t BootTimestamp{133500000000000000ULL};
            };
            constexpr uint32_t kPayloadSize = sizeof(SovereignBootMetadataPayload);
            constexpr uint32_t kTotalRequired = sizeof(uint32_t) + kPayloadSize;

            if (SystemInformationLength < sizeof(uint32_t)) {
                if (ReturnLength) *ReturnLength = kTotalRequired;
                return NtStatus::InfoLengthMismatch;
            }

            auto* pMeta = reinterpret_cast<SYSTEM_BOOT_METADATA_INFORMATION*>(SystemInformation);
            pMeta->Size = kTotalRequired;

            if (SystemInformationLength < kTotalRequired) {
                if (ReturnLength) *ReturnLength = kTotalRequired;
                return NtStatus::InfoLengthMismatch;
            }

            SovereignBootMetadataPayload payload;
            std::memcpy(pMeta->Data, &payload, sizeof(payload));

            if (ReturnLength) *ReturnLength = kTotalRequired;
            return NtStatus::Success;
        }
        case 151: { // SystemFirmwarePartitionInformation
            if (SystemInformationLength < sizeof(SYSTEM_FIRMWARE_PARTITION_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_FIRMWARE_PARTITION_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pPart = reinterpret_cast<SYSTEM_FIRMWARE_PARTITION_INFORMATION*>(SystemInformation);
            pPart->PartitionGuid = { 0xC12A7328, 0xF81F, 0x11D2, { 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } }; // EFI System Partition GUID
            pPart->PartitionNumber = 1;
            pPart->PartitionFlags = 0x00000003; // Boot | System ESP
            pPart->StartingLba = 2048;          // Standard 1MB alignment
            pPart->SectorCount = 524288;        // 256 MB ESP
            pPart->SectorSizeInBytes = 512;
            pPart->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_FIRMWARE_PARTITION_INFORMATION);
            return NtStatus::Success;
        }
        case 165: { // SystemIsolatedUserModeInformation
            if (SystemInformationLength < sizeof(SYSTEM_ISOLATED_USER_MODE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_ISOLATED_USER_MODE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pIum = reinterpret_cast<SYSTEM_ISOLATED_USER_MODE_INFORMATION*>(SystemInformation);
            std::memset(pIum, 0, sizeof(SYSTEM_ISOLATED_USER_MODE_INFORMATION));
            pIum->SecureKernelRunning = 1;
            pIum->HvciEnabled = 1;
            pIum->HvciStrictMode = 1;
            pIum->DebugEnabled = 0;
            pIum->FirmwarePageProtection = 1;
            pIum->EncryptionKeyAvailable = 1;
            pIum->SpareFlags = 0;
            pIum->TrustletRunning = 1;
            pIum->HvciOptions = 1;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_ISOLATED_USER_MODE_INFORMATION);
            return NtStatus::Success;
        }
        case 169: { // SystemVslProtectionInformation
            if (SystemInformationLength < sizeof(SYSTEM_VSL_PROTECTION_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_VSL_PROTECTION_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pVsl = reinterpret_cast<SYSTEM_VSL_PROTECTION_INFORMATION*>(SystemInformation);
            std::memset(pVsl, 0, sizeof(SYSTEM_VSL_PROTECTION_INFORMATION));
            pVsl->VslProtectionEnabled = 1;
            pVsl->VslHardwareEnforced = 1;
            pVsl->VslHypervisorEnforced = 1;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_VSL_PROTECTION_INFORMATION);
            return NtStatus::Success;
        }
        case 181: { // SystemControlFlowGuardInformation
            if (SystemInformationLength < sizeof(SYSTEM_CONTROL_FLOW_GUARD_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CONTROL_FLOW_GUARD_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pCfg = reinterpret_cast<SYSTEM_CONTROL_FLOW_GUARD_INFORMATION*>(SystemInformation);
            std::memset(pCfg, 0, sizeof(SYSTEM_CONTROL_FLOW_GUARD_INFORMATION));
            pCfg->ControlFlowGuardEnabled = 1;
            pCfg->ControlFlowGuardExportSuppressionEnabled = 1;
            pCfg->ControlFlowGuardStrict = 1;
            pCfg->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CONTROL_FLOW_GUARD_INFORMATION);
            return NtStatus::Success;
        }
        case 196: { // SystemKernelVaShadowInformation
            if (SystemInformationLength < sizeof(SYSTEM_KERNEL_VA_SHADOW_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_KERNEL_VA_SHADOW_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pVa = reinterpret_cast<SYSTEM_KERNEL_VA_SHADOW_INFORMATION*>(SystemInformation);
            pVa->KvaShadowFlags = 0;
            pVa->KvaShadowEnabled = 1;
            pVa->KvaShadowUserGlobal = 1;
            pVa->KvaShadowPcid = 1;
            pVa->KvaShadowInvpcid = 1;
            pVa->KvaShadowRequired = 1;
            pVa->KvaShadowRequiredAvailable = 1;
            pVa->L1DataCacheFlushSupported = 1;
            pVa->L1TerminalFaultMitigationPresent = 1;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_KERNEL_VA_SHADOW_INFORMATION);
            return NtStatus::Success;
        }
        case 201: { // SystemSpeculationControlInformation
            if (SystemInformationLength < sizeof(SYSTEM_SPECULATION_CONTROL_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SPECULATION_CONTROL_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pSpec = reinterpret_cast<SYSTEM_SPECULATION_CONTROL_INFORMATION*>(SystemInformation);
            pSpec->SpeculationControlFlags.Flags = 0;
            pSpec->SpeculationControlFlags.BpbEnabled = 1;
            pSpec->SpeculationControlFlags.SpecCtrlEnumerated = 1;
            pSpec->SpeculationControlFlags.SpecCmdEnumerated = 1;
            pSpec->SpeculationControlFlags.IbrsPresent = 1;
            pSpec->SpeculationControlFlags.StibpPresent = 1;
            pSpec->SpeculationControlFlags.SmepPresent = 1;
            pSpec->SpeculationControlFlags.SpeculativeStoreBypassDisableAvailable = 1;
            pSpec->SpeculationControlFlags.SpeculativeStoreBypassDisableSupported = 1;
            pSpec->SpeculationControlFlags.MdsCacheFlushSupported = 1;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SPECULATION_CONTROL_INFORMATION);
            return NtStatus::Success;
        }
        case 16: { // SystemHandleInformation
            constexpr uint32_t kNumHandles = 8;
            constexpr uint32_t kTotalSize = sizeof(uint32_t) + kNumHandles * sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO);
            if (SystemInformationLength < kTotalSize) {
                if (ReturnLength) *ReturnLength = kTotalSize;
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_HANDLE_INFORMATION*>(SystemInformation);
            pInfo->NumberOfHandles = kNumHandles;

            // Handles for PID 4 (System)
            pInfo->Handles[0] = { 4, 0, 19, 0, 0x0004, reinterpret_cast<void*>(0xFFFF800000100000ULL), 0x001F0003 }; // Event
            pInfo->Handles[1] = { 4, 0, 41, 0, 0x0008, reinterpret_cast<void*>(0xFFFF800000101000ULL), 0x000F001F }; // Section
            pInfo->Handles[2] = { 4, 0, 8,  0, 0x000C, reinterpret_cast<void*>(0xFFFF800000102000ULL), 0x00100000 }; // Thread
            pInfo->Handles[3] = { 4, 0, 37, 0, 0x0010, reinterpret_cast<void*>(0xFFFF800000103000ULL), 0x001F01FF }; // File

            // Handles for PID 1337 (micant.exe)
            pInfo->Handles[4] = { 1337, 0, 7,  0, 0x0040, reinterpret_cast<void*>(0xFFFF800000200000ULL), 0x00101000 }; // Process (self)
            pInfo->Handles[5] = { 1337, 0, 8,  0, 0x0044, reinterpret_cast<void*>(0xFFFF800000201000ULL), 0x001FFFFF }; // Thread
            pInfo->Handles[6] = { 1337, 0, 19, 0, 0x0048, reinterpret_cast<void*>(0xFFFF800000202000ULL), 0x001F0003 }; // Event
            pInfo->Handles[7] = { 1337, 0, 41, 0, 0x004C, reinterpret_cast<void*>(0xFFFF800000203000ULL), 0x000F001F }; // Section

            if (ReturnLength) *ReturnLength = kTotalSize;
            return NtStatus::Success;
        }
        case 64: { // SystemExtendedHandleInformation
            constexpr uint32_t kNumHandles = 8;
            constexpr uint32_t kTotalSize = sizeof(uint64_t) * 2 + kNumHandles * sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX);
            if (SystemInformationLength < kTotalSize) {
                if (ReturnLength) *ReturnLength = kTotalSize;
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(SystemInformation);
            pInfo->NumberOfHandles = kNumHandles;
            pInfo->Reserved = 0;

            // Handles for PID 4 (System)
            pInfo->Handles[0] = { reinterpret_cast<void*>(0xFFFF800000100000ULL), 4, 0x0004, 0x001F0003, 0, 19, 0, 0 }; // Event
            pInfo->Handles[1] = { reinterpret_cast<void*>(0xFFFF800000101000ULL), 4, 0x0008, 0x000F001F, 0, 41, 0, 0 }; // Section
            pInfo->Handles[2] = { reinterpret_cast<void*>(0xFFFF800000102000ULL), 4, 0x000C, 0x00100000, 0, 8,  0, 0 }; // Thread
            pInfo->Handles[3] = { reinterpret_cast<void*>(0xFFFF800000103000ULL), 4, 0x0010, 0x001F01FF, 0, 37, 0, 0 }; // File

            // Handles for PID 1337 (micant.exe)
            pInfo->Handles[4] = { reinterpret_cast<void*>(0xFFFF800000200000ULL), 1337, 0x0040, 0x00101000, 0, 7,  0, 0 }; // Process (self)
            pInfo->Handles[5] = { reinterpret_cast<void*>(0xFFFF800000201000ULL), 1337, 0x0044, 0x001FFFFF, 0, 8,  0, 0 }; // Thread
            pInfo->Handles[6] = { reinterpret_cast<void*>(0xFFFF800000202000ULL), 1337, 0x0048, 0x001F0003, 0, 19, 0, 0 }; // Event
            pInfo->Handles[7] = { reinterpret_cast<void*>(0xFFFF800000203000ULL), 1337, 0x004C, 0x000F001F, 0, 41, 0, 0 }; // Section

            if (ReturnLength) *ReturnLength = kTotalSize;
            return NtStatus::Success;
        }
        case 76: { // SystemFirmwareTableInformation
            if (SystemInformationLength < 16) {
                if (ReturnLength) *ReturnLength = 16;
                return NtStatus::InfoLengthMismatch;
            }
            auto* pFw = reinterpret_cast<SYSTEM_FIRMWARE_TABLE_INFORMATION*>(SystemInformation);
            if (pFw->Action == 0) { // Enumerate
                if (pFw->ProviderSignature == 0x52534D42) { // 'RSMB'
                    constexpr uint32_t needed = 4;
                    if (pFw->TableBufferLength < needed) {
                        pFw->TableBufferLength = needed;
                        if (ReturnLength) *ReturnLength = 16 + needed;
                        return NtStatus::BufferTooSmall;
                    }
                    *reinterpret_cast<ULONG*>(pFw->TableBuffer) = 0;
                    pFw->TableBufferLength = needed;
                } else if (pFw->ProviderSignature == 0x41435049) { // 'ACPI'
                    const uint32_t acpiTables[] = { 0x43495041, 0x50434146, 0x54455048, 0x54445353, 0x54445344 };
                    constexpr uint32_t needed = sizeof(acpiTables);
                    if (pFw->TableBufferLength < needed) {
                        pFw->TableBufferLength = needed;
                        if (ReturnLength) *ReturnLength = 16 + needed;
                        return NtStatus::BufferTooSmall;
                    }
                    std::memcpy(pFw->TableBuffer, acpiTables, needed);
                    pFw->TableBufferLength = needed;
                }
            } else if (pFw->Action == 1) { // Get
                if (pFw->ProviderSignature == 0x52534D42) { // 'RSMB'
                    static const uint8_t s_smbiosRaw[] = {
                        0x00, 0x03, 0x05, 0x35, 0x48, 0x00, 0x00, 0x00,
                        0x00, 0x1A, 0x01, 0x00, 0x01, 0x02, 0x00, 0xE8,
                        0x03, 0x09, 0x90, 0x90, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x05, 0xFF, 0xFF,
                        'M', 'i', 'c', 'a', 'N', 'T', 0x00, 0x00
                    };
                    uint32_t needed = static_cast<uint32_t>(sizeof(s_smbiosRaw));
                    if (pFw->TableBufferLength < needed) {
                        pFw->TableBufferLength = needed;
                        if (ReturnLength) *ReturnLength = 16 + needed;
                        return NtStatus::BufferTooSmall;
                    }
                    std::memcpy(pFw->TableBuffer, s_smbiosRaw, needed);
                    pFw->TableBufferLength = needed;
                }
            }
            if (ReturnLength) *ReturnLength = 16 + pFw->TableBufferLength;
            return NtStatus::Success;
        }
        case 103: { // SystemCodeIntegrityInformation
            if (SystemInformationLength < sizeof(SYSTEM_CODEINTEGRITY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODEINTEGRITY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CODEINTEGRITY_INFORMATION*>(SystemInformation);
            pInfo->Length = sizeof(SYSTEM_CODEINTEGRITY_INFORMATION);
            pInfo->CodeIntegrityOptions = CODEINTEGRITY_OPTION_ENABLED | CODEINTEGRITY_OPTION_HVCI_KMCI_ENABLED;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODEINTEGRITY_INFORMATION);
            return NtStatus::Success;
        }
        case 145: { // SystemSecureBootInformation
            if (SystemInformationLength < sizeof(SYSTEM_SECUREBOOT_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SECUREBOOT_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_SECUREBOOT_INFORMATION*>(SystemInformation);
            pInfo->SecureBootEnabled = 1;
            pInfo->SecureBootCapable = 1;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SECUREBOOT_INFORMATION);
            return NtStatus::Success;
        }
        case 159: { // SystemHypervisorInformation
            if (SystemInformationLength < sizeof(SYSTEM_HYPERVISOR_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_HYPERVISOR_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_HYPERVISOR_INFORMATION*>(SystemInformation);
            pInfo->HypervisorPresent = 0;
            pInfo->EnclaveFeaturePageAvailable = 1;
            pInfo->Reserved[0] = 0;
            pInfo->Reserved[1] = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_HYPERVISOR_INFORMATION);
            return NtStatus::Success;
        }
        case 42: { // SystemProcessorIdleCycleTimeInformation
            constexpr uint32_t processorCount = 16;
            size_t requiredLength = sizeof(uint64_t) * processorCount;
            if (SystemInformationLength < requiredLength) {
                if (ReturnLength) *ReturnLength = static_cast<ULONG>(requiredLength);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pCycles = reinterpret_cast<uint64_t*>(SystemInformation);
            for (uint32_t i = 0; i < processorCount; ++i) {
                pCycles[i] = 24500000000ULL + (static_cast<uint64_t>(i) * 1250000ULL);
            }
            if (ReturnLength) *ReturnLength = static_cast<ULONG>(requiredLength);
            return NtStatus::Success;
        }
        case 33: { // SystemExceptionInformation
            if (SystemInformationLength < sizeof(SYSTEM_EXCEPTION_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_EXCEPTION_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pEx = reinterpret_cast<SYSTEM_EXCEPTION_INFORMATION*>(SystemInformation);
            pEx->AlignmentFixupCount = 0;
            pEx->ExceptionDispatchCount = 142;
            pEx->FloatingEmulationCount = 0;
            pEx->ByteWordEmulationCount = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_EXCEPTION_INFORMATION);
            return NtStatus::Success;
        }
        case 108: { // SystemProcessorCycleTimeInformation
            constexpr uint32_t processorCount = 16;
            size_t requiredLength = sizeof(SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION) * processorCount;
            if (SystemInformationLength < requiredLength) {
                if (ReturnLength) *ReturnLength = static_cast<ULONG>(requiredLength);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pCycles = reinterpret_cast<SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION*>(SystemInformation);
            for (uint32_t i = 0; i < processorCount; ++i) {
                pCycles[i].CycleTime = 28800000000ULL + (static_cast<uint64_t>(i) * 1500000ULL);
            }
            if (ReturnLength) *ReturnLength = static_cast<ULONG>(requiredLength);
            return NtStatus::Success;
        }
        case 73: { // SystemLogicalProcessorInformationEx
            constexpr uint32_t entryCount = 20; // 16 cores + 2 caches + 2 numa nodes
            constexpr uint32_t entrySize = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
            constexpr size_t requiredLength = entryCount * entrySize;
            if (SystemInformationLength < requiredLength || !SystemInformation) {
                if (ReturnLength) *ReturnLength = static_cast<ULONG>(requiredLength);
                return NtStatus::InfoLengthMismatch;
            }
            auto* curr = reinterpret_cast<uint8_t*>(SystemInformation);
            std::memset(curr, 0, requiredLength);

            // 16 Cores (Cores 0-15)
            for (uint32_t i = 0; i < 16; ++i) {
                auto* pRec = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(curr);
                pRec->Relationship = RelationProcessorCore;
                pRec->Size = entrySize;
                pRec->Processor.Flags = (i < 8) ? 1 : 0; // SMT on P-cores
                pRec->Processor.EfficiencyClass = (i < 4) ? 2 : ((i < 8) ? 1 : 0); // 2 = 3D V-Cache P-core, 1 = P-core, 0 = E-core
                pRec->Processor.GroupCount = 1;
                pRec->Processor.GroupMasks[0] = (1ULL << i);
                curr += entrySize;
            }

            // 2 L3 Caches (CCD 0: 96MB 3D V-Cache, CCD 1: 32MB standard L3)
            {
                auto* pC1 = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(curr);
                pC1->Relationship = RelationCache;
                pC1->Size = entrySize;
                pC1->Cache.Level = 3;
                pC1->Cache.Associativity = 16;
                pC1->Cache.LineSize = 64;
                pC1->Cache.CacheSize = 96 * 1024 * 1024;
                pC1->Cache.Type = CacheUnified;
                pC1->Cache.GroupCount = 1;
                pC1->Cache.GroupMasks[0] = 0x00FF; // Cores 0-7
                curr += entrySize;

                auto* pC2 = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(curr);
                pC2->Relationship = RelationCache;
                pC2->Size = entrySize;
                pC2->Cache.Level = 3;
                pC2->Cache.Associativity = 16;
                pC2->Cache.LineSize = 64;
                pC2->Cache.CacheSize = 32 * 1024 * 1024;
                pC2->Cache.Type = CacheUnified;
                pC2->Cache.GroupCount = 1;
                pC2->Cache.GroupMasks[0] = 0xFF00; // Cores 8-15
                curr += entrySize;
            }

            // 2 NUMA Nodes (Node 0 & Node 1)
            {
                auto* pN0 = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(curr);
                pN0->Relationship = RelationNumaNode;
                pN0->Size = entrySize;
                pN0->NumaNode.NodeNumber = 0;
                pN0->NumaNode.GroupCount = 1;
                pN0->NumaNode.GroupMasks[0] = 0x00FF;
                curr += entrySize;

                auto* pN1 = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(curr);
                pN1->Relationship = RelationNumaNode;
                pN1->Size = entrySize;
                pN1->NumaNode.NodeNumber = 1;
                pN1->NumaNode.GroupCount = 1;
                pN1->NumaNode.GroupMasks[0] = 0xFF00;
                curr += entrySize;
            }

            if (ReturnLength) *ReturnLength = static_cast<ULONG>(requiredLength);
            return NtStatus::Success;
        }
        case 86: { // SystemNumaProcessorMap
            constexpr uint32_t nodeCount = 2;
            size_t requiredLength = sizeof(SYSTEM_NUMA_NODE_PROCESSOR_MAP) * nodeCount;
            if (SystemInformationLength < requiredLength) {
                if (ReturnLength) *ReturnLength = static_cast<ULONG>(requiredLength);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pMap = reinterpret_cast<SYSTEM_NUMA_NODE_PROCESSOR_MAP*>(SystemInformation);
            pMap[0].NodeNumber = 0;
            pMap[0].Reserved = 0;
            pMap[0].ProcessorMask = 0x00000000000000FFULL;
            pMap[1].NodeNumber = 1;
            pMap[1].Reserved = 0;
            pMap[1].ProcessorMask = 0x000000000000FF00ULL;
            if (ReturnLength) *ReturnLength = static_cast<ULONG>(requiredLength);
            return NtStatus::Success;
        }
        case 104: { // SystemCpuSetInformation
            constexpr uint32_t cpuCount = 16;
            constexpr uint32_t entrySize = sizeof(SYSTEM_CPU_SET_INFORMATION);
            constexpr size_t requiredLength = cpuCount * entrySize;
            if (SystemInformationLength < requiredLength || !SystemInformation) {
                if (ReturnLength) *ReturnLength = static_cast<ULONG>(requiredLength);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_SET_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, requiredLength);
            for (uint32_t i = 0; i < cpuCount; ++i) {
                pInfo[i].Size = entrySize;
                pInfo[i].Type = CpuSetInformation;
                pInfo[i].CpuSet.Id = 256 + i;
                pInfo[i].CpuSet.Group = 0;
                pInfo[i].CpuSet.LogicalProcessorIndex = static_cast<uint8_t>(i);
                pInfo[i].CpuSet.CoreIndex = static_cast<uint8_t>(i);
                pInfo[i].CpuSet.LastLevelCacheIndex = (i < 8) ? 0 : 1;
                pInfo[i].CpuSet.NumaNodeIndex = (i < 8) ? 0 : 1;
                pInfo[i].CpuSet.EfficiencyClass = (i < 4) ? 2 : ((i < 8) ? 1 : 0);
                pInfo[i].CpuSet.AllFlags = 0;
                pInfo[i].CpuSet.Reserved = 0;
                pInfo[i].CpuSet.AllocationTag = 0;
            }
            if (ReturnLength) *ReturnLength = static_cast<ULONG>(requiredLength);
            return NtStatus::Success;
        }
        case 87: { // SystemNumaDistanceInformation
            constexpr uint32_t distanceCount = 4;
            size_t requiredLength = sizeof(SYSTEM_NUMA_DISTANCE_INFORMATION) * distanceCount;
            if (SystemInformationLength < requiredLength) {
                if (ReturnLength) *ReturnLength = static_cast<ULONG>(requiredLength);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pDist = reinterpret_cast<SYSTEM_NUMA_DISTANCE_INFORMATION*>(SystemInformation);
            pDist[0] = { 0, 0, 10 };
            pDist[1] = { 0, 1, 20 };
            pDist[2] = { 1, 0, 20 };
            pDist[3] = { 1, 1, 10 };
            if (ReturnLength) *ReturnLength = static_cast<ULONG>(requiredLength);
            return NtStatus::Success;
        }
        case 102: { // SystemDynamicTimeZoneInformation
            if (SystemInformationLength < sizeof(RTL_DYNAMIC_TIME_ZONE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(RTL_DYNAMIC_TIME_ZONE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pTz = reinterpret_cast<RTL_DYNAMIC_TIME_ZONE_INFORMATION*>(SystemInformation);
            std::memset(pTz, 0, sizeof(*pTz));
            pTz->Bias = 480; // Pacific Time (UTC -8 hours)
            const wchar_t stdName[] = L"Pacific Standard Time";
            std::memcpy(pTz->StandardName, stdName, sizeof(stdName));
            pTz->StandardBias = 0;
            const wchar_t dltName[] = L"Pacific Daylight Time";
            std::memcpy(pTz->DaylightName, dltName, sizeof(dltName));
            pTz->DaylightBias = -60;
            const wchar_t keyName[] = L"Pacific Standard Time";
            std::memcpy(pTz->TimeZoneKeyName, keyName, sizeof(keyName));
            pTz->DynamicDaylightTimeDisabled = 0;
            if (ReturnLength) *ReturnLength = sizeof(RTL_DYNAMIC_TIME_ZONE_INFORMATION);
            return NtStatus::Success;
        }
        case 142: { // SystemTpmInformation
            if (SystemInformationLength < sizeof(SYSTEM_TPM_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_TPM_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_TPM_INFORMATION*>(SystemInformation);
            pInfo->Flags = TPM_FLAG_ENABLED |
                           TPM_FLAG_ACTIVATED |
                           TPM_FLAG_OWNED |
                           TPM_FLAG_SPEC_VERSION_2_0 |
                           TPM_FLAG_READY;
            pInfo->TpmId = 0x54504D32; // 'TPM2'
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_TPM_INFORMATION);
            return NtStatus::Success;
        }
        case 173: { // SystemCodeIntegrityPolicyInformation
            if (SystemInformationLength < sizeof(SYSTEM_CODEINTEGRITYPOLICY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODEINTEGRITYPOLICY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CODEINTEGRITYPOLICY_INFORMATION*>(SystemInformation);
            pInfo->Options = CODEINTEGRITYPOLICY_OPTION_ENABLED | 
                             CODEINTEGRITYPOLICY_OPTION_HVCI_ENFORCED | 
                             CODEINTEGRITYPOLICY_OPTION_TESTSIGNING_DISABLED |
                             CODEINTEGRITYPOLICY_OPTION_DEBUGGING_DISABLED;
            pInfo->HwId[0] = 0x4D494341; // 'MICA'
            pInfo->HwId[1] = 0x4E543031; // 'NT01'
            pInfo->HwId[2] = 0;
            pInfo->HwId[3] = 0;
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODEINTEGRITYPOLICY_INFORMATION);
            return NtStatus::Success;
        }
        case 197: { // SystemHypervisorSharedPageInformation
            if (SystemInformationLength < sizeof(SYSTEM_HYPERVISOR_SHARED_PAGE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_HYPERVISOR_SHARED_PAGE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_HYPERVISOR_SHARED_PAGE_INFORMATION*>(SystemInformation);
            pInfo->HypervisorSharedUserVa = reinterpret_cast<void*>(0x7FFE1000ULL);
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_HYPERVISOR_SHARED_PAGE_INFORMATION);
            return NtStatus::Success;
        }
        case 202: { // SystemDmaGuardPolicyInformation
            if (SystemInformationLength < sizeof(SYSTEM_DMA_GUARD_POLICY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_DMA_GUARD_POLICY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_DMA_GUARD_POLICY_INFORMATION*>(SystemInformation);
            pInfo->DmaGuardPolicyEnabled = 1;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_DMA_GUARD_POLICY_INFORMATION);
            return NtStatus::Success;
        }
        case 206: { // SystemSpeculationControlInformationEx
            if (SystemInformationLength < sizeof(SYSTEM_SPECULATION_CONTROL_INFORMATION_EX)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SPECULATION_CONTROL_INFORMATION_EX);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pSpecEx = reinterpret_cast<SYSTEM_SPECULATION_CONTROL_INFORMATION_EX*>(SystemInformation);
            pSpecEx->SpeculationControlFlagsEx.Flags = 0;
            pSpecEx->SpeculationControlFlagsEx.BpbEnabled = 1;
            pSpecEx->SpeculationControlFlagsEx.IbrsPresent = 1;
            pSpecEx->SpeculationControlFlagsEx.StibpPresent = 1;
            pSpecEx->SpeculationControlFlagsEx.SmepPresent = 1;
            pSpecEx->SpeculationControlFlagsEx.SpeculativeStoreBypassDisableAvailable = 1;
            pSpecEx->SpeculationControlFlagsEx.SpeculativeStoreBypassDisableSupported = 1;
            pSpecEx->SpeculationControlFlagsEx.IntelRsbFlushAvailable = 1;
            pSpecEx->SpeculationControlFlagsEx.BhbClearAvailable = 1;
            pSpecEx->SpeculationControlFlagsEx.BranchHistoryBarrierSupported = 1;

            pSpecEx->SpeculationControlFlagsEx2.Flags2 = 0;
            pSpecEx->SpeculationControlFlagsEx2.SbdrFlushAvailable = 1;
            pSpecEx->SpeculationControlFlagsEx2.FbsdpClearAvailable = 1;
            pSpecEx->SpeculationControlFlagsEx2.PsdpClearAvailable = 1;
            pSpecEx->SpeculationControlFlagsEx2.FbssClearAvailable = 1;

            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SPECULATION_CONTROL_INFORMATION_EX);
            return NtStatus::Success;
        }
        case 208: { // SystemProcessorMicrocodeUpdateInformation
            if (SystemInformationLength < sizeof(SYSTEM_PROCESSOR_MICROCODE_UPDATE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PROCESSOR_MICROCODE_UPDATE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pUcode = reinterpret_cast<SYSTEM_PROCESSOR_MICROCODE_UPDATE_INFORMATION*>(SystemInformation);
            pUcode->MicrocodeUpdateStatus = 0; // STATUS_SUCCESS
            pUcode->CurrentRevision = 0x000000F4;
            pUcode->MinimumRequiredRevision = 0x000000B0;
            pUcode->CpuVendor = 1; // GenuineIntel
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PROCESSOR_MICROCODE_UPDATE_INFORMATION);
            return NtStatus::Success;
        }
        case 213: { // SystemSecureSpeculationControlInformation
            if (SystemInformationLength < sizeof(SYSTEM_SECURE_SPECULATION_CONTROL_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SECURE_SPECULATION_CONTROL_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pSecSpec = reinterpret_cast<SYSTEM_SECURE_SPECULATION_CONTROL_INFORMATION*>(SystemInformation);
            pSecSpec->Flags = 0;
            pSecSpec->KvaShadowSupported = 1;
            pSecSpec->KvaShadowEnabled = 1;
            pSecSpec->KvaShadowUserGlobal = 0;
            pSecSpec->KvaShadowPcid = 1;
            pSecSpec->KvaShadowInvpcid = 1;
            pSecSpec->KvaShadowRequired = 1;
            pSecSpec->KvaShadowRequiredAvailable = 1;
            pSecSpec->IbrsPresent = 1;
            pSecSpec->IbrsEnabled = 1;
            pSecSpec->IbrsUserEnabled = 1;
            pSecSpec->RetpolinePresent = 1;
            pSecSpec->RetpolineEnabled = 1;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SECURE_SPECULATION_CONTROL_INFORMATION);
            return NtStatus::Success;
        }
        case 214: { // SystemSpacesBootInformation
            if (SystemInformationLength < sizeof(SYSTEM_SPACES_BOOT_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SPACES_BOOT_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pSpaces = reinterpret_cast<SYSTEM_SPACES_BOOT_INFORMATION*>(SystemInformation);
            pSpaces->BootFlags = 0x00000001; // SPACES_BOOT_FLAG_ENABLED
            pSpaces->SpacesFlags = 0x00000003; // SPACES_FLAG_TIERED_STORAGE | SPACES_FLAG_RESILIENT
            pSpaces->Reserved[0] = 0;
            pSpaces->Reserved[1] = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SPACES_BOOT_INFORMATION);
            return NtStatus::Success;
        }
        case 215: { // SystemFwRamdiskInformation
            if (SystemInformationLength < sizeof(SYSTEM_FIRMWARE_RAMDISK_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_FIRMWARE_RAMDISK_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pRamdisk = reinterpret_cast<SYSTEM_FIRMWARE_RAMDISK_INFORMATION*>(SystemInformation);
            pRamdisk->Version = 1;
            pRamdisk->BlockSize = 4096;
            pRamdisk->BaseAddress = 0x100000000ULL; // 4GB physical base
            pRamdisk->Size = 0x40000000ULL;        // 1GB RAM disk
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_FIRMWARE_RAMDISK_INFORMATION);
            return NtStatus::Success;
        }
        case 216: { // SystemWheaIpmiHardwareInformation
            if (SystemInformationLength < sizeof(SYSTEM_WHEA_IPMI_HARDWARE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_WHEA_IPMI_HARDWARE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pWhea = reinterpret_cast<SYSTEM_WHEA_IPMI_HARDWARE_INFORMATION*>(SystemInformation);
            pWhea->Version = 1;
            pWhea->IpmiInterfaceType = 1; // KCS (Keyboard Controller Style)
            pWhea->RecordId = 0x5748454153454C01ULL; // "WHEASEL\1"
            pWhea->EventType = 0x02; // Memory / Hardware diagnostic
            pWhea->DataLength = 16;
            std::memset(pWhea->Data, 0xAA, 16);
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_WHEA_IPMI_HARDWARE_INFORMATION);
            return NtStatus::Success;
        }
        case 217: { // SystemCodeIntegrityPolicyInformation (Class 217)
            if (SystemInformationLength < sizeof(SYSTEM_CODE_INTEGRITY_POLICY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODE_INTEGRITY_POLICY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CODE_INTEGRITY_POLICY_INFORMATION*>(SystemInformation);
            pInfo->Options = 0x00000001;
            pInfo->HwIdCount = 1;
            pInfo->PolicyFlags = 0x0000000000000001ULL;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODE_INTEGRITY_POLICY_INFORMATION);
            return NtStatus::Success;
        }
        case 218: { // SystemNumaProximityInformation (Class 218)
            if (SystemInformationLength < sizeof(SYSTEM_NUMA_PROXIMITY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_NUMA_PROXIMITY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_NUMA_PROXIMITY_INFORMATION*>(SystemInformation);
            pInfo->NodeNumber = 0;
            pInfo->ProximityId = 0x10;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_NUMA_PROXIMITY_INFORMATION);
            return NtStatus::Success;
        }
        case 219: { // SystemDefragInformation (Class 219)
            if (SystemInformationLength < sizeof(SYSTEM_DEFRAG_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_DEFRAG_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_DEFRAG_INFORMATION*>(SystemInformation);
            pInfo->Flags = 0x00000001; // Defrag online / SSD TRIM active
            pInfo->DefragPass = 1;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_DEFRAG_INFORMATION);
            return NtStatus::Success;
        }
        case 220: { // SystemVirtualizationInformation (Class 220)
            if (SystemInformationLength < sizeof(SYSTEM_VIRTUALIZATION_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_VIRTUALIZATION_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_VIRTUALIZATION_INFORMATION*>(SystemInformation);
            pInfo->VirtualizationFlags = 0x00000003; // HypervisorPresent | SvmCapable
            pInfo->HypervisorVendorId = 0x4143494D;   // 'MICA'
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_VIRTUALIZATION_INFORMATION);
            return NtStatus::Success;
        }
        case 221: { // SystemShadowStackInformation (Class 221)
            if (SystemInformationLength < sizeof(SYSTEM_SHADOW_STACK_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SHADOW_STACK_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_SHADOW_STACK_INFORMATION*>(SystemInformation);
            pInfo->Flags = 0x00000007; // CetCapable | UserCetAllowed | KernelCetEnabled
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SHADOW_STACK_INFORMATION);
            return NtStatus::Success;
        }
        case 222: { // SystemBuildVersionInformation (Class 222)
            if (SystemInformationLength < sizeof(SYSTEM_BUILD_VERSION_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_BUILD_VERSION_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_BUILD_VERSION_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->LayerNumber = 1;
            pInfo->LayerCount = 1;
            pInfo->OsMajorVersion = 10;
            pInfo->OsMinorVersion = 0;
            pInfo->NtBuildNumber = 26100;
            pInfo->NtBuildQfe = 1742;
            std::strncpy(reinterpret_cast<char*>(pInfo->LayerName), "BaseOS", sizeof(pInfo->LayerName) - 1);
            std::strncpy(reinterpret_cast<char*>(pInfo->NtBuildBranch), "ge_release", sizeof(pInfo->NtBuildBranch) - 1);
            std::strncpy(reinterpret_cast<char*>(pInfo->NtBuildLab), "26100.1.amd64fre.ge_release.240331-1435", sizeof(pInfo->NtBuildLab) - 1);
            std::strncpy(reinterpret_cast<char*>(pInfo->NtBuildStamp), "240331-1435", sizeof(pInfo->NtBuildStamp) - 1);
            std::strncpy(reinterpret_cast<char*>(pInfo->NtBuildArch), "amd64", sizeof(pInfo->NtBuildArch) - 1);
            pInfo->Flags = 0x00000001; // IsTopLevel
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_BUILD_VERSION_INFORMATION);
            return NtStatus::Success;
        }
        case 223: { // SystemPoolLimitInformation (Class 223)
            if (SystemInformationLength < sizeof(SYSTEM_POOL_LIMIT_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_POOL_LIMIT_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_POOL_LIMIT_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->PagedPoolLimit = 512 * 1024 * 1024ULL;
            pInfo->NonPagedPoolLimit = 256 * 1024 * 1024ULL;
            pInfo->PagedPoolSize = 64 * 1024 * 1024ULL;
            pInfo->NonPagedPoolSize = 32 * 1024 * 1024ULL;
            pInfo->PagedPoolWarningLimit = 80;
            pInfo->NonPagedPoolWarningLimit = 85;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_POOL_LIMIT_INFORMATION);
            return NtStatus::Success;
        }
        case 224: { // SystemCodeIntegrityAddDynamicStore (Class 224)
            if (SystemInformationLength < sizeof(SYSTEM_CODE_INTEGRITY_DYNAMIC_STORE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODE_INTEGRITY_DYNAMIC_STORE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CODE_INTEGRITY_DYNAMIC_STORE_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Flags = 0x00000001;
            pInfo->DynamicStoreId = 1001;
            pInfo->CatalogCount = 4;
            pInfo->HashAlgorithm = 0x800c; // CALG_SHA_256 / BCRYPT_SHA256
            pInfo->StoreGuid = { 0x6e2a86f1, 0x118b, 0x4f1b, { 0x93, 0xd0, 0x3d, 0x82, 0x61, 0x8c, 0xb7, 0x4e } };
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODE_INTEGRITY_DYNAMIC_STORE_INFORMATION);
            return NtStatus::Success;
        }
        case 225: { // SystemCodeIntegrityUnlockInformation (Class 225)
            if (SystemInformationLength < sizeof(SYSTEM_CODE_INTEGRITY_UNLOCK_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODE_INTEGRITY_UNLOCK_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CODE_INTEGRITY_UNLOCK_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->UnlockFlags = 0x00000001; // CI_UNLOCK_FLAG_TEST_SIGNING_PERMITTED
            pInfo->UnlockVersion = 1;
            pInfo->UnlockId = { 0x47e5b221, 0x8a23, 0x4f12, { 0x99, 0xb8, 0x1d, 0x3f, 0x5a, 0x7e, 0x91, 0x04 } };
            pInfo->UnlockSequenceNumber = 42;
            pInfo->Flags = 0x00000003; // Production test unlock active
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODE_INTEGRITY_UNLOCK_INFORMATION);
            return NtStatus::Success;
        }
        case 226: { // SystemLeapSecondInformation (Class 226)
            if (SystemInformationLength < sizeof(SYSTEM_LEAP_SECOND_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_LEAP_SECOND_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_LEAP_SECOND_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Enabled = 1; // Leap second adjustments enabled
            pInfo->Flags = 0x00000001; // Positive leap second transition
            pInfo->NextLeapSecond.QuadPart = 133500000000000000LL;     // Future leap second interval (100-ns units)
            pInfo->PreviousLeapSecond.QuadPart = 131276064000000000LL; // Past leap second interval (100-ns units)
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_LEAP_SECOND_INFORMATION);
            return NtStatus::Success;
        }
        case 227: { // SystemMemoryTopologyInformation (Class 227)
            if (SystemInformationLength < sizeof(SYSTEM_MEMORY_TOPOLOGY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_MEMORY_TOPOLOGY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_MEMORY_TOPOLOGY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->ChannelCount = 2; // Dual-channel
            pInfo->DimmCount = 2;    // 2 DIMMs
            pInfo->BusWidthBits = 128; // 128-bit bus width
            pInfo->MemorySpeedMTs = 6000; // DDR5-6000
            pInfo->BandwidthGBs = 96; // 96 GB/s bandwidth
            pInfo->AverageLatencyNs = 45; // 45 ns latency
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_MEMORY_TOPOLOGY_INFORMATION);
            return NtStatus::Success;
        }
        case 228: { // SystemDpcWatchdogInformation (Class 228)
            if (SystemInformationLength < sizeof(SYSTEM_DPC_WATCHDOG_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_DPC_WATCHDOG_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_DPC_WATCHDOG_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->DpcTimeLimitTicks = 50000; // 50,000 clock ticks limit
            pInfo->DpcWatchdogSequence = 1;
            pInfo->TotalDpcTime = 120; // 120 ticks accumulated
            pInfo->TotalInterruptTime = 45; // 45 ticks interrupt processing
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_DPC_WATCHDOG_INFORMATION);
            return NtStatus::Success;
        }
        case 229: { // SystemPoolZeroingInformation (Class 229)
            if (SystemInformationLength < sizeof(SYSTEM_POOL_ZEROING_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_POOL_ZEROING_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_POOL_ZEROING_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->PoolZeroingSupported = 1;
            pInfo->PoolZeroingEnabled = 1;
            pInfo->Reserved = 0;
            pInfo->ZeroedPagesCount = 1048576; // 1M zeroed physical pages (4 GB)
            pInfo->TotalAllocationsZeroed = 409600;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_POOL_ZEROING_INFORMATION);
            return NtStatus::Success;
        }
        case 230: { // SystemCodeIntegrityPlatformManifestInformation (Class 230)
            if (SystemInformationLength < sizeof(SYSTEM_CODE_INTEGRITY_PLATFORM_MANIFEST_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODE_INTEGRITY_PLATFORM_MANIFEST_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CODE_INTEGRITY_PLATFORM_MANIFEST_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->PlatformManifestPresent = 1;
            pInfo->ManifestHashType = 1; // SHA-256
            pInfo->ManifestCount = 2;
            pInfo->PlatformPolicyVersion = 0x00010002;
            std::memcpy(pInfo->PlatformId, "MICANT-PLAT-001\0", 16);
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODE_INTEGRITY_PLATFORM_MANIFEST_INFORMATION);
            return NtStatus::Success;
        }
        case 231: { // SystemInterruptCpuSetsInformation (Class 231)
            if (SystemInformationLength < sizeof(SYSTEM_INTERRUPT_CPU_SET_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_INTERRUPT_CPU_SET_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_INTERRUPT_CPU_SET_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->NumberOfCpuSets = 4;
            pInfo->ActiveCpuSetMask = 0x0000000F; // 4 cores active
            pInfo->DpcCpuSetMask = 0x00000003;    // Cores 0 and 1 steer DPCs
            pInfo->Flags = 0x00000001;            // VSM / real-time affinity steering active
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_INTERRUPT_CPU_SET_INFORMATION);
            return NtStatus::Success;
        }
        case 232: { // SystemSecureKernelProfileInformation (Class 232)
            if (SystemInformationLength < sizeof(SYSTEM_SECURE_KERNEL_PROFILE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SECURE_KERNEL_PROFILE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_SECURE_KERNEL_PROFILE_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->TotalPages = 131072;     // 512 MB VSM secure space
            pInfo->ActivePages = 32768;     // 128 MB active enclave allocations
            pInfo->VsmEnclaveCount = 2;     // Credential Guard & Isolated User Mode
            pInfo->Flags = 0x00000007;      // VSM_ENABLED | ENCLAVES_ACTIVE | ISOLATION_GUARD
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SECURE_KERNEL_PROFILE_INFORMATION);
            return NtStatus::Success;
        }
        case 233: { // SystemCodeIntegritySyntheticCacheInformation (Class 233)
            if (SystemInformationLength < sizeof(SYSTEM_CODE_INTEGRITY_SYNTHETIC_CACHE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODE_INTEGRITY_SYNTHETIC_CACHE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CODE_INTEGRITY_SYNTHETIC_CACHE_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->SyntheticCacheFlags = 0x00000003; // SYNTHETIC_CACHE_ENABLED | SYNTHETIC_CACHE_VERIFIED
            pInfo->CacheEntryCount = 1024;
            pInfo->TotalCachedBytes = 16777216;     // 16 MB
            pInfo->HitCount = 8192;
            pInfo->MissCount = 64;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CODE_INTEGRITY_SYNTHETIC_CACHE_INFORMATION);
            return NtStatus::Success;
        }
        case 234: { // SystemFirmwareRamdumpInformation (Class 234)
            if (SystemInformationLength < sizeof(SYSTEM_FIRMWARE_RAMDUMP_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_FIRMWARE_RAMDUMP_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_FIRMWARE_RAMDUMP_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->RamdumpSupported = 1;
            pInfo->RamdumpEnabled = 1;
            pInfo->Reserved = 0;
            pInfo->RamdumpBufferSize = 67108864ULL;       // 64 MB
            pInfo->RamdumpPhysicalAddress = 0x1E0000000ULL; // Physical RAM dump buffer base
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_FIRMWARE_RAMDUMP_INFORMATION);
            return NtStatus::Success;
        }
        case 235: { // SystemPhysicalMemoryZeroingInformation (Class 235)
            if (SystemInformationLength < sizeof(SYSTEM_PHYSICAL_MEMORY_ZEROING_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PHYSICAL_MEMORY_ZEROING_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PHYSICAL_MEMORY_ZEROING_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->TotalPhysicalBytesZeroed = 34359738368ULL; // 32 GB zeroed
            pInfo->PendingZeroingBytes = 1073741824ULL;       // 1 GB pending
            pInfo->ActiveZeroingWorkerThreads = 4;
            pInfo->ZeroingEfficiencyRating = 98;              // 98% efficiency
            pInfo->Flags = 0x00000003;                        // ZEROING_ENABLED | ZEROING_HW_ACCELERATED
            pInfo->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PHYSICAL_MEMORY_ZEROING_INFORMATION);
            return NtStatus::Success;
        }
        case 236: { // SystemHypervisorProcessorPerformanceInformation (Class 236)
            if (SystemInformationLength < sizeof(SYSTEM_HYPERVISOR_PROCESSOR_PERFORMANCE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_HYPERVISOR_PROCESSOR_PERFORMANCE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_HYPERVISOR_PROCESSOR_PERFORMANCE_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->ProcessorIndex = 0;
            pInfo->HypervisorEnlightenmentFlags = 0x00000005; // HYPERVISOR_ENLIGHTENED | HYPERVISOR_PV_SPINLOCK
            pInfo->GuestKernelTime100ns = 150000000ULL;
            pInfo->GuestUserTime100ns = 80000000ULL;
            pInfo->HypervisorOverheadTime100ns = 250000ULL;
            pInfo->HypercallCount = 16384;
            pInfo->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_HYPERVISOR_PROCESSOR_PERFORMANCE_INFORMATION);
            return NtStatus::Success;
        }
        case 237: { // SystemNumaProximityInformation (Class 237 / Extended)
            if (SystemInformationLength < sizeof(SYSTEM_NUMA_PROXIMITY_INFORMATION_EX)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_NUMA_PROXIMITY_INFORMATION_EX);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_NUMA_PROXIMITY_INFORMATION_EX*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->NodeId = 0;
            pInfo->ProximityId = 100;
            pInfo->Flags = 0x00000001; // NUMA_NODE_ONLINE
            pInfo->DistanceRatio = 10; // Local node latency baseline (1.0x)
            pInfo->TotalMemoryBytes = 17179869184ULL;     // 16 GB node memory
            pInfo->AvailableMemoryBytes = 15032385536ULL; // ~14 GB available
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_NUMA_PROXIMITY_INFORMATION_EX);
            return NtStatus::Success;
        }
        case 238: { // SystemEntropyPoolInformation (Class 238)
            if (SystemInformationLength < sizeof(SYSTEM_ENTROPY_POOL_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_ENTROPY_POOL_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_ENTROPY_POOL_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->PoolCapacityBits = 4096;
            pInfo->CurrentEntropyBits = 4096;
            pInfo->ReseedCount = 128;
            pInfo->JitterEntropyStatus = 1; // ACTIVE
            pInfo->HardwareRngHealth = 1;   // HEALTHY (RDRAND/RDSEED verified)
            pInfo->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_ENTROPY_POOL_INFORMATION);
            return NtStatus::Success;
        }
        case 239: { // SystemProcessorMicrocodeInformation (Class 239)
            if (SystemInformationLength < sizeof(SYSTEM_PROCESSOR_MICROCODE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PROCESSOR_MICROCODE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PROCESSOR_MICROCODE_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->ProcessorIndex = 0;
            pInfo->CurrentMicrocodeRevision = 0x000000F4;
            pInfo->MinimumSupportedRevision = 0x000000A0;
            pInfo->Flags = 0x00000003; // MICROCODE_UPDATE_APPLIED | MICROCODE_MITIGATIONS_ACTIVE
            pInfo->MicrocodeReleaseDate = 20261105ULL;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PROCESSOR_MICROCODE_INFORMATION);
            return NtStatus::Success;
        }
        case 240: { // SystemFirmwareSecureBootPolicyInformation (Class 240)
            if (SystemInformationLength < sizeof(SYSTEM_FIRMWARE_SECUREBOOT_POLICY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_FIRMWARE_SECUREBOOT_POLICY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_FIRMWARE_SECUREBOOT_POLICY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->SecureBootEnabled = 1;
            pInfo->SecureBootConfigured = 1;
            pInfo->PolicyPublisher = 0x534F5652; // 'SOVR' (Sovereign Root)
            pInfo->PolicyVersion = 0x00020001;   // Policy v2.1
            pInfo->Flags = 0x00000001;           // SECUREBOOT_POLICY_LOCKED
            pInfo->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_FIRMWARE_SECUREBOOT_POLICY_INFORMATION);
            return NtStatus::Success;
        }
        case 241: { // SystemRootSiloInformation (Class 241)
            if (SystemInformationLength < sizeof(SYSTEM_ROOT_SILO_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_ROOT_SILO_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_ROOT_SILO_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->NumberOfRootSilos = 1;
            pInfo->ActiveSilosCount = 1;
            pInfo->SiloFlags = 0x00000001; // SILO_FLAG_SERVER_CORE
            pInfo->MaxSiloDepth = 4;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_ROOT_SILO_INFORMATION);
            return NtStatus::Success;
        }
        case 242: { // SystemMemoryPartitionInformation (Class 242)
            if (SystemInformationLength < sizeof(SYSTEM_MEMORY_PARTITION_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_MEMORY_PARTITION_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_MEMORY_PARTITION_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->PartitionCount = 2;
            pInfo->ActivePartitions = 2;
            pInfo->TotalPartitionMemoryBytes = 64ULL * 1024 * 1024 * 1024;
            pInfo->AvailablePartitionMemoryBytes = 48ULL * 1024 * 1024 * 1024;
            pInfo->NumaNodesCovered = 2;
            pInfo->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_MEMORY_PARTITION_INFORMATION);
            return NtStatus::Success;
        }
        case 243: { // SystemSiloMemoryInformation (Class 243)
            if (SystemInformationLength < sizeof(SYSTEM_SILO_MEMORY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SILO_MEMORY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_SILO_MEMORY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->SiloId = 1;
            pInfo->SiloFlags = 0x00000003; // SILO_ACTIVE | SILO_MEMORY_ENFORCED
            pInfo->CommittedMemoryBytes = 4ULL * 1024 * 1024 * 1024;    // 4 GB
            pInfo->CommitLimitMemoryBytes = 16ULL * 1024 * 1024 * 1024; // 16 GB
            pInfo->WorkingSetBytes = 2ULL * 1024 * 1024 * 1024;        // 2 GB
            pInfo->PeakWorkingSetBytes = 3ULL * 1024 * 1024 * 1024;    // 3 GB
            pInfo->SharedMemoryBytes = 512ULL * 1024 * 1024;           // 512 MB
            pInfo->ProcessCount = 12;
            pInfo->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SILO_MEMORY_INFORMATION);
            return NtStatus::Success;
        }
        case 244: { // SystemMemoryTopologyExInformation (Class 244)
            if (SystemInformationLength < sizeof(SYSTEM_MEMORY_TOPOLOGY_EX_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_MEMORY_TOPOLOGY_EX_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_MEMORY_TOPOLOGY_EX_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->NumaNodeCount = 2;
            pInfo->ChannelCount = 4;
            pInfo->DimmSlotCount = 8;
            pInfo->PopulatedDimmCount = 4;
            pInfo->TotalPhysicalBytes = 64ULL * 1024 * 1024 * 1024;
            pInfo->AvailablePhysicalBytes = 48ULL * 1024 * 1024 * 1024;
            pInfo->InterleaveWays = 4;
            pInfo->MemorySpeedMhz = 5600; // DDR5-5600
            pInfo->MemoryType = 0x1E;     // DDR5
            pInfo->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_MEMORY_TOPOLOGY_EX_INFORMATION);
            return NtStatus::Success;
        }
        case 245: { // SystemSiloRootDirectoryInformation (Class 245)
            if (SystemInformationLength < sizeof(SYSTEM_SILO_ROOT_DIRECTORY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SILO_ROOT_DIRECTORY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_SILO_ROOT_DIRECTORY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->SiloId = 1;
            pInfo->Reserved = 0;
            pInfo->RootDirectoryHandle = 0x1000CAFE;
            const wchar_t path[] = L"\\Device\\HarddiskVolume1\\Silos\\Silo1";
            pInfo->DirectoryPathLength = static_cast<ULONG>(std::wcslen(path));
            std::wcsncpy(pInfo->DirectoryPath, path, 259);
            pInfo->DirectoryPath[259] = L'\0';
            pInfo->Flags = 0x00000007; // SILO_ROOT_MOUNTED | SILO_ROOT_ISOLATED | SILO_ROOT_PERSISTENT
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_SILO_ROOT_DIRECTORY_INFORMATION);
            return NtStatus::Success;
        }
        case 246: { // SystemPhysicalMemoryBandwidthInformation (Class 246)
            if (SystemInformationLength < sizeof(SYSTEM_PHYSICAL_MEMORY_BANDWIDTH_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PHYSICAL_MEMORY_BANDWIDTH_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PHYSICAL_MEMORY_BANDWIDTH_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->NumaNode = 0;
            pInfo->ChannelCount = 8;
            pInfo->TotalReadBandwidthMBs = 204800; // 200 GB/s
            pInfo->TotalWriteBandwidthMBs = 102400; // 100 GB/s
            pInfo->PeakBandwidthMBs = 307200; // 300 GB/s
            pInfo->ThrottledChannels = 0;
            pInfo->BandwidthEfficiencyPercent = 95;
            pInfo->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PHYSICAL_MEMORY_BANDWIDTH_INFORMATION);
            return NtStatus::Success;
        }
        case 247: { // SystemMemoryPowerStateInformation (Class 247)
            if (SystemInformationLength < sizeof(SYSTEM_MEMORY_POWER_STATE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_MEMORY_POWER_STATE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_MEMORY_POWER_STATE_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->NumaNode = 0;
            pInfo->PowerState = 0; // Active / Performance
            pInfo->ActivePowerMilliwatts = 14500; // 14.5W
            pInfo->StandbyPowerMilliwatts = 1800;  // 1.8W
            pInfo->ThermalThrottlingLimitDegreesC = 85;
            pInfo->CurrentTemperatureDegreesC = 42;
            pInfo->LowPowerTransitionsCount = 12;
            pInfo->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_MEMORY_POWER_STATE_INFORMATION);
            return NtStatus::Success;
        }
        case 248: { // SystemCpuExecutionCharacteristicsInformation (Class 248)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_EXECUTION_CHARACTERISTICS_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_EXECUTION_CHARACTERISTICS_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_EXECUTION_CHARACTERISTICS_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->ProcessorIndex = 0;
            pInfo->ExecutionEfficiencyClass = 1; // Performance core
            pInfo->InstructionIssueWidth = 8;    // 8-wide decode/issue
            pInfo->MicrocodeRevision = 0x0000012A;
            pInfo->HardwarePrefetcherFlags = 0x000000000000000F; // L1/L2 stream prefetchers enabled
            pInfo->BranchPredictorTier = 3;      // Advanced TAGE branch predictor
            pInfo->SpeculativeExecutionFeatures = 0x0000001F; // CET, TSX, IBRS, SSBD, STIBP
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_EXECUTION_CHARACTERISTICS_INFORMATION);
            return NtStatus::Success;
        }
        case 249: { // SystemPagefilePowerEfficiencyInformation (Class 249)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_POWER_EFFICIENCY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_POWER_EFFICIENCY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_POWER_EFFICIENCY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->PagefileIndex = 0;
            pInfo->Flags = 0x00000007; // PAGEFILE_ACTIVE | PAGEFILE_COMPRESSION_ENABLED | PAGEFILE_TIERED_STORAGE
            pInfo->TotalAllocatedBytes = 16ULL * 1024 * 1024 * 1024; // 16 GB
            pInfo->ActiveWrittenBytes = 2ULL * 1024 * 1024 * 1024;   // 2 GB
            pInfo->PowerSavedJoules = 450000ULL;                     // 450 kJ
            pInfo->CompressionRatioPercent = 250;                    // 2.5x
            pInfo->IoPowerState = 0;                                 // Autonomous Active/High Performance
            pInfo->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_POWER_EFFICIENCY_INFORMATION);
            return NtStatus::Success;
        }
        case 250: { // SystemCpuCooperativeAcceleratorInformation (Class 250)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_COOPERATIVE_ACCELERATOR_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_COOPERATIVE_ACCELERATOR_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_COOPERATIVE_ACCELERATOR_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->ProcessorIndex = 0;
            pInfo->AcceleratorType = 1;         // Matrix / AMX Core Accelerator
            pInfo->MaxTileDimensions = 2048;    // 2048x2048
            pInfo->SupportedDataTypes = 0x007F; // FP16, BF16, FP8, INT8, FP4, MXFP6, MXFP4
            pInfo->OperationsPerSecondPeak = 1250000000000ULL; // 1.25 TFLOPS
            pInfo->QueueAffinityMask = 0x0000003F;             // 6 Queues
            pInfo->CooperativeDispatchTier = 6;                // Tier 6 Work Graph Acceleration
            pInfo->Reserved = 0;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_COOPERATIVE_ACCELERATOR_INFORMATION);
            return NtStatus::Success;
        }
        case 251: { // SystemPagefileCompressionTelemetryInformation (Class 251)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_COMPRESSION_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_COMPRESSION_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_COMPRESSION_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->PagefileIndex = 0;
            pInfo->Flags = 0x00000007; // Active | CompressionActive | HardwareAccelerated
            pInfo->TotalPagesCompressed = 524288ULL;
            pInfo->TotalPagesDecompressed = 262144ULL;
            pInfo->UncompressedBytes = 2147483648ULL; // 2 GB
            pInfo->CompressedBytes = 751619276ULL;   // ~716 MB
            pInfo->CompressionRatioPercent = 285;    // 2.85x
            pInfo->AverageCompressionLatencyNanos = 1250;
            pInfo->CompressionAlgorithm = 3;         // Zstandard-NT Sovereign Engine
            pInfo->HardwareAcceleratorActive = 1;
            pInfo->HardwareAcceleratedPages = 400000ULL;
            pInfo->BypassedPagesDueToIncompressibility = 12000ULL;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_COMPRESSION_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 252: { // SystemCpuCooperativeFabricInformation (Class 252)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_COOPERATIVE_FABRIC_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_COOPERATIVE_FABRIC_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_COOPERATIVE_FABRIC_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->TotalFabricNodes = 16;
            pInfo->ActiveFabricInterconnects = 32;
            pInfo->FabricBandwidthGBps = 2048ULL;
            pInfo->CoherencyProtocolVersion = 0x0302; // Coherent v3.2
            pInfo->LatencyNanosecondsAverage = 45;
            pInfo->CacheCoherentAcceleratorCount = 8;
            pInfo->TotalFabricTransactions = 10000000000ULL;
            pInfo->InterNodeRetries = 12;
            pInfo->ThermalThrottleStatus = 0;        // Nominal
            pInfo->FabricFlags = 0x00000007;         // DynamicTopology | SubByteQuantRouting | CoherentPipelining
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_COOPERATIVE_FABRIC_INFORMATION);
            return NtStatus::Success;
        }
        case 253: { // SystemPagefileDecompressionTelemetryInformation (Class 253)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_DECOMPRESSION_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_DECOMPRESSION_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_DECOMPRESSION_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->PagefileIndex = 0;
            pInfo->Flags = 0x07;                      // Active | DecompressionActive | HardwareDecompressionActive
            pInfo->TotalPagesDecompressed = 1048576ULL;
            pInfo->HardwareDecompressedPages = 850000ULL;
            pInfo->SoftwareFallbackPages = 198576ULL;
            pInfo->CompressedBytesRead = 1503238552ULL;
            pInfo->DecompressedBytesProduced = 4294967296ULL;
            pInfo->AverageDecompressionLatencyNanos = 420;
            pInfo->DecompressionThroughputMBps = 12500;
            pInfo->DecompressionAlgorithm = 3;        // Zstandard-NT Sovereign Engine
            pInfo->IntegrityVerificationPasses = 1048576ULL;
            pInfo->DecompressedCacheHitRatioPercent = 94;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_DECOMPRESSION_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 254: { // SystemCpuFabricPartitionInformation (Class 254)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_PARTITION_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_PARTITION_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_PARTITION_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->TotalFabricPartitions = 4;
            pInfo->ActivePartitionId = 0;
            pInfo->PartitionAffinityMask = 0x00000000FFFFFFFFULL;
            pInfo->FabricBandwidthAllocationPercent = 75;
            pInfo->LowPriorityThrottleRate = 10;
            pInfo->FabricCongestionEvents = 5;
            pInfo->CrossPartitionInvalidations = 2500ULL;
            pInfo->PartitionIsolationLevel = 3;       // Sovereign Isolated
            pInfo->PartitionFlags = 0x0F;             // Coherent | QoSGuaranteed | SvmBound | EncryptedInterconnect
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_PARTITION_INFORMATION);
            return NtStatus::Success;
        }
        case 255: { // SystemPagefileClusterCompactionTelemetryInformation (Class 255)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_CLUSTER_COMPACTION_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_CLUSTER_COMPACTION_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_CLUSTER_COMPACTION_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->PagefileIndex = 0;
            pInfo->Flags = 0x07; // Active | CompactionActive | HardwareAssisted
            pInfo->TotalCompactionPasses = 1024ULL;
            pInfo->FragmentedClustersReclaimed = 524288ULL;
            pInfo->ContiguousClustersCreated = 131072ULL;
            pInfo->PagefileDiskSpaceFreedBytes = 2147483648ULL; // 2 GB
            pInfo->AverageCompactionLatencyNanos = 850;
            pInfo->CompactionThroughputMBps = 15000;
            pInfo->FragmentationRatioBeforePercent = 68;
            pInfo->FragmentationRatioAfterPercent = 12;
            pInfo->CompactionAlgorithm = 3; // Sovereign-Compactor-NT
            pInfo->CompactionEfficiencyRating = 96;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_CLUSTER_COMPACTION_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 256: { // SystemCpuFabricQosTelemetryInformation (Class 256)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_QOS_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_QOS_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_QOS_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->TotalQosQueues = 16;
            pInfo->ActiveQosDomainId = 0;
            pInfo->HighPriorityBandwidthDeliveredGBps = 1536ULL;
            pInfo->BestEffortBandwidthDeliveredGBps = 512ULL;
            pInfo->HighPriorityP99LatencyNanos = 18;
            pInfo->BestEffortP99LatencyNanos = 64;
            pInfo->QosSlaViolations = 0;
            pInfo->CongestionMitigationEvents = 3;
            pInfo->CreditStarvationEvents = 0;
            pInfo->ThermalBudgetUsagePercent = 72;
            pInfo->QosFlags = 0x0F; // PriorityQueuing | DynamicCreditReallocation | HardwareRateLimiting | SlaGuaranteed
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_QOS_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 257: { // SystemPagefileClusterDefragmentationTelemetryInformation (Class 257)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_CLUSTER_DEFRAGMENTATION_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_CLUSTER_DEFRAGMENTATION_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_CLUSTER_DEFRAGMENTATION_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->PagefileIndex = 0;
            pInfo->Flags = 0x0F; // Active | DefragRunning | ZeroCopyRelocation | HardwareDmaAccelerated
            pInfo->TotalDefragPasses = 512ULL;
            pInfo->ClustersRelocated = 1048576ULL;
            pInfo->ContinuousFreeExtentsFormed = 65536ULL;
            pInfo->PagefileSparsityFreedBytes = 4294967296ULL; // 4 GB
            pInfo->AverageRelocationLatencyNanos = 420;
            pInfo->DefragThroughputMBps = 24000;
            pInfo->PreDefragFragmentationIndex = 74;
            pInfo->PostDefragFragmentationIndex = 5;
            pInfo->DefragEngineVersion = 1; // MicaNT-NativeDefrag-v1
            pInfo->OptimizationScore = 98;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_CLUSTER_DEFRAGMENTATION_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 258: { // SystemCpuFabricLatencyTelemetryInformation (Class 258)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_LATENCY_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_LATENCY_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_LATENCY_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->FabricNodeId = 0;
            pInfo->InterconnectHopCount = 2;
            pInfo->TotalTransactionsSampled = 1000000000ULL;
            pInfo->MinHopLatencyNanos = 8;
            pInfo->AverageHopLatencyNanos = 14;
            pInfo->MaxHopLatencyNanos = 45;
            pInfo->P99HopLatencyNanos = 21;
            pInfo->LinkSaturationPercent = 48;
            pInfo->RetransmitRatePPM = 2;
            pInfo->CrosstalkMitigationEvents = 0;
            pInfo->FabricStatusFlags = 0x07; // Operational | LowJitter | DynamicRoutingActive
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_LATENCY_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 259: { // SystemPagefileExtentMigrationTelemetryInformation (Class 259)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_EXTENT_MIGRATION_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_EXTENT_MIGRATION_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_EXTENT_MIGRATION_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->PagefileId = 0;
            pInfo->TotalExtentsMigrated = 65536ULL;
            pInfo->TotalBytesMigrated = 17179869184ULL;
            pInfo->HotToWarmMigrations = 40960;
            pInfo->WarmToColdMigrations = 24576;
            pInfo->ColdToHotPromotions = 8192;
            pInfo->AverageMigrationLatencyMicros = 125;
            pInfo->MigrationThroughputMBps = 32000;
            pInfo->CompressionSavingsPercent = 45;
            pInfo->ExtentIntegrityFailures = 0;
            pInfo->StatusFlags = 0x07; // MigrationActive | ZeroCopySupported | DynamicTieringEnabled
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_EXTENT_MIGRATION_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 260: { // SystemCpuFabricCongestionTelemetryInformation (Class 260)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_CONGESTION_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_CONGESTION_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_CONGESTION_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->FabricId = 0;
            pInfo->CongestionDomainCount = 8;
            pInfo->TotalCreditsIssued = 5000000000ULL;
            pInfo->TotalCreditsStalled = 12000ULL;
            pInfo->HeadOfLineBlockingEvents = 15;
            pInfo->ThrottlingEpisodes = 4;
            pInfo->BufferOverflowDropCount = 0;
            pInfo->AverageBufferOccupancyPercent = 35;
            pInfo->PeakBufferOccupancyPercent = 68;
            pInfo->AdaptiveRoutingDiversions = 250;
            pInfo->FabricCongestionStatus = 0x05; // CongestionManagementActive | FlowControlEnabled
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_CONGESTION_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 261: { // SystemPagefileTierExpulsionTelemetryInformation (Class 261)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_EXPULSION_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_EXPULSION_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_EXPULSION_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->PagefileId = 0;
            pInfo->TotalExpulsions = 16384ULL;
            pInfo->BytesExpelled = 4294967296ULL;
            pInfo->ColdToPurgeExpulsions = 8192ULL;
            pInfo->AverageExpulsionLatencyMicros = 120ULL;
            pInfo->ExpulsionThroughputMBps = 3200ULL;
            pInfo->StatusFlags = 0x07; // ExpulsionActive | AutoTuningEnabled | PurgeCoalescingActive
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_EXPULSION_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 262: { // SystemCpuFabricBandwidthPartitionTelemetryInformation (Class 262)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_BANDWIDTH_PARTITION_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_BANDWIDTH_PARTITION_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_BANDWIDTH_PARTITION_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->FabricId = 0;
            pInfo->PartitionCount = 8;
            pInfo->GuaranteedBandwidthMBps = 128000ULL;
            pInfo->BurstBandwidthMBps = 256000ULL;
            pInfo->PartitionThrottlingCount = 18ULL;
            pInfo->StatusFlags = 0x05; // PartitioningEnforced | QosGuaranteed
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_BANDWIDTH_PARTITION_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 263: { // SystemPagefileTierIoQueueTelemetryInformation (Class 263)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_IO_QUEUE_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_IO_QUEUE_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_IO_QUEUE_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->TierId = 0;
            pInfo->QueueDepth = 12;
            pInfo->HighWatermarkQueueDepth = 64;
            pInfo->CompletedIoCount = 1048576;
            pInfo->PendingIoCount = 4;
            pInfo->ThrottledIoCount = 0;
            pInfo->ReorderedIoCount = 128;
            pInfo->AverageServiceTimeNs = 450;
            pInfo->TotalBytesServiced = 42949672960ULL;
            pInfo->TotalStallDurationNs = 1200;
            pInfo->StatusFlags = 0x07; // QueueActive | AsyncPipeliningEnabled | AutoPriorityCoalescing
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_IO_QUEUE_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 264: { // SystemCpuFabricPowerStateTelemetryInformation (Class 264)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_POWER_STATE_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_POWER_STATE_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_POWER_STATE_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->FabricId = 0;
            pInfo->CurrentPowerState = 1;
            pInfo->TargetPowerState = 1;
            pInfo->TransitionCount = 420;
            pInfo->TotalActiveDurationUs = 850000;
            pInfo->TotalIdleDurationUs = 150000;
            pInfo->VoltageMv = 950;
            pInfo->FrequencyMhz = 4200;
            pInfo->EnergyJoulesMicro = 55000000ULL;
            pInfo->ThrottleDurationNs = 0;
            pInfo->StatusFlags = 0x07; // AutonomousPowerManagement | RapidTransitionEnacted | VoltageGatingActive
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_POWER_STATE_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 265: { // SystemPagefileTierWearLevelingTelemetryInformation (Class 265)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_WEAR_LEVELING_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_WEAR_LEVELING_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_WEAR_LEVELING_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->TierId = 0;
            pInfo->TotalBlocks = 1048576;
            pInfo->FreeBlocks = 524288;
            pInfo->EraseCountMin = 12;
            pInfo->EraseCountMax = 48;
            pInfo->EraseCountAverage = 26;
            pInfo->BadBlockCount = 2;
            pInfo->SpareBlocksRemaining = 4096;
            pInfo->LifetimeBytesWritten = 1099511627776ULL;
            pInfo->ProjectedRemainingLifespanHours = 87600ULL;
            pInfo->StatusFlags = 0x07; // WearLevelingActive | DynamicBlockRemapping | EnduranceBudgetOptimal
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_WEAR_LEVELING_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 266: { // SystemCpuFabricVoltageDomainTelemetryInformation (Class 266)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_DOMAIN_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_DOMAIN_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_VOLTAGE_DOMAIN_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->DomainId = 0;
            pInfo->RailVoltageMv = 1050;
            pInfo->MinimumVoltageMv = 750;
            pInfo->MaximumVoltageMv = 1350;
            pInfo->TargetVoltageMv = 1050;
            pInfo->SlewRateMvPerUs = 25;
            pInfo->DroopCompensations = 128;
            pInfo->VoltageRippleUv = 4500;
            pInfo->VoltageSampleCount = 1000000ULL;
            pInfo->OutOfToleranceDurationNs = 0ULL;
            pInfo->StatusFlags = 0x07; // DomainRegulated | TelemetryOnline | FastTransientResponseActive
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_DOMAIN_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 267: { // SystemPagefileTierFailoverTelemetryInformation (Class 267)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_FAILOVER_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_FAILOVER_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_FAILOVER_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->PrimaryTierId = 0;
            pInfo->SecondaryTierId = 1;
            pInfo->FailoverEventsTotal = 3;
            pInfo->FailoverRetries = 0;
            pInfo->PagesMigratedOnFailover = 65536ULL;
            pInfo->FailoverLatencyMicroseconds = 420ULL;
            pInfo->SecondaryTierAvailable = 1;
            pInfo->HighPriorityFailoverPending = 0;
            pInfo->BytesRecoveredFromFaultedTier = 268435456ULL;
            pInfo->FailoverTriggerReason = 0x01;
            pInfo->StatusFlags = 0x07; // FailoverEngineActive | RedundantTierReady | ZeroDataLossGuarantee
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_FAILOVER_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 268: { // SystemCpuFabricClockStretchingTelemetryInformation (Class 268)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_CLOCK_STRETCHING_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_CLOCK_STRETCHING_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_CLOCK_STRETCHING_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->DomainId = 0;
            pInfo->ActiveClockStretchingDomains = 4;
            pInfo->TotalStretchingEvents = 256;
            pInfo->VoltageDroopCompensatedCount = 240;
            pInfo->ThermalThrottleStretchCount = 16;
            pInfo->MaxStretchPercentage = 12;
            pInfo->CumulativeStretchedDurationPicoSeconds = 1500000000ULL;
            pInfo->AverageStretchDurationPicoSeconds = 5859375ULL;
            pInfo->ClockCyclesPreserved = 12000000ULL;
            pInfo->StretchHysteresisState = 0;
            pInfo->StatusFlags = 0x07; // ClockStretchingEnabled | AutonomousDroopMitigation | PLLSynchronized
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_CLOCK_STRETCHING_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 269: { // SystemPagefileTierMigrationQosInformation (Class 269)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_MIGRATION_QOS_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_MIGRATION_QOS_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_MIGRATION_QOS_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->HighPriorityMigrationsTotal = 1024ULL;
            pInfo->StandardPriorityMigrationsTotal = 16384ULL;
            pInfo->BackgroundPriorityMigrationsTotal = 65536ULL;
            pInfo->ThrottledMigrationsTotal = 12ULL;
            pInfo->PreemptedMigrationsTotal = 4ULL;
            pInfo->AverageMigrationLatencyNs = 12500ULL;
            pInfo->PeakMigrationBandwidthBytesPerSec = 6442450944ULL;
            pInfo->ActiveMigrationQosQueues = 4;
            pInfo->TierMigrationFlags = 0x07;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_MIGRATION_QOS_INFORMATION);
            return NtStatus::Success;
        }
        case 270: { // SystemCpuFabricThermalThrottleTelemetryInformation (Class 270)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_THERMAL_THROTTLE_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_THERMAL_THROTTLE_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_THERMAL_THROTTLE_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->TotalThermalThrottleEvents = 42ULL;
            pInfo->ActiveThermalThrottleDurationUs = 150000ULL;
            pInfo->JunctionTemperatureMillicelsius = 65000ULL;
            pInfo->ThrottleTemperatureThresholdMillicelsius = 95000ULL;
            pInfo->CoreFabricFrequencyReductionMhz = 400ULL;
            pInfo->TjMaxMarginMillicelsius = 30000ULL;
            pInfo->ActiveThrottledNodesMask = 0x00000001;
            pInfo->ThermalGovernorFlags = 0x07;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_THERMAL_THROTTLE_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 271: { // SystemPagefileTierDynamicDrainInformation (Class 271)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_DYNAMIC_DRAIN_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_DYNAMIC_DRAIN_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_DYNAMIC_DRAIN_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->DrainedPagesTotal = 32768ULL;
            pInfo->DrainThroughputBytesPerSec = 4294967296ULL;
            pInfo->TargetTierIndex = 2;
            pInfo->SourceTierIndex = 0;
            pInfo->ActiveDrainWorkers = 8;
            pInfo->PendingDrainQueueDepth = 128;
            pInfo->EmergencyDrainTriggerCount = 3;
            pInfo->DrainCompletionLatencyUs = 450;
            pInfo->ReclaimedPhysicalMemoryBytes = 134217728ULL;
            pInfo->DynamicDrainFlags = 0x07;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_DYNAMIC_DRAIN_INFORMATION);
            return NtStatus::Success;
        }
        case 272: { // SystemCpuFabricVoltageDroopPredictiveInformation (Class 272)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_DROOP_PREDICTIVE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_DROOP_PREDICTIVE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_VOLTAGE_DROOP_PREDICTIVE_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->PredictiveDroopEventsTotal = 180ULL;
            pInfo->MitigatedDroopEventsTotal = 176ULL;
            pInfo->PredictedDroopMagnitudeMicrovolts = 45000;
            pInfo->TransientCurrentSlewRateMAperNs = 1250;
            pInfo->CompensationCapacitanceBoostPf = 3200;
            pInfo->PhaseAdvanceLatencyPicoseconds = 250;
            pInfo->VoltageStabilityMarginMillivolts = 85;
            pInfo->FabricPowerSupplyRailIndex = 1;
            pInfo->DroopMitigationActionFlags = 0x0F;
            pInfo->VoltageDomainStatus = 0x01;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_DROOP_PREDICTIVE_INFORMATION);
            return NtStatus::Success;
        }
        case 273: { // SystemPagefileTierPressurePredictiveInformation (Class 273)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_PRESSURE_PREDICTIVE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_PRESSURE_PREDICTIVE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_PRESSURE_PREDICTIVE_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->PredictedExhaustionTimestampNs = 1000000000ULL;
            pInfo->TierPressureRateBytesPerSec = 104857600ULL;
            pInfo->TierIndex = 1;
            pInfo->CurrentPressurePercentage = 68;
            pInfo->PredictedPressureThresholdPercentage = 85;
            pInfo->TimeToThresholdSeconds = 120;
            pInfo->ActiveEvictionWorkers = 4;
            pInfo->PendingAllocationBacklogPages = 512;
            pInfo->RecommendedPreemptiveAllocBytes = 67108864ULL;
            pInfo->PressureMitigationFlags = 0x07;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_PRESSURE_PREDICTIVE_INFORMATION);
            return NtStatus::Success;
        }
        case 274: { // SystemCpuFabricVoltageResonanceTelemetryInformation (Class 274)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_RESONANCE_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_RESONANCE_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_VOLTAGE_RESONANCE_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->ResonanceEventsTotal = 96ULL;
            pInfo->SuppressionCyclesTotal = 48000ULL;
            pInfo->ResonancePeakFrequencyKhz = 12500;
            pInfo->ResonanceHarmonicCount = 3;
            pInfo->PeakVoltageOscillationMicrovolts = 38000;
            pInfo->ActiveDampingResistanceMilliOhms = 45;
            pInfo->HarmonicSuppressionAttenuationDb = 24;
            pInfo->FabricClockPhaseModulationDegrees = 15;
            pInfo->ResonanceSuppressionFlags = 0x0F;
            pInfo->VoltageDomainStatus = 0x01;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_RESONANCE_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 275: { // SystemPagefileTierThrottlingTelemetryInformation (Class 275)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_THROTTLING_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_THROTTLING_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_THROTTLING_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->TierIndex = 1;
            pInfo->ThrottleLevelPercent = 45;
            pInfo->QueueBacklogDepth = 128;
            pInfo->ThermalBudgetPercent = 82;
            pInfo->ActiveThrottledCycles = 2500000ULL;
            pInfo->EnforcedLatencyDeltaNs = 15000ULL;
            pInfo->ThrottledWriteBytes = 52428800ULL;
            pInfo->RecoveredCycles = 1800000ULL;
            pInfo->ThrottlingMitigationFlags = 0x05;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_THROTTLING_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 276: { // SystemCpuFabricVoltagePhaseSpreadSpectrumInformation (Class 276)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_SPREAD_SPECTRUM_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_SPREAD_SPECTRUM_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_SPREAD_SPECTRUM_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->SocketId = 0;
            pInfo->PhaseCount = 8;
            pInfo->ModulationFrequencyKhz = 330;
            pInfo->SpreadSpectrumPercentagePpm = 5000;
            for (int i = 0; i < 8; ++i) {
                pInfo->PhaseCurrentMicroAmps[i] = 18500000ULL;
            }
            pInfo->VoltageJitterRmsMicroVolts = 4200ULL;
            pInfo->ActiveTransientSuppressionCycles = 960000ULL;
            pInfo->PhaseInterleavingFlags = 0x03;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_SPREAD_SPECTRUM_INFORMATION);
            return NtStatus::Success;
        }
        case 277: { // SystemPagefileTierCompactionPressureInformation (Class 277)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_COMPACTION_PRESSURE_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_COMPACTION_PRESSURE_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_COMPACTION_PRESSURE_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->TierIndex = 1;
            pInfo->CompactionPressurePercent = 68;
            pInfo->FragmentationLevelPercent = 35;
            pInfo->CompressionRatioBasisPoints = 2400;
            pInfo->CompactedPageCount = 1048576ULL;
            pInfo->ReclaimedMemoryBytes = 4294967296ULL;
            pInfo->CompactionDurationCycles = 3500000ULL;
            pInfo->DecompressionDurationCycles = 1200000ULL;
            pInfo->CompactionEngineFlags = 0x07;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_COMPACTION_PRESSURE_INFORMATION);
            return NtStatus::Success;
        }
        case 278: { // SystemCpuFabricVoltageSlewRateTelemetryInformation (Class 278)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_SLEW_RATE_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_SLEW_RATE_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_VOLTAGE_SLEW_RATE_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->SocketId = 0;
            pInfo->SlewRateVPerMicroSec = 250;
            pInfo->MaxAllowableSlewRateVPerMicroSec = 500;
            pInfo->OverShootMicroVolts = 15000;
            pInfo->TransientStepDurationNs = 40ULL;
            pInfo->DrcCompensatedCycles = 1850000ULL;
            pInfo->SlewRateViolationCount = 0ULL;
            pInfo->ActivePowerRailMicroAmps = 32000000ULL;
            pInfo->SlewRateControlFlags = 0x03;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_SLEW_RATE_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 279: { // SystemPagefileTierExpansionRateTelemetryInformation (Class 279)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_EXPANSION_RATE_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_EXPANSION_RATE_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_EXPANSION_RATE_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->TierIndex = 1;
            pInfo->ExpansionAllocationsPerSecond = 1250;
            pInfo->BytesAllocatedPerSecond = 1073741824ULL;
            pInfo->SustainedGrowthPressurePercent = 74;
            pInfo->PeakBurstRateMBPerSec = 4096;
            pInfo->DynamicAllocationHeadroomPages = 262144;
            pInfo->ThrottleSuppressionFactor = 12;
            pInfo->ExpansionLatencyMicroseconds = 85;
            pInfo->ExpansionEngineFlags = 0x07;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_EXPANSION_RATE_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 280: { // SystemCpuFabricVoltagePhaseMarginTelemetryInformation (Class 280)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_MARGIN_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_MARGIN_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_MARGIN_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->SocketId = 0;
            pInfo->PhaseIndex = 2;
            pInfo->VoltagePhaseMarginMillivolts = 65;
            pInfo->PhaseJitterPicoseconds = 18;
            pInfo->PhaseBalanceScorePercent = 96;
            pInfo->TransientRecoveryMarginMicrovolts = 45000;
            pInfo->PhaseDerateOperatingCycles = 4200000ULL;
            pInfo->PhaseMarginViolationCount = 0ULL;
            pInfo->ThermalPhaseDeratePercent = 4;
            pInfo->PhaseMarginControlFlags = 0x05;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_MARGIN_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 281: { // SystemPagefileTierCompactDedupEfficiencyInformation (Class 281)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_COMPACT_DEDUP_EFFICIENCY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_COMPACT_DEDUP_EFFICIENCY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_COMPACT_DEDUP_EFFICIENCY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->TierIndex = 1;
            pInfo->DedupPassCount = 84;
            pInfo->TotalDeduplicatedBytes = 8589934592ULL;
            pInfo->TotalCompactedPages = 2097152ULL;
            pInfo->EfficiencyRatioBasisPoints = 8450;
            pInfo->DedupHashCollisionsAvoided = 1420;
            pInfo->BackgroundDedupActive = 1;
            pInfo->TierCompactionThrottled = 0;
            pInfo->StatusFlags = 0x07;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_COMPACT_DEDUP_EFFICIENCY_INFORMATION);
            return NtStatus::Success;
        }
        case 282: { // SystemCpuFabricVoltageTransitionalDroopIndexInformation (Class 282)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_TRANSITIONAL_DROOP_INDEX_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_TRANSITIONAL_DROOP_INDEX_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_VOLTAGE_TRANSITIONAL_DROOP_INDEX_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->FabricDomainId = 0;
            pInfo->DroopEventsLogged = 38;
            pInfo->MaxTransitionalDroopMillivolts = 28;
            pInfo->AverageDroopRecoveryNanoseconds = 34;
            pInfo->DroopMitigationActivations = 195;
            pInfo->AdaptiveLoadlineActive = 1;
            pInfo->DroopSafetyMarginBreached = 0;
            pInfo->DroopIndexStatusFlags = 0x05;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_TRANSITIONAL_DROOP_INDEX_INFORMATION);
            return NtStatus::Success;
        }
        case 283: { // SystemPagefileTierEvictionPolicyTelemetryInformation (Class 283)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_EVICTION_POLICY_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_EVICTION_POLICY_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_EVICTION_POLICY_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->TierIndex = 1;
            pInfo->EvictionPassCount = 96;
            pInfo->TotalEvictedBytes = 17179869184ULL;
            pInfo->TotalEvictedPages = 4194304ULL;
            pInfo->EvictionLatencyNanoseconds = 125;
            pInfo->EvictionPressureScore = 7420;
            pInfo->PreemptiveEvictionActive = 1;
            pInfo->EvictionThrottlingEngaged = 0;
            pInfo->PolicyFlags = 0x07;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_EVICTION_POLICY_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        case 284: { // SystemCpuFabricVoltageDloadTransientMarginInformation (Class 284)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_DLOAD_TRANSIENT_MARGIN_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_DLOAD_TRANSIENT_MARGIN_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_FABRIC_VOLTAGE_DLOAD_TRANSIENT_MARGIN_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->FabricDomainId = 0;
            pInfo->DloadTransientEvents = 52;
            pInfo->TransientMarginMicrovolts = 35000;
            pInfo->DloadSlewRateMvPerMicrosec = 850;
            pInfo->AdaptiveDloadMitigations = 210;
            pInfo->DloadRegulationActive = 1;
            pInfo->TransientMarginWarning = 0;
            pInfo->DloadMarginStatusFlags = 0x06;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_FABRIC_VOLTAGE_DLOAD_TRANSIENT_MARGIN_INFORMATION);
            return NtStatus::Success;
        }
        case 285: { // SystemPagefileTierReclaimEfficiencyInformation (Class 285)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_RECLAIM_EFFICIENCY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_RECLAIM_EFFICIENCY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_RECLAIM_EFFICIENCY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->TierIndex = 1;
            pInfo->ReclaimCycleCount = 128;
            pInfo->TotalPagesScanned = 8388608ULL;
            pInfo->TotalPagesReclaimed = 6291456ULL;
            pInfo->ReclaimLatencyNanoseconds = 110;
            pInfo->EfficiencyRatioPermille = 750;
            pInfo->AdaptiveReclaimActive = 1;
            pInfo->ReclaimStarvationWarning = 0;
            pInfo->StatusFlags = 0x05;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_RECLAIM_EFFICIENCY_INFORMATION);
            return NtStatus::Success;
        }
        case 286: { // SystemCpuCoreVoltageGuardBandHeadroomInformation (Class 286)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_CORE_VOLTAGE_GUARD_BAND_HEADROOM_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_CORE_VOLTAGE_GUARD_BAND_HEADROOM_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_CORE_VOLTAGE_GUARD_BAND_HEADROOM_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->CoreIndex = 0;
            pInfo->GuardBandHeadroomMicrovolts = 42000;
            pInfo->NominalOperatingMicrovolts = 1150000;
            pInfo->TransientExcursionCounter = 18;
            pInfo->AdaptiveGuardBandMitigations = 145;
            pInfo->GuardBandTelemetryActive = 1;
            pInfo->CriticalHeadroomDepletion = 0;
            pInfo->GuardBandStatusFlags = 0x07;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_CORE_VOLTAGE_GUARD_BAND_HEADROOM_INFORMATION);
            return NtStatus::Success;
        }
        case 287: { // SystemPagefileTierMigrationLatencyInformation (Class 287)
            if (SystemInformationLength < sizeof(SYSTEM_PAGEFILE_TIER_MIGRATION_LATENCY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_MIGRATION_LATENCY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_PAGEFILE_TIER_MIGRATION_LATENCY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->SourceTierIndex = 0;
            pInfo->TargetTierIndex = 1;
            pInfo->PagesMigrated = 4194304ULL;
            pInfo->MigrationBytes = 17179869184ULL;
            pInfo->AverageLatencyNanoseconds = 85;
            pInfo->PeakLatencyNanoseconds = 240;
            pInfo->MigrationThrottleActive = 0;
            pInfo->DmaEngineAssisted = 1;
            pInfo->MigrationStatusFlags = 0x07;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_PAGEFILE_TIER_MIGRATION_LATENCY_INFORMATION);
            return NtStatus::Success;
        }
        case 288: { // SystemCpuCoreVoltageDroopPreventionTelemetryInformation (Class 288)
            if (SystemInformationLength < sizeof(SYSTEM_CPU_CORE_VOLTAGE_DROOP_PREVENTION_TELEMETRY_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_CORE_VOLTAGE_DROOP_PREVENTION_TELEMETRY_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<SYSTEM_CPU_CORE_VOLTAGE_DROOP_PREVENTION_TELEMETRY_INFORMATION*>(SystemInformation);
            std::memset(pInfo, 0, sizeof(*pInfo));
            pInfo->Version = 1;
            pInfo->Reserved = 0;
            pInfo->CoreIndex = 0;
            pInfo->DroopMitigationEvents = 312;
            pInfo->CurrentTargetMicrovolts = 1180000;
            pInfo->DroopCompensationMicrovolts = 35000;
            pInfo->ActiveVdroopLoadlineResistanceUohm = 1250;
            pInfo->DroopPreventionActive = 1;
            pInfo->UnderVoltageDetected = 0;
            pInfo->DroopPreventionStatusFlags = 0x05;
            if (ReturnLength) *ReturnLength = sizeof(SYSTEM_CPU_CORE_VOLTAGE_DROOP_PREVENTION_TELEMETRY_INFORMATION);
            return NtStatus::Success;
        }
        default: {
            if (SystemInformation && SystemInformationLength > 0) {
                std::memset(SystemInformation, 0, SystemInformationLength);
            }
            if (ReturnLength) *ReturnLength = SystemInformationLength;
            return NtStatus::Success;
        }
    }
}

inline NtStatus WINAPI RtlQueryDynamicTimeZoneInformation(RTL_DYNAMIC_TIME_ZONE_INFORMATION* TimeZoneInformation) noexcept {
    if (!TimeZoneInformation) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(102, TimeZoneInformation, sizeof(*TimeZoneInformation), &retLen);
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
using SHORT = int16_t;
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
using LONG64 = int64_t;
using ULONG64 = uint64_t;
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

enum MEMORY_PARTITION_INFORMATION_CLASS {
    MemoryPartitionInfo = 0,
    MemoryPartitionDedicatedMemoryInfo = 1,
    MemoryPartitionServerInfo = 2
};

struct MEMORY_PARTITION_CONFIGURATION_INFORMATION {
    ULONG Flags;
    ULONG TotalNumberOfPages;
    ULONG ResidentAvailablePages;
    ULONG TotalCommitLimitPages;
    ULONG CommittedPages;
    ULONG SharedCommitPages;
};

inline NtStatus WINAPI NtCreatePartition(HANDLE* PartitionHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/, ULONG /*PreferredNode*/) noexcept {
    if (!PartitionHandle) return NtStatus::InvalidParameter;
    *PartitionHandle = reinterpret_cast<HANDLE>(0x9040);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtOpenPartition(HANDLE* PartitionHandle, uint32_t /*DesiredAccess*/, void* /*ObjectAttributes*/) noexcept {
    if (!PartitionHandle) return NtStatus::InvalidParameter;
    *PartitionHandle = reinterpret_cast<HANDLE>(0x9041);
    return NtStatus::Success;
}

inline NtStatus WINAPI NtManagePartition(
    HANDLE /*TargetHandle*/,
    HANDLE /*SourceHandle*/,
    MEMORY_PARTITION_INFORMATION_CLASS PartitionInformationClass,
    void* PartitionInformation,
    ULONG PartitionInformationLength
) noexcept {
    if (!PartitionInformation) return NtStatus::InvalidParameter;
    if (PartitionInformationClass == MemoryPartitionInfo) {
        if (PartitionInformationLength < sizeof(MEMORY_PARTITION_CONFIGURATION_INFORMATION)) {
            return NtStatus::InfoLengthMismatch;
        }
        auto* pInfo = reinterpret_cast<MEMORY_PARTITION_CONFIGURATION_INFORMATION*>(PartitionInformation);
        pInfo->Flags = 0;
        pInfo->TotalNumberOfPages = 8388608;     // 32 GB at 4KB pages
        pInfo->ResidentAvailablePages = 6291456; // 24 GB available
        pInfo->TotalCommitLimitPages = 8388608;
        pInfo->CommittedPages = 2097152;         // 8 GB committed
        pInfo->SharedCommitPages = 524288;       // 2 GB shared
        return NtStatus::Success;
    }
    return NtStatus::Success;
}

inline NtStatus ZwCreatePartition(HANDLE* p, uint32_t a, void* o, ULONG n) { return NtCreatePartition(p, a, o, n); }
inline NtStatus ZwOpenPartition(HANDLE* p, uint32_t a, void* o) { return NtOpenPartition(p, a, o); }
inline NtStatus ZwManagePartition(HANDLE t, HANDLE s, MEMORY_PARTITION_INFORMATION_CLASS c, void* i, ULONG l) { return NtManagePartition(t, s, c, i, l); }

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

struct CLIENT_ID {
    void* UniqueProcess;
    void* UniqueThread;
};

struct THREAD_BASIC_INFORMATION {
    NtStatus  ExitStatus;
    void*     TebBaseAddress;
    CLIENT_ID ClientId;
    uintptr_t AffinityMask;
    int32_t   Priority;
    int32_t   BasePriority;
};

inline std::unordered_map<HANDLE, bool> s_hiddenThreads;
inline std::mutex s_hiddenThreadsMutex;
inline std::unordered_map<HANDLE, void*> s_threadStartAddresses;
inline std::unordered_map<HANDLE, std::wstring> s_ntThreadDescriptions;

inline NtStatus WINAPI NtQueryInformationThread(HANDLE ThreadHandle, uint32_t ThreadInformationClass, void* ThreadInformation, ULONG ThreadInformationLength, ULONG* ReturnLength) noexcept {
    if (!ThreadInformation && ThreadInformationLength > 0) return NtStatus::InvalidParameter;

    switch (ThreadInformationClass) {
        case 0: { // ThreadBasicInformation
            if (ThreadInformationLength < sizeof(THREAD_BASIC_INFORMATION)) {
                if (ReturnLength) *ReturnLength = sizeof(THREAD_BASIC_INFORMATION);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pInfo = reinterpret_cast<THREAD_BASIC_INFORMATION*>(ThreadInformation);
            pInfo->ExitStatus = NtStatus::Success;
            pInfo->TebBaseAddress = reinterpret_cast<void*>(0x00007FF6FFFFE000ULL);
            pInfo->ClientId.UniqueProcess = reinterpret_cast<HANDLE>(1337);
            pInfo->ClientId.UniqueThread = ThreadHandle ? ThreadHandle : reinterpret_cast<HANDLE>(0x1004);
            pInfo->AffinityMask = 0xFFFF;
            pInfo->Priority = 8;
            pInfo->BasePriority = 8;
            if (ReturnLength) *ReturnLength = sizeof(THREAD_BASIC_INFORMATION);
            return NtStatus::Success;
        }
        case 9: { // ThreadQuerySetWin32StartAddress
            if (ThreadInformationLength < sizeof(void*)) {
                if (ReturnLength) *ReturnLength = sizeof(void*);
                return NtStatus::InfoLengthMismatch;
            }
            std::lock_guard<std::mutex> lock(s_hiddenThreadsMutex);
            auto it = s_threadStartAddresses.find(ThreadHandle);
            void* startAddr = (it != s_threadStartAddresses.end()) ? it->second : reinterpret_cast<void*>(0x00007FF610001000ULL);
            *reinterpret_cast<void**>(ThreadInformation) = startAddr;
            if (ReturnLength) *ReturnLength = sizeof(void*);
            return NtStatus::Success;
        }
        case 17: { // ThreadHideFromDebugger
            if (ThreadInformationLength < sizeof(uint8_t)) {
                if (ReturnLength) *ReturnLength = sizeof(uint8_t);
                return NtStatus::InfoLengthMismatch;
            }
            std::lock_guard<std::mutex> lock(s_hiddenThreadsMutex);
            bool isHidden = s_hiddenThreads[ThreadHandle];
            *reinterpret_cast<uint8_t*>(ThreadInformation) = isHidden ? 1 : 0;
            if (ReturnLength) *ReturnLength = sizeof(uint8_t);
            return NtStatus::Success;
        }
        case 38: { // ThreadNameInformation / ThreadDescription
            std::lock_guard<std::mutex> lock(s_hiddenThreadsMutex);
            auto it = s_ntThreadDescriptions.find(ThreadHandle);
            std::wstring desc = (it != s_ntThreadDescriptions.end()) ? it->second : L"MainGameThread";
            size_t strBytes = desc.size() * sizeof(wchar_t);
            size_t totalBytes = sizeof(UNICODE_STRING) + strBytes + sizeof(wchar_t);
            if (ThreadInformationLength < totalBytes) {
                if (ReturnLength) *ReturnLength = static_cast<ULONG>(totalBytes);
                return NtStatus::InfoLengthMismatch;
            }
            auto* pUni = reinterpret_cast<UNICODE_STRING*>(ThreadInformation);
            auto* pBuf = reinterpret_cast<wchar_t*>(reinterpret_cast<uint8_t*>(ThreadInformation) + sizeof(UNICODE_STRING));
            std::memcpy(pBuf, desc.c_str(), strBytes);
            pBuf[desc.size()] = L'\0';
            pUni->length = static_cast<uint16_t>(strBytes);
            pUni->maximumLength = static_cast<uint16_t>(strBytes + sizeof(wchar_t));
            pUni->buffer = pBuf;
            if (ReturnLength) *ReturnLength = static_cast<ULONG>(totalBytes);
            return NtStatus::Success;
        }
        default: {
            if (ThreadInformation && ThreadInformationLength > 0) {
                std::memset(ThreadInformation, 0, ThreadInformationLength);
            }
            if (ReturnLength) *ReturnLength = ThreadInformationLength;
            return NtStatus::Success;
        }
    }
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

inline std::atomic<ULONG> s_NtTimerResolution{ 10000 }; // 1.0 ms default

inline NtStatus WINAPI NtSetTimerResolution(ULONG DesiredResolution, BOOLEAN SetResolution, ULONG* CurrentResolution) noexcept {
    if (SetResolution) {
        ULONG res = std::clamp(DesiredResolution, 5000U, 156250U);
        s_NtTimerResolution.store(res, std::memory_order_relaxed);
    } else {
        s_NtTimerResolution.store(156250U, std::memory_order_relaxed);
    }
    if (CurrentResolution) {
        *CurrentResolution = s_NtTimerResolution.load(std::memory_order_relaxed);
    }
    return NtStatus::Success;
}

inline NtStatus WINAPI NtQueryTimerResolution(ULONG* MaximumTime, ULONG* MinimumTime, ULONG* CurrentTime) noexcept {
    if (MaximumTime) *MaximumTime = 156250;
    if (MinimumTime) *MinimumTime = 5000;
    if (CurrentTime) *CurrentTime = s_NtTimerResolution.load(std::memory_order_relaxed);
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
inline NtStatus WINAPI NtSetInformationThread(HANDLE ThreadHandle, uint32_t ThreadInformationClass, void* ThreadInformation, ULONG ThreadInformationLength) noexcept {
    switch (ThreadInformationClass) {
        case 17: { // ThreadHideFromDebugger
            std::lock_guard<std::mutex> lock(s_hiddenThreadsMutex);
            s_hiddenThreads[ThreadHandle] = true;
            return NtStatus::Success;
        }
        case 9: { // ThreadQuerySetWin32StartAddress
            if (ThreadInformation && ThreadInformationLength >= sizeof(void*)) {
                std::lock_guard<std::mutex> lock(s_hiddenThreadsMutex);
                s_threadStartAddresses[ThreadHandle] = *reinterpret_cast<void**>(ThreadInformation);
                return NtStatus::Success;
            }
            return NtStatus::InvalidParameter;
        }
        case 38: { // ThreadNameInformation / ThreadDescription
            if (ThreadInformation && ThreadInformationLength >= sizeof(UNICODE_STRING)) {
                auto* pStr = reinterpret_cast<UNICODE_STRING*>(ThreadInformation);
                if (pStr->buffer && pStr->length > 0) {
                    std::lock_guard<std::mutex> lock(s_hiddenThreadsMutex);
                    s_ntThreadDescriptions[ThreadHandle] = std::wstring(pStr->buffer, pStr->length / sizeof(wchar_t));
                    return NtStatus::Success;
                }
            }
            return NtStatus::InvalidParameter;
        }
        default:
            return NtStatus::Success;
    }
}
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

using WORKERCALLBACKFUNC = void(WINAPI*)(void* Context);
using WAITORTIMERCALLBACKFUNC = void(WINAPI*)(void* Context, BOOLEAN TimerOrWaitFired);

inline NtStatus WINAPI RtlQueueWorkItem(WORKERCALLBACKFUNC Function, void* Context, ULONG Flags) noexcept {
    (void)Flags;
    if (!Function) return NtStatus::InvalidParameter;
    Function(Context);
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlCreateTimerQueue(HANDLE* TimerQueueHandle) noexcept {
    if (!TimerQueueHandle) return NtStatus::InvalidParameter;
    *TimerQueueHandle = reinterpret_cast<HANDLE>(0x9104);
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlCreateTimer(
    HANDLE TimerQueueHandle,
    HANDLE* Handle,
    WAITORTIMERCALLBACKFUNC Function,
    void* Context,
    ULONG DueTime,
    ULONG Period,
    ULONG Flags) noexcept {
    (void)TimerQueueHandle;
    (void)Function;
    (void)Context;
    (void)DueTime;
    (void)Period;
    (void)Flags;
    if (!Handle) return NtStatus::InvalidParameter;
    *Handle = reinterpret_cast<HANDLE>(0x9105);
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlUpdateTimer(
    HANDLE TimerQueueHandle,
    HANDLE TimerHandle,
    ULONG DueTime,
    ULONG Period) noexcept {
    (void)TimerQueueHandle;
    (void)TimerHandle;
    (void)DueTime;
    (void)Period;
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlDeleteTimer(
    HANDLE TimerQueueHandle,
    HANDLE TimerHandle,
    HANDLE CompletionEvent) noexcept {
    (void)TimerQueueHandle;
    (void)TimerHandle;
    (void)CompletionEvent;
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlDeleteTimerQueueEx(
    HANDLE TimerQueueHandle,
    HANDLE CompletionEvent) noexcept {
    (void)TimerQueueHandle;
    (void)CompletionEvent;
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlDeleteTimerQueue(HANDLE TimerQueueHandle) noexcept {
    return RtlDeleteTimerQueueEx(TimerQueueHandle, nullptr);
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

struct SLIST_ENTRY {
    SLIST_ENTRY* Next;
};
using PSLIST_ENTRY = SLIST_ENTRY*;

union alignas(16) SLIST_HEADER {
    struct {
        uint64_t Alignment;
        uint64_t Region;
    };
    struct {
        uint64_t Depth : 16;
        uint64_t Sequence : 48;
        uint64_t Reserved : 4;
        uint64_t NextEntry : 60;
    } HeaderX64;
};
using PSLIST_HEADER = SLIST_HEADER*;

inline void WINAPI RtlInitializeSListHead(PSLIST_HEADER SListHead) noexcept {
    if (SListHead) {
        SListHead->Alignment = 0;
        SListHead->Region = 0;
    }
}

inline NtStatus WINAPI RtlInitializeSListHeadEx(PSLIST_HEADER SListHead, ULONG Flags) noexcept {
    if (!SListHead) return NtStatus::InvalidParameter;
    SListHead->Alignment = 0;
    SListHead->Region = static_cast<uint64_t>(Flags) << 48;
    return NtStatus::Success;
}

inline uint16_t WINAPI RtlCompactSListHead(PSLIST_HEADER SListHead) noexcept {
    if (!SListHead) return 0;
    return static_cast<uint16_t>(SListHead->Region & 0xFFFF);
}

inline PSLIST_ENTRY WINAPI RtlFirstEntrySList(const SLIST_HEADER* SListHead) noexcept {
    if (!SListHead) return nullptr;
    return reinterpret_cast<PSLIST_ENTRY>(SListHead->Alignment);
}

inline uint16_t WINAPI RtlQueryDepthSList(const SLIST_HEADER* SListHead) noexcept {
    if (!SListHead) return 0;
    return static_cast<uint16_t>(SListHead->Region & 0xFFFF);
}

inline PSLIST_ENTRY WINAPI RtlInterlockedPushEntrySList(PSLIST_HEADER SListHead, PSLIST_ENTRY ListEntry) noexcept {
    if (!SListHead || !ListEntry) return nullptr;
    auto* oldFirst = reinterpret_cast<PSLIST_ENTRY>(SListHead->Alignment);
    ListEntry->Next = oldFirst;
    SListHead->Alignment = reinterpret_cast<uint64_t>(ListEntry);
    uint64_t depth = (SListHead->Region & 0xFFFF) + 1;
    uint64_t seq = ((SListHead->Region >> 16) + 1) & 0xFFFFFFFFFFFFULL;
    SListHead->Region = (seq << 16) | (depth & 0xFFFF);
    return oldFirst;
}

inline PSLIST_ENTRY WINAPI RtlInterlockedPushListSListEx(
    PSLIST_HEADER SListHead,
    PSLIST_ENTRY List,
    PSLIST_ENTRY ListEnd,
    ULONG Count) noexcept {
    if (!SListHead || !List || !ListEnd || Count == 0) return nullptr;
    auto* oldFirst = reinterpret_cast<PSLIST_ENTRY>(SListHead->Alignment);
    ListEnd->Next = oldFirst;
    SListHead->Alignment = reinterpret_cast<uint64_t>(List);
    uint64_t depth = (SListHead->Region & 0xFFFF) + Count;
    uint64_t seq = ((SListHead->Region >> 16) + 1) & 0xFFFFFFFFFFFFULL;
    SListHead->Region = (seq << 16) | (depth & 0xFFFF);
    return oldFirst;
}

inline PSLIST_ENTRY WINAPI RtlInterlockedPushEntrySListEx(
    PSLIST_HEADER SListHead,
    PSLIST_ENTRY ListEntry) noexcept {
    return RtlInterlockedPushListSListEx(SListHead, ListEntry, ListEntry, 1);
}

inline PSLIST_ENTRY WINAPI RtlInterlockedPopEntrySList(PSLIST_HEADER SListHead) noexcept {
    if (!SListHead) return nullptr;
    auto* first = reinterpret_cast<PSLIST_ENTRY>(SListHead->Alignment);
    if (!first) return nullptr;
    SListHead->Alignment = reinterpret_cast<uint64_t>(first->Next);
    uint64_t depth = SListHead->Region & 0xFFFF;
    if (depth > 0) depth--;
    uint64_t seq = ((SListHead->Region >> 16) + 1) & 0xFFFFFFFFFFFFULL;
    SListHead->Region = (seq << 16) | (depth & 0xFFFF);
    return first;
}

inline PSLIST_ENTRY WINAPI RtlInterlockedFlushSList(PSLIST_HEADER SListHead) noexcept {
    if (!SListHead) return nullptr;
    auto* first = reinterpret_cast<PSLIST_ENTRY>(SListHead->Alignment);
    SListHead->Alignment = 0;
    uint64_t seq = ((SListHead->Region >> 16) + 1) & 0xFFFFFFFFFFFFULL;
    SListHead->Region = (seq << 16);
    return first;
}

inline void* WINAPI RtlInterlockedCompareExchangePointer(void** Destination, void* Exchange, void* Comperand) noexcept {
    if (!Destination) return nullptr;
    auto* atomicDest = reinterpret_cast<std::atomic<void*>*>(Destination);
    void* expected = Comperand;
    atomicDest->compare_exchange_strong(expected, Exchange, std::memory_order_seq_cst);
    return expected;
}

inline void* WINAPI RtlInterlockedExchangePointer(void** Destination, void* Exchange) noexcept {
    if (!Destination) return nullptr;
    auto* atomicDest = reinterpret_cast<std::atomic<void*>*>(Destination);
    return atomicDest->exchange(Exchange, std::memory_order_seq_cst);
}

inline int64_t WINAPI RtlInterlockedAddLargeInteger(int64_t* Addend, int64_t Increment) noexcept {
    if (!Addend) return 0;
    auto* atomicVal = reinterpret_cast<std::atomic<int64_t>*>(Addend);
    return atomicVal->fetch_add(Increment, std::memory_order_seq_cst) + Increment;
}

inline int8_t WINAPI RtlFindLeastSignificantBit(uint64_t Set) noexcept {
    if (Set == 0) return -1;
    return static_cast<int8_t>(std::countr_zero(Set));
}

inline int8_t WINAPI RtlFindMostSignificantBit(uint64_t Set) noexcept {
    if (Set == 0) return -1;
    return static_cast<int8_t>(63 - std::countl_zero(Set));
}

inline void WINAPI RtlFillMemoryUlong(void* Destination, size_t Length, uint32_t Pattern) noexcept {
    if (!Destination || Length < sizeof(uint32_t)) return;
    auto* p = reinterpret_cast<uint32_t*>(Destination);
    size_t count = Length / sizeof(uint32_t);
    for (size_t i = 0; i < count; ++i) {
        p[i] = Pattern;
    }
}

inline void WINAPI RtlFillMemoryUlonglong(void* Destination, size_t Length, uint64_t Pattern) noexcept {
    if (!Destination || Length < sizeof(uint64_t)) return;
    auto* p = reinterpret_cast<uint64_t*>(Destination);
    size_t count = Length / sizeof(uint64_t);
    for (size_t i = 0; i < count; ++i) {
        p[i] = Pattern;
    }
}

inline void WINAPI RtlZeroMemoryEx(void* Destination, size_t Length, uint32_t /*Flags*/) noexcept {
    if (Destination && Length > 0) {
        std::memset(Destination, 0, Length);
    }
}

inline size_t WINAPI RtlCompareMemoryUlong(const void* Source, size_t Length, uint32_t Pattern) noexcept {
    if (!Source || Length < sizeof(uint32_t)) return 0;
    const auto* p = reinterpret_cast<const uint32_t*>(Source);
    size_t count = Length / sizeof(uint32_t);
    size_t matchedBytes = 0;
    for (size_t i = 0; i < count; ++i) {
        if (p[i] == Pattern) {
            matchedBytes += sizeof(uint32_t);
        } else {
            break;
        }
    }
    return matchedBytes;
}

inline BOOLEAN WINAPI RtlInterlockedBitTestAndSet(LONG volatile* Base, LONG Bit) noexcept {
    if (!Base) return FALSE;
    Base += (Bit / 32);
    LONG mask = 1L << (Bit % 32);
    auto* atomicBase = reinterpret_cast<std::atomic<LONG> volatile*>(Base);
    LONG oldVal = atomicBase->fetch_or(mask, std::memory_order_seq_cst);
    return (oldVal & mask) != 0 ? TRUE : FALSE;
}

inline BOOLEAN WINAPI RtlInterlockedBitTestAndReset(LONG volatile* Base, LONG Bit) noexcept {
    if (!Base) return FALSE;
    Base += (Bit / 32);
    LONG mask = 1L << (Bit % 32);
    auto* atomicBase = reinterpret_cast<std::atomic<LONG> volatile*>(Base);
    LONG oldVal = atomicBase->fetch_and(~mask, std::memory_order_seq_cst);
    return (oldVal & mask) != 0 ? TRUE : FALSE;
}

inline BOOLEAN WINAPI RtlInterlockedBitTestAndSet64(LONG64 volatile* Base, LONG64 Bit) noexcept {
    if (!Base) return FALSE;
    Base += (Bit / 64);
    LONG64 mask = 1LL << (Bit % 64);
    auto* atomicBase = reinterpret_cast<std::atomic<LONG64> volatile*>(Base);
    LONG64 oldVal = atomicBase->fetch_or(mask, std::memory_order_seq_cst);
    return (oldVal & mask) != 0 ? TRUE : FALSE;
}

inline BOOLEAN WINAPI RtlInterlockedBitTestAndReset64(LONG64 volatile* Base, LONG64 Bit) noexcept {
    if (!Base) return FALSE;
    Base += (Bit / 64);
    LONG64 mask = 1LL << (Bit % 64);
    auto* atomicBase = reinterpret_cast<std::atomic<LONG64> volatile*>(Base);
    LONG64 oldVal = atomicBase->fetch_and(~mask, std::memory_order_seq_cst);
    return (oldVal & mask) != 0 ? TRUE : FALSE;
}

inline BOOLEAN WINAPI RtlInterlockedBitTestAndComplement(LONG volatile* Base, LONG Bit) noexcept {
    if (!Base) return FALSE;
    Base += (Bit / 32);
    LONG mask = 1L << (Bit % 32);
    auto* atomicBase = reinterpret_cast<std::atomic<LONG> volatile*>(Base);
    LONG oldVal = atomicBase->fetch_xor(mask, std::memory_order_seq_cst);
    return (oldVal & mask) != 0 ? TRUE : FALSE;
}

inline BOOLEAN WINAPI RtlInterlockedBitTestAndComplement64(LONG64 volatile* Base, LONG64 Bit) noexcept {
    if (!Base) return FALSE;
    Base += (Bit / 64);
    LONG64 mask = 1LL << (Bit % 64);
    auto* atomicBase = reinterpret_cast<std::atomic<LONG64> volatile*>(Base);
    LONG64 oldVal = atomicBase->fetch_xor(mask, std::memory_order_seq_cst);
    return (oldVal & mask) != 0 ? TRUE : FALSE;
}

inline LONG WINAPI RtlInterlockedAnd(LONG volatile* Destination, LONG Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    return atomicDest->fetch_and(Value, std::memory_order_seq_cst);
}

inline LONG WINAPI RtlInterlockedOr(LONG volatile* Destination, LONG Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    return atomicDest->fetch_or(Value, std::memory_order_seq_cst);
}

inline LONG WINAPI RtlInterlockedXor(LONG volatile* Destination, LONG Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    return atomicDest->fetch_xor(Value, std::memory_order_seq_cst);
}

inline LONG64 WINAPI RtlInterlockedAnd64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    return atomicDest->fetch_and(Value, std::memory_order_seq_cst);
}

inline LONG64 WINAPI RtlInterlockedOr64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    return atomicDest->fetch_or(Value, std::memory_order_seq_cst);
}

inline LONG64 WINAPI RtlInterlockedXor64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    return atomicDest->fetch_xor(Value, std::memory_order_seq_cst);
}

inline LONG WINAPI RtlInterlockedAndNot(LONG volatile* Destination, LONG Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    return atomicDest->fetch_and(~Value, std::memory_order_seq_cst);
}

inline LONG64 WINAPI RtlInterlockedAndNot64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    return atomicDest->fetch_and(~Value, std::memory_order_seq_cst);
}

inline LONG64 WINAPI RtlInterlockedCompareExchange64(LONG64 volatile* Destination, LONG64 Exchange, LONG64 Comperand) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    LONG64 expected = Comperand;
    atomicDest->compare_exchange_strong(expected, Exchange, std::memory_order_seq_cst);
    return expected;
}

inline LONG64 WINAPI RtlInterlockedExchange64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    return atomicDest->exchange(Value, std::memory_order_seq_cst);
}

inline LONG64 WINAPI RtlInterlockedExchangeAdd64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    return atomicDest->fetch_add(Value, std::memory_order_seq_cst);
}

inline LONG64 WINAPI RtlInterlockedIncrement64(LONG64 volatile* Addend) noexcept {
    if (!Addend) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Addend);
    return atomicDest->fetch_add(1, std::memory_order_seq_cst) + 1;
}

inline LONG64 WINAPI RtlInterlockedDecrement64(LONG64 volatile* Addend) noexcept {
    if (!Addend) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Addend);
    return atomicDest->fetch_sub(1, std::memory_order_seq_cst) - 1;
}

inline SHORT WINAPI RtlInterlockedCompareExchange16(SHORT volatile* Destination, SHORT Exchange, SHORT Comperand) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    SHORT expected = Comperand;
    atomicDest->compare_exchange_strong(expected, Exchange, std::memory_order_seq_cst);
    return expected;
}

inline SHORT WINAPI RtlInterlockedExchange16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    return atomicDest->exchange(Value, std::memory_order_seq_cst);
}

inline SHORT WINAPI RtlInterlockedExchangeAdd16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    return atomicDest->fetch_add(Value, std::memory_order_seq_cst);
}

inline SHORT WINAPI RtlInterlockedIncrement16(SHORT volatile* Addend) noexcept {
    if (!Addend) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Addend);
    return atomicDest->fetch_add(1, std::memory_order_seq_cst) + 1;
}

inline SHORT WINAPI RtlInterlockedDecrement16(SHORT volatile* Addend) noexcept {
    if (!Addend) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Addend);
    return atomicDest->fetch_sub(1, std::memory_order_seq_cst) - 1;
}

inline SHORT WINAPI RtlInterlockedAnd16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    return atomicDest->fetch_and(Value, std::memory_order_seq_cst);
}

inline SHORT WINAPI RtlInterlockedOr16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    return atomicDest->fetch_or(Value, std::memory_order_seq_cst);
}

inline SHORT WINAPI RtlInterlockedXor16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    return atomicDest->fetch_xor(Value, std::memory_order_seq_cst);
}

inline CHAR WINAPI RtlInterlockedCompareExchange8(CHAR volatile* Destination, CHAR Exchange, CHAR Comperand) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    CHAR expected = Comperand;
    atomicDest->compare_exchange_strong(expected, Exchange, std::memory_order_seq_cst);
    return expected;
}

inline CHAR WINAPI RtlInterlockedExchange8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    return atomicDest->exchange(Value, std::memory_order_seq_cst);
}

inline CHAR WINAPI RtlInterlockedExchangeAdd8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    return atomicDest->fetch_add(Value, std::memory_order_seq_cst);
}

inline CHAR WINAPI RtlInterlockedIncrement8(CHAR volatile* Addend) noexcept {
    if (!Addend) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Addend);
    return atomicDest->fetch_add(1, std::memory_order_seq_cst) + 1;
}

inline CHAR WINAPI RtlInterlockedDecrement8(CHAR volatile* Addend) noexcept {
    if (!Addend) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Addend);
    return atomicDest->fetch_sub(1, std::memory_order_seq_cst) - 1;
}

inline CHAR WINAPI RtlInterlockedAnd8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    return atomicDest->fetch_and(Value, std::memory_order_seq_cst);
}

inline CHAR WINAPI RtlInterlockedOr8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    return atomicDest->fetch_or(Value, std::memory_order_seq_cst);
}

inline CHAR WINAPI RtlInterlockedXor8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    return atomicDest->fetch_xor(Value, std::memory_order_seq_cst);
}

inline SHORT WINAPI RtlInterlockedAndNot16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    return atomicDest->fetch_and(static_cast<SHORT>(~Value), std::memory_order_seq_cst);
}

inline CHAR WINAPI RtlInterlockedAndNot8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    return atomicDest->fetch_and(static_cast<CHAR>(~Value), std::memory_order_seq_cst);
}

inline LONG64 WINAPI RtlInterlockedXorNot64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    return atomicDest->fetch_xor(static_cast<LONG64>(~Value), std::memory_order_seq_cst);
}

inline LONG WINAPI RtlInterlockedXorNot32(LONG volatile* Destination, LONG Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    return atomicDest->fetch_xor(static_cast<LONG>(~Value), std::memory_order_seq_cst);
}

inline SHORT WINAPI RtlInterlockedXorNot16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    return atomicDest->fetch_xor(static_cast<SHORT>(~Value), std::memory_order_seq_cst);
}

inline CHAR WINAPI RtlInterlockedXorNot8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    return atomicDest->fetch_xor(static_cast<CHAR>(~Value), std::memory_order_seq_cst);
}

inline LONG64 WINAPI RtlInterlockedNand64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    LONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, static_cast<LONG64>(~(current & Value)), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline LONG WINAPI RtlInterlockedNand32(LONG volatile* Destination, LONG Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    LONG current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, static_cast<LONG>(~(current & Value)), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline SHORT WINAPI RtlInterlockedNand16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    SHORT current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, static_cast<SHORT>(~(current & Value)), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline CHAR WINAPI RtlInterlockedNand8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    CHAR current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, static_cast<CHAR>(~(current & Value)), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline LONG64 WINAPI RtlInterlockedNor64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    LONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, static_cast<LONG64>(~(current | Value)), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline LONG WINAPI RtlInterlockedNor32(LONG volatile* Destination, LONG Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    LONG current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, static_cast<LONG>(~(current | Value)), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline SHORT WINAPI RtlInterlockedNor16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    SHORT current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, static_cast<SHORT>(~(current | Value)), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline CHAR WINAPI RtlInterlockedNor8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    CHAR current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, static_cast<CHAR>(~(current | Value)), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline LONG64 WINAPI RtlInterlockedXnor64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    LONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, static_cast<LONG64>(~(current ^ Value)), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline LONG WINAPI RtlInterlockedXnor32(LONG volatile* Destination, LONG Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    LONG current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, static_cast<LONG>(~(current ^ Value)), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline SHORT WINAPI RtlInterlockedXnor16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    SHORT current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, static_cast<SHORT>(~(current ^ Value)), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline CHAR WINAPI RtlInterlockedXnor8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    CHAR current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, static_cast<CHAR>(~(current ^ Value)), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline LONG64 WINAPI RtlInterlockedMin64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    LONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, std::min(current, Value), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline LONG WINAPI RtlInterlockedMin32(LONG volatile* Destination, LONG Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    LONG current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, std::min(current, Value), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline SHORT WINAPI RtlInterlockedMin16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    SHORT current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, std::min(current, Value), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline CHAR WINAPI RtlInterlockedMin8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    CHAR current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, std::min(current, Value), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline LONG64 WINAPI RtlInterlockedMax64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    LONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, std::max(current, Value), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline LONG WINAPI RtlInterlockedMax32(LONG volatile* Destination, LONG Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    LONG current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, std::max(current, Value), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline SHORT WINAPI RtlInterlockedMax16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    SHORT current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, std::max(current, Value), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline CHAR WINAPI RtlInterlockedMax8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    CHAR current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, std::max(current, Value), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline LONG64 WINAPI RtlInterlockedClamp64(LONG64 volatile* Destination, LONG64 MinVal, LONG64 MaxVal) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    LONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, std::clamp(current, MinVal, MaxVal), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline LONG WINAPI RtlInterlockedClamp32(LONG volatile* Destination, LONG MinVal, LONG MaxVal) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    LONG current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, std::clamp(current, MinVal, MaxVal), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline SHORT WINAPI RtlInterlockedClamp16(SHORT volatile* Destination, SHORT MinVal, SHORT MaxVal) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    SHORT current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, std::clamp(current, MinVal, MaxVal), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline CHAR WINAPI RtlInterlockedClamp8(CHAR volatile* Destination, CHAR MinVal, CHAR MaxVal) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    CHAR current = atomicDest->load(std::memory_order_relaxed);
    while (!atomicDest->compare_exchange_weak(current, std::clamp(current, MinVal, MaxVal), std::memory_order_seq_cst, std::memory_order_relaxed)) {}
    return current;
}

inline LONG64 WINAPI RtlInterlockedAddModulo64(LONG64 volatile* Destination, LONG64 Value, LONG64 Modulus) noexcept {
    if (!Destination || Modulus <= 0) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    LONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        LONG64 next = (current + Value) % Modulus;
        if (next < 0) next += Modulus;
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline LONG WINAPI RtlInterlockedAddModulo32(LONG volatile* Destination, LONG Value, LONG Modulus) noexcept {
    if (!Destination || Modulus <= 0) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    LONG current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        LONG next = (current + Value) % Modulus;
        if (next < 0) next += Modulus;
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline SHORT WINAPI RtlInterlockedAddModulo16(SHORT volatile* Destination, SHORT Value, SHORT Modulus) noexcept {
    if (!Destination || Modulus <= 0) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    SHORT current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        SHORT next = static_cast<SHORT>((current + Value) % Modulus);
        if (next < 0) next = static_cast<SHORT>(next + Modulus);
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline CHAR WINAPI RtlInterlockedAddModulo8(CHAR volatile* Destination, CHAR Value, CHAR Modulus) noexcept {
    if (!Destination || Modulus <= 0) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    CHAR current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        CHAR next = static_cast<CHAR>((current + Value) % Modulus);
        if (next < 0) next = static_cast<CHAR>(next + Modulus);
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline LONG64 WINAPI RtlInterlockedMulModulo64(LONG64 volatile* Destination, LONG64 Multiplier, LONG64 Modulus) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    if (Modulus <= 0) return atomicDest->load(std::memory_order_relaxed);
    LONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        __int128 prod = static_cast<__int128>(current) * static_cast<__int128>(Multiplier);
        LONG64 next = static_cast<LONG64>(prod % Modulus);
        if (next < 0) next += Modulus;
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline LONG WINAPI RtlInterlockedMulModulo32(LONG volatile* Destination, LONG Multiplier, LONG Modulus) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    if (Modulus <= 0) return atomicDest->load(std::memory_order_relaxed);
    LONG current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        int64_t prod = static_cast<int64_t>(current) * static_cast<int64_t>(Multiplier);
        LONG next = static_cast<LONG>(prod % Modulus);
        if (next < 0) next += Modulus;
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline SHORT WINAPI RtlInterlockedMulModulo16(SHORT volatile* Destination, SHORT Multiplier, SHORT Modulus) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    if (Modulus <= 0) return atomicDest->load(std::memory_order_relaxed);
    SHORT current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        int32_t prod = static_cast<int32_t>(current) * static_cast<int32_t>(Multiplier);
        SHORT next = static_cast<SHORT>(prod % Modulus);
        if (next < 0) next = static_cast<SHORT>(next + Modulus);
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline CHAR WINAPI RtlInterlockedMulModulo8(CHAR volatile* Destination, CHAR Multiplier, CHAR Modulus) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    if (Modulus <= 0) return atomicDest->load(std::memory_order_relaxed);
    CHAR current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        int32_t prod = static_cast<int32_t>(current) * static_cast<int32_t>(Multiplier);
        CHAR next = static_cast<CHAR>(prod % Modulus);
        if (next < 0) next = static_cast<CHAR>(next + Modulus);
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG64 WINAPI RtlInterlockedRotateRight64(ULONG64 volatile* Destination, ULONG ShiftCount) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG64> volatile*>(Destination);
    ULONG shift = ShiftCount & 63u;
    ULONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG64 next = (shift == 0) ? current : ((current >> shift) | (current << (64u - shift)));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG WINAPI RtlInterlockedRotateRight32(ULONG volatile* Destination, ULONG ShiftCount) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG> volatile*>(Destination);
    ULONG shift = ShiftCount & 31u;
    ULONG current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG next = (shift == 0) ? current : ((current >> shift) | (current << (32u - shift)));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline USHORT WINAPI RtlInterlockedRotateRight16(USHORT volatile* Destination, ULONG ShiftCount) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<USHORT> volatile*>(Destination);
    ULONG shift = ShiftCount & 15u;
    USHORT current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        USHORT next = (shift == 0) ? current : static_cast<USHORT>((current >> shift) | (current << (16u - shift)));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline UCHAR WINAPI RtlInterlockedRotateRight8(UCHAR volatile* Destination, ULONG ShiftCount) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<UCHAR> volatile*>(Destination);
    ULONG shift = ShiftCount & 7u;
    UCHAR current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        UCHAR next = (shift == 0) ? current : static_cast<UCHAR>((current >> shift) | (current << (8u - shift)));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG64 WINAPI RtlInterlockedRotateLeft64(ULONG64 volatile* Destination, ULONG ShiftCount) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG64> volatile*>(Destination);
    ULONG shift = ShiftCount & 63u;
    ULONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG64 next = (shift == 0) ? current : ((current << shift) | (current >> (64u - shift)));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG WINAPI RtlInterlockedRotateLeft32(ULONG volatile* Destination, ULONG ShiftCount) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG> volatile*>(Destination);
    ULONG shift = ShiftCount & 31u;
    ULONG current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG next = (shift == 0) ? current : ((current << shift) | (current >> (32u - shift)));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline USHORT WINAPI RtlInterlockedRotateLeft16(USHORT volatile* Destination, ULONG ShiftCount) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<USHORT> volatile*>(Destination);
    ULONG shift = ShiftCount & 15u;
    USHORT current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        USHORT next = (shift == 0) ? current : static_cast<USHORT>((current << shift) | (current >> (16u - shift)));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline UCHAR WINAPI RtlInterlockedRotateLeft8(UCHAR volatile* Destination, ULONG ShiftCount) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<UCHAR> volatile*>(Destination);
    ULONG shift = ShiftCount & 7u;
    UCHAR current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        UCHAR next = (shift == 0) ? current : static_cast<UCHAR>((current << shift) | (current >> (8u - shift)));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline constexpr uint8_t RtlpBitReverse8(uint8_t b) noexcept {
    b = static_cast<uint8_t>(((b & 0xF0) >> 4) | ((b & 0x0F) << 4));
    b = static_cast<uint8_t>(((b & 0xCC) >> 2) | ((b & 0x33) << 2));
    b = static_cast<uint8_t>(((b & 0xAA) >> 1) | ((b & 0x55) << 1));
    return b;
}

inline constexpr uint16_t RtlpBitReverse16(uint16_t w) noexcept {
    w = static_cast<uint16_t>(((w & 0xFF00) >> 8) | ((w & 0x00FF) << 8));
    w = static_cast<uint16_t>(((w & 0xF0F0) >> 4) | ((w & 0x0F0F) << 4));
    w = static_cast<uint16_t>(((w & 0xCCCC) >> 2) | ((w & 0x3333) << 2));
    w = static_cast<uint16_t>(((w & 0xAAAA) >> 1) | ((w & 0x5555) << 1));
    return w;
}

inline constexpr uint32_t RtlpBitReverse32(uint32_t x) noexcept {
    x = ((x & 0xFFFF0000u) >> 16) | ((x & 0x0000FFFFu) << 16);
    x = ((x & 0xFF00FF00u) >> 8)  | ((x & 0x00FF00FFu) << 8);
    x = ((x & 0xF0F0F0F0u) >> 4)  | ((x & 0x0F0F0F0Fu) << 4);
    x = ((x & 0xCCCCCCCCu) >> 2)  | ((x & 0x33333333u) << 2);
    x = ((x & 0xAAAAAAAAu) >> 1)  | ((x & 0x55555555u) << 1);
    return x;
}

inline constexpr uint64_t RtlpBitReverse64(uint64_t x) noexcept {
    x = ((x & 0xFFFFFFFF00000000ULL) >> 32) | ((x & 0x00000000FFFFFFFFULL) << 32);
    x = ((x & 0xFFFF0000FFFF0000ULL) >> 16) | ((x & 0x0000FFFF0000FFFFULL) << 16);
    x = ((x & 0xFF00FF00FF00FF00ULL) >> 8)  | ((x & 0x00FF00FF00FF00FFULL) << 8);
    x = ((x & 0xF0F0F0F0F0F0F0F0ULL) >> 4)  | ((x & 0x0F0F0F0F0F0F0F0FULL) << 4);
    x = ((x & 0xCCCCCCCCCCCCCCCCULL) >> 2)  | ((x & 0x3333333333333333ULL) << 2);
    x = ((x & 0xAAAAAAAAAAAAAAAAULL) >> 1)  | ((x & 0x5555555555555555ULL) << 1);
    return x;
}

inline ULONG64 WINAPI RtlInterlockedBitReverse64(ULONG64 volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG64> volatile*>(Destination);
    ULONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG64 next = RtlpBitReverse64(current);
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG WINAPI RtlInterlockedBitReverse32(ULONG volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG> volatile*>(Destination);
    ULONG current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG next = RtlpBitReverse32(current);
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline USHORT WINAPI RtlInterlockedBitReverse16(USHORT volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<USHORT> volatile*>(Destination);
    USHORT current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        USHORT next = RtlpBitReverse16(current);
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline UCHAR WINAPI RtlInterlockedBitReverse8(UCHAR volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<UCHAR> volatile*>(Destination);
    UCHAR current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        UCHAR next = RtlpBitReverse8(current);
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG64 WINAPI RtlInterlockedPopCount64(ULONG64 volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG64> volatile*>(Destination);
    ULONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG64 next = static_cast<ULONG64>(std::popcount(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG WINAPI RtlInterlockedPopCount32(ULONG volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG> volatile*>(Destination);
    ULONG current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG next = static_cast<ULONG>(std::popcount(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline USHORT WINAPI RtlInterlockedPopCount16(USHORT volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<USHORT> volatile*>(Destination);
    USHORT current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        USHORT next = static_cast<USHORT>(std::popcount(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline UCHAR WINAPI RtlInterlockedPopCount8(UCHAR volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<UCHAR> volatile*>(Destination);
    UCHAR current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        UCHAR next = static_cast<UCHAR>(std::popcount(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG64 WINAPI RtlInterlockedCountLeadingZeros64(ULONG64 volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG64> volatile*>(Destination);
    ULONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG64 next = static_cast<ULONG64>(std::countl_zero(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG WINAPI RtlInterlockedCountLeadingZeros32(ULONG volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG> volatile*>(Destination);
    ULONG current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG next = static_cast<ULONG>(std::countl_zero(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline USHORT WINAPI RtlInterlockedCountLeadingZeros16(USHORT volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<USHORT> volatile*>(Destination);
    USHORT current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        USHORT next = static_cast<USHORT>(std::countl_zero(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline UCHAR WINAPI RtlInterlockedCountLeadingZeros8(UCHAR volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<UCHAR> volatile*>(Destination);
    UCHAR current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        UCHAR next = static_cast<UCHAR>(std::countl_zero(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG64 WINAPI RtlInterlockedCountTrailingZeros64(ULONG64 volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG64> volatile*>(Destination);
    ULONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG64 next = static_cast<ULONG64>(std::countr_zero(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG WINAPI RtlInterlockedCountTrailingZeros32(ULONG volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG> volatile*>(Destination);
    ULONG current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG next = static_cast<ULONG>(std::countr_zero(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline USHORT WINAPI RtlInterlockedCountTrailingZeros16(USHORT volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<USHORT> volatile*>(Destination);
    USHORT current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        USHORT next = static_cast<USHORT>(std::countr_zero(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline UCHAR WINAPI RtlInterlockedCountTrailingZeros8(UCHAR volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<UCHAR> volatile*>(Destination);
    UCHAR current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        UCHAR next = static_cast<UCHAR>(std::countr_zero(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG64 WINAPI RtlInterlockedCountSetBits64(ULONG64 volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG64> volatile*>(Destination);
    ULONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG64 next = static_cast<ULONG64>(std::popcount(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG WINAPI RtlInterlockedCountSetBits32(ULONG volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG> volatile*>(Destination);
    ULONG current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        ULONG next = static_cast<ULONG>(std::popcount(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline USHORT WINAPI RtlInterlockedCountSetBits16(USHORT volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<USHORT> volatile*>(Destination);
    USHORT current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        USHORT next = static_cast<USHORT>(std::popcount(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline UCHAR WINAPI RtlInterlockedCountSetBits8(UCHAR volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<UCHAR> volatile*>(Destination);
    UCHAR current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        UCHAR next = static_cast<UCHAR>(std::popcount(current));
        if (atomicDest->compare_exchange_weak(current, next, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG64 WINAPI RtlInterlockedCountLeadingSigns64(ULONG64 volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG64> volatile*>(Destination);
    ULONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        LONG64 sVal = static_cast<LONG64>(current);
        ULONG64 cls;
        if (sVal >= 0) {
            cls = (sVal == 0) ? 63 : static_cast<ULONG64>(std::countl_zero(static_cast<uint64_t>(sVal)) - 1);
        } else {
            cls = (sVal == -1LL) ? 63 : static_cast<ULONG64>(std::countl_one(static_cast<uint64_t>(sVal)) - 1);
        }
        if (atomicDest->compare_exchange_weak(current, cls, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline ULONG WINAPI RtlInterlockedCountLeadingSigns32(ULONG volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<ULONG> volatile*>(Destination);
    ULONG current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        LONG sVal = static_cast<LONG>(current);
        ULONG cls;
        if (sVal >= 0) {
            cls = (sVal == 0) ? 31 : static_cast<ULONG>(std::countl_zero(static_cast<uint32_t>(sVal)) - 1);
        } else {
            cls = (sVal == -1L) ? 31 : static_cast<ULONG>(std::countl_one(static_cast<uint32_t>(sVal)) - 1);
        }
        if (atomicDest->compare_exchange_weak(current, cls, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline USHORT WINAPI RtlInterlockedCountLeadingSigns16(USHORT volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<USHORT> volatile*>(Destination);
    USHORT current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        SHORT sVal = static_cast<SHORT>(current);
        USHORT cls;
        if (sVal >= 0) {
            cls = (sVal == 0) ? 15 : static_cast<USHORT>(std::countl_zero(static_cast<uint16_t>(sVal)) - 1);
        } else {
            cls = (sVal == -1) ? 15 : static_cast<USHORT>(std::countl_one(static_cast<uint16_t>(sVal)) - 1);
        }
        if (atomicDest->compare_exchange_weak(current, cls, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline UCHAR WINAPI RtlInterlockedCountLeadingSigns8(UCHAR volatile* Destination) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<UCHAR> volatile*>(Destination);
    UCHAR current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        CHAR sVal = static_cast<CHAR>(current);
        UCHAR cls;
        if (sVal >= 0) {
            cls = (sVal == 0) ? 7 : static_cast<UCHAR>(std::countl_zero(static_cast<uint8_t>(sVal)) - 1);
        } else {
            cls = (sVal == -1) ? 7 : static_cast<UCHAR>(std::countl_one(static_cast<uint8_t>(sVal)) - 1);
        }
        if (atomicDest->compare_exchange_weak(current, cls, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline LONG64 WINAPI RtlInterlockedAbsDiff64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    LONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        LONG64 diff = (current >= Value) ? (current - Value) : (Value - current);
        if (atomicDest->compare_exchange_weak(current, diff, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline LONG WINAPI RtlInterlockedAbsDiff32(LONG volatile* Destination, LONG Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    LONG current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        LONG diff = (current >= Value) ? (current - Value) : (Value - current);
        if (atomicDest->compare_exchange_weak(current, diff, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline SHORT WINAPI RtlInterlockedAbsDiff16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    SHORT current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        SHORT diff = static_cast<SHORT>((current >= Value) ? (current - Value) : (Value - current));
        if (atomicDest->compare_exchange_weak(current, diff, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline CHAR WINAPI RtlInterlockedAbsDiff8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    CHAR current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        CHAR diff = static_cast<CHAR>((current >= Value) ? (current - Value) : (Value - current));
        if (atomicDest->compare_exchange_weak(current, diff, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline LONG64 WINAPI RtlInterlockedAvgRoundUp64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    LONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        LONG64 avg = (current & Value) + ((current ^ Value) >> 1) + ((current ^ Value) & 1);
        if (atomicDest->compare_exchange_weak(current, avg, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline LONG WINAPI RtlInterlockedAvgRoundUp32(LONG volatile* Destination, LONG Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    LONG current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        LONG avg = (current & Value) + ((current ^ Value) >> 1) + ((current ^ Value) & 1);
        if (atomicDest->compare_exchange_weak(current, avg, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline SHORT WINAPI RtlInterlockedAvgRoundUp16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    SHORT current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        SHORT avg = static_cast<SHORT>((current & Value) + ((current ^ Value) >> 1) + ((current ^ Value) & 1));
        if (atomicDest->compare_exchange_weak(current, avg, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline CHAR WINAPI RtlInterlockedAvgRoundUp8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    CHAR current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        CHAR avg = static_cast<CHAR>((current & Value) + ((current ^ Value) >> 1) + ((current ^ Value) & 1));
        if (atomicDest->compare_exchange_weak(current, avg, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline LONG64 WINAPI RtlInterlockedAvgRoundDown64(LONG64 volatile* Destination, LONG64 Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG64> volatile*>(Destination);
    LONG64 current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        LONG64 avg = (current & Value) + ((current ^ Value) >> 1);
        if (atomicDest->compare_exchange_weak(current, avg, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline LONG WINAPI RtlInterlockedAvgRoundDown32(LONG volatile* Destination, LONG Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<LONG> volatile*>(Destination);
    LONG current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        LONG avg = (current & Value) + ((current ^ Value) >> 1);
        if (atomicDest->compare_exchange_weak(current, avg, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline SHORT WINAPI RtlInterlockedAvgRoundDown16(SHORT volatile* Destination, SHORT Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<SHORT> volatile*>(Destination);
    SHORT current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        SHORT avg = static_cast<SHORT>((current & Value) + ((current ^ Value) >> 1));
        if (atomicDest->compare_exchange_weak(current, avg, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline CHAR WINAPI RtlInterlockedAvgRoundDown8(CHAR volatile* Destination, CHAR Value) noexcept {
    if (!Destination) return 0;
    auto* atomicDest = reinterpret_cast<std::atomic<CHAR> volatile*>(Destination);
    CHAR current = atomicDest->load(std::memory_order_relaxed);
    while (true) {
        CHAR avg = static_cast<CHAR>((current & Value) + ((current ^ Value) >> 1));
        if (atomicDest->compare_exchange_weak(current, avg, std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return current;
        }
    }
}

inline NtStatus WINAPI RtlQuerySystemPagefileCompressionTelemetry(SYSTEM_PAGEFILE_COMPRESSION_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(251, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuCooperativeFabric(SYSTEM_CPU_COOPERATIVE_FABRIC_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(252, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileDecompressionTelemetry(SYSTEM_PAGEFILE_DECOMPRESSION_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(253, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricPartition(SYSTEM_CPU_FABRIC_PARTITION_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(254, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileClusterCompactionTelemetry(SYSTEM_PAGEFILE_CLUSTER_COMPACTION_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(255, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricQosTelemetry(SYSTEM_CPU_FABRIC_QOS_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(256, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileClusterDefragmentationTelemetry(SYSTEM_PAGEFILE_CLUSTER_DEFRAGMENTATION_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(257, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricLatencyTelemetry(SYSTEM_CPU_FABRIC_LATENCY_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(258, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileExtentMigrationTelemetry(SYSTEM_PAGEFILE_EXTENT_MIGRATION_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(259, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricCongestionTelemetry(SYSTEM_CPU_FABRIC_CONGESTION_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(260, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierExpulsionTelemetry(SYSTEM_PAGEFILE_TIER_EXPULSION_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(261, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricBandwidthPartitionTelemetry(SYSTEM_CPU_FABRIC_BANDWIDTH_PARTITION_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(262, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierIoQueueTelemetry(SYSTEM_PAGEFILE_TIER_IO_QUEUE_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(263, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricPowerStateTelemetry(SYSTEM_CPU_FABRIC_POWER_STATE_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(264, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierWearLevelingTelemetry(SYSTEM_PAGEFILE_TIER_WEAR_LEVELING_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(265, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricVoltageDomainTelemetry(SYSTEM_CPU_FABRIC_VOLTAGE_DOMAIN_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(266, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierFailoverTelemetry(SYSTEM_PAGEFILE_TIER_FAILOVER_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(267, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricClockStretchingTelemetry(SYSTEM_CPU_FABRIC_CLOCK_STRETCHING_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(268, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierMigrationQos(SYSTEM_PAGEFILE_TIER_MIGRATION_QOS_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(269, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricThermalThrottleTelemetry(SYSTEM_CPU_FABRIC_THERMAL_THROTTLE_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(270, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierDynamicDrain(SYSTEM_PAGEFILE_TIER_DYNAMIC_DRAIN_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(271, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricVoltageDroopPredictive(SYSTEM_CPU_FABRIC_VOLTAGE_DROOP_PREDICTIVE_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(272, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierPressurePredictive(SYSTEM_PAGEFILE_TIER_PRESSURE_PREDICTIVE_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(273, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricVoltageResonanceTelemetry(SYSTEM_CPU_FABRIC_VOLTAGE_RESONANCE_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(274, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierThrottlingTelemetry(SYSTEM_PAGEFILE_TIER_THROTTLING_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(275, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricVoltagePhaseSpreadSpectrum(SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_SPREAD_SPECTRUM_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(276, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierCompactionPressure(SYSTEM_PAGEFILE_TIER_COMPACTION_PRESSURE_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(277, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricVoltageSlewRateTelemetry(SYSTEM_CPU_FABRIC_VOLTAGE_SLEW_RATE_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(278, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierExpansionRateTelemetry(SYSTEM_PAGEFILE_TIER_EXPANSION_RATE_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(279, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricVoltagePhaseMarginTelemetry(SYSTEM_CPU_FABRIC_VOLTAGE_PHASE_MARGIN_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(280, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierCompactDedupEfficiency(SYSTEM_PAGEFILE_TIER_COMPACT_DEDUP_EFFICIENCY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(281, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricVoltageTransitionalDroopIndex(SYSTEM_CPU_FABRIC_VOLTAGE_TRANSITIONAL_DROOP_INDEX_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(282, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierEvictionPolicyTelemetry(SYSTEM_PAGEFILE_TIER_EVICTION_POLICY_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(283, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuFabricVoltageDloadTransientMargin(SYSTEM_CPU_FABRIC_VOLTAGE_DLOAD_TRANSIENT_MARGIN_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(284, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierReclaimEfficiency(SYSTEM_PAGEFILE_TIER_RECLAIM_EFFICIENCY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(285, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuCoreVoltageGuardBandHeadroom(SYSTEM_CPU_CORE_VOLTAGE_GUARD_BAND_HEADROOM_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(286, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemPagefileTierMigrationLatency(SYSTEM_PAGEFILE_TIER_MIGRATION_LATENCY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(287, Info, sizeof(*Info), &retLen);
}

inline NtStatus WINAPI RtlQuerySystemCpuCoreVoltageDroopPreventionTelemetry(SYSTEM_CPU_CORE_VOLTAGE_DROOP_PREVENTION_TELEMETRY_INFORMATION* Info) noexcept {
    if (!Info) return NtStatus::InvalidParameter;
    uint32_t retLen = 0;
    return NtQuerySystemInformation_Export(288, Info, sizeof(*Info), &retLen);
}

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

namespace details {
    inline const auto s_systemBootTime = std::chrono::steady_clock::now();
}

inline NtStatus WINAPI RtlQueryUnbiasedInterruptTimePrecise(uint64_t* InterruptTime) noexcept {
    if (!InterruptTime) return NtStatus::InvalidParameter;
    auto now = std::chrono::steady_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - details::s_systemBootTime).count();
    *InterruptTime = static_cast<uint64_t>(nanos / 100) + 10000000ULL;
    return NtStatus::Success;
}

inline NtStatus WINAPI RtlQueryInterruptTimePrecise(uint64_t* InterruptTime) noexcept {
    if (!InterruptTime) return NtStatus::InvalidParameter;
    auto now = std::chrono::steady_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - details::s_systemBootTime).count();
    // InterruptTime includes unbiased time plus system sleep/hibernation bias
    *InterruptTime = static_cast<uint64_t>(nanos / 100) + 10000000ULL + 50000000ULL;
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
    ldr.registerExport("ntdll.dll", "NtQueryInformationProcess", reinterpret_cast<void*>(NtQueryInformationProcess_Export));
    ldr.registerExport("ntdll.dll", "NtQueryInformationFile", reinterpret_cast<void*>(NtQueryInformationFile_Export));
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
    ldr.registerExport("ntdll.dll", "NtCreatePartition", reinterpret_cast<void*>(NtCreatePartition));
    ldr.registerExport("ntdll.dll", "NtOpenPartition", reinterpret_cast<void*>(NtOpenPartition));
    ldr.registerExport("ntdll.dll", "NtManagePartition", reinterpret_cast<void*>(NtManagePartition));
    ldr.registerExport("ntdll.dll", "ZwCreatePartition", reinterpret_cast<void*>(NtCreatePartition));
    ldr.registerExport("ntdll.dll", "ZwOpenPartition", reinterpret_cast<void*>(NtOpenPartition));
    ldr.registerExport("ntdll.dll", "ZwManagePartition", reinterpret_cast<void*>(NtManagePartition));
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
    ldr.registerExport("ntdll.dll", "RtlQueryDynamicTimeZoneInformation", reinterpret_cast<void*>(RtlQueryDynamicTimeZoneInformation));
    ldr.registerExport("ntdll.dll", "NtQueryTimer", reinterpret_cast<void*>(NtQueryTimer));
    ldr.registerExport("ntdll.dll", "NtQueryTimerResolution", reinterpret_cast<void*>(NtQueryTimerResolution));
    ldr.registerExport("ntdll.dll", "NtSetTimerResolution", reinterpret_cast<void*>(NtSetTimerResolution));
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
    ldr.registerExport("ntdll.dll", "RtlAddVectoredExceptionHandler", reinterpret_cast<void*>(RtlAddVectoredExceptionHandler));
    ldr.registerExport("ntdll.dll", "RtlRemoveVectoredExceptionHandler", reinterpret_cast<void*>(RtlRemoveVectoredExceptionHandler));
    ldr.registerExport("ntdll.dll", "RtlAddVectoredContinueHandler", reinterpret_cast<void*>(RtlAddVectoredContinueHandler));
    ldr.registerExport("ntdll.dll", "RtlRemoveVectoredContinueHandler", reinterpret_cast<void*>(RtlRemoveVectoredContinueHandler));
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
    ldr.registerExport("ntdll.dll", "RtlQueueWorkItem", reinterpret_cast<void*>(RtlQueueWorkItem));
    ldr.registerExport("ntdll.dll", "RtlCreateTimerQueue", reinterpret_cast<void*>(RtlCreateTimerQueue));
    ldr.registerExport("ntdll.dll", "RtlCreateTimer", reinterpret_cast<void*>(RtlCreateTimer));
    ldr.registerExport("ntdll.dll", "RtlUpdateTimer", reinterpret_cast<void*>(RtlUpdateTimer));
    ldr.registerExport("ntdll.dll", "RtlDeleteTimer", reinterpret_cast<void*>(RtlDeleteTimer));
    ldr.registerExport("ntdll.dll", "RtlDeleteTimerQueueEx", reinterpret_cast<void*>(RtlDeleteTimerQueueEx));
    ldr.registerExport("ntdll.dll", "RtlDeleteTimerQueue", reinterpret_cast<void*>(RtlDeleteTimerQueue));
    ldr.registerExport("ntdll.dll", "RtlDosPathNameToNtPathName_U_WithStatus", reinterpret_cast<void*>(RtlDosPathNameToNtPathName_U_WithStatus));
    ldr.registerExport("ntdll.dll", "RtlDowncaseUnicodeChar", reinterpret_cast<void*>(RtlDowncaseUnicodeChar));
    ldr.registerExport("ntdll.dll", "RtlUpcaseUnicodeChar", reinterpret_cast<void*>(RtlUpcaseUnicodeChar));
    ldr.registerExport("ntdll.dll", "RtlEncodePointer", reinterpret_cast<void*>(RtlEncodePointer));
    ldr.registerExport("ntdll.dll", "RtlExpandEnvironmentStrings_U", reinterpret_cast<void*>(RtlExpandEnvironmentStrings_U));
    ldr.registerExport("ntdll.dll", "RtlFindMessage", reinterpret_cast<void*>(RtlFindMessage));
    ldr.registerExport("ntdll.dll", "RtlFirstEntrySList", reinterpret_cast<void*>(RtlFirstEntrySList));
    ldr.registerExport("ntdll.dll", "RtlInitializeSListHead", reinterpret_cast<void*>(RtlInitializeSListHead));
    ldr.registerExport("ntdll.dll", "RtlInitializeSListHeadEx", reinterpret_cast<void*>(RtlInitializeSListHeadEx));
    ldr.registerExport("ntdll.dll", "RtlCompactSListHead", reinterpret_cast<void*>(RtlCompactSListHead));
    ldr.registerExport("ntdll.dll", "RtlQueryDepthSList", reinterpret_cast<void*>(RtlQueryDepthSList));
    ldr.registerExport("ntdll.dll", "RtlInterlockedFlushSList", reinterpret_cast<void*>(RtlInterlockedFlushSList));
    ldr.registerExport("ntdll.dll", "RtlInterlockedPopEntrySList", reinterpret_cast<void*>(RtlInterlockedPopEntrySList));
    ldr.registerExport("ntdll.dll", "RtlInterlockedPushEntrySList", reinterpret_cast<void*>(RtlInterlockedPushEntrySList));
    ldr.registerExport("ntdll.dll", "RtlInterlockedPushListSListEx", reinterpret_cast<void*>(RtlInterlockedPushListSListEx));
    ldr.registerExport("ntdll.dll", "RtlInterlockedPushEntrySListEx", reinterpret_cast<void*>(RtlInterlockedPushEntrySListEx));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCompareExchangePointer", reinterpret_cast<void*>(RtlInterlockedCompareExchangePointer));
    ldr.registerExport("ntdll.dll", "RtlInterlockedExchangePointer", reinterpret_cast<void*>(RtlInterlockedExchangePointer));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAddLargeInteger", reinterpret_cast<void*>(RtlInterlockedAddLargeInteger));
    ldr.registerExport("ntdll.dll", "RtlFindLeastSignificantBit", reinterpret_cast<void*>(RtlFindLeastSignificantBit));
    ldr.registerExport("ntdll.dll", "RtlFindMostSignificantBit", reinterpret_cast<void*>(RtlFindMostSignificantBit));
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
    ldr.registerExport("ntdll.dll", "RtlQueryUnbiasedInterruptTimePrecise", reinterpret_cast<void*>(RtlQueryUnbiasedInterruptTimePrecise));
    ldr.registerExport("ntdll.dll", "RtlQueryInterruptTimePrecise", reinterpret_cast<void*>(RtlQueryInterruptTimePrecise));
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
    ldr.registerExport("ntdll.dll", "RtlFillMemoryUlong", reinterpret_cast<void*>(RtlFillMemoryUlong));
    ldr.registerExport("ntdll.dll", "RtlFillMemoryUlonglong", reinterpret_cast<void*>(RtlFillMemoryUlonglong));
    ldr.registerExport("ntdll.dll", "RtlZeroMemoryEx", reinterpret_cast<void*>(RtlZeroMemoryEx));
    ldr.registerExport("ntdll.dll", "RtlCompareMemoryUlong", reinterpret_cast<void*>(RtlCompareMemoryUlong));
    ldr.registerExport("ntdll.dll", "RtlInterlockedBitTestAndSet", reinterpret_cast<void*>(RtlInterlockedBitTestAndSet));
    ldr.registerExport("ntdll.dll", "RtlInterlockedBitTestAndReset", reinterpret_cast<void*>(RtlInterlockedBitTestAndReset));
    ldr.registerExport("ntdll.dll", "RtlInterlockedBitTestAndSet64", reinterpret_cast<void*>(RtlInterlockedBitTestAndSet64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedBitTestAndReset64", reinterpret_cast<void*>(RtlInterlockedBitTestAndReset64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedBitTestAndComplement", reinterpret_cast<void*>(RtlInterlockedBitTestAndComplement));
    ldr.registerExport("ntdll.dll", "RtlInterlockedBitTestAndComplement64", reinterpret_cast<void*>(RtlInterlockedBitTestAndComplement64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAnd", reinterpret_cast<void*>(RtlInterlockedAnd));
    ldr.registerExport("ntdll.dll", "RtlInterlockedOr", reinterpret_cast<void*>(RtlInterlockedOr));
    ldr.registerExport("ntdll.dll", "RtlInterlockedXor", reinterpret_cast<void*>(RtlInterlockedXor));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAnd64", reinterpret_cast<void*>(RtlInterlockedAnd64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedOr64", reinterpret_cast<void*>(RtlInterlockedOr64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedXor64", reinterpret_cast<void*>(RtlInterlockedXor64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAndNot", reinterpret_cast<void*>(RtlInterlockedAndNot));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAndNot64", reinterpret_cast<void*>(RtlInterlockedAndNot64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCompareExchange64", reinterpret_cast<void*>(RtlInterlockedCompareExchange64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedExchange64", reinterpret_cast<void*>(RtlInterlockedExchange64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedExchangeAdd64", reinterpret_cast<void*>(RtlInterlockedExchangeAdd64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedIncrement64", reinterpret_cast<void*>(RtlInterlockedIncrement64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedDecrement64", reinterpret_cast<void*>(RtlInterlockedDecrement64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCompareExchange16", reinterpret_cast<void*>(RtlInterlockedCompareExchange16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedExchange16", reinterpret_cast<void*>(RtlInterlockedExchange16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedExchangeAdd16", reinterpret_cast<void*>(RtlInterlockedExchangeAdd16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedIncrement16", reinterpret_cast<void*>(RtlInterlockedIncrement16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedDecrement16", reinterpret_cast<void*>(RtlInterlockedDecrement16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAnd16", reinterpret_cast<void*>(RtlInterlockedAnd16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedOr16", reinterpret_cast<void*>(RtlInterlockedOr16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedXor16", reinterpret_cast<void*>(RtlInterlockedXor16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCompareExchange8", reinterpret_cast<void*>(RtlInterlockedCompareExchange8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedExchange8", reinterpret_cast<void*>(RtlInterlockedExchange8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedExchangeAdd8", reinterpret_cast<void*>(RtlInterlockedExchangeAdd8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedIncrement8", reinterpret_cast<void*>(RtlInterlockedIncrement8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedDecrement8", reinterpret_cast<void*>(RtlInterlockedDecrement8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAnd8", reinterpret_cast<void*>(RtlInterlockedAnd8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedOr8", reinterpret_cast<void*>(RtlInterlockedOr8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedXor8", reinterpret_cast<void*>(RtlInterlockedXor8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAndNot16", reinterpret_cast<void*>(RtlInterlockedAndNot16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAndNot8", reinterpret_cast<void*>(RtlInterlockedAndNot8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedXorNot64", reinterpret_cast<void*>(RtlInterlockedXorNot64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedXorNot32", reinterpret_cast<void*>(RtlInterlockedXorNot32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedXorNot16", reinterpret_cast<void*>(RtlInterlockedXorNot16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedXorNot8", reinterpret_cast<void*>(RtlInterlockedXorNot8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedNand64", reinterpret_cast<void*>(RtlInterlockedNand64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedNand32", reinterpret_cast<void*>(RtlInterlockedNand32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedNand16", reinterpret_cast<void*>(RtlInterlockedNand16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedNand8", reinterpret_cast<void*>(RtlInterlockedNand8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedNor64", reinterpret_cast<void*>(RtlInterlockedNor64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedNor32", reinterpret_cast<void*>(RtlInterlockedNor32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedNor16", reinterpret_cast<void*>(RtlInterlockedNor16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedNor8", reinterpret_cast<void*>(RtlInterlockedNor8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedXnor64", reinterpret_cast<void*>(RtlInterlockedXnor64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedXnor32", reinterpret_cast<void*>(RtlInterlockedXnor32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedXnor16", reinterpret_cast<void*>(RtlInterlockedXnor16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedXnor8", reinterpret_cast<void*>(RtlInterlockedXnor8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedMin64", reinterpret_cast<void*>(RtlInterlockedMin64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedMin32", reinterpret_cast<void*>(RtlInterlockedMin32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedMin16", reinterpret_cast<void*>(RtlInterlockedMin16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedMin8", reinterpret_cast<void*>(RtlInterlockedMin8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedMax64", reinterpret_cast<void*>(RtlInterlockedMax64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedMax32", reinterpret_cast<void*>(RtlInterlockedMax32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedMax16", reinterpret_cast<void*>(RtlInterlockedMax16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedMax8", reinterpret_cast<void*>(RtlInterlockedMax8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedClamp64", reinterpret_cast<void*>(RtlInterlockedClamp64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedClamp32", reinterpret_cast<void*>(RtlInterlockedClamp32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedClamp16", reinterpret_cast<void*>(RtlInterlockedClamp16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedClamp8", reinterpret_cast<void*>(RtlInterlockedClamp8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAddModulo64", reinterpret_cast<void*>(RtlInterlockedAddModulo64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAddModulo32", reinterpret_cast<void*>(RtlInterlockedAddModulo32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAddModulo16", reinterpret_cast<void*>(RtlInterlockedAddModulo16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAddModulo8", reinterpret_cast<void*>(RtlInterlockedAddModulo8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedMulModulo64", reinterpret_cast<void*>(RtlInterlockedMulModulo64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedMulModulo32", reinterpret_cast<void*>(RtlInterlockedMulModulo32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedMulModulo16", reinterpret_cast<void*>(RtlInterlockedMulModulo16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedMulModulo8", reinterpret_cast<void*>(RtlInterlockedMulModulo8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedRotateRight64", reinterpret_cast<void*>(RtlInterlockedRotateRight64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedRotateRight32", reinterpret_cast<void*>(RtlInterlockedRotateRight32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedRotateRight16", reinterpret_cast<void*>(RtlInterlockedRotateRight16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedRotateRight8", reinterpret_cast<void*>(RtlInterlockedRotateRight8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedRotateLeft64", reinterpret_cast<void*>(RtlInterlockedRotateLeft64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedRotateLeft32", reinterpret_cast<void*>(RtlInterlockedRotateLeft32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedRotateLeft16", reinterpret_cast<void*>(RtlInterlockedRotateLeft16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedRotateLeft8", reinterpret_cast<void*>(RtlInterlockedRotateLeft8));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileCompressionTelemetry", reinterpret_cast<void*>(RtlQuerySystemPagefileCompressionTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuCooperativeFabric", reinterpret_cast<void*>(RtlQuerySystemCpuCooperativeFabric));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileDecompressionTelemetry", reinterpret_cast<void*>(RtlQuerySystemPagefileDecompressionTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricPartition", reinterpret_cast<void*>(RtlQuerySystemCpuFabricPartition));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileClusterCompactionTelemetry", reinterpret_cast<void*>(RtlQuerySystemPagefileClusterCompactionTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricQosTelemetry", reinterpret_cast<void*>(RtlQuerySystemCpuFabricQosTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileClusterDefragmentationTelemetry", reinterpret_cast<void*>(RtlQuerySystemPagefileClusterDefragmentationTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricLatencyTelemetry", reinterpret_cast<void*>(RtlQuerySystemCpuFabricLatencyTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileExtentMigrationTelemetry", reinterpret_cast<void*>(RtlQuerySystemPagefileExtentMigrationTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricCongestionTelemetry", reinterpret_cast<void*>(RtlQuerySystemCpuFabricCongestionTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierExpulsionTelemetry", reinterpret_cast<void*>(RtlQuerySystemPagefileTierExpulsionTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricBandwidthPartitionTelemetry", reinterpret_cast<void*>(RtlQuerySystemCpuFabricBandwidthPartitionTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierIoQueueTelemetry", reinterpret_cast<void*>(RtlQuerySystemPagefileTierIoQueueTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricPowerStateTelemetry", reinterpret_cast<void*>(RtlQuerySystemCpuFabricPowerStateTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierWearLevelingTelemetry", reinterpret_cast<void*>(RtlQuerySystemPagefileTierWearLevelingTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricVoltageDomainTelemetry", reinterpret_cast<void*>(RtlQuerySystemCpuFabricVoltageDomainTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierFailoverTelemetry", reinterpret_cast<void*>(RtlQuerySystemPagefileTierFailoverTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricClockStretchingTelemetry", reinterpret_cast<void*>(RtlQuerySystemCpuFabricClockStretchingTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierMigrationQos", reinterpret_cast<void*>(RtlQuerySystemPagefileTierMigrationQos));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricThermalThrottleTelemetry", reinterpret_cast<void*>(RtlQuerySystemCpuFabricThermalThrottleTelemetry));
    ldr.registerExport("ntdll.dll", "RtlInterlockedBitReverse64", reinterpret_cast<void*>(RtlInterlockedBitReverse64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedBitReverse32", reinterpret_cast<void*>(RtlInterlockedBitReverse32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedBitReverse16", reinterpret_cast<void*>(RtlInterlockedBitReverse16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedBitReverse8", reinterpret_cast<void*>(RtlInterlockedBitReverse8));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierDynamicDrain", reinterpret_cast<void*>(RtlQuerySystemPagefileTierDynamicDrain));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricVoltageDroopPredictive", reinterpret_cast<void*>(RtlQuerySystemCpuFabricVoltageDroopPredictive));
    ldr.registerExport("ntdll.dll", "RtlInterlockedPopCount64", reinterpret_cast<void*>(RtlInterlockedPopCount64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedPopCount32", reinterpret_cast<void*>(RtlInterlockedPopCount32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedPopCount16", reinterpret_cast<void*>(RtlInterlockedPopCount16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedPopCount8", reinterpret_cast<void*>(RtlInterlockedPopCount8));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierPressurePredictive", reinterpret_cast<void*>(RtlQuerySystemPagefileTierPressurePredictive));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricVoltageResonanceTelemetry", reinterpret_cast<void*>(RtlQuerySystemCpuFabricVoltageResonanceTelemetry));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountLeadingZeros64", reinterpret_cast<void*>(RtlInterlockedCountLeadingZeros64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountLeadingZeros32", reinterpret_cast<void*>(RtlInterlockedCountLeadingZeros32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountLeadingZeros16", reinterpret_cast<void*>(RtlInterlockedCountLeadingZeros16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountLeadingZeros8", reinterpret_cast<void*>(RtlInterlockedCountLeadingZeros8));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierThrottlingTelemetry", reinterpret_cast<void*>(RtlQuerySystemPagefileTierThrottlingTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricVoltagePhaseSpreadSpectrum", reinterpret_cast<void*>(RtlQuerySystemCpuFabricVoltagePhaseSpreadSpectrum));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountTrailingZeros64", reinterpret_cast<void*>(RtlInterlockedCountTrailingZeros64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountTrailingZeros32", reinterpret_cast<void*>(RtlInterlockedCountTrailingZeros32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountTrailingZeros16", reinterpret_cast<void*>(RtlInterlockedCountTrailingZeros16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountTrailingZeros8", reinterpret_cast<void*>(RtlInterlockedCountTrailingZeros8));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierCompactionPressure", reinterpret_cast<void*>(RtlQuerySystemPagefileTierCompactionPressure));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricVoltageSlewRateTelemetry", reinterpret_cast<void*>(RtlQuerySystemCpuFabricVoltageSlewRateTelemetry));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountSetBits64", reinterpret_cast<void*>(RtlInterlockedCountSetBits64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountSetBits32", reinterpret_cast<void*>(RtlInterlockedCountSetBits32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountSetBits16", reinterpret_cast<void*>(RtlInterlockedCountSetBits16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountSetBits8", reinterpret_cast<void*>(RtlInterlockedCountSetBits8));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountLeadingSigns64", reinterpret_cast<void*>(RtlInterlockedCountLeadingSigns64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountLeadingSigns32", reinterpret_cast<void*>(RtlInterlockedCountLeadingSigns32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountLeadingSigns16", reinterpret_cast<void*>(RtlInterlockedCountLeadingSigns16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedCountLeadingSigns8", reinterpret_cast<void*>(RtlInterlockedCountLeadingSigns8));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierExpansionRateTelemetry", reinterpret_cast<void*>(RtlQuerySystemPagefileTierExpansionRateTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricVoltagePhaseMarginTelemetry", reinterpret_cast<void*>(RtlQuerySystemCpuFabricVoltagePhaseMarginTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierCompactDedupEfficiency", reinterpret_cast<void*>(RtlQuerySystemPagefileTierCompactDedupEfficiency));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricVoltageTransitionalDroopIndex", reinterpret_cast<void*>(RtlQuerySystemCpuFabricVoltageTransitionalDroopIndex));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAbsDiff64", reinterpret_cast<void*>(RtlInterlockedAbsDiff64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAbsDiff32", reinterpret_cast<void*>(RtlInterlockedAbsDiff32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAbsDiff16", reinterpret_cast<void*>(RtlInterlockedAbsDiff16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAbsDiff8", reinterpret_cast<void*>(RtlInterlockedAbsDiff8));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierEvictionPolicyTelemetry", reinterpret_cast<void*>(RtlQuerySystemPagefileTierEvictionPolicyTelemetry));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuFabricVoltageDloadTransientMargin", reinterpret_cast<void*>(RtlQuerySystemCpuFabricVoltageDloadTransientMargin));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAvgRoundUp64", reinterpret_cast<void*>(RtlInterlockedAvgRoundUp64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAvgRoundUp32", reinterpret_cast<void*>(RtlInterlockedAvgRoundUp32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAvgRoundUp16", reinterpret_cast<void*>(RtlInterlockedAvgRoundUp16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAvgRoundUp8", reinterpret_cast<void*>(RtlInterlockedAvgRoundUp8));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierReclaimEfficiency", reinterpret_cast<void*>(RtlQuerySystemPagefileTierReclaimEfficiency));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuCoreVoltageGuardBandHeadroom", reinterpret_cast<void*>(RtlQuerySystemCpuCoreVoltageGuardBandHeadroom));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAvgRoundDown64", reinterpret_cast<void*>(RtlInterlockedAvgRoundDown64));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAvgRoundDown32", reinterpret_cast<void*>(RtlInterlockedAvgRoundDown32));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAvgRoundDown16", reinterpret_cast<void*>(RtlInterlockedAvgRoundDown16));
    ldr.registerExport("ntdll.dll", "RtlInterlockedAvgRoundDown8", reinterpret_cast<void*>(RtlInterlockedAvgRoundDown8));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemPagefileTierMigrationLatency", reinterpret_cast<void*>(RtlQuerySystemPagefileTierMigrationLatency));
    ldr.registerExport("ntdll.dll", "RtlQuerySystemCpuCoreVoltageDroopPreventionTelemetry", reinterpret_cast<void*>(RtlQuerySystemCpuCoreVoltageDroopPreventionTelemetry));

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

