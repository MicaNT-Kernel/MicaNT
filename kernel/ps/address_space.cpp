#include "micant/km/executive/ps.hpp"
#include "micant/km/core/ke.hpp"
#include "micant/km/core/syscalls.hpp"

namespace micant::ps {

NtStatus EProcess::initializeAddressSpace() noexcept {
    using namespace hal::amd64;
    if (root_.pfn) return NtStatus::InvalidParameter;
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
    terminating_ = false;
    exitStatus_ = NtStatus::Pending;
    return NtStatus::Success;
}

NtStatus EProcess::closeAddressSpace() noexcept {
    using namespace hal::amd64;
    if (!root_.pfn || attachedThreads_) return NtStatus::InvalidParameter;
    uintptr_t previous{};
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(previous));
    if (previous == getDirectoryTableBase()) return NtStatus::InvalidParameter;
    if (!switchProcessPagingRoot(&root_)) return NtStatus::Unsuccessful;
    handles_->closeAllHandles();
    const NtStatus status = addressSpace_.close();
    if (!switchProcessPagingRootByPhysical(previous)) __builtin_trap();
    if (status != NtStatus::Success) return status;
    if (!destroyProcessPagingRoot(root_)) return NtStatus::Unsuccessful;
    handles_.reset();
    return NtStatus::Success;
}

NtStatus EProcess::attachThread(ke::KThread& thread) noexcept {
    if (!root_.pfn || terminating_ || thread.owningProcess || thread.readyQueued ||
        (thread.state != ke::ThreadState::Initialized && thread.state != ke::ThreadState::Ready))
        return NtStatus::InvalidParameter;
    thread.owningProcess = this;
    thread.directoryTableBase = getDirectoryTableBase();
    ++attachedThreads_;
    return NtStatus::Success;
}

NtStatus EProcess::terminateCurrent(NtStatus exitStatus) noexcept {
    uint64_t flags{};
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    auto* current = ke::KeGetCurrentThread();
    NtStatus result = NtStatus::InvalidParameter;
    const auto previous = ke::KeGetCurrentIrql();
    if (current && current->owningProcess == this && current->state == ke::ThreadState::Running &&
        previous <= ke::APC_LEVEL && !terminating_) {
        if (attachedThreads_ != 1) {
            result = NtStatus::NotImplemented;
        } else {
            ke::KfRaiseIrql(ke::DISPATCH_LEVEL);
            auto& scheduler = ke::PriorityScheduler::get();
            if (auto* next = scheduler.selectNextThread()) {
                terminating_ = true;
                exitStatus_ = exitStatus;
                current->state = ke::ThreadState::Terminated;
                if (ke::KiSwapContext(current, next)) __builtin_trap();
                current->state = ke::ThreadState::Running;
                terminating_ = false;
                exitStatus_ = NtStatus::Pending;
                next->state = ke::ThreadState::Ready;
                if (!scheduler.readyKThread(next)) __builtin_trap();
            }
            ke::KeLowerIrql(previous);
            result = NtStatus::Unsuccessful;
        }
    }
    if (flags & (uint64_t{1} << 9)) __asm__ __volatile__("sti" : : : "memory");
    return result;
}

NtStatus EProcess::detachThread(ke::KThread& thread) noexcept {
    if (thread.owningProcess != this || thread.readyQueued ||
        (thread.state != ke::ThreadState::Initialized && thread.state != ke::ThreadState::Terminated))
        return NtStatus::InvalidParameter;
    if (!attachedThreads_) __builtin_trap();
    --attachedThreads_;
    thread.owningProcess = nullptr;
    thread.directoryTableBase = 0;
    return NtStatus::Success;
}

EThread::~EThread() {
    if (thread_ || stack_ || process_) __builtin_trap();
}

NtStatus EThread::initialize(EProcess& process, size_t stackSize,
                             StartRoutine start, void* argument) noexcept {
    if (thread_ || stack_ || process_ || !start || stackSize < 4096 || stackSize > 1024 * 1024)
        return NtStatus::InvalidParameter;
    std::unique_ptr<ke::KThread> thread(new (std::nothrow_t{}) ke::KThread{});
    std::unique_ptr<uint8_t[]> stack(new (std::nothrow_t{}) uint8_t[stackSize]);
    if (!thread || !stack) return NtStatus::NoMemory;
    if (!ke::detail::prepareBootstrapThread(*thread, stack.get(), stackSize, startThread, this))
        return NtStatus::InvalidParameter;
    const auto status = process.attachThread(*thread);
    if (status != NtStatus::Success) return status;
    process_ = &process;
    thread_ = std::move(thread);
    stack_ = std::move(stack);
    start_ = start;
    argument_ = argument;
    return NtStatus::Success;
}

NtStatus EThread::ready() noexcept {
    if (!thread_ || thread_->state != ke::ThreadState::Initialized) return NtStatus::InvalidParameter;
    return ke::PriorityScheduler::get().readyKThread(thread_.get())
        ? NtStatus::Success : NtStatus::Unsuccessful;
}

NtStatus EThread::cancelBeforeStart() noexcept {
    if (!thread_ || started_ || thread_.get() == ke::KeGetCurrentThread())
        return NtStatus::InvalidParameter;
    if (thread_->state == ke::ThreadState::Ready) {
        if (!ke::PriorityScheduler::get().removeReadyThread(thread_.get()))
            return NtStatus::InvalidParameter;
    } else if (thread_->state != ke::ThreadState::Initialized || thread_->readyQueued) {
        return NtStatus::InvalidParameter;
    }
    thread_->state = ke::ThreadState::Terminated;
    return NtStatus::Success;
}

void EThread::startThread(void* argument) noexcept {
    auto* self = static_cast<EThread*>(argument);
    if (!self || !self->thread_ || ke::KeGetCurrentThread() != self->thread_.get()) __builtin_trap();
    self->started_ = true;
    ke::KeLowerIrql(ke::PASSIVE_LEVEL);
    self->start_(self->argument_);
    __asm__ __volatile__("cli" : : : "memory");
    ke::KfRaiseIrql(ke::DISPATCH_LEVEL);
    auto* next = ke::PriorityScheduler::get().selectNextThread();
    if (!next) __builtin_trap();
    self->thread_->state = ke::ThreadState::Terminated;
    if (!ke::KiSwapContext(self->thread_.get(), next)) __builtin_trap();
    __builtin_trap(); // A terminated thread may never resume.
}

NtStatus EThread::reap() noexcept {
    if (!thread_ || !process_ || thread_->readyQueued || thread_.get() == ke::KeGetCurrentThread() ||
        (thread_->state != ke::ThreadState::Initialized && thread_->state != ke::ThreadState::Terminated))
        return NtStatus::InvalidParameter;
    const auto status = process_->detachThread(*thread_);
    if (status != NtStatus::Success) return status;
    thread_.reset();
    stack_.reset();
    process_ = nullptr;
    start_ = nullptr;
    argument_ = nullptr;
    started_ = false;
    return NtStatus::Success;
}
}

namespace micant::sys {
NtStatus NtTerminateProcess(Handle processHandle, NtStatus exitStatus) {
    if (processHandle != InvalidHandleValue) return NtStatus::NotImplemented;
    auto* current = ke::KeGetCurrentThread();
    if (!current || !current->owningProcess) return NtStatus::InvalidParameter;
    return current->owningProcess->terminateCurrent(exitStatus);
}
}
