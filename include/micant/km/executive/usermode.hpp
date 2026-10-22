#pragma once

/**
 * @file usermode.hpp
 * @brief Horizon 121: Sovereign User-Mode Process & Thread Management, PEB/TEB Architecture,
 *        MSR_GS_BASE/SWAPGS Privilege Bridge & System Service Dispatch Table (SSDT) (Unit #93).
 *
 * Implements the canonical Windows NT 64-bit user-mode environment:
 * - Process Environment Block (PEB64) with OS version 10.0.26100, ProcessParameters, Heaps
 * - Thread Environment Block (TEB64) with GS:[0x30] Self pointer, GS:[0x60] PEB pointer, LastError
 * - MSR_GS_BASE (0xC0000101) & MSR_KERNEL_GS_BASE (0xC0000102) hardware SWAPGS model
 * - System Service Descriptor Table (SSDT / KiServiceTable) with fast syscall dispatch
 * - Multi-process management for smss.exe (PID 368), csrss.exe (PID 480), and cmd.exe (PID 1024)
 * - Safe memory probing (ProbeForRead / ProbeForWrite) across userland page boundaries
 *
 * Clean-room ISO C++23 implementation. Zero proprietary or decompiled code.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <array>
#include <span>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/hal/cpu.hpp"
#include "micant/km/hal/bootvid.hpp"
#include "micant/um/native/pe.hpp"

namespace micant::um {

// Canonical x86-64 User-Mode Virtual Address Space Boundaries
inline constexpr uint64_t MM_HIGHEST_USER_ADDRESS = 0x00007FFFFFFFFFFFULL;
inline constexpr uint64_t MM_LOWEST_USER_ADDRESS  = 0x0000000000010000ULL; // 64KB Null Allocation Trap

static inline micant::NtStatus ProbeUserMemory(const void* addr, size_t len, uint32_t alignment = 1) noexcept {
    if (!addr || len == 0) return micant::NtStatus::Success;
    uint64_t va = reinterpret_cast<uint64_t>(addr);
    if (alignment > 1 && (va & (alignment - 1)) != 0) {
        return micant::NtStatus::DatatypeMisalignment;
    }
    if (va < MM_LOWEST_USER_ADDRESS) {
        return micant::NtStatus::AccessViolation;
    }
    if (va + len < va || va + len > MM_HIGHEST_USER_ADDRESS) {
        return micant::NtStatus::AccessViolation;
    }
    return micant::NtStatus::Success;
}

} // namespace micant::um

namespace micant::um {

// ============================================================================
// 1. Canonical Windows NT 64-bit PEB & TEB Data Structures
// ============================================================================

#pragma pack(push, 1)

/**
 * @brief Process Environment Block (PEB64).
 * Mapped into user space and referenced via TEB64->ProcessEnvironmentBlock (GS:[0x60]).
 */
struct Peb64 {
    uint8_t  inheritedAddressSpace{0};
    uint8_t  readImageFileExecOptions{0};
    uint8_t  beingDebugged{0};
    uint8_t  bitField{0};
    uint32_t mutant{0};
    uint64_t imageBaseAddress{0x00007FF600000000ULL};
    uint64_t ldr{0};
    uint64_t processParameters{0};
    uint64_t subSystemData{0};
    uint64_t processHeap{0};
    uint64_t fastPebLock{0};
    uint64_t atlThunkSListPtr{0};
    uint64_t ifeoKey{0};
    uint32_t crossProcessFlags{0};
    uint32_t userSharedInfoPtr{0};
    uint32_t systemReserved{0};
    uint32_t atlThunkSListPtr32{0};
    uint64_t apiSetMap{0};
    uint32_t tlsExpansionCounter{0};
    uint32_t tlsBitmap{0};
    uint32_t tlsBitmapBits[2]{0, 0};
    uint64_t readOnlySharedMemoryBase{0};
    uint64_t sharedData{0};
    uint64_t readOnlyStaticServerData{0};
    uint64_t ansiCodePageData{0};
    uint64_t oemCodePageData{0};
    uint64_t unicodeCaseTableData{0};
    uint32_t numberOfProcessors{8}; // 4C/8T Xeon E3-1270 v6
    uint32_t ntGlobalFlag{0};
    uint64_t criticalSectionTimeout{0};
    uint64_t heapSegmentReserve{0};
    uint64_t heapSegmentCommit{0};
    uint64_t heapDeCommitTotalFreeThreshold{0};
    uint64_t heapDeCommitFreeBlockThreshold{0};
    uint32_t numberOfHeaps{0};
    uint32_t maximumNumberOfHeaps{16};
    uint64_t processHeaps{0};
    uint64_t gdiSharedHandleTable{0};
    uint64_t processStarterHelper{0};
    uint32_t gdiDCAttributeList{0};
    uint64_t loaderLock{0};
    uint32_t osMajorVersion{10};
    uint32_t osMinorVersion{0};
    uint16_t osBuildNumber{26100}; // Windows 11 24H2 baseline
    uint16_t osCsdVersion{0};
    uint32_t osPlatformId{2};      // VER_PLATFORM_WIN32_NT
    uint32_t imageSubsystem{3};    // IMAGE_SUBSYSTEM_WINDOWS_CUI
    uint32_t imageSubsystemMajorVersion{10};
    uint32_t imageSubsystemMinorVersion{0};
    uint64_t activeProcessAffinityMask{0xFF}; // 8 logical threads
    uint32_t gdiHandleBuffer[30]{0};
    uint64_t postProcessInitRoutine{0};
    uint64_t tlsExpansionBitmap{0};
    uint32_t tlsExpansionBitmapBits[32]{0};
    uint32_t sessionId{1};
};

/**
 * @brief Thread Environment Block (TEB64).
 * Mapped into user space at GS:[0x00].
 * GS:[0x30] = self pointer to TEB64.
 * GS:[0x60] = pointer to PEB64.
 * GS:[0x68] = last error value.
 */
struct Teb64 {
    // NT_TIB (0x00 - 0x38)
    uint64_t exceptionList{0};
    uint64_t stackBase{0};
    uint64_t stackLimit{0};
    uint64_t subSystemTib{0};
    uint64_t fiberData{0};
    uint64_t arbitraryUserPointer{0};
    uint64_t self{0}; // Offset 0x30: Must point to this Teb64 instance

    // Environment & IDs (0x38 - 0x60)
    uint64_t environmentPointer{0};
    uint64_t uniqueProcessId{0};
    uint64_t uniqueThreadId{0};
    uint64_t activeRpcHandle{0};
    uint64_t threadLocalStoragePointer{0};

    // PEB & Error (0x60 - 0x70)
    uint64_t processEnvironmentBlock{0}; // Offset 0x60: Must point to Peb64
    uint32_t lastErrorValue{0};          // Offset 0x68: Win32 SetLastError / GetLastError
    uint32_t countOfOwnedCriticalSections{0};
    uint64_t csrClientThread{0};
    uint64_t win32ThreadInfo{0};
    uint32_t user32Reserved[26]{0};
    uint32_t userReserved[5]{0};
    uint64_t wow32Reserved{0};
    uint32_t currentLocale{0x0409}; // en-US
    uint32_t fpSoftwareStatusRegister{0};
    uint64_t reservedForDebuggerInstrumentation[16]{0};
};

#pragma pack(pop)

// ============================================================================
// 2. User-Mode Process Context & Lifecycle State
// ============================================================================

enum class UserProcessState : uint32_t {
    Ready,
    Running,
    Terminated,
    Faulted
};

struct UserProcessContext64 {
    uint32_t         pid{0};
    uint32_t         parentPid{0};
    char             imageName[32]{};
    uint64_t         codeBaseVa{0};
    size_t           codeSize{0};
    uint64_t         stackTopVa{0};
    size_t           stackSize{0};
    uint64_t         pebVa{0};
    uint64_t         tebVa{0};
    Peb64            peb{};
    Teb64            teb{};
    UserProcessState state{UserProcessState::Ready};
    micant::NtStatus exitStatus{micant::NtStatus::Success};
    uint64_t         syscallCount{0};
    uint64_t         lastSyscallNumber{0};
    uint64_t         lastSyscallArg1{0};
    uint64_t         lastSyscallArg2{0};
    uint64_t         verifiedCpl{0};
    uint64_t         verifiedUserCs{0};
    bool             pebVerifiedByR3{false};
    bool             tebVerifiedByR3{false};
};

// ----------------------------------------------------------------------------
// Win32k GDI / Framebuffer Graphics System Service Structures (Ring 3 <-> Ring 0)
// ----------------------------------------------------------------------------
#pragma pack(push, 1)
struct UserVideoInfo64 {
    uint32_t width{0};
    uint32_t height{0};
    uint32_t pitch{0};        // In pixels
    uint32_t bpp{32};         // 32-bit color
    uint32_t pixelFormat{1};  // 1 = BGRA, 0 = RGBA
    uint32_t reserved{0};     // 64-bit alignment padding
    uint64_t physicalBase{0}; // Physical or mapped VRAM base
};

