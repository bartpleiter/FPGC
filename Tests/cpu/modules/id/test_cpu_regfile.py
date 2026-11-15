"""Unit test of cpu_regfile, against a Python list of registers.

The registers have no reset, so each test starts by writing every register, as a program
writes a register before it reads it.
"""

import random
from pathlib import Path

from ...unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="cpu_regfile",
    folder=Path(__file__).parent,
    inputs=["areg", "breg", "writes_dreg", "dreg", "dreg_value"],
    outputs=["areg_value", "breg_value"],
)

Case = tuple[str, Vector]  # label, inputs


def case(
    label: str,
    areg: int = 0,
    breg: int = 0,
    write: tuple[int, int] | None = None,
) -> Case:
    dreg, value = write or (0, 0)
    return label, {
        "areg": areg,
        "breg": breg,
        "writes_dreg": int(write is not None),
        "dreg": dreg,
        "dreg_value": value,
    }


# Writes every register first, with its number in each byte.
START = [case(f"write r{d}", write=(d, 0x01010101 * d)) for d in range(1, 16)]

# The demo writes and reads to show r0 and the write-through.
DEMO = [
    *START,
    case("write r1", 1, 2, write=(1, 0x11111111)),
    case("write r2", 1, 2, write=(2, 0x22222222)),
    case("read", 1, 2),
    case("write r0", 0, 0, write=(0, 0xDEADBEEF)),
    case("write r15", 15, 1, write=(15, 0xFFFFFFFF)),
    case("overwrite r1", 1, 1, write=(1, 5)),
    case("read", 1, 15),
]


def model(cases: list[Case]) -> list[Vector]:
    """What the read ports give in each cycle, before the write at its end."""
    regs: list[int | None] = [0] + [None] * 15  # None until written
    out = []
    for _, v in cases:
        if v["writes_dreg"] and v["dreg"] != 0:
            regs[v["dreg"]] = v["dreg_value"]  # with the write-through
        a, b = regs[v["areg"]], regs[v["breg"]]
        assert a is not None and b is not None, (
            "a test reads a register before writing it"
        )
        out.append({"areg_value": a, "breg_value": b})
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(
        [v for _, v in cases], model(cases), name, [label for label, _ in cases]
    )


def test_every_register() -> None:
    """Write each register, then read each one on both ports."""
    cases = [*START]
    cases += [case(f"write r{d}", write=(d, 0x10101010 * d)) for d in range(16)]
    cases += [case(f"read r{r}", r, 15 - r) for r in range(16)]
    check(cases, "every")


def test_write_through() -> None:
    """Read every register while every register is written."""
    cases = [*START]
    for d in range(16):
        for r in range(16):
            cases.append(case("write", r, d, write=(d, (d << 8) | r)))
    check(cases, "write_through")


def test_random() -> None:
    rng = random.Random(1)
    cases = [*START]
    for _ in range(5000):
        write = (rng.randrange(16), rng.getrandbits(32)) if rng.random() < 0.7 else None
        cases.append(case("random", rng.randrange(16), rng.randrange(16), write))
    check(cases, "random")


def test_demo() -> None:
    BENCH.demo([v for _, v in DEMO], model(DEMO), [label for label, _ in DEMO])
