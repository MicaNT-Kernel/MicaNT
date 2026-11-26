// ============================================================================
// MicaNT: Sovereign Freestanding Per-Process CR3 & Preemptive Scheduler Engine
// (process_cr3_baremetal.hpp)
//
// Description:
//   Pure -ffreestanding -nostdlib ISO C++23 bare-metal multi-process executive
//   compiled directly into ntoskrnl.exe.
//
//   Implements genuine Dave Cutler Windows NT virtual memory, loader & scheduling:
//     - Isolated per-process 4-level x86_64 page tables (distinct hardware CR3)
//     - Identical canonical user virtual layout across all processes in PML4[1]:
//         0x00000080'00000000 (256 KB) : PE32+ Executable Image (.reloc applied)
//         0x00000080'00100000 (512 KB) : Per-Process System32 DLL Arena (PEB.Ldr)
//         0x00000080'00200000 ( 64 KB) : Private Ring-3 User Stack
//         0x00000080'00300000 ( 16 KB) : Private PEB (+0x000) & TEB (+0x200)
//         0x00000080'00400000+         : Private PFN-backed VirtualAlloc Heap
//     - Unified 20-qword (160-byte) KTRAP_FRAME (TrapFrame64) shared across both
//       Local APIC Timer interrupts (Vector 0x30) and KiSystemCall64Entry
//     - Real hardware preemptive CR3 + MSR_GS_BASE + VAD context switching
//     - Real blocking synchronization: Running -> Waiting -> Ready transitions
//       for NtWaitForSingleObject (SSN 0x04) + NtQueryInformationProcess (0x19)
//     - Runtime dynamic DLL loading into the calling process's private CR3 arena
//       via NtLoadDll (SSN 0x60) -> LoadLibraryW / GetModuleHandleW
//     - Live NtCreateUserProcess (SSN 0x58) spawning isolated child PE32+ processes
//       directly off the mounted NTFS 3.1 volume via AHCI Bus-Master DMA
//
// Zero mocks, zero host STL, zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "micant/km/hal/cpu.hpp"
#include "micant/km/executive/mm_baremetal.hpp"
#include "micant/km/executive/pe_loader_baremetal.hpp"
#include "micant/km/drivers/fs/ntfs_baremetal.hpp"

namespace micant::km::ps_baremetal {

// ============================================================================
// 1. Canonical Per-Process Virtual Address Layout (Private PML4[1])
// ============================================================================

inline constexpr uint64_t PROC_VA_IMAGE_BASE = 0x00000080'00000000ULL;
inline constexpr size_t   PROC_IMAGE_SIZE    = 256 * 1024; // 256 KB (64 x 4KB pages)

inline constexpr uint64_t PROC_VA_DLL_BASE   = 0x00000080'00100000ULL;
inline constexpr size_t   PROC_DLL_SIZE      = 512 * 1024; // 512 KB (128 x 4KB pages)

inline constexpr uint64_t PROC_VA_STACK_BASE = 0x00000080'00200000ULL;
inline constexpr size_t   PROC_STACK_SIZE    = 64 * 1024;  // 64 KB (16 x 4KB pages)
inline constexpr uint64_t PROC_VA_STACK_TOP  = PROC_VA_STACK_BASE + PROC_STACK_SIZE - 8; // RSP % 16 == 8

inline constexpr uint64_t PROC_VA_TEB_BASE   = 0x00000080'00300000ULL;
inline constexpr size_t   PROC_TEB_SIZE      = 16 * 1024;  // 16 KB (4 x 4KB pages)
inline constexpr uint64_t PROC_VA_PEB        = PROC_VA_TEB_BASE + 0x0000ULL;
inline constexpr uint64_t PROC_VA_TEB        = PROC_VA_TEB_BASE + 0x0200ULL;

inline constexpr uint64_t PROC_VA_HEAP_BASE  = 0x00000080'00400000ULL;

inline constexpr uint32_t NT_STILL_ACTIVE    = 259u;       // STATUS_PENDING / STILL_ACTIVE

// ============================================================================
// 2. Unified Hardware x86_64 KTRAP_FRAME (160 bytes, 16-byte aligned)
// ============================================================================

struct TrapFrame64 {
    uint64_t r15{0};
    uint64_t r14{0};
    uint64_t r13{0};
    uint64_t r12{0};
    uint64_t r11{0};
    uint64_t r10{0};
    uint64_t r9{0};
    uint64_t r8{0};
    uint64_t rbp{0};
    uint64_t rdi{0};
    uint64_t rsi{0};
    uint64_t rdx{0};
    uint64_t rcx{0};
    uint64_t rbx{0};
    uint64_t rax{0};
    // Hardware IRETQ frame pushed by CPU on interrupt or synthesized by KiSystemCall64Entry:
    uint64_t rip{0};
    uint64_t cs{0};
    uint64_t rflags{0};
    uint64_t rsp{0};
    uint64_t ss{0};
};
static_assert(sizeof(TrapFrame64) == 160, "TrapFrame64 must be exactly 160 bytes (20 qwords)");

enum class Cr3ProcState : uint8_t {
    Free       = 0,
    Ready      = 1,
    Running    = 2,
    Waiting    = 3,
    Terminated = 4
};

struct Cr3ProcessSlot {
    bool         inUse{false};
    Cr3ProcState state{Cr3ProcState::Free};
    uint32_t     pid{0};
    uint32_t     tid{0};
    uint32_t     parentPid{0};
    uint32_t     exitCode{NT_STILL_ACTIVE};
    uint32_t     waitTargetPid{0};
    char         imageName[64]{};

