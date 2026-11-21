"""Unit test of cycle_counter, against a model of the count and the snapshot."""

import random
from pathlib import Path

from .unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="cycle_counter",
    folder=Path(__file__).parent,
    inputs=["reset", "take_snapshot", "poke", "poke_count"],
    outputs=["snapshot", "count"],
)

MASK64 = (1 << 64) - 1

Case = tuple[str, Vector]  # label, inputs


def case(
    label: str, snapshot: bool = False, reset: bool = False, poke: int | None = None
) -> Case:
    return label, {
        "reset": int(reset),
        "take_snapshot": int(snapshot),
        "poke": int(poke is not None),
        "poke_count": poke or 0,
    }


def model(cases: list[Case]) -> list[Vector]:
    """The outputs after each cycle's clock edge."""
    count = snapshot = 0
    out = []
    for _, v in cases:
        if v["poke"]:
            count = v["poke_count"]
        if v["reset"]:
            snapshot = 0
        elif v["take_snapshot"]:
            snapshot = count
        count = 0 if v["reset"] else (count + 1) & MASK64
        out.append({"snapshot": snapshot, "count": count})
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(
        [v for _, v in cases], model(cases), name, [label for label, _ in cases]
    )


# The demo takes a snapshot, which holds still while the count goes on, and takes
# another one just as the low word wraps around.
DEMO = [
    case("reset", reset=True),
    case("idle"),
    case("snapshot", snapshot=True),
    case("idle"),
    case("idle"),
    case("near wrap", poke=0x00000001_FFFFFFFE),
    case("idle"),
    case("snapshot", snapshot=True),
    case("idle"),
    case("idle"),
]


def test_random() -> None:
    rng = random.Random(1)
    cases = [case("reset", reset=True)]
    for _ in range(3000):
        poke = None
        if rng.random() < 0.05:
            poke = rng.choice([MASK64 - rng.randrange(4), (1 << 32) - 2, 0])
            poke = rng.getrandbits(64) if rng.random() < 0.5 else poke
        snapshot = rng.random() < 0.3
        reset = rng.random() < 0.01
        cases.append(case("random", snapshot, reset, poke))
    check(cases, "random")


def test_demo() -> None:
    BENCH.demo([v for _, v in DEMO], model(DEMO), [label for label, _ in DEMO])
