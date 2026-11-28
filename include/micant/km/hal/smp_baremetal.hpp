// ============================================================================
// MicaNT: Sovereign Freestanding Multi-Core x86_64 SMP Engine
// (smp_baremetal.hpp)
//
// Description:
//   Pure -ffreestanding -nostdlib ISO C++23 bare-metal Symmetric Multiprocessing
//   bring-up and multi-core Ring-3 thread dispatch engine compiled directly into
//   ntoskrnl.exe.
//
//   Implements authentic hardware multi-core execution on x86_64 silicon:
//     - Real ACPI 2.0 RSDP -> XSDT/RSDT -> MADT ("APIC") hardware processor parser
//     - Real 16-bit Real Mode -> 32-bit Protected Mode -> 64-bit Long Mode AP
//       bootstrap trampoline copied to physical 0x00008000 (SIPI Vector 0x08)
//     - Real Local APIC INIT - SIPI - SIPI hardware startup protocol (xAPIC + x2APIC)
//     - Per-CPU cache-line-aligned KPCR bound to hardware IA32_GS_BASE (0xC0000101)
//     - Per-CPU 64-bit GDT + TSS64 + SYSCALL MSRs (STAR/LSTAR/SFMASK) on EVERY AP
//     - True Multi-Core Ring-3 Win32 Thread Dispatch (CreateThread -> AP IRETQ CPL=3)
//     - Hardware Ticket Spinlock (atomic lock xadd) & multi-core contention benchmark
//     - Real Inter-Processor Interrupt (IPI Vector 0x3E) waking halted APs out of HLT
//
// Clean-Room ISO C++23. Zero mocks, zero third-party dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "micant/km/hal/cpu.hpp"
#include "micant/km/hal/idt.hpp"
#include "micant/km/executive/mm_baremetal.hpp"

namespace micant::km::smp_baremetal {

inline constexpr size_t   MAX_HW_CORES             = 16;
inline constexpr size_t   MAX_SMP_THREADS          = 16;
inline constexpr uint64_t AP_TRAMPOLINE_PHYS       = 0x00008000ULL;
inline constexpr uint8_t  AP_TRAMPOLINE_VECTOR     = 0x08;
inline constexpr uint64_t AP_MAILBOX_PHYS          = 0x00008200ULL;
inline constexpr uint8_t  VECTOR_SMP_IPI           = 0x3E;
inline constexpr uint32_t BENCH_ITERS_PER_CORE     = 25000;
inline constexpr uint64_t INVALID_THREAD_SLOT      = ~0ULL;
inline constexpr uint64_t SMP_THREAD_HANDLE_TAG    = 0x40000000ULL;
inline constexpr uint32_t SMP_STILL_ACTIVE         = 259u;

// ============================================================================
// 1. Hardware Ticket Spinlock (True Atomic x86_64 Serialization)
// ============================================================================

class TicketSpinLock {
private:
    alignas(64) volatile uint32_t m_nextTicket{0};
    alignas(64) volatile uint32_t m_nowServing{0};

public:
    void reset() noexcept {
        __atomic_store_n(&m_nextTicket, 0u, __ATOMIC_RELAXED);
        __atomic_store_n(&m_nowServing, 0u, __ATOMIC_RELAXED);
    }

    void lock() noexcept {
        uint32_t myTicket = __atomic_fetch_add(&m_nextTicket, 1u, __ATOMIC_RELAXED);
        while (__atomic_load_n(&m_nowServing, __ATOMIC_ACQUIRE) != myTicket) {
#if defined(__x86_64__)
            __asm__ __volatile__("pause" ::: "memory");
#endif
        }
    }

    void unlock() noexcept {
        uint32_t cur = __atomic_load_n(&m_nowServing, __ATOMIC_RELAXED);
        __atomic_store_n(&m_nowServing, cur + 1u, __ATOMIC_RELEASE);
    }
};

// ============================================================================
// 2. Per-CPU Kernel Processor Control Region (KPCR64, Cache-Line Aligned)
// ============================================================================

struct alignas(64) KpcrBareMetal64 {
    uint64_t self{0};                       // +0x00: GS:[0x00] self pointer
    uint64_t kernelStackTop{0};             // +0x08: GS:[0x08] Dedicated 16 KB Ring-0 stack top
    uint32_t logicalCoreIdx{0};             // +0x10: 0=BSP, 1..N-1=APs
    uint32_t hardwareApicId{0};             // +0x14: GS:[0x14] Hardware LAPIC ID read on this core
    uint64_t bootTsc{0};                    // +0x18: RDTSC captured on this core at Long Mode entry
    volatile uint32_t onlineFlag{0};        // +0x20: 1 once AP reached 64-bit Long Mode + KPCR bound
    volatile uint32_t verifiedGsApic{0};    // +0x24: Hardware APIC ID read back via GS:[0x14]
    volatile uint32_t benchItersDone{0};    // +0x28: Completed spinlock benchmark iterations
    volatile uint32_t ipiReceived{0};       // +0x2C: Live hardware IPIs (Vector 0x3E) handled on this core
    uint8_t  isBsp{0};                      // +0x30
    uint8_t  madtApicId{0};                 // +0x31
    volatile uint8_t r3Armed{0};            // +0x32: 1 once per-CPU GDT64 + TSS64 + SYSCALL MSRs armed
    uint8_t  pad0[5]{};                     // +0x33..+0x37
    uint64_t savedUserRsp{0};               // +0x38: GS:[0x38] per-CPU Ring-3 RSP saved on SWAPGS SYSCALL
    uint64_t savedReturnRip{0};             // +0x40: GS:[0x40] Ring-0 return RIP after AP thread exits
    uint64_t savedReturnRsp{0};             // +0x48: GS:[0x48] Ring-0 return RSP after AP thread exits
    uint64_t savedGprs[8]{};                // +0x50..+0x8F: rbx,rbp,rdi,rsi,r12,r13,r14,r15
    volatile uint32_t r3ThreadsExecuted{0}; // +0x90: Total Ring-3 Win32 threads executed on this core
    volatile uint32_t r3SyscallsHandled{0}; // +0x94: Total Ring-3 SYSCALLs handled via SWAPGS on this core
    volatile uint64_t activeThreadSlot{INVALID_THREAD_SLOT};  // +0x98
    volatile uint64_t pendingThreadSlot{INVALID_THREAD_SLOT}; // +0xA0
    // Per-CPU hardware GDT64 + TSS64 + GDTR64 (Intel SDM: each CPU core owns its own TSS descriptor!)
    alignas(16) micant::cpu::GdtTable64         gdt{};
    alignas(16) micant::cpu::TaskStateSegment64 tss{};
    micant::cpu::Gdtr64                         gdtr{};
};
static_assert(offsetof(KpcrBareMetal64, self) == 0x00);
static_assert(offsetof(KpcrBareMetal64, kernelStackTop) == 0x08);
static_assert(offsetof(KpcrBareMetal64, hardwareApicId) == 0x14);
static_assert(offsetof(KpcrBareMetal64, ipiReceived) == 0x2C);
static_assert(offsetof(KpcrBareMetal64, savedUserRsp) == 0x38);
static_assert(offsetof(KpcrBareMetal64, savedReturnRip) == 0x40);
static_assert(offsetof(KpcrBareMetal64, savedReturnRsp) == 0x48);
static_assert(offsetof(KpcrBareMetal64, savedGprs) == 0x50);

// Dedicated 16 KB stacks + 8 KB #DF IST1 stacks for up to 16 hardware cores
alignas(4096) static uint8_t s_apKernelStacks[MAX_HW_CORES][16384]{};
alignas(4096) static uint8_t s_apIst1Stacks[MAX_HW_CORES][8192]{};

// ============================================================================
// 2b. Hardware Multi-Core Ring-3 Worker Thread Descriptor
// ============================================================================

enum class SmpThreadState : uint32_t {
    Free       = 0,
    Dispatched = 1,
    Running    = 2,
    Terminated = 3
};

struct alignas(64) SmpThreadSlot {
    volatile uint32_t state{0};          // SmpThreadState
    uint32_t          ownerPid{0};
    uint32_t          threadId{0};
    uint32_t          targetCoreIdx{0};
    volatile uint32_t executedOnApicId{0};
    volatile uint32_t exitCode{SMP_STILL_ACTIVE};
    volatile uint32_t syscallsExecuted{0};
    uint32_t          pagesMapped{0};
    uint64_t          cr3Phys{0};
    uint64_t          startAddress{0};
    uint64_t          parameter{0};
    uint64_t          exitThunkAddress{0};
    uint64_t          userStackTop{0};
    uint64_t          tebVa{0};
};

// ============================================================================
// 3. Real ACPI 2.0 RSDP -> XSDT/RSDT -> MADT ("APIC") Structures
// ============================================================================

#pragma pack(push, 1)
struct AcpiRsdp20 {
    char     signature[8]; // "RSD PTR "
    uint8_t  checksum;
    char     oemId[6];
    uint8_t  revision;
    uint32_t rsdtAddress;
    uint32_t length;
    uint64_t xsdtAddress;
    uint8_t  extChecksum;
    uint8_t  reserved[3];
};

struct AcpiSdtHeader {
    uint32_t signature;
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oemId[6];
    char     oemTableId[8];
    uint32_t oemRevision;
    uint32_t creatorId;
    uint32_t creatorRevision;
};

struct ApBootstrapMailbox {
    uint64_t cr3Phys;          // 0x8200
    uint64_t entryRip;         // 0x8208 (&KiApSystemStartup64)
    uint64_t stackTop;         // 0x8210 (per-AP stack top)
    uint64_t logicalCoreIdx;   // 0x8218 (arg1 passed in RCX)
    uint64_t reserved0;        // 0x8220
    uint64_t reserved1;        // 0x8228
    uint16_t gdtrLimit;        // 0x8230
    uint32_t gdtrBase;         // 0x8232
    uint16_t gdtrPad[5];       // 0x8236..0x823F
    uint64_t gdtEntries[4];    // 0x8240..0x825F: 0x00 Null, 0x08 Code32, 0x10 Code64, 0x18 Data
    uint16_t idtrLimit;        // 0x8260
    uint64_t idtrBase;         // 0x8262
};
#pragma pack(pop)
static_assert(offsetof(ApBootstrapMailbox, cr3Phys) == 0x00);
static_assert(offsetof(ApBootstrapMailbox, entryRip) == 0x08);
static_assert(offsetof(ApBootstrapMailbox, stackTop) == 0x10);
static_assert(offsetof(ApBootstrapMailbox, logicalCoreIdx) == 0x18);
static_assert(offsetof(ApBootstrapMailbox, gdtrLimit) == 0x30);
static_assert(offsetof(ApBootstrapMailbox, gdtEntries) == 0x40);
static_assert(offsetof(ApBootstrapMailbox, idtrLimit) == 0x60);

// ============================================================================
// 4. Bare-Metal Multi-Core SMP Executive & Ring-3 Thread Dispatch Engine
// ============================================================================

extern "C" void KiApSystemStartup64(uint64_t logicalCoreIdx);
extern "C" void KiApIpiHandler();
extern "C" void KiApSystemCall64Entry();

class BareMetalSmpEngine {
private:
    KpcrBareMetal64 m_kpcrs[MAX_HW_CORES]{};
    SmpThreadSlot   m_threads[MAX_SMP_THREADS]{};
    TicketSpinLock  m_threadAllocLock{};
    uint32_t        m_nextThreadId{5001};
    uint32_t        m_rrNextApIdx{1};
    uint32_t        m_madtCoreCount{1};
    uint32_t        m_onlineCoreCount{1};
    uint32_t        m_bspApicId{0};
    uint64_t        m_lapicPhysBase{micant::hal::idt::LAPIC_DEFAULT_PHYSICAL_BASE};
    uint64_t        m_kernelCr3Phys{0};
    uint64_t        m_apSyscallEntryRip{0};
    bool            m_isX2Apic{false};
    bool            m_madtParsed{false};
    bool            m_trampolineInstalled{false};

