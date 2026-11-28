"""The SPI buses on the board: a first command on every device, and all 16 MiB of
flash 2 written, read back with 6Bh and with 03h, and timed. Run with `fpgc board test
-k spi`, with the current `fpgc system build` programmed on the board."""

import re
from pathlib import Path

from b32asm.assembler import assemble
from uartload.board import Board

PROGRAMS = Path(__file__).parent / "programs"
BOOT_LINES = b"boot: err 00000000 pc 00000000\r\nboot: ready\r\n"
SIZE = 16 << 20
RESULT = re.compile(rb"flash2 (quad|single) errors ([0-9a-f]{8}) cycles ([0-9a-f]{8})")


def load(name: str) -> bytes:
    path = PROGRAMS / f"{name}.asm"
    return assemble(path.read_text(), 0, str(path)).to_bytes()


def test_spi_devices(board: Board) -> None:
    assert board.run(load("spi_devices"), 5.0) == (
        b"flash1 001840ef\r\n"
        b"flash2 001840ef\r\n"
        b"usb1 000000aa\r\n"
        b"usb2 000000aa\r\n"
        b"eth 00000034\r\n"
        b"sd 00000001\r\n" + BOOT_LINES
    )


def test_spi_flash_full(board: Board) -> None:
    """Erases and programs flash 2, which takes about a minute and a half."""
    output = board.run(load("flash_full"), 900.0)
    assert output.startswith(b"flash2 erased\r\nflash2 programmed\r\n"), output
    results = {m[0]: (int(m[1], 16), int(m[2], 16)) for m in RESULT.findall(output)}
    assert results.keys() == {b"quad", b"single"}, output
    for kind, (errors, cycles) in results.items():
        speed = SIZE / (cycles / 100e6) / 1e6
        print(f"{kind.decode()}: {cycles} cycles, {speed:.1f} MB/s")
        assert errors == 0, output
