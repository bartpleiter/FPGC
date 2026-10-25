---
title: CPU part 10 - Stack and Error register
date: 2026-10-25T01:00:00
authors:
  - name: Bart Pleiter
    link: https://github.com/bartpleiter
tags: [CPU]
---

In this post the CPU gets its hardware stack for `push` and `pop`, and the CPU-internal I/O registers. One of these registers is new in B32P4: the error register, which tells you when a program did something it should not have done.

<!--more-->

## The hardware stack

The FPGC has a stack of 256 words that is separate from the rest of memory. It lives in a single block RAM inside the CPU, so a `push` or `pop` never has to go through the caches or wait for the SDRAM. It is meant for quickly saving registers, for example at the start of a function or an interrupt handler.

The stack pointer (SP) always points at the first free entry:

| Instruction | What it does |
| --- | --- |
| `push r1` | writes `r1` at SP, then SP goes up by one |
| `pop r1` | SP goes down by one, then reads the word there into `r1` |

SP is 8 bits, so it wraps around: a `push` at 255 continues at 0, and a `pop` at 0 continues at 255. Of course, this means you overwrite or read garbage, which is why both cases are reported in the error register (more on that below).

## Reading the stack in the pipeline

The stack is a block RAM, so like the data memory from the previous post, it needs its address one clock edge before it gives the word. A `pop` gets its word in MEM, so the stack gets the address at the clock edge where the `pop` moves from EX to MEM. The tricky part is that SP is not always up to date at that moment. Take two pops in a row, with two words on the stack (so SP is 2):

```text
cycle       0   1   2   3   4   5
pop r1      IF  ID  EX  MEM WB          ; should get the word at 1
pop r2          IF  ID  EX  MEM WB      ; should get the word at 0
```

In cycle 3, `pop r2` is in EX, so at the end of that cycle the stack has to get the address of the word `pop r2` needs: 0. But in that same cycle, `pop r1` is in MEM, and SP is still 2. It only becomes 1 at the end of cycle 3, when `pop r1` is done. If the stack just read the word below SP, `pop r2` would get word 1, the same word as `pop r1`.

So the stack does not read below the current SP, but below the SP as it will be after the clock edge, taking into account what the instruction in MEM does with it:

| Instruction in MEM | SP after the clock edge | Word read for a pop behind it |
| --- | --- | --- |
| anything else | SP | the word below SP |
| `pop` | SP − 1 | the word two below SP |
| a write to SP (see below) | the new value | the word below the new SP |
| `push` | SP + 1 | the word that is being pushed, which is not there yet |

The first three work out on their own, so a `pop` right behind another `pop`, which is very common at the end of a function, does not have to wait. Only the last case is a problem: the `push` writes the word at the same clock edge at which the `pop` reads it, and a block RAM then gives the old value. So a `pop` directly after a `push` waits one cycle in EX, just like a read waits behind a write:

```text
cycle       0   1   2   3   4   5   6
push r1     IF  ID  EX  MEM WB
pop r2          IF  ID  EX  EX  MEM WB
```

This is the push-then-pop stall. A `push` that is directly followed by a `pop` does not make much sense in a program, so this will hardly ever cost anything. Still, it is important to make sure this works because a compiler output might generate such sequences.

The value of a `pop` only exists at the end of MEM, so just like a read, it is a late result: an instruction that needs it right away waits a cycle in ID.

## The CPU-internal I/O registers

The CPU also has a few registers of its own, which a program can reach with normal reads and writes at `0x1F000000`:

| Address | Register | What it holds |
| --- | --- | --- |
| `0x1F000000` | `PC_BACKUP` | the address an interrupt returns to |
| `0x1F000004` | `SP` | the stack pointer |
| `0x1F000008` | `ERR_STATUS` | the error flags |
| `0x1F00000C` | `ERR_PC` | the address of the instruction that caused the first error |

`PC_BACKUP` will be used when interrupts are added, and reading and writing `SP` is useful for more advanced hardware stack usage. Only whole words can be read or written here, and `ERR_PC` can only be read. Reading one of these registers is a late result too, because the CPU reads its own state in MEM, when all older instructions are done with it.

## The error register

There are quite a few things a program can do that are almost certainly a bug, but that the CPU happily continues after: reading from an address where nothing is mapped gives 0, writing to the ROM does nothing, and an instruction with an encoding that does not exist is simply skipped. That keeps the hardware simple, but it also makes such bugs really hard to find, as the program just continues with wrong values. One of the new things in B32P4 is the error register, which records these events:

| Bit | Kind | Set by |
| --- | --- | --- |
| 0 | 1: illegal instruction | an encoding that does not exist, for example an ALU operation that is not defined |
| 1 | 2: unaligned access | a word access that is not at a multiple of 4, or a halfword access at an odd address |
| 2 | 3: bad address | an access to an unmapped address, a write to ROM, or a CPU-internal I/O access that is not a word access to a register |
| 3 | 4: bad jump target | a jump or taken branch to an address that is not a multiple of 4, or that is outside SDRAM and ROM |
| 4 | 5: stack overflow | a `push` when SP is 255 |
| 5 | 6: stack underflow | a `pop` when SP is 0 |