    // Multi-core hardware Ticket Spinlock benchmark state
    TicketSpinLock    m_benchLock{};
    volatile uint32_t m_benchGo{0};
    volatile uint32_t m_benchDoneCount{0};
    volatile uint64_t m_sharedProtectedCounter{0};
    uint64_t          m_benchExpectedTotal{0};
    bool              m_benchPassed{false};

    // Global Ring-3 multi-core telemetry
    volatile uint32_t m_totalApR3ThreadsDispatched{0};
    volatile uint32_t m_totalApR3ThreadsCompleted{0};

    static inline uint64_t readTsc() noexcept {
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        uint32_t lo = 0, hi = 0;
        __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
        return (static_cast<uint64_t>(hi) << 32) | lo;
#else
        return 0;
#endif
    }

    static inline void spinDelayCycles(uint64_t cycles) noexcept {
        uint64_t start = readTsc();
        while ((readTsc() - start) < cycles) {
#if defined(__x86_64__)
            __asm__ __volatile__("pause" ::: "memory");
#endif
        }
    }

    void writeLapicReg(uint32_t regOff, uint32_t val) const noexcept {
        auto* lapic = reinterpret_cast<volatile uint32_t*>(m_lapicPhysBase);
        lapic[regOff / 4] = val;
        (void)lapic[micant::hal::idt::LAPIC_REG_ID / 4]; // Memory fence readback
    }

    uint32_t readLapicReg(uint32_t regOff) const noexcept {
        auto* lapic = reinterpret_cast<volatile uint32_t*>(m_lapicPhysBase);
        return lapic[regOff / 4];
    }

    void waitIcrIdle() const noexcept {
        for (uint32_t i = 0; i < 1000000; ++i) {
            if ((readLapicReg(micant::hal::idt::LAPIC_REG_ICR_LOW) & (1u << 12)) == 0) return;
#if defined(__x86_64__)
            __asm__ __volatile__("pause");
#endif
        }
    }

    static constexpr uint32_t X2APIC_MSR_ID  = 0x802u;
    static constexpr uint32_t X2APIC_MSR_TPR = 0x808u;
    static constexpr uint32_t X2APIC_MSR_EOI = 0x80Bu;
    static constexpr uint32_t X2APIC_MSR_SVR = 0x80Fu;
    static constexpr uint32_t X2APIC_MSR_ICR = 0x830u;

    void enableSelfLapic() const noexcept {
        if (m_isX2Apic) {
            uint64_t base = micant::cpu::cpuRdmsr(micant::cpu::MSR_IA32_APIC_BASE);
            micant::cpu::cpuWrmsr(micant::cpu::MSR_IA32_APIC_BASE, base | (1ULL << 11) | (1ULL << 10));
            micant::cpu::cpuWrmsr(X2APIC_MSR_SVR, 0x1FFu);
            micant::cpu::cpuWrmsr(X2APIC_MSR_TPR, 0x00u);
        } else {
            writeLapicReg(micant::hal::idt::LAPIC_REG_SVR, 0x1FFu);
            writeLapicReg(micant::hal::idt::LAPIC_REG_TPR, 0x00u);
        }
    }

    void sendSelfEoi() const noexcept {
        if (m_isX2Apic) {
            micant::cpu::cpuWrmsr(X2APIC_MSR_EOI, 0);
        } else {
            writeLapicReg(micant::hal::idt::LAPIC_REG_EOI, 0);
        }
    }

    void sendIpiRaw(uint32_t targetApicId, uint32_t icrLow) const noexcept {
        if (m_isX2Apic) {
            uint64_t icr = (static_cast<uint64_t>(targetApicId) << 32) | static_cast<uint64_t>(icrLow);
            micant::cpu::cpuWrmsr(X2APIC_MSR_ICR, icr);
            return;
        }
        waitIcrIdle();
        writeLapicReg(micant::hal::idt::LAPIC_REG_ICR_HIGH, targetApicId << 24);
        writeLapicReg(micant::hal::idt::LAPIC_REG_ICR_LOW, icrLow);
        waitIcrIdle();
    }

