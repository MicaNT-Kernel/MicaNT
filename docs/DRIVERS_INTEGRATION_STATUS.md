# Imported Driver Integration Status

Updated: 2026-11-12

## Scope and verified results

The requested inventory is the 26 directories listed below. Each has a CMake
file, test source, INF, README, and one or two imported implementation headers.
None of the 26 imported directories contains a driver implementation `.cpp`
backend or a freestanding `DriverEntry` target; their CMake targets are hosted
test executables. Their `.hpp` files contain substantial modeled logic but do
not by themselves form loadable driver images.

- `HOST_BUILD_OK`: **26/26** compile and test on native macOS ARM64.
- `AMD64_HOSTED_COMPILE_OK`: **26/26** compile as Windows x64 test executables
  with MinGW-w64. These executables were not run; no x64 runtime/Wine is
  installed.
- `FREESTANDING_BUILD_OK`: **0/26**. The imported headers use hosted STL and do
  not define freestanding driver entry/backend targets.
- `BAREMETAL_INTEGRATED`: **0/26**. Existing canonical modules are identified
  below, but the imports are not connected as independent hardware drivers.
- `QEMU_VERIFIED`: **0/26**. No imported driver hardware behavior was verified
  in QEMU.

Root CMake configuration succeeded. `micant_drivers_check` built the 26
independent hosted test targets and ran all CTest cases successfully (26/26).
This aggregate passed again after the canonical NDIS/TCPIP boundary changes.
The same 26 targets compiled individually for Windows x64 using
`cmake/toolchains/hosted-windows-x64-mingw.cmake`. This MinGW toolchain is only
for hosted test executables; it is not used by the canonical kernel pipeline.

## Why the `.sys` files were missing

The normal `qemu.py` build requested only `ntoskrnl_core_baremetal`,
`bootx64_efi`, and `virtio_gpu_boot`; the four OEM `.sys` targets existed in
CMake but were never dependencies of that command. The 26 imported driver
targets intentionally produce hosted tests and have no `DriverEntry` images.
The QEMU build now requests the four real OEM targets. Their shared
freestanding byte-memory routines are built from the Executive's existing
runtime implementation, split into `kernel/ex/freestanding_memory.cpp`, so
the images no longer require CRT imports.

Verified AMD64 PE native images were generated at `build/storahci.sys`,
`build/usbxhci.sys`, `build/i210.sys`, and `build/virtio_net.sys`. Each has an
AMD64 machine type, Native subsystem, `DriverEntry` entry point, and no PE
imports. This proves freestanding compilation/linking only. The boot script
does not stage these images into its temporary ESP yet, and none is claimed as
QEMU verified. xHCI and VirtIO network sources currently fail closed when the
required DMA/interrupt or NDIS path is unavailable; their existence as `.sys`
does not mean those devices operate.

## Shared include boundary and kernel build

The driver test includes now use `micant/drivers/<driver>.hpp`, and imported
headers spell dependencies with repository paths under `micant/shared/`,
`micant/km/`, and `micant/um/`. The unneeded `kernel32.hpp` include was removed
from `um/servers/scm.hpp`; SCM now includes the filesystem interfaces it uses
directly. This lets native ARM64 hosted tests build without pulling NTDLL's
AMD64 `divq` into the hosted process. The NTDLL assembly was left unchanged.

This is not yet full core/adapter separation: imported headers still contain
hosted SCM, JanusLDR, and version-registration paths. Those paths must be split
into optional adapters before an imported header can qualify as a kernel core.
The canonical NDIS header's hosted registration paths and RTTI casts were split
from the bare-metal call path. AFD now calls the canonical network stack via
`tcpip_kernel_api.hpp`, a primitive ABI bridge, and the boot adapter ABI binds
directly to `NetworkStack` with explicit callback/worker cleanup. This removes
the former JanusLDR exception dependency and RTTI casts from that path.

A freestanding AMD64 build was configured with the repository's bundled
LLVM-MinGW toolchain in `/tmp/micant-kernel-amd64`. UEFI and VirtIO-GPU targets
compiled and linked, as did all four standalone OEM `.sys` targets. `afd_wsk.cpp`
and `kiinit.cpp` also compile freestanding after the ABI bridge/binding changes.
The current `ntoskrnl_core_baremetal` still fails at link. Remaining unresolved
libc++ dependencies originate from kernel TCP/IP/NDIS containers and AFD wide
string use, including `std::__1::__next_prime`, `ceilf`, `wcslen`, string growth,
and `std::nothrow`. The core still needs kernel-owned bounded containers and
string handling; no CRT shims were added. An undeclared `third`/`fourth` in the
`NtDeviceIoControlFile` dispatch case was corrected to `args.third`/`args.fourth`
to reach compilation. QEMU was not launched because the actual kernel target did
not link. The
existing `build/qemu-cmake-mingw` cache was already configured as Darwin and is
not used as evidence; no MinGW flags were added to the bare-metal pipeline.

