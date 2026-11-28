"""Tests for uartload, with a boot ROM that is faked on the host."""

from typing import cast

import pytest
import serial

from uartload.board import Board, LoadError
from uartload.protocol import crc, load_message, ok_line

BOOT_LINES = b"boot: err 00000000 pc 00000000\r\nboot: ready\r\n"


class FakeBoard:
    """A serial port with a boot ROM behind it. The first `bad` loads get
    `boot: bad`, and a program that loads prints "hello" and returns."""

    baudrate = 1_000_000

    def __init__(self, bad: int = 0) -> None:
        self.bad = bad
        self.out = bytearray()  # what the board sends
        self.dtr = False
        self._rts = False
        self.loads = 0

    @property
    def rts(self) -> bool:
        return self._rts

    @rts.setter
    def rts(self, value: bool) -> None:
        if self._rts and not value and not self.dtr:
            self.out += BOOT_LINES
        self._rts = value

    @property
    def in_waiting(self) -> int:
        return len(self.out)

    def read(self, size: int) -> bytes:
        data = bytes(self.out[:size])
        del self.out[:size]
        return data

    def write(self, message: bytes) -> None:
        self.loads += 1
        program = message[5:-4]
        if self.bad:
            self.bad -= 1
            self.out += f"boot: bad {crc(program) ^ 1:08x}\r\nboot: ready\r\n".encode()
        else:
            self.out += ok_line(program) + b"\r\nhello\r\n" + BOOT_LINES


def board(fake: FakeBoard) -> Board:
    return Board(cast(serial.Serial, fake), echo=False)


def test_load_message() -> None:
    program = bytes([1, 2, 3, 4, 5, 6, 7, 8])
    message = load_message(program)
    assert message[:5] == b"L\x08\x00\x00\x00"
    assert message[5:13] == program
    assert int.from_bytes(message[13:], "little") == crc(program)


def test_load_message_needs_whole_words() -> None:
    with pytest.raises(ValueError, match="whole words"):
        load_message(b"abc")


def test_crc_is_the_boot_roms() -> None:
    """The boot ROM computes the CRC-32 one bit at a time, as here."""
    program = b"hello, world\r\n\0\0"
    value = 0xFFFFFFFF
    for byte in program:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xEDB88320 if value & 1 else 0)
    assert value ^ 0xFFFFFFFF == crc(program)


def test_run() -> None:
    fake = FakeBoard()
    assert board(fake).run(bytes(4), 1.0) == b"hello\r\n" + BOOT_LINES
    assert fake.loads == 1


def test_retry() -> None:
    fake = FakeBoard(bad=2)
    assert board(fake).run(bytes(4), 1.0) == b"hello\r\n" + BOOT_LINES
    assert fake.loads == 3


def test_gives_up() -> None:
    with pytest.raises(LoadError, match="3 tries"):
        board(FakeBoard(bad=3)).load(bytes(4))