    /**
     * @brief Arm an Application Processor with its own private 64-bit GDT (Ring 0 + Ring 3),
     *        its own private TSS64 (RSP0 = AP kernel stack), and hardware SYSCALL/SYSRET MSRs!
     */
    void armApForRing3(KpcrBareMetal64& kpcr) noexcept {
        std::memset(&kpcr.tss, 0, sizeof(kpcr.tss));
        kpcr.tss.rsp0 = kpcr.kernelStackTop;
        size_t coreIdx = (kpcr.logicalCoreIdx < MAX_HW_CORES) ? kpcr.logicalCoreIdx : 0;
        kpcr.tss.ist1 = (reinterpret_cast<uint64_t>(&s_apIst1Stacks[coreIdx][0]) + sizeof(s_apIst1Stacks[0])) & ~0xFULL;
        kpcr.tss.ioMapBase = sizeof(micant::cpu::TaskStateSegment64);

        std::memset(&kpcr.gdt, 0, sizeof(kpcr.gdt));
        kpcr.gdt.kernelCode.limitLow = 0xFFFF;
        kpcr.gdt.kernelCode.access = 0x9A;
        kpcr.gdt.kernelCode.limitHighAndFlags = 0xAF;

        kpcr.gdt.kernelData.limitLow = 0xFFFF;
        kpcr.gdt.kernelData.access = 0x92;
        kpcr.gdt.kernelData.limitHighAndFlags = 0xCF;

        kpcr.gdt.userCmCode.limitLow = 0xFFFF;
        kpcr.gdt.userCmCode.access = 0xFA;
        kpcr.gdt.userCmCode.limitHighAndFlags = 0xCF;

        kpcr.gdt.userData.limitLow = 0xFFFF;
        kpcr.gdt.userData.access = 0xF2;
        kpcr.gdt.userData.limitHighAndFlags = 0xCF;

        kpcr.gdt.userCode.limitLow = 0xFFFF;
        kpcr.gdt.userCode.access = 0xFA;
        kpcr.gdt.userCode.limitHighAndFlags = 0xAF;

        uint64_t tssBase = reinterpret_cast<uint64_t>(&kpcr.tss);
        kpcr.gdt.tssDesc.limitLow = sizeof(micant::cpu::TaskStateSegment64) - 1;
        kpcr.gdt.tssDesc.baseLow = static_cast<uint16_t>(tssBase & 0xFFFF);
        kpcr.gdt.tssDesc.baseMiddle = static_cast<uint8_t>((tssBase >> 16) & 0xFF);
        kpcr.gdt.tssDesc.access = 0x89; // Available 64-bit TSS
        kpcr.gdt.tssDesc.limitHighAndFlags = 0x00;
        kpcr.gdt.tssDesc.baseHigh = static_cast<uint8_t>((tssBase >> 24) & 0xFF);
        kpcr.gdt.tssDesc.baseUpper = static_cast<uint32_t>((tssBase >> 32) & 0xFFFFFFFF);
        kpcr.gdt.tssDesc.reserved = 0;

        kpcr.gdtr.limit = sizeof(micant::cpu::GdtTable64) - 1;
        kpcr.gdtr.base = reinterpret_cast<uint64_t>(&kpcr.gdt);

#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        uint16_t tssSel = micant::cpu::SELECTOR_TSS64;
        __asm__ __volatile__(
            "lgdt %0\n\t"
            "pushq $0x10\n\t"
            "leaq 1f(%%rip), %%rax\n\t"
            "pushq %%rax\n\t"
            "lretq\n\t"
            "1:\n\t"
            "movw $0x18, %%ax\n\t"
            "movw %%ax, %%ds\n\t"
            "movw %%ax, %%es\n\t"
            "movw %%ax, %%ss\n\t"
            "xorw %%ax, %%ax\n\t"
            "movw %%ax, %%fs\n\t"
            "movw %%ax, %%gs\n\t"
            "ltr %1\n\t"
            :
            : "m"(kpcr.gdtr), "r"(tssSel)
            : "rax", "memory"
        );
#endif
        // Reloading GS resets IA32_GS_BASE to 0 -> re-bind private KPCR immediately!
        micant::cpu::cpuWrmsr(micant::cpu::MSR_GS_BASE, kpcr.self);
        micant::cpu::cpuWrmsr(micant::cpu::MSR_KERNEL_GS_BASE, 0);

        // Program per-AP SYSCALL/SYSRET hardware MSRs
        uint64_t efer = micant::cpu::cpuRdmsr(micant::cpu::MSR_EFER);
        micant::cpu::cpuWrmsr(micant::cpu::MSR_EFER, efer | micant::cpu::EFER_SCE | micant::cpu::EFER_NXE);
        uint64_t star = (static_cast<uint64_t>(micant::cpu::KGDT64_R3_CMCODE) << 48) |
                        (static_cast<uint64_t>(micant::cpu::KGDT64_R0_CODE) << 32);
        micant::cpu::cpuWrmsr(micant::cpu::MSR_STAR, star);
        micant::cpu::cpuWrmsr(micant::cpu::MSR_LSTAR, m_apSyscallEntryRip ? m_apSyscallEntryRip : reinterpret_cast<uint64_t>(&KiApSystemCall64Entry));
        micant::cpu::cpuWrmsr(micant::cpu::MSR_SFMASK, micant::cpu::SFMASK_DEFAULT);
        __atomic_store_n(&kpcr.r3Armed, static_cast<uint8_t>(1), __ATOMIC_RELEASE);
    }

    /**
     * @brief Transition THIS physical AP core into Ring 3 (CPL=3) inside the parent process's
     *        private CR3 address space to execute a live Win32 thread!
     *        Returns cleanly back to Ring 0 when the thread invokes NtTerminateThread (SSN 0x5B).
     */
    void executeRing3ThreadOnAp(KpcrBareMetal64& kpcr, uint64_t slotIdx) noexcept {
        if (slotIdx >= MAX_SMP_THREADS) return;
        auto& th = m_threads[slotIdx];
        __atomic_store_n(&th.executedOnApicId, kpcr.hardwareApicId, __ATOMIC_RELAXED);
        __atomic_store_n(&th.state, static_cast<uint32_t>(SmpThreadState::Running), __ATOMIC_RELEASE);
        __atomic_store_n(&kpcr.activeThreadSlot, slotIdx, __ATOMIC_RELEASE);

        uint64_t entryRip = th.exitThunkAddress ? th.exitThunkAddress : th.startAddress;
        uint64_t arg1Rcx  = th.parameter;
        uint64_t arg2Rdx  = th.startAddress;
        uint64_t userRsp  = th.userStackTop;
        uint64_t tebVa    = th.tebVa;
        uint64_t procCr3  = th.cr3Phys;
        uint64_t kpcrAddr = kpcr.self;

#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        // Load parent process's private hardware CR3 on this AP core!
        __asm__ __volatile__("mov %0, %%cr3" : : "r"(procCr3) : "memory");

        struct ApLaunchArgs {
            uint64_t kpcrAddr;   // +0x00
            uint64_t tebVa;      // +0x08
            uint64_t userRsp;    // +0x10
            uint64_t entryRip;   // +0x18
            uint64_t arg1Rcx;    // +0x20
            uint64_t arg2Rdx;    // +0x28
        } la{kpcrAddr, tebVa, userRsp, entryRip, arg1Rcx, arg2Rdx};

        register uint64_t r_la __asm__("r9") = reinterpret_cast<uint64_t>(&la);
        // Save callee-saved GPRs & return RIP/RSP into private kpcr, load Ring-3 segments,
        // bind KERNEL_GS_BASE=&kpcr + GS_BASE=tebVa, and IRETQ into Ring 3 (CPL=3)!
        __asm__ __volatile__(
            "movq 0x00(%%r9), %%r10\n\t"
            "movq %%rbx, 0x50(%%r10)\n\t"
            "movq %%rbp, 0x58(%%r10)\n\t"
            "movq %%rdi, 0x60(%%r10)\n\t"
            "movq %%rsi, 0x68(%%r10)\n\t"
            "movq %%r12, 0x70(%%r10)\n\t"
            "movq %%r13, 0x78(%%r10)\n\t"
            "movq %%r14, 0x80(%%r10)\n\t"
            "movq %%r15, 0x88(%%r10)\n\t"
            "leaq 1f(%%rip), %%rax\n\t"
            "movq %%rax, 0x40(%%r10)\n\t"
            "movq %%rsp, 0x48(%%r10)\n\t"
            "movw $0x002B, %%ax\n\t"
            "movw %%ax, %%ds\n\t"
            "movw %%ax, %%es\n\t"
            "movw %%ax, %%fs\n\t"
            "movw %%ax, %%gs\n\t"
            // MSR_KERNEL_GS_BASE (0xC0000102) = kpcrAddr (for SWAPGS upon SYSCALL!)
            "movl $0xC0000102, %%ecx\n\t"
            "movq %%r10, %%rax\n\t"
            "movq %%r10, %%rdx\n\t"
            "shrq $32, %%rdx\n\t"
            "wrmsr\n\t"
            // MSR_GS_BASE (0xC0000101) = tebVa (Ring-3 per-thread TEB!)
            "movq 0x08(%%r9), %%r11\n\t"
            "movl $0xC0000101, %%ecx\n\t"
            "movq %%r11, %%rax\n\t"
            "movq %%r11, %%rdx\n\t"
            "shrq $32, %%rdx\n\t"
            "wrmsr\n\t"
            // Build hardware IRETQ frame -> CPL=3 (CS=0x33, SS=0x2B, RFLAGS=0x0202 IF=1!)
            "pushq $0x002B\n\t"
            "pushq 0x10(%%r9)\n\t"
            "pushq $0x0202\n\t"
            "pushq $0x0033\n\t"
            "pushq 0x18(%%r9)\n\t"
            // Pass Win64 ABI args: RCX = lpParameter, RDX = lpStartAddress
            "movq 0x20(%%r9), %%rcx\n\t"
            "movq 0x28(%%r9), %%rdx\n\t"
            "xorq %%rax, %%rax\n\t"
            "iretq\n\t"
            "1:\n\t"
            : "+r"(r_la)
            :
            : "rax", "rcx", "rdx", "r8", "r10", "r11", "memory"
        );
#else
        (void)entryRip; (void)arg1Rcx; (void)arg2Rdx; (void)userRsp; (void)tebVa; (void)procCr3; (void)kpcrAddr;
#endif
    }

