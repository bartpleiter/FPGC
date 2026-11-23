"""Tests on the board (system decision 5): programs that the boot ROM loads and runs,
compared with the ISS or with what they must print. Run with `fpgc board test`, with
the current `fpgc system build` programmed on the board (`fpgc board program`).

The random programs and the cache programs run in board mode (board_mode.py), and must
print the same result line as in the ISS. That's the ISS comparison at 100 MHz against
the real SDRAM. --seeds sets the number of random programs, so a soak run is a large
number of seeds. The ISS runs the random programs ahead of the board, in a process per
CPU core, as it's slower than the board.
"""

import multiprocessing
import re
from collections.abc import Iterator
from concurrent.futures import Future, ProcessPoolExecutor
from pathlib import Path

import pytest

from fpgc.board import BoardLink
from fpgc.loader import program_bytes

from ..cpu.iss_comparison import RESET_COMMENT
from ..cpu.random_program import generate
from .board_mode import board_mode, expected

REPO = Path(__file__).resolve().parents[2]
PROGRAMS = REPO / "Software/programs"
CACHE_DIR = REPO / "Tests/cpu/cache"
BOOT_LINES = b"boot: err 00000000 pc 00000000\r\nboot: ready\r\n"
REGIONS = frozenset({"sdram", "rom", "cpu_io"})  # no device registers on the board
TIMEOUT = 5.0  # seconds for a short program

# The cache programs, without those that need a reset in the middle, and one that jumps
# into the ROM
CACHE_PROGRAMS = sorted(
    p
    for p in CACHE_DIR.glob("*.asm")
    if not RESET_COMMENT.search(p.read_text()) and p.stem != "redirect_to_rom"
)


def load(name: str, source: str) -> bytes:
    return program_bytes(source, name)


def test_hello(board: BoardLink) -> None:
    hello = load("hello", (PROGRAMS / "hello.asm").read_text())
    assert board.run(hello, TIMEOUT) == b"hello, world\r\n" + BOOT_LINES


def test_retry(board: BoardLink) -> None:
    """A program with a flipped bit gets `boot: bad`, and loads at the next try."""
    hello = load("hello", (PROGRAMS / "hello.asm").read_text())
    before = len(board.received)
    assert board.run(hello, TIMEOUT, corrupt=True) == b"hello, world\r\n" + BOOT_LINES
    assert b"boot: bad " in board.received[before:]


def test_after_reset(board: BoardLink) -> None:
    """A program also loads after a reset, not only after a program that returned."""
    hello = load("hello", (PROGRAMS / "hello.asm").read_text())
    assert board.reset()
    assert board.run(hello, TIMEOUT) == b"hello, world\r\n" + BOOT_LINES


def test_memtest(board: BoardLink) -> None:
    """The memory test over all 64 MiB behind it."""
    memtest = load("memtest", (PROGRAMS / "memtest.asm").read_text())
    assert board.run(memtest, 120.0) == (
        b"memtest: pass 00000000 done, errors 00000000\r\n"
        b"memtest: pass 00000001 done, errors 00000000\r\n"
        b"memtest: pass 00000002 done, errors 00000000\r\n"
        b"memtest: done, errors 00000000\r\n" + BOOT_LINES
    )


def xorshift_steps(value: int, steps: int) -> int:
    for _ in range(steps):
        value ^= (value << 13) & 0xFFFFFFFF
        value ^= value >> 17
        value ^= (value << 5) & 0xFFFFFFFF
    return value


def test_interrupts(board: BoardLink) -> None:
    """The timer interrupts a computation about once per 1,000 cycles: its result is
    the same as without interrupts, and the interrupts came. Then a reset, as the
    interrupts leave INTID set, which only a reset clears, and the random programs
    read it."""
    program = load("interrupts", (Path(__file__).parent / "interrupts.asm").read_text())
    output = board.run(program, TIMEOUT)
    assert board.reset()
    found = re.fullmatch(
        rb"result ([0-9a-f]{8}) interrupts ([0-9a-f]{8})\r\n" + re.escape(BOOT_LINES),
        output,
    )
    assert found, output
    assert int(found.group(1), 16) == xorshift_steps(0x12345678, 100_000)
    assert int(found.group(2), 16) > 500


@pytest.mark.parametrize("path", CACHE_PROGRAMS, ids=lambda p: p.stem)
def test_cache_program(board: BoardLink, path: Path) -> None:
    program = load(path.stem, board_mode(path.read_text()))
    assert board.run(program, TIMEOUT) == expected(program) + BOOT_LINES


def random_case(seed: int) -> tuple[bytes, bytes]:
    """The random program of `seed` in board mode, and what it prints in the ISS."""
    program = load(f"random_{seed}", board_mode(generate(seed, regions=REGIONS)))
    return program, expected(program)


@pytest.fixture(scope="session")
def random_cases(
    request: pytest.FixtureRequest,
) -> Iterator[dict[int, Future[tuple[bytes, bytes]]]]:
    """Every random program of the run, computed ahead in a pool of processes."""
    seeds = range(request.config.getoption("seeds"))
    context = multiprocessing.get_context("fork")
    with ProcessPoolExecutor(mp_context=context) as pool:
        futures = {seed: pool.submit(random_case, seed) for seed in seeds}
        yield futures
        for future in futures.values():
            future.cancel()


def test_random(
    board: BoardLink,
    random_cases: dict[int, Future[tuple[bytes, bytes]]],
    random_seed: int,
) -> None:
    program, want = random_cases[random_seed].result()
    assert board.run(program, TIMEOUT) == want + BOOT_LINES
