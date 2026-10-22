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
IPCONFIG_SOURCE = os.path.join(REPO_ROOT, r"build\ipconfig.exe")
WHOAMI_SOURCE = os.path.join(REPO_ROOT, r"build\whoami.exe")
TASKLIST_SOURCE = os.path.join(REPO_ROOT, r"build\tasklist.exe")
PING_SOURCE = os.path.join(REPO_ROOT, r"build\ping.exe")
CMD_SOURCE = os.path.join(REPO_ROOT, r"build\cmd.exe")
HOSTNAME_SOURCE = os.path.join(REPO_ROOT, r"build\hostname.exe")
SYSTEMINFO_SOURCE = os.path.join(REPO_ROOT, r"build\systeminfo.exe")
NETSTAT_SOURCE = os.path.join(REPO_ROOT, r"build\netstat.exe")
CHKDSK_SOURCE = os.path.join(REPO_ROOT, r"build\chkdsk.exe")
DISKPART_SOURCE = os.path.join(REPO_ROOT, r"build\diskpart.exe")
ARP_SOURCE = os.path.join(REPO_ROOT, r"build\arp.exe")
ROUTE_SOURCE = os.path.join(REPO_ROOT, r"build\route.exe")
REG_SOURCE = os.path.join(REPO_ROOT, r"build\reg.exe")
HOST = os.environ.get("MICANT_TARGET_HOST", "147.135.1.229")
USER = os.environ.get("MICANT_TARGET_USER", "ubuntu")
KEY_PATH = os.environ.get("MICANT_SSH_KEY", r"C:\Users\admin\.ssh\micant_ci_key")

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

    os.makedirs(os.path.join(REPO_ROOT, "build", "bin"), exist_ok=True)
    cmd_cube = [
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
        "tools/prismx_3d_cube_app.cpp",
        "-o", "build/bin/prismx_3d_cube_app.exe"
    ]
    res_c = subprocess.run(cmd_cube, cwd=REPO_ROOT, capture_output=True, text=True)
    if res_c.returncode != 0:
        print(f"[FATAL] prismx_3d_cube_app.exe build failed:\n{res_c.stderr}")
        sys.exit(1)
    sz_c = os.path.getsize(os.path.join(REPO_ROOT, "build", "bin", "prismx_3d_cube_app.exe"))
    print(f"      prismx_3d_cube_app.exe Compiled OK ({sz_c:,} bytes)")

    # Compile System32 Standalone Ring 3 Utilities
    system32_tools = [
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
    ]
    for src, out_path, name in system32_tools:
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
        sz = os.path.getsize(os.path.join(REPO_ROOT, out_path))
        print(f"      {name} Compiled OK ({sz:,} bytes)")

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

    print("[3/5] Uploading binaries via SFTP...")
    sftp = ssh.open_sftp()
    sftp.put(EFI_SOURCE, "/tmp/BOOTX64.EFI")
    sftp.put(KRNL_SOURCE, "/tmp/ntoskrnl.exe")
    sftp.put(APP_SOURCE, "/tmp/app.exe")
    sftp.put(BENCH_SOURCE, "/tmp/bench.exe")
    if os.path.exists(CUBE_SOURCE):
        sftp.put(CUBE_SOURCE, "/tmp/prismx_3d_cube_app.exe")
    if os.path.exists(IPCONFIG_SOURCE):
        sftp.put(IPCONFIG_SOURCE, "/tmp/ipconfig.exe")
    if os.path.exists(WHOAMI_SOURCE):
        sftp.put(WHOAMI_SOURCE, "/tmp/whoami.exe")
    if os.path.exists(TASKLIST_SOURCE):
        sftp.put(TASKLIST_SOURCE, "/tmp/tasklist.exe")
    if os.path.exists(PING_SOURCE):
        sftp.put(PING_SOURCE, "/tmp/ping.exe")
    if os.path.exists(CMD_SOURCE):
        sftp.put(CMD_SOURCE, "/tmp/cmd.exe")
    if os.path.exists(HOSTNAME_SOURCE):
        sftp.put(HOSTNAME_SOURCE, "/tmp/hostname.exe")
    if os.path.exists(SYSTEMINFO_SOURCE):
        sftp.put(SYSTEMINFO_SOURCE, "/tmp/systeminfo.exe")
    if os.path.exists(NETSTAT_SOURCE):
        sftp.put(NETSTAT_SOURCE, "/tmp/netstat.exe")
    if os.path.exists(CHKDSK_SOURCE):
        sftp.put(CHKDSK_SOURCE, "/tmp/chkdsk.exe")
    if os.path.exists(DISKPART_SOURCE):
        sftp.put(DISKPART_SOURCE, "/tmp/diskpart.exe")
    if os.path.exists(ARP_SOURCE):
        sftp.put(ARP_SOURCE, "/tmp/arp.exe")
    if os.path.exists(ROUTE_SOURCE):
        sftp.put(ROUTE_SOURCE, "/tmp/route.exe")
    if os.path.exists(REG_SOURCE):
        sftp.put(REG_SOURCE, "/tmp/reg.exe")
    sftp.close()

    print("      Staging binaries strictly to sovereign MicaNT partitions by PARTUUID...")
    # PARTUUID 0fb58002-611e-491c-829d-1d7c669fdef2 = MICA_ESP (1 GB FAT32)
    # PARTUUID b6457ee0-ed3e-4d78-b82c-a08971cc1b52 = MICA_SYS (128 GB FAT32)
    stage_cmd = (
        'mkdir -p /mnt/micasys /mnt/micaesp && '
        'umount /mnt/micasys 2>/dev/null || true; '
        'umount /mnt/micaesp 2>/dev/null || true; '
        'mount /dev/disk/by-partuuid/b6457ee0-ed3e-4d78-b82c-a08971cc1b52 /mnt/micasys && '
        'mount /dev/disk/by-partuuid/0fb58002-611e-491c-829d-1d7c669fdef2 /mnt/micaesp && '
        'mkdir -p /mnt/micasys/MicaNT/System32 && '
        'cp -v /tmp/ntoskrnl.exe /mnt/micasys/MicaNT/System32/ && '
        'cp -v /tmp/ntoskrnl.exe /mnt/micasys/ && '
        'cp -v /tmp/app.exe /mnt/micasys/MicaNT/System32/ && '
        'cp -v /tmp/bench.exe /mnt/micasys/MicaNT/System32/ && '
        '([ -f /tmp/prismx_3d_cube_app.exe ] && cp -v /tmp/prismx_3d_cube_app.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/prismx_3d_cube_app.exe /mnt/micasys/ || true); '
        '([ -f /tmp/ipconfig.exe ] && cp -v /tmp/ipconfig.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/ipconfig.exe /mnt/micasys/ || true); '
        '([ -f /tmp/whoami.exe ] && cp -v /tmp/whoami.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/whoami.exe /mnt/micasys/ || true); '
        '([ -f /tmp/tasklist.exe ] && cp -v /tmp/tasklist.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/tasklist.exe /mnt/micasys/ || true); '
        '([ -f /tmp/ping.exe ] && cp -v /tmp/ping.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/ping.exe /mnt/micasys/ || true); '
        '([ -f /tmp/cmd.exe ] && cp -v /tmp/cmd.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/cmd.exe /mnt/micasys/ || true); '
        '([ -f /tmp/hostname.exe ] && cp -v /tmp/hostname.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/hostname.exe /mnt/micasys/ || true); '
        '([ -f /tmp/systeminfo.exe ] && cp -v /tmp/systeminfo.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/systeminfo.exe /mnt/micasys/ || true); '
        '([ -f /tmp/netstat.exe ] && cp -v /tmp/netstat.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/netstat.exe /mnt/micasys/ || true); '
        '([ -f /tmp/chkdsk.exe ] && cp -v /tmp/chkdsk.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/chkdsk.exe /mnt/micasys/ || true); '
        '([ -f /tmp/diskpart.exe ] && cp -v /tmp/diskpart.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/diskpart.exe /mnt/micasys/ || true); '
        '([ -f /tmp/arp.exe ] && cp -v /tmp/arp.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/arp.exe /mnt/micasys/ || true); '
        '([ -f /tmp/route.exe ] && cp -v /tmp/route.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/route.exe /mnt/micasys/ || true); '
        '([ -f /tmp/reg.exe ] && cp -v /tmp/reg.exe /mnt/micasys/MicaNT/System32/ && cp -v /tmp/reg.exe /mnt/micasys/ || true); '
        'mkdir -p /mnt/micaesp/EFI/BOOT /mnt/micaesp/EFI/MicaNT && '
        'cp -v /tmp/BOOTX64.EFI /mnt/micaesp/EFI/BOOT/BOOTX64.EFI && '
        'cp -v /tmp/BOOTX64.EFI /mnt/micaesp/EFI/MicaNT/BOOTX64.EFI && '
        'cp -v /tmp/ntoskrnl.exe /mnt/micaesp/EFI/MicaNT/ && '
        'cp -v /tmp/ntoskrnl.exe /mnt/micaesp/ && '
        'cp -v /tmp/app.exe /mnt/micaesp/EFI/MicaNT/ && '
        'cp -v /tmp/bench.exe /mnt/micaesp/EFI/MicaNT/ && '
        '([ -f /tmp/prismx_3d_cube_app.exe ] && cp -v /tmp/prismx_3d_cube_app.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        '([ -f /tmp/ipconfig.exe ] && cp -v /tmp/ipconfig.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        '([ -f /tmp/whoami.exe ] && cp -v /tmp/whoami.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        '([ -f /tmp/tasklist.exe ] && cp -v /tmp/tasklist.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        '([ -f /tmp/ping.exe ] && cp -v /tmp/ping.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        '([ -f /tmp/cmd.exe ] && cp -v /tmp/cmd.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        '([ -f /tmp/hostname.exe ] && cp -v /tmp/hostname.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        '([ -f /tmp/systeminfo.exe ] && cp -v /tmp/systeminfo.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        '([ -f /tmp/netstat.exe ] && cp -v /tmp/netstat.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        '([ -f /tmp/chkdsk.exe ] && cp -v /tmp/chkdsk.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        '([ -f /tmp/diskpart.exe ] && cp -v /tmp/diskpart.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        '([ -f /tmp/arp.exe ] && cp -v /tmp/arp.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        '([ -f /tmp/route.exe ] && cp -v /tmp/route.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        '([ -f /tmp/reg.exe ] && cp -v /tmp/reg.exe /mnt/micaesp/EFI/MicaNT/ || true); '
        'rm -f /mnt/micaesp/EFI/MicaNT/boot.ini /mnt/micaesp/boot.ini /mnt/micasys/boot.ini /mnt/micasys/EFI/MicaNT/boot.ini 2>/dev/null || true; '
        'mkdir -p /boot/efi/EFI/MicaNT && '
        'cp -v /tmp/BOOTX64.EFI /boot/efi/EFI/MicaNT/BOOTX64.EFI && '
        'cp -v /tmp/ntoskrnl.exe /boot/efi/EFI/MicaNT/ && '
        'cp -v /tmp/ntoskrnl.exe /boot/efi/ && '
        'cp -v /tmp/app.exe /boot/efi/EFI/MicaNT/ && '
        'cp -v /tmp/bench.exe /boot/efi/EFI/MicaNT/ && '
        '([ -f /tmp/prismx_3d_cube_app.exe ] && cp -v /tmp/prismx_3d_cube_app.exe /boot/efi/EFI/MicaNT/ || true); '
        '([ -f /tmp/ipconfig.exe ] && cp -v /tmp/ipconfig.exe /boot/efi/EFI/MicaNT/ || true); '
        '([ -f /tmp/whoami.exe ] && cp -v /tmp/whoami.exe /boot/efi/EFI/MicaNT/ || true); '
        '([ -f /tmp/tasklist.exe ] && cp -v /tmp/tasklist.exe /boot/efi/EFI/MicaNT/ || true); '
        '([ -f /tmp/ping.exe ] && cp -v /tmp/ping.exe /boot/efi/EFI/MicaNT/ || true); '
        '([ -f /tmp/cmd.exe ] && cp -v /tmp/cmd.exe /boot/efi/EFI/MicaNT/ || true); '
        '([ -f /tmp/hostname.exe ] && cp -v /tmp/hostname.exe /boot/efi/EFI/MicaNT/ || true); '
        '([ -f /tmp/systeminfo.exe ] && cp -v /tmp/systeminfo.exe /boot/efi/EFI/MicaNT/ || true); '
        '([ -f /tmp/netstat.exe ] && cp -v /tmp/netstat.exe /boot/efi/EFI/MicaNT/ || true); '
        '([ -f /tmp/chkdsk.exe ] && cp -v /tmp/chkdsk.exe /boot/efi/EFI/MicaNT/ || true); '
        '([ -f /tmp/diskpart.exe ] && cp -v /tmp/diskpart.exe /boot/efi/EFI/MicaNT/ || true); '
        '([ -f /tmp/arp.exe ] && cp -v /tmp/arp.exe /boot/efi/EFI/MicaNT/ || true); '
        '([ -f /tmp/route.exe ] && cp -v /tmp/route.exe /boot/efi/EFI/MicaNT/ || true); '
        '([ -f /tmp/reg.exe ] && cp -v /tmp/reg.exe /boot/efi/EFI/MicaNT/ || true); '
        'rm -f /boot/efi/EFI/MicaNT/boot.ini /boot/efi/boot.ini 2>/dev/null || true; '
        'sync && umount /mnt/micasys /mnt/micaesp'
    )
    rc, out, err = exec_sudo(ssh, stage_cmd)
    if rc != 0:
        print(f"[FATAL] Copy failed:\nSTDOUT: {out}\nSTDERR: {err}")
        sys.exit(1)
    print("      Staging files copied successfully:\n" + out)

    print("[4/5] Verifying staged files...")
    verify_cmd = (
        'mkdir -p /mnt/micasys /mnt/micaesp && '
        'mount /dev/disk/by-partuuid/b6457ee0-ed3e-4d78-b82c-a08971cc1b52 /mnt/micasys && '
        'mount /dev/disk/by-partuuid/0fb58002-611e-491c-829d-1d7c669fdef2 /mnt/micaesp && '
        'echo "=== /mnt/micasys/MicaNT/System32/ ===" && '
        'ls -la /mnt/micasys/MicaNT/System32/ && '
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
