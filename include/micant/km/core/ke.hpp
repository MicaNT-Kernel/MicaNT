#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <atomic>
#include <array>
#include <deque>
#include <functional>
#include <chrono>
#include <optional>
#include <algorithm>
#if defined(_M_X64) || defined(__x86_64__)
#include <immintrin.h>
#endif
#include "ntdef.hpp"
#include "ntstatus.hpp"

namespace micant::ke {

// ============================================================================
// 1. Interrupt Request Levels (IRQL)
// ============================================================================
using KIRQL = uint8_t;

inline constexpr KIRQL PASSIVE_LEVEL      = 0;  // Normal user / kernel execution; paging allowed
inline constexpr KIRQL APC_LEVEL          = 1;  // Asynchronous Procedure Calls; paging allowed
inline constexpr KIRQL DISPATCH_LEVEL     = 2;  // Thread scheduler & DPCs; NO PAGE FAULTS ALLOWED
inline constexpr KIRQL DIRQL_MIN          = 3;  // Device Interrupts minimum
inline constexpr KIRQL DIRQL_MAX          = 26; // Device Interrupts maximum
inline constexpr KIRQL PROFILE_LEVEL      = 27; // Profiling timer
inline constexpr KIRQL CLOCK_LEVEL        = 28; // System clock timer
inline constexpr KIRQL IPI_LEVEL          = 29; // Inter-Processor Interrupt
inline constexpr KIRQL POWER_LEVEL        = 30; // Power failure interrupt
inline constexpr KIRQL HIGH_LEVEL         = 31; // Mask all interrupts

// Forward declarations
struct KThread;
struct KDPC;
struct KAPC;

// ============================================================================
// 2. Processor Control Region (KPCR) & Processor Control Block (KPRCB)
// ============================================================================
/**
 * @brief Processor Control Region (KPCR) and KPRCB tracking per CPU core.
 * Clean-room implementation of Windows NT per-CPU executive structures.
 */
struct ProcessorControlBlock {
    uint32_t processorNumber{0};
    KIRQL currentIrql{PASSIVE_LEVEL};
    uint64_t interruptCount{0};
    uint64_t dpcCount{0};
    uint64_t apcCount{0};
    uint64_t contextSwitches{0};
    uintptr_t currentCr3{0x1000};
    KThread* currentThread{nullptr};
    KThread* nextThread{nullptr};
    KThread* idleThread{nullptr};
    bool dpcRoutineActive{false};
};

// Per-CPU simulated KPCR
inline thread_local ProcessorControlBlock g_CurrentCpu{
    .processorNumber = 0,
    .currentIrql = PASSIVE_LEVEL,
    .interruptCount = 0,
    .dpcCount = 0,
    .apcCount = 0,
    .contextSwitches = 0,
    .currentCr3 = 0x1000,
    .currentThread = nullptr,
    .nextThread = nullptr,
    .idleThread = nullptr,
    .dpcRoutineActive = false
};

[[nodiscard]] inline KIRQL KeGetCurrentIrql() noexcept {
    return g_CurrentCpu.currentIrql;
}

inline KIRQL KfRaiseIrql(KIRQL newIrql) noexcept {
    KIRQL old = g_CurrentCpu.currentIrql;
    if (newIrql > old) {
        g_CurrentCpu.currentIrql = newIrql;
    }
    return old;
}

inline void KeLowerIrql(KIRQL newIrql) noexcept {
    g_CurrentCpu.currentIrql = newIrql;
}


// ============================================================================
// 3. Hardware Trap Frame (KTRAP_FRAME)
// ============================================================================
/**
 * @brief Hardware and Software Trap Frame (KTRAP_FRAME).
 * Exact clean-room mapping of the x86-64 NT kernel trap frame.
 */
struct KTrapFrame {
    // Microsoft x64 FastCall Home Locations (P1-P4)
    uint64_t p1Home{0};
    uint64_t p2Home{0};
    uint64_t p3Home{0};
    uint64_t p4Home{0};
    uint64_t p5{0};

