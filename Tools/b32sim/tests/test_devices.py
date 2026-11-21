"""The device models and the device bridge, in the ISS."""

import pytest

from b32asm.assembler import assemble
from b32sim.devices import Board, CycleCounter, DeviceBridge, Timer, Uart
from b32sim.sim import ROM_BASE, ErrorKind, IllegalError, Machine

UART, COUNTER, TIMER, BOARD = 0xFFFF8000, 0xFFFF8100, 0xFFFF8200, 0xFFFF8300


def machine(
    source: str, sdram: bool = False, incoming: bytes = b""
) -> tuple[Machine, Board, Timer, Uart]:
    """A machine with the four devices of phase B, the timer on interrupt 1 and the
    UART on 2, running `source` from the ROM, or from SDRAM after a jump from the
    ROM. The UART has `incoming` waiting."""
    uart, counter, timer = Uart(incoming), CycleCounter(), Timer()
    board = Board(switches=0b1010)
    bridge = DeviceBridge([uart, counter, timer, board], interrupts={1: timer, 2: uart})
    m = Machine(io=bridge)
    if sdram:
        m.load(ROM_BASE, assemble("jump 0").to_bytes())
        m.load(0, assemble(source, 0).to_bytes())
    else:
        m.load(ROM_BASE, assemble(source, ROM_BASE).to_bytes())
    return m, board, timer, uart


def test_board() -> None:
    m, board, _, _ = machine(
        f"""
        load 0b10110 r1
        write {BOARD} r0 r1
        read {BOARD} r0 r2
        read {BOARD + 4} r0 r3
        halt
        """
    )
    m.run()
    assert board.led == 0
    assert m.regs[2:4] == [0, 0b1010]


def test_counter_counts_instructions() -> None:
    m, _, _, _ = machine(
        f"""
        write {COUNTER} r0 r0
        nop
        nop
        write {COUNTER} r0 r0
        read {COUNTER + 4} r0 r1
        read {COUNTER + 8} r0 r2
        halt
        """
    )
    m.run()
    assert m.regs[1:3] == [3, 0]  # three instructions after the first snapshot


@pytest.mark.parametrize(
    "access",
    [
        f"readb {BOARD} r0 r1",  # a device register is a word
        f"read {UART + 0x10} r0 r1",  # not a register of the UART
        f"read {0xFFFF8400} r0 r1",  # no device 4
        f"read {BOARD + 8} r0 r1",  # not a register of the board
        f"write {TIMER + 12} r0 r0",  # not a register of the timer
    ],
)
def test_bad_device_access(access: str) -> None:
    m, _, _, _ = machine(access + "\nhalt")
    with pytest.raises(IllegalError) as e:
        m.run()
    assert e.value.kind == ErrorKind.BAD_ADDRESS


def test_timer_interrupt() -> None:
    """The timer expires while its interrupt is disabled, and the interrupt comes when
    it's enabled. The handler clears the flag and stops."""
    m, _, timer, _ = machine(
        f"""
            jump Main
            jump Handler
        Handler:
            readintid r5
            write {TIMER + 8} r0 r0     ; clear the flag
            load 1 r6
            reti
        Main:
            load 0 r6
            load 1 r1
            write 0xFFFFFF10 r0 r1      ; INT_ENABLE
            load 3 r1
            write {TIMER} r0 r1         ; 3 cycles
            nop
            nop
            nop
            nop
            read {TIMER + 8} r0 r2      ; expired, but no interrupt yet
            load 2 r1
            write {TIMER + 4} r0 r1     ; enable the timer's interrupt
        Wait:
            beq r6 r0 Wait              ; taken until the handler ran
            halt
        """,
        sdram=True,
    )
    m.run()
    assert m.regs[2] == 1 and m.regs[5] == 1 and m.regs[6] == 1
    assert not timer.expired


def test_periodic_timer() -> None:
    timer = Timer()
    timer.write(Timer.CONTROL, Timer.PERIODIC)
    timer.write(Timer.COUNT, 4)
    expiries = []
    for t in range(13):
        timer.tick()
        if timer.expired:
            expiries.append(t)
            timer.write(Timer.STATUS, 0)
    assert expiries == [3, 7, 11]


def test_uart_hello_and_echo() -> None:
    """Prints "hi", then echoes what's waiting, polling the status."""
    m, _, _, uart = machine(
        f"""
            load 104 r1
            write {UART} r0 r1
            load 105 r1
            write {UART} r0 r1
        Echo:
            read {UART + 8} r0 r2
            beq r2 r0 Done              ; nothing waiting
            read {UART + 4} r0 r1
            write {UART} r0 r1
            jump Echo
        Done:
            read {UART + 4} r0 r3       ; empty: 0
            halt
        """,
        incoming=b"OK",
    )
    m.run()
    assert uart.output == b"hiOK" and m.regs[3] == 0


def test_uart_interrupt() -> None:
    """The UART's interrupt is 2, and the handler echoes until nothing's waiting."""
    m, _, _, uart = machine(
        f"""
            jump Main
            jump Handler
        Handler:
            readintid r5
        Next:
            read {UART + 4} r0 r1
            write {UART} r0 r1
            read {UART + 8} r0 r2
            bne r2 r0 Next
            load 1 r6
            reti
        Main:
            load 0 r6
            load 1 r1
            write 0xFFFFFF10 r0 r1      ; INT_ENABLE
            write {UART + 12} r0 r1     ; the UART's interrupt
        Wait:
            beq r6 r0 Wait
            halt
        """,
        sdram=True,
        incoming=b"abc",
    )
    m.run()
    assert uart.output == b"abc" and m.regs[5] == 2
