import os
import sys
import paramiko

sys.stdout.reconfigure(encoding='utf-8')

HOST = os.environ.get("MICANT_TARGET_HOST", "147.135.1.229")
USER = os.environ.get("MICANT_TARGET_USER", "ubuntu")
KEY_PATH = os.environ.get("MICANT_SSH_KEY", r"C:\Users\admin\.ssh\micant_ci_key")

k = paramiko.RSAKey.from_private_key_file(KEY_PATH)
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect(HOST, 22, USER, pkey=k, timeout=10)

def exec_sudo(cmd):
    full_cmd = f"sudo {cmd}"
    stdin, stdout, stderr = ssh.exec_command(full_cmd)
    out = stdout.read().decode('utf-8', errors='replace').strip()
    err = stderr.read().decode('utf-8', errors='replace').strip()
    return out, err

out, err = exec_sudo("which grub-mkimage")
print("which grub-mkimage:", out, err)

# Write early config
early_cfg = "search.fs_uuid e5c1fc28-b885-4be5-bbff-cc23c4d443da root\\nset prefix=($root)/grub\\nconfigfile $prefix/grub.cfg\\n"
exec_sudo(f"printf '{early_cfg}' > /tmp/grub-early.cfg")
out, err = exec_sudo("cat /tmp/grub-early.cfg")
print("grub-early.cfg:\n", out)

cmd_mk = "grub-mkimage -O x86_64-efi -o /boot/efi/EFI/ubuntu/grubx64.efi -c /tmp/grub-early.cfg part_gpt part_msdos ext2 fat normal test search search_fs_uuid echo reboot"
out, err = exec_sudo(cmd_mk)
print("grub-mkimage out:", out, "err:", err)

out, err = exec_sudo("cp -v /boot/efi/EFI/ubuntu/grubx64.efi /boot/efi/EFI/BOOT/BOOTX64.EFI")
print("cp:", out, err)

out, err = exec_sudo("strings /boot/efi/EFI/ubuntu/grubx64.efi | grep e5c1fc28")
print("strings check:", out, err)

ssh.close()
