"""b32asm."""

import sys
from pathlib import Path

import click

from b32asm.assembler import AsmError, assemble


@click.command(context_settings={"help_option_names": ["-h", "--help"]})
@click.argument("source", type=click.Path(exists=True, dir_okay=False, path_type=Path))
@click.argument("output", type=click.Path(dir_okay=False, path_type=Path))
@click.option(
    "--base",
    default="0",
    show_default=True,
    help="Address of the first instruction, e.g. 0x1E000000 for ROM.",
)
def main(source: Path, output: Path, base: str) -> None:
    """Assemble SOURCE into a little-endian binary image OUTPUT."""
    try:
        program = assemble(source.read_text(), int(base, 0), str(source))
    except AsmError as e:
        click.echo(f"error: {e}", err=True)
        sys.exit(1)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(program.to_bytes())
