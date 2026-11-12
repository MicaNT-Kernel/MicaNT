| Milestone | Completamento | Chiuso e verificato | Mancante |
|---|---|---|---|
| M1 — Section mapping | Accettazione bootstrap verificata | Section anonima condivisa, handle Ob e cleanup in QEMU. | Contratto generale delle section e casi non supportati. |
| M2 — Address space | Accettazione bootstrap verificata | Due processi Ps con CR3, pagine private, section condivisa, cleanup indipendente e rollback PFN in QEMU. | Creazione e terminazione generali dei processi. |
| M3 — Scheduling preemptive | Parziale | Clock BSP persistente e preemption kernel/CPL3, priorità al tick e block/wake verificati in QEMU. | Calibrazione, dispatch immediato, IRQL, routing IRQ e stato esteso. |
| M4 — Processi e thread | Parziale | Lifecycle Ps e terminazione di tre thread bloccati su eventi non segnalati verificati in QEMU. | Oggetti attendibili Ob, creazione nativa e terminazione di altri stati di thread. |
| M5 — Syscall NT | Parziale | Section, close, memoria, eventi con attesa bloccante e terminazione limitata esercitati da CPL3. | Creazione processo/thread, timeout/APC, terminazione generale e copertura restante. |
| M6 — Processo PE Ring 3 | Parziale | Loader Ps, viste section Ob/Mm, import NTDLL, PEB minima e thread CPL3 verificati in QEMU. | Semantica `SEC_IMAGE` file-backed, dati PEB e startup PE/NTDLL completi. |
| M7 — Due thread, TLS e sincronizzazione | Parziale | Due thread Ps CPL3, TLS PE di due moduli, import NTDLL, eventi e status verificati in QEMU. | Callback TLS, creazione nativa thread e startup completo. |
