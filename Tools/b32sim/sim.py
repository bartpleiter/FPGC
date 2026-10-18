"""B32P4 instruction-set simulator (ISS).

It executes one instruction per step,
and reports each retired instruction as a trace record.

Two modes:

- strict (default): every illegal case raises IllegalError, so tests catch bugs.
- hardware: illegal cases behave as the hardware does, and set the error register.
"""

from collections.abc import Callable
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Protocol

from b32asm.isa import (
    ALU_OPS,
    ALU_ZEXT_OPS,
    BRANCH_OPS,
    MULDIV_OPS,
    READ_OPS,
    WRITE_OPS,
    Decoded,
    Opcode,
    decode,
    sext,
)

MASK = 0xFFFFFFFF

SDRAM_BASE, SDRAM_SIZE = 0x00000000, 64 << 20
IO_BASE, IO_SIZE = 0x1C000000, 0x02000000
ROM_BASE, ROM_SIZE = 0x1E000000, 4 << 10
CPU_IO_BASE, CPU_IO_SIZE = 0x1F000000, 0x10

# Video memory
VIDEO_REGIONS = {
    "vram32": (0x1E400000, 0x1080),
    "vram8": (0x1E800000, 0x8008),
    "vrampx": (0x1EC00000, 0x20000),
    "palette": (0x1EC80000, 0x400),
}

# CPU-internal I/O registers
PC_BACKUP_ADDR = CPU_IO_BASE + 0x0
SP_ADDR = CPU_IO_BASE + 0x4
ERR_STATUS_ADDR = CPU_IO_BASE + 0x8
ERR_PC_ADDR = CPU_IO_BASE + 0xC

STACK_DEPTH = 256
INTERRUPT_VECTOR = 0x4
NUM_INTERRUPTS = 7


class ErrorKind(IntEnum):
    """Kinds of illegal events. Flag bit = kind - 1 in ERR_STATUS."""

    ILLEGAL_INSTRUCTION = 1
    UNALIGNED_ACCESS = 2
    BAD_ADDRESS = 3
    BAD_JUMP_TARGET = 4
    STACK_OVERFLOW = 5
    STACK_UNDERFLOW = 6


class IllegalError(Exception):
    """An illegal case in strict mode. `kind` is None for simulator-only checks."""

    def __init__(self, kind: ErrorKind | None, pc: int, message: str) -> None:
        super().__init__(f"pc {pc:08x}: {message}")
        self.kind = kind
        self.pc = pc


class SimError(Exception):
    """Something the simulator can't model, such as fetching outside SDRAM and ROM."""


class Device(Protocol):
    """Word storage behind an address region. Offsets are word-aligned."""

    def read_word(self, offset: int) -> int: ...
    def write_word(self, offset: int, value: int, mask: int) -> None: ...


class Ram:
    """Word storage, allocated as it's written. Unwritten words read as 0."""

    def __init__(self) -> None:
        self.words: dict[int, int] = {}

    def read_word(self, offset: int) -> int:
        return self.words.get(offset, 0)

    def write_word(self, offset: int, value: int, mask: int) -> None:
        old = self.words.get(offset, 0)
        self.words[offset] = (old & ~mask) | (value & mask)


class NullIo:
    """The I/O region without devices: reads give 0, and writes are recorded."""

    def __init__(self) -> None:
        self.writes: list[tuple[int, int, int]] = []

    def read_word(self, offset: int) -> int:
        return 0

    def write_word(self, offset: int, value: int, mask: int) -> None:
        self.writes.append((IO_BASE + offset, value, mask))


@dataclass
class Region:
    name: str
    base: int
    size: int
    device: Device
    writable: bool = True


