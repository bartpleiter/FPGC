"""Unit test of activity_leds, against a model of the tick and the two flags per LED.
The tick comes every 4 cycles here."""

import random
from pathlib import Path

from .unit_bench import UnitBench, Vector

TICK_BITS = 2
PERIOD = 1 << TICK_BITS

BENCH = UnitBench(
    unit="activity_leds",
    folder=Path(__file__).parent,
    inputs=["reset", "activity"],
    outputs=["leds"],
)

Case = tuple[str, Vector]  # label, inputs


def case(label: str, activity: int = 0, reset: bool = False) -> Case:
    return label, {"reset": int(reset), "activity": activity}


# The synchronizer has no reset, so a reset of three cycles clears it as well
RESET = [case("reset", reset=True) for _ in range(3)]


def model(cases: list[Case]) -> list[Vector]:
    """The LEDs after each cycle's clock edge. The activity takes two cycles through
    the synchronizer."""
    sync = active = seen = leds = counter = 0
    out = []
    for _, v in cases:
        if v["reset"]:
            counter = seen = leds = 0
        else:
            if counter == PERIOD - 1:
                leds = seen & ~leds & 3
                seen = active
            else:
                seen |= active
            counter = (counter + 1) % PERIOD
        active, sync = sync, v["activity"]
        out.append({"leds": leds})
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(
        [v for _, v in cases], model(cases), name, [label for label, _ in cases]
    )


# The demo gives LED 0 one cycle of activity, which lights it for one period, and LED 1
# continuous activity, which blinks it: one period on, one off, until it stops.
DEMO = [
    *RESET,
    case("pulse", 0b01),
    *[case("idle") for _ in range(10)],
    *[case("busy", 0b10) for _ in range(20)],
    *[case("idle") for _ in range(14)],
]


def test_random() -> None:
    rng = random.Random(1)
    cases = list(RESET)
    for _ in range(3000):
        r = rng.random()
        activity = rng.getrandbits(2) if r < 0.3 else (3 if r < 0.5 else 0)
        cases.append(case("random", activity, rng.random() < 0.005))
    check(cases, "random")


def test_one_cycle_lights_one_period() -> None:
    cases = [*RESET, case("pulse", 1)]
    cases += [case("idle") for _ in range(4 * PERIOD)]
    leds = [o["leds"] for o in model(cases)]
    assert leds.count(1) == PERIOD
    check(cases, "one_cycle")


def test_demo() -> None:
    BENCH.demo([v for _, v in DEMO], model(DEMO), [label for label, _ in DEMO])
