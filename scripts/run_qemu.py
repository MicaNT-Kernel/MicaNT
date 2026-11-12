#!/usr/bin/env python3
"""
MicaNT QEMU UEFI Virtual Machine Launcher
Launches the assembled modular sysroot (build/sysroot) inside QEMU with UEFI (OVMF) firmware.
"""

import os
import sys
import shutil
import argparse
import subprocess
from pathlib import Path

# ANSI colors
CYAN = "\033[96m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
GRAY = "\033[90m"
RED = "\033[91m"
BOLD = "\033[1m"
RESET = "\033[0m"

if sys.platform == "win32":
    os.system("")

COMMON_OVMF_PATHS = [
    # Environment override
    os.environ.get("OVMF_PATH"),
    # Windows standard paths
    r"C:\Program Files\qemu\share\edk2-x86_64-code.fd",
    r"C:\Program Files\qemu\share\OVMF.fd",
    r"C:\Program Files (x86)\qemu\share\edk2-x86_64-code.fd",
    os.path.expandvars(r"%LOCALAPPDATA%\qemu\share\edk2-x86_64-code.fd"),
    # Linux standard paths
    "/usr/share/OVMF/OVMF_CODE.fd",
    "/usr/share/ovmf/OVMF.fd",
    "/usr/share/ovmf/x64/OVMF.fd",
    "/usr/share/edk2-ovmf/x64/OVMF_CODE.fd",
    "/usr/share/edk2/ovmf/OVMF_CODE.fd",
    "/usr/share/qemu/OVMF.fd",
    # macOS Homebrew paths
    "/opt/homebrew/share/qemu/edk2-x86_64-code.fd",
    "/usr/local/share/qemu/edk2-x86_64-code.fd",
]

def find_ovmf():
    for p in COMMON_OVMF_PATHS:
        if p and Path(p).is_file():
            return Path(p).resolve()
    return None

def main():
    parser = argparse.ArgumentParser(description="Boot MicaNT modular sysroot in QEMU with UEFI.")
    parser.add_argument("--ovmf", type=str, help="Path to OVMF/EDK2 UEFI firmware binary (.fd)")
    parser.add_argument("--memory", type=str, default="2G", help="VM memory size (default: 2G)")
    parser.add_argument("--cores", type=int, default=4, help="SMP core count (default: 4)")
    parser.add_argument("--no-gui", action="store_true", help="Run without graphical display (nographic)")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    sysroot = repo_root / "build" / "sysroot"
    bootloader = sysroot / "EFI" / "BOOT" / "BOOTX64.EFI"
    ntoskrnl = system32_krnl = sysroot / "Windows" / "System32" / "ntoskrnl.exe"

    print(f"{CYAN}{BOLD}========================================================================{RESET}")
    print(f"{CYAN}{BOLD}                 MicaNT QEMU UEFI Virtual Machine                       {RESET}")
    print(f"{CYAN}{BOLD}========================================================================{RESET}")

    # Check if sysroot is built
    if not bootloader.exists() or not ntoskrnl.exists():
        print(f"{YELLOW}[WARN] Modular sysroot not found or incomplete in build/sysroot.{RESET}")
        print(f"{YELLOW}       Building sysroot now...{RESET}\n")
        build_script = repo_root / "scripts" / "build_sysroot.py"
        subprocess.run([sys.executable, str(build_script)], check=True)

    # Check for QEMU
    qemu_bin = shutil.which("qemu-system-x86_64")
    if not qemu_bin:
        print(f"{RED}[ERROR] qemu-system-x86_64 was not found in PATH.{RESET}")
        print(f"{GRAY}Please install QEMU for your platform:")
        print(f"  - Windows: winget install SoftwareFreedomConservancy.QEMU")
        print(f"  - Ubuntu/Debian: sudo apt install qemu-system-x86 ovmf")
        print(f"  - Arch Linux: sudo pacman -S qemu-system-x86 edk2-ovmf")
        print(f"  - macOS: brew install qemu{RESET}")
        sys.exit(1)

    # Locate OVMF
    ovmf_path = Path(args.ovmf).resolve() if args.ovmf else find_ovmf()
    if not ovmf_path:
        print(f"{YELLOW}[WARN] OVMF UEFI firmware file could not be auto-detected.{RESET}")
        print(f"{GRAY}You can supply it explicitly with: python scripts/run_qemu.py --ovmf <path/to/OVMF.fd>{RESET}")
        print(f"{GRAY}Attempting direct boot without custom -bios...{RESET}\n")

    cmd = [
        qemu_bin,
        "-m", args.memory,
        "-smp", str(args.cores),
        "-drive", f"file=fat:rw:{sysroot},format=raw",
        "-serial", "stdio"
    ]
    if ovmf_path:
        cmd.extend(["-bios", str(ovmf_path)])
    if args.no_gui:
        cmd.append("-nographic")

    print(f"{GREEN}Executing QEMU command:{RESET}")
    print(f"{GRAY}{' '.join(cmd)}{RESET}\n")
    try:
        subprocess.run(cmd)
    except KeyboardInterrupt:
        print(f"\n{YELLOW}QEMU session terminated by user.{RESET}")

if __name__ == "__main__":
    main()
