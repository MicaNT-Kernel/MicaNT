# Building MicaNT: The Complete Architectural & Developer Guide

> **"The cleanest NT architecture on Earth."**  
> This guide details how to compile, verify, and run MicaNT—both as an ultra-fast host simulation testbed and as an authentic, freestanding, modular Windows NT operating system disk layout.

---

## 1. Dual Build Architectures

MicaNT provides two distinct, complementary build models to satisfy both kernel developers and operating system deployers:

```
                            MicaNT Build Pipeline
                                     │
           ┌─────────────────────────┴─────────────────────────┐
           ▼                                                   ▼
┌──────────────────────────────────────┐   ┌──────────────────────────────────────┐
│  Mode 1: Host Simulation & Harness   │   │  Mode 2: Authentic Modular Sysroot   │
├──────────────────────────────────────┤   ├──────────────────────────────────────┤
│ • Fast iterative development         │   │ • True freestanding PE32+ binaries   │
│ • All 516 test suites in ~5 seconds  │   │ • Ring 0 ntoskrnl.exe                │
│ • 100% C++23 standard library host   │   │ • BOOTX64.EFI UEFI bootloader        │
│ • Live interactive shell console     │   │ • 12 Known DLLs & 15 .sys drivers    │
│ • PrismX 3D Direct3D 12 frame graph  │   │ • 22 Subsystem services & tools      │
│ • Output: build/bin/*.exe            │   │ • Output: build/sysroot/             │
└──────────────────────────────────────┘   └──────────────────────────────────────┘
```

### Mode 1: Host Simulation & Developer Test Harness
Designed for everyday feature development, CI/CD validation, and subsystem testing. In this mode, `ntoskrnl_core` compiles as a static library linked directly against test harnesses (`micant_kernel`, `micant_tests`, `prismx_3d_cube_app`). It boots the 22 executive phases in-process and tests memory allocators, object tables, schedulers, and network stacks with zero hypervisor overhead.

### Mode 2: Authentic Freestanding Modular Kernel Sysroot
Compiles genuine freestanding PE32+ binaries with authentic subsystem headers (`--subsystem,native`, `--subsystem,efi_application`, `--subsystem,console`, or `-shared`), authentic entry points (`EfiMain`, `KiSystemStartup`, `DriverEntry`, `DllMain`, `_start`), and **no C runtime / no stdlib (`-nostdlib`)**. It populates an exact Windows NT disk sysroot ready for UEFI booting, QEMU virtualization, or bare-metal deployment.

---

## 2. Directory Layout of the Assembled Sysroot (`build/sysroot`)

Running the `sysroot` build target produces the following canonical Windows NT filesystem layout in `build/sysroot/`:

```
build/sysroot/
├── EFI/
│   └── BOOT/
│       └── BOOTX64.EFI           # Freestanding UEFI GOP Bootloader (84 KB)
└── Windows/
    └── System32/
        ├── ntoskrnl.exe          # Freestanding Ring 0 Kernel Executive (968 KB)
        │
        ├── Known DLLs (PE32+ Shared Libraries):
        │   ├── ntdll.dll         # Native NT system call stubs & heap runtime (6 KB)
        │   ├── kernel32.dll      # Win32 Base API subsystem router (5 KB)
        │   ├── kernelbase.dll    # Low-level Win32 core services (3 KB)
        │   ├── user32.dll        # Windowing, messaging & input manager (4 KB)
        │   ├── gdi32.dll         # Graphics Device Interface & primitives (4 KB)
        │   ├── advapi32.dll      # Security, crypto, tokens & registry services (5 KB)
        │   ├── comctl32.dll      # Common controls (buttons, lists, dialogs) (3 KB)
        │   ├── comdlg32.dll      # Standard file & color picker dialogs (3 KB)
        │   ├── shell32.dll       # Desktop shell, folders & shortcuts (3 KB)
        │   ├── shlwapi.dll       # Shell lightweight path & string utilities (3 KB)
        │   ├── ole32.dll         # COM/OLE object broker & moniker runtime (3 KB)
        │   └── ucrtbase.dll      # Universal C Runtime & string routines (5 KB)
        │
        ├── drivers/              # Ring 0 Native Kernel Drivers (.sys):
        │   ├── ahci.sys          # Serial ATA AHCI storage controller driver
        │   ├── i210.sys          # Intel I210 Gigabit PCIe network driver
        │   ├── tcpip.sys         # NDIS 6.x TCP/IPv4/IPv6 protocol engine
        │   ├── fltmgr.sys        # File System Minifilter Manager
        │   ├── xhci.sys          # USB 3.0 / xHCI host controller driver
        │   ├── usbhub.sys        # Universal Serial Bus hub root enumerator
        │   ├── extfs.sys         # EXT2/EXT3/EXT4 read/write filesystem driver
        │   ├── ntfs.sys          # Full NTFS volume & MFT parser driver
        │   ├── exfat.sys         # exFAT flash filesystem driver
        │   ├── cdfs.sys          # ISO 9660 CD-ROM filesystem driver
        │   ├── udfs.sys          # Universal Disk Format (DVD/Blu-ray) driver
        │   ├── refs.sys          # Resilient File System driver
        │   ├── zfs.sys           # ZFS storage pool & dataset driver
        │   ├── null.sys          # Null device driver (\Device\Null)
        │   ├── beep.sys          # PC speaker audio pulse driver
        │   └── etc/              # Network configuration databases:
        │       ├── hosts         # Static hostname-to-IP lookup mappings
        │       ├── services      # IANA port name-to-number mappings
        │       ├── networks      # Network name-to-address mappings
        │       └── protocol      # IP protocol number-to-name mappings
        │
        ├── config/               # Binary Registry Hives:
        │   ├── SYSTEM            # Hardware configuration, services, drivers
        │   ├── SOFTWARE          # Installed applications & subsystem policies
        │   ├── SAM               # Security Account Manager user database
        │   ├── SECURITY          # Local security policies & privilege rights
        │   └── DEFAULT           # Default user profile hive
        │
        └── System Services & Shell Tools:
            ├── smss.exe          # Session Manager Subsystem (Ring 1 -> Ring 3)
            ├── csrss.exe         # Client/Server Runtime Subsystem
            ├── winlogon.exe      # User logon, secure attention sequence (SAS)
            ├── services.exe      # Service Control Manager (SCM daemon)
            ├── lsass.exe         # Local Security Authority Subsystem Service
            ├── svchost.exe       # Generic Service Host container
            ├── conhost.exe       # Console Window Host (VT100 / Win32 console)
            ├── cmd.exe           # Sovereign NT Command Interpreter
            ├── explorer.exe      # Desktop Shell & Taskbar Manager
            ├── taskmgr.exe       # Sovereign Task Manager & Performance Monitor
            ├── notepad.exe       # Text Editor & Document Viewer
            ├── ipconfig.exe      # Network Adapter Configuration Tool
            ├── netstat.exe       # TCP/IP Active Connections & Protocol Stats
            ├── ping.exe          # ICMP Echo Network Diagnostic Tool
            ├── systeminfo.exe    # Hardware & Kernel Configuration Telemetry
            ├── whoami.exe        # Current Security Identity & Privileges Tool
            ├── chkdsk.exe        # Filesystem Integrity Checker & Repair
            ├── diskpart.exe      # Disk & Partition Management Utility
            ├── reg.exe           # Registry Command-Line Console Tool
            ├── tasklist.exe      # Active Process Listing Utility
            ├── taskkill.exe      # Process Termination Utility
            └── shutdown.exe      # System Power & State Controller
```

---

## 3. Toolchain & Prerequisites

To build MicaNT, ensure the following tools are installed:

| Tool | Minimum Version | Installation Command |
| :--- | :--- | :--- |
| **LLVM / Clang** | 17.0+ | `winget install MartinStorsjo.LLVM-MinGW.UCRT` or `sudo apt install clang lld llvm` |
| **CMake** | 3.25+ | `winget install Kitware.CMake` or `sudo apt install cmake` |
| **Ninja** | 1.11+ | `winget install Ninja-build.Ninja` or `sudo apt install ninja-build` |
| **Python** | 3.10+ | `winget install Python.Python.3.12` or `sudo apt install python3` |
| **Node.js** *(Optional)* | 18.0+ | Required only for `clean_room_sentinel.js` compliance audits |
| **QEMU** *(Optional)* | 8.0+ | Required for VM testing (`winget install SoftwareFreedomConservancy.QEMU`) |