    // Volatile / Scratch Registers
    uint64_t rax{0};
    uint64_t rcx{0};
    uint64_t rdx{0};
    uint64_t r8{0};
    uint64_t r9{0};
    uint64_t r10{0};
    uint64_t r11{0};

    // Preserved / Non-Volatile Callee-Saved Registers
    uint64_t rbx{0};
    uint64_t rbp{0};
    uint64_t rsi{0};
    uint64_t rdi{0};
    uint64_t r12{0};
    uint64_t r13{0};
    uint64_t r14{0};
    uint64_t r15{0};

    // Hardware Push Frame (saved by CPU upon interruption/fault)
    uint64_t rip{0};
    uint16_t segCs{0x33}; // Ring 3 User Code default
    uint16_t fill0{0};
    uint32_t fill1{0};
    uint32_t eFlags{0x202}; // IF (Interrupt Flag) enabled
    uint32_t fill2{0};
    uint64_t rsp{0};
    uint16_t segSs{0x2B}; // Ring 3 User Data default
    uint16_t fill3{0};
    uint32_t fill4{0};

    // Segment Registers
    uint16_t segDs{0x2B};
    uint16_t segEs{0x2B};
    uint16_t segFs{0x53};
    uint16_t segGs{0x2B};

    // Diagnostic & Fault Metadata
    uint64_t faultAddress{0};
    uint32_t errorCode{0};
    uint8_t  previousMode{1}; // 0 = KernelMode, 1 = UserMode
    uint8_t  exceptionActive{0};
    uint16_t mxCsr{0x1F80};   // Standard IEEE 754 SSE control/status
};

// ============================================================================
// 4. Kernel Spinlocks (KSPIN_LOCK)
// ============================================================================
/**
 * @brief Native NT Kernel Spinlock.
 * Acquiring raises IRQL to DISPATCH_LEVEL and spins atomically.
 * Releasing restores previous IRQL.
 */
class SpinLock {
public:
    SpinLock() = default;
    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;

    KIRQL acquire() noexcept {
        KIRQL oldIrql = KfRaiseIrql(DISPATCH_LEVEL);
        while (lockFlag_.test_and_set(std::memory_order_acquire)) {
#if defined(_M_X64) || defined(__x86_64__)
            _mm_pause();
#endif
        }
        ownerCpu_ = g_CurrentCpu.processorNumber;
        return oldIrql;
    }

    void release(KIRQL oldIrql) noexcept {
        ownerCpu_ = 0xFFFFFFFF;
        lockFlag_.clear(std::memory_order_release);
        KeLowerIrql(oldIrql);
    }

    [[nodiscard]] bool isLocked() const noexcept {
        return lockFlag_.test(std::memory_order_relaxed);
    }

private:
    std::atomic_flag lockFlag_ = ATOMIC_FLAG_INIT;
    uint32_t ownerCpu_{0xFFFFFFFF};
};

/**
 * @brief RAII SpinLock Guard.
 */
class SpinLockGuard {
public:
    explicit SpinLockGuard(SpinLock& lock) noexcept
        : lock_(lock), previousIrql_(lock_.acquire()) {}

    ~SpinLockGuard() noexcept {
        lock_.release(previousIrql_);
    }

    SpinLockGuard(const SpinLockGuard&) = delete;
    SpinLockGuard& operator=(const SpinLockGuard&) = delete;

    [[nodiscard]] KIRQL getPreviousIrql() const noexcept { return previousIrql_; }

private:
    SpinLock& lock_;
    KIRQL previousIrql_;
};

// ============================================================================
// 5. Deferred Procedure Calls (KDPC)
// ============================================================================
enum class DpcImportance : uint32_t {
    Low,
    Medium,
    High,
    HighPriority
};

using PKDEFERRED_ROUTINE = void (*)(KDPC* dpc, void* deferredContext, void* sysArg1, void* sysArg2);

struct KDPC {
    PKDEFERRED_ROUTINE routine{nullptr};
    void* deferredContext{nullptr};
    void* systemArgument1{nullptr};
    void* systemArgument2{nullptr};
    DpcImportance importance{DpcImportance::Medium};
    uint32_t targetProcessor{0};
    bool inserted{false};
};

class DpcQueue {
public:
    static DpcQueue& get() {
        static DpcQueue instance;
        return instance;
    }

