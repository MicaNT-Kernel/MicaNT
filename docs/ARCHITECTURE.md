# MicaNT Kernel & Subsystem Architecture Specification

> This is a catalog of source areas and intended subsystem ownership, not a
> claim that every listed operation is complete or verified. For the
> authoritative active-kernel design, build targets, boot status, and migration policy, see
> [KERNEL_ARCHITECTURE.md](KERNEL_ARCHITECTURE.md).

## 1. Canonical Source Tree Hierarchy (`base/ntos` & `WDK` Layout)

MicaNT separates **Kernel-Mode (`km`)**, **User-Mode (`um`)**, **Mode-Neutral Shared Contracts (`shared`)**, and **Compiled Kernel Subsystems (`kernel/`)**. The table describes intended ownership and representative source locations; actual completeness is tracked in the integration status documents.

| Layer | Path | Responsibility |
| :--- | :--- | :--- |
| **UEFI Bootloader** | `boot/bootx64.cpp` | Bare-metal UEFI GOP framebuffer setup, memory map ingestion, Loader Parameter Block (`LPB`) construction, and Ring-0 handover. |
| **Kernel Init** | `kernel/init/kiinit.cpp` | Shared initialization and bootstrap wiring; host and bare-metal entry paths have distinct roles. |
| **Microkernel (`Ke`)** | `kernel/ke/`, `include/micant/km/core/` | IRQL state machine, 32-queue scheduler, DPC/APC queues, Wait blocks, Timers, Trap/Exception frames (`trap.hpp`), Kernel Debugger (`kd.hpp`), and SSDT (`dispatcher.cpp`). |
| **Executive (`Ex`, `Ob`, `Mm`, `Io`, `Cc`, `Ps`, `Se`, `Cm`, `Lpc`, `PnP`, `Po`)** | `kernel/{ex,ob,mm,io,cc,ps,se,cm,lpc,pnp,po}/`, `include/micant/km/executive/` | Pool allocator (`NonPagedPool`/`PagedPool`), Object Manager namespace & handle tables, VAD/PFN Memory Manager, I/O Manager & Filter Manager (`fltmgr.hpp`), 256 KB VACB Cache Manager (`cc.hpp`), Process/Thread/Job Manager, Security Reference Monitor, Registry Hive Configuration Manager, ALPC, KMDF (`wdf.hpp`), PnP, Power, and Crash Dump/BugCheck engine (`bugcheck.hpp`). |
| **HAL (`Hal`)** | `kernel/hal/`, `include/micant/km/hal/` | KPCR/KPRCB (`GS:[0]`), Local APIC / IOAPIC / HPET, ACPI 6.5 AML interpreter (`acpi.hpp`), PCIe Root Complex (`pci.hpp`), IOMMU (`iommu.hpp`), UEFI Runtime Services, and ARM64 VMSA (`arm64.hpp`). |
| **Kernel Drivers (`WDK`)** | `include/micant/km/drivers/{storage,fs,net,usb,input,video,audio,bus,compute,security}/` | Categorized into 10 WDK device classes: Storage (`nvme.hpp`, `storage.hpp`), File Systems (`ntfs.hpp`, `fat32.hpp`, `emeraldfs.hpp`, `refs.hpp`, `npfs.hpp`), Networking (`ndis.hpp`, `netadaptercx.hpp`, `tcpip.hpp`, `wdiwifi.hpp`), USB (`xhci.hpp`, `usb.hpp`, `usb4.hpp`), Input (`hidclass.hpp`, `bthport.hpp`), Video (`wddm.hpp`, `dxgkrnl.hpp`), Audio (`hdaudio.hpp`), Bus (`cxl.hpp`, `vmbus.hpp`), Compute (`npu.hpp`, `amx.hpp`), and Security (`pluton.hpp`, `vbs_hvci.hpp`). |
| **Windowing (`Win32k`)** | `include/micant/km/win32k/win32k.hpp` | Shadow SSDT (`KeServiceDescriptorTableShadow`, index `0x1000+`), Window Station, Desktop, GDI handle table, and Raw Input thread queues. |
| **User-Mode Native (`NTDLL`)** | `include/micant/um/native/` | `ntdll.dll` syscall stubs, PE32+ image loader (`pe.hpp`), Janus Loader (`janusldr.hpp`), and WOW64 (`wow64.hpp`). |
| **User-Mode Subsystems** | `include/micant/um/subsystems/{winbase,winuser,graphics,media,net,com,security,mgmt}/` | Organized into 8 Windows user-mode domains (`kernel32`, `user32`, `gdi32`, `d2d1`, `prism3d12`, `wasapi`, `ws2_32`, `ole32`, `crypt32`, `dbgeng`, etc.). |

---

## 2. Core Executive Subsystems

### A. Object Manager (`ob/`) & Executive Handle Architecture
- Unified hierarchical namespace (`\`, `\Device`, `\DosDevices`, `\BaseNamedObjects`, `\Registry`, `\RPC Control`, `\ObjectTypes`).
- Process-private and kernel handle tables with 4-byte aligned handle indices (`0x4`, `0x8`, `0xC`...) and reference-counted `OBJECT_HEADER` lifecycles.

### B. Memory Manager (`mm/`) & Virtual Block Cache Manager (`cc/`)
- **Memory Manager (`mm/`)**: 4-level/5-level paging (`PML4`), Virtual Address Descriptor (`VAD`) balanced trees, Page Frame Number (`PFN`) database, and Section Objects (`NtCreateSection` / `NtMapViewOfSection`).
- **Executive Cache Manager (`cc/`)**: Virtual block cache mapping 256 KB Virtual Address Control Blocks (`VACB`s) in System Cache VA space (`0xFFFFF88000000000ULL`), supporting Fast Copy I/O (`CcCopyRead`/`CcCopyWrite`), Zero-Copy MDL DMA (`CcMdlRead`/`CcPrepareMdlWrite`), BCB metadata pinning (`CcPinRead`/`CcSetDirtyPinnedData` with WAL `LSN` ordering), sequential read-ahead, and background Lazy Writer (`CcLazyWriteScan`).

### C. I/O Manager (`io/`), Filter Manager (`fltmgr.hpp`) & WDF (`wdf.hpp`)
- Layered `DEVICE_OBJECT` stacks and asynchronous `IRP` packet routing (`Buffered`, `Direct` MDL, and `Neither` I/O modes) with FastIo dispatch tables and I/O Completion Ports (`IOCP`).
- Altitude-ordered File System Minifilter frame (`FltRegisterFilter`, `FLT_CALLBACK_DATA` pre/post-operation pipeline, and 6 context types).
- Kernel-Mode Driver Framework (`KMDF` `WDFDRIVER`, `WDFDEVICE`, `WDFQUEUE`, `WDFREQUEST`, `WDFDMAENABLER`).

---

## 3. Verification

Host unit tests, freestanding build checks, and QEMU/OVMF integration tests cover different behaviors. Passing one level does not establish another. Run the tests documented in the repository and consult [KERNEL_ARCHITECTURE.md](KERNEL_ARCHITECTURE.md) and [KERNEL_INTEGRATION_STATUS.md](KERNEL_INTEGRATION_STATUS.md) for verified milestones and remaining work.

---

## 4. Bare-Metal Verification

The currently documented bare-metal verification uses QEMU/OVMF; do not infer physical-hardware support from source declarations or host tests. The QEMU boot path and its limits are described in [KERNEL_ARCHITECTURE.md](KERNEL_ARCHITECTURE.md).
