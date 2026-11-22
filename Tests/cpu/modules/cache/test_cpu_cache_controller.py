"""Unit test of cpu_cache_controller, with both caches and the SDRAM line model.

Random steps of fetches and reads and writes, CCACHE, CCACHED and resets must give the
same words, the same SDRAM transfers in the same order, and the same contents of the
caches at the end, as the model of the caches in cache_model.py.
"""

import random
from dataclasses import dataclass
from pathlib import Path

import pytest

from ....common.presets import check_preset
from ....common.unit_bench import label_hex
from ...cache_model import LINE_WORDS, Caches, Read, Transfer, Write
from ...verilog import BUILD_DIR, RTL_FILES, WAVE_DIR, compile_tb, run_vvp

FOLDER = Path(__file__).parent
UNIT = "cpu_cache_controller"


# The kinds of access of a step, as the testbench numbers them
KINDS = {None: 0, "read": 1, "write": 2, "ccache": 3, "ccached": 4, "reset": 5}


@dataclass(frozen=True)
class Step:
    """A fetch, a read or write, or both. Each starts `*_start` cycles into the step.

    A CCACHE, CCACHED or reset is an access without a fetch.
    """

    label: str
    fetch: int | None = None  # the address
    fetch_start: int = 0
    access: str | None = None  # a key of KINDS
    addr: int = 0
    data: int = 0
    byte_enable: int = 0xF
    data_start: int = 0

    def columns(self) -> str:
        kind = KINDS[self.access]
        values = [
            int(self.fetch is not None),
            self.fetch or 0,
            self.fetch_start,
            kind,
            self.addr,
            self.data,
            self.byte_enable,
            self.data_start,
        ]
        return " ".join([label_hex(self.label), *(f"{v:x}" for v in values)])


@dataclass
class Result:
    words: list[tuple[int, int]]  # per step the fetched word and the read word
    traffic: list[Transfer]
    contents: list[str]  # per line of the L1I, then the L1D: the entry and the words


def run(
    steps: list[Step],
    sdram: dict[int, int],
    name: str,
    lines: tuple[int, int],
    latency: str,
    seed: int = 0,
    vcd: Path | None = None,
) -> Result:
    """Play `steps` on the testbench, with `lines` lines in the L1I and the L1D."""
    out_dir = BUILD_DIR / "modules" / UNIT / name
    parameters = {"L1I_LINES": lines[0], "L1D_LINES": lines[1]}
    tb = FOLDER / f"tb_{UNIT}.v"
    vvp = compile_tb(tb, RTL_FILES, out_dir / "tb.vvp", parameters)
    names = ["steps", "sdram", "outputs", "log", "contents"]
    files = {k: out_dir / f"{k}.txt" for k in names}
    files["steps"].write_text("".join(f"{s.columns()}\n" for s in steps))
    files["sdram"].write_text("".join(f"{i:x} {w:x}\n" for i, w in sdram.items()))
    plusargs: dict[str, object] = {
        "steps": files["steps"],
        "sdram_words": files["sdram"],
        "outputs": files["outputs"],
        "contents": files["contents"],
        "sdram_log": files["log"],
        "sdram_latency": latency,
        "seed": seed,
    }
    if vcd is not None:
        vcd.parent.mkdir(parents=True, exist_ok=True)
        plusargs["vcd"] = vcd
    output = run_vvp(vvp, **plusargs)
    assert "protocol error" not in output, output.strip()

    words = []
    for line in files["outputs"].read_text().splitlines():
        fetched, read = line.split()
        words.append((int(fetched, 16), int(read, 16)))
    assert len(words) == len(steps), f"the testbench stopped early:\n{output}"
    traffic: list[Transfer] = []
    for line in files["log"].read_text().splitlines():
        _, kind, *values = line.split()
        if kind == "read":
            traffic.append(Read(int(values[0], 16), int(values[1])))
        else:
            words_written = tuple(int(v, 16) for v in values[1:])
            traffic.append(Write(int(values[0], 16), words_written))
    contents = files["contents"].read_text().splitlines()
    return Result(words, traffic, contents)


