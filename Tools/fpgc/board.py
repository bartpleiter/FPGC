"""The board: programming the FPGA over JTAG, and talking to it over the UART."""

import sys
import threading
import time
from pathlib import Path

import click
import serial

from fpgc.common import run
from fpgc.config import CONFIG
from fpgc.loader import READY, crc, load_message, ok_line, program_bytes

cfg = CONFIG.board

RESET_SECONDS = 0.01  # the board resets after 1 ms of RTS without DTR
READY_SECONDS = 1.0  # the boot ROM says it's ready within this time after a reset
TRIES = 3  # loads before giving up


@click.group()
def board() -> None:
    """Program the board and talk to it."""


@board.command()
def program() -> None:
    """Program the FPGA over JTAG with the last `fpgc system build`.

    The design is lost at power-off: the board's flash isn't changed.
    """
    sof = CONFIG.system.quartus_dir / "output_files" / f"{CONFIG.system.top}.sof"
    if not sof.is_file():
        raise click.UsageError(f"{sof} doesn't exist: run `fpgc system build` first")
    run(
        CONFIG.cpu.quartus_bin / "quartus_pgm",
        "-c",
        cfg.jtag_cable,
        "-m",
        "JTAG",
        "-o",
        f"p;{sof}",
    )


def reset(port: serial.Serial) -> None:
    """Reset the board: RTS on and DTR off, then RTS off again."""
    port.dtr = False
    port.rts = True
    time.sleep(RESET_SECONDS)
    port.rts = False


@board.command()
@click.option("--reset", "do_reset", is_flag=True, help="Reset the board first.")
@click.option("--port", default=cfg.port, show_default=True, help="The serial port.")
def console(do_reset: bool, port: str) -> None:
    """Print what the board sends, and send each line that's typed, with CR LF.

    Opening and closing the port doesn't reset the board. Stop with Ctrl-C.
    """
    with serial.Serial(port, cfg.baud, timeout=0.1) as link:
        if do_reset:
            reset(link)
        threading.Thread(target=_send_lines, args=(link,), daemon=True).start()
        try:
            while True:
                data = link.read(256)
                if data:
                    sys.stdout.write(data.decode("ascii", errors="replace"))
                    sys.stdout.flush()
        except KeyboardInterrupt:
            pass


def _send_lines(link: serial.Serial) -> None:
    for line in sys.stdin:
        link.write(line.rstrip("\r\n").encode("ascii", errors="replace") + b"\r\n")


