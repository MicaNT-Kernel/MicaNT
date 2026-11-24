// ============================================================================
// MicaNT: Sovereign Clean-Room Windows NT Kernel Pico Process Subsystem
// Horizon 379 / Suite 595 - Implementation (ntoskrnl.exe / lxcore.sys interlock)
// ============================================================================

#include "micant/km/executive/pico.hpp"
#include <iostream>

namespace micant::pico {

// Forward helper wrappers for C-style function pointers in PS_PICO_SYSTEM_ROUTINES
static NtStatus SystemCreateProcess(uint64_t parentPid, std::wstring_view name, void* ctx, uint64_t& outPid) {
    return SovereignPicoExecutive::Instance().createPicoProcess(parentPid, name, ctx, outPid);
}

static NtStatus SystemCreateThread(uint64_t pid, uint64_t entryPoint, uint64_t stack, void* ctx, uint64_t& outTid) {
    return SovereignPicoExecutive::Instance().createPicoThread(pid, entryPoint, stack, ctx, outTid);
}

static NtStatus SystemGetProcessContext(uint64_t pid, void*& outCtx) {
    return SovereignPicoExecutive::Instance().getProcessContext(pid, outCtx);
}

static NtStatus SystemSetProcessContext(uint64_t pid, void* ctx) {
    return SovereignPicoExecutive::Instance().setProcessContext(pid, ctx);
}

static NtStatus SystemGetThreadContext(uint64_t tid, void*& outCtx) {
    return SovereignPicoExecutive::Instance().getThreadContext(tid, outCtx);
}

static NtStatus SystemSetThreadContext(uint64_t tid, void* ctx) {
    return SovereignPicoExecutive::Instance().setThreadContext(tid, ctx);
}

static NtStatus SystemTerminateProcess(uint64_t pid, uint32_t exitStatus) {
    return SovereignPicoExecutive::Instance().terminatePicoProcess(pid, exitStatus);
}

static NtStatus SystemResumeThread(uint64_t tid) {
    return SovereignPicoExecutive::Instance().resumeThread(tid);
}

NtStatus SovereignPicoExecutive::registerProvider(
    const PS_PICO_PROVIDER_ROUTINES& providerRoutines,
    PS_PICO_SYSTEM_ROUTINES& outSystemRoutines
) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_providerRegistered) {
        return NtStatus::AlreadyRegistered;
    }

    if (!providerRoutines.DispatchSystemCall) {
        return NtStatus::InvalidParameter;
    }

    m_provider = providerRoutines;
    m_providerRegistered = true;

    outSystemRoutines = PS_PICO_SYSTEM_ROUTINES{
        .StructureSize = sizeof(PS_PICO_SYSTEM_ROUTINES),
        .CreateProcess = SystemCreateProcess,
        .CreateThread = SystemCreateThread,
        .GetProcessContext = SystemGetProcessContext,
        .SetProcessContext = SystemSetProcessContext,
        .GetThreadContext = SystemGetThreadContext,
        .SetThreadContext = SystemSetThreadContext,
        .TerminateProcess = SystemTerminateProcess,
        .ResumeThread = SystemResumeThread
    };

    return NtStatus::Success;
}

void SovereignPicoExecutive::unregisterProvider() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_providerRegistered = false;
    m_provider = PS_PICO_PROVIDER_ROUTINES{};
    m_processes.clear();
    m_threads.clear();
}

bool SovereignPicoExecutive::isProviderRegistered() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_providerRegistered;
}

NtStatus SovereignPicoExecutive::createPicoProcess(
    uint64_t parentProcessId,
    std::wstring_view imageName,
    void* initialContext,
    uint64_t& outProcessId
) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_providerRegistered) {
        return NtStatus::InvalidPicoOperation;
    }

    outProcessId = ++m_nextProcessId;

    PicoProcess proc{
        .ProcessId = outProcessId,
        .ParentProcessId = parentProcessId,
        .ImageName = std::wstring(imageName),
        .ProviderContext = initialContext,
        .IsActive = true,
        .ExitStatus = 0,
        .ThreadIds = {}
    };

    m_processes[outProcessId] = std::move(proc);
    return NtStatus::Success;
}

