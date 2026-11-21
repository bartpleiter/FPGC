"""Unit test of board_io, against a model of the LEDs and the switches' synchronizer."""

import random
from pathlib import Path

from .unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="board_io",
    folder=Path(__file__).parent,
    inputs=["reset", "set_leds", "leds_in", "switch_pins"],
    outputs=["leds", "switches"],
)

Case = tuple[str, Vector]  # label, inputs


def case(
    label: str, leds: int | None = None, switches: int = 0, reset: bool = False
) -> Case:
    """A cycle that sets the LEDs to `leds`, or not, with the switches' pins."""
    return label, {
        "reset": int(reset),
        "set_leds": int(leds is not None),
        "leds_in": leds or 0,
        "switch_pins": switches,
    }


def model(cases: list[Case]) -> list[Vector]:
    """The outputs after each cycle's clock edge. The switches take two cycles."""
    leds = 0
    switches = [0, 0]  # the two synchronizer stages
    out = []
    for _, v in cases:
        if v["reset"]:
            leds = 0
        elif v["set_leds"]:
            leds = v["leds_in"]
        switches = [v["switch_pins"], switches[0]]
        out.append({"leds": leds, "switches": switches[1]})
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(
        [v for _, v in cases], model(cases), name, [label for label, _ in cases]
    )


# The demo turns on two LEDs, and then changes the switches, which show up two cycles
# later.
DEMO = [
    case("reset", reset=True),
    case("set LEDs", leds=0b00101),
    case("idle"),
    case("switches", switches=0b1010),
    case("idle", switches=0b1010),
    case("idle", switches=0b1010),
    case("idle", switches=0b1010),
]


def test_random() -> None:
    rng = random.Random(1)
    cases = [case("reset", reset=True)]  # the LEDs are x until then
    for _ in range(3000):
        leds = rng.getrandbits(5) if rng.random() < 0.3 else None
        reset = rng.random() < 0.02
        cases.append(case("random", leds, rng.getrandbits(4), reset))
    check(cases, "random")


def test_demo() -> None:
    BENCH.demo([v for _, v in DEMO], model(DEMO), [label for label, _ in DEMO])
