"""Unit test of cpu_regfile, against a Python list of registers."""

import random
from pathlib import Path

from ...unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="cpu_regfile",
    folder=Path(__file__).parent,
    inputs=["reset", "areg", "breg", "writes_dreg", "dreg", "dreg_value"],
    outputs=["areg_value", "breg_value"],
)

Case = tuple[str, Vector]  # label, inputs


def case(
    label: str,
    areg: int = 0,
    breg: int = 0,
    write: tuple[int, int] | None = None,
    reset: int = 0,
) -> Case:
    dreg, value = write or (0, 0)
    return label, {
        "reset": reset,
        "areg": areg,
        "breg": breg,
        "writes_dreg": int(write is not None),
        "dreg": dreg,
        "dreg_value": value,
    }


# The demo: writes and reads that show r0, the write-through and the reset.
DEMO = [
    case("reset", reset=1),
    case("write r1", 1, 2, write=(1, 0x11111111)),
    case("write r2", 1, 2, write=(2, 0x22222222)),
    case("read", 1, 2),
    case("write r0", 0, 0, write=(0, 0xDEADBEEF)),
    case("write r15", 15, 1, write=(15, 0xFFFFFFFF)),
    case("overwrite r1", 1, 1, write=(1, 5)),
    case("read", 1, 15),
    case("reset", 1, 15, reset=1),
    case("read", 1, 15),
]


def model(cases: list[Case]) -> list[Vector]:
    """What the read ports give in each cycle, before the write at its end."""
    regs = [0] * 16
    out = []
    for _, v in cases:
        writes = v["writes_dreg"] and v["dreg"] != 0
        now = regs.copy()  # with the write-through
        if writes:
            now[v["dreg"]] = v["dreg_value"]
        out.append({"areg_value": now[v["areg"]], "breg_value": now[v["breg"]]})
        regs = [0] * 16 if v["reset"] else now
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(
        [v for _, v in cases], model(cases), name, [label for label, _ in cases]
    )


def test_every_register() -> None:
    """Write each register, then read each one on both ports."""
    cases = [case("reset", reset=1)]
    cases += [case(f"write r{d}", write=(d, 0x01010101 * d)) for d in range(16)]
    cases += [case(f"read r{r}", r, 15 - r) for r in range(16)]
    check(cases, "every")


def test_write_through() -> None:
    """Read every register while every register is written."""
    cases = [case("reset", reset=1)]
    for d in range(16):
        for r in range(16):
            cases.append(case("write", r, d, write=(d, (d << 8) | r)))
    check(cases, "write_through")


def test_random() -> None:
    rng = random.Random(1)
    cases = [case("reset", reset=1)]
    for _ in range(5000):
        write = (rng.randrange(16), rng.getrandbits(32)) if rng.random() < 0.7 else None
        cases.append(
            case(
                "random",
                rng.randrange(16),
                rng.randrange(16),
                write,
                reset=int(rng.random() < 0.01),
            )
        )
    check(cases, "random")


def test_demo() -> None:
    BENCH.demo([v for _, v in DEMO], model(DEMO), [label for label, _ in DEMO])
