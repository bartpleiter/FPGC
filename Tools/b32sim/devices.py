"""Models of the I/O devices and the device bridge, for the ISS.

They follow the RTL in `Hardware/io/` register by register. The ISS has no clock, so
the devices get one tick per instruction: the cycle counter counts instructions, and a
timer of N cycles expires after N instructions. Where that matters, such as in the
ISS comparison, the values come from the RTL instead.

Every device that can interrupt has a level: its event flag while its interrupt is
enabled. The ISS makes an interrupt pending when its level rises.
"""

from typing import Protocol

MASK = 0xFFFFFFFF
SLOT = 256  # bytes per device


class BusError(Exception):
    """An access that nothing takes. The CPU sets the bad-address flag."""


class Device(Protocol):
    """A device behind the bridge, with registers at word offsets of its 256 bytes."""

    def read(self, reg: int) -> int: ...
    def write(self, reg: int, value: int) -> None: ...
    def tick(self) -> None: ...
    def reset(self) -> None: ...
    def irq(self) -> bool: ...


class DeviceBridge:
    """The device registers from 0xFFFF8000: device n at offset 256 * n, words only.

    `interrupts` maps an interrupt number to the device that drives it.
    """

    def __init__(
        self, devices: list[Device | None], interrupts: dict[int, Device] | None = None
    ) -> None:
        self.devices = devices
        self.interrupts = interrupts or {}

    def _device(self, offset: int) -> tuple[Device, int]:
        slot, reg = divmod(offset, SLOT)
        device = self.devices[slot] if slot < len(self.devices) else None
        if device is None:
            raise BusError(f"no device at offset {offset:#x}")
        return device, reg

    def read_word(self, offset: int, size: int = 4) -> int:
        if size != 4:
            raise BusError(f"a {size}-byte read of a device register")
        device, reg = self._device(offset)
        return device.read(reg) & MASK

    def write_word(self, offset: int, value: int, mask: int) -> None:
        if mask != MASK:
            raise BusError("a write of a device register that isn't a word")
        device, reg = self._device(offset)
        device.write(reg, value)

    def tick(self) -> None:
        for device in self.devices:
            if device is not None:
                device.tick()

    def reset(self) -> None:
        for device in self.devices:
            if device is not None:
                device.reset()

    def interrupt_levels(self) -> int:
        """Bit n - 1 is the level of interrupt n."""
        return sum(1 << (n - 1) for n, d in self.interrupts.items() if d.irq())


class Board:
    """The user LED and the DIP switches."""

    LED, SWITCHES = 0x00, 0x04

    def __init__(self, switches: int = 0) -> None:
        self.switches = switches
        self.led = 0

    def read(self, reg: int) -> int:
        if reg == self.LED:
            return self.led
        if reg == self.SWITCHES:
            return self.switches & 0xF
        raise BusError(f"no board register {reg:#x}")

    def write(self, reg: int, value: int) -> None:
        if reg == self.LED:
            self.led = value & 1
        elif reg != self.SWITCHES:
            raise BusError(f"no board register {reg:#x}")

    def tick(self) -> None:
        pass

    def reset(self) -> None:
        self.led = 0

    def irq(self) -> bool:
        return False


class CycleCounter:
    """Counts ticks since reset. A write to SNAPSHOT copies the count."""

    SNAPSHOT, LOW, HIGH = 0x00, 0x04, 0x08

    def __init__(self) -> None:
        self.count = 0
        self.snapshot = 0

    def read(self, reg: int) -> int:
        if reg == self.SNAPSHOT:
            return 0
        if reg == self.LOW:
            return self.snapshot & MASK
        if reg == self.HIGH:
            return self.snapshot >> 32
        raise BusError(f"no cycle counter register {reg:#x}")

    def write(self, reg: int, value: int) -> None:
        if reg == self.SNAPSHOT:
            self.snapshot = self.count
        elif reg not in (self.LOW, self.HIGH):
            raise BusError(f"no cycle counter register {reg:#x}")

    def tick(self) -> None:
        self.count = (self.count + 1) & ((1 << 64) - 1)

    def reset(self) -> None:
        self.count = 0
        self.snapshot = 0

    def irq(self) -> bool:
        return False


