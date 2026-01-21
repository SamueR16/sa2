#!/usr/bin/env python3
import sys
import os
from PIL import Image

def prepare_icon(input_path, output_path):
    if not os.path.exists(input_path):
        print(f"Error: {input_path} not found.")
        return

    img = Image.open(input_path)
    # Resize to 320x176 as requested for ICON0.PNG
    img = img.resize((320, 176), Image.Resampling.LANCZOS)
    img.save(output_path)
    print(f"Generated {output_path} from {input_path}")

if __name__ == "__main__":
    src = ".github/media/titlescreen.png"
    dst = "ICON0.PNG"
    if len(sys.argv) > 1: src = sys.argv[1]
    if len(sys.argv) > 2: dst = sys.argv[2]
    prepare_icon(src, dst)
