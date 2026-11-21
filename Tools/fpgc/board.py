"""The board: programming the FPGA over JTAG, and talking to it over the UART."""

import sys
import threading
import time

import click
import serial

from fpgc.common import run
from fpgc.config import CONFIG

cfg = CONFIG.board

RESET_SECONDS = 0.01  # the board resets after 1 ms of RTS without DTR


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