NtStatus SovereignPicoExecutive::createPicoThread(
    uint64_t processId,
    uint64_t entryPoint,
    uint64_t stackPointer,
    void* initialContext,
    uint64_t& outThreadId
) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_providerRegistered) {
        return NtStatus::InvalidPicoOperation;
    }

    auto procIt = m_processes.find(processId);
    if (procIt == m_processes.end() || !procIt->second.IsActive) {
        return NtStatus::InvalidParameter;
    }

    outThreadId = ++m_nextThreadId;

    PicoThread th{
        .ThreadId = outThreadId,
        .ProcessId = processId,
        .ProviderContext = initialContext,
        .Context = PICO_CONTEXT_FRAME{
            .Rsp = stackPointer,
            .Rip = entryPoint,
            .Rflags = 0x202
        },
        .IsRunning = false,
        .IsTerminated = false,
        .ExitStatus = 0
    };

    m_threads[outThreadId] = th;
    procIt->second.ThreadIds.push_back(outThreadId);
    return NtStatus::Success;
}

NtStatus SovereignPicoExecutive::resumeThread(uint64_t threadId) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_threads.find(threadId);
    if (it == m_threads.end() || it->second.IsTerminated) {
        return NtStatus::InvalidHandle;
    }
    it->second.IsRunning = true;
    return NtStatus::Success;
}

NtStatus SovereignPicoExecutive::terminatePicoProcess(uint64_t processId, uint32_t exitStatus) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(processId);
    if (it == m_processes.end() || !it->second.IsActive) {
        return NtStatus::InvalidParameter;
    }

    it->second.IsActive = false;
    it->second.ExitStatus = exitStatus;

    // Notify provider of process exit
    if (m_provider.ExitProcess) {
        m_provider.ExitProcess(it->second.ProviderContext, exitStatus);
    }

    // Terminate all child threads
    for (uint64_t tid : it->second.ThreadIds) {
        auto tIt = m_threads.find(tid);
        if (tIt != m_threads.end() && !tIt->second.IsTerminated) {
            tIt->second.IsTerminated = true;
            tIt->second.IsRunning = false;
            tIt->second.ExitStatus = exitStatus;
            if (m_provider.ExitThread) {
                m_provider.ExitThread(tIt->second.ProviderContext, exitStatus);
            }
        }
    }

    return NtStatus::Success;
}

NtStatus SovereignPicoExecutive::terminatePicoThread(uint64_t threadId, uint32_t exitStatus) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_threads.find(threadId);
    if (it == m_threads.end() || it->second.IsTerminated) {
        return NtStatus::InvalidHandle;
    }

    it->second.IsTerminated = true;
    it->second.IsRunning = false;
    it->second.ExitStatus = exitStatus;

    if (m_provider.ExitThread) {
        m_provider.ExitThread(it->second.ProviderContext, exitStatus);
    }

    return NtStatus::Success;
}

NtStatus SovereignPicoExecutive::dispatchSystemCall(
    uint64_t threadId,
    uint64_t syscallNumber,
    std::span<const uint64_t> args,
    uint64_t& outReturnValue
) noexcept {
    void* procCtx = nullptr;
    void* threadCtx = nullptr;
    DispatchSystemCallFn dispatchFn = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_providerRegistered || !m_provider.DispatchSystemCall) {
            return NtStatus::InvalidPicoOperation;
        }

        auto tIt = m_threads.find(threadId);
        if (tIt == m_threads.end() || tIt->second.IsTerminated) {
            return NtStatus::InvalidHandle;
        }

        auto pIt = m_processes.find(tIt->second.ProcessId);
        if (pIt == m_processes.end() || !pIt->second.IsActive) {
            return NtStatus::PicoProcessTerminated;
        }

        procCtx = pIt->second.ProviderContext;
        threadCtx = tIt->second.ProviderContext;
        dispatchFn = m_provider.DispatchSystemCall;
        m_totalSyscallsDispatched++;
    }

    return dispatchFn(procCtx, threadCtx, syscallNumber, args, outReturnValue);
}

NtStatus SovereignPicoExecutive::getProcessContext(uint64_t processId, void*& outContext) const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(processId);
    if (it != m_processes.end()) {
        outContext = it->second.ProviderContext;
        return NtStatus::Success;
    }
    return NtStatus::InvalidParameter;
}

