#include "micant/km/executive/ps.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/core/sync.hpp"
#include "micant/km/executive/usermode.hpp"
#include "micant/km/executive/ps_objects.hpp"
#include <cstring>

namespace micant::ps {
namespace {
// IDs share one namespace and are never reused by the bootstrap. Exhaustion
// fails creation instead of wrapping onto an existing process or thread.
std::atomic<uint32_t> nextClientId{4};
Handle allocateClientId() noexcept {
    uint32_t value = nextClientId.load(std::memory_order_relaxed);
    while (value != UINT32_MAX) {
        if (nextClientId.compare_exchange_weak(value, value + 1, std::memory_order_relaxed))
            return static_cast<Handle>(value);
    }
    return 0;
}

// Caller holds BSP interrupt exclusion, as for selectNextThread().
bool hasUsableIdleThread(const ke::KThread* current) noexcept {
    const auto* idle = ke::g_CurrentCpu.idleThread;
    return idle && idle != current && !idle->readyQueued &&
        (idle->state == ke::ThreadState::Initialized || idle->state == ke::ThreadState::Ready);
}
}

namespace detail {
// The queue owns no new reference: Ob-backed threads already hold a lifecycle
// reference until reap. All operations are serialized by BSP interrupt masking.
struct ThreadReaper {
    static inline EThread* head{};
    static inline EThread* tail{};
    static inline EProcess* processHead{};
    static inline EProcess* processTail{};
    static inline ob::ObjectHeader* deleteHead{};
    static inline ob::ObjectHeader* deleteTail{};
    static inline ke::KThread worker{};
    alignas(16) static inline uint8_t workerStack[65536]{};
    static inline bool workerInitialized{};

    static void enqueue(EThread& thread) noexcept {
        if (!thread.objectHeader_ || thread.deferredQueued_) return;
        thread.deferredNext_ = nullptr;
        thread.deferredQueued_ = true;
        if (tail) tail->deferredNext_ = &thread; else head = &thread;
        tail = &thread;
    }
    static void enqueueProcess(EProcess& process, bool alreadyPinned = false) noexcept {
        if (process.deferredQueued_) return;
        if (!process.objectHeader_) __builtin_trap();
        if (!alreadyPinned) process.objectHeader_->addReference();
        process.deferredNext_ = nullptr;
        process.deferredQueued_ = true;
        if (processTail) processTail->deferredNext_ = &process; else processHead = &process;
        processTail = &process;
    }
    static void enqueueDelete(ob::ObjectHeader& header) noexcept {
        header.deferredDeleteNext = nullptr;
        if (deleteTail) deleteTail->deferredDeleteNext = &header; else deleteHead = &header;
        deleteTail = &header;
    }
    static void remove(EThread& thread) noexcept {
        if (!thread.deferredQueued_) return;
        EThread* previous{};
        for (auto* item = head; item; item = item->deferredNext_) {
            if (item != &thread) { previous = item; continue; }
            if (previous) previous->deferredNext_ = item->deferredNext_; else head = item->deferredNext_;
            if (tail == item) tail = previous;
            thread.deferredNext_ = nullptr;
            thread.deferredQueued_ = false;
            return;
        }
        __builtin_trap();
    }
    [[noreturn]] static void workerEntry(void*) noexcept {
        ke::KeLowerIrql(ke::PASSIVE_LEVEL);
        for (;;) {
            __asm__ __volatile__("cli" : : : "memory");
            bool retry = false;
            while (head) {
                auto* thread = head;
                remove(*thread);
                if (thread->thread_.get() == ke::KeGetCurrentThread()) {
                    enqueue(*thread);
                    retry = true;
                    break;
                }
                // Pin the object before enabling interrupts: another caller
                // may race this worker and complete the reap first.
                auto* reference = thread->objectHeader_;
                if (!reference) __builtin_trap();
                reference->addReference();
                __asm__ __volatile__("sti" : : : "memory");
                const auto status = thread->reap();
                __asm__ __volatile__("cli" : : : "memory");
                const bool incomplete = status != NtStatus::Success && thread->thread_ != nullptr;
                if (incomplete) enqueue(*thread);
                // Drop the pin outside exclusion; its delete callback may run.
                __asm__ __volatile__("sti" : : : "memory");
                ob::ObDereferenceObject(reference);
                __asm__ __volatile__("cli" : : : "memory");
                if (incomplete) { retry = true; break; }
                // The object may have been destroyed by the last dereference.
            }
            while (!retry && processHead) {
                auto* process = processHead;
                processHead = process->deferredNext_;
                if (!processHead) processTail = nullptr;
                process->deferredNext_ = nullptr;
                process->deferredQueued_ = false;
                auto* reference = process->objectHeader_;
                if (!reference) __builtin_trap();
                __asm__ __volatile__("sti" : : : "memory");
                const auto status = process->closeAddressSpace();
                if (status == NtStatus::Success) (void)process->terminationEvent().set();
                __asm__ __volatile__("cli" : : : "memory");
                if (status != NtStatus::Success) {
                    enqueueProcess(*process, true);
                    retry = true;
                } else {
                    __asm__ __volatile__("sti" : : : "memory");
                    ob::ObDereferenceObject(reference);
                    __asm__ __volatile__("cli" : : : "memory");
                }
            }
            while (!retry && deleteHead) {
                auto* header = deleteHead;
                deleteHead = header->deferredDeleteNext;
                if (!deleteHead) deleteTail = nullptr;
                header->deferredDeleteNext = nullptr;
                // The zero-reference header remains allocated until this
                // worker completes its potentially blocking delete callback.
                __asm__ __volatile__("sti" : : : "memory");
                ob::ObFinishDelete(header);
                __asm__ __volatile__("cli" : : : "memory");
            }
            // Retry on a later scheduler boundary rather than spin on a
            // temporarily unavailable process or thread resource.
            if ((!head && !processHead && !deleteHead) || retry) {
                if (!ke::detail::blockCurrentThread())
                    __asm__ __volatile__("sti; hlt; cli" : : : "memory");
            }
        }
    }

