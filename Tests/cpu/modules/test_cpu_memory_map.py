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
    VIDEO_REGIONS,
)

from ..unit_bench import UnitBench, Vector

# Region name -> (base, size), the names as the outputs of cpu_memory_map
REGIONS = {
    "sdram": (SDRAM_BASE, SDRAM_SIZE),
    "io": (IO_BASE, IO_SIZE),
    "rom": (ROM_BASE, ROM_SIZE),
    **VIDEO_REGIONS,
    "cpu_io": (CPU_IO_BASE, CPU_IO_SIZE),
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
    ("after sdram", 0x04000000),
    ("io", 0x1C000000),
    ("rom", 0x1E000000),
    ("after rom", 0x1E001000),
    ("vram32", 0x1E400000),
    ("after vram32", 0x1E401080),
    ("vram8", 0x1E800000),
    ("vrampx", 0x1EC00000),
    ("palette", 0x1EC80000),
    ("cpu_io", 0x1F000000),
    ("after cpu_io", 0x1F000010),
]


def model(addr: int) -> Vector:
    out = {
        name: int(base <= addr < base + size) for name, (base, size) in REGIONS.items()
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
    for base, size in REGIONS.values():
        for edge in (base, base + size):
            addresses += [(edge + d) & MASK for d in range(-5, 5)]
    check(addresses, "edges")


def test_random() -> None:
    """Random addresses, and random addresses near or in each region."""
    rng = random.Random(1)
    addresses = [rng.getrandbits(32) for _ in range(500)]
    for base, size in REGIONS.values():
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
