# MicaNT — WFP Firewall Bare-Metal Integration Specification

**Stato:** proposta di implementazione, non dichiarazione di funzionalità completate  
**Target:** AMD64, C++23, kernel NT modulare canonico MicaNT  
**Percorso proposto nel repository:** `docs/KERNEL_FIREWALL_BAREMETAL_INTEGRATION_SPEC.md`  
**Ambito:** completare e integrare il firewall **WFP esistente** sul percorso dati reale della rete bare-metal.  
**Dipendenze:** M10 Native Networking Stack (NIC/NDIS, TCP/IP, AFD), primitive Io/Ke/Mm/Ps/Se necessarie.

## 1. Obiettivo e vincoli non negoziabili

Il firewall deve classificare **pacchetti realmente ricevuti e trasmessi** da una NIC controllata dal kernel MicaNT. L'azione BLOCK deve impedire la consegna al TCP/IP o la trasmissione alla NIC; PERMIT deve consentire il normale percorso senza alterare i dati. Le regole di gestione, quando abilitate, devono aggiornare **lo stesso** stato decisionale usato in Ring 0.

Questa specifica **non** autorizza la creazione di un altro firewall, un motore nftables/iptables, una VM JS/eBPF, un nuovo stack TCP/IP o un servizio che classifichi pacchetti per conto del kernel. Non accettare test hosted, registrazioni simboliche di servizi, UI o contatori sintetici come prova di protezione reale. Conservare gli ABI WFP pertinenti, rispettando l'architettura NT di MicaNT.

Criterio minimo di completamento: una connessione IPv4 testata tramite NIC QEMU viene rifiutata da una regola BLOCK effettivamente installata; viene consentita da una regola PERMIT; aggiornare/eliminare la regola modifica il risultato senza riavvio; default inbound block e risposte correlate sono esercitati da traffico reale e contatori verificabili.

## 2. Inventario verificato nel fork e gap

| Sorgente esistente | Da riutilizzare | Gap specifico da chiudere |
|---|---|---|
| `include/micant/km/drivers/net/wfp.hpp` | `WfpKernelFilterEngine`, `WfpConntrackEngine`, registry callout, struct WFP, contatori, IOCTL | Header dipendente anche da librerie hosted/user-mode; classificazione non collegata al datapath bare-metal; politica/state management e locking da irrobustire |
| `include/micant/km/drivers/net/tcpip.hpp` | `NetworkStack`, callback `setPacketFilterCallback`, parser IP/transport | Backend bare-metal reale e punti di controllo RX/TX da verificare; una callback installata non dimostra transito dei pacchetti |
| `include/micant/km/drivers/net/ndis.hpp` | NBL, adapter/packet contracts | Percorso NIC reale e lifetime buffer da integrare nel target AMD64 |
| `include/micant/um/subsystems/net/fwpuclnt.hpp` | FWPM API/strutture e interfacce management | `SovereignWfpManager` mantiene una collezione di regole user-mode distinta dal kernel: non deve restare fonte di decisione autonoma |
| `kernel/io/afd_wsk.cpp` | Interfaccia socket/trasporto | AFD non attesta da solo il funzionamento firewall |
| `tools/firewall_app.cpp` | UI diagnostica preesistente | Testo/status statici non equivalgono a stato e contatori reali |
| `CMakeLists.txt` | Build modulare e `ntoskrnl_core_baremetal` | WFP e relativo datapath non fanno parte dell'eseguibile bare-metal verificato |

### 2.1 Difetti concreti individuati da correggere