struct UserPresentSurfaceParams64 {
    uint64_t surfaceBufferVa{0}; // Userland 32-bpp BGRA buffer pointer
    uint32_t dstX{0};
    uint32_t dstY{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t pitch{0};           // Userland buffer pitch in pixels (0 = width)
    uint32_t reserved{0};        // 64-bit alignment padding
};
#pragma pack(pop)

// ============================================================================
// 3. System Service Descriptor Table (SSDT) & Fast Syscall Engine
// ============================================================================

inline constexpr uint32_t SSN_NT_TERMINATE_PROCESS        = 0x0001; // Process Exit
inline constexpr uint32_t SSN_NT_QUERY_SYSTEM_INFORMATION = 0x0011; // Hardware / CPU / Memory telemetry
inline constexpr uint32_t SSN_NT_READ_FILE                = 0x0023; // VFS Read
inline constexpr uint32_t SSN_NT_WRITE_FILE               = 0x0025; // Console / Serial Print
inline constexpr uint32_t SSN_NT_ALLOCATE_VIRTUAL_MEMORY  = 0x0055; // Userland Heap / Stack expansion
inline constexpr uint32_t SSN_NT_CLOSE                    = 0x001E; // Handle close
inline constexpr uint32_t SSN_NT_YIELD_EXECUTION          = 0x0046; // Cooperative CPU yield
inline constexpr uint32_t SSN_NT_GDI_GET_VIDEO_INFO       = 0x0080; // Query screen dimensions & framebuffer format
inline constexpr uint32_t SSN_NT_GDI_PRESENT_SURFACE      = 0x0081; // Scanline burst blit to physical VRAM

struct SsdtEntry {
    uint32_t    ssn{0};
    const char* name{""};
    uint32_t    argCount{0};
    uint64_t    invocationCount{0};
};

class SsdtTable {
private:
    std::array<SsdtEntry, 32> m_entries{};
    size_t m_entryCount{0};

public:
    constexpr SsdtTable() noexcept {
        addEntry(SSN_NT_TERMINATE_PROCESS, "NtTerminateProcess", 2);
        addEntry(SSN_NT_QUERY_SYSTEM_INFORMATION, "NtQuerySystemInformation", 4);
        addEntry(SSN_NT_READ_FILE, "NtReadFile", 9);
        addEntry(SSN_NT_WRITE_FILE, "NtWriteFile", 9);
        addEntry(SSN_NT_ALLOCATE_VIRTUAL_MEMORY, "NtAllocateVirtualMemory", 6);
        addEntry(SSN_NT_CLOSE, "NtClose", 1);
        addEntry(SSN_NT_YIELD_EXECUTION, "NtYieldExecution", 0);
        addEntry(SSN_NT_GDI_GET_VIDEO_INFO, "NtGdiGetVideoInfo", 1);
        addEntry(SSN_NT_GDI_PRESENT_SURFACE, "NtGdiPresentSurface", 1);
    }

    constexpr void addEntry(uint32_t ssn, const char* name, uint32_t argCount) noexcept {
        if (m_entryCount < m_entries.size()) {
            m_entries[m_entryCount++] = SsdtEntry{ssn, name, argCount, 0};
        }
    }

    [[nodiscard]] const SsdtEntry* find(uint32_t ssn) const noexcept {
        for (size_t i = 0; i < m_entryCount; ++i) {
            if (m_entries[i].ssn == ssn) {
                return &m_entries[i];
            }
        }
        return nullptr;
    }

    void recordInvocation(uint32_t ssn) noexcept {
        for (size_t i = 0; i < m_entryCount; ++i) {
            if (m_entries[i].ssn == ssn) {
                m_entries[i].invocationCount++;
                return;
            }
        }
    }

    [[nodiscard]] size_t getCount() const noexcept { return m_entryCount; }
    [[nodiscard]] const SsdtEntry& getEntry(size_t idx) const noexcept { return m_entries[idx]; }
};

// ============================================================================
// 4. Sovereign User-Mode Executive Manager (UserProcessManager64)
// ============================================================================

class UserProcessManager64 {
private:
    static constexpr size_t MAX_PROCESSES = 16;
    std::array<UserProcessContext64, MAX_PROCESSES> m_processes{};
    size_t    m_processCount{0};
    uint32_t  m_activePid{0};
    SsdtTable m_ssdt{};
    uint64_t  m_totalSyscallsDispatched{0};
    uint64_t  m_totalProcessesSpawned{0};
    uint64_t  m_totalProcessesTerminated{0};
    bootvid::BootVideoDriver* m_videoDriver{nullptr};
    uint64_t  m_totalFramesPresented{0};
    uint64_t  m_totalPixelsBlitted{0};

public:
    static UserProcessManager64& get() noexcept {
        static UserProcessManager64 s_instance;
        return s_instance;
    }

    UserProcessManager64() noexcept {
        reset();
    }

    void reset(bool populateSystem = true) noexcept {
        m_processes.fill(UserProcessContext64{});
        m_processCount = 0;
        m_activePid = populateSystem ? 4 : 0;
        m_totalSyscallsDispatched = 0;
        m_totalProcessesSpawned = 0;
        m_totalProcessesTerminated = 0;
        m_totalFramesPresented = 0;
        m_totalPixelsBlitted = 0;

        if (populateSystem) {
            // Core NT Executive Processes
            (void)createProcess("System Idle", 0, 0, 0, 0, 0, 0, 0, 0);
            (void)createProcess("System", 4, 0, 0xFFFFF80000000000ULL, 0x100000, 0, 0, 0, 0);
        }
    }

    [[nodiscard]] UserProcessContext64* createProcess(
        const char* imageName,
        uint32_t pid,
        uint32_t parentPid,
        uint64_t codeVa,
        size_t codeSize,
        uint64_t stackTopVa,
        size_t stackSize,
        uint64_t pebVa,
        uint64_t tebVa
    ) noexcept {
        if (m_processCount >= MAX_PROCESSES) return nullptr;

        UserProcessContext64& proc = m_processes[m_processCount++];
        proc.pid = pid;
        proc.parentPid = parentPid;
        proc.codeBaseVa = codeVa;
        proc.codeSize = codeSize;
        proc.stackTopVa = stackTopVa;
        proc.stackSize = stackSize;
        proc.pebVa = pebVa;
        proc.tebVa = tebVa;
        proc.state = UserProcessState::Ready;
        proc.exitStatus = micant::NtStatus::Success;
        proc.syscallCount = 0;

        // Copy Image Name
        size_t i = 0;
        for (; i + 1 < sizeof(proc.imageName) && imageName && imageName[i] != '\0'; ++i) {
            proc.imageName[i] = imageName[i];
        }
        proc.imageName[i] = '\0';

        // 1. Initialize PEB64
        proc.peb = Peb64{};
        proc.peb.imageBaseAddress = codeVa;
        proc.peb.numberOfProcessors = 8; // Xeon E3-1270 v6
        proc.peb.osMajorVersion = 10;
        proc.peb.osMinorVersion = 0;
        proc.peb.osBuildNumber = 26100;
        proc.peb.sessionId = (pid == 368) ? 0 : 1; // smss.exe is Session 0, interactive apps Session 1

        // 2. Initialize TEB64
        proc.teb = Teb64{};
        proc.teb.stackBase = stackTopVa;
        proc.teb.stackLimit = stackTopVa - stackSize;
        proc.teb.self = tebVa; // Canonical GS:[0x30] self pointer
        proc.teb.uniqueProcessId = pid;
        proc.teb.uniqueThreadId = pid + 1;
        proc.teb.processEnvironmentBlock = pebVa; // Canonical GS:[0x60] PEB pointer
        proc.teb.lastErrorValue = 0;

        m_totalProcessesSpawned++;
        return &proc;
    }

    [[nodiscard]] UserProcessContext64* findProcess(uint32_t pid) noexcept {
        for (size_t i = 0; i < m_processCount; ++i) {
            if (m_processes[i].pid == pid) {
                return &m_processes[i];
            }
        }
        return nullptr;
    }

    [[nodiscard]] const UserProcessContext64* getProcess(size_t index) const noexcept {
        if (index < m_processCount) return &m_processes[index];
        return nullptr;
    }

    [[nodiscard]] size_t getProcessCount() const noexcept { return m_processCount; }
    [[nodiscard]] uint32_t getActivePid() const noexcept { return m_activePid; }
    void setActivePid(uint32_t pid) noexcept { m_activePid = pid; }