    bool parseMadt(uint64_t rsdpPhys) noexcept {
        if (rsdpPhys == 0) return false;
        const auto* rsdp = reinterpret_cast<const AcpiRsdp20*>(rsdpPhys);
        if (std::memcmp(rsdp->signature, "RSD PTR ", 8) != 0) return false;

        const AcpiSdtHeader* madt = nullptr;
        constexpr uint32_t SIG_APIC = 0x43495041u; // "APIC"

        if (rsdp->revision >= 2 && rsdp->xsdtAddress != 0) {
            const auto* xsdt = reinterpret_cast<const AcpiSdtHeader*>(rsdp->xsdtAddress);
            if (xsdt->length >= sizeof(AcpiSdtHeader)) {
                size_t entries = (xsdt->length - sizeof(AcpiSdtHeader)) / sizeof(uint64_t);
                const auto* ptrs = reinterpret_cast<const uint64_t*>(rsdp->xsdtAddress + sizeof(AcpiSdtHeader));
                for (size_t i = 0; i < entries && i < 64; ++i) {
                    if (ptrs[i] == 0) continue;
                    const auto* hdr = reinterpret_cast<const AcpiSdtHeader*>(ptrs[i]);
                    if (hdr->signature == SIG_APIC) { madt = hdr; break; }
                }
            }
        }
        if (!madt && rsdp->rsdtAddress != 0) {
            const auto* rsdt = reinterpret_cast<const AcpiSdtHeader*>(static_cast<uint64_t>(rsdp->rsdtAddress));
            if (rsdt->length >= sizeof(AcpiSdtHeader)) {
                size_t entries = (rsdt->length - sizeof(AcpiSdtHeader)) / sizeof(uint32_t);
                const auto* ptrs = reinterpret_cast<const uint32_t*>(static_cast<uint64_t>(rsdp->rsdtAddress) + sizeof(AcpiSdtHeader));
                for (size_t i = 0; i < entries && i < 64; ++i) {
                    if (ptrs[i] == 0) continue;
                    const auto* hdr = reinterpret_cast<const AcpiSdtHeader*>(static_cast<uint64_t>(ptrs[i]));
                    if (hdr->signature == SIG_APIC) { madt = hdr; break; }
                }
            }
        }
        if (!madt || madt->length < 44) return false;

        const auto* base = reinterpret_cast<const uint8_t*>(madt);
        uint32_t lapicAddr = 0;
        std::memcpy(&lapicAddr, base + 36, sizeof(uint32_t));
        if (lapicAddr != 0) m_lapicPhysBase = lapicAddr;

        uint32_t count = 0;
        size_t pos = 44;
        while (pos + 2 <= madt->length && count < MAX_HW_CORES) {
            uint8_t type = base[pos];
            uint8_t rlen = base[pos + 1];
            if (rlen < 2 || pos + rlen > madt->length) break;
            if (type == 0 && rlen >= 8) { // Processor Local APIC
                uint8_t apicId = base[pos + 3];
                uint32_t flags = 0;
                std::memcpy(&flags, base + pos + 4, sizeof(uint32_t));
                if ((flags & 0x03u) != 0) {
                    m_kpcrs[count] = KpcrBareMetal64{};
                    m_kpcrs[count].logicalCoreIdx = count;
                    m_kpcrs[count].madtApicId = apicId;
                    m_kpcrs[count].isBsp = (apicId == m_bspApicId) ? 1 : 0;
                    count++;
                }
            }
            pos += rlen;
        }
        if (count == 0) return false;

        for (uint32_t i = 1; i < count; ++i) {
            if (m_kpcrs[i].isBsp) {
                KpcrBareMetal64 tmp = m_kpcrs[0];
                m_kpcrs[0] = m_kpcrs[i];
                m_kpcrs[i] = tmp;
                m_kpcrs[0].logicalCoreIdx = 0;
                m_kpcrs[i].logicalCoreIdx = i;
                break;
            }
        }

        m_madtCoreCount = count;
        m_madtParsed = true;
        return true;
    }

