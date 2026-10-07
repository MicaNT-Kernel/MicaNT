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
| **Antimalware Engine & Client** | **AegisDefender** | `micant::defender` | `mpengine.hpp` | Clean-room Microsoft Malware Protection Engine (`mpengine.dll`) and Client (`mpclient.dll`), providing Shannon entropy PE section heuristics, local offline threat database matching, and AES-256 encrypted quarantine vault isolation. |
| **Driver Frameworks (KMDF & UMDF)** | **TitanWDF / AegisWDF** | `micant::wdf` | `wdf.hpp` | Clean-room Windows Driver Frameworks (KMDF v1.33 / UMDF 2.0) object models, queue pacing, and user-mode driver host isolation. |
| **Pseudo Console & Terminal (ConPTY)** | **TitanPTY / SurPTY** | `micant::conpty` | `conpty.hpp` | Named in tribute to the Windows Terminal and Console Host architecture evolution. Unifies headless VT server, differential VT rendering, and Win32 character grids. |
| **Universal Serial Bus (USB 3.2 & xHCI)** | **TitanUSB / NexusUSB** | `micant::usb`<br/>`micant::xhci` | `usb.hpp` | Clean-room xHCI 1.2 Extensible Host Controller Interface, Transfer Request Blocks (TRB), Root Hub, Mass Storage (BOT/SCSI), HID, CDC-ACM, and WinUSB subsystem. |
| **PCI Express (PCIe 5.0 & 6.0 Bus)** | **TitanPCI / NexusPCI** | `micant::pci`<br/>`micant::pcie` | `pci.hpp` | Clean-room PCI Express 5.0/6.0 Root Complex, Type 0/1 configuration space, BAR dynamic sizing, MSI/MSI-X vector engines, and Advanced Error Reporting (AER). |
| **NVM Express & Flash Storage (NVMe/UFS/eMMC/AHCI)** | **TitanNVMe / TitanFlash / EmeraldNVMe** | `micant::nvme`<br/>`micant::storage` | `nvme.hpp`<br/>`storage.hpp` | Clean-room NVM Express 1.0e–2.0d host controller, multi-namespace flash storage, S.M.A.R.T. telemetry, UFS 4.0, eMMC 5.1 SDHCI, and AHCI SATA SSD with NCQ. |
| **ACPI Platform & AML Interpreter** | **TitanACPI / AegisACPI** | `micant::acpi` | `acpi.hpp` | Clean-room ACPI 6.5 table validation (RSDP/XSDT/FADT/MADT/MCFG/DMAR/SRAT), AML AST evaluation, ACPI namespace (`\_SB`, `\_PR`, `\_TZ`), and `acpi.sys` driver. |
| **High Definition Audio (HDA & UAC2)** | **TitanHDA / NexusHDA** | `micant::hda` | `hdaudio.hpp` | Clean-room Intel HDA 1.0a controller, CORB/RIRB DMA rings, Codec Widget tree (ALC887 DAC/ADC/Pins), Jack Sense, USB Audio Class 2.0, and `hdaudio.sys` driver. |
| **Windows Display Driver Model (WDDM 3.2)** | **TitanWDDM / NexusWDDM** | `micant::wddm` | `wddm.hpp`<br/>`dxgkrnl.hpp` | Clean-room WDDM 3.2 graphics kernel subsystem (`dxgkrnl.sys`, `displib.sys`), VidMm physical memory segments, VidPN 3.0 display topology, WDDM 3.2 direct hardware queues, monitored fences, TDR recovery, and multi-vendor miniports (NVIDIA, AMD, Intel, PrismX). |
| **Network Driver Specification (NDIS 6.88)** | **TitanNDIS / RazzleNet** | `micant::ndis` | `ndis.hpp` | Clean-room NDIS 6.88 network driver subsystem (`ndis.sys`), NET_BUFFER_LIST pools, Hardware Offloads (IPv4/IPv6 Checksum, LSOv2 64KB, RSC), RSS Toeplitz hash & 128-entry indirection table, SR-IOV 16 VFs, and RazzleNet 10GbE/100GbE PCIe miniport (`razzlenet.sys` at 00:04.0) with dual 512-entry DMA descriptor rings. |
| **Bluetooth 5.4 & LE Audio** | **TitanBTH / NexusBTH** | `micant::bth` | `bthport.hpp` | Clean-room Bluetooth 5.4 kernel port driver (`bthport.sys`), USB transport miniport (`bthusb.sys`), RFCOMM serial protocol (`rfcomm.sys`), and bus enumerator (`bthenum.sys`) with LE Audio (LC3 / Auracast). |
| **Wi-Fi 7 & WDI Miniport Subsystem** | **TitanWiFi / NexusWiFi** | `micant::wdi` | `wdiwifi.hpp` | Clean-room Wi-Fi 7 (802.11be EHT) & WDI framework (`wdiwifi.sys`), NetAdapterCx WDF class extension (`netadaptercx.sys`), and TitanWiFi PCIe miniport (`titanwifi.sys` at 00:06.0) with Multi-Link Operation (MLO STR), 320 MHz channels, and 4096-QAM. |
| **USB4 2.0 & Thunderbolt 4 Subsystem** | **TitanUSB4 / NexusUSB4** | `micant::usb4` | `usb4.hpp` | Clean-room USB4 2.0 & Thunderbolt 4/5 Host Router subsystem (`usb4host.sys`, `thunderbolt.sys`, `usb4router.sys` at 00:07.0) with PAM3 80G symmetric / 120G asymmetric signaling, native PCIe tunneling, DP 2.1 tunneling, and SL2 DMA protection. |
| **Neural Processing Unit & MCDM** | **TitanNPU / NexusNPU** | `micant::npu` | `npu.hpp` | Clean-room Microsoft Compute Driver Model (MCDM 1.0/2.0) & NPU subsystem (`mcdm.sys`, `npu.sys`, `titannpu.sys` at 00:08.0) with 48 INT8 TOPS Copilot+ compliance, 4 compute tiles, INT4/FP8/FP16/BF16 precisions, DirectML hardware queues, and SLM execution. |
| **Compute Express Link (CXL 2.0 / 3.1)** | **TitanCXL / NexusCXL** | `micant::cxl` | `cxl.hpp` | Clean-room Compute Express Link 2.0/3.1 heterogeneous memory fabric (`cxlhost.sys`, `cxlmem.sys`, `cxlbus.sys` at 00:09.0 & Bus 4) with CXL.io, CXL.cache, CXL.mem, Type 1/2/3 devices, HDM decoders, DMT NUMA tiering, and mailbox. |
| **USB Type-C & Power Delivery (UCSI 2.1/3.0 / PD 3.1)** | **TitanUCSI / NexusUCSI** | `micant::ucsi` | `ucsi.hpp` | Clean-room USB Type-C Connector System Software Interface (UCSI 2.1/3.0) and USB Power Delivery 3.1 (240W EPR) subsystem (`ucsi.sys`, `usbc.sys`, `ppm.sys`, ACPI `\_SB.UBTC`) with 4 physical ports, EPR AVS (15-48V @ 5A), E-Marker discovery, and dynamic role swapping (`PR_SWAP` / `DR_SWAP`). |
| **DirectStorage 1.2 & BypassIO Subsystem** | **TitanBypassIO / NexusBypassIO** | `micant::bypassio` | `bypassio.hpp` | Clean-room Windows BypassIO architecture (`FSCTL_MANAGE_BYPASS_IO`) & DirectStorage 1.2 GPU decompression driver stack (`bypassio.sys`, `storqos.sys`) with direct NVMe-to-VRAM DMA (<25us latency), GDeflate GPU compute/CPU SIMD codec, and 3-tier Storage QoS. |
| **Persistent Memory & DAX Storage Subsystem** | **TitanPMEM / NexusPMEM** | `micant::pmem` | `pmem.hpp` | Clean-room Persistent Memory (NVDIMM / Optane PMEM) & DAX driver stack (`pmem.sys`, `dax.sys`), ACPI 6.5 NFIT parser, App Direct zero-copy DAX userland mapping (`SEC_DAX`), Block Translation Table (BTT) 4KB atomic sector update crash protection, `clwb` + `sfence` persistence flush barriers (<100ns latency), and Asynchronous DRAM Refresh (ADR) power-loss protection. |
| **RDMA & SMB Direct Storage Subsystem** | **TitanRDMA / NexusSMB** | `micant::rdma` | `rdma.hpp` | Clean-room NetworkDirect (NDKPI 2.0) RDMA driver stack (`ndisrdma.sys`, `smbdirect.sys`), 100GbE RoCE v2 (UDP 4791) / InfiniBand NDR (400G), kernel-bypass Queue Pairs (RC/UD) and Completion Queues, zero-copy Memory Registration (`lkey`/`rkey`), TitanRoCE autonomous lossy recovery, and SMB Direct remote DirectStorage into client VRAM/DAX. |
| **Microsoft Pluton Security Processor Subsystem** | **TitanPluton / AegisPluton** | `micant::pluton` | `pluton.hpp` | Clean-room Microsoft Pluton on-die security processor driver (`pluton.sys`, ACPI `\_SB.PLTN`), physical bus-sniffing immunity, on-die crossbar fabric (`0xFEB00000`), 24 SHA-256 PCR banks (PCR 0..23), hardware keystore (SRK, EK, VMK), policy-sealed blobs, TRNG, and sub-5us command latency. |

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

