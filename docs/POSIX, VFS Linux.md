# MicaNT x64 — Specifiche implementative POSIX, VFS Linux-style e sh.exe

**Versione 1.0 — 11 novembre 2026**\
**Destinazione prevista nel repository:** `docs/POSIX_VFS_SH_IMPLEMENTATION_SPEC.md`\
**Stato:** specifica da implementare, non certificazione di funzionalità esistenti.

## 1. Obiettivo e principi vincolanti

Realizzare su MicaNT AMD64 un sottosistema POSIX per programmi nativi PE/COFF x64, con libc C/POSIX user-mode, namespace filesystem Linux-style e shell interattiva `sh.exe`, simile a `cmd.exe` nel ruolo ma con comandi e sintassi Unix.

Il progetto mantiene **un unico kernel NT**: Ke, Mm, Ps, Ob, Io, Se, FsRtl e un solo VFS. POSIX è una personalità applicativa, non un secondo kernel, un Linux syscall layer o un secondo filesystem.

**Regole non negoziabili:**

- Supportare esclusivamente AMD64/x86_64. Non introdurre ARM64.

- Rispettare `AGENTS.md`, il kernel modulare e `docs/KERNEL_ARCHITECTURE.md`.

- Non usare sorgenti `old/monolithic-kernel` nel prodotto.

- Non includere header privati `micant/km/` nelle DLL e nei programmi user-mode.

- Non utilizzare funzioni del macOS/Linux host per simulare il filesystem durante l'esecuzione in MicaNT.

- Non creare PID, processi, file, output, driver o mount fittizi.

- Rispondere con errori verificabili alle funzioni non ancora implementate.

- Test QEMU reali obbligatori per ogni milestone integrativa; ASan/UBSan hosted solo come verifica complementare.

- Non distogliere lo sviluppo principale da Mm/Ps/Io e dall'avvio di veri processi Ring 3.

- POSIX.1-2024 / Single UNIX Specification Issue 8 è il riferimento per il sottoinsieme scelto; nessuna pretesa di conformità completa fino a test di conformità.

## 2. Baseline del repository da riutilizzare

Verificare lo stato attuale prima di modificare i file:

| Percorso | Stato/azione |
| - | - |
| `include/micant/um/native/posix.hpp` | Contiene tipi, server e API simulate. Separare ABI user-mode da dati interni; riutilizzare soltanto contratti utili. |
| `include/micant/km/drivers/fs/fs.hpp` | VFS canonico MicaNT: aggiungere adapter POSIX senza duplicare i dati dei file. |
| `kernel/io/iomgr.cpp` | Owner canonico di FILE_OBJECT e IRP. |
| `kernel/mm/virtual.cpp`, `include/micant/km/executive/section.hpp` | Backend per mmap/munmap/mprotect e mapping condivisi. |
| `kernel/ob/obref.cpp` | Object/handle lifecycle e autorizzazioni. |
| `kernel/ps/address_space.cpp`, `include/micant/km/executive/ps.hpp` | Address space e processi reali; requisito di spawn/exec. |
| `include/micant/km/core/syscalls.hpp` | Usare syscall NT native, non una tabella Linux parallela. |
| `tools/cmd_app.cpp` | Modello del target eseguibile PE Ring 3, non copiare il comportamento simulato. |
| `tools/bash_app.cpp` | Attualmente mostra output dimostrativo: non costituisce una shell reale. |
| `CMakeLists.txt`, `scripts/stage_micant.py`, `qemu.py` | Integrare build, staging, avvio e verifiche. |

**Difetti da eliminare:** `psx_getpid()` restituisce un PID fisso; `fork()` copia metadati invece di clonare un processo; `execve()` cambia una stringa senza caricare l'immagine; `open()` usa una mappa di byte privata; `posix.hpp` include header kernel da codice user-mode. Non estendere questi comportamenti come implementazione finale.

## 3. Architettura

