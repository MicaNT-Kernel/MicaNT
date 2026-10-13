#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <chrono>
#include <atomic>
#include <memory>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <functional>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/core/ke.hpp"

namespace micant::hal {

// ============================================================================
// 1. Processor Architecture & Topology Types
// ============================================================================
enum class ProcessorArchitecture : uint16_t {
    Intel = 0,      // PROCESSOR_ARCHITECTURE_INTEL
    Arm64 = 12,     // PROCESSOR_ARCHITECTURE_ARM64
    Amd64 = 9       // PROCESSOR_ARCHITECTURE_AMD64
};

enum class InterruptMode : uint8_t {
    LevelSensitive = 0,
    Latched        = 1      // Edge-triggered
};

enum class IpiDeliveryMode : uint8_t {
    Fixed          = 0,
    LowestPriority = 1,
    SMI            = 2,
    NMI            = 4,
    INIT           = 5,
    StartUp        = 6
};

enum class IpiType : uint32_t {
    Apc            = 0x1F,  // Vector 0x1F: Execute pending APCs on remote core
    Dispatch       = 0x2F,  // Vector 0x2F: Reschedule / drain DPCs on remote core
    FlushMultipleTb= 0xFD,  // Vector 0xFD: SMP TLB Shootdown across all cores
    Freeze         = 0xFE   // Vector 0xFE: Kernel debugger / BugCheck core freeze
};

// ============================================================================
// 2. Advanced Programmable Interrupt Controller (LAPIC / Local APIC)
// ============================================================================
inline constexpr uintptr_t DEFAULT_LAPIC_BASE = 0xFEE00000ULL;

// Standard Intel/AMD APIC MMIO Register Offsets
inline constexpr uint32_t APIC_REG_ID          = 0x020;
inline constexpr uint32_t APIC_REG_VERSION     = 0x030;
inline constexpr uint32_t APIC_REG_TPR         = 0x080; // Task Priority Register
inline constexpr uint32_t APIC_REG_APR         = 0x090; // Arbitration Priority Register
inline constexpr uint32_t APIC_REG_PPR         = 0x0A0; // Processor Priority Register
inline constexpr uint32_t APIC_REG_EOI         = 0x0B0; // End Of Interrupt Register
inline constexpr uint32_t APIC_REG_LDR         = 0x0D0; // Logical Destination Register
inline constexpr uint32_t APIC_REG_DFR         = 0x0E0; // Destination Format Register
inline constexpr uint32_t APIC_REG_SVR         = 0x0F0; // Spurious Interrupt Vector Register
inline constexpr uint32_t APIC_REG_ISR_BASE    = 0x100; // In-Service Register (0x100..0x170, 8 dwords)
inline constexpr uint32_t APIC_REG_TMR_BASE    = 0x180; // Trigger Mode Register (0x180..0x1F0)
inline constexpr uint32_t APIC_REG_IRR_BASE    = 0x200; // Interrupt Request Register (0x200..0x270)
inline constexpr uint32_t APIC_REG_ESR         = 0x280; // Error Status Register
inline constexpr uint32_t APIC_REG_ICR_LOW     = 0x300; // Interrupt Command Register Low
inline constexpr uint32_t APIC_REG_ICR_HIGH    = 0x310; // Interrupt Command Register High
inline constexpr uint32_t APIC_REG_LVT_TIMER   = 0x320; // LVT Timer Register
inline constexpr uint32_t APIC_REG_LVT_THERMAL = 0x330; // LVT Thermal Monitor
inline constexpr uint32_t APIC_REG_LVT_PERF    = 0x340; // LVT Performance Counter
inline constexpr uint32_t APIC_REG_LVT_LINT0   = 0x350; // LVT LINT0
inline constexpr uint32_t APIC_REG_LVT_LINT1   = 0x360; // LVT LINT1
inline constexpr uint32_t APIC_REG_LVT_ERROR   = 0x370; // LVT Error Register
inline constexpr uint32_t APIC_REG_TICR        = 0x380; // Timer Initial Count Register
inline constexpr uint32_t APIC_REG_TCCR        = 0x390; // Timer Current Count Register
inline constexpr uint32_t APIC_REG_TDCR        = 0x3E0; // Timer Divide Configuration Register
inline constexpr uint32_t IA32_TSC_DEADLINE_MSR = 0x6E0;

enum class ApicTimerMode : uint8_t {
    OneShot     = 0,
    Periodic    = 1,
    TscDeadline = 2
};

class LocalApic {
public:
    explicit LocalApic(uint32_t apicId = 0, uintptr_t baseAddr = DEFAULT_LAPIC_BASE)
        : apicId_(apicId), baseAddress_(baseAddr) {
        reset();
    }

    void reset() {
        registers_.fill(0);
        registers_[APIC_REG_ID / 4] = (apicId_ << 24);
        registers_[APIC_REG_VERSION / 4] = 0x00050014; // Integrated APIC, max LVT 5, version 0x14
        registers_[APIC_REG_SVR / 4] = 0x1FF;          // APIC enabled, spurious vector 0xFF
        registers_[APIC_REG_TPR / 4] = 0;              // Task priority 0 (PASSIVE_LEVEL)
        enabled_ = true;
        tscDeadline_ = 0;
    }

    [[nodiscard]] uint32_t readRegister(uint32_t offset) const noexcept {
        uint32_t idx = offset / 4;
        if (idx < registers_.size()) {
            return registers_[idx];
        }
        return 0;
    }

    void writeRegister(uint32_t offset, uint32_t value) noexcept {
        uint32_t idx = offset / 4;
        if (idx >= registers_.size()) return;

        if (offset == APIC_REG_EOI) {
            clearHighestInServiceInterrupt();
            return;
        }

        registers_[idx] = value;
        if (offset == APIC_REG_SVR) {
            enabled_ = (value & 0x100) != 0;
        }
    }

    void setTaskPriority(uint8_t tpr) noexcept {
        registers_[APIC_REG_TPR / 4] = tpr;
    }

    [[nodiscard]] uint8_t getTaskPriority() const noexcept {
        return static_cast<uint8_t>(registers_[APIC_REG_TPR / 4] & 0xFF);
    }

    void setCr8(uint8_t cr8) noexcept {
        uint8_t val = static_cast<uint8_t>((cr8 & 0x0F) << 4);
        setTaskPriority(val);
    }

    [[nodiscard]] uint8_t getCr8() const noexcept {
        return static_cast<uint8_t>((getTaskPriority() >> 4) & 0x0F);
    }

