"""Tests for b32sim ISS."""

import pytest

from b32asm.assembler import assemble
from b32sim.sim import (
    ERR_PC_ADDR,
    ERR_STATUS_ADDR,
    ROM_BASE,
    ErrorKind,
    IllegalError,
    Machine,
)


def run_rom(source: str, strict: bool = True) -> Machine:
    """Run a program from ROM until it halts."""
    machine = Machine(strict=strict)
    machine.load(ROM_BASE, assemble(source, ROM_BASE).to_bytes())
    machine.run(max_steps=10_000)
    return machine


def sdram_machine(source: str) -> Machine:
    """A machine that jumps from ROM to a program at SDRAM address 0."""
    machine = Machine()
    machine.load(ROM_BASE, assemble("jump 0").to_bytes())
    machine.load(0, assemble(source, 0).to_bytes())
    return machine


def result(body: str) -> int:
    """Run `body` followed by halt, and return r3."""
    return run_rom(body + "\nhalt").regs[3]


# ---- Arithmetic ----


@pytest.mark.parametrize(
    ("body", "expected"),
    [
        ("load32 0x7FFFFFFF r1\nadd r1 1 r3", 0x80000000),
        ("load 1 r1\nload 33 r2\nshiftl r1 r2 r3", 2),  # shift amount is B[4:0]
        ("load32 0x80000000 r1\nshiftrs r1 4 r3", 0xF8000000),
        ("load32 0x80000000 r1\nshiftr r1 4 r3", 0x08000000),
        ("load 1 r1\nsltu r1 -1 r3", 1),  # the constant is sign-extended
        ("load 1 r1\nslt r1 -1 r3", 0),
        ("load 0xFFFF r3", 0x0000FFFF),  # load zero-extends
        ("load 0x5678 r3\nloadhi 0x1234 r3", 0x12345678),
        ("load 5 r1\nnot r1 r3", 0xFFFFFFFA),
        ("load 7 r1\nload 0 r2\ndivs r1 r2 r3", 0xFFFFFFFF),
        ("load 7 r1\nload 0 r2\ndivu r1 r2 r3", 0xFFFFFFFF),
        ("load 7 r1\nload 0 r2\ndivfp r1 r2 r3", 0xFFFFFFFF),
        ("load 7 r1\nload 0 r2\nmods r1 r2 r3", 7),
        ("load 7 r1\nload 0 r2\nmodu r1 r2 r3", 7),
        ("load32 0x80000000 r1\ndivs r1 -1 r3", 0x80000000),
        ("load32 0x80000000 r1\nmods r1 -1 r3", 0),
        ("load32 -7 r1\ndivs r1 2 r3", 0xFFFFFFFD),  # rounds toward zero
        ("load32 -7 r1\nmods r1 2 r3", 0xFFFFFFFF),  # sign of the dividend
        ("load32 0x18000 r1\nload32 0x20000 r2\nmultfp r1 r2 r3", 0x30000),  # 1.5 * 2
        ("load32 -1 r1\nload 1 r2\nmultfp r1 r2 r3", 0xFFFFFFFF),  # rounds down
        ("load32 0x30000 r1\nload32 0x20000 r2\ndivfp r1 r2 r3", 0x18000),  # 3 / 2
        ("load32 -1 r1\nload32 -1 r2\nmulshi r1 r2 r3", 0),
        ("load32 -1 r1\nload32 -1 r2\nmultuhi r1 r2 r3", 0xFFFFFFFE),
        ("load32 -2 r1\nload 3 r2\nmults r1 r2 r3", 0xFFFFFFFA),
    ],
)
def test_arithmetic(body: str, expected: int) -> None:
    assert result(body) == expected


def test_r0_stays_zero() -> None:
    machine = run_rom("load 5 r0\nadd r0 r0 r3\nhalt")
    assert machine.regs[0] == 0 and machine.regs[3] == 0


# ---- Memory ----


