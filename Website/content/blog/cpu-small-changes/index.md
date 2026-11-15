---
title: CPU part 17 - Small changes before going to hardware
date: 2026-11-15T01:00:00
authors:
  - name: Bart Pleiter
    link: https://github.com/bartpleiter
tags: [CPU]
---

With the caches done, the CPU is almost complete, and before some final steps and testing it on hardware I wanted to make several small changes after a review of the current progress. That is what this post is about, including some interesting timing and placement analysis on the register file.

<!--more-->

## `savpc` with an offset

`savpc` saves the PC in a register, which is how a function call gets its return address. Until now it saved the address of the `savpc` itself, so a call took three instructions: the return address is the instruction after the jump, two instructions further.

```text
savpc r15           ; r15 = the address of this savpc
add r15 8 r15       ; r15 = the address of the instruction after the jump
jump Function
```

Now `savpc` has a 16-bit constant that it adds to the PC, so the add is no longer needed:

```text
savpc 8 r15         ; r15 = the address of this savpc + 8
jump Function
```

Calls like this are everywhere in compiled C code (and also assembly if you were to write a non-trivial program), so this saves an instruction and a cycle in a lot of places. In the hardware, it is almost free as several other instructions already use a constant. The constant goes in the same bits as the constant of `jumpr`, and `savpc r15` without a constant still works, as `savpc 0 r15`.

## Turning interrupts off and on

Interrupts used to be always enabled, except during the interrupt handler itself or during bootup in ROM. This worked fine since but has two downsides: The interrupt hardware has to check the program counter address each time to validate we are not executing in ROM, and more importantly this does not allow for execution of critical code that should not be interrupted. To improve on this, the CPU now has an internal register `INT_ENABLE` that controls whether interrupts are enabled, and software can read and write this register as needed.

After a reset the interrupts are disabled, and the bootloader now has to enable them right before it jumps to the program it just copied to SDRAM. The check on the address is gone, so an interrupt can now be taken anywhere, also in the ROM (for which I see no use case as the handler is in SDRAM).

There is one catch, which comes from the pipeline: the CPU decides whether to take an interrupt while the jump is in EX, and the write to `INT_ENABLE` only happens a cycle later, when the write leaves MEM. So a jump right behind the write still decides with the old value:

```text
cycle               0   1   2   3
write 16 r10 r0     EX  MEM WB          ; disables interrupts at the end of cycle 1
jump Next           ID  EX  MEM         ; decides in cycle 1, while they are still enabled
```

I could fix this extremely rare case in hardware, for example by letting the write jump to the next instruction, so that the `jump Next` is fetched again. But that is extra logic for a case that only matters when a jump or branch comes right after the write. So the rule is simply that the instruction right after a write to `INT_ENABLE` may still use the old value. Code that really must not be interrupted puts another instruction between the write and its first jump. Otherwise, an interrupt could still be taken there, and the `reti` at the end of the handler would enable interrupts again. If I ever need to use code that must disable interrupts, I'll have to make sure to not forget this, although I cannot imagine that the C compiler would place a jump or branch directly after the write without other instructions in between.

## The error register survives a reset

The error register records things like illegal instructions and reads from addresses where nothing is mapped. Until now, a reset cleared it. That sounds logical, but requires you to do error handling after a crash (which honestly should be possible, just like a blue screen in Windows). I think it is easier to check the error after a reset during boot.

So now a reset leaves the error register alone. It is only cleared during boot from power-up, and when a program writes to `ERR_STATUS`. The plan is that the bootloader shows the error register at every start, but we'll have to see what I come up with.

## Registers are undefined after a reset

Until now, a reset set all 15 registers to 0. A program does not need that, as it always writes a register before it reads it anyway. Like I mentioned in the early post on the register file, the register file is built from flip-flops and not from block RAM, so the reset itself was cheap. But removing it had an interesting side effect: Quartus moved one of the two read ports of the register file into a small block RAM, all by itself.

So now only `r0` is 0 after a reset, and the other registers keep whatever they had. A program that reads a register before writing it is now wrong, just like reading an uninitialized variable in C. The stack pointer, program counter backup, and the interrupt state are still reset.

### How half of the register file ended up in block RAM

A block RAM on the FPGA always reads with a clock edge in between: the address goes into a register inside the block RAM at the edge, and the word comes out after it. The register file reads without a clock edge: the register numbers come from the instruction in the IF/ID pipeline register, and the values come out in the same cycle. So at first sight, they do not fit together.

But the register numbers are bits of the IF/ID register, and that register is itself loaded at a clock edge. If a register number is a plain copy of some of those bits, Quartus can move those flip-flops into the block RAM, as its address register. The block RAM then reads at the edge where the instruction enters ID, and the word is there during the ID cycle, exactly as before. Nothing changes for the rest of the CPU. That is what Quartus did for port B, whose register number is always bits 7 to 4 of the instruction.

For this to work, a few things have to be true:

- The registers have no reset. The contents of a block RAM cannot be cleared in one go, so a memory with a reset has to be flip-flops.
- The register number is a plain copy of register bits. Port A's register number is bits 11 to 8 for most instructions, but bits 7 to 4 for `arithc`, `arithmc` and `jumpr`, where the constant takes the other bits. So it comes out of a multiplexer that depends on the opcode, and a multiplexer cannot be moved into a block RAM.
- A read and a write of the same register in the same cycle must still give the new value. A block RAM gives the old one, so Quartus adds a small bypass next to it: it remembers the last write and its register number, and uses that value when the numbers match.

