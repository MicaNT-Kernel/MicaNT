// ============================================================================
// MicaNT: Sovereign Freestanding Multi-Core x86_64 SMP Engine
// (smp_baremetal.hpp)
//
// Description:
//   Pure -ffreestanding -nostdlib ISO C++23 bare-metal Symmetric Multiprocessing
//   bring-up engine compiled directly into ntoskrnl.exe.
//
//   Implements authentic hardware multi-core initialization on x86_64 silicon:
//     - Real ACPI 2.0 RSDP -> XSDT/RSDT -> MADT ("APIC") hardware processor parser
//     - Real 16-bit Real Mode -> 32-bit Protected Mode -> 64-bit Long Mode AP
//       bootstrap trampoline copied to physical 0x00008000 (SIPI Vector 0x08)
//     - Real Local APIC INIT - SIPI - SIPI hardware startup protocol
//     - Per-CPU cache-line-aligned KPCR bound to hardware IA32_GS_BASE (0xC0000101)
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
inline constexpr uint64_t AP_TRAMPOLINE_PHYS       = 0x00008000ULL;
inline constexpr uint8_t  AP_TRAMPOLINE_VECTOR     = 0x08;
inline constexpr uint64_t AP_MAILBOX_PHYS          = 0x00008200ULL;
inline constexpr uint8_t  VECTOR_SMP_IPI           = 0x3E;
inline constexpr uint32_t BENCH_ITERS_PER_CORE     = 25000;

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
    uint64_t self{0};                   // +0x00: GS:[0x00] self pointer
    uint64_t kernelStackTop{0};         // +0x08: Dedicated 16 KB Ring-0 stack top
    uint32_t logicalCoreIdx{0};         // +0x10: 0=BSP, 1..N-1=APs
    uint32_t hardwareApicId{0};         // +0x14: Hardware LAPIC ID read on this core
    uint64_t bootTsc{0};                // +0x18: RDTSC captured on this core at Long Mode entry
    volatile uint32_t onlineFlag{0};    // +0x20: 1 once AP reached 64-bit Long Mode + KPCR bound
    volatile uint32_t verifiedGsApic{0};// +0x24: Hardware APIC ID read back via GS:[0x14]
    volatile uint32_t benchItersDone{0};// +0x28: Completed spinlock benchmark iterations
    volatile uint32_t ipiReceived{0};   // +0x2C: Live hardware IPIs (Vector 0x3E) handled on this core
    uint8_t  isBsp{0};
    uint8_t  madtApicId{0};
    uint8_t  reserved[14]{};
};
static_assert(offsetof(KpcrBareMetal64, self) == 0x00);
static_assert(offsetof(KpcrBareMetal64, hardwareApicId) == 0x14);
static_assert(offsetof(KpcrBareMetal64, ipiReceived) == 0x2C);

// Dedicated 16 KB stacks for up to 16 hardware cores (256 KB total in kernel BSS)
alignas(4096) static uint8_t s_apKernelStacks[MAX_HW_CORES][16384]{};

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
// 4. Bare-Metal Multi-Core SMP Executive Engine
// ============================================================================

extern "C" void KiApSystemStartup64(uint64_t logicalCoreIdx);
extern "C" void KiApIpiHandler();

class BareMetalSmpEngine {
private:
    KpcrBareMetal64 m_kpcrs[MAX_HW_CORES]{};
    uint32_t        m_madtCoreCount{1};
    uint32_t        m_onlineCoreCount{1};
    uint32_t        m_bspApicId{0};
    uint64_t        m_lapicPhysBase{micant::hal::idt::LAPIC_DEFAULT_PHYSICAL_BASE};
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

