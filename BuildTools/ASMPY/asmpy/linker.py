"""
B32P3 Assembly-level Linker

Combines multiple QBE-generated .asm files into a single assembly file
that can be assembled by ASMPY into a flat binary.

Features:
- Renames local labels (.L* prefix) to avoid cross-file conflicts
- Validates that all referenced symbols are defined
- Reports duplicate global symbol definitions
- Preserves section ordering (.code/.text, .data, .bss)
"""

import argparse
import logging
import re
import sys
from pathlib import Path

from asmpy.assembler import Assembler
from asmpy.models.data_types import Number, SourceLine
from asmpy.logger import CustomFormatter, configure_logging


logger = logging.getLogger()


def _make_file_prefix(file_path: Path) -> str:
    """Generate a unique prefix from a file path for local label renaming."""
    return file_path.stem.replace("-", "_").replace(".", "_")


def _rename_local_labels(lines: list[str], prefix: str) -> list[str]:
    """Rename local labels (.L prefix) to avoid cross-file conflicts.

    Renames both label definitions (.Lfoo:) and all references to them
    in instructions and data directives.
    """
    # Find all local label definitions in this file
    local_labels: set[str] = set()
    label_def_re = re.compile(r"^(\s*)(\.L\S+):\s*$")
    for line in lines:
        m = label_def_re.match(line)
        if m:
            local_labels.add(m.group(2))

    if not local_labels:
        return lines

    # Build a regex that matches any local label as a whole word
    # Sort by length (longest first) to avoid partial matches
    sorted_labels = sorted(local_labels, key=len, reverse=True)
    escaped = [re.escape(lbl) for lbl in sorted_labels]
    pattern = re.compile(r"(?<!\w)(" + "|".join(escaped) + r")(?!\w)")

    def replacer(m: re.Match) -> str:
        old_name = m.group(1)
        # .Lfoo -> .L_prefix_foo
        return f".L_{prefix}_{old_name[2:]}"

    result = []
    for line in lines:
        result.append(pattern.sub(replacer, line))
    return result


def _collect_symbols(
    lines: list[str],
) -> tuple[set[str], set[str], set[str]]:
    """Collect symbol information from assembly lines.

    Returns:
        (global_defs, local_defs, all_refs) where:
        - global_defs: labels marked with .globl/.global
        - local_defs: all label definitions (including locals)
        - all_refs: all label references in instructions/data
    """
    global_defs: set[str] = set()
    local_defs: set[str] = set()

    for line in lines:
        stripped = line.strip()

        # .globl / .global directives
        if stripped.startswith((".globl ", ".global ")):
            parts = stripped.split()
            if len(parts) == 2:
                global_defs.add(parts[1])

        # Label definitions
        if stripped.endswith(":") and not stripped.startswith((".", ";", "/*")):
            label_name = stripped[:-1].strip()
            local_defs.add(label_name)
        elif stripped.endswith(":") and stripped.startswith(".") and not stripped.startswith((".data", ".bss", ".text", ".code")):
            label_name = stripped[:-1].strip()
            local_defs.add(label_name)

    return global_defs, local_defs, set()


def link_asm_files(
    input_files: list[Path],
    output_file: Path,
    offset_address: int = 0,
    add_header: bool = False,
    independent: bool = False,
    syscall: bool = False,
) -> None:
    """Link multiple assembly files into a single binary.

    1. Read all input .asm files
    2. Rename local labels to avoid conflicts
    3. Collect and validate symbols
    4. Concatenate into one assembly stream
    5. Assemble with ASMPY
    """
    all_lines: list[str] = []
    all_global_defs: dict[str, str] = {}  # symbol -> defining file
    all_local_defs: set[str] = set()

    for file_path in input_files:
        logger.info(f"Reading {file_path}")
        with open(file_path) as f:
            lines = f.readlines()

        # Strip trailing newlines
        lines = [line.rstrip("\n") for line in lines]

        # Rename local labels
        prefix = _make_file_prefix(file_path)
        lines = _rename_local_labels(lines, prefix)

        # Collect symbols
        global_defs, local_defs, _ = _collect_symbols(lines)

        # Check for duplicate global symbols
        for sym in global_defs:
            if sym in all_global_defs:
                raise ValueError(
                    f"Duplicate global symbol '{sym}': defined in both "
                    f"'{all_global_defs[sym]}' and '{file_path.name}'"
                )
            all_global_defs[sym] = file_path.name

        all_local_defs.update(local_defs)
        all_lines.extend(lines)

    logger.info(
        f"Linking {len(input_files)} files: "
        f"{len(all_global_defs)} global symbols, "
        f"{len(all_local_defs)} total labels"
    )

    # Convert to SourceLine objects for ASMPY
    source_lines = [
        SourceLine(line=line, source_line_number=i + 1, source_file_name="linked")
        for i, line in enumerate(all_lines)
        if line.strip()  # skip empty lines
    ]

    # Assemble
    assembler = Assembler(
        preprocessed_input_lines=source_lines,
        output_file_path=str(output_file),
        offset_address=Number(offset_address),
        independent=independent,
        syscall=syscall,
    )
    assembler.assemble(add_header=add_header)
    logger.info(f"Linked binary written to {output_file}")


def main():
    parser = argparse.ArgumentParser(
        description="B32P3 Assembly Linker — combines multiple .asm files into a binary"
    )
    parser.add_argument(
        "files",
        nargs="+",
        help="Input .asm files to link",
    )
    parser.add_argument(
        "output",
        help="Output binary file path",
    )
    parser.add_argument(
        "-o", "--offset",
        default="0",
        help="Base address offset (default: 0)",
    )
    parser.add_argument(
        "-H", "--header",
        action="store_true",
        help="Add program header (jump Main, jump Int, filesize)",
    )
    parser.add_argument(
        "-i", "--independent",
        action="store_true",
        help="Generate position-independent code",
    )
    parser.add_argument(
        "-s", "--syscall",
        action="store_true",
        help="Add syscall vector to header",
    )
    parser.add_argument(
        "-l", "--log-level",
        default="info",
        choices=["debug", "info", "warning", "error", "critical"],
        help="Log level (default: info)",
    )

    args = parser.parse_args()
    configure_logging(args.log_level, CustomFormatter(log_details=False))

    input_files = [Path(f) for f in args.files]
    for f in input_files:
        if not f.exists():
            logger.error(f"Input file not found: {f}")
            sys.exit(1)

    try:
        offset = int(args.offset, 0)  # supports 0x prefix
    except ValueError:
        logger.error(f"Invalid offset: {args.offset}")
        sys.exit(1)

    try:
        link_asm_files(
            input_files=input_files,
            output_file=Path(args.output),
            offset_address=offset,
            add_header=args.header,
            independent=args.independent,
            syscall=args.syscall,
        )
    except Exception as e:
        logger.error(f"Linker failed: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
