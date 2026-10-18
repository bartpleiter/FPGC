"""CPU tests and tools."""

from pathlib import Path

import click

from fpgc.common import run
from fpgc.config import CONFIG
from fpgc.quartus import compile_and_report

cfg = CONFIG.cpu


@click.group()
def cpu() -> None:
    """CPU tests and tools."""


@cpu.command()
@click.option(
    "-k", "pattern", default="", help="Only run tests matching this pytest pattern."
)
@click.option("--wave", is_flag=True, help="Dump a waveform for GTKWave.")
def test(pattern: str, wave: bool) -> None:
    """Run the CPU tests."""
    flags = ["--wave"] if wave else []
    run("pytest", cfg.tests_dir, *cfg.tool_dirs, "-k", pattern, *flags)


@cpu.command()
def lint() -> None:
    """Lint the CPU RTL with Verilator."""
    for top in [f.stem for f in cfg.rtl_files]:
        run(
            "verilator",
            "--lint-only",
            "-Wall",
            "--quiet",
            "--top-module",
            top,
            *cfg.rtl_files,
        )


@cpu.command()
@click.argument("module")
@click.argument(
    "program",
    required=False,
    type=click.Path(exists=True, dir_okay=False, path_type=Path),
)
def wave(module: str, program: Path | None) -> None:
    """Run the demo of MODULE, or PROGRAM, and show it in GTKWave with its preset."""
    presets = list(cfg.modules_dir.rglob(f"{module}.gtkw"))
    if not presets:
        raise click.UsageError(f"there is no {module}.gtkw in {cfg.modules_dir}")
    # The demo tests write the waveform. A failing run still has one, so go on.
    if program is None:
        run("pytest", cfg.modules_dir, "-q", "-k", f"demo and {module}", check=False)
        name = module
    else:
        demos = cfg.modules_dir / "test_demo_programs.py"
        run(
            "pytest", demos, "-q", "--program", program, "-k", program.stem, check=False
        )
        name = program.stem
    run("gtkwave", "-r", cfg.gtkwave_rc, "-a", presets[0], cfg.wave_dir / f"{name}.vcd")


@cpu.command()
@click.argument(
    "files",
    nargs=-1,
    required=True,
    type=click.Path(exists=True, dir_okay=False, path_type=Path),
)
@click.option("--top", help="Top-level module. Default: the name of the first file.")
@click.option(
    "--seed",
    type=int,
    default=1,
    show_default=True,
    help="Fitter seed.",
)
def timing(files: tuple[Path, ...], top: str | None, seed: int) -> None:
    """Compile Verilog files in Quartus and report the timing at every corner."""
    top = top or files[0].stem
    compile_and_report(
        cfg.quartus_bin,
        cfg.timing_dir / (top if seed == 1 else f"{top}-seed{seed}"),
        top,
        sorted(cfg.harness_dir.glob("*.v")) + list(files),
        cfg.device,
        cfg.clock_period_ns,
        seed,
    )
