#include "micant/km/executive/ps.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/core/sync.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"
#include "micant/km/executive/usermode.hpp"
#include "micant/km/executive/probe.hpp"
#include "micant/km/executive/section.hpp"
#include "micant/km/executive/ps_objects.hpp"
#include "micant/shared/context.hpp"
#include <algorithm>
#include <cstring>
#include <iterator>

#if defined(MICANT_FREESTANDING) && (defined(_M_X64) || defined(__x86_64__))
extern "C" uint32_t MicantAmd64XsaveMask;
#endif

namespace micant::sys {
NtStatus referenceProcessImageSection(Handle handle, ob::ObjectHeader*& reference) noexcept;
NtStatus mapBootstrapFileImageView(mm::ProcessAddressSpace& space, uintptr_t& base,
                                   section::SectionObject& image,
                                   ob::ObjectHeader* sectionReference) noexcept;
}

namespace micant::ps {
namespace {
constexpr size_t MinimumUserExceptionStackCommit = 24 * 1024;
constexpr size_t MinimumUserExceptionStackReserve =
    MinimumUserExceptionStackCommit + mm::PageSize4KB;

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

// The only freestanding dispatcher wait currently publishes this descriptor.
// A system thread's bootstrap critical region is the one APC-disable nesting
// level the termination callback can safely leave before exiting the thread.
bool canTerminateEventWait(const ke::KThread& thread) noexcept {
    const bool onlyInitialRegion = thread.systemInitialRegionActive && thread.kernelApcDisable == 1;
    return thread.eventWaitInProgress && thread.eventWaitCanTerminate &&
        thread.eventWaitObject && thread.eventWaitBlock && !thread.eventWaitCancelled &&
        (!thread.kernelApcDisable || onlyInitialRegion) && !thread.specialApcDisable &&
        !thread.normalKernelApcActive;
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
    // The caller takes responsibility for the queue's reference after
    // removing an item, and must dereference it outside BSP exclusion.
    static bool removeProcess(EProcess& process) noexcept {
        if (!process.deferredQueued_) return false;
        EProcess* previous{};
        for (auto* item = processHead; item; item = item->deferredNext_) {
            if (item != &process) { previous = item; continue; }
            if (previous) previous->deferredNext_ = item->deferredNext_; else processHead = item->deferredNext_;
            if (processTail == item) processTail = previous;
            item->deferredNext_ = nullptr;
            item->deferredQueued_ = false;
            return true;
        }
        __builtin_trap();
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
            size_t threadBudget{};
            for (auto* item = head; item; item = item->deferredNext_) ++threadBudget;
            while (head && threadBudget) {
                --threadBudget;
                auto* thread = head;
                remove(*thread);
                if (thread->thread_.get() == ke::KeGetCurrentThread()) {
                    enqueue(*thread);
                    retry = true;
                    continue;
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
                if (incomplete) retry = true;
                // The object may have been destroyed by the last dereference.
            }
            size_t processBudget{};
            for (auto* item = processHead; item; item = item->deferredNext_) ++processBudget;
            while (processHead && processBudget) {
                --processBudget;
                auto* process = processHead;
                processHead = process->deferredNext_;
                if (!processHead) processTail = nullptr;
                process->deferredNext_ = nullptr;
                process->deferredQueued_ = false;
                process->synchronousCleanupRetry_ = false;
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
            size_t deleteBudget{};
            for (auto* item = deleteHead; item; item = item->deferredDeleteNext) ++deleteBudget;
            while (deleteHead && deleteBudget) {
                --deleteBudget;
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
EProcess::~EProcess() {
    if (primaryTokenReference_) {
        se::SeDereferenceFreestandingToken(primaryTokenReference_);
        primaryTokenReference_ = nullptr;
        primaryToken_ = nullptr;
    }
}

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
    activeAttachReferences_ = 0;
    terminating_ = false;
    processPublished_ = false;
    synchronousCleanupRetry_ = false;
    exitStatus_ = NtStatus::Pending;
#if defined(MICANT_BAREMETAL) && (defined(_M_X64) || defined(__x86_64__))
    (void)terminationEvent_.reset();
#endif
    imagesFinalized_ = false;
    userPeb_ = 0;
    userLoaderData_ = 0;
    userProcessParameters_ = 0;
    userApiSetMap_ = 0;
    userApiSetMapSize_ = 0;
    nativeImagePaths_ = {};
    processImagePath_ = {};
    processCommandLine_ = {};
    processParametersConfigured_ = false;
    imageSectionBacked_ = false;
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
    if (!root_.pfn || attachedThreads_ || activeAttachReferences_ || addressSpaceClosing_ ||
        observed == getDirectoryTableBase()) {
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
        owner.releaseNow();
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
    userApiSetMap_ = 0;
    userApiSetMapSize_ = 0;
    nativeImagePaths_ = {};
    processImagePath_ = {};
    processCommandLine_ = {};
    processParametersConfigured_ = false;
    imageSectionBacked_ = false;
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

NtStatus EProcess::referenceAddressSpaceForAttach(uintptr_t& directoryTableBase,
                                                   ob::ObjectHeader*& reference) noexcept {
    reference = nullptr;
    directoryTableBase = 0;
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    NtStatus status = NtStatus::Success;
    if (!root_.pfn || !objectHeader_ || terminating_ || addressSpaceClosing_ ||
        activeAttachReferences_ == SIZE_MAX) {
        status = NtStatus::InvalidParameter;
    } else {
        reference = objectHeader_;
        reference->addReference();
        ++activeAttachReferences_;
        directoryTableBase = getDirectoryTableBase();
    }
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return status;
}

void EProcess::releaseAddressSpaceAttachReference(ob::ObjectHeader* reference) noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    if (!reference || !activeAttachReferences_) __builtin_trap();
    --activeAttachReferences_;
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    ob::ObDereferenceObject(reference);
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
        // A never-started system thread is safe to queue for target-context
        // cleanup: startThread observes terminationRequested and skips its
        // driver routine. A started system thread is accepted below only when
        // it is blocked in a cancellable Ke event wait.
        if (!thread->owningThread->started_ && (thread->kernelApcDisable ||
            thread->specialApcDisable || thread->normalKernelApcActive))
            return NtStatus::NotImplemented;
        // A user-mode wait can be interrupted to deliver termination. A
        // kernel-mode wait must first return normally so its syscall releases
        // the waited object's reference before target-context rundown.
        const bool cancellable = waiting && thread->eventWaitUserMode &&
            canTerminateEventWait(*thread);
        if (thread->owningThread->isSystemThread() && thread->owningThread->started_ && !cancellable)
            return NtStatus::AccessDenied;
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
            !thread->eventWaitUserMode || !canTerminateEventWait(*thread)) continue;
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
    if (!root_.pfn) result = NtStatus::ProcessIsTerminating;
    else if (!current || current->owningProcess == this || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL)
        result = NtStatus::InvalidParameter;
    else if (terminating_ && synchronousCleanupRetry_ && !attachedThreads_ && !addressSpaceClosing_) {
        // A prior synchronous cleanup may have removed some resources before
        // failing. Retry the remaining teardown without undoing termination
        // or replacing its original exit status.
        synchronousCleanupRetry_ = false;
        if (hal::amd64::bootstrapSchedulerClockActive() && objectHeader_) {
            detail::ThreadReaper::enqueueProcess(*this);
            if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
            return NtStatus::Success;
        }
        const bool queuedReference = detail::ThreadReaper::removeProcess(*this);
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        result = closeAddressSpace();
        if (result != NtStatus::Success) {
            ob::detail::HandleTableGuard guard;
            synchronousCleanupRetry_ = true;
            if (objectHeader_) detail::ThreadReaper::enqueueProcess(*this);
        }
        if (result == NtStatus::Success) (void)terminationEvent_.set();
        if (queuedReference) ob::ObDereferenceObject(objectHeader_);
        return result;
    }
    else if (terminating_) result = NtStatus::ProcessIsTerminating;
    else if (attachedThreads_) result = requestTermination(exitStatus, nullptr);
    else {
        terminating_ = true;
        exitStatus_ = exitStatus;
        if (hal::amd64::bootstrapSchedulerClockActive() && objectHeader_) {
            detail::ThreadReaper::enqueueProcess(*this);
            result = NtStatus::Success;
        } else {
            // Early bootstrap has no worker. A non-Ob bootstrap process also
            // has no reference the worker could retain; close synchronously.
            if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
            result = closeAddressSpace();
            if (result != NtStatus::Success) {
                ob::detail::HandleTableGuard guard;
                synchronousCleanupRetry_ = true;
                if (objectHeader_) detail::ThreadReaper::enqueueProcess(*this);
            }
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
    if (impersonationTokenReference_) {
        se::SeDereferenceFreestandingToken(impersonationTokenReference_);
        impersonationTokenReference_ = nullptr;
        impersonationToken_ = nullptr;
    }
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
    if (!commitSize) commitSize = stackSize - mm::PageSize4KB;
    if (commitSize > stackSize || commitSize > SIZE_MAX - mm::PageMask4KB)
        return NtStatus::InvalidParameter;
    commitSize = (commitSize + mm::PageMask4KB) & ~mm::PageMask4KB;
    if (commitSize < mm::PageSize4KB || commitSize > stackSize - mm::PageSize4KB)
        return NtStatus::InvalidParameter;
    // The exception producer writes CONTEXT/CONTEXT_EX below the initial
    // user SP, and NTDLL needs bounded stack for its handler snapshot. Commit
    // that guarantee at thread creation so even a common 4/8 KiB StackCommit
    // can deliver an exception. Larger reservations retain their moving guard
    // below this initial commit; a smaller reserve cannot satisfy the ABI.
    if (stackSize < MinimumUserExceptionStackReserve) return NtStatus::InvalidParameter;
    if (commitSize < MinimumUserExceptionStackCommit)
        commitSize = MinimumUserExceptionStackCommit;
    if (commitSize > stackSize - mm::PageSize4KB) return NtStatus::InvalidParameter;
    auto status = initialize(process, kernelStackSize, startUser, this);
    if (status != NtStatus::Success) return status;
    auto& space = process.getAddressSpace();
    status = space.allocate(userStack_, stackSize, mm::MEM_RESERVE, mm::PAGE_READWRITE);
    if (status == NtStatus::Success) {
        uintptr_t committed = userStack_ + stackSize - commitSize;
        status = space.allocate(committed, commitSize, mm::MEM_COMMIT, mm::PAGE_READWRITE);
    }
    if (status == NtStatus::Success && commitSize < stackSize) {
        uintptr_t guardPage = userStack_ + stackSize - commitSize - mm::PageSize4KB;
        status = space.allocate(guardPage, mm::PageSize4KB, mm::MEM_COMMIT, mm::PAGE_READWRITE);
        if (status == NtStatus::Success) status = space.setStackGuard(guardPage, true);
        if (status == NtStatus::Success) userStackGuard_ = guardPage;
    }
    if (status == NtStatus::Success) {
        userStackLimit_ = userStack_ + stackSize - commitSize;
        userStackTop_ = userStack_ + stackSize;
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
    teb->deallocationStack = userStack_;
    teb->guaranteedStackBytes = static_cast<uint32_t>(mm::PageSize4KB);
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

NtStatus EThread::resolveUserStackGuard(uintptr_t address) noexcept {
    ob::detail::HandleTableGuard guard;
    if (!userStackGuard_ ||
        (address & ~static_cast<uintptr_t>(mm::PageMask4KB)) != userStackGuard_)
        return NtStatus::AccessViolation;
    uintptr_t currentRoot{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(currentRoot));
    if (!thread_ || ke::KeGetCurrentThread() != thread_.get() || !process_ ||
        thread_->owningProcess != process_ ||
        ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL || currentRoot != process_->getDirectoryTableBase() ||
        !userStackGuard_)
        return NtStatus::GuardPageViolation;
    if (!hal::amd64::bootstrapUserRange(userTeb_, sizeof(um::Teb64), true)) return NtStatus::AccessViolation;
    auto& space = process_->getAddressSpace();
    if (userStackGuard_ < userStack_ + mm::PageSize4KB) return NtStatus::StackOverflow;
    const uintptr_t next = userStackGuard_ - mm::PageSize4KB;
    const auto* vad = space.findVad(next);
    if (!vad || vad != space.findVad(userStackGuard_) || vad->startingAddress != userStack_ || vad->isSectionView)
        return NtStatus::GuardPageViolation;
    bool nextCommitted = false;
    for (auto* page = vad->commitRanges.get(); page; page = page->next.get()) {
        if (page->firstVpn != (next >> mm::PageShift4KB)) continue;
        if (page->protection != mm::PAGE_READWRITE) return NtStatus::GuardPageViolation;
        nextCommitted = true;
        break;
    }
    const bool exhausted = userStackGuard_ - userStack_ <= 2 * mm::PageSize4KB;
    if (nextCommitted && !exhausted) return NtStatus::GuardPageViolation;
    uintptr_t target = next;
    auto status = nextCommitted ? NtStatus::Success :
        space.allocate(target, mm::PageSize4KB, mm::MEM_COMMIT, mm::PAGE_READWRITE);
    if (status != NtStatus::Success) return status;
    if (!exhausted) status = space.setStackGuard(next, true);
    if (status == NtStatus::Success) status = space.setStackGuard(userStackGuard_, false);
    if (status != NtStatus::Success) {
        if (!nextCommitted && space.free(next, mm::PageSize4KB, mm::MEM_DECOMMIT) != NtStatus::Success)
            __builtin_trap();
        return status;
    }
    userStackLimit_ = exhausted ? next : userStackGuard_;
    reinterpret_cast<um::Teb64*>(userTeb_)->stackLimit = userStackLimit_;
    userStackGuard_ = exhausted ? 0 : next;
    return exhausted ? NtStatus::StackOverflow : NtStatus::Success;
}

NtStatus EThread::prepareUserException(uint32_t exceptionCode, uint32_t exceptionVector,
                                       uintptr_t address,
                                       uint64_t* hardwareFrame, uint64_t* savedRegisters,
                                       const void* floatingState) noexcept {
    ob::detail::HandleTableGuard guard;
    if (!thread_ || ke::KeGetCurrentThread() != thread_.get() || !process_ ||
        thread_->owningProcess != process_ || !hardwareFrame || !savedRegisters ||
        !floatingState || (hardwareFrame[2] & 3) != 3 ||
        ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL)
        return NtStatus::InvalidParameter;

    struct ExceptionRecord64 {
        uint32_t code, flags;
        uint64_t nestedRecord, exceptionAddress;
        uint32_t parameterCount, alignment;
        uint64_t information[15];
    };
    static_assert(sizeof(ExceptionRecord64) == 0x98);
    static_assert(sizeof(micant::CONTEXT) == 0x4d0);
    static_assert(sizeof(micant::CONTEXT_EX) == 0x20);
    constexpr size_t contextOffset = (sizeof(ExceptionRecord64) + 15) & ~size_t{15};

    uint32_t xsaveMask{};
    uint32_t extendedSize{512};
#if defined(MICANT_FREESTANDING) && (defined(_M_X64) || defined(__x86_64__))
    xsaveMask = MicantAmd64XsaveMask;
    extendedSize = ke::MicantAmd64ExtendedSize;
#endif
    const bool hasExtendedXstate = (xsaveMask & ~uint32_t{3}) != 0;
    if (hasExtendedXstate && (extendedSize < 576 || extendedSize > 4096))
        return NtStatus::NotSupported;

    const auto* images = process_->nativeImages();
    uint64_t dispatcher{};
    if (!images || !process_->nativeImagesFinalized() ||
        images->resolve("ntdll.dll", "KiUserExceptionDispatcher", 0, dispatcher) != NtStatus::Success ||
        !hal::amd64::bootstrapUserRange(dispatcher, 1, false, true) ||
        !hal::amd64::bootstrapUserRange(userTeb_, sizeof(um::Teb64), true))
        return NtStatus::ProcedureNotFound;

    const uintptr_t stackTop = static_cast<uintptr_t>(hardwareFrame[4]);
    // NTDLL's frame walker keeps one CONTEXT_EX/XSTATE snapshot while running
    // a language handler. Reserve bounded user stack for that snapshot plus
    // the handler's ABI home space and ordinary dispatcher calls.
    constexpr size_t dispatcherStackReserve = 16 * 1024;
    constexpr size_t xstateAlignmentSlack = 63;
    const size_t xstateLength = hasExtendedXstate ? extendedSize - 512 : 0;
    const size_t xstateOffsetSlack = hasExtendedXstate ? xstateAlignmentSlack : 1;
    const size_t fixedPayload = contextOffset + sizeof(micant::CONTEXT) +
                                sizeof(micant::CONTEXT_EX) + xstateOffsetSlack +
                                dispatcherStackReserve + 64;
    if (xstateLength > SIZE_MAX - fixedPayload) return NtStatus::StackOverflow;
    const size_t payloadSize = fixedPayload + xstateLength;
    if (payloadSize > SIZE_MAX - mm::PageMask4KB) return NtStatus::StackOverflow;
    const size_t guaranteeBytes = (payloadSize + mm::PageMask4KB) & ~mm::PageMask4KB;
    if (!userStack_ || userStackTop_ <= userStackLimit_ ||
        userStackLimit_ < userStack_ ||
        userStackLimit_ > UINTPTR_MAX - guaranteeBytes ||
        userStackTop_ - userStackLimit_ < guaranteeBytes || payloadSize > guaranteeBytes)
        return NtStatus::StackOverflow;
    const bool stackPointerInOwnedStack = stackTop >= userStackLimit_ && stackTop <= userStackTop_;
    uintptr_t frameAddress = stackPointerInOwnedStack && stackTop >= payloadSize
        ? (stackTop - payloadSize) & ~uintptr_t{15} : 0;
    const bool needsGuarantee = exceptionCode == static_cast<uint32_t>(NtStatus::StackOverflow) ||
        !frameAddress || frameAddress < userStackLimit_ ||
        !hal::amd64::bootstrapUserRange(frameAddress, payloadSize, true);
    if (needsGuarantee) {
        // Use the kernel-tracked committed bottom page; user-writable TEB
        // stack bounds and guaranteedStackBytes are advisory only.
        frameAddress = (userStackLimit_ + guaranteeBytes - payloadSize) & ~uintptr_t{15};
        if (frameAddress < userStackLimit_ || frameAddress > UINTPTR_MAX - payloadSize ||
            !hal::amd64::bootstrapUserRange(frameAddress, payloadSize, true))
            return NtStatus::StackOverflow;
    }

    auto* record = reinterpret_cast<ExceptionRecord64*>(frameAddress);
    auto* context = reinterpret_cast<micant::CONTEXT*>(frameAddress + contextOffset);
    auto* contextEx = reinterpret_cast<micant::CONTEXT_EX*>(
        reinterpret_cast<uintptr_t>(context) + sizeof(*context));
    *record = {};
    *contextEx = {};
    record->code = exceptionCode;
    record->exceptionAddress = exceptionCode == static_cast<uint32_t>(NtStatus::Breakpoint) &&
        hardwareFrame[1] ? hardwareFrame[1] - 1 : hardwareFrame[1];
    if (exceptionCode == static_cast<uint32_t>(NtStatus::AccessViolation) &&
        exceptionVector == 14) {
        // Only #PF supplies the architectural access type and faulting data
        // address. Other exceptions with STATUS_ACCESS_VIOLATION (for example
        // #GP/#SS) carry selector/control error codes in hardwareFrame[0].
        record->parameterCount = 2;
        record->information[0] = (hardwareFrame[0] & 16) ? 8 :
                                 (hardwareFrame[0] & 2) ? 1 : 0;
        record->information[1] = address;
    } else if (exceptionCode == static_cast<uint32_t>(NtStatus::GuardPageViolation)) {
        record->parameterCount = 1;
        record->information[0] = address;
    }

    *context = {};
    // The interrupt wrapper preserves the architecturally enabled x87/SSE
    // state in an XSAVE area. CONTEXT contains the legacy x87/SSE component;
    // the upper AVX/AVX-512 components use the separate Windows XSTATE APIs.
    // Do not advertise debug-register state here: the trap frame does not
    // capture DR0-DR7 and restoring zeroes would silently change it.
    context->ContextFlags = CONTEXT_FULL | CONTEXT_SEGMENTS |
        (hasExtendedXstate ? CONTEXT_XSTATE : 0);
    context->SegCs = static_cast<uint16_t>(hardwareFrame[2]);
    context->SegSs = static_cast<uint16_t>(hardwareFrame[5]);
    __asm__ __volatile__("movw %%ds, %0" : "=rm"(context->SegDs));
    __asm__ __volatile__("movw %%es, %0" : "=rm"(context->SegEs));
    __asm__ __volatile__("movw %%fs, %0" : "=rm"(context->SegFs));
    __asm__ __volatile__("movw %%gs, %0" : "=rm"(context->SegGs));
    context->EFlags = static_cast<uint32_t>(hardwareFrame[3]);
    context->Rip = hardwareFrame[1];
    context->Rsp = hardwareFrame[4];
    context->R15 = savedRegisters[0]; context->R14 = savedRegisters[1];
    context->R13 = savedRegisters[2]; context->R12 = savedRegisters[3];
    context->R11 = savedRegisters[4]; context->R10 = savedRegisters[5];
    context->R9 = savedRegisters[6];  context->R8 = savedRegisters[7];
    context->Rdi = savedRegisters[8]; context->Rsi = savedRegisters[9];
    context->Rbp = savedRegisters[10]; context->Rbx = savedRegisters[11];
    context->Rdx = savedRegisters[12]; context->Rcx = savedRegisters[13];
    context->Rax = savedRegisters[14];
    std::memcpy(&context->FltSave, floatingState, sizeof(context->FltSave));
    context->MxCsr = context->FltSave.MxCsr;

    contextEx->All.Offset = -static_cast<int32_t>(sizeof(*context));
    contextEx->All.Length = static_cast<uint32_t>(sizeof(*context) + sizeof(*contextEx));
    contextEx->Legacy.Offset = -static_cast<int32_t>(sizeof(*context));
    contextEx->Legacy.Length = sizeof(*context);
    if (hasExtendedXstate) {
        const uintptr_t contextExAddress = reinterpret_cast<uintptr_t>(contextEx);
        const uintptr_t xstateAddress =
            (contextExAddress + sizeof(*contextEx) + 63) & ~uintptr_t{63};
        const size_t xstateOffset = xstateAddress - contextExAddress;
        contextEx->XState.Offset = static_cast<int32_t>(xstateOffset);
        contextEx->XState.Length = static_cast<uint32_t>(xstateLength);
        contextEx->All.Length = static_cast<uint32_t>(sizeof(*context) + xstateOffset + xstateLength);
        std::memcpy(reinterpret_cast<void*>(xstateAddress),
                    static_cast<const uint8_t*>(floatingState) + 512, xstateLength);
    } else {
        // Public CONTEXT_EX convention for a context without an XState chunk.
        contextEx->XState.Offset = static_cast<int32_t>(sizeof(*contextEx) + 1);
    }

    // KiUserExceptionDispatcher(Record, Context) receives the same two
    // pointers as RCX/RDX and a Windows AMD64 entry stack alignment.
    hardwareFrame[1] = dispatcher;
    hardwareFrame[3] = (hardwareFrame[3] & ~uint64_t{0x3000}) | 2;
    hardwareFrame[4] = frameAddress + payloadSize - 8;
    savedRegisters[12] = reinterpret_cast<uintptr_t>(context);
    savedRegisters[13] = reinterpret_cast<uintptr_t>(record);
    savedRegisters[14] = 0;
    return NtStatus::Success;
}

extern "C" uint32_t MicantPsResolveUserStackFault(uintptr_t address, uint64_t errorCode,
                                                   uint64_t savedCs) noexcept {
    // A stack guard is a non-present, user data access. Reserved-bit,
    // instruction-fetch and supervisor faults belong to the normal trap path.
    if ((savedCs & 3) != 3 || (errorCode & ~uint64_t{6}) || !(errorCode & 4))
        return static_cast<uint32_t>(NtStatus::AccessViolation);
    auto* thread = ke::KeGetCurrentThread();
    return static_cast<uint32_t>(thread && thread->owningThread
        ? thread->owningThread->resolveUserStackGuard(address) : NtStatus::AccessViolation);
}

extern "C" bool MicantPsPrepareUserException(uint32_t exceptionCode,
                                              uint32_t exceptionVector,
                                              uintptr_t address,
                                              uint64_t* hardwareFrame,
                                              uint64_t* savedRegisters,
                                              const void* floatingState) noexcept {
    auto* current = ke::KeGetCurrentThread();
    return current && current->owningThread &&
        current->owningThread->prepareUserException(exceptionCode, exceptionVector, address,
                                                     hardwareFrame, savedRegisters,
                                                     floatingState) == NtStatus::Success;
}

extern "C" bool MicantPsHandleUserStackFault(uintptr_t address, uint64_t errorCode,
                                              uint64_t savedCs) noexcept {
    return MicantPsResolveUserStackFault(address, errorCode, savedCs) ==
        static_cast<uint32_t>(NtStatus::Success);
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
        userStackGuard_ = 0;
        userStackLimit_ = userStackTop_ = 0;
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
    const bool cancellableWait = thread_->state == ke::ThreadState::Waiting &&
        canTerminateEventWait(*thread_);
    // A running kernel thread has no safe termination boundary. A thread
    // blocked in the Ke event wait can exit in its own context after Ke has
    // unlinked the waiter and released the waited object's Ob reference.
    if (started_ && !userEntry_ && !cancellableWait) return NtStatus::NotImplemented;
    if (thread_->state == ke::ThreadState::Waiting) {
        if (!cancellableWait) return NtStatus::NotImplemented;
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
    // System threads start in a critical region to suppress ordinary APCs.
    // The termination callback runs at a safe wait-return boundary, so leave
    // that owned region before exitCurrent checks APC-disable state.
    const bool leaveInitialRegion = current->systemInitialRegionActive;
    if (leaveInitialRegion) ke::KeLeaveCriticalRegion();
    const auto result = self->exitCurrent(self->requestedExitStatus_);
    if (leaveInitialRegion && result != NtStatus::Success &&
        current->state == ke::ThreadState::Running && !current->systemInitialRegionActive) {
        ke::KeEnterCriticalRegion();
        current->systemInitialRegionActive = true;
    }
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
    const bool rundown = current->apcRundownComplete || ke::detail::rundownCurrentApcs();
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
    if (next && current->kernelApcList.empty() && current->userApcList.empty() &&
        !current->queuedApcReferences && !current->pendingNormalCount &&
        !current->normalKernelApcActive && !current->apcRundownActive) {
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
        thread_->queuedApcReferences ||
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
NtStatus NtContinue(const void* contextAddress, bool testAlert, void* trapFrameAddress,
                    void* floatingStateAddress, size_t floatingStateSize,
                    uint32_t xsaveMask) {
    using hal::amd64::SyscallFrame;
    auto* current = ke::KeGetCurrentThread();
    uintptr_t currentRoot{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(currentRoot));
    if (!contextAddress || !trapFrameAddress || !floatingStateAddress || !current ||
        !current->previousUserMode || !current->owningThread || !current->owningProcess ||
        current->owningThread->kernelThread() != current ||
        current->apcStateIndex != ke::ApcStateIndexOriginal || current->attachedProcess ||
        currentRoot != current->owningProcess->getDirectoryTableBase() ||
        ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL)
        return NtStatus::InvalidParameter;

    auto* frame = static_cast<SyscallFrame*>(trapFrameAddress);
    if ((frame->cs & 3) != 3 || frame->cs != 0x33 || frame->ss != 0x2b)
        return NtStatus::InvalidParameter;
    if (floatingStateSize < 512 || floatingStateSize > 4096)
        return NtStatus::NotSupported;

    constexpr uint32_t ContextAmd64 = 0x00100000;
    constexpr uint32_t ContextGroupMask = 0x0000ffff;
    constexpr uint32_t ContextAllowedGroups = 0x0000005f;
    alignas(64) micant::CONTEXT captured{};
    auto copyStatus = ex::ExSafeCopyFromUser(&captured, contextAddress,
                                              sizeof(captured), mm::UserMode);
    if (copyStatus != NtStatus::Success) return copyStatus;
    const uint32_t groups = captured.ContextFlags & ContextGroupMask;
    // The architecture bit identifies the CONTEXT layout; it is not an
    // exclusive tag in the upper half of ContextFlags. NT also carries
    // exception/service state bits there, and KeContextToTrapFrame ignores
    // those metadata bits while selecting the low-order register groups.
    if (!(captured.ContextFlags & ContextAmd64) ||
        (groups & ~ContextAllowedGroups))
        return NtStatus::InvalidParameter;
    if (groups & CONTEXT_DEBUG_REGISTERS) return NtStatus::NotSupported;

    if (groups & CONTEXT_CONTROL) {
        // KeContextToTrapFrame selects CS/SS with CONTROL and sanitizes
        // privileged RFLAGS bits rather than trusting them.
        if (captured.SegCs != 0x33 || captured.SegSs != 0x2b ||
            captured.Rip < um::MM_LOWEST_USER_ADDRESS ||
            captured.Rsp < um::MM_LOWEST_USER_ADDRESS + sizeof(uint64_t) ||
            !hal::amd64::bootstrapUserRange(captured.Rip, 1, false, true) ||
            !hal::amd64::bootstrapUserRange(captured.Rsp - sizeof(uint64_t),
                                             sizeof(uint64_t), true))
            return NtStatus::InvalidParameter;
    }

    if (groups & CONTEXT_SEGMENTS) {
        uint16_t ds{}, es{}, fs{}, gs{};
        __asm__ __volatile__("movw %%ds, %0" : "=rm"(ds));
        __asm__ __volatile__("movw %%es, %0" : "=rm"(es));
        __asm__ __volatile__("movw %%fs, %0" : "=rm"(fs));
        __asm__ __volatile__("movw %%gs, %0" : "=rm"(gs));
        if (captured.SegDs != ds || captured.SegEs != es ||
            captured.SegFs != fs || captured.SegGs != gs)
            return NtStatus::InvalidParameter;
    }

    alignas(64) uint8_t stagedXstate[4096]{};
    std::memcpy(stagedXstate, floatingStateAddress, floatingStateSize);
    uint32_t mxcsrMask{};
    std::memcpy(&mxcsrMask, stagedXstate + offsetof(micant::XMM_SAVE_AREA32, MxCsr_Mask),
                sizeof(mxcsrMask));
    if (!mxcsrMask) mxcsrMask = 0x0000ffbfu;
    if (groups & CONTEXT_FLOATING_POINT) {
        if ((captured.MxCsr & ~mxcsrMask) ||
            (captured.FltSave.ControlWord & ~uint16_t{0x1f3f}))
            return NtStatus::InvalidParameter;
        captured.FltSave.MxCsr = captured.MxCsr;
    }
    if (groups & CONTEXT_XSTATE) {
        if (!(xsaveMask & ~uint32_t{3}) || floatingStateSize < 576)
            return NtStatus::NotSupported;
        const uintptr_t sourceAddress = reinterpret_cast<uintptr_t>(contextAddress);
        if (sourceAddress > UINTPTR_MAX - sizeof(captured)) return NtStatus::AccessViolation;
        const uintptr_t contextExAddress = sourceAddress + sizeof(captured);
        micant::CONTEXT_EX contextEx{};
        copyStatus = ex::ExSafeCopyFromUser(&contextEx,
            reinterpret_cast<const void*>(contextExAddress), sizeof(contextEx), mm::UserMode);
        if (copyStatus != NtStatus::Success) return copyStatus;
        const int32_t expectedContextOffset = -static_cast<int32_t>(sizeof(captured));
        const size_t maximumTail = floatingStateSize - 512;
        const size_t xstateLength = contextEx.XState.Length;
        if (contextEx.All.Length < sizeof(captured) + sizeof(contextEx) ||
            contextEx.All.Offset != expectedContextOffset ||
            contextEx.Legacy.Offset != expectedContextOffset ||
            contextEx.Legacy.Length != sizeof(captured) ||
            contextEx.XState.Offset < static_cast<int32_t>(sizeof(contextEx)) ||
            xstateLength < sizeof(micant::XSTATE_HEADER) || xstateLength > maximumTail ||
            static_cast<uint64_t>(contextEx.XState.Offset) + xstateLength >
                contextEx.All.Length - sizeof(captured) ||
            contextExAddress > UINTPTR_MAX - static_cast<uint32_t>(contextEx.XState.Offset))
            return NtStatus::InvalidParameter;
        const uintptr_t xstateAddress = contextExAddress +
            static_cast<uint32_t>(contextEx.XState.Offset);
        if (xstateAddress > UINTPTR_MAX - xstateLength || (xstateAddress & 63) ||
            ex::ExSafeCopyFromUser(stagedXstate + 512,
                reinterpret_cast<const void*>(xstateAddress), xstateLength, mm::UserMode) != NtStatus::Success)
            return NtStatus::AccessViolation;
        micant::XSTATE_HEADER xstateHeader{};
        std::memcpy(&xstateHeader, stagedXstate + 512, sizeof(xstateHeader));
        if (xstateHeader.Mask & ~static_cast<uint64_t>(xsaveMask))
            return NtStatus::InvalidParameter;
        if (xstateHeader.CompactionMask) {
            constexpr uint64_t compactedLayout = uint64_t{1} << 63;
            if (!(xstateHeader.CompactionMask & compactedLayout) ||
                (xstateHeader.CompactionMask &
                    ~(compactedLayout | static_cast<uint64_t>(xsaveMask))) ||
                (xstateHeader.Mask &
                    ~(xstateHeader.CompactionMask & ~compactedLayout)))
                return NtStatus::InvalidParameter;
            // The bootstrap XSAVE buffers and current feature walker use the
            // standard CPUID offsets. A valid compacted context needs the
            // compacted CPUID offsets and remains explicitly unsupported.
            return NtStatus::NotSupported;
        }
        if (std::any_of(std::begin(xstateHeader.Reserved), std::end(xstateHeader.Reserved),
                        [](uint64_t value) { return value != 0; }))
            return NtStatus::InvalidParameter;
        // In the standard, non-compacted XSAVE layout each feature retains its
        // CPUID leaf 0xD subleaf offset. A subset context may end after its
        // highest included feature; every component marked present in
        // XSTATE_BV must still fit wholly inside that bounded prefix.
        for (uint32_t feature = 2; feature < 32; ++feature) {
            const uint32_t featureBit = uint32_t{1} << feature;
            if (!(xstateHeader.Mask & featureBit)) continue;
            uint32_t eax = 0x0d, ebx{}, ecx = feature, edx{};
            __asm__ __volatile__("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
            const size_t componentSize = eax;
            const size_t componentOffset = ebx;
            if (!componentSize || componentOffset < 576 ||
                componentOffset > floatingStateSize ||
                componentSize > floatingStateSize - componentOffset ||
                componentOffset - 512 > xstateLength ||
                componentSize > xstateLength - (componentOffset - 512))
                return NtStatus::InvalidParameter;
        }
    }

    // All user data and control targets are validated before the live return
    // frame or XSAVE image is changed. NtContinue success returns through this
    // IRET frame instead of resuming after the syscall instruction.
    SyscallFrame next = *frame;
    if (groups & CONTEXT_INTEGER) {
        next.r15 = captured.R15; next.r14 = captured.R14;
        next.r13 = captured.R13; next.r12 = captured.R12;
        next.r10 = captured.R10; next.r9 = captured.R9;
        next.r8 = captured.R8; next.rdx = captured.Rdx;
        next.rsi = captured.Rsi; next.rdi = captured.Rdi;
        next.rbp = captured.Rbp; next.rbx = captured.Rbx;
        next.rax = captured.Rax; next.rcx = captured.Rcx;
        next.r11 = captured.R11;
    }
    if (groups & CONTEXT_CONTROL) {
        next.rip = captured.Rip; next.rsp = captured.Rsp;
        // KeContextToTrapFrame forces the architectural reserved bit and IF
        // for user continuation; the syscall exit path still masks every
        // user-unsafe flag before IRET.
        next.flags = static_cast<uint64_t>(captured.EFlags) | 0x202;
        next.cs = captured.SegCs;
        next.ss = captured.SegSs;
    }
    if (groups & CONTEXT_FLOATING_POINT) {
        std::memcpy(stagedXstate, &captured.FltSave, sizeof(captured.FltSave));
        std::memcpy(stagedXstate + offsetof(micant::XMM_SAVE_AREA32, MxCsr_Mask),
                    &mxcsrMask, sizeof(mxcsrMask));
    }
    if (groups & (CONTEXT_FLOATING_POINT | CONTEXT_XSTATE))
        std::memcpy(floatingStateAddress, stagedXstate, floatingStateSize);
    *frame = next;

    if (testAlert) (void)ke::KeTestAlertThread(ke::UserMode);
    return NtStatus::Success;
}

NtStatus NtCreateThreadEx(Handle* threadHandle, uint32_t desiredAccess,
    ObjectAttributes* attributes, Handle processHandle, uintptr_t startRoutine,
    uintptr_t argument, uint32_t flags, uintptr_t zeroBits, size_t stackSize,
    size_t maximumStackSize, void* attributeList) {
    if (!threadHandle || !startRoutine || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL)
        return NtStatus::InvalidParameter;
    auto* caller = ke::KeGetCurrentThread();
    const bool userCaller = caller && caller->previousUserMode;
    const auto pointerMode = [userCaller, caller](const void* pointer, size_t length) noexcept {
        if (!userCaller) return mm::KernelMode;
        const auto address = reinterpret_cast<uintptr_t>(pointer);
        const auto end = address + length;
        const bool kernelStackBuffer = caller && caller->kernelStackBase &&
            caller->kernelStackTop > caller->kernelStackBase && end >= address &&
            address >= caller->kernelStackBase && end <= caller->kernelStackTop;
        return kernelStackBuffer ? mm::KernelMode : mm::UserMode;
    };
    const auto withInterruptsDisabled = [](auto&& operation) noexcept {
        uint64_t flags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        const auto status = operation();
        if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return status;
    };
    const auto safeCopyIn = [&pointerMode, &withInterruptsDisabled](void* destination, const void* source,
                                           size_t length) noexcept {
        return withInterruptsDisabled([&]() noexcept {
            return ex::ExSafeCopyFromUser(destination, source, length, pointerMode(source, length));
        });
    };
    const auto safeCopyOut = [&pointerMode, &withInterruptsDisabled](void* destination, const void* source,
                                            size_t length) noexcept {
        return withInterruptsDisabled([&]() noexcept {
            return ex::ExSafeCopyToUser(destination, source, length, pointerMode(destination, length));
        });
    };
    const auto probeRead = [&pointerMode, &withInterruptsDisabled](const void* address,
        size_t size, size_t alignment) noexcept {
        return withInterruptsDisabled([&]() noexcept {
            return ex::ProbeForRead(address, size, alignment, pointerMode(address, size));
        });
    };
    const auto probeWrite = [&pointerMode, &withInterruptsDisabled](void* address,
        size_t size, size_t alignment) noexcept {
        return withInterruptsDisabled([&]() noexcept {
            return ex::ProbeForWrite(address, size, alignment, pointerMode(address, size));
        });
    };
    struct CapturedAttribute {
        uintptr_t attribute{};
        uintptr_t output{};
        size_t outputSize{};
        size_t* returnLength{};
        size_t returnedSize{};
    };
    std::array<CapturedAttribute, 2> capturedAttributes{};
    size_t attributeCount{};
    Handle capturedThreadHandle{};
    bool threadHandleOutputAttempted{};
    const auto threadHandleProbe = probeWrite(threadHandle, sizeof(*threadHandle), alignof(Handle));
    if (threadHandleProbe != NtStatus::Success) return threadHandleProbe;
    ObjectAttributes capturedObjectAttributes{};
    if (attributes) {
        const auto copyStatus = safeCopyIn(&capturedObjectAttributes, attributes, sizeof(capturedObjectAttributes));
        if (copyStatus != NtStatus::Success) return copyStatus;
        if (capturedObjectAttributes.length != sizeof(ObjectAttributes)) return NtStatus::InvalidParameter;
        if (capturedObjectAttributes.rootDirectory || capturedObjectAttributes.objectName ||
            capturedObjectAttributes.securityDescriptor || capturedObjectAttributes.securityQualityOfService ||
            (capturedObjectAttributes.attributes & ~OBJ_INHERIT)) return NtStatus::NotImplemented;
    }
    if ((flags & ~uint32_t{1}) || zeroBits) return NtStatus::NotImplemented;
    if (attributeList) {
        constexpr size_t prefix = offsetof(PsAttributeList, attributes);
        size_t totalLength{};
        auto copyStatus = safeCopyIn(&totalLength, attributeList, sizeof(totalLength));
        if (copyStatus != NtStatus::Success) return copyStatus;
        if (totalLength < prefix || (totalLength - prefix) % sizeof(PsAttribute) ||
            (totalLength - prefix) / sizeof(PsAttribute) > capturedAttributes.size())
            return NtStatus::InvalidParameter;
        if (reinterpret_cast<uintptr_t>(attributeList) > UINTPTR_MAX - totalLength)
            return NtStatus::AccessViolation;
        const auto fullListProbe = probeRead(attributeList, totalLength, alignof(size_t));
        if (fullListProbe != NtStatus::Success) return fullListProbe;
        attributeCount = (totalLength - prefix) / sizeof(PsAttribute);
        std::array<PsAttribute, 2> entries{};
        bool clientIdSeen{};
        bool tebSeen{};
        if (attributeCount) {
            const auto* entrySource = reinterpret_cast<const uint8_t*>(attributeList) + prefix;
            copyStatus = safeCopyIn(entries.data(), entrySource, attributeCount * sizeof(PsAttribute));
            if (copyStatus != NtStatus::Success) return copyStatus;
        }
        for (size_t index = 0; index < attributeCount; ++index) {
            const auto& item = entries[index];
            if (!item.value) return NtStatus::InvalidParameter;
            auto& captured = capturedAttributes[index];
            captured.attribute = item.attribute;
            captured.output = item.value;
            captured.outputSize = item.size;
            captured.returnLength = item.returnLength;
            if (item.attribute == PS_ATTRIBUTE_CLIENT_ID && item.size == sizeof(ClientId) && !clientIdSeen) {
                captured.returnedSize = sizeof(ClientId);
                clientIdSeen = true;
            } else if (item.attribute == PS_ATTRIBUTE_TEB_ADDRESS && item.size == sizeof(uintptr_t) && !tebSeen) {
                captured.returnedSize = sizeof(uintptr_t);
                tebSeen = true;
            } else if (item.attribute == PS_ATTRIBUTE_CLIENT_ID || item.attribute == PS_ATTRIBUTE_TEB_ADDRESS) {
                return NtStatus::InvalidParameter;
            } else return NtStatus::NotImplemented;
            const auto valueProbe = probeWrite(reinterpret_cast<void*>(item.value), item.size,
                alignof(uintptr_t));
            if (valueProbe != NtStatus::Success) return valueProbe;
            if (item.returnLength) {
                const auto lengthProbe = probeWrite(item.returnLength, sizeof(size_t), alignof(size_t));
                if (lengthProbe != NtStatus::Success) return lengthProbe;
            }
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
                const size_t roundedReserve = maximumStackSize <= SIZE_MAX - mm::PageMask4KB
                    ? (maximumStackSize + mm::PageMask4KB) & ~mm::PageMask4KB : 0;
                const size_t roundedCommit = stackSize <= SIZE_MAX - mm::PageMask4KB
                    ? (stackSize + mm::PageMask4KB) & ~mm::PageMask4KB : SIZE_MAX;
                if (!stackSize || !maximumStackSize || stackSize > maximumStackSize ||
                    maximumStackSize > 1024 * 1024 ||
                    roundedReserve < ps::MinimumUserExceptionStackReserve ||
                    roundedCommit > roundedReserve - mm::PageSize4KB)
                    result = NtStatus::InvalidParameter;
                else result = images->resolve("ntdll.dll", "RtlUserThreadStart", 0, entry);
            }
            // Reject a bad initial PC before publishing the thread; later
            // user execution faults use the exception dispatcher.
            if (result == NtStatus::Success && (!hal::amd64::bootstrapUserRange(entry, 1, false, true) ||
                !hal::amd64::bootstrapUserRange(startRoutine, 1, false, true)))
                result = NtStatus::BadInitialPc;
            if (result == NtStatus::Success)
                result = ps::detail::createUserThreadHandle(*process, entry, 32768, maximumStackSize,
                    hal::amd64::bootstrapSchedulerClockActive(), desiredAccess, handle, thread, stackSize,
                    attributes ? capturedObjectAttributes.attributes : 0);
            if (result == NtStatus::Success) result = thread->configureUserStart(startRoutine, argument);
            if (result == NtStatus::Success && (flags & 1)) result = thread->suspendInitially();
        }
        if (switched && !hal::amd64::switchProcessPagingRootByPhysical(previousRoot)) __builtin_trap();
        if (result == NtStatus::Success) {
            const ClientId clientId{process->processId(), thread->kernelThread()->tid};
            const uintptr_t teb = thread->userTebAddress();
            for (size_t index = 0; index < attributeCount && result == NtStatus::Success; ++index) {
                auto& captured = capturedAttributes[index];
                const void* value = captured.attribute == PS_ATTRIBUTE_CLIENT_ID
                    ? static_cast<const void*>(&clientId) : static_cast<const void*>(&teb);
                result = safeCopyOut(reinterpret_cast<void*>(captured.output), value, captured.outputSize);
                if (result == NtStatus::Success && captured.returnLength) {
                    captured.returnedSize = captured.outputSize;
                    result = safeCopyOut(captured.returnLength, &captured.returnedSize, sizeof(size_t));
                }
            }
            if (result == NtStatus::Success) {
                capturedThreadHandle = handle;
                threadHandleOutputAttempted = true;
                result = safeCopyOut(threadHandle, &capturedThreadHandle, sizeof(capturedThreadHandle));
            }
        }
        if (result == NtStatus::Success && !(flags & 1)) result = thread->ready();
        if (result != NtStatus::Success && thread) {
            if (threadHandleOutputAttempted) {
                const Handle invalidHandle{};
                (void)safeCopyOut(threadHandle, &invalidHandle, sizeof(invalidHandle));
            }
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
    auto* caller = ke::KeGetCurrentThread();
    const bool userCaller = caller && caller->previousUserMode;
    const auto pointerMode = [userCaller, caller](const void* pointer, size_t length) noexcept {
        if (!userCaller) return mm::KernelMode;
        const auto address = reinterpret_cast<uintptr_t>(pointer);
        const auto end = address + length;
        const bool kernelStackBuffer = caller && caller->kernelStackBase &&
            caller->kernelStackTop > caller->kernelStackBase && end >= address &&
            address >= caller->kernelStackBase && end <= caller->kernelStackTop;
        return kernelStackBuffer ? mm::KernelMode : mm::UserMode;
    };
    const auto withInterruptsDisabled = [](auto&& operation) noexcept {
        uint64_t interruptFlags{};
        __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(interruptFlags) : : "memory");
        const auto status = operation();
        if (interruptFlags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
        return status;
    };
    const auto safeCopyIn = [&](void* destination, const void* source, size_t length) noexcept {
        return withInterruptsDisabled([&]() noexcept {
            return ex::ExSafeCopyFromUser(destination, source, length, pointerMode(source, length));
        });
    };
    const auto safeCopyOut = [&](void* destination, const void* source, size_t length) noexcept {
        return withInterruptsDisabled([&]() noexcept {
            return ex::ExSafeCopyToUser(destination, source, length, pointerMode(destination, length));
        });
    };
    const auto probeWrite = [&](void* destination, size_t length, size_t alignment) noexcept {
        return withInterruptsDisabled([&]() noexcept {
            return ex::ProbeForWrite(destination, length, alignment, pointerMode(destination, length));
        });
    };
    const auto outputProbe = probeWrite(processHandle, sizeof(*processHandle), alignof(Handle));
    if (outputProbe != NtStatus::Success) return outputProbe;
    ObjectAttributes capturedAttributes{};
    if (attributes) {
        const auto captureStatus = safeCopyIn(&capturedAttributes, attributes, sizeof(capturedAttributes));
        if (captureStatus != NtStatus::Success) return captureStatus;
        if (capturedAttributes.length != sizeof(ObjectAttributes)) return NtStatus::InvalidParameter;
        if (capturedAttributes.rootDirectory || capturedAttributes.objectName ||
            capturedAttributes.securityDescriptor || capturedAttributes.securityQualityOfService ||
            (capturedAttributes.attributes & ~OBJ_INHERIT)) return NtStatus::NotImplemented;
        attributes = &capturedAttributes;
    }
    constexpr uint32_t inheritHandles = 0x00000004;
    if ((flags & ~inheritHandles) || debugPort || exceptionPort || inJob) return NtStatus::NotImplemented;
    ps::EProcess* parent{};
    ob::ObjectHeader* parentReference{};
    const auto status = parentProcess == InvalidHandleValue
        ? ps::detail::referenceCurrentProcess(parent, parentReference)
        : ps::detail::referenceProcessHandle(parentProcess, 0x0080, parent, parentReference);
    if (status != NtStatus::Success) return status;
    ob::ObjectHeader* imageSectionReference{};
    section::SectionObject* imageSection{};
    if (section) {
        const auto sectionStatus = referenceProcessImageSection(section, imageSectionReference);
        if (sectionStatus != NtStatus::Success) {
            ob::ObDereferenceObject(parentReference);
            return sectionStatus;
        }
        imageSection = imageSectionReference->as<section::SectionObject>();
    }
    NtStatus result = NtStatus::NotImplemented;
    {
        ob::detail::HandleTableGuard guard;
        if (parent->isTerminating()) result = NtStatus::ProcessIsTerminating;
        else result = NtStatus::Success;
    }

    struct ParentAddressSpacePin {
        ps::EProcess* process{};
        ob::ObjectHeader* reference{};
        uintptr_t root{};
        ~ParentAddressSpacePin() {
            if (reference) process->releaseAddressSpaceAttachReference(reference);
        }
    } parentAddressSpace;
    if (result == NtStatus::Success && !imageSection) {
        parentAddressSpace.process = parent;
        result = parent->referenceAddressSpaceForAttach(parentAddressSpace.root,
                                                          parentAddressSpace.reference);
        if (result != NtStatus::Success)
            result = parent->isTerminating() ? NtStatus::ProcessIsTerminating : result;
        // Keep the address space and optional image metadata pinned before
        // reading either; kernel-only parents may legitimately have no image
        // set, while their private address space can still be cloned.
    }

    ps::EProcess* child{};
    ob::ObjectHeader* childReference{};
    Handle handle{};
    if (result == NtStatus::Success)
        result = ps::detail::createProcessObject(child, childReference);
    if (result == NtStatus::Success)
        result = ps::detail::inheritProcessSecurityContext(*child, *parent);
    if (result == NtStatus::Success && imageSection) {
        uintptr_t originalRoot{};
        __asm__ __volatile__("movq %%cr3, %0" : "=r"(originalRoot));
        if (!hal::amd64::switchProcessPagingRootByPhysical(child->getDirectoryTableBase()))
            result = NtStatus::Unsuccessful;
        else {
            result = child->initializeMainImageFromSection(*imageSection, imageSectionReference);
        }
        if (!hal::amd64::switchProcessPagingRootByPhysical(originalRoot)) __builtin_trap();
    } else if (result == NtStatus::Success) {
        uintptr_t originalRoot{};
        __asm__ __volatile__("movq %%cr3, %0" : "=r"(originalRoot));
        result = parent->getAddressSpace().clonePrivateTo(child->getAddressSpace(),
            parentAddressSpace.root, child->getDirectoryTableBase(),
            hal::amd64::switchProcessPagingRootByPhysical);
        if (result == NtStatus::Success) {
            if (!hal::amd64::switchProcessPagingRootByPhysical(child->getDirectoryTableBase()))
                result = NtStatus::Unsuccessful;
            else result = child->cloneNativeMetadata(*parent);
        }
        if (!hal::amd64::switchProcessPagingRootByPhysical(originalRoot)) __builtin_trap();
    }
    if (result == NtStatus::Success && (flags & inheritHandles))
        result = child->getHandleTable().inheritHandlesFrom(parent->getHandleTable());
    if (result == NtStatus::Success)
        result = ps::detail::publishProcessHandle(*child, childReference, desiredAccess,
            attributes ? attributes->attributes : 0, handle);
    if (result == NtStatus::Success) {
        result = safeCopyOut(processHandle, &handle, sizeof(handle));
        if (result != NtStatus::Success) {
            auto* removed = ob::detail::currentHandleTable().takeHandle(handle);
            if (!removed) __builtin_trap();
            const auto closeStatus = ob::ObFinishClose(removed, handle);
            if (closeStatus != NtStatus::Success) __builtin_trap();
        }
    }
    if (childReference) ob::ObDereferenceObject(childReference);
    if (imageSectionReference) ob::ObDereferenceObject(imageSectionReference);
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

namespace {
NtStatus terminateCurrentProcessPseudoHandle(NtStatus exitStatus) noexcept {
    auto* current = ke::KeGetCurrentThread();
    if (!current || !current->owningProcess ||
        ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL)
        return NtStatus::InvalidParameter;
    // The current-process pseudo-handle does not pass through a handle table.
    // The running thread pins its owning process, including bootstrap process
    // objects which have not been published through Ob.
    auto* process = current->owningProcess;
    if (process->isSystemProcess()) {
        return NtStatus::AccessDenied;
    }

    // NULL and the current-process pseudo-handle identify the caller's own
    // process. Ordinary handles still go through the PROCESS_TERMINATE check.
    return process->terminateCurrent(exitStatus);
}
}

NtStatus NtTerminateProcess(Handle processHandle, NtStatus exitStatus) {
    constexpr uint32_t ProcessTerminateAccess = 0x0001;
    if (!processHandle || processHandle == InvalidHandleValue)
        return terminateCurrentProcessPseudoHandle(exitStatus);
    auto* current = ke::KeGetCurrentThread();
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
