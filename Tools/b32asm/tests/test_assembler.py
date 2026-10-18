"""Tests for the b32asm assembler."""

import pytest

from b32asm.assembler import AsmError, assemble
from b32asm.isa import FORMATS, Opcode, decode, encode, sext


def word(line: str, base: int = 0) -> int:
    (w,) = assemble(line, base).words
    return w


def test_known_encodings() -> None:
    assert word("nop") == 0x00000000
    assert word("halt") == 0xFFFFFFFF
    assert word("ccache") == 0x70000000
    assert word("ccached") == 0x70000001
    assert word("reti") == 0x40000000
    assert word("add r1 r2 r3") == 0x03000123
    assert word("add r1 -1 r3") == 0x13FFFF13
    assert word("load 0xFFFF r4") == 0x1CFFFF44
    assert word("read 8 r2 r3") == 0xE0008203
    assert word("readbu -1 r2 r3") == 0xEFFFF253
    assert word("write 4 r2 r3") == 0xD0004230
    assert word("writeh 0 r2 r3") == 0xD0000232
    assert word("push r5") == 0xB0000050
    assert word("pop r5") == 0xA0000005
    assert word("bges r1 r2 -8") == 0x6FFF8125
    assert word("jumpro 4 r7") == 0x80004071
    assert word("mulshi r1 r2 r3") == 0x2E000123
    assert word("divs r1 10 r3") == 0x33000A13


@pytest.mark.parametrize("opcode", list(Opcode))
def test_encode_decode_roundtrip(opcode: Opcode) -> None:
    fmt = FORMATS[opcode]
    values = {f.name: (1 << f.width) - 1 for f in fmt.fields}
    decoded = decode(encode(opcode, **values))
    assert decoded.opcode == opcode
    assert decoded.fields == values
    assert decoded.unused_ok


def test_unused_bits_are_detected() -> None:
    assert not decode(0x40000001).unused_ok  # RETI with bit 0 set
    assert not decode(0xFFFFFFFE).unused_ok  # HALT must be all ones


def test_sext() -> None:
    assert sext(0xFFFF, 16) == -1
    assert sext(0x7FFF, 16) == 0x7FFF
    assert sext(1 << 26, 27) == -(1 << 26)


def test_labels_and_relative_targets() -> None:
    program = assemble("Start:\n nop\n beq r0 r0 Start\n jump Start\n", 0x1E000000)
    assert program.labels == {"Start": 0x1E000000}
    beq, jump = decode(program.words[1]), decode(program.words[2])
    assert sext(beq["c16"], 16) == -4
    assert (
        jump["o"] == 1 and sext(jump["c27"], 27) == -8
    )  # jump to a label is PC-relative


def test_jump_to_number_is_absolute() -> None:
    jump = decode(word("jump 0x100"))
    assert jump["o"] == 0 and jump["c27"] == 0x100


def test_load32_and_addr2reg() -> None:
    assert len(assemble("load32 0x1234 r1").words) == 1
    assert assemble("load32 -1 r1").words == [0x1CFFFF11, 0x1DFFFF11]
    program = assemble("addr2reg Data r2\nData:\n.dw 7 -1", 0x1E000000)
    assert program.words == [0x1C000822, 0x1D1E0022, 7, 0xFFFFFFFF]


@pytest.mark.parametrize(
    ("source", "message"),
    [
        ("frobnicate r1", "unknown instruction"),
        ("add r1 40000 r2", "does not fit"),
        ("load -1 r1", "does not fit"),
        ("add r1 r2", "needs 3 operand"),
        ("push 5", "wrong kind"),
        ("add r16 r1 r2", "no register"),
        ("add r1 4x r2", "invalid operand"),
        ("jump Nowhere", "unknown label"),
        ("A:\nA:", "defined twice"),
        ("jumpo 2", "not a multiple of 4"),
        ("beq r0 r0 6", "not a multiple of 4"),
    ],
)
def test_errors(source: str, message: str) -> None:
    with pytest.raises(AsmError, match=message):
        assemble(source)


def test_error_location() -> None:
    with pytest.raises(AsmError, match=r"^test.asm:2: "):
        assemble("nop\nbad", filename="test.asm")
