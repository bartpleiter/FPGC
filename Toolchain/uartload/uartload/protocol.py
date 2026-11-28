"""The boot ROM's loading protocol, without the serial port.

The PC sends "L", the length of the program in bytes, the program, and its CRC-32,
each number lowest byte first. The boot ROM writes the program to SDRAM from address
0, and answers "boot: ok <crc>" and runs it, or "boot: bad <crc>".
"""

import zlib

READY = b"boot: ready"  # the line with which the boot ROM waits for a command
LOAD = b"L"
MAX_LENGTH = 64 * 1024 * 1024  # the SDRAM


def load_message(program: bytes) -> bytes:
    """What the PC sends to load and run `program`."""
    if len(program) % 4:
        raise ValueError(f"a program is whole words, not {len(program)} bytes")
    if len(program) > MAX_LENGTH:
        raise ValueError(f"a program is at most 64 MiB, not {len(program)} bytes")
    length = len(program).to_bytes(4, "little")
    return LOAD + length + program + crc(program).to_bytes(4, "little")


def crc(program: bytes) -> int:
    """The CRC-32 that the boot ROM computes, the same as zlib's."""
    return zlib.crc32(program)


def ok_line(program: bytes) -> bytes:
    """The boot ROM's answer when the program arrived whole."""
    return f"boot: ok {crc(program):08x}".encode()
