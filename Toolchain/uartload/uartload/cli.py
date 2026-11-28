"""uartload."""

import sys
from pathlib import Path

import click
import serial

from uartload.board import Board, LoadError


@click.command(context_settings={"help_option_names": ["-h", "--help"]})
@click.argument("program", type=click.Path(exists=True, dir_okay=False, path_type=Path))
@click.option(
    "--port", default="/dev/ttyUSB0", show_default=True, help="The board's serial port."
)
@click.option("--baud", default=1_000_000, show_default=True, help="The UART's speed.")
def main(program: Path, port: str, baud: int) -> None:
    """Load the binary image PROGRAM into the FPGC's SDRAM over UART, and run it.

    Resets the board, sends the program to the boot ROM with its CRC-32, with up to 3
    tries, and then shows what the board sends, until the program returns to the boot
    ROM or Ctrl-C.
    """
    try:
        with serial.Serial(port, baud, timeout=0.1) as link:
            Board(link).run(program.read_bytes())
    except KeyboardInterrupt:
        pass
    except (LoadError, ValueError, serial.SerialException) as e:
        click.echo(f"error: {e}", err=True)
        sys.exit(1)
