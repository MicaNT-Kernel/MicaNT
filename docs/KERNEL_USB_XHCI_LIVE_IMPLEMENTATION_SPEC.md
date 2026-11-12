# MicaNT — M8 USB Host, HID, Mass Storage & Live USB Implementation Specification

**Status:** Proposed / not yet implemented end-to-end  
**Architecture:** AMD64 (x86_64) only; C++23; UEFI; one canonical modular NT kernel  
**Target:** `ntoskrnl_core_baremetal`; real Ring 0 hardware paths, not hosted simulations  
**Repository:** `luca-saggese/MicaNT`  
**Scope:** xHCI, USBD/URB, USB HID keyboard/mouse, USB Mass Storage BOT, block I/O, GPT/FAT32, first physical-hardware diagnostics  
**Related:** `docs/KERNEL_ARCHITECTURE.md`, `docs/KERNEL_MILESTONES.md`, `docs/KERNEL_FILESYSTEM_SPEC.md`, `docs/KERNEL_GPU_DRIVER_IMPLEMENTATION_SPEC.md`

## 1. Objective and acceptance boundary

M8 delivers USB peripherals accessible by the **MicaNT kernel itself after `ExitBootServices`**, initially in QEMU and then on selected real UEFI AMD64 PCs. There are two independently demonstrable milestones:

- **U1 — Hardware xHCI + HID:** enumerate an actual xHCI controller, keyboard and mouse; receive input through real xHCI event/transfer rings; deliver it through the canonical HID and input-class pipeline. The test must not succeed using UEFI input, host-simulated USB, or PS/2 fallback.
- **U2 — USB Mass Storage:** enumerate a BOT-compatible USB drive, read actual SCSI sectors through xHCI bulk endpoints, expose a read-only block device, parse GPT and FAT32, and read a file **after** `ExitBootServices`. No UEFI Block I/O calls may service the runtime read.

A **Live USB boot** is a dependent but different deliverable. UEFI firmware can load `BOOTX64.EFI` and preload kernel images from USB before `ExitBootServices` even while U1/U2 are unfinished. A full live session additionally requires GOP fallback, usable user processes, a shell and persistent or RAM-backed root filesystem. Do not equate the bootloader loading from USB with a kernel USB mass-storage driver.

The initial USB implementation must be read-only on removable storage, bounded and diagnostically observable. No installer, disk formatting, automount of internal disks or NVRAM boot-entry writes are in scope.

## 2. Baseline inventory and reuse policy

The following is a **source audit**, not a claim of runtime readiness:

| Existing source | Useful content | Current limitation / action |
|---|---|---|
| `include/micant/km/drivers/usb/xhci_baremetal.hpp` | Register access, BIOS ownership handoff, DCBAA, command/event/transfer rings, slot/address/endpoint flow and HID polling | Hardcoded Intel/BDF/BAR/TSC and narrow single-device assumptions; extract reusable hardware code into the canonical driver |
| `include/micant/km/drivers/usb/xhci.hpp` | NT-oriented URB types, driver contracts and xHCI device/pipe model | Hosted state machine and virtual controller; do not link simulated completions into production |
| `include/micant/km/drivers/usb/usb.hpp` | USB descriptors, class definitions, URB model, simulated BOT/SCSI and HID devices | Hosted containers, locking, simulated storage; retain as test fixtures only, adapt portable contracts |
| `include/micant/km/drivers/input/hidusb.hpp` | HID USB URB translation | Currently depends on hosted xHCI submission; bind it to a single production USBD interface |
| `include/micant/km/drivers/input/kbdhid.hpp`, `mouhid.hpp` | Report-to-key/mouse input conversion | Connect to actual HID descriptors/reports and class-service callbacks |
| `include/micant/km/hal/pci.hpp` | PCI representation and configuration helpers | Identify the **actual** hardware PCI backend and separate hosted PCI emulation |
| `boot/bootx64.cpp` | UEFI Simple File System, Block I/O, GOP and image loading | Boot-phase only; never use boot services after `ExitBootServices` |
| `CMakeLists.txt` | Modular bare-metal target and test assets | USB hardware/core/HID/storage are not in the audited bare-metal source list |

