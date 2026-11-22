"""Run a program on the RTL and on the ISS, and compare the two retire traces.

The RTL runs in the testbench modules/tb_b32p4.v, compiled once per test process for
each number of cache lines.

Interrupts come at times the ISS can't know, so the RTL runs first, and the ISS takes
each interrupt at the instruction where the RTL took it. The ISS only does that where
the ISA allows it, so an interrupt at the wrong place is still a difference. A program
gets its interrupts from comments such as `; interrupt 2 at cycle 120`: interrupt
input 3 goes high at the end of cycle 120, and low again PULSE cycles later.

A program runs from the ROM, or from SDRAM, through the L1I: a program with interrupts,
because the interrupt handler is at address 4, and a program with the comment
`; runs from SDRAM`. The ROM then does what the bootloader does: it enables interrupts,
which are disabled after a reset, and jumps to address 0. It has a halt at
ROM_BASE + 4, for a program that ends by jumping to the ROM.

A program that ends with `ccache` (after its last write to SDRAM) has written every dirty
line back, so the SDRAM must then hold what the ISS's memory holds. The testbench writes out
the SDRAM at DATA_BASE, where the random programs keep their data, and the comparison
checks it too. This finds a write-back that got lost, also of a line that the program
never reads again.

A program with the comment `; reset at cycle C` gets a reset at the end of cycle C. The
ISS runs the instructions that the RTL retired before it, resets, and the comparison
goes on with what the RTL does after the reset, from the start of the program. SDRAM is
undefined after a reset, and the ISS clears it, so it loads the program's SDRAM image
again: that's what the testbench's SDRAM still has, as long as the program doesn't read
a line that was written back before the reset. RtlTrace.reset says where the reset came.

A program with the comment `; devices` runs with the device bridge and the devices of
Hardware/io on the bus, instead of the bus model (Tests/io/devices.vh), and their
interrupts instead of a schedule. The devices count cycles and the UART's line runs in
cycles, which the ISS doesn't have, so the ISS takes the word of every device read from
the RTL's trace (Replay). A comment `; uart input "text" at cycle C` sends the bytes of
the Python string to the UART from cycle C on. The bytes that the UART sends must be the
bytes that the program wrote to it in the ISS, and those of a comment
`; uart output "text"`, if the program has one.
"""

import ast
import functools
import re
from dataclasses import dataclass, field
from pathlib import Path

from b32asm.assembler import assemble
from b32sim.devices import Board, CycleCounter, DeviceBridge, Timer, Uart
from b32sim.sim import (
    ROM_BASE,
    ROM_SIZE,
    SDRAM_BASE,
    SDRAM_SIZE,
    Machine,
    Ram,
    SimError,
)

from ..common.verilog import compile_tb as compile_area_tb
from ..io.verilog import IO
from ..sdram.verilog import SDRAM
from .verilog import BUILD_DIR, CPU, MODULES_DIR, REPO, RTL_FILES, compile_tb, run_vvp

COMPARISON_DIR = BUILD_DIR / "iss_comparison"
CONTEXT = 10  # instructions shown before a mismatch
PULSE = 2  # the cycles an interrupt input stays high
SDRAM_CODE_SIZE = 0x10000  # SDRAM below the data of the random programs

DATA_BASE = 0x10000  # the SDRAM that the testbench writes out after a final ccache
DATA_BYTES = 0x2000

INTERRUPT_COMMENT = re.compile(r"^\s*; interrupt (\d) at cycle (\d+)", re.MULTILINE)
RESET_COMMENT = re.compile(r"^\s*; reset at cycle (\d+)", re.MULTILINE)
SDRAM_COMMENT = "; runs from SDRAM"
DEVICES_COMMENT = re.compile(r"^\s*; devices\s*$", re.MULTILINE)
UART_INPUT_COMMENT = re.compile(r'^\s*; uart input (".*") at cycle (\d+)', re.MULTILINE)
UART_OUTPUT_COMMENT = re.compile(r'^\s*; uart output (".*")', re.MULTILINE)
BOOT_ROM = """
    jump Boot
    halt
Boot:
    load32 0xFFFFFF00 r1
    load 1 r2
    write 16 r1 r2  ; INT_ENABLE
    jump 0
"""
CCACHE = 0x70000000  # the instruction word of ccache, and of ccached with bit 0 set


