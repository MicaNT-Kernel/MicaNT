#!/usr/bin/env python3
"""
MicaNT Remote Testing & Telemetry Harness.
Listens for live bare-metal silicon beacons on UDP port 9999,
monitors IIS access logs for netboot/telemetry HTTP hits,
and provides remote control injection (mouse, keyboard, commands).
"""
import sys
import os
import socket
import json
import time
import threading

SERVER_IP = os.environ.get("MICANT_SERVER_IP", "")
BIND_IP = "0.0.0.0"
UDP_PORT = 9999
IIS_LOG_DIR = r"C:\inetpub\logs\LogFiles\W3SVC1"

def udp_listener():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((BIND_IP, UDP_PORT))
        print(f"[Remote Harness] Listening on UDP {BIND_IP}:{UDP_PORT} for MicaNT silicon beacons...")
        while True:
            data, addr = sock.recvfrom(4096)
            ts = time.strftime("%H:%M:%S")
            try:
                msg = data.decode('utf-8', errors='replace')
                print(f"[{ts}][UDP Telemetry from {addr[0]}] {msg}")
            except Exception as e:
                print(f"[{ts}][UDP Telemetry Raw {len(data)}B from {addr[0]}]")
    except Exception as e:
        print(f"[Remote Harness] UDP bind error: {e}")
    finally:
        sock.close()

def send_command(cmd_str):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(cmd_str.encode('utf-8'), (SERVER_IP, UDP_PORT))
        print(f"[Remote Harness] Sent command to {SERVER_IP}:{UDP_PORT}: {cmd_str}")
    finally:
        sock.close()

def iis_log_monitor():
    print(f"[Remote Harness] Monitoring IIS logs in {IIS_LOG_DIR} for {SERVER_IP}...")
    current_log = None
    last_size = 0
    
    while True:
        try:
            # Pick latest log file
            log_files = [os.path.join(IIS_LOG_DIR, f) for f in os.listdir(IIS_LOG_DIR) if f.startswith("u_ex") and f.endswith(".log")]
            if log_files:
                latest_log = max(log_files, key=os.path.getmtime)
                if latest_log != current_log:
                    current_log = latest_log
                    last_size = os.path.getsize(current_log)
                    print(f"[Remote Harness] Watching latest IIS log: {os.path.basename(current_log)} (size {last_size})")
                
                cur_size = os.path.getsize(current_log)
                if cur_size > last_size:
                    with open(current_log, "r", encoding="utf-8", errors="replace") as f:
                        f.seek(last_size)
                        new_lines = f.readlines()
                        last_size = cur_size
                        for line in new_lines:
                            if SERVER_IP in line or "ipxe" in line.lower() or "bootx64" in line.lower():
                                print(f"[IIS Netboot] {line.strip()}", flush=True)
            time.sleep(0.5)
        except Exception as e:
            time.sleep(1)

if __name__ == "__main__":
    t_udp = threading.Thread(target=udp_listener, daemon=True)
    t_udp.start()
    
    t_iis = threading.Thread(target=iis_log_monitor, daemon=True)
    t_iis.start()
    
    print("[Remote Harness] Active and watching. Press Ctrl+C to stop.")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("[Remote Harness] Stopped.")