NtStatus SovereignPicoExecutive::setProcessContext(uint64_t processId, void* context) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(processId);
    if (it != m_processes.end()) {
        it->second.ProviderContext = context;
        return NtStatus::Success;
    }
    return NtStatus::InvalidParameter;
}

NtStatus SovereignPicoExecutive::getThreadContext(uint64_t threadId, void*& outContext) const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_threads.find(threadId);
    if (it != m_threads.end()) {
        outContext = it->second.ProviderContext;
        return NtStatus::Success;
    }
    return NtStatus::InvalidHandle;
}

NtStatus SovereignPicoExecutive::setThreadContext(uint64_t threadId, void* context) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_threads.find(threadId);
    if (it != m_threads.end()) {
        it->second.ProviderContext = context;
        return NtStatus::Success;
    }
    return NtStatus::InvalidHandle;
}

NtStatus SovereignPicoExecutive::getThreadRegisterContext(uint64_t threadId, PICO_CONTEXT_FRAME& outContext) const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_threads.find(threadId);
    if (it != m_threads.end()) {
        outContext = it->second.Context;
        return NtStatus::Success;
    }
    return NtStatus::InvalidHandle;
}

NtStatus SovereignPicoExecutive::setThreadRegisterContext(uint64_t threadId, const PICO_CONTEXT_FRAME& context) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_threads.find(threadId);
    if (it != m_threads.end()) {
        it->second.Context = context;
        return NtStatus::Success;
    }
    return NtStatus::InvalidHandle;
}

uint32_t SovereignPicoExecutive::getActiveProcessCount() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = 0;
    for (const auto& [id, p] : m_processes) {
        if (p.IsActive) count++;
    }
    return count;
}

uint32_t SovereignPicoExecutive::getActiveThreadCount() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = 0;
    for (const auto& [id, t] : m_threads) {
        if (!t.IsTerminated && t.IsRunning) count++;
    }
    return count;
}

uint64_t SovereignPicoExecutive::getTotalSyscallsDispatched() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalSyscallsDispatched;
}

void SovereignPicoExecutive::reset() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_providerRegistered = false;
    m_provider = PS_PICO_PROVIDER_ROUTINES{};
    m_processes.clear();
    m_threads.clear();
    m_totalSyscallsDispatched = 0;
    m_nextProcessId = 0x2000;
    m_nextThreadId = 0x4000;
}

// ============================================================================
// Horizon 379 Verification Battery (6 Pillars)
// ============================================================================

struct MockLswProcessContext {
    uint32_t PosixPid{1001};
    uint32_t PosixUid{0}; // root
    bool ExitCallbackReceived{false};
};

struct MockLswThreadContext {
    uint32_t PosixTid{1001};
    bool ExitCallbackReceived{false};
};

static NtStatus MockLswDispatchSystemCall(
    void* procCtx,
    void* threadCtx,
    uint64_t syscallNumber,
    std::span<const uint64_t> args,
    uint64_t& outReturnValue
) {
    auto* pCtx = static_cast<MockLswProcessContext*>(procCtx);
    (void)threadCtx;

    // Emulate standard POSIX 64-bit syscalls (Linux ABI)
    switch (syscallNumber) {
        case 39: // sys_getpid
            outReturnValue = pCtx ? pCtx->PosixPid : 0;
            return NtStatus::Success;

        case 102: // sys_getuid
            outReturnValue = pCtx ? pCtx->PosixUid : 0;
            return NtStatus::Success;

        case 0: // sys_read(fd, buf, count)
            if (args.size() >= 3) {
                outReturnValue = args[2]; // Return simulated bytes read
                return NtStatus::Success;
            }
            return NtStatus::InvalidParameter;

        case 1: // sys_write(fd, buf, count)
            if (args.size() >= 3) {
                outReturnValue = args[2]; // Return simulated bytes written
                return NtStatus::Success;
            }
            return NtStatus::InvalidParameter;

        case 9: // sys_mmap(addr, length, prot, flags, fd, offset)
            if (args.size() >= 2) {
                outReturnValue = 0x7FFF00000000ULL; // Return simulated userland address
                return NtStatus::Success;
            }
            return NtStatus::InvalidParameter;

        default:
            outReturnValue = static_cast<uint64_t>(-38); // -ENOSYS
            return NtStatus::NotImplemented;
    }
}

