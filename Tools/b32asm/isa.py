"""B32P4 instruction set."""

from dataclasses import dataclass
from enum import IntEnum


class Opcode(IntEnum):
    """Instruction type."""

    ARITH = 0b0000
    ARITHC = 0b0001
    ARITHM = 0b0010
    ARITHMC = 0b0011
    RETI = 0b0100
    SAVPC = 0b0101
    BRANCH = 0b0110
    CCACHE = 0b0111
    JUMPR = 0b1000
    JUMP = 0b1001
    POP = 0b1010
    PUSH = 0b1011
    INTID = 0b1100
    WRITE = 0b1101
    READ = 0b1110
    HALT = 0b1111


# ALU operations (ARITH, ARITHC), field "op". Codes not listed are reserved.
ALU_OPS = {
    "or": 0b0000,
    "and": 0b0001,
    "xor": 0b0010,
    "add": 0b0011,
    "sub": 0b0100,
    "shiftl": 0b0101,
    "shiftr": 0b0110,
    "not": 0b0111,
    "slt": 0b1010,
    "sltu": 0b1011,
    "load": 0b1100,
    "loadhi": 0b1101,
    "shiftrs": 0b1110,
}

# ALU operations whose ARITHC constant is zero-extended instead of sign-extended.
ALU_ZEXT_OPS = {ALU_OPS["load"], ALU_OPS["loadhi"]}

# Multiply and divide operations (ARITHM, ARITHMC), field "op".
MULDIV_OPS = {
    "mults": 0b0000,
    "multu": 0b0001,
    "multfp": 0b0010,
    "divs": 0b0011,
    "divu": 0b0100,
    "divfp": 0b0101,
    "mods": 0b0110,
    "modu": 0b0111,
    "mulshi": 0b1110,
    "multuhi": 0b1111,
}

# Branch conditions, as mnemonic -> (field "cond", field "s").
BRANCH_OPS = {
    "beq": (0b000, 0),
    "bgt": (0b001, 0),
    "bge": (0b010, 0),
    "bne": (0b100, 0),
    "blt": (0b101, 0),
    "ble": (0b110, 0),
    "bgts": (0b001, 1),
    "bges": (0b010, 1),
    "blts": (0b101, 1),
    "bles": (0b110, 1),
}

# READ sizes, field "size": mnemonic -> code.
READ_OPS = {
    "read": 0b0000,
    "readb": 0b0001,
    "readbu": 0b0101,
    "readh": 0b0010,
    "readhu": 0b0110,
}

# WRITE sizes, field "size": mnemonic -> code.
WRITE_OPS = {
    "write": 0b0000,
    "writeb": 0b0001,
    "writeh": 0b0010,
}


@dataclass(frozen=True)
class Field:
    """A named bit range [hi:lo] of an instruction word."""

    name: str
    hi: int
    lo: int

    @property
    def width(self) -> int:
        return self.hi - self.lo + 1


@dataclass(frozen=True)
class Format:
    """The fields of one instruction type.

    Bits [27:0] that no field covers are unused ("x"). They must equal the same
    bits of `fixed`, which is 0 for every instruction except HALT.
    """

    fields: tuple[Field, ...]
    fixed: int = 0

    @property
    def unused_mask(self) -> int:
        mask = (1 << 28) - 1
        for f in self.fields:
            mask &= ~(((1 << f.width) - 1) << f.lo)
        return mask


_A, _B, _D = Field("a", 11, 8), Field("b", 7, 4), Field("d", 3, 0)
_OP = Field("op", 27, 24)
_C16_HIGH = Field("c16", 27, 12)  # BRANCH, JUMPR, READ, WRITE
_C16_LOW = Field("c16", 23, 8)  # ARITHC, ARITHMC
_A_LOW = Field("a", 7, 4)  # ARITHC, ARITHMC

_REG_FORMAT = Format((_OP, _A, _B, _D))
_CONST_FORMAT = Format((_OP, _C16_LOW, _A_LOW, _D))

FORMATS = {
    Opcode.ARITH: _REG_FORMAT,
    Opcode.ARITHC: _CONST_FORMAT,
    Opcode.ARITHM: _REG_FORMAT,
    Opcode.ARITHMC: _CONST_FORMAT,
    Opcode.RETI: Format(()),
    Opcode.SAVPC: Format((_D,)),
    Opcode.BRANCH: Format((_C16_HIGH, _A, _B, Field("cond", 3, 1), Field("s", 0, 0))),
    Opcode.CCACHE: Format((Field("data_only", 0, 0),)),
    Opcode.JUMPR: Format((_C16_HIGH, _B, Field("o", 0, 0))),
    Opcode.JUMP: Format((Field("c27", 27, 1), Field("o", 0, 0))),
    Opcode.POP: Format((_D,)),
    Opcode.PUSH: Format((_B,)),
    Opcode.INTID: Format((_D,)),
    Opcode.WRITE: Format((_C16_HIGH, _A, _B, Field("size", 3, 0))),
    Opcode.READ: Format((_C16_HIGH, _A, Field("size", 7, 4), _D)),
    Opcode.HALT: Format((), fixed=(1 << 28) - 1),
}


def encode(opcode: Opcode, **values: int) -> int:
    """Build an instruction word. Missing fields are 0; values must fit their field."""
    fmt = FORMATS[opcode]
    word = (opcode << 28) | fmt.fixed
    for f in fmt.fields:
        value = values.pop(f.name, 0)
        if not 0 <= value < (1 << f.width):
            raise ValueError(f"{f.name}={value} does not fit in {f.width} bits")
        word |= value << f.lo
    if values:
        raise ValueError(f"{opcode.name} has no field(s) {', '.join(values)}")
    return word


@dataclass(frozen=True)
class Decoded:
    """An instruction word split into its fields."""

    opcode: Opcode
    fields: dict[str, int]
    unused_ok: bool  # False if an unused bit differs from the format's fixed value

    def __getitem__(self, name: str) -> int:
        return self.fields[name]


def decode(word: int) -> Decoded:
    """Split an instruction word into its fields."""
    opcode = Opcode(word >> 28)
    fmt = FORMATS[opcode]
    fields = {f.name: (word >> f.lo) & ((1 << f.width) - 1) for f in fmt.fields}
    unused_ok = (word & fmt.unused_mask) == (fmt.fixed & fmt.unused_mask)
    return Decoded(opcode, fields, unused_ok)


def sext(value: int, bits: int) -> int:
    """Sign-extend a `bits`-wide value to a Python int."""
    sign = 1 << (bits - 1)
    return (value & (sign - 1)) - (value & sign)
