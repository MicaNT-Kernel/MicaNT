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
- Isolated CPL3 TEB/GS probe: user code writes and reads
  `TEB64.LastErrorValue` at GS offset `0x68` and `TlsSlots[0]` at `0x1480`;
  compile-time layout assertions and the boot harness verify both accesses
  after a checked ring transition. The diagnostic TEB is mapped in two user,
  writable, NX pages; this does not initialize a process's TLS allocation map
  or static TLS template.
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
| `cmake --build build/qemu-cmake-mingw --target ntoskrnl_core` | Passed; canonical core builds the kernel-owned Win32k dispatcher and WDDM adapter |
| `cmake --build build/qemu-cmake-mingw --target micant_tests -j 4` | Test runner compiles, but link fails on duplicate `micant::ucrt::initializeOnExitTable`; tests did not run |
| `clang++ -std=c++23 -Wall -Wextra -Werror -Iinclude test/ke_context_hosted_test.cpp -o build/ke-context-hosted-test && build/ke-context-hosted-test` | Passed; hosted Ke behavior remains intact |
| `cmake --build build/qemu-cmake-mingw --target explorer_runtime_boot` | Rejected by the new boundary check: `win32k_boot` transitively includes private `micant/km/` headers |
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
`build/system32/ucrtbase.dll`; it is included in the Explorer build aggregate,
which remains blocked by the Win32k boundary violation below.
It exports 26 memory/string/errno/math/startup functions, including `ceilf`,
on-exit table operations, initializer-table dispatch, and aligned allocation,
with no external PE imports. `_aligned_malloc` validates nonzero size and
power-of-two alignment, sets `errno` on invalid input or allocation failure,
and `_aligned_free` returns the block to the same arena. The CRT invalid
parameter handler contract is not implemented.

Verified:

```bash
clang++ -std=c++23 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude \
  test/ucrt_bounded_arena_test.cpp -o build/native-ucrt-arena-sanitized
build/native-ucrt-arena-sanitized
clang++ -std=c++23 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -Iinclude test/ucrt_ceilf_test.cpp \
  -o build/ucrt-ceilf-test
build/ucrt-ceilf-test
clang++ -std=c++23 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -Iinclude test/ucrt_onexit_test.cpp \
  -o build/ucrt-onexit-test
build/ucrt-onexit-test
cmake --build build/qemu-cmake-mingw --target ucrtbase_boot
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
The PE closure measured before the boundary gate was introduced still has 23
Kernel32 and 21 UCRT missing exports (44 total), down from 73. The current
`python3 qemu.py --check-explorer` stops earlier: CMake rejects `win32k_boot`
because its transitive dependencies include private `micant/km/` headers. The
initializer and on-exit helpers are not full DLL CRT startup; the kernel loader
does not yet dispatch module initializers or TLS. No fake export, host DLL or
synthetic import trampoline fills those gaps.

### Freestanding Kernel32 last-error accessors

The canonical Kernel32 ABI source has a freestanding `kernel32_boot` target
that exports `GetCurrentProcessId`, `GetCurrentThreadId`, `GetLastError`, and
`SetLastError`. The ID accessors read `TEB64.UniqueProcessId` / `UniqueThreadId`
at GS offsets `0x40` / `0x48`; last-error accessors read/write offset `0x68`.
Compile-time assertions pin these offsets and the PEB pointer at `0x60`. The
DLL has no external PE imports. The native process loader does not yet
initialize a real process TEB or GS base; only the isolated CPL3 diagnostic
TEB has been exercised.
The API contracts are documented by Microsoft Learn for
[`GetCurrentProcessId`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentprocessid),
[`GetCurrentThreadId`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid),
[`GetLastError`](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror),
and
[`SetLastError`](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setlasterror).

Verified:

```bash
cmake --build build/qemu-cmake-mingw --target kernel32_boot
cmake --build build/qemu-cmake-mingw --target ntoskrnl_core
python3 scripts/verify_kernel_architecture.py
python3 qemu.py --headless
```

Results: passed. LLVM PE inspection confirms the four named exports; headless
QEMU confirms the isolated TEB GS field probe and the existing syscall, input,
and scanout checks. This does not establish real process TEB
initialization. Later increments resolved those conversion imports and two
locale-profile imports; the current preflight reports 23 missing Kernel32
exports and 21 missing UCRT exports (44 total), so import closure remains
incomplete.

### Freestanding UTF-8 / UTF-16 conversion

The canonical Kernel32 ABI source now exports `MultiByteToWideChar` and
`WideCharToMultiByte` from `kernel32_boot`. This implementation supports
`CP_UTF8` and `CP_ACP` (the project's `GetACP()` is 65001), UTF-8 scalar
validation, UTF-16 surrogate pairs, `-1` null-terminated lengths, size queries,
strict invalid-input flags, replacement characters when strict mode is off,
and insufficient-buffer/error reporting. Other code pages and UTF-8 default
character pointers are rejected; this is a bounded UTF-8 profile, not full
Windows NLS/code-page support.

The public contracts are documented by Microsoft Learn for
[`MultiByteToWideChar`](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar)
and
[`WideCharToMultiByte`](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte).
The allocation-free conversion routines have ASan/UBSan host coverage:

```bash
clang++ -std=c++23 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude \
  test/utf8_conversion_test.cpp -o build/utf8-conversion-test
