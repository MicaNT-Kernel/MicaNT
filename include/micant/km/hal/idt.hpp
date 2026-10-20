#pragma once

/**
 * @file idt.hpp
 * @brief Clean-room 64-bit Interrupt Descriptor Table (IDT), Local APIC & Hardware Preemption Timer (Unit #97).
 *
 * Implements canonical 64-bit x86-64 Interrupt Descriptor Table (IDT) architecture,
 * Local APIC MMIO register control (at physical 0xFEE00000 and high-canonical 0xFFFFF800FEE00000),
 * vector dispatch gates (Vectors 0..255), and periodic preemption clock accounting (KeUpdateSystemTime).
 *
 * Reference: Intel 64 and IA-32 Architectures Software Developer's Manual, Vol 3A, Chap 6 & 10.
 *            AMD64 Architecture Programmer's Manual, Vol 2: System Programming, Chap 8 & 16.
 */

#include <cstdint>
#include <cstddef>
#include <array>
#include <cstring>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/hal/cpu.hpp"
#include "micant/km/hal/highhalf.hpp"

namespace micant::hal::idt {

// ============================================================================
// 1. Canonical NT & x86-64 Interrupt Vector Constants
// ============================================================================
inline constexpr uint8_t VECTOR_DIVIDE_BY_ZERO         = 0x00; // #DE
inline constexpr uint8_t VECTOR_DEBUG_EXCEPTION        = 0x01; // #DB
inline constexpr uint8_t VECTOR_NMI                    = 0x02; // Non-Maskable Interrupt
inline constexpr uint8_t VECTOR_BREAKPOINT             = 0x03; // #BP (int 3, DPL 3)
inline constexpr uint8_t VECTOR_OVERFLOW               = 0x04; // #OF (into, DPL 3)
inline constexpr uint8_t VECTOR_BOUND_RANGE            = 0x05; // #BR
inline constexpr uint8_t VECTOR_INVALID_OPCODE         = 0x06; // #UD
inline constexpr uint8_t VECTOR_DEVICE_NOT_AVAILABLE   = 0x07; // #NM
inline constexpr uint8_t VECTOR_DOUBLE_FAULT           = 0x08; // #DF (uses IST1)
inline constexpr uint8_t VECTOR_INVALID_TSS            = 0x0A; // #TS
inline constexpr uint8_t VECTOR_SEGMENT_NOT_PRESENT    = 0x0B; // #NP
inline constexpr uint8_t VECTOR_STACK_FAULT            = 0x0C; // #SS
inline constexpr uint8_t VECTOR_GENERAL_PROTECTION     = 0x0D; // #GP
inline constexpr uint8_t VECTOR_PAGE_FAULT             = 0x0E; // #PF
inline constexpr uint8_t VECTOR_X87_FLOATING_POINT     = 0x10; // #MF
inline constexpr uint8_t VECTOR_ALIGNMENT_CHECK        = 0x11; // #AC
inline constexpr uint8_t VECTOR_MACHINE_CHECK          = 0x12; // #MC (uses IST3)
inline constexpr uint8_t VECTOR_SIMD_EXCEPTION         = 0x13; // #XM / #XF

// High Hardware IRQs & Inter-Processor Interrupts (NT Standard Mapping)
inline constexpr uint8_t VECTOR_APC                    = 0x1F; // Asynchronous Procedure Call
inline constexpr uint8_t VECTOR_DISPATCH               = 0x2F; // DPC / Dispatch Reschedule
inline constexpr uint8_t VECTOR_CLOCK_TIMER            = 0x30; // Primary System Clock (100 Hz / 10ms)
inline constexpr uint8_t VECTOR_KEYBOARD               = 0x31; // PS/2 Hardware Keyboard IRQ1
inline constexpr uint8_t VECTOR_TLB_SHOOTDOWN          = 0xFD; // SMP TLB Shootdown
inline constexpr uint8_t VECTOR_FREEZE                 = 0xFE; // Kernel Debugger Freeze
inline constexpr uint8_t VECTOR_SPURIOUS               = 0xFF; // Local APIC Spurious Interrupt

inline constexpr size_t IDT_ENTRY_COUNT                = 256;

// ============================================================================
// 2. 64-bit IDT Structures (Hardware Spec)
// ============================================================================
#pragma pack(push, 1)

struct IdtEntry64 {
    uint16_t offsetLow{0};       // Handler RIP bits 0..15
    uint16_t selector{0};        // Code segment selector (e.g. 0x0010 KGDT64_R0_CODE)
    uint8_t  ist{0};             // Bits 0..2: IST stack index (0=default, 1..7=TSS.ist1..ist7)
    uint8_t  typeAttributes{0};  // P (bit 7), DPL (bits 6:5), Type (bits 3:0: 0xE=Interrupt, 0xF=Trap)
    uint16_t offsetMid{0};       // Handler RIP bits 16..31
    uint32_t offsetHigh{0};      // Handler RIP bits 32..63
    uint32_t reserved{0};        // Must be zero

