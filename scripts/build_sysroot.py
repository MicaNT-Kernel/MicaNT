#!/usr/bin/env python3
"""
MicaNT Modular Sysroot Builder
Assembles the authentic freestanding Windows NT operating system directory tree:
  - EFI/BOOT/BOOTX64.EFI          (Freestanding UEFI Boot Application)
  - Windows/System32/ntoskrnl.exe  (Freestanding Native Ring 0 Kernel)
  - Windows/System32/*.dll        (Known DLLs: ntdll, kernel32, user32, etc.)
  - Windows/System32/drivers/*.sys(Kernel Drivers: AHCI, I210, TCP/IP, FS drivers)
  - Windows/System32/*.exe        (Subsystem Services & Shell utilities)
  - Windows/System32/config/*     (Registry Hives: SYSTEM, SOFTWARE, SAM, etc.)
  - Windows/System32/drivers/etc/*(Network configuration databases)
"""

import os
import sys
import shutil
import subprocess
from pathlib import Path

# ANSI colors for status logging
CYAN = "\033[96m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
GRAY = "\033[90m"
RED = "\033[91m"
BOLD = "\033[1m"
RESET = "\033[0m"

# Windows color support in cmd/powershell
if sys.platform == "win32":
    os.system("")

def find_compiler():
    """Locate suitable freestanding C++ compiler (prefer clang++)."""
    compiler = shutil.which("clang++")
    if compiler:
        return compiler
    compiler = shutil.which("g++")
    if compiler:
        return compiler
    return None

def run_command(cmd, desc):
    """Run compiler command and exit on error."""
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        print(f"{RED}[FAIL]{RESET} {desc}")
        if res.stderr:
            print(f"{RED}{res.stderr}{RESET}")
        sys.exit(res.returncode)

