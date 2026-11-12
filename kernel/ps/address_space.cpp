#include "micant/km/executive/ps.hpp"
#include "micant/km/core/ke.hpp"

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
    if (!root_.pfn || thread.owningProcess || thread.readyQueued ||
        (thread.state != ke::ThreadState::Initialized && thread.state != ke::ThreadState::Ready))
        return NtStatus::InvalidParameter;
    thread.owningProcess = this;
    thread.directoryTableBase = getDirectoryTableBase();
    ++attachedThreads_;
    return NtStatus::Success;
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

void EThread::startThread(void* argument) noexcept {
    auto* self = static_cast<EThread*>(argument);
    if (!self || !self->thread_ || ke::KeGetCurrentThread() != self->thread_.get()) __builtin_trap();
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
    return NtStatus::Success;
}
}