class BoardLink:
    """The boot ROM's protocol over an open serial port (boot decision 3).

    What the board sends is shown on the terminal as it comes, if `echo`, and kept in
    `received`. `ready` is whether the board's last line was `boot: ready`, as after a
    program that returned to the boot ROM.
    """

    def __init__(self, link: serial.Serial, echo: bool = True) -> None:
        self.link = link
        self.echo = echo
        self.received = bytearray()
        self.partial = b""  # the start of a line that hasn't ended yet
        self.lines: list[bytes] = []  # lines that came, and weren't looked at yet
        self.passed: list[bytes] = []  # the lines that the last wait_for looked at
        self.ready = False

    def wait_for(self, prefixes: tuple[bytes, ...], seconds: float) -> bytes | None:
        """Read until a line that starts with one of `prefixes`, and return it. None
        after `seconds` without such a line. The lines it looked at, with their line
        endings and the one it returns, are in `passed`."""
        self.passed = []
        end = time.monotonic() + seconds
        while True:
            while self.lines:
                raw = self.lines.pop(0)
                self.passed.append(raw)
                line = raw.rstrip(b"\r\n")
                if line.startswith(prefixes):
                    self.ready = line == READY
                    return line
            if time.monotonic() >= end:
                return None
            # What has come, or else wait for one byte, up to the port's timeout
            data = self.link.read(max(1, self.link.in_waiting))
            if data:
                if self.echo:
                    sys.stdout.write(data.decode("ascii", errors="replace"))
                    sys.stdout.flush()
                self.received += data
                *lines, self.partial = (self.partial + data).split(b"\n")
                self.lines += [line + b"\n" for line in lines]

    def reset(self) -> bool:
        """Reset the board, and wait for `boot: ready`. Returns whether it came."""
        reset(self.link)
        self.ready = False
        self.lines = []
        return self.wait_for((READY,), READY_SECONDS) == READY

    def load(self, program: bytes, corrupt: bool = False) -> None:
        """Load `program` and start it, with up to TRIES tries. The board is reset
        first, unless it's at `boot: ready`, and before every retry. With `corrupt`, a
        bit of the program is flipped in the first try."""
        message = load_message(program)
        seconds = 2.0 + len(message) / (cfg.baud / 10)
        for attempt in range(1, TRIES + 1):
            if not self.ready and not self.reset():
                self.note(f"no `boot: ready` after the reset (try {attempt})")
                continue
            self.ready = False
            if corrupt and attempt == 1:
                broken = bytearray(message)
                broken[5] ^= 0x01  # the first byte of the program
                self.link.write(bytes(broken))
            else:
                self.link.write(message)
            answer = self.wait_for((b"boot: ok", b"boot: bad"), seconds)
            if answer == ok_line(program):
                return
            self.note(f"the load failed (try {attempt}): {answer!r}")
        raise click.ClickException(
            f"the program didn't load in {TRIES} tries (CRC {crc(program):08x})"
        )

    def run(self, program: bytes, timeout: float, corrupt: bool = False) -> bytes:
        """Load and run `program`, and wait until it returns to the boot ROM. Returns
        what the board sent after `boot: ok`, up to and including `boot: ready`."""
        self.load(program, corrupt)
        if self.wait_for((READY,), timeout) is None:
            raise click.ClickException(
                f"the program didn't return to the boot ROM in {timeout:g} s"
            )
        return b"".join(self.passed)

    def note(self, text: str) -> None:
        if self.echo:
            print(f"\nfpgc: {text}")


@board.command("run")
@click.argument("source", type=click.Path(exists=True, dir_okay=False, path_type=Path))
@click.option("--port", default=cfg.port, show_default=True, help="The serial port.")
@click.option(
    "--timeout",
    default=60.0,
    show_default=True,
    help="Seconds to wait for the program to return to the boot ROM.",
)
@click.option(
    "--corrupt",
    is_flag=True,
    help="Flip a bit of the program in the first try, to see the retry.",
)
def run_program(source: Path, port: str, timeout: float, corrupt: bool) -> None:
    """Load the program SOURCE into SDRAM over the UART, run it, and show what it
    sends until it returns to the boot ROM.

    Resets the board, waits for `boot: ready`, sends the program with its CRC-32
    (boot decision 3), and checks the answer, with up to 3 tries. Stops at the next
    `boot: ready`, at Ctrl-C, or after the timeout. Fails if the program doesn't load,
    or doesn't return.
    """
    program = program_bytes(source.read_text(), str(source))
    with serial.Serial(port, cfg.baud, timeout=0.1) as link:
        try:
            BoardLink(link).run(program, timeout, corrupt)
        except KeyboardInterrupt:
            pass


@board.command()
@click.option(
    "-k", "pattern", default="", help="Only run tests matching this pytest pattern."
)
@click.option(
    "--seeds",
    type=int,
    default=100,
    show_default=True,
    help="The number of random programs. A soak run is a large number.",
)
def test(pattern: str, seeds: int) -> None:
    """Run the tests on the board (Tests/board), one at a time.

    The board must have the current `fpgc system build` on it (`fpgc board program`).
    """
    run(
        "pytest",
        "Tests/board",
        "--board",
        "-n",
        "0",
        "--seeds",
        str(seeds),
        "-k",
        pattern,
    )
