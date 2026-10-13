#include "micant/km/core/ntos_internal.hpp"
#include "micant/km/executive/ob.hpp"
#include "micant/km/core/sync.hpp"

namespace micant::sys {

static ob::HandleTable g_KernelHandleTable;
static Handle g_NextExecutiveHandle = 0x100;

namespace internal {

Handle ObAllocateExecutiveHandle() noexcept {
    Handle h = g_NextExecutiveHandle;
    g_NextExecutiveHandle += 4;
    return h;
}

NtStatus ObCloseGenericHandle(Handle h) {
    return g_KernelHandleTable.closeHandle(h);
}

} // namespace internal

NtStatus NtClose(Handle handle) {
    if (handle == 0 || handle == InvalidHandleValue) {
        return NtStatus::InvalidHandle;
    }

    bool wasSync = sync::DispatcherRegistry::get().lookup(handle) != nullptr;
    sync::DispatcherRegistry::get().unregister(handle);

    bool hadIo    = internal::IoCloseKernelHandle(handle);
    bool hadSec   = internal::MmCloseSectionHandle(handle);
    bool hadKey   = internal::CmCloseKeyHandle(handle);
    bool hadToken = internal::SeCloseTokenHandle(handle);
    bool hadPort  = internal::LpcClosePortHandle(handle);

    if (wasSync || hadIo || hadSec || hadKey || hadToken || hadPort) {
        return NtStatus::Success;
    }
    return internal::ObCloseGenericHandle(handle);
}

} // namespace micant::sys