    [[nodiscard]] bool isVectorMaskedByTpr(uint8_t vector) const noexcept {
        uint8_t tprClass = getTaskPriority() >> 4;
        uint8_t vecClass = vector >> 4;
        return vecClass <= tprClass;
    }

    [[nodiscard]] uint8_t getHighestPendingVector() const noexcept {
        for (int i = 7; i >= 0; --i) {
            uint32_t idx = (APIC_REG_IRR_BASE / 4) + i;
            uint32_t val = registers_[idx];
            if (val != 0) {
                for (int b = 31; b >= 0; --b) {
                    if (val & (1U << b)) {
                        return static_cast<uint8_t>(i * 32 + b);
                    }
                }
            }
        }
        return 0;
    }

    [[nodiscard]] uint8_t getHighestDeliverableVector() const noexcept {
        uint8_t tprClass = getTaskPriority() >> 4;
        for (int i = 7; i >= 0; --i) {
            uint32_t idx = (APIC_REG_IRR_BASE / 4) + i;
            uint32_t val = registers_[idx];
            if (val != 0) {
                for (int b = 31; b >= 0; --b) {
                    if (val & (1U << b)) {
                        uint8_t vec = static_cast<uint8_t>(i * 32 + b);
                        if ((vec >> 4) > tprClass) {
                            return vec;
                        }
                    }
                }
            }
        }
        return 0;
    }

    void setTimerMode(ApicTimerMode mode) noexcept {
        uint32_t& lvt = registers_[APIC_REG_LVT_TIMER / 4];
        lvt = (lvt & ~(0x3U << 17)) | ((static_cast<uint32_t>(mode) & 0x3U) << 17);
    }

    [[nodiscard]] ApicTimerMode getTimerMode() const noexcept {
        return static_cast<ApicTimerMode>((registers_[APIC_REG_LVT_TIMER / 4] >> 17) & 0x3U);
    }

    void setTimerVector(uint8_t vector) noexcept {
        uint32_t& lvt = registers_[APIC_REG_LVT_TIMER / 4];
        lvt = (lvt & ~0xFFU) | vector;
    }

    [[nodiscard]] uint8_t getTimerVector() const noexcept {
        return static_cast<uint8_t>(registers_[APIC_REG_LVT_TIMER / 4] & 0xFFU);
    }

    void setTimerMasked(bool masked) noexcept {
        uint32_t& lvt = registers_[APIC_REG_LVT_TIMER / 4];
        if (masked) {
            lvt |= (1U << 16);
        } else {
            lvt &= ~(1U << 16);
        }
    }

    [[nodiscard]] bool isTimerMasked() const noexcept {
        return (registers_[APIC_REG_LVT_TIMER / 4] & (1U << 16)) != 0;
    }

    void writeTscDeadline(uint64_t deadlineTicks) noexcept {
        tscDeadline_ = deadlineTicks;
    }

    [[nodiscard]] uint64_t readTscDeadline() const noexcept {
        return tscDeadline_;
    }

    bool checkTscDeadline(uint64_t currentTsc) noexcept {
        if (getTimerMode() != ApicTimerMode::TscDeadline) {
            return false;
        }
        if (tscDeadline_ == 0) {
            return false;
        }
        if (currentTsc >= tscDeadline_) {
            tscDeadline_ = 0;
            if (!isTimerMasked()) {
                uint8_t vec = getTimerVector();
                if (vec >= 16) {
                    requestInterrupt(vec);
                }
            }
            return true;
        }
        return false;
    }

    void signalEoi() noexcept {
        clearHighestInServiceInterrupt();
    }

    void requestInterrupt(uint8_t vector) noexcept {
        uint32_t regIdx = (APIC_REG_IRR_BASE / 4) + (vector / 32);
        uint32_t bit = 1U << (vector % 32);
        if (regIdx < registers_.size()) {
            registers_[regIdx] |= bit;
        }
    }

    void markInterruptInService(uint8_t vector) noexcept {
        // Clear from IRR
        uint32_t irrIdx = (APIC_REG_IRR_BASE / 4) + (vector / 32);
        uint32_t bit = 1U << (vector % 32);
        if (irrIdx < registers_.size()) {
            registers_[irrIdx] &= ~bit;
        }
        // Set in ISR
        uint32_t isrIdx = (APIC_REG_ISR_BASE / 4) + (vector / 32);
        if (isrIdx < registers_.size()) {
            registers_[isrIdx] |= bit;
        }
    }

    void clearHighestInServiceInterrupt() noexcept {
        for (int i = 7; i >= 0; --i) {
            uint32_t idx = (APIC_REG_ISR_BASE / 4) + i;
            if (registers_[idx] != 0) {
                for (int b = 31; b >= 0; --b) {
                    if (registers_[idx] & (1U << b)) {
                        registers_[idx] &= ~(1U << b);
                        return;
                    }
                }
            }
        }
    }

    [[nodiscard]] bool isInterruptInService(uint8_t vector) const noexcept {
        uint32_t isrIdx = (APIC_REG_ISR_BASE / 4) + (vector / 32);
        uint32_t bit = 1U << (vector % 32);
        if (isrIdx < registers_.size()) {
            return (registers_[isrIdx] & bit) != 0;
        }
        return false;
    }

    [[nodiscard]] bool isInterruptPending(uint8_t vector) const noexcept {
        uint32_t irrIdx = (APIC_REG_IRR_BASE / 4) + (vector / 32);
        uint32_t bit = 1U << (vector % 32);
        if (irrIdx < registers_.size()) {
            return (registers_[irrIdx] & bit) != 0;
        }
        return false;
    }

    [[nodiscard]] uint32_t getApicId() const noexcept { return apicId_; }
    [[nodiscard]] uintptr_t getBaseAddress() const noexcept { return baseAddress_; }
    [[nodiscard]] bool isEnabled() const noexcept { return enabled_; }

private:
    uint32_t apicId_{0};
    uintptr_t baseAddress_{DEFAULT_LAPIC_BASE};
    bool enabled_{true};
    uint64_t tscDeadline_{0};
    std::array<uint32_t, 0x400 / 4> registers_{};
};

// ============================================================================
// 3. I/O Advanced Programmable Interrupt Controller (I/O APIC)
// ============================================================================
inline constexpr uintptr_t DEFAULT_IOAPIC_BASE = 0xFEC00000ULL;
inline constexpr uint32_t IOAPIC_MAX_REDIR_ENTRIES = 24;

struct IoApicRedirectionEntry {
    uint8_t vector{0};
    IpiDeliveryMode deliveryMode{IpiDeliveryMode::Fixed};
    bool destinationModeLogical{false}; // 0 = Physical, 1 = Logical
    bool deliveryStatusPending{false};  // 0 = Idle, 1 = Send Pending
    bool polarityLow{false};            // 0 = Active High, 1 = Active Low
    bool remoteIrr{false};
    InterruptMode triggerMode{InterruptMode::Latched}; // Edge or Level
    bool masked{true};                  // 0 = Enabled, 1 = Masked
    uint8_t destinationApicId{0};       // Target APIC ID
};

class IoApic {
public:
    explicit IoApic(uint32_t id = 0, uintptr_t baseAddr = DEFAULT_IOAPIC_BASE)
        : ioApicId_(id), baseAddress_(baseAddr) {
        reset();
    }

