# MicaNT — Recupero rapido del kernel monolitico nel kernel modulare

**Tipo:** specifica operativa di migrazione, non nuova milestone di funzionalità  
**Destinazione:** `docs/KERNEL_MONOLITHIC_RECOVERY_MIGRATION_SPEC.md`  
**Target:** AMD64 / UEFI / C++23 / kernel NT canonico MicaNT  
**Baseline verificata:** fork `luca-saggese/MicaNT`, `main` commit `7affbebdbd04a75c25a7bdd3cb69c89384580ca1` (12 novembre 2026)  
**Stato:** piano proposto; nessuna implementazione o test eseguito da questo documento.

## 0. Decisione vincolante

**Mantenere intatto il lavoro già eseguito nel kernel modulare e importare soltanto il comportamento legacy mancante, riutilizzando le implementazioni bare-metal esistenti.** Non ricostruire un kernel monolitico; non duplicare scheduler, allocator, process manager, object manager, loader o I/O manager. Non considerare “presente in header” sinonimo di “funzionante su hardware reale”.

Obiettivo: trasformare rapidamente codice recuperabile in *driver e servizi del solo kernel canonico*, con test QEMU/OVMF end-to-end e successivo collaudo su PC x64. L'accettazione di una migrazione richiede comportamento reale, non compilazione isolata, stub o messaggi di log ottimistici.

### Invarianti architetturali

1. `ntoskrnl_core_baremetal` rimane l'unico target kernel produttivo; `kernel/init/kiinit.cpp` rimane il punto di composizione. `old/monolithic-kernel/ntoskrnl.cpp` resta archiviato, **mai compilato, linkato, incluso o messo in staging**.
2. Rimangono autoritativi i moduli canonici: `kernel/ke`, `mm`, `ps`, `ob`, `io`, `hal`, `fsrtl`, `win32k`; il codice importato si adatta ai loro contratti.
3. Nessun nuovo allocator PFN, CR3 manager, IDT globale, scheduler, syscall dispatcher, `KiSystemStartup`, handle table o user-mode loader parallelo.
4. Nessun uso di C++ hosted (`std::mutex`, `std::vector`, `std::thread`, loader/SCM hosted) nei percorsi freestanding senza porting esplicito e revisione di IRQL/allocazione.
5. Il percorso importato non deve aggirare `Io`/`PnP`/`Mm`: DMA, mapping MMIO, interrupt, risorse PCI, handle e block-device passano dai proprietari canonici.
6. Un errore hardware o una capacità assente restituiscono uno stato di errore reale. Vietato dichiarare controller “online”, pacchetti “trasmessi”, disco “montato” o driver “avviato” senza evidenza.
7. Si preservano manifest di build, validazione PE, UEFI handoff, test di bootstrap CPL3, QEMU e verifiche architetturali già accettate. Cambiamenti piccoli di *interfaccia*, ma consegna verticale fino al test; non fermarsi alla prima compilazione.
8. I driver OEM di bus, storage, USB, rete e classi hardware sono immagini native `.sys` autonome e non sorgenti linkate in `ntoskrnl_core_baremetal`. Il kernel contiene HAL, Io/PnP, allocator DMA e ABI/servizi generici; il driver `.sys` possiede il protocollo e registra i dispositivi presso gli owner canonici. Un adapter filesystem hardware-specifico va nel relativo `.sys`, non nel kernel.

### Collocazione dei driver OEM

Per le slice AHCI/FAT32, xHCI/HID e I210, CMake deve produrre immagini `.sys` separate dal target kernel. Le immagini consumano servizi espliciti del kernel per PCI, BAR/MMIO, DMA, timer, Io/PnP e logging; non usano accessi PCI privati o puntatori virtuali come indirizzi DMA. Il loader/ABI boot-start già usato da `virtio_gpu.sys` è il punto di partenza per l’integrazione; va esteso con un contratto neutro sufficiente ai driver e lifecycle/error handling, senza copiare i driver nell’immagine kernel. Il fatto che un `.sys` sia caricato o presente nella directory di boot non prova che sia inizializzato o collegato a Io/PnP.

