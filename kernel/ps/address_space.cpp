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
}