    static void service() noexcept {
        if (ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL || (!head && !processHead && !deleteHead) ||
            !hal::amd64::bootstrapSchedulerClockActive()) return;
        uint64_t flags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        if (!workerInitialized) {
            worker.directoryTableBase = hal::amd64::bootstrapKernelPagingRoot();
            worker.priority = ke::PRIORITY_NORMAL;
            if (!ke::detail::prepareBootstrapThread(worker, workerStack, sizeof(workerStack), workerEntry, nullptr))
                __builtin_trap();
            workerInitialized = true;
        }
        if (&worker != ke::KeGetCurrentThread() && !worker.readyQueued &&
            (worker.state == ke::ThreadState::Initialized || worker.state == ke::ThreadState::Waiting)) {
            if (!ke::PriorityScheduler::get().readyKThread(&worker)) __builtin_trap();
        }
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    }
};

bool deferProcessObjectDelete(ob::ObjectHeader* header) noexcept {
    if (!header || !hal::amd64::bootstrapSchedulerClockActive()) return false;
    // Once the address space has been closed the destructor has no external
    // cleanup callbacks and may release the empty object synchronously.
    if (!header->as<EProcess>()->getDirectoryTableBase()) return false;
    ob::detail::HandleTableGuard guard;
    ThreadReaper::enqueueDelete(*header);
    return true;
}
}

EProcess::EProcess() = default;
EProcess::~EProcess() = default;

size_t EProcess::activeThreadCount() const noexcept {
    ob::detail::HandleTableGuard guard;
    size_t count{};
    for (auto* thread = threads_; thread; thread = thread->processNext)
        if (thread->state != ke::ThreadState::Terminated) ++count;
    return count;
}

NtStatus EProcess::initializeAddressSpace() noexcept {
    using namespace hal::amd64;
    ke::g_CurrentCpu.serviceDeferredCleanup = detail::ThreadReaper::service;
    if (root_.pfn || addressSpaceClosing_ || (objectHeader_ && processId_)) return NtStatus::InvalidParameter;
    const Handle processId = allocateClientId();
    if (!processId) return NtStatus::InsufficientResources;
    handles_.reset(new (std::nothrow_t{}) ob::HandleTable{});
    if (!handles_) return NtStatus::NoMemory;
    if (!createProcessPagingRoot(root_)) { handles_.reset(); return NtStatus::NoMemory; }
    const mm::ProcessAddressSpace::PagingBackend backend{
        &root_, mapBootstrapManagedPage, unmapBootstrapManagedPage,
        mapBootstrapProtectManagedPage, mapBootstrapManagedView, unmapBootstrapManagedView,
        mapBootstrapGuardManagedPage};
    if (!addressSpace_.configurePagingBackend(backend, BootstrapManagedBase, BootstrapManagedSize)) {
        if (!destroyProcessPagingRoot(root_)) __builtin_trap();
        handles_.reset();
        return NtStatus::Unsuccessful;
    }
    processId_ = processId;
    terminating_ = false;
    exitStatus_ = NtStatus::Pending;
#if defined(MICANT_BAREMETAL) && (defined(_M_X64) || defined(__x86_64__))
    (void)terminationEvent_.reset();
#endif
    imagesFinalized_ = false;
    userPeb_ = 0;
    userLoaderData_ = 0;
    userProcessParameters_ = 0;
    nativeImagePaths_ = {};
    processImagePath_ = {};
    processCommandLine_ = {};
    processParametersConfigured_ = false;
    userTlsLayouts_.reset();
    userTlsCount_ = 0;
    return NtStatus::Success;
}

NtStatus EProcess::closeAddressSpace() noexcept {
    using namespace hal::amd64;
    uint64_t entryFlags{};
    uintptr_t observed{};
    __asm__ __volatile__("pushfq; popq %0; cli; movq %%cr3, %1"
                         : "=r"(entryFlags), "=r"(observed) : : "memory");
    if (!root_.pfn || attachedThreads_ || addressSpaceClosing_ || observed == getDirectoryTableBase()) {
        if (entryFlags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return NtStatus::InvalidParameter;
    }
    addressSpaceClosing_ = true;
    if (entryFlags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    // Handle deletion may invoke arbitrary object callbacks. Do it in the
    // caller's address space, before changing CR3 and outside Mm locks.
    handles_->closeAllHandles();
    NtStatus status = NtStatus::Success;
    bool done = false;
    while (!done) {
        uint64_t flags{};
        uintptr_t previous{};
        __asm__ __volatile__("pushfq; popq %0; cli; movq %%cr3, %1"
                             : "=r"(flags), "=r"(previous) : : "memory");
        mm::ProcessAddressSpace::DeferredOwnerRelease owner{};
        if (previous == getDirectoryTableBase()) status = NtStatus::InvalidParameter;
        else if (!switchProcessPagingRoot(&root_)) status = NtStatus::Unsuccessful;
        else {
            status = addressSpace_.closeOne(owner, done);
            if (!switchProcessPagingRootByPhysical(previous)) __builtin_trap();
        }
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        if (owner.release) owner.release(owner.context);
        if (status != NtStatus::Success) break;
    }
    if (status == NtStatus::Success) {
        uint64_t flags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        if (!destroyProcessPagingRoot(root_)) status = NtStatus::Unsuccessful;
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    }
    if (status != NtStatus::Success) {
        uint64_t resetFlags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(resetFlags) : : "memory");
        addressSpaceClosing_ = false;
        if (resetFlags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return status;
    }
    userPeb_ = 0;
    userLoaderData_ = 0;
    userProcessParameters_ = 0;
    nativeImagePaths_ = {};
    processImagePath_ = {};
    processCommandLine_ = {};
    processParametersConfigured_ = false;
    userTlsLayouts_.reset();
    userTlsCount_ = 0;
    images_.reset();
    ownedImageInputs_.reset();
    ownedImagePath_.reset();
    ownedCommandLine_.reset();
    handles_.reset();
    addressSpaceClosing_ = false;
    return NtStatus::Success;
}

NtStatus EProcess::attachThread(ke::KThread& thread) noexcept {
    ob::detail::HandleTableGuard guard;
    if (!root_.pfn || terminating_ || addressSpaceClosing_ || thread.owningProcess || thread.readyQueued ||
        (thread.state != ke::ThreadState::Initialized && thread.state != ke::ThreadState::Ready))
        return NtStatus::InvalidParameter;
    thread.owningProcess = this;
    thread.directoryTableBase = getDirectoryTableBase();
    thread.processNext = threads_;
    threads_ = &thread;
    ++attachedThreads_;
    if (objectHeader_) objectHeader_->addReference();
    return NtStatus::Success;
}

void EProcess::onThreadTermination(NtStatus exitStatus) noexcept {
    if (!objectHeader_ || systemProcess_ || terminating_) return;
    for (auto* thread = threads_; thread; thread = thread->processNext)
        if (thread->state != ke::ThreadState::Terminated) return;
    terminating_ = true;
    exitStatus_ = exitStatus;
}

NtStatus EProcess::requestTermination(NtStatus exitStatus, ke::KThread* callingThread,
                                      bool terminateProcess) noexcept {
    // Caller holds BSP interrupt exclusion throughout the preflight and commit.
    if (!root_.pfn || terminating_) return NtStatus::ProcessIsTerminating;
    bool willHaveNext = ke::PriorityScheduler::get().getReadyThreadCount() ||
        hasUsableIdleThread(ke::KeGetCurrentThread());
    for (auto* thread = threads_; thread; thread = thread->processNext) {
        if (thread == callingThread || thread->state == ke::ThreadState::Terminated) continue;
        const bool waiting = thread->state == ke::ThreadState::Waiting;
        const bool ready = thread->state == ke::ThreadState::Ready && thread->readyQueued;
        const bool unstarted = thread->owningThread && !thread->owningThread->started_ &&
            thread->state == ke::ThreadState::Initialized && !thread->readyQueued;
        if (!thread->owningThread || (!unstarted && !waiting && !ready)) return NtStatus::NotImplemented;
        if (thread->owningThread->isSystemThread()) return NtStatus::AccessDenied;
        if (!thread->owningThread->started_ && (thread->kernelApcDisable ||
            thread->specialApcDisable || thread->normalKernelApcActive))
            return NtStatus::NotImplemented;
        const bool cancellable = waiting && thread->eventWaitInProgress && thread->eventWaitUserMode &&
            thread->eventWaitCanTerminate && thread->eventWaitObject &&
            thread->eventWaitBlock && !thread->eventWaitCancelled && !thread->kernelApcDisable &&
            !thread->specialApcDisable && !thread->normalKernelApcActive;
        if (cancellable && thread->readyQueued) return NtStatus::Unsuccessful;
        if (unstarted || ready || cancellable) willHaveNext = true;
    }
    // A self-termination cannot commit without a runnable replacement stack.
    if (terminateProcess && callingThread && !willHaveNext) return NtStatus::Unsuccessful;
    // Queue never-started threads first. If that fails, rollback has not
    // disturbed event wait queues or their FIFO ordering.
    bool success = true;
    for (auto* thread = threads_; thread; thread = thread->processNext) {
        if (thread == callingThread || thread->state != ke::ThreadState::Initialized) continue;
        if (!ke::PriorityScheduler::get().readyKThread(thread)) { success = false; break; }
        thread->terminationQueuedByProcess = true;
    }
    if (!success) {
        for (auto* thread = threads_; thread; thread = thread->processNext) {
            if (thread == callingThread) continue;
            if (thread->terminationQueuedByProcess) {
                if (!ke::PriorityScheduler::get().removeReadyThread(thread)) __builtin_trap();
                thread->state = ke::ThreadState::Initialized;
                thread->terminationQueuedByProcess = false;
            }
        }
        return NtStatus::Unsuccessful;
    }
    // Every eligible waiter was preflighted while interrupts were excluded;
    // cancellation and ready insertion cannot fail without a broken invariant.
    for (auto* thread = threads_; thread; thread = thread->processNext) {
        if (thread == callingThread || thread->state != ke::ThreadState::Waiting ||
            !thread->eventWaitInProgress || !thread->eventWaitUserMode || !thread->eventWaitCanTerminate ||
            !thread->eventWaitObject || !thread->eventWaitBlock ||
            thread->eventWaitCancelled || thread->kernelApcDisable || thread->specialApcDisable ||
            thread->normalKernelApcActive) continue;
        if (!static_cast<sync::detail::BootstrapEvent*>(thread->eventWaitObject)->cancelWaitForTermination(*thread))
            __builtin_trap();
    }
    if (terminateProcess) {
        terminating_ = true;
        exitStatus_ = exitStatus;
    }
    for (auto* thread = threads_; thread; thread = thread->processNext) {
        thread->terminationQueuedByProcess = false;
        if (!terminateProcess && thread == callingThread) continue;
        if (thread->state != ke::ThreadState::Terminated && !thread->terminationRequested) {
            thread->owningThread->requestedExitStatus_ = exitStatus;
            thread->terminationRequested = true;
        }
    }
    return NtStatus::Success;
}

NtStatus EProcess::terminateOtherThreads(NtStatus exitStatus) noexcept {
    ob::detail::HandleTableGuard guard;
    auto* current = ke::KeGetCurrentThread();
    if (!current || current->owningProcess != this || !current->owningThread ||
        ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL) return NtStatus::InvalidParameter;
    return requestTermination(exitStatus, current, false);
}

NtStatus EProcess::terminateCurrent(NtStatus exitStatus) noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    auto* current = ke::KeGetCurrentThread();
    NtStatus status = NtStatus::InvalidParameter;
    if (current && current->owningProcess == this && current->owningThread &&
        current->state == ke::ThreadState::Running && ke::KeGetCurrentIrql() == ke::PASSIVE_LEVEL) {
        if (current->kernelApcDisable || current->specialApcDisable || current->normalKernelApcActive)
            status = NtStatus::NotImplemented;
        else if (terminating_)
            status = NtStatus::Success;
        else
            status = requestTermination(exitStatus, current);
    }
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    if (status != NtStatus::Success) return status;
    return current->owningThread->exitCurrent(exitStatus_);
}

NtStatus EProcess::terminateOther(NtStatus exitStatus) noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    auto* current = ke::KeGetCurrentThread();
    NtStatus result{};
    if (!root_.pfn || terminating_) result = NtStatus::ProcessIsTerminating;
    else if (!current || current->owningProcess == this || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL)
        result = NtStatus::InvalidParameter;
    else if (attachedThreads_) result = requestTermination(exitStatus, nullptr);
    else {
        terminating_ = true;
        exitStatus_ = exitStatus;
        if (hal::amd64::bootstrapSchedulerClockActive()) {
            detail::ThreadReaper::enqueueProcess(*this);
            result = NtStatus::Success;
        } else {
            // Early bootstrap has no worker; retain the caller's Ob reference
            // and close synchronously after restoring the caller's IF state.
            if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
            result = closeAddressSpace();
            if (result == NtStatus::Success) (void)terminationEvent_.set();
            return result;
        }
    }
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return result;
}

NtStatus EProcess::detachThread(ke::KThread& thread) noexcept {
    ob::ObjectHeader* owner{};
    {
        ob::detail::HandleTableGuard guard;
        if (thread.owningProcess != this || thread.readyQueued ||
            (thread.state != ke::ThreadState::Initialized && thread.state != ke::ThreadState::Terminated))
            return NtStatus::InvalidParameter;
        if (!attachedThreads_) __builtin_trap();
        ke::KThread** link = &threads_;
        while (*link && *link != &thread) link = &(*link)->processNext;
        if (!*link) __builtin_trap();
        *link = thread.processNext;
        thread.processNext = nullptr;
        --attachedThreads_;
        owner = objectHeader_;
        thread.owningProcess = nullptr;
        thread.directoryTableBase = 0;
    }
    // A final dereference can invoke a delete callback; do not hold the
    // process-list exclusion while it runs.
    if (owner) ob::ObDereferenceObject(owner);
    return NtStatus::Success;
}

EThread::~EThread() {
    if (thread_ || stack_ || process_ || reapProcessReference_ || processDetached_) __builtin_trap();
}

NtStatus EThread::initialize(EProcess& process, size_t stackSize,
                             StartRoutine start, void* argument) noexcept {
    if (objectOwned_ || thread_ || stack_ || process_ || !start || stackSize < 8192 || stackSize > 1024 * 1024)
        return NtStatus::InvalidParameter;
    const Handle threadId = allocateClientId();
    if (!threadId) return NtStatus::InsufficientResources;
    std::unique_ptr<ke::KThread> thread(new (std::nothrow_t{}) ke::KThread{});
    std::unique_ptr<uint8_t[]> stack(new (std::nothrow_t{}) uint8_t[stackSize]);
    if (!thread || !stack) return NtStatus::NoMemory;
    if (!ke::detail::prepareBootstrapThread(*thread, stack.get(), stackSize, startThread, this))
        return NtStatus::InvalidParameter;
    thread->tid = threadId;
    thread->pid = process.processId();
    thread->terminationNotify = signalTermination;
    thread->serviceTermination = serviceTermination;
    thread->terminationContext = this;
    thread->owningThread = this;
    // Publish the process membership and the owning EThread state as one
    // transition; termination must not schedule a partly initialized owner.
    ob::detail::HandleTableGuard guard;
    const auto status = process.attachThread(*thread);
    if (status != NtStatus::Success) return status;
    ke::detail::registerClientThread(*thread);
    ke::g_CurrentCpu.serviceDeferredCleanup = detail::ThreadReaper::service;
#if defined(MICANT_BAREMETAL) && (defined(_M_X64) || defined(__x86_64__))
    (void)terminationEvent_.reset();
#endif
    process_ = &process;
    thread_ = std::move(thread);
    stack_ = std::move(stack);
    start_ = start;
    argument_ = argument;
    exitStatus_ = NtStatus::Pending;
    requestedExitStatus_ = NtStatus::Success;
    systemThread_ = false;
    return NtStatus::Success;
}

NtStatus EThread::initializeSystem(EProcess& process, size_t stackSize,
                                    SystemStartRoutine start, void* argument) noexcept {
    if (!start) return NtStatus::InvalidParameter;
    ob::detail::HandleTableGuard guard;
    const auto status = initialize(process, stackSize, startSystem, this);
    if (status == NtStatus::Success) {
        systemThread_ = true;
        systemStart_ = start;
        systemArgument_ = argument;
    }
    return status;
}

void EThread::startSystem(void* argument) noexcept {
    auto* self = static_cast<EThread*>(argument);
    if (!self || !self->systemStart_ || !self->thread_->systemInitialRegionActive) __builtin_trap();
    self->systemStart_(self->systemArgument_);
    // Drivers are expected to call PsTerminateSystemThread themselves.
    // A returning routine still exits through the same lifecycle path.
    if (self->thread_->systemInitialRegionActive) ke::KeLeaveCriticalRegion();
}

NtStatus EThread::terminateSystemThread(NtStatus status) noexcept {
    if (!systemStart_ || ke::KeGetCurrentThread() != thread_.get() ||
        ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL) return NtStatus::InvalidParameter;
    const bool leaveInitialRegion = thread_->systemInitialRegionActive;
    if (leaveInitialRegion) ke::KeLeaveCriticalRegion();
    const auto result = exitCurrent(status);
    if (leaveInitialRegion) {
        ke::KeEnterCriticalRegion();
        thread_->systemInitialRegionActive = true;
    }
    return result;
}

extern "C" __declspec(dllexport) NtStatus PsTerminateSystemThread(NtStatus exitStatus) noexcept {
    auto* current = ke::KeGetCurrentThread();
    if (!current || !current->owningThread) return NtStatus::InvalidParameter;
    return current->owningThread->terminateSystemThread(exitStatus);
}

NtStatus EThread::ready() noexcept {
    ob::detail::HandleTableGuard guard;
    if (reapInProgress_ || !thread_ || thread_->terminationRequested ||
        thread_->initialSuspendCount || thread_->state != ke::ThreadState::Initialized)
        return NtStatus::InvalidParameter;
    return ke::PriorityScheduler::get().readyKThread(thread_.get())
        ? NtStatus::Success : NtStatus::Unsuccessful;
}

NtStatus EThread::suspendInitially() noexcept {
    ob::detail::HandleTableGuard guard;
    if (!thread_ || started_ || reapInProgress_ || thread_->readyQueued ||
        thread_->terminationRequested || thread_->state != ke::ThreadState::Initialized ||
        thread_->initialSuspendCount) return NtStatus::InvalidParameter;
    thread_->initialSuspendCount = 1;
    return NtStatus::Success;
}

NtStatus EThread::resumeInitial(uint32_t& previousCount) noexcept {
    ob::detail::HandleTableGuard guard;
    if (!thread_) {
        if (!objectOwned_) return NtStatus::InvalidParameter;
        previousCount = 0;
        return NtStatus::Success;
    }
    if (reapInProgress_) return NtStatus::InvalidParameter;
    previousCount = thread_->initialSuspendCount;
    if (thread_->terminationRequested || thread_->state == ke::ThreadState::Terminated) {
        thread_->initialSuspendCount = 0;
        return NtStatus::Success;
    }
    if (!previousCount) return NtStatus::Success;
    if (started_ || thread_->state != ke::ThreadState::Initialized || thread_->readyQueued)
        return NtStatus::InvalidParameter;
    thread_->initialSuspendCount = 0;
    if (!ke::PriorityScheduler::get().readyKThread(thread_.get())) {
        thread_->initialSuspendCount = 1;
        return NtStatus::Unsuccessful;
    }
    return NtStatus::Success;
}

NtStatus EThread::initializeUser(EProcess& process, uintptr_t entry, size_t kernelStackSize,
                                 size_t userStackSize, const pe::PeImageView64::TlsLayout* tls,
                                 bool interruptible) noexcept {
    return initializeUserWithTls(process, entry, kernelStackSize, userStackSize,
        tls ? std::span<const pe::PeImageView64::TlsLayout>{tls, 1} : std::span<const pe::PeImageView64::TlsLayout>{},
        interruptible);
}

NtStatus EThread::initializeUserWithTls(EProcess& process, uintptr_t entry, size_t kernelStackSize,
                                        size_t userStackSize, std::span<const pe::PeImageView64::TlsLayout> tls,
                                        bool interruptible, size_t commitSize) noexcept {
    // The target CR3 and the newly attached thread must remain stable until
    // its TEB, stack and startup state have all been installed.
    ob::detail::HandleTableGuard guard;
    uintptr_t currentRoot{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(currentRoot));
    if (currentRoot != process.getDirectoryTableBase() || !entry || userStackSize < mm::PageSize4KB ||
        userStackSize > 1024 * 1024 || userStackSize > SIZE_MAX - mm::PageMask4KB ||
        !hal::amd64::bootstrapUserRange(entry, 1, false, true)) return NtStatus::InvalidParameter;
    if (tls.size() > 16) return NtStatus::NotImplemented;
    for (const auto& layout : tls) {
        if (!layout.present || layout.callbackCount || !layout.alignment || layout.alignment > 256 ||
            layout.initialData.size() > mm::PageSize4KB - 0x100 ||
            layout.zeroFill > mm::PageSize4KB - 0x100 - layout.initialData.size()) return NtStatus::NotImplemented;
    }
    const size_t stackSize = (userStackSize + mm::PageMask4KB) & ~mm::PageMask4KB;
    if (!commitSize) commitSize = stackSize;
    if (commitSize > stackSize || commitSize > SIZE_MAX - mm::PageMask4KB)
        return NtStatus::InvalidParameter;
    commitSize = (commitSize + mm::PageMask4KB) & ~mm::PageMask4KB;
    if (commitSize < mm::PageSize4KB || commitSize > stackSize) return NtStatus::InvalidParameter;
    auto status = initialize(process, kernelStackSize, startUser, this);
    if (status != NtStatus::Success) return status;
    auto& space = process.getAddressSpace();
    status = space.allocate(userStack_, stackSize, mm::MEM_RESERVE, mm::PAGE_READWRITE);
    if (status == NtStatus::Success) {
        uintptr_t committed = userStack_ + stackSize - commitSize;
        status = space.allocate(committed, commitSize, mm::MEM_COMMIT, mm::PAGE_READWRITE);
    }
    if (status == NtStatus::Success)
        status = space.allocate(userTeb_, 2 * mm::PageSize4KB, mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE);
    if (status == NtStatus::Success && !tls.empty())
        status = space.allocate(userTls_, mm::PageSize4KB, mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE);
    if (status == NtStatus::Success && !tls.empty()) {
        size_t offset = 0x100;
        auto* vector = reinterpret_cast<uint64_t*>(userTls_);
        for (size_t i = 0; i < tls.size(); ++i) {
            const auto& layout = tls[i];
            if (layout.alignment & (layout.alignment - 1)) { status = NtStatus::InvalidParameter; break; }
            offset = (offset + layout.alignment - 1) & ~(layout.alignment - 1);
            const size_t length = layout.initialData.size() + layout.zeroFill;
            if (offset > mm::PageSize4KB || length > mm::PageSize4KB - offset) {
                status = NtStatus::NotImplemented;
                break;
            }
            vector[i] = userTls_ + offset;
            status = pe::PeImageView64::initializeTlsData(layout,
                std::span<uint8_t>{reinterpret_cast<uint8_t*>(userTls_ + offset), length});
            if (status != NtStatus::Success) break;
            offset += length;
        }
    }
    if (status != NtStatus::Success) {
        if (reap() != NtStatus::Success) return NtStatus::Unsuccessful;
        return status;
    }
    static_assert(sizeof(um::Teb64) <= 2 * mm::PageSize4KB);
    auto* teb = new (reinterpret_cast<void*>(userTeb_)) um::Teb64{};
    teb->self = userTeb_;
    teb->stackBase = userStack_ + stackSize;
    teb->stackLimit = userStack_ + stackSize - commitSize;
    teb->threadLocalStoragePointer = userTls_;
    teb->processEnvironmentBlock = process.userPebAddress();
    teb->uniqueProcessId = process.processId();
    teb->uniqueThreadId = thread_->tid;
    userEntry_ = entry;
    userInterruptible_ = interruptible;
    userStackPointer_ = userStack_ + stackSize - 128;
    thread_->tebAddress = userTeb_;
    thread_->previousUserMode = true;
    return NtStatus::Success;
}

NtStatus EThread::initializeUser(EProcess& process, uintptr_t entry, size_t kernelStackSize,
                                 size_t userStackSize, bool interruptible) noexcept {
    if (!process.nativeImagesFinalized()) return NtStatus::InvalidParameter;
    return initializeUserWithTls(process, entry, kernelStackSize, userStackSize,
                                 process.userTlsLayouts(), interruptible);
}

NtStatus EThread::initializeUser(EProcess& process, uintptr_t entry, size_t kernelStackSize,
                                 size_t reserveSize, size_t commitSize, bool interruptible) noexcept {
    if (!process.nativeImagesFinalized()) return NtStatus::InvalidParameter;
    return initializeUserWithTls(process, entry, kernelStackSize, reserveSize,
                                 process.userTlsLayouts(), interruptible, commitSize);
}

NtStatus EThread::configureUserStart(uintptr_t routine, uintptr_t argument) noexcept {
    ob::detail::HandleTableGuard guard;
    if (!thread_ || !userEntry_ || !routine || started_ || thread_->readyQueued || reapInProgress_ ||
        thread_->state != ke::ThreadState::Initialized) return NtStatus::InvalidParameter;
    userStartRoutine_ = routine;
    userStartArgument_ = argument;
    return NtStatus::Success;
}

void EThread::startUser(void* argument) noexcept {
    auto* self = static_cast<EThread*>(argument);
    if (!self || !self->userEntry_ || !self->userStackPointer_) __builtin_trap();
    hal::amd64::enterBootstrapUserThread(self->userEntry_, self->userStackPointer_,
        self->userInterruptible_, self->userStartRoutine_, self->userStartArgument_);
}

NtStatus EThread::releaseUserMemory() noexcept {
    if (!userStack_ && !userTeb_ && !userTls_) return NtStatus::Success;
    uint64_t flags{};
    uintptr_t previous{};
    __asm__ __volatile__("pushfq; popq %0; cli; movq %%cr3, %1"
                         : "=r"(flags), "=r"(previous) : : "memory");
    const uintptr_t target = process_->getDirectoryTableBase();
    if (previous != target && !hal::amd64::switchProcessPagingRootByPhysical(target)) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return NtStatus::Unsuccessful;
    }
    auto& space = process_->getAddressSpace();
    NtStatus status = NtStatus::Success;
    for (auto* address : {&userTls_, &userTeb_, &userStack_}) {
        if (*address && (status = space.free(*address, 0, mm::MEM_RELEASE)) != NtStatus::Success) break;
        *address = 0;
    }
    if (previous != target && !hal::amd64::switchProcessPagingRootByPhysical(previous)) __builtin_trap();
    if (status == NtStatus::Success) {
        userEntry_ = userStackPointer_ = 0;
        userStartRoutine_ = userStartArgument_ = 0;
        userInterruptible_ = false;
        thread_->tebAddress = 0;
    }
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return status;
}