Conseguenza per la baseline corrente: `kernel/io/ahci_file_provider.cpp`, che include e inizializza il controller AHCI/FAT32, non può restare nel manifest bare-metal del kernel. La sua parte filesystem-specifica va trasferita in un `.sys`; in `kernel/io` rimangono l’oggetto file generico e l’interfaccia read-only del block device.

## 1. Inventario reale e classificazione del recupero

Il file storico contiene circa 5.611 righe, ma il valore principale è nei driver/header già separati ai quali si appoggia. L'header non è prova di integrazione nel kernel modulare.

| Area | Codice legacy già disponibile | Situazione modulare / azione | Priorità |
|---|---|---|---|
| **AHCI SATA** | `include/micant/km/drivers/storage/ahci_baremetal.hpp`: PCI class discovery, ABAR, DMA, `readSectors`, `writeSectors` | Il precedente `kernel/io/ahci_file_provider.cpp` includeva il backend direttamente nel kernel; è stato spostato in `drivers/oem/ahci` e va completato come `.sys`, con adapter HAL/DMA/Io block device e sola lettura iniziale | **P0** |
| **FAT32 su AHCI** | Stesso header: `Fat32Volume`, `Fat32FileSystem::mountAll`, `readFile`, `createFile` | Recuperare parser/mount/read come backend FsRtl/Io, non come secondo VFS; scritture solo dopo crash/recovery test | **P0** |
| **xHCI fisico** | `include/micant/km/drivers/usb/xhci_baremetal.hpp`; init e polling nel monolitico | Adapter xHCI reale → USBD/Io/PnP; risolvere BAR/BDF e timing hardcoded prima dell'hardware fisico | **P0** |
| **USB HID** | Monolitico: init `XhciBareMetalEngine` e `tryGetKeystroke`; `hidusb.hpp`, `kbdhid.hpp`, `mouhid.hpp` già presenti | Convertire polling/special-case in completamenti URB → class driver di input canonico | **P0** |
| **Intel I210 Ethernet** | `include/micant/km/drivers/net/i210_baremetal.hpp` + callback shell remota | Adapter NIC → NDIS/TCPIP; recuperare RX/TX/PCI/DMA, non rendere la shell remota un servizio kernel | **P1** |
| **GOP/boot video** | Rendering/shell grafica nel monolitico, `bootvid.hpp` | Recuperare solo eventuale accesso a framebuffer GOP o diagnostica non già coperti; non duplicare il desktop/Win32k | **P1** |
| **ACPI/MADT/APIC/CPU** | Scansione ACPI, timer, interrupt, CPU telemetry nel monolitico | Confronto funzionale con `kernel/hal/amd64_bootstrap.cpp`, `amd64_firmware_interrupts.cpp`; patch mirate solo alle lacune | P1–P2 |
| **PE/Ring 3/syscall** | Monolitico: PE user-mode, syscall e transizione CPL3 | **Proteggere M4 e M6–M8**; recuperare solo capacità misurabilmente mancanti rispetto a `kernel/ps/image.cpp`, `address_space.cpp`, syscall modulari | P2 |
| **Mm/Ob/Ke** | `mm_baremetal.hpp`, `ob_baremetal.hpp`, `io_baremetal.hpp` + codice inline nel monolitico | **Non importare sottosistemi**; eventualmente portare test/algoritmi locali mancanti previo confronto | P2 |
| **Ext4** | Monolitico menziona VFS ext4 read-only; codice FsRtl modulare già esiste | Inventariare percorso e catena block I/O; non creare un secondo ext4; recuperare solo traversata/lettura provata | P2 |
| **Shell, GUI e applicazioni** | Loop interattivo e rendering direttamente in Ring 0 | Non migrare come runtime produttivo: conservare solo test/diagnostiche utili, applicazioni in Ring 3 | Escluso |
| **USB Mass Storage BOT** | Modello hosted in `usb.hpp`; nessuna implementazione hardware BOT nel monolitico verificata | È **lavoro ancora da implementare** su xHCI/USBD reale; non conteggiare come recuperato | P1 |
| **WFP firewall** | Nessun percorso WFP individuato nel monolitico | Usare solo `wfp.hpp` e percorso M11 già previsto, dopo networking reale | Escluso da recovery |
| **VirtIO-net / GPU accelerata / NPU** | Non identificati come driver completi nel monolitico | Seguire milestone dedicate; non attribuire loro un recupero legacy inesistente | Escluso |