@functools.cache
def compile_b32p4(lines: int, devices: bool = False, controller: bool = False) -> Path:
    """Compile the ISS comparison testbench with `lines` lines in each L1 cache, with
    the devices instead of the bus model if `devices`, and with the SDRAM controller and
    the chip models instead of the line model if `controller`."""
    if not devices and not controller:
        return compile_tb(
            MODULES_DIR / "tb_b32p4.v",
            RTL_FILES,
            COMPARISON_DIR / f"tb_b32p4_{lines}.vvp",
            {"LINES": lines},
        )
    rtl_files = CPU.rtl_files
    include_dirs = CPU.include_dirs
    defines = []
    suffix = ""
    if devices:
        rtl_files += IO.rtl_files
        include_dirs += IO.include_dirs
        defines.append("DEVICES")
        suffix += "_devices"
    if controller:
        rtl_files += SDRAM.rtl_files
        include_dirs += SDRAM.include_dirs
        defines.append("SDRAM_CONTROLLER")
        suffix += "_controller"
    return compile_area_tb(
        MODULES_DIR / "tb_b32p4.v",
        rtl_files,
        COMPARISON_DIR / f"tb_b32p4_{lines}{suffix}.vvp",
        {"LINES": lines},
        include_dirs,
        defines,
    )


class Replay:
    """The devices of the ISS in a comparison with the RTL's devices.

    A device read gives the word of the RTL's record (`word`), and the interrupts come
    from the RTL's trace, so the models' levels are left out. The models still refuse
    the accesses that the devices refuse, and the UART's model collects the output.
    """

    def __init__(self) -> None:
        self.uart = Uart()
        self.bridge = DeviceBridge([self.uart, CycleCounter(), Timer(), Board()])
        self.word: int | None = None

    def read_word(self, offset: int, size: int = 4) -> int:
        word = self.bridge.read_word(offset, size)
        return word if self.word is None else self.word

    def write_word(self, offset: int, value: int, mask: int) -> None:
        self.bridge.write_word(offset, value, mask)

    def tick(self) -> None:
        self.bridge.tick()

    def reset(self) -> None:
        self.bridge.reset()

    def interrupt_levels(self) -> int:
        return 0


CONTROLLER = "controller"  # Memories.sdram for the SDRAM controller


@dataclass(frozen=True)
class Memories:
    """The caches and the SDRAM model of the testbench.

    `lines` is the number of lines of each L1 cache. `sdram` is the latency of the SDRAM
    line model, the cycles to the first word of a line (at least 1), or "rand" for a
    random latency per request from `seed`, or CONTROLLER for the SDRAM controller with
    two chip models instead of the line model.
    """

    lines: int = 128
    sdram: str = "7"
    seed: int = 0

    def plusargs(self) -> dict[str, object]:
        if self.sdram == CONTROLLER:
            return {"seed": self.seed}
        return {"sdram_latency": self.sdram, "seed": self.seed}

    def options(self) -> str:
        """The same as options of `fpgc cpu wave`, empty for the defaults."""
        if self == DEFAULT:
            return ""
        return (
            f" --lines {self.lines} --sdram-latency {self.sdram}"
            f" --latency-seed {self.seed}"
        )


DEFAULT = Memories()  # the caches of B32P4 with the SDRAM controller's timing


@dataclass
class RtlTrace:
    records: list[str]  # one per retired instruction, in the ISS trace format
    cycles: list[int]  # the cycle in which each one retired
    ending: str  # the testbench's last line, which says how it ended
    # the lines read from SDRAM and written to it (address bits [25:5]), in order
    sdram_reads: list[int] = field(default_factory=list)
    sdram_writes: list[int] = field(default_factory=list)
    # the testbench's line about a reset in the middle of the run, if there was one, and
    # the instructions that retired before it
    reset: str | None = None
    before_reset: "RtlTrace | None" = None
    memory_checked: bool = False  # the SDRAM was compared with the ISS's memory
    uart_output: bytes = b""  # what the UART sent, with the devices


