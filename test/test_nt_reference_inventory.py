import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "nt_reference_inventory.py"


class NtReferenceInventoryTests(unittest.TestCase):
    def test_inventory_reports_functions_types_and_tests(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "ntoskrnl" / "ke" / "dispatcher.c"
            source.parent.mkdir(parents=True)
            source.write_text(
                "typedef struct _KTHREAD KTHREAD;\n"
                "NTSTATUS KeWaitForSingleObject(void) {\n"
                "    return STATUS_SUCCESS;\n"
                "}\n",
                encoding="utf-8",
            )
            test = source.parent / "tests" / "dispatcher_test.c"
            test.parent.mkdir()
            test.write_text("void test_wait(void) {}\n", encoding="utf-8")

            completed = subprocess.run(
                [sys.executable, str(SCRIPT), str(root)],
                check=True,
                capture_output=True,
                text=True,
            )
            report = json.loads(completed.stdout)

        area = report["areas"]["ke"]
        self.assertEqual(area["source_file_count"], 2)
        self.assertIn("ntoskrnl/ke/dispatcher.c", area["source_files"])
        self.assertIn("KeWaitForSingleObject", area["candidate_functions"])
        self.assertIn("_KTHREAD", area["candidate_types"])
        self.assertIn("KTHREAD", area["candidate_types"])
        self.assertEqual(area["test_paths"], ["ntoskrnl/ke/tests/dispatcher_test.c"])

    def test_nonexistent_reference_root_is_rejected(self):
        completed = subprocess.run(
            [sys.executable, str(SCRIPT), "/path/that/does/not/exist"],
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("not a directory", completed.stderr)


if __name__ == "__main__":
    unittest.main()
