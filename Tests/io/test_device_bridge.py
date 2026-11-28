"""Unit test of device_bridge, with its devices, through the bus.

A test is a script: bus accesses at given cycles, with the rx line and the switches
next to them. The test checks the answer of every access (the cycle it comes in, its
error and its word), the interrupts and the user LED at chosen cycles, and the bytes on
the tx line. The devices' own behavior is tested in their unit tests; these tests
check the decoding, the answers, and that every register reaches its device.
"""

import random
from dataclasses import dataclass
from pathlib import Path

from ..common.presets import check_preset
from .serial_line import BIT, Line, decode
from .unit_bench import UnitBench, Vector

INPUTS = [
    "reset",
    "bus_request",
    "bus_write",
    "bus_addr",
    "bus_write_data",
    "bus_byte_enable",
    "uart_rx",
    "switches",
    "spi_irq_n",
]
OUTPUTS = [
    "bus_ready",
    "bus_error",
    "bus_read_word",
    "user_led",
    "timer_irq",
    "uart_irq",
    "uart_tx",
    "spi_cs_n",
]
BENCH = UnitBench(
    unit="device_bridge", folder=Path(__file__).parent, inputs=INPUTS, outputs=OUTPUTS
)

ROM = 0x04000000
UART_TRANSMIT, UART_RECEIVE, UART_STATUS, UART_CONTROL = range(
    0xFFFF8000, 0xFFFF8010, 4
)
COUNTER_SNAPSHOT, COUNTER_LOW, COUNTER_HIGH = range(0xFFFF8100, 0xFFFF810C, 4)
TIMER_COUNT, TIMER_CONTROL, TIMER_STATUS = range(0xFFFF8200, 0xFFFF820C, 4)
BOARD_LED, BOARD_SWITCHES = 0xFFFF8300, 0xFFFF8304
REGISTERS = [
    *range(0xFFFF8000, 0xFFFF8010, 4),
    *range(0xFFFF8100, 0xFFFF810C, 4),
    *range(0xFFFF8200, 0xFFFF820C, 4),
    BOARD_LED,
    BOARD_SWITCHES,
]

WAITING, TX_FULL, OVERFLOW, FRAMING = 1, 2, 4, 8  # the bits of the UART's STATUS
BYTE = 10 * BIT
SWITCHES = 0b0101


def rom_word(addr: int) -> int:
    """The testbench's ROM."""
    return 0xC0DE0000 | ((addr >> 2) & 0x3FF)


@dataclass
class Access:
    cycle: int
    label: str
    addr: int
    data: int | None  # None for a read
    enables: int
    hold: int  # the cycles the access stays on the bus, as the CPU does until ready
    word: int  # the expected read word
    error: int
    latency: int = -1  # the cycles from the request to ready, after the run


