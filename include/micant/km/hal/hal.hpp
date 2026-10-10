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
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ke.hpp"

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
        }
    }

    void setRealTimeClock(const TIME_FIELDS* timeFields) {
        if (timeFields) {
            simulatedRtc_ = *timeFields;
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
    std::unordered_map<uint32_t, SystemInterruptRegistration> registeredInterrupts_;
    std::chrono::steady_clock::time_point bootTimestamp_;
    TIME_FIELDS simulatedRtc_{};
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

} // namespace micant::hal
