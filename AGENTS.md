# MicaNT Engineering Rules

These rules apply to every change in this repository. Read them together with `README.md`, `CONTRIBUTING.md`, `docs/CLEAN_ROOM.md`, and `docs/LEGAL.md`; where a narrower subsystem specification exists, follow it as well.

## Compatibility contract

- Implement Windows compatibility from the externally visible Windows contract. Use Microsoft's official `win32metadata` as the primary source for Win32 declarations, and public Microsoft Learn / WDK documentation for contracts that metadata does not describe.
- Match the declared Windows API names, exports, calling conventions, parameter and structure layouts, constants, return values, error behavior, and lifecycle semantics. Do not create MicaNT-only public APIs or invent Windows API names, signatures, flags, or behavior.
- Before adding or changing an externally visible API, verify its declaration and contract in the approved references. If the contract cannot be established, do not guess: investigate the official references and document the unresolved requirement.
- Keep private implementation details private. Internal helpers are allowed only when they do not alter or masquerade as the Windows ABI.
- Treat compatibility as an implementation requirement to be verified, not as a claim that an unimplemented surface is already 100% compatible.

## Research, reuse, rewrite, and integrate

- Study public specifications, metadata, tests, and relevant open-source implementations (including ReactOS and Wine) when useful. Compare their behavior and architecture with the existing MicaNT implementation before changing code.
- Reuse or adapt external code only when its license and required notices permit that use; otherwise use it as a technical reference and implement the required behavior in MicaNT. Preserve required attribution and notices. Do not copy leaked or confidential source, decompiled proprietary implementation, or proprietary assets.
- Prefer preserving valid MicaNT code, then completing or correcting it. Rewrite only when technically justified. Rework adopted algorithms to fit MicaNT's C++23 conventions and canonical subsystem boundaries; do not disguise provenance by renaming or reformatting.
- Do not add external runtime libraries or dependencies. Use the repository's existing code and approved toolchain. A proposed build-time tool or dependency must be justified against the project documentation before being introduced.
- Do not use leaked or confidential source, or decompiled proprietary binaries. Do not add third-party code or assets without permission under their applicable terms. Follow `CONTRIBUTING.md` and `docs/CLEAN_ROOM.md`.

## Architecture and implementation

- Follow the subsystem boundaries, directory layout, naming, ABI, and design specified in `README.md`, `docs/KEY_PILLARS.md`, `docs/SOVEREIGN_TAXONOMY.md`, and subsystem-specific documentation.
- Implement functionality in the canonical subsystem that owns the Windows contract. Avoid application-specific shims, satellite implementations, and workarounds that bypass the owning subsystem.
- Preserve freestanding and supported-architecture constraints. Do not assume hosted operating-system services or introduce a dependency that breaks the kernel/user-mode build model.
- Prefer RAII, bounded views such as `std::span`, explicit ownership, and the project's synchronization primitives. Validate sizes, state transitions, and failure paths.
- Preserve zero-telemetry behavior and avoid unrelated changes.

## Strict user-mode / kernel-mode boundary

- Enforce strict separation between Ring 3 system DLLs and Ring 0 kernel implementation.
- User-mode DLLs (`kernel32.dll`, `kernelbase.dll`, `ntdll.dll`, `advapi32.dll`, `user32.dll`, `gdi32.dll`, and other user-mode libraries) must not include or depend on private `micant/km/` implementation headers. Shared declarations may be included only through ABI-safe public or mode-neutral headers.
- Access kernel services through the appropriate NTDLL native API, system-call interface, or documented user-mode subsystem mechanism.
- User-mode libraries must not instantiate or manipulate kernel Executive objects, scheduler structures, process managers, memory managers, or driver objects.
- Do not implement simulated kernel services in user-mode DLLs when the corresponding MicaNT kernel service exists.
- Keep host-side test adapters separate from native user-mode DLL implementations.
- The build system must check the actual transitive header dependencies of native user-mode targets and reject any dependency on private `micant/km/` headers.
- Preserve legitimate user-mode operations, including TEB access, string conversion, loader bookkeeping, and other functionality that does not require a system call.

## Change workflow

- Read the relevant specification and existing implementation before editing. Identify the owning subsystem and the Windows contract first.
- Make changes in coherent, reviewable steps. Update relevant documentation when behavior or interfaces change.
- Build the affected targets and run the applicable existing checks when feasible. Do not claim a behavior is verified unless it was exercised; record environmental limits clearly.
- Keep commits focused on one implementation step and do not include unrelated working-tree changes.

## Single modular kernel policy

MicaNT has one canonical modular kernel. All kernel subsystem development must target the modular `ntoskrnl_core` architecture and its bare-metal entry point. The archived monolithic kernel under `old/` is historical reference only and must never be built, linked, staged, or used to satisfy missing symbols.

## Evidence and status

Use the Microsoft Windows contract as the compatibility requirement and official `win32metadata` plus Microsoft Learn/WDK as primary sources. Record unresolved contract details rather than guessing. Keep implementation status separate from declarations, compilation, host tests, QEMU results, and compatibility claims; use `docs/KERNEL_ARCHITECTURE.md` and the integration status documents as the evidence source.
