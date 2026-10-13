#pragma once
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/core/syscalls.hpp"
#include "micant/km/core/dispatcher.hpp"

namespace micant::sys::internal {

// Shared Executive Handle Allocator (4-byte aligned NT kernel handle values)
Handle ObAllocateExecutiveHandle() noexcept;
NtStatus ObCloseGenericHandle(Handle h);

// Subsystem Handle Cleanup Hooks (invoked by Ob / NtClose)
bool IoCloseKernelHandle(Handle h);
bool MmCloseSectionHandle(Handle h);
bool CmCloseKeyHandle(Handle h);
bool SeCloseTokenHandle(Handle h);
bool LpcClosePortHandle(Handle h);

// I/O Completion Port System Services (kernel/io/iomgr.cpp)
NtStatus NtCreateIoCompletionSys(const SyscallFrame& f);
NtStatus NtSetIoCompletionSys(const SyscallFrame& f);
NtStatus NtRemoveIoCompletionSys(const SyscallFrame& f);

// Configuration Manager Registry System Services (kernel/cm/ntreg.cpp)
NtStatus NtOpenKeySys(const SyscallFrame& f);
NtStatus NtQueryValueKeySys(const SyscallFrame& f);
NtStatus NtSetValueKeySys(const SyscallFrame& f);

// Security Reference Monitor System Services (kernel/se/ntsec.cpp)
NtStatus NtOpenProcessTokenSys(const SyscallFrame& f);
NtStatus NtAccessCheckSys(const SyscallFrame& f);

// Local / Advanced Local Procedure Call System Services (kernel/lpc/ntlpc.cpp)
NtStatus NtCreatePortSys(const SyscallFrame& f);
NtStatus NtConnectPortSys(const SyscallFrame& f);
NtStatus NtRequestWaitReplyPortSys(const SyscallFrame& f);

// Cache Manager Executive Initialization & Scan (kernel/cc/ccmgr.cpp)
void CcInitializeExecutiveCache() noexcept;

// Executive Pool, Sync & Worker Initialization (kernel/ex/exmgr.cpp)
void ExInitializeExecutiveSubsystem() noexcept;

// Hardware Abstraction Layer Initialization (kernel/hal/halinit.cpp)
void HalInitializeExecutiveSubsystem() noexcept;

// Plug and Play Manager Initialization (kernel/pnp/pnpmgr.cpp)
void PnpInitializeExecutiveSubsystem() noexcept;

// Power Manager Initialization (kernel/po/pomgr.cpp)
void PoInitializeExecutiveSubsystem() noexcept;

} // namespace micant::sys::internal
