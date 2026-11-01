"""Unit test of cpu_hazard, against the stall and flush rules, for every input."""

from itertools import pairwise, product
from pathlib import Path

from ..unit_bench import UnitBench, Vector

STAGES = ["if", "id", "ex", "mem"]  # WB never stalls

BENCH = UnitBench(
    unit="cpu_hazard",
    folder=Path(__file__).parent,
    inputs=[
        *("busy_if", "busy_id", "busy_ex", "busy_mem", "l1d_miss", "l1d_wait"),
        "redirect",
    ],
    outputs=[
        *("stall_if", "stall_id", "flush_id", "stall_ex", "flush_ex"),
        *("stall_mem", "flush_mem", "flush_wb", "refetch_if"),
    ],
)


def case(*busy: str, l1d_miss: int = 0, l1d_wait: int = 0, redirect: int = 0) -> Vector:
    return {f"busy_{s}": int(s in busy) for s in STAGES} | {
        "l1d_miss": l1d_miss,
        "l1d_wait": l1d_wait,
        "redirect": redirect,
    }


# The demo makes each stage busy on its own, then a few combinations.
DEMO = [
    ("run", case()),
    ("busy_if", case("if")),
    ("busy_id", case("id")),
    ("busy_ex", case("ex")),
    ("busy_mem", case("mem")),
    ("l1d_miss", case(l1d_miss=1)),
    ("l1d_wait", case(l1d_miss=1, l1d_wait=1)),
    ("redirect", case(redirect=1)),
    ("busy_if+id", case("if", "id")),
    ("busy_id+mem", case("id", "mem")),
]


def model(v: Vector) -> Vector:
    """A stage stalls when it or a stage after it is busy, and an L1D miss makes MEM
    busy. IF only sees the part of the miss that's known from registers (l1d_wait). A
    register is flushed by a redirect (except MEM/WB: the redirect comes from MEM), or
    gets a bubble when the stage before it stalls and its own stage doesn't. When ID
    stalls and IF doesn't, IF fetches its instruction again."""
    busy = {s: v[f"busy_{s}"] for s in STAGES}
    busy["mem"] |= v["l1d_miss"]
    stall = {s: any(busy[t] for t in STAGES[i:]) for i, s in enumerate(STAGES)}
    stall["if"] = any(v[f"busy_{s}"] for s in STAGES) or bool(v["l1d_wait"])
    out = {f"stall_{s}": int(stall[s]) for s in STAGES}
    for before, s in pairwise(STAGES):
        out[f"flush_{s}"] = int(v["redirect"] or (stall[before] and not stall[s]))
    out["flush_wb"] = int(stall["mem"])
    out["refetch_if"] = int(stall["id"] and not stall["if"])
    return out


def test_every_input() -> None:
    # l1d_wait is a part of l1d_miss, so it's never 1 without it.
    vectors = [
        dict(zip(BENCH.inputs, bits, strict=True)) for bits in product((0, 1), repeat=7)
    ]
    vectors = [v for v in vectors if v["l1d_miss"] or not v["l1d_wait"]]
    BENCH.check(vectors, [model(v) for v in vectors], "every")


def test_demo() -> None:
    vectors = [v for _, v in DEMO]
    BENCH.demo(vectors, [model(v) for v in vectors], [label for label, _ in DEMO])
