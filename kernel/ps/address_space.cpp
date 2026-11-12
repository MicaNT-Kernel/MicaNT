#include "micant/km/executive/ps.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/core/sync.hpp"
#include "micant/km/executive/usermode.hpp"
#include "micant/km/executive/ps_objects.hpp"

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
        mapBootstrapProtectManagedPage, mapBootstrapManagedView, unmapBootstrapManagedView};
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

NtStatus EProcess::requestTermination(NtStatus exitStatus, ke::KThread* callingThread) noexcept {
    // Caller holds BSP interrupt exclusion throughout the preflight and commit.
    if (!root_.pfn || terminating_) return NtStatus::ProcessIsTerminating;
    bool willHaveNext = ke::PriorityScheduler::get().getReadyThreadCount() || ke::g_CurrentCpu.idleThread;
    for (auto* thread = threads_; thread; thread = thread->processNext) {
        if (thread == callingThread || thread->state == ke::ThreadState::Terminated) continue;
        const bool waiting = thread->state == ke::ThreadState::Waiting;
        const bool ready = thread->state == ke::ThreadState::Ready && thread->readyQueued;
        const bool unstarted = thread->owningThread && !thread->owningThread->started_ &&
            thread->state == ke::ThreadState::Initialized && !thread->readyQueued;
        if (!thread->owningThread || (!unstarted && !waiting && !ready)) return NtStatus::NotImplemented;
        const bool cancellable = waiting && thread->eventWaitInProgress && thread->eventWaitUserMode &&
            thread->eventWaitCanTerminate && thread->eventWaitObject &&
            thread->eventWaitBlock && !thread->eventWaitCancelled && !thread->kernelApcDisable &&
            !thread->specialApcDisable && !thread->normalKernelApcActive;
        if (unstarted || ready || cancellable) willHaveNext = true;
    }
    // A self-termination cannot commit without a runnable replacement stack.
    if (callingThread && !willHaveNext) return NtStatus::Unsuccessful;
    bool success = true;
    for (auto* thread = threads_; thread; thread = thread->processNext) {
        if (thread == callingThread || thread->state != ke::ThreadState::Waiting ||
            !thread->eventWaitInProgress || !thread->eventWaitUserMode || !thread->eventWaitCanTerminate ||
            !thread->eventWaitObject || !thread->eventWaitBlock ||
            thread->eventWaitCancelled || thread->kernelApcDisable || thread->specialApcDisable ||
            thread->normalKernelApcActive) continue;
        if (!static_cast<sync::detail::BootstrapEvent*>(thread->eventWaitObject)->cancelWaitForTermination(*thread)) {
            success = false;
            break;
        }
    }
    if (success) {
        for (auto* thread = threads_; thread; thread = thread->processNext) {
            if (thread == callingThread || thread->state != ke::ThreadState::Initialized) continue;
            if (!ke::PriorityScheduler::get().readyKThread(thread)) { success = false; break; }
            thread->terminationQueuedByProcess = true;
        }
    }
    if (!success) {
        for (auto* thread = threads_; thread; thread = thread->processNext) {
            if (thread == callingThread) continue;
            if (thread->eventWaitCancelled &&
                !static_cast<sync::detail::BootstrapEvent*>(thread->eventWaitObject)->restoreCancelledWait(*thread))
                __builtin_trap();
            if (thread->terminationQueuedByProcess) {
                if (!ke::PriorityScheduler::get().removeReadyThread(thread)) __builtin_trap();
                thread->state = ke::ThreadState::Initialized;
                thread->terminationQueuedByProcess = false;
            }
        }
        return NtStatus::Unsuccessful;
    }
    terminating_ = true;
    exitStatus_ = exitStatus;
    for (auto* thread = threads_; thread; thread = thread->processNext) {
        thread->terminationQueuedByProcess = false;
        if (thread->state != ke::ThreadState::Terminated) thread->terminationRequested = true;
    }
    return NtStatus::Success;
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
    return NtStatus::Success;
}

NtStatus EThread::initializeSystem(EProcess& process, size_t stackSize,
                                    SystemStartRoutine start, void* argument) noexcept {
    if (!start) return NtStatus::InvalidParameter;
    ob::detail::HandleTableGuard guard;
    const auto status = initialize(process, stackSize, startSystem, this);
    if (status == NtStatus::Success) {
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
        thread_->state != ke::ThreadState::Initialized) return NtStatus::InvalidParameter;
    return ke::PriorityScheduler::get().readyKThread(thread_.get())
        ? NtStatus::Success : NtStatus::Unsuccessful;
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
                                        bool interruptible) noexcept {
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
    auto status = initialize(process, kernelStackSize, startUser, this);
    if (status != NtStatus::Success) return status;
    auto& space = process.getAddressSpace();
    status = space.allocate(userStack_, stackSize, mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE);
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
    teb->stackLimit = userStack_;
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

void EThread::startUser(void* argument) noexcept {
    auto* self = static_cast<EThread*>(argument);
    if (!self || !self->userEntry_ || !self->userStackPointer_) __builtin_trap();
    hal::amd64::enterBootstrapUserThread(self->userEntry_, self->userStackPointer_, self->userInterruptible_);
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
        !self->process_->isTerminating() || ke::KeGetCurrentIrql() != ke::PASSIVE_LEVEL) return;
    (void)self->exitCurrent(self->process_->exitStatus());
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
    const auto status = self->thread_->terminationRequested && self->process_->isTerminating()
        ? self->process_->exitStatus() : NtStatus::Success;
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
    current->terminationActive = true;
    // Do not remove a runnable successor while APC callbacks run: those
    // callbacks may themselves wait for that thread.
    if (!ke::PriorityScheduler::get().getReadyThreadCount() && !ke::g_CurrentCpu.idleThread) {
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
    ke::KfRaiseIrql(ke::DISPATCH_LEVEL);
    auto* next = ke::PriorityScheduler::get().selectNextThread();
    const NtStatus previousStatus = exitStatus_;
    if (next && rundown && current->kernelApcList.empty() && !current->pendingNormalCount &&
        !current->normalKernelApcActive) {
        exitStatus_ = status;
        current->state = ke::ThreadState::Terminated;
        if (ke::KiSwapContext(current, next)) __builtin_trap();
        current->state = ke::ThreadState::Running;
        exitStatus_ = previousStatus;
    }
    if (next) {
        next->state = ke::ThreadState::Ready;
        if (next != ke::g_CurrentCpu.idleThread && !ke::PriorityScheduler::get().readyKThread(next))
            __builtin_trap();
    }
    ke::KeLowerIrql(ke::PASSIVE_LEVEL);
    current->terminationActive = alreadyActive;
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return rundown ? NtStatus::Unsuccessful : NtStatus::NotImplemented;
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
NtStatus NtTerminateProcess(Handle processHandle, NtStatus exitStatus) {
    constexpr uint32_t ProcessTerminateAccess = 0x0001;
    auto* current = ke::KeGetCurrentThread();
    if (processHandle == InvalidHandleValue) {
        if (!current || !current->owningProcess) return NtStatus::InvalidParameter;
        return current->owningProcess->terminateCurrent(exitStatus);
    }
    if (!processHandle) return NtStatus::NotImplemented;
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
