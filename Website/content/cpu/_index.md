---
title: CPU (B32P4)
weight: 1
---

B32P4 (Bart's 32-bit Pipelined processor, version 4) is the CPU of the FPGC. It's a 32-bit RISC CPU with a five-stage pipeline, designed from scratch, and runs at 100 MHz on a Cyclone IV EP4CE40 FPGA.

The goal of the CPU is not to make a competitor to RISC-V or ARM. Instead, the goal is to create a good CPU that is designed by a single person from scratch without using existing components, meaning it has to be simple enough to be understood and implemented by one person. Still, it needs to be fast enough to be able to run Doom at a playable frame rate with C compiler overhead.

The story of how it was designed, step by step, is in the [CPU blog series](../blog/cpu-general-design/index.md).

## Features

| Feature | B32P4 |
| --- | --- |
| Instruction set | 32-bit RISC, fixed 32-bit instructions, 16 opcodes (see [Instruction set](isa.md)) |
| Registers | 16 registers of 32 bits, `r0` is always 0 |
| Memory | byte-addressed and little-endian, reads and writes of 8, 16 and 32 bits |
| Pipeline | five stages (IF, ID, EX, MEM, WB), at best one instruction per cycle |
| Clock | 100 MHz, one clock |
| Caches | an instruction cache (L1I) of 4 KiB and a data cache (L1D) of 16 KiB, direct-mapped, write-back, with lines of 8 words |
| Multiply and divide | a pipelined multiplier that can start a multiply every cycle, and a divider that computes one bit per cycle, so a divide takes 34 cycles (50 for `divfp`) |
| Fixed-point | Q16.16 multiply and divide instructions |
| Hardware stack | 256 words, for `push` and `pop` |
| Interrupts | a configurable number of inputs, taken at jumps and branches |
| Error register | records illegal instructions and accesses, and keeps its value during a reset |

B32P4 is a one-person learning project, not a competitor for RISC-V or ARM. So it leaves out what the FPGC doesn't need:

- No branch prediction. The CPU always continues with the next instruction, and a taken branch throws away the instructions behind it.
- No floating point. Fixed-point instructions are used instead.
- No privilege levels, memory protection or MMU. There is one user, and every program can access everything.
- No superscalar or out-of-order execution. One instruction enters the pipeline per cycle, in program order.

## Block diagram

```text
                                        interrupts
                                             v
            +----+    +----+    +----+    +-----+    +----+
ROM --------| IF |--->| ID |--->| EX |--->| MEM |--->| WB |
            +----+    +----+    +----+    +-----+    +----+
               |                          |    |
            +-----+                    +-----+ |
            | L1I |                    | L1D | +---- bus
            +-----+                    +-----+
               |                          |
            +--------------------------------+
            |        cache controller        |
            +--------------------------------+
                             |
                      SDRAM line port
```

| Part | What it does |
| --- | --- |
| IF (fetch) | holds the program counter, and reads the instruction from the ROM or the L1I |
| ID (decode) | decodes the instruction, reads its registers, and computes jump and branch targets |
| EX (execute) | the ALU, the branch compare, the memory address, multiply and divide, and the decision to take an interrupt |
| MEM (memory) | reads and writes memory, picks the result, and makes the instruction's changes final. Jumps, taken branches and interrupts continue at their target from here. |
| WB (write back) | writes the result into the register file |
| L1I, L1D | the instruction and data caches for SDRAM. A hit costs no extra cycles. |
| cache controller | handles misses: it writes a changed line back to SDRAM, and reads the missing line |
| ROM | the boot ROM, read directly by IF. The CPU starts here after a reset. |
| SDRAM line port | the connection to the SDRAM controller, which transfers whole cache lines |
| bus | the connection for every other access: data reads from the ROM, and the I/O devices |
| interrupts | the interrupt inputs |

## Pages

Each page says at the top who it's written for: programmers, hardware designers (who connect something to the CPU), or RTL readers (who read or change the Verilog).

| Page | For | What it answers |
| --- | --- | --- |
| [Instruction set](isa.md) | programmers | what every instruction does, and how it's encoded |
| [Programming notes](programming.md) | programmers | what instructions cost in cycles, and what software must do for caches, interrupts and reset |
| [Interface](interface.md) | hardware designers | the ports of the CPU and how to connect a ROM, an SDRAM controller and devices to them |
| [Pipeline](pipeline.md) | RTL readers | how instructions move through the stages, and how stalls, flushes and forwarding work |
| [Caches](caches.md) | RTL readers | how the L1I, the L1D and the cache controller work |
| [RTL structure](rtl.md) | RTL readers | the Verilog files and modules, and the rules that keep the design at 100 MHz |
| [History](history.md) | everyone | the CPUs before B32P4 |

The address space of the whole system, including the regions that the CPU reaches over the bus, is on the [memory map](../memory-map.md) page.

## Parameters

| Parameter | Default | Meaning |
| --- | --- | --- |
| `INTERRUPTS` | 2 | the number of interrupt inputs, at most 255 |
| `L1I_LINES` | 128 | the number of L1I lines, a power of two. A line is 32 bytes, so the default is 4 KiB. |
| `L1D_LINES` | 512 | the number of L1D lines, a power of two. The default is 16 KiB. |

The FPGC uses the default cache sizes. Smaller caches are only used in tests, because they cause more misses.

## Terms

The other CPU pages use these words without explaining them again.

| Term | Meaning |
| --- | --- |
| areg, breg, dreg | the two source registers and the destination register of an instruction |
| bubble | an empty slot in the pipeline: a pipeline register whose valid bit is 0 |
| bus | the CPU's connection for every access that isn't to SDRAM or to the CPU itself |
| busy | a stage that needs more cycles for its instruction. A busy stage stalls, and so do the stages before it. |
| cache line | 8 words (32 bytes) at an address that is a multiple of 32: what a cache stores and what a miss transfers |
| commit | the moment an instruction changes the state of the machine. This happens in MEM, except for the register write, which happens in WB. |
| constant | a value that is part of the instruction itself (`c16` or `c27` on the ISA page) |
| CPU-internal I/O registers | the registers inside the CPU at `0xFFFFFF00`, such as `PC_BACKUP`, `SP` and the error register |
| current | a cache output that belongs to the instruction in the stage right now: it was read at that instruction's address, and the cache controller hasn't changed that cache since. Only a current output is used. |
| dirty | a cache line that was written, and must be written back to SDRAM before it's replaced |
| early restart | taking the needed word from SDRAM the moment it arrives, before the rest of the line |
| flush | throwing away the instructions in the earlier stages, by setting their valid bits to 0 |
| forwarding | giving an instruction a result that isn't in the register file yet |
| hit, miss | whether the cache holds the line that an access needs |
| late result | a result that only exists at the end of MEM: reads, pops, multiplies, divides and `intid` |
| late-result stall | the one-cycle wait in ID of an instruction that needs the late result of the instruction directly ahead of it |
| line port | the connection between the cache controller and the SDRAM controller, which transfers one cache line at a time |
| pipeline register | the registers between two stages that hold an instruction and what's known about it so far (IF/ID, ID/EX, EX/MEM and MEM/WB) |
| push-then-pop stall | the one-cycle wait of a `pop` directly behind a `push` |
| redirect | continuing at another address than the next instruction: a jump, a taken branch, `halt`, an interrupt, `reti` or `ccache` |
| retire | an instruction finishing. For simulation, the CPU reports every retired instruction on the retire port. |
| stall | a stage keeping its instruction for another cycle |
| valid bit | the bit in each pipeline register that says whether it holds an instruction |
| write-then-access stall | the one-cycle wait of a read or write directly behind a write |
