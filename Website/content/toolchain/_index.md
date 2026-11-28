---
title: Toolchain
weight: 7
---

The tools that turn source code into programs for the FPGC. They run on a PC.

| Page | For | What |
| --- | --- | --- |
| [Assembly language](assembly.md) | programming | the language of B32P4 programs: lines, operands, labels, data, constants, and how functions are called |
| [Assembler](assembler.md) | programming | `b32asm`: how to run it, its errors, and how it turns the language into words in two passes |
| [Loading programs](uartload.md) | programming | `uartload`: how a program gets onto the FPGC over the UART, through the [boot ROM](../boot.md) |

Each tool is its own Python project in `Toolchain/`, with a README and tests, and can be installed without the rest of the repository.