La priorità rappresenta il valore di *recupero*, non pretende che dipendenze tecniche già aperte siano risolte. Per la Live USB, xHCI/HID e GOP sono il ramo più urgente; per M4/M7, una base block-device + FAT32 read-only può sbloccare `SEC_IMAGE` file-backed.

## 2. Metodo veloce: estrarre i backend, non copiare il kernel

### Fase R0 — Inventario automatico e gap list (una sola volta)

1. Fare baseline della branch pulita e dei test; **non modificare artefatti già verificati**.
2. Estrarre dal monolitico tutti gli `#include`, gli owner di stato globale, le inizializzazioni hardware, gli handler, i metodi di I/O e i percorsi di cleanup.
3. Individuare per ogni funzionalità il suo **owner canonico** e registrare `legacy symbol → nuovo owner → stato (già migrato / recuperabile / simulato / incompatibile / mancante) → test richiesto`.
4. Trovare i punti di aggancio con `rg` e `git grep`, senza riscrivere manualmente gli stessi algoritmi.
5. Per i sorgenti header-only, estrarre solo il codice produttivo in una o poche unità `.cpp` del relativo sottosistema, mantenendo un header piccolo per il contratto. Vietato includere l'intero header monolitico in `kiinit.cpp` solo per ottenere la compilazione.

Comandi indicativi da eseguire nel repository locale:

```bash
rg -n '^#include|\.initialize\(|\.mountAll\(|tryGetKeystroke|readSectors|writeSectors|sendTcpData' old/monolithic-kernel/ntoskrnl.cpp
rg -n 'xhci_baremetal|ahci_baremetal|i210_baremetal|Fat32FileSystem' kernel include CMakeLists.txt
sed -n '254,286p' CMakeLists.txt
python3 scripts/verify_kernel_architecture.py
```

Se `rg` non è installato, usare `git grep -n`. Il nome/esecuzione degli script esistenti va verificato prima di automatizzarli; la spec non certifica che tutti i comandi di test possano partire senza parametri.

**Output richiesto:** tabella con evidenze di file/funzione e un elenco chiuso di migrazioni ad alto valore. La fase successiva non può ricreare funzioni già contrassegnate «modulare verificato».

### Fase R1 — Adapter layer temporaneo e thin vertical slices

Per ogni backend legacy:

1. **Conservare l'algoritmo hardware già presente**, soprattutto register programming e sequenze di inizializzazione verificabili.
2. Sostituire solo i confini host-specific: PCI discovery, MMIO, allocazione DMA, timer, IRQ, logging e lock con servizi HAL/Io/Mm/Ke canonici.
3. Esporre una piccola interfaccia driver (`Probe`, `Start`, `Submit`, `Stop`/`Remove`) per lo stack corretto, senza esporre la vecchia singleton globale come API di sistema.
4. Collegare un **solo** percorso produttivo al runtime bare-metal; un adapter temporaneo può esistere solo se ha un piano di rimozione esplicito.
5. Implementare subito error path, cleanup e disconnessione: non accettare un `Start()` che riesce solo sul primo dispositivo o al primo boot.
6. Scrivere test negativi e test di trasferimento reale **prima** di dichiarare migrata la funzionalità.

### Fase R2 — Migrazione per vertical slice, senza accumulare refactor

Una slice viene considerata completa solo quando:

`controller individuato → inizializzato → trasferimento fisico → Io/HAL riceve risultato → chiamante canonico usa i dati → test QEMU passa → regressioni M1–M4 e M6–M8 passano`.

Non fermarsi dopo l'inclusione nel CMake, l'inizializzazione o un log di successo.

