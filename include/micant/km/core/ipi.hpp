#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>
#include <functional>
#include <algorithm>
#include <iostream>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ke.hpp"

namespace micant::ke {

// ============================================================================
// 1. Processor Affinity Mask & Architecture Limits
// ============================================================================
using KAFFINITY = uintptr_t;
inline constexpr uint32_t MAXIMUM_PROCESSORS          = 64;
inline constexpr KAFFINITY AFFINITY_MASK_ALL          = ~static_cast<KAFFINITY>(0);

// ============================================================================
// 2. Inter-Processor Interrupt (IPI) Request Vectors & Flags
// ============================================================================
inline constexpr uint32_t IPI_APC                     = 0x0001; // Request target core deliver pending APCs
inline constexpr uint32_t IPI_DPC                     = 0x0002; // Request target core drain DPC list & dispatch
inline constexpr uint32_t IPI_FREEZE                  = 0x0004; // Freeze target core in atomic spinlock wait
inline constexpr uint32_t IPI_PACKET_READY            = 0x0008; // Generic synchronized broadcast call packet ready
inline constexpr uint32_t IPI_SYNCH_REQUEST           = 0x0010; // Synchronization barrier
inline constexpr uint32_t IPI_FLUSH_TLB               = 0x0020; // Invalidate translation buffer (TLB shootdown)

// ============================================================================
// 3. Multiprocessor Broadcast & Synchronization Structures
// ============================================================================
using PKIPI_BROADCAST_WORKER = uintptr_t (*)(uintptr_t context);

struct KIPI_BROADCAST_PACKET {
    PKIPI_BROADCAST_WORKER workerRoutine{nullptr};
    uintptr_t context{0};
    std::atomic<KAFFINITY> targetMask{0};
    std::atomic<KAFFINITY> acknowledgedMask{0};
    std::atomic<KAFFINITY> completedMask{0};
    std::array<std::atomic<uintptr_t>, MAXIMUM_PROCESSORS> perCpuResults{};
};

struct KIPI_TLB_FLUSH_PACKET {
    void* virtualAddress{nullptr};
    size_t numberOfPages{0}; // 0 = entire TB
    std::atomic<KAFFINITY> targetMask{0};
    std::atomic<KAFFINITY> acknowledgedMask{0};
};

struct IpiStatistics {
    uint64_t totalIpisSent{0};
    uint64_t totalIpisReceived{0};
    uint64_t totalGenericCalls{0};
    uint64_t totalFreezeCycles{0};
    uint64_t totalTlbFlushes{0};
};

// ============================================================================
// 4. Per-Processor IPI State Machine Structure
// ============================================================================
struct ProcessorIpiState {
    uint32_t processorId{0};
    std::atomic<uint32_t> pendingRequests{0};
    std::atomic<uint64_t> ipiSentCount{0};
    std::atomic<uint64_t> ipiReceivedCount{0};
    std::atomic<uint64_t> tlbFlushCount{0};
    std::atomic<bool> isFrozen{false};
    std::atomic<bool> online{true};
};

// ============================================================================
// 5. Sovereign Inter-Processor Interrupt (IPI) & SMP Manager Subsystem
// ============================================================================
class IpiManager {
public:
    static IpiManager& get() {
        static IpiManager instance;
        return instance;
    }

    void initialize(uint32_t processorCount = 4) {
        std::lock_guard<std::mutex> lock(mutex_);
        initializeLocked(processorCount);
    }

    void initializeLocked(uint32_t processorCount = 4) {
        if (processorCount == 0) processorCount = 1;
        if (processorCount > MAXIMUM_PROCESSORS) processorCount = MAXIMUM_PROCESSORS;

        activeProcessorCount_ = processorCount;
        activeAffinityMask_ = (processorCount == MAXIMUM_PROCESSORS) 
            ? AFFINITY_MASK_ALL 
            : ((static_cast<KAFFINITY>(1) << processorCount) - 1);

        for (uint32_t i = 0; i < MAXIMUM_PROCESSORS; ++i) {
            processors_[i].processorId = i;
            processors_[i].pendingRequests.store(0, std::memory_order_relaxed);
            processors_[i].ipiSentCount.store(0, std::memory_order_relaxed);
            processors_[i].ipiReceivedCount.store(0, std::memory_order_relaxed);
            processors_[i].tlbFlushCount.store(0, std::memory_order_relaxed);
            processors_[i].isFrozen.store(false, std::memory_order_relaxed);
            processors_[i].online.store(i < processorCount, std::memory_order_relaxed);
        }

        executionFrozen_.store(false, std::memory_order_relaxed);
        freezingProcessor_.store(0xFFFFFFFF, std::memory_order_relaxed);
    }