---

## 4. Build Instructions

### Step 1: Configure the Workspace
```bash
# Clone the repository
git clone https://github.com/ssfdre38-hue/MicaNT.git
cd MicaNT

# Configure with CMake and Ninja
cmake -B build -G Ninja
```

### Step 2: Build Mode 1 (Host Simulation & Test Harnesses)
```bash
# Compile all host test runners and kernel simulation binaries:
cmake --build build

# Output binaries will be in build/bin/:
#   build/bin/micant_kernel.exe      - Native Kernel Simulation & Shell
#   build/bin/micant_tests.exe       - 516-Suite Test Battery
#   build/bin/prismx_3d_cube_app.exe - PrismX Direct3D 12 Hardware Harness
```

### Step 3: Build Mode 2 (Authentic Freestanding Modular Sysroot)
```bash
# Compile the genuine freestanding operating system layout:
cmake --build build --target sysroot

# Or directly execute the builder script:
python scripts/build_sysroot.py
# (On Windows PowerShell, you can also run: .\scripts\build_sysroot.ps1)
```

The build compiles each component using the appropriate PE subsystem:
- `BOOTX64.EFI`: `-Wl,--subsystem,efi_application -Wl,-e,EfiMain -nostdlib`
- `ntoskrnl.exe`: `-Wl,--subsystem,native -Wl,-e,KiSystemStartup -nostdlib`
- `*.sys` drivers: `-Wl,--subsystem,native -Wl,-e,DriverEntry -nostdlib`
- `*.dll` libraries: `-Wl,--subsystem,native -Wl,-e,DllMain -shared -nostdlib`
- `*.exe` utilities: `-Wl,--subsystem,console -Wl,-e,_start -nostdlib`

---

## 5. Running and Verifying MicaNT

### 1. Run the 516-Suite Verification Battery
```powershell
# Execute the complete 516-suite test battery (100% pass required):
.\build\bin\micant_tests.exe

# Run only the latest Horizon test suite:
.\build\bin\micant_tests.exe --last
```

### 2. Boot the Interactive Host Kernel Console
```powershell
# Boot the kernel executive directly into an interactive command shell:
.\build\bin\micant_kernel.exe --shell

# Inside the shell, explore live NT commands:
MicaNT:\> systeminfo
MicaNT:\> dir
MicaNT:\> prismx
MicaNT:\> prismx cube
MicaNT:\> exit
```

### 3. Run the PrismX 3D Direct3D 12 Hardware Verification
```powershell
# Renders a 5-pass D3D12/Vulkan frame graph with DXR raytracing & DirectML:
.\build\bin\prismx_3d_cube_app.exe
# Produces prismx_3d_render_cube.ppm (viewable in any image viewer)
```

### 4. Boot the Modular Sysroot in QEMU (UEFI)
```powershell
# Launch QEMU with UEFI (OVMF) firmware booting the build/sysroot tree:
python scripts/run_qemu.py

# Or specify custom OVMF firmware path:
python scripts/run_qemu.py --ovmf "C:\path\to\OVMF.fd" --memory 4G --cores 8
```

### 5. Run the Clean-Room Sentinel Audit
```powershell
# Verify zero proprietary contamination and 100% clean-room compliance:
cmake --build build --target sentinel
# Or directly: node scripts/clean_room_sentinel.js
```

---

## 6. Bare-Metal & USB Deployment

To flash the modular sysroot onto a physical bootable USB drive:
1. Format a USB drive with a GPT partition table.
2. Create an **EFI System Partition (ESP)** formatted as **FAT32**.
3. Copy the contents of `build/sysroot/` directly onto the FAT32 partition:
   - Ensure `EFI/BOOT/BOOTX64.EFI` is placed at the root of the ESP.
   - Ensure `Windows/System32/ntoskrnl.exe` and drivers are present in `Windows/System32/`.
4. Insert into any 64-bit UEFI PC (Secure Boot: Custom or Test Mode), select UEFI Boot from BIOS/boot menu.
5. The MicaNT UEFI bootloader will initialize GOP framebuffer graphics, map the LPB (Loader Parameter Block), load `ntoskrnl.exe` into physical memory, and jump to `KiSystemStartup`.
