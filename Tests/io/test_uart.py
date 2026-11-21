"""Unit test of uart: its buffers, its flags, its interrupt, and both lines.

A test is a script: strobes at given cycles, and the rx line next to them. The test
checks the outputs at chosen cycles, every byte that a `receive` takes, and the bytes
on the tx line.
"""

from pathlib import Path

from ..common.presets import check_preset
from .serial_line import BIT, Line, decode
from .unit_bench import UnitBench, Vector

INPUTS = [
    "reset",
    "transmit",
    "tx_byte",
    "receive",
    "clear",
    "configure",
    "irq_enable_in",
    "rx",
]
OUTPUTS = [
    "tx_full",
    "received",
    "rx_byte",
    "rx_waiting",
    "overflow",
    "framing_error",
    "irq_enable",
    "irq",
    "tx",
]
BENCH = UnitBench(
    unit="uart", folder=Path(__file__).parent, inputs=INPUTS, outputs=OUTPUTS
)

BYTE = 10 * BIT
DEPTH = 32
READY = BYTE + 2  # after a reset, the line stays high for a byte before a byte


class Script:
    def __init__(self) -> None:
        self.cycle = 1  # cycle 0 is the reset
        self.resets = {0}
        self.strobes: dict[int, tuple[str, Vector]] = {}
        self.line = Line()
        self.checks: dict[int, Vector] = {}
        self.received: dict[int, int] = {}  # cycle: the byte that comes out
        self.outputs: list[Vector] = []

    def strobe(self, label: str, **inputs: int) -> None:
        """One cycle with these inputs."""
        self.strobes[self.cycle] = (label, inputs)
        self.cycle += 1

    def transmit(self, data: bytes) -> None:
        for byte in data:
            self.strobe(f"tx {byte:02x}", transmit=1, tx_byte=byte)

    def receive(self, byte: int | None) -> None:
        """Take a byte, which is `byte`, or nothing when the buffer is empty."""
        self.strobe("receive", receive=1)
        if byte is not None:
            self.received[self.cycle - 1] = byte

    def configure(self, enable: int) -> None:
        self.strobe("configure", configure=1, irq_enable_in=enable)

    def clear(self) -> None:
        self.strobe("clear", clear=1)

    def wait_until(self, cycle: int) -> None:
        assert cycle >= self.cycle
        self.cycle = cycle

    def reset(self) -> None:
        """A reset for one cycle."""
        self.resets.add(self.cycle)
        self.cycle += 1

    def expect(self, **outputs: int) -> None:
        """The outputs after the edge of the last cycle."""
        self.checks.setdefault(self.cycle - 1, {}).update(outputs)

    def run(self, name: str, demo: bool = False, tx_from: int = 0) -> bytes:
        """Run the script, check it, and return the bytes on the tx line from cycle
        `tx_from` on."""
        __tracebackhide__ = True
        length = max(self.cycle, len(self.line.samples)) + 20
        rx = self.line.samples + [1] * (length - len(self.line.samples))
        labels = self.line.labels + [""] * (length - len(self.line.labels))
        vectors = [dict.fromkeys(INPUTS, 0) | {"rx": rx[t]} for t in range(length)]
        for t in self.resets:
            vectors[t]["reset"] = 1
            labels[t] = "reset"
        for t, (label, inputs) in self.strobes.items():
            vectors[t].update(inputs)
            labels[t] = label
        vcd = BENCH.area.wave_dir / "uart.vcd" if demo else None
        self.outputs = BENCH.run(vectors, name, labels, vcd)
        self._check()
        if vcd is not None:
            check_preset(BENCH.folder / "uart.gtkw", vcd)
        return decode([o["tx"] for o in self.outputs][tx_from:])

    def _check(self) -> None:
        __tracebackhide__ = True
        for t, o in enumerate(self.outputs):
            want = (1, self.received[t]) if t in self.received else (0, 0)
            got = (o["received"], o["rx_byte"])
            assert got == want, f"cycle {t}: received, rx_byte {got}, expected {want}"
        for t, outputs in self.checks.items():
            got = {k: self.outputs[t][k] for k in outputs}
            assert got == outputs, f"cycle {t}: {got}, expected {outputs}"


def test_settings() -> None:
    s = Script()
    s.expect(irq_enable=0, rx_waiting=0, tx_full=0, overflow=0, framing_error=0)
    s.configure(1)
    s.expect(irq_enable=1, irq=0)
    s.configure(0)
    s.expect(irq_enable=0)
    s.receive(None)  # nothing received
    s.clear()
    s.expect(overflow=0, framing_error=0)
    assert s.run("settings") == b""