    /**
     * @brief Central System Call Dispatcher.
     * Invoked from KiSystemCall64Entry when userland issues 'syscall'.
     */
    micant::NtStatus dispatchSyscall(
        uint64_t ssn,
        uint64_t arg1,
        uint64_t arg2,
        uint64_t arg3,
        uint64_t arg4,
        uint64_t cpl,
        uint64_t userCs
    ) noexcept {
        m_totalSyscallsDispatched++;
        m_ssdt.recordInvocation(static_cast<uint32_t>(ssn));

        UserProcessContext64* activeProc = findProcess(m_activePid);
        if (activeProc) {
            activeProc->syscallCount++;
            activeProc->lastSyscallNumber = ssn;
            activeProc->lastSyscallArg1 = arg1;
            activeProc->lastSyscallArg2 = arg2;
            activeProc->verifiedCpl = cpl;
            activeProc->verifiedUserCs = userCs;
            activeProc->state = UserProcessState::Running;
        }

        switch (ssn) {
            case SSN_NT_QUERY_SYSTEM_INFORMATION: {
                // arg1: SystemInformationClass, arg2: SystemInformation buffer pointer, arg3: bufferLength, arg4: returnLength ptr
                if (activeProc) {
                    activeProc->tebVerifiedByR3 = true;
                    activeProc->pebVerifiedByR3 = true;
                }
                // Memory probe check
                if (arg2 != 0) {
                    micant::NtStatus probeStat = ProbeUserMemory(
                        reinterpret_cast<void*>(arg2),
                        static_cast<size_t>(arg3),
                        sizeof(uint32_t)
                    );
                    if (probeStat != micant::NtStatus::Success) {
                        return probeStat;
                    }
                }
                return micant::NtStatus::Success;
            }

            case SSN_NT_WRITE_FILE: {
                // arg1: FileHandle/Sink (1 = stdout), arg2: Buffer pointer, arg3: ByteCount
                if (arg2 != 0 && arg3 > 0) {
                    micant::NtStatus probeStat = ProbeUserMemory(
                        reinterpret_cast<const void*>(arg2),
                        static_cast<size_t>(arg3),
                        1
                    );
                    if (probeStat != micant::NtStatus::Success) {
                        return probeStat;
                    }
                }
                return micant::NtStatus::Success;
            }

            case SSN_NT_TERMINATE_PROCESS: {
                // arg1: ProcessHandle (0 = current), arg2: ExitStatus
                if (activeProc) {
                    activeProc->state = UserProcessState::Terminated;
                    activeProc->exitStatus = static_cast<micant::NtStatus>(arg2);
                    m_totalProcessesTerminated++;
                }
                return micant::NtStatus::Success;
            }

            case SSN_NT_ALLOCATE_VIRTUAL_MEMORY: {
                // arg1: ProcessHandle, arg2: BaseAddress ptr, arg3: ZeroBits, arg4: RegionSize ptr
                return micant::NtStatus::Success;
            }

            case SSN_NT_CLOSE: {
                return micant::NtStatus::Success;
            }

            case SSN_NT_YIELD_EXECUTION: {
                return micant::NtStatus::Success;
            }

            case SSN_NT_GDI_GET_VIDEO_INFO: {
                // arg1: pointer to UserVideoInfo64
                if (arg1 == 0) return micant::NtStatus::InvalidParameter;
                micant::NtStatus probe = ProbeUserMemory(reinterpret_cast<void*>(arg1), sizeof(UserVideoInfo64), sizeof(uint32_t));
                if (probe != micant::NtStatus::Success) return probe;

                auto* info = reinterpret_cast<UserVideoInfo64*>(arg1);
                if (m_videoDriver && m_videoDriver->isInitialized()) {
                    info->width = m_videoDriver->getWidth();
                    info->height = m_videoDriver->getHeight();
                    info->pitch = m_videoDriver->getPitch();
                    info->bpp = 32;
                    info->pixelFormat = m_videoDriver->getPixelFormat();
                    info->physicalBase = reinterpret_cast<uint64_t>(m_videoDriver->getRawBuffer());
                } else {
                    info->width = 1024;
                    info->height = 768;
                    info->pitch = 1024;
                    info->bpp = 32;
                    info->pixelFormat = 1;
                    info->physicalBase = 0;
                }
                return micant::NtStatus::Success;
            }

            case SSN_NT_GDI_PRESENT_SURFACE: {
                // arg1: pointer to UserPresentSurfaceParams64
                if (arg1 == 0) return micant::NtStatus::InvalidParameter;
                micant::NtStatus probe = ProbeUserMemory(reinterpret_cast<void*>(arg1), sizeof(UserPresentSurfaceParams64), sizeof(uint32_t));
                if (probe != micant::NtStatus::Success) return probe;

                auto* params = reinterpret_cast<UserPresentSurfaceParams64*>(arg1);
                if (params->surfaceBufferVa == 0 || params->width == 0 || params->height == 0) {
                    return micant::NtStatus::InvalidParameter;
                }

                uint32_t sp = (params->pitch > 0) ? params->pitch : params->width;
                size_t bufferBytes = static_cast<size_t>(sp) * params->height * sizeof(uint32_t);
                micant::NtStatus bufProbe = ProbeUserMemory(reinterpret_cast<void*>(params->surfaceBufferVa), bufferBytes, sizeof(uint32_t));
                if (bufProbe != micant::NtStatus::Success) return bufProbe;

                if (m_videoDriver && m_videoDriver->isInitialized()) {
                    m_videoDriver->blit(
                        params->dstX,
                        params->dstY,
                        params->width,
                        params->height,
                        reinterpret_cast<const uint32_t*>(params->surfaceBufferVa),
                        params->pitch
                    );
                }
                m_totalFramesPresented++;
                m_totalPixelsBlitted += (static_cast<uint64_t>(params->width) * params->height);
                return micant::NtStatus::Success;
            }

            default:
                return micant::NtStatus::NotImplemented;
        }
    }

    void setVideoDriver(bootvid::BootVideoDriver* driver) noexcept { m_videoDriver = driver; }
    [[nodiscard]] bootvid::BootVideoDriver* getVideoDriver() const noexcept { return m_videoDriver; }
    [[nodiscard]] uint64_t getTotalFramesPresented() const noexcept { return m_totalFramesPresented; }
    [[nodiscard]] uint64_t getTotalPixelsBlitted() const noexcept { return m_totalPixelsBlitted; }

    [[nodiscard]] const SsdtTable& getSsdt() const noexcept { return m_ssdt; }
    [[nodiscard]] uint64_t getTotalSyscallsDispatched() const noexcept { return m_totalSyscallsDispatched; }
    [[nodiscard]] uint64_t getTotalProcessesSpawned() const noexcept { return m_totalProcessesSpawned; }
    [[nodiscard]] uint64_t getTotalProcessesTerminated() const noexcept { return m_totalProcessesTerminated; }
};

// ============================================================================
// 4. Horizon 122: Sovereign Ring 3 User Trap Frame, Thread Context & Scheduler
// ============================================================================

#pragma pack(push, 1)

/**
 * @brief Canonical 64-bit User-Mode Trap Frame (KTRAP_FRAME).
 * Preserves hardware and software execution context across privilege transitions
 * and cooperative / preemptive thread context switches.
 */
struct UserTrapFrame64 {
    // General Purpose Registers (GPRs)
    uint64_t rax{0};
    uint64_t rbx{0};
    uint64_t rcx{0};
    uint64_t rdx{0};
    uint64_t rsi{0};
    uint64_t rdi{0};
    uint64_t rbp{0};
    uint64_t r8{0};
    uint64_t r9{0};
    uint64_t r10{0};
    uint64_t r11{0};
    uint64_t r12{0};
    uint64_t r13{0};
    uint64_t r14{0};
    uint64_t r15{0};

    // Hardware IRETQ context
    uint64_t rip{0};
    uint64_t cs{0x0033};
    uint64_t rflags{0x0202};
    uint64_t rsp{0};
    uint64_t ss{0x002B};

    // Segment registers
    uint16_t ds{0x002B};
    uint16_t es{0x002B};
    uint16_t fs{0x0053};
    uint16_t gs{0x002B};
    uint32_t padding{0};

    // GS Base addresses
    uint64_t userGsBase{0};    // MSR 0xC0000101 (TEB64 address)
    uint64_t kernelGsBase{0};  // MSR 0xC0000102 (KPCR address)
};

#pragma pack(pop)

/**
 * @brief User-mode thread execution states.
 */
enum class UserThreadState : uint8_t {
    Initialized,
    Ready,
    Running,
    Waiting,
    Yielded,
    Terminated
};

/**
 * @brief Sovereign User-Mode Thread Execution Context (ETHREAD/KTHREAD Ring 3).
 */
struct UserThreadContext64 {
    uint32_t tid{0};
    uint32_t parentPid{0};
    uint8_t  priority{16}; // Normal priority (0-31)
    uint8_t  quantum{6};   // Default quantum ticks
    uint8_t  quantumRemaining{6};
    UserThreadState state{UserThreadState::Initialized};
    UserTrapFrame64 trapFrame{};
    uint64_t userStackTop{0};
    uint64_t kernelStackTop{0};
    uint64_t tebAddress{0};
    uint64_t contextSwitches{0};
    uint64_t totalYields{0};
    micant::NtStatus exitStatus{micant::NtStatus::Success};
    char name[32]{};
};

/**
 * @brief Sovereign Multi-Process / Multi-Thread Cooperative Scheduler (Unit #94).
 * Coordinates execution across multiple Ring 3 userland processes and threads.
 */
class UserThreadScheduler64 {
private:
    static constexpr size_t MAX_USER_THREADS = 16;
    UserThreadContext64 m_threads[MAX_USER_THREADS]{};
    size_t m_threadCount{0};
    uint32_t m_activeTid{0};
    uint64_t m_totalContextSwitches{0};
    uint64_t m_totalYields{0};

    UserThreadScheduler64() = default;

public:
    static UserThreadScheduler64& get() noexcept {
        static UserThreadScheduler64 instance;
        return instance;
    }

    void reset(bool populateSystem = true) noexcept {
        m_threadCount = 0;
        m_activeTid = populateSystem ? 4 : 0;
        m_totalContextSwitches = 0;
        m_totalYields = 0;
        for (size_t i = 0; i < MAX_USER_THREADS; ++i) {
            m_threads[i] = UserThreadContext64{};
        }
        if (populateSystem) {
            createThread(0, 0, 0, 0, 0, 0, "IdleThread");
            createThread(4, 4, 16, 0xFFFFF80000000000ULL, 0, 0, "SystemWorker");
        }
    }

    bool createThread(
        uint32_t tid,
        uint32_t parentPid,
        uint8_t  priority,
        uint64_t userRip,
        uint64_t userRsp,
        uint64_t tebAddress,
        const char* name
    ) noexcept {
        if (m_threadCount >= MAX_USER_THREADS) return false;

        // Check if thread already exists
        for (size_t i = 0; i < m_threadCount; ++i) {
            if (m_threads[i].tid == tid) return false;
        }

        UserThreadContext64& t = m_threads[m_threadCount++];
        t.tid = tid;
        t.parentPid = parentPid;
        t.priority = priority;
        t.quantum = 6;
        t.quantumRemaining = 6;
        t.state = UserThreadState::Ready;
        t.userStackTop = userRsp;
        t.tebAddress = tebAddress;

        // Initialize Trap Frame for Ring 3 entry
        t.trapFrame.rip = userRip;
        t.trapFrame.cs = 0x0033;
        t.trapFrame.rflags = 0x0202;
        t.trapFrame.rsp = userRsp;
        t.trapFrame.ss = 0x002B;
        t.trapFrame.userGsBase = tebAddress;

        if (name) {
            size_t len = 0;
            while (name[len] && len < sizeof(t.name) - 1) {
                t.name[len] = name[len];
                len++;
            }
            t.name[len] = '\0';
        }

        if (m_activeTid == 0) {
            m_activeTid = tid;
            t.state = UserThreadState::Running;
        }

        return true;
    }

    [[nodiscard]] UserThreadContext64* findThread(uint32_t tid) noexcept {
        for (size_t i = 0; i < m_threadCount; ++i) {
            if (m_threads[i].tid == tid) return &m_threads[i];
        }
        return nullptr;
    }

    [[nodiscard]] const UserThreadContext64* getThread(size_t index) const noexcept {
        if (index < m_threadCount) return &m_threads[index];
        return nullptr;
    }

    [[nodiscard]] size_t getThreadCount() const noexcept { return m_threadCount; }
    [[nodiscard]] uint32_t getActiveTid() const noexcept { return m_activeTid; }
    [[nodiscard]] uint64_t getTotalContextSwitches() const noexcept { return m_totalContextSwitches; }
    [[nodiscard]] uint64_t getTotalYields() const noexcept { return m_totalYields; }