class Timer:
    """Counts down one per tick, and sets `expired` when it reaches 0."""

    COUNT, CONTROL, STATUS = 0x00, 0x04, 0x08
    PERIODIC, ENABLE = 1, 2  # the bits of CONTROL

    def __init__(self) -> None:
        self.count = self.reload = self.control = 0
        self.expired = False

    def read(self, reg: int) -> int:
        if reg == self.COUNT:
            return self.count
        if reg == self.CONTROL:
            return self.control
        if reg == self.STATUS:
            return int(self.expired)
        raise BusError(f"no timer register {reg:#x}")

    def write(self, reg: int, value: int) -> None:
        if reg == self.COUNT:
            self.count = self.reload = value & MASK
        elif reg == self.CONTROL:
            self.control = value & 3
        elif reg == self.STATUS:
            self.expired = False
        else:
            raise BusError(f"no timer register {reg:#x}")

    def tick(self) -> None:
        if self.count == 1:
            self.expired = True
            self.count = self.reload if self.control & self.PERIODIC else 0
        elif self.count:
            self.count -= 1

    def reset(self) -> None:
        self.count = self.reload = self.control = 0
        self.expired = False

    def irq(self) -> bool:
        return self.expired and bool(self.control & self.ENABLE)


class Uart:
    """The serial port. Bytes written to TRANSMIT collect in `output`, and RECEIVE
    takes them from `incoming`, which a test fills. The ISS has no serial line, so
    every incoming byte has already arrived, and the buffers are never full."""

    TRANSMIT, RECEIVE, STATUS, CONTROL = 0x00, 0x04, 0x08, 0x0C
    WAITING = 1  # the bit of STATUS: a byte is waiting

    def __init__(self, incoming: bytes = b"") -> None:
        self.incoming = bytearray(incoming)
        self.output = bytearray()
        self.enabled = False

    def read(self, reg: int) -> int:
        if reg == self.TRANSMIT:
            return 0
        if reg == self.RECEIVE:
            return self.incoming.pop(0) if self.incoming else 0
        if reg == self.STATUS:
            return self.WAITING if self.incoming else 0
        if reg == self.CONTROL:
            return int(self.enabled)
        raise BusError(f"no UART register {reg:#x}")

    def write(self, reg: int, value: int) -> None:
        if reg == self.TRANSMIT:
            self.output.append(value & 0xFF)
        elif reg == self.CONTROL:
            self.enabled = bool(value & 1)
        elif reg not in (self.RECEIVE, self.STATUS):
            raise BusError(f"no UART register {reg:#x}")

    def tick(self) -> None:
        pass

    def reset(self) -> None:
        """Only the interrupt enable: `incoming` and `output` are the PC's side."""
        self.enabled = False

    def irq(self) -> bool:
        return self.enabled and bool(self.incoming)