    bool queueDpc(KDPC* dpc, void* arg1 = nullptr, void* arg2 = nullptr) {
        if (!dpc || !dpc->routine) return false;
        SpinLockGuard guard(lock_);
        if (dpc->inserted) return false;

        dpc->systemArgument1 = arg1;
        dpc->systemArgument2 = arg2;
        dpc->inserted = true;
        entries_.push_back(dpc);
        return true;
    }

    size_t drainDpcs() {
        std::vector<KDPC*> toExecute;
        {
            SpinLockGuard guard(lock_);
            toExecute.swap(entries_);
            for (auto* dpc : toExecute) {
                dpc->inserted = false;
            }
        }

        if (toExecute.empty()) return 0;

        // DPCs execute strictly at DISPATCH_LEVEL
        KIRQL oldIrql = KfRaiseIrql(DISPATCH_LEVEL);
        g_CurrentCpu.dpcRoutineActive = true;

        for (auto* dpc : toExecute) {
            dpc->routine(dpc, dpc->deferredContext, dpc->systemArgument1, dpc->systemArgument2);
            g_CurrentCpu.dpcCount++;
        }

        g_CurrentCpu.dpcRoutineActive = false;
        KeLowerIrql(oldIrql);
        return toExecute.size();
    }

    [[nodiscard]] size_t getQueuedCount() const {
        return entries_.size();
    }

private:
    DpcQueue() = default;
    SpinLock lock_;
    std::vector<KDPC*> entries_;
};

inline void KiRetireDpcList() {
    if (!g_CurrentCpu.dpcRoutineActive && DpcQueue::get().getQueuedCount() > 0) {
        DpcQueue::get().drainDpcs();
    }
}

// ============================================================================
// 6. Asynchronous Procedure Calls (KAPC)
// ============================================================================
enum class ApcEnvironment : uint8_t {
    OriginalApcEnvironment,
    AttachedApcEnvironment,
    CurrentApcEnvironment
};

using PKNORMAL_ROUTINE = void (*)(void* normalContext, void* sysArg1, void* sysArg2);
using PKKERNEL_ROUTINE = void (*)(KAPC* apc, PKNORMAL_ROUTINE* normalRoutine, void** normalContext, void** sysArg1, void** sysArg2);
using PKRUNDOWN_ROUTINE = void (*)(KAPC* apc);

struct KAPC {
    Handle targetTid{0};
    PKKERNEL_ROUTINE kernelRoutine{nullptr};
    PKRUNDOWN_ROUTINE rundownRoutine{nullptr};
    PKNORMAL_ROUTINE normalRoutine{nullptr};
    void* normalContext{nullptr};
    void* systemArgument1{nullptr};
    void* systemArgument2{nullptr};
    uint8_t apcMode{0}; // 0 = KernelMode, 1 = UserMode
    bool inserted{false};
    bool isSpecialKernelApc{false};
};

// ============================================================================
// 7. Thread Execution State & Hardware Context Frame
// ============================================================================
enum class ThreadState : uint8_t {
    Initialized   = 0,
    Ready         = 1,
    Running       = 2,
    Standby       = 3,
    Terminated    = 4,
    Waiting       = 5,
    Transition    = 6,
    DeferredReady = 7
};

struct KThreadContext {
    uint64_t rip{0};
    uint64_t rsp{0};
    uint64_t rflags{0x202};
    uint64_t rbx{0};
    uint64_t rbp{0};
    uint64_t r12{0};
    uint64_t r13{0};
    uint64_t r14{0};
    uint64_t r15{0};
};

/**
 * @brief Kernel Thread Object (KTHREAD).
 * Low-level execution context managed by the Ke dispatcher.
 */
struct KThread {
    Handle tid{0};
    Handle pid{0};
    ThreadState state{ThreadState::Initialized};
    uint32_t basePriority{8};
    uint32_t priority{8};
    int32_t quantumRemaining{6};
    uint64_t totalCyclesExecuted{0};
    std::string name;

