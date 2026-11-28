"""Serial interface to the FPGC's boot ROM."""

import sys
import time

import serial

from uartload.protocol import READY, crc, load_message, ok_line

RESET_SECONDS = 0.01  # the board resets after 1 ms of RTS without DTR
READY_SECONDS = 1.0  # the boot ROM is ready within this time after a reset
TRIES = 3  # loads before giving up


class LoadError(Exception):
    """The program didn't load, or didn't return to the boot ROM in time."""


class Board:
    """The boot ROM's protocol over an open serial port.

    What the board sends is shown on the terminal as it comes, if `echo`, and kept in
    `received`. `ready` is whether the board's last line was `boot: ready`, as after a
    program that returned to the boot ROM.
    """

    def __init__(self, port: serial.Serial, echo: bool = True) -> None:
        self.port = port
        self.echo = echo
        self.received = bytearray()
        self.partial = b""  # the start of a line that hasn't ended yet
        self.lines: list[bytes] = []  # lines that came, and weren't looked at yet
        self.passed: list[bytes] = []  # the lines that the last wait_for looked at
        self.ready = False

    def wait_for(
        self, prefixes: tuple[bytes, ...], seconds: float | None
    ) -> bytes | None:
        """Read until a line that starts with one of `prefixes`, and return it. None
        after `seconds` without such a line, or never if `seconds` is None. The lines
        it looked at, with their line endings and the one it returns, are in
        `passed`."""
        self.passed = []
        end = None if seconds is None else time.monotonic() + seconds
        while True:
            while self.lines:
                raw = self.lines.pop(0)
                self.passed.append(raw)
                line = raw.rstrip(b"\r\n")
                if line.startswith(prefixes):
                    self.ready = line == READY
                    return line
            if end is not None and time.monotonic() >= end:
                return None
            # What has come, or else wait for one byte, up to the port's timeout
            data = self.port.read(max(1, self.port.in_waiting))
            if data:
                if self.echo:
                    sys.stdout.write(data.decode("ascii", errors="replace"))
                    sys.stdout.flush()
                self.received += data
                *lines, self.partial = (self.partial + data).split(b"\n")
                self.lines += [line + b"\n" for line in lines]

    def reset(self) -> bool:
        """Reset the board, and wait for `boot: ready`. Returns whether it came."""
        self.port.dtr = False
        self.port.rts = True
        time.sleep(RESET_SECONDS)
        self.port.rts = False
        self.ready = False
        self.lines = []
        return self.wait_for((READY,), READY_SECONDS) == READY

    def send(self, message: bytes) -> bytes | None:
        """Send a load message to the boot ROM, which must be at `boot: ready`, and
        return its answer, or None if none came."""
        self.ready = False
        self.port.write(message)
        seconds = 2.0 + len(message) / (self.port.baudrate / 10)
        return self.wait_for((b"boot: ok", b"boot: bad"), seconds)

    def load(self, program: bytes) -> None:
        """Load `program` and start it, with up to TRIES tries. The board is reset
        first, unless it's at `boot: ready`, and before every retry."""
        message = load_message(program)
        for attempt in range(1, TRIES + 1):
            if not self.ready and not self.reset():
                self.note(f"no `boot: ready` after the reset (try {attempt})")
                continue
            answer = self.send(message)
            if answer == ok_line(program):
                return
            self.note(f"the load failed (try {attempt}): {answer!r}")
        raise LoadError(
            f"the program didn't load in {TRIES} tries (CRC {crc(program):08x})"
        )

    def run(self, program: bytes, timeout: float | None = None) -> bytes:
        """Load and run `program`, and wait until it returns to the boot ROM, for at
        most `timeout` seconds. Returns what the board sent after `boot: ok`, up to
        and including `boot: ready`."""
        self.load(program)
        if self.wait_for((READY,), timeout) is None:
            raise LoadError(
                f"the program didn't return to the boot ROM in {timeout:g} s"
            )
        return b"".join(self.passed)

    def note(self, text: str) -> None:
        if self.echo:
            print(f"\nuartload: {text}", file=sys.stderr)
