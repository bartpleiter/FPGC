"""Unit test of cpu_branch, against the branch conditions of the ISS."""

import random
from itertools import product
from pathlib import Path

from b32asm.isa import BRANCH_OPS
from b32sim.sim import MASK, branch_taken

from ...unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="cpu_branch",
    folder=Path(__file__).parent,
    inputs=["a", "b", "branch_op", "branch_signed"],
    outputs=["taken"],
)

# Operands at the edges: around 0, signed max and min, and all ones.
EDGES = [0, 1, 2, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0x80000001, MASK - 1, MASK]

# The demo compares 1 with 2, 2 with 2, and -1 with 1, which is the largest number
# when unsigned.
DEMO = [
    ("beq", 2, 2),
    ("beq", 1, 2),
    ("bne", 1, 2),
    ("bgt", 2, 1),
    ("bge", 2, 2),
    ("blt", 1, 2),
    ("ble", 2, 1),
    ("bgt", MASK, 1),
    ("bgts", MASK, 1),
    ("blt", MASK, 1),
    ("blts", MASK, 1),
]

Case = tuple[int, int, int, int]  # branch_op, branch_signed, a, b


def model(op: int, signed: int, a: int, b: int) -> int:
    """The ISS's condition, and not taken for a reserved one."""
    if op in (0b011, 0b111):
        return 0
    return int(branch_taken(op, signed, a, b))


def vectors(cases: list[Case]) -> list[Vector]:
    return [
        {"a": a, "b": b, "branch_op": op, "branch_signed": s} for op, s, a, b in cases
    ]


def expected(cases: list[Case]) -> list[Vector]:
    return [{"taken": model(*case)} for case in cases]


def test_edges() -> None:
    """Every condition code, reserved ones too, on every pair of edge operands."""
    cases = [
        (op, s, a, b)
        for op, s in product(range(8), (0, 1))
        for a, b in product(EDGES, EDGES)
    ]
    BENCH.check(vectors(cases), expected(cases), "edges")


def test_random() -> None:
    rng = random.Random(1)
    cases = []
    for op, s in product(range(8), (0, 1)):
        for _ in range(300):
            a = rng.getrandbits(32)
            # Equal and nearly equal operands too, which random numbers rarely are.
            b = rng.choice([rng.getrandbits(32), a, (a + 1) & MASK, (a - 1) & MASK])
            cases.append((op, s, a, b))
    BENCH.check(vectors(cases), expected(cases), "random")


def test_demo() -> None:
    cases = [(*BRANCH_OPS[name], a, b) for name, a, b in DEMO]
    labels = [name for name, _, _ in DEMO]
    BENCH.demo(vectors(cases), expected(cases), labels)