@dataclass
class Retired:
    """What one step did. Formatted as one line of the retire trace."""

    pc: int
    word: int
    rd: tuple[int, int] | None = None  # (register, value)
    store: tuple[int, int, int] | None = None  # (word address, data, byte enables)
    push: int | None = None
    pop: int | None = None
    redirect: int | None = None
    interrupt: int | None = None  # set when an interrupt was taken instead

    def __str__(self) -> str:
        parts = [f"{self.pc:08x} {self.word:08x}"]
        if self.interrupt is not None:
            parts.append(f"irq {self.interrupt}")
        if self.rd is not None:
            parts.append(f"r{self.rd[0]}={self.rd[1]:08x}")
        if self.store is not None:
            parts.append(
                f"store {self.store[0]:08x}={self.store[1]:08x}/{self.store[2]:x}"
            )
        if self.push is not None:
            parts.append(f"push {self.push:08x}")
        if self.pop is not None:
            parts.append(f"pop {self.pop:08x}")
        if self.redirect is not None:
            parts.append(f"-> {self.redirect:08x}")
        return " ".join(parts)


def _signed(x: int) -> int:
    return x - (1 << 32) if x & 0x80000000 else x


def _trunc_div(n: int, d: int) -> int:
    """Division rounded toward zero."""
    q = abs(n) // abs(d)
    return -q if (n < 0) != (d < 0) else q


# The operations by mnemonic
ALU_FUNCTIONS: dict[str, Callable[[int, int], int]] = {
    "or": lambda a, b: a | b,
    "and": lambda a, b: a & b,
    "xor": lambda a, b: a ^ b,
    "add": lambda a, b: a + b,
    "sub": lambda a, b: a - b,
    "shiftl": lambda a, b: a << (b & 31),
    "shiftr": lambda a, b: a >> (b & 31),
    "not": lambda a, b: ~a,
    "slt": lambda a, b: int(_signed(a) < _signed(b)),
    "sltu": lambda a, b: int(a < b),
    "load": lambda a, b: b,
    "loadhi": lambda a, b: ((b & 0xFFFF) << 16) | (a & 0xFFFF),
    "shiftrs": lambda a, b: _signed(a) >> (b & 31),
}

MULDIV_FUNCTIONS: dict[str, Callable[[int, int], int]] = {
    "mults": lambda a, b: _signed(a) * _signed(b),
    "multu": lambda a, b: a * b,
    "multfp": lambda a, b: (_signed(a) * _signed(b)) >> 16,
    "divs": lambda a, b: _trunc_div(_signed(a), _signed(b)) if b else MASK,
    "divu": lambda a, b: a // b if b else MASK,
    "divfp": lambda a, b: _trunc_div(_signed(a) << 16, _signed(b)) if b else MASK,
    "mods": lambda a, b: (
        _signed(a) - _signed(b) * _trunc_div(_signed(a), _signed(b)) if b else a
    ),
    "modu": lambda a, b: a % b if b else a,
    "mulshi": lambda a, b: (_signed(a) * _signed(b)) >> 32,
    "multuhi": lambda a, b: (a * b) >> 32,
}

_ALU_BY_CODE = {ALU_OPS[name]: fn for name, fn in ALU_FUNCTIONS.items()}
_MULDIV_BY_CODE = {MULDIV_OPS[name]: fn for name, fn in MULDIV_FUNCTIONS.items()}
_BRANCH_CODES = set(BRANCH_OPS.values()) | {
    (0b000, 1),
    (0b100, 1),
}  # S ignored for eq/ne
# READ size code -> (bytes, sign-extend); WRITE size code -> bytes.
_READ_SIZES = {
    READ_OPS["read"]: (4, False),
    READ_OPS["readb"]: (1, True),
    READ_OPS["readbu"]: (1, False),
    READ_OPS["readh"]: (2, True),
    READ_OPS["readhu"]: (2, False),
}
_WRITE_SIZES = {WRITE_OPS["write"]: 4, WRITE_OPS["writeb"]: 1, WRITE_OPS["writeh"]: 2}


def _branch_taken(cond: int, s: int, a: int, b: int) -> bool:
    if s:
        a, b = _signed(a), _signed(b)
    return {
        0b000: a == b,
        0b001: a > b,
        0b010: a >= b,
        0b100: a != b,
        0b101: a < b,
        0b110: a <= b,
    }[cond]


