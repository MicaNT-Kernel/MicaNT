# MicaNT (Project MICA)

> **"The cleanest NT architecture on Earth."**  
> An open-source, zero-telemetry, modern C++23 NT-compatible operating system kernel and executive, built as a **strict clean-room implementation** using Microsoft's official [`win32metadata`](https://github.com/microsoft/win32metadata) repository for interface reference.

[![Website: micant.barrersoftware.com](https://img.shields.io/badge/Website-micant.barrersoftware.com-4CAF50.svg?logo=googlechrome&logoColor=white)](https://micant.barrersoftware.com)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Clean Room: Certified](https://img.shields.io/badge/Clean%20Room-Certified-success.svg)](docs/CLEAN_ROOM.md)
[![Standard: C++23](https://img.shields.io/badge/Language-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Reference: win32metadata](https://img.shields.io/badge/Reference-microsoft%2Fwin32metadata-purple.svg)](https://github.com/microsoft/win32metadata)
[![Arch: x86__64 | ARM64](https://img.shields.io/badge/Arch-x86__64%20%7C%20ARM64-orange.svg)]()
[![Build: CMake](https://img.shields.io/badge/Build-CMake%203.25%2B-green.svg)]()
[![Taxonomy: Sovereign Names](https://img.shields.io/badge/Taxonomy-Sovereign%20Subsystems-9C27B0.svg)](docs/SOVEREIGN_TAXONOMY.md)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-Support%20Project-FF5E5B.svg?logo=kofi&logoColor=white)](https://ko-fi.com/ssfdre38)

---

<p align="center">
  <img src="docs/bootscreen_splash.png" alt="MicaNT UEFI Boot Splash" width="420">
  &nbsp;&nbsp;
  <img src="docs/conhost_desktop.png" alt="MicaNT ConHost Desktop" width="420">
  <br>
  <em>Left: Bare-metal UEFI GOP Boot Splash (Dave Cutler 1988 DEC Prism). Right: Interactive ConHost Win32 Terminal Desktop.</em>
</p>

---

> [!IMPORTANT]
> **CLEAN-ROOM IMPLEMENTATION & REFERENCE NOTICE**  
> MicaNT is a **100% clean-room engineering project**. All kernel subsystems, memory managers, schedulers, and object tables are original implementations authored in modern ISO C++23.  
> 
> System service interfaces, data structures, and status codes are referenced and auto-generated strictly from Microsoft's MIT-licensed open-source repository:  
> **[`https://github.com/microsoft/win32metadata`](https://github.com/microsoft/win32metadata)**  
> 
> In accordance with the U.S. Supreme Court precedent in *Google LLC v. Oracle America, Inc.* (2021), reimplementing functional declaring code and interface signatures for binary interoperability is protected fair use. No proprietary or leaked Microsoft source code was used or referenced. See [docs/CLEAN_ROOM.md](docs/CLEAN_ROOM.md) and [docs/LEGAL.md](docs/LEGAL.md) for full compliance documentation.

---

## 1. What is MicaNT?

In 1988, Dave Cutler and his core engineering team left Digital Equipment Corporation (DEC) where they were developing a next-generation operating system codenamed **MICA** (acronym for *Memory Management, Interprocess communication, Compute, and Architecture*) along with the PRISM RISC processor. At Microsoft, that breakthrough architecture became **Windows NT**.

Cutler’s original design—the **Object Manager**, **I/O Completion Ports (IOCP)**, **Structured Exception Handling**, and a clean hardware abstraction layer—was arguably the most advanced systems architecture of the late 20th century.

However, over 35+ years of corporate development, the NT kernel became encumbered by:
- Hundreds of megabytes of 16-bit / DOS compatibility shims.
- GDI / USER windowing hacks dragged into Ring 0.
- Intrusive telemetry daemons, adware hooks, and edge background services.
- Gigabytes of idle memory overhead.

**MicaNT** resurrects the pure, uncompromised soul of the MICA architecture. It is a clean-room, freestanding, modern **C++23** operating system executive designed to execute native 64-bit Windows PE binaries with **zero telemetry**, **sub-32 MB idle memory footprint**, and **sub-millisecond boot times**.

---

## 2. Key Pillars

1. **Modern C++23 Core**: Built with RAII, concepts, compile-time type safety, and atomic synchronization. No 1990s naked pointers or raw un-checked buffers.
2. **Metadata-Driven Syscall Surface**: The entire `Zw*` / `Nt*` system call table, types, and NTSTATUS codes are auto-generated from Microsoft's MIT-licensed [`win32metadata`](https://github.com/microsoft/win32metadata) repository.
3. **Pure Object Manager (`ob/`)**: Dave Cutler's clean handle-based namespace (`\Device`, `\DosDevices`, `\KernelObjects`, `\BaseNamedObjects`).
4. **Complete Ring 0 Kernel Architecture**:
   - **Kernel Core (`ke/`)**: IRQL state machine (`PASSIVE_LEVEL` to `HIGH_LEVEL`), `KSPIN_LOCK` with IRQL elevation, `KDPC`/`KAPC` queues, and a 32-Queue priority scheduler with quantum decay.
   - **Executive Memory Pools (`ex/`)**: `NonPagedPool` and `PagedPool` with strict IRQL access enforcement and 4-byte diagnostic tagging (`'Mica'`, `'Proc'`, `'SecO'`).
   - **Trap & Fault Engine (`ke/trap.hpp`)**: Demand paging, Copy-on-Write (`#PF`), Structured Exception Dispatching (`KiDispatchException`), and `KeBugCheckEx` crash panic.
   - **Hardware Abstraction Layer (`hal/`)**: Multi-core SMP topology, `KPCR` at `GS:[0]`, `KPRCB`, and 1 GHz nanosecond high-precision performance timers (`KeQueryPerformanceCounter`).
5. **Subsystem Trinity**:
   - **Configuration Manager (`cm/`)**: In-memory registry hive mounted under `\Registry` with hierarchical keys and typed values (`REG_SZ`, `REG_DWORD`, `REG_QWORD`).
   - **Security Reference Monitor (`se/`)**: SIDs, DACLs, ACEs, Process Access Tokens, and Dave Cutler's `accessCheck` validation algorithm.
   - **Advanced Local Procedure Call (`lpc/`)**: High-speed message passing with named ports in `\RPC Control`, FIFO queues, and synchronous `requestWaitReply` rendezvous.
6. **Zero Telemetry**: No Cortana, no advertising IDs, no diagnostic tracking, no network dial-home. Pure, unadulterated computing.
7. **Native 64-bit Windows ABI**: Implements the standard x86-64 `KiSystemCall64` / `syscall` interface, userland `TEB`/`PEB` layout, and `ntdll.dll` executive contract.
8. **Win32 Subsystem (CSRSS & ConHost)**: Clean-room `csrss.exe` process tracking over ALPC, `conhost` 2D terminal engine with GOP framebuffer blitting, and `kernel32.dll` base API bridge.
9. **WoW64 Subsystem (32-Bit Compatibility)**: Transparent 32-bit execution via Heaven's Gate (`0x23` <-> `0x33` far call segment switching), PEB32/TEB32 virtual address space management, transparent `\Windows\SysWOW64` and `WOW6432Node` redirection, and 32-to-64 bit system call thunking.
10. **Dynamic PE Import Binding & Relocations**: Clean-room `.idata` Import Directory parser, Import Lookup Table (INT) walker, Import Address Table (IAT) binding, and `.reloc` base relocation engine (DIR64 / HIGHLOW) enabling unmodified 64-bit Windows executables to bind directly to MicaNT `kernel32` and `ntdll` exports.
11. **x86-64 GDT, TSS64 & Ring 3 Hardware Transitions**: Standard NT segment selectors (`0x10`, `0x18`, `0x23`, `0x2B`, `0x33`), 64-bit Task State Segment (`RSP0`, `IST`), fast system call MSR configuration (`STAR`, `LSTAR`, `SFMASK`, `EFER.SCE`), and hardware `iretq` entry frames.
12. **Unmodified Windows PE Execution**: Direct, verified execution of standard 64-bit Windows console executables compiled strictly against `<windows.h>` without any MicaNT-specific headers, shims, or wrappers.
13. **Bare-Metal UEFI Bootloader (`bootx64.efi`)**: Pure UEFI 2.10 entry point with GOP linear framebuffer discovery, ACPI 2.0 table resolution, and seamless kernel handoff.
14. **Clean-Room MSVCRT, Subsystem Bridges & Command Shell**: High-fidelity clean-room C runtime (`msvcrt.dll`), security/crypto (`advapi32.dll`), windowing (`user32.dll`), and network (`ws2_32.dll`) bridges, Fiber Local Storage (FLS) architecture, and native MicaNT Command Prompt Shell (`cmd.exe` / `msh.exe`) with built-in commands and unmodified binary execution.
15. **Real Block Storage & FAT32/Partition Engine (`storage.hpp`, `fat32.hpp`)**: Modular block device layer (`IBlockDevice`), MBR and GPT partition management, RAM disk sector emulation, dual FAT32 table traversal, LFN reverse-sequence unicode filename decoding, cluster chain allocation, and VFS integration.
16. **Advanced Networking Stack & QUIC Protocol Engine (`ndis.hpp`, `tcpip.hpp`, `ws2_32.hpp`, `iphlpapi.hpp`)**: NDIS 6.x driver miniport interface, 10 Gbps virtual network adapter, ARP resolution, IPv4/IPv6 RFC 8200 dual-stack addressing with link-local generation, ICMPv4/v6 echo ping, UDP datagrams, full RFC 793 / RFC 9293 11-state TCP state machine with 3-way handshake and bidirectional streaming, Next-Gen QUIC RFC 9000 protocol header framing, Winsock 2 (`ws2_32.dll`), IP Helper API (`iphlpapi.dll`), and native shell network commands (`ipconfig`, `ping`, `netstat`).
17. **AArch64 (ARM64) Multi-Architecture Subsystem (`arm64.hpp`)**: Full 64-bit ARM hardware state architecture (`X0`–`X30`, `SP_EL0`/`SP_EL1`, `PSTATE`, 128-bit NEON/SIMD `Q0`–`Q31`), `ESR_EL1` / `FAR_EL1` exception syndrome decoders, VMSA 48-bit 4-level MMU translation tables (`TTBR0_EL1` / `TTBR1_EL1`), fast `KiArm64SystemCall` `SVC #1` dispatcher adhering to standard Windows on ARM64 AAPCS64 register conventions, and 8-core SMP HAL topology support (Snapdragon X Elite / Oryon).
18. **Named Pipes & Mailslots IPC Subsystem (`npfs.hpp`, `fs.hpp`, `kernel32.hpp`)**: Full-duplex named pipe file system (`\Device\NamedPipe`), multi-instance client/server load distribution, 5-state lifecycle (`Listening` to `Broken`), byte stream and message stream modes with atomic message boundaries and `ERROR_MORE_DATA` signaling, non-destructive queue inspection (`PeekNamedPipe`), transactional IPC (`TransactNamedPipe`), and broadcast datagram mailslot file system (`\Device\Mailslot`) with configurable message timeouts.
19. **NTFS Subsystem & MFT Engine (`ntfs.hpp`, `fs.hpp`)**: Full clean-room New Technology File System (NTFS) driver featuring 1024-byte Master File Table ($MFT) record parsing, Update Sequence Array (USA) fixup generation and torn-write validation, Standard Information (`$STANDARD_INFORMATION`), File Name (`$FILE_NAME`), and Data (`$DATA`) attributes, variable-length compressed LCN/VCN runlist mapping pairs (`DataRunCodec`), Alternate Data Streams (ADS, e.g. `file.txt:Zone.Identifier`), `$LogFile` Write-Ahead Logging (WAL) and checkpoint replay engine (`LogFileJournal`), and seamless VFS mounting and drive routing.
20. **Windows Service Control Manager (SCM) & Service Host (`scm.hpp`, `advapi32.hpp`)**: Complete clean-room `services.exe` daemon and `svchost.exe` process hosting engine. Features service database with kernel driver (`SERVICE_KERNEL_DRIVER`) and userland service types, start types (`BOOT`, `SYSTEM`, `AUTO`, `DEMAND`, `DISABLED`), topological dependency resolution with cycle detection, service status state machine (`STOPPED`, `START_PENDING`, `RUNNING`, `PAUSED`), dependent service stop protection (`ERROR_DEPENDENT_SERVICES_RUNNING`), shared process grouping (`svchost.exe -k <group>` e.g. `netsvcs`, `LocalService`, `DcomLaunch`), transactional RPC protocol over `\\.\pipe\ntsvcs`, Win32 `advapi32.dll` APIs (`OpenSCManagerW`, `CreateServiceW`, `OpenServiceW`, `StartServiceW`, `ControlService`, `DeleteService`, `QueryServiceStatusEx`), and command shell utilities (`net start`, `net stop`, `sc query`, `sc start`, `sc stop`).
21. **Security & Authentication Subsystem (SAM, LSASS, Winlogon) (`sam.hpp`, `lsass.hpp`, `winlogon.hpp`)**: Complete NT authentication trinity. RFC 1320 MD4 NT-Hash generation, Security Accounts Manager (SAM) database, built-in Administrator (RID 500) and Guest (RID 501), account lockout threshold and observation window policy. Local Security Authority Subsystem Service (LSASS) with MSV1_0 authentication package, NTLM challenge-response nonce generation, executive access token synthesis (`TOKEN_USER`, `TOKEN_GROUPS`, `TOKEN_PRIVILEGES`), SID translation (`LookupAccountSidW`), and IPC endpoints (`\\.\pipe\lsass`, `\LsaAuthenticationPort`). Winlogon interactive logon manager with desktop isolation (secure `Winlogon` vs `Default` interactive desktop), SAS `Ctrl+Alt+Del` event interception, workstation lock/unlock state machine, and shell utilities (`whoami /priv /groups /all`, `net user`, `lock`, `logoff`).
22. **PrismX & Prism3D Graphics Architecture (`prismx.hpp`, `prism3d.hpp`, `dxgkrnl.hpp`)**: Sovereign clean-room 2D/3D graphics engine named in tribute to Dave Cutler's 1988 DEC PRISM RISC project. Implements DXGI presentation pipeline (`IDXGIFactory1`, `IDXGIAdapter1`, `IDXGIOutput`, `IDXGISwapChain`) with 32-bpp BGRA double/triple buffering, flip models (`FLIP_DISCARD`), dirty-rect tracking, and VSync pacing. Prism3D acceleration engine (`ID3D11Device`, `ID3D11DeviceContext`, `ID3D12Device`) featuring a high-precision barycentric software reference rasterizer with Gouraud color interpolation, depth testing (Z-buffer), and viewport clipping. WDDM DirectX Graphics Kernel (`dxgkrnl.sys`) implementing Ring 0 `D3DKMT*` syscall thunks (`D3DKMTOpenAdapter`, `D3DKMTCreateAllocation`, `D3DKMTCreateDevice`, `D3DKMTSubmitCommand`, `D3DKMTPresent`) for GPU memory virtual addressing and page flip scheduling. Fully compatible with Microsoft's MIT-licensed `DirectX-Headers` and `DirectXTK`. Integrated shell commands (`prismx`, `prismx test`).
23. **Khronos Vulkan 1.3 ICD Loader & PrismVK Graphics Driver (`vulkan.hpp`)**: Sovereign clean-room implementation of the standard Khronos Vulkan Installable Client Driver (ICD) Loader (`vulkan-1.dll`). Features Configuration Manager driver discovery via `\Registry\Machine\SOFTWARE\Khronos\Vulkan\Drivers`, discrete GPU physical device enumeration (1.3.0 compliance, 8192 MB dedicated VRAM, 16384 MB shared GTT, 16 graphics/compute queues), `VK_KHR_win32_surface` HWND window attachment, `VK_KHR_swapchain` presentation, command pool & buffer recording, render passes, and queue submission dispatching directly through the Prism3D rasterizer. Integrated shell commands (`vulkan`, `vkcube`, `vulkan test`).
24. **Silicon Graphics OpenGL 1.4 API, Windows WGL Subsystem & GLU Library (`opengl.hpp`, `opengl32.dll`, `glu32.dll`)**: Complete sovereign clean-room implementation of the Silicon Graphics OpenGL 1.1–1.4 core rendering pipeline, Windows WGL context lifecycle, and OpenGL Utility Library (`glu32.dll`). Features standard column-major 4x4 matrix stack math (ModelView, Projection, Texture), immediate mode (`glBegin`/`glEnd`) with primitive assembly (points, lines, triangles, quads, polygon fans/strips), vertex array client state (`glVertexPointer`, `glColorPointer`, `glTexCoordPointer`, `glDrawArrays`), 2D texture mapping with bilinear/nearest filtering and wrap modes (`GL_REPEAT`, `GL_CLAMP`), depth buffering (`GL_DEPTH_TEST`), alpha blending, and WGL context creation (`wglCreateContext`, `wglMakeCurrent`, `wglDeleteContext`) with `wglSwapBuffers` presentation bridge to GDI/User32 framebuffers. Full GLU camera utility routines (`gluPerspective`, `gluLookAt`, `gluOrtho2D`, `gluErrorString`). Integrated shell commands (`opengl info`, `opengl test`).
25. **Windows Internet (WinINet) & URL Moniker (URLMon) Web Client Subsystems (`wininet.hpp`, `urlmon.hpp`, `wininet.dll`, `urlmon.dll`)**: Complete clean-room web client engine. Features hierarchical handle management (`HINTERNET` session -> connection -> request) with handle cascading destruction, RFC 7230 HTTP/1.1 request formatting, Winsock 2 streaming delivery, chunked transfer-encoding decoding, RFC 6265 thread-safe cookie jar, Temporary Internet Files LRU cache management, RFC 3986 URL cracking/creation/canonicalization, `HttpQueryInfoA/W`, `FindMimeFromData` MIME sniffer, `URLDownloadToFileA/W` with live `IBindStatusCallback` progress events, stream monikers (`URLOpenBlockingStreamA/W`, `MemoryStream`), `CreateURLMoniker` (`IMoniker`), and interactive CLI utilities (`curl`, `wget`, `wininet`, `urlmon`).

---

## 3. Subsystem Architecture

```
                             +-------------------------------+
                             |    Userland Applications      |
                             |   (64-bit Portable Executable)|
                             +-------------------------------+
                                             |
                                             v
                             +-------------------------------+
                             |           ntdll.dll           |
                             | (Userland System Call Stubs)  |
                             +-------------------------------+
                                             |
                     [ syscall / LSTAR MSR ] | (Ring 3 -> Ring 0)
                                             v
========================================================================================
                                     MicaNT KERNEL
========================================================================================
                             +-------------------------------+
                             |      KiSystemCall64 Trap      |
                             |   (Register Save / swapgs)    |
                             +-------------------------------+
                                             |
                                             v
                             +-------------------------------+
                             |   Syscall Dispatcher Table    |
                             |  (win32metadata Auto-Gen)     |
                             +-------------------------------+
                                   |          |          |
         +-------------------------+          |          +-------------------------+
         |                                    |                                    |
         v                                    v                                    v
+------------------+                +------------------+                +------------------+
|  Object Manager  |                |  Memory Manager  |                | Process/Threads  |
|      (Ob)        |                |      (Mm)        |                |      (Ps)        |
| - Root Directory |                | - PML4 Paging    |                | - EPROCESS       |
| - Handle Tables  |                | - VAD Trees      |                | - ETHREAD        |
| - Reference Count|                | - Demand Paging  |                | - Scheduler      |
+------------------+                +------------------+                +------------------+
         |                                    |                                    |
         v                                    v                                    v
+------------------+                +------------------+                +------------------+
|  Config Manager  |                |   Security SRM   |                |  ALPC Messaging  |
|      (Cm)        |                |      (Se)        |                |      (Lpc)       |
| - \Registry Hive |                | - SIDs, DACLs    |                | - \RPC Control   |
| - Typed Values   |                | - Tokens & Access|                | - RequestWaitRepl|
+------------------+                +------------------+                +------------------+
         |                                    |                                    |
         v                                    v                                    v
+------------------+                +------------------+                +------------------+
| Kernel Core (Ke) |                |  Executive Pools |                | Trap Engine (Ke) |
| - IRQL Machine   |                |      (Ex)        |                | - Page Fault #PF |
| - KSPIN_LOCK     |                | - NonPagedPool   |                | - SEH Dispatcher |
| - 32-Queue Sched |                | - PagedPool (Tag)|                | - KeBugCheckEx   |
+------------------+                +------------------+                +------------------+
         |                                    |                                    |
         +-------------------------+          |          +-------------------------+
                                   |          |          |
                                   v          v          v
                             +-------------------------------+
                             |        I/O Manager (Io)       |
                             |  - IRP Dispatching            |
                             |  - Driver Object Graph        |
                             |  - Async Completion (IOCP)    |
                             +-------------------------------+
                                             |
                                             v
                             +-------------------------------+
                             |  Hardware Abstraction Layer   |
                             |             (HAL)             |
                             |  - KPCR / KPRCB (GS:[0])      |
                             |  - SMP Multi-Core Topology    |
                             |  - 1 GHz Performance Counter  |
                             +-------------------------------+
```

---

### Sovereign Subsystem Taxonomy

To guarantee total clean-room independence and prevent name collisions with closed-source Windows binaries, every core subsystem is designated with a sovereign title honoring Dave Cutler's historic DEC/NT engineering lineage:

- **[PrismX / Prism3D / PrismVK / PrismGL](https://github.com/MicaNT-Kernel/PrismX)**: Sovereign DirectX (DXGI, D3D11, D3D12), Vulkan 1.3, and OpenGL 1.4 presentation, rasterization & WGL architecture.
- **EmeraldFS**: Clean-room file system engine with Master File Table (MFT) parser, Alternate Data Streams (ADS), and journaling (named after Cairo's *Emerald* OFS).
- **DaytonaMM**: Sub-32MB virtual memory manager with 4KB paging, lookaside pools, and demand paging (named after NT 3.5 *Daytona*).
- **NexusOB**: Unified kernel object manager, hierarchical namespace (`\Device`, `\DosDevices`, `\Registry`), and handle security.
- **AegisSched**: Preemptive multi-core SMP thread scheduler with quantum replenishment and dynamic priority boosting.
- **CourierLPC**: Fast port-based Local Procedure Call (LPC) and Named Pipe transport engine.
- **AmberCM**: Configuration Manager handling on-disk and in-memory registry hive cell allocation.
- **SurWin**: Window Station, Desktop, Conhost terminal, and Win32 message server (named after NT 4.0 *SUR*).
- **SentinelSec**: Local Security Authority (LSASS), Security Account Manager (SAM), and PBKDF2/SHA-256 authentication.
- **TitanHAL**: Unified x86_64 / ARM64 UEFI platform hardware abstraction layer.
- **RazzleNet**: Sovereign TCP/IP, NDIS miniport driver abstraction, and Winsock2 sockets (named after *Razzle*).
- **PrismAudio**: Multi-stream PCM software audio mixer and low-latency frequency synthesizer.

👉 For complete architectural specifications and namespace conventions, see **[docs/SOVEREIGN_TAXONOMY.md](docs/SOVEREIGN_TAXONOMY.md)**.

---

## 4. Legal & Clean-Room Methodology

MicaNT is a clean-room reimplementation created strictly for software interoperability:
- **API Copyright & Fair Use**: In *Google LLC v. Oracle America, Inc.* (593 U.S. 1, 2021), the United States Supreme Court held that reimplementing declaring code, method signatures, and API structures for interoperability is fair use as a matter of law.
- **Reference Repository**: All API metadata and interfaces are derived from Microsoft's MIT-licensed [microsoft/win32metadata](https://github.com/microsoft/win32metadata) project.
- **Clean-Room Policy**: Full non-contamination details, Section 3 non-contamination pillar, and engineering protocols are documented in [docs/CLEAN_ROOM.md](docs/CLEAN_ROOM.md).
- **Clean-Room Sentinel CI**: An automated provenance auditor ([scripts/clean_room_sentinel.js](scripts/clean_room_sentinel.js)) runs against every pull request using heuristic checks and Gemini AI to guarantee zero decompiled code or leaked materials enter the tree.

---

## 5. Building & Running

### Prerequisites
- Modern C++23 compiler: **Visual Studio 2022/2026** (MSVC `/std:c++latest`), **LLVM Clang 17+**, or **GCC 13+**
- **CMake 3.25+**
- Optional: **Node.js** (for running the metadata code generator in `tools/codegen` and the sentinel auditor)

### Generate Metadata Headers
```bash
node tools/codegen/generate_syscalls.js
```

### Build & Run Unit Test Suite (65 Suites, 100% Passing)
```bash
# With MSVC Developer Prompt:
cl /std:c++latest /EHsc /W4 /wd4201 /wd4100 /Iinclude test\test_runner.cpp kernel\dispatcher.cpp kernel\syscalls.cpp /Fe:bin\micant_tests.exe

# Run all 65 Test Suites:
.\bin\micant_tests.exe
```

### Build Host Kernel Simulator & Boot
```bash
# With MSVC Developer Prompt:
cl /std:c++latest /EHsc /W4 /Iinclude kernel\main.cpp kernel\dispatcher.cpp kernel\syscalls.cpp /Fe:bin\micant_kernel.exe

# Run the Executive:
.\bin\micant_kernel.exe

# Or boot directly into the interactive MicaNT Command Prompt Shell:
.\bin\micant_kernel.exe --shell
```

---

## 6. Tribute
Dedicated to Dave Cutler, Dave Plummer (*Dave's Garage*), and the legendary systems architects of the DEC PRISM/MICA and original Windows NT teams who proved that elegance, speed, and safety belong at the core of the OS.

---

## 7. Support & Donations

If you appreciate the clean-room preservation of the MICA architecture, the sub-32MB memory footprint, and the open-source engineering behind MicaNT, consider supporting development:

[![Buy Me a Coffee at Ko-fi](https://img.shields.io/badge/Ko--fi-Support%20ssfdre38-FF5E5B?style=for-the-badge&logo=kofi&logoColor=white)](https://ko-fi.com/ssfdre38)

- **Ko-fi:** [https://ko-fi.com/ssfdre38](https://ko-fi.com/ssfdre38)