def model(steps: list[Step], sdram: dict[int, int], lines: tuple[int, int]) -> Result:
    """What the controller and the caches give for `steps`.

    The fetch and the access of a step only change each other's result through SDRAM,
    when both miss. Then the one that misses first is handled first, and the L1D first
    when they miss in the same cycle.
    """
    caches = Caches(lines[0], lines[1], dict(sdram))
    words = []
    for step in steps:
        fetched = read = 0
        if step.fetch is not None and step.fetch_start < step.data_start:
            fetched = caches.fetch(step.fetch)
        if step.access == "read":
            read = caches.read(step.addr)
        elif step.access == "write":
            caches.write(step.addr, step.data, step.byte_enable)
        elif step.access in ("ccache", "ccached"):
            caches.clear(data_only=step.access == "ccached")
        elif step.access == "reset":
            caches.reset()
        if step.fetch is not None and step.fetch_start >= step.data_start:
            fetched = caches.fetch(step.fetch)
        words.append((fetched, read))

    # The entries as the testbench writes them: {valid, tag} for the L1I, and
    # {valid, dirty, tag} for the L1D, in hex
    contents = []
    for cache, dirty_bit in [(caches.l1i, False), (caches.l1d, True)]:
        tag_bits = 21 - cache.index_bits
        for entry, data in zip(cache.entries, cache.data, strict=True):
            flags = entry.valid << 1 | entry.dirty if dirty_bit else entry.valid
            value = flags << tag_bits | entry.tag
            digits = (tag_bits + dirty_bit + 4) // 4
            contents.append(
                " ".join([f"{value:0{digits}x}", *(f"{w:08x}" for w in data)])
            )
    return Result(words, caches.traffic, contents)


def check(
    steps: list[Step],
    sdram: dict[int, int],
    name: str,
    lines: tuple[int, int],
    latency: str,
    seed: int = 0,
    vcd: Path | None = None,
) -> None:
    __tracebackhide__ = True
    got = run(steps, sdram, name, lines, latency, seed, vcd)
    want = model(steps, sdram, lines)
    for number, (step, have, expected) in enumerate(
        zip(steps, got.words, want.words, strict=True)
    ):
        assert have == expected, (
            f"step {number} ({step}): fetched and read {have[0]:08x} {have[1]:08x},"
            f" expected {expected[0]:08x} {expected[1]:08x}"
        )
    for number, (have, expected) in enumerate(
        zip(got.traffic, want.traffic, strict=False)
    ):
        assert have == expected, f"SDRAM transfer {number}: {have}, expected {expected}"
    assert len(got.traffic) == len(want.traffic), (
        f"{len(got.traffic)} SDRAM transfers, expected {len(want.traffic)}"
    )
    for number, (have, expected) in enumerate(
        zip(got.contents, want.contents, strict=True)
    ):
        cache = "L1I" if number < lines[0] else "L1D"
        index = number if number < lines[0] else number - lines[0]
        assert have == expected, f"{cache} line {index}: {have}\n  expected {expected}"


def line_words(lines: list[int], rng: random.Random) -> dict[int, int]:
    """Random SDRAM words for `lines`."""
    return {
        line * LINE_WORDS + n: rng.getrandbits(32)
        for line in lines
        for n in range(LINE_WORDS)
    }


def random_steps(lines: list[int], count: int, rng: random.Random) -> list[Step]:
    """Random steps on a few lines, so they often miss and replace dirty lines.

    Now and then a step is a CCACHE, a CCACHED or a reset instead.
    """
    steps = []
    for _ in range(count):
        special = rng.choices(["ccache", "ccached", "reset", None], [3, 3, 1, 93])[0]
        if special:
            steps.append(
                Step(special, access=special, data_start=rng.choice([0, 1, 3]))
            )
            continue
        fetch = None
        access = None
        if rng.random() < 0.6:
            fetch = rng.choice(lines) << 5 | rng.randrange(LINE_WORDS) << 2
        if fetch is None or rng.random() < 0.7:
            access = rng.choice(["read", "write"])
        steps.append(
            Step(
                label="random",
                fetch=fetch,
                fetch_start=rng.choice([0, 0, 0, 1, 2, 5]),
                access=access,
                addr=rng.choice(lines) << 5 | rng.randrange(LINE_WORDS) << 2,
                data=rng.getrandbits(32),
                byte_enable=rng.choice([0x1, 0x2, 0x4, 0x8, 0x3, 0xC, 0xF, 0xF, 0x5]),
                data_start=rng.choice([0, 0, 0, 1, 2, 5]),
            )
        )
    return steps


