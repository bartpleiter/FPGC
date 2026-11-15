---
title: History
weight: 7
---

B32P4 is the sixth CPU I designed for the FPGC. Each one was designed from scratch or rebuilt from the one before it, based on what I learned from the previous design. The story of B32P4 itself is in the [CPU blog series](../blog/cpu-general-design/index.md).

## B32P4 (current)

B32P3 had reached its limits, so B32P4 is a rewrite, as part of the rewrite of the whole FPGC. The goal was to keep the good parts of B32P3, and to keep the instruction set mostly compatible, but to optimize every part from the ground up which also allowed me to write a blog post series about this project. That led to quite a few design differences:

- A five-stage pipeline with better thought out hazard handling, a single stall rule, and every side effect in MEM or WB.
- Cache hits handled by the pipeline itself, so a hit costs nothing, with early restart for misses.
- A pipelined multiplier, which can start a multiply every cycle.
- Proper CPU internal registers, like an error register for illegal instructions and accesses, and an interrupt enable register.
- One bus for everything outside SDRAM.
- Removal of components that were not worth the complexity to keep, like the FP64 coprocessor.

## B32P3

The third pipelined design, mainly a redesign of the pipeline of B32P2 with the same instruction set. Back to a classic five-stage pipeline (IF, ID, EX, MEM, WB), with the critical paths broken up to reach 100 MHz on the Cyclone IV for the first time. It was also the first CPU that was byte-addressable instead of word-addressable.

B32P3 was a bit of an experiment, to see how far the project could go. It eventually ran Doom, allowed me to test its performance limits, and it got features like DMA and a math coprocessor with its own registers, all mainly to get a first experience with these kinds of hardware extensions so a redesign could implement them properly. At some point the limits of the design was reached and timing problems became too severe to continue.

## B32P2

The second pipelined design, focused on making SDRAM faster to reach. It had a six-stage pipeline (FE1, FE2, REG, EXMEM1, EXMEM2, WB), L1 instruction and data caches with 8-word bursts from SDRAM, and a multi-cycle ALU for multiply, divide and fixed-point operations, instead of the memory-mapped ones before it. Most of the memory side ran at 100 MHz, but the CPU itself only ran at 50 MHz as higher clock speeds did not fit within timing, mostly because its hazard handling grew with every bug that was found.

## B32P1

The first pipelined CPU, inspired by the classic MIPS design. It added the `slt` instruction for the C compiler. Every SDRAM access stalled the pipeline, and an L1 cache didn't fit the design, so an L2 cache behind the SDRAM controller was used as a workaround.

## B322

The second CPU, introduced with FPGC4. An extended and refactored version of B32 with more interrupts. All memory was still reached through the Memory Unit. It was the first CPU to run BDOS and simple C programs, compiled with a modified Python-based C compiler.

## B32

The first proper CPU of the project, introduced with FPGC3, and the first one I designed with the help of simulation. It ran on the FPGA, together with the first working SDRAM controller, and handled one stage of an instruction per clock cycle.
