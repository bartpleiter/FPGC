"""CPU tests and tools."""

from pathlib import Path

import click

from fpgc.common import lint_rtl, run
from fpgc.config import CONFIG
from fpgc.quartus import compile_and_report, compile_seeds, harness_sdc, prepare

cfg = CONFIG.cpu


@click.group()
def cpu() -> None:
    """CPU tests and tools."""


@cpu.command()
@click.option(
    "-k", "pattern", default="", help="Only run tests matching this pytest pattern."
)
@click.option("--wave", is_flag=True, help="Dump a waveform for GTKWave.")
@click.option(
    "--seeds",
    type=int,
    default=100,
    show_default=True,
    help="The number of random programs compared with the ISS.",
)
def test(pattern: str, wave: bool, seeds: int) -> None:
    """Run the CPU tests."""
    flags = ["--wave"] if wave else []
    flags += ["--seeds", str(seeds)]
    run("pytest", cfg.tests_dir, *cfg.tool_dirs, "-k", pattern, *flags)


@cpu.command()
def lint() -> None:
    """Lint the CPU RTL with Verilator."""
    lint_rtl(cfg.rtl_files)


@cpu.command()
@click.argument("module")
@click.argument(
    "program",
    required=False,
    type=click.Path(exists=True, dir_okay=False, path_type=Path),
)
@click.option(
    "--view",
    help="Another preset of MODULE: <module>.<view>.gtkw, such as `--view data`.",
)
@click.option(
    "--lines",
    type=int,
    default=128,
    show_default=True,
    help="The number of lines of each L1 cache.",
)
@click.option(
    "--sdram-latency",
    default="6",
    show_default=True,
    help="The cycles from an SDRAM request to the first word of the line, or rand.",
)
@click.option(
    "--latency-seed",
    type=int,
    default=0,
    show_default=True,
    help="The seed of the random latencies.",
)
def wave(
    module: str,
    program: Path | None,
    view: str | None,
    lines: int,
    sdram_latency: str,
    latency_seed: int,
) -> None:
    """Run the demo of MODULE, or PROGRAM, and show it in GTKWave with its preset."""
    name = f"{module}.{view}.gtkw" if view else f"{module}.gtkw"
    presets = list(cfg.modules_dir.rglob(name))
    if not presets:
        raise click.UsageError(f"there is no {name} in {cfg.modules_dir}")
    memories = [
        f"--lines={lines}",
        f"--sdram-latency={sdram_latency}",
        f"--latency-seed={latency_seed}",
    ]
    # The demo tests write the waveform. A failing run still has one, so go on.
    tests: list[str | Path]
    if program is None:
        tests = [cfg.modules_dir, "-k", f"demo and {module}"]
        name = module
    else:
        demos = cfg.modules_dir / "test_demo_programs.py"
        tests = [demos, "--program", program, "-k", program.stem]
        name = program.stem
    run("pytest", *tests, "-q", *memories, check=False)
    run("gtkwave", "-r", cfg.gtkwave_rc, "-a", presets[0], cfg.wave_dir / f"{name}.vcd")


@cpu.command()
@click.argument(
    "files",
    nargs=-1,
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
@click.option(
    "--seeds",
    type=int,
    help="Compile with seeds 1 to SEEDS in parallel, and summarise over them.",
)
@click.option(
    "--l1i-lines",
    type=int,
    help="The number of lines of the L1I in the harness. Default: b32p4's.",
)
@click.option(
    "--l1d-lines",
    type=int,
    help="The number of lines of the L1D in the harness. Default: b32p4's.",
)
def timing(
    files: tuple[Path, ...],
    top: str | None,
    seed: int,
    seeds: int | None,
    l1i_lines: int | None,
    l1d_lines: int | None,
) -> None:
    """Compile the CPU in its harness, or Verilog FILES, and report the timing.

    Without FILES, this compiles the CPU's RTL with the harness top level, which makes
    the CPU clock from clk_50 with the PLL of the full system. A top level of FILES
    gets the CPU clock on its clk pin, or clk_50 as the harness. The timing is reported at every corner. With --seeds, this is the sign-off of
    the CPU: every seed must pass.
    """
    if not files:
        files, top = tuple(cfg.rtl_files), top or cfg.harness_top
    top = top or files[0].stem
    name = top
    parameters: dict[str, int | str] = {}
    if l1i_lines is not None:
        name += f"-l1i{l1i_lines}"
        parameters["L1I_LINES"] = l1i_lines
    if l1d_lines is not None:
        name += f"-l1d{l1d_lines}"
        parameters["L1D_LINES"] = l1d_lines
    # The harness, with the system's PLL
    sources = sorted(cfg.harness_dir.glob("*.v")) + [CONFIG.system.pll_file]
    sources += list(files)
    out_dirs = {
        s: cfg.timing_dir / (name if s == 1 else f"{name}-seed{s}")
        for s in (range(1, seeds + 1) if seeds else [seed])
    }
    for s, out_dir in out_dirs.items():
        sdc = harness_sdc((cfg.clock_period_ns, cfg.board_clock_period_ns))
        prepare(out_dir, top, sources, cfg.device, sdc, s, parameters)
    if seeds:
        compile_seeds(cfg.quartus_bin, out_dirs, top)
    else:
        compile_and_report(cfg.quartus_bin, out_dirs[seed], top)
