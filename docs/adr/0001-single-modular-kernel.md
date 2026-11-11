# ADR-0001 — Single Modular Kernel Architecture

- **Status:** Accepted
- **Date:** 2026-11-11

## Context

The modular NT subsystem source tree was introduced on 2026-10-13 (Git
revision `3503302e1d498fead6e93ad22b06f6a7f5d5b1d5`). A standalone bare-metal
kernel was introduced by `ce758bbdbd20a72025ae9da0570ac37745aa547d`; its Git
commit timestamp is 2026-10-19 UTC. It implemented a separate startup and
hardware path instead of using the canonical `ntoskrnl_core` modules.

Maintaining both paths creates duplicate Executive behavior, divergent
hardware assumptions, and ambiguity about which artifact is authoritative.
The standalone source also had independent build and staging paths, while the
loader block crosses runtime boundaries using STL-owned objects.

## Decision

MicaNT has exactly one canonical kernel: the modular implementation based on
`ntoskrnl_core`. Bare-metal and host tests must use the same canonical
subsystem implementations, with only platform code and host adapters differing.
The standalone implementation is archived under
[`old/monolithic-kernel/`](../../old/monolithic-kernel/) for history and
migration analysis only.

The archived source must never be compiled, linked, staged, installed, scanned
as active source, or used to satisfy missing symbols. No replacement standalone
kernel or duplicate subsystem implementation may be introduced.

## Consequences

- The existing hosted `micant_kernel` executable is a test harness, not the
  production kernel.
- A modular freestanding runtime, AMD64 entry point, versioned loader block,
  Native PE output, and UEFI handover must be implemented before a bare-metal
  kernel can be claimed.
- Staging must validate a build manifest and exact artifact hashes; stale
  artifacts and monolithic fallbacks are rejected.
- CI must fail when the required modular kernel artifact or QEMU boot evidence
  is absent, rather than converting mandatory failures into warnings.
- Host test success, compilation, QEMU boot, hardware boot, and Windows
  compatibility are separate verification claims.
- Bare-metal behavior from the archive is migrated as independent
  implementations in the canonical destination modules.

## Migration completion criteria

The migration is complete only when the modular sources produce a validated
PE32+ AMD64 Native kernel, the UEFI loader passes a versioned ABI-stable block,
the kernel reaches Phase 0 and Phase 1 under QEMU/OVMF, the smoke test reaches
a stable idle loop with real serial output, and CI makes those checks
mandatory. The detailed, current status is maintained in
[`KERNEL_ARCHITECTURE.md`](../KERNEL_ARCHITECTURE.md).

This decision prevails over older architectural descriptions where they
conflict.
