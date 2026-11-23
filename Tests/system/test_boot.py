"""The boot ROM program (Software/boot/boot.asm) in the ISS, with the devices: its boot
lines, and loading programs over the UART, with the programs of
Software/programs."""

from pathlib import Path

from b32asm.assembler import assemble
from b32sim.devices import Board, CycleCounter, DeviceBridge, Timer, Uart
from b32sim.sim import ROM_BASE, Machine
from fpgc.loader import crc, load_message, ok_line, program_bytes

REPO = Path(__file__).resolve().parents[2]
BOOT = REPO / "Software/boot/boot.asm"
PROGRAMS = REPO / "Software/programs"

BOOT_LINES = b"boot: err 00000000 pc 00000000\r\nboot: ready\r\n"
HELLO = program_bytes((PROGRAMS / "hello.asm").read_text(), "hello.asm")


def boot(
    incoming: bytes = b"", steps: int = 3000, machine: list[Machine] | None = None
) -> tuple[Uart, Timer]:
    """Run the boot ROM for `steps` instructions, with a timer that runs with its
    interrupt enabled, the UART's interrupt enabled, and PC_BACKUP and SP not 0, as a
    program may leave them when it jumps back to the ROM. The PC's bytes in `incoming`
    have all arrived. The Machine is appended to `machine`, if given."""
    uart, timer = Uart(incoming), Timer()
    bridge = DeviceBridge(
        [uart, CycleCounter(), timer, Board()], interrupts={1: timer, 2: uart}
    )
    uart.write(Uart.CONTROL, 1)
    timer.write(0x04, 3)  # CONTROL: periodic, with the interrupt
    timer.write(0x00, 100_000)  # COUNT
    m = Machine(io=bridge)
    m.load(ROM_BASE, assemble(BOOT.read_text(), ROM_BASE, str(BOOT)).to_bytes())
    m.pc_backup, m.sp = 0x1234, 5
    if machine is not None:
        machine.append(m)
    for _ in range(steps):
        m.step()
    return uart, timer


def test_boot_lines_and_quiet_devices() -> None:
    machine: list[Machine] = []
    uart, timer = boot(machine=machine)
    assert bytes(uart.output) == BOOT_LINES
    assert not uart.enabled
    assert [timer.read(r) for r in (0x00, 0x04, 0x08)] == [0, 0, 0]
    assert (machine[0].pc_backup, machine[0].sp) == (0, 0)


def test_load_and_return() -> None:
    """The program is loaded, runs from SDRAM, and returns to the boot ROM, which
    starts again from the top. Bytes before the "L" are ignored."""
    uart, _ = boot(b"xyz" + load_message(HELLO), steps=20_000)
    assert bytes(uart.output) == (
        BOOT_LINES + ok_line(HELLO) + b"\r\nhello, world\r\n" + BOOT_LINES
    )


def test_bad_crc() -> None:
    """A wrong CRC: the boot ROM answers with its own CRC, and waits again."""
    message = bytearray(load_message(HELLO))
    message[-1] ^= 0x01
    uart, _ = boot(bytes(message), steps=20_000)
    assert bytes(uart.output) == (
        BOOT_LINES + f"boot: bad {crc(HELLO):08x}\r\nboot: ready\r\n".encode()
    )


def test_bad_length() -> None:
    """A length that isn't whole words: no load."""
    uart, _ = boot(b"L" + (6).to_bytes(4, "little"))
    assert bytes(uart.output) == BOOT_LINES + b"boot: bad length\r\nboot: ready\r\n"


def memtest(source: str, steps: int) -> bytes:
    """Run the memory test over 64 words instead of 64 MiB, and return its lines."""
    assert ".equ END 0x04000000" in source
    source = source.replace(".equ END 0x04000000", ".equ END 0x00010100")
    program = program_bytes(source, "memtest.asm")
    uart, _ = boot(load_message(program), steps=steps)
    output = bytes(uart.output)
    assert output.startswith(BOOT_LINES + ok_line(program) + b"\r\n")
    assert output.endswith(BOOT_LINES), "the test didn't return to the boot ROM"
    return output[len(BOOT_LINES) + len(ok_line(program)) + 2 : -len(BOOT_LINES)]


def test_memtest() -> None:
    source = (PROGRAMS / "memtest.asm").read_text()
    assert memtest(source, 200_000) == (
        b"memtest: pass 00000000 done, errors 00000000\r\n"
        b"memtest: pass 00000001 done, errors 00000000\r\n"
        b"memtest: pass 00000002 done, errors 00000000\r\n"
        b"memtest: done, errors 00000000\r\n"
    )


def test_memtest_errors() -> None:
    """With another seed for the check than for the writes, every random word is
    wrong: the first 16 are printed, and all 64 counted."""
    source = (PROGRAMS / "memtest.asm").read_text()
    writes, checks = source.rsplit("load32 SEED r9", 1)
    source = writes + "load32 0x12345678 r9" + checks
    lines = memtest(source, 200_000).splitlines()
    errors = [line for line in lines if line.startswith(b"memtest: error at ")]
    assert len(errors) == 16
    assert errors[0].startswith(b"memtest: error at 00010000 wrote ")
    assert lines[-1] == b"memtest: done, errors 00000040"
