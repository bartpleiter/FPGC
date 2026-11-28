"""The SPI buses and the flash model, in the ISS."""

import random

import pytest

from b32sim.devices import BusError, Flash, Spi, spi_buses

BYTE, WORD, CONTROL, STATUS = Spi.BYTE, Spi.WORD, Spi.CONTROL, Spi.STATUS


def command(opcode: int, address: int) -> int:
    """The command and its address as a word: the command goes first."""
    return int.from_bytes(bytes([opcode, *address.to_bytes(3, "big")]), "little")


def run(bus: Spi, *accesses: tuple[int, int | None]) -> list[int]:
    """Writes (reg, value) and reads (reg, None). Returns the words read."""
    words = []
    for reg, value in accesses:
        if value is None:
            words.append(bus.read(reg))
        else:
            bus.write(reg, value)
    return words


def test_jedec_id_and_status() -> None:
    bus = Spi(Flash(), quad=True)
    ident = run(
        bus, (CONTROL, 1), (BYTE, 0x9F), (BYTE, None), (BYTE, None), (BYTE, None)
    )
    run(bus, (CONTROL, 0), (CONTROL, 1), (BYTE, 0x06), (CONTROL, 0))
    status = run(bus, (CONTROL, 1), (BYTE, 0x05), (BYTE, None), (CONTROL, 0))
    assert ident == [0xEF, 0x40, 0x18]
    assert status == [0x02]


def test_program_erase_and_read() -> None:
    data = random.Random(1).randbytes(16)
    words = [int.from_bytes(data[k : k + 4], "little") for k in range(0, 16, 4)]
    bus = Spi(Flash(b"\x00" * 0x2000), quad=True)
    run(bus, (CONTROL, 1), (BYTE, 0x06), (CONTROL, 0))
    run(bus, (CONTROL, 1), (WORD, command(0x20, 0x1000)), (CONTROL, 0))
    run(bus, (CONTROL, 1), (BYTE, 0x06), (CONTROL, 0))
    run(bus, (CONTROL, 1), (WORD, command(0x02, 0x1004)), *((WORD, w) for w in words))
    run(bus, (CONTROL, 0))
    slow = run(bus, (CONTROL, 1), (WORD, command(0x03, 0x1000)), *[(WORD, None)] * 6)
    run(bus, (CONTROL, 0))
    fast = run(bus, (CONTROL, 1), (WORD, command(0x0B, 0x1004)), (BYTE, None))
    fast += run(bus, *[(BYTE, None)] * 4, (CONTROL, 0))
    quad = run(bus, (CONTROL, 1), (WORD, command(0x6B, 0x1004)), (CONTROL, 3))
    quad += run(bus, (WORD, None), *[(WORD, None)] * 4, (CONTROL, 0))
    old = run(bus, (CONTROL, 1), (WORD, command(0x03, 0x0FFC)), (WORD, None))
    assert slow == [0xFFFFFFFF, *words, 0xFFFFFFFF]
    assert fast[1:] == list(data[:4])
    assert quad[1:] == words
    assert old == [0]  # outside the erased sector


def test_program_needs_write_enable() -> None:
    bus = Spi(Flash(), quad=True)
    run(bus, (CONTROL, 1), (WORD, command(0x02, 0)), (BYTE, 0), (CONTROL, 0))
    assert run(bus, (CONTROL, 1), (WORD, command(0x03, 0)), (BYTE, None)) == [0xFF]


def test_buses() -> None:
    """Only the flashes have QUAD; a bus without a device reads 1s; the interrupt
    pin in STATUS; a register that isn't there."""
    buses = spi_buses()
    for n, bus in enumerate(buses):
        bus.write(CONTROL, 3)
        assert bus.read(CONTROL) == (3 if n < 2 else 1)
        bus.reset()
        assert bus.read(CONTROL) == 0
    assert buses[5].read(WORD) == 0xFFFFFFFF
    buses[4].irq_pin = True
    assert buses[4].read(STATUS) == 1
    with pytest.raises(BusError):
        buses[0].read(0x10)
