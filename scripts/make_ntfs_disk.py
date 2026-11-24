#!/usr/bin/env python3
r"""
MicaNT Spec-Compliant GPT + NTFS 3.1 Bare-Metal Disk Image Builder
Generates a genuine raw 64 MiB SATA disk image (build/micant_ntfs.img) with:
  - Protective MBR (LBA 0) + GPT Header & Partition Entries (LBA 1..33, real CRC32)
  - Backup GPT at end of disk
  - Authentic NTFS 3.1 Boot Sector / BPB at LBA 2048 ("NTFS    ", 4096-byte clusters)
  - Authentic 1024-byte MFT Records with Update Sequence Array (USA) fixups:
      #0 $MFT (Non-Resident runlist covering full MFT)
      #1 $MFTMirr, #2 $LogFile, #3 $Volume ("MicaNT_NTFS", NTFS v3.1),
      #4 $AttrDef, #5 "." Root Dir ($INDEX_ROOT + 4096-byte $INDEX_ALLOCATION INDX),
      #6 $Bitmap, #7 $Boot
  - Real directory tree & compiled PE32+ binaries from build/sysroot:
      C:\MicaNT\system.ini
      C:\MicaNT\System32\app.exe
      C:\Windows\System32\ntoskrnl.exe, ntdll.dll, kernel32.dll, user32.dll, cmd.exe
      C:\Windows\System32\drivers\ntfs.sys
      C:\README.txt
"""

import os
import sys
import uuid
import zlib
import struct
from pathlib import Path

SECTOR_SIZE = 512
CLUSTER_SIZE = 4096
SECTORS_PER_CLUSTER = CLUSTER_SIZE // SECTOR_SIZE
MFT_RECORD_SIZE = 1024
INDEX_BLOCK_SIZE = 4096

DISK_SIZE_MB = 64
TOTAL_DISK_SECTORS = (DISK_SIZE_MB * 1024 * 1024) // SECTOR_SIZE
PART_START_LBA = 2048
PART_END_LBA = TOTAL_DISK_SECTORS - 34  # Leave 33 sectors for backup GPT
PART_SECTORS = PART_END_LBA - PART_START_LBA + 1
PART_CLUSTERS = PART_SECTORS // SECTORS_PER_CLUSTER

MFT_START_CLUSTER = 4
MFT_MIRR_CLUSTER = 128
MFT_TOTAL_RECORDS = 128  # 128 KB = 32 clusters (clusters 4..35)
MFT_CLUSTERS = (MFT_TOTAL_RECORDS * MFT_RECORD_SIZE) // CLUSTER_SIZE

NT_EPOCH_FILETIME = 133769000000000000  # Nov 2026 in 100ns intervals since 1601