build/utf8-conversion-test
cmake --build build/qemu-cmake-mingw --target kernel32_boot
build/llvm-mingw-20260616-ucrt-macos-universal/bin/llvm-readobj \
  --coff-exports build/system32/kernel32.dll
python3 qemu.py --check-explorer
```

Results: host conversion tests and DLL build passed; LLVM PE inspection confirms
the two new named exports. The preflight resolves both new imports and fails on
the remaining 46 exports (23 Kernel32, 23 UCRT). The full Explorer executable
still is not loaded or executed.

### Freestanding UCRT UTF-8 locale profile

The freestanding UCRT now exports `___lc_codepage_func` and
`___mb_cur_max_func`, returning codepage 65001 and maximum encoded character
length 4 for the UTF-8 locale profile. The canonical hosted UCRT helper's
return type now matches the MinGW UCRT declaration (`int`) and reports the
same maximum. This does not implement locale selection, locale data, or
`localeconv`.

The cross-toolchain declaration for `___mb_cur_max_func` is in its public
`stdlib.h` and declares an `int` return; `___lc_codepage_func` is declared in
`locale.h` with an `unsigned int` return. Verification:

```bash
cmake --build build/qemu-cmake-mingw --target ucrtbase_boot
build/llvm-mingw-20260616-ucrt-macos-universal/bin/llvm-readobj \
  --coff-exports --coff-imports build/system32/ucrtbase.dll
python3 qemu.py --check-explorer
```

Results: the freestanding UCRT target build passed and LLVM confirms both
named exports with no external imports. The previously built runtime resolves
both locale imports but still has 44 missing imports (23 Kernel32, 21 UCRT).
A legacy `build/` host cache points to a different, removed source checkout
and could not run `cmake --build build --target micant_tests`; it was left
untouched.

### Strict user-mode / kernel-mode build boundary

`scripts/check_user_mode_boundary.py` runs the configured C++ compiler's
preprocessor dependency scan for each native user-mode target variant. CMake
adds the corresponding check as a prerequisite of each DLL/application target
and of the Explorer aggregate. The compliant freestanding Kernel32, UCRT,
User32, GDI32 and IMM32 source variants pass. The Win32k DLL variant is
rejected: `kernel/ps/winnt_win32k_server.cpp` depends on `micant/km/win32k/`
and many transitive `micant/km/` Executive, memory, I/O, driver and HAL
headers. As a result, `win32k_boot` and `explorer_runtime_boot` are currently
blocked by design. That implementation must move to the canonical kernel
owner and expose real user-mode service calls before this gate can pass.

A kernel-owned Win32k dispatch entry and WDDM adapter now compile into
`ntoskrnl_core`; the standard Executive dispatcher installs that handler.
The entry rejects in-range shadow calls with `STATUS_NOT_SUPPORTED` rather
than invoking legacy handlers that dereference user pointers directly and
cannot return handles. It is a safety gate, not a working Win32k service.
This is only the hosted modular-kernel step, not a completed migration:
`win32k_boot` still contains the legacy user-mode implementation, the new
module is not part of `ntoskrnl_core_baremetal`, and its object currently has
unresolved C++ standard-library/runtime symbols that the freestanding kernel
cannot import. The next step is a bounded freestanding service implementation
and a user-mode syscall client with validated request marshalling and return
values. Explorer remains blocked.

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
| M3: Ke + Ps | Cooperative AMD64 context switching verified in QEMU | IRQ/timer backend, scheduler, independent address spaces, process/thread lifecycle |
| M4: Io + syscall | Partial platform boundary only | Canonical driver/device/IRP initialization, completion and NT service routing |
| M5: Ring 3 processes | Diagnostic only | Real process loader, PEB/TEB/TLS, termination and two concurrent processes |
| M6: system services | Not integrated | Registry/filesystem/service/console startup and native Win32 session |
| Interactive Explorer | Not running | Runtime closure, actual shell process, Start/window/message-loop verification |

The Ke bootstrap switch saves/restores the Windows x64 nonvolatile GPRs
(including RSI/RDI), XMM6-XMM15 and floating-point control words. Its QEMU
self-test switches between two real stacks, checks preserved-register canaries,
thread states and five transitions, and returns to the bootstrap stack. The
backend remains cooperative, single-CPU, interrupt-disabled and limited to a
shared address space; this does not establish a dispatcher, preemption,
independent processes or an Explorer process.

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
