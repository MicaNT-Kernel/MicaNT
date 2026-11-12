#!/usr/bin/env python3
"""Boot the verified modular kernel in QEMU/OVMF and check serial milestones."""

import argparse
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from verify_kernel_architecture import validate_build_manifest


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_BOOTSTRAP_SERIAL = (
    "[MicaNT COM1] EfiMain reached on physical x86_64 silicon!",
    "[MicaNT Boot] Probing FAT32 volume for \\EFI\\MicaNT\\ntoskrnl.exe...",
    "[MicaNT Boot] ExitBootServices SUCCESS!",
    "[MicaNT] Modular KiSystemStartup reached",
    "[MicaNT] Phase 0 started",
    "[MicaNT] Kernel-owned page-table root active",
    "[MicaNT Mm] Canonical physical allocation, zeroing and release verified",
    "[MicaNT Mm] PFN-backed page tables, TLB and reclaim verified",
    "[MicaNT Ex] Canonical page-backed C and C++ allocation verified",
    "[MicaNT Mm] Canonical VAD tree exclusion and cleanup verified",
    "[MicaNT Mm] VAD reserve, commit, mapping and release verified",
    "[MicaNT Mm] Managed protection, decommit and PFN reclamation verified",
    "[MicaNT Mm] QEMU managed noncommitted and read-only faults verified",
    "[MicaNT Ex] Kernel C++ constructors dispatched",
    "[MicaNT Ob] Canonical kernel handle storage and references verified",
    "[MicaNT Ke] Ready-queue withdrawal verified",
    "[MicaNT Ldr] Independent TLS templates verified",
)
EXPECTED_RUNTIME_SELFTEST_SERIAL = EXPECTED_BOOTSTRAP_SERIAL + (
    "[MicaNT Ex] Compiler-generated construction and LIFO teardown verified",
    "[MicaNT Ex] Runtime self-test halted",
)
EXPECTED_SERIAL = EXPECTED_BOOTSTRAP_SERIAL + (
    "[MicaNT Ke] Allocation-free ready queue selected cooperative contexts",
    "[MicaNT Ke] Kernel thread block and wake verified",
    "[MicaNT Display] virtio_gpu.sys: VirtIO 1.0 2D scanout initialized",
    "[MicaNT Display] Desktop presented through virtio_gpu.sys",
    "[MicaNT Explorer] Desktop and taskbar rendered",
    "[MicaNT Ps] CPL3 execution and Ring 0 trap return verified",
    "[MicaNT Ps] GS-based TEB TLS slot read/write verified",
    "[MicaNT Mm] User/kernel isolation and read-only user code verified",
    "[MicaNT Mm] Supervisor MMIO and non-executable user stack verified",
    "[MicaNT Hal] Local APIC periodic interrupts verified",
    "[MicaNT Ke] Timer-preempted kernel contexts verified",
    "[MicaNT Ke] CPL3 syscall ABI, register preservation and user-buffer checks verified",
    "[MicaNT Mm] CPL3 native allocation and release verified",
    "[MicaNT Ex] Canonical checked user copies verified at CPL3 boundary",
    "[MicaNT Ps] Bounded native PE mapping and compiled user entry verified",
    "[MicaNT Input] PS/2 mouse polling initialized",
    "[MicaNT Explorer] Shell idle loop entered",
)