You might wonder why an overflow happens at 255 and not at 256 pushes. After 256 pushes, SP would be back at 0, which looks exactly like an empty stack. So the 256th push is already the one that goes wrong.

The flags are sticky: once set, a flag stays set until the program clears the register by writing any value to `ERR_STATUS`. On top of the flags, the register remembers the first event in more detail: its kind in bits 10 to 8, and the address of its instruction in `ERR_PC`. Later events only set their flag, because the first error is usually the interesting one, and the rest is often caused by it. For example:

```text
load32 0x10000000 r12   ; an unmapped address
read 0 r12 r1           ; a bad address, so r1 becomes 0
pop r2                  ; the stack is empty: an underflow
load32 0x1F000000 r10
read 8 r10 r3           ; ERR_STATUS: 0x00000324
read 12 r10 r4          ; ERR_PC: the address of the read
```

`0x324` means: bit 2 (bad address) and bit 5 (stack underflow) are set, and the first event was kind 3, the bad address. There is no interrupt attached to the error register yet, so for now a program (or me, while debugging) has to read it. If this becomes a hassle, then I could easily add an interrupt later.

### When an error counts

An error is only recorded when its instruction leaves MEM, the moment from which it can no longer be cancelled. This matters, because the pipeline often starts on instructions that never execute:

```text
    beq r0 r0 Skip      ; always taken
    .dw 0x08000123      ; an illegal instruction, flushed by the branch
Skip:
    read 8 r10 r1       ; ERR_STATUS: still 0
```

The illegal instruction gets fetched, decoded and even reaches EX, but it is flushed by the branch, so it never sets a flag. This is the same rule as for every other side effect in the CPU: nothing changes until the instruction commits in MEM.

There are two more small rules. When one instruction causes two events, which can only happen with a read or write that is both unaligned and unmapped, the lower kind counts as the first. And when an instruction writes to `ERR_STATUS` while causing an event itself (for example by doing so with an unaligned address), the register is cleared first, and then that event is recorded.

## Simulation

Let's look at the stack and the CPU-internal I/O registers on their own, and then at the MEM stage running a demo program. In the first two, the testbench applies a new case at every falling edge, and the outputs are what the CPU would see during that cycle, just before the rising edge.

![Stack simulation in GTKWave](/images/sim-stack.png "Stack simulation in GTKWave")

Here you can see the stack, with a few pushes and pops and both ends of the stack.

- The first three pushes write `11`, `22` and `33`, and SP counts up at each rising edge. After an idle cycle, the two pops get `33` and `22`.
- push 44 is followed directly by a pop, which gets `22`: the old value of the word that `44` is being written into. That's exactly why the CPU makes a pop wait after a push.
- pop (under) happens at SP 0, so underflow is set and SP wraps around to 255. The push (over) right after it then overflows, and SP wraps back to 0.
- write SP 2 sets SP directly, and in the very next cycle the top of the stack is already the word below the new SP: `44`, which was pushed at word 1 earlier.

![CPU-internal I/O registers simulation in GTKWave](/images/sim-io-regs.png "CPU-internal I/O registers simulation in GTKWave")

Here you can see the CPU-internal I/O registers. SP itself lives in the stack, so in this testbench the SP value is just an input, and a write to SP only shows up as the signal that tells the stack to take the new value.

- `PC_BACKUP` is written with `1234` and read back.
- readb sp is a byte read, so it is a bad read and gives 0.
- The unaligned access sets bit 1 and records kind 2 and its address `100` as the first event. The next case has two events (kinds 1 and 3): their flags are set too, but the first event stays the same. Reading `ERR_STATUS` then gives `207`.
- The write to `ERR_PC` is not allowed, so nothing changes.
- clear+event writes `ERR_STATUS` while having a stack overflow: the register is cleared, and then the overflow is recorded as the new first event, so it reads `510` with address `8`.

![MEM stage with the stack and the error register in GTKWave](/images/sim-mem-stack.png "MEM stage with the stack and the error register in GTKWave. Note: the screenshots are getting large, so if you cannot see everything, try opening the image in a separate tab.")

Finally, here you can see the demo program of MEM, which is the one from the previous post with a few instructions added at the end. It pushes the value it just read, pops it again, does a halfword read of SP and reads `ERR_STATUS`.

- The push waits a cycle in ID, because it needs the value of the read right before it (a late result).
- The pop waits a cycle in EX behind the push, so there is a bubble in MEM, and then it gets `00BFBEEF` from the top of the stack.
- The halfword read of SP is a bad address: it reads 0, and when it leaves MEM, the error register records kind 3 with the address of the read.
- The read of `ERR_STATUS` right after it already sees the error: `304`.

## Next steps

With the stack and the error register, the pipeline now runs most instructions, with memories that always answer in one cycle. That is not how real memory works though: the SDRAM takes many cycles, and a cache miss can take even longer. So next up, the CPU learns to wait for memory that answers whenever it is ready.
