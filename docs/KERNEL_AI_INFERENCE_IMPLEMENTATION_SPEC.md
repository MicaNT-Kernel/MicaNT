# MicaNT — Native AI Inference and Accelerator Compute Specification

**Documento:** `docs/KERNEL_AI_INFERENCE_IMPLEMENTATION_SPEC.md`  
**Stato:** proposta di implementazione; nessuna capacità qui descritta è implicitamente certificata  
**Piattaforma:** MicaNT NT-compatible, **AMD64/x86_64 esclusivamente**, UEFI, C++23  
**Ambito:** inferenza locale, CPU/GPU/NPU, gestione kernel degli acceleratori, integrazione con Mm/Ob/Io/Ke/Ps  
**Priorità:** subordinata alla stabilizzazione dei processi Ring 3, delle section file-backed e dello storage reale

## 1. Obiettivi e confini

MicaNT deve poter eseguire **inferenza reale e locale** usando inizialmente la CPU x64 e, in fasi successive, GPU e NPU. Il kernel fornisce soltanto accesso controllato alle risorse hardware e ai servizi NT; **il parsing dei modelli, il grafo degli operatori, la tokenizzazione, l'esecuzione degli operatori e il ciclo di inferenza restano in Ring 3**.

Obiettivi verificabili:

1. Avviare un programma PE32+ user-mode che carica un modello da un file reale.
2. Eseguire una prima inferenza numerica corretta **senza servizi hosted**, senza rete e senza elaborazione in Ring 0.
3. Consentire un backend CPU di riferimento deterministico, poi backend SIMD AVX2 e, solo quando disponibili e correttamente salvati da Ke, AVX-512/altre estensioni.
4. Condividere pagine di pesi read-only mediante le section canoniche Mm, mantenendo buffer di attivazioni privati e quote per processo.
5. Esporre i dispositivi acceleratori tramite Io/Ob, con code, buffer, fence, cancellazione e cleanup corretti.
6. Ridurre le interferenze con input, compositor e thread interattivi mediante la politica di scheduling NT già definita.
7. Impiegare API di compatibilità Windows soltanto quando se ne implementa realmente il contratto; mantenere versionate e private le API specifiche MicaNT.

Fuori ambito nella prima versione: training in kernel, microservizio AI obbligatorio, daemon cloud, interfacce pubbliche Windows inventate, port diretto dell'intero DRM Linux, esecuzione kernel di modelli ONNX/GGUF, supporto simultaneo a tutte le NPU e garanzie real-time generali.

## 2. Baseline del repository e audit obbligatorio

Al momento dell'analisi risultano presenti:

- `include/micant/km/drivers/compute/npu.hpp`: interfacce e simulazioni TitanNPU/NexusNPU (code MCDM-like, descrittori di memoria, precisioni e operatori). Include attualmente dipendenze **hosted e user-mode** (`std::mutex`, `std::vector`, `std::shared_ptr`, `micant/um/...`); non è dimostrato un driver hardware freestanding.
- `include/micant/um/subsystems/graphics/directml.hpp`: interfaccia di livello user-mode, non prova di accelerazione effettiva.
- `kernel/io/prismx_directml_superres.cpp` e componenti PrismX: codice da verificare; non usare la loro presenza come prova di inferenza GPU.
- Kernel modulare con primitive PFN, VAD, page table, Ring 3 e scheduler in integrazione; la disponibilità generale di file-backed sections, processi e driver I/O deve essere verificata sulla revisione corrente.

Il primo incremento è un **audit mirato**, non una riscrittura: mappare ciò che può essere riutilizzato da `npu.hpp`, rimuovere dal percorso Ring 0 i riferimenti diretti a SCM, LDR e VersionDatabase user-mode, e identificare tutte le capacità simulate o che restituiscono dati fissi. Nessuna classe TitanNPU va collegata al boot solo per poter dichiarare il dispositivo presente.

Le capacità si classificano sempre come: **sorgente presente / compilato / linkato / inizializzato / eseguito con hardware reale / verificato funzionalmente**.

## 3. Architettura a livelli

