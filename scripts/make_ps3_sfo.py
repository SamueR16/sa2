#!/usr/bin/env python3
import struct
import sys

def make_sfo(title_id, title, output_path):
    # Minimal PARAM.SFO generator
    keys = [
        "APP_VER", "ATTRIBUTE", "BOOTABLE", "CATEGORY", "LICENSE",
        "PARENTAL_LEVEL", "RESOLUTION", "SOUND_FORMAT", "TITLE", "TITLE_ID", "VERSION"
    ]

    values = [
        ("01.00", 2), # utf8
        (struct.pack("<I", 0), 4), # integer
        (struct.pack("<I", 1), 4), # integer
        ("HG", 2), # utf8
        ("FREE", 2), # utf8
        (struct.pack("<I", 0), 4), # integer
        (struct.pack("<I", 63), 4), # integer (720p etc)
        (struct.pack("<I", 1), 4), # integer
        (title, 2), # utf8
        (title_id, 2), # utf8
        ("01.00", 2) # utf8
    ]

    key_table = b""
    key_offsets = []
    for k in keys:
        key_offsets.append(len(key_table))
        key_table += k.encode('utf-8') + b"\x00"

    # Align key table to 4 bytes
    # key_table += b"\x00" * ((4 - len(key_table) % 4) % 4)

    data_table = b""
    data_info = [] # (length, max_length, offset)
    for v, t in values:
        offset = len(data_table)
        if t == 2: # utf8
            v_bytes = v.encode('utf-8') + b"\x00"
            length = len(v_bytes)
            max_length = (length + 3) // 4 * 4 # align
            if max_length < 16: max_length = 16 # common
            if keys[len(data_info)] == "TITLE": max_length = 128
            v_bytes += b"\x00" * (max_length - length)
            data_info.append((length, max_length, offset))
            data_table += v_bytes
        else: # integer
            data_info.append((4, 4, offset))
            data_table += v

    header_size = 20
    index_table_size = len(keys) * 16
    key_table_start = header_size + index_table_size
    data_table_start = key_table_start + len(key_table)
    # data_table_start = (data_table_start + 7) // 8 * 8 # align?

    header = struct.pack("<4sIIII",
        b"\x00PSF",
        0x00000101,
        key_table_start,
        data_table_start,
        len(keys)
    )

    index_table = b""
    for i in range(len(keys)):
        k_off = key_offsets[i]
        d_len, d_max, d_off = data_info[i]
        # type: 0x0404 for integer, 0x0204 for utf8
        fmt = 0x0204 if values[i][1] == 2 else 0x0404
        index_table += struct.pack("<HHIII", k_off, fmt, d_len, d_max, d_off)

    with open(output_path, "wb") as f:
        f.write(header)
        f.write(index_table)
        f.write(key_table)
        # Pad between key and data if needed?
        # f.write(b"\x00" * (data_table_start - f.tell()))
        f.write(data_table)

    print(f"Generated {output_path} with ID {title_id} and Title '{title}'")

if __name__ == "__main__":
    tid = "SA2PS3001"
    name = "Sonic Advance 2 Decomp"
    out = "PARAM.SFO"
    if len(sys.argv) > 1: tid = sys.argv[1]
    if len(sys.argv) > 2: name = sys.argv[2]
    if len(sys.argv) > 3: out = sys.argv[3]
    make_sfo(tid, name, out)