    void setGate(uint64_t handlerRip, uint16_t csSelector, uint8_t istIndex, uint8_t dpl, bool isTrap) noexcept {
        offsetLow  = static_cast<uint16_t>(handlerRip & 0xFFFF);
        offsetMid  = static_cast<uint16_t>((handlerRip >> 16) & 0xFFFF);
        offsetHigh = static_cast<uint32_t>((handlerRip >> 32) & 0xFFFFFFFF);
        selector   = csSelector;
        ist        = static_cast<uint8_t>(istIndex & 0x07);
        // Type: 0xE = 64-bit Interrupt Gate (clears IF), 0xF = 64-bit Trap Gate (leaves IF)
        uint8_t gateType = isTrap ? 0x0F : 0x0E;
        // P=1 (0x80) | DPL (0..3 << 5) | gateType
        typeAttributes = static_cast<uint8_t>(0x80 | ((dpl & 0x03) << 5) | gateType);
        reserved   = 0;
    }

    [[nodiscard]] uint64_t getHandlerRip() const noexcept {
        return static_cast<uint64_t>(offsetLow) |
               (static_cast<uint64_t>(offsetMid) << 16) |
               (static_cast<uint64_t>(offsetHigh) << 32);
    }

    [[nodiscard]] bool isPresent() const noexcept {
        return (typeAttributes & 0x80) != 0;
    }

    [[nodiscard]] uint8_t getDpl() const noexcept {
        return static_cast<uint8_t>((typeAttributes >> 5) & 0x03);
    }

    [[nodiscard]] bool isTrapGate() const noexcept {
        return (typeAttributes & 0x0F) == 0x0F;
    }
};

struct Idtr64 {
    uint16_t limit{sizeof(IdtEntry64) * IDT_ENTRY_COUNT - 1};
    uint64_t base{0};
};

#pragma pack(pop)

// ============================================================================
// 3. Local APIC Memory-Mapped Register Definitions
// ============================================================================
inline constexpr uint64_t LAPIC_DEFAULT_PHYSICAL_BASE = 0xFEE00000ULL;
inline constexpr uint64_t LAPIC_HIGH_CANONICAL_BASE   = highhalf::toHighCanonical(LAPIC_DEFAULT_PHYSICAL_BASE);

inline constexpr uint32_t LAPIC_REG_ID                = 0x020;
inline constexpr uint32_t LAPIC_REG_VERSION           = 0x030;
inline constexpr uint32_t LAPIC_REG_TPR               = 0x080; // Task Priority Register
inline constexpr uint32_t LAPIC_REG_EOI               = 0x0B0; // End Of Interrupt (write 0)
inline constexpr uint32_t LAPIC_REG_LDR               = 0x0D0; // Logical Destination Register
inline constexpr uint32_t LAPIC_REG_DFR               = 0x0E0; // Destination Format Register
inline constexpr uint32_t LAPIC_REG_SVR               = 0x0F0; // Spurious Vector Register
inline constexpr uint32_t LAPIC_REG_ESR               = 0x280; // Error Status Register
inline constexpr uint32_t LAPIC_REG_ICR_LOW           = 0x300; // Interrupt Command Register Low
inline constexpr uint32_t LAPIC_REG_ICR_HIGH          = 0x310; // Interrupt Command Register High
inline constexpr uint32_t LAPIC_REG_LVT_TIMER         = 0x320; // LVT Timer Register
inline constexpr uint32_t LAPIC_REG_TICR              = 0x380; // Timer Initial Count Register
inline constexpr uint32_t LAPIC_REG_TCCR              = 0x390; // Timer Current Count Register
inline constexpr uint32_t LAPIC_REG_TDCR              = 0x3E0; // Timer Divide Configuration Register

// ============================================================================
// 4. System Time & Preemptive Scheduler Accounting
// ============================================================================
struct SystemTimeAccounting64 {
    uint64_t tickCount{0};              // Increments by 1 every 10ms (100 Hz)
    uint64_t systemTime100ns{0};        // Standard Windows 100-nanosecond units (100,000 per 10ms tick)
    uint32_t threadQuantumRemaining{3}; // Default quantum = 30ms (3 ticks)
    uint32_t contextSwitchesTriggered{0};
    uint64_t totalInterruptCount{0};
    uint64_t clockInterruptCount{0};
    uint64_t keyboardInterruptCount{0};
    uint64_t spuriousInterruptCount{0};
};

extern "C" __attribute__((naked)) inline void KiDefaultInterruptStub() {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "iretq\n\t"
    );
#endif
}

