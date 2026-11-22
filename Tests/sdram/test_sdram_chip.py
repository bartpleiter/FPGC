"""Tests of the SDRAM chip model, sdram_chip: legal command sequences give the right
data at the right times, and each broken rule gives its error number.

Each test is a list of cycles of the chip's pins (tb_sdram_chip.v). The clock is 10 ns,
so tRCD and tRP (15 ns) are 2 cycles, tRAS (42 ns) is 5, and tRC (60 ns) is 6.
"""

from dataclasses import dataclass
from pathlib import Path

import pytest

from ..common.presets import check_preset
from ..common.unit_bench import label_hex
from ..common.verilog import compile_tb, run_vvp
from .verilog import SDRAM

FOLDER = Path(__file__).parent

# The model's error numbers, as in sdram_chip.v
ERRORS = {
    "x": 1,
    "cke": 2,
    "pause": 3,
    "init": 4,
    "mode": 5,
    "command": 6,
    "active": 7,
    "idle": 8,
    "trcd": 9,
    "trp": 10,
    "tras": 11,
    "trc": 12,
    "trrd": 13,
    "trsc": 14,
    "twr": 15,
    "burst": 16,
    "refresh": 17,
    "write_x": 18,
    "conflict": 19,
}

PAUSE_CYCLES = 10  # the testbench's pause of 100 ns: the first edge after it is 105 ns
REFRESH_CYCLES = 782  # the first cycle after the 7.8125 us since an AUTO REFRESH

# {CS#, RAS#, CAS#, WE#}
NOP = 0b0111
ACTIVATE = 0b0011
READ = 0b0101
WRITE = 0b0100
PRECHARGE = 0b0010
AUTO_REFRESH = 0b0001
MODE_REGISTER = 0b0000
BURST_STOP = 0b0110

Value = int | str  # a number, or hex digits that may be x or z


@dataclass
class Cycle:
    label: str = ""
    command: int = NOP
    ba: int = 0
    a: Value = 0
    dqm: int = 0
    dq: Value | None = None  # driven on DQ by the testbench
    cke: int = 1

    def vector(self) -> str:
        driven = self.dq is not None
        columns = [self.cke, self.command, self.ba, self.a, self.dqm, int(driven)]
        columns.append(self.dq if self.dq is not None else 0)
        return " ".join([label_hex(self.label), *(_hex(c) for c in columns)])


@dataclass
class Out:
    dq: str  # at the rising edge
    between: str  # 4 ns after it
    error: int


def _hex(value: Value) -> str:
    return f"{value:x}" if isinstance(value, int) else value


def nops(n: int, dqm: int = 0, label: str = "") -> list[Cycle]:
    return [Cycle(label, dqm=dqm) for _ in range(n)]


def activate(bank: int, row: int) -> list[Cycle]:
    return [Cycle("activate", ACTIVATE, bank, row)]


def read(bank: int, column: int, auto: bool = True) -> list[Cycle]:
    return [Cycle("read", READ, bank, column | auto << 10)]


def write(
    bank: int,
    column: int,
    words: list[Value],
    auto: bool = True,
    dqm: list[int] | None = None,
) -> list[Cycle]:
    """The WRITE with the first word, and the 7 cycles with the others."""
    dqm = dqm or [0] * 8
    first = Cycle("write", WRITE, bank, column | auto << 10, dqm[0], words[0])
    return [first] + [Cycle("", dqm=m, dq=w) for w, m in zip(words[1:], dqm[1:])]


def precharge(bank: int) -> list[Cycle]:
    return [Cycle("precharge", PRECHARGE, bank, 0)]


def precharge_all() -> list[Cycle]:
    return [Cycle("precharge al", PRECHARGE, 0, 1 << 10, dqm=3)]


def refresh() -> list[Cycle]:
    return [Cycle("refresh", AUTO_REFRESH, dqm=3)]


def mode(value: int = 0x23) -> list[Cycle]:
    return [Cycle("mode", MODE_REGISTER, value >> 13, value & 0x1FFF, dqm=3)]


def pause() -> list[Cycle]:
    return nops(PAUSE_CYCLES, dqm=3, label="pause")


def init() -> list[Cycle]:
    """The initialization: the pause, PRECHARGE ALL, 8 AUTO REFRESH, the mode."""
    cycles = pause() + precharge_all() + nops(1, dqm=3)
    for _ in range(8):
        cycles += refresh() + nops(5, dqm=3)
    return cycles + mode() + nops(1)


def run(cycles: list[Cycle], name: str, vcd: Path | None = None) -> list[Out]:
    out_dir = SDRAM.build_dir / "modules" / "sdram_chip" / name
    vvp = compile_tb(
        FOLDER / "tb_sdram_chip.v",
        SDRAM.rtl_files,
        out_dir / "tb.vvp",
        include_dirs=SDRAM.include_dirs,
    )
    vectors = out_dir / "vectors.hex"
    outputs = out_dir / "outputs.hex"
    vectors.write_text("".join(c.vector() + "\n" for c in cycles))
    plusargs: dict[str, object] = {"vectors": vectors, "outputs": outputs}
    if vcd is not None:
        vcd.parent.mkdir(parents=True, exist_ok=True)
        plusargs["vcd"] = vcd
    run_vvp(vvp, **plusargs)
    lines = [line.split() for line in outputs.read_text().splitlines()]
    assert len(lines) == len(cycles), "the testbench didn't answer every cycle"
    return [Out(dq, between, int(error)) for dq, between, error in lines]


WORDS = [0x1000 + 0x111 * i for i in range(8)]


