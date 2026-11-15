---
title: Caches
weight: 5
reader: rtl
---

How the L1 instruction cache (L1I), the L1 data cache (L1D) and the cache controller work. What a miss costs is on the [programming notes](programming.md#memory) page, and the SDRAM side of the cache controller on the [interface](interface.md#sdram-line-port) page.

## Overview

- Two direct-mapped caches with lines of 8 words (32 bytes): the L1I for instructions from SDRAM, and the L1D for reads and writes of SDRAM.
- The L1D is write-back and write-allocate. A write hit only writes the cache and marks the line dirty. A write miss first reads the line, and then writes into it like a hit.
- The pipeline handles hits itself. IF and MEM read the caches like any block RAM, check the tag, and the L1D's write hits are written by MEM. A hit costs no extra cycles.
- The cache controller only handles misses and clears: it writes a dirty line back, reads the missing line, and empties the caches for `ccache`, `ccached` and after a reset. It handles one line at a time.
- Nothing is ever cancelled. A fill that's no longer needed (after a redirect) still finishes, and the line stays valid.

| Module | What it is |
| --- | --- |
| `cpu_l1i` | the L1I's word array and tag array |
| `cpu_l1d` | the L1D's word array, as 4 byte lanes, and tag array |
| `cpu_cache_controller` | the state machine for misses and clears, and the SDRAM line port |

## The arrays

Each cache is a word array and a tag array, each a block RAM with two ports. Port A belongs to the pipeline, port B to the cache controller.

| Array | Port A (pipeline) | Port B (cache controller) |
| --- | --- | --- |
| L1I words | read at `fetch_addr`, with the read enable `fetch_en` | write (fills) |
| L1I tags | the same | write (fills, clears) |
| L1D words, 4 byte lanes | read at `data_addr`; write hits at commit, with byte enables | read (write-backs), write (fills) |
| L1D tags | read at `data_addr`; write hits at commit (the dirty bit) | read (clears), write (fills, clears) |

An address is split into a tag, an index (which line of the cache) and the word in the line. With the default 512 lines of the L1D:

```text
bits    25          14 13          5 4      2 1    0
       +--------------+-------------+--------+------+
       |     tag      |    index    |  word  | byte |
       +--------------+-------------+--------+------+
```

With `LINES` lines, the index has log2(`LINES`) bits, and the tag is the rest of bits [25:5]. The L1I with its 128 lines has its index in bits [11:5] and its tag in bits [25:12].

| Entry | Contents |
| --- | --- |
| L1I tag entry | valid, tag |
| L1D tag entry | valid, dirty, tag |

- The dirty bit is in the tag array. The tag that MEM reads for its hit check also says whether the line it would replace is dirty, so a miss knows right away whether it needs a write-back.
- The arrays start at 0, so every line is invalid when the FPGA is configured.
- The L1I arrays have a read enable, like the ROM, so a stalled IF keeps their outputs. The L1D arrays have none, and are never held: they read whatever address the mux in front of them gives, and MEM only uses an output that belongs to its instruction (see [The current rule](#the-current-rule)).

## Hits

In IF, the ROM and the L1I are read together, at the fetch address. In the next cycle, IF uses the ROM's word when the PC is in the ROM, and otherwise the L1I's word if the tag matches. A hit gives the instruction in the same cycle as the ROM would, so code in SDRAM runs at the same speed as code in the ROM.

In MEM, the L1D is read one stage early. The address mux in front of the L1D (`data_addr`) normally gives the address from EX's address adder, so the L1D's output is there when the instruction enters MEM. There, MEM checks the tag:

- A read hit passes the word through the read mux to WB. It costs nothing.
- A write hit is written at commit, through port A at the instruction's own address: the bytes into the lanes, with their byte enables, and the tag entry with the dirty bit set. It costs nothing, but while the write is in MEM, the address mux gives its address `ex_mem_addr` to the L1D, so a read or write right behind it waits one cycle (the [write-then-access stall](pipeline.md#stalls-for-memory)).

## The current rule

A block RAM's output belongs to the address it was read at, and to the contents at that moment. The output that IF or MEM has can stop belonging to its instruction in two ways:

- The cache controller wrote that cache after the read, or in the same cycle.
- In MEM only, the L1D has read another address since. It's never held, so after an instruction's first cycle in MEM, it reads the address of the next instruction in EX.

So each stage has one register that says whether its output is current: `l1i_current` in IF and `l1d_current` in MEM. A stage only uses a current output:

- A current output that matches is a hit.
- A current output that doesn't match is a miss: the stage requests the line.
- An output that isn't current is neither. The stage waits until the cache controller stops writing that cache (`l1i_writing`, `l1d_writing`), and then reads the cache again at its own address (`l1i_reread`, `l1d_reread`). The next output is current.

This one rule handles the end of every fill, a fill that IF no longer needs after a redirect, the clears, and the reads that happen while a fill is in progress. An output read while the controller writes is never current, so it never matters what a block RAM gives when one port reads an address that the other port writes.

## Misses

A miss sets a request register in its stage, once, in the cycle the stage sees a current output that doesn't match:

- IF sets `l1i_request` with the address of `pc`. A request that the controller hasn't taken yet is dropped at a redirect.
- MEM sets `l1d_request` with `ex_mem_addr`, and takes the line it would replace from the tag entry at the same moment: `victim_dirty` and `victim_tag`. The instruction stays in MEM, so the address doesn't change while it waits.
- The controller takes one request at a time, an L1D request before an L1I request, and clears it with `l1i_take` or `l1d_take`. From then on, `l1i_writing` or `l1d_writing` is 1 until the line is written, so the stage's output isn't current.

A clean miss goes like this, cycle by cycle. `L` is the number of cycles that the SDRAM controller needs from seeing a request to the first word:

| Cycle | What happens |
| --- | --- |
| 0 | The read is in MEM, its output is current, and the tag doesn't match: `l1d_request` is set. |
| 1 | The controller sees the request, and takes it at the end of the cycle. |
| 2 | `sdram_req` and `l1d_writing` are 1. The SDRAM controller sees the request. |
| 2 + `L` | The needed word arrives first. MEM takes it as it arrives and moves on (early restart). |
| 3 + `L` to 9 + `L` | The other 7 words arrive. Each is written into the L1D in the cycle after it arrives, and the tag entry with the last one. |
| 11 + `L` | `l1d_writing` is 0 again. |

So MEM waits `L` + 2 cycles. An L1I miss works the same way, in IF.

A miss with a dirty line first writes the old line back: the controller reads its 8 words from L1D port B and sends them to the SDRAM controller. When the last word is sent, it requests the read of the new line, and continues as above.

## Early restart

The words of a line arrive one by one, starting at the word the stage waits for (`sdram_first`), and wrapping around inside the line. With early restart, the stage doesn't wait for the whole line: it takes its word from the SDRAM port in the cycle it arrives.

- The controller says which word arrives next. During a fill, `fill_addr` is the line and the word that arrives with the next `sdram_read_valid`, and `l1i_filling` or `l1d_filling` says which cache it's for. All three are registers.
- IF and MEM compare `fill_addr` with their own address. When it matches and a word arrives, they use `sdram_read_word` as if it were a hit (`l1i_bypass`, `l1d_bypass`). The word is one more input of the instruction mux in IF and the read mux in MEM.
- Straight-line code follows the fill: the next instruction is the next word, which arrives in the next cycle. A read of the next word, right behind the read that missed, works the same way.
- A word that the stage can't take as it arrives (it was stalled, or the word has passed) isn't used. The stage's output isn't current, so it waits until the fill ends and reads the cache again.
- Only reads use it. A write that missed waits until the whole line is in the L1D, and then writes it as a hit.

## The cache controller

| State | What it does |
| --- | --- |
| `IDLE` | takes a request: a clear first, then an L1D miss, then an L1I miss |
| `WRITE_BACK` | sends the 8 words of a dirty L1D line from port B to the SDRAM port, one per `sdram_next` |
| `FILL` | writes the 8 words from the SDRAM port into port B of the L1I or the L1D, each in the cycle after it arrives, and the tag entry with the last one |
| `WALK` | goes through every line for a clear: reads the L1D tag, writes the line back if it's dirty, and writes 0 to the tag entry |

- Every output is a register, except the word of a write-back, which comes straight from the L1D's port B.
- It streams the words. There's no line-wide register anywhere: a fill writes each word as it comes, and a write-back reads one word per cycle from the block RAM.
- It has no flush input. Whatever it starts, it finishes.

## Clears

`ccache` and `ccached` make a request to the cache controller from MEM, once, and MEM waits (`busy_mem`) until the controller is done. Then the instruction commits, and redirects to the next instruction, so that IF fetches it again from the emptied L1I.

- The walk takes 2 cycles per line: one to read the L1D tag entry, and one in which the entries are written 0 while the old L1D entry is checked. When that line was dirty, its words are written back before the walk goes on.
- `ccache` walks as many lines as the larger cache has, and clears both. `ccached` walks the L1D's lines, and clears only the L1D.

## Reset

After a reset, the cache controller empties both caches without writing anything back: one line per cycle, as many cycles as the larger cache has lines, and no SDRAM traffic. SDRAM's contents are undefined after a reset anyway. Meanwhile, the CPU runs from the ROM, and anything that needs a cache waits, because the `writing` signals are 1.