    void reset() {
        for (auto& entry : redirectionTable_) {
            entry = IoApicRedirectionEntry{};
            entry.masked = true;
        }
    }

    bool setRedirection(uint8_t irq, const IoApicRedirectionEntry& entry) noexcept {
        if (irq >= IOAPIC_MAX_REDIR_ENTRIES) return false;
        redirectionTable_[irq] = entry;
        return true;
    }

    [[nodiscard]] const IoApicRedirectionEntry* getRedirection(uint8_t irq) const noexcept {
        if (irq >= IOAPIC_MAX_REDIR_ENTRIES) return nullptr;
        return &redirectionTable_[irq];
    }

    bool routeIrq(uint8_t irq, uint8_t vector, uint8_t targetApicId, InterruptMode mode = InterruptMode::Latched) noexcept {
        if (irq >= IOAPIC_MAX_REDIR_ENTRIES) return false;
        redirectionTable_[irq].vector = vector;
        redirectionTable_[irq].destinationApicId = targetApicId;
        redirectionTable_[irq].triggerMode = mode;
        redirectionTable_[irq].masked = false;
        redirectionTable_[irq].deliveryMode = IpiDeliveryMode::Fixed;
        return true;
    }

    bool maskIrq(uint8_t irq, bool masked) noexcept {
        if (irq >= IOAPIC_MAX_REDIR_ENTRIES) return false;
        redirectionTable_[irq].masked = masked;
        return true;
    }

    [[nodiscard]] uint32_t getId() const noexcept { return ioApicId_; }
    [[nodiscard]] uintptr_t getBaseAddress() const noexcept { return baseAddress_; }

private:
    uint32_t ioApicId_{0};
    uintptr_t baseAddress_{DEFAULT_IOAPIC_BASE};
    std::array<IoApicRedirectionEntry, IOAPIC_MAX_REDIR_ENTRIES> redirectionTable_{};
};

// ============================================================================
// 4. Direct Memory Access (DMA) & Bus Master Subsystem
// ============================================================================
enum class InterfaceType : uint32_t {
    Internal,
    Isa,
    Eisa,
    MicroChannel,
    TurboChannel,
    PCIBus,
    VMEBus,
    NuBus,
    PCMCIABus,
    CBus,
    MPIBus,
    MPSABus,
    ProcessorInternal,
    InternalPowerBus,
    PNPISABus,
    PNPBus,
    MaximumInterfaceType
};

struct DEVICE_DESCRIPTION {
    uint32_t version{1};
    bool master{true};
    bool scatterGather{true};
    bool demandMode{false};
    bool autoInitialize{false};
    bool dma32BitAddresses{true};
    bool ignoreCount{false};
    bool reserved1{false};
    bool dma64BitAddresses{true};
    InterfaceType interfaceType{InterfaceType::PCIBus};
    uint32_t busNumber{0};
    uint32_t dmaChannel{0};
    uint32_t maximumLength{64 * 1024}; // Default 64KB max transfer
    uint32_t dmaSpeed{0};
    uint32_t dmaWidth{0};
    uint32_t dmaPort{0};
};

struct SCATTER_GATHER_ELEMENT {
    uint64_t address{0}; // Physical 64-bit Address
    uint32_t length{0};  // Segment byte count
    uint64_t reserved{0};
};

struct SCATTER_GATHER_LIST {
    uint32_t numberOfElements{0};
    uint64_t reserved{0};
    std::vector<SCATTER_GATHER_ELEMENT> elements;
};

class DmaAdapter {
public:
    explicit DmaAdapter(const DEVICE_DESCRIPTION& desc, uint32_t mapRegisters = 64)
        : description_(desc), mapRegisters_(mapRegisters) {}

    ~DmaAdapter() {
        for (auto& rec : allocatedBuffers_) {
            if (rec.virtAddr) {
                std::free(rec.virtAddr);
            }
        }
        allocatedBuffers_.clear();
    }

    void* allocateCommonBuffer(size_t length, uint64_t* logicalAddress, bool cacheEnabled = false) {
        if (length == 0 || !logicalAddress) return nullptr;
        void* buf = std::malloc(length);
        if (!buf) return nullptr;
        std::memset(buf, 0, length);
        
        *logicalAddress = reinterpret_cast<uint64_t>(buf);
        allocatedBuffers_.push_back({buf, length, *logicalAddress, cacheEnabled});
        return buf;
    }

    void freeCommonBuffer(size_t length, uint64_t logicalAddress, void* virtualAddress, bool cacheEnabled = false) {
        (void)length;
        (void)logicalAddress;
        (void)cacheEnabled;
        if (!virtualAddress) return;
        
        auto it = std::find_if(allocatedBuffers_.begin(), allocatedBuffers_.end(),
            [virtualAddress](const CommonBufferRecord& rec) {
                return rec.virtAddr == virtualAddress;
            });
        if (it != allocatedBuffers_.end()) {
            std::free(it->virtAddr);
            allocatedBuffers_.erase(it);
        }
    }

    SCATTER_GATHER_LIST buildScatterGatherList(uintptr_t virtualAddress, size_t length) {
        SCATTER_GATHER_LIST sgList{};
        if (length == 0) return sgList;

        constexpr size_t PAGE_CHUNK = 4096;
        size_t bytesLeft = length;
        uintptr_t currAddr = virtualAddress;

        while (bytesLeft > 0) {
            size_t chunk = std::min(bytesLeft, PAGE_CHUNK);
            SCATTER_GATHER_ELEMENT elem{};
            elem.address = static_cast<uint64_t>(currAddr);
            elem.length = static_cast<uint32_t>(chunk);
            sgList.elements.push_back(elem);
            currAddr += chunk;
            bytesLeft -= chunk;
        }

        sgList.numberOfElements = static_cast<uint32_t>(sgList.elements.size());
        return sgList;
    }

    [[nodiscard]] const DEVICE_DESCRIPTION& getDescription() const noexcept { return description_; }
    [[nodiscard]] uint32_t getMapRegisters() const noexcept { return mapRegisters_; }

private:
    struct CommonBufferRecord {
        void* virtAddr{nullptr};
        size_t length{0};
        uint64_t physAddr{0};
        bool cacheEnabled{false};
    };

