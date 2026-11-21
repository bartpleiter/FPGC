"""A serial line, one sample per clock cycle, as the UART's tests drive and check it.

`Line` makes the samples of a line that a PC sends on: bytes at a baud rate that may be
a little off, glitches, a missing stop bit, a line that's low. `decode` reads the bytes
back from the samples of the UART's transmitter, and checks that every bit is exactly
`BIT` cycles long.
"""

BIT = 100  # cycles per bit: 1 Mbaud at 100 MHz


class Line:
    """The samples of a line, built from left to right, with a label per sample."""

    def __init__(self) -> None:
        self.samples: list[int] = []
        self.labels: list[str] = []
        self.time = 0.0  # where the line is, in cycles, which may be fractional

    def level(self, level: int, cycles: float, label: str = "") -> None:
        """Hold the line at `level` for `cycles` cycles."""
        self.time += cycles
        n = round(self.time) - len(self.samples)
        self.samples += [level] * n
        self.labels += [label] * n

    def idle(self, cycles: float) -> None:
        self.level(1, cycles, "idle")

    def send(self, data: bytes, bit: float = BIT, stop: int = 1) -> None:
        """Send `data` back to back, each bit `bit` cycles long. A `stop` of 0 sends
        every byte without its stop bit."""
        for byte in data:
            bits = [0, *((byte >> i) & 1 for i in range(8)), stop]
            for b in bits:
                self.level(b, bit, f"rx {byte:02x}")


def decode(samples: list[int], bit: int = BIT) -> bytes:
    """The bytes on a line from a transmitter, which must keep every bit exactly
    `bit` cycles long."""
    out = bytearray()
    t = 0
    while t < len(samples):
        if samples[t]:
            t += 1
            continue
        frame = samples[t : t + 10 * bit]
        assert len(frame) == 10 * bit, f"cycle {t}: the line ends in a byte"
        levels = [frame[i * bit : (i + 1) * bit] for i in range(10)]
        for i, cycles in enumerate(levels):
            assert len(set(cycles)) == 1, f"cycle {t}: bit {i} changes in the middle"
        assert levels[9][0] == 1, f"cycle {t}: no stop bit"
        out.append(sum(levels[1 + i][0] << i for i in range(8)))
        t += 10 * bit
    return bytes(out)