def random_lines(rng: random.Random) -> list[int]:
    """A few lines that share their low index bits, with random tags.

    The indexes 0, 1, 2 and 127 conflict in caches of 128 lines and of 4 lines, and so
    do lines with the same index and different tags.
    """
    tags = [rng.getrandbits(21 - 7) for _ in range(4)]
    return [tag << 7 | index for tag in tags for index in [0, 1, 2, 127]]


LINES = [(128, 128), (4, 4), (4, 128), (128, 4)]
LATENCIES = ["1", "7", "rand"]


@pytest.mark.parametrize("latency", LATENCIES)
@pytest.mark.parametrize("lines", LINES, ids=lambda x: f"{x[0]}_{x[1]}")
def test_random(lines: tuple[int, int], latency: str) -> None:
    for seed in range(3):
        rng = random.Random(seed)
        used = random_lines(rng)
        steps = random_steps(used, 300, rng)
        name = f"random_{lines[0]}_{lines[1]}_{latency}_{seed}"
        check(steps, line_words(used, rng), name, lines, latency, seed)


@pytest.mark.parametrize("latency", LATENCIES)
def test_both_miss(latency: str) -> None:
    """A fetch and an access that miss at once, or one cycle apart.

    The access writes back a dirty line that the fetch then reads, so the order shows
    in the fetched word too.
    """
    rng = random.Random(1)
    a, b = 0x80, 0x480  # the same index in a cache of 128 lines
    steps = [Step("write a", access="write", addr=a << 5, data=0x11111111)]
    for fetch_start, data_start in [(0, 0), (1, 0), (0, 1), (3, 0), (0, 3)]:
        steps += [
            Step(
                "both miss",
                fetch=a << 5,
                fetch_start=fetch_start,
                access="read",
                addr=b << 5 | 4,
                data_start=data_start,
            ),
            Step("write a", access="write", addr=a << 5, data=rng.getrandbits(32)),
            Step("fetch b", fetch=b << 5),
        ]
    check(steps, line_words([a, b], rng), f"both_miss_{latency}", (128, 128), latency)


# The demo, with 4 lines in each cache so that the walks are short. First the walk after
# the reset, which only invalidates. Then a write that misses, so a clean fill (from word
# 5) and then the write, which makes the line dirty. Then a read of another line with the
# same index, which writes the dirty line back and fills the new one (from word 2), and a
# fetch of the first line, which gets the words that were written back. Then a write hit
# makes the second line dirty, and CCACHE walks through the 4 lines: it writes that line
# back and invalidates both caches.
DEMO_LINES = [0x800, 0x880]  # addresses 0x10000 and 0x11000
DEMO = [
    Step("write 10014", access="write", addr=0x10014, data=0x12345678),
    Step("read 11008", access="read", addr=0x11008),
    Step("fetch 10014", fetch=0x10014),
    Step("write 1100c", access="write", addr=0x1100C, data=0x9ABCDEF0),
    Step("ccache", access="ccache"),
]


def test_demo() -> None:
    sdram = {
        line * LINE_WORDS + n: 0xA0000000 | line << 5 | n << 2
        for line in DEMO_LINES
        for n in range(LINE_WORDS)
    }
    vcd = WAVE_DIR / f"{UNIT}.vcd"
    check(DEMO, sdram, "demo", (4, 4), "7", vcd=vcd)
    check_preset(FOLDER / f"{UNIT}.gtkw", vcd)
    for view in FOLDER.glob(f"{UNIT}.*.gtkw"):
        check_preset(view, vcd)