def main():
    repo_root = Path(__file__).resolve().parent.parent
    sysroot = repo_root / "build" / "sysroot"
    efi_dir = sysroot / "EFI" / "BOOT"
    system32 = sysroot / "Windows" / "System32"
    drivers_dir = system32 / "drivers"
    config_dir = system32 / "config"
    etc_dir = drivers_dir / "etc"

    print(f"{CYAN}{BOLD}========================================================================{RESET}")
    print(f"{CYAN}{BOLD}             Building Authentic MicaNT Kernel Sysroot                   {RESET}")
    print(f"{CYAN}{BOLD}========================================================================{RESET}")

    cxx = find_compiler()
    if not cxx:
        print(f"{RED}[ERROR] Neither clang++ nor g++ found in PATH. Install LLVM/Clang to build the sysroot.{RESET}")
        sys.exit(1)
    print(f"{GRAY}Compiler: {cxx}{RESET}")
    print(f"{GRAY}Target Sysroot: {sysroot}{RESET}\n")

    # 1. Directory Structure
    for d in [efi_dir, system32, drivers_dir, config_dir, etc_dir]:
        d.mkdir(parents=True, exist_ok=True)

    include_dir = str(repo_root / "include")
    tools_dir = str(repo_root / "tools")

    # 2. Build BOOTX64.EFI
    print(f"{YELLOW}[1/6] Compiling BOOTX64.EFI (Freestanding UEFI Boot Application)...{RESET}")
    efi_src = repo_root / "boot" / "bootx64.cpp"
    efi_out = efi_dir / "BOOTX64.EFI"
    cmd_efi = [
        cxx, "-target", "x86_64-w64-mingw32", "-std=c++23", "-ffreestanding",
        "-fno-exceptions", "-fno-rtti", "-fno-stack-protector", "-mno-red-zone",
        "-mno-stack-arg-probe", "-O2", f"-I{include_dir}",
        "-Wl,--subsystem,efi_application", "-Wl,-e,EfiMain", "-nostdlib",
        str(efi_src), "-o", str(efi_out)
    ]
    run_command(cmd_efi, "BOOTX64.EFI")
    sz = efi_out.stat().st_size
    print(f"      {GREEN}BOOTX64.EFI ready: {sz:,} bytes{RESET}")

    # 3. Build ntoskrnl.exe
    print(f"{YELLOW}[2/6] Compiling ntoskrnl.exe (Freestanding Native Ring 0 Kernel)...{RESET}")
    krnl_src = repo_root / "kernel" / "init" / "ntoskrnl.cpp"
    krnl_out = system32 / "ntoskrnl.exe"
    cmd_krnl = [
        cxx, "-target", "x86_64-w64-mingw32", "-std=c++23", "-ffreestanding",
        "-fno-exceptions", "-fno-rtti", "-fno-threadsafe-statics", "-fno-stack-protector",
        "-mno-red-zone", "-mno-stack-arg-probe", "-O2", f"-I{include_dir}",
        "-Wl,--subsystem,native", "-Wl,-e,KiSystemStartup", "-nostdlib",
        str(krnl_src), "-o", str(krnl_out)
    ]
    run_command(cmd_krnl, "ntoskrnl.exe")
    sz = krnl_out.stat().st_size
    print(f"      {GREEN}ntoskrnl.exe ready: {sz:,} bytes{RESET}")

    # 4. Build Known DLLs
    print(f"{YELLOW}[3/6] Compiling Known DLLs into System32...{RESET}")
    dll_list = [
        ("tools/dlls/ntdll.cpp", "ntdll.dll"),
        ("tools/dlls/kernel32.cpp", "kernel32.dll"),
        ("tools/dlls/kernelbase.cpp", "kernelbase.dll"),
        ("tools/dlls/user32.cpp", "user32.dll"),
        ("tools/dlls/gdi32.cpp", "gdi32.dll"),
        ("tools/dlls/advapi32.cpp", "advapi32.dll"),
        ("tools/dlls/comctl32.cpp", "comctl32.dll"),
        ("tools/dlls/comdlg32.cpp", "comdlg32.dll"),
        ("tools/dlls/shell32.cpp", "shell32.dll"),
        ("tools/dlls/shlwapi.cpp", "shlwapi.dll"),
        ("tools/dlls/ole32.cpp", "ole32.dll"),
        ("tools/dlls/ucrtbase.cpp", "ucrtbase.dll"),
    ]
    for rel_src, name in dll_list:
        src_path = repo_root / rel_src
        out_path = system32 / name
        if src_path.exists():
            cmd = [
                cxx, "-target", "x86_64-w64-mingw32", "-shared", "-nostdlib",
                "-Wl,-e,DllMain", "-Wl,--subsystem,native", "-O2",
                f"-I{include_dir}", str(src_path), "-o", str(out_path)
            ]
            run_command(cmd, name)
            print(f"      {GRAY}Compiled {name} ({out_path.stat().st_size:,} bytes){RESET}")

    # 5. Build Kernel Drivers (.sys)
    print(f"{YELLOW}[4/6] Compiling Kernel Drivers (.sys) into System32\\drivers...{RESET}")
    driver_list = [
        ("tools/drivers/driver_template.cpp", "ahci.sys"),
        ("tools/drivers/driver_template.cpp", "i210.sys"),
        ("tools/drivers/driver_template.cpp", "tcpip.sys"),
        ("tools/drivers/driver_template.cpp", "fltmgr.sys"),
        ("tools/drivers/driver_template.cpp", "xhci.sys"),
        ("tools/drivers/driver_template.cpp", "usbhub.sys"),
        ("tools/drivers/driver_template.cpp", "extfs.sys"),
        ("tools/drivers/driver_template.cpp", "null.sys"),
        ("tools/drivers/driver_template.cpp", "beep.sys"),
        ("tools/drivers/ntfs_driver.cpp", "ntfs.sys"),
        ("tools/drivers/exfat_driver.cpp", "exfat.sys"),
        ("tools/drivers/cdfs_driver.cpp", "cdfs.sys"),
        ("tools/drivers/udfs_driver.cpp", "udfs.sys"),
        ("tools/drivers/refs_driver.cpp", "refs.sys"),
        ("tools/drivers/zfs_driver.cpp", "zfs.sys"),
    ]
    for rel_src, name in driver_list:
        src_path = repo_root / rel_src
        out_path = drivers_dir / name
        if src_path.exists():
            cmd = [
                cxx, "-target", "x86_64-w64-mingw32", "-nostdlib",
                "-Wl,-e,DriverEntry", "-Wl,--subsystem,native", "-O2",
                f"-I{include_dir}", str(src_path), "-o", str(out_path)
            ]
            run_command(cmd, name)
            print(f"      {GRAY}Compiled {name} ({out_path.stat().st_size:,} bytes){RESET}")

    # 6. Build Core System Services & Shell Tools
    print(f"{YELLOW}[5/6] Compiling System Services and Shell Tools...{RESET}")
    svc_list = [
        ("tools/smss_app.cpp", "smss.exe"),
        ("tools/csrss_app.cpp", "csrss.exe"),
        ("tools/winlogon_app.cpp", "winlogon.exe"),
        ("tools/services_app.cpp", "services.exe"),
        ("tools/lsass_app.cpp", "lsass.exe"),
        ("tools/svchost_app.cpp", "svchost.exe"),
        ("tools/conhost_app.cpp", "conhost.exe"),
        ("tools/cmd_app.cpp", "cmd.exe"),
        ("tools/ipconfig_app.cpp", "ipconfig.exe"),
        ("tools/whoami_app.cpp", "whoami.exe"),
        ("tools/tasklist_app.cpp", "tasklist.exe"),
        ("tools/systeminfo_app.cpp", "systeminfo.exe"),
        ("tools/ping_app.cpp", "ping.exe"),
        ("tools/netstat_app.cpp", "netstat.exe"),
        ("tools/chkdsk_app.cpp", "chkdsk.exe"),
        ("tools/diskpart_app.cpp", "diskpart.exe"),
        ("tools/reg_app.cpp", "reg.exe"),
        ("tools/taskkill_app.cpp", "taskkill.exe"),
        ("tools/shutdown_app.cpp", "shutdown.exe"),
        ("tools/explorer_app.cpp", "explorer.exe"),
        ("tools/taskmgr_app.cpp", "taskmgr.exe"),
        ("tools/notepad_app.cpp", "notepad.exe"),
    ]
    for rel_src, name in svc_list:
        src_path = repo_root / rel_src
        out_path = system32 / name
        if src_path.exists():
            cmd = [
                cxx, "-target", "x86_64-w64-mingw32", "-std=c++23", "-ffreestanding",
                "-fno-exceptions", "-fno-rtti", "-fno-stack-protector", "-mno-red-zone",
                "-mno-stack-arg-probe", "-O2", f"-I{include_dir}", f"-I{tools_dir}",
                "-Wl,--subsystem,console", "-Wl,-e,_start", "-nostdlib",
                str(src_path), "-o", str(out_path)
            ]
            run_command(cmd, name)
            print(f"      {GRAY}Compiled {name} ({out_path.stat().st_size:,} bytes){RESET}")

    # 7. Generate Registry Hives & Network Configs
    print(f"{YELLOW}[6/6] Populating Registry Hives & Network Configs...{RESET}")
    gen_hives_script = repo_root / "tools" / "config" / "gen_hives.py"
    if gen_hives_script.exists():
        subprocess.run([sys.executable, str(gen_hives_script)], check=True, stdout=subprocess.PIPE)

    hives = ["SYSTEM", "SOFTWARE", "SAM", "SECURITY", "DEFAULT"]
    for h in hives:
        h_src = repo_root / "tools" / "config" / h
        if h_src.exists():
            shutil.copyfile(h_src, config_dir / h)
            print(f"      {GRAY}Installed hive: {h} ({h_src.stat().st_size:,} bytes){RESET}")

    net_files = ["hosts", "services", "networks", "protocol"]
    for nf in net_files:
        nf_src = repo_root / "tools" / "etc" / nf
        if nf_src.exists():
            shutil.copyfile(nf_src, etc_dir / nf)
            print(f"      {GRAY}Installed network database: {nf} ({nf_src.stat().st_size:,} bytes){RESET}")

    print(f"\n{GREEN}{BOLD}========================================================================{RESET}")
    print(f"{GREEN}{BOLD}       MicaNT Modular Sysroot Assembly Complete!                       {RESET}")
    print(f"{GREEN}{BOLD}========================================================================{RESET}")
    print(f"{CYAN}Sysroot Root: {sysroot}{RESET}")
    print(f"{CYAN}  ├── EFI/BOOT/BOOTX64.EFI ({efi_out.stat().st_size:,} bytes){RESET}")
    print(f"{CYAN}  └── Windows/System32/{RESET}")
    print(f"{CYAN}      ├── ntoskrnl.exe ({krnl_out.stat().st_size:,} bytes){RESET}")
    print(f"{CYAN}      ├── {len(dll_list)} Known DLLs{RESET}")
    print(f"{CYAN}      ├── drivers/ ({len(driver_list)} .sys kernel drivers + etc/){RESET}")
    print(f"{CYAN}      ├── {len(svc_list)} System Services & Console Executables{RESET}")
    print(f"{CYAN}      └── config/ ({len(hives)} Registry Hives: SYSTEM, SAM, etc.){RESET}")

if __name__ == "__main__":
    main()