    DEVICE_DESCRIPTION description_{};
    uint32_t mapRegisters_{64};
    std::vector<CommonBufferRecord> allocatedBuffers_;
};

// ============================================================================
// 5. Real-Time Clock (RTC) & High Precision Time Fields
// ============================================================================
struct TIME_FIELDS {
    int16_t year{2026};
    int16_t month{10};
    int16_t day{10};
    int16_t hour{12};
    int16_t minute{0};
    int16_t second{0};
    int16_t milliseconds{0};
    int16_t weekday{6}; // 0 = Sunday, 6 = Saturday
};

inline constexpr bool HalpIsLeapYear(int16_t year) noexcept {
    return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}

inline constexpr uint8_t HalpDaysInMonth(int16_t year, int16_t month) noexcept {
    constexpr uint8_t daysNormal[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12) return 0;
    if (month == 2 && HalpIsLeapYear(year)) return 29;
    return daysNormal[month];
}

inline constexpr uint8_t RtcBcdToBinary(uint8_t bcd) noexcept {
    return static_cast<uint8_t>(((bcd >> 4) * 10) + (bcd & 0x0F));
}

inline constexpr uint8_t RtcBinaryToBcd(uint8_t bin) noexcept {
    return static_cast<uint8_t>((((bin / 10) % 10) << 4) | (bin % 10));
}

inline bool HalpTimeFieldsToSystemTime(const TIME_FIELDS* timeFields, LargeInteger* systemTime) noexcept {
    if (!timeFields || !systemTime) return false;
    if (timeFields->year < 1601) return false;
    if (timeFields->month < 1 || timeFields->month > 12) return false;
    uint8_t maxDays = HalpDaysInMonth(timeFields->year, timeFields->month);
    if (timeFields->day < 1 || timeFields->day > maxDays) return false;
    if (timeFields->hour < 0 || timeFields->hour > 23) return false;
    if (timeFields->minute < 0 || timeFields->minute > 59) return false;
    if (timeFields->second < 0 || timeFields->second > 59) return false;
    if (timeFields->milliseconds < 0 || timeFields->milliseconds > 999) return false;

    int16_t Y = timeFields->year;
    uint64_t diffYears = static_cast<uint64_t>(Y - 1601);
    uint64_t prevYear = static_cast<uint64_t>(Y - 1);
    uint64_t leapYears = (prevYear / 4) - (prevYear / 100) + (prevYear / 400) - 388ULL;
    uint64_t totalDays = diffYears * 365ULL + leapYears;

    for (int16_t m = 1; m < timeFields->month; ++m) {
        totalDays += HalpDaysInMonth(Y, m);
    }
    totalDays += static_cast<uint64_t>(timeFields->day - 1);

    uint64_t totalSeconds = totalDays * 86400ULL
                          + static_cast<uint64_t>(timeFields->hour) * 3600ULL
                          + static_cast<uint64_t>(timeFields->minute) * 60ULL
                          + static_cast<uint64_t>(timeFields->second);

    uint64_t intervals = totalSeconds * 10'000'000ULL
                       + static_cast<uint64_t>(timeFields->milliseconds) * 10'000ULL;

    systemTime->quadPart = static_cast<int64_t>(intervals);
    return true;
}

inline bool HalpSystemTimeToTimeFields(const LargeInteger* systemTime, TIME_FIELDS* timeFields) noexcept {
    if (!systemTime || !timeFields || systemTime->quadPart < 0) return false;

    uint64_t intervals = static_cast<uint64_t>(systemTime->quadPart);
    uint64_t totalDays = intervals / 864'000'000'000ULL;
    uint64_t dayRemainder = intervals % 864'000'000'000ULL;

    timeFields->hour = static_cast<int16_t>(dayRemainder / 36'000'000'000ULL);
    dayRemainder %= 36'000'000'000ULL;

    timeFields->minute = static_cast<int16_t>(dayRemainder / 600'000'000ULL);
    dayRemainder %= 600'000'000ULL;

    timeFields->second = static_cast<int16_t>(dayRemainder / 10'000'000ULL);
    timeFields->milliseconds = static_cast<int16_t>((dayRemainder % 10'000'000ULL) / 10'000ULL);

    timeFields->weekday = static_cast<int16_t>((totalDays + 1) % 7);

    // Fast-forward 400-year blocks (146,097 days per 400 years)
    uint64_t num400 = totalDays / 146097ULL;
    totalDays %= 146097ULL;
    int16_t year = static_cast<int16_t>(1601 + num400 * 400);

    while (true) {
        uint32_t daysInYear = HalpIsLeapYear(year) ? 366 : 365;
        if (totalDays < daysInYear) break;
        totalDays -= daysInYear;
        year++;
    }
    timeFields->year = year;

    int16_t month = 1;
    while (month <= 12) {
        uint8_t dim = HalpDaysInMonth(year, month);
        if (totalDays < dim) break;
        totalDays -= dim;
        month++;
    }
    timeFields->month = month;
    timeFields->day = static_cast<int16_t>(totalDays + 1);
    return true;
}

// ============================================================================
// 6. Kernel Processor Control Region (KPCR) & Control Block (KPRCB)
// ============================================================================
struct KernelProcessorControlRegion;

struct KernelProcessorControlBlock {
    uint32_t cpuId{0};
    ProcessorArchitecture architecture{ProcessorArchitecture::Amd64};
    uint32_t coreClockMhz{3600};
    uint32_t numaNodeId{0};
    LocalApic lapic{0};

    // Diagnostics and scheduler accounting
    std::atomic<uint64_t> contextSwitches{0};
    std::atomic<uint64_t> interruptsServiced{0};
    std::atomic<uint64_t> dpcsServiced{0};
    std::atomic<uint64_t> totalCycles{0};
    std::atomic<uint64_t> tlbFlushRequests{0};
    std::atomic<uint64_t> ipiSentCount{0};
    std::atomic<uint64_t> ipiReceivedCount{0};

    bool dpcRoutineActive{false};

    // Thread execution pointers (clean-room mapping of NT KPRCB)
    void* currentThread{nullptr};  // ke::KThread*
    void* nextThread{nullptr};     // ke::KThread*
    void* idleThread{nullptr};     // ke::KThread*
};

struct KernelProcessorControlRegion {
    KernelProcessorControlRegion* self{this};
    void* currentThread{nullptr};  // KTHREAD* (mirrored from PRCB for fast GS lookup)
    void* nextThread{nullptr};     // KTHREAD*
    void* idleThread{nullptr};     // KTHREAD*
    ke::KIRQL currentIrql{ke::PASSIVE_LEVEL};

    uint32_t irr{0};               // Interrupt Request Register bitmap
    uint32_t irrActive{0};         // Active IRR mask
    uint32_t idr{0xFFFFFFFF};      // Interrupt Disable Register

    KernelProcessorControlBlock prcb{};
};

// ============================================================================
// 7. Hardware Abstraction Layer (HAL) Core Engine
// ============================================================================
struct HAL_TELEMETRY {
    uint64_t totalHardwareInterrupts{0};
    uint64_t totalSpuriousInterrupts{0};
    uint64_t totalIpisDispatched{0};
    uint64_t totalTscDeadlineExpiries{0};
    uint64_t totalHpetTicks{0};
    uint64_t totalMsiDelivered{0};
    uint64_t totalIrqlRaises{0};
    uint64_t totalIrqlLowers{0};
    uint64_t totalRtcSyncs{0};
};

struct MsiMessage {
    uint64_t address{0xFEE00000ULL};
    uint32_t data{0};

    static MsiMessage create(uint8_t targetApicId, uint8_t vector,
                             IpiDeliveryMode deliveryMode = IpiDeliveryMode::Fixed,
                             InterruptMode triggerMode = InterruptMode::Latched,
                             bool redirectionHint = false) {
        MsiMessage msg{};
        msg.address = 0xFEE00000ULL | (static_cast<uint64_t>(targetApicId) << 12) | (redirectionHint ? (1ULL << 3) : 0);
        msg.data = static_cast<uint32_t>(vector)
                 | ((static_cast<uint32_t>(deliveryMode) & 0x7U) << 8)
                 | (triggerMode == InterruptMode::LevelSensitive ? (1U << 15) | (1U << 14) : 0U);
        return msg;
    }

    [[nodiscard]] uint8_t getTargetApicId() const noexcept {
        return static_cast<uint8_t>((address >> 12) & 0xFF);
    }

    [[nodiscard]] uint8_t getVector() const noexcept {
        return static_cast<uint8_t>(data & 0xFF);
    }

    [[nodiscard]] IpiDeliveryMode getDeliveryMode() const noexcept {
        return static_cast<IpiDeliveryMode>((data >> 8) & 0x07);
    }

    [[nodiscard]] InterruptMode getTriggerMode() const noexcept {
        return (data & (1U << 15)) ? InterruptMode::LevelSensitive : InterruptMode::Latched;
    }
};

inline constexpr uintptr_t DEFAULT_HPET_BASE = 0xFED00000ULL;
inline constexpr size_t HPET_MAX_COMPARATORS = 8;

struct HpetComparatorConfig {
    uint64_t configurationAndCapabilities{0x0000000000000010ULL}; // Periodic capable (bit 4)
    uint64_t comparatorValue{0};
    uint64_t periodicAccumulator{0};
    uint8_t routedVector{0};
    bool interruptTriggered{false};
};

class HpetTimer {
public:
    explicit HpetTimer(uintptr_t baseAddress = DEFAULT_HPET_BASE, uint64_t periodFemtoseconds = 100'000'000ULL)
        : baseAddress_(baseAddress), periodFemtoseconds_(periodFemtoseconds) {
        reset();
    }

    void reset() {
        mainCounter_ = 0;
        enabled_ = false;
        legacyReplacement_ = false;
        for (size_t i = 0; i < comparators_.size(); ++i) {
            comparators_[i] = HpetComparatorConfig{};
            comparators_[i].configurationAndCapabilities = (1ULL << 4) | (1ULL << 5);
        }
    }

    [[nodiscard]] uintptr_t getBaseAddress() const noexcept { return baseAddress_; }
    [[nodiscard]] uint64_t getPeriodFemtoseconds() const noexcept { return periodFemtoseconds_; }
    void setPeriodFemtoseconds(uint64_t fs) noexcept { periodFemtoseconds_ = fs; }

    [[nodiscard]] bool isEnabled() const noexcept { return enabled_; }
    void setEnabled(bool enable) noexcept { enabled_ = enable; }

    [[nodiscard]] bool isLegacyReplacement() const noexcept { return legacyReplacement_; }
    void setLegacyReplacement(bool legRt) noexcept { legacyReplacement_ = legRt; }

    [[nodiscard]] uint64_t readCounter() const noexcept { return mainCounter_; }
    void setCounter(uint64_t val) noexcept { mainCounter_ = val; }

    [[nodiscard]] size_t getComparatorCount() const noexcept { return comparators_.size(); }

    bool configureComparator(size_t index, uint64_t matchValue, bool periodic, uint64_t periodicInterval, uint8_t vector, bool enableInterrupt) {
        if (index >= comparators_.size()) return false;
        auto& comp = comparators_[index];
        comp.comparatorValue = matchValue;
        comp.periodicAccumulator = periodicInterval;
        comp.routedVector = vector;
        comp.interruptTriggered = false;

        uint64_t cfg = comp.configurationAndCapabilities;
        if (enableInterrupt) cfg |= (1ULL << 2);
        else cfg &= ~(1ULL << 2);

        if (periodic) cfg |= (1ULL << 3);
        else cfg &= ~(1ULL << 3);

        cfg = (cfg & ~(0x1FULL << 9)) | (static_cast<uint64_t>(vector & 0x1F) << 9);
        comp.configurationAndCapabilities = cfg;
        return true;
    }

    [[nodiscard]] const HpetComparatorConfig* getComparator(size_t index) const noexcept {
        if (index >= comparators_.size()) return nullptr;
        return &comparators_[index];
    }

    [[nodiscard]] bool isComparatorInterruptTriggered(size_t index) const noexcept {
        if (index >= comparators_.size()) return false;
        return comparators_[index].interruptTriggered;
    }

    void clearComparatorInterrupt(size_t index) noexcept {
        if (index < comparators_.size()) {
            comparators_[index].interruptTriggered = false;
        }
    }

    uint32_t step(uint64_t ticks = 1) {
        if (!enabled_ || ticks == 0) return 0;
        uint64_t oldCounter = mainCounter_;
        mainCounter_ += ticks;
        uint32_t triggeredCount = 0;

        for (auto& comp : comparators_) {
            bool intEnabled = (comp.configurationAndCapabilities & (1ULL << 2)) != 0;
            if (intEnabled) {
                if (oldCounter < comp.comparatorValue && mainCounter_ >= comp.comparatorValue) {
                    comp.interruptTriggered = true;
                    triggeredCount++;
                    bool isPeriodic = (comp.configurationAndCapabilities & (1ULL << 3)) != 0;
                    if (isPeriodic && comp.periodicAccumulator > 0) {
                        while (comp.comparatorValue <= mainCounter_) {
                            comp.comparatorValue += comp.periodicAccumulator;
                        }
                    }
                }
            }
        }
        return triggeredCount;
    }

    [[nodiscard]] uint64_t calibrateTscFrequency(uint64_t tscElapsed, uint64_t hpetTicksElapsed) const noexcept {
        if (hpetTicksElapsed == 0 || periodFemtoseconds_ == 0) return 0;
        constexpr uint64_t fsPerSec = 1'000'000'000'000'000ULL;
        if (fsPerSec % periodFemtoseconds_ == 0) {
            uint64_t hpetTicksPerSec = fsPerSec / periodFemtoseconds_;
            if (tscElapsed <= (UINT64_MAX / hpetTicksPerSec)) {
                return (tscElapsed * hpetTicksPerSec) / hpetTicksElapsed;
            }
        }
        double num = static_cast<double>(tscElapsed) * 1e15;
        double denom = static_cast<double>(hpetTicksElapsed) * static_cast<double>(periodFemtoseconds_);
        return static_cast<uint64_t>(num / denom);
    }

private:
    uintptr_t baseAddress_{DEFAULT_HPET_BASE};
    uint64_t periodFemtoseconds_{100'000'000ULL};
    uint64_t mainCounter_{0};
    bool enabled_{false};
    bool legacyReplacement_{false};
    std::array<HpetComparatorConfig, HPET_MAX_COMPARATORS> comparators_{};
};

struct KINTERRUPT;

using PKSERVICE_ROUTINE = std::function<bool(KINTERRUPT* interrupt, void* serviceContext)>;

struct KINTERRUPT {
    uint32_t vector{0};
    ke::KIRQL irql{ke::PASSIVE_LEVEL};
    ke::KIRQL synchronizeIrql{ke::PASSIVE_LEVEL};
    InterruptMode mode{InterruptMode::Latched};
    bool shared{false};
    PKSERVICE_ROUTINE isr{nullptr};
    void* serviceContext{nullptr};
    std::atomic<uint64_t> interruptCount{0};
    KINTERRUPT* nextShared{nullptr};
    bool connected{false};

    KINTERRUPT() = default;
    KINTERRUPT(const KINTERRUPT& other)
        : vector(other.vector),
          irql(other.irql),
          synchronizeIrql(other.synchronizeIrql),
          mode(other.mode),
          shared(other.shared),
          isr(other.isr),
          serviceContext(other.serviceContext),
          interruptCount(other.interruptCount.load()),
          nextShared(other.nextShared),
          connected(other.connected) {}

    KINTERRUPT& operator=(const KINTERRUPT& other) {
        if (this != &other) {
            vector = other.vector;
            irql = other.irql;
            synchronizeIrql = other.synchronizeIrql;
            mode = other.mode;
            shared = other.shared;
            isr = other.isr;
            serviceContext = other.serviceContext;
            interruptCount.store(other.interruptCount.load());
            nextShared = other.nextShared;
            connected = other.connected;
        }
        return *this;
    }
};

struct SystemInterruptRegistration {
    uint32_t vector{0};
    ke::KIRQL irql{ke::PASSIVE_LEVEL};
    InterruptMode mode{InterruptMode::Latched};
    bool enabled{false};
    std::function<void(uint32_t vector)> isrRoutine{nullptr};
};

class HardwareAbstractionLayer {
public:
    static HardwareAbstractionLayer& get() {
        static HardwareAbstractionLayer instance;
        return instance;
    }

    void initialize(uint32_t processorCount = 4, ProcessorArchitecture arch = ProcessorArchitecture::Amd64, uint32_t clockMhz = 3600) {
        processors_.clear();
        for (uint32_t i = 0; i < processorCount; ++i) {
            auto pc = std::make_unique<KernelProcessorControlRegion>();
            pc->self = pc.get();
            pc->prcb.cpuId = i;
            pc->prcb.architecture = arch;
            pc->prcb.coreClockMhz = clockMhz;
            pc->prcb.lapic = LocalApic(i);
            processors_.push_back(std::move(pc));
        }
        ioApic_.reset();
        registeredInterrupts_.clear();
        connectedInterrupts_.clear();
        hpet_.reset();
        telemetry_ = HAL_TELEMETRY{};
        bootTimestamp_ = std::chrono::steady_clock::now();
        simulatedRtc_ = TIME_FIELDS{2026, 10, 10, 14, 0, 0, 0, 6};
    }

    [[nodiscard]] uint32_t getProcessorCount() const noexcept {
        return static_cast<uint32_t>(processors_.size());
    }

    [[nodiscard]] KernelProcessorControlRegion* getKpcr(uint32_t cpuIndex = 0) const {
        if (cpuIndex >= processors_.size()) return nullptr;
        return processors_[cpuIndex].get();
    }

    [[nodiscard]] LocalApic* getLocalApic(uint32_t cpuIndex = 0) {
        auto* kpcr = getKpcr(cpuIndex);
        return kpcr ? &kpcr->prcb.lapic : nullptr;
    }

    [[nodiscard]] IoApic& getIoApic() noexcept {
        return ioApic_;
    }

    [[nodiscard]] HpetTimer& getHpetTimer() noexcept {
        return hpet_;
    }

    [[nodiscard]] const HpetTimer& getHpetTimer() const noexcept {
        return hpet_;
    }

    uint32_t stepHpet(uint64_t ticks = 1) {
        uint32_t triggered = hpet_.step(ticks);
        if (hpet_.isEnabled()) {
            telemetry_.totalHpetTicks += ticks;
            if (triggered > 0) {
                telemetry_.totalHardwareInterrupts += triggered;
            }
        }
        return triggered;
    }

    bool deliverMsi(const MsiMessage& msg) {
        auto* lapic = getLocalApic(msg.getTargetApicId());
        if (!lapic) {
            lapic = getLocalApic(0);
        }
        if (lapic) {
            lapic->requestInterrupt(msg.getVector());
            telemetry_.totalMsiDelivered++;
            telemetry_.totalHardwareInterrupts++;
            return true;
        }
        return false;
    }

    bool checkTscDeadline(uint32_t cpuIndex, uint64_t currentTsc) {
        auto* lapic = getLocalApic(cpuIndex);
        if (lapic && lapic->checkTscDeadline(currentTsc)) {
            telemetry_.totalTscDeadlineExpiries++;
            telemetry_.totalHardwareInterrupts++;
            return true;
        }
        return false;
    }

    void setCr8(uint8_t cr8, uint32_t cpuIndex = 0) {
        auto* kpcr = getKpcr(cpuIndex);
        if (kpcr) {
            uint8_t oldCr8 = kpcr->prcb.lapic.getCr8();
            kpcr->prcb.lapic.setCr8(cr8);
            if (cr8 > oldCr8) {
                telemetry_.totalIrqlRaises++;
            } else if (cr8 < oldCr8) {
                telemetry_.totalIrqlLowers++;
            }
            kpcr->currentIrql = static_cast<ke::KIRQL>(cr8);
        }
    }

    [[nodiscard]] uint8_t getCr8(uint32_t cpuIndex = 0) const {
        auto* kpcr = getKpcr(cpuIndex);
        return kpcr ? kpcr->prcb.lapic.getCr8() : 0;
    }

    bool connectInterrupt(KINTERRUPT* interrupt) {
        if (!interrupt || interrupt->vector == 0 || !interrupt->isr) return false;
        if (interrupt->connected) return false;

        auto it = connectedInterrupts_.find(interrupt->vector);
        if (it == connectedInterrupts_.end() || it->second == nullptr) {
            interrupt->nextShared = nullptr;
            interrupt->connected = true;
            connectedInterrupts_[interrupt->vector] = interrupt;
            return true;
        }

        if (!it->second->shared || !interrupt->shared) {
            return false;
        }

        KINTERRUPT* curr = it->second;
        while (curr->nextShared != nullptr) {
            if (curr == interrupt) return false;
            curr = curr->nextShared;
        }
        if (curr == interrupt) return false;
        curr->nextShared = interrupt;
        interrupt->nextShared = nullptr;
        interrupt->connected = true;
        return true;
    }

    bool disconnectInterrupt(KINTERRUPT* interrupt) {
        if (!interrupt || !interrupt->connected) return false;
        auto it = connectedInterrupts_.find(interrupt->vector);
        if (it == connectedInterrupts_.end() || it->second == nullptr) {
            interrupt->connected = false;
            return false;
        }

        if (it->second == interrupt) {
            it->second = interrupt->nextShared;
            if (it->second == nullptr) {
                connectedInterrupts_.erase(it);
            }
            interrupt->connected = false;
            interrupt->nextShared = nullptr;
            return true;
        }

        KINTERRUPT* prev = it->second;
        while (prev->nextShared != nullptr && prev->nextShared != interrupt) {
            prev = prev->nextShared;
        }

        if (prev->nextShared == interrupt) {
            prev->nextShared = interrupt->nextShared;
            interrupt->connected = false;
            interrupt->nextShared = nullptr;
            return true;
        }

        return false;
    }

    bool dispatchInterrupt(uint32_t vector) {
        auto it = connectedInterrupts_.find(vector);
        if (it == connectedInterrupts_.end() || it->second == nullptr) {
            telemetry_.totalSpuriousInterrupts++;
            return false;
        }

        KINTERRUPT* curr = it->second;
        bool handled = false;
        while (curr != nullptr) {
            telemetry_.totalHardwareInterrupts++;
            curr->interruptCount++;
            if (curr->isr && curr->isr(curr, curr->serviceContext)) {
                handled = true;
                break;
            }
            curr = curr->nextShared;
        }
        return handled;
    }

    void queryTelemetry(HAL_TELEMETRY* telem) const noexcept {
        if (telem) {
            *telem = telemetry_;
        }
    }

    void resetTelemetry() noexcept {
        telemetry_ = HAL_TELEMETRY{};
    }

    /**
     * @brief High-precision timer query (KeQueryPerformanceCounter).
     */
    void queryPerformanceCounter(LargeInteger& performanceCounter, LargeInteger* performanceFrequency = nullptr) const noexcept {
        auto now = std::chrono::steady_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - bootTimestamp_).count();
        performanceCounter.quadPart = nanos;
        if (performanceFrequency) {
            performanceFrequency->quadPart = 1'000'000'000; // 1 GHz nanosecond resolution
        }
    }

    /**
     * @brief Stalls CPU execution for specified microseconds (KeStallExecutionProcessor).
     */
    void stallExecutionProcessor(uint32_t microseconds) const noexcept {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= microseconds) break;
#if defined(_M_X64) || defined(__x86_64__)
            _mm_pause();
#endif
        }
    }

