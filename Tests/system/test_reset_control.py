"""Unit test of reset_control, against a model of its synchronizers and the PC's
1 ms filter (5 cycles here)."""

import random
from pathlib import Path

from .unit_bench import UnitBench, Vector

PC_RESET_CYCLES = 5

BENCH = UnitBench(
    unit="reset_control",
    folder=Path(__file__).parent,
    inputs=["locked", "button_n", "rts_n", "dtr_n"],
    outputs=["reset"],
)

Case = tuple[str, Vector]  # label, inputs


def case(
    label: str,
    locked: int = 1,
    pressed: bool = False,
    rts: bool = False,
    dtr: bool = False,
) -> Case:
    """The pins, with the button and the PC's two lines as asserted or not."""
    return label, {
        "locked": locked,
        "button_n": int(not pressed),
        "rts_n": int(not rts),
        "dtr_n": int(not dtr),
    }


def model(cases: list[Case]) -> list[Vector]:
    """The reset after each cycle's clock edge. Everything starts as after the
    FPGA's configuration: the synchronizers 0, the count 0."""
    sync = [0, 0, 0, 0]
    pins = [0, 0, 0, 0]
    count = 0
    out = []
    for _, v in cases:
        stable, button_n, rts_n, dtr_n = pins
        pc_reset = count == PC_RESET_CYCLES
        reset = int(not stable or not button_n or pc_reset)
        pc = not rts_n and dtr_n
        count = 0 if not pc else count + (not pc_reset)
        pins = sync
        sync = [v["locked"], v["button_n"], v["rts_n"], v["dtr_n"]]
        out.append({"reset": reset})
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(
        [v for _, v in cases], model(cases), name, [label for label, _ in cases]
    )


def hold(label: str, n: int, **pins: int | bool) -> list[Case]:
    return [case(label, **pins) for _ in range(n)]  # type: ignore[arg-type]


# The demo waits for the PLL, then presses the button, opens the serial port (RTS and
# DTR together, no reset), holds RTS without DTR for 3 cycles (too short), and then
# long enough for a reset.
DEMO = [
    *hold("not locked", 4, locked=0),
    *hold("locked", 5),
    *hold("button", 2, pressed=True),
    *hold("idle", 4),
    *hold("open port", 8, rts=True, dtr=True),
    *hold("idle", 3),
    *hold("rts short", 3, rts=True),
    *hold("idle", 3),
    *hold("rts", 10, rts=True),
    *hold("idle", 6),
]


def test_random() -> None:
    rng = random.Random(1)
    cases: list[Case] = []
    for _ in range(300):
        pins = {
            "locked": int(rng.random() < 0.9),
            "pressed": rng.random() < 0.1,
            "rts": rng.random() < 0.5,
            "dtr": rng.random() < 0.3,
        }
        cases += hold("random", rng.choice([1, 1, 2, 3, 6, 9]), **pins)
    check(cases, "random")


def test_lines_change_a_cycle_apart() -> None:
    """Opening and closing the port with RTS and DTR a cycle apart doesn't reset."""
    cases = [*hold("idle", 6), case("open", rts=True)]
    cases += [*hold("open", 20, rts=True, dtr=True), case("close", rts=True)]
    cases += hold("idle", 6)
    check(cases, "a_cycle_apart")
    assert not any(o["reset"] for o in model(cases)[5:])


def test_demo() -> None:
    BENCH.demo([v for _, v in DEMO], model(DEMO), [label for label, _ in DEMO])