## 3. Piano d'importazione A — AHCI + FAT32 (massimo riuso per M14 e M4/M7)

**Sorgenti:** `ahci_baremetal.hpp`, monolitico nei blocchi di `probeMicaSysVolume`, `getTargetVolume`, file read/write e mount; `kernel/io/iomgr.cpp`, `kernel/fsrtl/*`, `kernel/mm/*` come destinazioni canoniche.

**Percorso minimo:**

1. Estrarre un backend AHCI nativo, usare la ricerca PCI per classe e non BDF o BAR fissi; scegliere porte SATA enumerate, non presupporre `sdb`, `C:` o LBA particolari.
2. Usare servizi canonici per mappe MMIO uncached, buffer DMA fisicamente accessibili e completion/timeout; gestire BAR 64-bit se richiesto dal dispositivo; allineamenti e cache coherence espliciti.
3. Esporre `ReadBlocks` nel layer block device di Io con geometria/capacity interrogate dal disco, propagando errori e short-read; **read-only per il primo gate**.
4. Riutilizzare `Fat32Volume::readFile` e la traversata FAT nel driver filesystem esistente, passando dal volume/block-device canonico; non mantenere un VFS alternativo con mount hardcoded `C:`/`D:`.
5. Integrare mount GPT/MBR e controlli bounds nel percorso storage esistente; collegare `IoCreateFile`/read path prima di richiedere file-backed `SEC_IMAGE`.
6. Test su immagine disco nota in QEMU AHCI: leggere settori, file di dimensioni diverse, cross-cluster, invalid path, partition bounds, error injection; confrontare SHA-256 dal lato host.
7. Le scritture FAT32 legacy possono essere recuperate **solo dopo** buffer flushing, rollback/error tests, power-loss handling e definizione del mount RW; non attivare in automatico su dischi interni.

**Accettazione:** il kernel modulare legge file reali da un'immagine SATA/FAT32 attraverso AHCI→Io→FsRtl. Nessuna chiamata diretta dal processo/shell al singleton `Fat32FileSystem`.

## 4. Piano d'importazione B — xHCI + HID (massimo riuso per M9 e live USB)

**Sorgenti:** `xhci_baremetal.hpp`; legacy `initialize()` e `tryGetKeystroke` nel monolitico; `usb.hpp`, `xhci.hpp`, `hidusb.hpp`, `kbdhid.hpp`, `mouhid.hpp`.

1. Mantenere le sequenze xHCI valide (DCBAA, command/event/transfer rings, slot, endpoint e USBLEGSUP) come base dell'unico driver hardware.
2. **Prima di attivare sul PC vero:** rimuovere accessi PCI fissati a `00:14.0`, fallback BAR `0xA2D00000`, ipotesi su un solo controller/porta, TSC fisso a 3,8 GHz; validare dimensione BAR e MMIO.
3. Collegare i buffer ring al backend DMA/physical mapping di HAL/Mm, distinguendo indirizzi CPU da indirizzi visibili alla NIC/USB e gestendo IOMMU quando presente.
4. Sostituire il polling monolitico della tastiera con completamenti URB e class driver per gli eventi input; polling diagnostico iniziale accettabile solo come step transitorio misurato.
5. Separare l'host controller fisico da `usb.hpp`/`xhci.hpp` hosted: i loro dispositivi sintetici e completamenti artificiali restano test fixtures, non backend produttivo.
6. Implementare enumerazione dinamica, parsing descriptor rigoroso, endpoint multipli, device disconnect/reset, timeout e ring wrap; gestione hub successiva ma necessaria per uso generale.
7. Test QEMU `qemu-xhci` + `usb-kbd`/`usb-mouse`: verificare report fisici fino a KbdClass/MouClass dopo `ExitBootServices`; scollegamento e riconnessione, almeno due dispositivi contemporanei.
8. **Mass Storage non era già completo nel monolitico:** BOT/CBW/CSW, SCSI READ CAPACITY/READ(10), block device USB e mount FAT32 restano nuova integrazione sopra il bus canonico, inizialmente read-only.

