"""Create a small deterministic FAT32 disk image from a directory tree."""

import math
import os
from pathlib import Path


SECTOR_SIZE = 512
SECTORS_PER_CLUSTER = 1
TOTAL_SECTORS = 131072
RESERVED_SECTORS = 32
FAT_COUNT = 2
FAT_SECTORS = 1024
DATA_START = RESERVED_SECTORS + FAT_COUNT * FAT_SECTORS


def _short_name(name: str, used: set[bytes]) -> bytes:
    stem, dot, extension = name.rpartition(".")
    if not dot:
        stem, extension = name, ""
    allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$~!#%&-{}()@'`"
    clean_stem = "".join(
        character for character in stem.upper() if character in allowed
    ) or "_"
    clean_extension = "".join(
        character for character in extension.upper() if character in allowed
    )[:3]
    direct = (clean_stem[:8].ljust(8) + clean_extension.ljust(3)).encode("ascii")
    if len(clean_stem) <= 8 and len(extension) <= 3 and direct not in used:
        candidate = direct
    else:
        suffix = 1
        while True:
            tail = f"~{suffix}"
            candidate = (
                clean_stem[: 8 - len(tail)] + tail
            ).ljust(8)
            candidate = (candidate + clean_extension.ljust(3)).encode("ascii")
            if candidate not in used:
                break
            suffix += 1
    used.add(candidate)
    return candidate


def _checksum(short_name: bytes) -> int:
    value = 0
    for byte in short_name:
        value = (((value & 1) << 7) + (value >> 1) + byte) & 0xFF
    return value


def _directory_entries(name: str, short_name: bytes, attributes: int,
                       first_cluster: int, size: int) -> list[bytes]:
    records: list[bytes] = []
    if name.upper() != name or len(name) > 12 or "." in name[:-1]:
        encoded = name.encode("utf-16le")
        units = [encoded[i] | encoded[i + 1] << 8 for i in range(0, len(encoded), 2)]
        if len(units) > 247:
            raise ValueError(f"FAT32 path component is too long: {name}")
        units.append(0)
        units.extend([0xFFFF] * ((13 - len(units) % 13) % 13))
        chunks = [units[i:i + 13] for i in range(0, len(units), 13)]
        offsets = (1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30)
        checksum = _checksum(short_name)
        for index in range(len(chunks), 0, -1):
            entry = bytearray(32)
            entry[0] = index | (0x40 if index == len(chunks) else 0)
            entry[11] = 0x0F
            entry[13] = checksum
            for offset, unit in zip(offsets, chunks[index - 1]):
                entry[offset:offset + 2] = unit.to_bytes(2, "little")
            records.append(bytes(entry))

    entry = bytearray(32)
    entry[:11] = short_name
    entry[11] = attributes
    entry[20:22] = ((first_cluster >> 16) & 0xFFFF).to_bytes(2, "little")
    entry[26:28] = (first_cluster & 0xFFFF).to_bytes(2, "little")
    entry[28:32] = size.to_bytes(4, "little")
    records.append(bytes(entry))
    return records