```text
   sh.exe   ls.exe   cp.exe   mv.exe   rm.exe   programmi POSIX C
      \        |        |        |        /
                    MicaPOSIX libc
                          |
                   NTDLL / NT API
                          |
                   AMD64 SYSCALL
                          |
              MicaNT NT Executive Ring 0
                Ps - Ob - Mm - Io - Se
                          |
                     VFS canonico
                     /          \
              namespace NT   mount POSIX
                   \            /
                    stessi file
```

**Modelli di proprietà:**

1. Ogni processo POSIX è un vero processo `EProcess` con CR3, thread Ke e tabella handle Ob. I dati POSIX aggiuntivi sono PID POSIX ove necessario, cwd, umask, UID/GID, environment, table FD e segnali.

2. Ogni FD è un piccolo intero per-processo che referenzia una **open-file description**. Più FD duplicati condividono offset e status flags, non `FD_CLOEXEC`.

3. Ogni file sottostante è un vero oggetto VFS/Io, non un `std::vector` di contenuto in un server globale.

4. Le DLL user-mode usano ABI POD/shared e syscall; nessun accesso diretto a strutture C++ kernel.

5. UID 0 non bypassa automaticamente la sicurezza NT; definire mapping credenziali ↔ token Se.

## 4. ABI POSIX / libc

Costruire una libc POSIX user-mode PE x64 con esportazioni C, tipi e calling convention validati, inizializzazione TLS ed `errno` **per thread**.

**Primo sottoinsieme obbligatorio:**

- Identità: `getpid`, `getppid`, `getuid`, `geteuid`, `getgid`, `getegid`.

- File: `open`/`openat`, `close`, `read`, `write`, `lseek`, `pread`, `pwrite`, `stat`, `fstat`, `lstat`, `unlink`, `rename`, `access`.

- Directory: `opendir`, `readdir`, `closedir`, `mkdir`, `rmdir`, `getcwd`, `chdir`.

- Descriptor: `dup`, `dup2`, `fcntl`, `O_CLOEXEC`, stdin/stdout/stderr, `isatty`, `pipe`/`pipe2`.

- Memoria: `mmap`, `munmap`, `mprotect`, `msync` quando i backend lo consentono.

- Processi: `posix_spawn`, `posix_spawnp`, `waitpid`/`wait`, `_exit` ed exit code reali.

- Successivi: `execve`, `fork` con COW, `sigaction`/`kill`, `pthread_create`/`join`, mutex/condvar, clock e timer.

**Regola API:** le funzioni POSIX restituiscono normalmente `-1` e impostano `errno`; non esporre direttamente NTSTATUS o negativi errno come risultato C. Definire mapping NTSTATUS→errno testato per `ENOENT`, `EACCES`, `EBADF`, `EISDIR`, `ENOTDIR`, `EEXIST`, `EINTR`, `EAGAIN`, `ENOSPC`, `EXDEV`, `EROFS`, `ENOSYS`.

`fork()` deve restare `ENOSYS` finché non clona stato d'esecuzione reale, address space COW, FD e processo schedulabile. Per la shell iniziale preferire `posix_spawn`.

## 5. VFS compatibile con namespace Linux

**Obiettivo:** due sintassi di pathname verso lo stesso backend e gli stessi byte.

| POSIX | Risoluzione richiesta |
| - | - |
| `/` | root mount POSIX, reale |
| `/bin`, `/usr/bin` | eseguibili reali POSIX PE x64 visibili via PATH |
| `/etc` | configurazione su volume montato |
| `/home/<user>` | home reale su volume NT |
| `/tmp` | tmpfs o volume temporaneo con semantica documentata |
| `/var` | dati variabili su volume |
| `/mnt/c` | vista del volume `C:` MicaNT |
| `/mnt/d` | vista del volume `D:` solo se presente |
| `/dev/null` | lettura EOF, scritture scartate |
| `/dev/zero` | letture a zero, scritture scartate |
| `/dev/tty` | terminale controllante reale, altrimenti errore |
| `/dev/urandom` | solo con CSPRNG correttamente inizializzato |
| `/proc` | procfs read-only con dati veri Ps/Mm, non valori inventati |
| `/sys` | facoltativo; esporre soltanto proprietà hardware attendibili |