NtStatus EThread::cancelBeforeStart() noexcept {
    // Serialize queue inspection and the terminal transition with APC
    // insertion and scheduler operations on the bootstrap processor.
    ob::detail::HandleTableGuard guard;
    if (reapInProgress_ || !thread_ || started_ || thread_->terminationRequested || thread_.get() == ke::KeGetCurrentThread())
        return NtStatus::InvalidParameter;
    if (!thread_->kernelApcList.empty() || !thread_->userApcList.empty()) {
        // A queued APC belongs to the target. Run the bootstrap entry solely
        // for target-context rundown, rather than discarding it on the caller.
        if (thread_->kernelApcDisable || thread_->specialApcDisable || thread_->normalKernelApcActive || thread_->pendingNormalCount)
            return NtStatus::NotImplemented;
        if (thread_->state != ke::ThreadState::Initialized &&
            !(thread_->state == ke::ThreadState::Ready && thread_->readyQueued))
            return NtStatus::InvalidParameter;
        thread_->terminationRequested = true;
        if (thread_->state == ke::ThreadState::Initialized &&
            !ke::PriorityScheduler::get().readyKThread(thread_.get())) {
            thread_->terminationRequested = false;
            return NtStatus::Unsuccessful;
        }
        return NtStatus::Success;
    }
    if (thread_->state == ke::ThreadState::Ready) {
        if (!ke::PriorityScheduler::get().removeReadyThread(thread_.get()))
            return NtStatus::InvalidParameter;
    } else if (thread_->state != ke::ThreadState::Initialized || thread_->readyQueued) {
        return NtStatus::InvalidParameter;
    }
    exitStatus_ = NtStatus::Success;
    thread_->state = ke::ThreadState::Terminated;
    signalTermination(this);
    return NtStatus::Success;
}