    [[nodiscard]] uint32_t getActiveProcessorCount() const noexcept {
        return activeProcessorCount_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] KAFFINITY getActiveProcessorsMask() const noexcept {
        return activeAffinityMask_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] bool isProcessorOnline(uint32_t cpu) const noexcept {
        if (cpu >= MAXIMUM_PROCESSORS) return false;
        return processors_[cpu].online.load(std::memory_order_relaxed);
    }

    // ------------------------------------------------------------------------
    // IPI Transmission & Packet Routing (KiIpiSend)
    // ------------------------------------------------------------------------
    bool sendIpi(uint32_t senderCpu, KAFFINITY targetAffinity, uint32_t ipiRequest) {
        KAFFINITY activeMask = activeAffinityMask_.load(std::memory_order_acquire);
        KAFFINITY validTargets = targetAffinity & activeMask;
        if (validTargets == 0) return false;

        uint32_t sentCount = 0;
        for (uint32_t i = 0; i < MAXIMUM_PROCESSORS; ++i) {
            if ((validTargets & (static_cast<KAFFINITY>(1) << i)) != 0) {
                processors_[i].pendingRequests.fetch_or(ipiRequest, std::memory_order_release);
                processors_[i].ipiReceivedCount.fetch_add(1, std::memory_order_relaxed);
                sentCount++;
            }
        }

        if (senderCpu < MAXIMUM_PROCESSORS) {
            processors_[senderCpu].ipiSentCount.fetch_add(sentCount, std::memory_order_relaxed);
        }
        totalIpisSent_.fetch_add(sentCount, std::memory_order_relaxed);
        return true;
    }

    // ------------------------------------------------------------------------
    // IPI Request Processing Epilogue (KiIpiProcessRequests)
    // ------------------------------------------------------------------------
    uint32_t processIncomingIpis(uint32_t targetCpu) {
        if (targetCpu >= MAXIMUM_PROCESSORS) return 0;
        uint32_t requests = processors_[targetCpu].pendingRequests.exchange(0, std::memory_order_acq_rel);
        if (requests == 0) return 0;

        // 1. Process Freeze Execution
        if (requests & IPI_FREEZE) {
            processors_[targetCpu].isFrozen.store(true, std::memory_order_release);
            while (executionFrozen_.load(std::memory_order_acquire)) {
#if defined(_M_X64) || defined(__x86_64__)
                _mm_pause();
#endif
            }
            processors_[targetCpu].isFrozen.store(false, std::memory_order_release);
        }

        // 2. Process Synchronized Generic Call Packet
        if (requests & IPI_PACKET_READY) {
            std::lock_guard<std::mutex> lock(packetMutex_);
            if (activeBroadcastPacket_) {
                KAFFINITY cpuMask = static_cast<KAFFINITY>(1) << targetCpu;
                activeBroadcastPacket_->acknowledgedMask.fetch_or(cpuMask, std::memory_order_acq_rel);

                if (activeBroadcastPacket_->workerRoutine) {
                    uintptr_t res = activeBroadcastPacket_->workerRoutine(activeBroadcastPacket_->context);
                    activeBroadcastPacket_->perCpuResults[targetCpu].store(res, std::memory_order_relaxed);
                }

                activeBroadcastPacket_->completedMask.fetch_or(cpuMask, std::memory_order_release);
            }
        }

        // 3. Process TLB Shootdown
        if (requests & IPI_FLUSH_TLB) {
            processors_[targetCpu].tlbFlushCount.fetch_add(1, std::memory_order_relaxed);
            totalTlbFlushes_.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(tlbMutex_);
            if (activeTlbPacket_) {
                KAFFINITY cpuMask = static_cast<KAFFINITY>(1) << targetCpu;
                activeTlbPacket_->acknowledgedMask.fetch_or(cpuMask, std::memory_order_release);
            }
        }

        // 4. Process DPC & Scheduler Preemption
        if (requests & IPI_DPC) {
            KiRetireDpcList();
        }

        // 5. Process APC Delivery
        if (requests & IPI_APC) {
            KiDeliverApc(0);
        }

        totalIpisReceived_.fetch_add(1, std::memory_order_relaxed);
        return requests;
    }