def verify_mouse(qmp_path: Path, temp: Path, serial_path: Path) -> None:
    """Exercise the emulated mouse and verify its actual scanout pixels."""
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
        connection.settimeout(5)
        connection.connect(str(qmp_path))
        with connection.makefile("rwb") as stream:
            def receive() -> dict:
                line = stream.readline()
                if not line:
                    raise RuntimeError("QMP closed during mouse verification")
                return json.loads(line)

            if "QMP" not in receive():
                raise RuntimeError("QEMU did not send a QMP greeting")

            def execute(command: str, arguments: dict | None = None) -> None:
                stream.write((json.dumps({
                    "execute": command, "arguments": arguments or {},
                }) + "\n").encode())
                stream.flush()
                while True:
                    response = receive()
                    if "error" in response:
                        raise RuntimeError(f"QMP {command} failed: {response['error']}")
                    if "return" in response:
                        return

            def screen(name: str) -> bytes:
                path = temp / name
                execute("screendump", {"filename": str(path)})
                with path.open("rb") as image:
                    if (image.readline() != b"P6\n" or
                            image.readline() != b"1024 768\n" or
                            image.readline() != b"255\n"):
                        raise RuntimeError("Unexpected VirtIO scanout screenshot format")
                    pixels = image.read()
                if len(pixels) != 1024 * 768 * 3:
                    raise RuntimeError("Truncated VirtIO scanout screenshot")
                return pixels

            execute("qmp_capabilities")
            before = screen("before.ppm")
            for y in range(2):
                for x in range(2):
                    pixel = (y * 1024 + x) * 3
                    if before[pixel:pixel + 3] != bytes((0x11, 0x22, 0x33)):
                        raise RuntimeError("CPL3 surface presentation did not reach VirtIO scanout")
            execute("input-send-event", {"events": [
                {"type": "rel", "data": {"axis": "x", "value": 40}},
                {"type": "rel", "data": {"axis": "y", "value": 20}},
            ]})
            time.sleep(0.5)
            after = screen("after.ppm")
            old_x, old_y = (1024 - 16) // 2, (768 - 16) // 2
            new_x, new_y = old_x + 40, old_y + 20
            old = (old_y * 1024 + old_x) * 3
            new = (new_y * 1024 + new_x) * 3
            if (before[old:old + 3] == after[old:old + 3] or
                    before[old:old + 3] != after[new:new + 3]):
                raise RuntimeError("PS/2 movement did not relocate the visible cursor")
            for y in range(768):
                for x in range(1024):
                    if ((old_x <= x < old_x + 16 and old_y <= y < old_y + 16) or
                            (new_x <= x < new_x + 16 and new_y <= y < new_y + 16)):
                        continue
                    pixel = (y * 1024 + x) * 3
                    if before[pixel:pixel + 3] != after[pixel:pixel + 3]:
                        raise RuntimeError("Mouse movement corrupted pixels outside cursor rectangles")
            execute("input-send-event", {"events": [
                {"type": "btn", "data": {"button": "left", "down": True}},
            ]})
            time.sleep(0.1)
            execute("input-send-event", {"events": [
                {"type": "btn", "data": {"button": "left", "down": False}},
            ]})
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                serial = serial_path.read_text(encoding="utf-8", errors="replace")
                if "[MicaNT Input] Left button pressed; user-mode shell not started" in serial:
                    return
                time.sleep(0.1)
            raise RuntimeError("PS/2 left click did not reach the kernel input loop")


def serial_progress(serial_log: str, expected: tuple[str, ...] = EXPECTED_SERIAL) -> int:
    """Return how many expected messages occur, in order, in the serial log."""
    cursor = 0
    for index, marker in enumerate(expected):
        found = serial_log.find(marker, cursor)
        if found < 0:
            return index
        cursor = found + len(marker)
    return len(expected)


def serial_has_failure(serial_log: str) -> bool:
    return "FATAL:" in serial_log or any(
        line.startswith("[MicaNT") and (" failed" in line or "Invalid " in line)
        for line in serial_log.splitlines()
    )


