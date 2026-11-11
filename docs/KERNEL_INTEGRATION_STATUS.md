# Modular Kernel Integration Status

This is the integration progress record required by
[ADR-0001](adr/0001-single-modular-kernel.md) and
[KERNEL_ARCHITECTURE.md](KERNEL_ARCHITECTURE.md). It does not replace either
specification. The compatibility inventory is
[FREESTANDING_GAP_MATRIX.md](FREESTANDING_GAP_MATRIX.md).

## Verified baseline

- One active modular AMD64 Native PE kernel, no external PE imports.
- Versioned UEFI handoff; separately loaded VirtIO-GPU 2D driver.
- Supervisor-owned page tables, GDT/TSS/IDT, checked CPL3 entry/return,
  supervisor/RO/NX protection probes.
- Hardware syscall entry with a kernel stack, register/SIMD preservation,
  bounded user-buffer checks and actual user-originated VirtIO presentation.
- Polled PS/2 cursor movement and click delivery; QMP pixel/input checks pass.
- Original bounded PE diagnostic mapped by the canonical native loader and
  entered at CPL3. This is not a process, DLL startup, or Explorer.
- Canonical PFN database initialized from validated firmware conventional
  ranges; actual physical writes, reference retention, complete page zeroing
  and contiguous release verified before display initialization.

Reproduced from the current sources, not by accepting an old kernel binary:

| Command | Result |
|---|---|
| `python3 qemu.py --build-only` | Passed; kernel, bootloader, driver and manifest generated/validated |
| `python3 qemu.py --headless` | Passed; ordered serial milestones, stable idle, PS/2 and scanout checks |
| `cmake --build build/qemu-cmake-mingw --target explorer_runtime_boot` | Passed; actual Explorer and windowing DLLs built, not executed |
| `python3 -m unittest discover -s test -p test_explorer_runtime.py -v` | 12 passed |
| `python3 scripts/verify_kernel_architecture.py` | Passed |

The runner invokes the affected build before staging and validates the exact
kernel/bootloader/driver artifact set. An up-to-date Ninja result is not a
substitute for those checks. Host tests and PE linking do not establish boot
integration.

## Integration increments

### Protected syscall boundary

Commit `d8e8d42`: one shared user-service ABI helper; real AMD64 entry/return
and bounded graphics/input services. There is still no Executive scheduler,
general NT service dispatch, interrupt-driven input, RTC service or accelerated
composition session.

### Bounded native image preparation

The allocation-free `PeImageView64` lives in the canonical
[`pe.hpp`](../include/micant/um/native/pe.hpp); the existing hosted
`PeLoader::inspect` delegates to it. `NativeUserImageSet64` in the canonical
[`usermode.hpp`](../include/micant/km/executive/usermode.hpp) owns no process
or address space: it borrows image storage and resolves bounded imports,
exports and forwarders without synthetic trampolines.

Host ASan/UBSan tests cover malformed/truncated PE input, mapping, relocations,
named/ordinal imports and exports, missing bindings, forwarder cycles, overlap,
TLS layout and real Explorer/windowing/UCRT images:

```bash
clang++ -std=c++23 -Wall -Wextra -Werror -Wno-unused-parameter \
  -fsanitize=address,undefined -Iinclude test/pe_image_view_test.cpp \
  -o build/pe_image_view_test
build/pe_image_view_test build/explorer.exe build/system32/user32.dll \
  build/system32/gdi32.dll build/system32/imm32.dll build/system32/win32k.dll
```

Result: passed. The QEMU diagnostic constructs an original small PE containing
the compiled syscall client, maps its headers RO/NX and code RX, then verifies
actual execution/return at CPL3. Minimal memory/string primitives are supplied
by [`kernel/ex/freestanding.cpp`](../kernel/ex/freestanding.cpp).

**Remaining:** firmware/user image handover, owned physical pages and address
spaces, process/thread lifecycle, PEB/TEB and static TLS initialization,
DLL attach/detach and complete native runtime closure. Inspecting TLS does not
initialize TLS. The older hosted loader/trampoline path is not a native
fallback and is not enabled by this increment.

### Native UCRT memory subset

