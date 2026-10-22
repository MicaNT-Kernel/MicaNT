import struct
import os

def create_regf_hive(filename, hive_name):
    # Base 4096-byte page
    data = bytearray(4096)
    
    # 0x00: Magic 'regf'
    data[0:4] = b'regf'
    # 0x04: Sequence numbers (1, 1)
    struct.pack_into('<II', data, 4, 1, 1)
    # 0x0C: Timestamp (FILETIME: ~2026-10-22)
    struct.pack_into('<Q', data, 12, 133742400000000000)
    # 0x14: Major version (1), Minor version (3)
    struct.pack_into('<II', data, 20, 1, 3)
    # 0x1C: Type (0 = normal), Format (1 = direct memory load)
    struct.pack_into('<II', data, 28, 0, 1)
    # 0x24: Root cell offset (0x20 in hive bins)
    struct.pack_into('<I', data, 36, 0x20)
    # 0x28: Hive bins data size (4096)
    struct.pack_into('<I', data, 40, 4096)
    # 0x2C: Clustering factor (1)
    struct.pack_into('<I', data, 44, 1)
    
    # 0x30: Hive filename (UTF-16LE, up to 64 bytes)
    encoded_name = hive_name.encode('utf-16le')[:64]
    data[48:48+len(encoded_name)] = encoded_name
    
    # 0x1FC: Checksum (XOR-32 of the first 0x1FC bytes)
    checksum = 0
    for i in range(0, 0x1FC, 4):
        val = struct.unpack_from('<I', data, i)[0]
        checksum ^= val
    struct.pack_into('<I', data, 0x1FC, checksum)
    
    # Hive bin header at offset 4096 (can append a 4096-byte bin)
    bin_data = bytearray(4096)
    # 0x00: Magic 'hbin'
    bin_data[0:4] = b'hbin'
    # 0x04: Offset from first bin (0)
    struct.pack_into('<I', bin_data, 4, 0)
    # 0x08: Bin size (4096)
    struct.pack_into('<I', bin_data, 8, 4096)
    
    total = data + bin_data
    with open(filename, 'wb') as f:
        f.write(total)
    print(f"Generated {filename}: {len(total)} bytes (regf header + hbin)")

if __name__ == '__main__':
    script_dir = os.path.dirname(os.path.abspath(__file__))
    hives = ['SYSTEM', 'SOFTWARE', 'SAM', 'SECURITY', 'DEFAULT']
    for h in hives:
        out_path = os.path.join(script_dir, h)
        create_regf_hive(out_path, h)