class Script:
    def __init__(self) -> None:
        self.cycle = 4  # the reset, and the switches through their synchronizer
        self.resets = {0}
        self.accesses: list[Access] = []
        self.line = Line()
        self.checks: dict[int, Vector] = {}
        self.outputs: list[Vector] = []
        self.irq_n: list[tuple[int, int]] = [(0, 0x3F)]  # cycle, SPI interrupt pins

    def spi_irq_n(self, pins: int) -> None:
        """The SPI buses' interrupt pins from now on, 0 is active."""
        self.irq_n.append((self.cycle, pins))

    def access(
        self,
        label: str,
        addr: int,
        data: int | None = None,
        word: int = 0,
        error: int = 0,
        enables: int = 0xF,
        hold: int = 4,
    ) -> Access:
        a = Access(self.cycle, label, addr, data, enables, hold, word, error)
        self.accesses.append(a)
        self.cycle += hold
        return a

    def wait_until(self, cycle: int) -> None:
        assert cycle >= self.cycle
        self.cycle = cycle

    def expect(self, **outputs: int) -> None:
        """The outputs after the edge of the last cycle before the next access."""
        self.checks.setdefault(self.cycle - 1, {}).update(outputs)

    def run(self, name: str, demo: bool = False) -> bytes:
        """Run the script, check it, and return the bytes on the tx line."""
        __tracebackhide__ = True
        length = max(self.cycle, len(self.line.samples)) + 20
        rx = self.line.samples + [1] * (length - len(self.line.samples))
        labels = self.line.labels + [""] * (length - len(self.line.labels))
        irq_n = [max((c, p) for c, p in self.irq_n if c <= t)[1] for t in range(length)]
        vectors = [
            dict.fromkeys(INPUTS, 0)
            | {"uart_rx": rx[t], "switches": SWITCHES, "spi_irq_n": irq_n[t]}
            for t in range(length)
        ]
        for t in self.resets:
            vectors[t]["reset"] = 1
            labels[t] = "reset"
        for a in self.accesses:
            for t in range(a.cycle, a.cycle + a.hold):
                vectors[t] |= {
                    "bus_write": int(a.data is not None),
                    "bus_addr": a.addr,
                    "bus_write_data": a.data or 0,
                    "bus_byte_enable": a.enables,
                }
                labels[t] = a.label
            vectors[a.cycle]["bus_request"] = 1
        vcd = BENCH.area.wave_dir / "device_bridge.vcd" if demo else None
        self.outputs = BENCH.run(vectors, name, labels, vcd)
        self._check()
        if vcd is not None:
            check_preset(BENCH.folder / "device_bridge.gtkw", vcd)
        return decode([o["uart_tx"] for o in self.outputs])

    def _check(self) -> None:
        __tracebackhide__ = True
        out = self.outputs
        readies = [t for t, o in enumerate(out) if o["bus_ready"]]
        assert len(readies) == len(self.accesses), "an access got no or two answers"
        for a, t in zip(self.accesses, readies, strict=True):
            assert a.cycle <= t < a.cycle + a.hold, f"{a.label} at {a.cycle}: no answer"
            a.latency = t - a.cycle
            got = (out[t]["bus_error"], out[t]["bus_read_word"])
            assert got == (a.error, a.word), (
                f"{a.label} at {a.cycle}: error, word {got}, expected {a.error, a.word}"
            )
        for t, o in enumerate(out):
            if not o["bus_ready"]:
                assert o["bus_error"] == 0, f"cycle {t}: an error without ready"
        for t, outputs in self.checks.items():
            got = {k: out[t][k] for k in outputs}
            assert got == outputs, f"cycle {t}: {got}, expected {outputs}"


def test_registers_after_reset() -> None:
    """Every register reads 0 after a reset, except the switches."""
    s = Script()
    for addr in REGISTERS:
        word = SWITCHES if addr == BOARD_SWITCHES else 0
        s.access(f"{addr & 0xFFF:x}", addr, word=word)
    s.run("after_reset")
    assert all(a.latency == 0 for a in s.accesses)


def test_writes_reach_the_devices() -> None:
    """A write to each register that has an effect, and a read that shows it."""
    s = Script()
    s.access("led", BOARD_LED, 0xFFFFFFF7)  # only bit 0
    s.expect(user_led=1)
    s.access("led", BOARD_LED, word=1)
    s.access("uart ctrl", UART_CONTROL, 3)
    s.access("uart ctrl", UART_CONTROL, word=1)
    s.access("timer ctrl", TIMER_CONTROL, 3)
    s.access("timer ctrl", TIMER_CONTROL, word=3)
    start = s.access("timer count", TIMER_COUNT, 1000)
    # The count is 1000 from the cycle after the start's strobe, and the read takes
    # it in the cycle of its request
    s.access("timer count", TIMER_COUNT, word=1000 - (s.cycle - start.cycle - 2))
    s.access("timer count", TIMER_COUNT, 0)
    # The snapshot is the count when its strobe arrives, which is the number of the
    # cycle of the request, as the count is 0 in the cycle after the reset
    snapshot = s.access("snapshot", COUNTER_SNAPSHOT, 0)
    s.access("low", COUNTER_LOW, word=snapshot.cycle)
    s.access("high", COUNTER_HIGH, word=0)
    # Writes that a register ignores, and reads that give 0
    s.access("low", COUNTER_LOW, 7)
    s.access("high", COUNTER_HIGH, 7)
    s.access("switches", BOARD_SWITCHES, 0xF)
    s.access("receive", UART_RECEIVE, 0x41)
    s.access("transmit", UART_TRANSMIT, word=0)
    s.access("snapshot", COUNTER_SNAPSHOT, word=0)
    s.access("switches", BOARD_SWITCHES, word=SWITCHES)
    s.access("uart status", UART_STATUS, 0xF)
    s.access("timer status", TIMER_STATUS, 1)
    s.run("writes")
    assert all(a.latency == 0 for a in s.accesses)


