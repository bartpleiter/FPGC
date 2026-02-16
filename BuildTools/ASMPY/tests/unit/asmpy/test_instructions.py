from asmpy.models.assembly_line import InstructionAssemblyLine
from asmpy.models.data_types import SourceLine
import pytest


def make_line(code: str) -> InstructionAssemblyLine:
    """Helper to create an InstructionAssemblyLine from assembly code."""
    src = SourceLine(line=code, source_line_number=1, source_file_name="test.asm")
    line = InstructionAssemblyLine.parse_line(src)  # type: ignore
    assert isinstance(line, InstructionAssemblyLine)
    return line  # type: ignore


def test_load_and_loadhi_label_resolution():
    """Test that LOAD and LOADHI instructions use correct ARITHC prefix."""
    # Arrange & Act
    low = make_line("load 0x1234 r1")
    hi = make_line("loadhi 0xABCD r1")

    # Assert
    # LOAD/LOADHI use ARITHC (0001) prefix
    assert low.to_binary_string().startswith("0001")
    assert hi.to_binary_string().startswith("0001")


def test_add_register_and_constant():
    """Test that ADD uses different prefixes for register vs constant operands."""
    # Arrange & Act
    add_reg = make_line("add r1 r2 r3")
    add_const = make_line("add r1 5 r3")
    b1 = add_reg.to_binary_string()
    b2 = add_const.to_binary_string()

    # Assert
    assert b1[:4] == "0000"  # ARITH opcode for register operands
    assert b2[:4] == "0001"  # ARITHC opcode for constant operands


def test_branch_equal():
    """Test that BEQ instruction has correct prefix and 32-bit length."""
    # Arrange & Act
    beq = make_line("beq r1 r2 4")
    b = beq.to_binary_string()

    # Assert
    assert b.startswith("0110")  # Branch prefix
    # Ensure lower bits contain branch opcode + signed bit (3+1 bits)
    assert len(b.split()[0]) == 32


def test_jump():
    """Test that JUMP instruction has correct prefix and offset flag."""
    # Arrange & Act
    jmp = make_line("jump 10")
    b = jmp.to_binary_string()

    # Assert
    assert b.startswith("1001")
    assert b.endswith("0")  # Not an offset jump


def test_jump_offset():
    """Test that JUMPO instruction has correct prefix and offset flag."""
    # Arrange & Act
    jmpo = make_line("jumpo 5")
    b = jmpo.to_binary_string()

    # Assert
    assert b.startswith("1001")
    assert b.endswith("1")  # Is an offset jump


def test_jump_offset_negative():
    """Test that JUMPO accepts signed negative offsets."""
    jmpo = make_line("jumpo -1")
    b = jmpo.to_binary_string()

    assert b.startswith("1001")
    assert b.endswith("1")
    assert b[4:31] == "1" * 27


def test_jump_offset_signed_range_error():
    """Test that JUMPO rejects out-of-range signed offsets."""
    jmpo = make_line("jumpo 67108864")
    with pytest.raises(ValueError):
        jmpo.to_binary_string()


def test_push_pop():
    """Test that PUSH and POP instructions have correct prefixes."""
    # Arrange & Act
    push = make_line("push r3")
    pop = make_line("pop r7")

    # Assert
    assert push.to_binary_string().startswith("1011")
    assert pop.to_binary_string().startswith("1010")


def test_add_with_negative_constant():
    """Test that instruction (ADD) with negative constant encodes correctly (two's complement)."""
    # Arrange & Act
    add_neg = make_line("add r1 -5 r3")
    b = add_neg.to_binary_string()

    # Assert
    assert b[:4] == "0001"  # ARITHC opcode for constant operands
    # Extract the 16-bit constant (bits 8-23 after opcode+arith_opcode)
    const_bits = b[8:24]
    const_value = int(const_bits, 2)
    # Convert from unsigned to signed 16-bit
    if const_value >= 0x8000:
        const_value -= 0x10000
    assert const_value == -5
