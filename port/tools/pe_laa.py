#!/usr/bin/env python3
"""Mark a PE executable large-address-aware (IMAGE_FILE_LARGE_ADDRESS_AWARE).

Zig's COFF linker has no flag for it. The port maps MEM1 at 0x80000000,
which a 32-bit process can only reach with a 4 GB address space.

Usage: pe_laa.py melee.exe
"""

import struct
import sys

LAA = 0x0020

path = sys.argv[1]
with open(path, "r+b") as f:
    data = f.read(0x400)
    if data[:2] != b"MZ":
        sys.exit(f"{path}: not a PE file")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        sys.exit(f"{path}: no PE signature")
    off = pe + 4 + 18  # IMAGE_FILE_HEADER.Characteristics
    chars = struct.unpack_from("<H", data, off)[0]
    f.seek(off)
    f.write(struct.pack("<H", chars | LAA))
print(f"{path}: characteristics 0x{chars:04x} -> 0x{chars | LAA:04x}")
