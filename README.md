# MicaNT (Project MICA)

> **"The cleanest NT architecture on Earth."**  
> An open-source, zero-telemetry, modern C++23 NT-compatible operating system kernel and executive, built as a **strict clean-room implementation** using Microsoft's official [`win32metadata`](https://github.com/microsoft/win32metadata) repository for interface reference.

[![Website: micant.barrersoftware.com](https://img.shields.io/badge/Website-micant.barrersoftware.com-4CAF50.svg?logo=googlechrome&logoColor=white)](https://micant.barrersoftware.com)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Clean Room: Certified](https://img.shields.io/badge/Clean%20Room-Certified-success.svg)](docs/CLEAN_ROOM.md)
[![Contributing: Guide](https://img.shields.io/badge/Contributing-Guide-blue.svg)](CONTRIBUTING.md)
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
> In accordance with the U.S. Supreme Court precedent in *Google LLC v. Oracle America, Inc.* (2021), reimplementing functional declaring code and interface signatures for binary interoperability is protected fair use. No proprietary or leaked Microsoft source code was used or referenced. See [docs/CLEAN_ROOM.md](docs/CLEAN_ROOM.md), [docs/LEGAL.md](docs/LEGAL.md), [CONTRIBUTING.md](CONTRIBUTING.md), and our [Open Statement to Microsoft Corporation](docs/LEGAL.md#8-an-open-statement--message-to-microsoft-corporation) for full compliance documentation and partnership intent.

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

MicaNT is engineered around foundational architectural tenets that deliver complete 64-bit Windows NT application compatibility without the historical technical debt, telemetry probes, or multi-gigabyte footprint of legacy systems:

1. **Modern C++23 Core & Zero Legacy Debt**: Built with RAII, concepts, compile-time type safety, and atomic synchronization. No naked pointers or raw un-checked buffers.
2. **Metadata-Driven Syscall Surface**: The entire `Zw*` / `Nt*` system call table, types, and NTSTATUS codes are auto-generated from Microsoft's MIT-licensed [`win32metadata`](https://github.com/microsoft/win32metadata) repository.
3. **Pure Object Manager (`ob/`)**: Dave Cutler's clean handle-based hierarchical namespace (`\Device`, `\DosDevices`, `\KernelObjects`, `\BaseNamedObjects`, `\Registry`).
4. **Complete Ring 0 Kernel Architecture**: Preemptive 32-queue priority scheduler (`ke/`), non-paged and paged memory pools with 4-byte diagnostic tagging (`ex/`), demand paging and structured exception handling (`ke/trap.hpp`), and multi-core SMP HAL topology (`hal/`).
5. **Subsystem Trinity**:
   - **Configuration Manager (`cm/`)**: In-memory registry hive mounted under `\Registry` with typed values (`REG_SZ`, `REG_DWORD`, `REG_QWORD`).
   - **Security Reference Monitor (`se/`)**: SIDs, DACLs, ACEs, Process Access Tokens, and Dave Cutler's `accessCheck` validation algorithm.
   - **Advanced Local Procedure Call (`lpc/`)**: High-speed message passing with named ports in `\RPC Control`, FIFO queues, and synchronous `requestWaitReply` rendezvous.
6. **Native 64-bit Windows PE Execution**: Direct execution of standard 64-bit Windows console binaries (`x86-64` / `ARM64`) with PE `.idata`/`.reloc` binding, userland `TEB`/`PEB` layout, and full `ntdll.dll` / `kernel32.dll` runtime bridges.
7. **Complete Zero Telemetry**: Pure sovereign computing with no telemetry probes, no advertising IDs, no diagnostic tracking, and no network dial-home.

> 📖 **Complete Subsystem Reference**: For an exhaustive, item-by-item technical specification of all subsystems, driver models, graphics pipelines, and userland services, see **[docs/KEY_PILLARS.md](docs/KEY_PILLARS.md)**.

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
- **Trademark Policy & Nominative Fair Use**: All third-party trademarks (e.g., Windows, BitLocker, WDAC) are used purely for nominative compatibility identification. MicaNT is an independent project and is neither affiliated with nor endorsed by Microsoft Corporation. See [docs/LEGAL.md](docs/LEGAL.md).
- **Reference Repository**: All API metadata and interfaces are derived from Microsoft's MIT-licensed [microsoft/win32metadata](https://github.com/microsoft/win32metadata) project.
- **Clean-Room Policy**: Full non-contamination details, Section 3 non-contamination pillar, and engineering protocols are documented in [docs/CLEAN_ROOM.md](docs/CLEAN_ROOM.md).
- **Clean-Room Sentinel CI**: An automated provenance auditor ([scripts/clean_room_sentinel.js](scripts/clean_room_sentinel.js)) runs against every pull request using heuristic checks and Gemini AI to guarantee zero decompiled code or leaked materials enter the tree.
- **Open Statement to Microsoft**: We have published a formal, open statement in [docs/LEGAL.md § 8](docs/LEGAL.md#8-an-open-statement--message-to-microsoft-corporation) stating our bona fides, research mission, and inviting open, cooperative dialogue with Microsoft's OSPO and legal teams.

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

### Build & Run Unit Test Suite (150 Suites, 100% Passing)
```bash
# With MSVC Developer Prompt:
cl /std:c++latest /EHsc /W4 /wd4201 /wd4100 /Iinclude test\test_runner.cpp kernel\dispatcher.cpp kernel\syscalls.cpp /Fe:bin\micant_tests.exe

# Run all 150 Test Suites:
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

---

## 8. Community & Contributing

We welcome community participation, technical discussions, and contributions!
- **[CONTRIBUTING.md](CONTRIBUTING.md)**: Clean-room engineering rules, developer workflow, and Microsoft OSPO authorized contribution channel.
- **[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)**: Contributor Covenant v2.1 standards for a welcoming, respectful, and harassment-free community.
- **[Security Policy](.github/SECURITY.md)**: Responsible disclosure and vulnerability reporting process.


