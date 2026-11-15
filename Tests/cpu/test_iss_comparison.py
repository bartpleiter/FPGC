"""Run programs on the RTL, and compare what they do with the ISS.

The CPU is built one step at a time, so only the programs that use instructions the
RTL already has run here. SUPPORTED grows with each step of the build order, and the
directed programs then join in by themselves.

Every program runs with each of the MEMORIES, and must give the same retire trace with
each of them. The directed programs run from the ROM and from SDRAM, through the L1I.
Their reads and writes of SDRAM go through the L1D either way.
"""

import re
from pathlib import Path

import pytest

from b32asm.assembler import assemble
from b32asm.isa import Opcode, decode
from b32sim.sim import ROM_BASE, SDRAM_BASE

from .iss_comparison import (
    DEFAULT,
    RESET_COMMENT,
    SDRAM_COMMENT,
    Memories,
    RtlTrace,
    run_iss_comparison,
)
from .random_program import BRANCH_OPS, MULDIV_OPS, READ_OPS, WRITE_OPS, generate
from .test_random import ALU_ONLY

# The opcodes the RTL executes, and the mnemonics for the random programs.
SUPPORTED = {Opcode.ARITH, Opcode.ARITHC, Opcode.SAVPC, Opcode.BRANCH, Opcode.JUMP}
SUPPORTED |= {Opcode.JUMPR, Opcode.HALT, Opcode.READ, Opcode.WRITE}
SUPPORTED |= {Opcode.PUSH, Opcode.POP, Opcode.ARITHM, Opcode.ARITHMC}
SUPPORTED |= {Opcode.RETI, Opcode.INTID, Opcode.CCACHE}
RANDOM_ONLY = ALU_ONLY | {*BRANCH_OPS, "jump", "jumpr", "jumpro", "savpc"}
RANDOM_ONLY |= {*READ_OPS, *WRITE_OPS, "push", "pop", *MULDIV_OPS, "readintid", "reti"}
RANDOM_ONLY |= {"ccache", "ccached"}
# The regions with a memory behind the data port. Only SDRAM and the CPU-internal I/O
# yet, so the directed programs that read ROM or video memory wait for the step that
# adds those memories.
RANDOM_REGIONS = frozenset({"sdram", "cpu_io"})
NEEDS_REGION = {"rom_to_ram_copy", "vram_loop_write", "vram_read_write"}

# Each L1 cache with 128 lines, and with 4, so that nearly every line misses and is
# evicted again. Each with the SDRAM at its shortest latency, and at random latencies.
# The random programs get their own seed for the latencies too.
MEMORIES = {
    "lines128": Memories(128, "1"),
    "lines4": Memories(4, "1"),
    "lines128_rand": Memories(128, "rand"),
    "lines4_rand": Memories(4, "rand"),
}

# The directed programs run from the ROM and from SDRAM, each with all of MEMORIES.
RUNS = [(place, mode) for place in ("rom", "sdram") for mode in MEMORIES]

PROGRAMS = sorted((Path(__file__).parent / "asm").rglob("*.asm"))
ERROR_PROGRAMS = sorted((Path(__file__).parent / "errors").glob("*.asm"))
INTERRUPT_PROGRAMS = sorted((Path(__file__).parent / "interrupts").glob("*.asm"))
CACHE_DIR = Path(__file__).parent / "cache"
# The programs with a reset in the middle of the run, and the others
RESET_PROGRAMS = sorted(
    p for p in CACHE_DIR.glob("*.asm") if RESET_COMMENT.search(p.read_text())
)
CACHE_PROGRAMS = sorted(set(CACHE_DIR.glob("*.asm")) - set(RESET_PROGRAMS))


def supported(path: Path) -> bool:
    if path.stem in NEEDS_REGION:
        return False
    words = assemble(path.read_text(), ROM_BASE, str(path)).words
    return all(decode(w).opcode in SUPPORTED for w in words)


def run_name(name: str, place: str, mode: str) -> str:
    return f"{name}_{mode}" if place == "rom" else f"{name}_sdram_{mode}"


def placed(source: str, place: str) -> str:
    """The program, with the comment that makes it run from SDRAM for `place` sdram."""
    return source if place == "rom" else f"{SDRAM_COMMENT}\n{source}"


@pytest.mark.parametrize(("place", "mode"), RUNS, ids=[f"{p}-{m}" for p, m in RUNS])
@pytest.mark.parametrize(
    "path", [p for p in PROGRAMS if supported(p)], ids=lambda p: p.stem
)
def test_program(wave_dir: Path | None, path: Path, place: str, mode: str) -> None:
    name = run_name(path.stem, place, mode)
    vcd = wave_dir / f"{name}.vcd" if wave_dir else None
    source = placed(path.read_text(), place)
    run_iss_comparison(source, name, vcd, memories=MEMORIES[mode])


