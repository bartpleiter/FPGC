"""Run a program on the RTL and on the ISS, and compare the two retire traces.

The RTL runs in the testbench modules/tb_b32p4.v,
compiled once per test session.

Interrupts come at times the ISS can't know, so the RTL runs first, and the ISS takes
each interrupt at the instruction where the RTL took it. The ISS only does that where
the ISA allows it, so an interrupt at the wrong place is still a difference. A program
gets its interrupts from comments such as `; interrupt 3 at cycle 120`: interrupt
input 3 goes high at the end of cycle 120, and low again PULSE cycles later. A program
with interrupts runs from SDRAM, because code in ROM is never interrupted, and the ROM
then only has a jump to it.
"""

import re
from dataclasses import dataclass
from pathlib import Path

from b32asm.assembler import assemble
from b32sim.sim import ROM_BASE, ROM_SIZE, SDRAM_BASE, Machine, SimError

from .verilog import BUILD_DIR, MODULES_DIR, REPO, RTL_FILES, compile_tb, run_vvp

COMPARISON_DIR = BUILD_DIR / "iss_comparison"
CONTEXT = 10  # instructions shown before a mismatch
PULSE = 2  # the cycles an interrupt input stays high
SDRAM_CODE_SIZE = 0x10000  # SDRAM below the data of the random programs

INTERRUPT_COMMENT = re.compile(r"^\s*; interrupt (\d) at cycle (\d+)", re.MULTILINE)


def compile_b32p4() -> Path:
    """Compile the ISS comparison testbench with the CPU."""
    return compile_tb(
        MODULES_DIR / "tb_b32p4.v", RTL_FILES, COMPARISON_DIR / "tb_b32p4.vvp"
    )


@dataclass(frozen=True)
class Latency:
    """How many cycles the memory models wait before they answer a request.

    Each is a number, or "rand" for a random number per request from `seed`.
    """

    fetch: str = "0"
    data: str = "0"
    seed: int = 0

    def plusargs(self) -> dict[str, object]:
        return {
            "fetch_latency": self.fetch,
            "data_latency": self.data,
            "seed": self.seed,
        }

    def options(self) -> str:
        """The same as options of `fpgc cpu wave`, empty for the ideal memories."""
        if self == IDEAL:
            return ""
        return (
            f" --fetch-latency {self.fetch} --data-latency {self.data}"
            f" --latency-seed {self.seed}"
        )


IDEAL = Latency()  # memories that answer right away, like a BRAM


@dataclass
class RtlTrace:
    records: list[str]  # one per retired instruction, in the ISS trace format
    cycles: list[int]  # the cycle in which each one retired
    ending: str  # the testbench's last line, which says how it ended


def interrupt_schedule(source: str) -> list[tuple[int, int, int]]:
    """The interrupt comments of a program, as (cycle, input, value) in time order."""
    events = []
    for number, cycle in INTERRUPT_COMMENT.findall(source):
        events += [(int(cycle), int(number), 1), (int(cycle) + PULSE, int(number), 0)]
    return sorted(events)