**One-owner rule:** Io/PnP own driver/device lifecycle; HAL owns PCI/MMIO/IRQ/DMA interfaces; the canonical USB stack owns bus/URB semantics; HID class owns input; Storage owns block devices. Do not add a third USB subsystem, a private PCI enumerator, an alternative allocator or a direct-path HID input stack.

### 2.1 Existing hardware-code defects to resolve first

The old `xhci_baremetal.hpp` includes assumptions unsuitable for general PC hardware:

- An initial Intel `8086:A12F` and `00:14.0` path, with a literal `0xA2D00000` BAR fallback. Even after scanning, some writes still address fixed `00:14.0`. All accesses must use the selected enumerated PCI function.
- The PCI search examines a narrow location and insufficiently validates PCI programming interface. Require class `0x0C`, subclass `0x03`, programming interface `0x30` for xHCI, and scan relevant PCI buses/functions exposed by HAL.
- `delayMs()` assumes a 3.8 GHz TSC. Replace with the calibrated HAL monotonic timer, bounded absolute deadlines and poll-with-yield only where legal.
- Physical addresses are inferred from a high-half translation and statically allocated DMA arrays. Use explicit device-safe DMA allocations, known physical/I/O addresses, physical contiguity and appropriate cache/coherency barriers.
- It selects one connected root port and configures a keyboard-style endpoint rather than enumerating actual device/configuration/interface/endpoint descriptors.
- Polling with optimistic status messages is not a substitute for checking xHCI completion codes and actual transferred bytes.

Do not perform controller reset or arbitrary MMIO writes when the BAR, device identity, resource ownership or safe mapping has not been validated.

## 3. Architecture and ownership

```text
UEFI stage: USB firmware drivers -> FAT32 EFI System Partition -> BOOTX64.EFI
                    (all UEFI calls finish before ExitBootServices)
                                     |
                              UEFI handoff
                                     v
                            MicaNT AMD64 HAL
          PCI enumeration / BAR / MMIO / DMA / interrupt routing
                                     |
                           xHCI host controller
                    DCBAA / rings / root ports / slots
                                     |
                           USB core / USBD
          device descriptors / addresses / hubs / URB dispatch
                      /                              \
             HIDUSB / HIDClass                   USBSTOR BOT
               /          \                            |
           KbdHid        MouHid                     SCSI block
              \            /                           |
                 KbdClass / MouClass          Io Storage / GPT / FAT32
                         |                             |
                    System input               Kernel file access
```

The production path must have **one** URB submission entry point. Each URB has explicit owner, state, endpoint identity, length, completion status, actual transfer length, cancellation state and deadline. The host-simulated controller implements the same abstraction only under a test-specific build configuration.

### 3.1 Suggested internal driver contract (not public NT ABI)

Use implementation-owned, versioned interfaces that adapt to existing MicaNT types rather than a new WDK ABI:

```cpp
struct UsbControllerServices {
    uint32_t size;
    uint32_t version;
    // Allocator, physical DMA address, MMIO, IRQ, time, logging services.
};

struct UsbTransferResult {
    NtStatus status;
    uint32_t bytesTransferred;
    uint32_t completionCode;
};
```

**Do not freeze these illustrative structures as public exports.** The actual definitions must reuse canonical `Io`, `HAL`, `Mm` and USBD contracts and document the native ABI boundary if exposed to drivers.

## 4. U1 — Hardware xHCI controller

### U1.1 PCI enumeration and ownership

1. Enumerate PCI topology through the HAL hardware backend; match xHCI class/subclass/prog-if. Record real segment/bus/device/function and vendor/device IDs.
2. Claim/initialize the device through the canonical PnP/Io ownership path; validate and map the full BAR aperture as device memory, not ordinary cached RAM.
3. Enable memory-space and bus-master bits **only after** resource checks and DMA readiness; preserve PCI state for teardown.
4. Validate MMIO capability/operational/runtime/doorbell offsets, supported address width, context size, slot/port counts and extended capability traversal. Reject overflow, malformed pointers and unsupported configurations.
5. Perform firmware-to-OS ownership handoff with a bounded deadline; do not reset a controller still owned by firmware without a defined and logged recovery policy.

**Failure criterion:** an unrecognized or unsupported xHCI controller is safely skipped; no fixed BAR fallback or blind write to BDF `00:14.0` is allowed.