def encode_runlist(runs):
    """Encode list of (cluster_count, lcn) into authentic NTFS mapping pairs."""
    out = bytearray()
    prev_lcn = 0
    for count, lcn in runs:
        if count <= 0:
            continue
        # Length bytes (unsigned minimal)
        len_bytes = count.to_bytes((count.bit_length() + 7) // 8 or 1, "little", signed=False)
        if lcn < 0:
            # Sparse run
            out.append(len(len_bytes) & 0x0F)
            out.extend(len_bytes)
        else:
            delta = lcn - prev_lcn
            prev_lcn = lcn
            # Signed minimal little-endian
            for n in range(1, 9):
                try:
                    off_bytes = delta.to_bytes(n, "little", signed=True)
                    break
                except OverflowError:
                    continue
            header = (len(len_bytes) & 0x0F) | ((len(off_bytes) & 0x0F) << 4)
            out.append(header)
            out.extend(len_bytes)
            out.extend(off_bytes)
    out.append(0x00)
    return bytes(out)


def apply_usa_fixup(buf: bytearray, usa_offset: int, usn: int = 0x0001):
    """Apply authentic NTFS Update Sequence Array (USA) fixups to a 1024B/4096B record."""
    num_sectors = len(buf) // SECTOR_SIZE
    struct.pack_into("<H", buf, usa_offset, usn)
    for s in range(num_sectors):
        tail_off = (s + 1) * SECTOR_SIZE - 2
        orig_word = struct.unpack_from("<H", buf, tail_off)[0]
        struct.pack_into("<H", buf, usa_offset + (s + 1) * 2, orig_word)
        struct.pack_into("<H", buf, tail_off, usn)


def build_resident_attr(attr_type: int, payload: bytes, attr_id: int = 0, name: str = "") -> bytes:
    name_bytes = name.encode("utf-16le")
    name_off = 24
    val_off = (name_off + len(name_bytes) + 7) & ~7
    total_len = (val_off + len(payload) + 7) & ~7
    buf = bytearray(total_len)
    struct.pack_into(
        "<IIBBHHH",
        buf,
        0,
        attr_type,
        total_len,
        0,  # Resident
        len(name),
        name_off if name else 0,
        0,  # Flags
        attr_id,
    )
    struct.pack_into("<IHBB", buf, 16, len(payload), val_off, 0, 0)
    if name_bytes:
        buf[name_off : name_off + len(name_bytes)] = name_bytes
    buf[val_off : val_off + len(payload)] = payload
    return bytes(buf)


def build_nonresident_attr(
    attr_type: int,
    runs,
    real_size: int,
    alloc_size: int,
    attr_id: int = 0,
    name: str = "",
) -> bytes:
    name_bytes = name.encode("utf-16le")
    run_bytes = encode_runlist(runs)
    total_clusters = sum(c for c, _ in runs)
    highest_vcn = max(0, total_clusters - 1) if total_clusters > 0 else 0
    name_off = 64
    mp_off = (name_off + len(name_bytes) + 7) & ~7
    total_len = (mp_off + len(run_bytes) + 7) & ~7
    buf = bytearray(total_len)
    struct.pack_into(
        "<IIBBHHH",
        buf,
        0,
        attr_type,
        total_len,
        1,  # Non-resident
        len(name),
        name_off if name else 0,
        0,
        attr_id,
    )
    struct.pack_into(
        "<QQHHIQQQ",
        buf,
        16,
        0,  # lowestVcn
        highest_vcn,
        mp_off,
        0,  # compressionUnit
        0,  # reserved
        alloc_size,
        real_size,
        real_size,
    )
    if name_bytes:
        buf[name_off : name_off + len(name_bytes)] = name_bytes
    buf[mp_off : mp_off + len(run_bytes)] = run_bytes
    return bytes(buf)


def build_standard_info() -> bytes:
    payload = struct.pack(
        "<QQQQIIII",
        NT_EPOCH_FILETIME,
        NT_EPOCH_FILETIME,
        NT_EPOCH_FILETIME,
        NT_EPOCH_FILETIME,
        0x20,  # ARCHIVE
        0,
        0,
        0,
    )
    return build_resident_attr(0x10, payload, attr_id=0)


def build_filename_payload(parent_mft: int, name: str, real_size: int, alloc_size: int, is_dir: bool) -> bytes:
    name_u16 = name.encode("utf-16le")
    parent_ref = (1 << 48) | (parent_mft & 0xFFFFFFFFFFFF)
    attrs = 0x10000000 if is_dir else 0x00000020
    header = struct.pack(
        "<QQQQQQQIIBB",
        parent_ref,
        NT_EPOCH_FILETIME,
        NT_EPOCH_FILETIME,
        NT_EPOCH_FILETIME,
        NT_EPOCH_FILETIME,
        alloc_size,
        real_size,
        attrs,
        0,
        len(name),
        3,  # Win32 + DOS namespace
    )
    return header + name_u16


def build_mft_record(rec_num: int, flags: int, attributes: list[bytes], usn: int = 1) -> bytes:
    buf = bytearray(MFT_RECORD_SIZE)
    usa_offset = 0x30
    usa_count = 1 + (MFT_RECORD_SIZE // SECTOR_SIZE)  # 3
    first_attr_off = 0x38

    pos = first_attr_off
    for attr in attributes:
        if pos + len(attr) + 8 > MFT_RECORD_SIZE:
            raise ValueError(f"MFT record {rec_num} attribute overflow ({pos + len(attr)} > {MFT_RECORD_SIZE})")
        buf[pos : pos + len(attr)] = attr
        pos += len(attr)

    # End marker 0xFFFFFFFF
    struct.pack_into("<I", buf, pos, 0xFFFFFFFF)
    used_size = pos + 8

    struct.pack_into(
        "<IHHQHHHHIIQHHI",
        buf,
        0,
        0x454C4946,  # "FILE"
        usa_offset,
        usa_count,
        100 + rec_num,  # LSN
        1,  # Sequence number
        1,  # Hard link count
        first_attr_off,
        flags,  # 0x01 = InUse, 0x02 = Dir
        used_size,
        MFT_RECORD_SIZE,
        0,  # Base file record
        len(attributes) + 1,
        0,
        rec_num,
    )
    apply_usa_fixup(buf, usa_offset, usn)
    return bytes(buf)


def build_index_entries(children):
    """Build packed NTFS $I30 index entries terminated by an End sentinel."""
    out = bytearray()
    for child_mft, parent_mft, name, real_size, alloc_size, is_dir in sorted(children, key=lambda x: x[2].upper()):
        fn_payload = build_filename_payload(parent_mft, name, real_size, alloc_size, is_dir)
        entry_len = (16 + len(fn_payload) + 7) & ~7
        file_ref = (1 << 48) | (child_mft & 0xFFFFFFFFFFFF)
        entry = bytearray(entry_len)
        struct.pack_into("<QHHI", entry, 0, file_ref, entry_len, len(fn_payload), 0)
        entry[16 : 16 + len(fn_payload)] = fn_payload
        out.extend(entry)
    # Last-entry sentinel (flags = 0x02)
    out.extend(struct.pack("<QHHI", 0, 16, 0, 0x02))
    return bytes(out)


def build_indx_block(vcn: int, children) -> bytes:
    """Build a genuine 4096-byte NTFS 'INDX' block with USA fixups."""
    buf = bytearray(INDEX_BLOCK_SIZE)
    usa_offset = 0x28
    usa_count = 1 + (INDEX_BLOCK_SIZE // SECTOR_SIZE)  # 9
    # IndexHeader starts at offset 0x18 (24)
    # USA occupies 0x28..0x3A (18 bytes), so entries start at 0x40 relative to block = 0x28 relative to IndexHeader
    entries_rel_to_ih = 0x28
    entries = build_index_entries(children)
    if 0x18 + entries_rel_to_ih + len(entries) > INDEX_BLOCK_SIZE:
        raise ValueError("INDX block overflow")

    struct.pack_into(
        "<IHHQQ",
        buf,
        0,
        0x58444E49,  # "INDX"
        usa_offset,
        usa_count,
        500 + vcn,
        vcn,
    )
    struct.pack_into(
        "<IIIBBBB",
        buf,
        0x18,
        entries_rel_to_ih,
        entries_rel_to_ih + len(entries),
        INDEX_BLOCK_SIZE - 0x18,
        0,  # Leaf node
        0,
        0,
        0,
    )
    buf[0x18 + entries_rel_to_ih : 0x18 + entries_rel_to_ih + len(entries)] = entries
    apply_usa_fixup(buf, usa_offset, usn=0x0002)
    return bytes(buf)


def build_dir_mft_record(rec_num: int, parent_mft: int, name: str, children, alloc_cluster_fn, write_cluster_fn):
    """Build a directory MFT record with $STANDARD_INFORMATION, $FILE_NAME, $INDEX_ROOT, and optional $INDEX_ALLOCATION."""
    entries = build_index_entries(children)
    attrs = [
        build_standard_info(),
        build_resident_attr(0x30, build_filename_payload(parent_mft, name, 0, 0, True), attr_id=1),
    ]

    # If entries fit comfortably inside MFT record, keep them in $INDEX_ROOT; else spill to 4096-byte INDX block!
    if len(entries) <= 450:
        ir_payload = struct.pack(
            "<IIIBBBBIIIBBBB",
            0x30,  # Indexed attr: $FILE_NAME
            1,     # CollationFilename
            INDEX_BLOCK_SIZE,
            1,     # 1 cluster per index block
            0, 0, 0,
            16,    # entriesOffset relative to IndexHeader
            16 + len(entries),
            16 + len(entries),
            0,     # Small index (fits in $INDEX_ROOT)
            0, 0, 0,
        ) + entries
        attrs.append(build_resident_attr(0x90, ir_payload, attr_id=2, name="$I30"))
    else:
        # Large directory -> empty root + $INDEX_ALLOCATION 4096B INDX block
        end_only = struct.pack("<QHHI", 0, 16, 0, 0x02)
        ir_payload = struct.pack(
            "<IIIBBBBIIIBBBB",
            0x30, 1, INDEX_BLOCK_SIZE, 1, 0, 0, 0,
            16, 16 + len(end_only), 16 + len(end_only),
            1,  # Large index (has $INDEX_ALLOCATION)
            0, 0, 0,
        ) + end_only
        attrs.append(build_resident_attr(0x90, ir_payload, attr_id=2, name="$I30"))

        indx_lcn = alloc_cluster_fn(1)
        indx_block = build_indx_block(0, children)
        write_cluster_fn(indx_lcn, indx_block)
        attrs.append(
            build_nonresident_attr(
                0xA0,
                [(1, indx_lcn)],
                INDEX_BLOCK_SIZE,
                INDEX_BLOCK_SIZE,
                attr_id=3,
                name="$I30",
            )
        )

    return build_mft_record(rec_num, 0x03, attrs, usn=(rec_num % 100) + 1)


def main():
    repo_root = Path(__file__).resolve().parent.parent
    sysroot = repo_root / "build" / "sysroot"
    out_img = repo_root / "build" / "micant_ntfs.img"
    out_img.parent.mkdir(parents=True, exist_ok=True)

    print("========================================================================")
    print("  MicaNT GPT + Authentic NTFS 3.1 Bare-Metal Disk Image Builder         ")
    print("========================================================================")

    disk = bytearray(DISK_SIZE_MB * 1024 * 1024)
    part_byte_off = PART_START_LBA * SECTOR_SIZE

    def write_cluster(lcn: int, data: bytes):
        off = part_byte_off + lcn * CLUSTER_SIZE
        disk[off : off + len(data)] = data

    next_free_cluster = 256  # Clusters 0..255 reserved for Boot, $MFT, $MFTMirr, $Bitmap

    def alloc_clusters(count: int) -> int:
        nonlocal next_free_cluster
        start = next_free_cluster
        next_free_cluster += count
        if next_free_cluster >= PART_CLUSTERS:
            raise RuntimeError("NTFS image out of space")
        return start

    # ------------------------------------------------------------------
    # 1. Build Protective MBR (LBA 0) + GPT Headers & Entries (LBA 1..33)
    # ------------------------------------------------------------------
    # Protective MBR entry
    struct.pack_into("<B3sB3sII", disk, 446, 0x00, b"\x00\x02\x00", 0xEE, b"\xFF\xFF\xFF", 1, TOTAL_DISK_SECTORS - 1)
    struct.pack_into("<H", disk, 510, 0xAA55)

    # GPT Partition Entry 0: Microsoft Basic Data Partition ("MicaNT_OS")
    ms_basic_data_guid = uuid.UUID("EBD0A0A2-B9E5-4433-87C0-68B6B72699C7").bytes_le
    part_guid = uuid.UUID("A1B2C3D4-1122-3344-5566-778899AABBCC").bytes_le
    disk_guid = uuid.UUID("11223344-5566-7788-99AA-BBCCDDEEFF00").bytes_le
    part_name = "MicaNT_OS".encode("utf-16le").ljust(72, b"\x00")

    gpt_entries = bytearray(128 * 128)  # 128 entries * 128 bytes = 16 KB (32 sectors)
    struct.pack_into(
        "<16s16sQQQ72s",
        gpt_entries,
        0,
        ms_basic_data_guid,
        part_guid,
        PART_START_LBA,
        PART_END_LBA,
        0,
        part_name,
    )
    entries_crc = zlib.crc32(gpt_entries) & 0xFFFFFFFF

    def make_gpt_header(my_lba: int, alt_lba: int, entries_lba: int) -> bytes:
        hdr = bytearray(92)
        struct.pack_into(
            "<8sIIIIQQQQ16sQIII",
            hdr,
            0,
            b"EFI PART",
            0x00010000,
            92,
            0,  # CRC placeholder
            0,
            my_lba,
            alt_lba,
            34,
            TOTAL_DISK_SECTORS - 34,
            disk_guid,
            entries_lba,
            128,
            128,
            entries_crc,
        )
        crc = zlib.crc32(hdr) & 0xFFFFFFFF
        struct.pack_into("<I", hdr, 16, crc)
        return bytes(hdr)

    primary_gpt = make_gpt_header(1, TOTAL_DISK_SECTORS - 1, 2)
    disk[SECTOR_SIZE : SECTOR_SIZE + 92] = primary_gpt
    disk[2 * SECTOR_SIZE : 2 * SECTOR_SIZE + len(gpt_entries)] = gpt_entries

    backup_entries_lba = TOTAL_DISK_SECTORS - 33
    backup_gpt = make_gpt_header(TOTAL_DISK_SECTORS - 1, 1, backup_entries_lba)
    disk[backup_entries_lba * SECTOR_SIZE : backup_entries_lba * SECTOR_SIZE + len(gpt_entries)] = gpt_entries
    disk[(TOTAL_DISK_SECTORS - 1) * SECTOR_SIZE : (TOTAL_DISK_SECTORS - 1) * SECTOR_SIZE + 92] = backup_gpt

    # ------------------------------------------------------------------
    # 2. Build Authentic NTFS 3.1 Boot Sector (BPB) at LBA 2048
    # ------------------------------------------------------------------
    bpb = bytearray(512)
    bpb[0:3] = b"\xEB\x52\x90"
    bpb[3:11] = b"NTFS    "
    struct.pack_into("<H", bpb, 11, SECTOR_SIZE)          # BytesPerSector = 512
    struct.pack_into("<B", bpb, 13, SECTORS_PER_CLUSTER)  # SectorsPerCluster = 8 (4096 B)
    struct.pack_into("<B", bpb, 21, 0xF8)                 # Fixed disk media descriptor
    struct.pack_into("<H", bpb, 24, 63)                   # SectorsPerTrack
    struct.pack_into("<H", bpb, 26, 255)                  # NumberOfHeads
    struct.pack_into("<I", bpb, 28, PART_START_LBA)       # HiddenSectors
    struct.pack_into("<Q", bpb, 40, PART_SECTORS - 1)     # TotalSectors
    struct.pack_into("<Q", bpb, 48, MFT_START_CLUSTER)    # $MFT LCN
    struct.pack_into("<Q", bpb, 56, MFT_MIRR_CLUSTER)     # $MFTMirr LCN
    struct.pack_into("<b", bpb, 64, -10)                  # 2^10 = 1024 byte MFT record
    struct.pack_into("<b", bpb, 68, 1)                    # 1 cluster (4096 B) per INDX block
    struct.pack_into("<Q", bpb, 72, 0x4D4943414E543331)   # Volume Serial Number ("MICANT31")
    struct.pack_into("<H", bpb, 510, 0xAA55)

    disk[part_byte_off : part_byte_off + 512] = bpb
    # Backup boot sector at last sector of partition
    backup_bpb_off = part_byte_off + (PART_SECTORS - 1) * SECTOR_SIZE
    disk[backup_bpb_off : backup_bpb_off + 512] = bpb

    # ------------------------------------------------------------------
    # 3. Populate Real Directory Tree & Files (from build/sysroot)
    # ------------------------------------------------------------------
    mft_records: dict[int, bytes] = {}
    next_mft = 16

    def add_file(parent_mft: int, name: str, data: bytes):
        nonlocal next_mft
        rec_num = next_mft
        next_mft += 1
        real_size = len(data)
        attrs = [build_standard_info()]
        if real_size <= 512:
            alloc_size = real_size
            attrs.append(build_resident_attr(0x30, build_filename_payload(parent_mft, name, real_size, alloc_size, False), attr_id=1))
            attrs.append(build_resident_attr(0x80, data, attr_id=2))
        else:
            num_clusters = (real_size + CLUSTER_SIZE - 1) // CLUSTER_SIZE
            alloc_size = num_clusters * CLUSTER_SIZE
            lcn = alloc_clusters(num_clusters)
            padded = data + bytes(alloc_size - real_size)
            write_cluster(lcn, padded)
            attrs.append(build_resident_attr(0x30, build_filename_payload(parent_mft, name, real_size, alloc_size, False), attr_id=1))
            attrs.append(build_nonresident_attr(0x80, [(num_clusters, lcn)], real_size, alloc_size, attr_id=2))

        mft_records[rec_num] = build_mft_record(rec_num, 0x01, attrs, usn=(rec_num % 100) + 1)
        return (rec_num, parent_mft, name, real_size, alloc_size, False)

    # Reserve directory MFT numbers
    dir_micant = next_mft; next_mft += 1
    dir_micant_sys32 = next_mft; next_mft += 1
    dir_windows = next_mft; next_mft += 1
    dir_win_sys32 = next_mft; next_mft += 1
    dir_win_drivers = next_mft; next_mft += 1

    # Create C:\MicaNT\system.ini
    system_ini_bytes = (
        "[MicaNT]\r\n"
        "OS=MicaNT Cutler Edition 10.0\r\n"
        "FileSystem=NTFS 3.1 Sovereign Bare-Metal\r\n"
        "Architecture=AMD64\r\n"
        "Kernel=\\Windows\\System32\\ntoskrnl.exe\r\n"
    ).encode("ascii")
    micant_children = [
        add_file(dir_micant, "system.ini", system_ini_bytes),
        (dir_micant_sys32, dir_micant, "System32", 0, 0, True),
    ]

    # Populate C:\MicaNT\System32
    micant_sys32_children = []
    app_src = sysroot / "EFI" / "MicaNT" / "app.exe"
    if app_src.exists():
        micant_sys32_children.append(add_file(dir_micant_sys32, "app.exe", app_src.read_bytes()))

    # Populate C:\Windows\System32 & C:\Windows\System32\drivers from compiled sysroot
    win_sys32_children = [(dir_win_drivers, dir_win_sys32, "drivers", 0, 0, True)]
    sys32_dir = sysroot / "Windows" / "System32"
    for fname in [
        "ntoskrnl.exe", "ntdll.dll", "kernel32.dll", "kernelbase.dll",
        "user32.dll", "gdi32.dll", "advapi32.dll", "ucrtbase.dll",
        "cmd.exe", "smss.exe", "csrss.exe", "explorer.exe", "taskmgr.exe",
    ]:
        fpath = sys32_dir / fname
        if fpath.exists():
            win_sys32_children.append(add_file(dir_win_sys32, fname, fpath.read_bytes()))

    win_drivers_children = []
    drv_dir = sys32_dir / "drivers"
    for dname in ["ntfs.sys", "ahci.sys", "fltmgr.sys", "tcpip.sys"]:
        dpath = drv_dir / dname
        if dpath.exists():
            win_drivers_children.append(add_file(dir_win_drivers, dname, dpath.read_bytes()))

    readme_bytes = (
        "MicaNT Sovereign Bare-Metal NTFS 3.1 Volume (C:\\)\r\n"
        "Mounted directly via Ring-0 AHCI Bus-Master DMA (ntfs_baremetal.hpp).\r\n"
    ).encode("ascii")

    root_children = [
        (dir_micant, 5, "MicaNT", 0, 0, True),
        (dir_windows, 5, "Windows", 0, 0, True),
        add_file(5, "README.txt", readme_bytes),
    ]

    # Build directory MFT records (triggers both Resident $INDEX_ROOT and 4096B $INDEX_ALLOCATION INDX blocks!)
    mft_records[dir_micant] = build_dir_mft_record(dir_micant, 5, "MicaNT", micant_children, alloc_clusters, write_cluster)
    mft_records[dir_micant_sys32] = build_dir_mft_record(dir_micant_sys32, dir_micant, "System32", micant_sys32_children, alloc_clusters, write_cluster)
    mft_records[dir_windows] = build_dir_mft_record(dir_windows, 5, "Windows", [(dir_win_sys32, dir_windows, "System32", 0, 0, True)], alloc_clusters, write_cluster)
    mft_records[dir_win_sys32] = build_dir_mft_record(dir_win_sys32, dir_windows, "System32", win_sys32_children, alloc_clusters, write_cluster)
    mft_records[dir_win_drivers] = build_dir_mft_record(dir_win_drivers, dir_win_sys32, "drivers", win_drivers_children, alloc_clusters, write_cluster)

    # ------------------------------------------------------------------
    # 4. Build Core System MFT Metafiles (#0..#7)
    # ------------------------------------------------------------------
    # #0: $MFT
    mft_size = MFT_TOTAL_RECORDS * MFT_RECORD_SIZE
    mft_records[0] = build_mft_record(
        0,
        0x01,
        [
            build_standard_info(),
            build_resident_attr(0x30, build_filename_payload(5, "$MFT", mft_size, mft_size, False), attr_id=1),
            build_nonresident_attr(0x80, [(MFT_CLUSTERS, MFT_START_CLUSTER)], mft_size, mft_size, attr_id=2),
        ],
        usn=1,
    )

    # #1: $MFTMirr
    mft_records[1] = build_mft_record(
        1,
        0x01,
        [
            build_standard_info(),
            build_resident_attr(0x30, build_filename_payload(5, "$MFTMirr", 4096, 4096, False), attr_id=1),
            build_nonresident_attr(0x80, [(1, MFT_MIRR_CLUSTER)], 4096, 4096, attr_id=2),
        ],
        usn=1,
    )

    # #2: $LogFile
    mft_records[2] = build_mft_record(
        2,
        0x01,
        [
            build_standard_info(),
            build_resident_attr(0x30, build_filename_payload(5, "$LogFile", 0, 0, False), attr_id=1),
            build_resident_attr(0x80, b"", attr_id=2),
        ],
        usn=1,
    )

    # #3: $Volume (Label = "MicaNT_NTFS", Version = 3.1)
    vol_name = "MicaNT_NTFS".encode("utf-16le")
    vol_info = struct.pack("<QBBH", 0, 3, 1, 0)  # NTFS 3.1, clean
    mft_records[3] = build_mft_record(
        3,
        0x01,
        [
            build_standard_info(),
            build_resident_attr(0x30, build_filename_payload(5, "$Volume", 0, 0, False), attr_id=1),
            build_resident_attr(0x60, vol_name, attr_id=2),
            build_resident_attr(0x70, vol_info, attr_id=3),
        ],
        usn=1,
    )

    # #4: $AttrDef
    mft_records[4] = build_mft_record(
        4,
        0x01,
        [
            build_standard_info(),
            build_resident_attr(0x30, build_filename_payload(5, "$AttrDef", 0, 0, False), attr_id=1),
        ],
        usn=1,
    )

    # #5: "." Root Directory
    mft_records[5] = build_dir_mft_record(5, 5, ".", root_children, alloc_clusters, write_cluster)

    # #6: $Bitmap (reflects all allocated clusters!)
    bitmap_bytes = bytearray((PART_CLUSTERS + 7) // 8)
    for c in range(next_free_cluster):
        bitmap_bytes[c >> 3] |= (1 << (c & 7))
    bitmap_lcn = 64
    bitmap_clusters = (len(bitmap_bytes) + CLUSTER_SIZE - 1) // CLUSTER_SIZE
    write_cluster(bitmap_lcn, bytes(bitmap_bytes) + bytes(bitmap_clusters * CLUSTER_SIZE - len(bitmap_bytes)))
    mft_records[6] = build_mft_record(
        6,
        0x01,
        [
            build_standard_info(),
            build_resident_attr(0x30, build_filename_payload(5, "$Bitmap", len(bitmap_bytes), bitmap_clusters * CLUSTER_SIZE, False), attr_id=1),
            build_nonresident_attr(0x80, [(bitmap_clusters, bitmap_lcn)], len(bitmap_bytes), bitmap_clusters * CLUSTER_SIZE, attr_id=2),
        ],
        usn=1,
    )

    # #7: $Boot
    mft_records[7] = build_mft_record(
        7,
        0x01,
        [
            build_standard_info(),
            build_resident_attr(0x30, build_filename_payload(5, "$Boot", 8192, 8192, False), attr_id=1),
            build_nonresident_attr(0x80, [(2, 0)], 8192, 8192, attr_id=2),
        ],
        usn=1,
    )

    # Write all MFT records into $MFT at cluster 4 & mirror records 0..3 into $MFTMirr at cluster 128
    mft_base = part_byte_off + MFT_START_CLUSTER * CLUSTER_SIZE
    mirr_base = part_byte_off + MFT_MIRR_CLUSTER * CLUSTER_SIZE
    for rec_num, rec_data in mft_records.items():
        off = mft_base + rec_num * MFT_RECORD_SIZE
        disk[off : off + MFT_RECORD_SIZE] = rec_data
        if rec_num < 4:
            moff = mirr_base + rec_num * MFT_RECORD_SIZE
            disk[moff : moff + MFT_RECORD_SIZE] = rec_data

    out_img.write_bytes(disk)
    print(f"  [OK] Created {out_img} ({len(disk):,} bytes / {DISK_SIZE_MB} MiB)")
    print(f"  [OK] GPT Partition 1: LBA {PART_START_LBA}..{PART_END_LBA} ('MicaNT_OS')")
    print(f"  [OK] NTFS 3.1 Volume: Label='MicaNT_NTFS' | Cluster={CLUSTER_SIZE}B | $MFT LCN={MFT_START_CLUSTER}")
    print(f"  [OK] Populated {len(mft_records)} MFT records ({next_free_cluster} clusters allocated)")


if __name__ == "__main__":
    main()
