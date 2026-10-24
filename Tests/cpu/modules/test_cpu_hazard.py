"""Unit test of cpu_hazard, against the stall and flush rules, for every input."""

from itertools import pairwise, product
from pathlib import Path

from ..unit_bench import UnitBench, Vector

STAGES = ["if", "id", "ex", "mem"]  # WB never stalls

BENCH = UnitBench(
    unit="cpu_hazard",
    folder=Path(__file__).parent,
    inputs=["busy_if", "busy_id", "busy_ex", "busy_mem", "redirect"],
    outputs=[
        *("stall_if", "stall_id", "flush_id", "stall_ex", "flush_ex"),
        *("stall_mem", "flush_mem", "flush_wb"),
    ],
)


def case(*busy: str, redirect: int = 0) -> Vector:
    return {f"busy_{s}": int(s in busy) for s in STAGES} | {"redirect": redirect}


# The demo makes each stage busy on its own, then a few combinations.
DEMO = [
    ("run", case()),
    ("busy_if", case("if")),
    ("busy_id", case("id")),
    ("busy_ex", case("ex")),
    ("busy_mem", case("mem")),
    ("redirect", case(redirect=1)),
    ("busy_if+id", case("if", "id")),
    ("busy_id+mem", case("id", "mem")),
]


def model(v: Vector) -> Vector:
    """A stage stalls when it or a stage after it is busy. A register is flushed by a
    redirect (except MEM/WB: the redirect comes from MEM), or gets a bubble when the
    stage before it stalls and its own stage doesn't."""
    stall = {s: any(v[f"busy_{t}"] for t in STAGES[i:]) for i, s in enumerate(STAGES)}
    out = {f"stall_{s}": int(stall[s]) for s in STAGES}
    for before, s in pairwise(STAGES):
        out[f"flush_{s}"] = int(v["redirect"] or (stall[before] and not stall[s]))
    out["flush_wb"] = int(stall["mem"])
    return out


def test_every_input() -> None:
    vectors = [
        dict(zip(BENCH.inputs, bits, strict=True)) for bits in product((0, 1), repeat=5)
    ]
    BENCH.check(vectors, [model(v) for v in vectors], "every")


def test_demo() -> None:
    vectors = [v for _, v in DEMO]
    BENCH.demo(vectors, [model(v) for v in vectors], [label for label, _ in DEMO])
