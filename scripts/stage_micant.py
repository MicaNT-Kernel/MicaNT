#!/usr/bin/env python3
import os
import re
import subprocess
import sys
import paramiko
import random
import socket

sys.stdout.reconfigure(encoding='utf-8')

REPO_ROOT = r"C:\Users\admin\source\MicaNT"
EFI_SOURCE = os.path.join(REPO_ROOT, r"build\esp\EFI\BOOT\BOOTX64.EFI")
KRNL_SOURCE = os.path.join(REPO_ROOT, r"build\ntoskrnl.exe")
APP_SOURCE = os.path.join(REPO_ROOT, r"build\app.exe")
BENCH_SOURCE = os.path.join(REPO_ROOT, r"build\bench.exe")
CUBE_SOURCE = os.path.join(REPO_ROOT, r"build\bin\prismx_3d_cube_app.exe")
if not os.path.exists(CUBE_SOURCE):
    CUBE_SOURCE = os.path.join(REPO_ROOT, r"build\prismx_3d_cube_app.exe")

HOST = os.environ.get("MICANT_TARGET_HOST", "147.135.1.229")
USER = os.environ.get("MICANT_TARGET_USER", "ubuntu")
KEY_PATH = os.environ.get("MICANT_SSH_KEY", r"C:\Users\admin\.ssh\micant_ci_key")

SYSTEM32_TOOLS = [
    ("tools/ipconfig_app.cpp", "build/ipconfig.exe", "ipconfig.exe"),
    ("tools/whoami_app.cpp", "build/whoami.exe", "whoami.exe"),
    ("tools/tasklist_app.cpp", "build/tasklist.exe", "tasklist.exe"),
    ("tools/ping_app.cpp", "build/ping.exe", "ping.exe"),
    ("tools/cmd_app.cpp", "build/cmd.exe", "cmd.exe"),
    ("tools/hostname_app.cpp", "build/hostname.exe", "hostname.exe"),
    ("tools/systeminfo_app.cpp", "build/systeminfo.exe", "systeminfo.exe"),
    ("tools/netstat_app.cpp", "build/netstat.exe", "netstat.exe"),
    ("tools/chkdsk_app.cpp", "build/chkdsk.exe", "chkdsk.exe"),
    ("tools/diskpart_app.cpp", "build/diskpart.exe", "diskpart.exe"),
    ("tools/arp_app.cpp", "build/arp.exe", "arp.exe"),
    ("tools/route_app.cpp", "build/route.exe", "route.exe"),
    ("tools/reg_app.cpp", "build/reg.exe", "reg.exe"),
    ("tools/sc_app.cpp", "build/sc.exe", "sc.exe"),
    ("tools/driverquery_app.cpp", "build/driverquery.exe", "driverquery.exe"),
    ("tools/type_app.cpp", "build/type.exe", "type.exe"),
    ("tools/find_app.cpp", "build/find.exe", "find.exe"),
    ("tools/fc_app.cpp", "build/fc.exe", "fc.exe"),
    ("tools/attrib_app.cpp", "build/attrib.exe", "attrib.exe"),
    ("tools/more_app.cpp", "build/more.exe", "more.exe"),
    ("tools/format_app.cpp", "build/format.exe", "format.exe"),
    ("tools/taskkill_app.cpp", "build/taskkill.exe", "taskkill.exe"),
    ("tools/shutdown_app.cpp", "build/shutdown.exe", "shutdown.exe"),
    ("tools/robocopy_app.cpp", "build/robocopy.exe", "robocopy.exe"),
    ("tools/mountvol_app.cpp", "build/mountvol.exe", "mountvol.exe"),
    ("tools/certutil_app.cpp", "build/certutil.exe", "certutil.exe"),
    ("tools/klist_app.cpp", "build/klist.exe", "klist.exe"),
    ("tools/fltmc.cpp", "build/fltmc.exe", "fltmc.exe"),
    ("tools/calc_app.cpp", "build/calc.exe", "calc.exe"),
    ("tools/notepad_app.cpp", "build/notepad.exe", "notepad.exe"),
    ("tools/taskmgr_app.cpp", "build/taskmgr.exe", "taskmgr.exe"),
    ("tools/regedit_app.cpp", "build/regedit.exe", "regedit.exe"),
    ("tools/explorer_app.cpp", "build/explorer.exe", "explorer.exe"),
    ("tools/paint_app.cpp", "build/mspaint.exe", "mspaint.exe"),
    ("tools/devmgmt_app.cpp", "build/devmgmt.exe", "devmgmt.exe"),
    ("tools/diskmgmt_app.cpp", "build/diskmgmt.exe", "diskmgmt.exe"),
    ("tools/services_mgmt_app.cpp", "build/services_mgmt.exe", "services_mgmt.exe"),
    ("tools/eventvwr_app.cpp", "build/eventvwr.exe", "eventvwr.exe"),
    ("tools/msinfo32_app.cpp", "build/msinfo32.exe", "msinfo32.exe"),
    ("tools/control_app.cpp", "build/control.exe", "control.exe"),
    ("tools/sysdm_app.cpp", "build/sysdm.exe", "sysdm.exe"),
    ("tools/ncpa_app.cpp", "build/ncpa.exe", "ncpa.exe"),
    ("tools/dxdiag_app.cpp", "build/dxdiag.exe", "dxdiag.exe"),
    ("tools/msconfig_app.cpp", "build/msconfig.exe", "msconfig.exe"),
    ("tools/resmon_app.cpp", "build/resmon.exe", "resmon.exe"),
    ("tools/compmgmt_app.cpp", "build/compmgmt.exe", "compmgmt.exe"),
    ("tools/perfmon_app.cpp", "build/perfmon.exe", "perfmon.exe"),
    ("tools/sfc_app.cpp", "build/sfc.exe", "sfc.exe"),
    ("tools/bcdedit_app.cpp", "build/bcdedit.exe", "bcdedit.exe"),
    ("tools/tracert_app.cpp", "build/tracert.exe", "tracert.exe"),
    ("tools/nslookup_app.cpp", "build/nslookup.exe", "nslookup.exe"),
    ("tools/nbtstat_app.cpp", "build/nbtstat.exe", "nbtstat.exe"),
    ("tools/query_app.cpp", "build/query.exe", "query.exe"),
]

