"""Unit test of cpu_write_align, against the writes of the ISS."""

import random
from itertools import product
from pathlib import Path

from b32sim.sim import MASK, write_lanes

from ...unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="cpu_write_align",
    folder=Path(__file__).parent,
    inputs=["data", "addr", "mem_byte", "mem_half"],
    outputs=["write_data", "byte_enable"],
)

# Write instruction -> bytes
WRITES = {"write": 4, "writeb": 1, "writeh": 2}

EDGE_DATA = [0, MASK, 0x000000FF, 0xFFFFFF00, 0x12345678]

# The demo writes the same data with every size at every place. The last two writes
# are unaligned, so they write at +0.
DEMO = [
    ("write", 0),
    ("writeb", 0),
    ("writeb", 1),
    ("writeb", 2),
    ("writeb", 3),
    ("writeh", 0),
    ("writeh", 2),
    ("writeh", 1),
    ("write", 2),
]
DEMO_DATA = 0x11223344

Case = tuple[str, int, int]  # the write, address bits [1:0], the data


def model(write: str, addr: int, data: int) -> Vector:
    """The ISS's write, at the address aligned down as the ISS does."""
    size = WRITES[write]
    write_data, byte_enable = write_lanes(data, addr & ~(size - 1), size)
    return {"write_data": write_data, "byte_enable": byte_enable}


def vectors(cases: list[Case]) -> list[Vector]:
    return [
        {
            "data": data,
            "addr": addr,
            "mem_byte": int(WRITES[write] == 1),
            "mem_half": int(WRITES[write] == 2),
        }
        for write, addr, data in cases
    ]


def expected(cases: list[Case]) -> list[Vector]:
    return [model(*case) for case in cases]


def test_edges() -> None:
    """Every write at every byte of the word, aligned or not, with the edge data."""
    cases = list(product(WRITES, range(4), EDGE_DATA))
    BENCH.check(vectors(cases), expected(cases), "edges")


def test_random() -> None:
    rng = random.Random(1)
    cases = [
        (rng.choice(list(WRITES)), rng.randrange(4), rng.getrandbits(32))
        for _ in range(2000)
    ]
    BENCH.check(vectors(cases), expected(cases), "random")


def test_demo() -> None:
    cases = [(write, addr, DEMO_DATA) for write, addr in DEMO]
    labels = [f"{write} +{addr}" for write, addr in DEMO]
    BENCH.demo(vectors(cases), expected(cases), labels)
