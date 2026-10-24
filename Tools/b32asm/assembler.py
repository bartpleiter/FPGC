"""Assemble B32P4 assembly source into instruction words.

Two passes:

1. Parse each line, expand pseudo-instructions into real instructions and give
   every instruction and data word its address. This collects the labels.
2. Encode each statement into one 32-bit word, now that all labels are known.
"""

import re
from dataclasses import dataclass, field
from typing import Any

from b32asm.isa import (
    ALU_OPS,
    BRANCH_OPS,
    MULDIV_OPS,
    READ_OPS,
    WRITE_OPS,
    Opcode,
    encode,
)

_LABEL_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*):$")
_NAME_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
_REG_RE = re.compile(r"^r([0-9]|1[0-5])$")


class AsmError(Exception):
    """An error in the assembly source, with its location."""


@dataclass(frozen=True)
class Reg:
    number: int


@dataclass(frozen=True)
class LabelRef:
    """A label used as an operand. `part` selects the whole address or a 16-bit half."""

    name: str
    part: str = "addr"  # "addr", "low" or "high"


Operand = Reg | int | LabelRef


@dataclass
class Statement:
    """One instruction or data word, after pseudo-instructions are expanded."""

    mnemonic: str
    operands: list[Operand]
    location: str  # "file:line", for error messages
    address: int = 0


@dataclass
class Program:
    """The result of assembling, with the words starting at `base` and the label addresses."""

    base: int
    words: list[int]
    labels: dict[str, int] = field(default_factory=dict)

    def to_bytes(self) -> bytes:
        """The program as a little-endian binary image."""
        return b"".join(w.to_bytes(4, "little") for w in self.words)


def assemble(source: str, base: int = 0, filename: str = "<input>") -> Program:
    """Assemble `source` for a program that starts at address `base`."""
    statements: list[Statement] = []
    labels: dict[str, int] = {}

    # Pass 1 parses, expands and places the instructions.
    for number, text in enumerate(source.splitlines(), start=1):
        location = f"{filename}:{number}"
        code = text.split(";", 1)[0].strip()
        if not code:
            continue
        address = base + 4 * len(statements)
        if match := _LABEL_RE.match(code):
            name = match.group(1)
            if name in labels:
                raise AsmError(f"{location}: label '{name}' is defined twice")
            labels[name] = address
            continue
        mnemonic, *tokens = code.split()
        try:
            operands = [_parse_operand(t) for t in tokens]
            statements += _expand(mnemonic, operands, location)
        except (ValueError, TypeError) as e:
            raise AsmError(f"{location}: {e}") from None

    for i, statement in enumerate(statements):
        statement.address = base + 4 * i

    # Pass 2 encodes them.
    words = []
    for statement in statements:
        try:
            words.append(_encode(statement, labels))
        except (ValueError, TypeError) as e:
            raise AsmError(f"{statement.location}: {e}") from None
    return Program(base, words, labels)


def _parse_operand(token: str) -> Operand:
    if match := _REG_RE.match(token):
        return Reg(int(match.group(1)))
    if re.match(r"^r[0-9]+$", token):
        raise ValueError(f"there is no register '{token}'")
    if _NAME_RE.match(token):
        return LabelRef(token)
    try:
        return int(token, 0)
    except ValueError:
        raise ValueError(f"invalid operand '{token}'") from None


def _expand(mnemonic: str, operands: list[Operand], location: str) -> list[Statement]:
    """Turn one source line into statements of exactly one word each."""
    if mnemonic == ".dw":
        if not operands:
            raise ValueError(".dw needs at least one value")
        return [Statement(".dw", [value], location) for value in operands]

    if mnemonic == "load32":
        value, rd = _check(mnemonic, operands, int, Reg)
        if not -(1 << 31) <= value < (1 << 32):
            raise ValueError(f"{value} does not fit in 32 bits")
        low, high = value & 0xFFFF, (value >> 16) & 0xFFFF
        # load clears the upper half, so loadhi is only needed when it isn't 0.
        result = [Statement("load", [low, rd], location)]
        if high:
            result.append(Statement("loadhi", [high, rd], location))
        return result

    if mnemonic == "addr2reg":
        label, rd = _check(mnemonic, operands, LabelRef, Reg)
        return [
            Statement("load", [LabelRef(label.name, "low"), rd], location),
            Statement("loadhi", [LabelRef(label.name, "high"), rd], location),
        ]

    return [Statement(mnemonic, operands, location)]


def _check(
    mnemonic: str, operands: list[Operand], *kinds: type | tuple[type, ...]
) -> list[Any]:
    """Check the number and kinds of operands, and return them."""
    if len(operands) != len(kinds):
        raise ValueError(
            f"{mnemonic} needs {len(kinds)} operand(s), got {len(operands)}"
        )
    for i, (operand, kind) in enumerate(zip(operands, kinds, strict=True), start=1):
        if not isinstance(operand, kind):
            raise TypeError(f"operand {i} of {mnemonic} has the wrong kind")
    return operands


def _fit_signed(value: int, bits: int, what: str) -> int:
    """Check that `value` fits as a signed field and return its raw bits."""
    if not -(1 << (bits - 1)) <= value < (1 << (bits - 1)):
        raise ValueError(f"{what} {value} does not fit in {bits} bits signed")
    return value & ((1 << bits) - 1)