    /**
     * @brief Dispatches LAPIC periodic scheduling tick.
     */
    void dispatchClockTick(uint32_t cpuIndex = 0) {
        auto* kpcr = getKpcr(cpuIndex);
        if (kpcr) {
            kpcr->prcb.totalCycles += 10000;
            kpcr->prcb.interruptsServiced++;
            kpcr->prcb.lapic.signalEoi();
            telemetry_.totalHardwareInterrupts++;
        }
    }

    // System Interrupt Routing
    bool enableSystemInterrupt(uint32_t vector, ke::KIRQL irql, InterruptMode mode, std::function<void(uint32_t)> isr = nullptr) {
        SystemInterruptRegistration reg{
            .vector = vector,
            .irql = irql,
            .mode = mode,
            .enabled = true,
            .isrRoutine = std::move(isr)
        };
        registeredInterrupts_[vector] = reg;
        return true;
    }

    bool disableSystemInterrupt(uint32_t vector) {
        auto it = registeredInterrupts_.find(vector);
        if (it != registeredInterrupts_.end()) {
            it->second.enabled = false;
            return true;
        }
        return false;
    }

    bool beginSystemInterrupt(ke::KIRQL irql, uint32_t vector, ke::KIRQL* oldIrql) {
        if (!oldIrql) return false;
        *oldIrql = ke::KfRaiseIrql(irql);
        telemetry_.totalIrqlRaises++;
        telemetry_.totalHardwareInterrupts++;
        auto* kpcr = getKpcr(0);
        if (kpcr) {
            kpcr->currentIrql = irql;
            kpcr->prcb.interruptsServiced++;
            kpcr->prcb.lapic.markInterruptInService(static_cast<uint8_t>(vector));
        }
        return true;
    }

