---
title: CPU part 16 - L1 Cache part 3 (early restart)
date: 2026-11-14T02:00:00
authors:
  - name: Bart Pleiter
    link: https://github.com/bartpleiter
tags: [CPU]
---

In the previous post, a stage with a cache miss waited until the whole line was in the cache. However, the CPU only needs a single word from that line which allows us to do some smart optimizations.

<!--more-->

## Critical word first, and early restart

When a line comes from SDRAM, its words arrive one per cycle. There are two tricks that make a miss shorter, and they work best together:

- Critical word first: ask the SDRAM for the needed word first, and the rest of the line after it.
- Early restart: let the waiting stage continue as soon as its word arrives, while the rest of the line is still coming in.

Both tricks are used in many CPUs. Critical word first comes almost for free with SDRAM. A read burst of 8 words can start at any word, and then wraps around inside its block of 8. So a burst that starts at word 5 gives the words in the order 5, 6, 7, 0, 1, 2, 3, 4. The SDRAM chip does that by itself, so the cache controller only has to tell which word to start with. It already did that since the previous post, but the stage still waited for the whole line.

## Early restart in the B32P4

The word that arrives from SDRAM is now one more input of IF (next to the ROM and the L1i), and of MEM (next to the L1d). In the cycle a word arrives, the stage checks if that is the word it waits for, by comparing the address of the word with its own address. Both addresses come straight from registers, so this check is done early in the cycle. If it is the right word, the stage takes it and continues, as if it was a hit.

For the read in the example of the previous post, the word now arrives first, in cycle 8, and the read continues right away, instead of in cycle 18.

### IF follows the line

For IF it gets even better. Code mostly runs straight on, and the next instruction is the next word of the same line, which arrives in the next cycle. So IF can follow the line as it comes in:

```text
cycle         0    1    2    3    4    5    6    7
arrives       w0   w1   w2   w3   w4   w5   w6   w7
IF passes on  i0   i1   i2   i3   i4   i5   i6   i7
```

Here IF missed on the first instruction of a line. From the moment the first word arrives, it passes one instruction per cycle on to ID, as fast as with a hit. This is the main reason why I did not implement a simple IF prefetch like in the previous FPGC version, as this early restart mechanism already achieves a similar effect.

### What does not change

The rule from the previous post, that a stage only uses a cache output that is current, stays exactly as it was. Early restart only adds a second way to get the word, so nothing else has to change:

- A word that a stage cannot take in the cycle it arrives is gone. For example, IF follows a line, but ID has to wait a cycle for the result of a read. Then the next word passes by while IF stands still. IF then waits until the controller is done with the line, and reads it from the L1i, just like without early restart.
- A write still waits for the whole line. A write changes a word in the line, and the cache controller is still writing the line. So the write waits until the line is complete, reads the L1d again, and then writes like any other write hit.
- Another instruction that needs the same line, but a word that has already passed, also waits for the end of the line, and then hits.

### Timing issue

Checking the data cache in MEM takes almost the entire clock cycle, leaving too little time to also tell IF to stop on a miss (as found by timing analysis in Quartus). It is a bit complex to explain, so I will not go into detail on this, but basically with the CPU almost done and the cache being the most complex part, I ran into timing issues that needed to be solved.

To fix this specific timing issue, IF now ignores the miss on the first cycle and advances to the next instruction anyway. Since the rest of the pipeline is paused, IF notices on the next cycle that the previous instruction wasn't taken, steps back one address, and fetches it again while MEM waits for the cache line. It is a bit ugly, but hopefully it leaves enough timing margin once the rest of the design is complete. If not, I might have to simplify certain parts of the CPU and compromise on instructions per clock cycle in some situations.

## Simulation

Let's look at early restart and the refetch in the pipeline. This is the same program as in the previous post, with fewer signals.

![Early restart and refetch in GTKWave](/images/sim-early-restart.png "Early restart and refetch in GTKWave")

```text
load32 0x10000 r1
load 7 r2
read 0 r1 r3        ; misses
write 4 r1 r2       ; the same line, which is still arriving
read 0x1000 r1 r4   ; the same line of the cache: the dirty line goes back first
add r3 r4 r5
halt
```

- The first fetch misses in the L1i. When the words arrive, IF takes each one from SDRAM, and the PC goes up by one instruction per cycle.
- The first `read` reaches MEM while IF is at the `add`, and finds its miss late in the cycle. IF moves on to the `halt`, sees that the `add` was lost, and goes back to it. The word of the `add` has already passed by then, so IF waits until the line is complete, and reads it from the L1i.
- The `read` gets its word as soon as it arrives from SDRAM: the 0 that is at `0x10000`.
- The `write` waits for the whole line, and then writes.
- The last `read` misses late too, so IF goes back once more. The dirty line is written back first, and then the new line comes in, and the `read` continues with its word as soon as it arrives.

## Next steps

With early restart, the caches are done, and with that almost the whole CPU. Next up is memory access to other parts than the SDRAM.
