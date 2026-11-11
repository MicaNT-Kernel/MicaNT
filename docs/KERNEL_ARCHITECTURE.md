# MicaNT Kernel Architecture

This is the authoritative specification for the active kernel architecture.
It supersedes older descriptions where they conflict. The design has one
modular kernel based on `ntoskrnl_core`; the archived standalone source under
[`old/monolithic-kernel/`](../old/monolithic-kernel/) is historical only.

## A. Kernel identity

MicaNT has one canonical kernel implementation: the modular subsystem sources
collected in `ntoskrnl_core`. Bare-metal and host-test builds must use those
same canonical subsystem implementations. Platform-specific code and test
adapters may differ; they must not implement a competing Executive.

`ntoskrnl_core` remains a CMake static library and `micant_kernel` remains a
hosted executable built from `kernel/init/kiinit.cpp`; the host harness is not
a freestanding kernel. An AMD64 freestanding bootstrap PE now validates a
versioned UEFI handoff and renders the shared Explorer desktop/taskbar drawing
code directly to GOP. It is an initial boot vertical slice, not yet the full
Executive or a user-mode `explorer.exe` process.

## B. Build targets

| Target | Current role | Output / status |
|---|---|---|
| `ntoskrnl_core` | Shared modular subsystem library | CMake static library |
| `micant_tests` | Host unit-test runner linked to `ntoskrnl_core` | `${CMAKE_BINARY_DIR}/host/micant_tests` |
| `micant_kernel` | Hosted initialization harness linked to `ntoskrnl_core` | `${CMAKE_BINARY_DIR}/host/micant_kernel`; not a kernel PE |
| `bootx64` | Bootloader object target in CMake | Does not by itself produce `BOOTX64.EFI` |
| `ntoskrnl_core_baremetal` | Freestanding AMD64 bootstrap and GOP Explorer desktop renderer | `build/baremetal/ntoskrnl.exe`; QEMU verified, Executive integration incomplete |
| `bootx64_efi` | UEFI PE bootloader | `build/esp/EFI/BOOT/BOOTX64.EFI` |

The bootstrap kernel is PE32+ AMD64, uses the Native subsystem, and has no
external PE imports. Its current entry preserves the firmware execution
context; it does not yet establish its own page tables, IDT, allocator,
scheduler, or user-mode environment. A host target passing does not validate
this artifact.

The build manifest required for staging is
`build/baremetal/kernel-build-manifest.json`. Its schema is versioned and must
identify the build, Git commit, toolchain, configuration, target, architecture,
both artifact paths, PE machine/subsystem/entry point, and each artifact's
SHA-256. Staging accepts only a manifest matching the current clean checkout
and the exact bootloader and modular kernel files that will be installed. No
previous-build or monolithic fallback is allowed.

## C. Boot sequence

The intended boot sequence is:

```text
UEFI
  ↓
BOOTX64.EFI
  ↓
MicaNT Loader Parameter Block
  ↓
Modular Kernel Entry
  ↓
KiSystemStartup
  ↓
Phase 0
  ↓
Phase 1
  ↓
Executive
  ↓
Ring 3
```

The current bootloader is `boot/bootx64.cpp`; it locates
`\EFI\MicaNT\ntoskrnl.exe` and passes a versioned, size-checked
`UefiKernelHandoffV1` POD contract. The legacy STL loader block remains local
to the bootloader and is not passed across `ExitBootServices`. The handoff
contains a pointer and byte range for the final UEFI memory map, GOP metadata,
and the ACPI root pointer. Its pointer fields are valid only while the
bootloader image and identity-mapped firmware memory remain available.

The current kernel entry validates the handoff, emits serial diagnostics,
renders the Explorer desktop and taskbar to GOP, and enters a halted idle loop.
It relies on the UEFI-provided AMD64 context and stack. Kernel-owned paging,
IDT/trap setup, allocator/runtime, interrupts, Executive initialization, and
Ring 3 launch remain to be implemented and verified.

## D. Kernel subsystem ownership

| Resource / responsibility | Owner |
|---|---|
| Thread scheduling, dispatch, timers, and interrupt dispatch policy | `Ke` |
| Process and thread lifecycle | `Ps`, using `Ke` scheduling primitives |
| Address spaces, virtual memory, physical pages, and mappings | `Mm` |
| Kernel objects, namespace, references, and handles | `Ob` |
| Drivers, devices, IRPs, and I/O dispatch | `Io` |
| Registry configuration | `Cm` |
| Tokens and access control | `Se` |
| File-system runtime interfaces | `FsRtl` |
| Cache and cached file data | `Cc` |
| CPU registers, interrupt-controller hardware, timers, MMIO, and context-switch mechanism | `Hal` / architecture platform code |
| Loader handover and early initialization sequencing | `kernel/init/` |

Platform code implements hardware mechanisms; it must not become a second
owner of Executive state. The AMD64 implementation belongs in the existing
HAL/platform boundary. Host-only facilities belong in a test adapter and must
not be linked into the bare-metal image.