    uint32_t readSelfApicId() const noexcept {
        if (m_isX2Apic) {
            return static_cast<uint32_t>(micant::cpu::cpuRdmsr(X2APIC_MSR_ID));
        }
        return (readLapicReg(micant::hal::idt::LAPIC_REG_ID) >> 24) & 0xFFu;
    }

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
                if ((flags & 0x03u) != 0) { // Enabled or Online Capable
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

        // Ensure BSP is always logical index 0 for clean indexing
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
        // Ensure physical page 0x8000 is identity-mapped Present + Writable + Executable (NX=0)
        micant::km::mm_baremetal::BareMetalPaging::mapVirtualPage(
            bspCr3Phys,
            AP_TRAMPOLINE_PHYS,
            AP_TRAMPOLINE_PHYS,
            micant::km::mm_baremetal::PTE_PRESENT | micant::km::mm_baremetal::PTE_WRITABLE
        );

        auto* page = reinterpret_cast<uint8_t*>(AP_TRAMPOLINE_PHYS);
        std::memset(page, 0x90, 0x200); // NOP fill code region
        std::memset(page + 0x200, 0x00, 0x200);

        // Stage A: 16-bit Real Mode Entry at 0x8000 (SIPI Vector 0x08 -> CS=0x0800, IP=0x0000)
        static const uint8_t kStage16[] = {
            0xFA,                                           // cli
            0xFC,                                           // cld
            0x31, 0xC0,                                     // xor ax, ax
            0x8E, 0xD8,                                     // mov ds, ax
            0x8E, 0xC0,                                     // mov es, ax
            0x8E, 0xD0,                                     // mov ss, ax
            0x66, 0x0F, 0x01, 0x16, 0x30, 0x82,             // lgdtd [0x8230] (full 32-bit GDT base)
            0x0F, 0x20, 0xC0,                               // mov eax, cr0
            0x66, 0x83, 0xC8, 0x01,                         // or eax, 1 (CR0.PE)
            0x0F, 0x22, 0xC0,                               // mov cr0, eax
            0x66, 0xEA, 0x40, 0x80, 0x00, 0x00, 0x08, 0x00  // ljmp 0x08:0x00008040
        };
        std::memcpy(page + 0x00, kStage16, sizeof(kStage16));

        // Stage B: 32-bit Protected Mode at 0x8040 (CS=0x08)
        static const uint8_t kStage32[] = {
            0x66, 0xB8, 0x18, 0x00,                         // mov ax, 0x18 (Flat Data)
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
            0x25, 0xFB, 0xFF, 0xFF, 0xFF,                   // and eax, ~4 (clear CR0.EM for SSE)
            0x0F, 0x22, 0xC0,                               // mov cr0, eax
            0xEA, 0xA0, 0x80, 0x00, 0x00, 0x10, 0x00        // ljmp 0x10:0x000080A0 (64-bit Long Mode!)
        };
        std::memcpy(page + 0x40, kStage32, sizeof(kStage32));

        // Stage C: 64-bit Long Mode at 0x80A0 (CS=0x10 == SELECTOR_KCODE64)
        static const uint8_t kStage64[] = {
            0x66, 0xB8, 0x18, 0x00,                         // mov ax, 0x18 (SELECTOR_KDATA64)
            0x8E, 0xD8,                                     // mov ds, ax
            0x8E, 0xC0,                                     // mov es, ax
            0x8E, 0xD0,                                     // mov ss, ax
            0x66, 0x31, 0xC0,                               // xor ax, ax
            0x8E, 0xE0,                                     // mov fs, ax
            0x8E, 0xE8,                                     // mov gs, ax
            0x48, 0xB8, 0x10, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, 0x8210
            0x48, 0x8B, 0x20,                               // mov rsp, [rax]
            0x48, 0x83, 0xEC, 0x20,                         // sub rsp, 32 (Win64 shadow + 16B align)
            0x48, 0xB8, 0x60, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, 0x8260
            0x0F, 0x01, 0x18,                               // lidt [rax]
            0x48, 0xB8, 0x18, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, 0x8218
            0x48, 0x8B, 0x08,                               // mov rcx, [rax] (logicalCoreIdx)
            0x48, 0xB8, 0x08, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, 0x8208
            0x48, 0x8B, 0x00,                               // mov rax, [rax] (&KiApSystemStartup64)
            0xFF, 0xD0,                                     // call rax
            0xF4,                                           // hlt
            0xEB, 0xFD                                      // jmp -3
        };
        std::memcpy(page + 0xA0, kStage64, sizeof(kStage64));

        // Populate Bootstrap Mailbox & 4-entry GDT at 0x8200
        auto* mb = reinterpret_cast<ApBootstrapMailbox*>(AP_MAILBOX_PHYS);
        mb->cr3Phys = bspCr3Phys;
        mb->entryRip = reinterpret_cast<uint64_t>(&KiApSystemStartup64);
        mb->stackTop = 0;
        mb->logicalCoreIdx = 0;
        mb->gdtrLimit = 31;
        mb->gdtrBase = static_cast<uint32_t>(AP_MAILBOX_PHYS + offsetof(ApBootstrapMailbox, gdtEntries));
        mb->gdtEntries[0] = 0x0000000000000000ULL; // 0x00: Null
        mb->gdtEntries[1] = 0x00CF9A000000FFFFULL; // 0x08: 32-bit Flat Code
        mb->gdtEntries[2] = 0x00AF9A000000FFFFULL; // 0x10: 64-bit Long Mode Code (SELECTOR_KCODE64)
        mb->gdtEntries[3] = 0x00CF92000000FFFFULL; // 0x18: Flat Read/Write Data (SELECTOR_KDATA64)

        // Capture live BSP IDTR so every AP loads the identical 64-bit IDT
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

    /**
     * @brief Discover real hardware APICs from ACPI MADT, install 16->32->64-bit trampoline
     *        at 0x8000, wake all Application Processors via hardware INIT-SIPI-SIPI,
     *        and execute a live multi-core Ticket Spinlock + Hardware IPI verification!
     */
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

        if (!parseMadt(acpiRsdpPhys)) {
            m_kpcrs[0] = KpcrBareMetal64{};
            m_kpcrs[0].logicalCoreIdx = 0;
            m_kpcrs[0].madtApicId = static_cast<uint8_t>(m_bspApicId);
            m_kpcrs[0].isBsp = 1;
            m_madtCoreCount = 1;
        }

        // Bind BSP's own KPCR to IA32_GS_BASE and verify GS:[0x14]
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
        if (bspCr3Phys == 0 || bspCr3Phys >= 0x100000000ULL || m_madtCoreCount <= 1) {
            return m_onlineCoreCount;
        }

        m_benchLock.reset();
        __atomic_store_n(&m_benchGo, 0u, __ATOMIC_RELAXED);
        __atomic_store_n(&m_benchDoneCount, 0u, __ATOMIC_RELAXED);
        m_sharedProtectedCounter = 0;

        installTrampolineCode(bspCr3Phys);
        auto* mb = reinterpret_cast<ApBootstrapMailbox*>(AP_MAILBOX_PHYS);

        // Wake each Application Processor sequentially via Intel SDM INIT - SIPI - SIPI
        for (uint32_t i = 1; i < m_madtCoreCount; ++i) {
            auto& ap = m_kpcrs[i];
            uint32_t targetApic = ap.madtApicId;
            ap.self = reinterpret_cast<uint64_t>(&ap);
            ap.kernelStackTop = reinterpret_cast<uint64_t>(&s_apKernelStacks[i][16384]);
            __atomic_store_n(&ap.onlineFlag, 0u, __ATOMIC_RELAXED);

            mb->stackTop = ap.kernelStackTop;
            mb->logicalCoreIdx = i;
            __atomic_thread_fence(__ATOMIC_SEQ_CST);

            // Clear Error Status Register
            if (m_isX2Apic) {
                micant::cpu::cpuWrmsr(0x828u, 0);
            } else {
                writeLapicReg(0x280, 0);
            }

            // 1. INIT IPI (Assert, Edge)
            sendIpiRaw(targetApic, 0x00004500u);
            spinDelayCycles(15000000ULL); // ~5-10ms

            // 2. Startup IPI #1 -> Vector 0x08 (0x00008000)
            sendIpiRaw(targetApic, 0x00004600u | AP_TRAMPOLINE_VECTOR);
            spinDelayCycles(1000000ULL);

            // 3. Startup IPI #2 if not yet online
            if (__atomic_load_n(&ap.onlineFlag, __ATOMIC_ACQUIRE) == 0) {
                sendIpiRaw(targetApic, 0x00004600u | AP_TRAMPOLINE_VECTOR);
            }

            // Wait up to ~50ms for AP to enter 64-bit Long Mode & bind KPCR
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

        // Release ALL online hardware cores simultaneously into Ticket Spinlock stress test!
        m_benchExpectedTotal = static_cast<uint64_t>(m_onlineCoreCount) * BENCH_ITERS_PER_CORE;
        __atomic_store_n(&m_benchGo, 1u, __ATOMIC_RELEASE);

        // BSP participates concurrently right alongside APs 1..N-1!
        for (uint32_t k = 0; k < BENCH_ITERS_PER_CORE; ++k) {
            m_benchLock.lock();
            m_sharedProtectedCounter = m_sharedProtectedCounter + 1;
            m_benchLock.unlock();
        }
        __atomic_store_n(&bsp.benchItersDone, BENCH_ITERS_PER_CORE, __ATOMIC_RELEASE);
        __atomic_fetch_add(&m_benchDoneCount, 1u, __ATOMIC_ACQ_REL);

        // Wait for all online AP cores to finish their 25,000 spinlock acquisitions
        for (uint32_t wait = 0; wait < 20000000; ++wait) {
            if (__atomic_load_n(&m_benchDoneCount, __ATOMIC_ACQUIRE) >= m_onlineCoreCount) break;
#if defined(__x86_64__)
            __asm__ __volatile__("pause");
#endif
        }
        m_benchPassed = (m_sharedProtectedCounter == m_benchExpectedTotal) &&
                        (__atomic_load_n(&m_benchDoneCount, __ATOMIC_ACQUIRE) == m_onlineCoreCount);

        // Fire a real hardware IPI (Vector 0x3E) to each online AP to wake it out of HLT and verify IPI delivery!
        spinDelayCycles(5000000ULL);
        broadcastIpiToAps();
        return m_onlineCoreCount;
    }

    /**
     * @brief Send a real hardware Fixed IPI (Vector 0x3E) to every online AP core.
     */
    uint32_t broadcastIpiToAps() noexcept {
        uint32_t acked = 0;
        for (uint32_t i = 1; i < m_madtCoreCount; ++i) {
            auto& ap = m_kpcrs[i];
            if (__atomic_load_n(&ap.onlineFlag, __ATOMIC_ACQUIRE) == 0) continue;
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

        // 3. Bind per-CPU KPCR to this AP's private hardware IA32_GS_BASE (0xC0000101)
        micant::cpu::cpuWrmsr(micant::cpu::MSR_GS_BASE, kpcr.self);
        uint32_t gsReadBack = 0;
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("movl %%gs:0x14, %0" : "=r"(gsReadBack));
#endif
        kpcr.verifiedGsApic = gsReadBack;

        // 4. Signal BSP that this physical core is ONLINE in 64-bit Long Mode!
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

        // 7. Enable interrupts (STI) and enter zero-overhead HLT loop awaiting hardware IPIs (Vector 0x3E)!
#if defined(__x86_64__) && (!defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0)
        __asm__ __volatile__("sti");
        while (true) {
            __asm__ __volatile__("hlt");
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
    [[nodiscard]] bool     isX2Apic() const noexcept { return m_isX2Apic; }
    [[nodiscard]] bool     isMadtParsed() const noexcept { return m_madtParsed; }
    [[nodiscard]] bool     isBenchPassed() const noexcept { return m_benchPassed; }
    [[nodiscard]] uint64_t getSharedProtectedCounter() const noexcept { return m_sharedProtectedCounter; }
    [[nodiscard]] uint64_t getBenchExpectedTotal() const noexcept { return m_benchExpectedTotal; }
    [[nodiscard]] const KpcrBareMetal64* getKpcr(uint32_t idx) const noexcept {
        return (idx < m_madtCoreCount) ? &m_kpcrs[idx] : nullptr;
    }
};

extern "C" inline __attribute__((used)) void KiApSystemStartup64(uint64_t logicalCoreIdx) {
    BareMetalSmpEngine::get().onApEntry64(logicalCoreIdx);
}

} // namespace micant::km::smp_baremetal