@pytest.mark.parametrize(("place", "mode"), RUNS, ids=[f"{p}-{m}" for p, m in RUNS])
@pytest.mark.parametrize("path", ERROR_PROGRAMS, ids=lambda p: p.stem)
def test_error_program(
    wave_dir: Path | None, path: Path, place: str, mode: str
) -> None:
    """The programs with illegal cases, compared with the ISS in hardware mode."""
    name = run_name(path.stem, place, mode)
    vcd = wave_dir / f"{name}.vcd" if wave_dir else None
    source = placed(path.read_text(), place)
    run_iss_comparison(source, name, vcd, strict=False, memories=MEMORIES[mode])


@pytest.mark.parametrize("mode", MEMORIES)
@pytest.mark.parametrize("path", INTERRUPT_PROGRAMS, ids=lambda p: p.stem)
def test_interrupt_program(wave_dir: Path | None, path: Path, mode: str) -> None:
    """The programs with interrupts, which run from SDRAM.

    Each says which interrupts it must take, in order: `; interrupts taken: 1 5`.
    """
    name = f"{path.stem}_{mode}"
    vcd = wave_dir / f"{name}.vcd" if wave_dir else None
    source = path.read_text()
    rtl = run_iss_comparison(source, name, vcd, memories=MEMORIES[mode])
    expected = re.search(r"^; interrupts taken: ([\d ]+)$", source, re.MULTILINE)
    assert expected, "no '; interrupts taken:' comment"
    taken = [m.group(1) for r in rtl.records if (m := re.search(r" irq (\d+)", r))]
    assert taken == expected.group(1).split()


@pytest.mark.parametrize("mode", MEMORIES)
def test_int_enable(mode: str) -> None:
    """The interrupt is taken at the jump right behind the write that enables
    interrupts, which is the first place where the ISA allows it."""
    path = Path(__file__).parent / "interrupts" / "int_enable.asm"
    source = path.read_text()
    rtl = run_iss_comparison(
        source, f"int_enable_check_{mode}", memories=MEMORIES[mode]
    )
    enabled = assemble(source, SDRAM_BASE).labels["Enabled"]
    irqs = [r for r in rtl.records if " irq " in r]
    assert len(irqs) == 1 and int(irqs[0].split()[0], 16) == enabled


@pytest.mark.parametrize("mode", MEMORIES)
@pytest.mark.parametrize("path", CACHE_PROGRAMS, ids=lambda p: p.stem)
def test_cache_program(wave_dir: Path | None, path: Path, mode: str) -> None:
    """The programs of the caches, most of which run from SDRAM.

    Each says which lines it reads from SDRAM, in order, as the line number (address
    bits [25:5]) in hex: `; sdram reads: 0 1`, and which lines it writes back, if any:
    `; sdram writes: 800`. That's the same with 128 and with 4 lines, as the programs
    fit in 4 lines of each cache.
    """
    name = f"{path.stem}_{mode}"
    vcd = wave_dir / f"{name}.vcd" if wave_dir else None
    source = path.read_text()
    rtl = run_iss_comparison(source, name, vcd, memories=MEMORIES[mode])
    reads = re.search(r"^; sdram reads: ([\da-f ]+)$", source, re.MULTILINE)
    assert reads, "no '; sdram reads:' comment"
    assert rtl.sdram_reads == [int(line, 16) for line in reads.group(1).split()]
    writes = re.search(r"^; sdram writes: ([\da-f ]+)$", source, re.MULTILINE)
    expected = writes.group(1).split() if writes else []
    assert rtl.sdram_writes == [int(line, 16) for line in expected]


@pytest.mark.parametrize("mode", MEMORIES)
@pytest.mark.parametrize("path", RESET_PROGRAMS, ids=lambda p: p.stem)
def test_reset_program(wave_dir: Path | None, path: Path, mode: str) -> None:
    """The programs with a reset in the middle of the run (`; reset at cycle C`).

    The comparison is of the run after the reset. The reset must come in the delay loop
    of the program: the last instruction that retired before it is one that the program
    retires many times.
    """
    name = f"{path.stem}_{mode}"
    vcd = wave_dir / f"{name}.vcd" if wave_dir else None
    rtl = run_iss_comparison(path.read_text(), name, vcd, memories=MEMORIES[mode])
    assert rtl.reset, "no reset in the run"
    last = rtl.reset.split()[-1]
    assert sum(r.startswith(last) for r in rtl.records) > 10, rtl.reset


def test_clean_l1i_miss() -> None:
    """The micro-benchmark of a clean L1I miss, with B32P3's SDRAM timing (DEFAULT).

    IF waits at most 8 cycles for the first word of the line, which comes first, and
    which IF passes on in the cycle it arrives (l1-cache.md, "Misses" and "Early
    restart"). So the first instruction of line 1 retires at most 9 cycles after the
    last one of line 0.
    """
    path = Path(__file__).parent / "cache" / "clean_miss.asm"
    rtl = run_iss_comparison(path.read_text(), "clean_l1i_miss", memories=DEFAULT)
    pcs = [int(r.split()[0], 16) for r in rtl.records]
    first = pcs.index(0x20)
    assert pcs[first - 1] == 0x1C
    assert rtl.cycles[first] - rtl.cycles[first - 1] - 1 <= 8


