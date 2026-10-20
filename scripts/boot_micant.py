#!/usr/bin/env python3
import sys
import time
import paramiko

sys.stdout.reconfigure(encoding='utf-8')

HOST = os.environ.get("MICANT_TARGET_HOST", "147.135.1.229")
USER = os.environ.get("MICANT_TARGET_USER", "ubuntu")
KEY_PATH = os.environ.get("MICANT_SSH_KEY", r"C:\Users\admin\.ssh\micant_ci_key")

def main():
    print("[1/2] Connecting to server via SSH...")
    import socket
    import random
    k = paramiko.RSAKey.from_private_key_file(KEY_PATH)
    ssh = None
    for _ in range(10):
        try:
            s = socket.socket()
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            port = random.randint(53000, 64000)
            s.bind(('0.0.0.0', port))
            s.connect((HOST, 22))
            ssh = paramiko.SSHClient()
            ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            ssh.connect(HOST, 22, USER, pkey=k, sock=s)
            break
        except OSError:
            pass
    if not ssh:
        raise RuntimeError("Could not connect to server")
    print("      Connected.")

    print("[2/2] Arming BootNext=000D (MicaNT_SDB) and issuing reboot...")
    cmd = "sudo efibootmgr --bootnext 000D && sudo reboot"
    try:
        stdin, stdout, stderr = ssh.exec_command(cmd, timeout=5)
        out = stdout.read().decode('utf-8', errors='replace').strip()
        print(f"      Server response:\n{out}")
    except Exception as e:
        print(f"      Reboot triggered (connection dropped as expected): {e}")

    ssh.close()
    print("      Server is now rebooting into MicaNT Cutler Edition on sda1!")

if __name__ == "__main__":
    main()
