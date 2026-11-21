"""The CPU with the device bridge and the devices, compared with the ISS.

Each program in programs/ runs in the ISS comparison testbench with the devices instead
of the bus model (the comment `; devices`), and must do the same as in the ISS, which
takes the words of device reads from the RTL. The UART's output must match too (see
Tests/cpu/iss_comparison.py). A program with illegal cases (bad_*) runs with the ISS in
hardware mode. `fpgc cpu wave b32p4 Tests/io/programs/<program>.asm` shows a run.
"""

import re
from pathlib import Path

import pytest

from ..cpu.iss_comparison import DEFAULT, Memories, RtlTrace, run_iss_comparison

PROGRAMS = sorted((Path(__file__).parent / "programs").glob("*.asm"))

# The caches as built with the shortest SDRAM, and with 4 lines and random latencies
MEMORIES = {"lines128": Memories(128, "1"), "lines4_rand": Memories(4, "rand")}


def run(path: Path, mode: str) -> RtlTrace:
    strict = not path.stem.startswith("bad_")
    return run_iss_comparison(
        path.read_text(),
        f"system_{path.stem}_{mode}",
        max_cycles=200_000,
        strict=strict,
        memories=MEMORIES[mode],
    )


def register(rtl: RtlTrace, reg: str) -> int:
    """The last value that the RTL wrote to `reg`."""
    values = [re.search(rf" {reg}=([\da-f]{{8}})", r) for r in rtl.records]
    return int([v for v in values if v][-1].group(1), 16)


@pytest.mark.parametrize("mode", MEMORIES)
@pytest.mark.parametrize("path", PROGRAMS, ids=lambda p: p.stem)
def test_program(path: Path, mode: str) -> None:
    run(path, mode)


def test_timer_cycles() -> None:
    """Four periods of 500 cycles, and the handlers."""
    rtl = run_iss_comparison(
        (Path(__file__).parent / "programs" / "timer_interrupt.asm").read_text(),
        "system_timer_cycles",
        memories=DEFAULT,
    )
    assert 2000 <= register(rtl, "r8") < 2100
    assert register(rtl, "r9") == 0


def test_switches_and_errors() -> None:
    rtl = run(Path(__file__).parent / "programs" / "bad_device_access.asm", "lines128")
    assert register(rtl, "r4") == 0b0101
    statuses = [re.search(r" r3=([\da-f]{8})", r) for r in rtl.records]
    # each time kind 3, a bad address
    assert [int(s.group(1), 16) for s in statuses if s] == [0x304] * 3
