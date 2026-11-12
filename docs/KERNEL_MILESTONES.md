| Milestone | Completamento | Chiuso e verificato | Mancante |
|---|---|---|---|
| M1 — Section mapping | Accettazione bootstrap verificata | Section anonima condivisa, handle Ob e cleanup in QEMU. | Contratto generale delle section e casi non supportati. |
| M2 — Address space | Accettazione bootstrap verificata | Due processi Ps con CR3, pagine private, section condivisa, cleanup indipendente e rollback PFN in QEMU. | Creazione e terminazione generali dei processi. |
| M3 — Scheduling preemptive | Parziale | Timer preemptive, selezione per priorità al tick e block/wake Ke verificati in diagnostici QEMU. | Dispatch immediato, preemption ordinaria, IRQL completo, contesto CPL3 e stato esteso. |
| M4 — Processi e thread | Parziale | Lifecycle Ps, stack kernel/TSS e terminazione limitata a due thread verificati in QEMU. | Oggetti attendibili Ob, creazione nativa e terminazione multi-thread generale. |
| M5 — Syscall NT | Parziale | Section, close, memoria, eventi con attesa bloccante e terminazione limitata esercitati da CPL3. | Creazione processo/thread, timeout/APC, terminazione generale e copertura restante. |
| M6 — Processo PE Ring 3 | Parziale | Fixture PE in processo Ps: entry CPL3, TEB minima, allocazione e terminazione verificati. | Image section, loader di processo, NTDLL, PEB e startup PE integrati. |
| M7 — Due thread, TLS e sincronizzazione | Parziale | Due thread CPL3, template TLS PE distinti, NTDLL, eventi bloccanti e status verificati in QEMU. | Inizializzazione TLS nel loader, creazione nativa thread e startup completo. |
