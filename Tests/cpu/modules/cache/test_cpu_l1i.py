"""Unit test of cpu_l1i, against a Python model of its two arrays."""

import random
from pathlib import Path

import pytest

from ...unit_bench import UnitBench, Vector

INPUTS = [
    "a_addr",
    "a_enable",
    "b_addr",
    "b_write",
    "b_write_data",
    "b_entry_write",
    "b_entry_write_value",
]


def bench(lines: int) -> UnitBench:
    return UnitBench(
        unit="cpu_l1i",
        folder=Path(__file__).parent,
        inputs=INPUTS,
        outputs=["a_word", "a_entry"],
        parameters={"LINES": lines},
    )


Case = tuple[str, Vector]  # label, inputs


def case(
    label: str,
    read: int | None = None,
    write: tuple[int, int] | None = None,
    entry: tuple[int, int] | None = None,
) -> Case:
    """A read on port A, and a word write and an entry write on port B.

    Addresses are word addresses in the cache (index * 8 + word in the line), and an
    entry write writes the entry of the line of its address.
    """
    return label, {
        "a_addr": read or 0,
        "a_enable": int(read is not None),
        "b_addr": (write or entry or (0, 0))[0],
        "b_write": int(write is not None),
        "b_write_data": write[1] if write else 0,
        "b_entry_write": int(entry is not None),
        "b_entry_write_value": entry[1] if entry else 0,
    }


def model(cases: list[Case], lines: int) -> list[Vector]:
    """The outputs after each clock edge.

    Port A reads when it's enabled, and otherwise keeps its outputs. Before the vectors,
    the testbench reads address 0.
    """
    data = [0] * (8 * lines)
    tags = [0] * lines
    word = entry = 0
    out = []
    for _, v in cases:
        if v["a_enable"]:
            word = data[v["a_addr"]]
            entry = tags[v["a_addr"] >> 3]
        if v["b_write"]:
            data[v["b_addr"]] = v["b_write_data"]
        if v["b_entry_write"]:
            tags[v["b_addr"] >> 3] = v["b_entry_write_value"]
        out.append({"a_word": word, "a_entry": entry})
    return out


def check(cases: list[Case], name: str, lines: int) -> None:
    __tracebackhide__ = True
    bench(lines).check(
        [v for _, v in cases], model(cases, lines), name, [label for label, _ in cases]
    )


def random_cases(lines: int, count: int, rng: random.Random) -> list[Case]:
    """Random reads, holds and writes, which never read what port B writes.

    What port A gives when port B writes the word or the entry it reads in the same
    cycle isn't defined, and the cache never uses it.
    """
    entry_bits = 22 - (lines.bit_length() - 1)
    # A few lines, so reads often find what was written
    used = rng.sample(range(lines), min(lines, 4))
    cases = []
    for _ in range(count):
        read = write = entry = None
        if rng.random() < 0.6:
            read = rng.choice(used) * 8 + rng.randrange(8)
        r = rng.random()
        addr = rng.choice(used) * 8 + rng.randrange(8)
        if r < 0.4 and addr != read:
            write = (addr, rng.getrandbits(32))
        elif r < 0.5 and (read is None or addr >> 3 != read >> 3):
            entry = (addr, rng.getrandbits(entry_bits))
        cases.append(case("random", read, write, entry))
    return cases


@pytest.mark.parametrize("lines", [128, 4])
def test_random(lines: int) -> None:
    check(random_cases(lines, 3000, random.Random(lines)), f"random_{lines}", lines)


# The demo writes two words and the entry {valid, tag 0x1234} of line 3, and reads them.
# Then port A holds its outputs while port B writes the word it read, and reads it again
# after that. The last read is of an empty line.
DEMO = [
    case("write 3.5", write=(3 * 8 + 5, 0x11111111)),
    case("write 3.6", write=(3 * 8 + 6, 0x22222222)),
    case("entry 3", entry=(3 * 8, 1 << 14 | 0x1234)),
    case("read 3.5", read=3 * 8 + 5),
    case("read 3.6", read=3 * 8 + 6),
    case("hold", write=(3 * 8 + 6, 0x33333333)),
    case("hold"),
    case("read 3.6", read=3 * 8 + 6),
    case("read 4.0", read=4 * 8),
]


def test_clock_enable() -> None:
    """A held output keeps its value while the array changes."""
    check(DEMO, "clock_enable", 128)


def test_demo() -> None:
    bench(128).demo(
        [v for _, v in DEMO], model(DEMO, 128), [label for label, _ in DEMO]
    )