def write_then_read(first: int) -> tuple[list[Cycle], int]:
    """A line written and read back from word `first`. Returns the cycles and the
    index of the READ."""
    cycles = init() + activate(1, 0x1234) + nops(1)
    cycles += write(1, 0x18, list(WORDS))
    # The auto-precharge starts 2 cycles after the last word, and tRP is 2 more.
    cycles += nops(3) + activate(1, 0x1234) + nops(1)
    at = len(cycles)
    cycles += read(1, 0x18 | first) + nops(10)
    return cycles, at


@pytest.mark.parametrize("first", [0, 5])
def test_write_then_read(first: int) -> None:
    cycles, at = write_then_read(first)
    out = run(cycles, f"write_then_read_{first}")
    assert all(o.error == 0 for o in out), [o.error for o in out]
    # The words at the edges 2 to 9 after the READ, in the burst order
    assert [o.dq for o in out[at + 2 : at + 10]] == [
        f"{WORDS[(first + i) % 8]:04x}" for i in range(8)
    ]
    # Z before the burst and after it, X between the edges while it runs
    assert out[at + 1].dq == "zzzz"
    assert out[at + 10].dq == "zzzz"
    assert [o.between for o in out[at + 1 : at + 10]] == ["xxxx"] * 9
    assert out[at + 10].between == "zzzz"


def test_dqm() -> None:
    """DQM masks the bytes of a write at the same edge, and of a read 2 edges later."""
    cycles = init() + activate(0, 7) + nops(1)
    cycles += write(0, 0, list(WORDS), dqm=[0, 1, 2, 3, 0, 0, 0, 0])
    cycles += nops(3) + activate(0, 7) + nops(1)
    at = len(cycles)
    cycles += read(0, 0)
    # DQM high at the edges after the READ masks the words of the edges 2 later
    cycles += [Cycle(dqm=0), Cycle(dqm=0), Cycle(dqm=2), Cycle(dqm=0)] + nops(7)
    out = run(cycles, "dqm")
    assert all(o.error == 0 for o in out)
    words = [o.dq for o in out[at + 2 : at + 10]]
    # DQM bit 0 masks the low byte. The write kept the low byte of word 1 (0), the high
    # byte of word 2, and both of word 3. The read masks the high byte of word 3.
    assert words[:4] == ["1000", "1100", "0022", "zz00"]
    assert words[4:] == [f"{w:04x}" for w in WORDS[4:]]


def test_refresh_interval() -> None:
    """An AUTO REFRESH every 7.8 us keeps the check quiet; it starts after the
    initialization."""
    cycles = init()
    last = max(i for i, c in enumerate(cycles) if c.command == AUTO_REFRESH)
    cycles += nops(last + REFRESH_CYCLES - 1 - len(cycles))
    cycles += refresh() + nops(REFRESH_CYCLES - 2)
    out = run(cycles, "refresh_interval")
    assert all(o.error == 0 for o in out)


def error_case(rule: str) -> list[Cycle]:
    """Cycles whose last cycle breaks `rule`, and nothing before it."""
    words: list[Value] = list(WORDS)
    match rule:
        case "x":
            return init() + [Cycle("activate", ACTIVATE, 0, "x")]
        case "cke":
            return init() + [Cycle(cke=0)]
        case "pause":
            return nops(5, dqm=3) + precharge_all()
        case "init":
            return pause() + precharge_all() + nops(1, dqm=3) + activate(0, 0)
        case "mode":
            return pause() + precharge_all() + nops(1, dqm=3) + mode(0x22)
        case "command":
            return init() + [Cycle("burst stop", BURST_STOP)]
        case "active":
            return init() + activate(0, 0) + nops(5) + refresh()
        case "idle":
            return init() + read(0, 0)
        case "trcd":
            return init() + activate(0, 0) + read(0, 0)
        case "trp":
            return init() + activate(0, 0) + nops(4) + precharge(0) + activate(0, 0)
        case "tras":
            return init() + activate(0, 0) + nops(3) + precharge(0)
        case "trc":
            return init() + refresh() + nops(4) + activate(0, 0)
        case "trrd":
            return init() + activate(0, 0) + activate(1, 0)
        case "trsc":
            return pause() + precharge_all() + nops(1, dqm=3) + mode() + refresh()
        case "twr":
            cycles = init() + activate(0, 0) + nops(1)
            return cycles + write(0, 0, words, auto=False) + precharge(0)
        case "burst":
            return init() + activate(0, 0) + nops(1) + read(0, 0) + nops(3) + read(0, 0)
        case "refresh":
            cycles = init()
            last = max(i for i, c in enumerate(cycles) if c.command == AUTO_REFRESH)
            return cycles + nops(last + REFRESH_CYCLES + 1 - len(cycles))
        case "write_x":
            words[3] = "x"
            cycles = init() + activate(0, 0) + nops(1) + write(0, 0, words)
            return cycles[: len(cycles) - 4]
        case "conflict":
            cycles, at = write_then_read(0)
            cycles = cycles[: at + 4]
            cycles[at + 3].dq = 0x1234
            return cycles
    raise ValueError(rule)


@pytest.mark.parametrize("rule", ERRORS)
def test_error(rule: str) -> None:
    cycles = error_case(rule)
    out = run(cycles, f"error_{rule}")
    assert [o.error for o in out[:-1]] == [0] * (len(out) - 1), "an error came early"
    assert out[-1].error == ERRORS[rule]


def test_demo() -> None:
    cycles, _ = write_then_read(5)
    vcd = SDRAM.wave_dir / "sdram_chip.vcd"
    out = run(cycles, "demo", vcd)
    assert all(o.error == 0 for o in out)
    check_preset(FOLDER / "sdram_chip.gtkw", vcd)