def interrupt_schedule(source: str) -> list[tuple[int, int, int]]:
    """The interrupt comments of a program, as (cycle, input, value) in time order."""
    events = []
    for number, cycle in INTERRUPT_COMMENT.findall(source):
        events += [(int(cycle), int(number), 1), (int(cycle) + PULSE, int(number), 0)]
    return sorted(events)


def run_iss_comparison(
    source: str,
    name: str,
    vcd: Path | None = None,
    max_cycles: int = 100_000,
    strict: bool = True,
    memories: Memories = DEFAULT,
) -> RtlTrace:
    """Run `source` on the RTL and the ISS; fail on a difference.

    The program is assembled at the ROM, or at the start of SDRAM if it has interrupts
    or SDRAM_COMMENT. With `strict` off, the ISS runs in hardware mode, for programs
    with illegal cases. `memories` sets the caches and the memory models of the RTL;
    without interrupts, the retire trace must be the same for all of them, only the
    cycles differ. The files of the run are kept in build/cpu/iss_comparison/<name>/.
    """
    __tracebackhide__ = True
    out_dir = COMPARISON_DIR / name
    out_dir.mkdir(parents=True, exist_ok=True)
    program_file = out_dir / "program.asm"
    program_file.write_text(source)
    schedule = interrupt_schedule(source)
    if schedule or SDRAM_COMMENT in source:
        rom = assemble(BOOT_ROM, ROM_BASE)
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
    sdram_log = out_dir / "sdram.log"
    plusargs: dict[str, object] = {
        "rom": rom_file,
        "trace": trace_file,
        "sdram_log": sdram_log,
        "max_cycles": max_cycles,
        **memories.plusargs(),
    }
    if sdram.words:
        sdram_file = out_dir / "sdram.hex"
        sdram_file.write_text("".join(f"{w:08x}\n" for w in sdram.words))
        plusargs["sdram"] = sdram_file
    if schedule:
        interrupts_file = out_dir / "interrupts.txt"
        interrupts_file.write_text("".join(f"{c} {n} {v}\n" for c, n, v in schedule))
        plusargs["interrupts"] = interrupts_file
    devices = DEVICES_COMMENT.search(source) is not None
    uart_output = out_dir / "uart_output.txt"
    uart_output.unlink(missing_ok=True)
    if devices:
        plusargs["uart_output"] = uart_output
        uart_input = [
            (int(cycle), byte)
            for text, cycle in UART_INPUT_COMMENT.findall(source)
            for byte in ast.literal_eval(text).encode("latin-1")
        ]
        if uart_input:
            input_file = out_dir / "uart_input.txt"
            input_file.write_text("".join(f"{c} {b:02x}\n" for c, b in uart_input))
            plusargs["uart_input"] = input_file
    if vcd is not None:
        vcd.parent.mkdir(parents=True, exist_ok=True)
        plusargs["vcd"] = vcd
    reset = RESET_COMMENT.search(source)
    if reset:
        plusargs["reset_at"] = int(reset.group(1))
    dump_file = out_dir / "sdram_dump.hex"
    dump_file.unlink(missing_ok=True)
    plusargs["sdram_dump"] = dump_file
    controller = memories.sdram == CONTROLLER
    output = run_vvp(
        compile_b32p4(memories.lines, devices, controller), **plusargs
    ).splitlines()

    reads = [line.split() for line in sdram_log.read_text().splitlines()]
    rtl = RtlTrace(
        [],
        [],
        output[-1] if output else "no output",
        [int(r[2], 16) for r in reads if r[1] == "read"],
        [int(r[2], 16) for r in reads if r[1] == "write"],
    )
    rtl.reset = next((line for line in output if line.startswith("reset in")), None)
    for line in trace_file.read_text().splitlines():
        cycle, record = line.split(" ", 1)
        if record == "reset":
            rtl.before_reset = RtlTrace(rtl.records, rtl.cycles, rtl.reset or "")
            rtl.records, rtl.cycles = [], []
            continue
        rtl.cycles.append(int(cycle))
        rtl.records.append(record)
    assert not reset or rtl.reset, "the reset didn't come before the HALT"

    machine = Machine(strict=strict, io=Replay() if devices else Ram())
    machine.load(ROM_BASE, rom.to_bytes())
    machine.load(SDRAM_BASE, sdram.to_bytes())
    rerun = f"fpgc cpu wave b32p4 {program_file.relative_to(REPO)}{memories.options()}"
    if rtl.before_reset is not None:
        before = [iss_step(machine, r) for r in rtl.before_reset.records]
        (out_dir / "iss_before_reset.trace").write_text(
            "".join(f"{r}\n" for r in before)
        )
        compare(before, rtl.before_reset, rerun)
        machine.reset()
        machine.load(SDRAM_BASE, sdram.to_bytes())
    expected = run_iss(machine, rtl.records, max_cycles)
    (out_dir / "iss.trace").write_text("".join(f"{r}\n" for r in expected))
    compare(expected, rtl, rerun)
    if written_back(expected):
        compare_memory(machine, dump_file, rerun)
        rtl.memory_checked = True
    if isinstance(machine.io, Replay):
        rtl.uart_output = compare_uart(source, machine.io.uart, uart_output, rerun)
    return rtl


