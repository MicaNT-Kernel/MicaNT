| Milestone | Completamento | Chiuso e verificato | Mancante |
|---|---|---|---|
| M1 — Section mapping | Accettazione bootstrap verificata | Section anonima condivisa, handle Ob e cleanup in QEMU. | Contratto generale delle section e casi non supportati. |
| M2 — Address space | Accettazione bootstrap verificata | Due processi Ps con CR3, pagine private, section condivisa, cleanup indipendente e rollback PFN in QEMU. | Creazione e terminazione generali dei processi. |
| M3 — Scheduling preemptive | Parziale | Timer preemptive, selezione per priorità al tick e block/wake Ke verificati in diagnostici QEMU. | Dispatch immediato, preemption ordinaria, IRQL completo, contesto CPL3 e stato esteso. |
| M4 — Processi e thread | Parziale | Lifecycle Ps limitato, terminazione del processo corrente con un thread, cleanup e stack kernel/TSS verificati in QEMU. | Oggetti attendibili Ob, creazione e terminazione multi-thread, ingresso syscall da thread utente. |
| M5 — Syscall NT | Parziale | Section, close, memoria, eventi e terminazione del processo corrente con un thread esercitati da CPL3. | Creazione processo/thread, attesa bloccante, terminazione multi-thread e copertura generale. |
| M6 — Processo PE Ring 3 | Parziale | Fixture PE in processo Ps: entry CPL3, TEB minima, allocazione e terminazione verificati. | Image section, loader di processo, NTDLL, PEB e startup PE integrati. |
| M7 — Due thread, TLS e sincronizzazione | Non completata | Evento Ob/Ke, polling NT CPL3 e accesso GS a uno slot TEB verificati in QEMU. | Due thread utente reali, TLS indipendente, attesa bloccante e status di uscita verificabile. |
