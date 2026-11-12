| Milestone | Completamento | Chiuso e verificato | Mancante |
|---|---|---|---|
| M1 — Section mapping | Accettazione bootstrap verificata | Section anonima condivisa, handle Ob e cleanup in QEMU. | Contratto generale delle section e casi non supportati. |
| M2 — Address space | Accettazione bootstrap verificata | Due processi Ps con CR3, pagine private, section condivisa, cleanup indipendente e rollback PFN in QEMU. | Creazione e terminazione generali dei processi. |
| M3 — Scheduling preemptive | Parziale | Build e boot QEMU: preemption, idle, stack boot e configurazione IRQ PS/2; aggiunti dispatch, DPC, APC kernel corrente, x2APIC e x87/SSE/AVX-512. | Verifica specifica di IRQ, DPC/APC, x2APIC e AVX-512; APC tra thread e user, altre sorgenti IRQ e stato esteso aggiuntivo. |
| M4 — Processi e thread | Parziale | Lifecycle Ps/Ob con handle processo, cleanup differito, terminazione di thread in attesa e coda APC verificati in QEMU. | Oggetti attendibili Ob, creazione nativa, terminazione di altri processi e rundown APC. |
| M5 — Syscall NT | Parziale | Section, close, memoria, eventi con attesa bloccante e terminazione limitata esercitati da CPL3. | Creazione processo/thread, timeout/APC, terminazione generale e copertura restante. |
| M6 — Processo PE Ring 3 | Parziale | Loader Ps, section Ob/Mm, import NTDLL, percorsi moduli e parametri essenziali PEB verificati in QEMU. | `SEC_IMAGE` file-backed, parametri completi e startup completo. |
| M7 — Due thread, TLS e sincronizzazione | Parziale | Due thread Ps CPL3, TLS PE di due moduli, import NTDLL, eventi e status verificati in QEMU. | Callback TLS, creazione nativa thread e startup completo. |
