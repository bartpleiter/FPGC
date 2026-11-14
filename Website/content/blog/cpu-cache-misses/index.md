---
title: CPU part 15 - L1 Cache part 2 (misses + controller)
date: 2026-11-14T01:00:00
authors:
  - name: Bart Pleiter
    link: https://github.com/bartpleiter
tags: [CPU]
---

In the previous post we saw that a cache hit costs nothing. This post is about the other case: a miss, where the line has to come from SDRAM first. It is also about the cache controller that does that work, about writing dirty lines back, and about the two instructions that clear the caches.

<!--more-->

## What happens on a miss

When the tag does not match, the stage that needs the data (IF for an instruction, MEM for a read or write) asks the cache controller for the line, and waits. The cache controller then:

1. writes the line that is thrown out back to SDRAM, if it is dirty
2. reads the new line from SDRAM, and writes it into the cache, word by word
3. writes the new tag, with the valid bit set

After that, the stage reads the cache again, and now it is a hit.

In the post about waiting for memory, IF and MEM talked to a memory outside the CPU, with requests and ready signals, and IF had to remember when an answer on its way was no longer needed. With the caches, that is all gone. The caches are inside the CPU now, and the only thing that talks to the SDRAM (for now, at some point we will cover DMA) is the cache controller.

## The cache controller

The cache controller handles one line at a time, and when both caches have a miss at the same time, the data cache goes first. It has only four states:

| State | What it does |
| --- | --- |
| Idle | waits for a request: first from MEM, then from IF |
| Write-back | sends the 8 words of the dirty line from the L1d to SDRAM |
| Fill | writes the 8 words from SDRAM into the cache, and the tag with the last word |
| Walk | goes over all lines to clear the caches: after a reset, and for the `ccache` instruction |

One of the new things in B32P4 is how little the cache controller has to do. Hits are handled in the pipeline itself, so the controller never sees a hit, and never has to compare a tag. In the previous FPGC version, every write went through the controller, and handling all cases took a state machine of almost thirty states.

### One line, one word per cycle

Later on, the cache controller will talk to the SDRAM controller (still needs to be rewritten). For now, the simulation has a model of it with some latency estimations. The cache controller asks for one line at a time. A line is 8 words, and that is exactly the longest burst configurable for the SDRAM (2 chips in parallel with each 16 bits bus): after a read command, the SDRAM chip gives 8 words in 8 cycles in a row, without needing a new command for each one.

The two SDRAM chips on the FPGC together have a 32-bit bus, so a line comes in one word per cycle. The cache controller writes each word into the block RAM of the cache as it comes in, and in the same way sends a dirty line out of the block RAM one word per cycle. That way it never needs to hold a whole line of 256 bits in registers (which was the case in the previous FPGC version).

### A miss, cycle by cycle

Here is a read that misses in the L1d, with estimated SDRAM timing:

| Cycle | What happens |
| --- | --- |
| 0 | The read is in MEM, and the tag does not match: a miss. MEM asks the cache controller for the line. |
| 1 | The cache controller takes the request, and asks the SDRAM controller for the line. |
| 2 to 7 | The SDRAM controller opens the row in the SDRAM chip, and sends the read command. |
| 8 to 15 | The 8 words of the line arrive, one per cycle. The cache controller writes each one into the L1d in the cycle after it arrives. |
| 16 | The last word is written, together with the tag of the new line. |
| 17 | The cache controller is done, and MEM reads the L1d again. |
| 18 | A hit: the read gets its word, and moves on. |

All that time, the instructions behind the read wait too. A miss in the L1i looks the same, with IF waiting instead of MEM.

### Dirty lines

When the line that is thrown out is dirty, it has to go back to SDRAM first. The SDRAM chip can only do one thing at a time, so the new line has to wait until the write is done, and a miss with a dirty line takes roughly twice as long as a clean one.

The cache knows right away if the old line is dirty, because the dirty bit is stored next to the tag, in the same tag array. The read that finds the miss also reads the dirty bit and the tag of the old line, so MEM can pass both on with its request. The cache controller then knows immediately what to do.

A write that misses is handled just like a read that misses (write-allocate): the cache controller fetches the line, and then the write is a normal write hit. So the controller never has to deal with the data of a write.

## Knowing when to read again

After the cache controller has written a line, the stage that waited for it has to read the cache again, and this is where the previous FPGC version got complicated, with "done" signals and flags to ignore answers that were no longer needed. In the B32P4, there is one rule for this:

A stage only uses the output of its cache when it is current: read at the address of the stage's own instruction, and not written by the cache controller since. When the output is not current, the stage reads the cache again as soon as the controller has stopped writing to it.

The controller tells each stage when it is writing to its cache, from the moment it takes a request until its last write. That is all the stages need to know:

- **At the end of a fill**, the output was read before the new line was there, so it is not current. The stage reads again, and gets a hit.
- **In MEM**, the L1d reads a new address every cycle (the address of the next instruction, which is in EX), so after the first cycle in MEM, the output belongs to another instruction. MEM then reads again with its own address.
- **A word that was read while the controller was writing** can be half old and half new. It is never current, so it is never used.

The same rule also covers the cases below, without any extra logic.

### Nothing is cancelled

Sometimes IF waits for a line that turns out not to be needed. For example, a branch is taken in MEM while IF waits for the line after the branch. In the B32P4, nothing is cancelled. If the cache controller has already started on the line, it finishes it: the line is valid, and might be useful later. IF gets its new address right away, but it waits until the controller has stopped writing the L1i, and then reads its new address:

