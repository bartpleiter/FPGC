"""Run a program on the RTL and on the ISS, and compare the two retire traces.

The RTL runs in the testbench modules/tb_b32p4.v,
compiled once per test session.
"""

from dataclasses import dataclass
from pathlib import Path

from b32asm.assembler import assemble
from b32sim.sim import ROM_BASE, ROM_SIZE, Machine

from .verilog import BUILD_DIR, MODULES_DIR, REPO, RTL_FILES, compile_tb, run_vvp

COMPARISON_DIR = BUILD_DIR / "iss_comparison"
CONTEXT = 10  # instructions shown before a mismatch


def compile_b32p4() -> Path:
    """Compile the ISS comparison testbench with the CPU."""
    return compile_tb(
        MODULES_DIR / "tb_b32p4.v", RTL_FILES, COMPARISON_DIR / "tb_b32p4.vvp"
    )


@dataclass
class RtlTrace:
    records: list[str]  # one per retired instruction, in the ISS trace format
    cycles: list[int]  # the cycle in which each one retired
    ending: str  # the testbench's last line, which says how it ended


def run_iss_comparison(
    vvp: Path,
    source: str,
    name: str,
    vcd: Path | None = None,
    max_cycles: int = 100_000,
) -> RtlTrace:
    """Run `source` (assembled at the ROM) on the RTL and the ISS; fail on a difference.

    The files of the run are kept in build/cpu/iss_comparison/<name>/.
    """
    __tracebackhide__ = True
    out_dir = COMPARISON_DIR / name
    out_dir.mkdir(parents=True, exist_ok=True)
    program_file = out_dir / "program.asm"
    program_file.write_text(source)
    program = assemble(source, ROM_BASE, str(program_file))
    words = program.words
    assert len(words) <= ROM_SIZE // 4, "the program doesn't fit in the ROM"

    machine = Machine()
    machine.load(ROM_BASE, program.to_bytes())
    expected = [str(r) for r in machine.run(max_steps=max_cycles)]
    (out_dir / "iss.trace").write_text("".join(f"{r}\n" for r in expected))

    rom_file = out_dir / "rom.hex"
    rom_file.write_text(
        "".join(f"{w:08x}\n" for w in words + [0] * (ROM_SIZE // 4 - len(words)))
    )
    trace_file = out_dir / "rtl.trace"
    plusargs: dict[str, object] = {
        "rom": rom_file,
        "trace": trace_file,
        "max_cycles": max_cycles,
    }
    if vcd is not None:
        vcd.parent.mkdir(parents=True, exist_ok=True)
        plusargs["vcd"] = vcd
    output = run_vvp(vvp, **plusargs).splitlines()

    rtl = RtlTrace([], [], output[-1] if output else "no output")
    for line in trace_file.read_text().splitlines():
        cycle, record = line.split(" ", 1)
        rtl.cycles.append(int(cycle))
        rtl.records.append(record)
    compare(expected, rtl, f"fpgc cpu wave b32p4 {program_file.relative_to(REPO)}")
    return rtl


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
