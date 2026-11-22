"""Tests of the SDRAM controller, sdram_controller, against two chip models, with
random reads and writes checked against a reference memory (tb_sdram_controller.v)."""

import re
from pathlib import Path

import pytest

from ..common.presets import check_preset
from ..common.verilog import compile_tb, run_vvp
from .verilog import SDRAM

FOLDER = Path(__file__).parent
DONE = re.compile(r"done: (\d+) reads, (\d+) writes, (\d+) refreshes, (\d+) resets")


def run(name: str, vcd: Path | None = None, **plusargs: object) -> dict[str, int]:
    """Run the testbench. Returns the counts of its last line."""
    vvp = compile_tb(
        FOLDER / "tb_sdram_controller.v",
        SDRAM.rtl_files,
        SDRAM.build_dir / "modules" / "sdram_controller" / name / "tb.vvp",
        include_dirs=SDRAM.include_dirs,
    )
    if vcd is not None:
        vcd.parent.mkdir(parents=True, exist_ok=True)
        plusargs["vcd"] = vcd
    output = run_vvp(vvp, **plusargs)
    done = DONE.search(output)
    assert done and "error" not in output, output
    reads, writes, refreshes, resets = (int(n) for n in done.groups())
    return {"reads": reads, "writes": writes, "refreshes": refreshes, "resets": resets}


@pytest.mark.parametrize("seed", range(4))
def test_random(seed: int) -> None:
    """Random reads and writes, long enough for many refreshes."""
    counts = run(f"random_{seed}", requests=3000, seed=seed)
    assert counts["reads"] > 1000 and counts["writes"] > 1000
    assert counts["refreshes"] > 40


@pytest.mark.parametrize("seed", range(4))
def test_resets(seed: int) -> None:
    """Resets at random moments, also during reads and writes: the controller starts
    again from its initialization, and works."""
    counts = run(f"resets_{seed}", requests=3000, seed=seed, resets=1)
    assert counts["resets"] > 10


def test_demo() -> None:
    vcd = SDRAM.wave_dir / "sdram_controller.vcd"
    counts = run("demo", vcd, demo=1)
    assert counts["reads"] == 2 and counts["writes"] == 2
    check_preset(FOLDER / "sdram_controller.gtkw", vcd)