NtStatus EThread::rollbackBeforeStart() noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0" : "=r"(flags));
    // The syscall owns BSP exclusion through the subsequent reap. Refuse a
    // caller that cannot provide that lifetime and scheduling guarantee.
    if (flags & (uint64_t{1} << 9)) return NtStatus::InvalidParameter;
    {
        ob::detail::HandleTableGuard guard;
        if (!thread_ || started_ || reapInProgress_ || thread_->terminationRequested ||
            !thread_->kernelApcList.empty() || !thread_->userApcList.empty())
            return NtStatus::InvalidParameter;
        if (thread_->state == ke::ThreadState::Ready) {
            if (!ke::PriorityScheduler::get().removeReadyThread(thread_.get()))
                return NtStatus::InvalidParameter;
            thread_->state = ke::ThreadState::Initialized;
        } else if (thread_->state != ke::ThreadState::Initialized || thread_->readyQueued)
            return NtStatus::InvalidParameter;
    }
    return reap();
}

NtStatus EThread::terminateOther(NtStatus status) noexcept {
    ob::detail::HandleTableGuard guard;
    if (!thread_) return objectOwned_ ? NtStatus::Success : NtStatus::InvalidParameter;
    if (thread_->state == ke::ThreadState::Terminated || thread_->terminationRequested)
        return NtStatus::Success;
    if (!process_ || reapInProgress_ || thread_.get() == ke::KeGetCurrentThread() ||
        ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL)
        return NtStatus::InvalidParameter;
    if (!started_ && (thread_->kernelApcDisable || thread_->specialApcDisable ||
                      thread_->normalKernelApcActive)) return NtStatus::NotImplemented;
    // Started kernel threads have no user-return boundary at which this
    // bootstrap mechanism can safely deliver the termination request.
    if (started_ && !userEntry_) return NtStatus::NotImplemented;
    if (thread_->state == ke::ThreadState::Waiting) {
        if (!thread_->eventWaitInProgress || !thread_->eventWaitUserMode ||
            !thread_->eventWaitCanTerminate || !thread_->eventWaitObject ||
            !thread_->eventWaitBlock || thread_->eventWaitCancelled ||
            thread_->kernelApcDisable || thread_->specialApcDisable || thread_->normalKernelApcActive)
            return NtStatus::NotImplemented;
        auto* event = static_cast<sync::detail::BootstrapEvent*>(thread_->eventWaitObject);
        if (!event->cancelWaitForTermination(*thread_)) return NtStatus::Unsuccessful;
    } else if (thread_->state == ke::ThreadState::Initialized && !thread_->readyQueued) {
        if (!ke::PriorityScheduler::get().readyKThread(thread_.get())) return NtStatus::Unsuccessful;
    } else if (thread_->state != ke::ThreadState::Ready || !thread_->readyQueued) {
        return NtStatus::NotImplemented;
    }
    requestedExitStatus_ = status;
    thread_->terminationRequested = true;
    return NtStatus::Success;
}