    // ------------------------------------------------------------------------
    // Synchronized Multiprocessor Generic Call (KeIpiGenericCall)
    // ------------------------------------------------------------------------
    uintptr_t genericCall(uint32_t senderCpu, PKIPI_BROADCAST_WORKER worker, uintptr_t context) {
        if (!worker) return 0;
        std::lock_guard<std::mutex> lock(genericCallMutex_);

        KAFFINITY activeMask = activeAffinityMask_.load(std::memory_order_acquire);
        KAFFINITY remoteMask = activeMask & ~(static_cast<KAFFINITY>(1) << senderCpu);

        KIPI_BROADCAST_PACKET packet;
        packet.workerRoutine = worker;
        packet.context = context;
        packet.targetMask.store(activeMask, std::memory_order_release);
        packet.acknowledgedMask.store(static_cast<KAFFINITY>(1) << senderCpu, std::memory_order_release);
        packet.completedMask.store(0, std::memory_order_release);

        {
            std::lock_guard<std::mutex> pLock(packetMutex_);
            activeBroadcastPacket_ = &packet;
        }

        // Send IPI_PACKET_READY to all other cores
        if (remoteMask != 0) {
            sendIpi(senderCpu, remoteMask, IPI_PACKET_READY);

            // Execute locally on caller core first
            uintptr_t localRes = worker(context);
            packet.perCpuResults[senderCpu].store(localRes, std::memory_order_relaxed);
            packet.completedMask.fetch_or(static_cast<KAFFINITY>(1) << senderCpu, std::memory_order_release);

            // Simulate remote cores handling the packet in current SMP simulation
            for (uint32_t i = 0; i < MAXIMUM_PROCESSORS; ++i) {
                if ((remoteMask & (static_cast<KAFFINITY>(1) << i)) != 0) {
                    processIncomingIpis(i);
                }
            }
        } else {
            // Uniprocessor path
            uintptr_t localRes = worker(context);
            packet.perCpuResults[senderCpu].store(localRes, std::memory_order_relaxed);
            packet.completedMask.store(activeMask, std::memory_order_release);
        }

        {
            std::lock_guard<std::mutex> pLock(packetMutex_);
            activeBroadcastPacket_ = nullptr;
        }

        totalGenericCalls_.fetch_add(1, std::memory_order_relaxed);
        return packet.perCpuResults[senderCpu].load(std::memory_order_relaxed);
    }

    // ------------------------------------------------------------------------
    // SMP Core Freeze & Thaw Architecture (KeFreezeExecution / KeThawExecution)
    // ------------------------------------------------------------------------
    bool freezeExecution(uint32_t senderCpu) {
        bool expected = false;
        if (!executionFrozen_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return false; // Already frozen by another processor
        }

        freezingProcessor_.store(senderCpu, std::memory_order_release);
        totalFreezeCycles_.fetch_add(1, std::memory_order_relaxed);

        KAFFINITY activeMask = activeAffinityMask_.load(std::memory_order_acquire);
        KAFFINITY remoteMask = activeMask & ~(static_cast<KAFFINITY>(1) << senderCpu);

        if (remoteMask != 0) {
            sendIpi(senderCpu, remoteMask, IPI_FREEZE);
            // In simulated SMP environment, mark remote cores as frozen
            for (uint32_t i = 0; i < MAXIMUM_PROCESSORS; ++i) {
                if ((remoteMask & (static_cast<KAFFINITY>(1) << i)) != 0) {
                    processors_[i].isFrozen.store(true, std::memory_order_release);
                }
            }
        }

        return true;
    }

    void thawExecution(uint32_t senderCpu) {
        if (!executionFrozen_.load(std::memory_order_acquire)) return;
        if (freezingProcessor_.load(std::memory_order_acquire) != senderCpu) return;

        for (uint32_t i = 0; i < MAXIMUM_PROCESSORS; ++i) {
            processors_[i].isFrozen.store(false, std::memory_order_release);
        }

        freezingProcessor_.store(0xFFFFFFFF, std::memory_order_release);
        executionFrozen_.store(false, std::memory_order_release);
    }

