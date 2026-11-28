---
title: Loading programs
weight: 3
reader: programming
---

`uartload` loads a program onto the FPGC over UART, and runs it. The program is a binary image from the [assembler](assembler.md), assembled for address 0. Its source is in `Toolchain/uartload`.

```text
b32asm hello.asm hello.bin
uartload hello.bin
```

| Option | What |
| --- | --- |
| `--port` | the FPGC's serial port, `/dev/ttyUSB0` by default |
| `--baud` | the UART's speed, 1,000,000 by default, as the FPGC's [UART](../io/uart.md) |

## What it does

1. It resets the FPGC, so it's back in the [boot ROM](../boot.md), by setting RTS without DTR for 10 ms (see [reset](../system.md#reset)). The boot ROM prints the error register, and then `boot: ready`.
2. It waits for `boot: ready`, at most 1 second.
3. It sends the load command: `L`, the length, the program, and its CRC-32.
4. It waits for the boot ROM's answer. With `boot: ok` and the right CRC, the program runs. Otherwise it starts again with a reset, up to 3 times in all, and then stops with an error.
5. It shows what the FPGC sends, until the program jumps back to the boot ROM (the next `boot: ready`), or until Ctrl-C.

```text
boot: err 00000000 pc 00000000
boot: ready
boot: ok 9afbeb43
hello, world
boot: err 00000000 pc 00000000
boot: ready
```

The protocol, with every byte and every answer, is on the [boot ROM](../boot.md#loading-a-program) page. At 1 Mbaud, a program loads at about 100,000 bytes per second.

## The source files

| File | What |
| --- | --- |
| `protocol.py` | the load command and the answers, without the serial port |
| `board.py` | the FPGC over its serial port: the reset, the load with its retries, and reading the lines it sends |
| `cli.py` | the command |

The tests are in `Toolchain/uartload/tests`, with a boot ROM that is faked on the PC.