    void installTrampolineCode(uint64_t bspCr3Phys) noexcept {
        micant::km::mm_baremetal::BareMetalPaging::mapVirtualPage(
            bspCr3Phys,
            AP_TRAMPOLINE_PHYS,
            AP_TRAMPOLINE_PHYS,
            micant::km::mm_baremetal::PTE_PRESENT | micant::km::mm_baremetal::PTE_WRITABLE
        );

        auto* page = reinterpret_cast<uint8_t*>(AP_TRAMPOLINE_PHYS);
        std::memset(page, 0x90, 0x200);
        std::memset(page + 0x200, 0x00, 0x200);

        // Stage A: 16-bit Real Mode Entry at 0x8000 (SIPI Vector 0x08 -> CS=0x0800, IP=0x0000)
        static const uint8_t kStage16[] = {
            0xFA,                                           // cli
            0xFC,                                           // cld
            0x31, 0xC0,                                     // xor ax, ax
            0x8E, 0xD8,                                     // mov ds, ax
            0x8E, 0xC0,                                     // mov es, ax
            0x8E, 0xD0,                                     // mov ss, ax
            0x66, 0x0F, 0x01, 0x16, 0x30, 0x82,             // lgdtd [0x8230]
            0x0F, 0x20, 0xC0,                               // mov eax, cr0
            0x66, 0x83, 0xC8, 0x01,                         // or eax, 1 (CR0.PE)
            0x0F, 0x22, 0xC0,                               // mov cr0, eax
            0x66, 0xEA, 0x40, 0x80, 0x00, 0x00, 0x08, 0x00  // ljmp 0x08:0x00008040
        };
        std::memcpy(page + 0x00, kStage16, sizeof(kStage16));

        // Stage B: 32-bit Protected Mode at 0x8040 (CS=0x08)
        static const uint8_t kStage32[] = {
            0x66, 0xB8, 0x18, 0x00,                         // mov ax, 0x18
            0x8E, 0xD8,                                     // mov ds, ax
            0x8E, 0xC0,                                     // mov es, ax
            0x8E, 0xD0,                                     // mov ss, ax
            0x0F, 0x20, 0xE0,                               // mov eax, cr4
            0x0D, 0x20, 0x06, 0x00, 0x00,                   // or eax, 0x620 (PAE | OSFXSR | OSXMMEXCPT)
            0x0F, 0x22, 0xE0,                               // mov cr4, eax
            0xA1, 0x00, 0x82, 0x00, 0x00,                   // mov eax, [0x00008200] (CR3)
            0x0F, 0x22, 0xD8,                               // mov cr3, eax
            0xB9, 0x80, 0x00, 0x00, 0xC0,                   // mov ecx, 0xC0000080 (MSR_EFER)
            0x0F, 0x32,                                     // rdmsr
            0x0D, 0x01, 0x09, 0x00, 0x00,                   // or eax, 0x901 (SCE | LME | NXE)
            0x0F, 0x30,                                     // wrmsr
            0x0F, 0x20, 0xC0,                               // mov eax, cr0
            0x0D, 0x02, 0x00, 0x00, 0x80,                   // or eax, 0x80000002 (PG | MP)
            0x25, 0xFB, 0xFF, 0xFF, 0xFF,                   // and eax, ~4 (clear CR0.EM)
            0x0F, 0x22, 0xC0,                               // mov cr0, eax
            0xEA, 0xA0, 0x80, 0x00, 0x00, 0x10, 0x00        // ljmp 0x10:0x000080A0
        };
        std::memcpy(page + 0x40, kStage32, sizeof(kStage32));

        // Stage C: 64-bit Long Mode at 0x80A0 (CS=0x10 == SELECTOR_KCODE64)
        static const uint8_t kStage64[] = {
            0x66, 0xB8, 0x18, 0x00,                         // mov ax, 0x18
            0x8E, 0xD8,                                     // mov ds, ax
            0x8E, 0xC0,                                     // mov es, ax
            0x8E, 0xD0,                                     // mov ss, ax
            0x66, 0x31, 0xC0,                               // xor ax, ax
            0x8E, 0xE0,                                     // mov fs, ax
            0x8E, 0xE8,                                     // mov gs, ax
            0x48, 0xB8, 0x10, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, 0x8210
            0x48, 0x8B, 0x20,                               // mov rsp, [rax]
            0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 32
            0x48, 0xB8, 0x60, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, 0x8260
            0x0F, 0x01, 0x18,                               // lidt [rax]
            0x48, 0xB8, 0x18, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, 0x8218
            0x48, 0x8B, 0x08,                               // mov rcx, [rax]
            0x48, 0xB8, 0x08, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, 0x8208
            0x48, 0x8B, 0x00,                               // mov rax, [rax]
            0xFF, 0xD0,                                     // call rax
            0xF4,                                           // hlt
            0xEB, 0xFD                                      // jmp -3
        };
        std::memcpy(page + 0xA0, kStage64, sizeof(kStage64));

        auto* mb = reinterpret_cast<ApBootstrapMailbox*>(AP_MAILBOX_PHYS);
        mb->cr3Phys = bspCr3Phys;
        mb->entryRip = reinterpret_cast<uint64_t>(&KiApSystemStartup64);
        mb->stackTop = 0;
        mb->logicalCoreIdx = 0;
        mb->gdtrLimit = 31;
        mb->gdtrBase = static_cast<uint32_t>(AP_MAILBOX_PHYS + offsetof(ApBootstrapMailbox, gdtEntries));
        mb->gdtEntries[0] = 0x0000000000000000ULL;
        mb->gdtEntries[1] = 0x00CF9A000000FFFFULL;
        mb->gdtEntries[2] = 0x00AF9A000000FFFFULL;
        mb->gdtEntries[3] = 0x00CF92000000FFFFULL;

        #pragma pack(push, 1)
        struct RawIdtr { uint16_t lim; uint64_t base; } idtr{};
        #pragma pack(pop)
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("sidt %0" : "=m"(idtr));
#endif
        mb->idtrLimit = idtr.lim;
        mb->idtrBase = idtr.base;
        m_trampolineInstalled = true;
    }

public:
    static BareMetalSmpEngine& get() noexcept {
        static BareMetalSmpEngine instance;
        return instance;
    }

    uint32_t readSelfApicId() const noexcept {
        if (m_isX2Apic) {
            return static_cast<uint32_t>(micant::cpu::cpuRdmsr(X2APIC_MSR_ID));
        }
        return (readLapicReg(micant::hal::idt::LAPIC_REG_ID) >> 24) & 0xFFu;
    }

    void setApSyscallEntry(uint64_t entryRip) noexcept {
        m_apSyscallEntryRip = entryRip;
    }