def run_iss_comparison(
    vvp: Path,
    source: str,
    name: str,
    vcd: Path | None = None,
    max_cycles: int = 100_000,
    strict: bool = True,
    latency: Latency = IDEAL,
) -> RtlTrace:
    """Run `source` on the RTL and the ISS; fail on a difference.

    The program is assembled at the ROM, or at the start of SDRAM if it has interrupts.
    With `strict` off, the ISS runs in hardware mode, for programs with illegal cases.
    `latency` sets the memory models of the RTL; without interrupts, the retire trace
    must be the same for every latency, only the cycles differ. The files of the run
    are kept in build/cpu/iss_comparison/<name>/.
    """
    __tracebackhide__ = True
    out_dir = COMPARISON_DIR / name
    out_dir.mkdir(parents=True, exist_ok=True)
    program_file = out_dir / "program.asm"
    program_file.write_text(source)
    schedule = interrupt_schedule(source)
    if schedule:
        rom = assemble("jump 0", ROM_BASE)
        sdram = assemble(source, SDRAM_BASE, str(program_file))
        assert len(sdram.words) <= SDRAM_CODE_SIZE // 4, "the program is too long"
    else:
        rom = assemble(source, ROM_BASE, str(program_file))
        sdram = assemble("", SDRAM_BASE)
        assert len(rom.words) <= ROM_SIZE // 4, "the program doesn't fit in the ROM"

    rom_file = out_dir / "rom.hex"
    padding = [0] * (ROM_SIZE // 4 - len(rom.words))
    rom_file.write_text("".join(f"{w:08x}\n" for w in rom.words + padding))
    trace_file = out_dir / "rtl.trace"
    plusargs: dict[str, object] = {
        "rom": rom_file,
        "trace": trace_file,
        "max_cycles": max_cycles,
        **latency.plusargs(),
    }
    if sdram.words:
        sdram_file = out_dir / "sdram.hex"
        sdram_file.write_text("".join(f"{w:08x}\n" for w in sdram.words))
        plusargs["sdram"] = sdram_file
    if schedule:
        interrupts_file = out_dir / "interrupts.txt"
        interrupts_file.write_text("".join(f"{c} {n} {v}\n" for c, n, v in schedule))
        plusargs["interrupts"] = interrupts_file
    if vcd is not None:
        vcd.parent.mkdir(parents=True, exist_ok=True)
        plusargs["vcd"] = vcd
    output = run_vvp(vvp, **plusargs).splitlines()

    rtl = RtlTrace([], [], output[-1] if output else "no output")
    for line in trace_file.read_text().splitlines():
        cycle, record = line.split(" ", 1)
        rtl.cycles.append(int(cycle))
        rtl.records.append(record)

    machine = Machine(strict=strict)
    machine.load(ROM_BASE, rom.to_bytes())
    machine.load(SDRAM_BASE, sdram.to_bytes())
    expected = run_iss(machine, rtl.records, max_cycles)
    (out_dir / "iss.trace").write_text("".join(f"{r}\n" for r in expected))

    rerun = f"fpgc cpu wave b32p4 {program_file.relative_to(REPO)}{latency.options()}"
    compare(expected, rtl, rerun)
    return rtl


def run_iss(machine: Machine, rtl_records: list[str], max_steps: int) -> list[str]:
    """Run the ISS until a HALT retires, with the interrupts the RTL took.

    Before each instruction, an interrupt that the RTL took at that point of its trace
    is made pending, so the ISS takes it there too, if the ISA allows it.
    """
    trace = []
    for i in range(max_steps):
        taken = (
            re.search(r" irq (\d)", rtl_records[i]) if i < len(rtl_records) else None
        )
        if taken:
            machine.request_interrupt(int(taken.group(1)))
        trace.append(str(machine.step()))
        if machine.halted:
            return trace
    raise SimError(f"no HALT within {max_steps} steps")


def compare(expected: list[str], rtl: RtlTrace, rerun: str) -> None:
    """Fail at the first record where the RTL trace differs from the ISS trace."""
    __tracebackhide__ = True  # makes pytest show the message, not this function
    pairs = enumerate(zip(expected, rtl.records))
    first = next((i for i, (want, have) in pairs if want != have), None)
    if first is not None:
        i = first
        title = f"RTL and ISS differ at instruction {i} (cycle {rtl.cycles[i]}):"
        lines = [f"  ISS  {expected[i]}", f"  RTL  {rtl.records[i]}"]
    elif len(rtl.records) < len(expected):
        i = len(rtl.records)
        title = (
            f"the RTL stopped after {i} instructions, the ISS retired {len(expected)}:"
        )
        lines = [f"  ISS  {expected[i]}", f"  RTL  {rtl.ending}"]
    elif len(rtl.records) > len(expected):
        i = len(expected)
        title = f"the RTL retired more than the ISS's {i} instructions:"
        lines = [f"  RTL  {rtl.records[i]}"]
    else:
        return
    before = [f"  {r}" for r in expected[max(0, i - CONTEXT) : i]] or ["  (none)"]
    message = [
        title,
        *lines,
        "the instructions before it:",
        *before,
        f"testbench: {rtl.ending}",
        f"waveform:  {rerun}",
    ]
    raise AssertionError("\n".join(message))
