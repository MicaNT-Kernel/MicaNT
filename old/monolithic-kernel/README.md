# Archived Standalone Kernel

`ntoskrnl.cpp` is the historical standalone bare-metal kernel implementation
that was introduced by commit `ce758bbdbd20a72025ae9da0570ac37745aa547d`.
It is retained solely to preserve project history and support migration
analysis. It is not part of the active kernel architecture.

The file was introduced on 2026-10-19 UTC. The modular kernel subsystem tree
was established on 2026-10-13. The standalone implementation duplicated
initialization and hardware-facing behavior instead of composing the existing
`ntoskrnl_core` subsystems.

Do not compile, link, stage, install, or use this source to resolve missing
symbols. New kernel work belongs in the canonical subsystem and must not
reintroduce a parallel kernel. The intended architecture and current migration
status are documented in [`docs/KERNEL_ARCHITECTURE.md`](../../docs/KERNEL_ARCHITECTURE.md).

Bare-metal work still to migrate or verify includes:

- A versioned, ABI-stable loader parameter block and validated UEFI handover.
- Early serial logging, CPU context setup, IDT/trap handling, APIC and timer
  initialization in the HAL and `Ke`.
- Initial page tables, physical-page allocator, and memory-map ownership in
  `Mm`.
- A freestanding C++ runtime limited to facilities actually used by the
  kernel.
- The AMD64 syscall path and a supported Ring 3 transition.
- PE image loading and relocation in the canonical loader/process path.
- Boot-start storage, USB, and display drivers using `Io` and the existing
  driver contracts.
- A production entry point that calls modular `KiSystemStartup`, followed by
  Phase 0, Phase 1, and a controlled idle loop.
- PE validation and an end-to-end QEMU/OVMF smoke test proving execution of
  the modular kernel.

These items are not claimed to be migrated merely because similarly named
headers, host tests, or legacy boot artifacts exist.