```text
Applicazioni MicaNT — assistente locale, OCR, voice, LLM, vision
  |
  +-- Runtime Ring 3 (AI Runtime)
  |     - parser sicuro ONNX o GGUF, shape/type validation
  |     - graph planner, tensor allocator, CPU inference
  |     - optional ONNX Runtime EP / llama.cpp port
  |     - optional Vulkan compute / hardware NPU provider
  |
  +-- NTDLL + API native NT + device handles
         |
         v
Ring 0: MicaNT Compute Device Manager (piccolo, senza operatori ML)
  - Ob: handles/refcount/access masks
  - Ps/Se: process ownership e limiti
  - Mm: pinning, mapping e buffer ownership
  - Io: device objects, IRP, cancellazione, completamenti
  - Ke: code kernel, waitable fence, interrupt/DPC
  - Hal: PCI, MSI/MSI-X, MMIO, DMA/IOMMU, firmware
         |
         +-- CPU: nessun driver accelerator necessario
         +-- GPU: miniport GPU canonico MicaNT
         +-- NPU: miniport Intel/AMD per una famiglia alla volta
```

Regole architetturali:

- L'esecuzione **CPU** è interamente user-mode: scheduler e Memory Manager esistenti bastano; non creare un percorso syscall per ogni GEMM.
- Il Compute Device Manager non replica `Mm`, `Ke`, `Io`, `Ob`, il GPU Memory Manager o la gestione dei processi.
- Una chiamata kernel valida comporta **submission, mapping, wait, cancel o device control**, non parsing di un intero modello.
- La stessa sessione può selezionare CPU, GPU o NPU tramite backend user-mode; il kernel non deve interpretare grafi ML generici.
- Nessuna dipendenza da host POSIX, librerie Linux o servizi Windows user-mode nel target freestanding.

## 4. API e ABI interne

### 4.1 Interfaccia user-mode del runtime

Progettare una libreria **privata MicaNT**; i nomi seguenti sono concettuali, non esportazioni Windows dichiarate:

| Operazione privata | Risultato / contratto |
|---|---|
| `OpenModel(path, options)` | Modello verificato, errore esplicito, nessuna allocazione persistente su failure |
| `CreateInferenceSession(model, backend)` | Sessione isolata con quota di memoria e thread limitati |
| `BindInput(index, tensor)` | Controllo di tipo, rank, shape, stride e lunghezza |
| `Run(session)` | Esecuzione sincrona con timeout/cancel espliciti oppure job asincrono |
| `Submit(session, inputs)` | Handle del job; nessun buffer user assunto stabile dopo la chiamata |
| `Wait(job)` / `Cancel(job)` | Risultato, cancellazione idempotente, nessun lavoro abbandonato |
| `ReadOutput(job)` | Copia oppure vista sicura, con ownership definita |
| `Close(...)` | Cleanup deterministico anche dopo errori e terminazione del processo |

Non pubblicare queste API come `Nt*`, `Dml*` o `Windows.AI.*` finché non esiste il relativo contratto Windows verificato. Se serve un'interfaccia Kernel32/NTDLL, usarne le entrypoint effettivamente documentate per aprire dispositivi, mappare memoria e attendere oggetti.

### 4.2 Device protocol kernel (solo acceleratori)

Definire una struttura **POD versionata** con `Size`, `Version`, `Flags`, `RequestId` e campi espliciti; non trasferire `std::vector`, puntatori a classi C++ o smart pointer attraverso Ring 0/Ring 3.

Operazioni minime: `QueryCapabilities`, `CreateContext`, `AllocateBuffer`, `ImportBuffer`, `MapBuffer`, `SubmitCommand`, `WaitFence`, `CancelSubmission`, `DestroyContext`.

Contratti inderogabili:

- Validazione delle lunghezze e overflow (`count * stride`, offset+length, allineamento, limite di dimensione).
- Access rights sul device e ownership del processo per **ogni** operazione.
- Riferimenti trattenuti da job e fence finché l'hardware può accedere alla memoria.
- Buffer DMA pinnati prima della submission, mappati in IOMMU quando disponibile, rilasciati dopo completamento/cancellazione **effettiva**.
- Handle non riutilizzabili in modo da consentire accesso a un contesto precedente.
- Timeout e device loss con stato esplicito; nessuna finta risposta `STATUS_SUCCESS`.
- Compatibilità solo AMD64; per altri processori/device dichiarare `NotSupported`, non emularne la presenza.

