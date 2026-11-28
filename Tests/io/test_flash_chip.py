"""Tests of the flash model, flash_chip: the commands give the right data, and each
broken rule gives its error number.

Each test plays the chip's pins as steps (tb_flash_chip.v). The clock is 50 MHz unless
a test says otherwise: 10 ns high and 10 ns low, with IO0 changing at the falling edge.
The chip in the testbench is 64 KiB, with programs of 1 us and erases of 2 us.
"""

import random
from pathlib import Path

import pytest

from ..common.verilog import compile_tb, run_vvp
from .verilog import IO

FOLDER = Path(__file__).parent

# The model's error numbers, as in flash_chip.v
ERRORS = {
    "x": 1,
    "tshsl": 2,
    "tcs": 3,
    "clock": 4,
    "setup": 5,
    "command": 6,
    "busy": 7,
    "wel": 8,
    "abort": 9,
    "wrap": 10,
    "conflict": 11,
}

HALF = 10_000  # ps
PROGRAM = 1_000_000
ERASE = 2_000_000


class Pins:
    """The steps of a test, and the data the chip sends at the rising edges."""

    def __init__(self, half: int = HALF) -> None:
        self.half = half
        self.steps: list[tuple[int, int, int, int, int]] = []
        self.cs_n, self.clk, self.out, self.enable = 1, 0, 0, 0
        self.reads: list[tuple[list[int], bool]] = []  # rising steps, quad

    def step(self, delay: int, **pins: int) -> int:
        self.cs_n = pins.get("cs_n", self.cs_n)
        self.clk = pins.get("clk", self.clk)
        self.out = pins.get("out", self.out)
        self.enable = pins.get("enable", self.enable)
        self.steps.append((delay, self.cs_n, self.clk, self.out, self.enable))
        return len(self.steps) - 1

    def select(self, gap: int = 60_000) -> None:
        self.step(gap, cs_n=0)

    def deselect(self) -> None:
        self.step(self.half, clk=0)
        self.step(self.half, cs_n=1)

    def wait(self, delay: int) -> None:
        self.step(delay)

    def send(self, *data: int) -> None:
        for byte in data:
            for i in range(7, -1, -1):
                self.step(self.half, clk=0, out=(byte >> i) & 1, enable=1)
                self.step(self.half, clk=1)

    def receive(self, count: int, quad: bool = False) -> None:
        """Clock in `count` bytes, sending 1s, or on all four lines without driving
        them."""
        rises = []
        for _ in range(count * (2 if quad else 8)):
            self.step(self.half, clk=0, out=1, enable=0 if quad else 1)
            rises.append(self.step(self.half, clk=1))
        self.reads.append((rises, quad))

    def command(self, *data: int, receive: int = 0, quad: bool = False) -> None:
        """A whole command: select, send, receive, deselect."""
        self.select()
        self.send(*data)
        if receive:
            self.receive(receive, quad)
        self.deselect()


def run(name: str, pins: Pins) -> tuple[str, list[list[int]]]:
    """Play the steps. Returns the output and the bytes of each receive."""
    folder = IO.build_dir / "modules" / "flash_chip" / name
    vvp = compile_tb(
        FOLDER / "tb_flash_chip.v",
        [FOLDER / "flash_chip.v"],
        folder / "tb.vvp",
        include_dirs=IO.include_dirs,
    )
    steps = folder / "steps.txt"
    steps.write_text(
        "".join(f"{d} {c:x} {k:x} {o:x} {e:x}\n" for d, c, k, o, e in pins.steps)
    )
    outputs = folder / "outputs.txt"
    output = run_vvp(vvp, steps=steps, outputs=outputs)
    lines = outputs.read_text().split()
    data = []
    if "flash error" in output:
        return output, data
    for rises, quad in pins.reads:
        bits = [lines[r] for r in rises]
        if quad:
            nibbles = [int(b, 2) if set(b) <= {"0", "1"} else -1 for b in bits]
            values = [(h << 4 | lo) for h, lo in zip(nibbles[::2], nibbles[1::2])]
            if -1 in nibbles:
                values = [-1] * len(values)
        else:
            line = [b[2] for b in bits]  # IO1
            values = []
            for k in range(0, len(line), 8):
                byte = line[k : k + 8]
                values.append(int("".join(byte), 2) if set(byte) <= {"0", "1"} else -1)
        data.append(values)
    return output, data


def ok(name: str, pins: Pins) -> list[list[int]]:
    output, data = run(name, pins)
    assert "done" in output and "error" not in output, output
    return data


def address(a: int) -> tuple[int, int, int]:
    return (a >> 16) & 0xFF, (a >> 8) & 0xFF, a & 0xFF


def program(pins: Pins, at: int, data: bytes) -> None:
    pins.command(0x06)
    pins.command(0x02, *address(at), *data)
    pins.wait(PROGRAM)


def test_jedec_id() -> None:
    pins = Pins()
    pins.command(0x9F, receive=3)
    assert ok("jedec_id", pins) == [[0xEF, 0x40, 0x18]]