Esempio: `/mnt/c/Users/admin/a.txt` e `C:\Users\admin\a.txt` devono referenziare lo stesso file, non copie sincronizzate.

**Resolver e mount:**

- mount table con source volume, target path, tipo, flags, reference count; matching longest-prefix a confini di componente;

- cwd e root per-processo; normalizzare `.`, `..` e slash ripetuti;

- seguire symlink con massimo 40 passaggi, rilevare `ELOOP`, gestire `O_NOFOLLOW`;

- percorso UTF-8 con conversione controllata in Unicode NT;

- case sensitivity secondo volume reale e policy mount: non dichiarare distinzione case-insensitive come piena compatibilità POSIX;

- autorizzazioni su attraversamento directory, lettura, scrittura, rename, unlink e mount;

- `openat` relativo a directory FD per ridurre TOCTOU;

- `rename` tra filesystem distinti ritorna `EXDEV`;

- unmount deve respingere mount busy o gestirne correttamente le reference;

- uso reale di NT `FILE_OBJECT`, IRP e FsRtl per file e directory.

**Metadata:** `stat` deve derivare dimensioni, mode/type, timestamp, dev/inode e link count da dati reali. Limitazioni FAT o driver specifici vanno dichiarate, mai mascherate con numeri fissi.

**Piano storage:** prima dimostrare un filesystem persistente reale e uno temporaneo; il fatto che siano presenti driver NTFS/ext4/Btrfs negli header non dimostra che siano integrati in QEMU.

## 6. File descriptor e pipe

Una tabella FD è proprietà del processo, non del singleton POSIX.

- `open` alloca FD e riferimento al FILE_OBJECT; `close` rilascia riferimento e slot.

- `dup/dup2` condividono offset e status flags; `FD_CLOEXEC` resta per FD.

- `O_APPEND` atomico; `O_CREAT|O_EXCL` atomico; `O_TRUNC` solo dopo controlli di permesso.

- Offset a 64 bit; `pread/pwrite` non spostano l'offset condiviso.

- `read/write` devono supportare conteggi parziali e EOF.

- Pipe: read blocca con writer aperto e buffer vuoto; read ritorna 0 dopo chiusura ultimo writer; write senza reader genera `EPIPE` e semantica `SIGPIPE` quando disponibile.

- Handle e reference vanno liberati in `exec` per `FD_CLOEXEC` e alla terminazione del processo.

- Nessuna lettura o scrittura di buffer Ring 3 senza validazione tramite la boundary NT esistente.

## 7. sh.exe — prodotto e funzionalità

**Nome binario esatto:** `sh.exe`, PE32+ x64, Ring 3, build e staging nativi. Non usare `bash_app.cpp` come implementazione e non chiamarlo Bash reale.

**Invocazioni:**

```text
sh.exe
sh.exe -c "echo hello; pwd"
sh.exe -i
sh.exe /home/admin/script.sh
```

Prompt esemplificativo `admin@micant:/home/admin$ `, generato da identità/cwd reali.

### 7.1 Parsing e shell engine

Implementare lexer e parser robusti con:

- argomenti, whitespace, quote singole/doppie, escape, commenti;

- variabili shell, export, espansione `$VAR` e `$?`;

- comandi separati da `;` e newline;

- redirezioni `<`, `>`, `>>`, `2>`, `2>&1`;

- pipeline `|` con FD reali e processi distinti;

- condizioni `&&` e `||`;

- script `#!` e invocazione interpreter;

- `if`/`then`/`else`, `for`, `while` in milestone successiva;

- background `&`/job control solo dopo process group, segnali e terminale controllante.

Builtin con modifica dello stato della shell: `cd`, `pwd`, `exit`, `export`, `unset`, `set`, `umask`, `echo`, `printf`, `read`, `true`, `false`, `help`.

