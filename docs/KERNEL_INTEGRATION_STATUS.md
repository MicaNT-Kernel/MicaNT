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

## Milestones

| Milestone | State | Missing acceptance evidence |
|---|---|---|
| M1: freestanding foundation | Partial | Shared C++ runtime/allocator backing and first full canonical Executive initialization |
| M2: Mm + Ob | Not integrated | Physical allocation from UEFI map, mapping/freeing, canonical namespace/handles in QEMU |
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