### U1.2 DMA/rings and interrupt transport

- Allocate aligned, device-addressable, physical DMA structures for DCBAA, scratchpads, command ring, event-ring segments/ERST, input/output contexts and transfer rings.
- Support both 32-byte and 64-byte contexts, ring cycle-state changes, link TRBs, full/empty handling and proper producer/consumer ownership.
- Track and verify actual command completion events with completion code, slot ID, endpoint ID and required transfer lengths.
- Program MSI/MSI-X when HAL supports safe routing; provide a bounded polling diagnostic fallback without making polling the long-term design.
- ISR acknowledges the controller and performs minimal work. Deferred handling completes URBs, wakes waiters and re-arms transfers using Ke synchronization, not host threads.
- Handle timeout, device removal, stalled endpoints, transfer-ring-full, short transfers, event-ring overrun, command failures and controller reset without leaking DMA pages.
- Validate memory barriers, DMA address-width limits and IOMMU mappings when an IOMMU is active. No user pointer may be passed directly as uncontrolled DMA memory.

### U1.3 USB bus/device enumeration

- Power and debounce multiple root ports; identify speed and link state without assuming the first connected device is a keyboard.
- Reset ports, enable slots, address devices, read device/configuration/interface/endpoint descriptors, select configuration, create endpoint contexts and associate pipes with slots.
- Validate descriptor sizes, total lengths, endpoint directions/types/intervals and class codes before constructing devices.
- Handle at least keyboard plus mouse concurrently on separate ports; support composite HID interfaces. Hub support may be staged but must explicitly reject unsupported topologies.
- Disconnect must cancel inflight URBs, retire endpoint contexts and references and stop injecting input events.

### U1.4 HID and input class integration

- Use `hidusb.hpp` as the transport adapter, `hidclass`/`hidparse` as the report owner, and `kbdhid`/`mouhid` to produce input-class packets. Preserve the canonical KbdClass/MouClass interfaces.
- Implement HID Boot Protocol keyboard/mouse as the first hardware subset; then parse report descriptors for non-boot/composite devices.
- Track key up/down, modifiers, rollover, key repeat policy and disconnect; mouse buttons, movement and wheel must be preserved.
- Input event source must be attributable to a completed xHCI transfer. Do not silently substitute firmware console input or PS/2 data in USB tests.

**U1 completion:** QEMU configured with an actual emulated xHCI controller and USB keyboard/mouse delivers verified input after `ExitBootServices`, including attach/detach, multiple devices and safe failure paths. A second, physical UEFI AMD64 PC test is required before claiming broad hardware support.

## 5. U2 — USB Mass Storage (BOT) and filesystem access

### U2.1 Class driver and transport

- Enumerate a device interface with USB Mass Storage class `0x08`, subclass SCSI transparent `0x06`, protocol BOT `0x50`.
- Discover actual Bulk IN/OUT endpoint addresses and maximum packet sizes from descriptors; do not hardcode `0x81`/`0x02` or assume one slot.
- Implement the Bulk-Only Transport sequence: valid 31-byte CBW -> data stage -> valid 13-byte CSW. Check signatures, tags, residue, direction, transfer lengths and status.
- Implement SCSI `INQUIRY`, `TEST UNIT READY`, `REQUEST SENSE`, `READ CAPACITY(10)` (and `READ CAPACITY(16)` for larger media when supported), `READ(10)` with bounded retries. Defer `WRITE(10)` and persistent write capability.
- Implement stall clear and BOT reset recovery; distinguish USB transport failure, SCSI command failure and device removal.
- Honor logical block size and capacity returned by the device. Prevent integer overflow, out-of-range LBA and unaligned buffer access.

### U2.2 Canonical block device and read-only volumes

- Register a read-only `storage::IBlockDevice` or existing canonical storage device representation with Io. Never create a separate private disk manager.
- Support partition selection via validated GPT (primary/backup header CRC and entry CRC) and optionally MBR for simple removable media.
- Mount FAT32 read-only through the existing filesystem implementation and read bytes from the USB-backed block device; reject malformed BPB/FAT chains, loops and out-of-bounds cluster chains.
- The FS path must use real kernel block reads after `ExitBootServices`. For an image/file read, compare content to a known checksum or exact fixture bytes.
- Avoid publishing a device object until the transport can reliably read its capacity and at least one sector. Disconnect must invalidate future I/O and complete pending IRPs with an appropriate failure.