    /**
     * @brief Switch execution context to target thread.
     */
    bool switchContext(uint32_t targetTid) noexcept {
        if (targetTid == m_activeTid) return true;

        UserThreadContext64* current = findThread(m_activeTid);
        UserThreadContext64* target = findThread(targetTid);

        if (!target || target->state == UserThreadState::Terminated) {
            return false;
        }

        if (current && current->state == UserThreadState::Running) {
            current->state = UserThreadState::Ready;
        }

        target->state = UserThreadState::Running;
        target->contextSwitches++;
        m_activeTid = targetTid;
        m_totalContextSwitches++;

        // Synchronize UserProcessManager active PID
        UserProcessManager64::get().setActivePid(target->parentPid);
        return true;
    }

    /**
     * @brief Yield execution to next ready thread (SSN_NT_YIELD_EXECUTION).
     */
    uint32_t yieldCurrentThread() noexcept {
        m_totalYields++;
        UserThreadContext64* current = findThread(m_activeTid);
        if (current) {
            current->totalYields++;
            current->state = UserThreadState::Yielded;
        }

        // Find next ready/yielded thread round-robin
        size_t startIdx = 0;
        for (size_t i = 0; i < m_threadCount; ++i) {
            if (m_threads[i].tid == m_activeTid) {
                startIdx = (i + 1) % m_threadCount;
                break;
            }
        }

        for (size_t step = 0; step < m_threadCount; ++step) {
            size_t idx = (startIdx + step) % m_threadCount;
            if (m_threads[idx].state == UserThreadState::Ready ||
                m_threads[idx].state == UserThreadState::Yielded) {
                switchContext(m_threads[idx].tid);
                return m_threads[idx].tid;
            }
        }

        // If no other thread ready, keep current running
        if (current && current->state != UserThreadState::Terminated) {
            current->state = UserThreadState::Running;
        }
        return m_activeTid;
    }

    /**
     * @brief Terminate current thread.
     */
    bool terminateCurrentThread(micant::NtStatus exitStatus) noexcept {
        UserThreadContext64* current = findThread(m_activeTid);
        if (!current) return false;

        current->state = UserThreadState::Terminated;
        current->exitStatus = exitStatus;

        // Switch to next available ready thread
        for (size_t i = 0; i < m_threadCount; ++i) {
            if (m_threads[i].state == UserThreadState::Ready ||
                m_threads[i].state == UserThreadState::Yielded) {
                switchContext(m_threads[i].tid);
                return true;
            }
        }

        m_activeTid = 0;
        return true;
    }
};

// ============================================================================
// 5. Horizon 123: Sovereign PE32+ Userland Binary Loader & Executive Bridge (Unit #95)
// ============================================================================

struct UserLoadedImageInfo {
    uint64_t imageBaseVa{0};
    uint64_t entryPointVa{0};
    uint64_t sizeOfImage{0};
    uint32_t numberOfSections{0};
    uint32_t boundImportsCount{0};
    uint32_t relocationsAppliedCount{0};
    uint32_t pid{0};
    uint32_t tid{0};
    char     imageName[32]{};
    bool     isRelocated{false};
    bool     headersValid{false};
    bool     sectionsMapped{false};
};

class UserImageLoader64 {
private:
    static inline bool streq(const char* a, const char* b) noexcept {
        if (!a || !b) return false;
        while (*a && *b) {
            if (*a != *b) return false;
            a++; b++;
        }
        return (*a == *b);
    }

    static inline bool strcaseeq(const char* a, const char* b) noexcept {
        if (!a || !b) return false;
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a + ('a' - 'A')) : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? static_cast<char>(*b + ('a' - 'A')) : *b;
            if (ca != cb) return false;
            a++; b++;
        }
        return (*a == *b);
    }

