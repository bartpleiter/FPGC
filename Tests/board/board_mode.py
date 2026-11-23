"""Programs in board mode: a CPU test program made into a program that the boot ROM
loads and runs on the board, with one result line that the ISS must give too.

- At the start, it clears SDRAM from 0x10000 to 0x1FFFF, where the test programs keep
  their data, and sets r1 to r15 to 0. On the board, both hold what ran before, and in
  the ISS they're 0. (INTID, the number of the last interrupt, only a reset clears: a
  test with interrupts resets the board after it.)
- Every `halt` becomes a jump to the end, which folds r1 to r15, the SDRAM from 0x10000
  to 0x11FFF and ERR_STATUS into one word, prints it as "result <word>", and returns to
  the boot ROM.

`expected` runs the same binary in the ISS, from address 0 with the boot ROM in the ROM,
and returns what it prints.
"""

from pathlib import Path

from b32asm.assembler import assemble
from b32sim.devices import Board, CycleCounter, DeviceBridge, Timer, Uart
from b32sim.sim import ROM_BASE, Machine

REPO = Path(__file__).resolve().parents[2]
BOOT = REPO / "Software/boot/boot.asm"


def _rotate_xor(value: str) -> list[str]:
    """r1 = (r1 rotated left by 1) xor `value`, with r3 as scratch."""
    return ["shiftl r1 1 r3", "shiftr r1 31 r1", "or r1 r3 r1", f"xor r1 {value} r1"]


HEADER = [
    "load32 0x00010000 r1",
    "load32 0x00020000 r2",
    "BoardClear:",
    "write 0 r1 r0",
    "add r1 4 r1",
    "bne r1 r2 BoardClear",
    *(f"or r0 r0 r{n}" for n in range(1, 16)),
]

FOOTER = [
    "BoardEnd:",
    *(f"push r{n}" for n in range(2, 16)),
    *(line for _ in range(2, 16) for line in ["pop r2", *_rotate_xor("r2")]),
    "load32 0x00010000 r4",
    "load32 0x00012000 r5",
    "BoardFold:",
    "read 0 r4 r2",
    *_rotate_xor("r2"),
    "add r4 4 r4",
    "bne r4 r5 BoardFold",
    "read 0xFFFFFF08 r0 r2",  # ERR_STATUS
    *_rotate_xor("r2"),
    # result <r1 in hex>
    *(f"load {ord(c)} r2\nwrite 0xFFFF8000 r0 r2" for c in "result "),
    "load 8 r3",
    "load 10 r4",
    "BoardHex:",
    "shiftr r1 28 r2",
    "shiftl r1 4 r1",
    "blt r2 r4 BoardDigit",
    "add r2 0x27 r2",
    "BoardDigit:",
    "add r2 0x30 r2",
    "write 0xFFFF8000 r0 r2",
    "sub r3 1 r3",
    "bne r3 r0 BoardHex",
    "load 0x0D r2",
    "write 0xFFFF8000 r0 r2",
    "load 0x0A r2",
    "write 0xFFFF8000 r0 r2",
    "jump 0x04000000",
]


def board_mode(source: str) -> str:
    """The program in board mode."""
    body = [
        "jump BoardEnd" if line.split(";")[0].strip() == "halt" else line
        for line in source.splitlines()
    ]
    return "\n".join([*HEADER, *body, *FOOTER]) + "\n"


def expected(program: bytes, max_steps: int = 2_000_000) -> bytes:
    """What the program prints in the ISS, in hardware mode, until it jumps back to the
    boot ROM."""
    uart = Uart()
    m = Machine(strict=False, io=DeviceBridge([uart, CycleCounter(), Timer(), Board()]))
    m.load(ROM_BASE, assemble(BOOT.read_text(), ROM_BASE, str(BOOT)).to_bytes())
    m.load(0, program)
    m.pc = 0
    for _ in range(max_steps):
        m.step()
        if m.pc == ROM_BASE:
            return bytes(uart.output)
    raise AssertionError(f"the program didn't return to the boot ROM in {max_steps}")
