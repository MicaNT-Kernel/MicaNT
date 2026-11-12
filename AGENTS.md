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
- Write code in clear, idiomatic C++23 that a human programmer can read and maintain. Prefer well-named functions and focused units over dense, monolithic, or opaque code blocks. Add concise comments to explain intent, non-obvious logic, and important constraints; do not use comments merely to restate the code.
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

# MicaNT — Continuous Execution Protocol

## Scopo

Questo protocollo governa il comportamento dell'agente durante incarichi che richiedono il completamento di una o più milestone.

Il repository contiene un kernel NT modulare C++23 esclusivamente AMD64. Le milestone correnti sono definite in `docs/KERNEL_MILESTONES.md`.

**Un batch completato non equivale a un incarico completato.**

## 1. Priorità delle istruzioni operative

Le richieste esplicite dell'utente per il mandato corrente prevalgono sulle preferenze generiche del workflow del repository, purché non violino vincoli tecnici o di sicurezza inderogabili.

In particolare:

- Se l'utente dice di non creare commit, non creare commit.
- Se l'utente dice di non eseguire verifiche durante il batch, non lanciare build, test, QEMU, sanitizzatori o altri controlli eseguibili durante quel batch.
- Non dichiarare verificato codice che non è stato verificato.
- Se la richiesta è di implementare, non sostituire l'implementazione con un rapporto.
- Non terminare volontariamente il mandato per applicare una consuetudine di reporting.
- Non ripetere una richiesta di autorizzazione per passaggi già inclusi nel mandato.

Una verifica differita deve essere segnalata come tale, non sostituita con una verifica fittizia.

## 2. Gestione della milestone

All'inizio dell'incarico:

1. Leggi il codice direttamente pertinente.
2. Leggi `docs/KERNEL_MILESTONES.md`.
3. Identifica i requisiti incompleti della milestone richiesta.
4. Ordinali per dipendenze tecniche.
5. Inizia immediatamente il primo requisito realizzabile.

Non creare una nuova roadmap generale.

Non ripetere un audit completo del repository.

La milestone corrente rimane attiva finché tutti i suoi criteri di accettazione non sono soddisfatti o finché interviene un vincolo reale che impedisce ogni ulteriore progresso.

## 3. Ciclo di lavoro continuo

Per ogni sottotask:

**IMPLEMENT → INTEGRATE → UPDATE STATE → NEXT TASK**

Questo ciclo non termina con un resoconto.

Dopo ogni intervento:

- Aggiorna il codice canonico.
- Completa i caller e i percorsi dipendenti necessari.
- Gestisci ownership, errori e cleanup.
- Registra mentalmente o nel documento di stato ciò che rimane aperto.
- Passa immediatamente al prossimo sottotask.

Non considerare un commit, un test QEMU passato o un singolo bugfix come un motivo per concludere la sessione.

Non utilizzare la complessità della milestone come motivo generico di arresto.

## 4. Condizioni di arresto

È consentito concludere volontariamente il mandato soltanto quando:

A. Tutti i criteri richiesti sono implementati, indicando separatamente le verifiche effettuate e quelle differite.

B. Esiste un blocco tecnico concreto, riproducibile e non risolvibile con le risorse disponibili, e nessun altro sottotask indipendente può proseguire.

C. Serve una decisione dell'utente che non può essere ricavata dalle specifiche o dalle istruzioni già fornite.

D. L'ambiente di esecuzione impone realmente la fine della sessione o non consente ulteriori operazioni.

**Non sono condizioni di arresto:**

- Un file modificato.
- Una funzione completata.
- Un test passato.
- Un commit creato.
- Un errore di compilazione ordinario.
- Una dipendenza ancora da implementare.
- Un problema complesso ma analizzabile.
- La presenza di test da scrivere.
- La necessità di leggere ReactOS.
- Il raggiungimento di un checkpoint interno.
- Il fatto di poter elencare il prossimo passo.

Se esiste un prossimo intervento concreto e fattibile, continua.

## 5. Gestione dei blocker

Quando incontri un problema:

1. Identifica precisamente la funzione, il contratto o l'invariante interessato.
2. Studia il codice MicaNT coinvolto.
3. Consulta ReactOS e Microsoft WDK quando necessario.
4. Individua la dipendenza minima.
5. Implementa la soluzione nel sottosistema canonico.
6. Completa i caller e i cleanup.
7. Riprendi la milestone.

Se il problema non può essere risolto subito, affronta un sottotask indipendente.

Non usare genericamente espressioni come "richiede attenzione", "è complesso" o "serve ulteriore studio" come giustificazione per fermarti.

## 6. Modalità NO-COMMIT / NO-TEST

Quando richiesta dall'utente:

- Mantieni tutte le modifiche nel worktree.
- Non creare commit Git, neppure WIP.
- Non lanciare QEMU.
- Non avviare build o test.
- Non dichiarare superate verifiche non eseguite.
- Continua comunque a leggere, modificare e integrare il codice.
- Conserva i test esistenti.
- Non disabilitare controlli per aggirare problemi.
- Annota le verifiche che saranno necessarie alla fine del mandato.

Questa modalità non giustifica un arresto anticipato.

Quando l'utente autorizzerà le verifiche, esegui i controlli pertinenti all'intero batch, evitando test ripetitivi.


## 9. Reporting senza interruzione

Puoi inviare brevi aggiornamenti sul lavoro in corso, ma non trattarli come rapporti conclusivi.

Un checkpoint deve indicare:

- Cosa è stato implementato.
- Cosa rimane.
- Quale sottotask viene affrontato successivamente.

Dopo il checkpoint continua a lavorare, se l'ambiente lo consente.

Il rapporto finale è riservato al completamento effettivo del mandato o a un arresto imposto da una delle condizioni della sezione 4.

## 10. Recupero della sessione

Se la sessione termina per limiti dell'ambiente, lascia uno stato recuperabile:

- Milestone corrente.
- Ultimo sottotask completato.
- Sottotask in corso.
- File modificati.
- Modifiche locali da preservare.
- Test eseguiti o differiti.
- Primo intervento concreto da riprendere.

Non descrivere come completata una milestone parziale.

Alla sessione successiva riparti direttamente dal sottotask in corso.

## Regola conclusiva

**Il mandato dell'utente è l'unità di completamento. Il batch è soltanto un'unità interna di organizzazione.**

Non fermarti dopo il primo miglioramento.

Non fermarti perché hai individuato il prossimo problema.

Non fermarti perché hai scritto un resoconto.

Continua a integrare il kernel canonico finché il mandato può effettivamente progredire.


## Single modular kernel policy

MicaNT has one canonical modular kernel. All kernel subsystem development must target the modular `ntoskrnl_core` architecture and its bare-metal entry point. The archived monolithic kernel under `old/` is historical reference only and must never be built, linked, staged, or used to satisfy missing symbols.

## Evidence and status

Use the Microsoft Windows contract as the compatibility requirement and official `win32metadata` plus Microsoft Learn/WDK as primary sources. Record unresolved contract details rather than guessing. Keep implementation status separate from declarations, compilation, host tests, QEMU results, and compatibility claims; use `docs/KERNEL_ARCHITECTURE.md` and the integration status documents as the evidence source.
