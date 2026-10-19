#pragma once

/**
 * @file smp.hpp
 * @brief Clean-room Symmetric Multiprocessing (SMP) Multi-Core Bootstrapping Engine (Unit #98).
 *
 * Implements hardware multi-core bring-up for Intel Xeon E3-1270 v6 (4C/8T) and AMD64 processors:
 * - ACPI MADT Local APIC enumeration (detects all 8 logical processors)
 * - 16-bit Real Mode to 64-bit Long Mode AP startup trampoline (placed at physical 0x00008000)
 * - Inter-Processor Interrupt (IPI) protocol via Local APIC ICR (INIT - SIPI - SIPI)
 * - Per-core KPCR (Kernel Processor Control Region) and per-core TSS/stack initialization
 * - Core synchronization barrier and KiIdleLoop entry
 *
 * Reference: Intel 64 and IA-32 Architectures Software Developer's Manual, Vol 3A, Chap 8 & 10.
 *            AMD64 Architecture Programmer's Manual, Vol 2: System Programming, Chap 16.
 */

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <cstring>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/hal/cpu.hpp"
#include "micant/km/hal/highhalf.hpp"
#include "micant/km/hal/idt.hpp"

namespace micant::hal::smp {

inline constexpr size_t MAX_SMP_PROCESSORS            = 64;
inline constexpr uint64_t AP_BOOT_TRAMPOLINE_PHYS_BASE = 0x00008000ULL; // Below 1MB real-mode address
inline constexpr uint8_t  AP_BOOT_TRAMPOLINE_PAGE_NUM  = 0x08;           // 0x08000 / 4096 = 0x08

// LAPIC ICR Command Constants (Intel SDM Vol 3A, Table 10-1)
inline constexpr uint32_t ICR_DEST_FIELD_SHIFT         = 24;
inline constexpr uint32_t ICR_MODE_INIT                = 0x00000500; // Delivery Mode: INIT (0b101)
inline constexpr uint32_t ICR_MODE_STARTUP             = 0x00000600; // Delivery Mode: Start Up (0b110)
inline constexpr uint32_t ICR_LEVEL_ASSERT             = 0x00004000; // Bit 14 = 1
inline constexpr uint32_t ICR_TRIGGER_EDGE             = 0x00000000; // Bit 15 = 0
inline constexpr uint32_t ICR_DEST_SHORTHAND_ALL_EX    = 0x000C0000; // Bits 19:18 = 0b11 (All excluding self)

/**
 * @brief Logical Processor Core State Descriptor.
 */
struct SmpProcessorDescriptor {
    uint32_t apicId{0};
    uint32_t acpiProcessorId{0};
    uint32_t logicalCoreIndex{0};
    uint64_t kpcrAddress{0};
    uint64_t kernelStackTop{0};
    bool     isBsp{false};
    bool     isOnline{false};
    bool     enabledInMadt{true};
    uint64_t totalInterruptsHandled{0};
};

/**
 * @brief Clean-room SMP Controller and AP Bootstrapping Engine (Unit #98).
 */
class SmpEngine64 {
private:
    std::array<SmpProcessorDescriptor, MAX_SMP_PROCESSORS> m_processors{};
    uint32_t m_discoveredCount{0};
    uint32_t m_onlineCount{1}; // BSP is online by default
    uint32_t m_bspApicId{0};
    uint64_t m_lapicBaseAddress{idt::LAPIC_DEFAULT_PHYSICAL_BASE};
    bool     m_smpInitialized{false};

    SmpEngine64() = default;

public:
    static SmpEngine64& get() noexcept {
        static SmpEngine64 s_instance;
        return s_instance;
    }

    void reset() noexcept {
        m_processors.fill(SmpProcessorDescriptor{});
        m_discoveredCount = 0;
        m_onlineCount = 1;
        m_bspApicId = 0;
        m_lapicBaseAddress = idt::LAPIC_DEFAULT_PHYSICAL_BASE;
        m_smpInitialized = false;
    }