## E. Initialization and dependency rules

Initialization order is a dependency graph, not the order of files in a build
list. Early bootstrap APIs may be separate from full subsystem initialization
where a dependency requires it.

### Phase 0 — early kernel

1. Establish serial logging and CPU context.
2. Validate and consume the loader memory map; establish the initial allocator.
3. Establish kernel mappings and paging.
4. Install IDT/trap handling and the minimum HAL.
5. Initialize only the Executive bootstrap needed to continue.
6. Initialize the bounded freestanding heap/runtime before using facilities
   that depend on it.

### Phase 1 — Executive

1. Complete `Mm`.
2. Initialize `Ob`, dispatcher/scheduler primitives, and `Ps` in dependency
   order.
3. Initialize `Io`, `Se`, and `Cm`.
4. Initialize `Cc` and `FsRtl`.
5. Load boot-start devices.
6. Create the first Ring 3 process only when its loader and isolation path are
   implemented.

Each phase must propagate initialization failures. A stub that reports success
does not satisfy initialization.

Dependencies flow from higher-level Executive subsystems to lower-level
kernel/platform contracts through declared interfaces. Hardware details do
not leak into portable subsystem logic. User-mode components are not part of
the kernel image. Shared source may be compiled with separate host and
freestanding toolchains, but the tested subsystem implementation must remain
the same.

## F. Legacy policy and migration matrix

The archived `old/monolithic-kernel/ntoskrnl.cpp` is excluded from active
source scans, compilation, linking, staging, installation, and symbol
resolution. No new implementation may be copied from it. Its history is
preserved by Git; migration work must be independently implemented in the
canonical owner.

| Legacy function area | Canonical destination | Current migration status |
|---|---|---|
| UEFI handover and loader data | `kernel/init/`, shared boot contract | Versioned POD handoff is validated in QEMU; memory-map consumption is pending |
| Serial debug | `Hal` / AMD64 platform | Bootstrap kernel initializes COM1 and emits verified serial milestones; HAL ownership is pending |
| IDT and exceptions | `Ke` + `Hal` / AMD64 | Headers/contracts exist; modular bare-metal initialization is not verified |
| APIC and timer | `Hal` + `Ke` | Headers/contracts exist; modular bare-metal initialization is not verified |
| Initial paging and PFN allocator | `Mm` | Modular host subsystem exists; freestanding boot path is not migrated |
| AMD64 syscall dispatch | `Ke` + native API boundary | Not migrated to a verified modular kernel entry |
| Ring 3 transition | `Ps` + platform code | Not migrated to a verified modular kernel path |
| PE image loading | Native loader + `Ps` | Not migrated to a verified modular kernel path |
| AHCI, USB/HID, and boot storage | `Io` + canonical drivers + `Hal` | Driver headers/implementations exist; boot-start integration is not verified |
| GOP/framebuffer output | Boot video + `Hal` / display driver | Bootstrap kernel renders the shared Explorer desktop/taskbar via the handoff framebuffer; display-driver ownership is pending |
| Bare-metal Object Manager | `Ob` | Host implementation exists; no freestanding boot integration |
| Bare-metal I/O Manager | `Io` | Host implementation exists; no freestanding boot integration |
| Runtime and idle loop | `Ex`, `Ke`, `kernel/init/` | QEMU bootstrap reaches a halted loop; no freestanding runtime, scheduler, or Executive idle thread yet |

This matrix is a migration inventory, not evidence that these functions
already work on hardware.

## G. Verification levels

These results are distinct and must be reported separately:

1. **Compilation** — a target compiles and links.
2. **Unit tests** — host tests exercise shared subsystem behavior.
3. **Kernel integration** — real kernel targets initialize together.
4. **QEMU/OVMF boot** — firmware loads and executes the modular bootstrap,
   which renders the initial Explorer desktop/taskbar and reaches its halt loop.
5. **Hardware boot** — the kernel runs on identified physical hardware.
6. **Windows compatibility** — externally visible Windows contracts are
   validated against approved metadata and public documentation.

The existing host build failure on the current ARM64 macOS environment and
the stale, relocated `build/` cache are not evidence of a successful host
baseline. The QEMU smoke test builds into an isolated CMake cache, captures
serial output from the kernel itself, and verifies the shell marker remains
stable. This does not verify the full Executive or a user-mode Explorer
process.

Build and launch the visible QEMU session from the repository root with:

```bash
python3 qemu.py
```

The runner configures the cross build, produces and validates the kernel,
bootloader, and manifest, then starts QEMU `q35` with one AMD64 virtual CPU,
OVMF pflash, a read-only FAT ESP, and a snapshot. It uses a visible display by
default and leaves QEMU open after serial milestones pass. Use `--headless` for
the automated smoke test, `--ovmf-code <path>` to select firmware explicitly,
or `--cxx <path>` / `MICANT_CXX` to select LLVM-MinGW.
