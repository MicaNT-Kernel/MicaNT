# Legal & Interoperability Compliance Framework

## 1. Statutory & Judicial Precedent

MicaNT is developed as a clean-room, open-source reimplementation of standard operating system application binary interfaces (ABIs) and software interfaces. Its legal foundation rests on authoritative statutory law and United States Supreme Court precedent regarding functional interoperability.

### A. Google LLC v. Oracle America, Inc. (2021)
In *Google LLC v. Oracle America, Inc.*, 593 U.S. 1 (2021), the Supreme Court of the United States ruled that reimplementing declaring code, method signatures, application programming interfaces (APIs), and functional software interfaces to achieve software interoperability constitutes **fair use** as a matter of law.

Key findings applicable to MicaNT:
- **Interoperability**: Reimplementing an established interface allows computer programs to interoperate and build upon existing developer knowledge and binary standards without copyright impediment.
- **Declaring Code vs. Implementing Code**: MicaNT contains **zero implementing code** copied from proprietary Microsoft Windows NT sources. All kernel implementation logic is authored clean-room in modern C++23.

### B. Sony Computer Entertainment, Inc. v. Connectix Corp. (2000) & Sega Enterprises Ltd. v. Accolade, Inc. (1992)
The Ninth Circuit established that clean-room reverse engineering and studying interfaces for the sole purpose of enabling functional compatibility with existing software and hardware is legally protected fair use.

---

## 2. Ingestion of Microsoft `win32metadata`

Microsoft Corporation officially created and released the [`microsoft/win32metadata`](https://github.com/microsoft/win32metadata) repository on GitHub.
- **License**: The repository is licensed under the **MIT License**.
- **Official Purpose**: Microsoft states that the repository exists to provide "a complete, machine-readable description of the Win32 API surface so that tools and languages can project the APIs accurately and idiomatically."
- **MicaNT Usage**: MicaNT's code-generation pipeline utilizes the metadata schemas provided in `win32metadata` to generate type definitions, NTSTATUS constants, and system call numbers.

---

## 3. Clean-Room Policy
To maintain strict compliance:
1. No contributor may use or reference leaked proprietary Microsoft source code (such as the Windows NT 4.0 / Windows 2000 source leaks).
2. All interface definitions must be derived from public documentation (MSDN / Microsoft Learn), official SDK headers, or the MIT-licensed `win32metadata` project.
3. All internal data structures, algorithms, schedulers, and memory managers must be original implementations.
