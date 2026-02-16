import argparse
from pathlib import Path

from asmpy.models.data_types import SourceLine, Number


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Assembler for B32P3.", add_help=False)
    parser.add_argument("file", help="The asm file to assemble")
    parser.add_argument("output", help="The assembled output file")
    parser.add_argument(
        "-l",
        "--log-level",
        default="info",
        choices=["debug", "info", "warning", "error", "critical"],
        help="Set the log level",
    )
    parser.add_argument(
        "-d",
        "--log-details",
        action="store_true",
        help="Enable detailed logging with extra details like line numbers and timestamps",
    )
    parser.add_argument(
        "-h",
        "--header",
        action="store_true",
        help="Add header instructions (jump Main, jump Int, .dw line_count) to the output",
    )
    parser.add_argument(
        "-o",
        "--offset",
        type=str,
        default="0",
        help="Offset address for the program (affects label addresses). "
        "Can be decimal (21), hex (0xabcd), or binary (0b1010). Default is 0.",
    )
    parser.add_argument(
        "-i",
        "--independent",
        action="store_true",
        help="Generate position independent output: ignore --offset, use relative jumps, and rewrite addr2reg as PC-relative code",
    )
    parser.add_argument(
        "--help",
        action="help",
        help="Show this help message and exit",
    )
    return parser.parse_args()


def read_input_file(input_file_path: Path) -> list[SourceLine]:
    """Read input file and return list of SourceLine objects."""
    try:
        with open(input_file_path, "r") as file:
            input_lines = [
                SourceLine(
                    line=line.strip(),
                    source_line_number=i + 1,
                    source_file_name=str(input_file_path),
                )
                for i, line in enumerate(file.readlines())
            ]
    except FileNotFoundError as e:
        raise FileNotFoundError(f"Input file not found: {input_file_path}") from e
    return input_lines


def split_32bit_to_16bit(number: Number) -> tuple[Number, Number]:
    """Split a 32-bit number into two 16-bit numbers, allowing signed numbers."""
    if not (-0x80000000 <= number.value <= 0xFFFFFFFF):
        raise ValueError("Number must fit in 32 bits")

    lower_16bit = number.value & 0xFFFF
    upper_16bit = (number.value >> 16) & 0xFFFF

    return Number(value=upper_16bit, original=str(number.original) + "[31:16]"), Number(
        value=lower_16bit, original=str(number.original) + "[15:0]"
    )