extern "C" __attribute__((naked)) inline void KiDefaultExceptionWithErrorCodeStub() {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "addq $8, %rsp\n\t"
        "iretq\n\t"
    );
#endif
}

// ============================================================================
// 5. Sovereign 64-bit IDT & Interrupt Engine (Unit #97)
// ============================================================================
class InterruptEngine64 {
private:
    alignas(16) std::array<IdtEntry64, IDT_ENTRY_COUNT> m_idtTable{};
    Idtr64                 m_idtr{};
    SystemTimeAccounting64 m_time{};
    uint64_t               m_lapicVirtualBase{LAPIC_DEFAULT_PHYSICAL_BASE};
    bool                   m_idtLoaded{false};
    bool                   m_apicInitialized{false};
    bool                   m_timerActive{false};
    bool                   m_isX2Apic{false};

    InterruptEngine64() = default;

public:
    static InterruptEngine64& get() noexcept {
        static InterruptEngine64 s_instance;
        return s_instance;
    }

    void reset() noexcept {
        m_idtTable.fill(IdtEntry64{});
        m_idtr = Idtr64{};
        m_time = SystemTimeAccounting64{};
        m_lapicVirtualBase = LAPIC_DEFAULT_PHYSICAL_BASE;
        m_idtLoaded = false;
        m_apicInitialized = false;
        m_timerActive = false;
        m_isX2Apic = false;
    }

    /**
     * @brief Installs a gate into the 64-bit IDT table.
     */
    bool setGate(
        uint8_t vector,
        uint64_t handlerRip,
        uint16_t csSelector = micant::cpu::SELECTOR_KCODE64,
        uint8_t istIndex = 0,
        uint8_t dpl = 0,
        bool isTrap = false
    ) noexcept {
        m_idtTable[vector].setGate(handlerRip, csSelector, istIndex, dpl, isTrap);
        return true;
    }

    /**
     * @brief Initializes the IDTR descriptor pointing to the internal table.
     */
    bool initializeIdt(uint64_t defaultStubHandlerRip = 0) noexcept {
        m_idtr.limit = static_cast<uint16_t>(sizeof(m_idtTable) - 1);
        m_idtr.base  = reinterpret_cast<uint64_t>(m_idtTable.data());

        // Fill all 256 gates with proper default handlers
        for (size_t i = 0; i < IDT_ENTRY_COUNT; ++i) {
            bool hasErrorCode = (i == VECTOR_DOUBLE_FAULT ||
                                 i == VECTOR_INVALID_TSS ||
                                 i == VECTOR_SEGMENT_NOT_PRESENT ||
                                 i == VECTOR_STACK_FAULT ||
                                 i == VECTOR_GENERAL_PROTECTION ||
                                 i == VECTOR_PAGE_FAULT ||
                                 i == VECTOR_ALIGNMENT_CHECK ||
                                 i == 21 || i == 29 || i == 30);

            uint64_t handler = (defaultStubHandlerRip != 0) ? defaultStubHandlerRip :
                (hasErrorCode ? reinterpret_cast<uint64_t>(&KiDefaultExceptionWithErrorCodeStub)
                              : reinterpret_cast<uint64_t>(&KiDefaultInterruptStub));

            m_idtTable[i].setGate(
                handler,
                micant::cpu::SELECTOR_KCODE64,
                (i == VECTOR_DOUBLE_FAULT) ? 1 : 0, // Use IST1 for #DF
                (i == VECTOR_BREAKPOINT || i == VECTOR_OVERFLOW) ? 3 : 0, // DPL 3 for INT3/INTO
                false
            );
        }

        m_idtLoaded = true;
        return true;
    }