def test_little_endian_subword_access() -> None:
    machine = run_rom(
        """load32 0x1000 r1
        load32 0x80FF1234 r2
        write 0 r1 r2
        readb 3 r1 r3
        readhu 0 r1 r4
        readh 2 r1 r5
        writeb 1 r1 r0
        read 0 r1 r6
        halt"""
    )
    assert machine.regs[3:7] == [0xFFFFFF80, 0x1234, 0xFFFF80FF, 0x80FF0034]


def test_cpu_io_registers() -> None:
    machine = run_rom(
        """load32 0x1F000000 r1
        load 99 r2
        write 0 r1 r2
        push r2
        push r2
        read 4 r1 r3
        load 0x1FF r2
        write 4 r1 r2
        read 4 r1 r4
        read 0 r1 r5
        halt"""
    )
    assert machine.regs[3:6] == [2, 0xFF, 99]


# ---- Stack ----


def test_push_pop_order() -> None:
    machine = run_rom("load 1 r1\nload 2 r2\npush r1\npush r2\npop r3\npop r4\nhalt")
    assert machine.regs[3:5] == [2, 1] and machine.sp == 0


def test_pop_r0_drops_a_value() -> None:
    machine = run_rom("load 1 r1\npush r1\npush r1\npop r0\nhalt")
    assert machine.sp == 1


# ---- Control flow ----


def test_savpc_and_call() -> None:
    machine = run_rom(
        """savpc r1
        add r1 12 r1
        jump Function
        halt
        Function:
        load 42 r3
        jumpr 0 r1"""
    )
    assert machine.regs[3] == 42 and machine.regs[1] == ROM_BASE + 12


@pytest.mark.parametrize(
    ("branch", "taken"),
    [
        ("bgt r1 r2", False),  # 1 > 0xFFFFFFFF unsigned
        ("bgts r1 r2", True),  # 1 > -1 signed
        ("blt r1 r2", True),
        ("blts r1 r2", False),
        ("bge r1 r1", True),
        ("ble r2 r1", False),
        ("bles r2 r1", True),
        ("bne r1 r2", True),
        ("beq r1 r2", False),
    ],
)
def test_branch_conditions(branch: str, taken: bool) -> None:
    source = f"load 1 r1\nload32 -1 r2\n{branch} 8\nload 1 r3\nhalt"
    assert result(source) == (0 if taken else 1)


# ---- Interrupts ----


def test_interrupt_is_taken_at_a_taken_branch_and_returns() -> None:
    machine = sdram_machine(
        "jump Main\njump Handler\nMain:\njump Main\nHandler:\nreadintid r5\nreti"
    )
    for _ in range(4):
        machine.step()
    machine.request_interrupt(3)
    machine.request_interrupt(2)
    first = machine.step()  # the jump at Main is replaced by the interrupt
    assert first.interrupt == 2 and machine.pc == 4
    assert machine.pc_backup == first.pc and not machine.interrupts_enabled
    machine.step()  # jump Handler
    machine.step()  # readintid
    assert machine.regs[5] == 2
    reti = machine.step()
    assert reti.redirect == first.pc and machine.interrupts_enabled
    second = machine.step()  # the next pending one, at the same jump
    assert second.interrupt == 3


def test_no_interrupts_in_rom() -> None:
    machine = Machine()
    machine.load(ROM_BASE, assemble("Loop:\njump Loop", ROM_BASE).to_bytes())
    machine.request_interrupt(1)
    for _ in range(10):
        assert machine.step().interrupt is None


def test_halt_in_sdram_is_interruptible() -> None:
    machine = sdram_machine("jump Main\njump Handler\nMain:\nhalt\nHandler:\nreti")
    machine.run()  # runs into the halt
    machine.request_interrupt(1)
    assert machine.step().interrupt == 1


# ---- Illegal cases ----