    void endSystemInterrupt(ke::KIRQL oldIrql, uint32_t vector) {
        auto* kpcr = getKpcr(0);
        if (kpcr) {
            kpcr->prcb.lapic.signalEoi();
            kpcr->currentIrql = oldIrql;
        }
        telemetry_.totalIrqlLowers++;
        ke::KeLowerIrql(oldIrql);
        (void)vector;
    }

    // Inter-Processor Interrupt (IPI) Subsystem
    bool requestIpi(uint32_t senderCpu, uint32_t targetCpu, IpiType type) {
        auto* sender = getKpcr(senderCpu);
        auto* target = getKpcr(targetCpu);
        if (!target) return false;

        if (sender) {
            sender->prcb.ipiSentCount++;
        }
        target->prcb.ipiReceivedCount++;
        target->prcb.lapic.requestInterrupt(static_cast<uint8_t>(type));
        telemetry_.totalIpisDispatched++;

        if (type == IpiType::FlushMultipleTb) {
            target->prcb.tlbFlushRequests++;
        }
        return true;
    }

    void requestIpiAllExceptSelf(uint32_t senderCpu, IpiType type) {
        for (uint32_t i = 0; i < processors_.size(); ++i) {
            if (i != senderCpu) {
                requestIpi(senderCpu, i, type);
            }
        }
    }