    uint32_t bringUpAllHardwareCores(uint64_t acpiRsdpPhys, uint64_t lapicPhysBase) noexcept {
        uint64_t apicBaseMsr = micant::cpu::cpuRdmsr(micant::cpu::MSR_IA32_APIC_BASE);
        m_isX2Apic = (apicBaseMsr & (1ULL << 10)) != 0;
        if (!m_isX2Apic && (apicBaseMsr & 0xFFFFF000ULL) != 0) {
            m_lapicPhysBase = apicBaseMsr & 0xFFFFF000ULL;
        } else {
            m_lapicPhysBase = lapicPhysBase ? lapicPhysBase : micant::hal::idt::LAPIC_DEFAULT_PHYSICAL_BASE;
        }
        m_bspApicId = readSelfApicId();
        m_madtCoreCount = 1;
        m_onlineCoreCount = 1;
        m_madtParsed = false;
        m_threadAllocLock.reset();
        for (size_t i = 0; i < MAX_SMP_THREADS; ++i) {
            m_threads[i] = SmpThreadSlot{};
        }

        if (!parseMadt(acpiRsdpPhys)) {
            m_kpcrs[0] = KpcrBareMetal64{};
            m_kpcrs[0].logicalCoreIdx = 0;
            m_kpcrs[0].madtApicId = static_cast<uint8_t>(m_bspApicId);
            m_kpcrs[0].isBsp = 1;
            m_madtCoreCount = 1;
        }

        auto& bsp = m_kpcrs[0];
        bsp.self = reinterpret_cast<uint64_t>(&bsp);
        bsp.kernelStackTop = reinterpret_cast<uint64_t>(&s_apKernelStacks[0][16384]);
        bsp.hardwareApicId = m_bspApicId;
        bsp.bootTsc = readTsc();
        micant::cpu::cpuWrmsr(micant::cpu::MSR_GS_BASE, bsp.self);
        uint32_t gsApic = 0;
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("movl %%gs:0x14, %0" : "=r"(gsApic));
#endif
        bsp.verifiedGsApic = gsApic;
        __atomic_store_n(&bsp.onlineFlag, 1u, __ATOMIC_RELEASE);

        uint64_t bspCr3 = 0;
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("mov %%cr3, %0" : "=r"(bspCr3));
#endif
        uint64_t bspCr3Phys = bspCr3 & micant::km::mm_baremetal::PTE_PHYS_MASK;
        m_kernelCr3Phys = bspCr3Phys;
        if (bspCr3Phys == 0 || bspCr3Phys >= 0x100000000ULL || m_madtCoreCount <= 1) {
            return m_onlineCoreCount;
        }

        m_benchLock.reset();
        __atomic_store_n(&m_benchGo, 0u, __ATOMIC_RELAXED);
        __atomic_store_n(&m_benchDoneCount, 0u, __ATOMIC_RELAXED);
        m_sharedProtectedCounter = 0;

        installTrampolineCode(bspCr3Phys);
        auto* mb = reinterpret_cast<ApBootstrapMailbox*>(AP_MAILBOX_PHYS);

        for (uint32_t i = 1; i < m_madtCoreCount; ++i) {
            auto& ap = m_kpcrs[i];
            uint32_t targetApic = ap.madtApicId;
            ap.self = reinterpret_cast<uint64_t>(&ap);
            ap.kernelStackTop = reinterpret_cast<uint64_t>(&s_apKernelStacks[i][16384]);
            __atomic_store_n(&ap.onlineFlag, 0u, __ATOMIC_RELAXED);
            __atomic_store_n(&ap.pendingThreadSlot, INVALID_THREAD_SLOT, __ATOMIC_RELAXED);
            __atomic_store_n(&ap.activeThreadSlot, INVALID_THREAD_SLOT, __ATOMIC_RELAXED);

            mb->stackTop = ap.kernelStackTop;
            mb->logicalCoreIdx = i;
            __atomic_thread_fence(__ATOMIC_SEQ_CST);

            if (m_isX2Apic) {
                micant::cpu::cpuWrmsr(0x828u, 0);
            } else {
                writeLapicReg(0x280, 0);
            }

            sendIpiRaw(targetApic, 0x00004500u);
            spinDelayCycles(15000000ULL);

            sendIpiRaw(targetApic, 0x00004600u | AP_TRAMPOLINE_VECTOR);
            spinDelayCycles(1000000ULL);

            if (__atomic_load_n(&ap.onlineFlag, __ATOMIC_ACQUIRE) == 0) {
                sendIpiRaw(targetApic, 0x00004600u | AP_TRAMPOLINE_VECTOR);
            }

            for (uint32_t spin = 0; spin < 500000; ++spin) {
                if (__atomic_load_n(&ap.onlineFlag, __ATOMIC_ACQUIRE) != 0) break;
#if defined(__x86_64__)
                __asm__ __volatile__("pause");
#endif
            }

            if (__atomic_load_n(&ap.onlineFlag, __ATOMIC_ACQUIRE) != 0) {
                m_onlineCoreCount++;
            }
        }

        m_benchExpectedTotal = static_cast<uint64_t>(m_onlineCoreCount) * BENCH_ITERS_PER_CORE;
        __atomic_store_n(&m_benchGo, 1u, __ATOMIC_RELEASE);

        for (uint32_t k = 0; k < BENCH_ITERS_PER_CORE; ++k) {
            m_benchLock.lock();
            m_sharedProtectedCounter = m_sharedProtectedCounter + 1;
            m_benchLock.unlock();
        }
        __atomic_store_n(&bsp.benchItersDone, BENCH_ITERS_PER_CORE, __ATOMIC_RELEASE);
        __atomic_fetch_add(&m_benchDoneCount, 1u, __ATOMIC_ACQ_REL);

        for (uint32_t wait = 0; wait < 20000000; ++wait) {
            if (__atomic_load_n(&m_benchDoneCount, __ATOMIC_ACQUIRE) >= m_onlineCoreCount) break;
#if defined(__x86_64__)
            __asm__ __volatile__("pause");
#endif
        }
        m_benchPassed = (m_sharedProtectedCounter == m_benchExpectedTotal) &&
                        (__atomic_load_n(&m_benchDoneCount, __ATOMIC_ACQUIRE) == m_onlineCoreCount);

        spinDelayCycles(5000000ULL);
        broadcastIpiToAps();
        return m_onlineCoreCount;
    }

    uint32_t broadcastIpiToAps() noexcept {
        uint32_t acked = 0;
        for (uint32_t i = 1; i < m_madtCoreCount; ++i) {
            auto& ap = m_kpcrs[i];
            if (__atomic_load_n(&ap.onlineFlag, __ATOMIC_ACQUIRE) == 0) continue;
            if (__atomic_load_n(&ap.activeThreadSlot, __ATOMIC_ACQUIRE) != INVALID_THREAD_SLOT) continue;
            uint32_t before = __atomic_load_n(&ap.ipiReceived, __ATOMIC_ACQUIRE);
            sendIpiRaw(ap.hardwareApicId, 0x00004000u | VECTOR_SMP_IPI);
            for (uint32_t spin = 0; spin < 2000000; ++spin) {
                if (__atomic_load_n(&ap.ipiReceived, __ATOMIC_ACQUIRE) > before) {
                    acked++;
                    break;
                }
#if defined(__x86_64__)
                __asm__ __volatile__("pause");
#endif
            }
        }
        return acked;
    }

