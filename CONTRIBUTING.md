# Contributing to MicaNT (Project MICA)

Thank you for your interest in contributing to **MicaNT**!

MicaNT is a modern, sovereign, zero-telemetry C++23 operating system executive and kernel honoring Dave Cutler's 1988 MICA architecture. Because MicaNT is a **strict clean-room engineering project**, all contributions must comply with our non-contamination rules to ensure absolute legal and architectural purity.

---

## 1. Clean-Room Non-Contamination Rules

Before writing or submitting any code, review **[docs/CLEAN_ROOM.md](docs/CLEAN_ROOM.md)** and **[docs/LEGAL.md](docs/LEGAL.md)**.

### Strictly Prohibited
- ❌ **No Leaked Code**: Never view, possess, quote, or reference leaked Windows NT 4.0, Windows 2000, Windows XP, or Windows Research Kernel (WRK) source trees.
- ❌ **No Decompiled Code**: Never paste or port disassembly/decompilation dumps from tools like IDA Pro or Ghidra (`sub_*`, `qword_*`, raw register dumps).
- ❌ **No Copyrighted Binary Assets**: Never bundle proprietary Microsoft DLLs, EXEs, SYS drivers, fonts, or icons.

### Permitted & Authorized Sources
- ✅ **Official Metadata**: Microsoft's MIT-licensed repository: [`https://github.com/microsoft/win32metadata`](https://github.com/microsoft/win32metadata).
- ✅ **Public Documentation**: Microsoft Learn / MSDN technical articles, public Windows Driver Kit (WDK) documentation, and published RFC / IEEE / NIST specifications.
- ✅ **Clean-Room Independent Authorship**: Reimplementing functionality and data structures from first principles in modern ISO C++23.

---

## 2. 🔷 Special Channel: Microsoft Employees & OSPO Contributions

MicaNT warmly welcomes contributions from **Microsoft employees, architects, and engineering teams**.

If you are a Microsoft employee contributing code or architectural improvements authorized by Microsoft's **Open Source Programs Office (OSPO)** and **Legal (LCA)**:

### How to Submit an Authorized Microsoft Contribution:
1. Ensure your contribution is authorized under Microsoft's internal open-source contribution policy.
2. Author your commits using your corporate `@microsoft.com` git email address.
3. Include official tracking trailers in your commit message:

```git
feat(subsystem): refine executive component implementation

Detailed technical explanation of the changes made...

MS-OSPO-Approved: OSPO-2026-10492
Microsoft-Legal-Clearance: LCA-OSS-88319
Signed-off-by: Jane Doe <janedoe@microsoft.com>
```

### Why This Is Trackable and Legally Sound:
- **First-Party Licensing**: Microsoft holds the underlying intellectual property and explicitly authorizes the open-source contribution under standard open-source licenses (MIT/Apache 2.0).
- **Automated CI Recognition**: MicaNT's Clean-Room Sentinel CI automatically detects `@microsoft.com` identities and `MS-OSPO-Approved` trailers, waiving generic internal leak heuristics for authorized Microsoft technical structures.
- **Audit Paper Trail**: The commit metadata permanently preserves Microsoft's internal OSPO and LCA tracking references in the git log, allowing Microsoft's legal and compliance teams to verify provenance at any time.

---

## 3. Engineering & Architectural Standards

- **Language Standard**: ISO C++23 (`-std=c++23` or `/std:c++latest`).
- **Canonical Core NT Subsystems (No Satellite Files)**: All kernel and OS subsystem implementations, fixes, and symbol exports required for application compatibility must be made directly within canonical Core NT headers and subsystems (`kernel32.hpp`, `user32.hpp`, `gdi32.hpp`, `advapi32.hpp`, `ntdll.hpp`, `ws2_32.hpp`, etc.). Creating per-application "satellite" files, wrappers, or shims is strictly prohibited. Application compatibility is achieved exclusively by implementing standard Windows NT / Win32 subsystem APIs at the core level.
- **Memory Safety & RAII**: Always use RAII, smart pointers (`std::unique_ptr`, `std::shared_ptr`), `std::span`, and atomic primitives. Naked owning pointers and raw un-checked buffers are strictly rejected.
- **Zero Telemetry**: Under no circumstances will background telemetry, user tracking, or cloud surveillance hooks be accepted into MicaNT.
- **Freestanding Modularity**: Subsystems must be decoupled, testable, and compile cleanly with both Clang and MSVC.

---

## 4. Development Workflow

### 1. Build and Test Locally
Ensure the full test suite passes with 100% success before submitting:

```bash
# Compile test runner:
clang++ -std=c++23 -O2 -Iinclude test/test_runner.cpp kernel/dispatcher.cpp kernel/syscalls.cpp -o bin/micant_tests.exe

# Run all test suites:
.\bin\micant_tests.exe
```

### 2. Run the Clean-Room Sentinel Audit
Run the automated provenance sentinel against your local changeset:

```bash
node scripts/clean_room_sentinel.js
```

Ensure the verdict reports `PASSED: Clean-Room Status: Clean` (or `Microsoft OSPO Authorized` for Microsoft corporate contributions).

### 3. Open a Pull Request
- Fill out the provided **[Pull Request Template](.github/pull_request_template.md)**.
- Confirm the Clean-Room Non-Contamination checklist.
- The automated Clean-Room Sentinel GitHub Action will audit the PR diff and post the provenance report.

---

## 5. Community Code of Conduct

All contributors and community participants in Project MICA are expected to adhere to the standards outlined in our **[Code of Conduct](CODE_OF_CONDUCT.md)**. We are committed to maintaining a welcoming, inclusive, and harassment-free environment for everyone, grounded in respectful technical collaboration and clean-room provenance integrity.