    void broadcastTlbFlush(uint32_t initiatorCpu) {
        requestIpiAllExceptSelf(initiatorCpu, IpiType::FlushMultipleTb);
    }

    // RTC
    void queryRealTimeClock(TIME_FIELDS* timeFields) const {
        if (timeFields) {
            *timeFields = simulatedRtc_;
            const_cast<HAL_TELEMETRY&>(telemetry_).totalRtcSyncs++;
        }
    }

    void setRealTimeClock(const TIME_FIELDS* timeFields) {
        if (timeFields) {
            simulatedRtc_ = *timeFields;
            telemetry_.totalRtcSyncs++;
        }
    }

    // DMA Adapter Factory
    std::unique_ptr<DmaAdapter> getAdapter(const DEVICE_DESCRIPTION* devDesc, uint32_t* numberOfMapRegisters = nullptr) {
        if (!devDesc) return nullptr;
        uint32_t mapRegs = (devDesc->maximumLength + 4095) / 4096;
        if (numberOfMapRegisters) {
            *numberOfMapRegisters = mapRegs;
        }
        return std::make_unique<DmaAdapter>(*devDesc, mapRegs);
    }

private:
    HardwareAbstractionLayer() { initialize(); }
    std::vector<std::unique_ptr<KernelProcessorControlRegion>> processors_;
    IoApic ioApic_{0};
    HpetTimer hpet_{};
    std::unordered_map<uint32_t, SystemInterruptRegistration> registeredInterrupts_;
    std::unordered_map<uint32_t, KINTERRUPT*> connectedInterrupts_;
    std::chrono::steady_clock::time_point bootTimestamp_;
    TIME_FIELDS simulatedRtc_{};
    HAL_TELEMETRY telemetry_{};
};