1. `InitializeWfpKernelDriver()` installa una callback in `tcpip::NetworkStack`, ma registra anche esportazioni e servizi tramite `DynamicLoader`/`SCM` hosted e ne segna lo stato `RUNNING`: separare installazione **funzionale** del filtro da registrazione diagnostica/servizi. Stato `RUNNING` solo se il percorso operativo è inizializzato.
2. In `WfpKernelFilterEngine::classifyPacket()`, `WfpConntrackEngine::processPacket()` viene chiamato **prima** della decisione sulle regole; inoltre un flusso `isEstablished` viene immediatamente consentito prima dell'esame delle regole. Non deve essere possibile che un pacchetto negato crei una voce autorizzata o che una regola BLOCK venga scavalcata da conntrack.
3. Il motore conserva regole e contatori dentro un `std::mutex`; questa implementazione non è automaticamente adatta all'IRQL del percorso RX/TX e allo scheduler bare-metal. La classificazione non deve attendere mutex sleeping, allocare da heap pageable o chiamare funzioni hosted.
4. Il parsing usa puntatori interpretati come `TcpHeader`/`UdpHeader` dopo controlli di lunghezza minimi; occorre gestire header lengths, alignment, checksum/fragmentazione, bounds e letture non allineate senza UB.
5. L'API `fwpuclnt.hpp` classifica anche pacchetti user-mode e mantiene regole indipendenti: impedire stati disaccoppiati. **Solo Ring 0 decide** sul traffico.
6. Preservare la semantica preesistente inbound BLOCK/outbound ALLOW come baseline del primo milestone, senza permessi hardcoded per porte come 22/80/443 soltanto perché elencati nella UI.

## 3. Topologia definitiva: un solo WFP

```text
Ring 3
  fwpuclnt.dll / tools/firewall_app.cpp (management, query, UI)
       | NT handle + IOCTL/IPC autenticato e validato
       v
Ring 0
  Io manager / device WFP management (controllo, NON datapath)
       |
       v
  WfpKernelFilterEngine (policy canonica, counters)
       |  ^
       |  +---- WfpConntrackEngine (solo flussi autorizzati)
       |
  inbound: NIC -> NDIS -> [WFP IPv4/IPv6 input] -> TCP/IP -> AFD
  outbound: AFD -> TCP/IP -> [WFP IPv4/IPv6 output] -> NDIS -> NIC
       +---- ALE auth (connect/accept) quando disponibili identità Ps/Se reali
```

- Il driver di rete consegna pacchetti al **solo stack TCP/IP canonico**.
- Il punto RX classifica prima che il pacchetto raggiunga i protocolli socket; TX prima che il frame venga trasmesso.
- Se in seguito si implementano livelli transport e ALE, evitare doppia classificazione inconsistente. La decisione per un dato layer deve essere esplicita, verificabile e con ordine dichiarato.
- Nessuna richiesta bloccante verso Ring 3 durante RX/TX.
- Se il firewall non è disponibile, applicare una policy di bootstrap **esplicita e sicura**; non fingere che il firewall sia attivo.

## 4. Sequenza di integrazione nel kernel

### 4.1 Separare il codice freestanding da quello hosted

1. Estrarre dal materiale preesistente il **core** WFP: tipi, rule matching, conntrack, statistiche, controllo callout.
2. Spostare DynamicLoader, SCM, VersionDatabase, `std::string`/`std::vector` e dipendenze incompatibili con il freestanding fuori dalle compilation unit critiche, senza duplicare semantiche o mantenere due engine operativi.
3. I tipi ABI condivisi devono restare in header shared appropriati e validati con `static_assert` su size, alignment e offset, se pertinenti.
4. Il nuovo adattatore di inizializzazione chiamerà l'engine kernel reale dopo il bring-up sufficiente di Ke/Mm/Io e **prima di abilitare il traffico NIC**.
5. Aggiornare il manifest `ntoskrnl_core_baremetal` per compilare e linkare soltanto le unità freestanding canoniche. Non considerare l'inclusione di un header una prova che il codice sia linkato o in esecuzione.

### 4.2 Sequenza di boot

1. `Ke/Mm/Io` pronti per buffer, lock e temporizzazioni.
2. WFP alloca pool e stato minimo e installa default policy.
3. WFP registra l'interfaccia di classificazione nel datapath TCP/IP canonico.
4. NIC/NDIS avviano RX/TX solo dopo che l'hook WFP è funzionante; se l'ordine imposto dal bring-up differisce, usare blocco esplicito fino alla readiness.
5. Pubblicare `WFP_READY` solo a init completata e test di sanità dei puntatori effettuati; no stato di successo presunto.
6. La gestione FWPM Ring 3 diventa disponibile quando esistono processi, handle, sicurezza e I/O management.

### 4.3 ABI di callback interna