public:
    static micant::NtStatus loadAndMapImage(
        std::span<const uint8_t> rawPeBytes,
        uint8_t* destinationBuffer,
        size_t destinationBufferSize,
        uint64_t targetBaseVa,
        uint8_t* trampolineBuffer,
        size_t trampolineBufferSize,
        uint64_t trampolineBaseVa,
        uint64_t userStackTopVa,
        size_t userStackSize,
        uint64_t pebVa,
        uint64_t tebVa,
        uint32_t pid,
        uint32_t parentPid,
        const char* imageName,
        UserLoadedImageInfo& outInfo,
        uint64_t userHeapVa = 0,
        size_t userHeapSize = 0
    ) noexcept {
        outInfo = UserLoadedImageInfo{};
        outInfo.pid = pid;
        outInfo.tid = pid;
        if (imageName) {
            size_t n = 0;
            while (imageName[n] && n < sizeof(outInfo.imageName) - 1) {
                outInfo.imageName[n] = imageName[n];
                n++;
            }
            outInfo.imageName[n] = '\0';
        }

        // 1. Inspect DOS Header
        if (rawPeBytes.size() < sizeof(pe::ImageDosHeader)) {
            return micant::NtStatus::InvalidParameter;
        }
        const auto* dos = reinterpret_cast<const pe::ImageDosHeader*>(rawPeBytes.data());
        if (dos->e_magic != pe::DOS_MAGIC) {
            return micant::NtStatus::InvalidImageFormat;
        }
        if (dos->e_lfanew <= 0 || static_cast<size_t>(dos->e_lfanew) + sizeof(pe::ImageNtHeaders64) > rawPeBytes.size()) {
            return micant::NtStatus::InvalidImageFormat;
        }

        // 2. Inspect NT Headers (PE32+ 64-bit AMD64)
        const auto* nt = reinterpret_cast<const pe::ImageNtHeaders64*>(rawPeBytes.data() + dos->e_lfanew);
        if (nt->signature != pe::NT_SIGNATURE ||
            nt->fileHeader.machine != pe::MACHINE_AMD64 ||
            nt->optionalHeader.magic != pe::PE32PLUS_MAGIC) {
            return micant::NtStatus::InvalidImageFormat;
        }

        outInfo.headersValid = true;
        outInfo.sizeOfImage = nt->optionalHeader.sizeOfImage;
        outInfo.numberOfSections = nt->fileHeader.numberOfSections;

        // 3. Verify Destination Buffer
        if (!destinationBuffer || destinationBufferSize < nt->optionalHeader.sizeOfImage) {
            return micant::NtStatus::BufferTooSmall;
        }
        std::memset(destinationBuffer, 0, destinationBufferSize);

        // 4. Map Headers
        if (nt->optionalHeader.sizeOfHeaders > rawPeBytes.size() ||
            nt->optionalHeader.sizeOfHeaders > destinationBufferSize) {
            return micant::NtStatus::InvalidImageFormat;
        }
        std::memcpy(destinationBuffer, rawPeBytes.data(), nt->optionalHeader.sizeOfHeaders);

        // 5. Map Sections
        const auto* sections = reinterpret_cast<const pe::ImageSectionHeader*>(
            rawPeBytes.data() + dos->e_lfanew + sizeof(uint32_t) + sizeof(pe::ImageFileHeader) + nt->fileHeader.sizeOfOptionalHeader
        );

        for (uint16_t i = 0; i < nt->fileHeader.numberOfSections; ++i) {
            const auto& sec = sections[i];
            if (sec.virtualAddress + sec.sizeOfRawData > destinationBufferSize) {
                return micant::NtStatus::InvalidImageFormat;
            }
            if (sec.pointerToRawData + sec.sizeOfRawData > rawPeBytes.size()) {
                return micant::NtStatus::InvalidImageFormat;
            }
            if (sec.sizeOfRawData > 0) {
                std::memcpy(destinationBuffer + sec.virtualAddress,
                            rawPeBytes.data() + sec.pointerToRawData,
                            sec.sizeOfRawData);
            }
            if (sec.misc.virtualSize > sec.sizeOfRawData) {
                size_t bssLen = sec.misc.virtualSize - sec.sizeOfRawData;
                if (sec.virtualAddress + sec.sizeOfRawData + bssLen <= destinationBufferSize) {
                    std::memset(destinationBuffer + sec.virtualAddress + sec.sizeOfRawData, 0, bssLen);
                }
            }
        }
        outInfo.sectionsMapped = true;

        // 6. Base Relocations
        int64_t delta = static_cast<int64_t>(targetBaseVa) - static_cast<int64_t>(nt->optionalHeader.imageBase);
        if (delta != 0) {
            outInfo.isRelocated = true;
            if (nt->optionalHeader.numberOfRvaAndSizes > pe::IMAGE_DIRECTORY_ENTRY_BASERELOC) {
                const auto& relocDir = nt->optionalHeader.dataDirectory[pe::IMAGE_DIRECTORY_ENTRY_BASERELOC];
                if (relocDir.virtualAddress != 0 && relocDir.size != 0 &&
                    relocDir.virtualAddress + relocDir.size <= destinationBufferSize) {
                    uint32_t offset = relocDir.virtualAddress;
                    uint32_t endOffset = relocDir.virtualAddress + relocDir.size;
                    while (offset < endOffset) {
                        const auto* block = reinterpret_cast<const pe::ImageBaseRelocation*>(destinationBuffer + offset);
                        if (block->sizeOfBlock < sizeof(pe::ImageBaseRelocation)) break;
                        uint32_t count = (block->sizeOfBlock - sizeof(pe::ImageBaseRelocation)) / sizeof(uint16_t);
                        const auto* entries = reinterpret_cast<const uint16_t*>(block + 1);
                        for (uint32_t e = 0; e < count; ++e) {
                            uint16_t type = entries[e] >> 12;
                            uint16_t off = entries[e] & 0x0FFF;
                            uint32_t rva = block->virtualAddress + off;
                            if (type == pe::IMAGE_REL_BASED_DIR64 && rva + sizeof(uint64_t) <= destinationBufferSize) {
                                *reinterpret_cast<uint64_t*>(destinationBuffer + rva) += delta;
                                outInfo.relocationsAppliedCount++;
                            } else if (type == pe::IMAGE_REL_BASED_HIGHLOW && rva + sizeof(uint32_t) <= destinationBufferSize) {
                                *reinterpret_cast<uint32_t*>(destinationBuffer + rva) += static_cast<uint32_t>(delta);
                                outInfo.relocationsAppliedCount++;
                            }
                        }
                        offset += block->sizeOfBlock;
                    }
                }
            }
        }

        // 7. Setup Fast Syscall Trampolines
        // Slot 0 (0x00): NtTerminateProcess / ExitProcess (SSN 0x01)
        // Slot 1 (0x20): NtQuerySystemInformation (SSN 0x11)
        // Slot 2 (0x40): NtReadFile / ReadFile (SSN 0x23)
        // Slot 3 (0x60): NtWriteFile / WriteFile (SSN 0x25)
        // Slot 4 (0x80): NtYieldExecution (SSN 0x46)
        // Slot 5 (0xA0): NtAllocateVirtualMemory (SSN 0x55)
        // Slot 6 (0xC0): NtClose / CloseHandle (SSN 0x1E)
        // Slot 7 (0xE0): GetStdHandle (returns 1 in RAX)
        // Slot 8 (0x100): GetTickCount64 (rdtsc + shl + or)
        if (trampolineBuffer && trampolineBufferSize >= 512) {
            std::memset(trampolineBuffer, 0xCC, trampolineBufferSize); // fill with INT3 break stubs

            // Slot 0 (0x00): ExitProcess / NtTerminateProcess (SSN 0x01)
            // 48 89 CA          mov %rcx, %rdx
            // B8 01 00 00 00    mov $0x01, %eax
            // 0F 05             syscall
            // C3                ret
            const uint8_t s0[] = { 0x48, 0x89, 0xCA, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x05, 0xC3 };
            std::memcpy(trampolineBuffer + 0x00, s0, sizeof(s0));

            // Slot 1 (0x20): NtQuerySystemInformation (SSN 0x11)
            // B8 11 00 00 00    mov $0x11, %eax
            // 0F 05             syscall
            // C3                ret
            const uint8_t s1[] = { 0xB8, 0x11, 0x00, 0x00, 0x00, 0x0F, 0x05, 0xC3 };
            std::memcpy(trampolineBuffer + 0x20, s1, sizeof(s1));

            // Slot 2 (0x40): NtReadFile / ReadFile (SSN 0x23)
            // B8 23 00 00 00    mov $0x23, %eax
            // 0F 05             syscall
            // C3                ret
            const uint8_t s2[] = { 0xB8, 0x23, 0x00, 0x00, 0x00, 0x0F, 0x05, 0xC3 };
            std::memcpy(trampolineBuffer + 0x40, s2, sizeof(s2));

            // Slot 3 (0x60): NtWriteFile / WriteFile (SSN 0x25)
            // B8 25 00 00 00    mov $0x25, %eax
            // 0F 05             syscall
            // C3                ret
            const uint8_t s3[] = { 0xB8, 0x25, 0x00, 0x00, 0x00, 0x0F, 0x05, 0xC3 };
            std::memcpy(trampolineBuffer + 0x60, s3, sizeof(s3));

            // Slot 4 (0x80): NtYieldExecution (SSN 0x46)
            // B8 46 00 00 00    mov $0x46, %eax
            // 0F 05             syscall
            // C3                ret
            const uint8_t s4[] = { 0xB8, 0x46, 0x00, 0x00, 0x00, 0x0F, 0x05, 0xC3 };
            std::memcpy(trampolineBuffer + 0x80, s4, sizeof(s4));

            // Slot 5 (0xA0): NtAllocateVirtualMemory (SSN 0x55)
            // B8 55 00 00 00    mov $0x55, %eax
            // 0F 05             syscall
            // C3                ret
            const uint8_t s5[] = { 0xB8, 0x55, 0x00, 0x00, 0x00, 0x0F, 0x05, 0xC3 };
            std::memcpy(trampolineBuffer + 0xA0, s5, sizeof(s5));

            // Slot 6 (0xC0): NtClose / CloseHandle (SSN 0x1E)
            // B8 1E 00 00 00    mov $0x1E, %eax
            // 0F 05             syscall
            // C3                ret
            const uint8_t s6[] = { 0xB8, 0x1E, 0x00, 0x00, 0x00, 0x0F, 0x05, 0xC3 };
            std::memcpy(trampolineBuffer + 0xC0, s6, sizeof(s6));

            // Slot 7 (0xE0): GetStdHandle (returns 1 in RAX)
            // B8 01 00 00 00    mov $1, %eax
            // C3                ret
            const uint8_t s7[] = { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 };
            std::memcpy(trampolineBuffer + 0xE0, s7, sizeof(s7));

            // Slot 8 (0x100): GetTickCount64 (rdtsc + shl + or)
            const uint8_t s8[] = { 0x0F, 0x31, 0x48, 0xC1, 0xE2, 0x20, 0x48, 0x09, 0xD0, 0xC3 };
            std::memcpy(trampolineBuffer + 0x100, s8, sizeof(s8));

            // Slot 9 (0x120): QueryPerformanceCounter
            const uint8_t s9[] = { 0x0F, 0x31, 0x48, 0xC1, 0xE2, 0x20, 0x48, 0x09, 0xD0, 0x48, 0x85, 0xC9, 0x74, 0x03, 0x48, 0x89, 0x01, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 };
            std::memcpy(trampolineBuffer + 0x120, s9, sizeof(s9));

            // Slot 10 (0x140): QueryPerformanceFrequency (10 MHz)
            const uint8_t s10[] = { 0x48, 0xB8, 0x80, 0x96, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC9, 0x74, 0x03, 0x48, 0x89, 0x01, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 };
            std::memcpy(trampolineBuffer + 0x140, s10, sizeof(s10));

            // Slot 11 (0x160): GetCurrentProcessId / GetCurrentThreadId (returns 1024)
            const uint8_t s11[] = { 0xB8, 0x00, 0x04, 0x00, 0x00, 0xC3 };
            std::memcpy(trampolineBuffer + 0x160, s11, sizeof(s11));

            // Slot 12 (0x180): GetLastError (returns 0)
            const uint8_t s12[] = { 0x31, 0xC0, 0xC3 };
            std::memcpy(trampolineBuffer + 0x180, s12, sizeof(s12));

            // Slot 13 (0x1A0): UniversalNoop / SetLastError / CritSec (ret)
            const uint8_t s13[] = { 0xC3 };
            std::memcpy(trampolineBuffer + 0x1A0, s13, sizeof(s13));

            // Slot 14 (0x1C0): GetModuleHandleW / GetModuleHandleA (returns 0x140000000 on NULL rcx)
            const uint8_t s14[] = { 0x48, 0x85, 0xC9, 0x75, 0x0B, 0x48, 0xB8, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0x00, 0xC3, 0x31, 0xC0, 0xC3 };
            std::memcpy(trampolineBuffer + 0x1C0, s14, sizeof(s14));

            // Slot 15 (0x1E0): UniversalTrue / VirtualFree / SetEvent / ResetEvent (returns 1)
            const uint8_t s15[] = { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 };
            std::memcpy(trampolineBuffer + 0x1E0, s15, sizeof(s15));

            // Slot 16 (0x200): UniversalSuccess / UniversalNull (returns 0 / S_OK)
            const uint8_t s16[] = { 0x31, 0xC0, 0xC3 };
            std::memcpy(trampolineBuffer + 0x200, s16, sizeof(s16));

            // Slot 17 (0x220): GetCurrentPackageFullName (writes 0, returns 15700)
            const uint8_t s17[] = { 0x48, 0x85, 0xC9, 0x74, 0x06, 0xC7, 0x01, 0x00, 0x00, 0x00, 0x00, 0xB8, 0x4C, 0x3D, 0x00, 0x00, 0xC3 };
            std::memcpy(trampolineBuffer + 0x220, s17, sizeof(s17));

            // Slot 18 (0x240): CreateEventW / CreateEventA (returns dummy handle 0x1000)
            const uint8_t s18[] = { 0xB8, 0x00, 0x10, 0x00, 0x00, 0xC3 };
            std::memcpy(trampolineBuffer + 0x240, s18, sizeof(s18));

            // Slot 19 (0x260): GetSystemTimeAsFileTime (writes timestamp to [rcx])
            const uint8_t s19[] = { 0x48, 0x85, 0xC9, 0x74, 0x0E, 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xD9, 0x01, 0x48, 0x89, 0x01, 0xC3 };
            std::memcpy(trampolineBuffer + 0x260, s19, sizeof(s19));

            // Slot 20 (0x280): GetProcessHeap (returns non-null handle 0x10000)
            const uint8_t s20[] = { 0x48, 0xB8, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3 };
            std::memcpy(trampolineBuffer + 0x280, s20, sizeof(s20));

            // Slot 21 (0x2A0): HeapAlloc (lock xadd bump pointer at 0x3F8)
            uint64_t bumpAddr = trampolineBaseVa + 0x3F8;
            uint8_t s21[27] = {
                0x49, 0x89, 0xC0,                         // mov %r8, %rax
                0x48, 0x83, 0xC0, 0x0F,                   // add $15, %rax
                0x48, 0x83, 0xE0, 0xF0,                   // and $-16, %rax
                0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov $bumpAddr, %rdx
                0xF0, 0x48, 0x0F, 0xC1, 0x02,             // lock xadd %rax, (%rdx)
                0xC3                                      // ret
            };
            std::memcpy(s21 + 10, &bumpAddr, sizeof(bumpAddr));
            std::memcpy(trampolineBuffer + 0x2A0, s21, sizeof(s21));

            // Slot 22 (0x2C0): GetCommandLineW (returns pointer to wide string at 0x3A0)
            uint64_t wcmdAddr = trampolineBaseVa + 0x3A0;
            uint8_t s22[11] = { 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3 };
            std::memcpy(s22 + 2, &wcmdAddr, sizeof(wcmdAddr));
            std::memcpy(trampolineBuffer + 0x2C0, s22, sizeof(s22));

            // Slot 23 (0x2E0): GetCommandLineA (returns pointer to ascii string at 0x360)
            uint64_t acmdAddr = trampolineBaseVa + 0x360;
            uint8_t s23[11] = { 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3 };
            std::memcpy(s23 + 2, &acmdAddr, sizeof(acmdAddr));
            std::memcpy(trampolineBuffer + 0x2E0, s23, sizeof(s23));

            // Slot 24 (0x300): GetFileType (returns FILE_TYPE_CHAR = 2)
            const uint8_t s24[] = { 0xB8, 0x02, 0x00, 0x00, 0x00, 0xC3 };
            std::memcpy(trampolineBuffer + 0x300, s24, sizeof(s24));

            // Slot 25 (0x320): GetConsoleMode (writes 0x07 to [rdx] if non-null, returns 1)
            const uint8_t s25[] = { 0x48, 0x85, 0xD2, 0x74, 0x06, 0xC7, 0x02, 0x07, 0x00, 0x00, 0x00, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 };
            std::memcpy(trampolineBuffer + 0x320, s25, sizeof(s25));

            // Slot 26 (0x340): TlsAlloc (returns 1)
            const uint8_t s26[] = { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 };
            std::memcpy(trampolineBuffer + 0x340, s26, sizeof(s26));

            // 0x360: ASCII command line string
            const char asciiCmd[] = "surshell_app.exe";
            std::memcpy(trampolineBuffer + 0x360, asciiCmd, sizeof(asciiCmd));

            // 0x3A0: Wide command line string
            const char16_t wideCmd[] = u"surshell_app.exe";
            std::memcpy(trampolineBuffer + 0x3A0, wideCmd, sizeof(wideCmd));

            // 0x3F8: Initial bump pointer address (4MB into user buffer if 0x140000000, else 0x1000)
            uint64_t initialBump = (userHeapVa != 0) ? userHeapVa :
                                   ((targetBaseVa == 0x140000000ULL) ? (targetBaseVa + 0x400000ULL) : (trampolineBaseVa + 0x1000ULL));
            std::memcpy(trampolineBuffer + 0x3F8, &initialBump, sizeof(initialBump));

            // Slot 27 (0x400): malloc / _aligned_malloc (RCX = size)
            uint8_t s27[] = {
                0x48, 0x85, 0xC9,                         // test %rcx, %rcx
                0x75, 0x04,                               // jnz 1f
                0x48, 0xFF, 0xC1,                         // inc %rcx
                0x48, 0x89, 0xC8,                         // 1: mov %rcx, %rax
                0x48, 0x83, 0xC0, 0x0F,                   // add $15, %rax
                0x48, 0x83, 0xE0, 0xF0,                   // and $-16, %rax
                0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov $bumpAddr, %rdx
                0xF0, 0x48, 0x0F, 0xC1, 0x02,             // lock xadd %rax, (%rdx)
                0xC3                                      // ret
            };
            std::memcpy(s27 + 16, &bumpAddr, sizeof(bumpAddr));
            std::memcpy(trampolineBuffer + 0x400, s27, sizeof(s27));

            // Slot 28 (0x440): calloc (RCX = num, RDX = size)
            uint8_t s28[49] = {
                0x48, 0x0F, 0xAF, 0xCA,                   // imul %rdx, %rcx
                0x48, 0x85, 0xC9,                         // test %rcx, %rcx
                0x75, 0x04,                               // jnz 1f
                0x48, 0xFF, 0xC1,                         // inc %rcx
                0x48, 0x89, 0xC8,                         // 1: mov %rcx, %rax
                0x48, 0x83, 0xC0, 0x0F,                   // add $15, %rax
                0x48, 0x83, 0xE0, 0xF0,                   // and $-16, %rax
                0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov $bumpAddr, %rdx
                0xF0, 0x48, 0x0F, 0xC1, 0x02,             // lock xadd %rax, (%rdx)
                0x50,                                     // push %rax
                0x48, 0x89, 0xC7,                         // mov %rax, %rdi
                0x31, 0xC0,                               // xor %eax, %eax
                0xFC,                                     // cld
                0xF3, 0xAA,                               // rep stosb
                0x58,                                     // pop %rax
                0xC3                                      // ret
            };
            std::memcpy(s28 + 20, &bumpAddr, sizeof(bumpAddr));
            std::memcpy(trampolineBuffer + 0x440, s28, sizeof(s28));

            // Slot 29 (0x480): realloc (RCX = ptr, RDX = size)
            uint8_t s29[] = {
                0x48, 0x85, 0xC9,                         // test %rcx, %rcx
                0x75, 0x03,                               // jnz 1f
                0x48, 0x89, 0xD1,                         // mov %rdx, %rcx
                0x48, 0x89, 0xC8,                         // 1: mov %rcx, %rax
                0x48, 0x83, 0xC0, 0x0F,                   // add $15, %rax
                0x48, 0x83, 0xE0, 0xF0,                   // and $-16, %rax
                0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov $bumpAddr, %rdx
                0xF0, 0x48, 0x0F, 0xC1, 0x02,             // lock xadd %rax, (%rdx)
                0xC3                                      // ret
            };
            std::memcpy(s29 + 16, &bumpAddr, sizeof(bumpAddr));
            std::memcpy(trampolineBuffer + 0x480, s29, sizeof(s29));

            // Slot 30 (0x4C0): _initterm (RCX = first, RDX = last)
            const uint8_t s30[] = {
                0x48, 0x39, 0xD1,                         // 1: cmp %rdx, %rcx
                0x73, 0x1A,                               // jae 2f (+26)
                0x48, 0x8B, 0x01,                         // mov (%rcx), %rax
                0x48, 0x85, 0xC0,                         // test %rax, %rax
                0x74, 0x0D,                               // jz 3f (+13)
                0x51,                                     // push %rcx
                0x52,                                     // push %rdx
                0x48, 0x83, 0xEC, 0x20,                   // sub $32, %rsp
                0xFF, 0xD0,                               // call *%rax
                0x48, 0x83, 0xC4, 0x20,                   // add $32, %rsp
                0x5A,                                     // pop %rdx
                0x59,                                     // pop %rcx
                0x48, 0x83, 0xC1, 0x08,                   // 3: add $8, %rcx
                0xEB, 0xDF,                               // jmp 1b (-33)
                0xC3                                      // 2: ret
            };
            std::memcpy(trampolineBuffer + 0x4C0, s30, sizeof(s30));

            // Slot 31 (0x500): _initterm_e (RCX = first, RDX = last)
            const uint8_t s31[] = {
                0x48, 0x39, 0xD1,                         // 1: cmp %rdx, %rcx
                0x73, 0x24,                               // jae 2f (+36)
                0x48, 0x8B, 0x01,                         // mov (%rcx), %rax
                0x48, 0x85, 0xC0,                         // test %rax, %rax
                0x74, 0x17,                               // jz 3f (+23)
                0x51,                                     // push %rcx
                0x52,                                     // push %rdx
                0x48, 0x83, 0xEC, 0x20,                   // sub $32, %rsp
                0xFF, 0xD0,                               // call *%rax
                0x48, 0x83, 0xC4, 0x20,                   // add $32, %rsp
                0x5A,                                     // pop %rdx
                0x59,                                     // pop %rcx
                0x85, 0xC0,                               // test %eax, %eax
                0x75, 0x09,                               // jne 4f (+9)
                0x48, 0x83, 0xC1, 0x08,                   // 3: add $8, %rcx
                0xEB, 0xD5,                               // jmp 1b (-43)
                0x31, 0xC0,                               // 2: xor %eax, %eax
                0xC3,                                     // ret
                0xC3                                      // 4: ret
            };
            std::memcpy(trampolineBuffer + 0x500, s31, sizeof(s31));

            // Slot 32 (0x540): memcpy / memmove (RCX = dst, RDX = src, R8 = count)
            const uint8_t s32[] = {
                0x48, 0x89, 0xC8,                         // mov %rcx, %rax
                0x49, 0x89, 0xC9,                         // mov %r8, %rcx
                0x48, 0x89, 0xC7,                         // mov %rax, %rdi
                0x48, 0x89, 0xD6,                         // mov %rdx, %rsi
                0xFC,                                     // cld
                0xF3, 0xA4,                               // rep movsb
                0xC3                                      // ret
            };
            std::memcpy(trampolineBuffer + 0x540, s32, sizeof(s32));

            // Slot 33 (0x580): memset (RCX = dst, RDX = val, R8 = count)
            const uint8_t s33[] = {
                0x49, 0x89, 0xC9,                         // mov %rcx, %r9 (save dst)
                0x49, 0x89, 0xC1,                         // mov %r8, %rcx (count)
                0x4C, 0x89, 0xCF,                         // mov %r9, %rdi (dst)
                0x88, 0xD0,                               // mov %dl, %al
                0xFC,                                     // cld
                0xF3, 0xAA,                               // rep stosb
                0x4C, 0x89, 0xC8,                         // mov %r9, %rax (return dst)
                0xC3                                      // ret
            };
            std::memcpy(trampolineBuffer + 0x580, s33, sizeof(s33));

            // Slot 34 (0x5C0): memcmp (RCX = s1, RDX = s2, R8 = count)
            const uint8_t s34[] = {
                0x31, 0xC0,                               // xor %eax, %eax
                0x4D, 0x85, 0xC0,                         // test %r8, %r8
                0x74, 0x15,                               // jz 2f
                0x44, 0x0F, 0xB6, 0x09,                   // 1: movzbl (%rcx), %r9d
                0x44, 0x0F, 0xB6, 0x12,                   // movzbl (%rdx), %r10d
                0x45, 0x39, 0xD1,                         // cmp %r10d, %r9d
                0x75, 0x08,                               // jne 3f
                0x48, 0xFF, 0xC1,                         // inc %rcx
                0x48, 0xFF, 0xC2,                         // inc %rdx
                0x49, 0xFF, 0xC8,                         // dec %r8
                0x75, 0xEB,                               // jnz 1b
                0xC3,                                     // 2: ret
                0x45, 0x29, 0xD1,                         // 3: subl %r10d, %r9d
                0x44, 0x89, 0xC8,                         // mov %r9d, %eax
                0xC3                                      // ret
            };
            std::memcpy(trampolineBuffer + 0x5C0, s34, sizeof(s34));

            // Slot 35 (0x600): strlen (RCX = str)
            const uint8_t s35[] = {
                0x48, 0x31, 0xC0,                         // xor %rax, %rax
                0x80, 0x3C, 0x01, 0x00,                   // 1: cmpb $0, (%rcx, %rax)
                0x74, 0x04,                               // jz 2f
                0x48, 0xFF, 0xC0,                         // inc %rax
                0xEB, 0xF6,                               // jmp 1b
                0xC3                                      // 2: ret
            };
            std::memcpy(trampolineBuffer + 0x600, s35, sizeof(s35));

            // Slot 36 (0x640): strcmp (RCX = s1, RDX = s2)
            const uint8_t s36[] = {
                0x0F, 0xB6, 0x01,                         // 1: movzbl (%rcx), %eax
                0x44, 0x0F, 0xB6, 0x02,                   // movzbl (%rdx), %r8d
                0x41, 0x38, 0xC0,                         // cmp %al, %r8b
                0x75, 0x08,                               // jne 2f
                0x84, 0xC0,                               // test %al, %al
                0x74, 0x08,                               // jz 3f
                0x48, 0xFF, 0xC1,                         // inc %rcx
                0x48, 0xFF, 0xC2,                         // inc %rdx
                0xEB, 0xED,                               // jmp 1b
                0x44, 0x29, 0xC0,                         // 2: subl %r8d, %eax
                0xC3,                                     // ret
                0x31, 0xC0,                               // 3: xor %eax, %eax
                0xC3                                      // ret
            };
            std::memcpy(trampolineBuffer + 0x640, s36, sizeof(s36));

            // Slot 37 (0x680): __p___argc
            uint64_t argcAddr = trampolineBaseVa + 0x700;
            uint8_t s37[11] = { 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3 };
            std::memcpy(s37 + 2, &argcAddr, sizeof(argcAddr));
            std::memcpy(trampolineBuffer + 0x680, s37, sizeof(s37));

            // Slot 38 (0x6C0): __p___argv
            uint64_t argvAddr = trampolineBaseVa + 0x708;
            uint8_t s38[11] = { 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3 };
            std::memcpy(s38 + 2, &argvAddr, sizeof(argvAddr));
            std::memcpy(trampolineBuffer + 0x6C0, s38, sizeof(s38));

            // Slot 39 (0x700): __p__commode / __p__fmode / __p__environ / _errno
            uint64_t dataAddr = trampolineBaseVa + 0x720;
            uint8_t s39[11] = { 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3 };
            std::memcpy(s39 + 2, &dataAddr, sizeof(dataAddr));
            std::memcpy(trampolineBuffer + 0x700, s39, sizeof(s39));

            // Slot 40 (0x740): __acrt_iob_func (RCX = index)
            uint64_t streamsAddr = trampolineBaseVa + 0x740;
            uint8_t s40[18] = {
                0x48, 0xC1, 0xE1, 0x06,                   // shl $6, %rcx (rcx * 64)
                0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov $streamsAddr, %rax
                0x48, 0x01, 0xC8,                         // add %rcx, %rax
                0xC3                                      // ret
            };
            std::memcpy(s40 + 6, &streamsAddr, sizeof(streamsAddr));
            std::memcpy(trampolineBuffer + 0x740, s40, sizeof(s40));

            // Slot 41 (0x780): sqrtf / sqrt
            const uint8_t s41[] = { 0xF3, 0x0F, 0x51, 0xC0, 0xC3 };
            std::memcpy(trampolineBuffer + 0x780, s41, sizeof(s41));

            // Slot 42 (0x7C0): sinf / sin
            const uint8_t s42[] = {
                0x48, 0x83, 0xEC, 0x08,                   // sub $8, %rsp
                0xF3, 0x0F, 0x11, 0x04, 0x24,             // movss %xmm0, (%rsp)
                0xD9, 0x04, 0x24,                         // flds (%rsp)
                0xD9, 0xFE,                               // fsin
                0xD9, 0x1C, 0x24,                         // fstps (%rsp)
                0xF3, 0x0F, 0x10, 0x04, 0x24,             // movss (%rsp), %xmm0
                0x48, 0x83, 0xC4, 0x08,                   // add $8, %rsp
                0xC3                                      // ret
            };
            std::memcpy(trampolineBuffer + 0x7C0, s42, sizeof(s42));

            // Slot 43 (0x800): cosf / cos
            const uint8_t s43[] = {
                0x48, 0x83, 0xEC, 0x08,                   // sub $8, %rsp
                0xF3, 0x0F, 0x11, 0x04, 0x24,             // movss %xmm0, (%rsp)
                0xD9, 0x04, 0x24,                         // flds (%rsp)
                0xD9, 0xFF,                               // fcos
                0xD9, 0x1C, 0x24,                         // fstps (%rsp)
                0xF3, 0x0F, 0x10, 0x04, 0x24,             // movss (%rsp), %xmm0
                0x48, 0x83, 0xC4, 0x08,                   // add $8, %rsp
                0xC3                                      // ret
            };
            std::memcpy(trampolineBuffer + 0x800, s43, sizeof(s43));

            // Slot 44 (0x840): round / roundf
            const uint8_t s44[] = { 0x66, 0x0F, 0x3A, 0x0A, 0xC0, 0x00, 0xC3 };
            std::memcpy(trampolineBuffer + 0x840, s44, sizeof(s44));

            // Slot 45 (0x880): floorf / ceilf
            const uint8_t s45[] = { 0x66, 0x0F, 0x3A, 0x0A, 0xC0, 0x01, 0xC3 };
            std::memcpy(trampolineBuffer + 0x880, s45, sizeof(s45));

            // Slot 46 (0x8C0): fwrite / puts / fputc
            const uint8_t s46[] = {
                0x49, 0x0F, 0xAF, 0xD0,                   // imul %r8, %rdx
                0x48, 0x89, 0xCA,                         // mov %rcx, %rdx (arg1 = buf)
                0x49, 0x89, 0xD0,                         // mov %rdx, %r8 (arg2 = len)
                0xB8, 0x25, 0x00, 0x00, 0x00,             // mov $0x25, %eax
                0x0F, 0x05,                               // syscall
                0xC3                                      // ret
            };
            std::memcpy(trampolineBuffer + 0x8C0, s46, sizeof(s46));

            // Data Area initialization
            int initArgc = 1;
            std::memcpy(trampolineBuffer + 0x700, &initArgc, sizeof(initArgc));

            uint64_t arg0Ptr = trampolineBaseVa + 0x360;
            uint64_t arg1Null = 0;
            std::memcpy(trampolineBuffer + 0x708, &arg0Ptr, sizeof(arg0Ptr));
            std::memcpy(trampolineBuffer + 0x710, &arg1Null, sizeof(arg1Null));

            int initZero = 0;
            std::memcpy(trampolineBuffer + 0x720, &initZero, sizeof(initZero));
            std::memcpy(trampolineBuffer + 0x724, &initZero, sizeof(initZero));
            std::memcpy(trampolineBuffer + 0x728, &initZero, sizeof(initZero));
        }

        // 8. Parse and Bind Imports
        if (nt->optionalHeader.numberOfRvaAndSizes > pe::IMAGE_DIRECTORY_ENTRY_IMPORT) {
            const auto& importDir = nt->optionalHeader.dataDirectory[pe::IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (importDir.virtualAddress != 0 && importDir.size != 0 &&
                importDir.virtualAddress + importDir.size <= destinationBufferSize) {
                const auto* desc = reinterpret_cast<const pe::ImageImportDescriptor*>(destinationBuffer + importDir.virtualAddress);
                while (desc->name != 0 && desc->firstThunk != 0 &&
                       desc->name < destinationBufferSize && desc->firstThunk < destinationBufferSize) {
                    uint32_t thunkRva = desc->originalFirstThunk ? desc->originalFirstThunk : desc->firstThunk;
                    if (thunkRva >= destinationBufferSize) break;

                    const auto* thunkArray = reinterpret_cast<const uint64_t*>(destinationBuffer + thunkRva);
                    size_t idx = 0;
                    while (thunkRva + (idx + 1) * sizeof(uint64_t) <= destinationBufferSize && thunkArray[idx] != 0) {
                        uint64_t val = thunkArray[idx];
                        uint32_t iatRva = desc->firstThunk + static_cast<uint32_t>(idx * sizeof(uint64_t));
                        if (iatRva + sizeof(uint64_t) > destinationBufferSize) break;

                        uint64_t targetTrampoline = trampolineBaseVa + 0x200; // default to UniversalSuccess (returns 0/S_OK)

                        if (!(val & pe::IMAGE_ORDINAL_FLAG64)) {
                            uint32_t nameRva = static_cast<uint32_t>(val & 0xFFFFFFFF);
                            if (nameRva + 2 < destinationBufferSize) {
                                const auto* ibn = reinterpret_cast<const pe::ImageImportByName*>(destinationBuffer + nameRva);
                                const char* symName = ibn->name;

                                if (streq(symName, "ExitProcess") || streq(symName, "NtTerminateProcess") || streq(symName, "abort") || streq(symName, "_exit") || streq(symName, "exit")) {
                                    targetTrampoline = trampolineBaseVa + 0x00;
                                } else if (streq(symName, "NtQuerySystemInformation")) {
                                    targetTrampoline = trampolineBaseVa + 0x20;
                                } else if (streq(symName, "ReadFile") || streq(symName, "NtReadFile")) {
                                    targetTrampoline = trampolineBaseVa + 0x40;
                                } else if (streq(symName, "WriteFile") || streq(symName, "NtWriteFile")) {
                                    targetTrampoline = trampolineBaseVa + 0x60;
                                } else if (streq(symName, "NtYieldExecution") || streq(symName, "SwitchToThread") || streq(symName, "Sleep")) {
                                    targetTrampoline = trampolineBaseVa + 0x80;
                                } else if (streq(symName, "NtAllocateVirtualMemory") || streq(symName, "VirtualAlloc")) {
                                    targetTrampoline = trampolineBaseVa + 0xA0;
                                } else if (streq(symName, "CloseHandle") || streq(symName, "NtClose")) {
                                    targetTrampoline = trampolineBaseVa + 0xC0;
                                } else if (streq(symName, "GetStdHandle")) {
                                    targetTrampoline = trampolineBaseVa + 0xE0;
                                } else if (streq(symName, "GetTickCount64") || streq(symName, "GetTickCount")) {
                                    targetTrampoline = trampolineBaseVa + 0x100;
                                } else if (streq(symName, "QueryPerformanceCounter")) {
                                    targetTrampoline = trampolineBaseVa + 0x120;
                                } else if (streq(symName, "QueryPerformanceFrequency")) {
                                    targetTrampoline = trampolineBaseVa + 0x140;
                                } else if (streq(symName, "GetCurrentProcessId") || streq(symName, "GetCurrentThreadId") || streq(symName, "GetThreadId")) {
                                    targetTrampoline = trampolineBaseVa + 0x160;
                                } else if (streq(symName, "GetLastError")) {
                                    targetTrampoline = trampolineBaseVa + 0x180;
                                } else if (streq(symName, "SetLastError") || streq(symName, "InitializeCriticalSection") ||
                                           streq(symName, "DeleteCriticalSection") || streq(symName, "EnterCriticalSection") ||
                                           streq(symName, "LeaveCriticalSection") || streq(symName, "InitializeCriticalSectionAndSpinCount") ||
                                           streq(symName, "InitializeCriticalSectionEx") || streq(symName, "AcquireSRWLockExclusive") ||
                                           streq(symName, "ReleaseSRWLockExclusive") || streq(symName, "TryAcquireSRWLockExclusive") ||
                                           streq(symName, "TryEnterCriticalSection") || streq(symName, "WakeConditionVariable") ||
                                           streq(symName, "WakeAllConditionVariable") || streq(symName, "SleepConditionVariableSRW")) {
                                    targetTrampoline = trampolineBaseVa + 0x1A0;
                                } else if (streq(symName, "GetModuleHandleW") || streq(symName, "GetModuleHandleA") || streq(symName, "GetModuleHandleExW")) {
                                    targetTrampoline = trampolineBaseVa + 0x1C0;
                                } else if (streq(symName, "VirtualFree") || streq(symName, "SetEvent") || streq(symName, "ResetEvent") ||
                                           streq(symName, "TlsSetValue") || streq(symName, "TlsFree") || streq(symName, "IsNetworkAlive") ||
                                           streq(symName, "IsDestinationReachableW") || streq(symName, "IsProcessorFeaturePresent") ||
                                           streq(symName, "HeapFree") || streq(symName, "free") || streq(symName, "_aligned_free") ||
                                           streq(symName, "LocalFree")) {
                                    targetTrampoline = trampolineBaseVa + 0x1E0;
                                } else if (streq(symName, "GetCurrentPackageFullName")) {
                                    targetTrampoline = trampolineBaseVa + 0x220;
                                } else if (streq(symName, "CreateEventW") || streq(symName, "CreateEventA") || streq(symName, "CreateEventExW")) {
                                    targetTrampoline = trampolineBaseVa + 0x240;
                                } else if (streq(symName, "GetSystemTimeAsFileTime")) {
                                    targetTrampoline = trampolineBaseVa + 0x260;
                                } else if (streq(symName, "GetProcessHeap")) {
                                    targetTrampoline = trampolineBaseVa + 0x280;
                                } else if (streq(symName, "HeapAlloc") || streq(symName, "HeapReAlloc")) {
                                    targetTrampoline = trampolineBaseVa + 0x2A0;
                                } else if (streq(symName, "GetCommandLineW")) {
                                    targetTrampoline = trampolineBaseVa + 0x2C0;
                                } else if (streq(symName, "GetCommandLineA")) {
                                    targetTrampoline = trampolineBaseVa + 0x2E0;
                                } else if (streq(symName, "GetFileType")) {
                                    targetTrampoline = trampolineBaseVa + 0x300;
                                } else if (streq(symName, "GetConsoleMode") || streq(symName, "GetConsoleCP") || streq(symName, "GetConsoleOutputCP")) {
                                    targetTrampoline = trampolineBaseVa + 0x320;
                                } else if (streq(symName, "TlsAlloc") || streq(symName, "FlsAlloc")) {
                                    targetTrampoline = trampolineBaseVa + 0x340;
                                } else if (streq(symName, "malloc") || streq(symName, "_aligned_malloc")) {
                                    targetTrampoline = trampolineBaseVa + 0x400;
                                } else if (streq(symName, "calloc")) {
                                    targetTrampoline = trampolineBaseVa + 0x440;
                                } else if (streq(symName, "realloc")) {
                                    targetTrampoline = trampolineBaseVa + 0x480;
                                } else if (streq(symName, "_initterm")) {
                                    targetTrampoline = trampolineBaseVa + 0x4C0;
                                } else if (streq(symName, "_initterm_e")) {
                                    targetTrampoline = trampolineBaseVa + 0x500;
                                } else if (streq(symName, "memcpy") || streq(symName, "memmove")) {
                                    targetTrampoline = trampolineBaseVa + 0x540;
                                } else if (streq(symName, "memset")) {
                                    targetTrampoline = trampolineBaseVa + 0x580;
                                } else if (streq(symName, "memcmp") || streq(symName, "memchr")) {
                                    targetTrampoline = trampolineBaseVa + 0x5C0;
                                } else if (streq(symName, "strlen") || streq(symName, "strnlen") || streq(symName, "wcslen") || streq(symName, "wcsnlen")) {
                                    targetTrampoline = trampolineBaseVa + 0x600;
                                } else if (streq(symName, "strcmp") || streq(symName, "strncmp") || streq(symName, "_strcoll_l") || streq(symName, "_wcscoll_l")) {
                                    targetTrampoline = trampolineBaseVa + 0x640;
                                } else if (streq(symName, "__p___argc")) {
                                    targetTrampoline = trampolineBaseVa + 0x680;
                                } else if (streq(symName, "__p___argv") || streq(symName, "__p___wargv")) {
                                    targetTrampoline = trampolineBaseVa + 0x6C0;
                                } else if (streq(symName, "__p__commode") || streq(symName, "__p__fmode") || streq(symName, "__p__environ") ||
                                           streq(symName, "__p___initenv") || streq(symName, "_errno") || streq(symName, "__sys_nerr")) {
                                    targetTrampoline = trampolineBaseVa + 0x700;
                                } else if (streq(symName, "__acrt_iob_func")) {
                                    targetTrampoline = trampolineBaseVa + 0x740;
                                } else if (streq(symName, "sqrtf") || streq(symName, "sqrt")) {
                                    targetTrampoline = trampolineBaseVa + 0x780;
                                } else if (streq(symName, "sinf") || streq(symName, "sin")) {
                                    targetTrampoline = trampolineBaseVa + 0x7C0;
                                } else if (streq(symName, "cosf") || streq(symName, "cos")) {
                                    targetTrampoline = trampolineBaseVa + 0x800;
                                } else if (streq(symName, "round") || streq(symName, "roundf")) {
                                    targetTrampoline = trampolineBaseVa + 0x840;
                                } else if (streq(symName, "floorf") || streq(symName, "floor") || streq(symName, "ceilf") || streq(symName, "ceil")) {
                                    targetTrampoline = trampolineBaseVa + 0x880;
                                } else if (streq(symName, "fwrite") || streq(symName, "puts") || streq(symName, "fputc") || streq(symName, "fputwc")) {
                                    targetTrampoline = trampolineBaseVa + 0x8C0;
                                } else if (streq(symName, "TlsGetValue") || streq(symName, "FlsGetValue") || streq(symName, "IsDebuggerPresent") ||
                                           streq(symName, "_set_app_type") || streq(symName, "_configure_narrow_argv") ||
                                           streq(symName, "_initialize_narrow_environment") || streq(symName, "_seh_filter_exe") ||
                                           streq(symName, "_crt_atexit") || streq(symName, "_set_invalid_parameter_handler") ||
                                           streq(symName, "_set_new_mode") || streq(symName, "fpreset") || streq(symName, "setvbuf") ||
                                           streq(symName, "setbuf") || streq(symName, "fflush") || streq(symName, "fclose") ||
                                           streq(symName, "_lock_file") || streq(symName, "_unlock_file") ||
                                           streq(symName, "SetUnhandledExceptionFilter") || streq(symName, "VirtualProtect") ||
                                           streq(symName, "VirtualQuery") || streq(symName, "InitOnceExecuteOnce")) {
                                    targetTrampoline = trampolineBaseVa + 0x200;
                                }
                            }
                        }

                        *reinterpret_cast<uint64_t*>(destinationBuffer + iatRva) = targetTrampoline;
                        outInfo.boundImportsCount++;
                        idx++;
                    }
                    desc++;
                }
            }
        }

        // 9. Register Process & Initial Thread Context
        uint64_t entryPointVa = targetBaseVa + nt->optionalHeader.addressOfEntryPoint;
        outInfo.imageBaseVa = targetBaseVa;
        outInfo.entryPointVa = entryPointVa;

        auto& pm = UserProcessManager64::get();
        auto* proc = pm.createProcess(imageName, pid, parentPid,
                                      targetBaseVa, nt->optionalHeader.sizeOfImage,
                                      userStackTopVa, userStackSize,
                                      pebVa, tebVa);
        if (proc) {
            proc->peb.imageBaseAddress = targetBaseVa;
            proc->peb.imageSubsystem = nt->optionalHeader.subsystem;
            proc->peb.imageSubsystemMajorVersion = nt->optionalHeader.majorSubsystemVersion;
            proc->peb.imageSubsystemMinorVersion = nt->optionalHeader.minorSubsystemVersion;
            proc->peb.osMajorVersion = 10;
            proc->peb.osMinorVersion = 0;
            proc->peb.osBuildNumber = 26100;
        }

        auto& sched = UserThreadScheduler64::get();
        sched.createThread(pid, pid, 16, entryPointVa, userStackTopVa, tebVa, imageName);

        return micant::NtStatus::Success;
    }
};

} // namespace micant::um

