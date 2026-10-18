"""Random programs on the ISS: each one must be valid and halt with an empty stack.

This checks the generator. Once there is RTL, the same programs run in lockstep.
"""

import pytest

from b32asm.assembler import assemble
from b32asm.isa import Opcode, decode
from b32sim.sim import ROM_BASE, ROM_SIZE, Machine

from .random_program import generate

SEEDS = range(100)


@pytest.mark.parametrize("seed", SEEDS)
def test_random_program_runs_cleanly(seed: int) -> None:
    image = assemble(generate(seed), ROM_BASE).to_bytes()
    assert len(image) <= ROM_SIZE
    machine = Machine()
    machine.load(ROM_BASE, image)
    machine.run(max_steps=100_000)
    assert machine.sp == 0
    assert machine.err_status == 0


def test_same_seed_same_program() -> None:
    assert generate(7) == generate(7)
    assert generate(7) != generate(8)


def test_exclude() -> None:
    assert "divfp" not in generate(3, exclude=frozenset({"divfp"}))


ALU_ONLY = frozenset(
    ["or", "and", "xor", "add", "sub", "shiftl", "shiftr", "shiftrs", "slt", "sltu"]
    + ["not", "load", "loadhi", "nop"]
)


@pytest.mark.parametrize("seed", range(10))
def test_only(seed: int) -> None:
    program = assemble(generate(seed, only=ALU_ONLY), ROM_BASE)
    opcodes = {decode(word).opcode for word in program.words}
    assert opcodes == {Opcode.ARITH, Opcode.ARITHC, Opcode.HALT}
    machine = Machine()
    machine.load(ROM_BASE, program.to_bytes())
    machine.run(max_steps=100_000)
