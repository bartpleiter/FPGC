"""Unit test of cpu_alu, against the ALU operations of the ISS."""

import random
from pathlib import Path

import pytest

from b32asm.isa import ALU_OPS
from b32sim.sim import ALU_FUNCTIONS, MASK

from ...unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="cpu_alu",
    folder=Path(__file__).parent,
    inputs=["op", "a", "b"],
    outputs=["y"],
)

# Operands at the edges, like 0, small numbers, the 16-bit boundary, signed max and
# min, and all ones.
EDGES = [0, 1, 2, 31, 32, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, MASK]

# The demo has one case per operation, with small numbers. Pairs show the difference
# between shiftr and shiftrs, and between slt and sltu.
DEMO = [
    ("or", 0xC, 0xA),
    ("and", 0xC, 0xA),
    ("xor", 0xC, 0xA),
    ("add", 5, 3),
    ("sub", 5, 7),
    ("shiftl", 1, 4),
    ("shiftr", 0x80000000, 4),
    ("shiftrs", 0x80000000, 4),
    ("not", 0x0000FFFF, 0),
    ("slt", MASK, 1),
    ("sltu", MASK, 1),
    ("load", 0, 0x1234),
    ("loadhi", 0x5678, 0x1234),
]

Case = tuple[str, int, int]  # operation, a, b


def vectors(cases: list[Case]) -> list[Vector]:
    return [{"op": ALU_OPS[name], "a": a, "b": b} for name, a, b in cases]


def model(cases: list[Case]) -> list[Vector]:
    return [{"y": ALU_FUNCTIONS[name](a, b) & MASK} for name, a, b in cases]


def labels(cases: list[Case]) -> list[str]:
    return [name for name, _, _ in cases]


def test_operations_cover_the_isa() -> None:
    assert set(ALU_FUNCTIONS) == set(ALU_OPS)


def test_edges() -> None:
    cases = [(name, a, b) for name in ALU_OPS for a in EDGES for b in EDGES]
    # Every shift amount, with the bits above b[4:0] set or not.
    for name in ("shiftl", "shiftr", "shiftrs"):
        for amount in range(32):
            for a in (1, 0x80000001, MASK):
                cases += [(name, a, amount), (name, a, amount | 0xFFFFFFE0)]
    BENCH.check(vectors(cases), model(cases), "edges", labels(cases))


def test_random() -> None:
    rng = random.Random(1)
    cases = [
        (name, rng.getrandbits(32), rng.getrandbits(32))
        for name in ALU_OPS
        for _ in range(500)
    ]
    BENCH.check(vectors(cases), model(cases), "random", labels(cases))


def test_demo() -> None:
    BENCH.demo(vectors(DEMO), model(DEMO), labels(DEMO))


def test_a_wrong_output_is_reported() -> None:
    wrong = [{"y": 8}, {"y": 9}]
    with pytest.raises(AssertionError, match=r"vector 1 \(add\): inputs op=3 a=5 b=3"):
        BENCH.check(vectors(DEMO[3:4] * 2), wrong, "wrong", ["add", "add"])
