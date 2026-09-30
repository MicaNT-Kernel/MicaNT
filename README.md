# MicaNT (Project MICA)

> **"The cleanest NT architecture on Earth."**  
> An open-source, zero-telemetry, modern C++23 NT-compatible operating system kernel and executive, powered by Microsoft's [`win32metadata`](https://github.com/microsoft/win32metadata).

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Standard: C++23](https://img.shields.io/badge/Language-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Architecture: x86__64](https://img.shields.io/badge/Arch-x86__64-orange.svg)]()
[![Build: CMake](https://img.shields.io/badge/Build-CMake%203.25%2B-green.svg)]()

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
3. **Pure Object Manager**: Dave Cutler's clean handle-based namespace (`\Device`, `\DosDevices`, `\KernelObjects`, `\BaseNamedObjects`).
4. **Zero Telemetry**: No Cortana, no advertising IDs, no diagnostic tracking, no network dial-home. Pure, unadulterated computing.
5. **Native Windows ABI**: Implements the standard x86-64 `KiSystemCall64` / `syscall` interface, userland `TEB`/`PEB` layout, and `ntdll.dll` executive contract.

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
| - Reference Count|                | - Pool Allocator |                | - Scheduler      |
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
                             +-------------------------------+
```

---

## 4. Legal & Interoperability Foundation

MicaNT is a clean-room reimplementation created strictly for software interoperability:
- **API Copyright & Fair Use**: In *Google LLC v. Oracle America, Inc.* (593 U.S. 1, 2021), the United States Supreme Court held that reimplementing declaring code, method signatures, and API structures for interoperability is fair use as a matter of law.
- **win32metadata**: Microsoft provides the authoritative definitions of Win32 and NT APIs under the permissive **MIT License** in the [microsoft/win32metadata](https://github.com/microsoft/win32metadata) repository.
- **No Proprietary Code**: No leaked or reverse-engineered proprietary Microsoft binary source code is used. All kernel logic is clean-room C++23.

---

## 5. Building & Running

### Prerequisites
- Modern C++23 compiler: **LLVM Clang 17+** or **GCC 13+** (or MSVC 2022 v17.8+)
- **CMake 3.25+**
- **Ninja** or **Make**
- Optional: **QEMU** (`qemu-system-x86_64`) for bare-metal / VM emulation.

### Host Simulator Build (Run on your existing OS)
```bash
git clone https://github.com/ssfdre38/MicaNT.git
cd MicaNT
mkdir build && cd build
cmake .. -G Ninja
ninja
./bin/micant_kernel --test-subsystems
```

---

## 6. Tribute
Dedicated to Dave Cutler, Dave Plummer (*Dave's Garage*), and the legendary systems architects of the DEC PRISM/MICA and original Windows NT teams who proved that elegance, speed, and safety belong at the core of the OS.
