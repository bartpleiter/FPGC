#!/usr/bin/env python3
"""
Binary to Flash Writer C File Converter

Converts a binary file into a C source file containing the binary data
as inline assembly .dw directives, suitable for use with the flash_writer program.
"""

import argparse
import logging
import sys
from pathlib import Path


def binary_to_flash_c(input_path: Path, output_path: Path) -> int:
    """Convert a binary file to flash_binary.c format.

    Args:
        input_path: Path to input binary file
        output_path: Path to output C file

    Returns:
        Number of 32-bit words written

    Raises:
        FileNotFoundError: If input file doesn't exist
        ValueError: If binary file size is not word-aligned
    """
    if not input_path.exists():
        raise FileNotFoundError(f"Input file not found: {input_path}")

    binary_data = input_path.read_bytes()
    num_bytes = len(binary_data)

    if num_bytes % 4 != 0:
        # Pad to word boundary
        padding = 4 - (num_bytes % 4)
        binary_data = binary_data + b"\x00" * padding
        logging.warning(f"Padded binary with {padding} zero bytes for word alignment")

    num_words = len(binary_data) // 4

    # Generate C file content
    lines = [
        f"#define FLASH_PROGRAM_SIZE_WORDS {num_words}",
        "",
        "void flash_binary()",
        "{",
        "  asm(",
    ]

    # Convert each 32-bit word to a .dw directive
    for i in range(num_words):
        offset = i * 4
        # Read 4 bytes and convert to big-endian 32-bit word
        word = (
            (binary_data[offset] << 24)
            | (binary_data[offset + 1] << 16)
            | (binary_data[offset + 2] << 8)
            | binary_data[offset + 3]
        )
        lines.append(f'    ".dw 0x{word:08X}"')

    lines.append("  );")
    lines.append("}")
    lines.append("")

    output_path.write_text("\n".join(lines))
    logging.info(
        f"Generated {output_path} with {num_words} words ({num_words * 4} bytes)"
    )

    return num_words


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Convert binary file to flash_binary.c format for SPI flash programming"
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Input binary file path",
    )
    parser.add_argument(
        "output",
        type=Path,
        help="Output C file path",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Enable verbose output",
    )

    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(message)s",
    )

    try:
        num_words = binary_to_flash_c(args.input, args.output)
        print(f"Successfully converted {args.input} to {args.output}")
        print(f"  Size: {num_words} words ({num_words * 4} bytes)")
        return 0
    except FileNotFoundError as e:
        logging.error(str(e))
        return 1
    except Exception as e:
        logging.error(f"Error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