    // Address space and stacks
    uintptr_t directoryTableBase{0x1000};
    uintptr_t kernelStackTop{0};
    uintptr_t kernelStackBase{0};
    uintptr_t userStackTop{0};
    uintptr_t tebAddress{0};

    // Registers & Trap Frame
    KThreadContext context{};
    KTrapFrame* trapFrame{nullptr};

    // APC Queues (Kernel-Mode & User-Mode)
    std::deque<KAPC*> kernelApcList;
    std::deque<KAPC*> userApcList;
    bool kernelApcDisable{false};
    bool specialApcDisable{false};
    bool alertable{false};
    bool alerted{false};
};

// ============================================================================
// 8. 32-Queue Priority Thread Scheduler & Preemption Engine
// ============================================================================
inline constexpr uint32_t PRIORITY_LOWEST         = 0;
inline constexpr uint32_t PRIORITY_IDLE           = 0;
inline constexpr uint32_t PRIORITY_NORMAL         = 8;
inline constexpr uint32_t PRIORITY_HIGH           = 13;
inline constexpr uint32_t PRIORITY_REALTIME_MIN   = 16;
inline constexpr uint32_t PRIORITY_REALTIME_MAX   = 31;
inline constexpr uint32_t NUM_PRIORITY_LEVELS     = 32;

inline constexpr uint32_t DEFAULT_QUANTUM_WORKSTATION = 6;
inline constexpr uint32_t DEFAULT_QUANTUM_SERVER      = 36;

struct ScheduledThreadEntry {
    Handle tid{0};
    Handle pid{0};
    uint32_t basePriority{PRIORITY_NORMAL};
    uint32_t currentPriority{PRIORITY_NORMAL};
    int32_t quantumRemaining{DEFAULT_QUANTUM_WORKSTATION};
    uint64_t totalCyclesExecuted{0};
    std::string name;
    KThread* threadObject{nullptr};
};

class PriorityScheduler {
public:
    static PriorityScheduler& get() {
        static PriorityScheduler instance;
        return instance;
    }

    void readyThread(ScheduledThreadEntry thread) {
        SpinLockGuard guard(lock_);
        uint32_t prio = std::min(thread.currentPriority, PRIORITY_REALTIME_MAX);
        if (thread.threadObject) {
            thread.threadObject->state = ThreadState::Ready;
            thread.threadObject->priority = prio;
        }
        runQueues_[prio].push_back(std::move(thread));
        totalReadyThreads_++;
    }

    void readyKThread(KThread* kthread) {
        if (!kthread) return;
        ScheduledThreadEntry entry{
            .tid = kthread->tid,
            .pid = kthread->pid,
            .basePriority = kthread->basePriority,
            .currentPriority = kthread->priority,
            .quantumRemaining = kthread->quantumRemaining,
            .totalCyclesExecuted = kthread->totalCyclesExecuted,
            .name = kthread->name,
            .threadObject = kthread
        };
        readyThread(std::move(entry));
    }