def test_nothing_there() -> None:
    """Accesses that nothing takes are answered with an error, and change nothing."""
    s = Script()
    for addr in [0xFFFF8010, 0xFFFF80FC, 0xFFFF810C, 0xFFFF820C, 0xFFFF8308]:
        s.access(f"{addr & 0xFFF:x}", addr, error=1)
        s.access(f"{addr & 0xFFF:x}", addr, 5, error=1)
    for addr in [0xFFFF8A00, 0xFFFF9000, 0xFFFFFE00]:  # devices 10, 16 and 126
        s.access("no device", addr, error=1)
    s.access("readb", UART_STATUS, error=1, enables=0x1)
    s.access("writeh", BOARD_LED, 0x1, error=1, enables=0x3)
    s.expect(user_led=0)
    s.access("rom end", ROM + 0x1000, error=1)
    s.access("gpu", 0x05000000, error=1)
    s.access("sram", 0x06000000, 1, error=1)
    s.access("rom write", ROM, 1, error=1)
    s.run("nothing_there")
    assert all(a.latency == 0 for a in s.accesses)


def test_rom() -> None:
    """The ROM answers a read of any size with its word, a cycle later than a
    register."""
    s = Script()
    for addr, enables in [
        (0, 0xF),
        (0x10, 0xF),
        (0x13, 0x8),
        (0x22, 0xC),
        (0xFFC, 0xF),
    ]:
        s.access("rom", ROM + addr, word=rom_word(addr), enables=enables)
    s.run("rom")
    assert all(a.latency == 1 for a in s.accesses)


def test_random() -> None:
    """Random accesses to every region, with every size, against a model of the
    registers that hold still: the user LED, the switches and the settings."""
    rng = random.Random(1)
    s = Script()
    led = uart_control = timer_control = 0
    still = [UART_TRANSMIT, UART_RECEIVE, UART_STATUS, UART_CONTROL]
    still += [COUNTER_SNAPSHOT, COUNTER_LOW]
    still += [COUNTER_HIGH, TIMER_COUNT, TIMER_CONTROL, TIMER_STATUS, BOARD_LED]
    still += [BOARD_SWITCHES]
    writable = [UART_RECEIVE, UART_STATUS, UART_CONTROL, COUNTER_LOW, COUNTER_HIGH]
    writable += [TIMER_CONTROL, TIMER_STATUS, BOARD_LED, BOARD_SWITCHES]
    for _ in range(1500):
        kind = rng.random()
        if kind < 0.15:
            addr = ROM + rng.randrange(-8, 0x1008)
        elif kind < 0.75:
            addr = rng.choice(still) + rng.choice([0, 0, 0, 0, 1, 2, 3])
        elif kind < 0.9:
            addr = 0xFFFF8000 + rng.choice([0, 0xA00]) + rng.randrange(0, 0x400)
        else:
            addr = rng.getrandbits(32)
        enables = rng.choice([0xF, 0xF, 0xF, 0x1, 0x2, 0x4, 0x8, 0x3, 0xC])
        word_addr = addr & ~3
        write = rng.random() < 0.4 and word_addr in writable
        data = rng.getrandbits(32) if write else None
        word = error = 0
        rom = ROM <= addr < ROM + 0x1000
        if rom and not write:
            word = rom_word(addr)
        elif word_addr != addr or enables != 0xF or word_addr not in still:
            error = 1
        elif write:
            assert data is not None
            if word_addr == BOARD_LED:
                led = data & 1
            elif word_addr == UART_CONTROL:
                uart_control = data & 1
            elif word_addr == TIMER_CONTROL:
                timer_control = data & 3
        else:
            word = {
                UART_CONTROL: uart_control,
                TIMER_CONTROL: timer_control,
                BOARD_LED: led,
                BOARD_SWITCHES: SWITCHES,
            }.get(word_addr, 0)
        s.access(
            "random", word_addr if error == 0 else addr, data, word, error, enables
        )
    s.run("random")