**U2 completion:** a file is read from a USB mass-storage device attached to emulated xHCI in QEMU with known bytes, and the same path is observed on selected real hardware. No `EFI_BLOCK_IO_PROTOCOL`, staged copy, simulated device buffer or host directory mapping may satisfy the runtime acceptance test.

## 6. UEFI Live USB and GOP dependency

The bootloader currently loads `BOOTX64.EFI`, `ntoskrnl.exe` and driver images using UEFI filesystem protocols. Preserve that stage and the versioned UEFI handoff.

- **L0:** create a removable-media layout with `EFI/BOOT/BOOTX64.EFI` and `EFI/MicaNT/ntoskrnl.exe`, required boot drivers and optional diagnostic configuration. Build artifacts must match the manifest; no stale image reuse.
- **L1:** maintain serial diagnostics and a GOP framebuffer fallback when VirtIO-GPU is absent. Do not assume any real PC offers VirtIO-GPU.
- **L2:** successfully enter the AMD64 kernel, establish its memory/interrupt hardware state and retain display diagnostics through the post-UEFI handoff.
- **L3:** bring up xHCI and HID input (U1) without UEFI input dependence.
- **L4:** optionally preload a small read-only RAM system image before exit; use this to enable a basic shell while U2 is in development.
- **L5:** mount the physical USB volume at runtime using U2; transition to a full live read-only session as Ps/loader/system runtime permit.

The first live test must not write internal disks, format media, alter firmware boot order or require any automatic installation. Secure Boot policy must be explicit; unsigned development images are not implicitly accepted by firmware.

## 7. Security and reliability invariants

1. USB, PCI and xHCI metadata are untrusted. Validate every descriptor, offset, length, index, alignment, integer calculation and DMA extent.
2. Device DMA buffers and user buffers have distinct ownership. An URB may be completed or cancelled exactly once, and must never reference freed memory.
3. Controller and device lifecycle are serialized with real Ke/Io primitives, never synthetic `std::thread` or hosted clocks.
4. Do not hold an interrupt-level lock while waiting, allocating memory or issuing a synchronous request.
5. On enumeration or transfer failure, do not return success with zero data unless that is the documented valid outcome.
6. No arbitrary PCI function configuration changes, hardcoded physical MMIO addresses or unsafe controller resets.
7. Removable storage is **read-only by default** until write ordering, flushing and crash-safety have independent acceptance tests.
8. Missing xHCI, a malformed device or a USB disconnect must not bring down the OS; diagnostics should identify the failing stage.
9. Keep untrusted USB data handling behind bounded copies, verified memory mappings and an explicit cleanup path.
10. Do not claim compliance with the full Microsoft USB or WDK driver surface from a limited boot implementation.

## 8. External references and acceleration without architecture duplication

Preferred investigation order: existing MicaNT hardware/USBD code -> the USB xHCI specification -> public Microsoft WDK USB/URB contracts -> relevant ReactOS USB stack code -> Linux xHCI/usb-storage implementations for device-specific corner cases.

- **xHCI specification:** register layout, TRB formats, link/cycle rules, transfer/event completion and device context handling.
- **USB specifications:** standard descriptors, HID 1.11 / HID usage tables, Mass Storage BOT, SCSI Primary/Block Commands.
- **ReactOS:** USB class/URB/Io/PnP architecture and NT behavior; preserve provenance and applicable licensing obligations if adapting source.
- **Linux:** `drivers/usb/host/xhci*`, `drivers/usb/storage/` as technical implementation references. The Linux USB code is not a drop-in NT driver; evaluate licenses before importing any code.

Do not copy a complete Linux USB stack into Ring 0. Reuse tested algorithms and structures where permitted, adapt to the **single** canonical HAL/Io/USB/Storage ownership structure, and maintain a focused mapping from reference source to implemented behavior.

## 9. Build and verification strategy

Development targets remain incremental. Build the affected xHCI/USB/HID/Storage translation units, then link `ntoskrnl_core_baremetal` only when the integration is coherent. Do not rebuild Explorer or unrelated applications during U1/U2 development.

