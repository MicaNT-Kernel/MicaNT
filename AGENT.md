# MicaNT Engineering Rules

These rules apply to every change in this repository. Read them together with `README.md`, `CONTRIBUTING.md`, `docs/CLEAN_ROOM.md`, and `docs/LEGAL.md`; where a narrower subsystem specification exists, follow it as well.

## Compatibility contract

- Implement Windows compatibility from the externally visible Windows contract. Use Microsoft's official `win32metadata` as the primary source for Win32 declarations, and public Microsoft Learn / WDK documentation for contracts that metadata does not describe.
- Match the declared Windows API names, exports, calling conventions, parameter and structure layouts, constants, return values, error behavior, and lifecycle semantics. Do not create MicaNT-only public APIs or invent Windows API names, signatures, flags, or behavior.
- Before adding or changing an externally visible API, verify its declaration and contract in the approved references. If the contract cannot be established, do not guess: investigate the official references and document the unresolved requirement.
- Keep private implementation details private. Internal helpers are allowed only when they do not alter or masquerade as the Windows ABI.
- Treat compatibility as an implementation requirement to be verified, not as a claim that an unimplemented surface is already 100% compatible.

## Clean-room implementation and dependencies

- Write new, independent MicaNT implementations in the repository's established C++23 architecture. Public documentation and interface metadata may guide behavior; they do not authorize copying implementation code.
- Do not import, vendor, port, or copy source code or assets from ReactOS, Wine, Windows, or another external project. Such projects may be consulted only as structural or behavioral references when the task permits it; implement the functionality anew.
- Do not add external runtime libraries or dependencies. Use the repository's existing code and approved toolchain. A proposed build-time tool or dependency must be justified against the project documentation before being introduced.
- Do not use leaked or confidential source, decompiled proprietary binaries, or copyrighted Microsoft binaries/assets. Follow the clean-room requirements in `CONTRIBUTING.md` and `docs/CLEAN_ROOM.md`.

## Architecture and implementation

- Follow the subsystem boundaries, directory layout, naming, ABI, and design specified in `README.md`, `docs/KEY_PILLARS.md`, `docs/SOVEREIGN_TAXONOMY.md`, and subsystem-specific documentation.
- Implement functionality in the canonical subsystem that owns the Windows contract. Avoid application-specific shims, satellite implementations, and workarounds that bypass the owning subsystem.
- Preserve freestanding and supported-architecture constraints. Do not assume hosted operating-system services or introduce a dependency that breaks the kernel/user-mode build model.
- Prefer RAII, bounded views such as `std::span`, explicit ownership, and the project's synchronization primitives. Validate sizes, state transitions, and failure paths.
- Preserve zero-telemetry behavior and avoid unrelated changes.

## Change workflow

- Read the relevant specification and existing implementation before editing. Identify the owning subsystem and the Windows contract first.
- Make changes in coherent, reviewable steps. Update relevant documentation when behavior or interfaces change.
- Build the affected targets and run the applicable existing checks when feasible. Do not claim a behavior is verified unless it was exercised; record environmental limits clearly.
- Keep commits focused on one implementation step and do not include unrelated working-tree changes.