void EThread::signalTermination(void* argument) noexcept {
#if defined(MICANT_BAREMETAL) && (defined(_M_X64) || defined(__x86_64__))
    auto* self = static_cast<EThread*>(argument);
    if (self->process_) self->process_->onThreadTermination(self->exitStatus_);
    (void)self->terminationEvent_.set();
    detail::ThreadReaper::enqueue(*self);
#else
    (void)argument;
#endif
}

void EThread::serviceTermination(void* argument) noexcept {
#if defined(MICANT_BAREMETAL) && (defined(_M_X64) || defined(__x86_64__))
    auto* self = static_cast<EThread*>(argument);
    auto* current = ke::KeGetCurrentThread();
    if (!self || current != self->thread_.get() || !self->process_ ||
        !current->terminationRequested || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL) return;
    (void)self->exitCurrent(self->requestedExitStatus_);
#else
    (void)argument;
#endif
}

void EThread::startThread(void* argument) noexcept {
    auto* self = static_cast<EThread*>(argument);
    if (!self || !self->thread_ || ke::KeGetCurrentThread() != self->thread_.get()) __builtin_trap();
    self->started_ = true;
    ke::KeLowerIrql(ke::PASSIVE_LEVEL);
    if (self->systemStart_ && !self->thread_->terminationRequested) {
        ke::KeEnterCriticalRegion();
        self->thread_->systemInitialRegionActive = true;
    }
    // The runtime clock owns IRQ routing before new threads enable IF.
    // Early bootstrap probes retain their original interrupt-disabled mode.
    if (hal::amd64::bootstrapSchedulerClockActive()) __asm__ __volatile__("sti" : : : "memory");
    // A newly scheduled target may already have kernel APCs queued.
    ke::KiDeliverApc(0);
    // A cancelled, never-started thread enters only to run target-owned APC
    // cleanup; its original start routine must not execute.
    if (!self->thread_->terminationRequested) self->start_(self->argument_);
    if (self->thread_->systemInitialRegionActive) ke::KeLeaveCriticalRegion();
    const auto status = self->thread_->terminationRequested
        ? self->requestedExitStatus_ : NtStatus::Success;
    (void)self->exitCurrent(status);
    __builtin_trap(); // A terminated thread may never resume.
}

