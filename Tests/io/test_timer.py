"""Unit test of timer, against a model of its count, settings and flag."""

import random
from pathlib import Path

from .unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="timer",
    folder=Path(__file__).parent,
    inputs=[
        "reset",
        "start",
        "start_count",
        "configure",
        "periodic_in",
        "irq_enable_in",
        "clear",
    ],
    outputs=["count", "periodic", "irq_enable", "expired", "irq"],
)

Case = tuple[str, Vector]  # label, inputs


def case(
    label: str,
    start: int | None = None,
    configure: tuple[int, int] | None = None,
    clear: bool = False,
    reset: bool = False,
) -> Case:
    """A cycle with any of: a start with a count, new settings (periodic, interrupt
    enable), a clear of the flag, a reset."""
    periodic, enable = configure or (0, 0)
    return label, {
        "reset": int(reset),
        "start": int(start is not None),
        "start_count": start or 0,
        "configure": int(configure is not None),
        "periodic_in": periodic,
        "irq_enable_in": enable,
        "clear": int(clear),
    }


def model(cases: list[Case]) -> list[Vector]:
    """The outputs after each cycle's clock edge."""
    count = reload = periodic = enable = expired = 0
    out = []
    for _, v in cases:
        if v["reset"]:
            count = reload = periodic = enable = expired = 0
        else:
            expire = count == 1
            if v["start"]:
                count = reload = v["start_count"]
            elif expire:
                count = reload if periodic else 0
            elif count:
                count -= 1
            if v["configure"]:
                periodic, enable = v["periodic_in"], v["irq_enable_in"]
            expired = int(expire or (bool(expired) and not v["clear"]))
        out.append(
            {
                "count": count,
                "periodic": periodic,
                "irq_enable": enable,
                "expired": expired,
                "irq": expired & enable,
            }
        )
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(
        [v for _, v in cases], model(cases), name, [label for label, _ in cases]
    )


def idle(n: int) -> list[Case]:
    return [case("idle") for _ in range(n)]


# The demo runs the timer once for 3 cycles, with the interrupt disabled, so only the
# flag is set. Then it enables the interrupt, which comes at once, and clears the flag.
# Then it runs periodically every 4 cycles, and stops it with a count of 0.
DEMO = [
    case("reset", reset=True),
    case("start 3", start=3),
    *idle(4),
    case("enable", configure=(0, 1)),
    case("idle"),
    case("clear", clear=True),
    case("periodic", configure=(1, 1)),
    case("start 4", start=4),
    *idle(3),
    case("clear", clear=True),
    *idle(3),
    case("stop", start=0),
    *idle(3),
]


def test_random() -> None:
    rng = random.Random(1)
    cases = [case("reset", reset=True)]
    for _ in range(5000):
        start = None
        if rng.random() < 0.03:
            start = rng.choice([0, 1, 2, 3, rng.randrange(20), rng.getrandbits(32)])
        configure = None
        if rng.random() < 0.03:
            configure = (rng.getrandbits(1), rng.getrandbits(1))
        clear = rng.random() < 0.05
        reset = rng.random() < 0.002
        cases.append(case("random", start, configure, clear, reset))
    check(cases, "random")


def test_one_cycle_periodic() -> None:
    """A period of 1 expires in every cycle, and a clear in the same cycle loses."""
    cases = [case("reset", reset=True), case("periodic", configure=(1, 1))]
    cases += [case("start 1", start=1), *idle(3), case("clear", clear=True), *idle(2)]
    check(cases, "one_cycle_periodic")


def test_demo() -> None:
    BENCH.demo([v for _, v in DEMO], model(DEMO), [label for label, _ in DEMO])
