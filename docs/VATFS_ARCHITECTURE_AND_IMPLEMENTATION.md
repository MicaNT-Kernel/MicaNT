# VATfs — Architecture and Implementation Specification

**Project:** MicaNT (AMD64 / x86-64, C++23)  
**Status:** Proposed design; not an implementation or compatibility claim  
**Suggested repository path:** `docs/VATFS_ARCHITECTURE_AND_IMPLEMENTATION.md`  
**Scope:** Native semantic search subsystem, client system DLL, background service, autonomous persistent vector store

## 1. Executive decision

VATfs is a **user-mode semantic storage and search subsystem**. It augments ordinary filesystem search without becoming a filesystem driver and without requiring ext4, NTFS, FAT32, exFAT, or any other specific on-disk filesystem.

The system has three deployable elements:

1. **`vatfs.dll`** — stable, versioned MicaNT-specific client ABI for applications, Explorer and shells.
2. **`VATfsSvc.exe`** — Ring 3 service that owns indexing, job scheduling, durable storage, search, synchronization and recovery.
3. **VATfs container** — versioned binary store containing a vector allocation table, embeddings, indexes, pending-work queue, write-ahead log (WAL), checkpoints and free-space metadata. It may initially live in a regular file and later on a dedicated partition without changing client APIs.

Inference, document parsing, tokenization, chunking and ANN graph maintenance execute in Ring 3. The kernel provides ordinary NT file I/O, identity, notifications, IPC, object handles, access control and compute-device mechanisms; it does **not** interpret embeddings or own a special VATfs syscall.

**Consistency model:** eventual consistency. Normal writes must never wait for the creation of embeddings. VATfs is derived and can be discarded and rebuilt from authorized source files. An index may lag behind its source, but results must not bypass current file-access controls or be represented as fresh when known to be stale.

**Not in scope for the first release:** a new Ring 0 filesystem, cross-volume atomic transactions, an SQL engine, full WinFS semantics, mandatory NPU hardware, or changes to the semantics of standard NT directory enumeration calls.

## 2. Requirements and invariants

### Functional requirements

- Support all mounted filesystems exposed via the canonical MicaNT `Io` / `FsRtl` APIs, subject to actual file-ID and notification capabilities.
- Index filenames and metadata first, then extracted content and semantic embeddings.
- Support path-scoped semantic queries, including the proposed shell forms `ls --search "..."` and `search ./ "..."`.
- Support file create, modify, close, truncate, rename, move, delete and volume mount/unmount as indexing events; coalesce bursts of changes.
- Support a query while indexing is backlogged, with freshness information where relevant.
- Work on the AMD64 CPU backend when GPU/NPU drivers are unavailable; accelerate through the shared MicaNT compute subsystem when available.
- Rebuild all derived vectors and indexes without damaging the original filesystem.

### Non-functional invariants

- Filesystem writes and application startup must not depend on the availability of VATfs.
- Only one writer owns an open VATfs container at a time; readers see a consistent published generation.
- The persistent work queue and store are crash recoverable.
- No successful response may authorize a file the caller cannot currently access.
- No arbitrary file content, embedding or model executes in Ring 0.
- An embedding from a different model revision or content version must not silently masquerade as current.
- Neither `vatfs.dll` nor the service may include private kernel-mode `micant/km/` implementation headers. Use mode-neutral ABI declarations and native NT calls.

## 3. Architecture and execution boundaries

```text
Explorer / Start / MicaNT shell / third-party apps
                       |
            vatfs.dll (Ring 3)
        public C ABI; async query API
                       |
          native LPC/ALPC-equivalent IPC
                       |
           VATfsSvc.exe (Ring 3)
       +---------------+----------------+
       |               |                |
  Query engine    Indexing scheduler   Recovery coordinator
  full-text/ANN   parsers + chunks     WAL + queue replay
       |               |                |
       +------- VATfs container --------+
                 file or raw partition
                       |
       MicaNT Io/FsRtl/Ob/Se/Mm (Ring 0)
                       |
              Source filesystems
         ext4 / NTFS / FAT32 / exFAT / ...
                       |
       CPU inference / GPU / NPU compute
```

