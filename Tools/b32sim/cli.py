"""b32sim: B32P4 instruction-set simulator."""

import sys
from pathlib import Path

import click

from b32sim.sim import ROM_BASE, SDRAM_BASE, IllegalError, Machine, SimError

_FILE = click.Path(exists=True, dir_okay=False, path_type=Path)


@click.command(context_settings={"help_option_names": ["-h", "--help"]})
@click.option("--rom", type=_FILE, required=True, help="Binary image for the ROM.")
@click.option("--sdram", type=_FILE, help="Binary image for SDRAM, loaded at 0.")
@click.option(
    "--hardware",
    is_flag=True,
    help="Behave like the hardware on illegal cases, instead of stopping.",
)
@click.option(
    "--trace",
    type=click.Path(dir_okay=False, path_type=Path),
    help="Write the retire trace here.",
)
@click.option(
    "--max-steps",
    default=1_000_000,
    show_default=True,
    help="Stop after this many steps.",
)
def main(
    rom: Path, sdram: Path | None, hardware: bool, trace: Path | None, max_steps: int
) -> None:
    """Run a program from reset until it halts, then print the registers."""
    machine = Machine(strict=not hardware)
    machine.load(ROM_BASE, rom.read_bytes())
    if sdram is not None:
        machine.load(SDRAM_BASE, sdram.read_bytes())

    retired = []
    error = None
    try:
        for _ in range(max_steps):
            retired.append(machine.step())
            if machine.halted:
                break
        else:
            error = f"no HALT within {max_steps} steps"
    except (IllegalError, SimError) as e:
        error = f"{type(e).__name__}: {e}"

    if trace is not None:
        trace.parent.mkdir(parents=True, exist_ok=True)
        trace.write_text("".join(f"{r}\n" for r in retired))
    for i in range(0, 16, 4):
        click.echo("  ".join(f"r{n:<2} {machine.regs[n]:08x}" for n in range(i, i + 4)))
    click.echo(
        f"pc  {machine.pc:08x}  sp  {machine.sp:02x}  err {machine.err_status:03x}"
    )
    click.echo(f"{len(retired)} instructions retired")
    if error:
        click.echo(f"error: {error}", err=True)
        sys.exit(1)
