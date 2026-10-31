"""Unit test of cpu_divider, against the divide operations of the ISS."""

import random
from pathlib import Path

from b32asm.isa import MULDIV_OPS
from b32sim.sim import MASK, MULDIV_FUNCTIONS

from ...unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="cpu_divider",
    folder=Path(__file__).parent,
    inputs=["start", "op", "a", "b"],
    outputs=["busy", "y"],
)

DIVIDE_OPS = [name for name in MULDIV_OPS if name[:3] in ("div", "mod")]

# The steps of each divide, after the cycle of the start
STEPS = {name: 48 if name == "divfp" else 32 for name in DIVIDE_OPS}

EDGES = [
    0,
    1,
    2,
    7,
    0xFFFF,
    0x10000,
    0x7FFFFFFF,
    0x80000000,
    0x80000001,
    MASK - 6,
    MASK,
]

Case = tuple[str, Vector]  # label, inputs


def start(name: str, a: int, b: int, label: str | None = None) -> Case:
    return label or name, {"start": 1, "op": MULDIV_OPS[name], "a": a, "b": b}


def idle(label: str = "") -> Case:
    return label, {"start": 0, "op": 0, "a": 0, "b": 0}


def divide(name: str, a: int, b: int, label: str | None = None) -> list[Case]:
    """A whole divide: the start, its steps, and one cycle with the result."""
    return [start(name, a, b, label), *[idle() for _ in range(STEPS[name])], idle()]


def model(cases: list[Case]) -> list[Vector]:
    """What the divider gives in each cycle, before the clock edge.

    It's busy in the cycle of a start and during the steps after it. Then the result
    is there, until the next start. A start while it's busy begins a new divide.
    """
    out = []
    steps = 0
    result = 0
    for _, v in cases:
        busy = bool(v["start"]) or steps > 0
        out.append({"busy": int(busy), "y": 0 if busy else result})
        if v["start"]:
            name = DIVIDE_OPS[[MULDIV_OPS[n] for n in DIVIDE_OPS].index(v["op"])]
            steps = STEPS[name]
            result = MULDIV_FUNCTIONS[name](v["a"], v["b"]) & MASK
        elif steps:
            steps -= 1
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(
        [v for _, v in cases], model(cases), name, [label for label, _ in cases]
    )


# The demo divides 13 by 3, which only does something in the last four steps, the ones
# for the four bits of 13. Then the remainder of -13 by 3, which gets the sign of the
# dividend.
DEMO = [
    *divide("divu", 13, 3, "divu 13 3"),
    *divide("mods", -13 & MASK, 3, "mods -13 3"),
]


def test_operations_cover_the_isa() -> None:
    assert set(DIVIDE_OPS) | {"mults", "multu", "multfp", "mulshi", "multuhi"} == set(
        MULDIV_FUNCTIONS
    )


def test_edges() -> None:
    cases = []
    for name in DIVIDE_OPS:
        for a in EDGES:
            for b in EDGES:
                cases += divide(name, a, b)
    check(cases, "edges")


def test_random() -> None:
    """Random divides, sometimes started again before the last one is done."""
    rng = random.Random(1)
    cases = []
    for _ in range(1500):
        name = rng.choice(DIVIDE_OPS)
        a = rng.choice([rng.getrandbits(32), rng.getrandbits(rng.randint(1, 32))])
        b = rng.choice([rng.getrandbits(32), rng.getrandbits(rng.randint(1, 32))])
        cases.append(start(name, a, b))
        wait = (
            STEPS[name] + 1 if rng.random() < 0.8 else rng.randint(0, STEPS[name] + 3)
        )
        cases += [idle() for _ in range(wait)]
    cases.append(idle())
    check(cases, "random")


def test_demo() -> None:
    BENCH.demo([v for _, v in DEMO], model(DEMO), [label for label, _ in DEMO])
