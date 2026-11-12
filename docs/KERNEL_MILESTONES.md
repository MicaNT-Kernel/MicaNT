| Milestone | Completamento | Chiuso e verificato | Mancante |
|---|---|---|---|
| M1 — Section mapping | Accettazione bootstrap verificata | Section anonima condivisa, handle Ob e cleanup in QEMU. | Contratto generale delle section e casi non supportati. |
| M2 — Address space | Accettazione bootstrap verificata | Due processi Ps con CR3, pagine private, section condivisa, cleanup indipendente e rollback PFN in QEMU. | Creazione e terminazione generali dei processi. |
| M3 — Scheduling preemptive | Parziale | Timer preemptive e block/wake Ke verificati in diagnostici QEMU. | Preemption ordinaria, IRQL completo, contesto CPL3 e stato esteso. |
| M4 — Processi e thread | Parziale | Lifecycle limitato dei thread kernel Ps e rilascio stack verificati in QEMU. | Oggetti attendibili Ob, creazione e terminazione native di processi e thread. |
| M5 — Syscall NT | Parziale | Section, close e allocazione/rilascio della memoria corrente esercitati da CPL3. | Servizi di processo, thread, attesa, sincronizzazione e terminazione. |
| M6 — Processo PE Ring 3 | Non completata | Parser PE e protezione RX esercitati separatamente. | Image section, loader di processo, PEB/TEB, primo thread e terminazione integrati. |
| M7 — Due thread, TLS e sincronizzazione | Non completata | Primitiva Ke block/wake kernel verificata. | Due thread utente reali, TLS indipendente, evento e status di uscita verificabile. |
