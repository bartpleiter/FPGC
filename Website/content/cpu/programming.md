---
title: Programming notes
weight: 2
reader: programming
---

What software needs to know about B32P4 beyond the [instruction set](isa.md): what instructions cost, and what software has to do itself for the caches, interrupts and reset.

The pipeline handles every hazard in hardware, so a program is always correct, whatever the order of its instructions. The order only changes the speed.

## What instructions cost

At best, B32P4 finishes one instruction per cycle. This table lists the cycles that come on top of that. Everything that isn't in the table costs nothing extra.

| Situation | Extra cycles |
| --- | --- |
| an ALU instruction, `savpc`, a branch that isn't taken | 0 |
| a result used by the next instruction (forwarding) | 0 |
| a multiply | 0 (a new multiply can start every cycle) |
| the next instruction uses the result of a read, `pop`, multiply, divide or `readintid` | 1 |
| a taken branch, `jump`, `jumpr`, `halt` or `reti` | 3 |
| a divide or modulo | 33, so 34 cycles in total (49 for `divfp`) |
| a read or write of SDRAM that hits the L1D | 0 |
| a read or write directly behind a write | 1 |
| a `pop` directly behind a `push` | 1 |
| a read of a CPU-internal I/O register | 1 |
| a write to a CPU-internal I/O register | 0 |
| a miss in the L1I or the L1D | see [Memory](#memory) below |
| a data read from the ROM, or an access to an I/O device (the bus) | see [Memory](#memory) below |
| `ccache` or `ccached` | see [Caches](#caches) below |

The costs add up. For example, a read of a CPU-internal register directly behind a write costs 2 extra cycles.

### A read followed by a use

A read's result only exists at the end of the MEM stage, so an instruction that needs it right away waits one cycle:

```text
cycle            1    2    3    4    5    6    7
read 0 r2 r1     IF   ID   EX   MEM  WB
add r1 r3 r4          IF   ID   ID   EX   MEM  WB      waits once
```

One instruction in between removes the wait:

```text
read 0 r2 r1     IF   ID   EX   MEM  WB
add r5 r6 r7          IF   ID   EX   MEM  WB
add r1 r3 r4               IF   ID   EX   MEM  WB      no wait
```

The same goes for `pop`, multiplies, divides and `readintid`.

### Jumps and branches

The CPU always fetches the next instruction. When a branch is taken, or for any jump, the three instructions behind it are thrown away, so a taken branch takes 4 cycles in total. A branch that isn't taken costs nothing.

- In a loop, the branch back is taken on every iteration. Unrolling a short loop saves those cycles.
- For an `if`, put the common case where the branch isn't taken.

## Memory

### SDRAM and the caches

SDRAM is reached through the caches. A hit costs nothing. A miss waits for the line of 8 words to come from SDRAM. These costs depend on the SDRAM controller, so they're given with two numbers of the controller:

- `L`: the cycles from a line request until the first word arrives
- `W`: the cycles that a write of a line keeps the SDRAM controller busy before it can start the next request

| Situation | Extra cycles | With `L` = 6 and `W` = 14 |
| --- | --- | --- |
| a miss | `L` + 2 | 8 |
| a miss that first writes back a changed line | `W` + `L` + 2 | 22 |
| a read of the next word of the line that just missed | 0 | 0 |

The example numbers are those of the SDRAM controller of the previous FPGC version. They'll be replaced by those of the new SDRAM controller once it exists.

- The line comes in order, starting at the word that was needed. So straight-line code that runs into a new line continues one instruction per cycle as the words arrive, and so does a read of the next word.
- A read or write miss works the same way. A write miss reads the line first, and then writes into it.
- Lines are 32 bytes. Reading a large array in order costs one miss per 8 words.
- The caches are direct-mapped. Two addresses that are a multiple of the cache size apart (4 KiB for the L1I, 16 KiB for the L1D) use the same place in the cache, so a loop that alternates between them misses every time.

### The bus

Everything outside SDRAM and the CPU itself goes over the bus: reads of data from the ROM, and the I/O devices. A bus access costs `D` + 2 extra cycles, where `D` is the number of cycles the device needs to answer (at least 1).

| Device | Extra cycles |
| --- | --- |
| a device that answers in the cycle after the request | 3 |
| a device that needs `D` cycles | `D` + 2 |
| a data read from the ROM | 4 |

So for data that's used a lot, SDRAM is faster than the ROM, because the cache makes most of its reads free.

## Caches

The CPU doesn't keep the caches up to date with changes that it doesn't make through the L1D. Software does that with two instructions:

| Instruction | What it does | When it's needed |
| --- | --- | --- |
| `ccache` | writes every changed L1D line back to SDRAM, then empties the L1I and the L1D | after writing code to SDRAM that will be run: a loader, or self-modifying code |
| `ccached` | the same, but empties only the L1D | when something else than the CPU reads or writes SDRAM (DMA, later) |

- To load code, write it as data, then `ccache`, then jump to it. Without the `ccache`, the new code may still be only in the L1D, and the L1I may still hold the old code.
- Use `ccached` before a DMA transfer reads data that the CPU wrote, so the data is in SDRAM. And `ccached` after a DMA transfer wrote data that the CPU will read, so the CPU doesn't read old copies from the L1D.
- Code in ROM isn't cached, and needs nothing.

Both instructions go through every line, which takes about 2 cycles per line, plus `W` for each changed line that's written back. With the default cache sizes that's about 1,030 cycles plus the write-backs. So they're for loading programs and for DMA, not for inner loops.

## Interrupts

Interrupts are disabled after a reset. Software enables them by writing 1 to `INT_ENABLE` (`0xFFFFFF10`), for example with `write 0xFFFFFF10 r0 r1`.

- The handler starts at address `0x4`, the second word of SDRAM. So a program usually has a `jump` to its start at `0x0`, and a `jump` to its handler at `0x4`.
- The bootloader enables interrupts right before it jumps to the program at `0x0`, because only then is there a handler at `0x4`.
- An interrupt is taken at a jump, `jumpr`, taken branch or `halt`, in place of that instruction. It costs the same as that jump: 3 extra cycles. A long stretch of code without a taken branch or jump delays the interrupt until the next one. To wait for an interrupt, use `halt`.
- The CPU only saves the PC (in `PC_BACKUP`), and disables interrupts. The handler must save every register it changes, for example with `push` and `pop`.
- `readintid D` gives the number of the interrupt. `reti` jumps back to the interrupted jump or branch, which then runs as if nothing happened, and enables interrupts again.
- Interrupts don't nest. An interrupt that arrives during the handler waits until after `reti`.
- Writing `PC_BACKUP` in the handler makes `reti` continue somewhere else, for example in another task.
- The instruction right after a write to `INT_ENABLE` may still use the old value. If that instruction is a jump or a taken branch, an interrupt can still be taken there right after interrupts were disabled (and `reti` enables them again), or not yet right after they were enabled. Code that must not be interrupted puts another instruction between the write and the first jump or branch.

## Reset

After a reset, the CPU starts at `0x04000000` in the ROM. The full list of what a reset does is on the [instruction set](isa.md#reset) page. For software, these points matter:

- Registers `r1`–`r15` are undefined. A program must write a register before it reads it.
- The caches are emptied in the first 512 cycles (with the default cache sizes), while the CPU runs from the ROM. An access to SDRAM in that time waits until it's done.
- The contents of SDRAM are undefined. Changed lines that were still in the L1D are not written back, so after a reset SDRAM may miss the last writes of the program that ran before.
- The error register keeps its value. A bootloader can print it at every start to see what went wrong before the reset, and then clear it by writing `ERR_STATUS`.

## The hardware stack

The stack holds 256 words, and is separate from memory.

- It's meant for saving registers, in functions and interrupt handlers. Larger data belongs in memory.
- The stack pointer wraps around. A `push` at 255 or a `pop` at 0 sets a flag in the error register, but the CPU continues.
- A `pop` directly behind a `push` waits one cycle. A `pop` behind a `pop` doesn't.
