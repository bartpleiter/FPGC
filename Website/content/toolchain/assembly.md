---
title: Assembly language
weight: 1
reader: programming
---

Programs for B32P4 are written in its assembly language: a text file with one instruction or directive per line, which the assembler turns into a binary image of 32-bit words. This page describes the language around the instructions: lines, operands, labels, data and constants. What each instruction does is on the [instruction set](../cpu/isa.md) page.

## A first program

```text
; Prints a line over the UART, and returns to the boot ROM.

.equ UART_TRANSMIT 0xFFFF8000
.equ BOOT_ROM 0x04000000

    addr2reg Text r1            ; r1 = the address of the text
Next:
    readbu 0 r1 r2              ; r2 = the next character
    beq r2 r0 Done              ; a 0 ends the text
    write UART_TRANSMIT r0 r2
    add r1 1 r1
    jump Next
Done:
    jump BOOT_ROM

Text:
    .ds "hello, world\r\n"
```

- The two `.equ` lines give names to numbers: the UART's transmit register and the start of the boot ROM.
- `addr2reg` puts the address of the label `Text` in r1.
- The loop reads the text one byte at a time, and writes each character to the UART. `write UART_TRANSMIT r0 r2` writes r2 to the address r0 + `0xFFFF8000`, and r0 is always 0.
- At the end, the program jumps back to the boot ROM. The text is placed after the last instruction, where it's never run as code.

## Lines

| A line is | Example |
| --- | --- |
| empty, or only a comment | `; Prints a line` |
| a label | `Next:` |
| an instruction | `add r1 1 r1` |
| a directive | `.ds "hello"` |

- A comment starts with `;` and runs to the end of the line. It can follow an instruction or a directive. A `;` inside a string or a character is part of it.
- A label is alone on its line.
- Spaces and tabs separate the parts of a line, and indentation has no meaning. By convention, instructions and directives are indented by four spaces, and labels aren't.
- Mnemonics, directives and registers are lowercase. Names are case-sensitive: `Loop` and `loop` are two different labels.

## Operands

| Operand | Written as | Examples |
| --- | --- | --- |
| register | `r0` to `r15` | `r1`, `r15` |
| number | decimal, hex (`0x`) or binary (`0b`), optionally negative, with `_` allowed between digits | `42`, `-1`, `0x1F`, `0xFFFF_8000`, `0b1010_0000` |
| character | one ASCII character in single quotes, which stands for its number | `'L'`, `' '`, `'\n'` |
| name | a label or a constant: letters, digits and `_`, not starting with a digit | `Next`, `UART_TRANSMIT` |
| string | ASCII in double quotes, only after `.ds` | `"hello, world\r\n"` |

