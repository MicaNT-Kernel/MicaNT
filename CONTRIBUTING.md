# Contributing to MicaNT (Project MICA)

Thank you for your interest in contributing to **MicaNT**!

MicaNT is a C++23 operating system kernel and executive. Contributions should follow the research, reuse, rewrite, and integration approach in [docs/CLEAN_ROOM.md](docs/CLEAN_ROOM.md), while preserving MicaNT's single modular-kernel architecture and Windows compatibility contracts.

---

## 1. Research and implementation references

Before changing a subsystem, inspect its current implementation, applicable Microsoft contract, and relevant public references.

Primary contract references:

- Microsoft's [`win32metadata`](https://github.com/microsoft/win32metadata), Microsoft Learn, and public WDK documentation.
- Relevant open-source implementation and test references, including ReactOS for NT kernel behavior and Wine for user-mode behavior.
- Other public technical projects where their subject matter is relevant.

Apply appropriate usage terms when incorporating external code, retain required notices, and do not use leaked/confidential source, decompiled proprietary implementation, or proprietary binary assets. See [docs/CLEAN_ROOM.md](docs/CLEAN_ROOM.md). No particular external implementation or architecture is mandatory; preserve valid MicaNT code and make focused, testable changes in the canonical subsystem.

---

## 3. Engineering & Architectural Standards

- **Language Standard**: ISO C++23 (`-std=c++23` or `/std:c++latest`).
- **Canonical Windows NT Architecture Layout**: To strictly match standard Microsoft Windows NT SDK/WDK design patterns, headers and implementations are organized canonically into:
  - `include/micant/shared/`: Mode-agnostic types, constants, and status codes (`ntdef.hpp`, `ntstatus.hpp`, `subsystems.hpp`).
  - `include/micant/km/`: Kernel-Mode (Ring 0) components:
    - `km/hal/`: Hardware Abstraction Layer (CPU, APIC, PIC, PIT, RTC, PCI, ACPI, UEFI, bootvid).
    - `km/core/`: Microkernel Core (`ke`, `trap`, `dispatcher`, `timer`, `sync`, `syscalls`).
    - `km/executive/`: NT Executive Managers (`ob`, `mm`, `ps`, `io`, `se`, `cm`, `lpc`, `po`, `ex`, `heap`, `section`, `etw`, `driver`, `wdf`).
    - `km/drivers/`: Storage (`storage`, `nvme`), File Systems (`fs`, `fat32`, `ntfs`, `refs`, `npfs`), Network (`ndis`, `tcpip`, `rdma`, `wdi`), Security (`ci`, `sysguard`, `vsm`, `pluton`), and Bus/Hardware accelerators.
  - `include/micant/um/`: User-Mode (Ring 3) components:
    - `um/native/`: Native NT API & Image Loaders (`ntdll.hpp`, `pe.hpp`, `ldr.hpp`, `janusldr.hpp`, `wow64.hpp`).
    - `um/servers/`: Subsystem Servers (`csrss.hpp`, `conhost.hpp`, `smss.hpp`, `lsass.hpp`, `scm.hpp`, `winlogon.hpp`, services).
    - `um/subsystems/`: Win32 & Extended Client DLLs (`kernel32.hpp`, `user32.hpp`, `gdi32.hpp`, `advapi32.hpp`, `ws2_32.hpp`, `shell32.hpp`, `ole32.hpp`, `gdiplus.hpp`, etc.).
  - `include/micant/ucrt/`: Universal C Runtime library emulation (`msvcrt.hpp`, `icuuc.hpp`, `daytonamm.hpp`).
  - Root `include/micant/*.hpp` headers act as transparent backward-compatibility forwarders so legacy includes never break.
- **Canonical Core NT Subsystems (No Satellite Files)**: All kernel and OS subsystem implementations, fixes, and symbol exports required for application compatibility must be made directly within canonical Core NT headers and subsystems (`kernel32.hpp`, `user32.hpp`, `gdi32.hpp`, `advapi32.hpp`, `ntdll.hpp`, `ws2_32.hpp`, etc.). Creating per-application "satellite" files, wrappers, or shims is strictly prohibited. Application compatibility is achieved exclusively by implementing standard Windows NT / Win32 subsystem APIs at the core level.
- **Memory Safety & RAII**: Always use RAII, smart pointers (`std::unique_ptr`, `std::shared_ptr`), `std::span`, and atomic primitives. Naked owning pointers and raw un-checked buffers are strictly rejected.
- **Zero Telemetry**: Under no circumstances will background telemetry, user tracking, or cloud surveillance hooks be accepted into MicaNT.
- **Freestanding Modularity**: Subsystems must be decoupled, testable, and compile cleanly with both Clang and MSVC.

---

## 4. Development Workflow

### 1. Build and Test Locally
Run the relevant existing tests and report their exact results. Host tests do
not replace freestanding build or QEMU validation when the change affects
those paths:

```bash
# Configure and compile the host test runner:
cmake -S . -B build -G Ninja
cmake --build build --target micant_tests

# Run all test suites:
.\build\host\micant_tests.exe
```

### 2. Run the repository checks
Run the checks relevant to the change. The sentinel is a repository-specific static check, not proof of compatibility or provenance:

```bash
node scripts/clean_room_sentinel.js
```

Review the command output and report what was run and its result.

### 3. Open a Pull Request
- Fill out the provided **[Pull Request Template](.github/pull_request_template.md)**.
- Include the affected subsystem, implementation behavior, and relevant validation evidence.
- The automated Clean-Room Sentinel GitHub Action may run its configured checks; it does not replace contract review or tests.

---

## 5. Community Code of Conduct

All contributors and community participants in Project MICA are expected to adhere to the standards outlined in our **[Code of Conduct](CODE_OF_CONDUCT.md)**. We are committed to maintaining a welcoming, inclusive, and harassment-free environment for everyone, grounded in respectful technical collaboration.
