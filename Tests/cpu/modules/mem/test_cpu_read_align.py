"""Unit test of cpu_read_align, against the reads of the ISS."""

import random
from itertools import product
from pathlib import Path

from b32sim.sim import MASK, read_value

from ...unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="cpu_read_align",
    folder=Path(__file__).parent,
    inputs=["word", "addr", "mem_byte", "mem_half", "sign_extend"],
    outputs=["value"],
)

# Read instruction -> (bytes, sign-extend), as the decoder drives the inputs
READS = {
    "read": (4, False),
    "readb": (1, True),
    "readbu": (1, False),
    "readh": (2, True),
    "readhu": (2, False),
}

# Words with the sign bit of every byte and half set, cleared, or mixed.
EDGE_WORDS = [0, MASK, 0x7F7F7F7F, 0x80808080, 0x7FFF8000, 0x80007FFF, 0x12345678]

# The demo reads from a word with negative bytes and halves, and once from a word with
# positive ones. The last read is unaligned, so it reads the half at +0.
DEMO = [
    ("read", 0, 0xAABBCCDD),
    ("readbu", 0, 0xAABBCCDD),
    ("readb", 0, 0xAABBCCDD),
    ("readbu", 1, 0xAABBCCDD),
    ("readbu", 3, 0xAABBCCDD),
    ("readhu", 2, 0xAABBCCDD),
    ("readh", 2, 0xAABBCCDD),
    ("readb", 1, 0x11223344),
    ("readh", 1, 0xAABBCCDD),
]

Case = tuple[str, int, int]  # the read, address bits [1:0], the word


def model(read: str, addr: int, word: int) -> int:
    """The ISS's read, at the address aligned down as the ISS does."""
    size, signed = READS[read]
    return read_value(word, addr & ~(size - 1), size, signed)


def vectors(cases: list[Case]) -> list[Vector]:
    return [
        {
            "word": word,
            "addr": addr,
            "mem_byte": int(READS[read][0] == 1),
            "mem_half": int(READS[read][0] == 2),
            "sign_extend": int(READS[read][1]),
        }
        for read, addr, word in cases
    ]


def expected(cases: list[Case]) -> list[Vector]:
    return [{"value": model(*case)} for case in cases]


def test_edges() -> None:
    """Every read at every byte of the word, aligned or not, on the edge words."""
    cases = list(product(READS, range(4), EDGE_WORDS))
    BENCH.check(vectors(cases), expected(cases), "edges")


def test_random() -> None:
    rng = random.Random(1)
    cases = [
        (rng.choice(list(READS)), rng.randrange(4), rng.getrandbits(32))
        for _ in range(2000)
    ]
    BENCH.check(vectors(cases), expected(cases), "random")


def test_demo() -> None:
    labels = [f"{read} +{addr}" for read, addr, _ in DEMO]
    BENCH.demo(vectors(DEMO), expected(DEMO), labels)