**Note:** Inference is invoked by the Ring 3 service via a separate user-mode runtime. Device submission, DMA mappings and memory isolation, when hardware accelerators exist, remain the responsibility of their canonical kernel drivers.

### Responsibility matrix

| Component | Owns | Must not own |
|---|---|---|
| `vatfs.dll` | ABI translation, argument validation, IPC session, cancellation, result enumeration | Index data, document parsers, persistent writes |
| `VATfsSvc.exe` | Queue, indexer coordination, container, query engine, access-control revalidation | Source filesystem transaction or kernel scheduling policy |
| VATfs container | Vector payloads, FileKey mapping, free space, WAL, ANN generations, queue, checkpoints | Authoritative file data or access rights |
| `Io`/`FsRtl` | Open/read/close, file IDs when available, notifications and storage access | Embedding computation |
| `Ob`/`Se` | Handles, caller identity and effective access | Stale cached authorization decisions |
| AI runtime | Embeddings and numerical similarity kernels | File metadata authority |
| Compute Manager | Accelerator memory, queues, fences, DMA isolation | Search semantics or model parsing |

## 4. Repository integration

Before adding files, inspect the actual current project tree and build definitions. The following paths are proposals, not assertions that the folders already exist:

```text
include/micant/shared/vatfs_protocol.hpp
include/micant/um/subsystems/search/vatfs.h
user/vatfs/client/vatfs_dll.cpp
user/vatfs/service/main.cpp
user/vatfs/service/indexer.cpp
user/vatfs/service/query.cpp
user/vatfs/store/container.cpp
user/vatfs/store/wal.cpp
user/vatfs/store/queue.cpp
user/vatfs/store/vector_index.cpp
user/vatfs/compute/embedding.cpp
test/vatfs/...
docs/VATFS_ARCHITECTURE_AND_IMPLEMENTATION.md
```

Respect the project's existing user-mode DLL and system-service build conventions. Link with native user-mode runtime only. Do not add a second Kernel Object Manager, Memory Manager, storage stack or compute manager.

**Installation targets (future):** `System32\\vatfs.dll`, `System32\\VATfsSvc.exe`, and initially `ProgramData\\MicaNT\\VATfs\\index.vat` under an appropriately restricted directory. A dedicated VATfs partition is optional and must use the same logical container format.

## 5. Source file identity and freshness

Never use the file path as the primary persistent key. Use an internal `FileKey` equivalent to:

```cpp
struct VatFileKeyV1 {
    uint8_t volumeUuid[16];
    uint64_t fileIdLow;
    uint64_t fileIdHigh;
    uint64_t fileGeneration;
};

struct VatContentRevisionV1 {
    uint64_t observedChangeSequence;
    uint64_t observedSize;
    uint64_t observedWriteTime;
    uint64_t optionalContentFingerprint;
};
```

These are **illustrative internal representations, not committed ABI or packed disk structures**. Persist explicit little-endian fields rather than serializing C++ objects or structures with compiler-dependent padding.

- File IDs and generations have filesystem-specific guarantees. Inode numbers may be reused, FAT-family identifiers may be unstable and portable 128-bit IDs may not exist. Mark identity quality per volume and schedule rescan when it cannot be trusted.
- For ext4, use volume UUID plus inode and generation where reliably exposed by the MicaNT driver.
- For NTFS, use the volume identity and file reference semantics provided by its verified driver; use change journal sequencing if implemented.
- For filesystems lacking suitable IDs, use a fallback reconciliation key and treat rename as possibly delete+create.
- For each vector include the source content revision, model ID and model version, dimension, encoding, chunk range and extraction pipeline version.
- A rename normally changes the displayed path, not the embedding. A content change invalidates the prior embedding generation. A deletion tombstones the corresponding FileKey.
- New results must be revalidated against the source object, including current access rights; do not present an old path cached by VATfs as proof of identity.

## 6. Persistent change capture and queue

VATfs owns its **entire persistent queue**. Source filesystems need not support JBD2 and do not participate in VATfs transactions.

