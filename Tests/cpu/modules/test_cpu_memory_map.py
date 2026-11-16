"""Unit test of cpu_memory_map, against the regions of the ISS."""

import random
from pathlib import Path

from b32sim.sim import (
    CPU_IO_BASE,
    CPU_IO_SIZE,
    IO_BASE,
    IO_SIZE,
    MASK,
    ROM_BASE,
    ROM_SIZE,
    SDRAM_BASE,
    SDRAM_SIZE,
)

from ..unit_bench import UnitBench, Vector

# Region name -> its ranges (base, size), the names as the outputs of cpu_memory_map.
# The bus has the memories at 64 to 128 MiB, the ROM included, and the device registers.
# The device bridge finds what's unused there, so the CPU sends all of it to the bus.
REGIONS = {
    "sdram": [(SDRAM_BASE, SDRAM_SIZE)],
    "rom": [(ROM_BASE, ROM_SIZE)],
    "bus": [(0x04000000, 0x04000000), (IO_BASE, IO_SIZE)],
    "cpu_io": [(CPU_IO_BASE, CPU_IO_SIZE)],
}

BENCH = UnitBench(
    unit="cpu_memory_map",
    folder=Path(__file__).parent,
    inputs=["addr"],
    outputs=[*REGIONS, "code", "unmapped"],
)

# One address in every region, and the first address after most of them.
DEMO = [
    ("sdram", 0x00010000),
    ("sdram end", 0x03FFFFFC),
    ("rom", 0x04000000),
    ("after rom", 0x04001000),
    ("gpu", 0x05000000),
    ("bus end", 0x07FFFFFC),
    ("unmapped", 0x08000000),
    ("devices", 0xFFFF8000),
    ("cpu_io", 0xFFFFFF00),
    ("after cpu_io", 0xFFFFFF14),
]


def model(addr: int) -> Vector:
    out = {
        name: int(any(base <= addr < base + size for base, size in ranges))
        for name, ranges in REGIONS.items()
    }
    out["code"] = out["sdram"] | out["rom"]
    out["unmapped"] = int(not any(out[name] for name in REGIONS))
    return out


def check(addresses: list[int], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check([{"addr": a} for a in addresses], [model(a) for a in addresses], name)


def test_edges() -> None:
    """The bytes around the start and the end of every region, and 0 and all ones."""
    addresses = [0, MASK]
    for base, size in (r for ranges in REGIONS.values() for r in ranges):
        for edge in (base, base + size):
            addresses += [(edge + d) & MASK for d in range(-5, 5)]
    check(addresses, "edges")


def test_random() -> None:
    """Random addresses, and random addresses near or in each region."""
    rng = random.Random(1)
    addresses = [rng.getrandbits(32) for _ in range(500)]
    for base, size in (r for ranges in REGIONS.values() for r in ranges):
        addresses += [
            (base + rng.randrange(-size, 2 * size)) & MASK for _ in range(200)
        ]
    check(addresses, "random")


def test_demo() -> None:
    addresses = [a for _, a in DEMO]
    BENCH.demo(
        [{"addr": a} for a in addresses],
        [model(a) for a in addresses],
        [label for label, _ in DEMO],
    )
