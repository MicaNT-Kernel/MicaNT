# MicaNT Sovereign Operating System
## FOSS Applications Master Compatibility Matrix & Validation Roadmap

This document establishes the official **Free and Open-Source Software (FOSS) Compatibility Matrix** for the MicaNT Sovereign Operating System. 

To establish MicaNT as the ultimate, zero-telemetry, clean-room alternative to proprietary Microsoft Windows, MicaNT executes **100% unmodified, retail-signed binary releases** of major FOSS desktop software. By ensuring flawless compatibility with these industry-standard open-source applications, MicaNT guarantees that users, software engineers, creatives, enterprise staff, and power users can immediately transition without workflow interruption.

---

## 1. Office Suites, Document Production & Enterprise Productivity

Office software represents the cornerstone of desktop operating systems. Office suites exercise complex COM/OLE automation, rich typography engines (DirectWrite/HarfBuzz), print spoolers, structured storage compound files (`IStorage`), and multi-threaded calculation grids.

| Application | GUI / Runtime Architecture | Core Windows APIs & Kernel Subsystems Stressed | MicaNT Strategic Importance | Status |
| :--- | :--- | :--- | :--- | :--- |
| **LibreOffice**<br>*(Writer, Calc, Impress, Draw, Math, Base)* | C++ / VCL (Visual Class Library), Skia GPU backend, UNO IPC | `GDI32`, `DWrite.dll`, `ole32.dll` (`IStorage`, `IStream`, `IDataObject`), `winspool.drv` (Print Spooler), `advapi32.dll` (Registry configuration), MSVCRT multithreading | **The Premier FOSS Office Suite.** Replaces Microsoft Office 365 natively. Validates complex compound documents (`.docx`, `.xlsx`, `.pptx`), OpenDocument standard (ODF), and macro execution. | **Priority Tier 1** |
| **ONLYOFFICE Desktop Editors** | C++ core with Chromium Embedded Framework (CEF) / Qt tabbed shell | Multi-process Chromium sandbox, shared memory IPC (`CreateFileMappingW`), DirectComposition, high-DPI scaling, clipboard OLE transfers | **Modern Cloud/Desktop Office.** Validates multi-process document tab isolation, modern UI hardware acceleration, and seamless MS Office file fidelity. | **Priority Tier 2** |
| **Scribus** | C++ / Qt6 | CMYK color management (`mscms.dll` / Windows Color System), GDI+ Bézier curves, PostScript/PDF export streams, font metric caches | **Professional Desktop Publishing.** Replaces Adobe InDesign and Microsoft Publisher for magazines, brochures, and print layouts. | **Priority Tier 2** |
| **SumatraPDF** | Pure Win32 C++ / MuPDF Engine | Direct GDI blitting (`BitBlt`, `StretchDIBits`), Win32 scrollbars, memory-mapped PDF parsing (`CreateFileMappingW`), minimal footprint | **Ultra-Fast Document Viewer.** Replaces Adobe Acrobat Reader with zero lag, instant startup, and zero background services. | **Ready for Audit** |
| **TeXstudio / MiKTeX** | C++ / Qt + CLI TeX toolchain | Process spawning (`CreateProcessW`), standard I/O pipes (`CreatePipe`), temporary file spooling, high-precision mathematical typography | **Academic & Scientific Publishing.** Validates heavy CLI-to-GUI pipelining and complex LaTeX compilation toolchains. | **Priority Tier 2** |
| **AbiWord & Gnumeric** | C / GTK+ for Windows | Cairo 2D rendering, Gnumeric fast calculation engine, font substitution tables, clipboard data interchange | **Ultra-Lightweight Office.** Sub-20MB memory footprint office suite for low-resource environments and legacy system emulation. | **Priority Tier 3** |
| **Mozilla Thunderbird** | C++ / Rust / Gecko Engine | Winsock 2.0 (`ws2_32.dll`), TLS encryption, Windows MAPI integration, multi-pane window layout, local SQLite profile storage | **Universal Enterprise Email & Calendar.** Replaces Microsoft Outlook. Handles IMAP, POP3, SMTP, and CalDAV calendaring. | **Priority Tier 1** |
| **GnuCash** | C / Scheme / GTK+ | Double-entry ledger calculations, SQL database backend (SQLite/PostgreSQL client), report printing, high-precision decimal math | **Small Business Accounting & Personal Finance.** Validates financial accounting, transactional integrity, and data reporting. | **Priority Tier 3** |
| **Joplin / Logseq** | Node.js / Electron / Local SQLite | Chromium sandbox, Job Objects, SQLite direct file locking, local Markdown parsing, filesystem watchers (`ReadDirectoryChangesW`) | **Local-First Knowledge Base.** Personal knowledge management, encrypted note-taking, and documentation journaling. | **Priority Tier 2** |
| **Xournal++** | C++ / GTK3 | Windows Pointer / Pen Tablet API (`WinTab` / Windows Ink), vector stroke blitting, PDF background annotation, undo/redo stacks | **Digital Ink & Handwriting.** Replaces Microsoft OneNote for stylus note-taking, sketching, and grading PDF submissions. | **Priority Tier 2** |