So now port B reads a copy of the registers in a block RAM, port A still reads the flip-flops, and every write goes into both.

### An experiment: all of it in block RAM

This made me curious whether the whole register file could go into block RAM, and whether that would make the CPU smaller or faster (timing wise, not instructions per cycle!). So I compiled a few versions of the register file, each with ten different seeds (Quartus places the design starting from a random seed, and there is quite some run variance). Some background: with the CPU almost done there is very little timing headroom left.

| Register file | Logic elements | Flip-flops | Block RAMs | Timing |
| --- | --- | --- | --- | --- |
| 1. Flip-flops with a reset (the old design) | 521 | 480 | 0 | baseline |
| 2. No reset, Quartus decides (the current design) | 667 | 553 | 1 | a bit better than 1 |
| 3. No reset, forced to flip-flops | 783 | 512 | 0 | about the same as 1 |
| 4. No reset, the A register always in bits 11 to 8, Quartus decides | 658 | 553 | 1 | the same as 2 |
| 5. Both read ports written as block RAMs | 88 | 66 | 2 | fails |

Version 4 tests the obvious fix: give every instruction its A register in the same bits (I simply broke `arithc`, `arithmc` and `jumpr` for this compile). It changed nothing: Quartus still put only port B in a block RAM. The reason is that a block RAM has one read port next to its write port. Two read ports need two copies of the registers, and Quartus only makes one block RAM out of one memory in the code. I even tried writing two copies in the code, but both copies always get exactly the same writes, so Quartus noticed they were the same and merged them back into one.

Only version 5 put both ports in block RAMs. There, I wrote the register file as two memories that each read with a clock edge, with the register numbers taken from the instruction as it comes out of fetch, a cycle before ID (with a second decoder, just for those). That is how I did it in the previous FPGC version (B32P3 CPU). The register file shrinks to almost nothing, and the whole CPU gets about 15% smaller. But it fails timing on every seed, and the failing paths say why.

A block RAM that reads at a clock edge has to know whether ID stalls in that cycle: during a stall it must read the same register again, and its bypass must keep track of writes during the stall. The stall is decided late in the cycle, because it depends on whether the data cache hits. That stall decision now goes into the register file too, on top of one of the longest paths in the CPU. This is exactly the extra logic around the register file for stalls that I wanted to get rid of from the previous FPGC version. The flip-flop register file does not care about stalls at all: a stalled ID simply reads it again in the next cycle.

The other differences are small. Version 4 is functionally almost the same as version 2, yet its timing moved about as much as the differences between versions 1 to 3. So those are mostly the fitter placing things differently. Also, Quartus's choice did not make the register file smaller: the flip-flops for port A stay, and the bypass is extra. Forcing everything into flip-flops without the reset used even more logic, which I did not expect (and cannot explain, Quartus is a magical black box in that sense, but somehow it manages to get to the best decisions mostly).

So, the conclusion: dropping the reset and letting Quartus decide is at least as good as before, and keeps the register file simple. Putting the whole register file in block RAM would save a lot of logic, but only by bringing back the stall handling that made B32P3 hard to get right and to get fast. If logic ever runs out on the FPGA, this would be a place to look, but it is not worth it now.

## A bigger data cache

The data cache had 128 lines of 32 bytes, so 4 KiB. It now has 512 lines, so 16 KiB, which takes 12 more of the 126 block RAMs of the FPGA. The instruction cache stays at 128 lines. The number of lines is a parameter, so this was a one-line change, but the reason for 512 is a nice example of a direct-mapped cache at work.

Take Doom, which I will run on the FPGC again when we get there. It draws its walls column by column, into a screen buffer of 320 × 200 pixels in SDRAM, with one byte per pixel. Our memory cache works on a row-by-row basis instead of column-by-column. Two pixels above each other are 320 bytes apart, which is exactly 10 lines of the cache. So a column of 200 pixels touches 200 different lines, and the next column, one byte to the right, touches the same 200 lines again. Ideally, all 200 stay in the cache in between.

Remember that the index of a line is its address divided by 32, modulo the number of lines. Going down one pixel adds 10 to the line number, so the index of each pixel in a column is 10 further than the one above:

| Lines in the cache | Indexes used by a column | Fits 200 lines? |
| --- | --- | --- |
| 128 | 0, 10, 20, ... 120, 2, 12, ...: only the even ones, so 64 | no |
| 256 | the even ones, so 128 | no |
| 512 | the even ones, so 256 | yes |

With 128 lines, pixel 64 of a column lands on the same index as pixel 0, and throws its line out. That line is dirty, as the pixel was just written, so it goes back to SDRAM first. By the time the next column comes along, none of its lines are left, and nearly every pixel is a miss with a write-back. With 512 lines, the whole column fits, and the next 31 columns hit.

I did not analyze the cache access pattern in the previous version of FPGC for Doom, so I hope this change will improve the performance by a lot. Fun fact: the intel 486 did not have this problem as it wrote directly to video memory (would be interesting to explore if I could do something similar with the FPGC).

## Next steps

The next step is the rest of the memory map in MEM: reading the ROM as data, and the I/O devices. We're getting close to being able to test on actual hardware! For that, we need to have an SDRAM controller, UART IO device, and a simple program in ROM.
