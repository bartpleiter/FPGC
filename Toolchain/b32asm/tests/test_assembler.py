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
    assert word("savpc r7") == 0x50000007
    assert word("savpc 8 r15") == 0x5000800F
    assert word("savpc -4 r1") == 0x5FFFC001


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
    program = assemble("Start:\n nop\n beq r0 r0 Start\n jump Start\n", 0x04000000)
    assert program.labels == {"Start": 0x04000000}
    beq, jump = decode(program.words[1]), decode(program.words[2])
    assert sext(beq["c16"], 16) == -4
    assert (
        jump["o"] == 1 and sext(jump["c27"], 27) == -8
    )  # jump to a label is PC-relative


def test_savpc_to_a_label_is_relative() -> None:
    program = assemble("savpc Next r1\njump 0\nNext:\n", 0x04000000)
    assert sext(decode(program.words[0])["c16"], 16) == 8


def test_jump_to_number_is_absolute() -> None:
    jump = decode(word("jump 0x100"))
    assert jump["o"] == 0 and jump["c27"] == 0x100


def test_load32_and_addr2reg() -> None:
    assert len(assemble("load32 0x1234 r1").words) == 1
    assert assemble("load32 -1 r1").words == [0x1CFFFF11, 0x1DFFFF11]
    program = assemble("addr2reg Data r2\nData:\n.dw 7 -1", 0x04000000)
    assert program.words == [0x1C000822, 0x1D040022, 7, 0xFFFFFFFF]


@pytest.mark.parametrize(
    ("source", "word"),
    [
        # A constant may be written as the 32-bit value it's sign-extended to.
        ("write 0xFFFF8000 r0 r1", assemble("write -32768 r0 r1").words[0]),
        ("read 0xFFFFFF08 r0 r2", assemble("read -248 r0 r2").words[0]),
        ("add r1 0xFFFFFFFF r2", assemble("add r1 -1 r2").words[0]),
        ("jumpr 0xFFFFFFFC r3", assemble("jumpr -4 r3").words[0]),
    ],
)
def test_sign_extended_constants(source: str, word: int) -> None:
    assert assemble(source).words == [word]


@pytest.mark.parametrize(
    ("source", "message"),
    [
        ("frobnicate r1", "unknown instruction"),
        ("add r1 40000 r2", "does not fit"),
        ("write 0xFFFF7FFC r0 r1", "does not fit"),
        ("read 0x8000 r0 r1", "does not fit"),
        ("load -1 r1", "does not fit"),
        ("add r1 r2", "needs 3 operand"),
        ("push 5", "wrong kind"),
        ("add r16 r1 r2", "no register"),
        ("add r1 4x r2", "invalid operand"),
        ("jump Nowhere", "unknown label"),
        ("A:\nA:", "defined twice"),
        (".equ A 1\nA:", "defined twice"),
        ("A:\n.equ A 1", "defined twice"),
        (".equ A 1\n.equ A 2", "defined twice"),
        (".equ A", "a name and a value"),
        (".equ r1 5", "can't be the name"),
        (".equ A B", "must be a number"),
        (".ds hello", "double quotes"),
        ('.ds "a" "b"', "inside quotes"),
        (r'.ds "a\q"', "unknown escape"),
        ("load 0o17 r1", "invalid operand"),
        ("load 0X10 r1", "invalid operand"),
        ("load 1__0 r1", "invalid operand"),
        ("load 0x_10 r1", "invalid operand"),
        ("load 10_ r1", "invalid operand"),
        ("load 017x r1", "invalid operand"),
        ("load 'ab' r1", "one character"),
        ("load '' r1", "one character"),
        ("load 'a r1", "invalid character"),
        ("load ''' r1", "invalid character"),
        (".dw Nowhere", "unknown label"),
        ('.ds "é"', "not an ASCII"),
        ("jumpo 2", "not a multiple of 4"),
        ("beq r0 r0 6", "not a multiple of 4"),
    ],
)
def test_errors(source: str, message: str) -> None:
    with pytest.raises(AsmError, match=message):
        assemble(source)


def test_strings() -> None:
    """4 characters per word, the first in the lowest byte, ended by a 0 and padded
    to a whole word. A ; in a string isn't a comment."""
    assert assemble('.ds "abc"').words == [0x00636261]
    assert assemble('.ds "abcd"').words == [0x64636261, 0]
    assert assemble('.ds ""').words == [0]
    assert assemble('.ds "a;b" ; comment').words == [0x00623B61]
    assert assemble(r'.ds "\r\n\t\\\"\0"').words == [0x5C090A0D, 0x00000022]
    program = assemble('.ds "hello"\nNext:\n', 0x100)
    assert program.labels == {"Next": 0x108}


def test_numbers() -> None:
    same = assemble(".dw 10 0x1F 5 -3 -16 0xFFFF8000 1000").words
    assert assemble(".dw 1_0 0x1f 0b101 -3 -0x10 0xFFFF_8000 1_000").words == same
    assert assemble(".dw 0b1010_0000 007").words == [0xA0, 7]


def test_characters() -> None:
    """A character is its number, with the escapes of strings. ; and spaces
    inside a character are part of it."""
    assert assemble("load 'L' r2").words == assemble("load 0x4C r2").words
    source = r".dw 'a' ' ' ';' '\n' '\'' '\\' '\"' '\0' ; ' ' is a space"
    assert assemble(source).words == [0x61, 0x20, 0x3B, 0x0A, 0x27, 0x5C, 0x22, 0]
    assert assemble('.ds "it\'s"').words == [0x73277469, 0]


def test_labels_in_data() -> None:
    program = assemble("Table:\n.dw First Second\nFirst:\nnop\nSecond:\n", 0x100)
    assert program.words == [0x108, 0x10C, 0]


def test_constants() -> None:
    source = """
        .equ UART 0xFFFF8000
        .equ TRANSMIT UART
        .equ BIG 0x12345678
        write TRANSMIT r0 r1
        load32 BIG r2
        .dw BIG
    """
    same = "write 0xFFFF8000 r0 r1\nload32 0x12345678 r2\n.dw 0x12345678"
    assert assemble(source).words == assemble(same).words


def test_error_location() -> None:
    with pytest.raises(AsmError, match=r"^test.asm:2: "):
        assemble("nop\nbad", filename="test.asm")
