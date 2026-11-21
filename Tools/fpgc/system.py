"""Top-level tests and tools."""

import click

from fpgc.common import run
from fpgc.config import CONFIG
from fpgc.quartus import compile_and_report, prepare
from fpgc.rom import write_rom_hex

cfg = CONFIG.system


@click.group()
def system() -> None:
    """Top-level tests and tools."""


@system.command()
@click.option(
    "-k", "pattern", default="", help="Only run tests matching this pytest pattern."
)
def test(pattern: str) -> None:
    """Run the top-level tests."""
    run("pytest", cfg.tests_dir, "-k", pattern)


@system.command()
def lint() -> None:
    """Lint the top level's RTL with Verilator, with the PLL's simulation model."""
    files = [
        *cfg.rtl_files,
        cfg.pll_model,
        *CONFIG.cpu.rtl_files,
        *CONFIG.io.rtl_files,
    ]
    for top in [f.stem for f in cfg.rtl_files]:
        run("verilator", "--lint-only", "-Wall", "--quiet", "--top-module", top, *files)


@system.command()
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


@system.command()
def build() -> None:
    """Build the top level in Quartus, with the boot ROM, and report its timing.

    Assembles the boot ROM, generates the Quartus project in the build folder, compiles
    it, and prints the timing at every corner and the resources. The bitstream is
    output_files/fpgc.sof in that folder.
    """
    out_dir = cfg.quartus_dir
    rom = write_rom_hex(cfg.boot_rom, out_dir / "rom.hex")
    files = [
        *cfg.rtl_files,
        cfg.pll_file,
        *CONFIG.cpu.rtl_files,
        *CONFIG.io.rtl_files,
    ]
    prepare(
        out_dir,
        cfg.top,
        files,
        CONFIG.cpu.device,
        cfg.sdc_file.read_text(),
        seed=1,
        parameters={"ROM_CONTENTS": str(rom.absolute())},
        extra_qsf=cfg.pins_file.read_text(),
    )
    compile_and_report(CONFIG.cpu.quartus_bin, out_dir, cfg.top)