## 5. Motore CPU Ring 3 — prima milestone operativa

### 5.1 Backend di riferimento

Implementare un motore tensoriale **scalare** prima dei percorsi SIMD: `MatMul/GEMM`, add, mul, reshape/transpose (quando necessario), RMSNorm/LayerNorm, Softmax, embedding lookup e primitive di attention richieste dal primo modello. Non promettere copertura ONNX completa: dichiarare il subset di operatori e opset realmente supportato.

Invarianti numerici:

- Validazione delle shape prima dell'allocazione, inclusi casi zero e dimensioni avversariali.
- Gestione NaN/Inf e di accumulazioni numeriche secondo la precisione dichiarata.
- Riferimento CPU scalare deterministico per test di accuratezza.
- FP32 iniziale; INT8/INT4 e FP16/BF16 soltanto dopo definizione precisa di packing, scale, zero-point e accumulazione.
- Limite per numero di thread, tensor bytes e scratch bytes; niente `malloc` non controllate su input non affidabile.

### 5.2 SIMD x64

Implementare dispatch runtime con CPUID + XGETBV; usare AVX2 soltanto quando CPU e OS supportano salvataggio/ripristino dello stato necessario. AVX-512 richiede verifica di XCR0 e dell'effettivo supporto di Ke su **ogni** context switch e interrupt path. Niente esecuzione di istruzioni non supportate: fallback scalare/SSE/AVX2 dove pertinente.

Ottimizzazioni progressive: blocking della GEMM, packing dei pesi, SIMD su blocchi contigui, prefetch misurato, thread pools bounded e quantizzazione. Non usare un solo benchmark sintetico come misura della qualità di un LLM.

### 5.3 Prima prova end-to-end

Avviare in MicaNT Ring 3 un PE32+ che legge da un file reale un piccolo modello con output noto e completa un'inferenza CPU. Testare anche due sessioni, teardown, file malformato, memoria insufficiente e cancellazione.

**Criterio di successo AI1:** risultato numerico entro tolleranza definita, nessun intervento di Ring 0 durante i singoli operatori, nessuna fuga di handle/pagine dopo la chiusura, desktop/input ancora responsive.

## 6. Modelli e filesystem

Ordine di integrazione:

1. File object reale da Io / filesystem, con `NtCreateFile`, `NtReadFile` e close nei limiti delle API realmente implementate.
2. Section **data file-backed** in Mm per pesi read-only; distinguere da `SEC_IMAGE`, che serve al loader PE e ha semantica differente. Non usare `SEC_IMAGE` per mappare file GGUF/ONNX di dati.
3. Parser user-mode streaming o bounded: verificare header, versione, directory tensor, offset, allineamento, checksum quando il formato lo prevede e bounds di ogni tensore.
4. L'owner di ogni vista dei pesi è un oggetto di sessione; la section rimane viva finché esiste una vista e i job hanno riferimenti validi.
5. Condivisione tra processi **solo read-only e con politiche di accesso esplicite**; buffer di attivazioni e KV cache sempre per-sessione salvo un contratto diverso deliberatamente sicuro.
6. Gestione di pressione memoria: quote, fail-fast controllato, rilasci, page faults e ripresa; non trasformare automaticamente tutti i model weights in resident memory pinnata.

I file possono arrivare da ext4, NTFS o altri filesystem solo dopo che tali driver abbiano un percorso reale e verificato. Nessun modello embedded nel kernel può essere usato per dichiarare conclusa la fase di caricamento da disco.

## 7. Compute Manager kernel e isolamento

Il nucleo minimo del Compute Manager è un **resource broker**, non uno scheduler AI parallelo:

- Registra i device nel namespace Ob/Io canonico.
- Crea device contexts per processo e associati a un handle Ob.
- Controlla quote RAM/VRAM/DMA e numero massimo di job in flight.
- Gestisce submission rings e fence event-like con Ke.
- Mantiene lifecycle `queued → submitted → completed/failed/cancelled → reaped` senza doppio completamento.
- Gestisce teardown dell'ultimo handle, terminazione del processo e reset hardware senza use-after-free.
- Propaga l'errore specifico, distinguendo timeout, device lost, invalid buffer e access denied.