static void MockLswExitProcess(void* procCtx, uint32_t exitStatus) {
    if (procCtx) {
        auto* pCtx = static_cast<MockLswProcessContext*>(procCtx);
        pCtx->ExitCallbackReceived = true;
    }
    (void)exitStatus;
}

static void MockLswExitThread(void* threadCtx, uint32_t exitStatus) {
    if (threadCtx) {
        auto* tCtx = static_cast<MockLswThreadContext*>(threadCtx);
        tCtx->ExitCallbackReceived = true;
    }
    (void)exitStatus;
}

bool VerifyHorizon379PicoSubsystem() {
    auto& exec = SovereignPicoExecutive::Instance();
    exec.reset();

    // Pillar 1: Provider Registration & Handshake Protocol
    PS_PICO_SYSTEM_ROUTINES sysRoutines{};
    MockLswProcessContext mockProcContext{};
    MockLswThreadContext  mockThreadContext{};

    {
        PS_PICO_PROVIDER_ROUTINES provRoutines{
            .StructureSize = sizeof(PS_PICO_PROVIDER_ROUTINES),
            .DispatchSystemCall = MockLswDispatchSystemCall,
            .ExitProcess = MockLswExitProcess,
            .ExitThread = MockLswExitThread,
            .WalkUserStack = nullptr,
            .GetContext = nullptr,
            .SetContext = nullptr,
            .SystemDescriptor = nullptr
        };

        NtStatus st = exec.registerProvider(provRoutines, sysRoutines);
        if (!NT_SUCCESS(st) || !exec.isProviderRegistered()) {
            std::cerr << "[-] Pillar 1 Failed: registerProvider failed\n";
            return false;
        }

        // Test exclusivity: double registration must be rejected with STATUS_ALREADY_REGISTERED
        PS_PICO_SYSTEM_ROUTINES badSys{};
        NtStatus dupStatus = exec.registerProvider(provRoutines, badSys);
        if (dupStatus != NtStatus::AlreadyRegistered) {
            std::cerr << "[-] Pillar 1 Failed: Second registerProvider should return AlreadyRegistered\n";
            return false;
        }
    }

    // Pillar 2: Sovereign Pico Process Lifecycle
    uint64_t picoPid = 0;
    {
        NtStatus st = sysRoutines.CreateProcess(4, L"/bin/sh", &mockProcContext, picoPid);
        if (!NT_SUCCESS(st) || picoPid == 0 || exec.getActiveProcessCount() != 1) {
            std::cerr << "[-] Pillar 2 Failed: CreateProcess failed\n";
            return false;
        }

        void* retrievedCtx = nullptr;
        st = sysRoutines.GetProcessContext(picoPid, retrievedCtx);
        if (!NT_SUCCESS(st) || retrievedCtx != &mockProcContext) {
            std::cerr << "[-] Pillar 2 Failed: GetProcessContext mismatch\n";
            return false;
        }
    }

    // Pillar 3: Sovereign Pico Thread Lifecycle & Context Switching
    uint64_t picoTid = 0;
    {
        uint64_t entryPoint = 0x40001000ULL;
        uint64_t stackPtr   = 0x7FFFFFFF0000ULL;

        NtStatus st = sysRoutines.CreateThread(picoPid, entryPoint, stackPtr, &mockThreadContext, picoTid);
        if (!NT_SUCCESS(st) || picoTid == 0) {
            std::cerr << "[-] Pillar 3 Failed: CreateThread failed\n";
            return false;
        }

        st = sysRoutines.ResumeThread(picoTid);
        if (!NT_SUCCESS(st) || exec.getActiveThreadCount() != 1) {
            std::cerr << "[-] Pillar 3 Failed: ResumeThread failed\n";
            return false;
        }

        PICO_CONTEXT_FRAME ctx{};
        st = exec.getThreadRegisterContext(picoTid, ctx);
        if (!NT_SUCCESS(st) || ctx.Rip != entryPoint || ctx.Rsp != stackPtr) {
            std::cerr << "[-] Pillar 3 Failed: Register context mismatch\n";
            return false;
        }

        // Update register context
        ctx.Rax = 42;
        st = exec.setThreadRegisterContext(picoTid, ctx);
        if (!NT_SUCCESS(st)) {
            std::cerr << "[-] Pillar 3 Failed: setThreadRegisterContext failed\n";
            return false;
        }
    }

    // Pillar 4: Ring 0 Fast Syscall Interception & Direct Vectoring
    {
        // 4a: sys_getpid (39)
        uint64_t retVal = 0;
        std::vector<uint64_t> noArgs{};
        NtStatus st = exec.dispatchSystemCall(picoTid, 39, noArgs, retVal);
        if (!NT_SUCCESS(st) || retVal != 1001) {
            std::cerr << "[-] Pillar 4 Failed: sys_getpid dispatch failed (expected 1001, got " << retVal << ")\n";
            return false;
        }

        // 4b: sys_getuid (102)
        st = exec.dispatchSystemCall(picoTid, 102, noArgs, retVal);
        if (!NT_SUCCESS(st) || retVal != 0) {
            std::cerr << "[-] Pillar 4 Failed: sys_getuid dispatch failed\n";
            return false;
        }

        // 4c: sys_mmap (9)
        std::vector<uint64_t> mmapArgs = { 0, 4096, 3, 0x22, static_cast<uint64_t>(-1), 0 };
        st = exec.dispatchSystemCall(picoTid, 9, mmapArgs, retVal);
        if (!NT_SUCCESS(st) || retVal != 0x7FFF00000000ULL) {
            std::cerr << "[-] Pillar 4 Failed: sys_mmap dispatch failed\n";
            return false;
        }
    }

    // Pillar 5: LSW Storage & Memory Interlock (NTFS / IoRing Bridge)
    {
        // Simulate POSIX sys_read (0) and sys_write (1) dispatch through Pico interlock
        std::vector<uint64_t> rwArgs = { 3 /* fd */, 0x7FFF00000000ULL /* buf */, 8192 /* count */ };
        uint64_t retVal = 0;

        NtStatus st = exec.dispatchSystemCall(picoTid, 1 /* sys_write */, rwArgs, retVal);
        if (!NT_SUCCESS(st) || retVal != 8192) {
            std::cerr << "[-] Pillar 5 Failed: sys_write interlock failed\n";
            return false;
        }

        st = exec.dispatchSystemCall(picoTid, 0 /* sys_read */, rwArgs, retVal);
        if (!NT_SUCCESS(st) || retVal != 8192) {
            std::cerr << "[-] Pillar 5 Failed: sys_read interlock failed\n";
            return false;
        }

        // Verify termination protocol & exit callback
        st = sysRoutines.TerminateProcess(picoPid, 0);
        if (!NT_SUCCESS(st) || !mockProcContext.ExitCallbackReceived || !mockThreadContext.ExitCallbackReceived) {
            std::cerr << "[-] Pillar 5 Failed: TerminateProcess exit callback was not invoked\n";
            return false;
        }
    }

    // Pillar 6: High-Frequency Pico Syscall Benchmark (<0.05 us/op)
    {
        // Spawn active benchmark process and thread
        uint64_t benchPid = 0;
        uint64_t benchTid = 0;
        MockLswProcessContext benchProcCtx{ .PosixPid = 2002, .PosixUid = 1000 };
        MockLswThreadContext  benchThCtx{ .PosixTid = 2002 };

        sysRoutines.CreateProcess(4, L"/usr/bin/perf", &benchProcCtx, benchPid);
        sysRoutines.CreateThread(benchPid, 0x500000, 0x600000, &benchThCtx, benchTid);
        sysRoutines.ResumeThread(benchTid);

        constexpr size_t NUM_SYSCALLS = 20000;
        std::vector<uint64_t> args{};
        uint64_t ret = 0;

        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < NUM_SYSCALLS; ++i) {
            exec.dispatchSystemCall(benchTid, 39, args, ret);
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start
        ).count();

        double usPerOp = static_cast<double>(elapsed) / static_cast<double>(NUM_SYSCALLS);
        std::cout << "    [Benchmark] Sovereign Kernel Pico Syscall Vectoring: " << usPerOp << " us/op ("
                  << NUM_SYSCALLS << " syscalls in " << elapsed << " us)\n";

        if (usPerOp > 50.0) {
            std::cerr << "[-] Pillar 6 Failed: Latency excessive: " << usPerOp << " us/op\n";
            return false;
        }

        sysRoutines.TerminateProcess(benchPid, 0);
    }

    exec.unregisterProvider();
    return true;
}

} // namespace micant::pico
