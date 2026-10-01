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
│ Phase 3: Ring 3 Userland Runtime & ntdll.dll          [COMPLETED 100%] │
├────────────────────────────────────────────────────────────────────────┤
│ Phase 4: Bare-Metal UEFI Loader (bootx64.efi)         [COMPLETED 100%] │
├────────────────────────────────────────────────────────────────────────┤
│ Phase 5: Client-Server Runtime Subsystem (CSRSS)      [COMPLETED 100%] │
├────────────────────────────────────────────────────────────────────────┤
│ Phase 6: Visual Identity & Custom Boot Splash (bootvid)[COMPLETED 100%] │
├────────────────────────────────────────────────────────────────────────┤
│ Phase 7: WoW64 32-Bit Subsystem                       [COMPLETED 100%] │
├────────────────────────────────────────────────────────────────────────┤
│ Phase 8: Dynamic PE Import Binding & Base Relocations [COMPLETED 100%] │
├────────────────────────────────────────────────────────────────────────┤
│ Phase 9: AArch64 (ARM64) Architecture Port                [NEXT]       │
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

### Phase 3: Ring 3 Userland Bridge & Runtime (100% Completed)
*Goal: Enable standard 64-bit Windows userland applications to execute against MicaNT.*
- [x] **Clean-Room `ntdll.dll` Export Surface**: Exported all 33 `Nt*` / `Zw*` system call stubs with `KiSystemCall64` dispatch frames (`include/micant/ntdll.hpp`).
- [x] **Userland Heap Manager**: `RtlCreateHeap`, `RtlAllocateHeap`, `RtlFreeHeap`, `RtlDestroyHeap`, `RtlSizeHeap`, `RtlReAllocateHeap` with Best-Fit, chunk headers, and free list coalescing (`include/micant/heap.hpp`).
- [x] **Userland PE Loader (`ntdll!Ldr`)**:
  - `LdrInitializeThunk`: Userland entry thunk initializing PEB, TEB, default process heap, and standard I/O handles (`include/micant/ldr.hpp`).
  - `LdrLoadDll` & `LdrGetProcedureAddress`: Dynamic library loading and export symbol resolution.
- [x] **Native Userland Execution Harness**: `test/userland_app.cpp` demonstrating end-to-end userland process lifecycle, heap allocation, console writing, and clean exit.
- [x] **Test Suites 24 & 25**: Full verification of `ntdll` stubs, TEB/PEB linkage, Best-Fit heap allocation, and bidirectional chunk coalescing (25/25 suites passing).

---

### Phase 4: Bare-Metal UEFI Bootloader (bootx64.efi) (100% Completed)
*Goal: Boot MicaNT directly on physical hardware and QEMU virtual machines.*
- [x] **`bootx64.efi` PE32+ Application**: Pure UEFI 2.10 entry point (`boot/bootx64.cpp`, `include/micant/uefi.hpp`).
- [x] **GOP Framebuffer Discovery**: Query native resolution, pitch, and pixel format via UEFI Graphics Output Protocol.
- [x] **ACPI RSDP Discovery**: Extract ACPI 2.0 / 1.0 root system pointer from `EFI_CONFIGURATION_TABLE`.
- [x] **UEFI Memory Map Translation**: Map Conventional, LoaderCode, BootServices, ACPIReclaim to NT `LoaderMemoryType` descriptors.
- [x] **ExitBootServices Handoff**: Terminate UEFI services, build `LOADER_PARAMETER_BLOCK`, and handoff to kernel.
- [x] **Test Suite 26**: `Test_UefiBootloader_GopAndMemoryMap` verified.

---

### Phase 5: Client-Server Runtime Subsystem (CSRSS & ConHost) (100% Completed)
*Goal: Implement the core Win32 subsystem daemons and standard Win32 base API parity.*
- [x] **`csrss.exe` Subsystem Server (`include/micant/csrss.hpp`)**: High-speed ALPC message processor maintaining Win32 process/thread tracking tables, process registration, termination broadcast, and `\RPC Control\WindowsSubsystem` rendezvous.
- [x] **Console Host Engine (`conhost.hpp`)**: Standard Win32 text console with 2D character matrix, scrolling, automatic line wrapping, cursor tracking, and GOP linear framebuffer terminal blitting.
- [x] **`kernel32.dll` / `kernelbase.dll` API Bridge (`include/micant/kernel32.hpp`)**: Clean-room implementation of standard Win32 Base APIs wrapping `ntdll` syscalls and CSRSS:
  - Memory: `GetProcessHeap`, `HeapAlloc`, `HeapFree`, `HeapReAlloc`, `HeapSize`, `VirtualAlloc`, `VirtualFree`.
  - Process/Thread: `GetCurrentProcess`, `GetCurrentProcessId`, `GetCurrentThread`, `GetCurrentThreadId`, `ExitProcess`.
  - Console: `AllocConsole`, `FreeConsole`, `SetConsoleTitleW`, `GetConsoleTitleW`, `GetStdHandle`, `SetStdHandle`, `WriteConsoleW`.
  - File I/O: `CreateFileW`, `ReadFile`, `WriteFile`, `CloseHandle`.
  - Synchronization: `CreateEventW`, `SetEvent`, `ResetEvent`, `WaitForSingleObject`, `WaitForMultipleObjects`, `Sleep`.
  - System Info & Time: `GetSystemInfo`, `GetTickCount64`, `LoadLibraryW`, `GetProcAddress`, `FreeLibrary`.
