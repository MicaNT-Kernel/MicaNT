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
| **Audio Architecture** | **PrismAudio** | `micant::audio`<br/>`micant::sound` | `conhost.hpp` (audio mixer) | Direct audio sibling to PrismX, handling multi-channel PCM software mixing, wave out synthesis, and session volume ducking. |

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
- **Capabilities:**
  - Multi-stream PCM software mixer with 16-bit 44.1kHz / 48kHz audio pipelines.
  - Low-latency session management and synthesized frequency tone generation.

---

## 4. Coding & Header Conventions

Whenever adding or extending a subsystem:
1. Include a clean-room provenance banner citing public, openly licensed specifications.
2. Use the designated sovereign namespace (or alias to it).
3. Ensure zero external dependencies outside the ISO C++23 standard library.
4. Maintain deterministic memory footprints with zero background telemetry.
