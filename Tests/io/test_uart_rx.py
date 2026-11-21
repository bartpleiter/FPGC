"""Unit test of uart_rx: lines from a PC, good and bad, and the bytes it receives.

The test doesn't model the receiver cycle by cycle. It checks what comes out: the
received bytes and the framing errors, in order.
"""

import random
from pathlib import Path

import pytest

from ..common.presets import check_preset
from .serial_line import BIT, Line
from .unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="uart_rx",
    folder=Path(__file__).parent,
    inputs=["reset", "rx"],
    outputs=["valid", "data", "framing_error"],
)

BYTE = 10 * BIT
Event = int | str  # a received byte, or "framing"


def run(line: Line, name: str, resets: tuple[int, ...] = (0,)) -> list[Event]:
    """Apply the line, with a reset in the given cycles, and return the events."""
    line.idle(BYTE)  # time for the last byte to come out
    vectors = [{"reset": int(t in resets), "rx": s} for t, s in enumerate(line.samples)]
    labels = ["reset" if t in resets else lab for t, lab in enumerate(line.labels)]
    return events(BENCH.run(vectors, name, labels))


def events(outputs: list[Vector]) -> list[Event]:
    out: list[Event] = []
    for o in outputs:
        if o["valid"]:
            out.append(o["data"])
        if o["framing_error"]:
            out.append("framing")
    return out


@pytest.mark.parametrize("error", [-0.04, -0.02, 0.0, 0.02, 0.04])
def test_baud_rate_off(error: float) -> None:
    """Bytes back to back from a PC whose baud rate is up to 4% off."""
    data = random.Random(1).randbytes(40) + b"\x00\xff\x55\xaa"
    line = Line()
    line.idle(BYTE + 100)
    line.send(data, bit=BIT * (1 + error))
    assert run(line, f"baud_{error:+.2f}") == list(data)


def test_low_after_reset() -> None:
    """A line that's low after a reset, as when the CH340 has no power. The receiver
    waits until the line has been high for a whole byte, so the byte that comes too
    soon after the line goes high is lost, and the next one, after a pause, isn't."""
    line = Line()
    line.level(0, 3000, "low")
    line.idle(BYTE // 2)
    line.send(b"\x55")
    line.idle(BYTE + 100)
    line.send(b"A")
    assert run(line, "low_after_reset") == [ord("A")]


@pytest.mark.parametrize("length", [1, 5, 20, 45])
def test_glitch(length: int) -> None:
    """A short low pulse on an idle line isn't a start bit."""
    line = Line()
    line.idle(BYTE + 100)
    line.level(0, length, "glitch")
    line.idle(300)
    line.send(b"OK")
    assert run(line, f"glitch_{length}") == list(b"OK")


def test_missing_stop_bit() -> None:
    """A byte without its stop bit is a framing error. The bytes that follow back to
    back are lost, until the line has been high for a whole byte."""
    line = Line()
    line.idle(BYTE + 100)
    line.send(b"A")
    line.send(b"B", stop=0)
    line.send(b"\x00\x12\xff")
    line.idle(BYTE + 100)
    line.send(b"C")
    assert run(line, "missing_stop") == [ord("A"), "framing", ord("C")]


def test_reset_in_a_stream() -> None:
    """A reset in the middle of a stream: the receiver doesn't start in the middle of
    a byte, but waits for a pause."""
    data = random.Random(2).randbytes(10)
    line = Line()
    line.idle(BYTE + 100)
    start = len(line.samples)
    line.send(data)
    line.idle(BYTE + 100)
    line.send(b"Z")
    reset = start + int(2.5 * BYTE)  # in the middle of the third byte
    assert run(line, "reset_in_stream", resets=(0, reset)) == [*data[:2], ord("Z")]


# The demo receives "Hi", ignores a glitch, and then gets a byte without a stop bit,
# a framing error. The line is idle for a whole byte after the reset.
def test_demo() -> None:
    line = Line()
    line.idle(BYTE + 50)
    line.send(b"Hi")
    line.idle(200)
    line.level(0, 20, "glitch")
    line.idle(300)
    line.send(b"!", stop=0)
    line.idle(300)
    vectors = [{"reset": int(t == 0), "rx": s} for t, s in enumerate(line.samples)]
    labels = ["reset" if t == 0 else lab for t, lab in enumerate(line.labels)]
    vcd = BENCH.area.wave_dir / "uart_rx.vcd"
    assert events(BENCH.run(vectors, "demo", labels, vcd)) == [*b"Hi", "framing"]
    check_preset(BENCH.folder / "uart_rx.gtkw", vcd)
