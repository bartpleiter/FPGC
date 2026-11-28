"""Tests of the SPI controller, spi, with the flash model on its pins: random programs,
erases and reads checked against a reference copy of the flash (tb_spi.v), at each of
the three speeds of the buses."""

import re
from pathlib import Path

import pytest

from ..common.presets import check_preset
from ..common.verilog import compile_tb, run_vvp
from .verilog import IO

FOLDER = Path(__file__).parent
DONE = re.compile(r"done: (\d+) reads, (\d+) programs, (\d+) erases, (\d+) resets")
SPEEDS = [1, 2, 4]  # HALF_PERIOD: 50, 25 and 12.5 MHz


def run(
    name: str, half_period: int, vcd: Path | None = None, **plusargs: object
) -> dict[str, int]:
    """Run the testbench. Returns the counts of its last line."""
    vvp = compile_tb(
        FOLDER / "tb_spi.v",
        [*IO.rtl_files, FOLDER / "flash_chip.v"],
        IO.build_dir / "modules" / "spi" / f"half_{half_period}" / "tb.vvp",
        parameters={"HALF_PERIOD": half_period},
        include_dirs=IO.include_dirs,
    )
    if vcd is not None:
        vcd.parent.mkdir(parents=True, exist_ok=True)
        plusargs["vcd"] = vcd
    output = run_vvp(vvp, **plusargs)
    done = DONE.search(output)
    assert done and "error" not in output, f"{name}:\n{output}"
    reads, programs, erases, resets = (int(n) for n in done.groups())
    return {"reads": reads, "programs": programs, "erases": erases, "resets": resets}


@pytest.mark.parametrize("seed", range(2))
@pytest.mark.parametrize("half_period", SPEEDS)
def test_random(half_period: int, seed: int) -> None:
    counts = run("random", half_period, operations=300, seed=seed)
    assert counts["reads"] > 100 and counts["programs"] > 50 and counts["erases"] > 2


@pytest.mark.parametrize("half_period", SPEEDS)
def test_resets(half_period: int) -> None:
    """Resets in the middle of reads: the chip select rises, and the next operations
    work."""
    counts = run("resets", half_period, operations=300, seed=7, resets=1)
    assert counts["resets"] > 10 and counts["reads"] > 100


def test_demo() -> None:
    vcd = IO.wave_dir / "spi.vcd"
    counts = run("demo", 1, vcd, demo=1)
    assert counts["reads"] == 1 and counts["programs"] == 1
    check_preset(FOLDER / "spi.gtkw", vcd)