ILLEGAL = [
    (".dw 0x08000123", ErrorKind.ILLEGAL_INSTRUCTION),  # ALU op 1000
    (".dw 0x28000123", ErrorKind.ILLEGAL_INSTRUCTION),  # former FP64 opcode
    (".dw 0x60000016", ErrorKind.ILLEGAL_INSTRUCTION),  # branch condition 011
    (".dw 0xE0000033", ErrorKind.ILLEGAL_INSTRUCTION),  # read size 0011
    ("load 2 r1\nread 0 r1 r2", ErrorKind.UNALIGNED_ACCESS),
    ("load 1 r1\nwriteh 0 r1 r1", ErrorKind.UNALIGNED_ACCESS),
    ("load32 0x10000000 r1\nread 0 r1 r2", ErrorKind.BAD_ADDRESS),
    ("load32 0x1E000000 r1\nwrite 0 r1 r1", ErrorKind.BAD_ADDRESS),
    ("load32 0x1F000000 r1\nreadb 0 r1 r2", ErrorKind.BAD_ADDRESS),
    ("load32 0x1F00000C r1\nwrite 0 r1 r1", ErrorKind.BAD_ADDRESS),
    ("pop r1", ErrorKind.STACK_UNDERFLOW),
    (
        "load32 0x1F000000 r1\nload 255 r2\nwrite 4 r1 r2\npush r2",
        ErrorKind.STACK_OVERFLOW,
    ),
    ("load32 0x1E000002 r1\njumpr 4 r1", ErrorKind.BAD_JUMP_TARGET),
    ("jump 0x4000000", ErrorKind.BAD_JUMP_TARGET),
]


@pytest.mark.parametrize(("body", "kind"), ILLEGAL)
def test_illegal_stops_in_strict_mode(body: str, kind: ErrorKind) -> None:
    with pytest.raises(IllegalError) as info:
        run_rom(body + "\nhalt")
    assert info.value.kind == kind


@pytest.mark.parametrize(("body", "kind"), ILLEGAL)
def test_illegal_sets_error_register_in_hardware_mode(
    body: str, kind: ErrorKind
) -> None:
    machine = Machine(strict=False)
    machine.load(ROM_BASE, assemble(body + "\nhalt", ROM_BASE).to_bytes())
    for _ in range(100):
        retired = machine.step()
        if machine.err_flags:
            break
    assert machine.err_status == (1 << (kind - 1)) | (kind << 8)
    assert machine.err_pc == retired.pc


def test_hardware_mode_behaviour() -> None:
    machine = run_rom(
        """.dw 0x08000123         ; reserved ALU op: no register write
        load32 0x1006 r1
        load32 0x11223344 r2
        write 0 r1 r2          ; unaligned: stored at 0x1004
        read 0 r1 r4
        load32 0x10000000 r1
        read 0 r1 r5           ; unmapped: reads 0
        pop r6                 ; underflow: reads entry 255
        halt""",
        strict=False,
    )
    assert (
        machine.regs[3] == 0 and machine.regs[4] == 0x11223344 and machine.regs[5] == 0
    )
    assert machine.sp == 255
    assert machine.err_flags == 0b100111 and machine.err_first == 1
    assert machine.err_pc == ROM_BASE


def test_error_register_read_and_clear() -> None:
    machine = run_rom(
        f"""pop r0
        load32 {ERR_STATUS_ADDR} r1
        read 0 r1 r2
        read {ERR_PC_ADDR - ERR_STATUS_ADDR} r1 r3
        write 0 r1 r0
        read 0 r1 r4
        halt""",
        strict=False,
    )
    assert machine.regs[2] == (1 << 5) | (6 << 8) and machine.regs[3] == ROM_BASE
    assert machine.regs[4] == 0


def test_unused_bits() -> None:
    with pytest.raises(IllegalError) as info:
        run_rom(".dw 0x40000001")  # RETI with an unused bit set
    assert info.value.kind is None
    machine = run_rom(".dw 0x00F00000\nhalt", strict=False)  # ignored, no flag
    assert machine.err_status == 0