    /**
     * @brief Executes the LIDT instruction on physical hardware.
     */
    void loadIdtr() noexcept {
#if defined(__x86_64__)
        __asm__ __volatile__("lidt %0" : : "m"(m_idtr) : "memory");
#endif
    }

    /**
     * @brief Initializes Local APIC registers (supporting both x2APIC MSR and legacy xAPIC MMIO).
     */
    bool initializeLocalApic(uint64_t lapicBaseAddress = LAPIC_DEFAULT_PHYSICAL_BASE) noexcept {
        m_lapicVirtualBase = lapicBaseAddress;

        // Query IA32_APIC_BASE MSR (0x1B)
        uint64_t apicBase = micant::cpu::cpuRdmsr(micant::cpu::MSR_IA32_APIC_BASE);
        // Bit 10 = x2APIC Mode Enable (EXTD)
        m_isX2Apic = (apicBase & (1ULL << 10)) != 0;

        if (m_isX2Apic) {
            // x2APIC Architecture: all accesses via MSRs 0x800-0x8FF
            // SVR: Enable APIC (bit 8 = 1) and set spurious vector to 0xFF (Vector 255)
            micant::cpu::cpuWrmsr(micant::cpu::MSR_X2APIC_SVR, 0x100 | VECTOR_SPURIOUS);

            // TPR: Set Task Priority to 0 (accept all interrupts)
            micant::cpu::cpuWrmsr(micant::cpu::MSR_X2APIC_TPR, 0x00);

            // In x2APIC mode, DFR does not exist (flat/clustered is fixed) and LDR is hardware-assigned read-only.
        } else {
            // Legacy xAPIC Architecture: memory-mapped I/O at base address
            uint64_t hwBase = apicBase & 0xFFFFF000ULL;
            if (hwBase != 0) {
                m_lapicVirtualBase = hwBase;
            }

            auto* lapic = reinterpret_cast<volatile uint32_t*>(m_lapicVirtualBase);
            if (lapic) {
                // SVR: Enable APIC (bit 8 = 1) and set spurious vector to 0xFF
                lapic[LAPIC_REG_SVR / 4] = 0x100 | VECTOR_SPURIOUS;

                // TPR: Set Task Priority to 0 (accept all interrupts)
                lapic[LAPIC_REG_TPR / 4] = 0x00;

                // DFR: Flat model (0xFFFFFFFF)
                lapic[LAPIC_REG_DFR / 4] = 0xFFFFFFFF;

                // LDR: All logical APICs set to cluster 1
                lapic[LAPIC_REG_LDR / 4] = (lapic[LAPIC_REG_LDR / 4] & 0x00FFFFFF) | 1;
            }
        }

        m_apicInitialized = true;
        return true;
    }

