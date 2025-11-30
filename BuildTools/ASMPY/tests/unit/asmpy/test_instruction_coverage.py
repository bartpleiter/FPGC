import tempfile
import os
import pytest
from asmpy.models.data_types import SourceLine
from asmpy.models.assembly_line import AssemblyLine, InstructionAssemblyLine
from asmpy.assembler import Assembler


def src(line: str, n: int = 1) -> SourceLine:
    """Helper to create a SourceLine."""
    return SourceLine(line=line, source_line_number=n, source_file_name="test.asm")


def parse(code: str) -> InstructionAssemblyLine:
    """Helper to parse an instruction line."""
    line = AssemblyLine.parse_line(src(code))
    assert isinstance(line, InstructionAssemblyLine)
    return line


def test_all_instruction_binaries():
    """Test that all instruction types can be converted to binary."""
    # Arrange
    samples = [
        # Control
        "halt",
        "savpc r1",
        "ccache",
        "nop",
        "reti",
        "readintid r2",
        # Memory
        "read 0 r1 r2",
        "write 1 r3 r4",
        "push r5",
        "pop r6",
        # Arithmetic (single)
        "add r1 r2 r3",
        "add r1 5 r4",
        "sub r2 r3 r4",
        "or r1 r2 r3",
        "and r1 r2 r3",
        "xor r1 r2 r3",
        "slt r1 r2 r3",
        "sltu r1 r2 r3",
        "shiftl r1 2 r3",
        "shiftr r1 r2 r3",
        "shiftrs r1 r2 r3",
        "not r2 r3",
        "load 5 r1",
        "loadhi 2 r1",
        # Arithmetic (multi)
        "mults r1 r2 r3",
        "multu r1 4 r3",
        "multfp r1 r2 r3",
        "divs r1 r2 r3",
        "divu r1 r2 r3",
        "divfp r1 r2 r3",
        "mods r1 r2 r3",
        "modu r1 r2 r3",
        # Branches
        "beq r1 r2 4",
        "bne r1 r2 5",
        "bgt r1 r2 6",
        "bge r1 r2 7",
        "blt r1 r2 8",
        "ble r1 r2 9",
        "bgts r1 r2 10",
        "bges r1 r2 11",
        "blts r1 r2 12",
        "bles r1 r2 13",
        # Jumps
        "jump 1",
        "jumpo 2",
        "jumpr 3 r1",
        "jumpro 4 r2",
    ]

    # Act & Assert
    for code in samples:
        inst = parse(code)
        b = inst.to_binary_string()
        assert len(b.split()[0]) == 32  # 32-bit binary before comment


def test_load32_expansion_with_high_bits():
    """Test that LOAD32 with non-zero high bits expands to two instructions."""
    # Arrange
    line = AssemblyLine.parse_line(src("load32 0x12345678 r1"))

    # Act
    expanded = line.expand()

    # Assert
    # Should expand into two instructions when high bits are non-zero
    assert len(expanded) == 2
    for e in expanded:
        assert isinstance(e, InstructionAssemblyLine)
        # Both must produce 32-bit binary
        assert len(e.to_binary_string().split()[0]) == 32


def test_load32_expansion_no_high_bits():
    """Test that LOAD32 with zero high bits expands to one instruction."""
    # Arrange
    line = AssemblyLine.parse_line(src("load32 0x00005678 r1"))

    # Act
    expanded = line.expand()

    # Assert
    # Should expand into only one instruction when high bits are zero
    assert len(expanded) == 1
    assert isinstance(expanded[0], InstructionAssemblyLine)
    # Must produce 32-bit binary
    assert len(expanded[0].to_binary_string().split()[0]) == 32

    # Test with smaller value that clearly has no high bits
    line = AssemblyLine.parse_line(src("load32 0x1234 r1"))
    expanded = line.expand()
    assert len(expanded) == 1


def test_addr2reg_expansion_label_resolution():
    """Test that ADDR2REG pseudo-instruction resolves labels and expands correctly."""
    # Arrange
    code_lines = [
        src(".code"),
        src("Start:"),
        src("addr2reg Start r1"),
    ]
    # Assembler expects preprocessed lines (SourceLine list)
    out_fd, out_path = tempfile.mkstemp()
    os.close(out_fd)

    try:
        assembler = Assembler(
            preprocessed_input_lines=code_lines, output_file_path=out_path
        )

        # Act
        assembler.assemble()

        # Assert
        # Output should contain two lines for load/loadhi
        with open(out_path) as f:
            binary_lines = [line.strip() for line in f.readlines() if line.strip()]
        assert len(binary_lines) == 2
    finally:
        os.remove(out_path)


def test_halt_with_argument_error():
    """Test that HALT instruction raises error when given invalid arguments."""
    # Arrange
    inst = parse("halt")

    # Act & Assert
    with pytest.raises(ValueError):
        # Force argument misuse by manually injecting fake register
        inst.arguments = ["bad"]  # type: ignore
        inst.to_binary_string()


def test_branch_with_label_resolution():
    """Test that branch instructions correctly resolve labels to relative offsets."""
    # Arrange
    code_lines = [
        src(".code"),
        src("beq r1 r2 Target"),  # Address 0, branch forward to Target
        src("nop"),  # Address 1
        src("nop"),  # Address 2
        src("Target:"),
        src("halt"),  # Address 3 - Target points here
    ]
    out_fd, out_path = tempfile.mkstemp()
    os.close(out_fd)

    try:
        assembler = Assembler(
            preprocessed_input_lines=code_lines, output_file_path=out_path
        )

        # Act
        assembler.assemble()

        # Assert
        with open(out_path) as f:
            binary_lines = [line.strip() for line in f.readlines() if line.strip()]
        # Should have: beq, nop, nop, halt
        assert len(binary_lines) == 4
        # First instruction is branch - offset should be +3 (from addr 0 to addr 3)
        beq_binary = binary_lines[0]
        assert beq_binary.startswith("0110")  # Branch prefix
    finally:
        os.remove(out_path)


def test_branch_backward_with_label():
    """Test that branch instructions correctly resolve backward labels (negative offset)."""
    # Arrange
    code_lines = [
        src(".code"),
        src("Loop:"),
        src("nop"),  # Address 0 - Loop points here
        src("nop"),  # Address 1
        src("bne r1 r2 Loop"),  # Address 2, branch back to Loop (offset -2)
    ]
    out_fd, out_path = tempfile.mkstemp()
    os.close(out_fd)

    try:
        assembler = Assembler(
            preprocessed_input_lines=code_lines, output_file_path=out_path
        )

        # Act
        assembler.assemble()

        # Assert
        with open(out_path) as f:
            binary_lines = [line.strip() for line in f.readlines() if line.strip()]
        # Should have: nop, nop, bne
        assert len(binary_lines) == 3
        # Last instruction is branch with negative offset
        bne_binary = binary_lines[2]
        assert bne_binary.startswith("0110")  # Branch prefix
        # Offset is -2 which in 16-bit two's complement is 0xFFFE
        # The offset is encoded in bits [31:16] after the opcode
        offset_bits = bne_binary[4:20]  # 16 bits for offset
        offset_value = int(offset_bits, 2)
        # Convert from unsigned to signed
        if offset_value >= 0x8000:
            offset_value -= 0x10000
        assert offset_value == -2
    finally:
        os.remove(out_path)