### Event ingestion

1. Subscribe to ordinary NT directory-change notifications when available. Prefer a filesystem change journal for efficient catch-up if a specific driver implements one.
2. Normalize events to file identity, volume, operation, and observed content revision.
3. Append the event or an equivalent pending-state update to the VATfs WAL and persist it according to the configured durability policy.
4. Acknowledge ingestion internally only after the queue record is durable.
5. Schedule user-mode processing; return immediately to the notification consumer. **Never wait for embedding generation in a filesystem write path.**

**Crash window:** An OS may crash after a source file becomes durable but before VATfs receives or persists the notification. Because no cross-filesystem atomicity is assumed, the queue alone cannot close this gap. Detect journal discontinuities, missed notifications, offline intervals and unclean restart; then reconcile against the source volume. Never claim perfect event delivery across independent stores.

### Queue record (logical schema)

| Field | Meaning |
|---|---|
| `jobSequence` | Local monotonically increasing VATfs sequence |
| `fileKey` | Source identity plus quality descriptor |
| `observedRevision` | Content state believed current at ingestion |
| `operation` | Upsert, delete, rename/metadata, volume reconcile, rebuild |
| `priorityClass` | Interactive, user-requested, background, maintenance |
| `attempts`, `notBefore` | Retry and backoff |
| `modelVersion` | Required embedding pipeline version |
| `state` | Pending, leased, committed, tombstoned/failed |

### Coalescing and scheduling

- Coalesce repeated upserts by FileKey, retaining the newest observed revision. Preserve delete/tombstone ordering so a stale upsert cannot resurrect a removed file.
- Debounce rapid saves without suppressing eventual processing. When files are modified during embedding, discard the stale output and enqueue the latest revision.
- Queue is at-least-once; workers may repeat jobs. Publication is idempotent using `(FileKey, content revision, model version, chunk ID)`.
- A worker lease is ephemeral; on crash, the durable pending job is reclaimable.
- Do not require unbounded journal retention. Compact the WAL after a durable checkpoint.
- On queue-capacity exhaustion, mark the affected source volume as requiring a full reconciliation **durably**; do not block filesystem writes indefinitely or silently discard required work.

## 7. VATfs autonomous container format

VATfs is a **specialized vector allocation and index store**, not a general POSIX/NT filesystem. A binary container file and a dedicated block partition are interchangeable backends for the same abstract block I/O interface.

### Proposed regions

| Region | Contents |
|---|---|
| Superblock A/B | Magic, format version, store UUID, generation, block size, region roots, checksums |
| WAL | Transaction records, commit markers and bounded replay metadata |
| Persistent queue | Pending jobs, retry state, volume reconciliation flags |
| Allocation bitmap + extents | Logical blocks occupied by vector payloads and index pages |
| FileKey directory | B+tree or comparable mapping of source key to vector records |
| Vector arena | Packed FP16/INT8/other explicitly versioned encodings |
| Search index | Exact vector table first, versioned ANN structure later |
| Checkpoints | Published generation, applied jobs, per-volume scan state |
| Optional caches | Regenerable working data, never indispensable metadata |

### Storage requirements

- Specify disk endian, alignment, limits and overflow checks before implementation.
- Version all persistent structures and reject unsupported versions without corrupting storage.
- Checksum metadata blocks and validate extent bounds, object reference counts and tree cycles.
- Keep superblock A/B with generation markers and integrity validation; choose the newest fully committed root.
- Reserve and free extents only inside WAL-protected metadata operations.
- Write vector payloads before publishing the metadata root that references them; flush in the required order.
- Keep ANN index generations immutable while readers use them; publish a new generation atomically and reclaim old nodes only when no readers retain them.
- Enforce container quotas and maximum dimensions/vector counts to prevent resource exhaustion from malformed records.
- Detect unsupported physical-sector sizes and device errors. Do not assume all block backends implement identical flush semantics.

**WAL is VATfs-owned.** Do not embed JBD2 in the initial format: it would add ext4-specific recovery complexity despite the service being filesystem independent. Reusing well-tested journaling algorithms is possible later, but it must not alter this contract.