SYSTEM32_SERVICES = [
    ("tools/smss_app.cpp", "build/smss.exe", "smss.exe"),
    ("tools/csrss_app.cpp", "build/csrss.exe", "csrss.exe"),
    ("tools/winlogon_app.cpp", "build/winlogon.exe", "winlogon.exe"),
    ("tools/services_app.cpp", "build/services.exe", "services.exe"),
    ("tools/lsass_app.cpp", "build/lsass.exe", "lsass.exe"),
    ("tools/svchost_app.cpp", "build/svchost.exe", "svchost.exe"),
    ("tools/conhost_app.cpp", "build/conhost.exe", "conhost.exe"),
]

KNOWN_DLLS = [
    ("tools/dlls/ntdll.cpp", "build/ntdll.dll", "ntdll.dll"),
    ("tools/dlls/kernel32.cpp", "build/kernel32.dll", "kernel32.dll"),
    ("tools/dlls/kernelbase.cpp", "build/kernelbase.dll", "kernelbase.dll"),
    ("tools/dlls/user32.cpp", "build/user32.dll", "user32.dll"),
    ("tools/dlls/gdi32.cpp", "build/gdi32.dll", "gdi32.dll"),
    ("tools/dlls/advapi32.cpp", "build/advapi32.dll", "advapi32.dll"),
    ("tools/dlls/comctl32.cpp", "build/comctl32.dll", "comctl32.dll"),
    ("tools/dlls/comdlg32.cpp", "build/comdlg32.dll", "comdlg32.dll"),
    ("tools/dlls/shell32.cpp", "build/shell32.dll", "shell32.dll"),
    ("tools/dlls/shlwapi.cpp", "build/shlwapi.dll", "shlwapi.dll"),
    ("tools/dlls/ole32.cpp", "build/ole32.dll", "ole32.dll"),
    ("tools/dlls/ucrtbase.cpp", "build/ucrtbase.dll", "ucrtbase.dll"),
]

DRIVER_STUBS = [
    ("tools/drivers/driver_template.cpp", "build/ahci.sys", "ahci.sys"),
    ("tools/drivers/driver_template.cpp", "build/i210.sys", "i210.sys"),
    ("tools/drivers/driver_template.cpp", "build/tcpip.sys", "tcpip.sys"),
    ("tools/drivers/driver_template.cpp", "build/fltmgr.sys", "fltmgr.sys"),
    ("tools/drivers/driver_template.cpp", "build/xhci.sys", "xhci.sys"),
    ("tools/drivers/driver_template.cpp", "build/usbhub.sys", "usbhub.sys"),
    ("tools/drivers/driver_template.cpp", "build/extfs.sys", "extfs.sys"),
    ("tools/drivers/driver_template.cpp", "build/null.sys", "null.sys"),
    ("tools/drivers/driver_template.cpp", "build/beep.sys", "beep.sys"),
]

