import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from fat32_image import DATA_START, SECTOR_SIZE, create_fat32_image


class Fat32ImageTests(unittest.TestCase):
    def test_image_contains_long_named_file_on_a_partitionless_fat32_volume(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            nested = source / "EFI" / "MicaNT" / "System32"
            nested.mkdir(parents=True)
            expected = b"bounded image fixture"
            (nested / "ntoskrnl.exe").write_bytes(expected)
            image_path = root / "disk.img"
            create_fat32_image(source, image_path)

            image = image_path.read_bytes()
            self.assertEqual(int.from_bytes(image[11:13], "little"), SECTOR_SIZE)
            self.assertEqual(image[16], 2)
            self.assertEqual(image[82:90], b"FAT32   ")
            self.assertEqual(image[446:510], bytes(64))
            self.assertEqual(image[510:512], b"\x55\xAA")

            def read_fat(cluster):
                offset = SECTOR_SIZE * 32 + cluster * 4
                return int.from_bytes(image[offset:offset + 4], "little") & 0x0FFFFFFF

            def directory(cluster):
                offset = (DATA_START + cluster - 2) * SECTOR_SIZE
                result = bytearray()
                while True:
                    result.extend(image[offset:offset + SECTOR_SIZE])
                    following = read_fat(cluster)
                    if following >= 0x0FFFFFF8:
                        return bytes(result)
                    cluster = following
                    offset = (DATA_START + cluster - 2) * SECTOR_SIZE

            def find(parent, name):
                contents = directory(parent)
                pending = []
                for offset in range(0, len(contents), 32):
                    entry = contents[offset:offset + 32]
                    if not entry or entry[0] == 0:
                        break
                    if entry[0] == 0xE5:
                        pending.clear()
                    elif entry[11] == 0x0F:
                        pending.append(entry)
                    else:
                        if pending:
                            characters = []
                            for long_entry in reversed(pending):
                                for index in (1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30):
                                    character = int.from_bytes(long_entry[index:index + 2], "little")
                                    if character in (0, 0xFFFF):
                                        break
                                    characters.append(chr(character))
                            if "".join(characters).lower() == name.lower():
                                return (
                                    (int.from_bytes(entry[20:22], "little") << 16)
                                    | int.from_bytes(entry[26:28], "little"),
                                    int.from_bytes(entry[28:32], "little"),
                                )
                        pending.clear()
                        short_name = entry[:11].decode("ascii").rstrip()
                        if short_name.lower() == name.lower():
                            return (
                                (int.from_bytes(entry[20:22], "little") << 16)
                                | int.from_bytes(entry[26:28], "little"),
                                int.from_bytes(entry[28:32], "little"),
                            )
                self.fail(f"directory entry not found: {name}")

            root_cluster = int.from_bytes(image[44:48], "little")
            efi_cluster, _ = find(root_cluster, "EFI")
            app_cluster, _ = find(efi_cluster, "MicaNT")
            system_cluster, _ = find(app_cluster, "System32")
            file_cluster, file_size = find(system_cluster, "ntoskrnl.exe")
            self.assertEqual(file_size, len(expected))
            file_offset = (DATA_START + file_cluster - 2) * SECTOR_SIZE
            self.assertEqual(image[file_offset:file_offset + file_size], expected)


if __name__ == "__main__":
    unittest.main()