## 8. Recovery and synchronization protocol

### Startup recovery

1. Acquire exclusive writer ownership of the container.
2. Validate redundant superblocks, checksums and format version.
3. Replay committed WAL records only; discard incomplete transactions and orphaned payload extents.
4. Restore durable queue and checkpoints; expired worker leases return to pending.
5. Enumerate mounted source volumes and establish whether prior change streams are continuous.
6. Reconcile any volume with missing notifications, unknown identity quality, unclean shutdown or a stale checkpoint.
7. Mark search indexes available for verified generations; perform rebuild work in the background.

### Publication protocol for an embedding

1. Read source file under a valid user/service access context.
2. Capture a source revision; parse and compute embeddings outside kernel mode.
3. Reopen or revalidate file identity and revision before publication.
4. Write new payload extents and checksum them.
5. In a WAL transaction, add vector records, update index root and mark the queue job applied; persist commit metadata.
6. Publish the new index generation only after durable commit.
7. Reclaim superseded payloads after reader references expire.

If a crash occurs before step 5, the job remains retryable. If it occurs after step 5 but before the worker sees completion, replay is idempotent. Never separately persist the job ACK ahead of the vectors it acknowledges.

### Freshness semantics

- `CURRENT`: index record matches the source revision verified for the query.
- `STALE`: source is known to have changed; result may be omitted by strict queries or returned with an explicit stale flag under permitted policy.
- `UNKNOWN`: source cannot currently be opened or validated; omit from security-sensitive results.
- `INDEXING`: source exists but current vectors have not been produced.

Eventual consistency is acceptable; **authorization staleness is not**.

## 9. Public DLL and IPC contract

`vatfs.dll` exports MicaNT-specific APIs. Do not claim any of these names are Microsoft Windows exports. Implement a C ABI with opaque handles, fixed-width integer fields, explicit structure sizes and version negotiation; use idiomatic C++23 internally.

### Proposed exports

```c
VAT_STATUS VatFsOpenSession(const VAT_SESSION_OPTIONS* options,
                            VAT_SESSION_HANDLE* session);
VAT_STATUS VatFsSearch(VAT_SESSION_HANDLE session,
                       const VAT_SEARCH_REQUEST* request,
                       VAT_SEARCH_HANDLE* search);
VAT_STATUS VatFsNextResult(VAT_SEARCH_HANDLE search,
                           VAT_SEARCH_RESULT* result);
VAT_STATUS VatFsCancelRequest(VAT_SEARCH_HANDLE search);
VAT_STATUS VatFsCloseSearch(VAT_SEARCH_HANDLE search);
VAT_STATUS VatFsRequestIndex(VAT_SESSION_HANDLE session,
                             const VAT_INDEX_REQUEST* request);
VAT_STATUS VatFsGetIndexStatus(VAT_SESSION_HANDLE session,
                               VAT_INDEX_STATUS* status);
VAT_STATUS VatFsCloseSession(VAT_SESSION_HANDLE session);
```

The names and types above are **proposed**, not fixed ABI. Define calling convention, character encoding (prefer UTF-16 for Windows-facing paths), output ownership, status mapping, and synchronous/asynchronous forms before freezing v1.

### IPC protocol

- Use MicaNT's canonical user-mode IPC transport (LPC/ALPC-equivalent when implemented), not a private kernel syscall.
- Include protocol version, message length, operation, request ID and bounded serialization in each message.
- Validate lengths and handles on both sides; never pass raw client pointers to the service.
- Impersonate or securely bind the requesting user when validating file access. An elevated indexer must not return privileged data to an unprivileged caller.
- Support paging/batching of results, cancellation and request deadlines.
- The client DLL must fail quickly with a clear service-unavailable status if the service is stopped; Explorer and the shell must remain functional.
- Enforce per-client memory, request-rate and concurrent-search quotas.

### Result model

Each search hit includes a source identity token, similarity/ranking score, model/index generation, freshness status and optional safe display metadata. The service resolves or reopens source files using their current identity and access rights. Do not treat cached paths, snippets or scores as authorization evidence.