### 9.1 QEMU validation fixtures

Run the **same produced AMD64 PE kernel** under OVMF with an xHCI controller and attached virtual USB devices. Extend the QEMU runner with a dedicated USB test mode; do not replace or disable existing VirtIO/PS2 regression tests. Define explicit serial markers for:

- PCI class-matched xHCI discovery and validated BAR.
- Controller ownership and ring initialization.
- Real command and transfer completion event.
- Keyboard and mouse report receipt through xHCI.
- USB device removal/cancellation.
- Mass Storage INQUIRY, capacity, READ(10) and BOT CSW validation.
- GPT/FAT32 mount and exact file-data verification after `ExitBootServices`.
- DMA/PFN resources and handle references restored after teardown.

Also exercise invalid descriptors, malformed CSW, short transfers, ring exhaustion, timeout, removal mid-transfer and allocation rollback in isolated tests. QEMU results are not hardware-compatibility proof.

### 9.2 Physical AMD64 bring-up

Use a disposable/test PC with UEFI x64 and a display path independent of VirtIO. Capture logs on serial if available or on GOP diagnostic screen. Probe one controller family first; record PCI ID, BIOS/UEFI revision, ACPI layout, port topology, HID devices and the removable-drive model.

For each hardware claim, require evidence of boot completion, controller enumeration, actual HID input and/or USB block read. Do not use broad labels such as “all Intel/AMD USB controllers supported” based on one tested device.

## 10. Milestones and Definition of Done

| Phase | Required deliverable | Acceptance evidence |
|---|---|---|
| U0 — ownership/portability | One real xHCI backend; remove hardcoded BDF/BAR/TSC and simulator dependencies | Code and static contract checks |
| U1a — hardware transport | PCI, MMIO, DMA rings, command/event completion | Real QEMU xHCI command and transfer evidence |
| U1b — USB HID | Keyboard/mouse report pipeline through KbdClass/MouClass | QEMU USB input and attach/detach checks |
| U2a — BOT/SCSI | Real Bulk IN/OUT and read-only SCSI operations | QEMU USB storage response/sector bytes |
| U2b — block/filesystem | Canonical Io block device, GPT/FAT32, read-only file access | Runtime USB file bytes from QEMU disk image |
| L0–L2 — live boot | Bootable USB layout, kernel handoff and GOP diagnostics | Same disk image booted under OVMF, then selected physical PC |
| L3–L5 — live interactive | HID input, read-only storage, basic shell | Live session on physical hardware, no firmware or internal-disk writes |

A phase is not complete if it only compiles, exercises simulated USB objects or runs while UEFI boot services are still active. Report separately: source implemented, PE linked, QEMU exercised, physical hardware exercised.

### 10.1 Exclusions from the first release

- USB4/Thunderbolt, isochronous streaming, USB audio/video, UASP, full WinUSB and arbitrary third-party USB driver support.
- High-speed or superspeed hubs beyond the topologies needed by the first HID/BOT fixtures.
- Storage write/format support, full PnP hotplug policy and advanced selective suspend/power management.
- Universal hardware support, complete WDK parity and installer functions.

## 11. Recommended next engineering batch

1. Read the current `ntoskrnl_core_baremetal` target and extract only reusable, non-hosted pieces from `xhci_baremetal.hpp`.
2. Refactor the PCI/BAR/physical DMA/timer assumptions into canonical HAL services; do not initialize a controller until these are correct.
3. Bind real xHCI completions to the existing USBD/URB contract and exercise one transfer under QEMU.
4. Extend USB device enumeration and connect `hidusb` → `kbdhid` / `mouhid` → class input.
5. After U1, reuse the BOT/SCSI modeling as a guide for a **real** USB Mass Storage class driver, then connect Storage/GPT/FAT32.
6. Keep the GOP fallback and Live USB image builder as an independent parallel workstream with no overlapping kernel ownership changes.

**Ultimate criterion:** On an AMD64 UEFI PC, a USB-booted MicaNT kernel must receive USB keyboard/mouse input and read an on-stick file through its own xHCI + USB Mass Storage stack after `ExitBootServices`, while preserving memory safety and remaining read-only by default.
