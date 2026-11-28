"""The board: programming the FPGA over JTAG, and talking to it over the UART."""

import sys
import threading
from pathlib import Path

import click
import serial

from b32asm.assembler import AsmError, assemble
from fpgc.common import run
from fpgc.config import CONFIG
from uartload.board import Board, LoadError

cfg = CONFIG.board


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


@board.command()
@click.option("--reset", "do_reset", is_flag=True, help="Reset the board first.")
@click.option("--port", default=cfg.port, show_default=True, help="The serial port.")
def console(do_reset: bool, port: str) -> None:
    """Print what the board sends, and send each line that's typed, with CR LF.

    Opening and closing the port doesn't reset the board. Stop with Ctrl-C.
    """
    with serial.Serial(port, cfg.baud, timeout=0.1) as link:
        if do_reset:
            Board(link).reset()
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


@board.command("run")
@click.argument("source", type=click.Path(exists=True, dir_okay=False, path_type=Path))
@click.option("--port", default=cfg.port, show_default=True, help="The serial port.")
@click.option(
    "--timeout",
    default=60.0,
    show_default=True,
    help="Seconds to wait for the program to return to the boot ROM.",
)
def run_program(source: Path, port: str, timeout: float) -> None:
    """Assemble SOURCE for address 0, load it with uartload, and show what it sends
    until it returns to the boot ROM.

    Fails if the program doesn't load in 3 tries, or doesn't return in time.
    """
    try:
        program = assemble(source.read_text(), 0, str(source)).to_bytes()
        with serial.Serial(port, cfg.baud, timeout=0.1) as link:
            Board(link).run(program, timeout)
    except KeyboardInterrupt:
        pass
    except (AsmError, LoadError) as e:
        raise click.ClickException(str(e)) from None


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