    /**
     * @brief Configures the Local APIC Timer for periodic clock interrupts.
     * @param targetHz Target frequency (e.g. 100 Hz for 10ms quantum ticks).
     * @param busFrequencyHz Bus clock frequency (default 100MHz on modern Xeon/Core).
     */
    bool configureTimer(uint32_t targetHz = 100, uint32_t busFrequencyHz = 100000000) noexcept {
        if (!m_apicInitialized || targetHz == 0) return false;

        uint32_t timerCount = (busFrequencyHz / 16) / targetHz;

        if (m_isX2Apic) {
            // TDCR: Divide by 16 (0x03)
            micant::cpu::cpuWrmsr(micant::cpu::MSR_X2APIC_TDCR, 0x03);

            // LVT Timer: Periodic mode (bit 17 = 1), unmasked (bit 16 = 0), Vector 0x30
            micant::cpu::cpuWrmsr(micant::cpu::MSR_X2APIC_LVT_TIMER, (1U << 17) | VECTOR_CLOCK_TIMER);

            // TICR: Write Initial Count to start the timer
            micant::cpu::cpuWrmsr(micant::cpu::MSR_X2APIC_TICR, timerCount);
        } else {
            auto* lapic = reinterpret_cast<volatile uint32_t*>(m_lapicVirtualBase);
            if (lapic) {
                // TDCR: Divide by 16 (0x03)
                lapic[LAPIC_REG_TDCR / 4] = 0x03;

                // LVT Timer: Periodic mode (bit 17 = 1), unmasked (bit 16 = 0), Vector 0x30
                lapic[LAPIC_REG_LVT_TIMER / 4] = (1U << 17) | VECTOR_CLOCK_TIMER;

                // TICR: Write Initial Count to start the timer
                lapic[LAPIC_REG_TICR / 4] = timerCount;
            }
        }

        m_timerActive = true;
        return true;
    }

    /**
     * @brief Sends End of Interrupt (EOI) to the Local APIC.
     */
    void sendEoi() noexcept {
        if (m_isX2Apic) {
            micant::cpu::cpuWrmsr(micant::cpu::MSR_X2APIC_EOI, 0);
        } else {
            auto* lapic = reinterpret_cast<volatile uint32_t*>(m_lapicVirtualBase);
            if (lapic) {
                lapic[LAPIC_REG_EOI / 4] = 0;
            }
        }
    }

    /**
     * @brief KeUpdateSystemTime - Canonical NT clock tick interrupt service handler.
     * Called on every LAPIC Timer tick (Vector 0x30, 100 Hz).
     * Updates system uptime, 100ns timestamp, and decrements thread quantum.
     * @return true if running thread quantum expired and rescheduling is requested.
     */
    bool updateSystemTime() noexcept {
        m_time.totalInterruptCount++;
        m_time.clockInterruptCount++;
        m_time.tickCount++;
        m_time.systemTime100ns += 100000; // 10ms = 100,000 100ns units

        bool quantumExpired = false;
        if (m_time.threadQuantumRemaining > 0) {
            m_time.threadQuantumRemaining--;
        }

        if (m_time.threadQuantumRemaining == 0) {
            m_time.threadQuantumRemaining = 3; // Reset quantum to 30ms
            m_time.contextSwitchesTriggered++;
            quantumExpired = true;
        }

        return quantumExpired;
    }

    /**
     * @brief Records a keyboard hardware interrupt event.
     */
    void recordKeyboardInterrupt() noexcept {
        m_time.totalInterruptCount++;
        m_time.keyboardInterruptCount++;
    }

    /**
     * @brief Records a spurious interrupt event.
     */
    void recordSpuriousInterrupt() noexcept {
        m_time.totalInterruptCount++;
        m_time.spuriousInterruptCount++;
    }

    // Accessors
    [[nodiscard]] const IdtEntry64& getGate(uint8_t vector) const noexcept {
        return m_idtTable[vector];
    }

    [[nodiscard]] const Idtr64& getIdtr() const noexcept {
        return m_idtr;
    }

    [[nodiscard]] const SystemTimeAccounting64& getTimeAccounting() const noexcept {
        return m_time;
    }

    [[nodiscard]] bool isIdtLoaded() const noexcept { return m_idtLoaded; }
    [[nodiscard]] bool isApicInitialized() const noexcept { return m_apicInitialized; }
    [[nodiscard]] bool isTimerActive() const noexcept { return m_timerActive; }
    [[nodiscard]] bool isX2Apic() const noexcept { return m_isX2Apic; }
    [[nodiscard]] uint64_t getLapicBase() const noexcept { return m_lapicVirtualBase; }
};

} // namespace micant::hal::idt