**Accettazione:** kernel modulare riceve input da xHCI emulato e legge settori da `usb-storage` QEMU senza chiamate firmware UEFI post-handoff. In hardware reale, prima disabilitare qualsiasi scrittura su storage.

## 5. Piano d'importazione C — Intel I210 / Ethernet (riuso per M10)

**Sorgenti:** `i210_baremetal.hpp` e inizializzazione/callback TCP nel vecchio monolitico; `ndis.hpp`, `tcpip.hpp`, `afd_wsk_rio.hpp`.

1. Recuperare le primitive NIC fisiche e i ring RX/TX dalla vecchia implementazione, sostituendo BDF, BAR, DMA e interrupt specifici con risorse HAL/Io.
2. Esporre pacchetti reali al percorso NDIS canonico; se ancora non freestanding, aggiungere un bridge **stretto e temporaneo** per RX/TX, non un secondo TCP/IP.
3. Riutilizzare ARP/ICMP e framing solo dove coerenti con i contratti dello stack TCPIP scelto. Il vecchio protocollo della shell remota non è un sostituto di TCP con socket standard.
4. Evitare credenziali, indirizzi IP, MAC, porte o servizi shell hardcoded; non riattivare servizi remoti insicuri durante il boot.
5. Test su NIC Intel I210 reale quando disponibile; per l'accettazione QEMU della milestone di rete, VirtIO-net è un backend separato necessario: il driver I210 **non lo sostituisce**.

**Accettazione:** frame reali RX/TX raggiungono NDIS/TCPIP nel kernel modulare; no shell kernel come dipendenza del network stack.

## 6. Piano d'importazione D — HAL, video, Mm, Ps e Ke: solo delta utili

Il monolitico implementa IDT, APIC, memory mapping, CPU/ACPI, syscall, PE loader e percorso Ring 3, ma la versione modulare ha già M1–M3 verificati e M4, M6–M8 parzialmente verificati. Quindi:

- Fare una **tabella comportamentale** funzione per funzione, non portare interi file o state globals.
- ACPI: recuperare solo parsing/tabella/checksum/test che mancano al HAL attuale e possano essere provati su più firmware.
- GOP: recuperare soltanto logica di accesso framebuffer e diagnostica che il percorso display canonico ancora non copre; non reintrodurre shell/desktop in Ring 0.
- Paging: il PFN allocator, la section map, CR3 per processo e le guard pages canoniche restano invariati; importazioni solo se risolvono un test concreto fallito.
- Ke: nessun secondo interrupt dispatch, clock loop o ready queue. Eventuali helper HAL devono seguire IRQL e controllo ownership correnti.
- Ps/loader/syscall: confrontare ABI e contratti di M4, M6–M8; se manca una funzionalità, isolare e portare l'algoritmo, non il dispatcher globale legacy.
- `mm_baremetal.hpp`, `ob_baremetal.hpp`, `io_baremetal.hpp`: **fonti di recupero e confronto**, non nuove autorità di runtime.
- Non portare dal monolitico il suo shell loop, il rendering desktop, l'editor che scrive direttamente al disco, la registrazione fittizia di processi/servizi o i messaggi hardware precompilati.

**Gate:** nessuna regressione ai test M1–M4 e M6–M8 già verificati in QEMU; lo stesso percorso utente/Ring3 continua a funzionare.

## 7. Architettura bersaglio

```text
UEFI BOOTX64.EFI  [invariato]
       |
UefiKernelHandoffV3  [invariato]
       |
ntoskrnl_core_baremetal  [unico kernel]
       |
  kernel/init/kiinit.cpp  -- orchestrazione unica
       |
       +-- kernel/hal    <-- PCI BAR, MMIO, timer, IRQ, DMA
       +-- kernel/io     <-- PnP, device object, IRP, block device
       |     +-- AHCI backend recuperato
       |     +-- xHCI backend recuperato --> USBD --> HID / USB storage
       |     +-- I210 NIC recuperato ------> NDIS --> TCP/IP --> AFD
       +-- kernel/fsrtl  <-- FAT32 recuperato, ext4 canonico
       +-- kernel/ke,mm,ps,ob  [preservati]
       +-- kernel/win32k     [display canonico]
       |
Ring 3: app, shell, servizi  [nessun vecchio kernel-shell]
```

