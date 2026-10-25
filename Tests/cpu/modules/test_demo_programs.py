"""The demo programs, one per stage, plus b32p4.asm for the whole pipeline.

Each is compared with the ISS and dumps a waveform to build/cpu/wave/, and the
presets next to it must match that dump: <stem>.gtkw, and <stem>.<view>.gtkw for other
views. `fpgc cpu wave` shows them. Programs given with --program run here too, for
`fpgc cpu wave <module> <program>`.

The ISS runs them in hardware mode, so a demo can show an illegal case, such as the
error register recording it.
"""

from pathlib import Path

import pytest

from ..iss_comparison import run_iss_comparison
from ..presets import check_preset
from ..verilog import WAVE_DIR

DEMOS = sorted(Path(__file__).parent.rglob("*.asm"))


def pytest_generate_tests(metafunc: pytest.Metafunc) -> None:
    if "program" in metafunc.fixturenames:
        programs = DEMOS + metafunc.config.getoption("program")
        metafunc.parametrize("program", programs, ids=[p.stem for p in programs])


def test_demo_program(b32p4_sim: Path, program: Path) -> None:
    vcd = WAVE_DIR / f"{program.stem}.vcd"
    run_iss_comparison(b32p4_sim, program.read_text(), program.stem, vcd, strict=False)
    if program in DEMOS:
        # The preset, and the other views of the same demo (<stem>.<view>.gtkw)
        check_preset(program.with_suffix(".gtkw"), vcd)
        for view in program.parent.glob(f"{program.stem}.*.gtkw"):
            check_preset(view, vcd)
