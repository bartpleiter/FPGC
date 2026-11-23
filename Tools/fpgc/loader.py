"""The boot ROM's loading protocol on the PC's side (see the Boot ROM page)."""

import zlib

from b32asm.assembler import assemble

READY = b"boot: ready"  # the line with which the boot ROM waits for a command
LOAD = b"L"


def program_bytes(source: str, name: str) -> bytes:
    """A program assembled for address 0 of SDRAM, where the boot ROM loads it."""
    return assemble(source, 0, name).to_bytes()


def load_message(program: bytes) -> bytes:
    """What the PC sends to load and run `program`: "L", the length, the program and
    its CRC-32, each word lowest byte first."""
    assert len(program) % 4 == 0, "a program is whole words"
    length = len(program).to_bytes(4, "little")
    return LOAD + length + program + crc(program).to_bytes(4, "little")


def crc(program: bytes) -> int:
    """The CRC-32 of zlib, which the boot ROM computes bit by bit."""
    return zlib.crc32(program)


def ok_line(program: bytes) -> bytes:
    """The boot ROM's answer when the program arrived whole."""
    return f"boot: ok {crc(program):08x}".encode()