---

## 2. Core Desktop Utilities & Shell Tools

Utilities that power-users and administrators rely upon every minute of the day.

| Application | GUI / Runtime Architecture | Core Windows APIs & Kernel Subsystems Stressed | MicaNT Strategic Importance | Status |
| :--- | :--- | :--- | :--- | :--- |
| **7-Zip** | Pure C/C++ Win32 | `KERNEL32.dll`, `USER32.dll`, `OLEAUT32.dll`, multithreaded LZMA/AES, memory streams | **Universal Archiver & File Manager.** Demonstrates pristine Win32 baseline satisfaction. | **100% VALIDATED (Milestone 214)** |
| **Notepad++** | C++ / Win32 / Scintilla | Custom Win32 message pump, Scintilla editor control, syntax coloring, session persistence | **Developer Editor.** Proves interactive editing, multi-document tab handling, and Win32 controls. | **100% VALIDATED (Milestone 214)** |
| **WizTree 4.x / WinDirStat** | Modern C++ / Win32 | Direct NTFS `$MFT` record traversal (`FSCTL_GET_NTFS_FILE_RECORD`), `FSCTL_ENUM_USN_DATA`, Volume Handles | **Instant Disk Analyzer.** Confirms native NTFS direct file record querying and USN enumerations. | **100% VALIDATED (Milestone 216)** |
| **Everything** (voidtools) | Ultra-compact Win32 C | NTFS USN Change Journal reader, multi-threaded query engine, Win32 system tray, global hotkeys | **Instant File Search.** Proves NTFS journal change notification speed and system-wide indexing. | **Ready for Audit** |
| **WinMerge** | C++ / MFC / Win32 | Visual diff algorithms, split-window scrolling, RichEdit controls, Unicode directory comparisons | **Visual File & Folder Diff.** Essential for developers and sysadmins reconciling configuration files. | **Priority Tier 1** |
| **KeePassXC** | C++ / Qt6 | `bcrypt.dll` / CNG cryptography, DPAPI secure memory allocation, clipboard timer auto-clear | **Sovereign Password Manager.** Validates cryptographically secure desktop vaults and memory sanitization. | **Ready for Audit** |
| **BleachBit** | Python / C / GTK+ | Filesystem cleaning, registry deep scanning, secure file shredding, cache purging | **Privacy & System Hygiene.** Ensures system temporary storage, logs, and caches are sovereignly purgable. | **Priority Tier 3** |

---

## 3. System Diagnostics, Low-Level Hardware & Administration

The ultimate stress-test for native NT kernel fidelity: handle enumeration, security tokens, memory maps, raw disk access, and WMI hardware telemetry.

