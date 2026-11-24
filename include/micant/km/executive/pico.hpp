// ============================================================================
// MicaNT: Sovereign Clean-Room Windows NT Kernel Pico Process Subsystem
// Horizon 379 / Suite 595 - Pico Provider Architecture, Fast Syscall
// Interception, Linux Subsystem (LSW) Core Interlock, and Register Frame Dispatch
//
// Clean-Room Implementation in modern freestanding ISO C++23.
// Conforms to Microsoft Windows 10/11 Pico Process & Provider architecture
// (ntoskrnl.exe, lxcore.sys, lxss.sys). Zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <span>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <functional>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::pico {

// ============================================================================
// 1. Pico Register Frame & Attribute Definitions
// ============================================================================

struct alignas(16) PICO_CONTEXT_FRAME {
    uint64_t Rax{0};
    uint64_t Rbx{0};
    uint64_t Rcx{0};
    uint64_t Rdx{0};
    uint64_t Rsi{0};
    uint64_t Rdi{0};
    uint64_t Rbp{0};
    uint64_t Rsp{0};
    uint64_t R8{0};
    uint64_t R9{0};
    uint64_t R10{0};
    uint64_t R11{0};
    uint64_t R12{0};
    uint64_t R13{0};
    uint64_t R14{0};
    uint64_t R15{0};
    uint64_t Rip{0};
    uint64_t Rflags{0x202}; // IF set
    uint64_t FsBase{0};
    uint64_t GsBase{0};
};

// ============================================================================
// 2. Pico Provider & System Routines Function Signatures
// ============================================================================

using DispatchSystemCallFn = NtStatus(*)(
    void* processContext,
    void* threadContext,
    uint64_t syscallNumber,
    std::span<const uint64_t> args,
    uint64_t& outReturnValue
);

using ExitProcessFn = void(*)(void* processContext, uint32_t exitStatus);
using ExitThreadFn = void(*)(void* threadContext, uint32_t exitStatus);
using WalkUserStackFn = NtStatus(*)(void* threadContext, std::vector<uint64_t>& outStackFrames);
using GetContextFn = NtStatus(*)(void* threadContext, PICO_CONTEXT_FRAME& outContext);
using SetContextFn = NtStatus(*)(void* threadContext, const PICO_CONTEXT_FRAME& context);
using SystemDescriptorFn = NtStatus(*)(void* threadContext, uint32_t descriptorType, uint64_t baseAddress);

struct PS_PICO_PROVIDER_ROUTINES {
    uint32_t             StructureSize{sizeof(PS_PICO_PROVIDER_ROUTINES)};
    DispatchSystemCallFn DispatchSystemCall{nullptr};
    ExitProcessFn        ExitProcess{nullptr};
    ExitThreadFn         ExitThread{nullptr};
    WalkUserStackFn      WalkUserStack{nullptr};
    GetContextFn         GetContext{nullptr};
    SetContextFn         SetContext{nullptr};
    SystemDescriptorFn   SystemDescriptor{nullptr};
};

struct PS_PICO_SYSTEM_ROUTINES {
    uint32_t StructureSize{sizeof(PS_PICO_SYSTEM_ROUTINES)};
    NtStatus (*CreateProcess)(uint64_t parentPid, std::wstring_view name, void* ctx, uint64_t& outPid){nullptr};
    NtStatus (*CreateThread)(uint64_t pid, uint64_t entryPoint, uint64_t stack, void* ctx, uint64_t& outTid){nullptr};
    NtStatus (*GetProcessContext)(uint64_t pid, void*& outCtx){nullptr};
    NtStatus (*SetProcessContext)(uint64_t pid, void* ctx){nullptr};
    NtStatus (*GetThreadContext)(uint64_t tid, void*& outCtx){nullptr};
    NtStatus (*SetThreadContext)(uint64_t tid, void* ctx){nullptr};
    NtStatus (*TerminateProcess)(uint64_t pid, uint32_t exitStatus){nullptr};
    NtStatus (*ResumeThread)(uint64_t tid){nullptr};
};

