"""Run the CPU test programs on the ISS, from ROM and from SDRAM.

Each program in asm/ ends with `halt` and has an `; expected=N` comment: the value
r15 must have at that point. Programs run in strict mode, so any illegal case fails.
"""

import re
from pathlib import Path

import pytest

from b32asm.assembler import assemble
from b32sim.sim import ROM_BASE, SDRAM_BASE, Machine

PROGRAMS = sorted((Path(__file__).parent / "asm").rglob("*.asm"))


def expected_r15(source: str) -> int:
    match = re.search(r"expected=(\d+)", source)
    assert match, "no '; expected=' comment"
    return int(match.group(1))


def run_program(source: str, name: str, from_sdram: bool) -> Machine:
    machine = Machine()
    if from_sdram:
        machine.load(ROM_BASE, assemble("jump 0").to_bytes())
        machine.load(SDRAM_BASE, assemble(source, SDRAM_BASE, name).to_bytes())
    else:
        machine.load(ROM_BASE, assemble(source, ROM_BASE, name).to_bytes())
    machine.run(max_steps=100_000)
    return machine


@pytest.mark.parametrize("from_sdram", [False, True], ids=["rom", "sdram"])
@pytest.mark.parametrize("path", PROGRAMS, ids=lambda p: p.stem)
def test_program(path: Path, from_sdram: bool) -> None:
    source = path.read_text()
    machine = run_program(source, str(path), from_sdram)
    assert machine.regs[15] == expected_r15(source)
    assert machine.err_status == 0
