"""Random CPU test programs: valid, guaranteed to terminate, and dense in hazards.

`generate(seed)` returns assembly source. The same seed always gives the same program.

How a program is built:

- r1-r6 are the working registers: every source and destination comes from this small
  pool, so most instructions depend on the ones just before them.
- Fixed registers, never written by the random instructions:
  r10 = CPU-internal I/O base, r11 = scratch for jumps, r12 = loop counter,
  r13 = data window in SDRAM, r14 = data window in video memory (word access only).
- The data windows are filled with known values first, so every read is defined.
- Control flow only goes forward, except the backward branch of a counted loop.
  Forward branches and jumps skip over the next few units, never into a loop.
- The stack is only used outside skipped regions, so its depth is always known.
  It's emptied before the final halt.
- `only` restricts a program to some instructions, for the RTL steps that don't have
  all of them yet. Whatever needs a missing instruction is left out: without `write`
  there are no data windows and no memory accesses, without `load` and `loadhi` the
  registers start at 0, and so on.
"""

import random
from collections.abc import Callable

WORK_REGS = [f"r{n}" for n in range(1, 7)]
SOURCE_REGS = ["r0", *WORK_REGS]

CPU_IO_BASE = 0x1F000000
SP_OFFSET = 4

# SDRAM data: two cache lines at 0x10000, and two that map to the same L1 lines
# 4 KiB further, so accesses cause evictions and write-backs.
SDRAM_DATA_BASE = 0x10000
SDRAM_DATA_OFFSETS = [*range(0, 64, 4), *range(0x1000, 0x1040, 4)]
VIDEO_DATA_BASE = 0x1E400000  # vram32
VIDEO_DATA_OFFSETS = list(range(0, 64, 4))

MAX_STACK_DEPTH = 32

ALU_OPS = ["or", "and", "xor", "add", "sub", "shiftl", "shiftr", "shiftrs"]
ALU_OPS += ["slt", "sltu"]
MULDIV_OPS = ["mults", "multu", "multfp", "divs", "divu", "divfp"]
MULDIV_OPS += ["mods", "modu", "mulshi", "multuhi"]
BRANCH_OPS = ["beq", "bne", "bgt", "bge", "blt", "ble", "bgts", "bges", "blts", "bles"]
READ_OPS = {"read": 4, "readh": 2, "readhu": 2, "readb": 1, "readbu": 1}
WRITE_OPS = {"write": 4, "writeh": 2, "writeb": 1}

# Values that tend to find corner cases, mixed with random ones.
INTERESTING = [0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFF, 0x10000, 31, 32]


Action = Callable[[], None]


def _available(*options: tuple[int, Action, bool]) -> list[tuple[int, Action]]:
    """The (weight, action) pairs of the options whose condition holds."""
    return [(weight, action) for weight, action, ok in options if ok]


