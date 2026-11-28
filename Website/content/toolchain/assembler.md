---
title: Assembler
weight: 2
reader: programming
---

`b32asm` turns a program in the [assembly language](assembly.md) into a binary image for B32P4. It reads one source file and writes one binary file, and stops at the first error. Its source is in `Toolchain/b32asm`.

## Running it

```text
b32asm hello.asm hello.bin
b32asm boot.asm boot.bin --base 0x04000000
```

| Argument | What |
| --- | --- |
| the source | one assembly file |
| the output | the binary image to write |
| `--base` | the address of the first word, 0 by default. Programs that the [boot ROM](../boot.md) loads run from 0, and the boot ROM itself from `0x04000000`. |

The output is every instruction and data word in the order of the source, 4 bytes each, lowest byte first (see [the output](assembly.md#the-output)). It has no header: the first 4 bytes are the word at the base address. A program is then loaded onto the FPGC with [uartload](uartload.md).

## Errors

An error stops the assembler, and nothing is written. The message gives the file and the line, and what's wrong:

```text
error: hello.asm:12: unknown label 'Nxt'
```

| Message | Example | Cause |
| --- | --- | --- |
| unknown instruction | `frobnicate r1` | a mnemonic that doesn't exist |
| needs 3 operand(s), got 2 | `add r1 r2` | too few or too many operands |
| operand 1 of push has the wrong kind | `push 5` | a number where a register goes, or the other way around |
| there is no register 'r16' | `add r16 r1 r2` | only `r0` to `r15` exist |
| invalid operand | `add r1 4x r2` | not a register, number, character or name |
| does not fit in 16 bits signed | `add r1 40000 r2` | a number too large for its field |
| is not a multiple of 4 | `beq r0 r0 6` | a jump or branch to an address between two words |
| unknown label | `jump Nxt` | a name that's no label and no constant |
| is defined twice | `Loop:` twice | a name that's already a label or a constant |
| unknown escape | `.ds "a\q"` | a `\` that isn't one of the [escapes](assembly.md#operands) |

A constant that's used before its `.equ` is taken as a label, so it gives "unknown label", or "the wrong kind" where a label can't go.

## How it works

The assembler reads the source twice, in two passes. One pass isn't enough, because a label can be used before the line that defines it: in the hello world, `beq r2 r0 Done` comes before `Done:`, so the address of `Done` isn't known yet when the `beq` is read.

- Pass 1 reads each line, and turns it into statements of exactly one word each. It gives every statement its address, and so finds the address of every label. Constants get their value here, and pseudo-instructions become the instructions they stand for.
- Pass 2 turns each statement into its word. Now that every label is known, it can put in the addresses and offsets, and check that each value fits in its field.

### Pass 1

This is the hello world from the [language page](assembly.md#a-first-program), without its comments, after pass 1:

```text
source                         address   statements
.equ UART_TRANSMIT 0xFFFF8000            UART_TRANSMIT = 0xFFFF8000
.equ BOOT_ROM 0x04000000                 BOOT_ROM = 0x04000000
    addr2reg Text r1           0x00      load    Text[15:0] r1
                               0x04      loadhi  Text[31:16] r1
Next:                                    Next = 0x08
    readbu 0 r1 r2             0x08      readbu  0 r1 r2
    beq r2 r0 Done             0x0C      beq     r2 r0 Done
    write UART_TRANSMIT r0 r2  0x10      write   0xFFFF8000 r0 r2
    add r1 1 r1                0x14      add     r1 1 r1
    jump Next                  0x18      jump    Next
Done:                                    Done = 0x1C
    jump BOOT_ROM              0x1C      jump    0x04000000
Text:                                    Text = 0x20
    .ds "hello, world\r\n"     0x20      .dw     'h' 'e' 'l' 'l'
                               0x24      .dw     'o' ',' ' ' 'w'
                               0x28      .dw     'o' 'r' 'l' 'd'
                               0x2C      .dw     '\r' '\n' 0 0
```

- A label takes the address of the next statement, and adds no word itself.
- A constant is replaced by its number right away. That's why a constant is defined before it's used: `load32` is one or two instructions depending on its value, and pass 1 has to know how many to give the right addresses to everything after it.
- `addr2reg` is always two instructions, because the label's address may not be known yet. Its two halves are filled in by pass 2.
- `.ds` becomes a data word per 4 characters.

### Pass 2

Each statement becomes one word. For an instruction, its fields are put in their place in the word, as on the [instruction set](../cpu/isa.md#instruction-encoding) page:

```text
write 0xFFFF8000 r0 r2

  bits     31:28    27:12               11:8   7:4    3:0
  field    WRITE    constant            A      B      size
  value    1101     1000000000000000    0000   0010   0000
                                                        = 0xD8000020
```

The constant `0xFFFF8000` becomes `0x8000` in the field, which the CPU sign-extends back to `0xFFFF8000`.

Where a statement has a label, pass 2 puts in what the label means for that instruction (see [labels](assembly.md#labels)):

| Address | Statement | Label | Word |
| --- | --- | --- | --- |
| `0x00` | `load Text[15:0] r1` | `Text` is `0x20`, low half `0x0020` | `0x1C002011` |
| `0x04` | `loadhi Text[31:16] r1` | high half `0x0000` | `0x1D000011` |
| `0x0C` | `beq r2 r0 Done` | offset `0x1C` − `0x0C` = 16 | `0x60010200` |
| `0x18` | `jump Next` | offset `0x08` − `0x18` = −16, as `jumpo` | `0x9FFFFFE1` |
| `0x1C` | `jump 0x04000000` | a number, so the absolute `jump` | `0x98000000` |

The whole program is 12 words, 48 bytes.

Errors are found in the pass that knows enough: a wrong operand or a name defined twice in pass 1, an unknown label or a value that doesn't fit in pass 2.

## The source files

| File | What |
| --- | --- |
| `isa.py` | the instruction set as a table: every instruction type with its fields and their bits, and the codes of the ALU, multiply, branch, read and write operations |
| `assembler.py` | the two passes: reading the lines and operands, the pseudo-instructions, and the encoding of each statement |
| `cli.py` | the command: reads the file, assembles it, writes the image |

The tests are in `Toolchain/b32asm/tests`. The ISS (the instruction set simulator, which the CPU is tested against) decodes instructions with the same table in `isa.py`, so the assembler and the simulator can't disagree about the encoding.