Le unità sorgente e i nomi finali si devono conformare ai contratti reali del repository; la figura è un target logico, non dichiara driver o interfacce già realizzati.

## 8. Importazione veloce in pratica: sequenza obbligatoria

**Ordine suggerito per il massimo effetto utile:**

| Passo | Attività | Prerequisito | Definizione di finito |
|---|---|---|---|
| R0 | Baseline, inventario automatico, matrice di gap | Repository attuale | Evidenza per ogni funzione; nessuna duplicazione |
| R1 | Adapter HAL comuni (PCI/MMIO/DMA/timer/logging) | R0 + MM/Io validi | Test MMIO/DMA e failure paths, niente indirizzi hardcoded |
| R2 | Recupero AHCI read-only | R1 | Sector read e checksum da disco QEMU AHCI |
| R3 | Recupero FAT32 read-only | R2 | File read vero via Io/FsRtl; blocco per M4/M7 rimosso ove possibile |
| R4 | Recupero xHCI fisico + input HID | R1 | Tastiera/mouse QEMU xHCI post-UEFI, disconnessione |
| R5 | USB BOT/SCSI → block device | R4 | Read reali della chiavetta, GPT/FAT32 read-only |
| R6 | Recupero Intel I210 hardware NIC | R1 | Packet RX/TX via NDIS; no secondo stack TCP/IP |
| R7 | Delta HAL/GOP/ACPI | R0, quando richiesto da test | Fix mirato con regressione hardware/QEMU |
| R8 | Pulizia adapter e chiusura documentazione milestone | R2–R7 | Nessun doppio owner, integrazione canonica, suite verde |

**Parallelismo consentito:** R2–R3, R4–R5, R6 possono procedere su branch/worktree separati **dopo** l'accordo stabile sui contratti HAL/DMA/Io (R1). Non fare cherry-pick di modifiche che duplicano allocator, dispatcher o driver manager.

**Ottimizzazione cruciale:** se HAL, PnP o DMA non hanno ancora tutte le API, implementare una piccola capability comune soltanto quando serve ai tre backend; non riscrivere il modello driver nella stessa PR. Il coordinamento deve essere un contratto comune documentato, non copie locali di `pciRead32`, `toPhys` e `delayMs` in ogni driver.

## 9. Test e prove in QEMU/OVMF

### Sempre: regressione dell'attuale kernel

- Build **freestanding** del target `ntoskrnl_core_baremetal`, non soltanto host `micant_tests`.
- Validazione manifest, PE e `scripts/verify_kernel_architecture.py` secondo il workflow esistente.
- Test delle milestone M1–M4 e M6–M8 già presenti nel target; QEMU runtime normale e, quando previsto, `--runtime-selftest` come modalità separata.
- Zero nuovi `#include "old/..."`, zero link al vecchio `ntoskrnl.cpp`, zero simboli duplicati o fallbacks dal vecchio kernel.
- Preservazione di paging, IRQL, APC, Ps lifecycle, section condivise, import PE e TLS verificati.

### Gating per backend

| Backend | Prova positiva | Prova negativa obbligatoria |
|---|---|---|
| AHCI | Leggere settori e file con checksum esatto da immagine QEMU | Disco assente, LBA oltre capacità, timeout e error propagation |
| FAT32 | Traversare directory, file multi-cluster, file zero-byte | FAT corrotta, cluster loop, directory invalida, mount fallito |
| xHCI | Enumerare e trasferire report HID USB QEMU | No controller, descrittori malformati, device unplug, timeout ring |
| USB BOT | Leggere settori reali QEMU USB Mass Storage | CSW errato, SCSI CHECK CONDITION, disconnessione durante il read |
| Intel I210 | RX/TX reale su controller supportato | Link down, controller assente, DMA failure, IRQ timeout |
| GOP/ACPI | Display + tabelle ACPI su firmware diversi | GOP assente, RSDP invalido, tabelle con checksum errato |