| Application | GUI / Runtime Architecture | Core Windows APIs & Kernel Subsystems Stressed | MicaNT Strategic Importance | Status |
| :--- | :--- | :--- | :--- | :--- |
| **System Informer** *(Process Hacker 3)* | Pure C / Native NT API | `NtQuerySystemInformation`, `NtOpenProcess`, `NtQueryInformationToken`, handle inspection, PEB/TEB walk | **The Quintessential NT Kernel Audit.** Proves full compliance of the undocumented native NT kernel API. | **Priority Tier 1** |
| **Rufus** | Pure C / Win32 | Direct raw block storage (`\\.\PhysicalDriveX`), SCSI/ATAPI passthrough, VDS (Virtual Disk Service), partition formatting | **Bootable Media & Drive Formatter.** Proves low-level block I/O, partition table creation, and raw disk access. | **Priority Tier 1** |
| **LibreHardwareMonitor** | C# / .NET / Native C driver helper | ACPI WMI tables (`root\wmi`), Ring-0 SMBus reading, CPU MSRs, GPU telemetry | **Hardware Sensor Suite.** Validates WMI query engine, device driver interface, and thermal reporting. | **Priority Tier 2** |
| **ImHex** | C++20 / ImGui | Massive 64-bit memory-mapped files (`CreateFileMappingW`), pattern engines, reverse-engineering disassembler | **Advanced Binary & Hex Inspector.** Proves huge virtual memory allocations and 64-bit pointer arithmetic. | **Priority Tier 2** |
| **PuTTY / KiTTY** | Pure C Win32 | Winsock 2.0 asynchronous TCP sockets, non-blocking I/O, COM serial UART drivers | **Terminal & Remote Administration.** Proves rock-solid remote network console sessions and local serial ports. | **Ready for Audit** |

---

## 4. Multimedia, Audio Workstations & Creative Production

Proves high-throughput audio/video streaming, low-latency DSP pipelines, and modern hardware-accelerated rendering.

| Application | GUI / Runtime Architecture | Core Windows APIs & Kernel Subsystems Stressed | MicaNT Strategic Importance | Status |
| :--- | :--- | :--- | :--- | :--- |
| **VLC Media Player** | C/C++ / Qt / Direct3D 12 | Hardware video decoding (D3D12VA), WASAPI audio routing, high-resolution multimedia timers | **Universal Media Player.** Validates D3D12 hardware acceleration, audio streams, and codec decoding. | **100% VALIDATED (Milestone 214)** |
| **OBS Studio** | C/C++ / Qt6 | DXGI desktop duplication (`IDXGIOutputDuplication`), WASAPI loopback capture, D3D11 rendering, multi-threaded encoder | **Broadcast & Screen Recording.** The gold standard for game/screen recording and live streaming. | **Priority Tier 1** |
| **Audacity / Tenacity** | C++ / wxWidgets | Real-time WASAPI/DirectSound multi-channel audio buffers, DSP waveform rendering, high-priority audio threads | **Multitrack Audio Editor.** Replaces Adobe Audition. Validates low-latency audio capture and editing. | **Priority Tier 1** |
| **GIMP** | C / GTK+ | Multi-layer raster compositing, color management (ICC/ICM), tile-based image caches, plug-in IPC | **Professional Image Editor.** Replaces Adobe Photoshop. Proves heavy graphics image processing. | **Priority Tier 2** |
| **Krita** | C++ / Qt5/6 | OpenGL/Direct3D canvas blitting, high-resolution pressure-sensitive brush engines, HDR color space | **Digital Painting & Concept Art.** Validates responsive canvas rendering and pen tablet input. | **Priority Tier 2** |
| **Inkscape** | C++ / GTKMM | SVG XML parsing, Cairo vector rasterization, gradient meshes, complex font layout | **Vector Illustration & Graphic Design.** Replaces Adobe Illustrator. Validates scalable vector rendering. | **Priority Tier 2** |
| **Blender** | C/C++ / Python / OpenGL / Vulkan | Modern 3D graphics pipeline, compute shaders, multithreaded cycles raytracing, high-precision timers | **Industry-Standard 3D Suite.** Validates 3D modeling, sculpting, rigging, and hardware render performance. | **Priority Tier 1** |
| **HandBrake** | C# / C / FFmpeg backend | Multi-threaded CPU/GPU video encoding (NVENC/AMF/QSV/SVT-AV1), media container muxing | **Universal Video Transcoder.** Validates sustained 100% CPU/GPU multi-core compute workloads. | **Priority Tier 2** |

---

## 5. Web Browsing, Internet & Network Analysis

