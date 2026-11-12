#pragma once

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::ob { struct ObjectHeader; }
namespace micant::ps {
class EProcess;

namespace detail {
// Single-BSP internal helpers; the caller serializes access with interrupts
// disabled. The process pointer returned by creation is borrowed from handle.
NtStatus createProcessHandle(uint32_t access, Handle& handle, EProcess*& process) noexcept;
NtStatus referenceProcessHandle(Handle handle, uint32_t access, EProcess*& process,
                                ob::ObjectHeader*& reference) noexcept;
bool isProcessObject(const ob::ObjectHeader& header) noexcept;
uint32_t bootstrapProcessObjectCount() noexcept;
}
}