// ============================================================================
// 8. Standard NT HAL C/C++ API Helpers
// ============================================================================
inline void KeQueryPerformanceCounter(LargeInteger& counter, LargeInteger* frequency = nullptr) {
    HardwareAbstractionLayer::get().queryPerformanceCounter(counter, frequency);
}

inline void KeStallExecutionProcessor(uint32_t microseconds) {
    HardwareAbstractionLayer::get().stallExecutionProcessor(microseconds);
}

inline bool HalEnableSystemInterrupt(uint32_t vector, ke::KIRQL irql, InterruptMode mode) {
    return HardwareAbstractionLayer::get().enableSystemInterrupt(vector, irql, mode);
}

inline bool HalDisableSystemInterrupt(uint32_t vector) {
    return HardwareAbstractionLayer::get().disableSystemInterrupt(vector);
}

inline bool HalBeginSystemInterrupt(ke::KIRQL irql, uint32_t vector, ke::KIRQL* oldIrql) {
    return HardwareAbstractionLayer::get().beginSystemInterrupt(irql, vector, oldIrql);
}

inline void HalEndSystemInterrupt(ke::KIRQL oldIrql, uint32_t vector) {
    HardwareAbstractionLayer::get().endSystemInterrupt(oldIrql, vector);
}

inline bool HalRequestIpi(uint32_t targetCpu, IpiType type) {
    return HardwareAbstractionLayer::get().requestIpi(0, targetCpu, type);
}

inline void HalQueryRealTimeClock(TIME_FIELDS* timeFields) {
    HardwareAbstractionLayer::get().queryRealTimeClock(timeFields);
}

inline void HalSetRealTimeClock(const TIME_FIELDS* timeFields) {
    HardwareAbstractionLayer::get().setRealTimeClock(timeFields);
}

inline std::unique_ptr<DmaAdapter> HalGetAdapter(const DEVICE_DESCRIPTION* devDesc, uint32_t* mapRegisters = nullptr) {
    return HardwareAbstractionLayer::get().getAdapter(devDesc, mapRegisters);
}

inline HpetTimer& HalGetHpetTimer() noexcept {
    return HardwareAbstractionLayer::get().getHpetTimer();
}

inline bool HalConnectInterrupt(KINTERRUPT* interrupt) {
    return HardwareAbstractionLayer::get().connectInterrupt(interrupt);
}

inline bool HalDisconnectInterrupt(KINTERRUPT* interrupt) {
    return HardwareAbstractionLayer::get().disconnectInterrupt(interrupt);
}

inline bool HalDispatchInterrupt(uint32_t vector) {
    return HardwareAbstractionLayer::get().dispatchInterrupt(vector);
}

inline bool HalDeliverMsi(const MsiMessage& msg) {
    return HardwareAbstractionLayer::get().deliverMsi(msg);
}

inline bool HalCheckTscDeadline(uint32_t cpuIndex, uint64_t currentTsc) {
    return HardwareAbstractionLayer::get().checkTscDeadline(cpuIndex, currentTsc);
}

inline uint32_t HalStepHpet(uint64_t ticks = 1) {
    return HardwareAbstractionLayer::get().stepHpet(ticks);
}

inline bool HalTimeFieldsToSystemTime(const TIME_FIELDS* tf, LargeInteger* st) {
    return HalpTimeFieldsToSystemTime(tf, st);
}

inline bool HalSystemTimeToTimeFields(const LargeInteger* st, TIME_FIELDS* tf) {
    return HalpSystemTimeToTimeFields(st, tf);
}

inline void HalQueryTelemetry(HAL_TELEMETRY* telemetry) {
    HardwareAbstractionLayer::get().queryTelemetry(telemetry);
}

inline void HalResetTelemetryForTesting() {
    HardwareAbstractionLayer::get().resetTelemetry();
}

inline void HalSetCr8(uint8_t cr8, uint32_t cpuIndex = 0) {
    HardwareAbstractionLayer::get().setCr8(cr8, cpuIndex);
}

inline uint8_t HalGetCr8(uint32_t cpuIndex = 0) {
    return HardwareAbstractionLayer::get().getCr8(cpuIndex);
}

} // namespace micant::hal