    uint64_t     cr3Phys{0};
    uint64_t     entryPointVa{0};
    uint64_t     pebVa{PROC_VA_PEB};
    uint64_t     tebVa{PROC_VA_TEB};

    uint32_t     exeRelocs{0};
    uint32_t     exeImportsBound{0};
    uint32_t     dllCount{0};
    uint32_t     dllImportsBound{0};

    uint64_t     cpuTicks{0};
    uint64_t     preemptedInCount{0};
    uint64_t     yieldCount{0};
    uint64_t     waitCount{0};
    uint64_t     syscallCount{0};

    TrapFrame64  trapFrame{};
    alignas(16) uint8_t fxState[512]{};
    bool         fxValid{false};
    mm_baremetal::BareMetalVadTable vadTable{};
    pe_baremetal::BareMetalPeLoader peLoader{};
};

// Scratch kernel buffer for raw EXE DMA reads during NtCreateUserProcess (512 KB)
alignas(4096) static uint8_t s_mpExeRawBuf[524288];

// ============================================================================
// 3. Bare-Metal Per-Process CR3 & Preemptive Scheduler Manager
// ============================================================================

class BareMetalCr3ProcessManager {
public:
    static constexpr size_t MAX_PROCS = 8;

private:
    Cr3ProcessSlot m_procs[MAX_PROCS]{};
    size_t         m_currentIdx{0};
    uint32_t       m_nextPid{2001};
    uint64_t       m_kernelCr3Phys{0};
    bool           m_active{false};

    uint64_t       m_totalClockTicksInR3{0};
    uint64_t       m_totalPreemptiveSwitches{0};
    uint64_t       m_totalVoluntaryYields{0};
    uint64_t       m_totalBlockingWaits{0};
    uint64_t       m_totalWaitWakeups{0};
    uint64_t       m_totalProcessesCreated{0};
    uint64_t       m_totalProcessesExited{0};

    static inline uint64_t readCr3() noexcept {
        uint64_t cr3 = 0;
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
#endif
        return cr3;
    }

    static inline void writeCr3(uint64_t cr3Phys) noexcept {
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("mov %0, %%cr3" : : "r"(cr3Phys) : "memory");
#else
        (void)cr3Phys;
#endif
    }

    static inline void writeGsBase(uint64_t tebVa) noexcept {
        micant::cpu::cpuWrmsr(micant::cpu::MSR_GS_BASE, tebVa);
    }

    static void copyShortName(char* dst, const char* src, size_t cap) noexcept {
        if (!dst || cap == 0) return;
        const char* base = src ? src : "proc.exe";
        for (const char* p = base; *p; ++p) {
            if (*p == '\\' || *p == '/') base = p + 1;
        }
        size_t i = 0;
        while (base[i] && i + 1 < cap) {
            dst[i] = base[i];
            ++i;
        }
        dst[i] = '\0';
    }