def test_uart() -> None:
    """Bytes go out on the tx line, and come in with the interrupt. A write waits
    while the transmit buffer is full, and a read that takes a byte waits for it."""
    data = bytes(range(0x40, 0x40 + 34))
    s = Script()
    s.line.idle(BYTE + 100)
    s.line.send(b"ab")
    s.wait_until(BYTE + 10)  # the line is high for a byte after the reset
    quick = [s.access("tx", UART_TRANSMIT, c) for c in data[:33]]
    s.access("status", UART_STATUS, word=TX_FULL)
    slow = s.access("tx", UART_TRANSMIT, data[33], hold=BYTE)
    s.wait_until(4 * BYTE)
    s.expect(uart_irq=0)
    s.access("status", UART_STATUS, word=WAITING)
    s.access("enable", UART_CONTROL, 1)
    s.expect(uart_irq=1)
    s.access("rx", UART_RECEIVE, word=ord("a"))
    s.access("rx", UART_RECEIVE, word=ord("b"))
    s.expect(uart_irq=0)
    s.access("rx", UART_RECEIVE, word=0)
    s.wait_until(37 * BYTE)
    assert s.run("uart") == data
    assert all(a.latency == 0 for a in quick)
    assert BYTE // 2 < slow.latency < BYTE
    receives = [a for a in s.accesses if a.addr == UART_RECEIVE]
    assert [a.latency for a in receives] == [2, 2, 0]


def test_timer_interrupt() -> None:
    s = Script()
    s.access("enable", TIMER_CONTROL, 2)
    start = s.access("count 20", TIMER_COUNT, 20)
    # The count is 20 two cycles after the request, and expires 20 cycles later
    s.wait_until(start.cycle + 21)
    s.expect(timer_irq=0)
    s.wait_until(start.cycle + 22)
    s.expect(timer_irq=1)
    s.access("status", TIMER_STATUS, word=1)
    s.access("clear", TIMER_STATUS, 0)
    s.expect(timer_irq=0)
    s.run("timer")


# The demo reads a word and a byte of the ROM, turns on the user LED, starts the timer, reads
# its count, and takes a snapshot of the cycle counter. Then it makes three accesses
# that nothing takes: a byte read of a device register, device 10, which doesn't
# exist, and the GPU's region, which is empty for now. Last, it reads the JEDEC ID of
# flash 1, and a word of it with a quad read.
def test_demo() -> None:
    s = Script()
    s.access("read rom", ROM + 0x10, word=rom_word(0x10))
    s.access("readb rom", ROM + 0x13, word=rom_word(0x13), enables=0x8)
    s.access("write led", BOARD_LED, 1)
    s.access("timer 100", TIMER_COUNT, 100)
    s.access("read count", TIMER_COUNT, word=100 - 2)
    snapshot = s.access("snapshot", COUNTER_SNAPSHOT, 0)
    s.access("read low", COUNTER_LOW, word=snapshot.cycle)
    s.access("readb uart", UART_STATUS, enables=0x1, error=1)
    s.access("read dev 10", 0xFFFF8A00, error=1)
    s.access("write gpu", 0x05000000, 1, error=1)
    s.access("select", spi(0, SPI_CONTROL), 1)
    s.access("9F", spi(0, SPI_BYTE), 0x9F)
    for value in (0xEF, 0x40, 0x18):
        s.access("id", spi(0, SPI_BYTE), word=value, hold=SPI_HOLD)
    s.access("deselect", spi(0, SPI_CONTROL), 0, hold=SPI_HOLD)
    quad_read(s, 0, 0x20)
    s.run("demo", demo=True)


SPI = 0xFFFF8400
MASK = 0xFFFFFFFF
SPI_BYTE, SPI_WORD, SPI_CONTROL, SPI_STATUS = 0x0, 0x4, 0x8, 0xC
SPI_HOLD = 400  # enough for a word at 12.5 MHz


def spi(bus: int, reg: int) -> int:
    return SPI + 0x100 * bus + reg


def flash_byte(bus: int, k: int) -> int:
    """The testbench's flash content."""
    return (k * 3 + 1 + 64 * bus) & 0xFF


def flash_word(bus: int, k: int) -> int:
    return sum(flash_byte(bus, k + i) << (8 * i) for i in range(4))


def command(opcode: int, address: int) -> int:
    """The command and its address as a word: the command goes first."""
    a = address.to_bytes(3, "big")
    return int.from_bytes(bytes([opcode, *a]), "little")


