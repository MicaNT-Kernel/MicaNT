#!/usr/bin/env python3
"""
Provision robust auto-netboot architecture on remote OVH dedicated server.
Bakes static OVH network addressing into iPXE EFI chainloader with automated
fallback to local cached kernel on EFI System Partition (/dev/sda1).
"""
import os
import sys
import paramiko

HOST = "147.135.1.229"
PORT = 22
USER = "root"
PASS = os.environ.get("RESCUE_PASS", "6rpPo9J3YDuRbnpkg")

EMBED_IPXE_SCRIPT = """#!ipxe
prompt --key 0x02 --timeout 3000 Press Ctrl-B for iPXE shell... || goto boot_barrer
:boot_barrer
echo [MicaNT] Configuring network interface net0...
dhcp || goto static_ip
goto fetch_barrer

:static_ip
echo [MicaNT] Static OVH network config: 147.135.1.229/24 gw 147.135.1.254...
set net0/ip 147.135.1.229
set net0/netmask 255.255.255.0
set net0/gateway 147.135.1.254
set dns 8.8.8.8

:fetch_barrer
echo [MicaNT] Netbooting live kernel from http://barrer.net/ipxe/boot.ipxe...
chain http://barrer.net/ipxe/boot.ipxe || chain http://40.160.53.129/ipxe/boot.ipxe || goto fallback

:fallback
echo [MicaNT] Network unreachable! Falling back to local disk copy...
chain \\EFI\\MicaNT\\BOOTX64.EFI || shell
"""

def provision():
    print(f"[MicaNT Provision] Connecting to {HOST}:{PORT} as {USER}...")
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(HOST, port=PORT, username=USER, password=PASS, timeout=20)
    print("[MicaNT Provision] SSH connection established.")

    commands = [
        ("Mounting ESP /dev/sda1", "mkdir -p /mnt/esp && mount /dev/sda1 /mnt/esp"),
        ("Updating local cached BOOTX64.EFI from barrer.net", "curl -sSf http://barrer.net/ipxe/BOOTX64.EFI -o /mnt/esp/EFI/MicaNT/BOOTX64.EFI && ls -lh /mnt/esp/EFI/MicaNT/BOOTX64.EFI"),
        ("Writing static-IP embed.ipxe script", f"cat << 'EOF' > /root/ipxe/src/embed.ipxe\n{EMBED_IPXE_SCRIPT}\nEOF\n"),
        ("Compiling x86_64 UEFI iPXE with static IP support", "cd /root/ipxe/src && make -j8 bin-x86_64-efi/ipxe.efi EMBED=embed.ipxe && ls -lh bin-x86_64-efi/ipxe.efi"),
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
    print("\n[MicaNT Provision] SUCCESS: Server persistent netboot configured with static OVH networking!")
    return 0

if __name__ == "__main__":
    if len(sys.argv) > 1:
        PASS = sys.argv[1]
    sys.exit(provision())
