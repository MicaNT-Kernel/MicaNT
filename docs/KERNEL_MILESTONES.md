| Milestone | Completamento | Chiuso e verificato | Mancante |
|---|---|---|---|
| M1 — Section mapping | Accettazione bootstrap verificata | Section anonima condivisa, handle Ob e cleanup in QEMU. | Contratto generale delle section e casi non supportati. |
| M2 — Address space | Accettazione bootstrap verificata | Due processi Ps con CR3, pagine private, section condivisa, cleanup indipendente e rollback PFN in QEMU. | Creazione e terminazione generali dei processi. |
| M3 — Scheduling preemptive | Parziale | Timer preemptive su contesti kernel e CPL3, priorità al tick e block/wake verificati in QEMU. | Dispatch immediato, preemption ordinaria, IRQL completo e stato esteso. |
| M4 — Processi e thread | Parziale | Lifecycle Ps, stack kernel/TSS e terminazione limitata a due thread verificati in QEMU. | Oggetti attendibili Ob, creazione nativa e terminazione multi-thread generale. |
| M5 — Syscall NT | Parziale | Section, close, memoria, eventi con attesa bloccante e terminazione limitata esercitati da CPL3. | Creazione processo/thread, timeout/APC, terminazione generale e copertura restante. |
| M6 — Processo PE Ring 3 | Parziale | Loader Ps, viste section Ob/Mm, import NTDLL, PEB minima e thread CPL3 verificati in QEMU. | Semantica `SEC_IMAGE` file-backed, dati PEB e startup PE/NTDLL completi. |
| M7 — Due thread, TLS e sincronizzazione | Parziale | Due thread Ps CPL3, TLS PE distinti, import NTDLL, eventi e status verificati in QEMU. | Inizializzazione TLS completa nel loader, creazione nativa thread e startup completo. |