NtStatus EThread::exitCurrent(NtStatus status) noexcept {
#if defined(MICANT_BAREMETAL) && (defined(_M_X64) || defined(__x86_64__))
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    auto* current = ke::KeGetCurrentThread();
    if (!thread_ || current != thread_.get() || current->state != ke::ThreadState::Running ||
        ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL || current->kernelApcDisable ||
        current->specialApcDisable || current->normalKernelApcActive ||
        current->eventWaitInProgress || current->eventWaitObject) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return NtStatus::InvalidParameter;
    }
    const bool alreadyActive = current->terminationActive;
    const bool previouslyQueueable = current->apcQueueable;
    current->terminationActive = true;
    // Do not remove a runnable successor while APC callbacks run: those
    // callbacks may themselves wait for that thread.
    if (!ke::PriorityScheduler::get().getReadyThreadCount() && !hasUsableIdleThread(current)) {
        current->terminationActive = alreadyActive;
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return NtStatus::Unsuccessful;
    }
    // Freeze insertion under BSP exclusion before callbacks can be
    // preempted; pending kernel APCs still drain in the target context.
    current->apcQueueable = false;
    if ((flags & (uint64_t{1} << 9)) || hal::amd64::bootstrapSchedulerClockActive())
        __asm__ __volatile__("sti" : : : "memory");
    ke::KiDeliverApc(0);
    const bool rundown = current->apcRundownComplete || ke::detail::rundownCurrentUserApcs();
    __asm__ __volatile__("cli" : : : "memory");
    if (!rundown) {
        // No rundown callback ran if the preconditions were not met. In
        // particular, do not leave a live thread with its APC queue frozen.
        current->apcQueueable = previouslyQueueable;
        current->terminationActive = alreadyActive;
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return NtStatus::NotImplemented;
    }
    ke::KfRaiseIrql(ke::DISPATCH_LEVEL);
    auto* next = ke::PriorityScheduler::get().selectNextThread();
    if (next && current->kernelApcList.empty() && !current->pendingNormalCount &&
        !current->normalKernelApcActive) {
        exitStatus_ = status;
        current->state = ke::ThreadState::Terminated;
        if (ke::KiSwapContext(current, next)) __builtin_trap();
    }
    // Rundown is irreversible: callbacks may have freed APC storage. A
    // missing successor or rejected context switch is a scheduler invariant
    // failure, not a recoverable exit that can return to the live thread.
    __builtin_trap();
#else
    (void)status;
    return NtStatus::NotImplemented;
#endif
}

NtStatus EThread::reap() noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (reapInProgress_ || !thread_ || !process_ || thread_->readyQueued ||
        thread_.get() == ke::KeGetCurrentThread() ||
        (thread_->state != ke::ThreadState::Initialized && thread_->state != ke::ThreadState::Terminated)) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return NtStatus::InvalidParameter;
    }
    if (!thread_->kernelApcList.empty() || !thread_->userApcList.empty() ||
        thread_->pendingNormalCount || thread_->normalKernelApcActive || thread_->apcRundownActive) {
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return NtStatus::NotImplemented;
    }
    const bool wasApcQueueable = thread_->apcQueueable;
    thread_->apcQueueable = false;
    reapInProgress_ = true;
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    const auto fail = [this, wasApcQueueable](NtStatus status) noexcept {
        uint64_t localFlags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(localFlags) : : "memory");
        reapInProgress_ = false;
        if (thread_) thread_->apcQueueable = wasApcQueueable;
        if (localFlags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return status;
    };
    if (!processDetached_) {
        const auto cleanup = releaseUserMemory();
        if (cleanup != NtStatus::Success) return fail(cleanup);
        uint64_t detachFlags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(detachFlags) : : "memory");
        // Hold an extra reference across detach so process deletion and its
        // callbacks never run inside the lifecycle exclusion.
        auto* processReference = process_->objectHeader_;
        if (processReference) processReference->addReference();
        if (processReference && process_->isTerminating() && process_->attachedThreads_ == 1)
            reapProcessReference_ = processReference;
        const auto status = process_->detachThread(*thread_);
        if (status == NtStatus::Success) processDetached_ = true;
        if (status == NtStatus::Success && processReference && !reapProcessReference_) process_ = nullptr;
        if (detachFlags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        if (status != NtStatus::Success) {
            reapProcessReference_ = nullptr;
            if (processReference) ob::ObDereferenceObject(processReference);
            return fail(status);
        }
        if (processReference && !reapProcessReference_) ob::ObDereferenceObject(processReference);
    }
    if (reapProcessReference_) {
        const auto status = process_->closeAddressSpace();
        if (status != NtStatus::Success) return fail(status);
        auto* reference = reapProcessReference_;
        reapProcessReference_ = nullptr;
#if defined(MICANT_BAREMETAL) && (defined(_M_X64) || defined(__x86_64__))
        (void)process_->terminationEvent().set();
#endif
        process_ = nullptr;
        ob::ObDereferenceObject(reference);
    }
    uint64_t finalFlags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(finalFlags) : : "memory");
    detail::ThreadReaper::remove(*this);
    ke::detail::unregisterClientThread(*thread_);
    thread_.reset();
    stack_.reset();
    process_ = nullptr;
    start_ = nullptr;
    argument_ = nullptr;
    systemStart_ = nullptr;
    systemArgument_ = nullptr;
    started_ = false;
    processDetached_ = false;
    reapInProgress_ = false;
    // The last handle may already be closed. Do not touch this object after
    // releasing the lifecycle reference, since that may destroy it.
    auto* owner = objectHeader_;
    objectHeader_ = nullptr;
    if (finalFlags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    if (owner) ob::ObDereferenceObject(owner);
    return NtStatus::Success;
}

}