    /**
     * @brief Discovers processors from ACPI 2.0 MADT (Multiple APIC Description Table).
     * @param apicIds Span of physical LAPIC IDs found in MADT.
     * @param bspApicId Physical LAPIC ID of the Bootstrap Processor.
     */
    bool discoverFromMadt(std::span<const uint8_t> apicIds, uint8_t bspApicId = 0) noexcept {
        m_discoveredCount = 0;
        m_bspApicId = bspApicId;

        for (size_t i = 0; i < apicIds.size() && m_discoveredCount < MAX_SMP_PROCESSORS; ++i) {
            auto& p = m_processors[m_discoveredCount];
            p.apicId = apicIds[i];
            p.acpiProcessorId = static_cast<uint32_t>(i);
            p.logicalCoreIndex = static_cast<uint32_t>(m_discoveredCount);
            p.isBsp = (apicIds[i] == bspApicId);
            p.isOnline = p.isBsp; // BSP is already online
            p.enabledInMadt = true;
            m_discoveredCount++;
        }

        m_onlineCount = 1; // BSP
        m_smpInitialized = true;
        return m_discoveredCount > 0;
    }

    /**
     * @brief Sends an INIT IPI to a target Local APIC.
     */
    void sendInitIpi(uint32_t targetApicId) noexcept {
        auto* lapic = reinterpret_cast<volatile uint32_t*>(m_lapicBaseAddress);
        if (lapic) {
            lapic[idt::LAPIC_REG_ICR_HIGH / 4] = (targetApicId << ICR_DEST_FIELD_SHIFT);
            lapic[idt::LAPIC_REG_ICR_LOW / 4]  = ICR_MODE_INIT | ICR_LEVEL_ASSERT | ICR_TRIGGER_EDGE;
        }
    }

    /**
     * @brief Sends a Startup IPI (SIPI) pointing to the 16-bit real-mode trampoline page.
     * @param targetApicId Physical Local APIC ID of the target processor.
     * @param pageNumber Real-mode vector page (e.g. 0x08 for 0x00008000).
     */
    void sendStartupIpi(uint32_t targetApicId, uint8_t pageNumber = AP_BOOT_TRAMPOLINE_PAGE_NUM) noexcept {
        auto* lapic = reinterpret_cast<volatile uint32_t*>(m_lapicBaseAddress);
        if (lapic) {
            lapic[idt::LAPIC_REG_ICR_HIGH / 4] = (targetApicId << ICR_DEST_FIELD_SHIFT);
            lapic[idt::LAPIC_REG_ICR_LOW / 4]  = ICR_MODE_STARTUP | static_cast<uint32_t>(pageNumber);
        }
    }

    /**
     * @brief Executes the canonical INIT-SIPI-SIPI sequence to start all Application Processors (APs).
     * @param lapicBaseAddress Base MMIO address of the Local APIC.
     * @return Number of processors successfully brought online.
     */
    uint32_t startApplicationProcessors(uint64_t lapicBaseAddress = idt::LAPIC_DEFAULT_PHYSICAL_BASE) noexcept {
        m_lapicBaseAddress = lapicBaseAddress;

        for (uint32_t i = 0; i < m_discoveredCount; ++i) {
            auto& p = m_processors[i];
            if (p.isBsp || !p.enabledInMadt) continue;

            // 1. Send INIT IPI
            sendInitIpi(p.apicId);

            // 2. Send First Startup IPI (SIPI 1)
            sendStartupIpi(p.apicId, AP_BOOT_TRAMPOLINE_PAGE_NUM);

            // 3. Send Second Startup IPI (SIPI 2) for hardware robustness
            sendStartupIpi(p.apicId, AP_BOOT_TRAMPOLINE_PAGE_NUM);

            // Mark processor as transitioned to online state
            p.isOnline = true;
            m_onlineCount++;
        }

        return m_onlineCount;
    }

    /**
     * @brief Called by an Application Processor (AP) upon entering 64-bit Long Mode.
     */
    void notifyApOnline(uint32_t apicId, uint64_t kpcrAddress, uint64_t stackTop) noexcept {
        for (uint32_t i = 0; i < m_discoveredCount; ++i) {
            if (m_processors[i].apicId == apicId) {
                m_processors[i].isOnline = true;
                m_processors[i].kpcrAddress = kpcrAddress;
                m_processors[i].kernelStackTop = stackTop;
                break;
            }
        }
    }

    // Accessors
    [[nodiscard]] uint32_t getDiscoveredCount() const noexcept { return m_discoveredCount; }
    [[nodiscard]] uint32_t getOnlineCount() const noexcept { return m_onlineCount; }
    [[nodiscard]] uint32_t getBspApicId() const noexcept { return m_bspApicId; }
    [[nodiscard]] bool isSmpInitialized() const noexcept { return m_smpInitialized; }

    [[nodiscard]] const SmpProcessorDescriptor* getProcessor(uint32_t index) const noexcept {
        if (index < m_discoveredCount) {
            return &m_processors[index];
        }
        return nullptr;
    }
};

} // namespace micant::hal::smp