## Per-driver status

`HOST_BUILD_OK` means the imported hosted test passed on ARM64. `AMD64_HOSTED`
means the Windows x64 test target compiled but was not executed. Remaining
columns explicitly separate device integration from compilation.

| Driver | Canonical owner to reuse | HOST_BUILD_OK | AMD64_HOSTED_COMPILE_OK | FREESTANDING_BUILD_OK | BAREMETAL_INTEGRATED | QEMU_VERIFIED | Hardware/backend work still missing |
|---|---|---:|---:|---:|---:|---:|---|
| acpi | `km/hal/acpi.hpp`, `km/drivers/bus/acpi.hpp` | Yes | Yes | No | No | No | ACPI bus/PnP device lifecycle and hardware resource ownership. |
| amx | `km/drivers/compute/amx.hpp` | Yes | Yes | No | No | No | AMD64 CPUID/XSTATE validation and safe per-thread tile-state handling. |
| bluetooth | `km/drivers/input/bthport.hpp` | Yes | Yes | No | No | No | USB/PCI transport binding, HCI state machine, interrupts and DMA buffers. |
| bypassio | `km/drivers/storage/bypassio.hpp` | Yes | Yes | No | No | No | Connect bypass policy to canonical Io/storage dispatch and real queue ownership. |
| cet | `km/drivers/security/cet.hpp` | Yes | Yes | No | No | No | Wire hardware CET state and enforcement into canonical thread/process lifecycle. |
| cxl | `km/drivers/bus/cxl.hpp` | Yes | Yes | No | No | No | ACPI/PCI discovery, decoder programming, memory hot-add and rollback. |
| dsa | `km/drivers/compute/dsa.hpp` | Yes | Yes | No | No | No | PCI discovery, MMIO/DMA queue setup, interrupt and completion handling. |
| graphics | `km/drivers/video/dxgkrnl.hpp`, `wddm.hpp` | Yes | Yes | No | No | No | Bind miniport callbacks to a real display backend; reuse existing VirtIO-GPU path. |
| hdaudio | `km/drivers/audio/hdaudio.hpp` | Yes | Yes | No | No | No | PCI BAR/interrupt/DMA/DPC integration and PnP codec lifecycle. |
| hfi | `km/drivers/compute/hfi.hpp` | Yes | Yes | No | No | No | CPU capability discovery and per-core hardware feedback acquisition. |
| iommu | `km/hal/iommu.hpp` | Yes | Yes | No | No | No | ACPI DMAR discovery, translation-table ownership, DMA remapping and fault handling. |
| ndis | `km/drivers/net/ndis.hpp` | Yes | Yes | No | No | No | Remove hosted loader/SCM and RTTI from the canonical kernel path; bind real miniports. |
| npu | `km/drivers/compute/npu.hpp` | Yes | Yes | No | No | No | PCI discovery, MMIO/DMA queues, interrupt completion and device memory ownership. |
| nvme | `km/drivers/storage/nvme.hpp` | Yes | Yes | No | No | No | Real PCI BAR/admin/I/O queues, DMA cleanup and Io block-device registration. |
| pci | `km/hal/pci.hpp` | Yes | Yes | No | No | No | Imported PCI model is duplicate; connect tests and consumers to HAL enumeration. |
| pluton | `km/drivers/security/pluton.hpp` | Yes | Yes | No | No | No | Secure hardware transport, firmware protocol, attestation and failure handling. |
| pmem | `km/drivers/storage/pmem.hpp` | Yes | Yes | No | No | No | Firmware memory-region discovery and persistent block-device durability semantics. |
| qat | `km/drivers/compute/qat.hpp` | Yes | Yes | No | No | No | PCI discovery, DMA rings, interrupt handling and hardware completion path. |
| rdma | `km/drivers/net/rdma.hpp` | Yes | Yes | No | No | No | Bind to real PCI function, DMA memory registration and NDIS/TCPIP packet path. |
| sriov | `km/drivers/bus/sriov.hpp` | Yes | Yes | No | No | No | PCI capability discovery, VF enable/disable, resource assignment and teardown. |
| tee | `km/drivers/security/tee.hpp` | Yes | Yes | No | No | No | Hardware/firmware transport, isolation boundary and trust/attestation lifecycle. |
| ucsi | `km/drivers/usb/ucsi.hpp` | Yes | Yes | No | No | No | ACPI/USB transport, UCSI command/event handling and PnP power lifecycle. |
| usb | `km/drivers/usb/usb.hpp`, `kernel/io/usb_host.cpp`, OEM xHCI transport | Yes | Yes | No | No | No | Current freestanding path handles one USB2 boot HID device and BOT/SCSI mass-storage probing/reads into an Io block handle. Filesystem/namespace handoff and QEMU device proof remain; hubs, composite devices, SuperSpeed and general HID are unsupported. |
| usb4 | `km/drivers/usb/usb4.hpp` | Yes | Yes | No | No | No | PCI/ACPI router discovery, link state machine, DMA and hot-plug lifecycle. |
| wdf | `km/executive/wdf.hpp` | Yes | Yes | No | No | No | Reuse canonical IRP/device lifecycle; imported framework is not a kernel backend. |
| wifi | `km/drivers/net/wdiwifi.hpp` | Yes | Yes | No | No | No | Bind WDI to real miniport, NDIS/TCPIP datapath, radio firmware and power handling. |