class _Generator:
    def __init__(self, seed: int, exclude: set[str], only: set[str] | None) -> None:
        self.rng = random.Random(seed)
        self.exclude = exclude
        self.only = only
        self.lines: list[str] = []
        self.labels = 0
        self.pending: list[list] = []  # [units left, label] of forward branches
        self.stack_depth = 0

        # What the allowed instructions make possible.
        self.alu_ops = self.ops(ALU_OPS)
        self.muldiv_ops = self.ops(MULDIV_OPS)
        self.branch_ops = self.ops(BRANCH_OPS)
        self.read_ops = {o: s for o, s in READ_OPS.items() if self.ok(o)}
        self.write_ops = {o: s for o, s in WRITE_OPS.items() if self.ok(o)}
        self.load32 = self.ok("load", "loadhi")
        # The data windows need their base registers, and are filled with `write`.
        self.memory_ok = self.load32 and self.ok("write")
        self.alu_forms = _available(
            (45, self.alu_regs, bool(self.alu_ops)),
            (35, self.alu_const, bool(self.alu_ops)),
            (10, self.alu_not, self.ok("not")),
            (10, self.alu_load32, self.load32),
        )
        self.special_forms = _available(
            (2, self.savpc, self.ok("savpc")),
            (2, self.readintid, self.ok("readintid")),
            (2, self.read_sp, self.load32 and self.ok("read")),
            (1, self.ccache, self.ok("ccache")),
            (1, self.ccached, self.ok("ccached")),
            (2, self.nop, self.ok("nop")),
        )
        self.straight_forms = _available(
            (50, self.alu, bool(self.alu_forms)),
            (15, self.muldiv, bool(self.muldiv_ops)),
            (30, self.memory, self.memory_ok),
            (5, self.special, bool(self.special_forms)),
        )
        if not self.straight_forms:
            raise ValueError("the allowed instructions can't make a program")
        self.branch_forms = _available(
            (2, self.branch, bool(self.branch_ops)),
            (2, self.jump, self.ok("jump")),
            (2, self.jumpr, self.load32 and self.ok("jumpr")),
            (1, self.skip_savpc, self.ok("savpc", "jumpr") and bool(self.alu_forms)),
            (1, self.skip_jumpro, self.ok("jumpro") and bool(self.alu_forms)),
        )
        self.loop_ok = self.ok("load", "sub", "bne")
        self.stack_ok = self.ok("push", "pop")

    def ok(self, *mnemonics: str) -> bool:
        """Whether the program may use all of these mnemonics."""
        return all(
            m not in self.exclude and (self.only is None or m in self.only)
            for m in mnemonics
        )

    def ops(self, names: list[str]) -> list[str]:
        return [n for n in names if self.ok(n)]

    def pick(self, options: list[tuple[int, Action]]) -> None:
        """Do one of the options, chosen by weight."""
        weights, actions = zip(*options, strict=True)
        self.rng.choices(actions, weights)[0]()

    # ---- Values and operands ----

    def value(self) -> int:
        if self.rng.random() < 0.4:
            return self.rng.choice(INTERESTING)
        return self.rng.getrandbits(32)

    def const16(self) -> int:
        """A signed 16-bit constant."""
        if self.rng.random() < 0.3:
            return self.rng.choice([0, 1, -1, 31, 32, 0x7FFF, -0x8000])
        return self.rng.randint(-0x8000, 0x7FFF)

    def src(self) -> str:
        return self.rng.choice(SOURCE_REGS)

    def dst(self) -> str:
        return "r0" if self.rng.random() < 0.03 else self.rng.choice(WORK_REGS)

    def new_label(self) -> str:
        self.labels += 1
        return f"L{self.labels}"

    def emit(self, *lines: str) -> None:
        self.lines.extend(lines)

    # ---- Straight-line instructions ----

    def alu(self) -> None:
        self.pick(self.alu_forms)

    def alu_regs(self) -> None:
        op = self.rng.choice(self.alu_ops)
        self.emit(f"{op} {self.src()} {self.src()} {self.dst()}")

    def alu_const(self) -> None:
        op = self.rng.choice(self.alu_ops)
        self.emit(f"{op} {self.src()} {self.const16()} {self.dst()}")

    def alu_not(self) -> None:
        self.emit(f"not {self.src()} {self.dst()}")

    def alu_load32(self) -> None:
        d = self.dst()
        v = self.value()
        self.emit(f"load {v & 0xFFFF} {d}", f"loadhi {v >> 16} {d}")

    def muldiv(self) -> None:
        op = self.rng.choice(self.muldiv_ops)
        if self.rng.random() < 0.6:
            self.emit(f"{op} {self.src()} {self.src()} {self.dst()}")
        else:
            self.emit(f"{op} {self.src()} {self.const16()} {self.dst()}")

    def memory(self) -> None:
        if self.rng.random() < 0.8:
            base, offsets, sizes = "r13", SDRAM_DATA_OFFSETS, (1, 2, 4)
        else:
            base, offsets, sizes = "r14", VIDEO_DATA_OFFSETS, (4,)
        reads = [o for o, s in self.read_ops.items() if s in sizes]
        writes = [o for o, s in self.write_ops.items() if s in sizes]
        offset = self.rng.choice(offsets)
        if reads and self.rng.random() < 0.5:
            op = self.rng.choice(reads)
            offset += self.rng.randrange(0, 4, READ_OPS[op])
            self.emit(f"{op} {offset} {base} {self.dst()}")
        else:
            op = self.rng.choice(writes)  # never empty: `write` is allowed
            offset += self.rng.randrange(0, 4, WRITE_OPS[op])
            self.emit(f"{op} {offset} {base} {self.src()}")

    def special(self) -> None:
        self.pick(self.special_forms)

    def savpc(self) -> None:
        self.emit(f"savpc {self.dst()}")

    def readintid(self) -> None:
        self.emit(f"readintid {self.dst()}")

    def read_sp(self) -> None:
        self.emit(f"read {SP_OFFSET} r10 {self.dst()}")

    def ccache(self) -> None:
        self.emit("ccache")

    def ccached(self) -> None:
        self.emit("ccached")

    def nop(self) -> None:
        self.emit("nop")

    def straight(self) -> None:
        """One straight-line instruction (or a load/loadhi pair)."""
        self.pick(self.straight_forms)

    # ---- Units with control flow or the stack ----

    def later_label(self) -> str:
        """A label that unit() places after this unit and up to two more."""
        label = self.new_label()
        self.pending.append([self.rng.randint(1, 3), label])
        return label

    def forward_branch(self) -> None:
        self.pick(self.branch_forms)

    def branch(self) -> None:
        label = self.later_label()
        op = self.rng.choice(self.branch_ops)
        self.emit(f"{op} {self.src()} {self.src()} {label}")

    def jump(self) -> None:
        self.emit(f"jump {self.later_label()}")

    def jumpr(self) -> None:
        self.emit(f"addr2reg {self.later_label()} r11", "jumpr 0 r11")

    def skip_savpc(self) -> None:
        """Skip exactly one instruction, through savpc + jumpr."""
        self.emit("savpc r11", "jumpr 12 r11")
        self.alu()

    def skip_jumpro(self) -> None:
        """Skip exactly one instruction, through jumpro."""
        self.emit("jumpro 8 r0")
        self.alu()

    def loop(self) -> None:
        label = self.new_label()
        self.emit(f"load {self.rng.randint(1, 4)} r12", f"{label}:")
        for _ in range(self.rng.randint(1, 4)):
            self.straight()
        self.emit("sub r12 1 r12", f"bne r12 r0 {label}")

    def stack(self) -> None:
        if self.stack_depth > 0 and self.rng.random() < 0.5:
            self.stack_depth -= 1
            self.emit(f"pop {self.dst()}")
        elif self.stack_depth < MAX_STACK_DEPTH:
            self.stack_depth += 1
            self.emit(f"push {self.src()}")

    def unit(self) -> None:
        self.pick(
            _available(
                (70, self.straight, True),
                (15, self.forward_branch, bool(self.branch_forms)),
                (5, self.loop, self.loop_ok),
                (10, self.stack, self.stack_ok and not self.pending),
            )
        )
        for p in self.pending:
            p[0] -= 1
        for p in [p for p in self.pending if p[0] <= 0]:
            self.emit(f"{p[1]}:")
            self.pending.remove(p)

    # ---- The whole program ----

    def program(self, length: int) -> str:
        if self.load32:
            self.emit(
                f"load32 {CPU_IO_BASE} r10",
                f"load32 {SDRAM_DATA_BASE} r13",
                f"load32 {VIDEO_DATA_BASE} r14",
            )
        if self.memory_ok:
            for base, offsets in (
                ("r13", SDRAM_DATA_OFFSETS),
                ("r14", VIDEO_DATA_OFFSETS),
            ):
                for offset in offsets:
                    self.emit(f"load32 {self.value()} r1", f"write {offset} {base} r1")
        if self.load32:
            for reg in WORK_REGS:
                self.emit(f"load32 {self.value()} {reg}")
        while len(self.lines) < length or self.pending:
            self.unit()
        self.emit(*(f"pop {self.dst()}" for _ in range(self.stack_depth)), "halt")
        return "\n".join(self.lines) + "\n"


def generate(
    seed: int,
    length: int = 500,
    exclude: frozenset[str] = frozenset(),
    only: frozenset[str] | None = None,
) -> str:
    """A random program of about `length` lines.

    It doesn't use the mnemonics in `exclude`, and with `only`, it uses only those
    (plus the final `halt`).
    """
    return _Generator(seed, set(exclude), None if only is None else set(only)).program(
        length
    )