### 3.22 AegisDefender (Microsoft Malware Protection Engine & Client Subsystem)
- **Role:** Clean-room offline antimalware engine (`mpengine.dll`) and client interface (`mpclient.dll`) providing heuristic threat classification, Shannon entropy analysis, and encrypted quarantine vault isolation without cloud beaconing.
- **Capabilities:**
  - Standard Win32 C ABI exports (`mpclient.dll` & `mpengine.dll`): `MpManagerOpen`, `MpManagerClose`, `MpScanStart`, `MpScanControl`, `MpThreatOpen`, `MpThreatEnumerate`, `MpThreatClose`, `MpCleanOpen`, `MpCleanStart`, `MpCleanClose`, `MpGetThreatInfo`, `MpGetQuarantineVault`, `MpQuarantineRestore`, `MpQuarantineDelete`, `MpFreeMemory`, `MpErrorMessageFormat`.
  - Shannon entropy PE section heuristic analyzer ($H = -\sum p_i \log_2(p_i)$) detecting packed/encrypted droppers (> 7.2 bits/byte in executable sections) and shellcode in data sections (> 7.6 bits/byte).
  - Local threat catalog and signature matching (`PowerDrop`, `LsaDump`, `AmsiTamper`, `WannaCrypt`, `ShellcodeStager`) operating 100% offline with zero cloud telemetry.
  - Encrypted Quarantine Vault (`C:\ProgramData\MicaNT\Quarantine`) using AES-256-CBC, unique IV generation, and SHA-256 cryptographic verification for isolated containment and authenticated restoration.
  - Interactive CLI: `MpCmdRun.exe` parity via `defender` / `mpcmdrun` (`-Scan`, `-ListQuarantine`, `-Restore`, `-PurgeQuarantine`, `-SignatureUpdate`, `-GetFiles`, `status`, `test`).

### 3.23 TitanWDF (Windows Driver Frameworks: KMDF v1.33 & UMDF 2.0 Subsystem)
- **Role:** Clean-room driver infrastructure supporting both kernel-mode (`Wdf01000.sys`) and user-mode (`WUDFHost.exe` / `wudfrd.sys`) device drivers with unified object lifetimes, PnP/Power state machines, and flexible I/O queue dispatching.
- **Capabilities:**
  - Unified WDF object hierarchy (`WDFOBJECT`, `WDFDRIVER`, `WDFDEVICE`, `WDFQUEUE`, `WDFREQUEST`, `WDFMEMORY`, `WDFIOTARGET`) with recursive cascade cleanup and type-safe typed context associations (`WDF_OBJECT_CONTEXT_TYPE_INFO`).
  - PnP and Power state machines (`WdfDevStatePnpStarted`, `WdfDevStatePowerD0` through `WdfDevStatePowerD3`) with callback routing (`EvtDevicePrepareHardware`, `EvtDeviceD0Entry`, etc.).
  - Queue dispatching modes:
    - **Sequential:** Single request serialization with automatic pacing upon request completion.
    - **Parallel:** Concurrent multi-request dispatch.
    - **Manual:** Explicit polling via `WdfIoQueueRetrieveNextRequest`.
  - UMDF 2.0 Host Isolation: User-mode driver crashes are contained inside `WUDFHost.exe` by the Sovereign Reflector (`wudfrd.sys`), preventing OS bugchecks.
  - Interactive CLI: `wdf` (`wdf status`, `wdf drivers`, `wdf devices`, `wdf queues`, `wdf umdf`, `wdf test`).

### 3.24 TitanPTY (Windows Pseudo Console & Terminal Host Subsystem)
- **Role:** Clean-room pseudo console (ConPTY) and terminal server bridging legacy Win32 console applications with modern DEC VT100/VT220, xterm-256, and 24-bit TrueColor VT terminal emulators.
- **Heritage:** Conceived as the sovereign dual to SurWin (`conhost.exe`), named *TitanPTY* / *SurPTY* to reflect high-performance terminal plumbing and the modern Windows Terminal hosting model.
- **Capabilities:**
  - Standard Win32 C ABI exports (`kernel32.dll` / `conpty.dll`): `CreatePseudoConsole`, `ResizePseudoConsole`, `ClosePseudoConsole`.
  - Process Creation Integration: `PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE` support for attaching child processes cleanly to pseudo consoles.
  - Headless VT Terminal Server: Decoupled in-memory pipe infrastructure (`ConptyPipe`) with bidirectional streaming, input parsing, and asynchronous worker dispatching.
  - Differential VT Rendering Engine: Minimizes I/O bandwidth by calculating cell differences between the primary character grid and a shadow buffer, emitting optimized ANSI/VT sequences (cursor movements, SGR text formatting, bold, underline, 24-bit RGB TrueColor `\x1b[38;2;R;G;Bm`).
  - Terminal Control Sequence Support:
    - Primary and alternate screen buffers (`\x1b[?1049h`, `\x1b[?1049l`).
    - Dynamic window title manipulation (`\x1b]0;Title\x07` / `\x1b]2;Title\x07`).
    - Dynamic console window resize renegotiation (`\x1b[8;H;Wt`).
    - Soft terminal reset (`\x1b[!p`).
    - Cursor styling (blinking block, steady block, blinking underline, steady underline, blinking bar, steady bar via `\x1b[q`).
  - Bidirectional VT Input Parser: Translates incoming terminal sequences (arrows, home, end, function keys, SGR 1006 mouse events) into standard Win32 `INPUT_RECORD` structures (`KEY_EVENT`, `MOUSE_EVENT`, `WINDOW_BUFFER_SIZE_EVENT`).
  - System Service Registration: SCM registered service `OpenConsole` (`TitanPTY Headless Console Host Server`).
  - Interactive CLI: `conpty` / `pty` / `pseudoconsole` (`status`, `list`, `create`, `write`, `resize`, `close`, `test`).

### 3.25 TitanUSB (Universal Serial Bus 3.2 & xHCI Host Controller Subsystem)
- **Role:** Clean-room Universal Serial Bus (USB 3.2 Gen 2) and Extensible Host Controller Interface (xHCI 1.2) architecture providing hardware register virtualization, TRB ring execution, root hub port status management, standard device class drivers, and WinUSB client driver APIs (`winusb.dll`).
- **Heritage:** Conceived as the sovereign hardware I/O sibling to TitanHAL and VanguardDriver, named *TitanUSB* / *NexusUSB* to reflect direct host controller virtualization and device tree topology mapping.
- **Capabilities:**
  - Standard Win32 / WinUSB C ABI exports (`winusb.dll`): `WinUsb_Initialize`, `WinUsb_Free`, `WinUsb_GetAssociatedInterface`, `WinUsb_GetDescriptor`, `WinUsb_QueryInterfaceSettings`, `WinUsb_QueryDeviceInformation`, `WinUsb_SetCurrentAlternateSetting`, `WinUsb_QueryPipe`, `WinUsb_SetPipePolicy`, `WinUsb_GetPipePolicy`, `WinUsb_ReadPipe`, `WinUsb_WritePipe`, `WinUsb_ControlTransfer`, `WinUsb_ResetPipe`, `WinUsb_AbortPipe`, `WinUsb_FlushPipe`.
  - xHCI 1.2 Extensible Host Controller Register Model: Capability registers (CAPLENGTH, HCIVERSION, HCSPARAMS1-3, HCCPARAMS1-2, DBOFF, RTSOFF), Operational registers (USBCMD, USBSTS, PAGESIZE, DNCTRL, CRCR, DCBAAP, CONFIG), and Interrupter Runtime registers (IMAN, IMOD, ERSTSZ, ERSTBA, ERDP).
  - TRB Ring Engine: Command Ring, Event Ring, and Transfer Rings with 64-byte TRB cycle bits, event interrupts, transfer completion codes (`Success`, `DataBufferError`, `BabbleDetected`, `ShortPacket`, `StallError`), and slot/endpoint doorbell mechanisms.
  - Standard USB 3.2 Descriptor Model: Full support for Device (`USB_DEVICE_DESCRIPTOR`), Configuration (`USB_CONFIGURATION_DESCRIPTOR`), Interface (`USB_INTERFACE_DESCRIPTOR`), Endpoint (`USB_ENDPOINT_DESCRIPTOR`), and SuperSpeed Companion (`USB_SS_ENDPOINT_COMPANION_DESCRIPTOR`) descriptors.
  - Root Hub & Port State Machine: Port status and control registers (`PORTSC`) handling connect detection (`CurrentConnectStatus`), port reset signaling (`PortReset`), link speed negotiation (`LowSpeed`, `FullSpeed`, `HighSpeed`, `SuperSpeed`, `SuperSpeedPlus`), and port disable/enable transitions.
  - Device Class Drivers:
    - **Mass Storage (BOT / SCSI):** Bulk-Only Transport state machine with Command Block Wrapper (CBW) and Command Status Wrapper (CSW), processing SCSI `INQUIRY`, `READ_CAPACITY_10`, `READ_10`, and `WRITE_10` over 512-byte sectors.
    - **Human Interface Device (HID):** Standard USB mouse reports (3-button, X/Y relative coordinate delta packets) with input report buffering and endpoint polling.
    - **Communication Device Class (CDC-ACM):** Virtual serial COM port emulation (`USBSER`) with line coding configuration (`SetLineCoding`, `GetLineCoding`), modem control lines (`SetControlLineState`), and bidirectional data stream transfers.
  - System Service & Driver Integration: SCM registered services for `usbxhci` (TitanUSB xHCI Controller Driver) and `usbhub3` (TitanUSB SuperSpeed Hub Driver), integrated with JanusLDR Dynamic Loader and Version Database.
  - Interactive CLI: `usb` / `xhci` / `winusb` (`status`, `list` / `tree`, `attach`, `detach`, `xhci`, `test`).