Il device può imporre work quotas per sessione; non introdurre una nuova classe di priorità NT pubblica. Le priorità delle worker thread restano soggette alla policy Ke e alla specifica `docs/KERNEL_SCHEDULING_RESPONSIVENESS_SPEC.md` se presente.

## 8. GPU inference — percorso successivo

Dipendenze: miniport GPU nativo, allocazione/residency GPU, comandi/fence, dispositivo in stato operativo, user-mode runtime Vulkan o equivalente. Il driver VirtIO 2D usato per il display **non** dimostra un backend Vulkan compute.

1. Prima ottenere un piccolo kernel compute verificabile su GPU reale (per esempio vector-add/GEMM su buffer), con risultato confrontato al backend CPU.
2. Validare shader/module, descriptors, command lengths e memory isolation al boundary del driver.
3. Valutare un port user-mode Mesa o backend Vulkan esistente dopo che la GPU compute API nativa è stabile.
4. Aggiungere execution provider che selezioni automaticamente operatori supportati e fallback CPU per operatori non supportati.
5. Evitare che inferenza saturi VRAM e code necessarie al compositor; controllo di budget e fairness.

Le API Direct3D/DirectML non devono essere dichiarate funzionanti solo perché i loro header sono compilabili. Compatibilità DirectML/Windows ML è una fase distinta, basata sulle rispettive API reali.

## 9. NPU Intel/AMD — percorso successivo

`TitanNPU/NexusNPU` è il punto di partenza concettuale per capability descriptor e comandi, **non** un driver hardware già pronto. Scegliere una sola famiglia di chip AMD64 con ID PCI, firmware e interfaccia documentati; mantenere un adapter specifico:

- Enumerazione PCI/PnP e capability genuine, non costanti preimpostate.
- Firmware loading, version checking e gestione di errori.
- MMIO e DMA con confini di sicurezza, mapping IOMMU, MSI/MSI-X.
- Runtime context per processo, buffer e queue hardware reali.
- Submission di un operatore semplice e confronto numerico col backend CPU.
- Interrupt di completamento, fence, timeout, reset, cancellazione.
- Isolamento tra processi e recupero del device dopo crash del client.

Intel NPU e AMD XDNA richiedono backend separati. Il runtime deve poter scegliere CPU quando il firmware non è caricato, il chip è assente o una richiesta non è supportata. NVIDIA viene considerata principalmente come backend GPU, non come NPU generica.

## 10. Policy di scheduling e responsiveness

Classificazioni interne proposte (non API Microsoft): **InteractiveAI**, **UserInitiated**, **Utility**, **Background**. Il kernel applica il contratto NT delle priorità 0–31 tramite Ke e non riserva arbitrariamente priorità real-time alle inferenze.

- Un input foreground interattivo può elevare temporaneamente i thread necessari alla risposta, senza boost permanenti.
- Inferenza lunga in background usa thread normali/low priority, batch bounded e yield/wait nei punti appropriati.
- La latenza di input e del compositor ha precedenza operativa rispetto al throughput massimo dell'inferenza.
- Per compute GPU/NPU, preferire job a granularità controllabile, fence asincrone e limiti dei job in flight.
- Prevedere revoca di quote/pressure handling quando altri processi necessitano RAM/VRAM.
- Nessun thread in Ring 0 gira in spin attendendo inferenza; completamenti via interrupt/DPC/Ke event.

**Metriche future** (non risultati correnti): latenza p95 input→thread interattivo pronto, frame drops sotto carico AI, throughput/tokens-per-second, memoria di picco, memoria recuperata dopo cancel e fairness tra due sessioni. Un obiettivo iniziale ragionevole da testare è p95 di dispatch interattivo inferiore a 5 ms sotto carico CPU in QEMU, da rivalidare su hardware reale.

## 11. Sicurezza, privacy e affidabilità

