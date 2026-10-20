import socket
import random
import paramiko

def get_ssh():
    k = paramiko.RSAKey.from_private_key_file(r"C:\Users\admin\.ssh\micant_ci_key")
    for _ in range(10):
        try:
            s = socket.socket()
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            port = random.randint(53000, 64000)
            s.bind(('0.0.0.0', port))
            s.connect(('147.135.1.229', 22))
            client = paramiko.SSHClient()
            client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            client.connect('147.135.1.229', 22, 'ubuntu', pkey=k, sock=s)
            return client
        except OSError:
            pass
    raise RuntimeError('Could not connect')

ssh = get_ssh()

def exec_sudo(cmd):
    full_cmd = f"sudo {cmd}"
    stdin, stdout, stderr = ssh.exec_command(full_cmd)
    out = stdout.read().decode('utf-8', errors='replace').strip()
    err = stderr.read().decode('utf-8', errors='replace').strip()
    return out, err

default_grub = """# Configured for MicaNT / Ubuntu Dual-Boot Architecture
GRUB_DEFAULT=0
GRUB_TIMEOUT_STYLE=menu
GRUB_TIMEOUT=5
GRUB_DISTRIBUTOR="Ubuntu"
GRUB_CMDLINE_LINUX_DEFAULT=""
GRUB_CMDLINE_LINUX="nomodeset iommu=pt console=tty0 console=ttyS0,115200n8"
GRUB_TERMINAL="console serial"
GRUB_SERIAL_COMMAND="serial --unit=0 --speed=115200 --parity=no --word=8"
"""

custom_grub = """#!/bin/sh
exec tail -n +3 $0
# This file provides an easy way to add custom menu entries.  Simply type the
# menu entries you want to add after this comment.  Be careful not to change
# the 'exec tail' line above.

menuentry 'MicaNT Cutler Edition (Sovereign OS on sdb)' --class os {
    insmod part_gpt
    insmod fat
    insmod chain
    search --no-floppy --fs-uuid --set=root 4746-DEFA
    chainloader /EFI/MicaNT/BOOTX64.EFI
}

menuentry 'MicaNT Cutler Edition (Mirror on sda)' --class os {
    insmod part_gpt
    insmod fat
    insmod chain
    search --no-floppy --fs-uuid --set=root D39D-CC9F
    chainloader /EFI/MicaNT/BOOTX64.EFI
}
"""

sftp = ssh.open_sftp()
with sftp.file('/tmp/default_grub', 'w') as f:
    f.write(default_grub)
with sftp.file('/tmp/40_custom', 'w') as f:
    f.write(custom_grub)
sftp.close()

out, err = exec_sudo("cp -v /tmp/default_grub /etc/default/grub")
print(out)
out, err = exec_sudo("cp -v /tmp/40_custom /etc/grub.d/40_custom && chmod +x /etc/grub.d/40_custom")
print(out)

print("=== Running update-grub ===")
out, err = exec_sudo("update-grub")
print(out)
if err:
    print("stderr:", err)

print("=== Menu entries in /boot/grub/grub.cfg ===")
out, err = exec_sudo("grep -E 'menuentry ' /boot/grub/grub.cfg")
print(out)

ssh.close()
