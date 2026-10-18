"""The demo programs: one per stage, plus b32p4.asm for the whole pipeline.

Each runs in lockstep with the ISS and dumps a waveform to build/cpu/wave/, and the
preset next to it must match that dump. `fpgc cpu wave` shows them. Programs given
with --program run here too, for `fpgc cpu wave <module> <program>`.
"""

from pathlib import Path

import pytest

from ..lockstep import run_lockstep
from ..presets import check_preset
from ..verilog import WAVE_DIR

DEMOS = sorted(Path(__file__).parent.rglob("*.asm"))


def pytest_generate_tests(metafunc: pytest.Metafunc) -> None:
    if "program" in metafunc.fixturenames:
        programs = DEMOS + metafunc.config.getoption("program")
        metafunc.parametrize("program", programs, ids=[p.stem for p in programs])


def test_demo_program(b32p4_sim: Path, program: Path) -> None:
    vcd = WAVE_DIR / f"{program.stem}.vcd"
    run_lockstep(b32p4_sim, program.read_text(), program.stem, vcd)
    preset = program.with_suffix(".gtkw")
    if program in DEMOS:
        check_preset(preset, vcd)