- [x] **Native Win32 App Harness (`test/win32_app.cpp`)**: Standalone binary exercising end-to-end Win32 APIs without host CRT dependencies.
- [x] **Test Suites 28, 29, 30 (`test/test_runner.cpp`)**: Complete verification of CSRSS process tracking/ALPC, ConHost framebuffer blitting, and Kernel32 Win32 API parity (30/30 suites passing).

---

### Phase 6: Visual Identity & Custom Boot Splash (bootvid) (100% Completed)
*Goal: Provide a distinctive, customizable, and polished user experience.*
- [x] **GOP Boot Video Driver (`include/micant/bootvid.hpp`)**: Direct linear framebuffer primitives, alpha blending, gradients, Bresenham lines, and circles.
- [x] **Clean-Room 8x8 Typography**: Scalable font rendering with automatic center alignment.
- [x] **Dave Cutler's 1988 DEC Mica Prism Emblem**: Procedural multi-faceted crystal prism refracting cyan, blue, violet, and amber spectral beams.
- [x] **BMP / Bitmap Parser & Encoder**: Load and decode custom 24-bit/32-bit user boot images (`BmpCodec::decode` and `BmpCodec::encode`).
- [x] **Interactive Boot Splash & Progress Bar**: Dynamic progress tracking and orbital loading spinners across boot stages.
- [x] **Classic NT `bootvid.dll` Export Parity**: `VidInitialize`, `VidResetDisplay`, `VidDisplayString`, `VidSolidColorFill`, `VidBufferToScreenBlt`.
- [x] **Test Suite 27**: `Test_BootVid_FramebufferAndSplashRenderer` verified.

---

### Phase 7: WoW64 32-Bit Subsystem (100% Completed)
*Goal: Enable transparent execution of 32-bit x86 Windows applications on 64-bit MicaNT.*
- [x] **32-Bit PE Format Support (`include/micant/pe.hpp`)**:
  - `ImageOptionalHeader32` and `ImageNtHeaders32` structures.
  - `MACHINE_I386` (0x014C) and `PE32_MAGIC` (0x010B) recognition.
  - `PeLoader::inspect32`: Clean-room 32-bit PE header inspection and section table parsing.
- [x] **WoW64 Subsystem Architecture (`include/micant/wow64.hpp`)**:
  - 32-bit types: `PVOID32`, `HANDLE32`, `SIZE_T32`, `BOOL32`, `ClientId32`, `UnicodeString32`, `IoStatusBlock32`, `LargeInteger32`.
  - 32-bit PEB (`ProcessEnvironmentBlock32`) and TEB (`ThreadEnvironmentBlock32`) within 4GB virtual address space.
  - `RtlGetCurrentTeb32`, `RtlGetCurrentPeb32`, `RtlSetCurrentTeb32`, `RtlSetCurrentPeb32`.
  - **Heaven's Gate Mode Transition Engine (`HeavensGate`)**: Segment selector transitions (`0x23` compatibility mode <-> `0x33` long mode) with full x86 register state preservation (`Wow64Context32`).
  - **File System Redirection (`Wow64FsRedirection`)**: Transparent `\Windows\System32` -> `\Windows\SysWOW64` redirection with exemption paths (`drivers\etc`, `spool`, `catroot`) and thread-local disable/revert controls (`Wow64DisableFsRedirection`, `Wow64RevertFsRedirection`).
  - **Registry Redirection (`Wow64FsRedirection::translateRegistryKey`)**: Transparent `\Registry\Machine\Software` -> `\Registry\Machine\Software\WOW6432Node` virtualization.
  - **System Call Thunking Engine (`Wow64ThunkDispatcher`)**: 32-to-64 bit pointer widening, 32-bit address space constraint, and system call marshaling:
    - `thunkNtAllocateVirtualMemory` & `thunkNtFreeVirtualMemory`
    - `thunkNtWriteFile` & `thunkNtReadFile` (with `IoStatusBlock32` marshaling)
    - `thunkNtClose` & `thunkNtWaitForSingleObject`
