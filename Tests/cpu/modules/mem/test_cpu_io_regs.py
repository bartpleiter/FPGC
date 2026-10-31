"""Unit test of cpu_io_regs, against the rules of the CPU-internal I/O registers and
the interrupt state."""

import random
from pathlib import Path

from b32sim.sim import ErrorKind

from ...unit_bench import UnitBench, Vector

BENCH = UnitBench(
    unit="cpu_io_regs",
    folder=Path(__file__).parent,
    inputs=[
        "reset",
        "addr",
        "word",
        "write",
        "write_data",
        "sp",
        "pc",
        "events",
        "take_interrupt",
        "interrupt_id",
        "reti",
    ],
    outputs=[
        "bad_read",
        "bad_write",
        "read_value",
        "sp_write",
        "pc_backup",
        "interrupts_enabled",
        "last_interrupt",
    ],
)

# The registers by address bits [3:2]
PC_BACKUP, SP, ERR_STATUS, ERR_PC = range(4)
NAMES = ["PC_BACKUP", "SP", "ERR_STATUS", "ERR_PC"]

Case = tuple[str, Vector]  # label, inputs


def case(
    label: str,
    reg: int = PC_BACKUP,
    write: int | None = None,
    word: bool = True,
    sp: int = 0,
    events: tuple[ErrorKind, ...] = (),
    pc: int = 0,
    reset: bool = False,
    interrupt: int = 0,
    reti: bool = False,
) -> Case:
    """An access to register `reg`: a write of `write`, or else a read.

    `interrupt` is the number of an interrupt taken at the instruction at `pc`.
    """
    return label, {
        "reset": int(reset),
        "addr": reg,
        "word": int(word),
        "write": int(write is not None),
        "write_data": write or 0,
        "sp": sp,
        "pc": pc,
        "events": sum(1 << (k - 1) for k in set(events)),
        "take_interrupt": int(interrupt != 0),
        "interrupt_id": interrupt,
        "reti": int(reti),
    }


def read(reg: int, **kwargs) -> Case:
    return case(f"read {NAMES[reg].lower()}"[:12], reg, **kwargs)


# The demo writes and reads PC_BACKUP and SP, then has two events, reads the error
# register, clears it, and ends with a write to ERR_STATUS by an instruction that has
# an event itself, which is recorded after the clear.
DEMO = [
    case("write pc_bak", PC_BACKUP, write=0x1234),
    read(PC_BACKUP),
    case("write sp", SP, write=0x105, sp=4),
    read(SP, sp=5),
    case("readb sp", SP, word=False, sp=5),
    case("unaligned", PC_BACKUP, events=(ErrorKind.UNALIGNED_ACCESS,), pc=0x100),
    case(
        "two events",
        PC_BACKUP,
        events=(ErrorKind.BAD_ADDRESS, ErrorKind.ILLEGAL_INSTRUCTION),
        pc=0x104,
    ),
    read(ERR_STATUS),
    read(ERR_PC),
    case("write err_pc", ERR_PC, write=0x55),
    case("clear", ERR_STATUS, write=0),
    read(ERR_STATUS),
    case("clear+event", ERR_STATUS, write=0, events=(ErrorKind.STACK_OVERFLOW,), pc=8),
    read(ERR_STATUS),
    read(ERR_PC),
    case("reset", reset=True),
    read(ERR_STATUS),
    case("interrupt 3", interrupt=3, pc=0x40),
    read(PC_BACKUP),
    case("reti", reti=True),
    read(PC_BACKUP),
]


def model(cases: list[Case]) -> list[Vector]:
    """What the registers give in each cycle, before the clock edge."""
    pc_backup = flags = first = err_pc = last_interrupt = 0
    enabled = 1
    out = []
    for _, v in cases:
        reg, word = v["addr"], v["word"]
        bad_read = not word
        bad_write = not word or reg == ERR_PC
        values = [pc_backup, v["sp"], flags | first << 8, err_pc]
        writes = v["write"] and not bad_write
        out.append(
            {
                "bad_read": int(bad_read),
                "bad_write": int(bad_write),
                "read_value": 0 if bad_read else values[reg],
                "sp_write": int(bool(writes) and reg == SP),
                "pc_backup": pc_backup,
                "interrupts_enabled": enabled,
                "last_interrupt": last_interrupt,
            }
        )
        if v["reset"]:
            pc_backup = flags = first = err_pc = last_interrupt = 0
            enabled = 1
            continue
        if v["take_interrupt"]:
            pc_backup, enabled, last_interrupt = v["pc"], 0, v["interrupt_id"]
        elif v["reti"]:
            enabled = 1
        if writes and reg == PC_BACKUP:
            pc_backup = v["write_data"]
        if writes and reg == ERR_STATUS:
            flags = first = err_pc = 0
        if v["events"]:
            flags |= v["events"]
            if not first:
                first = (v["events"] & -v["events"]).bit_length()
                err_pc = v["pc"]
    return out


def check(cases: list[Case], name: str) -> None:
    __tracebackhide__ = True
    BENCH.check(
        [v for _, v in cases], model(cases), name, [label for label, _ in cases]
    )


def test_every_access() -> None:
    """Read and write every register, as a word and not."""
    cases = []
    for reg in range(4):
        for word in (True, False):
            cases.append(case("write", reg, write=0x80 + reg, word=word, sp=reg))
            cases.append(case("read", reg, word=word, sp=reg))
    check(cases, "every_access")


def test_first_event() -> None:
    """Every kind is recorded as the first, also together with every higher kind."""
    cases = []
    for kind in ErrorKind:
        for other in ErrorKind:
            events = (kind, other) if other >= kind else (kind,)
            cases.append(case("clear", ERR_STATUS, write=0))
            cases.append(case("event", events=events, pc=kind * 16 + other))
            cases.append(case("event", events=(other,), pc=0xBAD))
            cases.append(read(ERR_STATUS))
            cases.append(read(ERR_PC))
    check(cases, "first_event")


def test_interrupt_and_reti() -> None:
    """An interrupt saves the PC, disables interrupts and records its number. RETI
    enables them again, and a write to PC_BACKUP changes the return address."""
    cases = []
    for n in range(1, 8):
        cases.append(case("interrupt", interrupt=n, pc=0x100 * n))
        cases.append(read(PC_BACKUP))
        cases.append(case("write pc_bak", PC_BACKUP, write=0x2000 + n))
        cases.append(read(PC_BACKUP))
        cases.append(case("reti", reti=True))
        cases.append(case("reti", reti=True))
    check(cases, "interrupt_and_reti")


def test_random() -> None:
    rng = random.Random(1)
    cases = []
    for _ in range(5000):
        r = rng.random()
        events = tuple(k for k in ErrorKind if rng.random() < 0.05)
        special = rng.random()
        # An interrupt is taken instead of a jump, so never together with a write.
        interrupt = rng.randint(1, 7) if special < 0.05 and r >= 0.3 else 0
        cases.append(
            case(
                "random",
                rng.randrange(4),
                write=rng.getrandbits(32) if r < 0.3 else None,
                word=rng.random() < 0.9,
                sp=rng.getrandbits(8),
                events=events,
                pc=rng.getrandbits(32),
                reset=rng.random() < 0.005,
                interrupt=interrupt,
                reti=0.05 <= special < 0.1,
            )
        )
    check(cases, "random")


def test_demo() -> None:
    BENCH.demo([v for _, v in DEMO], model(DEMO), [label for label, _ in DEMO])
