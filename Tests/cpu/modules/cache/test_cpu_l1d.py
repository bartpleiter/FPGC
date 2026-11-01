"""Unit test of cpu_l1d, against a Python model of its arrays and its two ports."""

import random
from pathlib import Path

import pytest

from ...unit_bench import UnitBench, Vector

INPUTS = [
    "a_addr",
    "a_write",
    "a_byte_enable",
    "a_write_data",
    "a_entry_write",
    "a_entry_write_value",
    "b_addr",
    "b_write",
    "b_write_data",
    "b_entry_write",
    "b_entry_write_value",
]


def bench(lines: int) -> UnitBench:
    return UnitBench(
        unit="cpu_l1d",
        folder=Path(__file__).parent,
        inputs=INPUTS,
        outputs=["a_word", "a_entry", "b_word", "b_entry"],
        parameters={"LINES": lines},
    )


Case = tuple[str, Vector]  # label, inputs


def case(
    label: str,
    a: int = 0,
    a_write: tuple[int, int] | None = None,
    a_entry: int | None = None,
    b: int = 0,
    b_write: int | None = None,
    b_entry: int | None = None,
) -> Case:
    """What both ports do in a cycle. Each port reads at its address (index * 8 + word
    in the line), and can write the word there (port A with byte enables), and the
    entry of its line.
    """
    return label, {
        "a_addr": a,
        "a_write": int(a_write is not None),
        "a_byte_enable": a_write[0] if a_write else 0,
        "a_write_data": a_write[1] if a_write else 0,
        "a_entry_write": int(a_entry is not None),
        "a_entry_write_value": a_entry or 0,
        "b_addr": b,
        "b_write": int(b_write is not None),
        "b_write_data": b_write or 0,
        "b_entry_write": int(b_entry is not None),
        "b_entry_write_value": b_entry or 0,
    }


def merge(word: int, data: int, byte_enable: int) -> int:
    """`word` with the bytes of `data` that `byte_enable` selects."""
    for lane in range(4):
        if byte_enable >> lane & 1:
            mask = 0xFF << 8 * lane
            word = word & ~mask | data & mask
    return word


def model(cases: list[Case], lines: int) -> list[Vector]:
    """The outputs after each clock edge.

    Each port reads at every edge. A port that writes reads what it writes: for port A,
    the word with the written bytes. Before the vectors, both ports read address 0.
    """
    data = [0] * (8 * lines)
    tags = [0] * lines
    out = []
    for _, v in cases:
        a, b = v["a_addr"], v["b_addr"]
        a_word = data[a]
        if v["a_write"]:
            a_word = merge(a_word, v["a_write_data"], v["a_byte_enable"])
        b_word = v["b_write_data"] if v["b_write"] else data[b]
        a_entry = v["a_entry_write_value"] if v["a_entry_write"] else tags[a >> 3]
        b_entry = v["b_entry_write_value"] if v["b_entry_write"] else tags[b >> 3]
        if v["a_write"]:
            data[a] = a_word
        if v["b_write"]:
            data[b] = b_word
        tags[a >> 3] = a_entry
        tags[b >> 3] = b_entry
        out.append(
            {"a_word": a_word, "a_entry": a_entry, "b_word": b_word, "b_entry": b_entry}
        )
    return out


def check(cases: list[Case], name: str, lines: int) -> None:
    __tracebackhide__ = True
    bench(lines).check(
        [v for _, v in cases], model(cases, lines), name, [label for label, _ in cases]
    )


def random_cases(lines: int, count: int, rng: random.Random) -> list[Case]:
    """Random reads and writes on both ports, which never touch the same line in a cycle
    when one of them writes.

    What a port reads while the other port writes the same word or entry isn't defined,
    and two writes of the same word at once can't happen in the cache.
    """
    entry_bits = 23 - (lines.bit_length() - 1)
    used = rng.sample(range(lines), min(lines, 4))
    cases = []
    for _ in range(count):
        a = rng.choice(used) * 8 + rng.randrange(8)
        b = rng.choice(used) * 8 + rng.randrange(8)
        a_write = a_entry = b_write = b_entry = None
        if rng.random() < 0.3:
            a_write = (rng.randrange(16), rng.getrandbits(32))
        if rng.random() < 0.15:
            a_entry = rng.getrandbits(entry_bits)
        if rng.random() < 0.3:
            b_write = rng.getrandbits(32)
        if rng.random() < 0.15:
            b_entry = rng.getrandbits(entry_bits)
        writes = any(w is not None for w in [a_write, a_entry, b_write, b_entry])
        if writes and a >> 3 == b >> 3:
            continue
        cases.append(case("random", a, a_write, a_entry, b, b_write, b_entry))
    return cases


@pytest.mark.parametrize("lines", [128, 4])
def test_random(lines: int) -> None:
    check(random_cases(lines, 4000, random.Random(lines)), f"random_{lines}", lines)


def test_byte_lanes() -> None:
    """Every byte enable, each on a word of 0xFF bytes."""
    cases = []
    for enable in range(16):
        cases.append(case("fill", b=enable, b_write=0xFFFFFFFF))
        cases.append(case("write", a=enable, a_write=(enable, 0x11223344)))
        cases.append(case("read", a=enable, b=8))
    check(cases, "byte_lanes", 128)


# The demo fills two words and the entry {valid, clean, tag 0x42} of line 2 through port
# B, as the cache controller does. Port A reads the word and entry, writes a byte and
# a halfword and makes the line dirty, as a write hit does, and reads what it wrote in
# the same cycle. Port B reads the line back, as for a write-back. At the end port B
# writes a word and a clean entry, and reads them in the same cycle.
DEMO = [
    case("B write 2.0", b=2 * 8, b_write=0xAABBCCDD),
    case("B write 2.1", b=2 * 8 + 1, b_write=0x11223344),
    case("B entry 2", b=2 * 8, b_entry=0b10 << 14 | 0x42),
    case("A read 2.0", a=2 * 8),
    case("A byte 2.0", a=2 * 8, a_write=(0x2, 0x0000EE00), a_entry=0b11 << 14 | 0x42),
    case("A half 2.1", a=2 * 8 + 1, a_write=(0xC, 0x55660000)),
    case("B read 2.0", b=2 * 8),
    case("B read 2.1", b=2 * 8 + 1),
    case("B write 2.1", b=2 * 8 + 1, b_write=0x99999999, b_entry=0b10 << 14 | 0x42),
    case("A read 2.1", a=2 * 8 + 1),
]


def test_demo() -> None:
    bench(128).demo(
        [v for _, v in DEMO], model(DEMO, 128), [label for label, _ in DEMO]
    )
