# MicaNT Sovereign Architecture Taxonomy & Subsystem Naming Reference

This document establishes the official sovereign naming conventions, architectural boundaries, and historical lineage for all core subsystems within **MicaNT**.

## 1. Purpose & Clean-Room Transparency

To maintain rigorous clean-room compliance, avoid intellectual property ambiguity, and make it instantly transparent to observers (including Microsoft, security auditors, and open-source contributors) that MicaNT is an independently authored operating system, **all internal subsystems are assigned sovereign names**.

These names:
1. **Pay Tribute:** Honor Dave Cutler's legendary engineering heritage across DEC Mica, DEC PRISM, and foundational Windows NT development projects.
2. **Prevent Name Collisions:** Distinguish MicaNT's clean-room ISO C++23 implementations from proprietary Microsoft Windows system binaries and drivers.
3. **Establish Modularity:** Allow subsystems to be developed, unit-tested, and published as standalone decoupled components (such as [PrismX](https://github.com/MicaNT-Kernel/PrismX)).

---

## 2. Master Taxonomy Matrix

| Subsystem Domain | Sovereign Name | C++ Namespace | Primary Headers | Historical Heritage & Inspiration |
| :--- | :--- | :--- | :--- | :--- |
| **Graphics & 3D Acceleration** | **PrismX / Prism3D / PrismVK** | `micant::prismx`<br/>`micant::prism3d`<br/>`micant::vulkan` | `prismx.hpp`<br/>`prism3d.hpp`<br/>`prism3d12.hpp`<br/>`prism_shader_vm.hpp`<br/>`vulkan.hpp` | Named in tribute to Dave Cutler's 1985–1988 **DEC PRISM** RISC architecture and the WDDM display foundation. |
| **File System Engine** | **EmeraldFS** | `micant::emeraldfs`<br/>`micant::ntfs`<br/>`micant::fs` | `ntfs.hpp`<br/>`fat32.hpp`<br/>`fs.hpp`<br/>`storage.hpp` | **Cairo / OFS (Emerald):** The 1991–1995 next-gen Object File System codenamed *Emerald*, coupled with Cutler's VMS Files-11 heritage. |
| **Memory Manager & Paging** | **DaytonaMM** | `micant::daytonamm`<br/>`micant::mm` | `mm.hpp`<br/>`heap.hpp`<br/>`lookaside.hpp` | **Daytona:** Internal codename for Windows NT 3.5 (1994), celebrated for radically slashing NT's memory footprint and stabilizing the VMM. |
| **Object Manager & Namespace** | **NexusOB** | `micant::nexus`<br/>`micant::ob` | `ob.hpp` | The unifying kernel object hierarchy (`\Device`, `\DosDevices`, `\Registry`, handle tables, and security descriptors). |
| **Preemptive Scheduler** | **AegisSched** | `micant::aegis`<br/>`micant::ke` | `ke.hpp`<br/>`dispatcher.hpp`<br/>`cpu.hpp` | Multi-architecture symmetric multiprocessing (SMP) quantum scheduler with priority boost and real-time thread dispatching. |
| **Local Inter-Process Comm (IPC)** | **CourierLPC** | `micant::courier`<br/>`micant::lpc` | `lpc.hpp`<br/>`npfs.hpp` | Cutler's high-speed port-based LPC message passing and named pipe transport engine. |
| **Configuration Manager (Registry)** | **AmberCM** | `micant::amber`<br/>`micant::cm` | `cm.hpp` | The persistent hierarchical tree-cell store representing registry hives (`\Registry\Machine\SOFTWARE`, etc.). |
| **Display Server & Userland Shell** | **SurWin** | `micant::surwin`<br/>`micant::user32`<br/>`micant::csrss` | `csrss.hpp`<br/>`user32.hpp`<br/>`conhost.hpp` | **SUR (Shell Update Release):** Codename for the Windows NT 4.0 desktop upgrade, unifying Conhost, CSRSS, and Window Stations. |
| **Security & Authentication** | **SentinelSec** | `micant::sentinel`<br/>`micant::se`<br/>`micant::sam` | `se.hpp`<br/>`lsass.hpp`<br/>`sam.hpp` | Sovereign Local Security Authority (LSA), SAM user/group database, token impersonation, and PBKDF2/SHA-256 credentials. |
| **Hardware Abstraction Layer** | **TitanHAL** | `micant::titan`<br/>`micant::hal` | `hal.hpp`<br/>`arm64.hpp`<br/>`trap.hpp`<br/>`uefi.hpp` | Unified x86_64 / ARM64 UEFI platform abstraction, interrupt controller, PIT/APIC timers, and CPU exception routing. |
| **Networking & Sockets** | **RazzleNet** | `micant::razzle`<br/>`micant::tcpip` | `tcpip.hpp`<br/>`ndis.hpp`<br/>`ws2_32.hpp`<br/>`iphlpapi.hpp` | **Razzle:** Legendary internal build environment for Windows NT. Represents clean-room NDIS, ARP, IPv4, TCP, UDP, and Winsock2. |
| **Audio Architecture** | **PrismAudio** | `micant::audio`<br/>`micant::sound` | `prismaudio.hpp`<br/>`xaudio2.hpp` | Direct audio sibling to PrismX, handling multi-channel PCM software mixing, wave out synthesis, and session volume ducking. |
| **Gaming & Controller Input** | **VectorHID** | `micant::hid`<br/>`micant::input` | `xinput.hpp` | High-frequency game controller input polling, force feedback vibration, and DirectInput/XInput parity. |
| **Driver & Device Subsystem** | **VanguardDriver** | `micant::driver`<br/>`micant::pnp` | `driver.hpp`<br/>`io.hpp`<br/>`po.hpp` | I/O Request Packet (IRP) dispatching, Plug-and-Play (PnP) hardware enumeration, and device stack orchestration. |
| **Dynamic Executable Loader** | **JanusLDR** | `micant::ldr` | `ldr.hpp`<br/>`pe.hpp` | Clean-room PE/COFF dynamic linker, export table resolver, IAT thunk binder, and side-by-side (SxS) manifest parser. |
| **Crash Diagnostics & Reliability** | **PolarisDiag** | `micant::diag`<br/>`micant::crash` | `trap.hpp`<br/>`po.hpp` | Clean-room kernel bugcheck (`KeBugCheckEx`), crashdump capture, and telemetry-free error reporting. |
| **Process Sandbox & Isolation** | **AegisSandbox** | `micant::sandbox`<br/>`micant::job` | `ps.hpp`<br/>`section.hpp` | Win32 Job Object containment, process isolation boundaries, CPU rate limits, and memory quota fences. |
| **Cryptographic Services** | **CipherKSP** | `micant::crypto`<br/>`micant::ksp` | `se.hpp`<br/>`sam.hpp` | Clean-room Cryptography Next Generation (CNG / BCrypt), PBKDF2 key derivation, SHA-256, and AES symmetric encryption. |
| **File-Level Encryption (EFS)** | **EmeraldCrypt** | `micant::efs` | `feclient.hpp` | Clean-room Encrypting File System (EFS) client, per-file AES-256 symmetric encryption, NTFS alternate utility stream ($EFS), and multi-user DDF/DRF key management. |
| **Endpoint Security System** | **Sentinel Security System for MicaNT** | `micant::wsc`<br/>`micant::sentinel` | `wscapi.hpp`<br/>`se.hpp` | Sovereign endpoint security umbrella providing unified telemetry-free protection and health aggregation across SentinelCenter (`wscapi.dll`), Firewall (WFP), Antivirus (AegisDefender), Volume Encryption (FVE), and UAC. |
| **In-Memory & Script Inspection (AMSI)** | **SentinelScan** | `micant::amsi` | `amsi.hpp` | Clean-room Antimalware Scan Interface (`amsi.dll`), providing in-memory buffer inspection, shellcode/NOP sled detection, script de-obfuscation heuristics, and command prompt interception. |

---

## 3. Subsystem Architectural Details

### 3.1 PrismX / Prism3D / PrismVK (Graphics & Display Architecture)
- **Role:** Independent graphics presentation infrastructure, software rasterization, and multi-API driver interface.
- **Components:**
  - **PrismX (DXGI 1.0/1.1):** Adapter enumeration, swap chain flip models, display modes.
  - **Prism3D (Direct3D 11):** 3D matrix engine, sub-pixel barycentric rasterizer with Z-buffer, indexed draw calls (`DrawIndexed`).
  - **Prism3D12 (Direct3D 12):** Asynchronous command lists, allocators, pipeline state objects (PSOs), explicit barrier state transitions, timeline fences.
  - **PrismShaderVM:** 16-opcode RISC SIMD programmable bytecode virtual machine for vertex and pixel shading.
  - **PrismVK (Khronos Vulkan 1.3):** ICD loader dispatch tables (`vulkan-1.dll`) and built-in sovereign software driver.
- **Standalone Repository:** [`MicaNT-Kernel/PrismX`](https://github.com/MicaNT-Kernel/PrismX)

### 3.2 EmeraldFS (Storage & File System Engine)
- **Heritage:** Named in honor of the *Emerald* project (the internal codename for Cairo's Object File System - OFS).
- **Capabilities:**
  - Full Master File Table (MFT) parser and record management ($MFT, $Bitmap, $LogFile).
  - Alternate Data Streams (ADS) support (`file.txt:stream`).
  - Transactional intent-logging for atomic commit and crash-recovery.
  - Extensible file system dispatcher supporting FAT32 (`fat32.hpp`) and raw partition abstractions (`storage.hpp`).

### 3.3 DaytonaMM (Memory Management Subsystem)
- **Heritage:** Named after Windows NT 3.5 *Daytona*, which optimized memory usage across the operating system.
- **Capabilities:**
  - Virtual Address Space (VAS) management with 4KB paging and multi-level page tables.
  - Lookaside lists for fixed-size high-frequency kernel allocations.
  - Dynamic kernel slab heap with zero-fragmentation coalescing.
  - Working set trimming and page-fault demand paging.

### 3.4 NexusOB (Kernel Object Manager)
- **Capabilities:**
  - Hierarchical unified namespace (`\`, `\Device`, `\DosDevices`, `\Driver`, `\KernelObjects`).
  - Reference counting (`AddRef` / `Release`), object headers, and symbolic links.
  - Per-process handle tables with handle inheritance and security rights masks.

### 3.5 AegisSched (Executive Thread Scheduler)
- **Capabilities:**
  - Priority-based preemptive scheduling (32 priority levels, Realtime vs. Dynamic).
  - Multi-core SMP thread balancing with CPU affinity masks.
  - Quantum accounting, priority boost on I/O completion, and priority decay.

### 3.6 CourierLPC (Local Procedure Call & Named Pipes)
- **Capabilities:**
  - Connection-oriented message ports for fast Ring 3 ↔ Ring 3 and Ring 3 ↔ Ring 0 communication.
  - Shared memory message transfer for payloads exceeding standard LPC packet sizes.
  - Named Pipe File System (NPFS) engine supporting duplex streaming and byte/message modes.

### 3.7 AmberCM (Configuration Manager)
- **Capabilities:**
  - On-disk and in-memory registry hive parser (`\Registry\Machine\SYSTEM`, `\SOFTWARE`).
  - Cell-based storage allocation and dirty-cell transaction flushing.
  - Key path resolution, subkey enumeration, and strongly typed value storage (DWORD, QWORD, SZ, MULTI_SZ, BINARY).

### 3.8 SurWin (Userland Windowing Server)
- **Heritage:** Named after Windows NT 4.0 *SUR* (Shell Update Release).
- **Capabilities:**
  - Sovereign Conhost virtual terminal engine (ANSI/VT100 escape sequence processing).
  - Window Station and Desktop object isolation (`WinSta0\Default`).
  - User32 message pump, window rectangles, clipping lists, and paint event dispatching.

### 3.9 SentinelSec (Security Authority Subsystem)
- **Capabilities:**
  - Clean-room Local Security Authority Subsystem Service (LSASS).
  - Security Account Manager (SAM) database with PBKDF2/SHA-256 salted password hashing.
  - Access Token generation, User/Group SID assignment, and privilege validation (`SeDebugPrivilege`, `SeShutdownPrivilege`, etc.).

### 3.10 TitanHAL (Hardware Abstraction Layer)
- **Capabilities:**
  - Pure UEFI 64-bit bootloader handoff (`bootx64.efi`).
  - Architecture-independent hardware timer, interrupt dispatching, and trap frame handling across x86_64 and ARM64.

### 3.11 RazzleNet (Network Transport Engine)
- **Heritage:** Named after NT's iconic internal build environment *Razzle*.
- **Capabilities:**
  - Sovereign TCP/IP stack with ARP resolution, IPv4 routing, TCP state machine (SYN/ACK/FIN), and UDP datagrams.
  - NDIS miniport driver abstraction and Winsock2 (`ws2_32.dll`) socket descriptor tables.

### 3.12 PrismAudio (Audio Engine)
- **Role:** Independent sovereign audio presentation engine, multi-channel software mixer, and Microsoft XAudio2 / WinMM parity.
- **Capabilities:**
  - Multi-stream PCM and IEEE 32-bit floating point software mixer supporting 44.1kHz and 48kHz audio pipelines.
  - Low-latency voice session management, volume ducking, and frequency tone synthesis (sine, square, triangle, saw, noise).
  - Sovereign XAudio2 interface (`IXAudio2`, `IXAudio2SourceVoice`, `IXAudio2MasteringVoice`, `XAUDIO2_BUFFER`).
  - 3D spatial audio positional calculation (inverse-distance falloff and stereo listener orientation).

### 3.13 VectorHID (Gaming & Controller Input Subsystem)
- **Role:** Low-latency human interface device input pipeline with Xbox gamepad and joystick parity.
- **Capabilities:**
  - Standard `xinput1_4.dll` polling interface (`XInputGetState`, `XInputSetState`, `XInputGetCapabilities`).
  - Dual-motor force feedback vibration emulation.
  - Digital thumbstick deadzone normalization and analog trigger axis scaling.
  - Multi-controller slot assignment (controllers 0 through 3).

### 3.14 VanguardDriver (Driver Framework & Device Subsystem)
- **Heritage:** Named in honor of the vanguard kernel engineering principles established during the DEC Alpha and Windows NT driver architecture evolutions.
- **Capabilities:**
  - I/O Request Packet (IRP) dispatching, completion routines, and cancel-safe queues.
  - Layered device stack orchestration (Functional Device Object - FDO, Physical Device Object - PDO, Filter Drivers).
  - Plug and Play (PnP) hardware enumeration and dynamic device interface registration.
  - System power state transitions (S0 Working, S3 Sleep, S4 Hibernate, S5 Soft-Off).

### 3.15 JanusLDR (Dynamic Executable Loader)
- **Heritage:** Named after Janus, the Roman deity of transitions, beginnings, and doorways, representing the gateway from disk PE binaries into running process memory.
- **Capabilities:**
  - Clean-room Portable Executable (PE32+) image parsing and section mapping.
  - Import Address Table (IAT) binding, export ordinal resolution, and forwarder chain resolution.
  - Delay-load helper thunks and Side-by-Side (SxS) manifest activation contexts.
  - Dynamic module registration (`LoadLibraryExW`, `GetProcAddress`).

### 3.16 PolarisDiag (Crash Diagnostics & Reliability Engine)
- **Role:** Deterministic crash capture, kernel bugcheck routing, and telemetry-free incident forensics.
- **Capabilities:**
  - Kernel bugcheck routing (`KeBugCheck`, `KeBugCheckEx`) with architecture-specific trap context dump.
  - Clean-room Windows Error Reporting (WER) minidump generation without remote cloud transmissions.
  - Sovereign panic screen rendering (BootVid crash screen / Blue Screen).

### 3.17 AegisSandbox (Process Containment & Job Objects)
- **Role:** Security sandboxing, hardware quota enforcement, and process tree isolation.
- **Capabilities:**
  - Win32 Job Object abstraction (`CreateJobObjectW`, `AssignProcessToJobObject`).
  - Per-job CPU time limits, working set memory caps, and process count restrictions.
  - Isolated object namespaces and restricted token containment for untrusted binaries.

### 3.18 CipherKSP (Cryptographic Services Engine)
- **Role:** High-assurance clean-room cryptographic primitives and key storage provider.
- **Capabilities:**
  - Cryptography Next Generation (CNG / `bcrypt.dll`) parity.
  - Deterministic random number generation (CSPRNG).
  - SHA-256 / HMAC hashing, PBKDF2 key derivation, and AES symmetric encryption.

### 3.19 EmeraldCrypt (File-Level Encryption & EFS Subsystem)
- **Role:** High-assurance transparent per-file encryption, NTFS `$EFS` alternate data stream engine, and enterprise Data Recovery Agent management.
- **Capabilities:**
  - Standard Win32 EFS C ABI exports (`feclient.dll` / `advapi32.dll`): `EncryptFileW`, `DecryptFileW`, `FileEncryptionStatusW`, `QueryUsersOnEncryptedFile`, `QueryRecoveryAgentsOnEncryptedFile`, `AddUsersToEncryptedFile`, `RemoveUsersFromEncryptedFile`.
  - Transparent per-file AES-256-CBC symmetric encryption with unique File Encryption Keys (FEK).
  - Data Decryption Field (DDF) supporting multi-user access control and key wrapping.
  - Data Recovery Field (DRF) supporting corporate Data Recovery Agents (DRA).
  - Zero-knowledge raw encrypted streaming (`OpenEncryptedFileRawW`, `ReadEncryptedFileRaw`, `WriteEncryptedFileRaw`) for enterprise backup agents.
  - DoD 5220.22-M NISPOM 3-pass disk space sanitization (`cipher /w`).

### 3.20 Sentinel Security System for MicaNT (SentinelCenter & wscapi.dll Subsystem)
- **Role:** Central telemetry-free health interrogation, security provider registration, asynchronous change notification dispatcher, and COM integration (`IWscProduct`, `IWSCProductList`).
- **Capabilities:**
  - Standard Win32 C ABI exports (`wscapi.dll`): `WscGetSecurityProviderHealth`, `WscRegisterForChanges`, `WscUnRegisterChanges`, `WscQueryAntiVirusStatus`, `WscRegisterProduct`, `WscUnregisterProduct`, `WscUpdateProductStatus`, `WscGetAntiVirusProducts`, `WscFreeMemory`.
  - Health aggregation with worst-case priority resolution (`POOR` > `SNOOZE` > `NOTMONITORED` > `GOOD`).
  - Real-time provider integration with WFP firewall, AegisDefender engine, UAC, CBS servicing stack, and `wscsvc`.
  - Observer pattern with thread-safe subscription and callback dispatch.
  - COM interfaces: `IWscProduct`, `IWscProduct2`, `IWscProduct3`, `IWSCProductList` with standard reference counting.
  - CLI: `sentinel` / `wsc` (`sentinel status`, `sentinel health [provider]`, `sentinel products`, `sentinel test`).

### 3.21 SentinelScan (Antimalware Scan Interface & amsi.dll Subsystem)
- **Role:** In-memory code and script buffer inspection bridge between calling applications (command shells, PowerShell runtimes, scripting engines, and office suites) and installed antimalware engines.
- **Capabilities:**
  - Standard Win32 C ABI exports (`amsi.dll`): `AmsiInitialize`, `AmsiOpenSession`, `AmsiScanBuffer`, `AmsiScanString`, `AmsiNotifyOperation`, `AmsiCloseSession`, `AmsiUninitialize`, `AmsiResultIsMalware`, `AmsiResultIsBlockedByAdmin`, `AmsiResultIsValid`.
  - Standard COM interfaces: `IAmsiStream` (`{3E47F2E5-81D4-4AE7-897E-585A823CE1F8}`) and `IAmsiProvider` (`{B2CABFE3-F61D-4729-A586-64623C669004}`).
  - Built-in sovereign heuristic provider (`SovereignSentinelScanProvider`) with zero telemetry:
    - EICAR standard test detection.
    - NOP sled & binary shellcode detection (16+ consecutive 0x90s, common stack pivot signatures).
    - Obfuscated PowerShell download cradle detection (`Invoke-Expression` / `IEX` with `WebClient` / `DownloadString`).
    - Credential dumping pattern matching (`mimikatz`, `sekurlsa`, `logonpasswords`).
    - AMSI tampering attempt detection (`amsiInitFailed`, `AmsiUtils`).
    - In-memory process injection primitives (`VirtualAlloc` + `WriteProcessMemory` + `CreateRemoteThread`).
    - Shannon block entropy analysis for encrypted dropper detection (> 7.6 bits/byte).
  - Administrative policy block rules (`AMSI_RESULT_BLOCKED_BY_ADMIN_START`).
  - Interactive shell pre-execution interception and blocking (`0x800700DF` `ERROR_VIRUS_INFECTED`).
  - CLI: `amsi` (`amsi status`, `amsi scan <content>`, `amsi block <pattern>`, `amsi unblock <pattern>`, `amsi clear`, `amsi test`).

---

## 4. Coding & Header Conventions

Whenever adding or extending a subsystem:
1. Include a clean-room provenance banner citing public, openly licensed specifications.
2. Use the designated sovereign namespace (or alias to it).
3. Ensure zero external dependencies outside the ISO C++23 standard library.
4. Maintain deterministic memory footprints with zero background telemetry.