- If the branch target is in ROM, IF does not need the L1i, so it continues right away.
- If the branch target is in the cache, IF reads it after the fill, and continues.
- If the branch target misses too, IF asks for that line after the fill.

If the controller had not taken the request yet, IF simply drops it. This makes everything simpler: the cache controller (and later the SDRAM controller) never has to stop halfway, and a redirect never has to reach outside the CPU.

### Other options

There are many ways to make misses less painful. These are the ones I did not do (yet):

| Option | Idea | Pros | Cons | Used in |
| --- | --- | --- | --- | --- |
| Non-blocking cache | instructions that do not need the missing data continue during a miss | hides a lot of the waiting | needs bookkeeping of all misses in flight, and instructions finish out of order | modern out-of-order CPUs |
| Prefetching | fetch the next line before it is needed | sequential code stops missing every 8 instructions | wastes SDRAM time when it guesses wrong, and can delay a real miss | the L1i of the previous FPGC had a simple next-line prefetch; modern CPUs have very clever prefetchers |
| Victim buffer | put the dirty line in a small buffer, fetch the new line first, and write the old one back afterwards | a miss with a dirty line becomes about as fast as a clean one | an extra buffer, and a read of the old line has to look in that buffer too | many CPUs, in some form |

All of these can be added later, if measurements on real programs show that they are worth it.

## Clearing the caches

Normally the caches take care of themselves, but there are two cases where the program has to help:

- **Code written as data.** When a program writes instructions to memory, for example when the bootloader copies a program into memory and then runs it, those instructions end up in the L1d partially as dirty lines. The L1i knows nothing about this, and might even still have the old instructions of that address.
- **DMA.** Later on, the FPGC will have a DMA unit that copies data to and from SDRAM by itself, without the CPU. It reads and writes SDRAM directly, so it can see old data from before a write-back, and the L1d can keep old data from before a DMA transfer.

For this, the FPGC has two instructions:

| Instruction | What it does |
| --- | --- |
| `ccache` | writes all dirty lines of the L1d back to SDRAM, and then clears both caches |
| `ccached` | writes all dirty lines of the L1d back to SDRAM, and then clears only the L1d |

The cache controller does this with its walk: for every line, it reads the tag, writes the line back when it is dirty, and then clears the valid bit. That takes two cycles per line, plus a write-back for each dirty line, so it is not something to do often. Meanwhile, the instruction waits in MEM, and then continues with the next instruction, which IF fetches again (as described in the decoder post, `ccache` is a jump to the next instruction). Because both caches are now cleared, that fetch goes all the way to SDRAM, and gets the new instructions.

### After a reset

After a reset, the caches could still have lines from before. So after every reset, the cache controller walks over all lines once, and clears the valid bits of both caches, one line per cycle. In the meantime the CPU starts running the bootloader from ROM, which does not need the caches.

Dirty lines are not written back after a reset, which is fine because programs should not read uninitialized memory anyway.

## Simulation

Let's look at the cache controller first, on its own. This simulation uses caches of only 4 lines, to keep the clear after the reset short.

![Cache controller simulation in GTKWave](/images/sim-cache-controller.png "Cache controller simulation in GTKWave")

Here you can see the requests from MEM and IF, and the line port to the (simulated) SDRAM controller. The line numbers on that port are the address divided by 32, so line `800` is address `0x10000`, and line `880` is address `0x11000`. Both go into the same line of the cache.

- The demo starts with the clear after the reset: the controller walks over the 4 lines, and both caches are being written.
- A write to `0x10014` misses. The line in the cache is not dirty, so the controller only reads line `800`, starting at word 5, the word the write is waiting for. A few cycles later the words arrive, one per cycle. After that, the write hits, and makes the line dirty.
- A read of `0x11008` misses on the same line of the cache, and now the line is dirty. The controller first writes line `800` back: it asks for each word with the "next" signal of the port. Then it reads line `880`, starting at word 2.
- IF misses on `0x10014`, so the controller fills the L1i with line `800`, which now has the data written back a moment ago.
- A write to `0x1100c` hits, so the controller has nothing to do, but line `880` is now dirty.
- A `ccache` starts the walk. The controller writes the dirty line `880` back on its way, and clears both caches.

![The caches in the pipeline in GTKWave](/images/sim-cache-pipeline.png "The caches in the pipeline in GTKWave")

Here you can see the caches in the pipeline, with this program running from SDRAM:

```text
load32 0x10000 r1
load 7 r2
read 0 r1 r3        ; misses
write 4 r1 r2       ; the same line, which is still arriving
read 0x1000 r1 r4   ; the same line of the cache: the dirty line goes back first
add r3 r4 r5
halt
```

- After the jump from the ROM, IF first waits for the clear after the reset to finish (128 lines this time, so I scrolled past it).
- The first fetch misses in the L1i: IF asks for the line, and the controller fills it.
- The first `read` misses in the L1d. MEM asks for the line and waits.
- The `write` behind it is to the same line, which is still arriving. Its output is not current, so it waits until the controller is done, reads the L1d again, hits, and writes. From then on the line is dirty.
- The last `read` is of a line with the same index, so the controller first writes the dirty line back, and then fills the new one.

You might notice that IF and the first `read` do not wait for the whole line to arrive. That is the topic of the next post.

## Next steps

The caches are now complete. In the next post I will most likely cover some optimizations.