def compare_uart(source: str, uart: Uart, output_file: Path, rerun: str) -> bytes:
    """Fail if the bytes from the RTL's UART aren't those that the ISS's program wrote
    to it, or those of the program's output comment. Returns them."""
    __tracebackhide__ = True
    lines = output_file.read_text().split("\n") if output_file.exists() else []
    bad = [line for line in lines if line.endswith(" bad")]
    assert not bad, f"the UART sent a byte without its stop bit\nwaveform:  {rerun}"
    sent = bytes(int(line, 16) for line in lines if line)
    assert sent == bytes(uart.output), (
        f"the UART sent {sent!r}, the ISS's program wrote {bytes(uart.output)!r}"
        f"\nwaveform:  {rerun}"
    )
    comment = UART_OUTPUT_COMMENT.search(source)
    if comment:
        expected = ast.literal_eval(comment.group(1)).encode("latin-1")
        assert sent == expected, f"the UART sent {sent!r}, expected {expected!r}"
    return sent


def written_back(trace: list[str]) -> bool:
    """Whether the SDRAM holds every write of the trace: there's a ccache or ccached after
    the last write to SDRAM (an interrupt handler may still run after it)."""
    words = [int(r.split()[1], 16) & ~1 for r in trace]
    if CCACHE not in words:
        return False
    last = len(words) - 1 - words[::-1].index(CCACHE)
    writes = [re.search(r" write ([\da-f]{8})", r) for r in trace[last:]]
    return not any(w and int(w.group(1), 16) < SDRAM_SIZE for w in writes)


def compare_memory(machine: Machine, dump_file: Path, rerun: str) -> None:
    """Fail at the first word of the SDRAM dump that differs from the ISS's memory."""
    __tracebackhide__ = True
    words = [int(w, 16) for w in dump_file.read_text().split()]
    assert len(words) == DATA_BYTES // 4, "the testbench didn't write out the SDRAM"
    for n, word in enumerate(words):
        address = DATA_BASE + 4 * n
        want = machine.read_word(address)
        if word != want:
            raise AssertionError(
                f"after the final ccache, the SDRAM has {word:08x} at {address:08x},"
                f" the ISS {want:08x}\nwaveform:  {rerun}"
            )


def run_iss(machine: Machine, rtl_records: list[str], max_steps: int) -> list[str]:
    """Run the ISS until a HALT retires, with the interrupts the RTL took.

    Before each instruction, an interrupt that the RTL took at that point of its trace
    is made pending, so the ISS takes it there too, if the ISA allows it.
    """
    trace = []
    for i in range(max_steps):
        trace.append(
            iss_step(machine, rtl_records[i] if i < len(rtl_records) else None)
        )
        if machine.halted:
            return trace
    raise SimError(f"no HALT within {max_steps} steps")


def iss_step(machine: Machine, rtl_record: str | None) -> str:
    """One step of the ISS, which takes an interrupt if the RTL's record has one."""
    taken = re.search(r" irq (\d)", rtl_record) if rtl_record else None
    if taken:
        machine.request_interrupt(int(taken.group(1)))
    if isinstance(machine.io, Replay):
        word = re.search(r" r\d+=([\da-f]{8})", rtl_record) if rtl_record else None
        machine.io.word = int(word.group(1), 16) if word else None
    return str(machine.step())


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
