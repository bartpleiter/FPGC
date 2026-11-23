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
