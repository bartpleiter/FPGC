from asmpy.models.assembly_line import InstructionAssemblyLine
from asmpy.models.data_types import SourceLine

# Helper to parse an instruction line quickly


def parse(code: str) -> InstructionAssemblyLine:
    src = SourceLine(line=code, source_line_number=1, source_file_name="test.asm")
    inst = InstructionAssemblyLine.parse_line(src)  # type: ignore
    assert isinstance(inst, InstructionAssemblyLine)
    return inst  # type: ignore


def bits(instr: str) -> str:
    return instr.split()[0]


def test_halt_pattern():
    b = bits(parse("halt").to_binary_string())
    assert b == "1" * 32


def test_opcode_prefixes_basic():
    # (instruction, expected 4-bit prefix)
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
    for code, prefix in cases:
        assert bits(parse(code).to_binary_string()).startswith(prefix), code


def test_branch_field_layout_signed_flag():
    # beq unsigned -> last bit should be 0, opcode 000
    beq = bits(parse("beq r1 r2 1").to_binary_string())
    assert beq[:4] == "0110"
    assert beq[-4:-1] == "000"  # branch opcode
    assert beq[-1] == "0"  # unsigned
    # bgts signed -> opcode 001, signed bit 1
    bgts = bits(parse("bgts r1 r2 1").to_binary_string())
    assert bgts[-4:-1] == "001"
    assert bgts[-1] == "1"


def test_jump_offset_flag():
    j_abs = bits(parse("jump 5").to_binary_string())
    j_off = bits(parse("jumpo 5").to_binary_string())
    assert j_abs[:4] == "1001" and j_off[:4] == "1001"
    assert j_abs[-1] == "0"
    assert j_off[-1] == "1"


def test_jumpr_offset_flag():
    jr_abs = bits(parse("jumpr 0 r2").to_binary_string())
    jr_off = bits(parse("jumpro 0 r2").to_binary_string())
    assert jr_abs[:4] == "1000" and jr_off[:4] == "1000"
    assert jr_abs[-1] == "0"
    assert jr_off[-1] == "1"


def test_arith_single_register_vs_constant():
    reg_form = bits(parse("add r1 r2 r3").to_binary_string())
    const_form = bits(parse("add r1 5 r3").to_binary_string())
    # Register form -> ARITH prefix 0000
    assert reg_form[:4] == "0000"
    # Constant form -> ARITHC prefix 0001
    assert const_form[:4] == "0001"


def test_arithm_multi_cycle_forms():
    reg_form = bits(parse("mults r1 r2 r3").to_binary_string())
    const_form = bits(parse("multu r1 7 r3").to_binary_string())
    # Register form -> ARITHMC prefix 0010
    assert reg_form[:4] == "0010"
    # Constant form -> ARITHM prefix 0011
    assert const_form[:4] == "0011"


def test_not_encoding():
    pattern = bits(parse("not r2 r3").to_binary_string())
    assert pattern[:4] == "0000"  # ARITH
    # opcode for NOT is 0111
    assert pattern[4:8] == "0111"


def test_load_and_loadhi():
    load = bits(parse("load 5 r1").to_binary_string())
    loadhi = bits(parse("loadhi 6 r1").to_binary_string())
    assert load[:4] == "0001" and loadhi[:4] == "0001"
    # opcode bits (after prefix) should match 1100 and 1101
    assert load[4:8] == "1100"
    assert loadhi[4:8] == "1101"