- There is no octal. A decimal number with a leading 0 is still decimal: `010` is ten.
- A number must fit in its field of the instruction, or the assembler stops with an error. A constant that the CPU sign-extends may also be written as the 32-bit value it becomes, so `0xFFFF8000` can be written for `-32768` (see [memory instructions](../cpu/isa.md#memory-instructions)).

Strings and characters have the same escapes:

| Escape | Character | Number |
| --- | --- | --- |
| `\r` | carriage return | 13 |
| `\n` | line feed | 10 |
| `\t` | tab | 9 |
| `\0` | the zero byte | 0 |
| `\\` | `\` | 92 |
| `\"` | `"` | 34 |
| `\'` | `'` | 39 |

## The order of operands

Operands are separated by spaces, without commas. The sources come first, and the destination register last:

| Instructions | Form | Example | Means |
| --- | --- | --- | --- |
| ALU, multiply and divide | `op A B D`, `op A constant D` | `add r1 r2 r3` | r3 = r1 + r2 |
| constants | `load constant D` | `load 'L' r2` | r2 = 76 |
| reads | `read offset base D` | `read 8 r1 r2` | r2 = the word at r1 + 8 |
| writes | `write offset base data` | `write 4 r1 r2` | the word at r1 + 4 = r2 |
| branches | `beq A B target` | `beq r1 r0 Done` | go to `Done` if r1 = 0 |
| jumps | `jump target`, `jumpr offset B` | `jumpr 0 r15` | go to the address in r15 |
| the hardware stack | `push B`, `pop D` | `push r15` | |

The instruction set page has every instruction with its operands.

## Labels

A label, `Name:`, gives a name to the address of what follows it: the next instruction or data word. A label can be used before the line that defines it.

How an instruction uses a label:

| Written | Becomes | Reaches |
| --- | --- | --- |
| `jump Label` | `jumpo`, with the offset from the jump to the label | ±64 MiB |
| `beq A B Label` and the other branches | the offset from the branch to the label | ±32 KiB |
| `savpc Label D` | the offset from the `savpc`, so D gets the label's address | ±32 KiB |
| `addr2reg Label D` | the label's full address, in two instructions | everywhere |
| `.dw Label` | the label's full address, as a data word | everywhere |

- A jump to a number is the absolute `jump`: `jump 0x04000000` goes to that address.
- Jumps and branches to labels are relative, so they don't depend on where the program is placed. Only `addr2reg` and `.dw` hold addresses that change with the [base address](#the-output).

## Pseudo-instructions

Instructions that the assembler turns into others:

| Written | Becomes |
| --- | --- |
| `load32 value D` | `load` with the low 16 bits, and `loadhi` with the high 16 bits if they aren't 0. Any 32-bit number, also negative. |
| `addr2reg Label D` | `load` and `loadhi` with the two halves of the label's address |
| `nop` | `or r0 r0 r0`, the word 0 |
| `savpc D` | `savpc 0 D` |

## Data

`.dw` puts words into the program, at the place where it's written. Each value is a number, a character, a constant or a label:

```text
Table:
    .dw 1 -1 0xFFFF_0000 'A' Table
```

`.ds` puts a string: four characters per word, the first one in the lowest byte. The string ends with a zero byte, and is padded with zero bytes to a whole word, so what follows starts at a word again:

```text
Text:
    .ds "hello!"

address    byte 0   byte 1   byte 2   byte 3
Text       'h'      'e'      'l'      'l'
Text+4     'o'      '!'      0        0
```

The first 0 ends the string, the second is padding. A program reads the string one byte at a time with `readbu`, as in the first program.

Data sits between the code, exactly where it's written. The CPU would run it as instructions if it got there, so data goes after the last instruction, or behind a jump.

## Constants

`.equ NAME value` gives a name to a number. The value is a number, a character or an earlier constant. The name can then be used everywhere a number can:

```text
.equ UART_TRANSMIT 0xFFFF8000
.equ NEWLINE '\n'

    load NEWLINE r1
    write UART_TRANSMIT r0 r1
```

- A constant is defined before it's used.
- A name is either a label or a constant, and is defined once.
- Constants are most useful for the addresses of the [device registers](../memory-map.md), which every program that uses a device needs.

## The output

The assembler turns one source file into one binary image:

- every instruction and data word, in the order of the source, 4 bytes each, the lowest byte first
- the first word at the base address, which sets the address of every label

Programs that the boot ROM loads run from address 0, the default base. The boot ROM itself is assembled for `0x04000000`. What a loaded program can expect when it starts is on the [boot ROM](../boot.md) page.

There are no sections, and a program is one file. Programs of several files will be possible with a linker later.

## Functions

The CPU has no call instruction. A function is called with two instructions, and returns with a jump to the saved address. The convention for hand-written assembly is r15 for the return address:

```text
    savpc 8 r15                 ; r15 = the address after the jump
    jump PrintText
    ...                         ; PrintText returns here

PrintText:
    ...
    jumpr 0 r15                 ; back to the caller
```

`savpc 8 r15` puts its own address plus 8 into r15: the address of the instruction after the `jump`.

A function that calls another function first saves its own return address on the hardware stack, and takes it back before it returns:

```text
PrintLine:
    push r15
    savpc 8 r15
    jump PrintText
    ...
    pop r15
    jumpr 0 r15
```

Suggestions for the other registers:

- Write above each function which registers it takes its inputs in, gives its result in, and changes. The caller then knows what to save.
- Save registers with `push` and `pop`. The hardware stack has 256 words, and is separate from memory, so it's fast but small: enough for return addresses and saved registers, not for large data or deep recursion.
- An interrupt handler must keep every register it uses, because the interrupted code doesn't know it ran (see the [programming notes](../cpu/programming.md#interrupts)).

The full convention of which registers hold arguments and results, and which ones a function must keep, will be set with the C compiler.
