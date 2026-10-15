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

// File System Runtime Library Initialization (kernel/fsrtl/fsrtlmgr.cpp)
void FsRtlInitializeExecutiveSubsystem() noexcept;

// User-Mode Debugging Executive Initialization (kernel/dbgk/dbgkobj.cpp)
void DbgkInitializeExecutiveSubsystem() noexcept;

// Kernel Transaction Manager & CLFS Initialization (kernel/tm/ktmmgr.cpp)
void KtmInitializeExecutiveSubsystem() noexcept;

// Executive Pool, Sync & Worker Initialization (kernel/ex/exmgr.cpp)
void ExInitializeExecutiveSubsystem() noexcept;

// Hardware Abstraction Layer Initialization (kernel/hal/halinit.cpp)
void HalInitializeExecutiveSubsystem() noexcept;

// Plug and Play Manager Initialization (kernel/pnp/pnpmgr.cpp)
void PnpInitializeExecutiveSubsystem() noexcept;

// Power Manager Initialization (kernel/po/pomgr.cpp)
void PoInitializeExecutiveSubsystem() noexcept;

// Driver Verifier Executive Initialization (kernel/verifier/vfmgr.cpp)
void VfInitializeExecutiveSubsystem() noexcept;

// Hardware Resource Arbiter & WDK .sys Loader Initialization (kernel/arb/arbmgr.cpp)
void ArbInitializeExecutiveSubsystem() noexcept;

// Kernel WMI, WPP Recorder & Real Vendor .sys Compatibility Initialization (kernel/wmi/wmimgr.cpp)
void WmiInitializeExecutiveSubsystem() noexcept;

// Boot-Time Driver Scheduler & Live Hardware Bus Scanner Initialization (kernel/io/pnpboot.cpp)
void PnpBootInitializeExecutiveSubsystem() noexcept;

// Anti-Vista/ME Driver Resilience Engine Initialization (kernel/io/pnpresilience.cpp)
void PnpResilienceInitializeExecutiveSubsystem() noexcept;

// Runtime Power Management (PoFx*), Live PnP Hot-Plug & WHEA Initialization (kernel/po/pofx_whea.cpp)
void PoFxWheaInitializeExecutiveSubsystem() noexcept;

// UMDF 2.0 Kernel Reflector (Wudfrd.sys), Anti-CrowdStrike SafeChannel & MSI-X Router (kernel/io/umdf_msix.cpp)
void UmdfMsixInitializeExecutiveSubsystem() noexcept;

// Zero-BSOD Self-Healing Kernel Guard Initialization (kernel/ke/zerobsod.cpp)
void ZeroBsodInitializeExecutiveSubsystem() noexcept;

// Zero-Downtime Live Driver Hot-Swap, 0x7A/0xF4 Inpage Healer & Intel VT-d DMA Firewall (kernel/io/livehotswap.cpp)
void LiveHotSwapInitializeExecutiveSubsystem() noexcept;

// Sovereign Kernel Telemetry (NtQuerySystemInformation), Taskmgr Kernel Times & MMCSS SMP Scheduler (kernel/ex/sysinfo_taskmgr.cpp)
void SysInfoTaskmgrInitializeExecutiveSubsystem() noexcept;

// Sovereign Anti-Bloat Process/GDI Leak Governor, Runaway svchost Healer & Primes Drag-Race Engine (kernel/ps/quotagov.cpp)
void QuotaGovInitializeExecutiveSubsystem() noexcept;

// Sovereign Zero-Freeze 2ms Input Hook Guard, Instant $MFT Search & Zero-Copy zipfldr Section Engine (kernel/fsrtl/mftzipui.cpp)
void MftZipUiInitializeExecutiveSubsystem() noexcept;

// Sovereign Ring 0 Security Reference Monitor, MIC/UIPI, AppContainer & Zero-Lag ci.dll Engine (kernel/se/srm_ci.cpp)
void SrmCiInitializeExecutiveSubsystem() noexcept;

// Sovereign Ring 0 afd.sys Fast-I/O, Winsock Kernel (WSK) & Registered I/O (RIO) Engine (kernel/io/afd_wsk.cpp)
void AfdWskInitializeExecutiveSubsystem() noexcept;

// Sovereign Ring 0 <-> Ring 3 Hardware ABI Seal (KUSER_SHARED_DATA, SYSCALL MSRs & KiUser* Trampolines) (kernel/ke/kuser_seal.cpp)
void KuserSealInitializeExecutiveSubsystem() noexcept;

// Sovereign Ring 1 VFS & Minifilter Fault-Isolated Fast-I/O Plane (kernel/fsrtl/ring1_vfs.cpp)
void Ring1VfsInitializeExecutiveSubsystem() noexcept;

// Sovereign Ring 1 Graphics & Display Scanout Bridge (kernel/io/ring1_dxgk.cpp)
void Ring1DxgkInitializeExecutiveSubsystem() noexcept;

// Sovereign Ring 1 Real-Time WaveRT Audio & Multimedia DMA Engine (kernel/io/ring1_audio.cpp)
void Ring1AudioInitializeExecutiveSubsystem() noexcept;

// Sovereign Ring 1 Native Session Bootstrap (smss.exe, \KnownDlls & csrss.exe) (kernel/ps/ring1_smss.cpp)
void Ring1SmssInitializeExecutiveSubsystem() noexcept;

// Sovereign Ring 2 Isolated Device & Parser Host (WUDFHost.exe & fontdrvhost.exe) (kernel/io/ring2_wudf.cpp)
void Ring2WudfInitializeExecutiveSubsystem() noexcept;

// Sovereign Ring 2 Security Authority & Isolated Credential Vault (lsass.exe, lsaiso.exe & KeyIso) (kernel/se/ring2_lsa.cpp)
void Ring2LsaInitializeExecutiveSubsystem() noexcept;

// Sovereign Ring 2 Service Control Manager & Core System Daemons (services.exe, RpcSs & DcomLaunch) (kernel/ps/ring2_scm.cpp)
void Ring2ScmInitializeExecutiveSubsystem() noexcept;

// Sovereign Ring 3 Win32 Base DLL & DynamicLoader Bridge (kernel/ps/ring3_win32_base.cpp)
void Ring3BaseInitializeExecutiveSubsystem() noexcept;

// Sovereign Ring 3 Interactive Session winlogon.exe, Native C++23 explorer.exe & Zero-Copy dwm.exe (kernel/ps/ring3_desktop_shell.cpp)
void Ring3ShellInitializeExecutiveSubsystem() noexcept;

// Sovereign Ring 3 Real-World Application Triad Roll-In (Notepad++, VLC, 7-Zip & TaskMgr/Primes) (kernel/ps/ring3_real_apps.cpp)
void Ring3AppsInitializeExecutiveSubsystem() noexcept;

} // namespace micant::sys::internal