def run_qemu(
    ovmf_code: Path,
    timeout: int,
    stable_seconds: int,
    display: str,
    interactive: bool,
    verify_input: bool,
    runtime_selftest: bool = False,
) -> None:
    if runtime_selftest and (interactive or verify_input):
        raise RuntimeError("runtime self-test halts before desktop/input; incompatible runner options")
    validate_build_manifest(ROOT, allow_dirty=True, runtime_selftest=runtime_selftest)
    expected = EXPECTED_RUNTIME_SELFTEST_SERIAL if runtime_selftest else EXPECTED_SERIAL

    qemu = shutil.which(os.environ.get("MICANT_QEMU", "qemu-system-x86_64"))
    if not qemu:
        raise RuntimeError("qemu-system-x86_64 was not found on PATH")
    if not ovmf_code.is_file():
        raise RuntimeError(f"OVMF code image does not exist: {ovmf_code}")

    kernel = ROOT / "build" / "baremetal" / "ntoskrnl.exe"
    bootloader = ROOT / "build" / "esp" / "EFI" / "BOOT" / "BOOTX64.EFI"

    with tempfile.TemporaryDirectory(prefix="qemu-", dir=ROOT / "build") as temp_root:
        temp = Path(temp_root)
        esp = temp / "esp"
        boot_dir = esp / "EFI" / "BOOT"
        kernel_dir = esp / "EFI" / "MicaNT"
        boot_dir.mkdir(parents=True)
        kernel_dir.mkdir(parents=True)
        shutil.copyfile(bootloader, boot_dir / "BOOTX64.EFI")
        shutil.copyfile(kernel, kernel_dir / "ntoskrnl.exe")
        driver_dir = kernel_dir / "System32" / "drivers"
        driver_dir.mkdir(parents=True)
        shutil.copyfile(ROOT / "build" / "virtio_gpu.sys", driver_dir / "virtio_gpu.sys")
        serial_path = temp / "serial.log"
        qemu_output_path = temp / "qemu.log"

        command = [
            qemu,
            "-machine", "q35",
            "-accel", "tcg",
            "-cpu", "qemu64",
            "-smp", "1",
            "-m", "1024",
            "-vga", "none",
            "-device", "virtio-vga",
            "-drive",
            f"if=pflash,format=raw,unit=0,readonly=on,file={ovmf_code.resolve()}",
            "-drive", f"file=fat:ro:{esp},format=raw,if=ide,index=0",
            "-display", display,
            "-serial", f"file:{serial_path}",
            "-monitor", "none",
            "-qmp", f"unix:{temp / 'qmp.sock'},server=on,wait=off",
            "-snapshot",
            "-no-reboot",
            "-no-shutdown",
        ]
        started = time.monotonic()
        complete_since = None

        with qemu_output_path.open("w", encoding="utf-8") as qemu_output:
            process = subprocess.Popen(
                command,
                cwd=ROOT,
                stdout=qemu_output,
                stderr=subprocess.STDOUT,
            )
            try:
                while time.monotonic() - started < timeout:
                    return_code = process.poll()
                    if return_code is not None:
                        qemu_output.flush()
                        details = qemu_output_path.read_text(
                            encoding="utf-8", errors="replace"
                        )
                        raise RuntimeError(
                            f"QEMU exited with status {return_code} before the "
                            f"required boot state.\n{details}"
                        )

                    serial_log = (
                        serial_path.read_text(encoding="utf-8", errors="replace")
                        if serial_path.exists()
                        else ""
                    )
                    if serial_has_failure(serial_log):
                        raise RuntimeError(f"Kernel bootstrap failed:\n{serial_log[-8000:]}")
                    progress = serial_progress(serial_log, expected)
                    if progress == len(expected):
                        if complete_since is None:
                            complete_since = time.monotonic()
                        elif time.monotonic() - complete_since >= stable_seconds:
                            if verify_input:
                                verify_mouse(temp / "qmp.sock", temp, serial_path)
                                print("PS/2 movement, scanout updates and left-click delivery passed.", flush=True)
                            result = ("C++ runtime self-test" if runtime_selftest
                                      else "modular boot")
                            print(f"QEMU {result} passed; serial remained stable for "
                                  f"{stable_seconds}s.", flush=True)
                            if interactive:
                                print("QEMU is running interactively; close its window or press Ctrl+C to stop.", flush=True)
                                return_code = process.wait()
                                if return_code != 0:
                                    raise RuntimeError(
                                        f"QEMU exited with status {return_code} "
                                        "after the modular shell started"
                                    )
                            return
                    else:
                        complete_since = None
                    time.sleep(0.1)

                serial_log = (
                    serial_path.read_text(encoding="utf-8", errors="replace")
                    if serial_path.exists()
                    else ""
                )
                progress = serial_progress(serial_log, expected)
                missing = (expected[progress] if progress < len(expected)
                           else "stable idle interval")
                qemu_output.flush()
                details = qemu_output_path.read_text(encoding="utf-8", errors="replace")
                raise RuntimeError(
                    f"QEMU boot timed out after {timeout}s; next required serial "
                    f"milestone: {missing}\nSerial tail:\n{serial_log[-8000:]}"
                    f"\nQEMU output:\n{details[-4000:]}"
                )
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--ovmf-code",
        type=Path,
        default=os.environ.get("MICANT_OVMF_CODE"),
        help="path to the x86_64 OVMF code image (or MICANT_OVMF_CODE)",
    )
    parser.add_argument("--timeout", type=int, default=60)
    parser.add_argument("--stable-seconds", type=int, default=5)
    parser.add_argument("--display", default="none")
    parser.add_argument("--interactive", action="store_true")
    parser.add_argument("--verify-input", action="store_true")
    parser.add_argument("--runtime-selftest", action="store_true")
    args = parser.parse_args()

    if args.ovmf_code is None:
        parser.error("--ovmf-code or MICANT_OVMF_CODE is required")
    if args.timeout <= 0 or args.stable_seconds <= 0:
        parser.error("--timeout and --stable-seconds must be positive")

    try:
        if args.interactive and args.display == "none":
            parser.error("--interactive requires a visible --display backend")
        run_qemu(
            args.ovmf_code,
            args.timeout,
            args.stable_seconds,
            args.display,
            args.interactive,
            args.verify_input,
            args.runtime_selftest,
        )
    except KeyboardInterrupt:
        return 130
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"QEMU boot test failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