Definire un contratto piccolo, non un secondo firewall. Input: direzione, layer, versione IP, interfaccia, indirizzi, protocollo, porte **solo se validamente decodificate**, flag TCP, buffer read-only/length, metadati opzionali di flow/processo. Output: `Permit`, `Block` oppure errore interno controllato; distinguere chiaramente packet-drop, reject e caso unsupported. Ogni risultato deve essere attribuibile a `ruleId` o `defaultPolicy`.

Non conservare puntatori ai buffer NDIS dopo il ritorno senza esplicito ownership/refcount. Per fragments IP, evitare di inventare L4 ports: ricomposizione o politica di frammentazione deterministica e documentata. Traffico IPv6 non deve eludere le regole IPv4/default; se IPv6 non è supportato, disabilitarlo o applicare una default policy restrittiva.

## 5. Correzione del conntrack: autorizzazione prima dello stato

Il principio è: **un flusso non è attendibile perché è stato osservato; lo è dopo autorizzazione**.

1. Validare pacchetto e metadati prima di qualsiasi update.
2. Cercare flow esistente e rilevare stato TCP/UDP in modo read-only.
3. Verificare **regole esplicite BLOCK** applicabili, incluse eventuali regole per identity/flow, *prima* del fast path established. Rendere deterministica la precedenza per layer e peso; non introdurre arbitrariamente "block overrides all" se ABI/precedenze WFP differiscono: i conflitti vanno risolti secondo il contratto adottato, documentato e testato.
4. Valutare policy e regole PERMIT; per il traffico di risposta usare solamente un flow precedentemente autorizzato e non revocato dalla policy corrente.
5. **Solo dopo PERMIT**, creare/aggiornare la voce conntrack; su BLOCK non creare nuova autorizzazione.
6. Alla modifica regole definire una politica esplicita sui flussi esistenti. Prima release: cambi di policy invalidano/riclassificano i flussi interessati in modo sicuro, senza consentire implicitamente connessioni prima negate.
7. Per TCP verificare almeno handshake SYN/SYN-ACK/ACK, RST, FIN, retransmission e timeout; per UDP usare finestre temporali e tuple bidirezionali delimitate. ICMP echo e risposte devono seguire regole non ambigue.
8. Conntrack deve avere cap su numero di flow, eviction, lifetime e resistenza ad attacchi con SYN flood; nessuna crescita RAM illimitata.

## 6. Concorrenza, IRQL, memoria e resilienza

- Il percorso pacchetto opera a un IRQL documentato dal chiamante: non usare sleeping mutex, librerie CRT o `std::chrono` hosted nel fast path. Scegliere spinlock corti dove consentiti, o strutture snapshot immutabili pubblicate atomicamente.
- Aggiornamento regole: costruzione e validazione a `PASSIVE_LEVEL`, commit atomico dello snapshot; reclamazione differita con lifetime/refcount/epoch compatibile Ke. Non liberare regole ancora lette da un core.
- Pool di conntrack preallocato o allocazioni nonpaged con limiti; niente allocazioni dipendenti da input arbitrario in ISR/DPC.
- Statistiche per-CPU o atomiche; logging fuori dal fast path e con code bounded; non stampare ogni pacchetto su seriale.
- Gestire detach NIC, reset, shutdown, cancellazioni e liberazione risorse senza use-after-free; rimuovere hook prima della distruzione dell'engine.
- Timeout e condizioni di allocazione fallita producono risultati definiti; non chiamare indistintamente `PERMIT` per errori o protocolli sconosciuti.

## 7. Regole, gestione e sicurezza

**Unica autorità:** `WfpKernelFilterEngine` in Ring 0. Le FWPM API user-mode sono client di configurazione, non un secondo classificatore né una sorgente di regole parallela.