def _fit_unsigned(value: int, bits: int, what: str) -> int:
    if not 0 <= value < (1 << bits):
        raise ValueError(f"{what} {value} does not fit in {bits} bits unsigned")
    return value


def _check_aligned(offset: int, what: str) -> None:
    if offset % 4:
        raise ValueError(f"{what} {offset} is not a multiple of 4")


def _resolve(ref: LabelRef, labels: dict[str, int]) -> int:
    if ref.name not in labels:
        raise ValueError(f"unknown label '{ref.name}'")
    address = labels[ref.name]
    if ref.part == "low":
        return address & 0xFFFF
    if ref.part == "high":
        return (address >> 16) & 0xFFFF
    return address


def _encode(st: Statement, labels: dict[str, int]) -> int:
    """Encode one statement into its 32-bit word."""
    m, ops = st.mnemonic, st.operands

    if m == ".dw":
        (value,) = _check(m, ops, int)
        if not -(1 << 31) <= value < (1 << 32):
            raise ValueError(f"{value} does not fit in 32 bits")
        return value & 0xFFFFFFFF

    if m in ("load", "loadhi"):
        value, rd = _check(m, ops, (int, LabelRef), Reg)
        if isinstance(value, LabelRef):
            value = _resolve(value, labels)
        c16 = _fit_unsigned(value, 16, "constant")
        return encode(Opcode.ARITHC, op=ALU_OPS[m], c16=c16, a=rd.number, d=rd.number)

    if m == "not":
        ra, rd = _check(m, ops, Reg, Reg)
        return encode(Opcode.ARITH, op=ALU_OPS[m], a=ra.number, d=rd.number)

    if m in ALU_OPS or m in MULDIV_OPS:
        ra, b, rd = _check(m, ops, Reg, (Reg, int), Reg)
        op = ALU_OPS[m] if m in ALU_OPS else MULDIV_OPS[m]
        if isinstance(b, Reg):
            opcode = Opcode.ARITH if m in ALU_OPS else Opcode.ARITHM
            return encode(opcode, op=op, a=ra.number, b=b.number, d=rd.number)
        opcode = Opcode.ARITHC if m in ALU_OPS else Opcode.ARITHMC
        c16 = _fit_signed(b, 16, "constant")
        return encode(opcode, op=op, c16=c16, a=ra.number, d=rd.number)

    if m in READ_OPS:
        offset, ra, rd = _check(m, ops, int, Reg, Reg)
        c16 = _fit_signed(offset, 16, "offset")
        return encode(Opcode.READ, c16=c16, a=ra.number, size=READ_OPS[m], d=rd.number)

    if m in WRITE_OPS:
        offset, ra, rb = _check(m, ops, int, Reg, Reg)
        c16 = _fit_signed(offset, 16, "offset")
        return encode(
            Opcode.WRITE, c16=c16, a=ra.number, b=rb.number, size=WRITE_OPS[m]
        )

    if m in BRANCH_OPS:
        ra, rb, target = _check(m, ops, Reg, Reg, (int, LabelRef))
        offset = (
            _resolve(target, labels) - st.address
            if isinstance(target, LabelRef)
            else target
        )
        _check_aligned(offset, "branch offset")
        cond, s = BRANCH_OPS[m]
        c16 = _fit_signed(offset, 16, "branch offset")
        return encode(Opcode.BRANCH, c16=c16, a=ra.number, b=rb.number, cond=cond, s=s)

    if m == "jump":
        (target,) = _check(m, ops, (int, LabelRef))
        if isinstance(target, LabelRef):
            # A jump to a label is PC-relative, so it also reaches code in ROM.
            offset = _resolve(target, labels) - st.address
            return encode(Opcode.JUMP, c27=_fit_signed(offset, 27, "jump offset"), o=1)
        _check_aligned(target, "jump address")
        return encode(Opcode.JUMP, c27=_fit_unsigned(target, 27, "jump address"))

    if m == "jumpo":
        (offset,) = _check(m, ops, int)
        _check_aligned(offset, "jump offset")
        return encode(Opcode.JUMP, c27=_fit_signed(offset, 27, "jump offset"), o=1)

    if m in ("jumpr", "jumpro"):
        offset, rb = _check(m, ops, int, Reg)
        c16 = _fit_signed(offset, 16, "offset")
        return encode(Opcode.JUMPR, c16=c16, b=rb.number, o=int(m == "jumpro"))

    if m == "push":
        (rb,) = _check(m, ops, Reg)
        return encode(Opcode.PUSH, b=rb.number)

    single_register = {
        "pop": Opcode.POP,
        "savpc": Opcode.SAVPC,
        "readintid": Opcode.INTID,
    }
    if m in single_register:
        (rd,) = _check(m, ops, Reg)
        return encode(single_register[m], d=rd.number)

    no_operands = {
        "halt": (Opcode.HALT, {}),
        "reti": (Opcode.RETI, {}),
        "ccache": (Opcode.CCACHE, {}),
        "ccached": (Opcode.CCACHE, {"data_only": 1}),
        "nop": (Opcode.ARITH, {"op": ALU_OPS["or"]}),  # or r0 r0 r0, the all-zero word
    }
    if m in no_operands:
        _check(m, ops)
        opcode, values = no_operands[m]
        return encode(opcode, **values)

    raise ValueError(f"unknown instruction '{m}'")
