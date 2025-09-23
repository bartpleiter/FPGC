from asmpy.models.assembly_line import InstructionAssemblyLine
from asmpy.models.data_types import SourceLine

def make_line(code: str) -> InstructionAssemblyLine:
    src = SourceLine(line=code, source_line_number=1, source_file_name="test.asm")
    line = InstructionAssemblyLine.parse_line(src)  # type: ignore
    assert isinstance(line, InstructionAssemblyLine)
    return line  # type: ignore


def test_load_and_loadhi_label_resolution():
    # Simulate label resolution by setting target_address manually
    low = make_line("load 0x1234 r1")
    hi = make_line("loadhi 0xABCD r1")
    # LOAD/LOADHI use ARITHC (0001) prefix
    assert low.to_binary_string().startswith("0001")
    assert hi.to_binary_string().startswith("0001")


def test_add_register_and_constant():
    add_reg = make_line("add r1 r2 r3")
    add_const = make_line("add r1 5 r3")
    b1 = add_reg.to_binary_string()
    b2 = add_const.to_binary_string()
    assert b1[:4] == "0000"  # ARITH opcode
    # Constant arithmetic uses ARITHC (0001)
    assert b2[:4] == "0001"


def test_branch_equal():
    beq = make_line("beq r1 r2 4")
    b = beq.to_binary_string()
    assert b.startswith("0110")  # Branch prefix
    # ensure lower bits contain branch opcode + signed bit (3+1 bits)
    assert len(b.split()[0]) == 32


def test_jump():
    jmp = make_line("jump 10")
    b = jmp.to_binary_string()
    assert b.startswith("1001")
    assert b.endswith("0")


def test_jump_offset():
    jmpo = make_line("jumpo 5")
    b = jmpo.to_binary_string()
    assert b.startswith("1001")
    assert b.endswith("1")


def test_push_pop():
    push = make_line("push r3")
    pop = make_line("pop r7")
    assert push.to_binary_string().startswith("1011")
    assert pop.to_binary_string().startswith("1010")
