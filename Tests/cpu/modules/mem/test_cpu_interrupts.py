"""Unit test of cpu_interrupts, against a model of the synchronizer, edges and priority."""

import random
from dataclasses import replace
from pathlib import Path

import pytest

from ...unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="cpu_interrupts",
    folder=Path(__file__).parent,
    inputs=["reset", "interrupts", "take"],
    outputs=["pending", "id"],
)

NUMBERS = range(1, 8)

Case = tuple[str, Vector]  # label, inputs


def case(label: str = "", high: tuple[int, ...] = (), take: bool = False) -> Case:
    """One cycle with the inputs in `high` at 1, and maybe a take."""
    lines = sum(1 << (n - 1) for n in set(high))
    return label, {"reset": 0, "interrupts": lines, "take": int(take)}


def reset() -> Case:
    return "reset", {"reset": 1, "interrupts": 0, "take": 0}


def model(cases: list[Case]) -> list[Vector]:
    """What the controller gives in each cycle, before the clock edge.

    An input reaches `sync2` two edges after it's applied, and an edge is seen when
    `sync2` is 1 and was 0 a cycle before. Bit n - 1 is interrupt n.
    """
    sync1 = sync2 = last = pending = 0
    out = []
    for _, v in cases:
        lowest = (pending & -pending).bit_length()
        out.append({"pending": int(pending != 0), "id": lowest})
        rising = sync2 & ~last
        if v["reset"]:
            pending = 0
        else:
            if v["take"] and lowest:
                pending &= ~(1 << (lowest - 1))
            pending |= rising
        sync1, sync2, last = v["interrupts"], sync1, sync2
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(
        [v for _, v in cases], model(cases), name, [label for label, _ in cases]
    )


# The demo raises interrupt 3 and keeps it high, which makes it pending once. Then 1 and
# 2 rise together while 3 is still pending, and the CPU takes all three, in the order
# 1, 2, 3.
DEMO = [
    case("idle"),
    case("3 rises", (3,)),
    *[case("3 high", (3,)) for _ in range(4)],
    case("1, 2 rise", (1, 2, 3)),
    *[case("all high", (1, 2, 3)) for _ in range(3)],
    case("take", (1, 2, 3), take=True),
    case("take", (1, 2, 3), take=True),
    case("take", (1, 2, 3), take=True),
    case("all high", (1, 2, 3)),
    case("all low"),
    case("idle"),
]


def test_held_input_is_one_edge() -> None:
    """An input that stays high makes its interrupt pending only once."""
    cases = [case("rise", (5,)), *[case("high", (5,)) for _ in range(5)]]
    cases += [case("take", (5,), take=True), *[case("high", (5,)) for _ in range(5)]]
    check(cases, "held")


def test_priority() -> None:
    """Every pair of interrupts at the same time: the lower number goes first."""
    cases = []
    for a in NUMBERS:
        for b in NUMBERS:
            cases += [case("rise", (a, b)), *[case() for _ in range(3)]]
            cases += [case("take", take=True), case("take", take=True), case()]
    check(cases, "priority")


def test_edge_while_taken() -> None:
    """An edge in the cycle that its interrupt is taken makes it pending again."""
    cases = []
    for n in NUMBERS:
        for delay in range(5):
            cases += [case("rise", (n,)), case(), case(), case()]
            cases += [case("pulse")] * delay + [case("pulse", (n,))]
            cases += [case() for _ in range(3 - delay)] + [case("take", take=True)]
            cases += [case() for _ in range(3)]
            cases += [case("take", take=True), case()]
    check(cases, "edge_while_taken")


def test_reset() -> None:
    """A reset clears what is pending."""
    cases = [case("rise", (1, 4, 7)), *[case() for _ in range(3)], reset(), case()]
    check(cases, "reset")


def random_cases(numbers: range, count: int) -> list[Case]:
    rng = random.Random(1)
    cases = []
    lines = ()
    for _ in range(count):
        if rng.random() < 0.1:
            lines = tuple(n for n in numbers if rng.random() < 0.3)
        if rng.random() < 0.002:
            cases.append(reset())
        else:
            cases.append(case("random", lines, take=rng.random() < 0.2))
    return cases


def test_random() -> None:
    check(random_cases(NUMBERS, 10000), "random")


@pytest.mark.parametrize("inputs", [1, 4, 12, 255])
def test_number_of_inputs(inputs: int) -> None:
    """The number of inputs is a parameter."""
    bench = replace(BENCH, parameters={"INTERRUPTS": inputs})
    cases = random_cases(range(1, inputs + 1), 3000)
    vectors = [v for _, v in cases]
    bench.check(vectors, model(cases), f"inputs_{inputs}", [lb for lb, _ in cases])


def test_demo() -> None:
    BENCH.demo([v for _, v in DEMO], model(DEMO), [label for label, _ in DEMO])
