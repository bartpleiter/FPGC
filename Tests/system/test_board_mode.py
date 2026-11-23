"""The board tests' programs (Tests/board) before the board: in board mode, every one
gives its result line in the ISS, and in the system simulation (the CPU, the SDRAM
controller and the chip models) the same line as in the ISS."""

import re
from pathlib import Path

import pytest

from fpgc.loader import load_message, ok_line, program_bytes

from ..board.board_mode import board_mode, expected
from ..board.test_board import CACHE_PROGRAMS, REGIONS, xorshift_steps
from ..cpu.random_program import generate
from .test_fpgc import BOOT_LINES, BOOTED, BYTE, run, sent

BOARD_DIR = Path(__file__).parents[1] / "board"
RESULT = re.compile(rb"result [0-9a-f]{8}\r\n")


@pytest.mark.parametrize("path", CACHE_PROGRAMS, ids=lambda p: p.stem)
def test_cache_program_in_iss(path: Path) -> None:
    program = program_bytes(board_mode(path.read_text()), path.stem)
    assert RESULT.fullmatch(expected(program))


@pytest.mark.parametrize("seed", range(5))
def test_random_in_iss(seed: int) -> None:
    program = program_bytes(board_mode(generate(seed, regions=REGIONS)), "random")
    assert RESULT.fullmatch(expected(program))


def run_loaded(name: str, program: bytes, cycles: int) -> bytes:
    """Load `program` in the system simulation, and return what it printed."""
    message = load_message(program)
    r = run(name, BOOTED + len(message) * BYTE + cycles, sent(message, BOOTED))
    head = BOOT_LINES + ok_line(program) + b"\r\n"
    assert r.output.startswith(head) and r.output.endswith(BOOT_LINES), r.output
    return r.output[len(head) : -len(BOOT_LINES)]


def test_random_in_system() -> None:
    program = program_bytes(board_mode(generate(0, regions=REGIONS)), "random")
    assert run_loaded("board_random", program, 300_000) == expected(program)


def test_interrupts_in_system() -> None:
    """The interrupt program with 3,000 steps instead of 100,000."""
    source = (BOARD_DIR / "interrupts.asm").read_text()
    source = source.replace("load32 100000 r2", "load32 3000 r2")
    output = run_loaded(
        "board_interrupts", program_bytes(source, "interrupts"), 100_000
    )
    found = re.fullmatch(rb"result ([0-9a-f]{8}) interrupts ([0-9a-f]{8})\r\n", output)
    assert found, output
    assert int(found.group(1), 16) == xorshift_steps(0x12345678, 3000)
    assert int(found.group(2), 16) > 15