// ============================================================================
// 3. Pico Process & Thread Internal Descriptors
// ============================================================================

struct PicoProcess {
    uint64_t              ProcessId{0};
    uint64_t              ParentProcessId{0};
    std::wstring          ImageName;
    void*                 ProviderContext{nullptr};
    bool                  IsActive{true};
    uint32_t              ExitStatus{0};
    std::vector<uint64_t> ThreadIds;
};

struct PicoThread {
    uint64_t           ThreadId{0};
    uint64_t           ProcessId{0};
    void*              ProviderContext{nullptr};
    PICO_CONTEXT_FRAME Context{};
    bool               IsRunning{false};
    bool               IsTerminated{false};
    uint32_t           ExitStatus{0};
};

// ============================================================================
// 4. Sovereign Pico Executive Subsystem
// ============================================================================

class SovereignPicoExecutive {
public:
    static SovereignPicoExecutive& Instance() noexcept {
        static SovereignPicoExecutive s_instance;
        return s_instance;
    }

    // Provider Registration
    NtStatus registerProvider(
        const PS_PICO_PROVIDER_ROUTINES& providerRoutines,
        PS_PICO_SYSTEM_ROUTINES& outSystemRoutines
    ) noexcept;

    void unregisterProvider() noexcept;
    [[nodiscard]] bool isProviderRegistered() const noexcept;

    // Process & Thread Lifecycle
    NtStatus createPicoProcess(
        uint64_t parentProcessId,
        std::wstring_view imageName,
        void* initialContext,
        uint64_t& outProcessId
    ) noexcept;

    NtStatus createPicoThread(
        uint64_t processId,
        uint64_t entryPoint,
        uint64_t stackPointer,
        void* initialContext,
        uint64_t& outThreadId
    ) noexcept;

    NtStatus resumeThread(uint64_t threadId) noexcept;
    NtStatus terminatePicoProcess(uint64_t processId, uint32_t exitStatus) noexcept;
    NtStatus terminatePicoThread(uint64_t threadId, uint32_t exitStatus) noexcept;

    // Syscall Interception Dispatcher
    NtStatus dispatchSystemCall(
        uint64_t threadId,
        uint64_t syscallNumber,
        std::span<const uint64_t> args,
        uint64_t& outReturnValue
    ) noexcept;

    // Context Accessors
    NtStatus getProcessContext(uint64_t processId, void*& outContext) const noexcept;
    NtStatus setProcessContext(uint64_t processId, void* context) noexcept;
    NtStatus getThreadContext(uint64_t threadId, void*& outContext) const noexcept;
    NtStatus setThreadContext(uint64_t threadId, void* context) noexcept;

    // Register State
    NtStatus getThreadRegisterContext(uint64_t threadId, PICO_CONTEXT_FRAME& outContext) const noexcept;
    NtStatus setThreadRegisterContext(uint64_t threadId, const PICO_CONTEXT_FRAME& context) noexcept;

    // Telemetry & Statistics
    [[nodiscard]] uint32_t getActiveProcessCount() const noexcept;
    [[nodiscard]] uint32_t getActiveThreadCount() const noexcept;
    [[nodiscard]] uint64_t getTotalSyscallsDispatched() const noexcept;

    void reset() noexcept;

private:
    SovereignPicoExecutive() = default;
    ~SovereignPicoExecutive() = default;

    mutable std::mutex m_mutex;
    bool m_providerRegistered{false};
    PS_PICO_PROVIDER_ROUTINES m_provider{};

    uint64_t m_nextProcessId{0x2000};
    uint64_t m_nextThreadId{0x4000};

    std::unordered_map<uint64_t, PicoProcess> m_processes;
    std::unordered_map<uint64_t, PicoThread>  m_threads;

    uint64_t m_totalSyscallsDispatched{0};
};

bool VerifyHorizon379PicoSubsystem();

} // namespace micant::pico
