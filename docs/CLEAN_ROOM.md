# Clean-Room Engineering Certification & Methodology

## 1. Clean-Room Declaration

**MicaNT** is an independent, clean-room implementation of standard operating system functional interfaces. 

It is designed, structured, and implemented from first principles in **Modern ISO C++23** without the use, possession, or reference of proprietary, leaked, or confidential Microsoft source code.

---

## 2. Authorized Reference: `microsoft/win32metadata`

The interface definitions, system service signatures, data structures, and status codes used in MicaNT are derived and projected exclusively from public, authorized sources:

1. **Primary Reference**: Microsoft Corporation's official, MIT-licensed open-source repository:  
   [`https://github.com/microsoft/win32metadata`](https://github.com/microsoft/win32metadata)
   - Microsoft explicitly published this repository to provide "a complete, machine-readable description of the Win32 API surface so that tools and languages can project the APIs accurately and idiomatically."
   - MicaNT's code-generation pipeline ([`tools/codegen/generate_syscalls.js`](file:///C:/Users/admin/source/MicaNT/tools/codegen/generate_syscalls.js)) directly parses this open metadata to generate typed C++23 interface catalogs.
2. **Public Documentation**: Publicly available Microsoft Learn / MSDN technical articles and public Windows Driver Kit (WDK) specifications describing the external contract of the Windows executive.

---

## 3. Strict Non-Contamination Guardrails

To preserve absolute legal purity:

- **Strictly Prohibited**:
  - Leaked Microsoft Windows NT 4.0 / Windows 2000 / Windows XP / Windows Research Kernel (WRK) source dumps.
  - Disassembly or decompilation of proprietary Microsoft binaries (`ntoskrnl.exe`, `ntdll.dll`) for the purpose of copying algorithms.
  - Proprietary internal symbols or confidential architectural memos.

- **Mandatory Requirements**:
  - All internal kernel implementations (schedulers, slab allocators, page-table managers, handle tables, and synchronization primitives) are original clean-room code written in C++23.
  - All functional interfaces follow standard US legal precedents (*Google LLC v. Oracle America, Inc.*, 593 U.S. 1, 2021) protecting functional interoperability and declaring code fair use.
