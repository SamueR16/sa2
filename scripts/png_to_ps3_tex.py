#!/usr/bin/env python3
import sys
import os
try:
    from PIL import Image
except ImportError:
    print("Error: PIL (Pillow) is required for this script.")
    print("Install it with: pip install Pillow")
    sys.exit(1)

def convert_to_ps3_texture(png_path, out_path):
    """
    Converts a PNG image to a raw ARGB8888 format suitable for PS3/Tiny3D.
    PS3 is Big-Endian, so ARGB means bytes are A, R, G, B in that order.
    """
    if not os.path.exists(png_path):
        print(f"Error: {png_path} not found.")
        return

    img = Image.open(png_path).convert('RGBA')
    width, height = img.size

    # Get raw data (RGBA)
    raw_data = img.tobytes("raw", "RGBA")

    # Convert RGBA to ARGB
    # Input (RGBA): R, G, B, A
    # Output (ARGB): A, R, G, B
    argb_data = bytearray()
    for i in range(0, len(raw_data), 4):
        r, g, b, a = raw_data[i:i+4]
        argb_data.append(a)
        argb_data.append(r)
        argb_data.append(g)
        argb_data.append(b)

    with open(out_path, 'wb') as f:
        f.write(argb_data)

    print(f"Converted {png_path} ({width}x{height}) to {out_path}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: png_to_ps3_tex.py input.png output.bin")
        sys.exit(1)

    convert_to_ps3_texture(sys.argv[1], sys.argv[2])
