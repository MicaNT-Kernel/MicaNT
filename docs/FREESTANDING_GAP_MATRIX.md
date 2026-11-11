# Freestanding Gap Matrix

## Scope and evidence

Inventory: all **98** translation units in `NTOSKRNL_SUBSYSTEM_SOURCES` in
[CMakeLists.txt](../CMakeLists.txt), audited at `d8e8d42` in four isolated
worker worktrees plus the primary runtime/build stream. Pending native PE
and UCRT increments are recorded separately below. This inventory precedes
further Executive integration; it is not permission to link the whole hosted
manifest into the kernel.

The production `ntoskrnl_core_baremetal` source list contains the bootstrap,
AMD64 platform, minimal memory/string runtime and shared drawing sources.
**None of the 98 listed subsystem translation units is currently linked,
initialized or exercised as an Executive module in QEMU.** Header-only native
PE functionality exercised by the diagnostic is a narrower, separate claim.
Compilation alone does not change those integration states.

**Integration update:** the later C++ lifecycle increment links and initializes
`kernel/ob/obref.cpp`'s canonical global handle table in QEMU and verifies
handle/reference/access lifecycle. Its native `NtClose` remains absent pending
the owning subsystem close paths; no full Ob/namespace claim follows.
Consequently one of the baseline 98 units is now partially active; the
original audit table is historical evidence, not the current link manifest.

Subsequent verified increment: the canonical header-only `PfnDatabase` now
supports borrowed boot backing and runs against real physical pages in QEMU.
This does not make the baseline's hosted virtual-memory translation unit
freestanding or activate `mm_baremetal.hpp`; see the integration record.

Later runtime increments also supply PFN-backed kernel C/C++ allocation and
connect the canonical `probe.hpp` status-returning copy helpers to real AMD64
page permissions. The native branch removes its `try/catch` parser blocker;
it requires immutable mappings and IRQ-disabled single-CPU use, not general
SEH or scheduled memory access. Hosted synthetic permissions remain test-only.
These header/runtime improvements do not clear the remaining R/S dependencies
or change the original translation-unit audit results below.

Classes:

- **A:** verified freestanding without adaptation.
- **B:** minimal adapter/header-boundary change.
- **C:** shared C++/CRT runtime, allocation, synchronization or lifecycle work.
- **D:** real hardware/platform backend required.
- **E:** concrete incomplete, simulated or fixed-report behavior identified.
- **F:** duplicate/competing implementation; do not activate another owner.

Multiple tags are intentional. `U` in the compile column means unprobed, not
failure or success. `P`/`F` mean the bounded object-only probe passed/failed;
no PE link was attempted for these objects. No class A claim is established.
Absence from the production target is **not** by itself a class E finding.

Common dependency codes used below:

- **R:** STL strings/containers/shared ownership and `new/delete` or C heap;
  libc++ out-of-line symbols and explicit static-constructor/destructor
  handling may remain even with `-fno-use-cxa-atexit`.
- **S:** locks, condition variables, host threads or clocks need Ke/HAL-backed
  primitives; allocation alone is insufficient.
