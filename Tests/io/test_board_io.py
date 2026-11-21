"""Unit test of board_io, against a model of the user LED and the switches' synchronizer."""

import random
from pathlib import Path

from .unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="board_io",
    folder=Path(__file__).parent,
    inputs=["reset", "set_led", "led_in", "switch_pins"],
    outputs=["led", "switches"],
)

Case = tuple[str, Vector]  # label, inputs


def case(
    label: str, led: int | None = None, switches: int = 0, reset: bool = False
) -> Case:
    """A cycle that sets the LED to `led`, or not, with the switches' pins."""
    return label, {
        "reset": int(reset),
        "set_led": int(led is not None),
        "led_in": led or 0,
        "switch_pins": switches,
    }


def model(cases: list[Case]) -> list[Vector]:
    """The outputs after each cycle's clock edge. The switches take two cycles."""
    led = 0
    switches = [0, 0]  # the two synchronizer stages
    out = []
    for _, v in cases:
        if v["reset"]:
            led = 0
        elif v["set_led"]:
            led = v["led_in"]
        switches = [v["switch_pins"], switches[0]]
        out.append({"led": led, "switches": switches[1]})
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(
        [v for _, v in cases], model(cases), name, [label for label, _ in cases]
    )


# The demo turns on the LED, and then changes the switches, which show up two cycles
# later.
DEMO = [
    case("reset", reset=True),
    case("LED on", led=1),
    case("idle"),
    case("switches", switches=0b1010),
    case("idle", switches=0b1010),
    case("idle", switches=0b1010),
    case("idle", switches=0b1010),
]


def test_random() -> None:
    rng = random.Random(1)
    cases = [case("reset", reset=True)]  # the LED is x until then
    for _ in range(3000):
        led = rng.getrandbits(1) if rng.random() < 0.3 else None
        reset = rng.random() < 0.02
        cases.append(case("random", led, rng.getrandbits(4), reset))
    check(cases, "random")


def test_demo() -> None:
    BENCH.demo([v for _, v in DEMO], model(DEMO), [label for label, _ in DEMO])
