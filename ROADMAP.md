# MicaNT Master Architecture Roadmap

> **Mission:** Build the world's first clean-room, zero-telemetry, modern ISO C++23 NT-compatible operating system kernel and executive, built from first principles referencing Microsoft's MIT-licensed [`microsoft/win32metadata`](https://github.com/microsoft/win32metadata).

---

## Roadmap Phases

```
┌────────────────────────────────────────────────────────────────────────┐
│ Phase 1: Ring 0 Kernel & Executive Architecture       [COMPLETED 100%] │
├────────────────────────────────────────────────────────────────────────┤
│ Phase 2: Kernel Hardening & Concurrency Stress Tests  [COMPLETED 100%] │
├────────────────────────────────────────────────────────────────────────┤
│ Phase 3: Ring 3 Userland Runtime & ntdll.dll                [NEXT]     │
├────────────────────────────────────────────────────────────────────────┤
│ Phase 4: Bare-Metal UEFI Loader (bootx64.efi on QEMU)      [PLANNED]  │
├────────────────────────────────────────────────────────────────────────┤
│ Phase 5: Client-Server Runtime Subsystem (CSRSS & ConHost)  [PLANNED]  │
├────────────────────────────────────────────────────────────────────────┤
│ Phase 6: Visual Identity & Custom Boot Splash (BGRT / GOP) [PLANNED]  │
├────────────────────────────────────────────────────────────────────────┤
│ Phase 7: WoW64 32-Bit Subsystem & AArch64 Port             [FUTURE]   │
└────────────────────────────────────────────────────────────────────────┘
```

---

### Phase 1: Ring 0 Kernel & Executive Subsystems (100% Completed)
- [x] **HAL**: Multi-core SMP topology (4 cores), `KPCR` at `GS:[0]`, `KPRCB`, 1 GHz nanosecond timers.
- [x] **Kernel Core (KE)**: 5-level IRQL state machine, `KSPIN_LOCK` with auto-IRQL elevation, DPCs, APCs, 32-queue priority scheduler.
- [x] **Executive Pools (EX)**: `NonPagedPool` and `PagedPool` with strict IRQL access control and 4-byte diagnostic pool tagging (`'Mica'`).
- [x] **Trap Engine**: Demand Paging, Copy-on-Write (#PF Vector 14), SEH Dispatcher, and `KeBugCheckEx` crash minidumps.
- [x] **Configuration Manager (CM)**: `\Registry` hive mounted in-memory with pre-seeded Session Manager keys (`OSBuild` = 26100, `ZeroTelemetryEnabled` = 1).
- [x] **Security Reference Monitor (SRM)**: SIDs, DACLs, ACEs, Process Access Tokens, Cutler's `accessCheck` validation.
- [x] **Advanced Local Procedure Call (ALPC)**: Named ports under `\RPC Control`, synchronous `requestWaitReply` rendezvous.
- [x] **I/O Manager & IOCP**: Driver and Device object graph (`\Device\Null`, `\Device\Harddisk0`), IRP dispatching, I/O Completion Ports.
- [x] **Virtual File System & FastFAT**: `\Driver\Fastfat` mounting `\Device\Harddisk0\Partition1` as `\DosDevices\C:`, `FileObject` lifecycle, pre-seeded system binaries.
- [x] **Executive Work Queues**: Background worker threads at `PASSIVE_LEVEL` servicing `CriticalWorkQueue` and `DelayedWorkQueue`.
- [x] **Boot Contract**: Standard UEFI `LOADER_PARAMETER_BLOCK` parsing physical memory descriptors and load options (`/ZERO_TELEMETRY=1`).
- [x] **Driver Model & IOCTLs**: `CTL_CODE` constexpr macro, `DriverEntry` contract, `NtDeviceIoControlFile` (SSN `0x0007`).
- [x] **Kernel Timers & DPCs**: `KTIMER`, notification/synchronization timer objects, timer DPCs, `NtDelayExecution` (SSN `0x0034`).
- [x] **Multi-Object Synchronization**: `NtWaitForMultipleObjects` (SSN `0x005A`) supporting up to 64 handles with `WaitAny` and `WaitAll`.
- [x] **Driver Lookaside Lists**: `NPAGED_LOOKASIDE_LIST` (lock-free/spinlock O(1) allocation) and `PAGED_LOOKASIDE_LIST` with IRQL enforcement.
- [x] **Power Management & Shutdown**: `Po` manager, `IRP_MJ_POWER` broadcast, `NtShutdownSystem` (SSN `0x0118`) cleanly transitioning to S5 Soft Off.
- [x] **KiSystemCall64**: Central dispatch table with 33 registered core NT system calls.

---

### Phase 2: Kernel Hardening & Concurrency Stress Testing (100% Completed)
*Goal: Ensure the kernel is rock-solid, deadlock-free, crash-resilient, and capable of sustained high-concurrency workloads.*
- [x] **Multi-Threaded Pool Concurrency**: Concurrent allocations/frees from 8+ threads across `NonPagedPool` and `PagedPool` (Suite 18).
- [x] **Spinlock Contention & IRQL Invariants**: Zero deadlocks and correct IRQL restoration under heavy SMP lock contention (8,000 ops - Suite 18).
- [x] **High-Throughput IOCP Stress**: 1,000+ asynchronous completion packets dispatched across concurrent worker threads (Suite 18).
- [x] **Kernel Driver Lifecycle & IOCTL Polling**: `DriverEntry` hardware polling, parameter validation, small buffer detection (Suite 19).
- [x] **Timer DPCs & Sleep Interval Accuracy**: Expiration callbacks, cancellation, nanosecond sleep intervals (Suite 20).
- [x] **Multi-Handle Wait & Timeout Resiliency**: Synchronizing combinations of Events, Mutants, Semaphores, and Timers (Suite 21).
- [x] **Lookaside Allocation Caching & Telemetry**: 100% cache hit recycling, memory depth management, pool bypass (Suite 22).
- [x] **Device Power Handover & Shutdown Pipeline**: ACPI sleep state propagation and clean subsystem termination (Suite 23).
- [x] **Robust Parameter Probing (`ProbeForRead` / `ProbeForWrite`)**: 48-bit canonical user address range checks and power-of-2 alignment.

---

### Phase 3: Ring 3 Userland Bridge & Runtime
*Goal: Enable standard 64-bit Windows userland applications to execute against MicaNT.*
- [ ] **Clean-Room `ntdll.dll` Export Surface**: Export `Nt*` / `Zw*` system call stubs with inline `syscall` instructions.
- [ ] **Userland Heap Manager**: `RtlCreateHeap`, `RtlAllocateHeap`, `RtlFreeHeap` with block headers and free lists.
- [ ] **Userland PE Loader (`ntdll!Ldr`)**:
  - `LdrInitializeThunk`: Userland entry thunk initializing PEB and TEB.
  - `LdrLoadDll`: Dynamic library loading and import resolution.
- [ ] **CRT Initialization**: Run target binary's `mainCRTStartup` / `WinMainCRTStartup`.

---

### Phase 4: Bare-Metal UEFI Bootloader (bootx64.efi on QEMU)
*Goal: Boot MicaNT directly on physical hardware and QEMU virtual machines.*
- [ ] **`bootx64.efi` PE32+ Application**: Built using standard UEFI 2.x headers.
- [ ] **GOP Framebuffer Discovery**: Query native display mode via UEFI Graphics Output Protocol.
- [ ] **ACPI RSDP Discovery**: Extract ACPI tables from `EFI_CONFIGURATION_TABLE`.
- [ ] **ExitBootServices Handoff**: Terminate UEFI services, initialize higher-half paging, and jump to `KiSystemStartup(LOADER_PARAMETER_BLOCK*)`.
- [ ] **Automated QEMU Runner**: `scripts/run_qemu.bat` with OVMF firmware.

---

### Phase 5: Client-Server Runtime Subsystem (CSRSS & ConHost)
*Goal: Implement the core Win32 subsystem daemons.*
- [ ] **`csrss.exe` Daemon**: High-speed ALPC message processor maintaining Win32 process table.
- [ ] **Console Host (`conhost.exe`)**: Standard Win32 text console rendering to the screen buffer.
- [ ] **`kernel32.dll` / `kernelbase.dll` Implementation**: Standard Win32 API layer wrapping `ntdll` syscalls.

---

### Phase 6: Visual Identity & Custom Boot Splash
*Goal: Provide a distinctive, customizable, and polished user experience.*
- [ ] **GOP Boot Video Driver (`bootvid.hpp`)**: Direct linear framebuffer blitting with double-buffering.
- [ ] **BMP / Bitmap Parser**: Load custom 24-bit/32-bit boot images from `\DosDevices\C:\Windows\Boot\bootlogo.bmp`.
- [ ] **Registry-Configured Splash**: Custom logo path, background color, and progress bar controls.
- [ ] **ACPI BGRT Passthrough**: Adopt OEM boot logo from motherboard firmware seamlessly.
- [ ] **`/SOS` Verbose Diagnostic Toggle**: Stream live kernel diagnostic trace during boot.

---

### Phase 7: WoW64 32-Bit Subsystem & AArch64 Architecture Expansion
*Goal: Expand binary compatibility and hardware reach.*
- [ ] **WoW64 Subsystem (Windows 32-bit on Windows 64-bit)**:
  - 32-bit address space layout with 32-bit PEB and TEB32.
  - `wow64cpu.dll` instruction thunking between 32-bit and 64-bit modes (`heaven's gate` / `sysenter`).
- [ ] **AArch64 (ARM64) Port**:
  - Exception levels (EL1 kernel, EL0 user).
  - ARM64 translation tables (TTBR0/TTBR1).
  - SVC instruction syscall trap dispatcher.
