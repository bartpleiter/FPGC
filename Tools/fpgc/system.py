"""Top-level tests and tools."""

import click

from fpgc.common import run
from fpgc.config import CONFIG
from fpgc.quartus import compile_and_report, compile_seeds, prepare
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
    """Lint the top level's RTL with Verilator, with the PLL's simulation model, whose
    delay for the SDRAM's clock Verilator ignores (--no-timing)."""
    files = [
        *cfg.rtl_files,
        cfg.pll_model,
        *CONFIG.cpu.rtl_files,
        *CONFIG.io.rtl_files,
        *CONFIG.sdram.rtl_files,
    ]
    for top in [f.stem for f in cfg.rtl_files]:
        run(
            "verilator",
            "--lint-only",
            "-Wall",
            "--no-timing",
            "--quiet",
            "--top-module",
            top,
            *files,
        )


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


# The SDRAM's and the SPI buses' pins, for the timing summary
PIN_GROUPS = {
    "SDRAM commands": "-to [get_ports {SDRAM_A[*] SDRAM_BA[*] SDRAM_CSn SDRAM_RASn"
    " SDRAM_CASn SDRAM_WEn}]",
    "SDRAM data out": "-to [get_ports {SDRAM_DQ[*] SDRAM_DQM[*]}]",
    "SDRAM data in": "-from [get_ports {SDRAM_DQ[*]}]",
    "Flash data out": "-to [get_ports {flash1_mosi flash2_mosi}]",
    "Flash data in": "-from [get_ports {flash1_mosi flash1_miso flash1_wp_n"
    " flash1_hold_n flash2_mosi flash2_miso flash2_wp_n flash2_hold_n}]",
    "SPI data out": "-to [get_ports {usb1_mosi usb2_mosi eth_mosi sd_mosi}]",
    "SPI data in": "-from [get_ports {usb1_miso usb2_miso eth_miso sd_miso}]",
}


@system.command()
@click.option(
    "--seeds",
    type=int,
    help="Compile with seeds 1 to SEEDS in parallel, and summarise over them: the"
    " sign-off, in which every seed must pass.",
)
def build(seeds: int | None) -> None:
    """Build the top level in Quartus, with the boot ROM, and report its timing.

    Assembles the boot ROM, generates the Quartus project in the build folder, compiles
    it, and prints the timing at every corner, the slack of the SDRAM's and SPI pins and the
    resources. The bitstream is output_files/fpgc.sof in that folder, from seed 1.
    """
    rom = write_rom_hex(cfg.boot_rom, cfg.quartus_dir / "rom.hex")
    files = [
        *cfg.rtl_files,
        cfg.pll_file,
        *CONFIG.cpu.rtl_files,
        *CONFIG.io.rtl_files,
        *CONFIG.sdram.rtl_files,
    ]
    out_dirs = {
        s: cfg.quartus_dir if s == 1 else cfg.quartus_dir.with_name(f"quartus-seed{s}")
        for s in range(1, (seeds or 1) + 1)
    }
    for s, out_dir in out_dirs.items():
        prepare(
            out_dir,
            cfg.top,
            files,
            CONFIG.cpu.device,
            cfg.sdc_file.read_text(),
            seed=s,
            parameters={"ROM_CONTENTS": str(rom.absolute())},
            extra_qsf=cfg.pins_file.read_text(),
            pin_groups=PIN_GROUPS,
        )
    if seeds:
        compile_seeds(CONFIG.cpu.quartus_bin, out_dirs, cfg.top)
    else:
        compile_and_report(CONFIG.cpu.quartus_bin, cfg.quartus_dir, cfg.top)