1. `FwpmEngineOpen0`, `FwpmFilterAdd0`, `FwpmFilterDeleteById0` e query devono usare handle e I/O autenticati, verificando permission e ownership, con copia degli input da Ring 3 in memoria kernel validata.
2. Se la gestione Ring 3 non è ancora eseguibile, iniziare con regole bootstrap statiche **nel medesimo engine**, non con un manager alternativo. Non dichiarare completata la gestione FWPM finché la serializzazione reale non esiste.
3. Applicare policy anti-tamper: solo amministratori/autorizzati possono variare firewall o profili; impedire che processi standard disabilitino il filtro.
4. Controllare overflow delle strutture, GUID/layer sconosciuti, range porte, CIDR, protocollo, dimensione delle regole e compatibilità ABI.
5. Per regole per applicazione, usare metadati autorevoli Ps/Se del processo/endpoint; il path visualizzato non deve essere la fonte primaria di identità. Se non disponibili, rimandare le regole per processo anziché fidarsi di un campo caller-controlled.
6. Rendere la GUI `firewall_app.cpp` un vero consumer delle query di stato e statistiche, oppure etichettarla diagnostica non funzionale. Non visualizzare “protected” o “running” basandosi su stringhe hardcoded.
7. Persistenza regole rimandabile finché non è disponibile l'I/O persistente M14; nell'immediato usare policy bootstrap firmata/compiled e aggiornamenti non persistenti esplicitamente documentati.

### 7.1 Semantica minima policy

- Inbound: **BLOCK predefinito** per nuove connessioni non autorizzate.
- Outbound: **ALLOW predefinito** nella release iniziale, ma esplicitamente configurabile.
- Traffico di risposta: PERMIT soltanto se correlato a un flow autorizzato e compatibile con la policy corrente.
- Loopback, ARP, DHCP, ICMP e traffico necessario al bring-up devono avere casi espliciti, niente whitelist indiscriminata silenziosa.
- Se non esiste alcuna NIC o TCP/IP, WFP può inizializzarsi ma la fase network protection resta **non verificata**.

## 8. Piano di implementazione mirato (nessun secondo motore)

| Fase | Modifica richiesta | Evidenza per chiudere |
|---|---|---|
| FW0 — Audit/link | Selezionare il core WFP canonico, rimuovere dipendenze hosted dai path attivi, aggiungerlo al manifest bare-metal | Compile/link freestanding, symbol map, boot trace `WFP_READY` |
| FW1 — Datapath | Collegare ingress/egress TCP/IP o NIC senza bypass, con parsing bounds-checked | Pacchetti QEMU raggiungono WFP e BLOCK altera outcome reale |
| FW2 — Regole | Implementare matching coerente, default policy e precedence; assenza falsi successi | Test BLOCK/PERMIT/default su traffico vero |
| FW3 — Conntrack | Prevenire bypass `established`, tenere solo flussi autorizzati, timeout/eviction | Test handshake e modifica policy mentre flow è attivo |
| FW4 — Harden | Lock/IRQL/DMA ownership, input ostile, resize e disconnect | Stress, fuzz dei parser, no crash/leak/lockup |
| FW5 — Management NT | Collegare fwpuclnt e IOCTL/handle al motore kernel, policy immutabile/atomica | Da processo Ring 3 regola add/delete altera pacchetto successivo |
| FW6 — Telemetria/UI | Contatori reali, query statistiche e stato senza hardcode | Contatori coerenti con PCAP QEMU, UI non sintetica |
| FW7 — IPv6/ALE | Completare layer v6 e per-process se già pronti nel datapath e in Ps | Traffico v6 e identity tests reali; nessun fallback permissivo |

FW0–FW3 sono il percorso minimo per un firewall operativo; FW4 necessario prima di trattarlo come robusto; FW5–FW7 completano la superficie WFP prevista. Non marcare un FW come concluso sulla sola base di test di unità.

## 9. Test verificabili

### 9.1 Test hosted (non sufficienti per il completamento)

- Tabelle di verità per direzione, protocollo, indirizzi, range porte, default, peso/precedenza.
- Packets malformati: IPv4/IHL, TCP data offset, UDP length, IP fragments, header truncated, endianess, overlapping/zero lengths.
- Conntrack: SYN senza autorizzazione non crea flow, SYN+FIN drop, handshake valido, retransmission, FIN/RST, timeout, eviction, policy update mentre connessione è attiva.
- Stress: rule add/delete concurrente alla classificazione, cancellazioni callout, migliaia di flow, limiti RAM.

### 9.2 QEMU bare-metal (obbligatori)

**Prerequisiti:** NIC emulata con RX/TX reale (es. VirtIO-net) collegata allo stack canonico, IP configurato, host collegato a una rete di test controllata. Se M10 non ha ancora connettività end-to-end, testare FW0 con fixture kernel e dichiarare FW1+ bloccati, non completati.