NETWORK_FILES = [
    ("tools/etc/hosts", "hosts"),
    ("tools/etc/services", "services"),
    ("tools/etc/networks", "networks"),
    ("tools/etc/protocol", "protocol"),
]

REGISTRY_HIVES = [
    ("tools/config/SYSTEM", "SYSTEM"),
    ("tools/config/SOFTWARE", "SOFTWARE"),
    ("tools/config/SAM", "SAM"),
    ("tools/config/SECURITY", "SECURITY"),
    ("tools/config/DEFAULT", "DEFAULT"),
]

def run_local_build():
    print("[1/5] Compiling clean-room freestanding BOOTX64.EFI, ntoskrnl.exe, app.exe, and bench.exe...")
    os.makedirs(os.path.join(REPO_ROOT, "build", "esp", "EFI", "BOOT"), exist_ok=True)
    
    cmd_efi = [
        "clang++",
        "-target", "x86_64-w64-mingw32",
        "-std=c++23",
        "-ffreestanding",
        "-fno-exceptions",
        "-fno-rtti",
        "-fno-stack-protector",
        "-mno-red-zone",
        "-mno-stack-arg-probe",
        "-O2",
        "-Iinclude",
        "-Wl,--subsystem,efi_application",
        "-Wl,-e,EfiMain",
        "-nostdlib",
        "boot/bootx64.cpp",
        "-o", "build/esp/EFI/BOOT/BOOTX64.EFI"
    ]
    res = subprocess.run(cmd_efi, cwd=REPO_ROOT, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[FATAL] BOOTX64.EFI build failed:\n{res.stderr}")
        sys.exit(1)
    sz = os.path.getsize(EFI_SOURCE)
    print(f"      BOOTX64.EFI Compiled OK ({sz:,} bytes)")

    cmd_krnl = [
        "clang++",
        "-target", "x86_64-w64-mingw32",
        "-std=c++23",
        "-ffreestanding",
        "-fno-exceptions",
        "-fno-rtti",
        "-fno-threadsafe-statics",
        "-fno-stack-protector",
        "-mno-red-zone",
        "-mno-stack-arg-probe",
        "-O2",
        "-Iinclude",
        "-Wl,--subsystem,native",
        "-Wl,-e,KiSystemStartup",
        "-nostdlib",
        "kernel/init/ntoskrnl.cpp",
        "-o", "build/ntoskrnl.exe"
    ]
    res_k = subprocess.run(cmd_krnl, cwd=REPO_ROOT, capture_output=True, text=True)
    if res_k.returncode != 0:
        print(f"[FATAL] ntoskrnl.exe build failed:\n{res_k.stderr}")
        sys.exit(1)
    sz_k = os.path.getsize(KRNL_SOURCE)
    print(f"      ntoskrnl.exe Compiled OK ({sz_k:,} bytes)")

    cmd_app = [
        "clang++",
        "-target", "x86_64-w64-mingw32",
        "-std=c++23",
        "-ffreestanding",
        "-fno-exceptions",
        "-fno-rtti",
        "-fno-stack-protector",
        "-mno-red-zone",
        "-mno-stack-arg-probe",
        "-O2",
        "-Wl,--subsystem,console",
        "-Wl,-e,_start",
        "-nostdlib",
        "tools/micant_app.cpp",
        "-o", "build/app.exe"
    ]
    res_a = subprocess.run(cmd_app, cwd=REPO_ROOT, capture_output=True, text=True)
    if res_a.returncode != 0:
        print(f"[FATAL] app.exe build failed:\n{res_a.stderr}")
        sys.exit(1)
    sz_a = os.path.getsize(APP_SOURCE)
    print(f"      app.exe Compiled OK ({sz_a:,} bytes)")

    cmd_bench = [
        "clang++",
        "-target", "x86_64-w64-mingw32",
        "-std=c++23",
        "-ffreestanding",
        "-fno-exceptions",
        "-fno-rtti",
        "-fno-stack-protector",
        "-mno-red-zone",
        "-mno-stack-arg-probe",
        "-O2",
        "-Wl,--subsystem,console",
        "-Wl,-e,_start",
        "-nostdlib",
        "tools/bench_app.cpp",
        "-o", "build/bench.exe"
    ]
    res_b = subprocess.run(cmd_bench, cwd=REPO_ROOT, capture_output=True, text=True)
    if res_b.returncode != 0:
        print(f"[FATAL] bench.exe build failed:\n{res_b.stderr}")
        sys.exit(1)
    sz_b = os.path.getsize(BENCH_SOURCE)
    print(f"      bench.exe Compiled OK ({sz_b:,} bytes)")

    # Compile System32 Standalone Ring 3 Utilities
    print(f"      Compiling {len(SYSTEM32_TOOLS)} System32 Utilities & Desktop Apps...")
    for src, out_path, name in SYSTEM32_TOOLS:
        c_cmd = [
            "clang++",
            "-target", "x86_64-w64-mingw32",
            "-std=c++23",
            "-ffreestanding",
            "-fno-exceptions",
            "-fno-rtti",
            "-fno-stack-protector",
            "-mno-red-zone",
            "-mno-stack-arg-probe",
            "-O2",
            "-Wl,--subsystem,console",
            "-Wl,-e,_start",
            "-nostdlib",
            src,
            "-o", out_path
        ]
        r = subprocess.run(c_cmd, cwd=REPO_ROOT, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"[FATAL] {name} build failed:\n{r.stderr}")
            sys.exit(1)

    # Compile System32 Subsystem Services
    print("      Compiling 7 System32 Subsystem Services...")
    for src, out_path, name in SYSTEM32_SERVICES:
        c_cmd = [
            "clang++",
            "-target", "x86_64-w64-mingw32",
            "-std=c++23",
            "-ffreestanding",
            "-fno-exceptions",
            "-fno-rtti",
            "-fno-stack-protector",
            "-mno-red-zone",
            "-mno-stack-arg-probe",
            "-O2",
            "-Wl,--subsystem,console",
            "-Wl,-e,_start",
            "-nostdlib",
            src,
            "-o", out_path
        ]
        r = subprocess.run(c_cmd, cwd=REPO_ROOT, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"[FATAL] {name} build failed:\n{r.stderr}")
            sys.exit(1)

    # Compile 12 Known DLLs (PE32+ Shared Libraries)
    print("      Compiling 12 Known DLLs...")
    for src, out_path, name in KNOWN_DLLS:
        c_cmd = [
            "clang++",
            "-target", "x86_64-w64-mingw32",
            "-shared",
            "-nostdlib",
            "-Wl,-e,DllMain",
            "-Wl,--subsystem,native",
            "-O2",
            src,
            "-o", out_path
        ]
        r = subprocess.run(c_cmd, cwd=REPO_ROOT, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"[FATAL] {name} build failed:\n{r.stderr}")
            sys.exit(1)

    # Compile Kernel Driver Stubs (PE32+ Native Drivers)
    print("      Compiling 9 Kernel Driver Stubs (.sys)...")
    for src, out_path, name in DRIVER_STUBS:
        c_cmd = [
            "clang++",
            "-target", "x86_64-w64-mingw32",
            "-nostdlib",
            "-Wl,-e,DriverEntry",
            "-Wl,--subsystem,native",
            "-O2",
            src,
            "-o", out_path
        ]
        r = subprocess.run(c_cmd, cwd=REPO_ROOT, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"[FATAL] {name} build failed:\n{r.stderr}")
            sys.exit(1)

    # Generate Registry Hives if needed
    hives_script = os.path.join(REPO_ROOT, "tools", "config", "gen_hives.py")
    if os.path.exists(hives_script):
        subprocess.run(["python", hives_script], cwd=REPO_ROOT, check=True)

    print("      Local build completed successfully!")

def get_ssh():
    k = paramiko.RSAKey.from_private_key_file(KEY_PATH)
    for _ in range(20):
        try:
            s = socket.socket()
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            port = random.randint(49152, 65535)
            s.bind(('0.0.0.0', port))
            s.settimeout(5.0)
            s.connect((HOST, 22))
            client = paramiko.SSHClient()
            client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            client.connect(HOST, 22, USER, pkey=k, sock=s, timeout=5)
            return client
        except OSError:
            pass
    raise RuntimeError('Could not connect')

def exec_sudo(ssh, cmd):
    sudo_pass = os.environ.get("MICANT_SUDO_PASS", "")
    if sudo_pass:
        full_cmd = f"echo {sudo_pass} | sudo -S bash -c {cmd!r}"
    else:
        full_cmd = f"sudo bash -c {cmd!r}"
    stdin, stdout, stderr = ssh.exec_command(full_cmd)
    out = stdout.read().decode('utf-8', errors='replace').strip()
    err = stderr.read().decode('utf-8', errors='replace').strip()
    rc = stdout.channel.recv_exit_status()
    clean_err = "\n".join(l for l in err.splitlines() if "[sudo]" not in l and "password" not in l.lower())
    return rc, out, clean_err

def main():
    run_local_build()

    print("[2/5] Connecting to server via SSH key...")
    ssh = get_ssh()
    print("      Connected as 'ubuntu'.")

    print("[3/5] Uploading binaries and configuration files via SFTP...")
    sftp = ssh.open_sftp()
    sftp.put(EFI_SOURCE, "/tmp/BOOTX64.EFI")
    sftp.put(KRNL_SOURCE, "/tmp/ntoskrnl.exe")
    sftp.put(APP_SOURCE, "/tmp/app.exe")
    sftp.put(BENCH_SOURCE, "/tmp/bench.exe")
    if os.path.exists(CUBE_SOURCE):
        sftp.put(CUBE_SOURCE, "/tmp/prismx_3d_cube_app.exe")
    surshell_path = os.path.join(REPO_ROOT, r"build\surshell.exe")
    if not os.path.exists(surshell_path):
        alt_surshell = r"C:\Users\admin\source\SurShell\build\bin\surshell_app.exe"
        if os.path.exists(alt_surshell):
            import shutil
            shutil.copy2(alt_surshell, surshell_path)
    if os.path.exists(surshell_path):
        sftp.put(surshell_path, "/tmp/surshell.exe")

    for _, out_path, name in SYSTEM32_TOOLS:
        p = os.path.join(REPO_ROOT, out_path)
        if os.path.exists(p):
            sftp.put(p, f"/tmp/{name}")

    for _, out_path, name in SYSTEM32_SERVICES:
        p = os.path.join(REPO_ROOT, out_path)
        if os.path.exists(p):
            sftp.put(p, f"/tmp/{name}")

    for _, out_path, name in KNOWN_DLLS:
        p = os.path.join(REPO_ROOT, out_path)
        if os.path.exists(p):
            sftp.put(p, f"/tmp/{name}")

    for _, out_path, name in DRIVER_STUBS:
        p = os.path.join(REPO_ROOT, out_path)
        if os.path.exists(p):
            sftp.put(p, f"/tmp/{name}")

    for src_rel, name in NETWORK_FILES:
        p = os.path.join(REPO_ROOT, src_rel)
        if os.path.exists(p):
            sftp.put(p, f"/tmp/{name}")

    for src_rel, name in REGISTRY_HIVES:
        p = os.path.join(REPO_ROOT, src_rel)
        if os.path.exists(p):
            sftp.put(p, f"/tmp/{name}")

    stage_script = """#!/bin/bash
set -e
echo "--- Unmounting and Checking Filesystems ---"
umount /mnt/micasys 2>/dev/null || true
umount /mnt/micaesp 2>/dev/null || true
mkdir -p /mnt/micasys /mnt/micaesp

# Clear any dirty bits or FAT discrepancies
fsck.vfat -a -w /dev/disk/by-partuuid/b6457ee0-ed3e-4d78-b82c-a08971cc1b52 || true
fsck.vfat -a -w /dev/disk/by-partuuid/0fb58002-611e-491c-829d-1d7c669fdef2 || true

echo "--- Mounting Target Partitions ---"
mount -o rw,umask=000 /dev/disk/by-partuuid/b6457ee0-ed3e-4d78-b82c-a08971cc1b52 /mnt/micasys
mount -o rw,umask=000 /dev/disk/by-partuuid/0fb58002-611e-491c-829d-1d7c669fdef2 /mnt/micaesp

mkdir -p /mnt/micasys/MicaNT/System32/drivers/etc
mkdir -p /mnt/micasys/MicaNT/System32/config
mkdir -p /mnt/micasys/MicaNT/System32/LogFiles

echo "--- Copying Kernel and Userland Executables ---"
cp -v /tmp/ntoskrnl.exe /mnt/micasys/MicaNT/System32/
cp -v /tmp/ntoskrnl.exe /mnt/micasys/
cp -v /tmp/app.exe /mnt/micasys/MicaNT/System32/
cp -v /tmp/bench.exe /mnt/micasys/MicaNT/System32/
[ -f /tmp/prismx_3d_cube_app.exe ] && cp -v /tmp/prismx_3d_cube_app.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/prismx_3d_cube_app.exe /mnt/micasys/ || true

for f in /tmp/*.exe; do
    if [ -f "$f" ]; then
        cp -v "$f" /mnt/micasys/MicaNT/System32/
        cp -v "$f" /mnt/micasys/
    fi
done

echo "--- Generating MMC Snap-in Consoles (.msc) & Control Panel Applets (.cpl) ---"
cp -v /mnt/micasys/MicaNT/System32/devmgmt.exe /mnt/micasys/MicaNT/System32/devmgmt.msc || true
cp -v /mnt/micasys/MicaNT/System32/devmgmt.exe /mnt/micasys/devmgmt.msc || true
cp -v /mnt/micasys/MicaNT/System32/diskmgmt.exe /mnt/micasys/MicaNT/System32/diskmgmt.msc || true
cp -v /mnt/micasys/MicaNT/System32/diskmgmt.exe /mnt/micasys/diskmgmt.msc || true
cp -v /mnt/micasys/MicaNT/System32/services_mgmt.exe /mnt/micasys/MicaNT/System32/services.msc || true
cp -v /mnt/micasys/MicaNT/System32/services_mgmt.exe /mnt/micasys/services.msc || true
cp -v /mnt/micasys/MicaNT/System32/eventvwr.exe /mnt/micasys/MicaNT/System32/eventvwr.msc || true
cp -v /mnt/micasys/MicaNT/System32/eventvwr.exe /mnt/micasys/eventvwr.msc || true
cp -v /mnt/micasys/MicaNT/System32/compmgmt.exe /mnt/micasys/MicaNT/System32/compmgmt.msc || true
cp -v /mnt/micasys/MicaNT/System32/compmgmt.exe /mnt/micasys/compmgmt.msc || true
cp -v /mnt/micasys/MicaNT/System32/perfmon.exe /mnt/micasys/MicaNT/System32/perfmon.msc || true
cp -v /mnt/micasys/MicaNT/System32/perfmon.exe /mnt/micasys/perfmon.msc || true
cp -v /mnt/micasys/MicaNT/System32/sysdm.exe /mnt/micasys/MicaNT/System32/sysdm.cpl || true
cp -v /mnt/micasys/MicaNT/System32/sysdm.exe /mnt/micasys/sysdm.cpl || true
cp -v /mnt/micasys/MicaNT/System32/ncpa.exe /mnt/micasys/MicaNT/System32/ncpa.cpl || true
cp -v /mnt/micasys/MicaNT/System32/ncpa.exe /mnt/micasys/ncpa.cpl || true

echo "--- Copying Known DLLs ---"
for f in /tmp/*.dll; do
    if [ -f "$f" ]; then
        cp -v "$f" /mnt/micasys/MicaNT/System32/
    fi
done

echo "--- Copying Kernel Drivers (.sys) ---"
for f in /tmp/*.sys; do
    if [ -f "$f" ]; then
        cp -v "$f" /mnt/micasys/MicaNT/System32/drivers/
    fi
done

echo "--- Copying Network Tables ---"
for f in hosts services networks protocol; do
    if [ -f "/tmp/$f" ]; then
        cp -v "/tmp/$f" /mnt/micasys/MicaNT/System32/drivers/etc/
    fi
done

echo "--- Copying Registry Hives ---"
for f in SYSTEM SOFTWARE SAM SECURITY DEFAULT; do
    if [ -f "/tmp/$f" ]; then
        cp -v "/tmp/$f" /mnt/micasys/MicaNT/System32/config/
    fi
done

echo "--- Mirroring Bootloader to ESP ---"
mkdir -p /mnt/micaesp/EFI/BOOT /mnt/micaesp/EFI/MicaNT
cp -v /tmp/BOOTX64.EFI /mnt/micaesp/EFI/BOOT/BOOTX64.EFI
cp -v /tmp/BOOTX64.EFI /mnt/micaesp/EFI/MicaNT/BOOTX64.EFI
cp -v /tmp/ntoskrnl.exe /mnt/micaesp/EFI/MicaNT/
cp -v /tmp/ntoskrnl.exe /mnt/micaesp/
rm -f /mnt/micaesp/EFI/MicaNT/boot.ini /mnt/micaesp/boot.ini /mnt/micasys/boot.ini /mnt/micasys/EFI/MicaNT/boot.ini 2>/dev/null || true

mkdir -p /boot/efi/EFI/MicaNT
cp -v /tmp/BOOTX64.EFI /boot/efi/EFI/MicaNT/BOOTX64.EFI
cp -v /tmp/ntoskrnl.exe /boot/efi/EFI/MicaNT/
cp -v /tmp/ntoskrnl.exe /boot/efi/
rm -f /boot/efi/EFI/MicaNT/boot.ini /boot/efi/boot.ini 2>/dev/null || true

sync
umount /mnt/micasys /mnt/micaesp
echo "--- Staging Script Completed Successfully ---"
"""
    with sftp.open("/tmp/stage.sh", "w") as sf:
        sf.write(stage_script)
    sftp.close()

    print("      Staging files strictly to sovereign MicaNT partitions by PARTUUID...")
    exec_sudo(ssh, "chmod +x /tmp/stage.sh")
    rc, out, err = exec_sudo(ssh, "/tmp/stage.sh")
    if rc != 0:
        print(f"[FATAL] Copy failed:\nSTDOUT: {out}\nSTDERR: {err}")
        sys.exit(1)
    print("      Staging files copied successfully:\n" + out)

    print("[4/5] Verifying staged files...")
    verify_cmd = (
        'mkdir -p /mnt/micasys /mnt/micaesp && '
        'mount /dev/disk/by-partuuid/b6457ee0-ed3e-4d78-b82c-a08971cc1b52 /mnt/micasys && '
        'mount /dev/disk/by-partuuid/0fb58002-611e-491c-829d-1d7c669fdef2 /mnt/micaesp && '
        'echo "=== /mnt/micasys/MicaNT/System32/ (Executables & DLLs) ===" && '
        'ls -la /mnt/micasys/MicaNT/System32/ && '
        'echo "=== /mnt/micasys/MicaNT/System32/drivers/ (Kernel Drivers) ===" && '
        'ls -la /mnt/micasys/MicaNT/System32/drivers/ && '
        'echo "=== /mnt/micasys/MicaNT/System32/drivers/etc/ (Network Tables) ===" && '
        'ls -la /mnt/micasys/MicaNT/System32/drivers/etc/ && '
        'echo "=== /mnt/micasys/MicaNT/System32/config/ (Registry Hives) ===" && '
        'ls -la /mnt/micasys/MicaNT/System32/config/ && '
        'echo "=== /mnt/micaesp/EFI/MicaNT/ ===" && '
        'ls -la /mnt/micaesp/EFI/MicaNT/ && '
        'umount /mnt/micasys /mnt/micaesp && '
        'echo "=== /boot/efi/EFI/MicaNT/ ===" && '
        'ls -la /boot/efi/EFI/MicaNT/'
    )
    rc, out, err = exec_sudo(ssh, verify_cmd)
    print(f"      Verified files:\n{out}")

    print("[5/5] Ensuring UEFI NVRAM Boot Order (Ubuntu fallback primary)...")
    target_boot_order = "000C,000D,0003,0000,0008,0006,0004,0007,0005,0001,0002,0009,000B"
    exec_sudo(ssh, f"efibootmgr -o {target_boot_order}")
    exec_sudo(ssh, "efibootmgr -a -b 000D 2>/dev/null || true")

    rc, out, err = exec_sudo(ssh, "efibootmgr")
    print(out)

    sz = os.path.getsize(EFI_SOURCE)
    print(f"\n=======================================================")
    print(f"  MicaNT Cutler Edition is staged and ready on bare metal!")
    print(f"  MicaNT ESP (PARTUUID 0fb58002...): /EFI/MicaNT/BOOTX64.EFI ({sz:,} bytes)")
    print(f"  Ubuntu ESP (PARTUUID 63426ac5...): /EFI/MicaNT/BOOTX64.EFI mirror")
    print(f"  Default BootOrder: {target_boot_order}")
    print(f"=======================================================")

    ssh.close()

if __name__ == "__main__":
    main()
