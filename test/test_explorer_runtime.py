import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

from verify_explorer_runtime import PeSymbols, verify_runtime
from verify_kernel_architecture import inspect_pe


def image(
    path: Path,
    imported: tuple[str, str | int] | None = None,
    exported: str | None = None,
    forwarder: str | None = None,
) -> bytearray:
    data = bytearray(0x1200)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, 0x84, 0x8664, 1, 0, 0, 0, 0xF0, 0x22)
    optional = 0x98
    struct.pack_into("<H", data, optional, 0x20B)
    struct.pack_into("<I", data, optional + 16, 0x1000)
    struct.pack_into("<II", data, optional + 56, 0x2000, 0x200)
    struct.pack_into("<H", data, optional + 68, 2)
    struct.pack_into("<I", data, optional + 108, 16)
    section = optional + 0xF0
    data[section:section + 8] = b".text\0\0\0"
    struct.pack_into("<IIII", data, section + 8, 0x1000, 0x1000, 0x1000, 0x200)
    struct.pack_into("<I", data, section + 36, 0x60000020)

    def offset(rva: int) -> int:
        return rva - 0x1000 + 0x200

    def string(rva: int, value: str) -> None:
        encoded = value.encode("ascii") + b"\0"
        data[offset(rva):offset(rva) + len(encoded)] = encoded

    if exported:
        struct.pack_into("<II", data, optional + 112, 0x1100, 0x100)
        struct.pack_into("<IIHHIIIIIII", data, offset(0x1100),
                         0, 0, 0, 0, 0, 1, 1, 1, 0x1140, 0x1150, 0x1160)
        struct.pack_into("<I", data, offset(0x1140), 0x1180 if forwarder else 0x1000)
        struct.pack_into("<I", data, offset(0x1150), 0x1200)
        struct.pack_into("<H", data, offset(0x1160), 0)
        string(0x1200, exported)
        if forwarder:
            string(0x1180, forwarder)
    if imported:
        dll, symbol = imported
        struct.pack_into("<II", data, optional + 120, 0x1300, 40)
        struct.pack_into("<IIIII", data, offset(0x1300), 0x1380, 0, 0, 0x1340, 0x13C0)
        string(0x1340, dll)
        value = (1 << 63) | symbol if isinstance(symbol, int) else 0x1400
        struct.pack_into("<Q", data, offset(0x1380), value)
        struct.pack_into("<Q", data, offset(0x13C0), value)
        if isinstance(symbol, str):
            string(0x1402, symbol)
    path.write_bytes(data)
    return data


class ExplorerRuntimeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.exe = self.root / "explorer.exe"
        self.system32 = self.root / "system32"
        self.system32.mkdir()

    def test_transitive_imports_and_case_insensitive_modules(self) -> None:
        image(self.exe, ("USER32.dll", "Window"))
        image(self.system32 / "User32.dll", ("win32k.dll", "Create"), "Window")
        image(self.system32 / "win32k.dll", exported="Create")
        self.assertEqual(verify_runtime(self.exe, self.system32), [])

    def test_missing_export_fails(self) -> None:
        image(self.exe, ("user32.dll", "Missing"))
        image(self.system32 / "user32.dll", exported="Window")
        self.assertEqual(verify_runtime(self.exe, self.system32),
                         ["Missing export: user32.dll!Missing"])

    def test_missing_module_does_not_use_host_dll(self) -> None:
        image(self.exe, ("KERNEL32.dll", "Sleep"))
        errors = verify_runtime(self.exe, self.system32)
        self.assertEqual(errors, ["Missing DLL: kernel32.dll (required by kernel32.dll!Sleep)"])

    def test_crt_api_set_and_ordinal_import(self) -> None:
        image(self.exe, ("api-ms-win-crt-heap-l1-1-0.dll", 1))
        image(self.system32 / "ucrtbase.dll", exported="malloc")
        self.assertEqual(verify_runtime(self.exe, self.system32), [])

    def test_forwarded_export(self) -> None:
        image(self.exe, ("user32.dll", "Window"))
        image(self.system32 / "user32.dll", exported="Window", forwarder="win32k.#1")
        image(self.system32 / "win32k.dll", exported="Create")
        self.assertEqual(verify_runtime(self.exe, self.system32), [])

    def test_forwarder_cycle_fails(self) -> None:
        image(self.exe, ("user32.dll", "Window"))
        image(self.system32 / "user32.dll", exported="Window", forwarder="user32.Window")
        self.assertEqual(verify_runtime(self.exe, self.system32),
                         ["Forwarded export cycle: user32.dll!Window"])

    def test_truncated_section_table(self) -> None:
        image(self.exe)
        self.exe.write_bytes(self.exe.read_bytes()[:0x190])
        with self.assertRaisesRegex(RuntimeError, "truncated PE section table"):
            PeSymbols(self.exe)

    def test_unterminated_import_directory(self) -> None:
        data = image(self.exe, ("user32.dll", "Window"))
        struct.pack_into("<I", data, 0x98 + 124, 20)
        self.exe.write_bytes(data)
        with self.assertRaisesRegex(RuntimeError, "unterminated import descriptor"):
            PeSymbols(self.exe)

    def test_unbacked_import_name(self) -> None:
        data = image(self.exe, ("user32.dll", "Window"))
        struct.pack_into("<I", data, 0x200 + 0x300 + 12, 0x2000)
        self.exe.write_bytes(data)
        with self.assertRaisesRegex(RuntimeError, "not file-backed"):
            PeSymbols(self.exe)

    def test_data_directory_bounds(self) -> None:
        data = image(self.exe)
        struct.pack_into("<I", data, 0x98 + 108, 17)
        self.exe.write_bytes(data)
        with self.assertRaisesRegex(RuntimeError, "data directories"):
            PeSymbols(self.exe)

    def test_export_target_outside_sections(self) -> None:
        data = image(self.exe, exported="Window")
        struct.pack_into("<I", data, 0x200 + 0x140, 0x80000000)
        self.exe.write_bytes(data)
        with self.assertRaisesRegex(RuntimeError, "outside the image"):
            PeSymbols(self.exe)

    def test_imports_still_rejected_for_freestanding_artifacts(self) -> None:
        image(self.exe, ("user32.dll", "Window"))
        with self.assertRaisesRegex(RuntimeError, "external import directory"):
            inspect_pe(self.exe, "WINDOWS_GUI")


if __name__ == "__main__":
    unittest.main()