@dataclass
class Machine:
    """The architectural state of a B32P4"""

    strict: bool = True
    io: Device = field(default_factory=NullIo)

    def __post_init__(self) -> None:
        self.regs = [0] * 16
        self.pc = ROM_BASE
        self.sp = 0
        self.stack = [0] * STACK_DEPTH
        self.pc_backup = 0
        self.interrupts_enabled = True
        self.int_id = 0
        self.pending = 0  # bit n-1 set: interrupt n is pending
        self.err_flags = 0
        self.err_first = 0
        self.err_pc = 0
        self.halted = False
        self.regions = [
            Region("sdram", SDRAM_BASE, SDRAM_SIZE, Ram()),
            Region("io", IO_BASE, IO_SIZE, self.io),
            Region("rom", ROM_BASE, ROM_SIZE, Ram(), writable=False),
            *(
                Region(n, base, size, Ram())
                for n, (base, size) in VIDEO_REGIONS.items()
            ),
        ]
        self._events: list[ErrorKind] = []
        self._current_pc = self.pc

    # ---- Loading and inspecting ----

    def load(self, address: int, image: bytes) -> None:
        """Put a binary image into memory (ROM included) before running."""
        for i in range(0, len(image), 4):
            word = int.from_bytes(image[i : i + 4].ljust(4, b"\0"), "little")
            found = self._find(address + i)
            if found is None:
                raise SimError(f"cannot load at unmapped address {address + i:08x}")
            region, offset = found
            region.device.write_word(offset, word, MASK)

    def read_word(self, address: int) -> int:
        """Read a word without side effects, for tests and debugging."""
        found = self._find(address)
        if found is None:
            raise SimError(f"unmapped address {address:08x}")
        region, offset = found
        return region.device.read_word(offset)

    @property
    def err_status(self) -> int:
        return self.err_flags | (self.err_first << 8)

    def request_interrupt(self, number: int) -> None:
        """A rising edge on interrupt input `number` (1 to 7)."""
        if not 1 <= number <= NUM_INTERRUPTS:
            raise ValueError(f"no interrupt {number}")
        self.pending |= 1 << (number - 1)

    # ---- Running ----

    def run(self, max_steps: int = 1_000_000) -> list[Retired]:
        """Step until a HALT retires. Returns the retire trace."""
        trace = []
        for _ in range(max_steps):
            trace.append(self.step())
            if self.halted:
                return trace
        raise SimError(f"no HALT within {max_steps} steps")

    def step(self) -> Retired:
        """Execute one instruction, or take an interrupt instead of it."""
        pc = self.pc
        word = self._fetch(pc)
        ins = decode(word)
        self._events = []
        self._current_pc = pc
        r = Retired(pc, word)
        if not ins.unused_ok:
            what = "not all ones" if ins.opcode == Opcode.HALT else "unused bits not 0"
            self._illegal(None, f"{ins.opcode.name}: {what}")

        target = self._redirect_target(ins, pc)
        if target is not None and self._interrupt_point(pc):
            return self._take_interrupt(r)

        self.pc = (pc + 4) & MASK
        self._execute(ins, r)
        if target is not None:
            self.pc = r.redirect = self._jump_target(target)

        if self._events:
            self.err_flags |= sum(1 << (k - 1) for k in set(self._events))
            if not self.err_first:
                self.err_first, self.err_pc = min(self._events), pc
        return r

    # ---- Control flow and interrupts ----

    def _redirect_target(self, ins: Decoded, pc: int) -> int | None:
        """The target of a jump, taken branch or halt; None for other instructions."""
        op = ins.opcode
        if op == Opcode.JUMP:
            c27 = ins["c27"]
            return pc + sext(c27, 27) if ins["o"] else c27
        if op == Opcode.JUMPR:
            target = self.regs[ins["b"]] + sext(ins["c16"], 16)
            return target + pc if ins["o"] else target
        if op == Opcode.BRANCH:
            cond, s = ins["cond"], ins["s"]
            if (cond, s) not in _BRANCH_CODES:
                return None  # reserved: handled as an illegal instruction in _execute
            a, b = self.regs[ins["a"]], self.regs[ins["b"]]
            return pc + sext(ins["c16"], 16) if _branch_taken(cond, s, a, b) else None
        if op == Opcode.HALT:
            return pc
        return None

    def _interrupt_point(self, pc: int) -> bool:
        return bool(self.pending) and self.interrupts_enabled and pc < ROM_BASE

    def _take_interrupt(self, r: Retired) -> Retired:
        number = (self.pending & -self.pending).bit_length()  # lowest number first
        self.pending &= ~(1 << (number - 1))
        self.pc_backup = r.pc
        self.interrupts_enabled = False
        self.int_id = number
        self.pc = INTERRUPT_VECTOR
        r.interrupt = number
        r.redirect = INTERRUPT_VECTOR
        return r

    def _jump_target(self, target: int) -> int:
        target &= MASK
        if target % 4 or not self._is_code(target):
            self._illegal(ErrorKind.BAD_JUMP_TARGET, f"jump target {target:08x}")
        return target & ~3

    @staticmethod
    def _is_code(address: int) -> bool:
        return (SDRAM_BASE <= address < SDRAM_BASE + SDRAM_SIZE) or (
            ROM_BASE <= address < ROM_BASE + ROM_SIZE
        )

    def _fetch(self, pc: int) -> int:
        if not self._is_code(pc):
            raise SimError(f"fetch from {pc:08x}, outside SDRAM and ROM")
        found = self._find(pc)
        assert found is not None
        region, offset = found
        return region.device.read_word(offset)

    # ---- Execution ----

    def _execute(self, ins: Decoded, r: Retired) -> None:
        op = ins.opcode
        if op in (Opcode.ARITH, Opcode.ARITHC, Opcode.ARITHM, Opcode.ARITHMC):
            self._arith(ins, r)
        elif op == Opcode.BRANCH:
            if (ins["cond"], ins["s"]) not in _BRANCH_CODES:
                self._illegal(
                    ErrorKind.ILLEGAL_INSTRUCTION, "reserved branch condition"
                )
        elif op == Opcode.SAVPC:
            self._write_reg(ins["d"], r.pc, r)
        elif op == Opcode.RETI:
            self.interrupts_enabled = True
            self.pc = r.redirect = self.pc_backup
        elif op == Opcode.INTID:
            self._write_reg(ins["d"], self.int_id, r)
        elif op == Opcode.PUSH:
            if self.sp == STACK_DEPTH - 1:
                self._illegal(ErrorKind.STACK_OVERFLOW, "push on a full stack")
            r.push = self.stack[self.sp] = self.regs[ins["b"]]
            self.sp = (self.sp + 1) % STACK_DEPTH
        elif op == Opcode.POP:
            if self.sp == 0:
                self._illegal(ErrorKind.STACK_UNDERFLOW, "pop on an empty stack")
            self.sp = (self.sp - 1) % STACK_DEPTH
            r.pop = self.stack[self.sp]
            self._write_reg(ins["d"], r.pop, r)
        elif op == Opcode.READ:
            self._read(ins, r)
        elif op == Opcode.WRITE:
            self._write(ins, r)
        elif op == Opcode.CCACHE:
            pass  # the ISS has no caches; execution continues at PC + 4
        elif op in (Opcode.JUMP, Opcode.JUMPR, Opcode.HALT):
            self.halted = op == Opcode.HALT

    def _arith(self, ins: Decoded, r: Retired) -> None:
        op = ins["op"]
        a = self.regs[ins["a"]]
        if ins.opcode in (Opcode.ARITH, Opcode.ARITHM):
            b = self.regs[ins["b"]]
        elif ins.opcode == Opcode.ARITHC and op in ALU_ZEXT_OPS:
            b = ins["c16"]
        else:
            b = sext(ins["c16"], 16) & MASK
        table = (
            _ALU_BY_CODE
            if ins.opcode in (Opcode.ARITH, Opcode.ARITHC)
            else _MULDIV_BY_CODE
        )
        if op not in table:
            self._illegal(ErrorKind.ILLEGAL_INSTRUCTION, f"reserved operation {op:04b}")
            return
        self._write_reg(ins["d"], table[op](a, b) & MASK, r)

    def _write_reg(self, d: int, value: int, r: Retired) -> None:
        if d != 0:
            self.regs[d] = value
            r.rd = (d, value)

    # ---- Memory ----

    def _read(self, ins: Decoded, r: Retired) -> None:
        if ins["size"] not in _READ_SIZES:
            self._illegal(
                ErrorKind.ILLEGAL_INSTRUCTION, f"reserved read size {ins['size']:04b}"
            )
            return
        size, signed = _READ_SIZES[ins["size"]]
        address = self._address(ins, size)
        word = self._load_word(address, size)
        shift = (address & 3) * 8
        value = (word >> shift) & ((1 << (8 * size)) - 1)
        if signed:
            value = sext(value, 8 * size) & MASK
        self._write_reg(ins["d"], value, r)

    def _write(self, ins: Decoded, r: Retired) -> None:
        if ins["size"] not in _WRITE_SIZES:
            self._illegal(
                ErrorKind.ILLEGAL_INSTRUCTION, f"reserved write size {ins['size']:04b}"
            )
            return
        size = _WRITE_SIZES[ins["size"]]
        address = self._address(ins, size)
        shift = (address & 3) * 8
        enables = ((1 << size) - 1) << (address & 3)
        mask = sum(0xFF << (8 * i) for i in range(4) if enables >> i & 1)
        data = (self.regs[ins["b"]] << shift) & mask
        r.store = (address & ~3, data, enables)
        self._store_word(address & ~3, data, mask)

    def _address(self, ins: Decoded, size: int) -> int:
        address = (self.regs[ins["a"]] + sext(ins["c16"], 16)) & MASK
        if address % size:
            self._illegal(
                ErrorKind.UNALIGNED_ACCESS, f"{size}-byte access at {address:08x}"
            )
            address &= ~(size - 1)
        return address

    def _find(self, address: int) -> tuple[Region, int] | None:
        for region in self.regions:
            if region.base <= address < region.base + region.size:
                return region, (address - region.base) & ~3
        return None

    def _load_word(self, address: int, size: int) -> int:
        if CPU_IO_BASE <= address < CPU_IO_BASE + CPU_IO_SIZE:
            return self._cpu_io_read(address, size)
        found = self._find(address)
        if found is None:
            self._illegal(ErrorKind.BAD_ADDRESS, f"read from unmapped {address:08x}")
            return 0
        region, offset = found
        return region.device.read_word(offset)

    def _store_word(self, address: int, data: int, mask: int) -> None:
        if CPU_IO_BASE <= address < CPU_IO_BASE + CPU_IO_SIZE:
            self._cpu_io_write(address, data, mask)
            return
        found = self._find(address)
        if found is None:
            self._illegal(ErrorKind.BAD_ADDRESS, f"write to unmapped {address:08x}")
            return
        region, offset = found
        if not region.writable:
            self._illegal(
                ErrorKind.BAD_ADDRESS, f"write to {region.name} at {address:08x}"
            )
            return
        region.device.write_word(offset, data, mask)

    def _cpu_io_read(self, address: int, size: int) -> int:
        values = {
            PC_BACKUP_ADDR: self.pc_backup,
            SP_ADDR: self.sp,
            ERR_STATUS_ADDR: self.err_status,
            ERR_PC_ADDR: self.err_pc,
        }
        if size != 4 or address not in values:
            self._illegal(ErrorKind.BAD_ADDRESS, f"read from CPU I/O {address:08x}")
            return 0
        return values[address]

    def _cpu_io_write(self, address: int, data: int, mask: int) -> None:
        if mask != MASK or address not in (PC_BACKUP_ADDR, SP_ADDR, ERR_STATUS_ADDR):
            self._illegal(ErrorKind.BAD_ADDRESS, f"write to CPU I/O {address:08x}")
            return
        if address == PC_BACKUP_ADDR:
            self.pc_backup = data
        elif address == SP_ADDR:
            self.sp = data & (STACK_DEPTH - 1)
        else:
            self.err_flags = self.err_first = self.err_pc = 0

    # ---- Illegal cases ----

    def _illegal(self, kind: ErrorKind | None, message: str) -> None:
        """Strict mode: stop. Hardware mode: remember the event and carry on."""
        if self.strict:
            raise IllegalError(kind, self._current_pc, message)
        if kind is not None:
            self._events.append(kind)
