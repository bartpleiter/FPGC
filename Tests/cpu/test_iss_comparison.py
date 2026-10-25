"""Run programs on the RTL, and compare what they do with the ISS.

The CPU is built one step at a time, so only the programs that use instructions the
RTL already has run here. SUPPORTED grows with each step of the build order, and the
directed programs then join in by themselves.
"""

from pathlib import Path

import pytest

from b32asm.assembler import assemble
from b32asm.isa import Opcode, decode
from b32sim.sim import ROM_BASE

from .iss_comparison import run_iss_comparison
from .random_program import BRANCH_OPS, READ_OPS, WRITE_OPS, generate
from .test_random import ALU_ONLY

# The opcodes the RTL executes, and the mnemonics for the random programs.
SUPPORTED = {Opcode.ARITH, Opcode.ARITHC, Opcode.SAVPC, Opcode.BRANCH, Opcode.JUMP}
SUPPORTED |= {Opcode.JUMPR, Opcode.HALT, Opcode.READ, Opcode.WRITE}
RANDOM_ONLY = ALU_ONLY | {*BRANCH_OPS, "jump", "jumpr", "jumpro", "savpc"}
RANDOM_ONLY |= {*READ_OPS, *WRITE_OPS}
# The regions with a memory behind the data port. Only SDRAM yet, so the directed
# programs that read ROM or video memory wait for the step that adds those memories.
RANDOM_REGIONS = frozenset({"sdram"})
NEEDS_REGION = {"rom_to_ram_copy", "vram_loop_write", "vram_read_write"}

PROGRAMS = sorted((Path(__file__).parent / "asm").rglob("*.asm"))


def supported(path: Path) -> bool:
    if path.stem in NEEDS_REGION:
        return False
    words = assemble(path.read_text(), ROM_BASE, str(path)).words
    return all(decode(w).opcode in SUPPORTED for w in words)


@pytest.mark.parametrize(
    "path", [p for p in PROGRAMS if supported(p)], ids=lambda p: p.stem
)
def test_program(b32p4_sim: Path, wave_dir: Path | None, path: Path) -> None:
    vcd = wave_dir / f"{path.stem}.vcd" if wave_dir else None
    run_iss_comparison(b32p4_sim, path.read_text(), path.stem, vcd)


def test_random(b32p4_sim: Path, wave_dir: Path | None, random_seed: int) -> None:
    name = f"random_{random_seed}"
    vcd = wave_dir / f"{name}.vcd" if wave_dir else None
    run_iss_comparison(
        b32p4_sim,
        generate(random_seed, only=RANDOM_ONLY, regions=RANDOM_REGIONS),
        name,
        vcd,
    )
