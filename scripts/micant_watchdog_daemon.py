#!/usr/bin/env python3
"""
MicaNT Telemetry Watchdog Daemon
Continuously monitors bare-metal execution vitals and logs health to telemetry/history.jsonl.
Runs every 30 seconds.
"""

import os
import sys
import time
import subprocess
from datetime import datetime, timezone

BASE_DIR = r"C:\Users\admin\source\MicaNT"
PYTHON_EXE = sys.executable

def log(msg):
    now = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{now}] [MicaNT-Watchdog] {msg}", flush=True)

def main():
    log("Starting MicaNT Bare-Metal Silicon Watchdog Daemon...")
    record_script = os.path.join(BASE_DIR, "scripts", "record_telemetry.py")

    iteration = 0
    while True:
        try:
            iteration += 1
            # Run telemetry capture & ingestion
            proc = subprocess.run(
                [PYTHON_EXE, record_script],
                capture_output=True,
                text=True,
                cwd=BASE_DIR,
                timeout=15
            )
            if proc.returncode == 0:
                out = proc.stdout.strip()
                log(f"Iteration #{iteration}: {out}")
            else:
                log(f"Iteration #{iteration} warning: {proc.stderr.strip()}")
        except Exception as e:
            log(f"Watchdog error: {e}")

        time.sleep(30)

if __name__ == "__main__":
    main()
