---
title: Boot ROM
weight: 4
reader: programming
---

The boot ROM is the program that runs after every reset. It shows what went wrong before the reset, and then loads a program from the PC over the [UART](io/uart.md) into SDRAM and runs it. A program that's done jumps back to it, so the next program can be loaded without a reset.

Its source is `Software/boot/boot.asm`, and it's in the 4 KiB ROM at `0x04000000`.

## After a reset

1. Interrupts off: `INT_ENABLE` is 0, the UART's interrupt is disabled, and the timer is stopped, with its interrupt disabled and its flag cleared. `PC_BACKUP` and `SP` are set to 0, so the stack is empty. So a program that jumped back can't leave anything running into the next one. Only `INTID`, the number of the last interrupt, keeps its value until a reset.
2. It prints the error register, and then clears it:

   ```text
   boot: err 00000000 pc 00000000
   ```

   The first number is `ERR_STATUS`, the second `ERR_PC`. After a crash and a reset, they tell what went wrong and where (see the [instruction set](cpu/isa.md)).
3. It prints `boot: ready`, and waits for a command.

The same happens when a program jumps to `0x04000000`.

## Loading a program

The PC sends nothing before `boot: ready`. Then it sends one command:

| Bytes | What |
| --- | --- |
| 1 | `L` (`0x4C`) |
| 4 | the length of the program in bytes: a multiple of 4, at most 64 MiB |
| the length | the program, as it goes into SDRAM from address `0x0` |
| 4 | the CRC-32 of the program |

Each number is sent lowest byte first. Other bytes than `L` are ignored while the boot ROM waits for a command.

The boot ROM writes the program to SDRAM from address `0x0`, and computes the CRC-32 while it receives. Then it answers with a line:

| Answer | Then |
| --- | --- |
| `boot: ok <crc>` | the program is whole: `ccache`, and a jump to `0x0` |
| `boot: bad <crc>` | the CRCs differ: `boot: ready` again |
| `boot: bad length` | the length isn't a multiple of 4, or is more than 64 MiB: `boot: ready` again |

The `<crc>` is the one the boot ROM computed, in hex. If the board gets fewer bytes than the length, it keeps waiting for them. The PC then resets the board and tries again.

- At 1 Mbaud, a program loads at 100,000 bytes per second: 1 KiB in about 10 ms. The boot ROM handles each byte well within the 1,000 cycles that a byte takes on the line, so the PC sends the whole program without pauses.
- Everything the board sends is lines of text, ending with `\r\n`. What the PC sends is binary.
- [uartload](toolchain/uartload.md) does all of this on the PC: it resets the board, waits for `boot: ready`, sends the program, and tries up to 3 times. Then it shows what the program sends, until the program is back in the boot ROM.

## What a program can expect, and must do

- It starts at `0x0`, with interrupts disabled, the UART's interrupt disabled, the timer stopped, `PC_BACKUP` 0 and an empty stack. Registers `r1`–`r15` are undefined.
- SDRAM holds the program from `0x0`. The rest of SDRAM is undefined.
- If it uses interrupts, it needs a handler at `0x4`, and enables interrupts itself once the handler is ready (see the [programming notes](cpu/programming.md#interrupts)).
- When it's done, it jumps back with `jump 0x04000000`. The boot ROM then starts from the top, prints the error register, and waits for the next program.

A hello world (the [assembly language](toolchain/assembly.md) page explains it line by line):

```text
.equ UART_TRANSMIT 0xFFFF8000
.equ BOOT_ROM 0x04000000

    addr2reg Text r1
Next:
    readbu 0 r1 r2              ; the next character
    beq r2 r0 Done
    write UART_TRANSMIT r0 r2
    add r1 1 r1
    jump Next
Done:
    jump BOOT_ROM
Text:
    .ds "hello, world\r\n"
```

`Software/programs/` has this one as `hello.asm`, and a memory test, `memtest.asm`, which tests the SDRAM behind it with three patterns and prints each error.
