"""Unit test of cpu_stack, against a Python list as the stack."""

import random
from pathlib import Path

from b32sim.sim import STACK_DEPTH

from ...unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="cpu_stack",
    folder=Path(__file__).parent,
    inputs=["reset", "push", "push_value", "pop", "sp_write", "sp_write_value"],
    outputs=["sp", "top", "overflow", "underflow"],
)

Case = tuple[str, Vector]  # label, inputs


def case(
    label: str,
    push: int | None = None,
    pop: bool = False,
    sp: int | None = None,
    reset: bool = False,
) -> Case:
    """At most one of a push (with its value), a pop and an SP write (with SP)."""
    return label, {
        "reset": int(reset),
        "push": int(push is not None),
        "push_value": push or 0,
        "pop": int(pop),
        "sp_write": int(sp is not None),
        "sp_write_value": sp or 0,
    }


# The demo pushes three words and pops them, once with a cycle in between the last push
# and the first pop, and once without, which shows the old word. Then it underflows and
# overflows, so SP wraps around at both ends, and it sets SP.
DEMO = [
    case("push 11", push=0x11),
    case("push 22", push=0x22),
    case("push 33", push=0x33),
    case("idle"),
    case("pop", pop=True),
    case("pop", pop=True),
    case("push 44", push=0x44),
    case("pop (old)", pop=True),
    case("pop", pop=True),
    case("pop (under)", pop=True),
    case("idle"),
    case("push (over)", push=0x55),
    case("push 66", push=0x66),
    case("idle"),
    case("write SP 2", sp=2),
    case("pop", pop=True),
    case("reset", reset=True),
]


def model(cases: list[Case]) -> list[Vector]:
    """What the stack gives in each cycle, before the clock edge.

    `top` is the word below SP, except in the cycle after a push, where it's the old
    value of that word.
    """
    stack = [0] * STACK_DEPTH
    sp = 0
    after_push: int | None = None  # the old value, in the cycle after a push
    out = []
    for _, v in cases:
        top = stack[(sp - 1) % STACK_DEPTH] if after_push is None else after_push
        out.append(
            {
                "sp": sp,
                "top": top,
                "overflow": int(bool(v["push"]) and sp == STACK_DEPTH - 1),
                "underflow": int(bool(v["pop"]) and sp == 0),
            }
        )
        after_push = None
        if v["push"]:
            after_push = stack[sp]
            stack[sp] = v["push_value"]
        if v["reset"]:
            sp = 0
        elif v["sp_write"]:
            sp = v["sp_write_value"]
        elif v["push"]:
            sp = (sp + 1) % STACK_DEPTH
        elif v["pop"]:
            sp = (sp - 1) % STACK_DEPTH
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(
        [v for _, v in cases], model(cases), name, [label for label, _ in cases]
    )


def test_fill_and_empty() -> None:
    """Push until SP wraps around, then pop until it wraps back."""
    cases = [case("push", push=0x1000 + n) for n in range(STACK_DEPTH + 2)]
    cases.append(case("idle"))
    cases += [case("pop", pop=True) for _ in range(STACK_DEPTH + 4)]
    check(cases, "fill_and_empty")


def test_random() -> None:
    rng = random.Random(1)
    cases = []
    for _ in range(5000):
        r = rng.random()
        if r < 0.4:
            cases.append(case("push", push=rng.getrandbits(32)))
        elif r < 0.8:
            cases.append(case("pop", pop=True))
        elif r < 0.85:
            cases.append(case("write SP", sp=rng.choice([0, 1, 254, 255, *range(8)])))
        elif r < 0.86:
            cases.append(case("reset", reset=True))
        else:
            cases.append(case("idle"))
    check(cases, "random")


def test_demo() -> None:
    BENCH.demo([v for _, v in DEMO], model(DEMO), [label for label, _ in DEMO])
