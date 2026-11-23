# b32asm

The assembler for B32P4, the CPU of the [FPGC](https://fpgc.bartpleiter.com). It turns one assembly file into a binary image: 32-bit words, lowest byte first.

```bash
uv tool install Toolchain/b32asm    # or use it from the FPGC repository's venv
b32asm program.asm program.bin
b32asm boot.asm boot.bin --base 0x04000000
```

`--base` is the address of the first word, 0 by default: the start of SDRAM, where programs run.

The instructions are described on the website's [instruction set](https://fpgc.bartpleiter.com/cpu/isa/) page. Besides them, the assembler has:

```text
; a comment, up to the end of the line
Name:                         ; a label: the address of what follows
.dw 1 0x20 -3                 ; data words
.ds "hello\r\n"               ; a string, 4 characters per word, ended by a 0
.equ UART_TRANSMIT 0xFFFF8000 ; a name for a number, defined before it's used
load32 0x12345678 r1          ; any 32-bit number into a register
addr2reg Name r1              ; the address of a label into a register
```

Run the tests with `pytest Toolchain/b32asm`.
