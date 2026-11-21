"""Unit test of fifo, with 32 bytes, against a model with a deque."""

import random
from collections import deque
from pathlib import Path

from .unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="fifo",
    folder=Path(__file__).parent,
    inputs=["reset", "push", "write_data", "pop"],
    outputs=["read_data", "empty", "full"],
)

DEPTH = 32

Case = tuple[str, Vector]  # label, inputs


def case(
    label: str, push: int | None = None, pop: bool = False, reset: bool = False
) -> Case:
    return label, {
        "reset": int(reset),
        "push": int(push is not None),
        "write_data": push or 0,
        "pop": int(pop),
    }


def model(cases: list[Case]) -> list[Vector]:
    """The outputs after each cycle's clock edge."""
    words: deque[int] = deque()
    out = []
    for _, v in cases:
        read = 0
        if v["reset"]:
            words.clear()
        else:
            popping = v["pop"] and words
            pushing = v["push"] and len(words) < DEPTH
            if popping:
                read = words.popleft()
            if pushing:
                words.append(v["write_data"])
        out.append(
            {
                "read_data": read,
                "empty": int(not words),
                "full": int(len(words) == DEPTH),
            }
        )
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(
        [v for _, v in cases], model(cases), name, [label for label, _ in cases]
    )


def test_random() -> None:
    """Pushes and pops with changing odds, so the FIFO is often empty and often full."""
    rng = random.Random(1)
    cases = [case("reset", reset=True)]
    for block in range(40):
        p_push = [0.2, 0.5, 0.8][block % 3]
        for _ in range(200):
            push = rng.getrandbits(8) if rng.random() < p_push else None
            pop = rng.random() < 1 - p_push
            cases.append(case("step", push, pop))
        if block == 20:
            cases.append(case("reset", reset=True))
    check(cases, "random")


def test_full_and_empty() -> None:
    """A push when full and a pop when empty are ignored, also together with the
    other one."""
    cases = [case("reset", reset=True)]
    cases += [case("push", i) for i in range(DEPTH)]
    cases += [case("push full", 0xAA), case("push & pop", 0xBB, pop=True)]
    cases += [case("pop", pop=True) for _ in range(DEPTH)]
    cases += [case("pop empty", pop=True), case("push & pop", 0xCC, pop=True)]
    cases += [case("pop", pop=True), case("idle")]
    check(cases, "full_and_empty")


# The demo pushes three bytes, pops one, pushes and pops in the same cycle, and pops
# the rest. The pop of an empty FIFO does nothing. Then it fills the FIFO, and the
# push of a full FIFO does nothing.
DEMO = [
    case("reset", reset=True),
    case("push 11", 0x11),
    case("push 22", 0x22),
    case("push 33", 0x33),
    case("pop", pop=True),
    case("push 44, pop", 0x44, pop=True),
    case("pop", pop=True),
    case("pop", pop=True),
    case("pop", pop=True),
    case("idle"),
    *(case(f"push {i:02x}", i) for i in range(DEPTH)),
    case("push full", 0xFF),
    case("pop", pop=True),
    case("idle"),
]


def test_demo() -> None:
    BENCH.demo([v for _, v in DEMO], model(DEMO), [label for label, _ in DEMO])
