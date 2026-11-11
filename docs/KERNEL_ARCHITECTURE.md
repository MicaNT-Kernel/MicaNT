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

At this migration stage, `ntoskrnl_core` is a CMake static library and
`micant_kernel` is a hosted executable built from `kernel/init/kiinit.cpp`.
That executable is a host harness, not a freestanding kernel. A production
freestanding entry point and modular bare-metal PE are not yet available.
This distinction is intentional and prevents a host test or a legacy binary
from being reported as a successful kernel boot.

## B. Build targets

| Target | Current role | Output / status |
|---|---|---|
| `ntoskrnl_core` | Shared modular subsystem library | CMake static library |
| `micant_tests` | Host unit-test runner linked to `ntoskrnl_core` | `${CMAKE_BINARY_DIR}/host/micant_tests` |
| `micant_kernel` | Hosted initialization harness linked to `ntoskrnl_core` | `${CMAKE_BINARY_DIR}/host/micant_kernel`; not a kernel PE |
| `bootx64` | Bootloader object target in CMake | Does not by itself produce `BOOTX64.EFI` |
| Modular bare-metal kernel | Required future target | `build/baremetal/ntoskrnl.exe`; not yet implemented |

The bare-metal kernel must be PE32+ AMD64, use the Native subsystem, and link
only the modular kernel sources plus an explicitly supported freestanding
runtime. It must not depend on host CRT imports. A host target passing does
not validate this artifact.

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
`\EFI\MicaNT\ntoskrnl.exe` and builds the existing C++ loader block. That block
currently contains `std::vector` and `std::wstring`, so it is not a stable
cross-runtime handover ABI. Before modular bare-metal boot is claimed, replace
that boundary with a versioned, size-checked POD contract defining alignment,
pointer validity, ownership, and lifetime across `ExitBootServices`.

The kernel entry must validate the block, establish the required AMD64
execution context, and emit serial diagnostics before complex hardware work.
The PE entry, relocation behavior, mappings, stack, calling convention,
interrupt state, runtime initialization, and Ring 0 handover remain to be
implemented and verified together.

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
| UEFI handover and loader data | `kernel/init/`, shared boot contract | Bootloader has a handover path, but the STL-based block is not ABI-stable; not migrated |
| Serial debug | `Hal` / AMD64 platform | Bootloader and legacy code have serial routines; kernel-owned early serial path is not verified |
| IDT and exceptions | `Ke` + `Hal` / AMD64 | Headers/contracts exist; modular bare-metal initialization is not verified |
| APIC and timer | `Hal` + `Ke` | Headers/contracts exist; modular bare-metal initialization is not verified |
| Initial paging and PFN allocator | `Mm` | Modular host subsystem exists; freestanding boot path is not migrated |
| AMD64 syscall dispatch | `Ke` + native API boundary | Not migrated to a verified modular kernel entry |
| Ring 3 transition | `Ps` + platform code | Not migrated to a verified modular kernel path |
| PE image loading | Native loader + `Ps` | Not migrated to a verified modular kernel path |
| AHCI, USB/HID, and boot storage | `Io` + canonical drivers + `Hal` | Driver headers/implementations exist; boot-start integration is not verified |
| GOP/framebuffer output | Boot video + `Hal` / display driver | Bootloader GOP exists; modular kernel ownership/handover is not verified |
| Bare-metal Object Manager | `Ob` | Host implementation exists; no freestanding boot integration |
| Bare-metal I/O Manager | `Io` | Host implementation exists; no freestanding boot integration |
| Runtime and idle loop | `Ex`, `Ke`, `kernel/init/` | No supported modular freestanding runtime or QEMU-verified idle loop |

This matrix is a migration inventory, not evidence that these functions
already work on hardware.

## G. Verification levels

These results are distinct and must be reported separately:

1. **Compilation** — a target compiles and links.
2. **Unit tests** — host tests exercise shared subsystem behavior.
3. **Kernel integration** — real kernel targets initialize together.
4. **QEMU/OVMF boot** — firmware loads and executes the modular kernel.
5. **Hardware boot** — the kernel runs on identified physical hardware.
6. **Windows compatibility** — externally visible Windows contracts are
   validated against approved metadata and public documentation.

The existing host build failure on the current ARM64 macOS environment and
the stale, relocated `build/` cache are not evidence of a successful baseline.
No QEMU boot result is claimed. A QEMU smoke test must capture serial output
from the modular kernel itself and reach a controlled idle loop; printing
expected messages from the bootloader or a demonstration function is
insufficient.