## 10. Search semantics

Initial query engine:

1. Path/name and metadata filtering through ordinary source filesystem APIs.
2. Exact cosine or dot-product similarity over normalized embeddings for a bounded corpus.
3. Hybrid lexical/vector ranking when a reliable lexical index becomes available.
4. HNSW or disk-oriented ANN only after exact-search correctness, recall and persistence are measured.

Search is scoped by volume, root directory, caller identity, optional file type and freshness policy. Filtering by directory must use verified ancestry/path resolution; FileKey lookup alone does not prove that a file is underneath the requested directory.

Proposed shell UX:

```sh
ls --search "documenti sulla memoria virtuale"
search ./ "fotografie con montagne innevate"
search C:\\Projects "thread scheduler AMD64" --type md
```

These are **shell extensions**, not modifications to `NtQueryDirectoryFile` semantics. A shell may return filenames immediately and append semantic-ranked results when available; a non-running VATfs service must not prevent ordinary `ls`.

## 11. AI inference and accelerator use

The first usable backend is CPU AMD64, with portable correctness and optional AVX2/AVX-512 dispatch after CPUID/XCR0 checks. Keep the model and tokenization runtime in user mode. Do not assume an NPU exists or that every model maps efficiently to one.

- Support at least one explicitly selected embedding model with stable model ID, dimension, normalization and tokenizer version.
- Model upgrades create a new index namespace/generation; do not compare vectors from incompatible embedding spaces.
- Reuse the MicaNT AI runtime and Compute Manager where implemented; avoid creating a second driver or memory-management stack.
- GPU/NPU jobs are asynchronous, quota-limited and cancellable where the hardware allows. Handle device disappearance, timeout and reset by falling back to CPU or rescheduling the job.
- Keep interactive query latency separate from background embedding workload. No real-time NT thread priorities merely for indexing.
- Run untrusted document parsers in lower-privilege sandboxed workers where supported; an invalid PDF or image must not crash the service or kernel.

## 12. Security, privacy and resource controls

- All indexes and vector payloads remain local by default; no implicit cloud uploads or telemetry.
- Define per-user or per-security-domain index namespaces and permissions. A shared physical store is permitted only with strict authorization on every query and maintenance operation.
- Protect the container with filesystem permissions and, where available, encryption. Embeddings and filenames can reveal private information.
- Do not index credentials, key stores or protected system areas by default; allow policy-based exclusions.
- Do not trust cached ACLs after file replacement, rename, delete or security descriptor changes.
- Enforce quotas for WAL, queue, vector arena, caches, model memory, CPU time and accelerator memory.
- Apply bounded parsing to files, on-disk structures and IPC. Log faults without logging source-document content.
- Permit explicit pause, reindex, delete-index and full-store-reset operations without touching source files.

## 13. Failure matrix

| Failure | Required behavior |
|---|---|
| `VATfsSvc.exe` absent | Filesystem and desktop continue; DLL reports service unavailable |
| Queue replay after crash | Reclaim expired jobs and deduplicate by FileKey + revision + model |
| Filesystem change missed | Detect gap/offline state and rescan relevant volume |
| Source file removed | Tombstone old vectors; do not return stale protected content |
| File ID reused | Check generation/revision and reject mismatches |
| Source file changes during embedding | Discard generated vectors and reschedule latest version |
| Container metadata corrupt | Recover latest validated root or rebuild from source; never overwrite source files |
| Container full | Persist dirty/reconcile marker; throttle indexing, not source writes |
| Model version changes | Build new namespace and atomically switch published index generation |
| NPU/GPU unavailable | Fall back to CPU, defer or return explicit compute-unavailable state |
| ACL changes | Revalidate at search result resolution; never rely solely on indexed ACLs |
| Volume offline | Mark results unavailable and schedule reconciliation on remount |

## 14. Implementation milestones

### VAT0 — Build and architecture foundation

**Deliverable:** real user-mode `vatfs.dll` and `VATfsSvc.exe` targets, versioned IPC contract, client/service lifecycle and service-unavailable error handling. No fake successful search API.