class Flash:
    """The W25Q128JV SPI flash, as the testbench's model `Tests/io/flash_chip.v`, with
    the commands the FPGC uses: 9Fh, 05h, 06h, 04h, 03h, 0Bh, 6Bh, 02h, 20h and D8h.
    Program and erase are done at once, so the busy bit is never set. Commands it
    doesn't know are ignored, as the chip does.

    It works one clock at a time: `clock` gives the four data lines as the chip shows
    them before a rising edge, then takes IO0 at the edge.
    """

    SIZE = 1 << 24
    ID = 0xEF4018

    def __init__(self, content: bytes = b"") -> None:
        self.content = content
        self._memory: bytearray | None = None  # made at the first use: 16 MiB
        self.wel = False
        self.selected = False
        self.n = 0  # rising edges since the chip select fell
        self.bits = 0  # the bits taken, the last in bit 0
        self.opcode = 0
        self.address = 0
        self.page = bytearray()

    @property
    def memory(self) -> bytearray:
        if self._memory is None:
            self._memory = bytearray(b"\xff" * self.SIZE)
            self._memory[: len(self.content)] = self.content
        return self._memory

    def select(self) -> None:
        self.selected = True
        self.n = self.bits = 0

    def deselect(self) -> None:
        """The command is complete when the chip select rises."""
        if self.selected and self.n >= 8:
            op = self.opcode
            if op == 0x06:
                self.wel = True
            elif op == 0x04:
                self.wel = False
            elif op == 0x02 and self.wel and self.n >= 40 and self.n % 8 == 0:
                base = self.address & ~0xFF
                for k, b in enumerate(self.page):
                    a = base | ((self.address + k) & 0xFF)
                    self.memory[a] &= b
                self.wel = False
            elif op in (0x20, 0xD8) and self.wel and self.n == 32:
                size = 4096 if op == 0x20 else 65536
                start = self.address & ~(size - 1)
                self.memory[start : start + size] = b"\xff" * size
                self.wel = False
        self.selected = False

    def _shown(self) -> int:
        """The data lines after the falling edge that follows rising edge n."""
        n, op = self.n, self.opcode
        if n < 8:
            return 0xF
        if op == 0x9F:
            return (self.ID >> (31 - n) & 1) << 1 if n < 32 else 0xF
        if op == 0x05:
            return ((int(self.wel) << 1) >> (7 - n % 8) & 1) << 1
        if op in (0x03, 0x0B) and n >= (32 if op == 0x03 else 40):
            j = n - (32 if op == 0x03 else 40)
            b = self.memory[(self.address + j // 8) % self.SIZE]
            return (b >> (7 - j % 8) & 1) << 1
        if op == 0x6B and n >= 40:
            j = n - 40
            b = self.memory[(self.address + j // 2) % self.SIZE]
            return b >> 4 if j % 2 == 0 else b & 0xF
        return 0xF

    def clock(self, io0: int) -> int:
        if not self.selected:
            return 0xF
        shown = self._shown()
        self.bits = (self.bits << 1 | (io0 & 1)) & MASK
        self.n += 1
        if self.n == 8:
            self.opcode = self.bits & 0xFF
            self.page = bytearray()
        elif self.n == 32:
            self.address = self.bits & 0xFFFFFF
        elif self.opcode == 0x02 and self.n > 32 and self.n % 8 == 0:
            if len(self.page) < 256:
                self.page.append(self.bits & 0xFF)
        return shown


class Spi:
    """An SPI bus, with a flash behind it or nothing. A transfer either sends or
    receives, and a receive sends 1s. With QUAD on, a receive takes four bits per
    clock. Without a device, the data lines read 1."""

    BYTE, WORD, CONTROL, STATUS = 0x00, 0x04, 0x08, 0x0C
    SELECT, QUAD = 1, 2  # the bits of CONTROL

    def __init__(self, flash: Flash | None = None, quad: bool = False) -> None:
        self.flash = flash
        self.can_quad = quad
        self.control = 0
        self.irq_pin = False  # the device's interrupt pin is active

    def _lines(self, io0: int) -> int:
        return self.flash.clock(io0) if self.flash is not None else 0xF

    def _send(self, byte: int) -> None:
        for i in range(7, -1, -1):
            self._lines(byte >> i & 1)

    def _receive(self) -> int:
        if self.control & self.QUAD:
            high = self._lines(1)
            return high << 4 | self._lines(1)
        value = 0
        for _ in range(8):
            value = value << 1 | (self._lines(1) >> 1 & 1)
        return value

    def read(self, reg: int) -> int:
        if reg == self.BYTE:
            return self._receive()
        if reg == self.WORD:
            return sum(self._receive() << (8 * i) for i in range(4))
        if reg == self.CONTROL:
            return self.control
        if reg == self.STATUS:
            return int(self.irq_pin)
        raise BusError(f"no SPI register {reg:#x}")

    def write(self, reg: int, value: int) -> None:
        if reg == self.BYTE:
            self._send(value & 0xFF)
        elif reg == self.WORD:
            for i in range(4):
                self._send(value >> (8 * i) & 0xFF)
        elif reg == self.CONTROL:
            self._configure(value & (3 if self.can_quad else 1))
        elif reg != self.STATUS:
            raise BusError(f"no SPI register {reg:#x}")

    def _configure(self, control: int) -> None:
        if self.flash is not None:
            if control & self.SELECT and not self.control & self.SELECT:
                self.flash.select()
            elif not control & self.SELECT and self.control & self.SELECT:
                self.flash.deselect()
        self.control = control

    def tick(self) -> None:
        pass

    def reset(self) -> None:
        self._configure(0)

    def irq(self) -> bool:
        return False


def spi_buses(flash1: Flash | None = None, flash2: Flash | None = None) -> list[Spi]:
    """The six SPI buses, devices 4 to 9: flash 1, flash 2, USB 1, USB 2, Ethernet and
    the SD card. Only the flashes have a model, empty unless given."""
    return [
        Spi(flash1 or Flash(), quad=True),
        Spi(flash2 or Flash(), quad=True),
        *(Spi() for _ in range(4)),
    ]