def quad_read(s: Script, bus: int, address: int) -> None:
    """6Bh: a word of flash `bus` from `address`."""
    s.access("select", spi(bus, SPI_CONTROL), 1)
    s.access("6B", spi(bus, SPI_WORD), command(0x6B, address))
    s.access("quad", spi(bus, SPI_CONTROL), 3, hold=SPI_HOLD)
    s.access("dummy", spi(bus, SPI_WORD), word=0xFFFFFFFF, hold=SPI_HOLD)
    s.access("data", spi(bus, SPI_WORD), word=flash_word(bus, address), hold=SPI_HOLD)
    s.access("deselect", spi(bus, SPI_CONTROL), 0, hold=SPI_HOLD)


def test_spi_control_and_status() -> None:
    """CONTROL on every bus: only the flashes have QUAD. STATUS shows the interrupt
    pins, two cycles late through the synchronizer."""
    s = Script()
    for bus in range(6):
        s.access("control", spi(bus, SPI_CONTROL), word=0)
        s.access("status", spi(bus, SPI_STATUS), word=0)
        s.access("set", spi(bus, SPI_CONTROL), 3)
        s.access("control", spi(bus, SPI_CONTROL), word=3 if bus < 2 else 1)
    s.expect(spi_cs_n=0)
    for bus in range(6):
        s.access("clear", spi(bus, SPI_CONTROL), 0)
    s.expect(spi_cs_n=0x3F)
    s.spi_irq_n(0b101011)  # USB 1 and Ethernet
    s.wait_until(s.cycle + 3)
    for bus in range(6):
        s.access("status", spi(bus, SPI_STATUS), word=int(bus in (2, 4)))
    s.access("write status", spi(2, SPI_STATUS), 0)  # ignored
    s.access("status", spi(2, SPI_STATUS), word=1)
    s.run("spi_control")
    assert all(a.latency == 0 for a in s.accesses)


def test_spi_nothing_there() -> None:
    """Accesses that the SPI buses refuse."""
    s = Script()
    s.access("readb", spi(0, SPI_BYTE), error=1, enables=0x1)
    s.access("writeh", spi(1, SPI_CONTROL), 1, error=1, enables=0x3)
    s.expect(spi_cs_n=0x3F)
    for bus in range(6):
        s.access("0x10", spi(bus, 0x10), error=1)
        s.access("0xFC", spi(bus, 0xFC), 1, error=1)
    s.run("spi_nothing_there")


def test_spi_receive_without_device() -> None:
    """A receive on a bus without a device gives 1s, and takes 8 or 32 clocks."""
    s = Script()
    byte = [
        s.access("byte", spi(bus, SPI_BYTE), word=0xFF, hold=SPI_HOLD)
        for bus in range(6)
    ]
    word = [
        s.access("word", spi(bus, SPI_WORD), word=MASK, hold=SPI_HOLD)
        for bus in range(6)
    ]
    s.run("spi_no_device")
    for accesses, clocks in ((byte, 8), (word, 32)):
        for bus, a in enumerate(accesses):
            half = [1, 1, 4, 4, 4, 2][bus]
            assert 2 * half * clocks < a.latency < 2 * half * clocks + 6, (
                bus,
                a.latency,
            )


def test_spi_flash() -> None:
    """The JEDEC ID and reads with 03h and 6Bh from both flashes, and a write that
    waits until the transfer before it is done."""
    s = Script()
    for bus in (0, 1):
        s.access("select", spi(bus, SPI_CONTROL), 1)
        s.access("9F", spi(bus, SPI_BYTE), 0x9F)
        for value in (0xEF, 0x40, 0x18):
            s.access("id", spi(bus, SPI_BYTE), word=value, hold=SPI_HOLD)
        s.access("deselect", spi(bus, SPI_CONTROL), 0, hold=SPI_HOLD)
        s.access("select", spi(bus, SPI_CONTROL), 1)
        s.access("03", spi(bus, SPI_WORD), command(0x03, 0x123))
        waits = s.access(
            "data", spi(bus, SPI_WORD), word=flash_word(bus, 0x123), hold=SPI_HOLD
        )
        s.access("data", spi(bus, SPI_BYTE), word=flash_byte(bus, 0x127), hold=SPI_HOLD)
        s.access("deselect", spi(bus, SPI_CONTROL), 0, hold=SPI_HOLD)
        quad_read(s, bus, 0x200 + bus)
    s.run("spi_flash")
    assert waits.latency > 120  # the command's 64 cycles, then its own 64