    int findNextReadySlot(size_t afterIdx) const noexcept {
        for (size_t step = 1; step <= MAX_PROCS; ++step) {
            size_t idx = (afterIdx + step) % MAX_PROCS;
            if (m_procs[idx].inUse && m_procs[idx].state == Cr3ProcState::Ready) {
                return static_cast<int>(idx);
            }
        }
        return -1;
    }

    int findSlotByPid(uint32_t pid) const noexcept {
        for (size_t i = 0; i < MAX_PROCS; ++i) {
            if (m_procs[i].inUse && m_procs[i].pid == pid) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

public:
    static BareMetalCr3ProcessManager& get() noexcept {
        static BareMetalCr3ProcessManager instance;
        return instance;
    }

    void reset(uint64_t kernelCr3Phys) noexcept {
        m_kernelCr3Phys = kernelCr3Phys & mm_baremetal::PTE_PHYS_MASK;
        m_active = false;
        m_currentIdx = 0;
        m_nextPid = 2001;
        m_totalClockTicksInR3 = 0;
        m_totalPreemptiveSwitches = 0;
        m_totalVoluntaryYields = 0;
        m_totalBlockingWaits = 0;
        m_totalWaitWakeups = 0;
        m_totalProcessesCreated = 0;
        m_totalProcessesExited = 0;
        for (size_t i = 0; i < MAX_PROCS; ++i) {
            m_procs[i] = Cr3ProcessSlot{};
        }
    }

    [[nodiscard]] bool isActive() const noexcept { return m_active; }
    [[nodiscard]] uint64_t getKernelCr3Phys() const noexcept { return m_kernelCr3Phys; }
    [[nodiscard]] size_t getCurrentIndex() const noexcept { return m_currentIdx; }
    [[nodiscard]] Cr3ProcessSlot* getCurrentProcess() noexcept {
        return (m_currentIdx < MAX_PROCS && m_procs[m_currentIdx].inUse) ? &m_procs[m_currentIdx] : nullptr;
    }
    [[nodiscard]] const Cr3ProcessSlot* getSlot(size_t idx) const noexcept {
        return (idx < MAX_PROCS && m_procs[idx].inUse) ? &m_procs[idx] : nullptr;
    }
    [[nodiscard]] uint64_t getTotalClockTicksInR3() const noexcept { return m_totalClockTicksInR3; }
    [[nodiscard]] uint64_t getTotalPreemptiveSwitches() const noexcept { return m_totalPreemptiveSwitches; }
    [[nodiscard]] uint64_t getTotalVoluntaryYields() const noexcept { return m_totalVoluntaryYields; }
    [[nodiscard]] uint64_t getTotalBlockingWaits() const noexcept { return m_totalBlockingWaits; }
    [[nodiscard]] uint64_t getTotalWaitWakeups() const noexcept { return m_totalWaitWakeups; }
    [[nodiscard]] uint64_t getTotalProcessesCreated() const noexcept { return m_totalProcessesCreated; }
    [[nodiscard]] uint64_t getTotalProcessesExited() const noexcept { return m_totalProcessesExited; }

    /**
     * @brief Create a fully isolated Ring-3 Win32 PE32+ process with its OWN hardware CR3,
     *        private PFN-backed PML4[1] address space, per-process PEB.Ldr (peLoader),
     *        dynamically loaded kernel32.dll -> ntdll.dll off live NTFS 3.1, private TEB/PEB,
     *        and IF=1 preemptive TrapFrame64.
     * @return Process slot index (>= 0) on success, or -1 on error.
     */
    int createProcessFromNtfs(
        ntfs_baremetal::BareMetalNtfsVolume& ntfs,
        const char* ntfsExePath,
        uint32_t parentPid = 4,
        const char* cmdLineAscii = nullptr
    ) noexcept {
        if (!ntfs.isMounted() || !ntfsExePath || m_kernelCr3Phys == 0) return -1;

        int freeSlot = -1;
        for (size_t i = 0; i < MAX_PROCS; ++i) {
            if (!m_procs[i].inUse) {
                freeSlot = static_cast<int>(i);
                break;
            }
        }
        if (freeSlot < 0) return -1;

        // 1. Read raw PE32+ executable bytes directly off live NTFS 3.1 via AHCI DMA
        size_t exeBytes = 0;
        if (!ntfs.readFile(ntfsExePath, s_mpExeRawBuf, sizeof(s_mpExeRawBuf), exeBytes) || exeBytes < 128) {
            return -1;
        }

        // 2. Allocate brand-new isolated hardware PML4 (CR3) cloning Ring 0 kernel + MMIO
        uint64_t procCr3 = mm_baremetal::BareMetalPaging::cloneKernelPml4(m_kernelCr3Phys);
        if (procCr3 == 0) return -1;

        // 3. Map private PFN-backed physical pages into canonical PML4[1] virtual layout
        const uint64_t rwxUser = mm_baremetal::PTE_PRESENT | mm_baremetal::PTE_WRITABLE | mm_baremetal::PTE_USER;
        if (!mm_baremetal::BareMetalPaging::mapPrivateUserRange(procCr3, PROC_VA_IMAGE_BASE, PROC_IMAGE_SIZE, rwxUser)) return -1;
        if (!mm_baremetal::BareMetalPaging::mapPrivateUserRange(procCr3, PROC_VA_DLL_BASE,   PROC_DLL_SIZE,   rwxUser)) return -1;
        if (!mm_baremetal::BareMetalPaging::mapPrivateUserRange(procCr3, PROC_VA_STACK_BASE, PROC_STACK_SIZE, rwxUser)) return -1;
        if (!mm_baremetal::BareMetalPaging::mapPrivateUserRange(procCr3, PROC_VA_TEB_BASE,   PROC_TEB_SIZE,   rwxUser)) return -1;

        auto& slot = m_procs[freeSlot];
        slot = Cr3ProcessSlot{};

        // 4. Switch CR3 temporarily into the new process's address space to map PE32+ + DLLs
        uint64_t savedCr3 = readCr3();
        writeCr3(procCr3);

        slot.peLoader.initialize(reinterpret_cast<uint8_t*>(PROC_VA_DLL_BASE), PROC_DLL_SIZE);

        pe_baremetal::LoadedPeModule exeMod{};
        bool mapped = slot.peLoader.mapAndRelocateImage(
            s_mpExeRawBuf, exeBytes,
            reinterpret_cast<uint8_t*>(PROC_VA_IMAGE_BASE), PROC_IMAGE_SIZE,
            ntfsExePath, exeMod
        );
        bool importsOk = mapped && slot.peLoader.bindImportsFromNtfs(ntfs, exeMod);

        bool allDllsOk = true;
        uint32_t dllCount = static_cast<uint32_t>(slot.peLoader.getLoadedModuleCount());
        uint32_t dllImports = 0;
        for (size_t d = 0; d < slot.peLoader.getLoadedModuleCount(); ++d) {
            const auto* mod = slot.peLoader.getModule(d);
            if (mod && mod->inUse) {
                dllImports += mod->importsBound;
                if (mod->importsUnresolved != 0) allDllsOk = false;
            }
        }

        if (!mapped || !importsOk || !allDllsOk || exeMod.entryPointVa == 0) {
            writeCr3(savedCr3);
            slot = Cr3ProcessSlot{};
            return -1;
        }

        // 5. Populate canonical Windows x64 TEB (+0x30 Self, +0x40 PID, +0x48 TID, +0x60 PEB)
        //    and ProcessParameters CommandLine at PEB+0x1000 (ASCII) & PEB+0x1400 (UTF-16LE)
        uint32_t pid = m_nextPid++;
        uint32_t tid = pid + 100;
        std::memset(reinterpret_cast<void*>(PROC_VA_TEB_BASE), 0, PROC_TEB_SIZE);
        *reinterpret_cast<uint64_t*>(PROC_VA_TEB + 0x30) = PROC_VA_TEB;
        *reinterpret_cast<uint32_t*>(PROC_VA_TEB + 0x40) = pid;
        *reinterpret_cast<uint32_t*>(PROC_VA_TEB + 0x48) = tid;
        *reinterpret_cast<uint64_t*>(PROC_VA_TEB + 0x60) = PROC_VA_PEB;

        const char* effectiveCmd = (cmdLineAscii && cmdLineAscii[0]) ? cmdLineAscii : ntfsExePath;
        if (effectiveCmd) {
            auto* cmdA = reinterpret_cast<char*>(PROC_VA_PEB + 0x1000ULL);
            auto* cmdW = reinterpret_cast<uint16_t*>(PROC_VA_PEB + 0x1400ULL);
            size_t ci = 0;
            while (effectiveCmd[ci] && ci < 250) {
                cmdA[ci] = effectiveCmd[ci];
                cmdW[ci] = static_cast<uint16_t>(static_cast<unsigned char>(effectiveCmd[ci]));
                ++ci;
            }
            cmdA[ci] = '\0';
            cmdW[ci] = 0;
        }

        // Restore caller CR3 immediately
        writeCr3(savedCr3);

        // 6. Initialize Cr3ProcessSlot, private VAD Table (PFN-backed heap at 0x80'00400000), & TrapFrame64
        slot.inUse = true;
        slot.state = Cr3ProcState::Ready;
        slot.pid = pid;
        slot.tid = tid;
        slot.parentPid = parentPid;
        slot.exitCode = NT_STILL_ACTIVE;
        slot.waitTargetPid = 0;
        copyShortName(slot.imageName, ntfsExePath, sizeof(slot.imageName));

        slot.cr3Phys = procCr3;
        slot.entryPointVa = exeMod.entryPointVa;
        slot.pebVa = PROC_VA_PEB;
        slot.tebVa = PROC_VA_TEB;
        slot.exeRelocs = exeMod.relocsApplied;
        slot.exeImportsBound = exeMod.importsBound;
        slot.dllCount = dllCount;
        slot.dllImportsBound = dllImports;

        slot.vadTable.initialize();
        slot.vadTable.registerRegion(PROC_VA_IMAGE_BASE, PROC_IMAGE_SIZE, mm_baremetal::MEM_COMMIT, mm_baremetal::PAGE_EXECUTE_READWRITE);
        slot.vadTable.registerRegion(PROC_VA_DLL_BASE,   PROC_DLL_SIZE,   mm_baremetal::MEM_COMMIT, mm_baremetal::PAGE_EXECUTE_READWRITE);
        slot.vadTable.registerRegion(PROC_VA_STACK_BASE, PROC_STACK_SIZE, mm_baremetal::MEM_COMMIT, mm_baremetal::PAGE_READWRITE, true);
        slot.vadTable.registerRegion(PROC_VA_TEB_BASE,   PROC_TEB_SIZE,   mm_baremetal::MEM_COMMIT, mm_baremetal::PAGE_READWRITE);
        slot.vadTable.setNextDynamicVa(PROC_VA_HEAP_BASE);

        // Hardware IRETQ frame: Ring 3 CS=0x33, SS=0x2B, RFLAGS=0x0202 (IF=1 for real LAPIC preemption!)
        slot.trapFrame.rip    = exeMod.entryPointVa;
        slot.trapFrame.cs     = micant::cpu::SELECTOR_UCODE64;
        slot.trapFrame.rflags = 0x00000202ULL; // IF=1!
        slot.trapFrame.rsp    = PROC_VA_STACK_TOP;
        slot.trapFrame.ss     = micant::cpu::SELECTOR_UDATA64;

        m_totalProcessesCreated++;
        return freeSlot;
    }

    /**
     * @brief Dynamically load or query a System32 DLL directly inside the active process's private CR3 arena.
     */
    const pe_baremetal::LoadedPeModule* loadOrFindDllForCurrentProcess(
        ntfs_baremetal::BareMetalNtfsVolume& ntfs,
        const char* dllName,
        bool loadIfMissing
    ) noexcept {
        auto* cur = getCurrentProcess();
        if (!cur || !dllName) return nullptr;
        if (!loadIfMissing) {
            return cur->peLoader.findModule(dllName);
        }
        const auto* mod = cur->peLoader.loadSystemDllFromNtfs(ntfs, dllName);
        if (mod) {
            cur->dllCount = static_cast<uint32_t>(cur->peLoader.getLoadedModuleCount());
        }
        return mod;
    }

    /**
     * @brief Query process exit code (returns STILL_ACTIVE = 259 while running/ready/waiting).
     */
    bool queryProcessExitCode(uint32_t targetPid, uint32_t& outExitCode, bool& outStillActive) const noexcept {
        int idx = findSlotByPid(targetPid);
        if (idx < 0) return false;
        const auto& sl = m_procs[idx];
        if (sl.state == Cr3ProcState::Terminated) {
            outStillActive = false;
            outExitCode = sl.exitCode;
        } else {
            outStillActive = true;
            outExitCode = NT_STILL_ACTIVE;
        }
        return true;
    }

    /**
     * @brief Activate initial process slot for Ring-3 hardware dispatch.
     */
    bool activateInitialSlot(size_t idx) noexcept {
        if (idx >= MAX_PROCS || !m_procs[idx].inUse || m_procs[idx].state != Cr3ProcState::Ready) {
            return false;
        }
        m_currentIdx = idx;
        m_procs[idx].state = Cr3ProcState::Running;
        m_active = true;
        mm_baremetal::BareMetalVadTable::setActiveTable(&m_procs[idx].vadTable);
        writeCr3(m_procs[idx].cr3Phys);
        writeGsBase(m_procs[idx].tebVa);
        return true;
    }

    /**
     * @brief Preemptive 100 Hz Local APIC Timer hook (Vector 0x30).
     */
    bool onClockInterrupt(
        TrapFrame64* frame,
        bool quantumExpired,
        uint32_t& outOldPid,
        uint32_t& outNewPid,
        uint64_t& outOldCr3,
        uint64_t& outNewCr3,
        uint8_t* fxBuf = nullptr
    ) noexcept {
        outOldPid = outNewPid = 0;
        outOldCr3 = outNewCr3 = 0;
        if (!m_active || !frame) return false;
        if ((frame->cs & 3ULL) != 3ULL) return false; // Never preempt Ring 0!

        m_totalClockTicksInR3++;
        auto& cur = m_procs[m_currentIdx];
        cur.cpuTicks++;

        if (!quantumExpired) return false;

        int nextIdx = findNextReadySlot(m_currentIdx);
        if (nextIdx < 0 || static_cast<size_t>(nextIdx) == m_currentIdx) {
            return false;
        }

        auto& nxt = m_procs[nextIdx];
        outOldPid = cur.pid;
        outNewPid = nxt.pid;
        outOldCr3 = cur.cr3Phys;
        outNewCr3 = nxt.cr3Phys;

        // Save preempted process full 160-byte hardware TrapFrame64 + 512-byte FXSAVE XMM state
        cur.trapFrame = *frame;
        if (fxBuf) {
            std::memcpy(cur.fxState, fxBuf, 512);
            cur.fxValid = true;
        }
        cur.state = Cr3ProcState::Ready;

        // Restore next process TrapFrame64, FXSAVE XMM state, VAD table, CR3 page tables & TEB GS_BASE
        m_currentIdx = static_cast<size_t>(nextIdx);
        nxt.state = Cr3ProcState::Running;
        nxt.preemptedInCount++;
        m_totalPreemptiveSwitches++;

        *frame = nxt.trapFrame;
        if (fxBuf && nxt.fxValid) {
            std::memcpy(fxBuf, nxt.fxState, 512);
        }
        mm_baremetal::BareMetalVadTable::setActiveTable(&nxt.vadTable);
        writeCr3(nxt.cr3Phys);
        writeGsBase(nxt.tebVa);
        return true;
    }

    /**
     * @brief Blocking wait on NtWaitForSingleObject (SSN 0x04): transitions caller Running -> Waiting
     *        until targetPid terminates, switching CR3 straight to target (or next Ready process).
     * @return true if caller was blocked and CR3 switched; false if target was already signaled.
     */
    bool onWaitForProcessSyscall(
        TrapFrame64* frame,
        uint32_t targetPid,
        uint32_t& outCallerPid,
        uint32_t& outSwitchedPid,
        uint64_t& outNextCr3
    ) noexcept {
        outCallerPid = outSwitchedPid = 0;
        outNextCr3 = 0;
        if (!m_active || !frame) return false;

        auto& cur = m_procs[m_currentIdx];
        outCallerPid = cur.pid;
        frame->rax = 0; // WAIT_OBJECT_0 upon wakeup or immediate completion

        int targetIdx = findSlotByPid(targetPid);
        if (targetIdx < 0 || m_procs[targetIdx].state == Cr3ProcState::Terminated ||
            static_cast<size_t>(targetIdx) == m_currentIdx) {
            return false; // Already signaled!
        }

        // Prefer scheduling the waited-on child directly if Ready, else next Ready slot
        int nextIdx = (m_procs[targetIdx].state == Cr3ProcState::Ready)
            ? targetIdx
            : findNextReadySlot(m_currentIdx);
        if (nextIdx < 0) {
            return false;
        }

        cur.trapFrame = *frame;
        cur.state = Cr3ProcState::Waiting;
        cur.waitTargetPid = targetPid;
        cur.waitCount++;
        m_totalBlockingWaits++;

        auto& nxt = m_procs[nextIdx];
        m_currentIdx = static_cast<size_t>(nextIdx);
        nxt.state = Cr3ProcState::Running;
        outSwitchedPid = nxt.pid;
        outNextCr3 = nxt.cr3Phys;

        *frame = nxt.trapFrame;
        mm_baremetal::BareMetalVadTable::setActiveTable(&nxt.vadTable);
        writeCr3(nxt.cr3Phys);
        writeGsBase(nxt.tebVa);
        return true;
    }

    /**
     * @brief Voluntary context switch on NtYieldExecution (SSN 0x46).
     */
    bool onYieldSyscall(TrapFrame64* frame, uint32_t& outOldPid, uint32_t& outNewPid) noexcept {
        outOldPid = outNewPid = 0;
        if (!m_active || !frame) return false;
        frame->rax = 0;

        auto& cur = m_procs[m_currentIdx];
        cur.yieldCount++;
        int nextIdx = findNextReadySlot(m_currentIdx);
        if (nextIdx < 0 || static_cast<size_t>(nextIdx) == m_currentIdx) {
            return false;
        }

        auto& nxt = m_procs[nextIdx];
        outOldPid = cur.pid;
        outNewPid = nxt.pid;

        cur.trapFrame = *frame;
        cur.state = Cr3ProcState::Ready;

        m_currentIdx = static_cast<size_t>(nextIdx);
        nxt.state = Cr3ProcState::Running;
        m_totalVoluntaryYields++;

        *frame = nxt.trapFrame;
        mm_baremetal::BareMetalVadTable::setActiveTable(&nxt.vadTable);
        writeCr3(nxt.cr3Phys);
        writeGsBase(nxt.tebVa);
        return true;
    }

    /**
     * @brief Handle NtTerminateProcess (SSN 0x01) in multi-process CR3 mode, waking any Waiting parents.
     */
    bool onTerminateSyscall(
        TrapFrame64* frame,
        uint32_t exitCode,
        uint32_t& outExitedPid,
        uint32_t& outNextPid,
        uint64_t& outNextCr3,
        uint32_t& outWokenPid
    ) noexcept {
        outExitedPid = outNextPid = outWokenPid = 0;
        outNextCr3 = 0;
        if (!m_active || !frame) return true;

        auto& cur = m_procs[m_currentIdx];
        outExitedPid = cur.pid;
        cur.exitCode = exitCode;
        cur.state = Cr3ProcState::Terminated;
        m_totalProcessesExited++;

        // Wake any parent process blocked in NtWaitForSingleObject(cur.pid)
        for (size_t i = 0; i < MAX_PROCS; ++i) {
            if (m_procs[i].inUse &&
                m_procs[i].state == Cr3ProcState::Waiting &&
                m_procs[i].waitTargetPid == cur.pid) {
                m_procs[i].state = Cr3ProcState::Ready;
                m_procs[i].waitTargetPid = 0;
                outWokenPid = m_procs[i].pid;
                m_totalWaitWakeups++;
            }
        }

        int nextIdx = findNextReadySlot(m_currentIdx);
        if (nextIdx >= 0) {
            auto& nxt = m_procs[nextIdx];
            m_currentIdx = static_cast<size_t>(nextIdx);
            nxt.state = Cr3ProcState::Running;
            outNextPid = nxt.pid;
            outNextCr3 = nxt.cr3Phys;

            *frame = nxt.trapFrame;
            mm_baremetal::BareMetalVadTable::setActiveTable(&nxt.vadTable);
            writeCr3(nxt.cr3Phys);
            writeGsBase(nxt.tebVa);
            return false; // Continue running surviving / woken process!
        }

        // All isolated Ring-3 processes exited cleanly -> restore kernel CR3 & VAD!
        m_active = false;
        mm_baremetal::BareMetalVadTable::setActiveTable(nullptr);
        if (m_kernelCr3Phys != 0) {
            writeCr3(m_kernelCr3Phys);
        }
        return true;
    }
};

} // namespace micant::km::ps_baremetal