I comandi esterni devono avviarsi attraverso `posix_spawn`/Ps ed ereditare correttamente FDs, cwd e ambiente. Redirection e pipeline non devono essere simulate concatenando stringhe dentro il processo shell.

Console: stdin/stdout/stderr reali, prompt e input da tastiera, backspace, Ctrl+C, Ctrl+D, history minima quando possibile, testabile anche in seriale QEMU. `-c` noninterattivo obbligatorio prima del lancio GUI.

### 7.2 Utility Unix reali

| Priorità | Eseguibili | Requisiti |
| - | - | - |
| P0 | `ls` `cat` `cp` `mv` `rm` `mkdir` `rmdir` `touch` `pwd` | Directory listing, veri byte I/O, rename/unlink e codici di errore |
| P1 | `grep` `find` `head` `tail` `wc` | Lettura streaming, ricerca e pipe |
| P1 | `sort` `uniq` `cut` `tr` `tee` `xargs` | Filtri stdin/stdout realmente concatenabili |
| P1 | `ln` `readlink` `stat` `chmod` `env` `which` `uname` `date` | Metadati, symlink e variabili reali |
| P2 | `ps` `kill` `sleep` `mount` `df` `du` | Dipendono da Ps, Ke e mount reali |

Opzioni minime: `ls -a -l`, `cp -r`, `mv` anche con overwrite gestito, `rm -r -f`, `mkdir -p`, `grep -n -i`, `find -name`, `wc -l`. Restituire errori su stderr e codici d'uscita appropriati.

**Vincolo:** `uname` deve indicare MicaNT e non fingersi Linux 6.x. Niente output di directory o processi precompilato nel binario.

## 8. Processi, fork, memoria, segnali

MicaPOSIX è una personalità sopra Ps. Nessun `PosixProcess` metadata-only può essere presentato come processo reale.

- `posix_spawn` crea un EProcess NT con address space CR3, carica eseguibile PE, crea thread, prepara argv/env e FD, lo schedula e registra exit status.

- `waitpid` attende/riapre il figlio con stati reali; `WNOHANG` corretto; niente `ECHILD` inventato per figlio ancora vivo.

- `execve` sostituisce l'immagine mantenendo identità del processo e corrette regole FD quando supportato; altrimenti `ENOSYS`.

- `fork` richiede duplicazione reale dell'esecuzione e COW. Test parent/child che scrivono sulle stesse VA e osservano valori indipendenti, mentre i FD duplicati condividono offset.

- `mmap(MAP_ANONYMOUS|MAP_PRIVATE)` va su Mm; `MAP_SHARED` su section/PFN; `MAP_PRIVATE` file-backed richiede COW autentico.

- `pthread` necessita veri thread Ps/Ke, TLS, mutex e wait; vietato `std::thread` host nel runtime.

- I segnali necessitano lifecycle Ps/Ke, permessi Se e trampolino user-mode verificato. Le eccezioni hardware non si traducono in SIGSEGV semplicemente rinominandole.

## 9. Build e qualità

Target desiderati: `micant_posix_libc` user-mode, `sh_boot` o naming coerente con CMake esistente e utility PE x64 compilate. Integrare in staging soltanto eseguibili reali.

Controlli:

- nessun import o header kernel privato nelle DLL/applicazioni;

- PE ABI, symbol/export resolution e stack alignment AMD64;

- compilazione incrementale mirata, build QEMU per checkpoint funzionali;

- niente `std::filesystem`/`open` macOS come scorciatoia nel native target;

- niente demo text al posto di operazioni file/processo;

- failure injection, cleanup, leak check handle/PFN/FD.

## 10. Test di accettazione QEMU

Avviare da una console MicaNT reale, su volume test scrivibile:

```sh
sh.exe -c "pwd; ls -la /; ls /mnt/c"
sh.exe -c "mkdir -p /tmp/demo && echo hello > /tmp/demo/a.txt"
sh.exe -c "cp /tmp/demo/a.txt /tmp/demo/b.txt && cat /tmp/demo/b.txt"
sh.exe -c "mv /tmp/demo/b.txt /tmp/demo/c.txt && ls /tmp/demo"
sh.exe -c "cat /tmp/demo/c.txt | grep hello | wc -l"
sh.exe -c "rm /tmp/demo/c.txt /tmp/demo/a.txt && rmdir /tmp/demo"
```

Accettare solo se:

1. `sh.exe` è realmente in CPL3, con processo Ps e CR3 propri.

2. `ls.exe`, `cp.exe`, `mv.exe` e `rm.exe` vengono davvero eseguiti, non solo imitati dal parser.

3. I file creati con POSIX sono aperti via percorso Windows e viceversa, con contenuto identico.

4. Pipeline sono processi concorrenti con pipe kernel; EOF, redirection ed exit code corretti.

5. Permessi, assenza percorso, rename cross-volume e errori FD producono `errno` corretti.

6. Chiudere file/processi riporta a zero reference e PFN/handle/FD allocati dal test.

7. Riavviare QEMU preserva i dati nel volume dichiarato persistente.

8. Regressioni UEFI, CPL3, syscall, VirtIO e kernel modulari restano verdi.

I test parser, UTF-8, path resolver e flag mapping possono eseguire anche con sanitizzatori hosted, ma non contano come prova Ring3.

## 11. Milestone agentiche (in ordine di dipendenza)

| Milestone | Deliverable verificabile |
| - | - |
| P0 — ABI e audit | Separazione header, errno/TLS, nessun fake success e analisi dei requisiti Io/Ps |
| P1 — Io / filesystem reale | FILE_OBJECT, directory, read/write/rename/unlink testati in QEMU su volume persistente |
| P2 — POSIX mount namespace | `/mnt/c` e `C:` puntano agli stessi oggetti; `/tmp`, `/dev`, mount e cwd corretti |
| P3 — libc e FD | `open/read/write/close/stat/readdir/dup` da un vero PE Ring3; pipe e cleanup |
| P4 — sh.exe e utility base | sh -c, ls/cat/cp/mv/rm/mkdir/touch con operazioni reali |
| P5 — spawn e pipeline | Due processi Ps, due CR3, pipe kernel, wait e exit status in QEMU |
| P6 — shell scripting e filtri | grep/find/head/tail/wc e script, redirezioni e conditional execution |
| P7 — POSIX esteso | mmap completo, signals, pthread, execve e fork/COW quando i prerequisiti funzionano |

Milestone completata soltanto con codice integrato, link PE valido, QEMU e acceptance test. Se Ps/Io bloccano, correggere l'owner canonico o procedere con parser/API indipendenti senza simulare il risultato.

## 12. Organizzazione del lavoro

Possibili worker separati per:

- Io/FsRtl: filesystem e namespace;

- Ps/Ob: fd lifecycle, processo, spawn;

- libc/ABI: interfacce e traduzione errno;

- sh: parser, utility e output streaming;

- integrator: build, staging, QEMU, documentazione.

Usare worktree distinti e commit piccoli, senza sovrapposizione sui medesimi file. Non ricompilare tutto ad ogni intervento; QEMU alla fine di ogni batch rilevante.

Aggiornare `docs/KERNEL_INTEGRATION_STATUS.md` con stato **analizzato / compilato / linkato / esercitato in QEMU / integrato / verificato sul contratto**, comandi di test, regressioni e blocker.

**Prima demo richiesta:** MicaNT avvia un vero `sh.exe` che esegue `ls /mnt/c` sul VFS NT reale e legge i file anche attraverso la corrispondente unità Windows.

**Definition of Done principale:** `sh.exe` in Ring3 esegue `ls, cp, mv, rm` su file persistenti, redirection e pipeline con processi distinti, condivisione dei dati tra namespace NT/POSIX e resource cleanup verificati.