    /**
     * @brief Allocate a Ring-3 stack + private TEB inside the parent process's private CR3,
     *        assign an idle physical AP core (1..N-1), and fire a real Vector 0x3E IPI to launch
     *        the thread into Ring 3 (CPL=3) in true parallel execution alongside BSP!
     */
    bool dispatchRing3ThreadToAp(
        uint32_t ownerPid,
        uint64_t procCr3Phys,
        uint64_t pebVa,
        uint64_t startAddress,
        uint64_t parameter,
        uint64_t exitThunkAddress,
        uint32_t preferredCoreIdx,
        uint64_t& outThreadHandle,
        uint32_t& outThreadId,
        uint32_t& outTargetApicId
    ) noexcept {
        outThreadHandle = 0;
        outThreadId = 0;
        outTargetApicId = 0;
        if (m_onlineCoreCount <= 1 || procCr3Phys == 0 || startAddress == 0) return false;

        m_threadAllocLock.lock();

        // 1. Choose an idle online AP core (preferring preferredCoreIdx if 1..m_madtCoreCount-1)
        uint32_t chosenCore = 0;
        if (preferredCoreIdx >= 1 && preferredCoreIdx < m_madtCoreCount) {
            auto& cand = m_kpcrs[preferredCoreIdx];
            if (__atomic_load_n(&cand.onlineFlag, __ATOMIC_ACQUIRE) != 0 &&
                __atomic_load_n(&cand.r3Armed, __ATOMIC_ACQUIRE) != 0 &&
                __atomic_load_n(&cand.activeThreadSlot, __ATOMIC_ACQUIRE) == INVALID_THREAD_SLOT &&
                __atomic_load_n(&cand.pendingThreadSlot, __ATOMIC_ACQUIRE) == INVALID_THREAD_SLOT) {
                chosenCore = preferredCoreIdx;
            }
        }
        if (chosenCore == 0) {
            for (uint32_t tries = 0; tries < m_madtCoreCount - 1; ++tries) {
                uint32_t idx = 1 + ((m_rrNextApIdx - 1 + tries) % (m_madtCoreCount - 1));
                auto& cand = m_kpcrs[idx];
                if (__atomic_load_n(&cand.onlineFlag, __ATOMIC_ACQUIRE) != 0 &&
                    __atomic_load_n(&cand.r3Armed, __ATOMIC_ACQUIRE) != 0 &&
                    __atomic_load_n(&cand.activeThreadSlot, __ATOMIC_ACQUIRE) == INVALID_THREAD_SLOT &&
                    __atomic_load_n(&cand.pendingThreadSlot, __ATOMIC_ACQUIRE) == INVALID_THREAD_SLOT) {
                    chosenCore = idx;
                    m_rrNextApIdx = 1 + (idx % (m_madtCoreCount - 1));
                    break;
                }
            }
        }
        if (chosenCore == 0) {
            m_threadAllocLock.unlock();
            return false;
        }

        // 2. Allocate an SmpThreadSlot (slot indices 1..MAX_SMP_THREADS-1 so stack/TEB offsets stay clean)
        size_t slotIdx = 0;
        for (size_t i = 1; i < MAX_SMP_THREADS; ++i) {
            if (__atomic_load_n(&m_threads[i].state, __ATOMIC_ACQUIRE) == static_cast<uint32_t>(SmpThreadState::Free)) {
                slotIdx = i;
                break;
            }
        }
        if (slotIdx == 0) {
            // Reclaim any already-terminated slot if all slots were used
            for (size_t i = 1; i < MAX_SMP_THREADS; ++i) {
                if (__atomic_load_n(&m_threads[i].state, __ATOMIC_ACQUIRE) == static_cast<uint32_t>(SmpThreadState::Terminated)) {
                    reclaimThreadPages(i);
                    slotIdx = i;
                    break;
                }
            }
        }
        if (slotIdx == 0) {
            m_threadAllocLock.unlock();
            return false;
        }

        // 3. Map a fresh 64 KB Ring-3 stack + 4 KB private TEB inside parent process's private CR3!
        //    Layout inside PML4[1] (completely disjoint from main thread stack 0x200000 & PEB/TEB0 0x300000):
        //      Stack: 0x00000080'00200000 + slotIdx * 0x10000 (64 KB = 16 pages)
        //      TEB:   0x00000080'00310000 + slotIdx * 0x1000  (4 KB = 1 page)
        uint64_t stackBaseVa = 0x00000080'00200000ULL + (static_cast<uint64_t>(slotIdx) * 0x10000ULL);
        uint64_t stackTopVa  = stackBaseVa + 0x10000ULL - 8ULL; // Win64 ABI: RSP % 16 == 8
        uint64_t tebVa       = 0x00000080'00310000ULL + (static_cast<uint64_t>(slotIdx) * 0x1000ULL);

        auto& pfn = micant::km::mm_baremetal::BareMetalPfnAllocator::get();
        uint64_t flags = micant::km::mm_baremetal::PTE_PRESENT |
                         micant::km::mm_baremetal::PTE_WRITABLE |
                         micant::km::mm_baremetal::PTE_USER;
        for (uint64_t p = 0; p < 16; ++p) {
            uint64_t phys = pfn.allocatePage();
            if (!phys) {
                m_threadAllocLock.unlock();
                return false;
            }
            micant::km::mm_baremetal::BareMetalPaging::mapVirtualPage(procCr3Phys, stackBaseVa + p * 4096ULL, phys, flags);
        }
        uint64_t tebPhys = pfn.allocatePage();
        if (!tebPhys) {
            m_threadAllocLock.unlock();
            return false;
        }
        micant::km::mm_baremetal::BareMetalPaging::mapVirtualPage(procCr3Phys, tebVa, tebPhys, flags);

        // Populate canonical Windows x64 TEB fields via physical address (identity-mapped in Ring 0!)
        uint32_t tid = m_nextThreadId++;
        auto* tebMem = reinterpret_cast<uint8_t*>(tebPhys);
        std::memset(tebMem, 0, 4096);
        *reinterpret_cast<uint64_t*>(tebMem + 0x30) = tebVa;     // NtTib.Self
        *reinterpret_cast<uint32_t*>(tebMem + 0x40) = ownerPid;  // ClientId.UniqueProcess
        *reinterpret_cast<uint32_t*>(tebMem + 0x48) = tid;       // ClientId.UniqueThread
        *reinterpret_cast<uint64_t*>(tebMem + 0x60) = pebVa;     // ProcessEnvironmentBlock

        auto& th = m_threads[slotIdx];
        th.ownerPid = ownerPid;
        th.threadId = tid;
        th.targetCoreIdx = chosenCore;
        th.executedOnApicId = 0xFFFFFFFFu;
        th.exitCode = SMP_STILL_ACTIVE;
        th.syscallsExecuted = 0;
        th.pagesMapped = 1;
        th.cr3Phys = procCr3Phys;
        th.startAddress = startAddress;
        th.parameter = parameter;
        th.exitThunkAddress = exitThunkAddress;
        th.userStackTop = stackTopVa;
        th.tebVa = tebVa;
        __atomic_store_n(&th.state, static_cast<uint32_t>(SmpThreadState::Dispatched), __ATOMIC_RELEASE);

        auto& targetAp = m_kpcrs[chosenCore];
        __atomic_store_n(&targetAp.pendingThreadSlot, static_cast<uint64_t>(slotIdx), __ATOMIC_RELEASE);
        __atomic_fetch_add(&m_totalApR3ThreadsDispatched, 1u, __ATOMIC_ACQ_REL);

        outThreadHandle = SMP_THREAD_HANDLE_TAG | static_cast<uint64_t>(slotIdx);
        outThreadId = tid;
        outTargetApicId = targetAp.hardwareApicId;

        // Fire real hardware Vector 0x3E IPI to wake targetAp out of HLT into Ring 3!
        sendIpiRaw(targetAp.hardwareApicId, 0x00004000u | VECTOR_SMP_IPI);

        m_threadAllocLock.unlock();
        return true;
    }

    bool waitForThreadHandle(uint64_t hThread) noexcept {
        if ((hThread & SMP_THREAD_HANDLE_TAG) == 0) return false;
        size_t slotIdx = static_cast<size_t>(hThread & ~SMP_THREAD_HANDLE_TAG);
        if (slotIdx == 0 || slotIdx >= MAX_SMP_THREADS) return false;
        auto& th = m_threads[slotIdx];
        for (uint64_t spin = 0; spin < 500000000ULL; ++spin) {
            uint32_t st = __atomic_load_n(&th.state, __ATOMIC_ACQUIRE);
            if (st == static_cast<uint32_t>(SmpThreadState::Terminated) ||
                st == static_cast<uint32_t>(SmpThreadState::Free)) {
                return true;
            }
#if defined(__x86_64__)
            __asm__ __volatile__("pause" ::: "memory");
#endif
        }
        return false;
    }

    void reclaimThreadPages(size_t slotIdx) noexcept {
        if (slotIdx == 0 || slotIdx >= MAX_SMP_THREADS) return;
        auto& th = m_threads[slotIdx];
        if (!th.pagesMapped || th.cr3Phys == 0) return;
        uint64_t stackBaseVa = 0x00000080'00200000ULL + (static_cast<uint64_t>(slotIdx) * 0x10000ULL);
        uint64_t tebVa       = 0x00000080'00310000ULL + (static_cast<uint64_t>(slotIdx) * 0x1000ULL);
        for (uint64_t p = 0; p < 16; ++p) {
            micant::km::mm_baremetal::BareMetalPaging::unmapVirtualPage(th.cr3Phys, stackBaseVa + p * 4096ULL, true);
        }
        micant::km::mm_baremetal::BareMetalPaging::unmapVirtualPage(th.cr3Phys, tebVa, true);
        th.pagesMapped = 0;
        th.cr3Phys = 0;
    }

    void waitAllThreadsForProcess(uint32_t ownerPid) noexcept {
        for (size_t i = 1; i < MAX_SMP_THREADS; ++i) {
            if (m_threads[i].ownerPid == ownerPid) {
                uint32_t st = __atomic_load_n(&m_threads[i].state, __ATOMIC_ACQUIRE);
                if (st == static_cast<uint32_t>(SmpThreadState::Dispatched) ||
                    st == static_cast<uint32_t>(SmpThreadState::Running)) {
                    waitForThreadHandle(SMP_THREAD_HANDLE_TAG | i);
                }
                // Parent CR3 destroyProcessPml4() reclaims all PML4[1] pages right after this call
                m_threads[i].pagesMapped = 0;
                m_threads[i].cr3Phys = 0;
                __atomic_store_n(&m_threads[i].state, static_cast<uint32_t>(SmpThreadState::Free), __ATOMIC_RELEASE);
                m_threads[i].ownerPid = 0;
            }
        }
    }

    bool queryThreadExitCode(uint64_t hThread, uint32_t& outExitCode) noexcept {
        outExitCode = SMP_STILL_ACTIVE;
        if ((hThread & 0xFF000000ULL) != SMP_THREAD_HANDLE_TAG) return false;
        size_t slotIdx = static_cast<size_t>(hThread & ~SMP_THREAD_HANDLE_TAG);
        if (slotIdx == 0 || slotIdx >= MAX_SMP_THREADS) return false;
        auto& th = m_threads[slotIdx];
        uint32_t st = __atomic_load_n(&th.state, __ATOMIC_ACQUIRE);
        if (st == static_cast<uint32_t>(SmpThreadState::Free)) return false;
        outExitCode = (st == static_cast<uint32_t>(SmpThreadState::Terminated))
            ? __atomic_load_n(&th.exitCode, __ATOMIC_ACQUIRE)
            : SMP_STILL_ACTIVE;
        return true;
    }

