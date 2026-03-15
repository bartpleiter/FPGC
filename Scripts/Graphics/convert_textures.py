#!/usr/bin/env python3
"""
convert_textures.py — Convert PNG images to FPGC R3G3B2 texture data.

Each input image is resized to 64×64 and quantised to R3G3B2 (8-bit).
Output is a binary file: each pixel occupies 4 bytes (big-endian word,
value in low byte) so that fnp_tool.py binary upload produces one BRFS
word per pixel, matching the w3d.c texture layout.

Usage:
  python convert_textures.py -o textures.dat tex0.png tex1.png ...

The output file contains all textures concatenated (4096 words each).
"""

import argparse
import struct
import sys

try:
    from PIL import Image
except ImportError:
    print(
        "Error: Pillow is required.  Install with:  pip install Pillow", file=sys.stderr
    )
    sys.exit(1)

TEX_SIZE = 64
TEX_PIXELS = TEX_SIZE * TEX_SIZE


def rgb_to_r3g3b2(r: int, g: int, b: int) -> int:
    """Quantise an 8-bit RGB triple to R3G3B2."""
    return ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6)


def convert_image(path: str) -> list[int]:
    """Load an image, resize to 64×64, return list of R3G3B2 pixel values."""
    img = Image.open(path).convert("RGB")
    img = img.resize((TEX_SIZE, TEX_SIZE), Image.LANCZOS)
    pixels = []
    for y in range(TEX_SIZE):
        for x in range(TEX_SIZE):
            r, g, b = img.getpixel((x, y))
            pixels.append(rgb_to_r3g3b2(r, g, b))
    return pixels


def main():
    parser = argparse.ArgumentParser(
        description="Convert PNG textures to FPGC R3G3B2 format"
    )
    parser.add_argument(
        "images", nargs="+", help="Input image files (64×64 recommended)"
    )
    parser.add_argument(
        "-o",
        "--output",
        default="textures.dat",
        help="Output binary file (default: textures.dat)",
    )
    args = parser.parse_args()

    all_pixels = []
    for path in args.images:
        pixels = convert_image(path)
        assert len(pixels) == TEX_PIXELS
        all_pixels.extend(pixels)
        print(f"  Converted {path} ({TEX_PIXELS} pixels)")

    # Write as big-endian 32-bit words, one pixel per word
    with open(args.output, "wb") as f:
        for px in all_pixels:
            f.write(struct.pack("!I", px))

    word_count = len(all_pixels)
    byte_count = word_count * 4
    print(f"Wrote {word_count} words ({byte_count} bytes) to {args.output}")
    print(f"  {len(args.images)} texture(s), {TEX_PIXELS} pixels each")


if __name__ == "__main__":
    main()
