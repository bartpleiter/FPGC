"""SDRAM controller tests and tools."""

import click

from fpgc.common import lint_rtl, run
from fpgc.config import CONFIG

cfg = CONFIG.sdram


@click.group()
def sdram() -> None:
    """SDRAM controller tests and tools."""


@sdram.command()
@click.option(
    "-k", "pattern", default="", help="Only run tests matching this pytest pattern."
)
def test(pattern: str) -> None:
    """Run the SDRAM controller and chip model tests."""
    run("pytest", cfg.tests_dir, "-k", pattern)


@sdram.command()
def lint() -> None:
    """Lint the SDRAM controller with Verilator."""
    lint_rtl(cfg.rtl_files)


@sdram.command()
@click.argument("module")
def wave(module: str) -> None:
    """Run the demo of MODULE and show it in GTKWave with its preset."""
    presets = list(cfg.tests_dir.rglob(f"{module}.gtkw"))
    if not presets:
        raise click.UsageError(f"there is no {module}.gtkw in {cfg.tests_dir}")
    # The demo test writes the waveform. A failing run still has one, so go on.
    run("pytest", cfg.tests_dir, "-k", f"demo and {module}", "-q", check=False)
    run(
        "gtkwave",
        "-r",
        cfg.gtkwave_rc,
        "-a",
        presets[0],
        cfg.wave_dir / f"{module}.vcd",
    )
