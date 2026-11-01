"""A model of the two L1 caches and the SDRAM traffic they cause (l1-cache.md).

Both caches are direct-mapped, with lines of 8 words. A miss fills the whole line from
SDRAM, starting at the word that missed and wrapping around. The L1D is write-back with
write-allocate: a write hit only writes the cache and makes the line dirty, and a miss
that replaces a dirty line first writes that line back. The L1I is only filled. The two
caches aren't kept coherent with each other: a fetch of a line that is dirty in the
L1D gets the line as it is in SDRAM.

CCACHE writes every dirty line of the L1D back, in the order of the lines, and makes
every line of both caches invalid. CCACHED does the same for the L1D only. A reset makes
every line of both caches invalid, without writing anything back. Neither changes the
data words in the caches, only the entries, which become 0.

The model keeps what the arrays of `cpu_l1i` and `cpu_l1d` hold, so the unit test of the
cache controller can compare them at the end, and the list of SDRAM transfers in the
order the controller makes them.
"""

from dataclasses import dataclass, field

LINE_WORDS = 8
LINE_BITS = 21  # a line is bits [25:5] of an address


@dataclass(frozen=True)
class Read:
    """A fill: a read of a line from SDRAM, starting at word `first`."""

    line: int
    first: int


@dataclass(frozen=True)
class Write:
    """A write-back of a whole line to SDRAM."""

    line: int
    words: tuple[int, ...]


Transfer = Read | Write


@dataclass
class Entry:
    """A tag entry: {valid, dirty, tag}."""

    valid: bool = False
    dirty: bool = False
    tag: int = 0


class Cache:
    """One direct-mapped cache of `lines` lines: the tag entries and the data words."""

    def __init__(self, lines: int) -> None:
        assert lines >= 2 and lines & (lines - 1) == 0, "lines must be a power of two"
        self.lines = lines
        self.index_bits = lines.bit_length() - 1
        self.entries = [Entry() for _ in range(lines)]
        self.data = [[0] * LINE_WORDS for _ in range(lines)]

    def index(self, addr: int) -> int:
        return (addr >> 5) % self.lines

    def tag(self, addr: int) -> int:
        return (addr >> 5) >> self.index_bits & (
            (1 << (LINE_BITS - self.index_bits)) - 1
        )

    def hit(self, addr: int) -> bool:
        entry = self.entries[self.index(addr)]
        return entry.valid and entry.tag == self.tag(addr)

    def word(self, addr: int) -> int:
        return self.data[self.index(addr)][addr >> 2 & 7]


@dataclass
class Caches:
    """The L1I and the L1D in front of an SDRAM (word index → word, 0 if missing)."""

    l1i_lines: int
    l1d_lines: int
    sdram: dict[int, int] = field(default_factory=dict)
    traffic: list[Transfer] = field(default_factory=list)

    def __post_init__(self) -> None:
        self.l1i = Cache(self.l1i_lines)
        self.l1d = Cache(self.l1d_lines)

    def fetch(self, addr: int) -> int:
        """Fetch the word at `addr` through the L1I."""
        if not self.l1i.hit(addr):
            self._fill(self.l1i, addr)
        return self.l1i.word(addr)

    def read(self, addr: int) -> int:
        """Read the word at `addr` through the L1D."""
        self._make_present(addr)
        return self.l1d.word(addr)

    def write(self, addr: int, data: int, byte_enable: int) -> None:
        """Write the bytes of `data` that `byte_enable` selects through the L1D."""
        self._make_present(addr)
        index = self.l1d.index(addr)
        words = self.l1d.data[index]
        word = words[addr >> 2 & 7]
        for lane in range(4):
            if byte_enable >> lane & 1:
                mask = 0xFF << 8 * lane
                word = word & ~mask | data & mask
        words[addr >> 2 & 7] = word
        self.l1d.entries[index].dirty = True

    def clear(self, data_only: bool = False) -> None:
        """CCACHE, or CCACHED with `data_only`."""
        for index in range(self.l1d.lines):
            self._write_back(index)
            self.l1d.entries[index] = Entry()
        if not data_only:
            self.l1i.entries = [Entry() for _ in range(self.l1i.lines)]

    def reset(self) -> None:
        """Make every line invalid, and lose what the L1D hasn't written back."""
        self.l1i.entries = [Entry() for _ in range(self.l1i.lines)]
        self.l1d.entries = [Entry() for _ in range(self.l1d.lines)]

    def _make_present(self, addr: int) -> None:
        """On an L1D miss, write the line it replaces back if it's dirty, then fill."""
        if self.l1d.hit(addr):
            return
        self._write_back(self.l1d.index(addr))
        self._fill(self.l1d, addr)

    def _write_back(self, index: int) -> None:
        """Write line `index` of the L1D back to SDRAM if it's valid and dirty."""
        entry = self.l1d.entries[index]
        if entry.valid and entry.dirty:
            line = entry.tag << self.l1d.index_bits | index
            words = tuple(self.l1d.data[index])
            for n, word in enumerate(words):
                self.sdram[line * LINE_WORDS + n] = word
            self.traffic.append(Write(line, words))

    def _fill(self, cache: Cache, addr: int) -> None:
        line = addr >> 5 & ((1 << LINE_BITS) - 1)
        first = addr >> 2 & 7
        index = cache.index(addr)
        cache.data[index] = [
            self.sdram.get(line * LINE_WORDS + n, 0) for n in range(LINE_WORDS)
        ]
        cache.entries[index] = Entry(valid=True, dirty=False, tag=cache.tag(addr))
        self.traffic.append(Read(line, first))
