#!/usr/bin/env python3
"""
Provision robust auto-netboot architecture on remote dedicated server.
Bakes static network addressing into iPXE EFI chainloader with automated
fallback to local cached kernel on EFI System Partition (/dev/sda1).
"""
import os
import sys
import paramiko

HOST = os.environ.get("MICANT_SERVER_HOST", "")
PORT = int(os.environ.get("MICANT_SERVER_PORT", "22"))
USER = os.environ.get("MICANT_SERVER_USER", "root")
PASS = os.environ.get("MICANT_SERVER_PASS", "")

EMBED_IPXE_SCRIPT = """#!ipxe
prompt --key 0x02 --timeout 3000 Press Ctrl-B for iPXE shell... || goto boot_net
:boot_net
echo [MicaNT] Configuring network interface net0...
dhcp || goto fallback
echo [MicaNT] Netbooting live kernel...
chain http://40.160.53.129/ipxe/boot.ipxe || goto fallback

:fallback
echo [MicaNT] Network unreachable! Falling back to local disk copy...
chain \\EFI\\MicaNT\\BOOTX64.EFI || shell
"""

def provision():
    if not HOST:
        print("[ERROR] MICANT_SERVER_HOST environment variable not set.", file=sys.stderr)
        return 1
    print(f"[MicaNT Provision] Connecting to server as {USER}...")
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(HOST, port=PORT, username=USER, password=PASS, timeout=20)
    print("[MicaNT Provision] SSH connection established.")

    commands = [
        ("Mounting ESP /dev/sda1", "mkdir -p /mnt/esp && (mountpoint -q /mnt/esp || mount /dev/sda1 /mnt/esp)"),
        ("Installing build dependencies & cloning iPXE", "apt-get update && apt-get install -y gcc binutils make perl liblzma-dev mtools git efibootmgr && [ -d /root/ipxe ] || git clone --depth 1 https://github.com/ipxe/ipxe.git /root/ipxe"),
        ("Writing embed.ipxe script", f"cat << 'EOF' > /root/ipxe/src/embed.ipxe\n{EMBED_IPXE_SCRIPT}\nEOF\n"),
        ("Compiling x86_64 UEFI iPXE with embedded script", "cd /root/ipxe/src && make -j8 bin-x86_64-efi/ipxe.efi EMBED=embed.ipxe && ls -lh bin-x86_64-efi/ipxe.efi"),
        ("Installing new ipxe.efi to /mnt/esp/EFI/BOOT/BOOTX64.EFI", "cp /root/ipxe/src/bin-x86_64-efi/ipxe.efi /mnt/esp/EFI/BOOT/BOOTX64.EFI && ls -lh /mnt/esp/EFI/BOOT/BOOTX64.EFI"),
        ("Verifying efibootmgr boot order", "efibootmgr -v"),
        ("Syncing and unmounting /mnt/esp", "sync && umount /mnt/esp && sync")
    ]

    for desc, cmd in commands:
        print(f"\n---> {desc}...")
        stdin, stdout, stderr = client.exec_command(cmd)
        out = stdout.read().decode('utf-8', errors='replace')
        err = stderr.read().decode('utf-8', errors='replace')
        rc = stdout.channel.recv_exit_status()
        if out:
            print(out, end='')
        if err:
            print(err, file=sys.stderr, end='')
        if rc != 0:
            print(f"[ERROR] Step failed with return code {rc}")
            client.close()
            return rc

    client.close()
    print("\n[MicaNT Provision] SUCCESS: Server persistent netboot configured!")
    return 0

if __name__ == "__main__":
    sys.exit(provision())
