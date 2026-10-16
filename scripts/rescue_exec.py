#!/usr/bin/env python3
import sys
import os
import paramiko

HOST = os.environ.get("MICANT_SERVER_HOST", "")
PORT = int(os.environ.get("MICANT_SERVER_PORT", "22"))
USER = os.environ.get("MICANT_SERVER_USER", "root")
PASS = os.environ.get("MICANT_SERVER_PASS", "")

def run_remote(cmd):
    if not HOST:
        print("[ERROR] MICANT_SERVER_HOST environment variable not set.", file=sys.stderr)
        return 1
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        client.connect(HOST, port=PORT, username=USER, password=PASS, timeout=15)
        stdin, stdout, stderr = client.exec_command(cmd)
        out = stdout.read().decode('utf-8', errors='replace')
        err = stderr.read().decode('utf-8', errors='replace')
        rc = stdout.channel.recv_exit_status()
        if out:
            print(out, end='')
        if err:
            print(err, file=sys.stderr, end='')
        return rc
    finally:
        client.close()

if __name__ == "__main__":
    if len(sys.argv) > 1:
        cmd = " ".join(sys.argv[1:])
    else:
        cmd = "uname -a && lscpu | grep 'Model name' && lsblk"
    rc = run_remote(cmd)
    sys.exit(rc)