- Inferenza locale per impostazione predefinita: nessun upload di input, pesi, risultati o telemetria.
- Parsing dei modelli **solo Ring 3**, con controlli anti overflow, bounds, quantità di tensori, dimensioni e nesting.
- Non trattare i file modello come codice eseguibile attendibile; gli operatori custom e plugin richiedono policy separata.
- Process isolation: quote, ACL, access rights, handle e mapping che non attraversano i processi accidentalmente.
- DMA sempre limitato a buffer validati/pinnati; usare IOMMU se disponibile, disabilitare funzionalità insicure se l'isolamento non può essere garantito.
- GPU/NPU command buffers non vengono copiati o dereferenziati dopo aver perso ownership; rimuovere i mapping solo dopo completamento effettivo hardware.
- Timeout e reset del device devono lasciare l'OS funzionante e propagare errori ai client.
- Verificare licenze di framework, modelli, firmware e dataset separatamente, con attribuzioni necessarie.
- Nessuna modifica all'ABI Windows pubblica per introdurre funzioni sperimentali AI.

## 12. Piano di implementazione per milestone

| Fase | Implementazione minima | Evidenza necessaria | Gate |
|---|---|---|---|
| **AI0 — Audit** | Separare TitanNPU hosted da ABI, isolare fake capabilities | Report di interfacce, stub e dipendenze | Nessun device simulato dichiarato hardware |
| **AI1 — CPU** | Motore CPU Ring 3, scalare + AVX2 opzionale, piccolo modello | PE user-mode esegue inferenza numerica corretta in QEMU | Process lifecycle e syscall sufficienti |
| **AI2 — Storage** | File-backed data sections, model loader, pagine pesi RO | Due sessioni leggono lo stesso modello, cleanup corretto | Io/Mm/Ob file-backed funzionanti |
| **AI3 — Compute device** | Device objects, contexts, buffer, queue, fence, cancel | Lifecycle e permission checks QEMU con device test reale o esplicitamente virtuale | Io/Ke/Ob/Mm integrati |
| **AI4 — GPU** | Un device GPU compute, backend user-mode | GEMM GPU confrontata con CPU su hardware reale | Miniport/VRAM/fence operativi |
| **AI5 — NPU** | Un device Intel o AMD con firmware | Operatore NPU e IRQ/fence realmente verificati | Specifica hardware e device disponibili |
| **AI6 — Compatibilità** | ONNX Runtime EP e API Windows documentate selezionate | Modello ONNX reale su CPU, poi GPU/NPU | ABI e semantica verificate |
| **AI7 — QoS** | Budget, fairness e preemption cooperante | Due sessioni AI + desktop attivo senza deadlock | Scheduler e compositor effettivi |

**Sequenza obbligatoria:** AI0 → AI1 → AI2 prima di fare affermazioni di inferenza locale integrata. AI3–AI5 possono essere sviluppate in stream separati **solo dopo** aver stabilizzato le ABI condivise. AI6–AI7 seguono evidenza reale di esecuzione.

## 13. Criteri di accettazione dettagliati

### AI1 — First inference

- [ ] PE AMD64 avviato come processo gestito da Ps, non dalla sola fixture CPL3.
- [ ] Modello letto da file reale, validato senza crash kernel.
- [ ] Operatori del primo modello implementati in Ring 3 e documentati.
- [ ] Output confrontato a dati di riferimento con tolleranza definita per dtype.
- [ ] Test scalare e SIMD danno risultati coerenti.
- [ ] CPUID/XGETBV proteggono i percorsi AVX.
- [ ] Due processi possono eseguire inferenze senza condividere attivazioni.
- [ ] Kill/cancel del processo recupera handle, heap e viste di mapping.
- [ ] QEMU esegue il programma e verifica i risultati via stdout/serial/exit status.

### AI3 — Kernel device management

- [ ] Le richieste con handle invalidi o permessi insufficienti sono respinte.
- [ ] Un buffer liberato non resta accessibile dopo device completion.
- [ ] L'annullamento della submission non permette doppio completamento o liberazione anticipata.
- [ ] Terminazione del client rilascia quote, memoria pinned e riferimenti.
- [ ] Device loss lascia le altre sessioni coerenti.
- [ ] Nessun driver user-mode viene linkato nel kernel freestanding.

### AI4 / AI5 — Hardware acceleration