The modern operating system lives on the web. These applications validate multi-process sandboxing, packet capture, and high-throughput network transfers.

| Application | GUI / Runtime Architecture | Core Windows APIs & Kernel Subsystems Stressed | MicaNT Strategic Importance | Status |
| :--- | :--- | :--- | :--- | :--- |
| **Supermium** *(Chromium for Custom NT)* | C++ / Skia / V8 | Windows Job Objects, Restricted Security Tokens, IPC Named Pipes, Shared Memory, DirectWrite | **Modern Web Browsing.** Chromium engine optimized for custom NT kernels. Proves full multi-process sandboxing. | **Priority Tier 1** |
| **LibreWolf / Floorp** | C++ / Rust / Gecko | Multi-process Gecko layout engine, WebRender GPU pipeline, network TLS socket pools | **Privacy-Focused Web Browsing.** Validates the Mozilla Gecko engine, WebAssembly runtime, and tabs. | **Priority Tier 1** |
| **qBittorrent** | C++ / Qt6 / libtorrent | High-concurrency Winsock TCP/UDP sockets, sparse file pre-allocation, disk cache I/O | **P2P File Transfer.** Proves high-bandwidth network throughput and sustained disk write streams. | **Ready for Audit** |
| **FileZilla Client / WinSCP** | C++ / wxWidgets / Win32 | FTP, SFTP, WebDAV protocols, drag-and-drop OLE/COM integration, shell folder trees | **Secure Remote File Management.** Standard enterprise file transfer and directory synchronization. | **Ready for Audit** |
| **Wireshark** | C/C++ / Qt | Raw network packet capture, Npcap kernel driver interface, protocol packet dissection | **Network Protocol Analyzer.** Proves low-level packet capture and protocol diagnostic reliability. | **Priority Tier 2** |

---

## 6. Software Development, Engineering & Virtualization

Empowers MicaNT to be self-hosting and capable of building, testing, and debugging software natively.

| Application | GUI / Runtime Architecture | Core Windows APIs & Kernel Subsystems Stressed | MicaNT Strategic Importance | Status |
| :--- | :--- | :--- | :--- | :--- |
| **VSCodium** | TypeScript / Node.js / Electron | Chromium IPC, Node.js asynchronous I/O, language server protocol (LSP) subprocesses | **Extensible Code Editor.** Validates full Electron desktop stack and developer extensions. | **Priority Tier 1** |
| **Git for Windows** | C / MSYS2 / MinGW | POSIX-on-NT fork/exec emulation, console stdio redirection, NTFS symbolic links and reparse points | **Source Control Standard.** Critical for pulling repositories, managing branches, and CI/CD. | **Priority Tier 1** |
| **Godot Engine** | C++20 / Vulkan / OpenGL | Win32 raw input, XInput gamepads, Vulkan swapchains, high-frequency audio playback | **Universal Game Engine.** Proves modern 2D/3D game rendering, physics execution, and asset pipelines. | **Priority Tier 2** |
| **DOSBox-Staging** | C++ / SDL2 | CPU instruction emulation, real-time timer interrupts (`NtDelayExecution`), Sound Blaster audio | **Legacy Application Emulation.** Runs historical 16-bit / 32-bit DOS business and gaming applications. | **Ready for Audit** |
| **QEMU for Windows** | C / Glib / Win32 | Hypervisor hypercalls, virtual storage devices, hardware virtualization, tap network bridges | **Nested Virtualization.** Enables MicaNT to run nested guest operating systems and test kernels. | **Priority Tier 2** |

---

## 7. Execution Strategy & Verification Milestones

1. **Clean-Room Integrity**: Every application binary remains 100% untouched and unmodified from upstream release packages.
2. **Deterministic PE Auditing**: For each target application, the MicaNT toolchain parses its PE Import Address Table (IAT) across all dependent DLLs and logs exact symbol coverage.
3. **Automated CI Regression**: Missing API stubs and subsystem primitives are implemented in clean-room ISO C++23, verified via automated test suites in the CI battery.
4. **Interactive Screendump Verification**: Successful launch and interaction are verified on bare-metal UEFI GOP framebuffer under live QEMU emulation.
