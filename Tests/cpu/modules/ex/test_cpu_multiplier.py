"""Unit test of cpu_multiplier, against the multiply operations of the ISS."""

import random
from pathlib import Path

from b32asm.isa import MULDIV_OPS
from b32sim.sim import MASK, MULDIV_FUNCTIONS

from ...unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="cpu_multiplier",
    folder=Path(__file__).parent,
    inputs=["enable", "op", "a", "b"],
    outputs=["y"],
)

MULTIPLY_OPS = [name for name in MULDIV_OPS if name.startswith("mul")]

# Operands at the edges, like 0, small numbers, the 16-bit boundary where the operands
# are split, signed max and min, and all ones.
EDGES = [0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, MASK]

# The demo multiplies small numbers, where the signed and unsigned low words are the
# same, then a product that only has bits in the high word, signed and unsigned high
# words, and two fixed-point multiplies with 16 fraction bits: 1.5 * 2.25 = 3.375 and
# -0.5 * 4 = -2. In the last case, enable is 0, so the result stays.
DEMO = [
    ("mults", 6, -7 & MASK, 1),
    ("multu", 6, -7 & MASK, 1),
    ("mults", 0x10000, 0x10000, 1),
    ("mulshi", 0x10000, 0x10000, 1),
    ("mulshi", MASK, 2, 1),
    ("multuhi", MASK, 2, 1),
    ("multfp", 0x18000, 0x24000, 1),
    ("multfp", 0xFFFF8000, 0x40000, 1),
    ("hold", 0, 0, 0),
]

Case = tuple[str, int, int, int]  # operation, a, b, enable


def vectors(cases: list[Case]) -> list[Vector]:
    return [
        {"enable": enable, "op": MULDIV_OPS.get(name, 0), "a": a, "b": b}
        for name, a, b, enable in cases
    ]


def model(cases: list[Case]) -> list[Vector]:
    """The result of the operands that were taken last."""
    out = []
    y = 0
    for name, a, b, enable in cases:
        if enable:
            y = MULDIV_FUNCTIONS[name](a, b) & MASK
        out.append({"y": y})
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(vectors(cases), model(cases), name, [c[0] for c in cases])


def test_operations_cover_the_isa() -> None:
    assert set(MULTIPLY_OPS) == {"mults", "multu", "multfp", "mulshi", "multuhi"}


def test_edges() -> None:
    check(
        [(name, a, b, 1) for name in MULTIPLY_OPS for a in EDGES for b in EDGES],
        "edges",
    )


def test_random() -> None:
    rng = random.Random(1)
    cases = [
        (name, rng.getrandbits(32), rng.getrandbits(32), 1)
        for name in MULTIPLY_OPS
        for _ in range(1000)
    ]
    check(cases, "random")


def test_hold() -> None:
    """With enable 0, the multiplier keeps the operands it took last."""
    rng = random.Random(2)
    cases = [
        (
            rng.choice(MULTIPLY_OPS),
            rng.getrandbits(32),
            rng.getrandbits(32),
            int(i == 0 or rng.random() < 0.5),
        )
        for i in range(2000)
    ]
    check(cases, "hold")


def test_demo() -> None:
    BENCH.demo(vectors(DEMO), model(DEMO), [c[0] for c in DEMO])