- [x] **Test Suites 31 & 32 (`test/test_runner.cpp`)**:
  - Suite 31: `Test_Wow64_PebTebAndHeavensGate` (PEB32/TEB32 layout, 32-bit PE parsing, Heaven's Gate transitions).
  - Suite 32: `Test_Wow64_SyscallThunkingAndFsRedirection` (FS/Registry virtualization, 32-to-64 bit syscall thunking).

---

### Phase 8: Dynamic PE Import Binding & Base Relocations (100% Completed)
*Goal: Enable unmodified, third-party compiled 64-bit Windows PE binaries to execute against MicaNT through dynamic Import Address Table (IAT) binding and base relocations.*
- [x] **PE `.idata` Import Directory Parser (`include/micant/pe.hpp`)**:
  - `ImageImportDescriptor` directory table traversal with null-descriptor termination.
  - RVA-to-file-offset translation (`PeLoader::rvaToOffset`) across multi-section PE images.
  - Import Lookup Table (INT / `OriginalFirstThunk`) walking with dual resolution modes:
    - Named symbol resolution (`ImageImportByName`, hint + ASCII function name).
    - Ordinal symbol resolution (`IMAGE_ORDINAL_FLAG64` / `IMAGE_ORDINAL_FLAG32`).
- [x] **Import Address Table (IAT) Binding Engine (`PeLoader::bindImports`)**:
  - Direct userland IAT slot binding writing 64-bit function pointers directly into mapped image memory.
  - Seamless integration with clean-room `ldr::DynamicLoader` export registry.
  - Standard Win32 export table pre-registration (`win32::InitializeWin32SubsystemExports()` in `include/micant/kernel32.hpp`) providing 34+ core `kernel32.dll` and `ntdll.dll` functions.
  - Case-insensitive DLL module name normalization matching Windows OS behavior (`KERNEL32.DLL` == `kernel32.dll`).
- [x] **Base Relocation Engine (`PeLoader::applyRelocations`)**:
  - Traversal of PE `.reloc` section (`IMAGE_DIRECTORY_ENTRY_BASERELOC`).
  - Parsing multi-entry `ImageBaseRelocation` blocks with bounds-checked arithmetic.
  - `IMAGE_REL_BASED_DIR64` (64-bit pointer adjustment) and `IMAGE_REL_BASED_HIGHLOW` (32-bit pointer adjustment) delta application for images loaded away from preferred `ImageBase`.
- [x] **x86-64 GDT, TSS64 & Fast System Call Architecture (`include/micant/cpu.hpp`)**:
  - Global Descriptor Table (GDT) layout matching standard 64-bit Windows NT selectors: `0x10` (Kernel Code), `0x18` (Kernel Data), `0x23` (User 32-bit Compat Code), `0x2B` (User 64-bit Data), `0x33` (User 64-bit Code), `0x40` (16-byte Task State Segment).
  - 64-bit Task State Segment (`TaskStateSegment64`) with `RSP0` kernel interrupt stack, `IST1`..`IST7` privilege stacks, and I/O permission bitmap offset.
  - Model Specific Registers (MSRs) initialization: `MSR_STAR` (`0xC0000081`), `MSR_LSTAR` (`0xC0000082`), `MSR_SFMASK` (`0xC0000084`), and `MSR_EFER` (`0xC0000080` with `EFER_SCE`).
  - Ring 3 User Mode Transition Engine: `createRing3EntryFrame` constructing standard 64-bit `IretFrame64` for hardware `iretq` execution (`CS=0x33`, `SS=0x2B`, `RFLAGS=0x202`).
- [x] **Unmodified Third-Party Windows PE Execution**:
  - Authored `test/unmodified_sample.cpp`, compiled strictly against Microsoft `<windows.h>` without any MicaNT headers or shims into `bin/unmodified_sample.exe`.
  - Executed raw PE from disk in `Test_Execution_UnmodifiedThirdPartyBinary`: PE inspect, section mapping, dynamic IAT binding to clean-room `kernel32.dll` exports, CPU entry point transfer, and clean termination via `ExitProcess(0)`.
- [x] **Test Suites 33, 34, 35 (`test/test_runner.cpp`)**:
  - Suite 33: `Test_PeLoader_DynamicImportBindingAndUnmodifiedBinary` (dynamic imports, IAT binding, base relocations).
  - Suite 34: `Test_Cpu_GdtTssAndRing3HardwareTransitions` (GDT selectors, TSS64 layout, MSRs, `iretq` user mode frame).
  - Suite 35: `Test_Execution_UnmodifiedThirdPartyBinary` (unmodified 64-bit Windows PE execution via clean-room kernel32).
  - All 35 unit test suites passing with 100% success rate (35 Passed, 0 Failed).

---

### Phase 9: AArch64 (ARM64) Architecture Port (Next)
*Goal: Expand MicaNT hardware reach to 64-bit ARM architectures (Apple Silicon, Snapdragon X Elite, Raspberry Pi 5).*
- [ ] **ARM64 Exception Levels**: EL1 (Kernel / Executive) and EL0 (Ring 3 Userland).
- [ ] **ARM64 Translation Tables**: TTBR0 (Userland) / TTBR1 (Kernel Executive) page table walks.
- [ ] **ARM64 System Registers & Trap Dispatcher**: `ESR_EL1`, `FAR_EL1`, and `SVC` instruction trap dispatcher.
- [ ] **ARM64 Calling Convention Bridge**: AAPCS64 parameter register passing (X0-X7) for `KiSystemCall64`.