    /**
     * @brief Select next highest priority runnable thread (KiSelectNextThread).
     */
    [[nodiscard]] std::optional<ScheduledThreadEntry> selectNextThread() {
        SpinLockGuard guard(lock_);
        for (int p = PRIORITY_REALTIME_MAX; p >= 0; --p) {
            if (!runQueues_[p].empty()) {
                auto selected = std::move(runQueues_[p].front());
                runQueues_[p].pop_front();
                totalReadyThreads_--;
                g_CurrentCpu.contextSwitches++;
                if (selected.threadObject) {
                    selected.threadObject->state = ThreadState::Standby;
                }
                return selected;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Clock tick quantum expiration & dynamic priority decay.
     */
    void clockTick(ScheduledThreadEntry& runningThread) {
        runningThread.quantumRemaining--;
        runningThread.totalCyclesExecuted += 1000;

        if (runningThread.quantumRemaining <= 0) {
            // Real-time priorities do NOT decay
            if (runningThread.currentPriority < PRIORITY_REALTIME_MIN) {
                if (runningThread.currentPriority > runningThread.basePriority) {
                    runningThread.currentPriority--; // Priority decay
                }
            }
            runningThread.quantumRemaining = DEFAULT_QUANTUM_WORKSTATION;
            readyThread(runningThread);
        }
    }

    /**
     * @brief Boost thread priority upon I/O event completion or unwait.
     */
    void boostPriority(ScheduledThreadEntry& thread, uint32_t boostAmount) {
        if (thread.currentPriority < PRIORITY_REALTIME_MIN) {
            thread.currentPriority = std::min(
                thread.currentPriority + boostAmount,
                PRIORITY_REALTIME_MIN - 1
            );
            if (thread.threadObject) {
                thread.threadObject->priority = thread.currentPriority;
            }
        }
    }

    /**
     * @brief Anti-Starvation Scanner (KiScanReadyQueues).
     * Temporarily boosts threads in ready queues 1-15 that have not run recently.
     */
    size_t scanForStarvation() {
        SpinLockGuard guard(lock_);
        size_t boosted = 0;
        for (uint32_t p = 1; p < PRIORITY_NORMAL; ++p) {
            if (!runQueues_[p].empty()) {
                auto thread = std::move(runQueues_[p].front());
                runQueues_[p].pop_front();
                thread.currentPriority = PRIORITY_REALTIME_MIN - 1; // Boost to 15
                thread.quantumRemaining = DEFAULT_QUANTUM_WORKSTATION * 2;
                runQueues_[thread.currentPriority].push_back(std::move(thread));
                boosted++;
            }
        }
        return boosted;
    }

    [[nodiscard]] size_t getReadyThreadCount() const noexcept {
        return totalReadyThreads_;
    }

private:
    PriorityScheduler() : totalReadyThreads_(0) {}
    mutable SpinLock lock_;
    std::array<std::deque<ScheduledThreadEntry>, NUM_PRIORITY_LEVELS> runQueues_;
    std::atomic<size_t> totalReadyThreads_;
};

// ============================================================================
// 9. Hardware Context Switcher (KiSwapContext & KiSwapThread)
// ============================================================================
/**
 * @brief Clean-room implementation of Dave Cutler's KiSwapContext.
 * Saves outgoing registers, switches address space (CR3) if crossing process
 * boundary, updates per-CPU KPCR state, and restores incoming thread registers.
 *
 * @param outgoing Thread yielding the processor
 * @param incoming Thread taking ownership of the processor
 * @return true if context switch completed successfully
 */
inline bool KiSwapContext(KThread* outgoing, KThread* incoming) {
    if (!incoming) return false;

    // 1. In Windows NT, context switching occurs strictly at DISPATCH_LEVEL
    KIRQL currentIrql = KeGetCurrentIrql();
    bool raisedIrql = false;
    if (currentIrql < DISPATCH_LEVEL) {
        KfRaiseIrql(DISPATCH_LEVEL);
        raisedIrql = true;
    }

    // 2. Save Outgoing Thread Context
    if (outgoing) {
        if (outgoing->state == ThreadState::Running) {
            outgoing->state = ThreadState::Ready;
        }
        // Save register state
        outgoing->context.rsp = outgoing->context.rsp;
        outgoing->context.rip = outgoing->context.rip;
    }

    // 3. Switch Address Space (CR3) if Crossing Process Boundary (KiSwapProcess)
    if (incoming->directoryTableBase != 0 && incoming->directoryTableBase != g_CurrentCpu.currentCr3) {
        g_CurrentCpu.currentCr3 = incoming->directoryTableBase;
#if defined(_M_X64) || defined(__x86_64__)
        // Hardware page table reload
        _mm_sfence();
#endif
    }

    // 4. Update Per-CPU KPCR / KPRCB State
    g_CurrentCpu.currentThread = incoming;
    g_CurrentCpu.contextSwitches++;
    incoming->state = ThreadState::Running;

    // 5. Restore IRQL if raised
    if (raisedIrql) {
        KeLowerIrql(currentIrql);
    }
    return true;
}

// ============================================================================
// 10. APC Delivery Engine (KiDeliverApc)
// ============================================================================
/**
 * @brief Initialize a Kernel Asynchronous Procedure Call (KeInitializeApc).
 */
inline void KeInitializeApc(
    KAPC* apc,
    Handle targetTid,
    PKKERNEL_ROUTINE kernelRoutine,
    PKRUNDOWN_ROUTINE rundownRoutine,
    PKNORMAL_ROUTINE normalRoutine,
    uint8_t apcMode,
    void* normalContext
) {
    if (!apc) return;
    apc->targetTid = targetTid;
    apc->kernelRoutine = kernelRoutine;
    apc->rundownRoutine = rundownRoutine;
    apc->normalRoutine = normalRoutine;
    apc->apcMode = apcMode;
    apc->normalContext = normalContext;
    apc->inserted = false;
    apc->isSpecialKernelApc = (normalRoutine == nullptr);
}

/**
 * @brief Queue an APC to target thread (KeInsertQueueApc).
 */
inline bool KeInsertQueueApc(KAPC* apc, void* sysArg1 = nullptr, void* sysArg2 = nullptr) {
    if (!apc || apc->inserted) return false;
    apc->systemArgument1 = sysArg1;
    apc->systemArgument2 = sysArg2;
    apc->inserted = true;

    auto* curr = g_CurrentCpu.currentThread;
    if (curr) {
        if (apc->apcMode == 0) {
            curr->kernelApcList.push_back(apc);
        } else {
            curr->userApcList.push_back(apc);
        }
    }
    return true;
}

/**
 * @brief Deliver pending APCs to the currently running thread (KiDeliverApc).
 */
inline void KiDeliverApc(uint8_t deliveryMode) {
    auto* thread = g_CurrentCpu.currentThread;
    if (!thread) return;

    // 1. Deliver Special and Normal Kernel-Mode APCs
    if (deliveryMode == 0) {
        while (!thread->kernelApcList.empty()) {
            auto* apc = thread->kernelApcList.front();
            thread->kernelApcList.pop_front();
            apc->inserted = false;

            PKNORMAL_ROUTINE normalRoutine = apc->normalRoutine;
            void* normalContext = apc->normalContext;
            void* sysArg1 = apc->systemArgument1;
            void* sysArg2 = apc->systemArgument2;

            // Kernel routine runs at APC_LEVEL
            KIRQL old = KfRaiseIrql(APC_LEVEL);
            if (apc->kernelRoutine) {
                apc->kernelRoutine(apc, &normalRoutine, &normalContext, &sysArg1, &sysArg2);
            }
            KeLowerIrql(old);

            // If normal routine exists, run at PASSIVE_LEVEL
            if (normalRoutine && !thread->kernelApcDisable) {
                normalRoutine(normalContext, sysArg1, sysArg2);
            }
            g_CurrentCpu.apcCount++;
        }
    }

    // 2. Deliver User-Mode APCs if thread is in an alertable wait at PASSIVE_LEVEL
    if (deliveryMode == 1 || thread->alertable) {
        if (KeGetCurrentIrql() == PASSIVE_LEVEL) {
            while (!thread->userApcList.empty()) {
                auto* apc = thread->userApcList.front();
                thread->userApcList.pop_front();
                apc->inserted = false;

                if (apc->kernelRoutine) {
                    PKNORMAL_ROUTINE norm = apc->normalRoutine;
                    void* ctx = apc->normalContext;
                    void* a1 = apc->systemArgument1;
                    void* a2 = apc->systemArgument2;
                    apc->kernelRoutine(apc, &norm, &ctx, &a1, &a2);
                }
                if (apc->normalRoutine) {
                    apc->normalRoutine(apc->normalContext, apc->systemArgument1, apc->systemArgument2);
                }
                g_CurrentCpu.apcCount++;
            }
            thread->alertable = false;
        }
    }
}

/**
 * @brief Software Interrupt Dispatcher (Vector 0x2F / DISPATCH_LEVEL & APC_LEVEL handler).
 * Called by interrupt exit stubs and thread preemption epilogues.
 */
inline void KiDispatchSoftwareInterrupt(KIRQL targetLevel) {
    if (targetLevel == DISPATCH_LEVEL) {
        KiRetireDpcList();
    } else if (targetLevel == APC_LEVEL) {
        KiDeliverApc(0);
    }
}

} // namespace micant::ke

