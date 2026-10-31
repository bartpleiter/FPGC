"""Run programs on the RTL, and compare what they do with the ISS.

The CPU is built one step at a time, so only the programs that use instructions the
RTL already has run here. SUPPORTED grows with each step of the build order, and the
directed programs then join in by themselves.

Every program runs under each of the LATENCIES of the memory models, and must give the
same retire trace under each of them.
"""

from pathlib import Path

import pytest

from b32asm.assembler import assemble
from b32asm.isa import Opcode, decode
from b32sim.sim import ROM_BASE

from .iss_comparison import IDEAL, Latency, run_iss_comparison
from .random_program import BRANCH_OPS, MULDIV_OPS, READ_OPS, WRITE_OPS, generate
from .test_random import ALU_ONLY

# The opcodes the RTL executes, and the mnemonics for the random programs.
SUPPORTED = {Opcode.ARITH, Opcode.ARITHC, Opcode.SAVPC, Opcode.BRANCH, Opcode.JUMP}
SUPPORTED |= {Opcode.JUMPR, Opcode.HALT, Opcode.READ, Opcode.WRITE}
SUPPORTED |= {Opcode.PUSH, Opcode.POP, Opcode.ARITHM, Opcode.ARITHMC}
RANDOM_ONLY = ALU_ONLY | {*BRANCH_OPS, "jump", "jumpr", "jumpro", "savpc"}
RANDOM_ONLY |= {*READ_OPS, *WRITE_OPS, "push", "pop", *MULDIV_OPS}
# The regions with a memory behind the data port. Only SDRAM and the CPU-internal I/O
# yet, so the directed programs that read ROM or video memory wait for the step that
# adds those memories.
RANDOM_REGIONS = frozenset({"sdram", "cpu_io"})
NEEDS_REGION = {"rom_to_ram_copy", "vram_loop_write", "vram_read_write"}

# Memories that always answer right away, memories that always wait a cycle, so as many
# waits as possible, and memories that wait a random number of cycles. The random
# programs get their own seed for the latencies too.
LATENCIES = {
    "lat0": IDEAL,
    "lat1": Latency("1", "1"),
    "rand": Latency("rand", "rand"),
}

PROGRAMS = sorted((Path(__file__).parent / "asm").rglob("*.asm"))
ERROR_PROGRAMS = sorted((Path(__file__).parent / "errors").glob("*.asm"))


def supported(path: Path) -> bool:
    if path.stem in NEEDS_REGION:
        return False
    words = assemble(path.read_text(), ROM_BASE, str(path)).words
    return all(decode(w).opcode in SUPPORTED for w in words)


def run_name(name: str, mode: str) -> str:
    return name if mode == "lat0" else f"{name}_{mode}"


@pytest.mark.parametrize("mode", LATENCIES)
@pytest.mark.parametrize(
    "path", [p for p in PROGRAMS if supported(p)], ids=lambda p: p.stem
)
def test_program(b32p4_sim: Path, wave_dir: Path | None, path: Path, mode: str) -> None:
    name = run_name(path.stem, mode)
    vcd = wave_dir / f"{name}.vcd" if wave_dir else None
    run_iss_comparison(b32p4_sim, path.read_text(), name, vcd, latency=LATENCIES[mode])


@pytest.mark.parametrize("mode", LATENCIES)
@pytest.mark.parametrize("path", ERROR_PROGRAMS, ids=lambda p: p.stem)
def test_error_program(
    b32p4_sim: Path, wave_dir: Path | None, path: Path, mode: str
) -> None:
    """The programs with illegal cases, compared with the ISS in hardware mode."""
    name = run_name(path.stem, mode)
    vcd = wave_dir / f"{name}.vcd" if wave_dir else None
    run_iss_comparison(
        b32p4_sim,
        path.read_text(),
        name,
        vcd,
        strict=False,
        latency=LATENCIES[mode],
    )


@pytest.mark.parametrize("mode", LATENCIES)
def test_random(
    b32p4_sim: Path, wave_dir: Path | None, random_seed: int, mode: str
) -> None:
    name = run_name(f"random_{random_seed}", mode)
    vcd = wave_dir / f"{name}.vcd" if wave_dir else None
    latency = LATENCIES[mode]
    run_iss_comparison(
        b32p4_sim,
        generate(random_seed, only=RANDOM_ONLY, regions=RANDOM_REGIONS),
        name,
        vcd,
        latency=Latency(latency.fetch, latency.data, random_seed),
    )
