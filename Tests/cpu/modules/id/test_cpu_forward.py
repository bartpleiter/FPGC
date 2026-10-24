"""Unit test of cpu_forward, against the forwarding rules of the pipeline."""

import random
from itertools import product
from pathlib import Path

from ...unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="cpu_forward",
    folder=Path(__file__).parent,
    inputs=[
        *("areg", "breg", "reads_areg", "reads_breg"),
        *("ex_writes_dreg", "ex_dreg", "ex_late", "mem_writes_dreg", "mem_dreg"),
    ],
    outputs=[
        "areg_from_mem",
        "areg_from_wb",
        "breg_from_mem",
        "breg_from_wb",
        "late_result_stall",
    ],
)

REGS = ["areg", "breg", "ex_dreg", "mem_dreg"]
FLAGS = ["reads_areg", "reads_breg", "ex_writes_dreg", "ex_late", "mem_writes_dreg"]


def case(
    areg: int,
    breg: int,
    ex: int | None = None,
    mem: int | None = None,
    reads: str = "ab",
    ex_late: int = 0,
) -> Vector:
    """An instruction in ID reading `areg` and `breg` (those in `reads`), behind
    instructions in EX and MEM writing `ex` and `mem` (None: not writing)."""
    return {
        "areg": areg,
        "breg": breg,
        "reads_areg": int("a" in reads),
        "reads_breg": int("b" in reads),
        "ex_writes_dreg": int(ex is not None),
        "ex_dreg": ex or 0,
        "ex_late": ex_late,
        "mem_writes_dreg": int(mem is not None),
        "mem_dreg": mem or 0,
    }


# The demo shows an add r1 r2 behind instructions that write other or the same
# registers.
DEMO = [
    ("independent", case(1, 2, ex=4, mem=5)),
    ("areg in EX", case(1, 2, ex=1, mem=5)),
    ("breg in MEM", case(1, 2, ex=4, mem=2)),
    ("both", case(1, 2, ex=2, mem=1)),
    ("areg twice", case(1, 2, ex=1, mem=1)),
    ("late in EX", case(1, 2, ex=1, ex_late=1)),
    ("late unread", case(1, 2, ex=3, ex_late=1)),
    ("breg unread", case(1, 2, ex=2, reads="a")),
    ("no writes", case(1, 2)),
]


def model(v: Vector) -> Vector:
    out: Vector = {"late_result_stall": 0}
    for reg in ("areg", "breg"):
        r = v[reg]
        reads = v[f"reads_{reg}"]
        in_ex = reads and v["ex_writes_dreg"] and v["ex_dreg"] == r
        in_mem = reads and v["mem_writes_dreg"] and v["mem_dreg"] == r
        # The instruction in EX is younger, so its value is the newest.
        out[f"{reg}_from_mem"] = int(bool(in_ex))
        out[f"{reg}_from_wb"] = int(bool(in_mem and not in_ex))
        if in_ex and v["ex_late"]:
            out["late_result_stall"] = 1
    return out


def check(vectors: list[Vector], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(vectors, [model(v) for v in vectors], name)


def test_every_equality() -> None:
    """Only whether register numbers are equal matters. Four registers out of 0-3
    give every pattern of equal and different ones, also with r0."""
    vectors = []
    for regs in product(range(4), repeat=4):
        for flags in product((0, 1), repeat=len(FLAGS)):
            vectors.append(
                dict(zip(REGS, regs, strict=True))
                | dict(zip(FLAGS, flags, strict=True))
            )
    check(vectors, "every")


def test_random() -> None:
    rng = random.Random(1)
    vectors = [
        {k: rng.getrandbits(4 if k in REGS else 1) for k in BENCH.inputs}
        for _ in range(2000)
    ]
    check(vectors, "random")


def test_demo() -> None:
    vectors = [v for _, v in DEMO]
    BENCH.demo(vectors, [model(v) for v in vectors], [label for label, _ in DEMO])