## Other files found under `drivers/`

The recursive inventory also found content outside the requested 26:
`drivers/oem/{ahci,virtio_net,xhci}`, `drivers/virtio_gpu`, and
`drivers/windows_hmb`. These remain outside `micant_drivers_check`. Their
existing worktree state was preserved; they require separate provenance and
ownership review before integration.

## OEM native driver images

| Image | AMD64 freestanding build | Current implementation evidence | BAREMETAL_INTEGRATED | QEMU_VERIFIED |
|---|---|---|---|---|
| `storahci.sys` | Yes | AHCI PCI claim, BAR/DMA setup and block-device publication are implemented. Its private read-only MBR/GPT/FAT32 file parser duplicates canonical filesystem ownership and is not considered integrated. | No; the Io block object is not yet adapted to canonical `storage::IBlockDevice`/FAT32 dispatch, and no running-kernel exercise exists. | No |
| `usbxhci.sys` | Yes | PCI discovery/claim, MMIO/DMA setup, USB host service registration, polled xHCI events, boot HID and bulk BOT transport are implemented. Interrupt delivery is not enabled; supported device scope is limited as described above. | No; linked device paths have not been run in the kernel. | No |
| `i210.sys` | Yes | Intel I210 controller setup and the boot network-adapter registration ABI are present; hardware datapath still needs end-to-end binding/validation. | No; service bridge and packet path are not yet verified. | No |
| `virtio_net.sys` | Yes | VirtIO PCI capability discovery is present; entry returns `STATUS_NOT_SUPPORTED` until DMA, interrupt and canonical NDIS/TCPIP binding are available. | No | No |

The `qemu.py` build now compiles and links these OEM targets so their `.sys`
artifacts are produced during the canonical AMD64 cross-build. The temporary
QEMU ESP still stages only `virtio_gpu.sys`; unsupported or hardware-dependent
OEM images are not loaded automatically. This keeps build success separate
from device operation and QEMU evidence.

Storage review confirmed that the AHCI source publishes a bounded Io block
object, but the `.sys` still owns a duplicate FAT32/partition parser. M14's
canonical `storage::IBlockDevice` and FAT32 filesystem are not yet connected to
the freestanding block object, so this is not a filesystem integration claim.

## Next work

1. Split SCM, JanusLDR and version registration into optional hosted adapters for
   the imported models; do not carry these dependencies into kernel core.
2. Fix canonical NDIS hosted/RTTI dependencies without weakening freestanding
   ABI flags, then link `ntoskrnl_core_baremetal` with the existing LLVM-MinGW
   path.
3. Connect AHCI and USB block handles to canonical storage/FAT32 dispatch;
   validate the i210 NDIS adapter with actual packets. Extend xHCI only with
   tested interrupt, hub and SuperSpeed ownership paths.
4. Remove hosted STL containers/synchronization from the kernel TCP/IP path so
   the bare-metal kernel links without libc++ imports.
5. Run the existing runtime-selftest and hardware-capable QEMU scenarios after
   the canonical kernel links. Do not promote any status based on hosted tests.