    bool closeThreadHandle(uint64_t hThread) noexcept {
        if ((hThread & 0xFF000000ULL) != SMP_THREAD_HANDLE_TAG) return false;
        size_t slotIdx = static_cast<size_t>(hThread & ~SMP_THREAD_HANDLE_TAG);
        if (slotIdx == 0 || slotIdx >= MAX_SMP_THREADS) return false;
        auto& th = m_threads[slotIdx];
        if (__atomic_load_n(&th.state, __ATOMIC_ACQUIRE) == static_cast<uint32_t>(SmpThreadState::Terminated)) {
            reclaimThreadPages(slotIdx);
            th.ownerPid = 0;
            __atomic_store_n(&th.state, static_cast<uint32_t>(SmpThreadState::Free), __ATOMIC_RELEASE);
        }
        return true;
    }

    /**
     * @brief Complete an AP Ring-3 thread cleanly on NtTerminateThread (SSN 0x5B).
     *        Restores kernel CR3 BEFORE publishing Terminated state so parent can safely tear down CR3!
     */
    void completeApRing3Thread(KpcrBareMetal64* kpcr, uint32_t exitCode) noexcept {
        if (!kpcr) return;
        uint64_t slotIdx = __atomic_load_n(&kpcr->activeThreadSlot, __ATOMIC_ACQUIRE);
        // Switch back to kernel CR3 FIRST while still in Ring 0!
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        if (m_kernelCr3Phys != 0) {
            __asm__ __volatile__("mov %0, %%cr3" : : "r"(m_kernelCr3Phys) : "memory");
        }
#endif
        micant::cpu::cpuWrmsr(micant::cpu::MSR_KERNEL_GS_BASE, 0);
        __atomic_store_n(&kpcr->activeThreadSlot, INVALID_THREAD_SLOT, __ATOMIC_RELEASE);
        __atomic_fetch_add(&kpcr->r3ThreadsExecuted, 1u, __ATOMIC_ACQ_REL);
        __atomic_fetch_add(&m_totalApR3ThreadsCompleted, 1u, __ATOMIC_ACQ_REL);

        if (slotIdx > 0 && slotIdx < MAX_SMP_THREADS) {
            auto& th = m_threads[slotIdx];
            __atomic_store_n(&th.exitCode, exitCode, __ATOMIC_RELEASE);
            __atomic_store_n(&th.state, static_cast<uint32_t>(SmpThreadState::Terminated), __ATOMIC_RELEASE);
        }
    }

    /**
     * @brief Invoked directly by each newly awakened Application Processor in 64-bit Long Mode!
     */
    void onApEntry64(uint64_t logicalCoreIdx) noexcept {
        if (logicalCoreIdx == 0 || logicalCoreIdx >= MAX_HW_CORES) {
            while (true) { __asm__ __volatile__("cli; hlt"); }
        }
        auto& kpcr = m_kpcrs[logicalCoreIdx];

        // 1. Enable this AP's own Local APIC (SVR = 0x1FF, TPR = 0x00)
        enableSelfLapic();

        // 2. Read this AP's OWN physical Local APIC ID and TSC
        uint32_t hwApic = readSelfApicId();
        kpcr.hardwareApicId = hwApic;
        kpcr.bootTsc = readTsc();

        // 3. Arm private per-AP GDT64 (Ring 0 + Ring 3), TSS64, SYSCALL MSRs, and bind IA32_GS_BASE
        armApForRing3(kpcr);
        uint32_t gsReadBack = 0;
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("movl %%gs:0x14, %0" : "=r"(gsReadBack));
#endif
        kpcr.verifiedGsApic = gsReadBack;

        // 4. Signal BSP that this physical core is ONLINE & Ring-3 ARMED in 64-bit Long Mode!
        __atomic_store_n(&kpcr.onlineFlag, 1u, __ATOMIC_RELEASE);

        // 5. Wait at multi-core barrier until BSP starts the concurrent Ticket Spinlock test
        while (__atomic_load_n(&m_benchGo, __ATOMIC_ACQUIRE) == 0) {
#if defined(__x86_64__)
            __asm__ __volatile__("pause");
#endif
        }

        // 6. Race all other physical cores on the hardware Ticket Spinlock!
        for (uint32_t k = 0; k < BENCH_ITERS_PER_CORE; ++k) {
            m_benchLock.lock();
            m_sharedProtectedCounter = m_sharedProtectedCounter + 1;
            m_benchLock.unlock();
        }
        __atomic_store_n(&kpcr.benchItersDone, BENCH_ITERS_PER_CORE, __ATOMIC_RELEASE);
        __atomic_fetch_add(&m_benchDoneCount, 1u, __ATOMIC_ACQ_REL);

        // 7. Zero-lost-wakeup dual-purpose loop:
        //    Services both live hardware IPIs (Vector 0x3E) AND Ring-3 Win32 worker threads!
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        while (true) {
            __asm__ __volatile__("cli" ::: "memory");
            uint64_t pendingSlot = __atomic_load_n(&kpcr.pendingThreadSlot, __ATOMIC_ACQUIRE);
            if (pendingSlot != INVALID_THREAD_SLOT) {
                __atomic_store_n(&kpcr.pendingThreadSlot, INVALID_THREAD_SLOT, __ATOMIC_RELAXED);
                executeRing3ThreadOnAp(kpcr, pendingSlot);
                continue;
            }
            __asm__ __volatile__("sti; hlt" ::: "memory");
        }
#endif
    }

    /**
     * @brief Invoked by KiApIpiStub when Vector 0x3E hardware IPI arrives on an AP core.
     */
    void onApIpiInterrupt() noexcept {
        uint64_t kpcrPtr = 0;
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("movq %%gs:0x00, %0" : "=r"(kpcrPtr));
#endif
        if (kpcrPtr != 0) {
            auto* kpcr = reinterpret_cast<KpcrBareMetal64*>(kpcrPtr);
            __atomic_fetch_add(&kpcr->ipiReceived, 1u, __ATOMIC_ACQ_REL);
        }
        sendSelfEoi();
    }

    // Telemetry Accessors
    [[nodiscard]] uint32_t getMadtCoreCount() const noexcept { return m_madtCoreCount; }
    [[nodiscard]] uint32_t getOnlineCoreCount() const noexcept { return m_onlineCoreCount; }
    [[nodiscard]] uint32_t getBspApicId() const noexcept { return m_bspApicId; }
    [[nodiscard]] uint64_t getLapicPhysBase() const noexcept { return m_lapicPhysBase; }
    [[nodiscard]] uint64_t getKernelCr3Phys() const noexcept { return m_kernelCr3Phys; }
    [[nodiscard]] bool     isX2Apic() const noexcept { return m_isX2Apic; }
    [[nodiscard]] bool     isMadtParsed() const noexcept { return m_madtParsed; }
    [[nodiscard]] bool     isBenchPassed() const noexcept { return m_benchPassed; }
    [[nodiscard]] uint64_t getSharedProtectedCounter() const noexcept { return m_sharedProtectedCounter; }
    [[nodiscard]] uint64_t getBenchExpectedTotal() const noexcept { return m_benchExpectedTotal; }
    [[nodiscard]] uint32_t getTotalApR3ThreadsDispatched() const noexcept { return m_totalApR3ThreadsDispatched; }
    [[nodiscard]] uint32_t getTotalApR3ThreadsCompleted() const noexcept { return m_totalApR3ThreadsCompleted; }
    [[nodiscard]] const KpcrBareMetal64* getKpcr(uint32_t idx) const noexcept {
        return (idx < m_madtCoreCount) ? &m_kpcrs[idx] : nullptr;
    }
    [[nodiscard]] SmpThreadSlot* getThreadSlot(size_t slotIdx) noexcept {
        return (slotIdx > 0 && slotIdx < MAX_SMP_THREADS) ? &m_threads[slotIdx] : nullptr;
    }
};

extern "C" inline __attribute__((used)) void KiApSystemStartup64(uint64_t logicalCoreIdx) {
    BareMetalSmpEngine::get().onApEntry64(logicalCoreIdx);
}

} // namespace micant::km::smp_baremetal