**Acceptance:** client communicates with service, can cancel requests, correctly rejects bad versions and malformed messages; no `micant/km/` header leaks into DLL.

### VAT1 — Persistent autonomous container

**Deliverable:** superblock A/B, block allocator, WAL, queue, checkpoint and recovery over a regular container file.

**Acceptance:** after forced process termination at every write/commit boundary, reopen shows either the old or new complete generation, with no double allocation and with queue replay intact.

### VAT2 — Source identity and reconciliation

**Deliverable:** filesystem-independent FileKey adapters, notifications, persistent pending jobs, coalescing and recovery scans.

**Acceptance:** create/update/rename/delete events across at least two source filesystem drivers preserve identity where supported; missed events are recovered by rescan.

### VAT3 — CPU embeddings and exact search

**Deliverable:** a real locally executed embedding model, extracted text for a limited supported format, vector serialization, exact top-k similarity and stale-version filtering.

**Acceptance:** query returns a semantically related document without depending on an exact filename match. Restart preserves results; model/version mismatch never yields false-current hits.

### VAT4 — Shell and Explorer integration

**Deliverable:** `ls --search`, `search ./`, result paging, cancellation, scope and current-permission validation.

**Acceptance:** user lacking read access receives no protected result, even if the index was populated by a privileged service. Service outage leaves normal directory listings usable.

### VAT5 — Optimized ANN and storage

**Deliverable:** persistent HNSW or measured disk-based ANN, atomic index generation publication, compaction and cache tuning.

**Acceptance:** crash-safe updates/deletes, recall measurements against exact top-k reference, and bounded memory under a large corpus.

### VAT6 — GPU/NPU acceleration

**Deliverable:** shared Compute Manager backend for indexing; CPU fallback and scheduler-aware throttling.

**Acceptance:** numerical output agrees with the CPU reference within documented tolerances; device reset or loss does not corrupt VATfs or hang ordinary filesystem I/O.

### VAT7 — Optional dedicated partition

**Deliverable:** raw-partition block backend with the **same** container format, recovery and v1 DLL ABI.

**Acceptance:** migration/restore between file-container and partition-container backends preserves FileKeys, data integrity, indexed generations and pending queue state.

## 15. Test and validation requirements

- **Hosted unit tests:** block allocator, on-disk parser, WAL replay, B+tree keys, queue coalescing, idempotence, index generation, vector encoding and access-control hooks.
- **Fault injection:** crash after each persistence boundary; corrupt checksum, truncated writes, duplicate jobs, full container, stale file identities and partial ANN generation.
- **Integration QEMU AMD64:** actual filesystem files and Ring 3 service process, IPC, model loading, source-file mutation, restart, persistent replay and real semantic query.
- **Performance:** index write throughput, embedding latency, query p50/p95, CPU/NPU utilization, memory footprint and UI responsiveness while indexing. Never publish claimed benchmarks without measured evidence.
- **Security:** cross-user query leakage, file-ID reuse, malicious model metadata, malformed IPC and privilege boundaries.
- **Compatibility:** `vatfs.dll` MicaNT extensions do not masquerade as official Windows APIs, and do not alter existing NT file enumeration semantics.

Separate **implemented**, **compiled**, **linked**, **service executed**, **QEMU verified**, and **supported hardware verified** in reports. VATfs is not complete when only the headers, simulated results or hosted fixtures exist.

## 16. Dependency and execution policy

VATfs must not block completion of the canonical NT kernel, general process/thread lifecycle, Io/FS drivers, real file-backed mapping or Ring 3 service startup. Deliver VAT0–VAT3 only when the necessary primitives are available, or develop standalone testable components without claiming native integration.

Implement in coherent end-to-end batches. Reuse MicaNT's existing canonical service architecture and its shared scheduling/AI/runtime facilities. Do not introduce kernel-mode vector parsers or an incompatible second filesystem/object manager.

The current design decision is **one autonomous VATfs container and background service, independent from all source filesystem journals, with eventual consistency and recoverable indexing**. Changes to this contract require a dedicated architecture decision record rather than an implicit implementation shortcut.