def test_transmit() -> None:
    s = Script()
    s.transmit(b"Hello")
    s.expect(tx_full=0)
    s.wait_until(6 * BYTE)
    assert s.run("transmit") == b"Hello"


def test_transmit_full() -> None:
    """33 bytes fit: 32 in the buffer and one in the transmitter. There's room again
    when the first byte is sent."""
    data = bytes(range(0x40, 0x40 + 34))
    s = Script()
    s.wait_until(READY)
    s.transmit(data[:32])
    s.expect(tx_full=0)
    s.transmit(data[32:33])
    s.expect(tx_full=1)
    s.wait_until(READY + BYTE - 10)
    s.expect(tx_full=1)
    s.wait_until(READY + BYTE + 20)
    s.expect(tx_full=0)
    s.transmit(data[33:])
    s.wait_until(37 * BYTE)
    assert s.run("transmit_full") == data


def test_receive_and_interrupt() -> None:
    s = Script()
    s.line.idle(BYTE + 100)
    s.line.send(b"abc")
    s.wait_until(5 * BYTE)
    s.expect(rx_waiting=1, irq=0)  # disabled
    s.configure(1)
    s.expect(irq=1)
    s.receive(ord("a"))
    s.receive(ord("b"))
    s.expect(rx_waiting=1, irq=1)
    s.receive(ord("c"))
    s.expect(rx_waiting=0, irq=0)
    s.receive(None)
    s.run("receive")


def test_overflow() -> None:
    """34 bytes and nobody takes them: the first 32 are kept."""
    data = bytes(range(0x60, 0x60 + 34))
    s = Script()
    s.line.idle(BYTE + 100)
    s.line.send(data)
    s.wait_until(37 * BYTE)
    s.expect(overflow=1, rx_waiting=1)
    for c in data[:DEPTH]:
        s.receive(c)
    s.receive(None)
    s.expect(overflow=1, rx_waiting=0)
    s.clear()
    s.expect(overflow=0)
    s.run("overflow")


def test_framing_error() -> None:
    """A byte without its stop bit is dropped and sets the flag. The next byte after
    a pause is received."""
    s = Script()
    s.line.idle(BYTE + 100)
    s.line.send(b"A", stop=0)
    s.line.idle(BYTE + 200)
    s.line.send(b"B")
    s.wait_until(5 * BYTE)
    s.expect(framing_error=1, rx_waiting=1)
    s.receive(ord("B"))
    s.clear()
    s.expect(framing_error=0, rx_waiting=0)
    s.run("framing")


def test_reset() -> None:
    """A reset in the middle of a transmission stops it, and empties both buffers."""
    s = Script()
    s.line.idle(BYTE + 100)
    s.line.send(b"ab")
    s.transmit(b"xyz")
    s.configure(1)
    s.wait_until(3 * BYTE + 500)
    s.expect(irq=1)
    s.reset()
    after = s.cycle
    s.expect(irq=0, irq_enable=0, rx_waiting=0, tx_full=0)
    s.receive(None)
    s.wait_until(after + 3 * BYTE)
    s.run("reset", tx_from=after)
    assert all(o["tx"] for o in s.outputs[after:])


def test_line_high_for_a_byte_after_reset() -> None:
    """After a reset, also one in the middle of a byte, the line stays high for a
    whole byte before the next byte starts, so a receiver always sees the cut-off byte
    end first."""
    s = Script()
    s.wait_until(READY)
    s.transmit(b"x")
    s.wait_until(READY + 4 * BIT)  # in the middle of the x
    s.reset()
    after = s.cycle
    s.transmit(b"y")
    s.wait_until(after + 3 * BYTE)
    assert s.run("high_after_reset", tx_from=after) == b"y"
    tx = [o["tx"] for o in s.outputs]
    assert all(tx[after - 1 : after - 1 + BYTE])
    assert not all(tx[after - 1 : after - 1 + BYTE + 10])


# The demo sends "Hi", while a "!" arrives on the rx line. The line stays high for a
# byte after the reset before the "H". Then it enables the interrupt, which comes at
# once, and takes the "!", which ends the interrupt. Scroll to about 21 µs for that.
def test_demo() -> None:
    s = Script()
    s.transmit(b"Hi")
    s.line.idle(BYTE + 20)
    s.line.send(b"!")
    s.wait_until(2 * BYTE + 100)
    s.configure(1)
    s.expect(irq=1)
    s.receive(ord("!"))
    s.expect(irq=0)
    s.wait_until(3 * BYTE + 400)
    assert s.run("demo", demo=True) == b"Hi"