The canonical UCRT source has an isolated freestanding memory/string branch
with a bounded 64 MiB coalescing arena and native PE static TLS for `errno`.
The supported `ucrtbase_boot` CMake target builds it into
`build/system32/ucrtbase.dll`; the Explorer build aggregate includes it.
It exports 16 memory/string/errno functions, with no external PE imports.
Incomplete aligned-allocation exports are deliberately absent.

Verified:

```bash
clang++ -std=c++23 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude \
  test/ucrt_bounded_arena_test.cpp -o build/native-ucrt-arena-sanitized
build/native-ucrt-arena-sanitized
cmake --build build/qemu-cmake-mingw --target explorer_runtime_boot
build/pe_image_view_test build/system32/ucrtbase.dll
```

Results: passed. LLVM PE inspection confirms the actual target's empty import
table and valid TLS template/index; `_errno` disassembly reads the GS TLS vector
at offset `0x58`. The native target uses `-fno-emulated-tls`, no hosted CRT,
and an entry appropriate only to its constant-initialized memory subset.
The windowing DLLs still retain their real CRT constructor startup.
The canonical hosted UCRT branch also compiles with the cross-toolchain.

This is not a staged boot module or complete UCRT. TLS has not been executed
by the kernel; mapping/inspection is not a substitute. Its contract follows
the public [CRT errno documentation](https://learn.microsoft.com/en-us/cpp/c-runtime-library/errno-doserrno-sys-errlist-and-sys-nerr)
and [PE TLS specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#the-tls-section).
`python3 qemu.py --check-explorer` still **fails explicitly** on 26 Kernel32
requirements and 31 UCRT exports, reduced from 73 total missing requirements
to 57. No fake export, host DLL or synthetic import trampoline fills those
gaps. Headless boot, the 12 parser tests and architecture checks still pass.

### Allocation-free firmware memory descriptors

The existing UEFI memory-type and descriptor declarations now live in
[`boot_contract.hpp`](../include/micant/km/hal/boot_contract.hpp), which the
hosted UEFI header already includes. There is one unchanged declaration, not
a second boot memory-map format. Static assertions preserve the 40-byte
descriptor and field offsets; `UefiKernelHandoffV2` remains 104 bytes.
The standalone handoff test and headless QEMU boot pass after the extraction.
This only removes a hosted-header dependency; PFN enrollment is not claimed
by the extraction itself.

### Canonical PFN boot backing

Commit `9b6d735` integrates the isolated Mm worker's canonical `PfnDatabase`
adapter. It keeps one manager and shared allocation/reference/free algorithms;
intrusive PFN links replace dynamic page lists. Native metadata is borrowed,
not allocated through a hosted runtime. Hosted legacy initialization remains
available. Initial native pages are Free, never falsely reported Zeroed.

The boot integration owns stable metadata for **65,536 PFNs (256 MiB)**.
It validates descriptor version/stride/range/overlap and supervisor mappings,
enrolls only non-runtime conventional pages above the first MiB, and leaves
all other PFNs reserved. Higher and intentionally unmanaged conventional
pages are counted and logged explicitly, not silently reclaimed.
Loader/firmware, kernel, driver, MMIO and runtime-service pages are not
reclassified as free.

The supplied zero routine verifies writable supervisor identity backing
before clearing actual RAM. Boot dirties all 4096 bytes of an allocated
physical page, verifies reference retention, frees and zeros it, checks every
byte, reallocates that zeroed frame, and verifies two contiguous frames and
restored free/active counts. The smoke runner now requires this real
verification before the existing display/CPL3/input markers.

```bash
clang++ -std=c++23 -DMICANT_FREESTANDING -Wall -Wextra -Werror \
  -Wno-unused-parameter -fsanitize=address,undefined -Iinclude \
  test/pfn_database_bootstrap_test.cpp -o build/pfn-native-sanitized
build/pfn-native-sanitized
clang++ -std=c++23 -Wall -Wextra -Werror -Wno-unused-parameter \
  -fsanitize=address,undefined -Iinclude test/pfn_database_bootstrap_test.cpp \
  -o build/pfn-hosted-sanitized
build/pfn-hosted-sanitized
python3 qemu.py --headless
```

Results: native and hosted sanitizer tests passed, including sparse and
invalid maps, references, zeroing, contiguous allocation, exhaustion and
reinitialization; QEMU passed every old and new required milestone.
The isolated production-flag PFN probe needs only memory copying and its
supplied zero routine, not heap, mutex, TLS, atexit or static-guard symbols.

**Limits:** this is the canonical physical allocator component, not the full
Mm virtual-memory API. Borrowed backing and its private atomic lock are for
the current IRQ-disabled bootstrap only; they do not implement Ke IRQL or
scheduled waits. No parallel bare-metal memory manager is activated.

The subsequent sparse-map regression (`9a10596`) prevents partial physical
release when a requested contiguous range crosses an allocated page and a
reserved hole. Native and hosted sanitizer tests check PFN 0, holes, wrapping
and oversized release ranges without mutating counts or invoking the zero
callback. Headless boot passed again after integration.

### PFN-backed kernel C/C++ allocation

The private runtime frontends in `kernel/ex/freestanding.cpp` now allocate
through canonical `PfnDatabase::allocateContiguousPages` and release through
its corresponding free path. The boot callback verifies actual writable
supervisor identity backing for every frame before returning a mapping.
No user-mode arena, hosted CRT, new memory manager or dummy export is used.
The existing no-import Native kernel PE still links.

`python3 qemu.py --headless` passes a new mandatory runtime check exercising
actual `malloc`, `calloc`, preserving grow/shrink/failed `realloc`, `free`,
ordinary/array/over-aligned/nothrow C++ allocation and deletion, real RAM
contents, overlap-safe memory moves, overflow rejection logging and restored
PFN accounting. Throwing allocation failure and invalid frees explicitly
stop rather than pretending success. The runtime has no unresolved hosted
symbols in the freestanding object probe.

This is deliberately a page-granular bootstrap allocator: each allocation
consumes contiguous pages, with in-page ownership metadata. It is not a
fragmentation-resistant Executive pool, a pageable heap, an SMP allocator,
general C++ library support, constructor registration, synchronization or
exception/unwind runtime. Its private contract requires one CPU and disabled
interrupts, matching the current boot. Higher managers still need their
shared runtime and lifecycle dependencies from the gap matrix.

### Canonical Ex checked-copy boundary

The existing status-returning helpers in `probe.hpp` now bind a private,
allocation-free native range checker rather than the hosted synthetic
permission map. The binding uses the effective permissions from the active
AMD64 hardware tables and rejects use with interrupts enabled. The native
branch has no C++ `try/catch`; it checks the complete user span and the
supervisor source/destination before copying. The hosted synthetic backend
and exception-enabled helpers retain their existing behavior.

All current bootstrap graphics, serial and mouse syscall buffer accesses now
use those canonical helpers. The real CPL3 diagnostic still checks returned
contents, null/RO/unmapped/supervisor/wrapping rejection, registers and SIMD;
the submitted pixels are checked through QMP. The boot also requires observed
successful and rejected canonical-copy paths, not just linked symbols.

`python3 qemu.py --headless` passed after wiring. Both standalone modes below
passed ASan/UBSan, including absent backend, binding validation, misalignment,
overflow, cross-page protection, untouched rejected destinations and kernel
source/destination validation:

```bash
clang++ -std=c++23 -DMICANT_FREESTANDING -fno-exceptions -fno-rtti \
  -Wall -Wextra -Werror -Wno-unused-parameter -fsanitize=address,undefined \
  -Iinclude test/bootstrap_probe_test.cpp -o build/bootstrap-probe-native-sanitized
build/bootstrap-probe-native-sanitized
clang++ -std=c++23 -Wall -Wextra -Werror -Wno-unused-parameter \
  -fsanitize=address,undefined -Iinclude test/bootstrap_probe_test.cpp \
  -o build/bootstrap-probe-hosted-sanitized
build/bootstrap-probe-hosted-sanitized
```

These are private C++ status adapters, **not** exported WDK `ProbeForRead` or
`ProbeForWrite` implementations: the documented Windows ABI is void and
raises exceptions. See the official [read](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-probeforread)
and [write](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-probeforwrite)
contracts. Native validation/copy requires immutable mappings on one CPU with
interrupts disabled; this is not recoverable access to arbitrary faulting
memory or protection against concurrent unmapping. Before enabling scheduling,
IRQs or SMP, replace this restricted backend with proper pinning/fault
recovery. No hosted synthetic access permission is accepted by the kernel.

### Kernel C++ lifecycle and canonical Ob handle storage

The private kernel runtime now dispatches real compiler-generated `.ctors`
and PE `.CRT` initialization after the PFN heap. Bounded `atexit` registrations
run in LIFO order on final shutdown; overflow, invalid phases and library
contract failures explicitly log and halt. The compiler ABI abort path logs
its original format verbatim, not fabricated formatted arguments.

The actual `kernel/ob/obref.cpp` is now linked, with its one original
`g_KernelHandleTable`. Its compiler-generated global constructor allocates the
canonical vector through the real kernel heap. Boot verifies actual handle
creation, granted access, duplication/inheritance, reference/handle counters,
close, rejected double close and empty-table restoration. There is no second
Ob or alternate table. Native `NtClose` is deliberately absent until the
sync/Io/Mm/Cm/Se/Lpc close paths exist: the hosted implementation is preserved,
not replaced by fake close hooks. Namespace/security/process ownership remain
unintegrated.

Verified:

| Command | Result |
|---|---|
| `python3 qemu.py --headless --runtime-selftest` | Passed; genuine C/global constructors allocate/write/check memory, destructors execute in order 21 and all PFN allocation accounting returns to its pre-construction value, including Ob vector destruction |
| `python3 qemu.py --headless` | Passed; normal boot keeps globals alive and preserves CPL3/protection/scanout/mouse tests |
| `python3 -m unittest discover -s test -p 'test_*runtime*.py' -v` | 19 passed; mode-specific milestones, fatal detection, provenance and normal-mode restoration |
| `build/bootstrap-object-handles-sanitized` | Passed; ASan/UBSan canonical handle lifecycle |
| Cross-toolchain compilation of hosted `kernel/ob/obref.cpp` | Passed; native gating does not remove hosted close functionality |
| `llvm-nm -u build/baremetal/ntoskrnl.exe` / `llvm-readobj --coff-imports build/baremetal/ntoskrnl.exe` | No undefined symbols or external PE imports |

The self-test is opt-in and halts after final teardown, never using destroyed
objects to continue a desktop boot. Normal invocations explicitly reset the
CMake option to OFF. Its manifest records the mode; ordinary staging rejects
self-test artifacts. This foundation is still single-CPU/IRQ-disabled,
bounded to 4096 initializers per table and 1024 destructor registrations,
without C++ exceptions, arbitrary libc++ linkage, native TLS, thread-safe
static guards or a general shutdown/session lifecycle.

## Milestones

| Milestone | State | Missing acceptance evidence |
|---|---|---|
| M1: freestanding foundation | Partial; PFN heap and kernel constructor/destructor dispatch verified | Shared C++ library/synchronization and broader Executive initialization |
| M2: Mm + Ob | PFN and canonical kernel handle storage verified | Full mapping/freeing/address spaces, namespace/security and NtClose closure |
| M3: Ke + Ps | Not integrated | IRQ/timer backend, scheduler, independent address spaces, actual context switching |
| M4: Io + syscall | Partial platform boundary only | Canonical driver/device/IRP initialization, completion and NT service routing |
| M5: Ring 3 processes | Diagnostic only | Real process loader, PEB/TEB/TLS, termination and two concurrent processes |
| M6: system services | Not integrated | Registry/filesystem/service/console startup and native Win32 session |
| Interactive Explorer | Not running | Runtime closure, actual shell process, Start/window/message-loop verification |

The displayed bootstrap taskbar is shared drawing code, not proof of an
Explorer process. VirtIO 2D scanout is not VirGL or an accelerated DWM.

## Parallel audit boundaries

Independent Mm/Cc/FsRtl, Ob/Se/Cm/auxiliary, Ke/Ps and Io/PnP/Po audits use
separate `copilot/integration-*-615faeb8` branches/worktrees based on `d8e8d42`.
They do not change production interfaces. The primary stream owns shared
runtime/build decisions and integration. Worker compilation probes are
compatibility experiments, not production links.

No new allocator, mapping, object, process, IRQL or driver ABI is frozen by
this audit: use the existing canonical contracts. Any required signature
change must be reviewed centrally before worker implementation. Integrate in
dependency order: runtime/backing, Mm, Ob, Ke, Ps, Io, then higher services.
Advanced incomplete features are not prerequisites for the first boot.