def test_status() -> None:
    """Write Enable sets WEL, Write Disable clears it, and the status repeats."""
    pins = Pins()
    pins.command(0x05, receive=2)
    pins.command(0x06)
    pins.command(0x05, receive=2)
    pins.command(0x04)
    pins.command(0x05, receive=1)
    assert ok("status", pins) == [[0, 0], [2, 2], [0]]


def test_program_and_read() -> None:
    """A page program, busy while it runs, then read with 03h, 0Bh and 6Bh."""
    data = random.Random(1).randbytes(40)
    at = 0x1234
    pins = Pins()
    pins.command(0x06)
    pins.command(0x02, *address(at), *data)
    pins.command(0x05, receive=1)  # busy, WEL
    pins.wait(PROGRAM)
    pins.command(0x05, receive=1)  # done, WEL cleared
    pins.command(0x03, *address(at - 2), receive=len(data) + 4)
    pins.command(0x0B, *address(at), 0xFF, receive=len(data))
    pins.command(0x6B, *address(at), receive=1)  # the dummy clocks: 1 byte on 1 line
    status_busy, status_done, read, fast, _ = ok("program_and_read", pins)
    assert status_busy == [3] and status_done == [0]
    assert read == [0xFF, 0xFF, *data, 0xFF, 0xFF]
    assert fast == list(data)


def test_quad_read() -> None:
    """6Bh: 8 dummy clocks, then 2 clocks per byte on all four lines."""
    data = random.Random(2).randbytes(32)
    pins = Pins()
    program(pins, 0x2000, data)
    pins.select()
    pins.send(0x6B, *address(0x2000))
    pins.receive(4, quad=True)  # the 8 dummy clocks
    pins.receive(len(data), quad=True)
    pins.deselect()
    assert ok("quad_read", pins)[1] == list(data)


def test_program_clears_bits() -> None:
    """A program can only clear bits: 0x0F over 0xF0 gives 0x00."""
    pins = Pins()
    program(pins, 0x300, bytes([0xF0, 0xF0]))
    program(pins, 0x300, bytes([0x0F, 0xFF]))
    pins.command(0x03, *address(0x300), receive=2)
    assert ok("program_clears_bits", pins) == [[0x00, 0xF0]]


def test_erase() -> None:
    """A sector erase clears its 4 KiB and nothing else; a block erase its 64 KiB."""
    pins = Pins()
    for at in (0x0FFF, 0x1000, 0x1FFF, 0x2000):
        program(pins, at, bytes([0x55]))
    pins.command(0x06)
    pins.command(0x20, *address(0x1800))
    pins.wait(ERASE)
    for at in (0x0FFF, 0x1000, 0x1FFF, 0x2000):
        pins.command(0x03, *address(at), receive=1)
    pins.command(0x06)
    pins.command(0xD8, *address(0x8000))
    pins.wait(ERASE)
    pins.command(0x03, *address(0x0FFF), receive=1)
    pins.command(0x03, *address(0x2000), receive=1)
    assert ok("erase", pins) == [[0x55], [0xFF], [0xFF], [0x55], [0xFF], [0xFF]]


def broken(rule: str) -> Pins:
    """A sequence that breaks one rule."""
    pins = Pins()
    if rule == "x":
        pins.select()
        pins.step(HALF, clk=0, enable=0)  # IO0 floats
        pins.step(HALF, clk=1)
    elif rule == "tshsl":
        pins.command(0x06)
        pins.select(gap=30_000)  # 50 ns after a Write Enable
    elif rule == "tcs":
        pins.select()
        pins.step(1_000, clk=1)
    elif rule == "clock":
        pins.half = 8_000  # 62.5 MHz: too fast for Read Data
        pins.command(0x03, 0, 0, 0, receive=1)
    elif rule == "setup":
        pins.select()
        pins.step(HALF, out=0, enable=1)
        pins.step(9_500, out=1)
        pins.step(500, clk=1)
    elif rule == "command":
        pins.command(0xAB)
    elif rule == "busy":
        program(pins, 0, bytes([1]))
        pins.steps.pop()  # no wait
        pins.command(0x03, 0, 0, 0, receive=1)
    elif rule == "wel":
        pins.command(0x02, 0, 0, 0, 1)
    elif rule == "abort":
        pins.command(0x06)
        pins.command(0x20, 0, 0)
    elif rule == "wrap":
        pins.command(0x06)
        pins.command(0x02, *address(0x10FF), 1, 2)
    elif rule == "conflict":
        pins.select()
        pins.send(0x03, 0, 0, 0)
        for _ in range(8):
            pins.step(HALF, clk=0, out=0b0001, enable=0b0011)  # drives IO1 low
            pins.step(HALF, clk=1)
    return pins


@pytest.mark.parametrize("rule", ERRORS)
def test_errors(rule: str) -> None:
    output, _ = run(f"error_{rule}", broken(rule))
    assert f"flash error {ERRORS[rule]} " in output, output