def create_fat32_image(source: Path, destination: Path) -> None:
    """Write a whole-disk, partitionless FAT32 image containing source."""
    if not source.is_dir():
        raise ValueError(f"FAT32 source is not a directory: {source}")
    if destination.exists():
        raise FileExistsError(destination)

    image = bytearray(TOTAL_SECTORS * SECTOR_SIZE)
    fat = [0] * (FAT_SECTORS * SECTOR_SIZE // 4)
    fat[0], fat[1], fat[2] = 0x0FFFFFF8, 0x0FFFFFFF, 0x0FFFFFFF
    next_cluster = 3

    def allocate(data: bytes) -> tuple[int, int]:
        nonlocal next_cluster
        count = math.ceil(len(data) / SECTOR_SIZE) if data else 0
        if count == 0:
            return 0, 0
        first = next_cluster
        for cluster in range(first, first + count):
            if cluster >= len(fat) or cluster - 2 >= TOTAL_SECTORS - DATA_START:
                raise ValueError("FAT32 image capacity exceeded")
            fat[cluster] = cluster + 1 if cluster + 1 < first + count else 0x0FFFFFFF
        next_cluster += count
        for index in range(count):
            offset = DATA_START * SECTOR_SIZE + (first + index - 2) * SECTOR_SIZE
            chunk = data[index * SECTOR_SIZE:(index + 1) * SECTOR_SIZE]
            image[offset:offset + len(chunk)] = chunk
        return first, len(data)

    def build_directory(path: Path, cluster: int, parent_cluster: int) -> None:
        entries = [
            _directory_entries(".", b".          ", 0x10, cluster, 0)[-1],
            _directory_entries("..", b"..         ", 0x10, parent_cluster, 0)[-1],
        ]
        used: set[bytes] = {b".          ", b"..         "}
        children = sorted(path.iterdir(), key=lambda item: item.name.lower())
        pending: list[tuple[Path, bytes, int, int, bool]] = []
        for child in children:
            short = _short_name(child.name, used)
            if child.is_dir():
                child_cluster = next_cluster
                allocate(b"\0" * SECTOR_SIZE)
                pending.append((child, short, child_cluster, 0, True))
            elif child.is_file():
                data = child.read_bytes()
                first, size = allocate(data)
                pending.append((child, short, first, size, False))
        for child, short, child_cluster, size, is_directory in pending:
            entries.extend(_directory_entries(
                child.name, short, 0x10 if is_directory else 0x20,
                child_cluster, size,
            ))
        entries.append(bytes(32))
        directory_data = b"".join(entries)
        required_clusters = max(1, math.ceil(len(directory_data) / SECTOR_SIZE))
        if required_clusters > 1:
            extra_first, _ = allocate(b"\0" * ((required_clusters - 1) * SECTOR_SIZE))
            fat[cluster] = extra_first
            chain = list(range(extra_first, extra_first + required_clusters - 1))
        else:
            chain = []
        chain.insert(0, cluster)
        for index, current_cluster in enumerate(chain):
            chunk = directory_data[index * SECTOR_SIZE:(index + 1) * SECTOR_SIZE]
            offset = DATA_START * SECTOR_SIZE + (current_cluster - 2) * SECTOR_SIZE
            image[offset:offset + len(chunk)] = chunk
        for child, _, child_cluster, _, is_directory in pending:
            if is_directory:
                build_directory(child, child_cluster, cluster)

    root_cluster = 2
    build_directory(source, root_cluster, root_cluster)

    boot = bytearray(SECTOR_SIZE)
    boot[0:3] = b"\xEB\x58\x90"
    boot[3:11] = b"MICANT  "
    boot[11:13] = SECTOR_SIZE.to_bytes(2, "little")
    boot[13] = SECTORS_PER_CLUSTER
    boot[14:16] = RESERVED_SECTORS.to_bytes(2, "little")
    boot[16] = FAT_COUNT
    boot[32:36] = TOTAL_SECTORS.to_bytes(4, "little")
    boot[36:40] = FAT_SECTORS.to_bytes(4, "little")
    boot[44:48] = root_cluster.to_bytes(4, "little")
    boot[48:50] = (1).to_bytes(2, "little")
    boot[50:52] = (6).to_bytes(2, "little")
    boot[64] = 0x80
    boot[66] = 0x29
    boot[67:71] = b"MICA"
    boot[71:82] = b"MICANT TEST"
    boot[82:90] = b"FAT32   "
    boot[510:512] = b"\x55\xAA"
    image[:SECTOR_SIZE] = boot
    fsinfo = bytearray(SECTOR_SIZE)
    fsinfo[0:4] = b"RRaA"
    fsinfo[484:488] = b"rrAa"
    fsinfo[488:492] = (len(fat) - next_cluster).to_bytes(4, "little")
    fsinfo[492:496] = next_cluster.to_bytes(4, "little")
    fsinfo[508:512] = b"\x00\x00\x55\xAA"
    image[SECTOR_SIZE:2 * SECTOR_SIZE] = fsinfo
    for copy in range(FAT_COUNT):
        offset = (RESERVED_SECTORS + copy * FAT_SECTORS) * SECTOR_SIZE
        for index, value in enumerate(fat):
            start = offset + index * 4
            image[start:start + 4] = value.to_bytes(4, "little")

    with destination.open("xb") as output:
        output.write(image)