- **J:** transitive user-loader exception dependency:
  [janusldr.hpp](../include/micant/um/native/janusldr.hpp#L245) uses
  `try`/`std::stoul`/catch under the no-exceptions build.
- **V:** user-mode VersionDatabase/loader/SCM registration is not Executive
  initialization; separate that boundary rather than embedding user mode.
- **H:** hardware/service integration is unverified and needs a real backend,
  not a report value.

Host tests mentioned here were located, **not run by the audit workers**.
The primary targeted PE/UCRT tests and QEMU checks are listed in
[KERNEL_INTEGRATION_STATUS.md](KERNEL_INTEGRATION_STATUS.md).

## Runtime and build foundations (3 units)

| Source | Class | Compile | Evidence / dependency / next adapter |
|---|---|---|---|
| [kernel/ex/exmgr.cpp](../kernel/ex/exmgr.cpp) | C | F | Initializes canonical ExecutivePool. [ex.hpp](../include/micant/km/executive/ex.hpp#L95) allocates through `malloc`, records tags in a dynamic map and logs via iostreams; `probe.hpp:211,239` and `ex_work.hpp:385` require exceptions. R/S and constructor lifecycle before initialization. |
| [kernel/ex/sysinfo_taskmgr.cpp](../kernel/ex/sysinfo_taskmgr.cpp) | C/E | P | [sysinfo_taskmgr.hpp](../include/micant/km/executive/sysinfo_taskmgr.hpp#L66) contains fixed memory/counter defaults, strings/vectors/mutexes. Object leaves libc++ strings/abort, regular/aligned new/delete, `atexit`, `___chkstk_ms`, `__imp_tolower` and memory/string calls unresolved. Not live accounting. |
| [kernel/hal/halinit.cpp](../kernel/hal/halinit.cpp) | C/D | F | Calls `initialize(4)` at source line 12. [hal.hpp](../include/micant/km/hal/hal.hpp#L934) allocates KPCRs and takes a hosted steady-clock timestamp. ACPI inclusion reaches J. Real CPU enumeration/timer/IRQ backing and R/S required. |

The HAL probe used the primary tree, including the preserved pre-existing
ACPI edits; workers used the committed baseline. Neither result authorizes
committing those unrelated edits.

## Mm, Cc and FsRtl (11 units)

| Source | Class | Compile | Evidence / dependency / next adapter |
|---|---|---|---|
| [kernel/mm/virtual.cpp](../kernel/mm/virtual.cpp) | C/E/D | P | Source lines 10-12 own global address-space/map/shared-pointer state; protection ignored at 56-60, section bytes treated as an address at 127. [mm.hpp](../include/micant/km/executive/mm.hpp#L125) uses dynamic PFN storage and locks. R/S; connect canonical Mm to boot descriptors and HAL mappings, not heap-backed addresses. |
| [kernel/cc/ccmgr.cpp](../kernel/cc/ccmgr.cpp) | B/C | F | Thin canonical [cc.hpp](../include/micant/km/executive/cc.hpp) initializer; transitive J blocks compilation. Separate registration/header dependencies before claiming a working cache. |
| [kernel/fsrtl/fsrtlmgr.cpp](../kernel/fsrtl/fsrtlmgr.cpp) | B | U | Lines 6-7 forward to canonical [fsrtl.hpp](../include/micant/km/executive/fsrtl.hpp) initialization; actual backend/runtime closure remains unprobed. |
| [kernel/fsrtl/extfs.cpp](../kernel/fsrtl/extfs.cpp) | C/D | P | Source lines 67,109,198: shared block-device ownership and vector buffers; [extfs.hpp](../include/micant/km/drivers/fs/extfs.hpp). R, allocator and connected block-device backend. |
| [kernel/fsrtl/ntfs.cpp](../kernel/fsrtl/ntfs.cpp) | C/E/D | U | Lines 15,44,59: Fast I/O stubs; 147-178: shared volume APIs; [ntfs.hpp](../include/micant/km/drivers/fs/ntfs.hpp). R and storage/Io integration. |
| [kernel/fsrtl/exfat.cpp](../kernel/fsrtl/exfat.cpp) | C/E/D | U | Lines 15,44,59: Fast I/O stubs; 147-178: shared volume APIs; [exfat.hpp](../include/micant/km/drivers/fs/exfat.hpp). R and storage/Io integration. |
| [kernel/fsrtl/cdfs.cpp](../kernel/fsrtl/cdfs.cpp) | C/E/D | U | Lines 36,54,73: unconditional false Fast I/O paths; [cdfs.hpp](../include/micant/km/drivers/fs/cdfs.hpp). R and actual device reads required. |
| [kernel/fsrtl/refs.cpp](../kernel/fsrtl/refs.cpp) | C/E/D | U | Lines 12-13,32 import/call user SCM; Fast I/O stubs at 38,67,82; [refs.hpp](../include/micant/km/drivers/fs/refs.hpp). R/V; separate user registration and storage backend. |
| [kernel/fsrtl/zfs.cpp](../kernel/fsrtl/zfs.cpp) | C/E/D | U | Lines 31,78-80 contain vector-based pool/demo state, 106 calls SCM; Fast I/O stubs 113,142,157; [zfs.hpp](../include/micant/km/drivers/fs/zfs.hpp). R/V/H. |
| [kernel/fsrtl/mftzipui.cpp](../kernel/fsrtl/mftzipui.cpp) | E | U | Lines 13-14 initialize [mftsearch_uiguard_zipfldr.hpp](../include/micant/km/drivers/fs/mftsearch_uiguard_zipfldr.hpp), a higher-level feature/report coordinator; not a first-boot dependency or verified storage path. |
| [kernel/fsrtl/ring1_vfs.cpp](../kernel/fsrtl/ring1_vfs.cpp) | E | U | Lines 12-13 initialize [ring1_vfs_minifilter_plane.hpp](../include/micant/km/drivers/fs/ring1_vfs_minifilter_plane.hpp); isolated execution is not established by that initializer. |

`virtual.cpp` leaves libc++ containers/strings/shared counts/mutexes,
new/delete, `atexit`, C memory/string/math and `LookupKernelFileObject`
unresolved. ExtFS has similar allocation/string/shared-ownership families.
Cache, NTFS, ExtFS, CDFS/UDFS and ZFS tests are registered in the hosted runner;
none prove real block-device boot integration.

[mm_baremetal.hpp](../include/micant/km/executive/mm_baremetal.hpp) is **F**:
a separate implementation with a hard-coded physical pool at `0x08000000`
(line 72), not the active canonical Mm backend. Do not initialize it alongside
[mm.hpp](../include/micant/km/executive/mm.hpp).

## Ob, Se, Cm and auxiliary Executive modules (17 units)

| Source | Class | Compile | Evidence / dependency / next adapter |
|---|---|---|---|
| [kernel/ob/obref.cpp](../kernel/ob/obref.cpp) | C | P | Global handle table at line 7; [ob.hpp](../include/micant/km/executive/ob.hpp#L365) constructs vector storage and uses mutexes. R/S, five Executive close helpers, static constructor and `atexit`. |
| [kernel/cm/ntreg.cpp](../kernel/cm/ntreg.cpp) | C | U | Global map and dynamic string insertion at 9,24,49,75; [cm.hpp](../include/micant/km/executive/cm.hpp#L1113) constructs registry objects. R/S; persistence additionally requires Io. |
| [kernel/se/ntsec.cpp](../kernel/se/ntsec.cpp) | C/E | P | Global token map and SYSTEM-token fallback at 9,20-30; [se.hpp](../include/micant/km/executive/se.hpp#L837) allocates shared token state and locks. R/S, Ps clock/recursive mutex and static constructor. Fallback is not correct token lifecycle. |
| [kernel/lpc/ntlpc.cpp](../kernel/lpc/ntlpc.cpp) | C | U | Lines 12,60-64: global map and dynamic reply data; [lpc.hpp](../include/micant/km/executive/lpc.hpp#L571) uses mutex/CV, promises and timed futures. R/S; needs actual scheduling and timeouts. |
| [kernel/dbgk/dbgkobj.cpp](../kernel/dbgk/dbgkobj.cpp) | C | U | [dbgk.hpp](../include/micant/km/executive/dbgk.hpp#L336): singleton, locks, allocated debug objects; J/V registration at 1405-1433. Real trap/debug integration remains unverified. |
| [kernel/tm/ktmmgr.cpp](../kernel/tm/ktmmgr.cpp) | C | F | [ktm.hpp](../include/micant/km/executive/ktm.hpp#L352): mutex, singleton and allocated in-memory log; R/S/J/V. Persistent recovery requires actual storage, not merely an in-memory object. |
| [kernel/verifier/vfmgr.cpp](../kernel/verifier/vfmgr.cpp) | C/D | U | [verifier.hpp](../include/micant/km/executive/verifier.hpp#L307): dynamic ledgers and locks; J/V. Actual special pool/IRQL/DMA enforcement needs Mm/Ke/Io/HAL. |
| [kernel/arb/arbmgr.cpp](../kernel/arb/arbmgr.cpp) | C/D | U | [arbiter.hpp](../include/micant/km/executive/arbiter.hpp#L490): allocated range lists and mutexes; J/V. Simulated image addresses at 827-828 are not PCI/IRQ/DMA resource programming. |
| [kernel/wmi/wmimgr.cpp](../kernel/wmi/wmimgr.cpp) | E/C | U | [wmi_driver_compat.hpp](../include/micant/km/executive/wmi_driver_compat.hpp#L273): host filesystem/ifstream; generic-success driver exports at 633,644,820-961. R/S/J/V; closure is not driver operation. |
| [kernel/se/srm_ci.cpp](../kernel/se/srm_ci.cpp) | E | U | [srm_token_ci.hpp](../include/micant/km/executive/srm_token_ci.hpp#L100): folded non-SHA hash, fixed verification reports at 170-204. POD/memset locally, V on initialization; real signature enforcement absent. |
| [kernel/se/ring2_lsa.cpp](../kernel/se/ring2_lsa.cpp) | E | U | [ring2_lsa_vault_keyiso.hpp](../include/micant/km/drivers/security/ring2_lsa_vault_keyiso.hpp#L94): fixed identity/token and isolation/key-operation reports. POD/memset plus V; not actual isolated processes/ALPC/key custody. |
| [kernel/lpc/ring3_com_rpc_shell.cpp](../kernel/lpc/ring3_com_rpc_shell.cpp) | E/C | U | [ring3_com_rpc_shell_controls_tunnel.hpp](../include/micant/um/subsystems/com/ring3_com_rpc_shell_controls_tunnel.hpp#L51) initializes user COM/DLLs, then consumes fixed-latency broker reports. R/S/V; no demonstrated cross-process tunnel. |
| [kernel/se/ring3_bcrypt_crypt32_wintrust.cpp](../kernel/se/ring3_bcrypt_crypt32_wintrust.cpp) | E/C | U | [ring3_bcrypt_crypt32_wintrust_tunnel.hpp](../include/micant/um/subsystems/security/ring3_bcrypt_crypt32_wintrust_tunnel.hpp#L90) consumes isolation/CI reports. Real helper roundtrips do not establish broker isolation; R/S/V. |
| [kernel/se/ring0_anticheat_eac_battleye_callbacks.cpp](../kernel/se/ring0_anticheat_eac_battleye_callbacks.cpp) | E | U | [ring0_anticheat_eac_battleye_callbacks.hpp](../include/micant/km/drivers/security/ring0_anticheat_eac_battleye_callbacks.hpp#L57) assigns callback/enforcement counters and booleans. POD plus graphics initializer chain, not callback interception. |
| [kernel/se/ring0_vanguard_ricochet_hvci_iommu_shield.cpp](../kernel/se/ring0_vanguard_ricochet_hvci_iommu_shield.cpp) | E/D | U | [ring0_vanguard_ricochet_hvci_iommu_shield.hpp](../include/micant/km/drivers/security/ring0_vanguard_ricochet_hvci_iommu_shield.hpp#L54) assigns TPM/Secure Boot/IOMMU claims. Needs actual platform evidence; transitive feature initialization. |
| [kernel/se/horizon100_gaming_drm_hwid_vault_seal.cpp](../kernel/se/horizon100_gaming_drm_hwid_vault_seal.cpp) | E/D | U | [horizon100_gaming_drm_hwid_vault_seal.hpp](../include/micant/km/drivers/security/horizon100_gaming_drm_hwid_vault_seal.hpp#L58) assigns timing/context/compatibility totals. Local reports do not prove context switching or hardware isolation. |
| [kernel/se/winnt_bcrypt_crypt32_secur32_wintrust_abi.cpp](../kernel/se/winnt_bcrypt_crypt32_secur32_wintrust_abi.cpp) | F/E | U | Source 29-40,51-77,135-141,203-209: fixed-seed RNG, non-SHA hash, XOR encryption and trust shortcut. Duplicates contracts in canonical [cipherksp.hpp](../include/micant/um/subsystems/security/cipherksp.hpp), [crypt32.hpp](../include/micant/um/subsystems/security/crypt32.hpp), [wintrust.hpp](../include/micant/um/subsystems/security/wintrust.hpp). Do not use this as a real native crypto backend. |

Ob emits 16 undefined symbols; Se 29, including regular/aligned allocation,
libc++ ownership/locks/clock, C memory and `atexit`. Actual global constructors
were confirmed in both. `-fno-use-cxa-atexit` does not remove that obligation.
Hosted Ob/Se/Cm/LPC/KTM/debug/verifier/arbiter/WMI and feature-report tests
exist in [kernel_suites.hpp](../test/suites/kernel_suites.hpp); they are not
freestanding initialization or enforcement tests.

## Ke and Ps (20 units)

| Source | Class | Compile | Evidence / dependency / next adapter |
|---|---|---|---|
| [kernel/ke/dispatcher.cpp](../kernel/ke/dispatcher.cpp) | C/B | F | [dispatcher.hpp](../include/micant/km/core/dispatcher.hpp#L18) owns function/map state; source allocates shared handlers. `sync.hpp:944` instantiates `dynamic_pointer_cast`, incompatible with disabled RTTI. R/S; use canonical kernel object identity, not a second dispatcher. |
| [kernel/ke/wait.cpp](../kernel/ke/wait.cpp) | C/B | U | Source 216,222,227,304 uses hosted thread yield/sleep, plus CV/mutex/chrono/vector. R/S; Ke wait queues, scheduler block/wake and timer IRQs must replace host services. |
| [kernel/ps/psmgr.cpp](../kernel/ps/psmgr.cpp) | E/C | F | Termination logs, system-information stub produces no data, process-information paths return fixed values. Includes iostream and reaches `ex_work.hpp:385` try/catch. Reuse canonical Ps lifecycle and checked user buffers. |
| [kernel/ke/zerobsod.cpp](../kernel/ke/zerobsod.cpp) | C/E | U | [zero_bsod_guard.hpp](../include/micant/km/core/zero_bsod_guard.hpp#L283) initializer uses strings, vectors, maps and mutexes. R/S; recovery claims need real IRQL/DPC/driver behavior. |
| [kernel/ps/quotagov.cpp](../kernel/ps/quotagov.cpp) | C/E | U | [quota_svchost_primes.hpp](../include/micant/km/executive/quota_svchost_primes.hpp#L272) imports threads, maps, strings and locks. R/S; resource accounting and ownership need actual Ps integration. |
| [kernel/ke/kuser_seal.cpp](../kernel/ke/kuser_seal.cpp) | D/E | U | [kuser_upcall_seal.hpp](../include/micant/km/core/kuser_upcall_seal.hpp#L168) export/report initializer is not actual KUSER mapping/MSR/upcall setup. Connect existing HAL mechanisms and shared-page lifecycle. |
| [kernel/ps/ring1_smss.cpp](../kernel/ps/ring1_smss.cpp) | E | U | [ring1_smss_csrss_bootstrap.hpp](../include/micant/km/executive/ring1_smss_csrss_bootstrap.hpp#L150) bootstrap reports include fixed process identity. Ps/loader/Ob/LPC execution required. |
| [kernel/ps/ring2_scm.cpp](../kernel/ps/ring2_scm.cpp) | E | U | [ring2_scm_services_broker.hpp](../include/micant/km/executive/ring2_scm_services_broker.hpp#L163) reports fixed service graph/IDs, not service process startup. |
| [kernel/ps/ring3_win32_base.cpp](../kernel/ps/ring3_win32_base.cpp) | B/E | U | [ring3_win32_base_bridge.hpp](../include/micant/um/native/ring3_win32_base_bridge.hpp#L48) and source `printf` summaries. Actual user DLL loading, TLS/TEB and service routing required. |
| [kernel/ps/ring3_desktop_shell.cpp](../kernel/ps/ring3_desktop_shell.cpp) | B/E | U | [ring3_winlogon_explorer_dwm.hpp](../include/micant/um/servers/ring3_winlogon_explorer_dwm.hpp#L49) and source print reports. No evidence of actual Explorer/DWM process execution. |
| [kernel/ps/ring3_real_apps.cpp](../kernel/ps/ring3_real_apps.cpp) | B/E | U | [ring3_real_apps_triad.hpp](../include/micant/um/subsystems/winuser/ring3_real_apps_triad.hpp#L51) report printing is not loading/scheduling named applications. |
| [kernel/ps/ring3_workstation_apps.cpp](../kernel/ps/ring3_workstation_apps.cpp) | E | U | [ring3_workstation_apps_tunnel.hpp](../include/micant/um/subsystems/winuser/ring3_workstation_apps_tunnel.hpp#L102) report coordinator; actual Ps/Ke application launches unverified. |
| [kernel/ps/ring3_wsl2_hyperv_closure.cpp](../kernel/ps/ring3_wsl2_hyperv_closure.cpp) | E | U | [ring3_wsl2_hyperv_enterprise_closure.hpp](../include/micant/um/servers/ring3_wsl2_hyperv_enterprise_closure.hpp#L112) closure initializer; not a live service/VM/process path. |
| [kernel/ps/winnt_kernel32_advapi32_system32_abi.cpp](../kernel/ps/winnt_kernel32_advapi32_system32_abi.cpp) | E | U | User DLL ABI coordinator at header line 59 and zero-like API implementations. Must stay in user-mode targets, not become the kernel allocator/loader service implementation. |
| [kernel/ps/winnt_ole32_dwmapi_winmm_system32_seal.cpp](../kernel/ps/winnt_ole32_dwmapi_winmm_system32_seal.cpp) | C/E | U | User DLL coordinator at header line 60; C allocation/free/memory and thread-local state. Actual COM/window/composition lifecycle unverified. |
| [kernel/ps/winnt_ntdll_ws2_iphlpapi_winhttp_abi.cpp](../kernel/ps/winnt_ntdll_ws2_iphlpapi_winhttp_abi.cpp) | C/E | U | User DLL coordinator at header line 142; C allocation, TLS and memory/string operations. Route ntdll to canonical NT services; do not link user networking API ownership into the kernel. |
| [kernel/ps/winnt_shell32_dbghelp_version_40dll_iat_seal.cpp](../kernel/ps/winnt_shell32_dbghelp_version_40dll_iat_seal.cpp) | C/E | U | User DLL coordinator at header line 126, heap/memory routines; runtime/procedure/loader dependencies and placeholder behaviors. |
| [kernel/ps/winnt_ucrtbase_runtime_abi.cpp](../kernel/ps/winnt_ucrtbase_runtime_abi.cpp) | C | P | Baseline hosted branch leaves `_tls_index`, heap, memory/string, stdio and math symbols unresolved. Pending native memory subset is separate below; neither branch is a kernel runtime. |
| [kernel/ps/winnt_vcruntime140_eh_abi.cpp](../kernel/ps/winnt_vcruntime140_eh_abi.cpp) | C/E | U | User EH/runtime coordinator at header line 68, including exception and setjmp/longjmp surfaces. Requires deliberate unwind/runtime contracts, not success stubs under no-exceptions. |
| [kernel/ps/winnt_msvcp140_pdata_unwind_44dll_seal.cpp](../kernel/ps/winnt_msvcp140_pdata_unwind_44dll_seal.cpp) | C | U | User C++ coordinator at header line 90 uses vector and C strings. Bound allocation and actual unwind support; not a kernel runtime provider. |

Reuse `KThread`, `PriorityScheduler` and `KiSwapContext` in
[ke.hpp](../include/micant/km/core/ke.hpp#L399), and `EThread`/`ProcessManager`
in [ps.hpp](../include/micant/km/executive/ps.hpp#L279).
The array-based `UserProcessManager64` and `UserThreadScheduler64` in
[usermode.hpp](../include/micant/km/executive/usermode.hpp) are a **class F
ownership conflict** if activated as a second Ps/Ke. Image preparation must
not become a third process manager. The new bounded image set owns neither
lifecycle nor scheduling. Hosted process/wait/report suites cover different
levels, not hardware context switching.

## Io, PnP and Po (47 units)

Header basenames below identify the canonical include at the cited source
line; subsequent mechanism line numbers refer to that header. The E findings
identify particular report/no-op paths, not a claim that every helper in a
header is empty. Transitive runtime closure of the uncompiled advanced
features remains unverified; they are not first-boot dependencies.

| Source | Class | Compile | Evidence / dependency / next adapter |
|---|---|---|---|
| [kernel/io/iomgr.cpp](../kernel/io/iomgr.cpp) | C | P | Includes `io.hpp` at 2; global maps at source 13-15, header 121/516/477: STL, allocation and singleton. R/S, MicaNT filesystem/handle dependencies and explicit constructor lifecycle. |
| [kernel/io/afd_wsk.cpp](../kernel/io/afd_wsk.cpp) | C | U | Includes `afd_wsk_rio.hpp` at 9; header 243/238: STL and singleton; initializer called at source 14. R/S; actual network dispatch backend required. |
| [kernel/io/livehotswap.cpp](../kernel/io/livehotswap.cpp) | C/D | U | Includes `live_hotswap_storheal.hpp` at 9; header 52/128: STL and singleton; source 14 initializer. R/S/H; actual device removal/recovery is separate from state registration. |
| [kernel/io/pnpboot.cpp](../kernel/io/pnpboot.cpp) | C | F | Includes `pnp_boot_scheduler.hpp` at 2; header 75/191: STL and singleton. J blocks compile; boot scheduling needs canonical Io/Ke rather than hosted loader registration. |
| [kernel/io/pnpresilience.cpp](../kernel/io/pnpresilience.cpp) | C | U | Includes `driver_resilience.hpp` at 2; header 43/160: STL and singleton; source 7 initializer. R/S and real recovery/driver lifecycle. |
| [kernel/pnp/pnpmgr.cpp](../kernel/pnp/pnpmgr.cpp) | E/C | U | Includes `pnp.hpp` at 2; source 6-8 initializer is a no-op; header 160/527: STL and singleton. Canonical device tree/IRP state machine must actually be initialized. |
| [kernel/po/pomgr.cpp](../kernel/po/pomgr.cpp) | C | U | Includes `po.hpp` at 2; calls at source 9-14; header 182/265: STL and singleton. R/S; actual power IRPs and state transitions need Io/Ke. |
| [kernel/po/pofx_whea.cpp](../kernel/po/pofx_whea.cpp) | C/D | U | Includes `pofx_whea.hpp` at 9; header 69/181: STL and singleton. R/S/H; platform error/power delivery remains unverified. |
| [kernel/po/bms.cpp](../kernel/po/bms.cpp) | D/C | F | Includes `bms.hpp` at 9; header 80 initializes ACPI bus, 164/183: STL/singleton. J and `ex_work.hpp:385` block compile; actual firmware/bus backend needed. |
| [kernel/po/ppm.cpp](../kernel/po/ppm.cpp) | D/C | U | Includes `ppm.hpp` at 10; header 442 writes HWP MSR with `wrmsr`, 146 uses STL. R/H; capability checks and correct CPU/backend execution required. |
| [kernel/po/thermal.cpp](../kernel/po/thermal.cpp) | C/D | U | Includes `thermal.hpp` at 10; header 73 initialization, 110 STL. R/S/H; real ACPI thermal readings/actions are separate from host state. |
| [kernel/io/umdf_msix.cpp](../kernel/io/umdf_msix.cpp) | C/D | U | Includes `umdf_msix_guard.hpp` at 9; header 57/139: STL/singleton. R/S/H; actual MSI-X programming and driver process ownership needed. |
| [kernel/io/ring1_dxgk.cpp](../kernel/io/ring1_dxgk.cpp) | E/D | U | Includes `ring1_dxgkrnl_flip_plane.hpp` at 9; header 129-134 contains simulated cursor/register reports. Not the verified VirtIO boot-driver path. |
| [kernel/io/ring1_audio.cpp](../kernel/io/ring1_audio.cpp) | E/D | U | Includes `ring1_wavert_audio_dma.hpp` at 9; header 110-115 contains fixed APO reports, not verified DMA/audio operation. |
| [kernel/io/ring2_wudf.cpp](../kernel/io/ring2_wudf.cpp) | E | U | Includes `ring2_wudf_fontdrv_host.hpp` at 9; header 89-91 contains canned host PID/counts. Requires actual Ps/driver-host processes. |
| [kernel/io/ring3_prismx_3d12.cpp](../kernel/io/ring3_prismx_3d12.cpp) | E/C | U | Includes `ring3_prismx_3d12_shader_tunnel.hpp` at 10; header 118 fixed command-list result; source 8/18 uses host streams. R/V; no live GPU tunnel established. |
| [kernel/io/ring3_directstorage_d2d.cpp](../kernel/io/ring3_directstorage_d2d.cpp) | E/C | U | Includes `ring3_directstorage_d2d_vulkan_tunnel.hpp` at 10; header 160 fixed queued bytes; source host stream/report output. Actual storage/GPU work unverified. |
| [kernel/io/ring3_prismaudio_mf.cpp](../kernel/io/ring3_prismaudio_mf.cpp) | E/C | U | Includes `ring3_prismaudio_wasapi_mf_tunnel.hpp` at 10; header 135 fixed voice state; source host streams/report output. Real audio/service execution required. |
| [kernel/io/ring3_winsock_winhttp_tls.cpp](../kernel/io/ring3_winsock_winhttp_tls.cpp) | E | U | Includes `ring3_winsock_winhttp_tls_tunnel.hpp` at 8; header 84 hard-coded bound port. Export initialization is not a live socket/TLS session. |
| [kernel/io/ring3_fiber_net_apps.cpp](../kernel/io/ring3_fiber_net_apps.cpp) | E | U | Includes `ring3_fiber_net_apps_tunnel.hpp` at 8; header 60 fixed network counters. Need real process/fiber/network execution. |
| [kernel/io/prismx_directx_headers_kmt.cpp](../kernel/io/prismx_directx_headers_kmt.cpp) | E | U | Includes `prismx_directx_headers_kmt_runtime.hpp` at 8; header 115 fixed adapter count. Actual adapter enumeration/dispatch unverified. |
| [kernel/io/prismx_dxmath_dxtex_dxmesh.cpp](../kernel/io/prismx_dxmath_dxtex_dxmesh.cpp) | E/C | U | Includes `prismx_dxmath_dxtex_dxmesh_tk12.hpp` at 8; header 91 fixed draw count, 82 STL. Real math/helpers do not prove GPU draws. |
| [kernel/io/prismx_d3d12_translation_on12.cpp](../kernel/io/prismx_d3d12_translation_on12.cpp) | E | U | Includes `prismx_d3d12_translation_layer_on12.hpp` at 8; header 83 fixed translated-operation telemetry. Real command translation/submission unverified. |
| [kernel/io/prismx_dxc_sm68_pix.cpp](../kernel/io/prismx_dxc_sm68_pix.cpp) | E/C | U | Includes `prismx_dxc_sm68_pix_runtime.hpp` at 8; header 52 compiler-initialized flag, 43 singleton. Flag alone does not establish a compiler/profiler backend. |
| [kernel/io/prismx_directstorage_gdeflate.cpp](../kernel/io/prismx_directstorage_gdeflate.cpp) | E | U | Includes `prismx_directstorage_gdeflate_gpu.hpp` at 8; header 94 fixed throughput. Actual decompression/transfer path must be measured independently. |
| [kernel/io/prismx_directml_superres.cpp](../kernel/io/prismx_directml_superres.cpp) | E | U | Includes `prismx_directml_superres_graph.hpp` at 8; header 99-100 fixed device/resolution report. No verified ML GPU execution. |
| [kernel/io/prismx_enhanced_barriers_workgraphs.cpp](../kernel/io/prismx_enhanced_barriers_workgraphs.cpp) | E | U | Includes `prismx_enhanced_barriers_workgraphs_sm69.hpp` at 8; header 42-43 fixed transition count. Barrier/workgraph execution not proven. |
| [kernel/io/prismx_vulkanon12_video_av1.cpp](../kernel/io/prismx_vulkanon12_video_av1.cpp) | E | U | Includes `prismx_vulkanon12_video_av1_hevc.hpp` at 8; header 44-45 fixed capability flags. Real video/translation backend remains unverified. |
| [kernel/io/prismx_dred_shadercache_zerotdr.cpp](../kernel/io/prismx_dred_shadercache_zerotdr.cpp) | E | U | Includes `prismx_dred_shadercache_zerotdr_seal.hpp` at 8; header 49-50 fixed active/capacity report. Actual device-removal/cache/recovery lifecycle required. |
| [kernel/io/prismx_live_3d_scene_framegraph.cpp](../kernel/io/prismx_live_3d_scene_framegraph.cpp) | E/C | U | Includes same-basename header at 8; header 113-122 sets all GPU-pass flags true, then software-renders into vector storage. R; not hardware-accelerated composition evidence. |
| [kernel/io/prismx_gop_scanout_frame_pacing.cpp](../kernel/io/prismx_gop_scanout_frame_pacing.cpp) | E/C | U | Includes same-basename header at 8; header 62-63 claims mapped GOP/direct flip after vector-backed rendering (47). R; distinguish simulation from actual scanout. |
| [kernel/io/prismx_game_benchmark_proton_seal.cpp](../kernel/io/prismx_game_benchmark_proton_seal.cpp) | E | U | Includes same-basename header at 8; header 52-61 fixed pass/FPS/latency results. Not measured game/Proton execution. |
| [kernel/io/prismx_directsr_vrs_rebar_gaming.cpp](../kernel/io/prismx_directsr_vrs_rebar_gaming.cpp) | E | U | Includes same-basename header at 8; header 65-66 fixed engine/capability report. Actual device/backend support unverified. |
| [kernel/io/prismx_execute_indirect_sampler_feedback.cpp](../kernel/io/prismx_execute_indirect_sampler_feedback.cpp) | E | U | Includes same-basename header at 8; header 60-61 fixed command-signature report. Real indirect execution/feedback path required. |
| [kernel/io/prismx_gameinput_pix_classic_gaming_seal.cpp](../kernel/io/prismx_gameinput_pix_classic_gaming_seal.cpp) | E | U | Includes same-basename header at 8; header 57-58 fixed polling report. Requires actual input/profiler/backend evidence. |
| [kernel/io/prismx_hlsl_fxc_dxbc_reflection_compiler.cpp](../kernel/io/prismx_hlsl_fxc_dxbc_reflection_compiler.cpp) | E | U | Includes same-basename header at 8; header 57-58 fixed compiler-active flag. Real parsing/translation/output must be tested separately. |
| [kernel/io/prismx_dxil_sm69_wave32_simd_vm.cpp](../kernel/io/prismx_dxil_sm69_wave32_simd_vm.cpp) | E | U | Includes same-basename header at 9; header 56-57 fixed container/version report. This does not verify actual shader VM execution. |
| [kernel/io/prismx_spirv16_dxil_cross_translator_seal.cpp](../kernel/io/prismx_spirv16_dxil_cross_translator_seal.cpp) | E | U | Includes same-basename header at 8; header 54-55 fixed module-valid/magic report. Actual translator correctness unverified. |
| [kernel/io/prism_d2d1_dwrite_dcomp_11dll_seal.cpp](../kernel/io/prism_d2d1_dwrite_dcomp_11dll_seal.cpp) | E | U | Includes same-basename header at 13; header 65-66 fixed DLL/export counts. Symbol reports do not establish window/composition services. |
| [kernel/io/prism_d3d12_vtable_options21.cpp](../kernel/io/prism_d3d12_vtable_options21.cpp) | E | U | Includes same-basename header at 8; header 55-68 fixed vtable/capability values. Actual device/API behavior unverified. |
| [kernel/io/prism_d3d9ex_com_vtable_abi.cpp](../kernel/io/prism_d3d9ex_com_vtable_abi.cpp) | E | U | Includes same-basename header at 13; header 80-81 fixed vtable counts. Correct vtables/contracts need behavioral tests. |
| [kernel/io/prism_directx_system32_dll_router.cpp](../kernel/io/prism_directx_system32_dll_router.cpp) | E | U | Includes same-basename header at 14; header 47-48 fixed DLL/export counts. Real loader resolution/lifecycle not established by registration. |
| [kernel/io/prism_dxcore_adapter_factory_com_abi.cpp](../kernel/io/prism_dxcore_adapter_factory_com_abi.cpp) | E | U | Includes same-basename header at 13; header 75-76 fixed vtable sizes. Actual adapter factory/device enumeration unverified. |
| [kernel/io/prism_dxgi16_com_hierarchy.cpp](../kernel/io/prism_dxgi16_com_hierarchy.cpp) | E | U | Includes same-basename header at 8; header 147-148 fixed interface-query counts. No actual display/session backend demonstrated. |
| [kernel/io/prism_mfplat_mfreadwrite_cutscene_abi.cpp](../kernel/io/prism_mfplat_mfreadwrite_cutscene_abi.cpp) | E | U | Includes same-basename header at 13; header 74-75 fixed version/export counts. Decode/render/lifecycle behavior unverified. |
| [kernel/io/prism_vulkan1_opengl32_16dll_seal.cpp](../kernel/io/prism_vulkan1_opengl32_16dll_seal.cpp) | E | U | Includes same-basename header at 13; header 56-57 fixed profile/DLL report. Actual Vulkan/OpenGL dispatch and device work required. |
| [kernel/io/prism_xaudio2_dinput8_com_abi.cpp](../kernel/io/prism_xaudio2_dinput8_com_abi.cpp) | E | U | Includes same-basename header at 13; header 72-73 fixed vtable/channel report. Actual audio/input backend remains unverified. |

`iomgr.cpp`'s object leaves libc++ strings/shared pointers/locks/CV/chrono/
iostream, allocation/deallocation, wide-character/memory functions, `atexit`,
and MicaNT filesystem/handle helpers unresolved. Existing hosted Io, driver,
power and feature suites are not driver/IRP execution in QEMU.

[io_baremetal.hpp](../include/micant/km/executive/io_baremetal.hpp) is **F**:
separate fixed-pool `BareMetalIrp`/device types and a disk callback, not an
adapter to canonical `micant::io`. Retain the canonical `Irp`,
`IoStackLocation`, `DriverObject`, `DeviceObject` and completion interfaces.
The first integration should be one statically registered driver and an
actual synchronous IRP, after shared runtime and locking are available.

## Reproducible probes and baseline

All worker production-definition probes used the repository's existing
AMD64 LLVM-MinGW compiler with:

```bash
CXX=build/llvm-mingw-20260616-ucrt-macos-universal/bin/x86_64-w64-mingw32-clang++
"$CXX" -std=c++23 -DMICANT_FREESTANDING -ffreestanding \
  -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit \
  -mno-red-zone -fno-stack-protector -Iinclude -O0 \
  -c kernel/mm/virtual.cpp -o build/freestanding-audit/virtual.obj
build/llvm-mingw-20260616-ucrt-macos-universal/bin/llvm-nm \
  --undefined-only build/freestanding-audit/virtual.obj
```

Result: object compile passed; unresolved families recorded above. Substitute
the table's exact source to reproduce the other production-definition probes.
Generic probes without the production define were also used for Mm/ExtFS/Cc,
Ob/Se/KTM, dispatcher/Ps; only the explicitly repeated units establish the
production-definition result. Cc, ExtFS and Ps generic outcomes are retained
as such, not relabeled production builds. Io's three probes and the primary
three probes all used the define. Ps was not repeated; its unguarded exception
syntax remains a demonstrated generic-build blocker.

`cmake --build build/qemu-host-config --target micant_tests -j 1` was attempted
on the ARM64 macOS host. It stopped in Cc's transitive
[ntdll.hpp](../include/micant/um/native/ntdll.hpp#L8140) with an x86 `=a`
assembly constraint and additional `__uint128_t` parsing errors at 8325.
**Full host suites did not run.** This is separate from the passing standalone
sanitizer tests and the AMD64 QEMU baseline.

Primary verified commands:

- `python3 qemu.py --build-only`: passed.
- `python3 qemu.py --headless`: passed, including PE diagnostic, CPL3 memory
  protections, mouse and scanout checks.
- `python3 scripts/verify_kernel_architecture.py`: passed.

## Integration boundary

First solve shared bounded allocation/backing, constructor lifecycle,
exception-free header boundaries and synchronization/time adapters. Then
connect the existing Mm, Ob, Ke, Ps and Io in dependency order. Do not solve
missing symbols with the archived kernel, hosted CRT DLLs, fake Windows
exports or generic-success trampolines.

The native UCRT memory branch has no imports, a 64 MiB coalescing arena and
real PE static TLS for `errno`. It is user-mode-only, not an Mm/Ex replacement;
static TLS initialization and actual execution are still prerequisites.
Canonical bounded PE parsing/binding is exercised, but neither a PE mapping
test nor module symbol closure establishes DLL attach or process startup.

Defer the advanced filesystem, graphics, feature-report and security-seal
entries outside the first-boot dependency path. Preserve their existing host
behavior until they are explicitly integrated and verified; do not rewrite
them just to make a first boot appear complete.
