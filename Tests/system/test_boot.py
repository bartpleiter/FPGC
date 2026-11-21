"""The boot ROM program (Software/boot/boot.asm) in the ISS, with the devices."""

from pathlib import Path

from b32asm.assembler import assemble
from b32sim.devices import Board, CycleCounter, DeviceBridge, Timer, Uart
from b32sim.sim import ROM_BASE, Machine

REPO = Path(__file__).resolve().parents[2]
BOOT = REPO / "Software/boot/boot.asm"

BOOT_LINES = b"boot: err 00000000 pc 00000000\r\nboot: ready\r\n"


def boot(incoming: bytes = b"", steps: int = 3000) -> tuple[Uart, Timer]:
    """Run the boot ROM for `steps` instructions, with a timer that runs with its
    interrupt enabled, and the UART's interrupt enabled, as a program may leave them
    when it jumps back to the ROM."""
    uart, timer = Uart(incoming), Timer()
    bridge = DeviceBridge(
        [uart, CycleCounter(), timer, Board()], interrupts={1: timer, 2: uart}
    )
    uart.write(Uart.CONTROL, 1)
    timer.write(0x04, 3)  # CONTROL: periodic, with the interrupt
    timer.write(0x00, 100_000)  # COUNT
    m = Machine(io=bridge)
    m.load(ROM_BASE, assemble(BOOT.read_text(), ROM_BASE, str(BOOT)).to_bytes())
    for _ in range(steps):
        m.step()
    return uart, timer


def test_boot_lines_and_quiet_devices() -> None:
    uart, timer = boot()
    assert bytes(uart.output) == BOOT_LINES
    assert not uart.enabled
    assert [timer.read(r) for r in (0x00, 0x04, 0x08)] == [0, 0, 0]


def test_echo() -> None:
    uart, _ = boot(b"Hi!\r\n")
    assert bytes(uart.output) == BOOT_LINES + b"Hi!\r\n"
