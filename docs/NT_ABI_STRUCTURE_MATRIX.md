# NT ABI Structure Matrix

This matrix distinguishes public interface requirements from implementation
details. Microsoft [`win32metadata`](https://github.com/microsoft/win32metadata)
is the primary declaration reference; use Microsoft Learn and public WDK
documentation for structure semantics and field visibility. ReactOS headers
are a comparison aid, not the authority when they conflict with the selected
Windows contract.

**No ABI conformance is implied by a matching name.** Current MicaNT status
below reflects source inspection; only explicit compile-time assertions and
tests count as layout evidence. No ABI-size/offset report for these structures
is currently maintained, except where noted. Review the target Windows AMD64
ABI rather than relying on host compiler layout.

## Classification

1. **Public layout** — the Windows interface publicly defines the layout or
   contract; MicaNT must match it where exposed.
2. **Partially visible** — drivers or user code may depend on documented
   fields, but the complete layout/semantics are not a stable general-purpose
   contract. Match documented fields and preserve opaque portions.
3. **Internal/opaque** — private Executive representation. Keep MicaNT's
   internal layout unless a documented external contract requires otherwise.

## Structure comparison

| Structure | Windows visibility | MicaNT declaration / evidence | Current ABI status and next check |
|---|---|---|---|
| `PEB` | Partially visible; user-mode consumers use documented fields, with many internal or version-dependent fields. | `Peb` in [`ps.hpp`](../include/micant/km/executive/ps.hpp), allocated and initialized by hosted process code. | **Unverified.** Compare required AMD64 field offsets with the selected public declarations; keep undocumented fields opaque and do not claim a complete PEB. |
| `TEB` | Partially visible; stable documented thread-environment fields coexist with internal/version-dependent fields. | `Teb` in [`ps.hpp`](../include/micant/km/executive/ps.hpp), with an NT_TIB-like prefix and process/thread fields. | **Unverified.** Verify required field offsets and GS access conventions on the Windows AMD64 target; current source has no layout assertions. |
| `CONTEXT` | Public architecture-specific context record; field order, alignment, and flags are contract-sensitive. | `CONTEXT` in [`context.hpp`](../include/micant/shared/context.hpp); `CONTEXT_AMD64` in [`bugcheck.hpp`](../include/micant/km/executive/bugcheck.hpp). `context.hpp` has `static_assert(sizeof(CONTEXT) == 1232)`. | **Partially asserted, not fully verified.** Confirm exact AMD64 declaration, alignment, member offsets, extended-register representation, and `ContextFlags`; the size assertion alone is insufficient. |
| `EXCEPTION_RECORD` | Public exception record layout. | `EXCEPTION_RECORD` in [`ntdll.hpp`](../include/micant/um/native/ntdll.hpp) and another user-mode declaration in [`wer.hpp`](../include/micant/um/subsystems/mgmt/wer.hpp). | **Unverified / duplicate declarations.** Compare size, alignment, offsets, pointer width, and array bounds; consolidate only after confirming owner and callers. |
| `UNICODE_STRING` | Public layout: byte lengths and a pointer-sized buffer. | `UnicodeString` / `UNICODE_STRING` in [`ntdef.hpp`](../include/micant/shared/ntdef.hpp). | **Unverified.** Check AMD64 size/offsets and Windows 16-bit `WCHAR` semantics in both cross and host builds; no matching layout assertion found. |
| `OBJECT_ATTRIBUTES` | Public layout and initialization contract. | `ObjectAttributes` in [`ntdef.hpp`](../include/micant/shared/ntdef.hpp), with an internal `Handle` and `UnicodeString` pointer. | **Unverified.** Verify AMD64 field types, offsets, total size, object-name lifetime, and supported attribute semantics against `win32metadata`/WDK. |
| `IO_STATUS_BLOCK` | Public layout with a status/pointer union and pointer-sized information field. | `IoStatusBlock` in [`ntdef.hpp`](../include/micant/shared/ntdef.hpp). | **Unverified.** Check signed status width, union alignment, field offsets, and total size on the Windows AMD64 ABI. |
| `IRP` | Partially visible WDM structure; drivers interact with documented fields and stack locations, while lifecycle and internal fields are constrained by the I/O contract. | `Irp` in [`io.hpp`](../include/micant/km/executive/io.hpp). | **Not ABI-verified.** Compare every driver-visible member and stack/ownership semantic; a MicaNT data packet is not interchangeable merely because it is called `Irp`. |
| `IO_STACK_LOCATION` | Partially visible WDM structure with documented major/minor function and parameter overlays. | `IoStackLocation` in [`io.hpp`](../include/micant/km/executive/io.hpp). | **Not ABI-verified.** Check union arms, alignment, size, stack traversal, and completion behavior against WDK. |
| `DRIVER_OBJECT` | Partially visible WDM structure; driver initialization and dispatch fields are externally consumed. | `DriverObject` in [`io.hpp`](../include/micant/km/executive/io.hpp). | **Not ABI-verified.** Verify documented AMD64 offsets and lifecycle before exposing it to binary drivers. |
| `DEVICE_OBJECT` | Partially visible WDM structure; device/driver linkage and documented flags are driver-visible. | `DeviceObject` in [`io.hpp`](../include/micant/km/executive/io.hpp). | **Not ABI-verified.** Compare documented members, alignment, stack links, and reference/delete lifecycle. |
| `MDL` | Partially visible WDM structure with documented header and page-array semantics; internal fields are not all stable. | `MDL` in [`mm.hpp`](../include/micant/km/executive/mm.hpp) contains a C++ `std::vector<uint64_t>` page list. | **Not WDK-layout-compatible as declared.** Keep it internal until redesigned as a target-compatible public MDL representation; verify sizing, PFN array placement, probing, locking, and mapping semantics. |
| `KAPC` | Partially visible kernel DDI structure; routines and initialization behavior are contract-sensitive. | `KAPC` in [`ke.hpp`](../include/micant/km/core/ke.hpp). | **Unverified.** Verify exposed fields and APC insertion/delivery/rundown semantics; do not assume layout from field-name similarity. |
| `KDPC` | Partially visible kernel DDI structure; routine/context/argument fields and queue lifecycle are contract-sensitive. | `KDPC` in [`ke.hpp`](../include/micant/km/core/ke.hpp). | **Unverified.** Verify structure fields, target processor semantics, synchronization, and DPC execution constraints. |
| `KEVENT` | Partially visible dispatcher object; storage is commonly opaque to callers, but initialization/wait contract is public. | `KEVENT` in [`sync.hpp`](../include/micant/km/core/sync.hpp), wrapping MicaNT's dispatcher header. | **Layout unverified; internal wrapper may remain private.** Verify the callable DDI and any driver-visible storage requirement before exposing a WDK ABI. |
| `KMUTANT` | Partially visible/legacy synchronization object; owner and wait semantics are contract-sensitive. | `KMUTANT` and `KMUTEX` in [`sync.hpp`](../include/micant/km/core/sync.hpp). | **Unverified.** Verify the supported contract and distinguish public storage from MicaNT-only accounting fields. |
| `KSEMAPHORE` | Partially visible dispatcher object; public initialization/release/wait semantics matter more than private scheduler representation. | `KSEMAPHORE` in [`sync.hpp`](../include/micant/km/core/sync.hpp). | **Unverified.** Verify DDI semantics, field visibility, alignment, and any required ABI prefix. |
| `KTIMER` | Partially visible dispatcher object; expiration and DPC semantics are public, internal timer-wheel representation is not. | `KTIMER` in [`timer.hpp`](../include/micant/km/core/timer.hpp), wrapping a C++ `TimerObject`. | **MicaNT internal wrapper, not asserted as WDK layout.** Keep private or provide a separately verified public representation if drivers require it. |
| `KTHREAD` | Internal Executive structure; generally opaque to third-party drivers. | `KThread` in [`ke.hpp`](../include/micant/km/core/ke.hpp). | **Internal.** Preserve MicaNT's design; verify only externally observable scheduling, synchronization, and thread lifecycle contracts. |
| `EPROCESS` | Internal Executive structure; generally opaque. | `EProcess` in [`ps.hpp`](../include/micant/km/executive/ps.hpp). | **Internal.** No ReactOS layout copying required; validate process behavior and exposed query APIs. |
| `ETHREAD` | Internal Executive structure; generally opaque. | `EThread` in [`ps.hpp`](../include/micant/km/executive/ps.hpp). | **Internal.** Validate exposed thread behavior and lifecycle, not private field offsets. |
| `SECURITY_DESCRIPTOR` | Public descriptor representation, with absolute and self-relative forms and defined field contracts. | `SECURITY_DESCRIPTOR` and `SECURITY_DESCRIPTOR_RELATIVE` in [`acl.hpp`](../include/micant/um/subsystems/security/acl.hpp). | **Unverified.** Compare alignment, control flags, SID/ACL pointers or relative offsets, and conversion behavior. |
| `ACCESS_STATE` | Partially visible/internal security-manager state used by kernel I/O/object access paths; not a general user ABI. | No canonical MicaNT declaration was found in the inspected public headers. | **Missing / not applicable as a public layout until an owner requires it.** Establish the WDK-visible contract and minimum fields before adding it. |

## Verification work required

For each layout that MicaNT exposes to Windows-compatible code:

1. Record the exact AMD64 declaration source and applicable target profile.
2. Add target-compiled `sizeof`, `alignof`, and `offsetof` assertions for every
   documented field that must match.
3. Add a host-side layout test only when the host ABI is intentionally the same;
   never treat host success as a Windows ABI result.
4. Test packing, union overlays, pointer-width conditionals, and fixed-width
   character types.
5. Test observable lifecycle and semantics separately from memory layout.

Internal structures (`KTHREAD`, `EPROCESS`, `ETHREAD`, and private portions of
partially visible types) remain MicaNT design choices unless a public contract
or binary driver compatibility requirement proves otherwise.