    [[nodiscard]] bool isExecutionFrozen() const noexcept {
        return executionFrozen_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool isProcessorFrozen(uint32_t cpu) const noexcept {
        if (cpu >= MAXIMUM_PROCESSORS) return false;
        return processors_[cpu].isFrozen.load(std::memory_order_acquire);
    }

    // ------------------------------------------------------------------------
    // SMP TLB Shootdown Engine (KeFlushEntireTb / KeFlushMultipleTb / KeFlushSingleTb)
    // ------------------------------------------------------------------------
    void flushEntireTb(uint32_t senderCpu, bool broadcast = true) {
        processors_[senderCpu].tlbFlushCount.fetch_add(1, std::memory_order_relaxed);
        totalTlbFlushes_.fetch_add(1, std::memory_order_relaxed);

        if (broadcast) {
            KAFFINITY activeMask = activeAffinityMask_.load(std::memory_order_acquire);
            KAFFINITY remoteMask = activeMask & ~(static_cast<KAFFINITY>(1) << senderCpu);

            if (remoteMask != 0) {
                KIPI_TLB_FLUSH_PACKET packet;
                packet.virtualAddress = nullptr;
                packet.numberOfPages = 0;
                packet.targetMask.store(remoteMask, std::memory_order_release);
                packet.acknowledgedMask.store(0, std::memory_order_release);

                {
                    std::lock_guard<std::mutex> lock(tlbMutex_);
                    activeTlbPacket_ = &packet;
                }

                sendIpi(senderCpu, remoteMask, IPI_FLUSH_TLB);
                for (uint32_t i = 0; i < MAXIMUM_PROCESSORS; ++i) {
                    if ((remoteMask & (static_cast<KAFFINITY>(1) << i)) != 0) {
                        processIncomingIpis(i);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(tlbMutex_);
                    activeTlbPacket_ = nullptr;
                }
            }
        }
    }

    void flushSingleTb(uint32_t senderCpu, void* va, bool broadcast = true) {
        processors_[senderCpu].tlbFlushCount.fetch_add(1, std::memory_order_relaxed);
        totalTlbFlushes_.fetch_add(1, std::memory_order_relaxed);

        if (broadcast) {
            KAFFINITY activeMask = activeAffinityMask_.load(std::memory_order_acquire);
            KAFFINITY remoteMask = activeMask & ~(static_cast<KAFFINITY>(1) << senderCpu);

            if (remoteMask != 0) {
                KIPI_TLB_FLUSH_PACKET packet;
                packet.virtualAddress = va;
                packet.numberOfPages = 1;
                packet.targetMask.store(remoteMask, std::memory_order_release);
                packet.acknowledgedMask.store(0, std::memory_order_release);

                {
                    std::lock_guard<std::mutex> lock(tlbMutex_);
                    activeTlbPacket_ = &packet;
                }

                sendIpi(senderCpu, remoteMask, IPI_FLUSH_TLB);
                for (uint32_t i = 0; i < MAXIMUM_PROCESSORS; ++i) {
                    if ((remoteMask & (static_cast<KAFFINITY>(1) << i)) != 0) {
                        processIncomingIpis(i);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(tlbMutex_);
                    activeTlbPacket_ = nullptr;
                }
            }
        }
    }

    void flushMultipleTb(uint32_t senderCpu, size_t numberOfPages, void** vaList, bool broadcast = true) {
        (void)vaList;
        processors_[senderCpu].tlbFlushCount.fetch_add(1, std::memory_order_relaxed);
        totalTlbFlushes_.fetch_add(1, std::memory_order_relaxed);

        if (broadcast) {
            KAFFINITY activeMask = activeAffinityMask_.load(std::memory_order_acquire);
            KAFFINITY remoteMask = activeMask & ~(static_cast<KAFFINITY>(1) << senderCpu);

            if (remoteMask != 0) {
                KIPI_TLB_FLUSH_PACKET packet;
                packet.virtualAddress = nullptr;
                packet.numberOfPages = numberOfPages;
                packet.targetMask.store(remoteMask, std::memory_order_release);
                packet.acknowledgedMask.store(0, std::memory_order_release);

                {
                    std::lock_guard<std::mutex> lock(tlbMutex_);
                    activeTlbPacket_ = &packet;
                }

                sendIpi(senderCpu, remoteMask, IPI_FLUSH_TLB);
                for (uint32_t i = 0; i < MAXIMUM_PROCESSORS; ++i) {
                    if ((remoteMask & (static_cast<KAFFINITY>(1) << i)) != 0) {
                        processIncomingIpis(i);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(tlbMutex_);
                    activeTlbPacket_ = nullptr;
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // Telemetry & Diagnostics
    // ------------------------------------------------------------------------
    [[nodiscard]] IpiStatistics getStatistics() const noexcept {
        IpiStatistics stats{};
        stats.totalIpisSent = totalIpisSent_.load(std::memory_order_relaxed);
        stats.totalIpisReceived = totalIpisReceived_.load(std::memory_order_relaxed);
        stats.totalGenericCalls = totalGenericCalls_.load(std::memory_order_relaxed);
        stats.totalFreezeCycles = totalFreezeCycles_.load(std::memory_order_relaxed);
        stats.totalTlbFlushes = totalTlbFlushes_.load(std::memory_order_relaxed);
        return stats;
    }

    [[nodiscard]] const ProcessorIpiState& getProcessorState(uint32_t cpu) const {
        static ProcessorIpiState dummy{};
        if (cpu >= MAXIMUM_PROCESSORS) return dummy;
        return processors_[cpu];
    }

    void resetForTesting() {
        std::lock_guard<std::mutex> lock(mutex_);
        initializeLocked(4);
        totalIpisSent_.store(0, std::memory_order_relaxed);
        totalIpisReceived_.store(0, std::memory_order_relaxed);
        totalGenericCalls_.store(0, std::memory_order_relaxed);
        totalFreezeCycles_.store(0, std::memory_order_relaxed);
        totalTlbFlushes_.store(0, std::memory_order_relaxed);
    }

private:
    IpiManager() {
        initializeLocked(4);
    }

    mutable std::mutex mutex_;
    std::mutex genericCallMutex_;
    std::mutex packetMutex_;
    std::mutex tlbMutex_;

    std::atomic<uint32_t> activeProcessorCount_{4};
    std::atomic<KAFFINITY> activeAffinityMask_{0x0F};
    std::array<ProcessorIpiState, MAXIMUM_PROCESSORS> processors_{};

    std::atomic<bool> executionFrozen_{false};
    std::atomic<uint32_t> freezingProcessor_{0xFFFFFFFF};

    KIPI_BROADCAST_PACKET* activeBroadcastPacket_{nullptr};
    KIPI_TLB_FLUSH_PACKET* activeTlbPacket_{nullptr};

    std::atomic<uint64_t> totalIpisSent_{0};
    std::atomic<uint64_t> totalIpisReceived_{0};
    std::atomic<uint64_t> totalGenericCalls_{0};
    std::atomic<uint64_t> totalFreezeCycles_{0};
    std::atomic<uint64_t> totalTlbFlushes_{0};
};

// ============================================================================
// 6. Standard Windows NT Kernel SMP / IPI DDK Exports
// ============================================================================
inline KAFFINITY KeQueryActiveProcessors() {
    return IpiManager::get().getActiveProcessorsMask();
}

inline uint32_t KeQueryActiveProcessorCount(KAFFINITY* activeProcessors = nullptr) {
    if (activeProcessors) {
        *activeProcessors = IpiManager::get().getActiveProcessorsMask();
    }
    return IpiManager::get().getActiveProcessorCount();
}

inline void KiIpiSend(KAFFINITY targetProcessors, uint32_t ipiRequest) {
    IpiManager::get().sendIpi(KeGetCurrentProcessorNumber(), targetProcessors, ipiRequest);
}

inline void KiIpiProcessRequests() {
    IpiManager::get().processIncomingIpis(KeGetCurrentProcessorNumber());
}

inline uintptr_t KeIpiGenericCall(PKIPI_BROADCAST_WORKER broadcastFunction, uintptr_t context) {
    return IpiManager::get().genericCall(KeGetCurrentProcessorNumber(), broadcastFunction, context);
}

inline bool KeFreezeExecution(KTrapFrame* /*trapFrame*/ = nullptr, void* /*exceptionRecord*/ = nullptr) {
    return IpiManager::get().freezeExecution(KeGetCurrentProcessorNumber());
}

inline void KeThawExecution(bool /*previousState*/ = true) {
    IpiManager::get().thawExecution(KeGetCurrentProcessorNumber());
}

inline void KeFlushEntireTb(bool invalidateAllCores = true) {
    IpiManager::get().flushEntireTb(KeGetCurrentProcessorNumber(), invalidateAllCores);
}

inline void KeFlushSingleTb(void* virtualAddress, bool invalidateAllCores = true) {
    IpiManager::get().flushSingleTb(KeGetCurrentProcessorNumber(), virtualAddress, invalidateAllCores);
}

inline void KeFlushMultipleTb(size_t numberOfPages, void** virtualAddresses, bool invalidateAllCores = true) {
    IpiManager::get().flushMultipleTb(KeGetCurrentProcessorNumber(), numberOfPages, virtualAddresses, invalidateAllCores);
}

} // namespace micant::ke