### 3.26 TitanPCI (PCI Express 5.0/6.0 Bus, Root Complex & AER Subsystem)
- **Role:** Clean-room PCI Express (PCIe 5.0/6.0) bus architecture, Root Complex virtualization, Type 0/1 configuration space engine, Base Address Register (BAR) dynamic sizing, MSI/MSI-X interrupt distribution, and Advanced Error Reporting (AER) telemetry subsystem (`pci.sys`).
- **Heritage:** Conceived as the unifying hardware bus interconnect across Dave Cutler's DEC Alpha / PRISM / VAX systems and modern PC architecture, named *TitanPCI* / *NexusPCI* to reflect central hardware bus mastery and endpoint topology mapping.
- **Capabilities:**
  - Standard PCI Bus Driver C ABI exports (`pci.sys`): `PciReadConfigByte`, `PciReadConfigWord`, `PciReadConfigDword`, `PciWriteConfigByte`, `PciWriteConfigWord`, `PciWriteConfigDword`, `PciFindDevice`, `PciGetBarInfo`, `PciEnableBusMastering`, `PciEnableMemorySpace`, `PciConfigureMsi`, `PciConfigureMsix`, `PciTriggerMsiVector`, `PciInjectAerError`, `PciClearAerStatus`.
  - Configuration Space Model: 4096-byte PCIe configuration space with Type 0 (Endpoint) and Type 1 (PCI-to-PCI Bridge / Root Port) header layouts, Command/Status register bitmask evaluation, and capabilities pointer traversal.
  - Base Address Register (BAR) Dynamic Sizing: 32-bit Memory, 64-bit Memory (low/high paired registers with prefetchable flags), and I/O Space windows with standard OS sizing probe (`0xFFFFFFFF` write inquiry) simulation.
  - Standard Capability List: Power Management (PMCAP), Message Signaled Interrupts (MSI with 32/64-bit addresses and up to 32 vectors), PCI Express Capability (Gen 1..5 link speeds and x1..x16 link widths), and MSI-X Capability (up to 2048 vectors with per-vector masking and Pending Bit Array PBA tracking).
  - PCIe Extended Capabilities: Advanced Error Reporting (AER with uncorrectable fatal/non-fatal status, correctable status, and 16-byte TLP packet header log recording) and Single Root I/O Virtualization (SR-IOV with InitialVFs/TotalVFs and VF BAR sizing).
  - Sovereign Root Complex Topology: Pre-seeded device tree containing Host Bridge (`00:00.0`), 3 PCIe Root Ports (`00:01.0` through `00:03.0`), PrismX 3D GPU (`01:00.0`, Gen 5 x16), TitanNVMe Controller (`02:00.0`, Gen 4 x4), TitanUSB xHCI Controller (`03:00.0`), RazzleNet 10GbE NIC (`00:04.0`), and PrismAudio Controller (`00:05.0`).
  - System Service & Driver Integration: SCM registered boot driver for `pci.sys` (`SERVICE_KERNEL_DRIVER`, `SERVICE_BOOT_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `pci` / `pcie` / `lspci` (`status`, `list`, `tree`, `read <b:d.f> <offset>`, `aer`, `msi`, `test`).

### 3.27 TitanNVMe & TitanFlash (NVM Express 1.0–2.0 & Universal Flash Storage Subsystem)
- **Role:** Clean-room multi-generation NVM Express (NVMe 1.0e through 2.0d) controller, multi-namespace flash storage engine, S.M.A.R.T. health telemetry, and universal flash storage architectures including UFS 4.0, eMMC 5.1 SDHCI, and AHCI 1.3.1 SATA SSD with NCQ (`stornvme.sys`, `storahci.sys`, `storufs.sys`).
- **Heritage:** Conceived as the ultra-high performance solid-state storage foundation directly interfaced with TitanPCI (PCIe Gen 4 x4, BDF `02:00.0`) and EmeraldFS, named *TitanNVMe* / *TitanFlash* / *EmeraldNVMe* to honor next-generation flash storage speed and reliability.
- **Capabilities:**
  - Multi-Generation NVMe Versioning: Seamless compatibility across legacy NVMe 1.0e, 1.1, 1.2.1, 1.3d, 1.4b, and latest modular NVMe 2.0d specifications via register `VS` and software feature gating.
  - Hardware MMIO Register Virtualization: Full register map emulation including Controller Capabilities (`CAP` with 1024 max queue entries, doorbell stride), Version (`VS`), Controller Configuration (`CC` enable, arbitration, I/O queue sizes), Controller Status (`CSTS` ready bit), Admin Queue Attributes (`AQA`), Admin SQ/CQ Base Addresses (`ASQ`, `ACQ`), and Doorbell arrays for SQ/CQ 0..1024.
  - Submission & Completion Queuing Architecture: 64-byte Submission Queue Entries (SQE) with PRP/SGL pointer formats and 16-byte Completion Queue Entries (CQE) featuring deterministic hardware phase tag toggling for zero-copy completion synchronization.
  - Admin & NVM Command Sets:
    - **Admin Commands:** `Identify Controller`, `Identify Namespace`, `Create/Delete I/O Submission Queue`, `Create/Delete I/O Completion Queue`, `Get/Set Features` (arbitration, interrupt coalescing, power states), and `Get Log Page`.
    - **NVM I/O Commands:** `NVME_NVM_READ`, `NVME_NVM_WRITE`, `NVME_NVM_FLUSH`, `NVME_NVM_WRITE_ZEROES`, and `NVME_NVM_DATASET_MGMT` (TRIM / deallocate).
  - Multi-Namespace Flash Architecture: Dynamic namespace management supporting `NSID 1` (System OS volume, 512-byte LBA) and `NSID 2` (High-performance 4Kn database volume, 4096-byte LBA) with full `storage::IBlockDevice` filesystem mount parity for EmeraldFS, NTFS, and FAT32.
  - S.M.A.R.T. / Health Telemetry: Comprehensive telemetry log page (`0x02`) tracking composite temperature (Kelvin), spare endurance capacity remaining, percentage used, data units read/written, host read/write commands, unsafe shutdowns, media error counts, and temperature warning alarms.
  - Universal Flash Storage Architecture Spectrum:
    - **Universal Flash Storage (UFS 3.1/4.0):** Host controller interface with UTP Transfer Request Lists, UniPro M-PHY link layer, LUN 0 Boot Partition, and LUN 1 User Data Partition.
    - **eMMC 5.1 / SDHCI:** Host controller interface with CMD/DAT bus protocol, HS400 dual-data rate timing, Boot Partition 1/2, RPMB replay-protected block partition, and User Area partition.
    - **AHCI 1.3.1 SATA SSD with NCQ:** Advanced Host Controller Interface with 32-tag Native Command Queuing (NCQ FPDMA Read/Write), Command List, Command Table, and PRDT scatter-gather descriptors.
  - StorPort Miniport Driver Architecture: Standard C ABI exports in `stornvme.sys` (`NvmeControllerReset`, `NvmeSubmitAdminCommand`, `NvmeSubmitIoCommand`, `NvmeReadSectors`, `NvmeWriteSectors`, `NvmeGetSmartLog`, `NvmeCreateIoQueuePair`).
  - System Service & Driver Integration: SCM registered boot/system drivers for `stornvme` (NVMe StorPort Miniport), `storahci` (SATA AHCI Driver), and `storufs` (Universal Flash Storage Driver), integrated with JanusLDR Dynamic Loader and Version Database.
  - Interactive CLI: `nvme` / `flash` (`status`, `list` / `ns`, `smart`, `ufs`, `emmc`, `ahci`, `test`).

### 3.28 TitanACPI & AegisACPI (ACPI 6.5 Platform Architecture & AML Interpreter Subsystem)
- **Role:** Clean-room ACPI 6.5 platform management, physical table validation (`RSDP`, `XSDT`, `FADT`, `MADT`, `MCFG`, `DMAR`, `SRAT`), ACPI Machine Language (AML) AST evaluation engine, ACPI namespace tree (`\_SB`, `\_PR`, `\_TZ`), and Windows ACPI Platform Driver (`acpi.sys`).
- **Heritage:** Conceived as the unifying hardware platform discovery and power management layer bridging UEFI bootloader handoff and kernel drivers, named *TitanACPI* / *AegisACPI* to signify platform-level sovereignty and energy governance.
- **Capabilities:**
  - Standard ACPI Platform Driver C ABI exports (`acpi.sys`): `AcpiFindTable`, `AcpiEvaluateObject`, `AcpiGetSystemPowerState`, `AcpiSetSystemPowerState`, `AcpiGetThermalZoneTemp`, `AcpiGetBatteryStatus`, `AcpiGetProcessorCount`.
  - Physical Hardware Table Validation: 8-bit checksum verification across all ACPI tables:
    - `RSDP`: Root System Description Pointer supporting both ACPI 1.0 (20-byte) and ACPI 2.0+ (36-byte extended with 64-bit XsdtAddress).
    - `XSDT`: Extended System Description Table containing 64-bit physical entry pointers to system description tables.
    - `FADT`: Fixed ACPI Description Table with Preferred PM Profile (`PM_DESKTOP`), hardware reset register (`0xCF9`, reset value `0x06`), 24-bit PM timer (`0x0408` at 3.579545 MHz), and SCI IRQ 9.
    - `MADT` (`APIC`): Multiple APIC Description Table with 4 SMP Local APIC processor cores (IDs 0..3), 1 I/O APIC at `0xFEC00000`, and Interrupt Source Overrides (IRQ 0 -> GSI 2, IRQ 9 -> GSI 9 level/active-high).
    - `MCFG`: PCI Express Memory Mapped Configuration Space Base Address Table with PCIe ECAM MMIO Base `0xE0000000` spanning buses 0..255.
    - `DMAR`: DMA Remapping Table modeling Intel VT-d / AMD-Vi hardware IOMMU page-table translation and security isolation flags.
    - `SRAT`: System Resource Affinity Table mapping processors and memory affinity ranges to NUMA domain 0.
  - AML AST Evaluation Engine & Namespace Tree:
    - Evaluates AML opcodes and hierarchical object paths.
    - `\_OSI`: Responds positively to standard OS interface inquiries (`Windows 2022`, `Windows 2019`, `Windows 2016`, `Windows 2015`, `Linux`).
    - `\_PTS` & `\_WAK`: Prepare-to-Sleep and System Wake methods managing system power state transitions (S0 Working, S3 Sleep, S4 Hibernate, S5 Soft Off).
    - `\_PR.CPU0..3`: Processor power management objects exporting `_PSS` (P-states P0 3.2GHz, P1 2.4GHz, P2 1.6GHz) and `_CST` (C-states C1 active halt, C2 stop-grant, C3 deep sleep).
    - `\_SB.PCI0`: PCIe Root Bus bridge device (`_HID PNP0A08`) hosting child endpoint devices `NVME`, `GFX0`, and `XUSB`.
    - `\_SB.BAT0`: Smart Battery fuel gauge subsystem exporting `_BST` (battery status, rate, remaining capacity, voltage) and `_BIF` (battery information, design capacity, chemistry).
    - `\_SB.PWRB`: ACPI Power Button device (`_HID PNP0C0C`).
    - `\_TZ.TZ00`: Thermal Zone policy object exporting `_TMP` (current temperature in tenths of Kelvin, 45.0°C), `_CRT` (critical trip point, 100.0°C), and `_AC0` (active cooling fan threshold, 55.0°C).
  - System Service & Driver Integration: SCM registered boot driver for `acpi.sys` (`SERVICE_KERNEL_DRIVER`, `SERVICE_BOOT_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `acpi` / `aml` (`status`, `tables` / `list`, `tree` / `devices`, `power [state]`, `thermal`, `battery`, `cpu`, `eval <path>`, `test`).

### 3.29 TitanHDA & NexusHDA (Intel High Definition Audio 1.0a & USB Audio Platform Subsystem)
- **Role:** Clean-room Intel High Definition Audio (HDA 1.0a / Azalia) controller, Command Outbound Ring Buffer (CORB), Response Inbound Ring Buffer (RIRB), Audio Codec Widget hierarchy (DAC, ADC, Mixer, Pin Complexes with Jack Sense), USB Audio Class (UAC 2.0/3.0), and Windows HD Audio Driver (`hdaudio.sys`, `usbaudio2.sys`).
- **Heritage:** Conceived as the bare-metal hardware audio presentation sibling to PrismAudio and VectorHID, interfacing directly with PCIe device `00:05.0` and the USB Root Hub to provide low-latency multi-channel PCM streaming.
- **Capabilities:**
  - Standard Windows HD Audio Driver C ABI exports (`hdaudio.sys`): `HdaControllerReset`, `HdaSendVerb`, `HdaSetupStream`, `HdaStartStream`, `HdaStopStream`, `HdaGetStreamPosition`, `HdaGetCodecInfo`, `HdaGetJackStatus`, `HdaSynthesizeTone`.
  - Hardware MMIO Register Virtualization: Full register map including Global Capabilities (`GCAP`), Global Control (`GCTL` with `CRST` reset state machine), Interrupt Control/Status (`INTCTL`, `INTSTS`), Wall Clock (`WALCLK` 24 MHz counter), and Stream Synchronization (`SSYNC`).
  - Circular Ring Buffer Engines:
    - **CORB:** 1024-byte Command Outbound Ring Buffer (256 entries) with write/read pointer pacing (`CORBWP`, `CORBRP`, `CORBCTL`).
    - **RIRB:** 2048-byte Response Inbound Ring Buffer (256 entries) with 64-bit response handling (`RIRBWP`, `RINTCNT`, `RIRBCTL`).
    - **Immediate Command Interface:** Fallback register interface (`ICO`, `ICI`, `ICS`) for direct verb dispatch.
  - Stream Descriptor & DMA Architecture: 8 independent streams (4 Input, 4 Output) with 16-byte Buffer Descriptor List (BDL) entries, cyclic DMA buffers, Link Position in Buffer (`SD_LPIB`), and format encoding (`HdaEncodeFormat` for 44.1k/48k/96k/192k, 16/20/24/32-bit, 1..16 channels).
  - Codec Widget Architecture: Pre-seeded Realtek ALC887 / Sovereign Studio HDA Codec at address 0 (Node 0 Root, Node 1 Audio Function Group, Node 0x02 DAC 0 Front Stereo, Node 0x03 DAC 1 Headphone, Node 0x04 ADC 0 Mic/Line In, Node 0x0C Mixer, Nodes 0x14/0x15/0x18 Pin Complexes) supporting verb execution (`GET_PARAM`, `SET_STREAM_CHANNEL`, `SET_AMP_GAIN_MUTE`, `SET_PIN_CTRL`, `GET_PIN_SENSE` Jack Presence Detect with unsolicited event injection).
  - USB Audio Class 2.0 / 3.0: Studio DAC dongle emulation (48kHz 24-bit stereo isochronous streaming) with master volume and mute controls.
  - Tone Synthesis & Audio Engine Bridge: Realtime PCM sine wave DMA generation linked with `micant::audio` / `PrismAudioSubsystem` software mixer.
  - System Service & Driver Integration: SCM registered boot/system drivers for `hdaudio` (`SERVICE_KERNEL_DRIVER`, `SERVICE_BOOT_START`) and `usbaudio2` (`SERVICE_DEMAND_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `hda` / `hdaudio` / `azalia` (`status`, `codecs` / `list`, `widgets`, `streams`, `jacks`, `uac` / `usb`, `play <hz> [ms] [vol]`, `test`).

### 3.30 TitanWDDM & NexusWDDM (Windows Display Driver Model 3.2 & Graphics Kernel Subsystem)
- **Role:** Clean-room Windows Display Driver Model (WDDM 1.x through 3.2) architecture, DirectX Graphics Kernel Subsystem (`dxgkrnl.sys`, `displib.sys`), Video Memory Manager (`VidMm`), Video Present Network (`VidPN`), GPU Scheduler (`VidSch`) with Direct Hardware Queues, Multi-Plane Overlay (`MPO 3.0`), 64-bit Monitored Fences, Timeout Detection & Recovery (`TDR`), and Multi-Vendor Display Miniport Driver bindings (NVIDIA `nvlddmkm.sys`, AMD `amdkmdag.sys`, Intel `igdkmdn64.sys`, PrismX `prismx_kmd.sys`, and `basicdisplay.sys`).
- **Heritage:** Conceived as the core kernel-mode graphics engine orchestrating physical GPU hardware, dedicated video memory (VRAM), and display outputs, establishing seamless driver compatibility across all major graphics cards.
- **Capabilities:**
  - Standard Graphics Driver C ABI exports (`dxgkrnl.sys`, `displib.sys`): `DxgkInitialize`, `DxgkCreateDevice`, `DxgkCreateAllocation`, `DxgkDestroyAllocation`, `DxgkCreateHwQueue`, `DxgkSubmitCommandHwQueue`, `DxgkPresentFrame`, `DxgkTriggerTdr`.
  - Video Memory Manager (`VidMm`):
    - Multi-segment physical memory topologies: Segment 1 (PCIe Aperture / GTT System RAM) and Segment 2 (Dedicated Local VRAM 16GB).
    - 48-bit GPU Virtual Addressing (`GPUVA`) with per-allocation address reservation and paging.
    - Dynamic residency engine (`MakeResident`, `Evict`) managing memory pressure.
  - Video Present Network (`VidPN 3.0`):
    - Graph topology linking Sources (Primary Desktop, Extended Desktop) to Targets (DisplayPort 2.1 UHBR20, HDMI 2.1 FRL 48Gbps, eDP 1.5, USB-C DP Alt-mode).
    - Display mode sets supporting 1080p, 1440p, 4K UHD 120Hz HDR10, and 8K 60Hz across SDR (`B8G8R8A8_UNORM`), HDR10 (`R10G10B10A2_UNORM`), and scRGB Float (`R16G16B16A16_FLOAT`).
    - Variable Refresh Rate (`VRR`): G-Sync Compatible and AMD FreeSync Premium Pro (48 Hz to 240 Hz).
    - Multi-Plane Overlay (`MPO 3.0`): 4 hardware composition planes with direct scanout flip (`DirectFlip`) bypassing desktop compositor copy overhead.
  - GPU Scheduler (`VidSch`) & Hardware Scheduling:
    - WDDM 3.2 Direct Hardware Queues across 3D/Render, Async Compute, Video Decode (NVDEC/VCN/QSV), Video Encode (NVENC/VCE/QSV), and DMA Copy engines.
    - Monitored Fences: 64-bit monotonically advancing GPU/CPU synchronization fences.
    - VBlank interrupt timing and scan line tracking.
    - Timeout Detection & Recovery (`TDR`): Watchdog timer detecting engine hangs, executing state machine recovery (`DETECTED` -> `PREPARE` -> `RESET` -> `RESTART` -> `RECOVERED`) without system crash or blue screen (BSOD).
  - Multi-Vendor Display Miniport Drivers:
    - **NVIDIA GeForce / RTX:** `nvlddmkm.sys` (Vendor `0x10DE`)
    - **AMD Radeon:** `amdkmdag.sys` (Vendor `0x1002`)
    - **Intel Arc & Iris:** `igdkmdn64.sys` (Vendor `0x8086`)
    - **MicaNT PrismX Discrete 3D GPU:** `prismx_kmd.sys` (PCIe BDF `01:00.0`, Gen 5 x16, 16GB VRAM)
    - **Microsoft Basic Display Driver:** `basicdisplay.sys` (VGA / UEFI GOP / VirtIO fallback)
  - System Service & Driver Integration: SCM registered boot drivers for `dxgkrnl` and `displib` (`SERVICE_KERNEL_DRIVER`, `SERVICE_BOOT_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `wddm` / `gpu` / `graphics` (`status`, `adapters` / `list`, `vidmm` / `vram`, `vidpn` / `displays`, `queues` / `engines`, `tdr`, `test`).

### 3.31 TitanNDIS & RazzleNet (Network Driver Interface Specification 6.88 & High-Speed PCIe Miniports)
- **Designation:** `TitanNDIS` (NDIS 6.88 Subsystem) / `RazzleNet` (10GbE/40GbE/100GbE PCIe Miniport Adapter)
- **Role:** Clean-room Windows Network Driver Interface Specification (NDIS 6.0 through 6.88) architecture, NDIS miniport driver model (`ndis.sys`), high-throughput NET_BUFFER (NB) and NET_BUFFER_LIST (NBL) memory pools, Hardware Offload Engine (IPv4/IPv6 Checksum, LSOv2 64KB, RSC), Receive Side Scaling (RSS) with 40-byte Toeplitz Hash and 128-entry indirection table, Single Root I/O Virtualization (SR-IOV) Virtual Function management, and high-speed RazzleNet PCIe Miniport (`razzlenet.sys`) at PCIe BDF `00:04.0`.
- **Heritage:** Conceived as the core kernel-mode network driver orchestrator, linking physical high-speed network interfaces, DMA descriptor rings, and protocol stacks (TCP/IP) into a unified zero-copy packet pipeline.
- **Capabilities:**
  - Standard Network Driver C ABI exports (`ndis.sys`, `razzlenet.sys`): `NdisMRegisterMiniportDriver`, `NdisMDeregisterMiniportDriver`, `NdisMSetMiniportAttributes`, `NdisAllocateNetBufferListPool`, `NdisFreeNetBufferListPool`, `NdisAllocateNetBufferList`, `NdisFreeNetBufferList`, `NdisMIndicateReceiveNetBufferLists`, `NdisMSendNetBufferListsComplete`, `NdisQueryAdapterInformation`, `RazzleNetInitializeMiniport`, `RazzleNetTransmitPacket`.
  - Memory Management & Descriptor Pools:
    - Pre-allocated thread-safe `NetBufferListPool` recycling `NET_BUFFER_LIST` and `NET_BUFFER` objects.
    - OOB metadata channels (`TcpIpChecksumNetBufferListInfo`, `TcpLargeSendNetBufferListInfo`, `TcpReceiveSegmentCoalescingInfo`).
  - Hardware Offload Engine:
    - Checksum Offload: IPv4 header checksum and TCP/UDP (IPv4/IPv6) calculation and verification via RFC 1071 16-bit 1's complement sum.
    - Large Send Offload v2 (LSOv2 / TSO): Hardware segmentation of up to 64KB TCP payloads into MTU-sized Ethernet frames in DMA.
    - Receive Segment Coalescing (RSC): Aggregation of consecutive incoming TCP segments into coalesced NBLs to minimize CPU interrupt load.
  - Receive Side Scaling (RSS):
    - 40-byte Toeplitz Hash algorithm calculated over packet 4-tuples (SrcIP, DstIP, SrcPort, DstPort).
    - 128-entry Indirection Table distributing incoming traffic across multi-core CPU processor queues.
  - High-Speed DMA Ring Buffers:
    - Dual 512-entry circular DMA descriptor rings (TX and RX) with Head/Tail doorbell registers (`TDT`/`RDT`).
    - Adaptive Interrupt Moderation (AIM) dynamically tuning interrupt frequency based on line rate throughput.
  - Virtualization & Multi-Tenancy:
    - Single Root I/O Virtualization (SR-IOV) supporting Physical Function (PF) resource management and up to 16 Virtual Functions (VFs 0..15).
    - Dedicated VF MAC addresses, 802.1Q VLAN isolation (1..4095), rate limiting (Mbps), and hardware MAC anti-spoofing enforcement.
  - System Service & Driver Integration: SCM registered boot drivers for `ndis` (`SERVICE_BOOT_START`) and `razzlenet` (`SERVICE_SYSTEM_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `ndis` / `nic` / `razzlenet` (`status`, `adapters` / `nics`, `rings` / `dma`, `rss`, `offloads`, `sriov`, `stats`, `test`).

### 3.32 TitanBTH & NexusBTH (Bluetooth 5.4 Kernel Port Driver & LE Audio Subsystem)
- **Designation:** `TitanBTH` (Kernel Port Driver Subsystem) / `NexusBTH` (Bluetooth Bus Enumerator & Peripheral Stack)
- **Role:** Clean-room Windows Bluetooth Kernel Port Driver architecture (`bthport.sys`), Bluetooth USB Transport Miniport (`bthusb.sys`), RFCOMM Serial Protocol Driver (`rfcomm.sys`), and Bluetooth Bus Enumerator (`bthenum.sys`) authored from Bluetooth Core Specification v5.4, ETSI TS 07.10, and open `win32metadata`.
- **Heritage:** Conceived as the core kernel-mode wireless peripheral interconnect, bridging physical USB/PCIe Bluetooth controllers, L2CAP protocol multiplexing, RFCOMM serial streaming, and next-generation LE Audio isochronous streams.
- **Capabilities:**
  - Standard Bluetooth Driver C ABI exports (`bthport.sys`, `bthusb.sys`, `rfcomm.sys`, `bthenum.sys`): `BthPortInitialize`, `BthPortSendHciCommand`, `BthPortOpenL2capChannel`, `BthPortCloseL2capChannel`, `BthPortCreateRfcommPort`, `BthUsbInitialize`, `BthEnumEnumerateDevices`.
  - Host Controller Interface (HCI) Engine:
    - Standard H4 packet types: Command (0x01), ACL Data (0x02), SCO Audio (0x03), Event (0x04), and ISO Data (0x05 for LE Audio).
    - Core Opcodes: `HCI_OP_RESET`, `HCI_OP_READ_BD_ADDR`, `HCI_OP_READ_LOCAL_VERSION_INFO`, `HCI_OP_WRITE_SCAN_ENABLE`, `HCI_OP_LE_SET_CIG_PARAMETERS`, `HCI_OP_LE_CREATE_CIS`.
    - Bluetooth Core Spec 5.4 identification: HCI Version `0x0D` (5.4), LMP Version 13, Sovereign Controller Vendor ID `0x005D`.
  - Logical Link Control and Adaptation Protocol (L2CAP):
    - Dynamic channel identifier allocation (`0x0040` through `0xFFFF`).
    - Fixed signaling channels: Signaling (`0x0001`), Connectionless (`0x0002`), ATT/GATT (`0x0004`), SMP (`0x0006`).
    - Protocol/Service Multiplexing (PSM): SDP (`0x0001`), RFCOMM (`0x0003`), HID Control/Interrupt (`0x0011`/`0x0013`), AVDTP (`0x0019`).
  - RFCOMM Serial Port Emulation (`rfcomm.sys`):
    - ETSI TS 07.10 multiplexer protocol supporting server channels 1..30.
    - Virtual serial COM port creation (`\Device\BthModem0` / `COM4`) at 115,200 baud.
    - Modem control status signaling (RTC, RTR, DV).
  - Low Energy Audio (LE Audio) & Isochronous Streams:
    - Connected Isochronous Streams (CIS 0x0010) delivering 48 kHz stereo audio at 10ms frame durations via the Low Complexity Communication Codec (LC3).
    - Broadcast Isochronous Streams (BIS / Auracast public broadcast).
  - Bluetooth Bus Enumerator (`bthenum.sys`):
    - Plug-and-Play (PnP) peripheral device tree managing paired devices.
    - Pre-seeded devices: Titan Wireless Mechanical Keyboard (HID Keyboard, `BTHENUM\{00001124-...}`) and PrismAudio Studio Auracast Headset (LE Audio LC3, `BTHENUM\{0000110B-...}`).
  - System Service & Driver Integration: SCM registered drivers for `bthport` (`SERVICE_BOOT_START`), `bthusb` (`SERVICE_SYSTEM_START`), `rfcomm` (`SERVICE_SYSTEM_START`), and `bthenum` (`SERVICE_SYSTEM_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `bth` / `bt` / `bthport` (`status`, `devices` / `list`, `l2cap`, `rfcomm`, `leaudio` / `iso`, `test`).

### 3.33 TitanWiFi & NexusWiFi (Wi-Fi 7 / 802.11be & WDI NetAdapterCx Subsystem)
- **Designation:** `TitanWiFi` (Wi-Fi 7 Miniport Subsystem) / `NexusWiFi` (WDI Framework & NetAdapterCx Stack)
- **Role:** Clean-room Windows WLAN Device Driver Interface (WDI) framework (`wdiwifi.sys`), Network Adapter WDF Class Extension (`netadaptercx.sys`), and Sovereign TitanWiFi 7 (802.11be) PCIe Miniport Driver (`titanwifi.sys`) authored from IEEE 802.11be-2024 and open `win32metadata`.
- **Heritage:** Conceived as the ultra-high throughput wireless counterpart to RazzleNet and TitanBTH, delivering multi-gigabit wireless networking, sub-millisecond deterministic latency, and multi-band radio coordination.
- **Capabilities:**
  - Standard Wi-Fi Driver C ABI exports (`wdiwifi.sys`, `netadaptercx.sys`, `titanwifi.sys`): `WdiInitialize`, `WdiRegisterMiniportDriver`, `WdiDeregisterMiniportDriver`, `WdiSendTaskCommand`, `WdiIndicateTaskComplete`, `WdiGetAdapterCapabilities`, `NetAdapterCreate`, `NetAdapterStart`, `NetAdapterStop`, `TitanWiFiInitialize`, `TitanWiFiTransmitFrame`.
  - Multi-Link Operation (MLO):
    - Simultaneous Transmit and Receive (STR / MLMR) bonding across 2.4 GHz, 5 GHz, and 6 GHz bands.
    - Station MLD (`00:1A:7D:DA:72:01`) and AP MLD coordination with hitless link failover and packet-level aggregation.
    - Combined aggregate PHY throughput exceeding 8.64 Gbps (5.76 Gbps on 6 GHz 320 MHz + 2.88 Gbps on 5 GHz 160 MHz).
  - 802.11be Extremely High Throughput (EHT) Radio Architecture:
    - 320 MHz ultra-wide channelization in the 6 GHz band (UNII-5 through UNII-8).
    - 4096-QAM (4K-QAM) constellation modulation delivering 12 bits per OFDM symbol.
    - Preamble Puncturing (Multi-RU) enabling wide channel operation under partial frequency interference.
  - Robust Security Suites:
    - WPA3-Personal with Simultaneous Authentication of Equals (SAE) with Hash-to-Element (H2E).
    - WPA3-Enterprise 192-bit CNSA mode with GCMP-256 (Galois/Counter Mode).
    - Mandatory Protected Management Frames (802.11w PMF).
  - WDI Task & Command Engine:
    - Asynchronous TLV serialized task processing: `WDI_TASK_SCAN`, `WDI_TASK_CONNECT`, `WDI_TASK_DISCONNECT`, `WDI_TASK_DOT11_RESET`, `WDI_TASK_SET_RADIO_STATE`.
    - Capability interrogation and real-time BSS scan catalog maintenance.
  - NetAdapterCx Ring Queues & DMA Datapath:
    - High-throughput Tx/Rx ring descriptor queues bypassing legacy NDIS protocol latency.
    - Hardware packet classification and offload extensions.
  - System Service & Driver Integration: SCM registered drivers for `wdiwifi` (`SERVICE_BOOT_START`), `netadaptercx` (`SERVICE_BOOT_START`), and `titanwifi` (`SERVICE_SYSTEM_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `wifi7` / `wdiwifi` / `titanwifi` / `mlo` (`status`, `scan` / `list`, `mlo`, `connect`, `disconnect`, `radio`, `test`).

### 3.34 TitanUSB4 & NexusUSB4 (USB4 2.0 / Thunderbolt 4 Protocol Tunneling Subsystem)
- **Designation:** `TitanUSB4` (USB4 2.0 Host Router & Transport Fabric) / `NexusUSB4` (Thunderbolt 4 Security & Protocol Tunneling Manager)
- **Role:** Clean-room implementation of the USB4™ Specification Version 2.0 and Intel Thunderbolt™ 4 / 5 architecture (`usb4host.sys`, `thunderbolt.sys`, `usb4router.sys`) authored from open industry specifications and `win32metadata`.
- **Heritage:** Conceived to provide sovereign multi-protocol converged I/O over Type-C, enabling deterministic PCIe tunneling for external GPUs and storage arrays, DisplayPort 2.1 video tunneling, and SuperSpeed USB 3.2 data tunneling with hardware DMA protection.
- **Capabilities:**
  - Standard USB4 Driver C ABI exports (`usb4host.sys`, `thunderbolt.sys`, `usb4router.sys`): `Usb4HostInitialize`, `Usb4HostEnumerateTopology`, `Usb4HostCreatePath`, `Usb4HostDestroyPath`, `Usb4HostGetRouterCapabilities`, `ThunderboltGetSecurityLevel`, `ThunderboltSetSecurityLevel`, `ThunderboltAuthorizeDevice`, `Usb4TunnelPciePacket`.
  - Next-Generation Physical Layer Signaling (PAM3):
    - 80 Gbps symmetric mode (40 Gbps x 2 PAM3) for standard high-speed interconnects.
    - 120 Gbps asymmetric mode (120 Gbps Tx / 40 Gbps Rx PAM3) dynamically engaged for extreme external display bandwidth (dual 8K HDR) and external GPU compute streaming.
  - Protocol Tunneling Layer & Adapters:
    - Native PCIe Tunneling (Adapter Type 0x01/0x02) encapsulating PCIe Transaction Layer Packets (TLP) into USB4 transport frames over Hop ID paths, bridging external devices directly into the `TitanPCI` bus hierarchy.
    - DisplayPort 2.1 Tunneling (Adapter Type 0x03/0x04) supporting UHBR20 video multiplexing.
    - SuperSpeed USB 3.2 Tunneling (Adapter Type 0x05/0x06) streaming 10/20 Gbps USB packets.
  - Connection Manager & Router Topology Tree:
    - Multi-hop router discovery and depth management (Host Router at Depth 0, Tier 1 Docks at Depth 1, Tier 2 eGPUs / Storage Enclosures at Depth 2).
    - Dynamic path creation (`USB4_PATH`) with Hop ID allocation, bandwidth reservation, and credit-based flow control.
  - Thunderbolt DMA Guard & Security Policies:
    - Strict enforcement of security levels: SL0 (No Security), SL1 (User Authorization), SL2 (Secure Connection with HMAC-SHA256 challenge-response peripheral authorization), SL3 (DisplayPort Only - PCIe blocked), SL4 (USB Only).
    - Deep integration with Kernel DMA Protection / IOMMU (Pillar 64).
  - PCIe Miniport Placement: Host Router bound to PCIe BDF `00:07.0` (`VEN_8086&DEV_9A1B`, Intel Arrow Lake class) with 64KB MMIO BAR0 and 8 MSI-X vectors.
  - System Service & Driver Integration: SCM registered drivers for `usb4host` (`SERVICE_BOOT_START`), `thunderbolt` (`SERVICE_BOOT_START`), and `usb4router` (`SERVICE_SYSTEM_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `usb4` / `thunderbolt` / `tbt` / `titanusb4` (`status`, `tree` / `topology`, `paths`, `security`, `asymmetric`, `tunnel`, `test`).

### 3.35 TitanNPU & NexusNPU (Neural Processing Unit & Microsoft Compute Driver Model Subsystem)
- **Role:** Sovereign AI acceleration subsystem implementing the Microsoft Compute Driver Model (MCDM 1.0/2.0) and dedicated Neural Processing Unit (NPU) accelerator driver architecture for modern Copilot+ PC silicon.
- **Capabilities:**
  - Standard MCDM & NPU Driver C ABI exports (`mcdm.sys`, `npu.sys`, `titannpu.sys`): `McdmDeviceCreate`, `McdmDeviceDestroy`, `McdmCreateCommandQueue`, `McdmSubmitCommandBuffer`, `McdmSignalFence`, `McdmWaitForFence`, `McdmAllocateVirtualMemory`, `McdmFreeVirtualMemory`, `NpuGetCapabilities`, `NpuExecuteModel`, `NpuSetPowerState`, `NpuGetTelemetry`, `TitanNpuHardwareReset`.
  - Microsoft Compute Driver Model (MCDM 1.0/2.0) Architecture:
    - Headless compute execution queues eliminating display/presentation engine overhead.
    - 64-bit monotonic fence synchronization (`McdmFence`) with lock-free atomic advancement.
    - Asynchronous command rings with zero CPU interrupt overhead during model weight streaming.
    - Advanced virtual memory management (`McdmAllocateVirtualMemory`) supporting on-chip high-bandwidth scratchpad SRAM (`LocalSram`) and host-visible pinned memory.
  - Multi-Tile Neural Compute Engine (NCE Array):
    - 4 independent compute tiles operating at 1600 MHz with 16 MB total on-chip dedicated SRAM cache (4 MB per tile).
    - 4,096 INT8 MAC units per tile (16,384 total MACs) delivering 48.0 INT8 TOPS, 24.0 FP16 TFLOPS, and 48.0 FP8 TOPS (exceeding Microsoft's 40 TOPS Copilot+ requirement).
  - Tensor Precision & Operator Matrix:
    - Native hardware execution for INT4, INT8, FP8 (E4M3 / E5M2), FP16, BF16, and FP32 tensor formats.
    - Dedicated silicon acceleration for modern generative AI operators: `MatMul`, `Conv2D`, `LayerNorm`, `RMSNorm`, `Softmax`, `RoPE` (Rotary Positional Embeddings), `Attention`, and `SiLU`/`GELU`.
  - Pre-Compiled DirectML / ONNX Model Catalog:
    - Embedded execution paths for Phi-3 Mini 4K Instruct (3.8B INT4 SLM), LLaMA-3 8B INT4, Mistral 7B, Mobile Segment Anything, and DirectSR 4x super-resolution @ 240 FPS.
  - Intelligent Power & Thermal Management:
    - Dynamic power states: D0 Active (15W peak), D0 Low-Power (3.5W with tile clock gating), D3 Hot sleep (< 1ms resume), and D3 Cold (0W).
  - PCIe Miniport Placement: Processing Accelerator bound to PCIe BDF `00:08.0` (`VEN_8086&DEV_7D1D`, Class 0x12) with 16MB MMIO BAR0, 128MB prefetchable SRAM BAR2, and 16 MSI-X vectors.
  - System Service & Driver Integration: SCM registered drivers for `mcdm` (`SERVICE_BOOT_START`), `npu` (`SERVICE_BOOT_START`), and `titannpu` (`SERVICE_BOOT_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `npu` / `mcdm` / `titannpu` / `ai` (`status`, `caps` / `info`, `tiles`, `models`, `infer <model>`, `benchmark`, `power <state>`).

### 3.36 TitanCXL & NexusCXL (Compute Express Link & Heterogeneous Memory Fabric Subsystem)
- **Role:** Sovereign memory fabric and accelerator interconnect subsystem implementing the Compute Express Link™ (CXL™) Specifications Revision 2.0 and 3.1.
- **Capabilities:**
  - Standard CXL Driver C ABI exports (`cxlhost.sys`, `cxlmem.sys`, `cxlbus.sys`): `CxlHostInitialize`, `CxlHostGetVersion`, `CxlHostEnumerateBridges`, `CxlMemGetDeviceInfo`, `CxlMemConfigureHdmDecoder`, `CxlMemGetSmartHealth`, `CxlMemQueryPoisonList`, `CxlMemInjectPoison`, `CxlMemMigratePages`, `CxlBusRegisterDevice`, `CxlBusGetDeviceCount`, `CxlBusSendMailboxCommand`.
  - Protocol Triad Implementation:
    - **`CXL.io`:** Enhanced PCIe 5.0/6.0 configuration space, device enumeration, AER, and non-coherent DMA.
    - **`CXL.cache`:** Coherent device caching of host memory with ultra-low latency D2H/H2D flits.
    - **`CXL.mem`:** Byte-addressable host read/write access to device-attached volatile (DRAM) or non-volatile memory via M2S/S2M flits.
  - Multi-Generation Device Topologies:
    - **Type 1 Accelerators:** Offload processors without host memory (CXL.io + CXL.cache).
    - **Type 2 Dense Accelerators:** High-performance GPUs/NPUs with local coherent HBM (CXL.io + CXL.cache + CXL.mem).
    - **Type 3 Memory Expanders:** Byte-addressable DRAM expanders and memory pooling blades (CXL.io + CXL.mem).
  - Host-Managed Device Memory (HDM) Decoders:
    - Decoders 0..3 mapping CXL device memory directly into the System Physical Address (SPA) map with configurable interleave granularities (256B to 16KB) and ways (1 to 16-way).
  - Dynamic Memory Tiering (DMT) & NUMA Node 1 Expansion:
    - Far Memory NUMA Node 1 managing 128GB expansion capacity (~140ns read latency, 64 GB/s PCIe Gen5 x16 bandwidth) with automated hot/cold page migration.
  - CXL Mailbox Command Processing & S.M.A.R.T. Telemetry:
    - Standard command set (`IDENTIFY_MEMORY_DEVICE`, `GET_SMART_HEALTH`, `GET_POISON_LIST`, `INJECT_POISON`, `CLEAR_POISON`).
  - Address Poisoning & Error Isolation:
    - Hardware-isolated 64-byte poisoned cache line table preventing blue screen bugchecks.
  - PCIe Bus Placement: CXL Host Bridge at `00:09.0` (`VEN_1E98&DEV_0001`, Bridge Class 0x06, Subclass 0x04) bridging to Bus 4 hosting `TitanCXL 128GB DDR5 Expander` (`04:00.0`) and `TitanCXL Type 2 Heterogeneous Accelerator` (`04:01.0`).
  - System Service & Driver Integration: SCM registered drivers for `cxlhost` (`SERVICE_BOOT_START`), `cxlmem` (`SERVICE_BOOT_START`), and `cxlbus` (`SERVICE_SYSTEM_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `cxl` / `cxlmem` / `cxlhost` / `cxlbus` / `titancxl` (`status`, `devices` / `list`, `hdm` / `decoders`, `smart` / `health`, `numa` / `tiering`, `poison`, `mailbox`, `test`).

### 3.37 TitanUCSI & NexusUCSI (USB Type-C UCSI 2.1/3.0 & Power Delivery 3.1 Subsystem)
- **Role:** Sovereign USB Type-C subsystem implementing the USB Type-C™ Connector System Software Interface (UCSI) Revision 2.1/3.0 and USB Power Delivery Specification Revision 3.1 Version 1.8.
- **Capabilities:**
  - Standard UCSI & USB-C Driver C ABI exports (`ucsi.sys`, `usbc.sys`, `ppm.sys`): `UcsiPpmInitialize`, `UcsiPpmGetVersion`, `UcsiGetConnectorCount`, `UcsiGetConnectorStatus`, `UcsiGetCableProperties`, `UcsiNegotiateEprContract`, `UcsiSwapPowerRole`, `UcsiSwapDataRole`, `UcsiGetPdoList`, `UcsiAcknowledgeCci`.
  - Platform Policy Manager (PPM) & Operating System Policy Manager (OPM) Architecture:
    - Standard ACPI mailbox interface on `\_SB.UBTC` (`USBC000` / `PNP0CA0`) at MMIO base `0xFEDC0000`.
    - 16-byte Mailbox Message In/Out registers, 64-bit Control register, and asynchronous Connector Change Indication (CCI) interrupts.
  - Multi-Port Physical Connector Matrix:
    - **Port 1 (Left Rear):** 240W Extended Power Range (EPR) Sink charging port (48V @ 5A peak power delivery).
    - **Port 2 (Left Front):** DisplayPort 2.1 UHBR20 Alt-Mode 65W Source port (DP 4-lane video output + 20V @ 3.25A PD Source).
    - **Port 3 (Right Rear):** 15W High-Speed USB 3.2 / USB4 Peripheral SSD port (5V @ 3A Source, DFP Host data role).
    - **Port 4 (Right Front):** 27W Programmable Power Supply (PPS) Fast-Charging port (9V @ 3A Source, dynamic voltage scaling).
  - USB Power Delivery 3.1 Extended Power Range (EPR) Protocol:
    - Fixed PDOs: 5V/3A (15W), 9V/3A (27W), 15V/3A (45W), 20V/5A (100W Standard Power Range - SPR), 28V/5A (140W), 36V/5A (180W), and 48V/5A (240W Extended Power Range - EPR).
    - Adjustable Voltage Supply (AVS): Fine-grained power stepping from 15V to 48V in 100mV increments for high-efficiency workstation charging.
  - Cable Electronic Marker (E-Marker) SOP' Discovery:
    - Queries active/passive cable capabilities including 50V/5A electrical ratings and 80 Gbps PAM3 high-speed signaling flags.
  - Dynamic Role Renegotiation:
    - Power Role Swap (`PR_SWAP` Source <-> Sink) enabling bidirectional laptop-to-monitor and laptop-to-phone charging.
    - Data Role Swap (`DR_SWAP` DFP Host <-> UFP Device) and VCONN swapping.
  - System Service & Driver Integration: SCM registered drivers for `ucsi` (`SERVICE_BOOT_START`), `usbc` (`SERVICE_BOOT_START`), and `ppm` (`SERVICE_SYSTEM_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `ucsi` / `usbpd` / `titanucsi` / `usbc` (`status`, `ports` / `list`, `pd` / `power`, `cable`, `swap`).

### 3.38 TitanBypassIO & NexusBypassIO (DirectStorage 1.2 & BypassIO Subsystem)
- **Role:** Sovereign high-speed storage acceleration subsystem implementing the Microsoft Windows BypassIO architecture (`FSCTL_MANAGE_BYPASS_IO` `0x00090280`), DirectStorage 1.2 GDeflate GPU decompression driver stack (`bypassio.sys`), and Storage Quality of Service (`storqos.sys`).
- **Capabilities:**
  - Standard BypassIO Driver C ABI exports (`bypassio.sys`, `storqos.sys`): `BypassIoInitialize`, `BypassIoGetVersion`, `BypassIoManageOperation`, `BypassIoProcessFastRead`, `BypassIoQueryVolumeStatus`, `BypassIoPauseVolume`, `BypassIoResumeVolume`, `DirectStorageKernelDecompress`, `DirectStorageTransferNvmeToVram`, `StorQosConfigureStream`, `StorQosGetTelemetry`.
  - Kernel-Mode Fast-Path Architecture (`bypassio.sys`):
    - Completely eliminates filesystem minifilter stack traversal (`fltmgr.sys`) for authorized game streaming and dense compute I/O.
    - Achieves sub-25 microsecond read latency and zero CPU cache pollution by delegating NVMe completion rings straight to target memories.
  - Minifilter Stack Compatibility Enforcement:
    - Scans and validates all registered volume minifilters (`EmeraldFlt`, `SentinelScanFlt`, `WfpTrafficFlt`).
    - Rejects BypassIO activation with `FS_BPIO_STATUS_FILTER_INCOMPATIBLE` and identifies culprit driver if any non-compatible filter is attached.
  - Dynamic Volume Stack State Management:
    - Supports Volume Stack Pause (`FS_BPIO_OP_VOLUME_STACK_PAUSE`) and Resume (`FS_BPIO_OP_VOLUME_STACK_RESUME`) for atomic VSS volume shadow copies.
  - Direct NVMe-to-VRAM DMA Engine:
    - Direct DMA streaming from PCIe Gen 5 x4 NVMe controllers to WDDM 3.2 GPU Virtual Address (`GPUVA`) apertures exceeding 7.4 GB/s.
    - Enforces 64KB page / 2MB large page alignment constraints for GPU memory protection.
  - DirectStorage 1.2 GDeflate Hardware & Compute Acceleration:
    - Full container format compliance (`GDEFLATE_MAGIC` `0x44474447`, `GDEFLATE_VERSION` `0x00010200`) with 64KB tile partitioning and ~2.4x compression ratio.
    - Dual decompression pipelines: Direct3D 12 compute shader dispatch on GPU or multi-threaded CPU SIMD fallback.
  - Storage Quality of Service (`storqos.sys`):
    - 3-tier traffic scheduling: Tier 0 DirectStorage Real-Time Streaming (5.5 GB/s guaranteed, 70% weight, 1.5M IOPS), Tier 1 Foreground Applications (25% weight), and Tier 2 Background Maintenance (5% weight).
  - System Service & Driver Integration: SCM registered drivers for `bypassio` (`SERVICE_BOOT_START`) and `storqos` (`SERVICE_SYSTEM_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `bypassio` / `bpio` / `storqos` / `titanstorage` (`status`, `filters` / `stack`, `qos`, `gdeflate`, `bench` / `benchmark`, `pause` / `resume`).

### 3.39 TitanPMEM & NexusPMEM (Persistent Memory & Direct Access Storage Subsystem)
- **Role:** Sovereign non-volatile byte-addressable persistent storage subsystem implementing the Microsoft Windows PMEM architecture (`pmem.sys`), Direct Access filesystem filter driver (`dax.sys`), JEDEC NVDIMM-N standard (JESD245), ACPI 6.5 NFIT specification, and SNIA NVM Programming Model.
- **Capabilities:**
  - Standard PMEM Driver C ABI exports (`pmem.sys`, `dax.sys`): `PmemInitialize`, `PmemGetVersion`, `PmemGetDeviceCount`, `PmemGetDeviceInfo`, `PmemGetPoolCount`, `PmemGetPoolInfo`, `PmemFlushCacheLine`, `PmemGetTelemetry`, `DaxMapFileToMemory`, `DaxUnmapFile`.
  - ACPI 6.5 NFIT Table & Range Parsing:
    - Parses System Physical Address (SPA) range structures, NVDIMM Control Regions, and Interleave structures.
    - Full standard GUID compliance: `GUID_PMEM_BYTE_ADDRESSABLE` (`7305944F-FDDA-44E3-A162-98240E7F3D76`), `GUID_PMEM_BLOCK_TRANSLATION_TABLE` (`1928CDAB-7065-4ADE-B887-6199A7911012`), and `GUID_PMEM_VOLATILE_MEMORY`.
  - Physical NVDIMM Hardware Topology:
    - Pre-seeded dual physical Intel Optane PMEM 300 Series 512GB modules (1 TB aggregate pool) across dual memory controller channels.
    - Health monitoring tracking module temperature (~38.5°C), life percentage used, dirty shutdown counts, and health status codes.
  - App Direct Mode & Direct Access (DAX) Zero-Copy Userland Mappings:
    - Zero-copy virtual memory window assignment at `0x7FFF00000000ULL` (`VirtualAlloc` with `SEC_DAX` flag `0x02000000`).
    - Eliminates operating system page cache, filesystem buffer trees, and page fault overhead, providing direct byte-addressable CPU loads and stores (~210ns read, ~180ns write).
  - Block Translation Table (BTT) Crash Protection:
    - Atomic 4KB sector update engine with pre-allocation table logging, guaranteeing no torn writes on sudden power loss for conventional block filesystems.
  - Hardware Persistence Barrier Flush:
    - Enforces Cache Line Write Back (`clwb`) and Store Fence (`sfence`) pipeline synchronization with sub-100ns latency (~85ns).
    - Asynchronous DRAM Refresh (ADR) circuit armed and active, guaranteeing in-flight write queue drainage to non-volatile media upon system power failure.
  - System Service & Driver Integration: SCM registered drivers for `pmem` (`SERVICE_BOOT_START`) and `dax` (`SERVICE_SYSTEM_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `pmem` / `optane` / `nvdimm` / `dax` / `titanpmem` (`status`, `devices` / `list`, `pools`, `dax`, `bench` / `benchmark`).

### 3.40 TitanRDMA & NexusSMB (RDMA & SMB Direct Storage Subsystem)
- **Role:** Sovereign Remote Direct Memory Access (RDMA) and high-speed cluster storage subsystem implementing Microsoft NetworkDirect Kernel Provider Interface (NDKPI 2.0), RoCE v2 (UDP 4791), InfiniBand NDR, and SMB Direct (`[MS-SMBD]`).
- **Capabilities:**
  - Standard RDMA Driver C ABI exports (`ndisrdma.sys`, `smbdirect.sys`): `RdmaInitialize`, `RdmaGetVersion`, `RdmaCreateProtectionDomain`, `RdmaCreateCompletionQueue`, `RdmaCreateQueuePair`, `RdmaRegisterMemoryRegion`, `RdmaPostSend`, `RdmaPostReceive`, `RdmaPollCq`, `RdmaGetTelemetry`, `SmbDirectInitialize`, `SmbDirectConnect`, `SmbDirectDisconnect`, `SmbDirectRemoteWrite`, `SmbDirectRemoteRead`.
  - True Kernel Bypass & MMIO Doorbells:
    - Userland and kernel storage queues directly ring hardware doorbell apertures (`DB_RECORD`), completely bypassing NDKPI IRP and spinlock contention.
  - Zero-Copy Hardware Memory Registration (MR):
    - Fast register memory keys (`lkey` / `rkey`) with Protection Domain isolation, allowing remote direct memory access at wire speed without CPU page table locks.
  - Sub-3 Microsecond Remote Latency:
    - 100 Gbps line rate with ~1.8us RDMA Write and ~2.4us RDMA Read latency, delivering over 12.2 GB/s sustained throughput.
  - TitanRoCE Autonomous Lossless & Lossy Recovery:
    - Eliminates fragile enterprise Priority Flow Control (PFC / 802.1Qbb) switch configuration requirements, executing hardware-level packet loss recovery and adaptive retransmission even across standard unmanaged Ethernet switches.
  - SMB Direct 3.1.1 & Remote DirectStorage:
    - Native zero-copy file sharing transport for clustered storage, streaming remote NVMe payloads directly into client GPU VRAM (`GPUVA`) and DAX persistent memory without CPU cache pollution.
  - System Service & Driver Integration: SCM registered drivers for `ndisrdma` (`SERVICE_BOOT_START`) and `smbdirect` (`SERVICE_SYSTEM_START`), dynamic loader exports, and Version Database registration.
  - Interactive CLI: `rdma` / `roce` / `infiniband` / `smbdirect` / `titanrdma` (`status`, `devices` / `list`, `qp` / `queues`, `mr` / `memory`, `smb`, `bench` / `benchmark`).

### 3.41 TitanPluton & AegisPluton (Microsoft Pluton Security Processor & Hardware Root-of-Trust Subsystem)
- **Role:** Sovereign on-die cryptographic security processor and hardware Root-of-Trust (RoT) subsystem implementing the Microsoft Pluton architecture, TCG TPM 2.0 Library Specification, ACPI 6.5 Hardware Security Devices (`\_SB.PLTN`), and Windows Pluton driver (`pluton.sys`).
- **Capabilities:**
  - Standard Pluton Driver C ABI exports (`pluton.sys`): `PlutonInitialize`, `PlutonGetVersion`, `PlutonGetCapabilities`, `PlutonReadPcr`, `PlutonExtendPcr`, `PlutonSealData`, `PlutonUnsealData`, `PlutonGenerateRandom`, `PlutonGetTelemetry`.
  - Physical Bus-Sniffing Immunity:
    - 100% on-die CPU silicon integration connected via internal crossbar interconnect fabric (`0xFEB00000`), completely eliminating exposed motherboard traces (LPC/SPI/I2C) vulnerable to hardware interposer bus-probing attacks.
  - On-Die Platform Configuration Registers (PCRs):
    - 24 on-die SHA-256 PCR banks (PCR 0..23) tracking firmware integrity (PCR 0), Secure Boot configuration (PCR 7), BitLocker policies (PCR 11), and operating system boot phases with cryptographic extend operations.
  - Hardware Keystore & Policy Sealing:
    - Dedicated secure enclave housing Storage Root Keys (SRK ECC-P384), Endorsement Keys (EK RSA-4096), and sealed BitLocker Volume Master Keys (VMK-Sealed AES-256-GCM).
    - Multi-PCR policy sealing (`PlutonSealData` / `PlutonUnsealData`) with automatic tamper rejection (`STATUS_ACCESS_DENIED`) on state changes.
  - True Random Number Generator (TRNG):
    - High-entropy hardware generator providing secure random nonces and cryptographic keys with zero external bias.
  - System Service & Driver Integration: SCM registered boot driver for `pluton` (`SERVICE_BOOT_START`), dynamic loader exports, and Version Database registration (`pluton.sys` 10.0.26100.1).
  - Interactive CLI: `pluton` / `titanpluton` / `aegispluton` (`status`, `pcrs`, `keys`, `seal`, `bench` / `benchmark`).

---

## 4. Coding & Header Conventions

Whenever adding or extending a subsystem:
1. Include a clean-room provenance banner citing public, openly licensed specifications.
2. Use the designated sovereign namespace (or alias to it).
3. Ensure zero external dependencies outside the ISO C++23 standard library.
4. Maintain deterministic memory footprints with zero background telemetry.