namespace micant::sys {
NtStatus NtCreateThreadEx(Handle* threadHandle, uint32_t desiredAccess,
    ObjectAttributes* attributes, Handle processHandle, uintptr_t startRoutine,
    uintptr_t argument, uint32_t flags, uintptr_t zeroBits, size_t stackSize,
    size_t maximumStackSize, void* attributeList) {
    if (!threadHandle || !startRoutine || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL)
        return NtStatus::InvalidParameter;
    if (attributes) {
        if (attributes->length != sizeof(ObjectAttributes)) return NtStatus::InvalidParameter;
        if (attributes->rootDirectory || attributes->objectName || attributes->securityDescriptor ||
            attributes->securityQualityOfService || (attributes->attributes & ~OBJ_INHERIT)) return NtStatus::NotImplemented;
    }
    if ((flags & ~uint32_t{1}) || zeroBits) return NtStatus::NotImplemented;
    PsAttribute* clientAttribute{};
    PsAttribute* tebAttribute{};
    if (attributeList) {
        constexpr size_t prefix = offsetof(PsAttributeList, attributes);
        size_t totalLength{};
        std::memcpy(&totalLength, attributeList, sizeof(totalLength));
        if (totalLength < prefix || (totalLength - prefix) % sizeof(PsAttribute) ||
            (totalLength - prefix) / sizeof(PsAttribute) > 2) return NtStatus::InvalidParameter;
        const size_t count = (totalLength - prefix) / sizeof(PsAttribute);
        auto* entries = reinterpret_cast<PsAttribute*>(reinterpret_cast<uint8_t*>(attributeList) + prefix);
        for (size_t index = 0; index < count; ++index) {
            auto& item = entries[index];
            if (!item.value) return NtStatus::InvalidParameter;
            if (item.attribute == PS_ATTRIBUTE_CLIENT_ID && !clientAttribute && item.size == sizeof(ClientId))
                clientAttribute = &item;
            else if (item.attribute == PS_ATTRIBUTE_TEB_ADDRESS && !tebAttribute && item.size == sizeof(uintptr_t))
                tebAttribute = &item;
            else return NtStatus::NotImplemented;
        }
    }
    ps::EProcess* process{};
    ob::ObjectHeader* reference{};
    auto result = processHandle == InvalidHandleValue
        ? ps::detail::referenceCurrentProcess(process, reference)
        : ps::detail::referenceProcessHandle(processHandle, 0x0002, process, reference);
    if (result != NtStatus::Success) return result;
    Handle handle{};
    ps::EThread* thread{};
    ob::ObjectHeader* failedThreadHandle{};
    {
        ob::detail::HandleTableGuard guard;
        uintptr_t previousRoot{};
        __asm__ __volatile__("movq %%cr3, %0" : "=r"(previousRoot));
        bool switched = false;
        if (process->isTerminating()) result = NtStatus::ProcessIsTerminating;
        else if (!process->nativeImagesFinalized() || !process->nativeImages()) result = NtStatus::InvalidParameter;
        else if (!hal::amd64::switchProcessPagingRootByPhysical(process->getDirectoryTableBase()))
            result = NtStatus::Unsuccessful;
        else {
            switched = true;
            uint64_t entry{};
            const auto* images = process->nativeImages();
            for (size_t i = 0; i < images->size(); ++i) {
                const auto* module = images->module(i);
                if (!module || !hal::amd64::bootstrapUserRange(module->base, module->image.size(), false)) {
                    result = NtStatus::InvalidImageFormat;
                    break;
                }
            }
            const auto* image = images->module(0);
            if (!image) result = NtStatus::InvalidImageFormat;
            else if (result == NtStatus::Success) {
                const auto& optional = image->view.headers().optionalHeader;
                if (!maximumStackSize) maximumStackSize = optional.sizeOfStackReserve;
                if (!stackSize) stackSize = optional.sizeOfStackCommit;
                if (!stackSize || !maximumStackSize || stackSize > maximumStackSize ||
                    maximumStackSize > 1024 * 1024 ||
                    ((stackSize + mm::PageMask4KB) & ~mm::PageMask4KB) !=
                        ((maximumStackSize + mm::PageMask4KB) & ~mm::PageMask4KB))
                    result = NtStatus::NotImplemented;
                else result = images->resolve("ntdll.dll", "RtlUserThreadStart", 0, entry);
            }
            // Until user exception dispatch exists, an invalid start address
            // would otherwise turn a thread fault into a kernel-wide trap.
            if (result == NtStatus::Success && (!hal::amd64::bootstrapUserRange(entry, 1, false, true) ||
                !hal::amd64::bootstrapUserRange(startRoutine, 1, false, true)))
                result = NtStatus::BadInitialPc;
            if (result == NtStatus::Success)
                result = ps::detail::createUserThreadHandle(*process, entry, 32768, maximumStackSize,
                    hal::amd64::bootstrapSchedulerClockActive(), desiredAccess, handle, thread, stackSize,
                    attributes ? attributes->attributes : 0);
            if (result == NtStatus::Success) result = thread->configureUserStart(startRoutine, argument);
            if (result == NtStatus::Success && (flags & 1)) result = thread->suspendInitially();
        }
        if (switched && !hal::amd64::switchProcessPagingRootByPhysical(previousRoot)) __builtin_trap();
        if (result == NtStatus::Success && !(flags & 1)) result = thread->ready();
        if (result != NtStatus::Success && thread) {
            auto* failedThread = thread->kernelThread();
            if (!failedThread || failedThread->state != ke::ThreadState::Initialized ||
                failedThread->readyQueued || !failedThread->kernelApcList.empty() ||
                !failedThread->userApcList.empty()) __builtin_trap();
            // This thread was never published or started. Make it ineligible
            // for APC insertion and process termination before releasing BSP
            // exclusion; its lifecycle reference pins it through reap.
            failedThread->apcQueueable = false;
            failedThread->state = ke::ThreadState::Terminated;
            failedThreadHandle = ob::detail::currentHandleTable().takeHandle(handle);
            if (!failedThreadHandle) __builtin_trap();
        }
        if (result == NtStatus::Success) {
            if (clientAttribute) {
                *reinterpret_cast<ClientId*>(clientAttribute->value) = {process->processId(), thread->kernelThread()->tid};
                if (clientAttribute->returnLength) *clientAttribute->returnLength = sizeof(ClientId);
            }
            if (tebAttribute) {
                *reinterpret_cast<uintptr_t*>(tebAttribute->value) = thread->userTebAddress();
                if (tebAttribute->returnLength) *tebAttribute->returnLength = sizeof(uintptr_t);
            }
            *threadHandle = handle;
        }
    }
    if (failedThreadHandle) {
        if (thread->reap() != NtStatus::Success) __builtin_trap();
        (void)ob::ObFinishClose(failedThreadHandle, handle);
    }
    ob::ObDereferenceObject(reference);
    return result;
}

NtStatus NtResumeThread(Handle threadHandle, uint32_t* previousSuspendCount) {
    if (ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL) return NtStatus::InvalidParameter;
    ps::EThread* thread{};
    ob::ObjectHeader* reference{};
    const auto status = threadHandle == static_cast<Handle>(-2)
        ? ps::detail::referenceCurrentThread(thread, reference)
        : ps::detail::referenceThreadHandle(threadHandle, 0x0002, thread, reference);
    if (status != NtStatus::Success) return status;
    uint32_t previous{};
    const auto result = thread->resumeInitial(previous);
    ob::ObDereferenceObject(reference);
    if (result == NtStatus::Success && previousSuspendCount) *previousSuspendCount = previous;
    return result;
}

NtStatus NtCreateProcessEx(Handle* processHandle, uint32_t desiredAccess,
    ObjectAttributes* attributes, Handle parentProcess, uint32_t flags,
    Handle section, Handle debugPort, Handle exceptionPort, bool inJob) {
    if (!processHandle || !parentProcess || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL)
        return NtStatus::InvalidParameter;
    if (attributes) {
        if (attributes->length != sizeof(ObjectAttributes)) return NtStatus::InvalidParameter;
        if (attributes->rootDirectory || attributes->objectName || attributes->securityDescriptor ||
            attributes->securityQualityOfService || (attributes->attributes & ~OBJ_INHERIT)) return NtStatus::NotImplemented;
    }
    constexpr uint32_t inheritHandles = 0x00000004;
    if ((flags & ~inheritHandles) || section || debugPort || exceptionPort || inJob) return NtStatus::NotImplemented;
    ps::EProcess* parent{};
    ob::ObjectHeader* parentReference{};
    const auto status = parentProcess == InvalidHandleValue
        ? ps::detail::referenceCurrentProcess(parent, parentReference)
        : ps::detail::referenceProcessHandle(parentProcess, 0x0080, parent, parentReference);
    if (status != NtStatus::Success) return status;
    NtStatus result = NtStatus::NotImplemented;
    ob::ObjectHeader* failedHandleObject{};
    Handle failedHandle{};
    {
        ob::detail::HandleTableGuard guard;
        if (parent->isTerminating()) result = NtStatus::ProcessIsTerminating;
        else if (!parent->isSystemProcess() && !parent->nativeImages()) {
            // A non-system parent without an image section cannot supply the
            // inherited image required by this native creation form.
            result = NtStatus::ProcessIsTerminating;
        } else {
            ps::EProcess* child{};
            Handle handle{};
            // The child handle must not be inherited by the child itself.
            // Publish its requested attributes only after copying the parent table.
            result = ps::detail::createProcessHandle(desiredAccess, handle, child);
            if (result == NtStatus::Success && !parent->isSystemProcess()) {
                uintptr_t originalRoot{};
                __asm__ __volatile__("movq %%cr3, %0" : "=r"(originalRoot));
                result = parent->getAddressSpace().clonePrivateTo(child->getAddressSpace(),
                    parent->getDirectoryTableBase(), child->getDirectoryTableBase(),
                    hal::amd64::switchProcessPagingRootByPhysical);
                if (result == NtStatus::Success) {
                    if (!hal::amd64::switchProcessPagingRootByPhysical(child->getDirectoryTableBase()))
                        result = NtStatus::Unsuccessful;
                    else result = child->cloneNativeMetadata(*parent);
                }
                if (!hal::amd64::switchProcessPagingRootByPhysical(originalRoot)) __builtin_trap();
            }
            if (result == NtStatus::Success && (flags & inheritHandles)) {
                result = child->getHandleTable().inheritHandlesFrom(parent->getHandleTable());
            }
            if (result == NtStatus::Success && attributes && attributes->attributes)
                result = ob::detail::currentHandleTable().setHandleAttributes(handle, attributes->attributes);
            if (result != NtStatus::Success && handle) {
                // Withdraw the unpublished handle under exclusion, then run
                // close/delete callbacks only after leaving that exclusion.
                failedHandleObject = ob::detail::currentHandleTable().takeHandle(handle);
                if (!failedHandleObject) __builtin_trap();
                failedHandle = handle;
            }
            if (result == NtStatus::Success) *processHandle = handle;
        }
    }
    if (failedHandleObject) (void)ob::ObFinishClose(failedHandleObject, failedHandle);
    ob::ObDereferenceObject(parentReference);
    return result;
}

NtStatus NtTerminateThread(Handle threadHandle, NtStatus exitStatus) {
    constexpr uint32_t ThreadTerminateAccess = 0x0001;
    auto* current = ke::KeGetCurrentThread();
    if (!threadHandle) {
        if (!current || !current->owningThread) return NtStatus::InvalidHandle;
        uint64_t flags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        NtStatus result = NtStatus::CantTerminateSelf;
        if (current->owningProcess && current->owningProcess->activeThreadCount() > 1)
            result = current->owningThread->exitCurrent(exitStatus);
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return result;
    }
    if (threadHandle == static_cast<Handle>(-2)) {
        if (!current || !current->owningThread) return NtStatus::InvalidHandle;
        return current->owningThread->exitCurrent(exitStatus);
    }
    ob::ObjectHeader* reference{};
    ps::EThread* target{};
    const auto status = ps::detail::referenceThreadHandle(threadHandle, ThreadTerminateAccess,
                                                           target, reference);
    if (status != NtStatus::Success) return status;
    const bool self = current && current->owningThread == target;
    if (self) {
        // A running thread's lifecycle reference pins it across self-exit.
        ob::ObDereferenceObject(reference);
        return target->exitCurrent(exitStatus);
    }
    const auto result = target->isSystemThread() ? NtStatus::AccessDenied : target->terminateOther(exitStatus);
    ob::ObDereferenceObject(reference);
    return result;
}

NtStatus NtTerminateProcess(Handle processHandle, NtStatus exitStatus) {
    constexpr uint32_t ProcessTerminateAccess = 0x0001;
    auto* current = ke::KeGetCurrentThread();
    if (processHandle == InvalidHandleValue) {
        if (!current || !current->owningProcess) return NtStatus::InvalidParameter;
        return current->owningProcess->terminateCurrent(exitStatus);
    }
    if (!processHandle) {
        if (!current || !current->owningProcess) return NtStatus::InvalidParameter;
        return current->owningProcess->terminateOtherThreads(exitStatus);
    }
    ob::ObjectHeader* reference{};
    ps::EProcess* process{};
    const auto status = ps::detail::referenceProcessHandle(processHandle, ProcessTerminateAccess, process, reference);
    if (status != NtStatus::Success) return status;
    const bool isCurrent = current && current->owningProcess == process;
    if (isCurrent) {
        // The attached current thread pins its process until deferred reap.
        ob::ObDereferenceObject(reference);
        return process->terminateCurrent(exitStatus);
    }
    const auto result = process->terminateOther(exitStatus);
    ob::ObDereferenceObject(reference);
    return result;
}
}
