"""
Authentic Windows NT Binary Registry Hive Generator (regf v1.3 / hbin / nk / lf / vk).
Generates spec-compliant binary hives for MicaNT System32\\config\\ (SYSTEM, SOFTWARE, SAM, SECURITY, DEFAULT).
"""
import os
import struct

REG_SZ        = 1
REG_EXPAND_SZ = 2
REG_BINARY    = 3
REG_DWORD     = 4
REG_QWORD     = 11


class HbinBuilder:
    """Packs authentic 8-byte-aligned negative-size hive cells into a 4096-byte-aligned hbin."""
    def __init__(self):
        self.buf = bytearray(32)  # 32-byte hbin header at offset 0

    def alloc_cell(self, payload: bytes) -> int:
        raw_len = 4 + len(payload)
        aligned = (raw_len + 7) & ~7
        offset = len(self.buf)
        self.buf.extend(struct.pack('<i', -aligned))
        self.buf.extend(payload)
        pad = aligned - raw_len
        if pad:
            self.buf.extend(b'\x00' * pad)
        return offset

    def write_value_node(self, name: str, reg_type: int, value) -> int:
        name_bytes = name.encode('ascii')
        if reg_type == REG_DWORD:
            val_u32 = int(value) & 0xFFFFFFFF
            data_len = 0x80000004  # Inline 4-byte data bit set
            data_off = val_u32
        elif reg_type in (REG_SZ, REG_EXPAND_SZ):
            raw = (str(value) + '\x00').encode('utf-16le')
            data_len = len(raw)
            data_off = self.alloc_cell(raw)
        elif reg_type == REG_QWORD:
            raw = struct.pack('<Q', int(value))
            data_len = len(raw)
            data_off = self.alloc_cell(raw)
        else:
            raw = bytes(value)
            data_len = len(raw)
            data_off = self.alloc_cell(raw) if raw else 0xFFFFFFFF

        hdr = struct.pack(
            '<2sHIIIHH',
            b'vk',
            len(name_bytes),
            data_len,
            data_off,
            reg_type,
            0x0001 if name_bytes else 0x0000,  # VALUE_COMP_NAME
            0,
        )
        return self.alloc_cell(hdr + name_bytes)

    def write_key_tree(self, name: str, node: dict, parent_off: int, is_root: bool = False) -> int:
        # 1. Serialize children first so we know their nk offsets
        subkeys = node.get('_subkeys', {})
        values = node.get('_values', [])

        # Reserve nk cell placeholder first so children know exact parent_off!
        name_bytes = name.encode('ascii')
        flags = 0x002C if is_root else 0x0020  # KEY_HIVE_ENTRY|KEY_COMP_NAME or KEY_COMP_NAME
        nk_off = self.alloc_cell(b'\x00' * (76 + len(name_bytes)))

        child_entries = []
        max_subkey_name = 0
        for sub_name, sub_node in subkeys.items():
            c_off = self.write_key_tree(sub_name, sub_node, nk_off, is_root=False)
            hint = sub_name.encode('ascii')[:4].ljust(4, b'\x00')
            child_entries.append((c_off, hint))
            if len(sub_name) * 2 > max_subkey_name:
                max_subkey_name = len(sub_name) * 2

        subkey_list_off = 0xFFFFFFFF
        if child_entries:
            lf_payload = bytearray(struct.pack('<2sH', b'lf', len(child_entries)))
            for c_off, hint in child_entries:
                lf_payload.extend(struct.pack('<I4s', c_off, hint))
            subkey_list_off = self.alloc_cell(bytes(lf_payload))

        val_offsets = []
        max_val_name = 0
        max_val_data = 0
        for v_name, v_type, v_val in values:
            v_off = self.write_value_node(v_name, v_type, v_val)
            val_offsets.append(v_off)
            if len(v_name) * 2 > max_val_name:
                max_val_name = len(v_name) * 2
            dlen = 4 if v_type == REG_DWORD else (len(str(v_val)) + 1) * 2
            if dlen > max_val_data:
                max_val_data = dlen

        val_list_off = 0xFFFFFFFF
        if val_offsets:
            vl_payload = b''.join(struct.pack('<I', o) for o in val_offsets)
            val_list_off = self.alloc_cell(vl_payload)

        # Patch nk cell header in-place (offset +4 skips int32 cellSize)
        nk_payload = struct.pack(
            '<2sHQIIIIIIIIIIIIIIIHH',
            b'nk',
            flags,
            133742400000000000,  # FILETIME
            0,                   # spare
            parent_off,
            len(child_entries),
            0,                   # volatile subkeys
            subkey_list_off,
            0xFFFFFFFF,          # volatile subkey list
            len(val_offsets),
            val_list_off,
            0xFFFFFFFF,          # security cell
            0xFFFFFFFF,          # class name
            max_subkey_name,
            0,                   # max class len
            max_val_name,
            max_val_data,
            0,                   # workvar
            len(name_bytes),
            0,                   # class len
        ) + name_bytes
        self.buf[nk_off + 4 : nk_off + 4 + len(nk_payload)] = nk_payload
        return nk_off

    def finalize(self) -> bytes:
        # Round up to 4096-byte page boundary (minimum 8192 bytes for headroom)
        cur = len(self.buf)
        bin_size = max(8192, ((cur + 4 + 4095) // 4096) * 4096)
        rem = bin_size - cur
        if rem >= 4:
            self.buf.extend(struct.pack('<i', rem))  # Trailing positive free cell
            self.buf.extend(b'\x00' * (rem - 4))
        struct.pack_into('<4sIIQQI', self.buf, 0, b'hbin', 0, bin_size, 0, 133742400000000000, 0)
        return bytes(self.buf)


def create_regf_hive(filename: str, hive_name: str, tree: dict):
    hb = HbinBuilder()
    root_off = hb.write_key_tree(hive_name, tree, 0xFFFFFFFF, is_root=True)
    hbin_bytes = hb.finalize()

    base = bytearray(4096)
    base[0:4] = b'regf'
    struct.pack_into('<II', base, 4, 1, 1)              # Sequence1 = 1, Sequence2 = 1
    struct.pack_into('<Q', base, 12, 133742400000000000) # FILETIME
    struct.pack_into('<II', base, 20, 1, 3)             # Version 1.3
    struct.pack_into('<II', base, 28, 0, 1)             # Type 0, Format 1
    struct.pack_into('<I', base, 36, root_off)          # RootCellOffset (0x20)
    struct.pack_into('<I', base, 40, len(hbin_bytes))   # HiveBinsDataSize
    struct.pack_into('<I', base, 44, 1)                 # ClusteringFactor

    encoded_name = hive_name.encode('utf-16le')[:64]
    base[48 : 48 + len(encoded_name)] = encoded_name

    # NT regf XOR-32 header checksum across bytes 0x000..0x1FB
    checksum = 0
    for i in range(0, 0x1FC, 4):
        checksum ^= struct.unpack_from('<I', base, i)[0]
    if checksum == 0:
        checksum = 1
    elif checksum == 0xFFFFFFFF:
        checksum = 0xFFFFFFFE
    struct.pack_into('<I', base, 0x1FC, checksum)

    total = bytes(base) + hbin_bytes
    with open(filename, 'wb') as f:
        f.write(total)
    print(f"Generated {filename}: {len(total)} bytes (regf v1.3 + hbin, root=0x{root_off:X}, crc=0x{checksum:08X})")


SYSTEM_TREE = {
    '_subkeys': {
        'Select': {
            '_values': [
                ('Current', REG_DWORD, 1),
                ('Default', REG_DWORD, 1),
                ('LastKnownGood', REG_DWORD, 1),
            ]
        },
        'CurrentControlSet': {
            '_subkeys': {
                'Control': {
                    '_values': [
                        ('SystemBootDevice', REG_SZ, r'\Device\Harddisk0\Partition1'),
                        ('SystemRoot', REG_SZ, r'C:\Windows'),
                        ('CurrentVersion', REG_SZ, '10.0.26100.1'),
                        ('ComputerName', REG_SZ, 'MICANT-SMP'),
                    ],
                    '_subkeys': {
                        'Session Manager': {
                            '_subkeys': {
                                'Memory Management': {
                                    '_values': [
                                        ('PagingFiles', REG_SZ, r'C:\pagefile.sys'),
                                        ('LargeSystemCache', REG_DWORD, 1),
                                    ]
                                }
                            }
                        }
                    }
                },
                'Services': {
                    '_subkeys': {
                        'Ntfs': {
                            '_values': [
                                ('ImagePath', REG_EXPAND_SZ, r'\SystemRoot\System32\drivers\ntfs.sys'),
                                ('Start', REG_DWORD, 0),
                                ('Type', REG_DWORD, 2),
                            ]
                        },
                        'Ahci': {
                            '_values': [
                                ('ImagePath', REG_EXPAND_SZ, r'\SystemRoot\System32\drivers\ahci.sys'),
                                ('Start', REG_DWORD, 0),
                                ('Type', REG_DWORD, 1),
                            ]
                        },
                    }
                }
            }
        }
    }
}

SOFTWARE_TREE = {
    '_subkeys': {
        'Microsoft': {
            '_subkeys': {
                'Windows NT': {
                    '_subkeys': {
                        'CurrentVersion': {
                            '_values': [
                                ('ProductName', REG_SZ, 'MicaNT Cutler Edition'),
                                ('EditionID', REG_SZ, 'SovereignNT'),
                                ('CurrentBuild', REG_SZ, '26100'),
                                ('BuildLabEx', REG_SZ, '26100.1.amd64fre.micant'),
                                ('InstallCount', REG_DWORD, 1),
                            ]
                        }
                    }
                }
            }
        },
        'MicaNT': {
            '_values': [
                ('Architecture', REG_SZ, 'AMD64-SMP'),
                ('FileSystem', REG_SZ, 'NTFS 3.1'),
            ]
        }
    }
}

if __name__ == '__main__':
    script_dir = os.path.dirname(os.path.abspath(__file__))
    specs = [
        ('SYSTEM', SYSTEM_TREE),
        ('SOFTWARE', SOFTWARE_TREE),
        ('SAM', {'_subkeys': {'Domains': {'_values': [('AccountCount', REG_DWORD, 1)]}}}),
        ('SECURITY', {'_subkeys': {'Policy': {'_values': [('AuditEnabled', REG_DWORD, 1)]}}}),
        ('DEFAULT', {'_subkeys': {'Environment': {'_values': [('OS', REG_SZ, 'MicaNT')]}}}),
    ]
    for name, tree in specs:
        create_regf_hive(os.path.join(script_dir, name), name, tree)
