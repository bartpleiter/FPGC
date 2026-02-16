from asmpy.models.assembly_line import InstructionAssemblyLine
from asmpy.models.data_types import SourceLine


def parse(code: str) -> InstructionAssemblyLine:
    """Helper to parse an instruction line quickly."""
    src = SourceLine(line=code, source_line_number=1, source_file_name="test.asm")
    inst = InstructionAssemblyLine.parse_line(src)  # type: ignore
    assert isinstance(inst, InstructionAssemblyLine)
    return inst  # type: ignore


def bits(instr: str) -> str:
    """Extract just the binary bits from instruction binary string."""
    return instr.split()[0]


def test_halt_pattern():
    """Test that HALT instruction produces all 1s pattern."""
    # Arrange & Act
    b = bits(parse("halt").to_binary_string())

    # Assert
    assert b == "1" * 32


def test_opcode_prefixes_basic():
    """Test that basic instructions have correct 4-bit prefixes."""
    # Arrange
    cases = [
        ("read 0 r1 r2", "1110"),
        ("write 0 r1 r2", "1101"),
        ("readintid r3", "1100"),
        ("push r4", "1011"),
        ("pop r5", "1010"),
        ("jump 1", "1001"),
        ("jumpo 1", "1001"),
        ("jumpr 0 r1", "1000"),
        ("jumpro 0 r1", "1000"),
        ("ccache", "0111"),
        ("beq r1 r2 1", "0110"),
        ("savpc r7", "0101"),
        ("reti", "0100"),
    ]

    # Act & Assert
    for code, prefix in cases:
        assert bits(parse(code).to_binary_string()).startswith(prefix), code


def test_branch_field_layout_signed_flag():
    """Test that branch instructions correctly encode signed flag."""
    # Arrange & Act
    # beq unsigned -> last bit should be 0, opcode 000
    beq = bits(parse("beq r1 r2 1").to_binary_string())
    # bgts signed -> opcode 001, signed bit 1
    bgts = bits(parse("bgts r1 r2 1").to_binary_string())

    # Assert
    assert beq[:4] == "0110"
    assert beq[-4:-1] == "000"  # branch opcode
    assert beq[-1] == "0"  # unsigned
    assert bgts[-4:-1] == "001"
    assert bgts[-1] == "1"


def test_jump_offset_flag():
    """Test that JUMP and JUMPO have correct offset flags."""
    # Arrange & Act
    j_abs = bits(parse("jump 5").to_binary_string())
    j_off = bits(parse("jumpo 5").to_binary_string())

    # Assert
    assert j_abs[:4] == "1001" and j_off[:4] == "1001"
    assert j_abs[-1] == "0"
    assert j_off[-1] == "1"


def test_jump_offset_negative_encoding():
    """Test that negative JUMPO offset is encoded as 27-bit two's complement."""
    j_neg = bits(parse("jumpo -1").to_binary_string())

    assert j_neg[:4] == "1001"
    assert j_neg[-1] == "1"
    assert j_neg[4:31] == "1" * 27


def test_jumpr_offset_flag():
    """Test that JUMPR and JUMPRO have correct offset flags."""
    # Arrange & Act
    jr_abs = bits(parse("jumpr 0 r2").to_binary_string())
    jr_off = bits(parse("jumpro 0 r2").to_binary_string())

    # Assert
    assert jr_abs[:4] == "1000" and jr_off[:4] == "1000"
    assert jr_abs[-1] == "0"
    assert jr_off[-1] == "1"


def test_arith_single_register_vs_constant():
    """Test that arithmetic instructions use different prefixes for register vs constant."""
    # Arrange & Act
    reg_form = bits(parse("add r1 r2 r3").to_binary_string())
    const_form = bits(parse("add r1 5 r3").to_binary_string())

    # Assert
    # Register form -> ARITH prefix 0000
    assert reg_form[:4] == "0000"
    # Constant form -> ARITHC prefix 0001
    assert const_form[:4] == "0001"


def test_arithm_multi_cycle_forms():
    """Test that multi-cycle arithmetic instructions use correct prefixes."""
    # Arrange & Act
    reg_form = bits(parse("mults r1 r2 r3").to_binary_string())
    const_form = bits(parse("multu r1 7 r3").to_binary_string())

    # Assert
    # Register form -> ARITHMC prefix 0010
    assert reg_form[:4] == "0010"
    # Constant form -> ARITHM prefix 0011
    assert const_form[:4] == "0011"


def test_not_encoding():
    """Test that NOT instruction has correct prefix and opcode."""
    # Arrange & Act
    pattern = bits(parse("not r2 r3").to_binary_string())

    # Assert
    assert pattern[:4] == "0000"  # ARITH
    # opcode for NOT is 0111
    assert pattern[4:8] == "0111"


def test_load_and_loadhi():
    """Test that LOAD and LOADHI have correct prefixes and opcodes."""
    # Arrange & Act
    load = bits(parse("load 5 r1").to_binary_string())
    loadhi = bits(parse("loadhi 6 r1").to_binary_string())

    # Assert
    assert load[:4] == "0001" and loadhi[:4] == "0001"
    # opcode bits (after prefix) should match 1100 and 1101
    assert load[4:8] == "1100"
    assert loadhi[4:8] == "1101"