**Nessun test di successo fittizio** può sostituire il dato hardware: devono essere verificati bytes e sequenze acquisite da dispositivo emulato o fisico, non da oggetti sintetici `UsbDevice` o memoria locale inventata.

## 10. Rischi che devono bloccare una migrazione

- Il codice legacy assume Xeon C230, BDF `00:14.0`, BAR `0xA2D00000`, disco fisico, LBA, indirizzo IP o TSC a 3,8 GHz: **non è pronto per hardware generico**.
- Richiede memoria fisica contigua ma usa un puntatore virtuale senza traduzione DMA valida: **non attivare bus mastering**.
- La procedura di reset di un controller non rispetta ownership firmware, limiti o timeout: **non testare su PC principale**.
- Si attiva `writeSectors`/`createFile` sui dischi interni prima di aver provato isolamento e recovery: **blocco**.
- Il percorso dipende da `std::mutex`/heap hosted in ISR/DPC o durante early boot: **blocco**.
- Una singleton simula un risultato, e il chiamante lo considera hardware reale: **blocco**.
- I test richiedono modifiche invasive a Ps/Mm/Ke già verificati o introdurrebbero owner duplicati: **ridisegnare l'adapter**, non sostituire il kernel.

## 11. Istruzioni operative per un coding agent

> Missione: recuperare tutto il codice hardware e i percorsi funzionali **mancanti** dal monolitico e dai suoi header, senza toccare o regredire ciò che è già implementato nel kernel modulare. Importare in sequenza verticale AHCI/FAT32, xHCI/HID/USB storage e Intel I210, sfruttando HAL/Io/PnP/Mm/Ke canonici. `old/monolithic-kernel/ntoskrnl.cpp` è **read-only** e non può entrare nel build. Prima di scrivere codice produrre una matrice legacy→canonico verificata; per ogni backend riportare prove di dati reali su QEMU, test negativi, regressioni e file modificati. L'obiettivo non è produrre più codice: è usare il codice esistente per chiudere capacità pratiche con il minor numero di nuove linee e senza percorsi paralleli. Proseguire fino all'accettazione della slice assegnata, anziché fermarsi dopo una build verde.

**Criterio di completamento del programma di recupero:** le funzionalità hardware realmente esistenti nel legacy sono collegate e testate attraverso il kernel modulare; ciò che non esiste o è simulato è esplicitamente marcato come nuova implementazione e resta nella rispettiva milestone. Nessuna regressione alle milestone M1–M4 e M6–M8, nessuna ricomparsa del kernel monolitico come target produttivo.

## 12. Riferimenti repository e confini della verifica

- `old/monolithic-kernel/ntoskrnl.cpp` — sorgente storico e init logic; **fonte di analisi, mai target di build**.
- `old/monolithic-kernel/README.md` — divieto esplicito di compilare/linkare/stagiare il vecchio monolitico.
- `docs/KERNEL_ARCHITECTURE.md` — architettura canonica e build manifest.
- `docs/KERNEL_MILESTONES.md` — stato delle milestone M1–M17.
- `include/micant/km/drivers/storage/ahci_baremetal.hpp` — controller AHCI e FAT32 legacy.
- `include/micant/km/drivers/usb/xhci_baremetal.hpp` — xHCI fisico, path HID specializzato.
- `include/micant/km/drivers/net/i210_baremetal.hpp` — Intel I210 bare-metal.
- `include/micant/km/drivers/usb/usb.hpp`, `xhci.hpp`, `include/micant/km/drivers/input/*` — modelli/test/contratti USB e HID.
- `CMakeLists.txt`, `qemu.py`, `scripts/verify_kernel_architecture.py` — build e verifica attiva.

**Limite di questo audit:** confronto statico dei file GitHub al commit indicato; non è stata eseguita una compilazione, un boot QEMU né una prova su hardware fisico. La matrice definitiva delle funzioni recuperabili deve essere confermata da build e test dei componenti importati.
