import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "scripts"))

import boot_micant
import qemu
from verify_kernel_architecture import validate_build_manifest


class RuntimeRunnerTests(unittest.TestCase):
    def test_mode_specific_ordered_milestones(self):
        for expected in (
            boot_micant.EXPECTED_SERIAL,
            boot_micant.EXPECTED_RUNTIME_SELFTEST_SERIAL,
            boot_micant.EXPECTED_TIMER_SELFTEST_SERIAL,
        ):
            log = "\n".join(expected)
            self.assertEqual(boot_micant.serial_progress(log, expected), len(expected))
            self.assertEqual(
                boot_micant.serial_progress("\n".join(reversed(expected)), expected), 1
            )
        self.assertLess(
            boot_micant.serial_progress(
                "\n".join(boot_micant.EXPECTED_SERIAL),
                boot_micant.EXPECTED_RUNTIME_SELFTEST_SERIAL,
            ),
            len(boot_micant.EXPECTED_RUNTIME_SELFTEST_SERIAL),
        )
        self.assertLess(
            boot_micant.serial_progress(
                "\n".join(boot_micant.EXPECTED_RUNTIME_SELFTEST_SERIAL)
            ),
            len(boot_micant.EXPECTED_SERIAL),
        )
        self.assertLess(
            boot_micant.serial_progress(
                "\n".join(boot_micant.EXPECTED_TIMER_SELFTEST_SERIAL)
            ),
            len(boot_micant.EXPECTED_SERIAL),
        )

    def test_runtime_fatal_never_counts_as_success(self):
        for line in (
            "[MicaNT Hal] FATAL: trap",
            "ExHeap FATAL: invalid free",
            "[MicaNT Ex] FATAL: constructor state",
            "[MicaNT Ob] Handle verification failed",
        ):
            self.assertTrue(boot_micant.serial_has_failure(line))
        self.assertFalse(
            boot_micant.serial_has_failure("ExHeap: allocation rejected (overflow)")
        )

    def test_runtime_mode_rejects_input_or_interactive_runner(self):
        for interactive, verify_input in ((True, False), (False, True)):
            with self.assertRaisesRegex(RuntimeError, "halts before desktop"):
                boot_micant.run_qemu(
                    Path("unused"), 1, 1, "none", interactive, verify_input, True
                )

    def test_timer_mode_rejects_input_or_interactive_runner(self):
        for interactive, verify_input in ((True, False), (False, True)):
            with self.assertRaisesRegex(RuntimeError, "halts before process tests"):
                boot_micant.run_qemu(
                    Path("unused"), 1, 1, "none", interactive, verify_input,
                    timer_selftest=True,
                )

    def test_manifest_mode_must_be_boolean_and_match(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            kernel = root / "build/baremetal/ntoskrnl.exe"
            loader = root / "build/esp/EFI/BOOT/BOOTX64.EFI"
            kernel.parent.mkdir(parents=True)
            loader.parent.mkdir(parents=True)
            kernel.touch()
            loader.touch()
            manifest = kernel.parent / "kernel-build-manifest.json"
            for recorded, requested in ((True, False), (False, True), (1, True)):
                manifest.write_text(json.dumps({
                    "schema_version": 2, "runtime_selftest": recorded,
                }), encoding="utf-8")
                with self.assertRaisesRegex(RuntimeError, "requested runtime self-test mode"):
                    validate_build_manifest(root, allow_dirty=True, runtime_selftest=requested)
            manifest.write_text(json.dumps({
                "schema_version": 2, "timer_selftest": True,
            }), encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "requested timer self-test mode"):
                validate_build_manifest(root, allow_dirty=True)
            manifest.write_text(json.dumps({
                "schema_version": 2, "runtime_selftest": True,
            }), encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "requested runtime self-test mode"):
                validate_build_manifest(root, allow_dirty=True)
            manifest.write_text('{"schema_version": 2}', encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "has no build_id"):
                validate_build_manifest(root, allow_dirty=True, runtime_selftest=False)

    def check_root_mode(self, runtime_selftest, timer_selftest=False):
        arguments = ["qemu.py", "--headless", "--diagnostics-dir", "/tmp/micant-diagnostics"]
        if runtime_selftest:
            arguments.append("--runtime-selftest")
        if timer_selftest:
            arguments.append("--timer-selftest")
        with (
            mock.patch.object(sys, "argv", arguments),
            mock.patch.object(qemu.shutil, "which", return_value="/tools/tool"),
            mock.patch.object(qemu, "find_compiler", return_value=Path("/tools/clang++")),
            mock.patch.object(qemu, "find_c_compiler", return_value=Path("/tools/clang")),
            mock.patch.object(qemu, "find_qemu", return_value="/tools/qemu"),
            mock.patch.object(qemu, "find_ovmf", return_value=Path("/tools/ovmf.fd")),
            mock.patch.object(qemu, "write_manifest") as manifest,
            mock.patch.object(qemu, "validate_build_manifest") as validate,
            mock.patch.object(qemu, "run") as run,
        ):
            self.assertEqual(qemu.main(), 0)
            commands = [call.args[0] for call in run.call_args_list]
            self.assertIn(
                f"-DMICANT_BOOTSTRAP_RUNTIME_SELFTEST:BOOL={'ON' if runtime_selftest else 'OFF'}",
                commands[0],
            )
            self.assertIn(
                f"-DMICANT_BOOTSTRAP_TIMER_SELFTEST:BOOL={'ON' if timer_selftest else 'OFF'}",
                commands[0],
            )
            manifest.assert_called_once_with(
                Path("/tools/clang++"),
                runtime_selftest=runtime_selftest,
                timer_selftest=timer_selftest,
            )
            validate.assert_called_once_with(
                qemu.ROOT,
                allow_dirty=True,
                runtime_selftest=runtime_selftest,
                timer_selftest=timer_selftest,
            )
            self.assertEqual("--runtime-selftest" in commands[-1], runtime_selftest)
            self.assertEqual("--timer-selftest" in commands[-1], timer_selftest)
            self.assertEqual(
                "--verify-input" in commands[-1],
                not runtime_selftest and not timer_selftest,
            )
            self.assertNotIn("--interactive", commands[-1])
            self.assertEqual(
                commands[-1][commands[-1].index("--diagnostics-dir") + 1],
                str(Path("/tmp/micant-diagnostics").resolve()),
            )

    def test_diagnostics_survive_kernel_failure_with_matching_artifacts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            kernel = root / "build/baremetal/ntoskrnl.exe"
            loader = root / "build/esp/EFI/BOOT/BOOTX64.EFI"
            kernel.parent.mkdir(parents=True)
            loader.parent.mkdir(parents=True)
            kernel.write_bytes(b"exact-kernel")
            loader.write_bytes(b"loader")
            manifest = kernel.parent / "kernel-build-manifest.json"
            manifest.write_bytes(b'{"build_id":"diagnostic-test"}')
            (root / "build/virtio_gpu.sys").write_bytes(b"driver")
            firmware = root / "ovmf.fd"
            firmware.touch()
            diagnostics = root / "diagnostics"
            failure = "[MicaNT Ps] Two user threads verification failed"
            process = mock.Mock()
            process.poll.return_value = None

            def launch(command, **kwargs):
                serial = Path(command[command.index("-serial") + 1].removeprefix("file:"))
                serial.write_text(failure, encoding="utf-8")
                return process

            with (
                mock.patch.object(boot_micant, "ROOT", root),
                mock.patch.object(boot_micant, "validate_build_manifest"),
                mock.patch.object(boot_micant.shutil, "which", return_value="/tools/qemu"),
                mock.patch.object(boot_micant.subprocess, "Popen", side_effect=launch),
                self.assertRaisesRegex(RuntimeError, "Kernel bootstrap failed"),
            ):
                boot_micant.run_qemu(firmware, 1, 1, "none", False, False,
                                     diagnostics_dir=diagnostics)
            self.assertEqual((diagnostics / "serial.log").read_text(), failure)
            self.assertEqual((diagnostics / kernel.name).read_bytes(), kernel.read_bytes())
            self.assertEqual((diagnostics / manifest.name).read_bytes(), manifest.read_bytes())
            process.terminate.assert_called_once()
            self.assertTrue((diagnostics / "qemu.log").exists())

    def test_root_selftest_builds_requested_mode(self):
        self.check_root_mode(True)

    def test_normal_boot_explicitly_disables_selftest(self):
        self.check_root_mode(False)

    def test_root_timer_selftest_builds_requested_mode(self):
        self.check_root_mode(False, True)

    def test_runtime_selftest_cannot_be_an_explorer_preflight(self):
        with (
            mock.patch.object(sys, "argv", ["qemu.py", "--runtime-selftest", "--check-explorer"]),
            contextlib.redirect_stderr(io.StringIO()),
            self.assertRaises(SystemExit) as result,
        ):
            qemu.main()
        self.assertEqual(result.exception.code, 2)

    def test_timer_selftest_cannot_be_an_explorer_preflight(self):
        with (
            mock.patch.object(sys, "argv", ["qemu.py", "--timer-selftest", "--check-explorer"]),
            contextlib.redirect_stderr(io.StringIO()),
            self.assertRaises(SystemExit) as result,
        ):
            qemu.main()
        self.assertEqual(result.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