1. Verificare manifest/link WFP nel kernel che QEMU avvia, versione build e init logs.
2. Avviare un listener TCP/UDP dove supportato e inviare da host un pacchetto/connessione su porta vietata: osservare drop e assenza consegna ad AFD; verificare che il counter BLOCK incrementi.
3. Installare una regola PERMIT per quella tuple/protocollo: ripetere la connessione, vedere successo end-to-end e contatore PERMIT.
4. Rimuovere la regola: una nuova connessione deve essere negata; verificare policy per la connessione già attiva.
5. Avviare connessione outbound permessa e verificarne il traffico di risposta con conntrack; un pacchetto inbound non correlato deve restare negato.
6. Provare input invalidi/frammentati/IPv6 e verificare che nessun percorso bypassi il filtro o mandi in crash il kernel.
7. Registrare pcap/tcpdump lato host, contatori kernel e log seriali; correlare eventi per numero/protocollo, tenendo conto degli ARP/retx.
8. Simulare policy update e reset NIC durante traffico: nessuna deadlock, nessuna memoria liberata ancora in uso.

### 9.3 Hardware fisico

Dopo le prove QEMU e la disponibilità di un driver NIC: rete isolata, nessun servizio esposto per default, console diagnostica, test di connessione su una macchina di laboratorio, successivo scollegamento/reset del dispositivo. Non collegare il sistema a Internet pubblica prima della validazione di input malformato, isolamento e politica default.

## 10. Acceptance / Definition of Done

La feature è conclusa **solo** quando:

- WFP è incluso nel target kernel bare-metal e inizializzato prima di abilitare traffico RX/TX.
- Il medesimo engine applica PERMIT/BLOCK al traffico realmente acquisito dalla NIC e diretto a TCP/IP, e a quello in uscita.
- Gli aggiornamenti rule/classify sono concorrenti e sicuri a IRQL appropriato, senza API hosted.
- Conntrack **non** autorizza flussi negati e non aggira BLOCK dopo aggiornamenti di policy.
- Le regole non dipendono dal valore falsificabile di path o metadati user-mode.
- Esistono test QEMU riproducibili, pcap/contatori coerenti e prove positive/negative; non solo screenshot della GUI.
- Le implementazioni e simulazioni hosted non sono più dichiarate come supporto hardware produttivo.
- Mancanze di IPv6, ALE, gestione dinamica o persistenza sono dichiarate in modo esplicito, senza claim di compatibilità WFP completa.

## 11. Esclusioni

Non rientrano nel task: importare Netfilter, implementare CLI iptables/nftables, nuova VM, ispezione TLS/HTTPS, deep packet inspection generica, IDS/antivirus, VPN, NAT, proxy, HTTP parser, kernel scripting, UI redesign, nuovo driver NIC, completamento totale di NDIS/AFD/TCPIP (salvo adapter minimi indispensabili al punto di filtro). Non usare task laterali per sostituire il deliverable centrale.

## 12. Istruzioni operative per l'agente di sviluppo

1. Modificare **solo il kernel canonico modulare** e le superfici esistenti pertinenti; non ripristinare `old/monolithic-kernel/`.
2. Prima verificare la revisione locale e gli eventuali cambiamenti non committati, poi compilare la baseline, così da non sovrascrivere lavoro in corso.
3. Usare i sorgenti citati come base; rifattorizzare per arrivare a un solo `WfpKernelFilterEngine` reale. Non introdurre nuovi sistemi di policy paralleli.
4. Per ogni fase produrre build bare-metal e test QEMU, mantenendo test hosted solo come supporto; evitare contatori o return success precompilati per fingere funzionamento.
5. Documentare esattamente la feature verificata e ciò che ancora manca, con output riproducibile e dipendenze di M10 esplicite.
6. Preferire riuso di codice esistente; studiare WDK/ReactOS solo per contratti e comportamento dove serva. Rispettare licenze e separare materiale con vincoli incompatibili.
7. Non fermarsi ai primi simboli linkati o alla comparsa di `WFP_READY`: la milestone richiede l'effetto **osservabile su pacchetti reali**.

---

**Principio guida:** *Completare il WFP che MicaNT possiede già, eliminare ogni falso percorso di successo, e provarne l'efficacia nel kernel AMD64 avviato in QEMU.*