def retired_at(rtl: RtlTrace, pc: int, occurrence: int = 1) -> int:
    """The index in the trace of the record of `pc`, the first time or a later one."""
    pcs = [int(r.split()[0], 16) for r in rtl.records]
    return [i for i, p in enumerate(pcs) if p == pc][occurrence - 1]


def waited(rtl: RtlTrace, pc: int, occurrence: int = 1) -> int:
    """The cycles between the retire of `pc` and the one before it, less one.

    An instruction that doesn't wait retires in the cycle after the one before it.
    """
    i = retired_at(rtl, pc, occurrence)
    return rtl.cycles[i] - rtl.cycles[i - 1] - 1


def test_l1d_micro_benchmarks() -> None:
    """The micro-benchmarks of the L1D, with B32P3's SDRAM timing (DEFAULT).

    In the second iteration of l1d_benchmarks.asm, each access waits in MEM for at
    most: a clean miss 8 cycles and a miss with a dirty line 22, as each continues with
    its word as it arrives (l1-cache.md, "Misses" and "Early restart"), the read of the
    next word, which arrives in the cycle after, nothing, a read or write hit nothing,
    and a read right behind a write one cycle (the write-then-access stall). An
    instruction that doesn't wait retires in the cycle after the one before it.
    """
    path = Path(__file__).parent / "cache" / "l1d_benchmarks.asm"
    rtl = run_iss_comparison(path.read_text(), "l1d_benchmarks", memories=DEFAULT)
    assert waited(rtl, 0x18, 2) <= 8  # the clean miss
    assert waited(rtl, 0x1C, 2) == 0  # the next word
    assert waited(rtl, 0x40, 2) == 0  # a read hit
    assert waited(rtl, 0x44, 2) == 0  # a write hit
    assert waited(rtl, 0x48, 2) <= 1  # a read right behind a write
    assert waited(rtl, 0x4C, 2) <= 22  # the miss with a dirty line


def test_early_restart() -> None:
    """Early restart in IF and MEM, with B32P3's SDRAM timing (DEFAULT).

    A word that a stage waits for goes on in the cycle it arrives from SDRAM, and one
    that has passed is read from the cache after the fill. The programs are in cache/.
    """
    cache = Path(__file__).parent / "cache"

    def run(name: str) -> RtlTrace:
        path = cache / f"{name}.asm"
        return run_iss_comparison(path.read_text(), name, memories=DEFAULT)

    # Straight-line code retires one instruction per cycle as the words of its line
    # arrive. The first instruction of the next line waits for the fill to end, then
    # for the first word of its own line (the clean miss).
    rtl = run("follow_fill")
    assert all(waited(rtl, pc) == 0 for pc in range(0x24, 0x40, 4))
    assert waited(rtl, 0x40) <= 10

    # The jump back to word 4, the last word of the fill, takes it as it arrives: one
    # cycle more than the 3 of a taken jump to a hit. Word 5 has passed by then.
    rtl = run("wrap_fill")
    assert waited(rtl, 0x38) == 0
    assert waited(rtl, 0x30) <= 4
    assert waited(rtl, 0x34, 2) > 0

    # The word that arrives while IF is stalled is read from the L1I after the fill.
    rtl = run("lost_word")
    assert waited(rtl, 0x28) == 1  # the late-result stall
    assert waited(rtl, 0x2C) > 0

    # A read of the next word takes it as it arrives, also when the fill wraps around,
    # and a read of a word that has passed reads the L1D after the fill.
    rtl = run("read_bypass")
    assert waited(rtl, 0x0C) == 0
    assert waited(rtl, 0x14) > 0
    assert waited(rtl, 0x1C) == 0


def test_ccache_micro_benchmarks() -> None:
    """The micro-benchmarks of CCACHE, with B32P3's SDRAM timing (DEFAULT).

    With 128 lines, the walk takes 2 cycles per line, and about 15 more for each dirty
    line that it writes back (l1-cache.md, "CCACHE, CCACHED and reset"). The first
    ccache waits in MEM right behind a write, the second right behind the redirect of a
    ccached, which costs 4 cycles from one retire to the next.
    """
    path = Path(__file__).parent / "cache" / "ccache_benchmarks.asm"
    rtl = run_iss_comparison(path.read_text(), "ccache_benchmarks", memories=DEFAULT)
    dirty = retired_at(rtl, 0x1C)
    clean = retired_at(rtl, 0x24)
    assert rtl.cycles[dirty] - rtl.cycles[dirty - 1] - 1 <= 260 + 4 * 15
    assert rtl.cycles[clean] - rtl.cycles[clean - 1] - 4 <= 260


@pytest.mark.parametrize("mode", MEMORIES)
def test_random(wave_dir: Path | None, random_seed: int, mode: str) -> None:
    name = f"random_{random_seed}_{mode}"
    vcd = wave_dir / f"{name}.vcd" if wave_dir else None
    memories = MEMORIES[mode]
    run_iss_comparison(
        generate(
            random_seed, only=RANDOM_ONLY, regions=RANDOM_REGIONS, interrupts=True
        ),
        name,
        vcd,
        memories=Memories(memories.lines, memories.sdram, random_seed),
    )