- [ ] Enumerazione e ID dispositivo corrispondono all'hardware effettivo.
- [ ] DMA e MMIO rispettano ownership, mapping e limiti di indirizzo.
- [ ] Output numerico confrontato a CPU su almeno tre dimensioni tensoriali.
- [ ] Fence/IRQ/timeout/reset esercitati; fallback CPU verificato.
- [ ] Specificare il dispositivo fisico e la versione firmware usati nei test.

### Performance and stability

- [ ] Nessun blocco del desktop durante inferenza CPU intensa a priorità background.
- [ ] Misurare p50/p95/p99 di latenza di dispatch interattivo e tempi inferenza.
- [ ] Nessuna perdita PFN/VRAM/riferimenti dopo cicli ripetuti o kill anticipato.
- [ ] Nessuna dipendenza da host, cloud o risposte numeriche hardcoded per superare le prove.

## 14. Verifiche e reporting

Ordine normale di verifica (salvo diversa istruzione esplicita dell'utente):

1. Test unitari host **solo** per parser, shape algebra, operatori scalari e quantizzazione.
2. Cross-compile AMD64 del runtime user-mode e verifica degli import PE.
3. Build incrementale del kernel soltanto quando sono cambiati Io/Ke/Mm/Ob/HAL.
4. QEMU headless con un vero programma di inferenza, modello su media virtuale e status di uscita verificabile.
5. Test di fault injection: file corrotto, OOM, process termination, mapping fault, cancel, device reset.
6. Hardware fisico per prove GPU/NPU; QEMU non può validare l'accelerazione di una GPU assente.

Gli esiti vanno distinti: **progettato**, **compilato**, **linkato**, **eseguito in user-mode**, **verificato in QEMU**, **verificato su acceleratore fisico**. Non dichiarare una fase completata grazie a una sola struttura o un mock.

Se l'utente chiede di non eseguire test/commit per un batch, rispettare il vincolo: il codice rimane **non verificato** fino alla fase autorizzata. Nessun batch secondario conclude automaticamente il mandato agentico; la definizione della milestone è nel presente documento.

## 15. Dipendenze e integrazione con le specifiche MicaNT

- `docs/KERNEL_ARCHITECTURE.md`: un solo kernel modulare canonico.
- `docs/KERNEL_INTEGRATION_STATUS.md` e `docs/KERNEL_MILESTONES.md`: stato **effettivo** e dipendenze M4–M7.
- `docs/KERNEL_FILESYSTEM_SPEC.md` (se installato): Io, lettura file e sections data file-backed.
- `docs/KERNEL_GPU_DRIVER_IMPLEMENTATION_SPEC.md` (se installato): miniport GPU/VRAM/compute.
- `docs/KERNEL_SCHEDULING_RESPONSIVENESS_SPEC.md` (se installato): scheduling e policy desktop.
- `AGENTS.md`: limiti freestanding, riuso del codice MicaNT e comportamento dell'agente.

Questa specifica **non cambia l'ordine di priorità** di M4–M7 né giustifica sviluppare grandi nuove API AI prima che MicaNT possa avviare un PE user-mode da disco.

## 16. Riferimenti tecnici

- ONNX Runtime architecture / Execution Providers: https://onnxruntime.ai/docs/reference/high-level-design.html
- ONNX Runtime Execution Providers: https://onnxruntime.ai/docs/execution-providers/
- llama.cpp: https://github.com/ggml-org/llama.cpp
- GGUF format: https://github.com/ggml-org/ggml/blob/master/docs/gguf.md
- Microsoft Windows ML EP: https://learn.microsoft.com/en-us/windows/ai/new-windows-ml/supported-execution-providers
- Linux DRM accelerator (`accel`) subsystem: https://docs.kernel.org/accel/index.html
- Vulkan compute: https://docs.vulkan.org/spec/latest/chapters/dispatch.html
- Intel/AMD NPU backend: utilizzare documentazione pubblica del dispositivo effettivamente selezionato, senza dedurre hardware dalle enumerazioni presenti in `npu.hpp`.

---

**Decisione architetturale:** prima un motore di inferenza reale **CPU user-mode**, poi un kernel **resource manager** per acceleratori GPU/NPU. Il successo si misura con risultati numerici, isolamento, cleanup e reattività del sistema, non con il numero di API o dispositivi nominalmente registrati.
