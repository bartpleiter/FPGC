---
title: CPU part 11 - Waiting for memory
date: 2026-10-25T02:00:00
authors:
  - name: Bart Pleiter
    link: https://github.com/bartpleiter
tags: [CPU]
---

Until now, every memory the CPU talked to answered in exactly one cycle. Real memory is not that nice: the SDRAM the FPGC uses as main memory takes many cycles, and even with caches, a cache miss still causes a delay. In this post the CPU learns to wait for memory that answers whenever it is ready.

<!--more-->

## Asking and answering

IF and MEM each talk to their own memory (important note: in the end they will mostly share the same SDRAM memory, but that will be hidden by their L1i and L1d caches), and both do it in the same way. At a clock edge, the CPU asks for a word, just like it gives a block RAM its address. This is the request. The memory then answers when it has the word, by setting its ready signal, and keeps the word at its output until the next request. In previous FPGC designs I called these signals `start` and `done`, but `request` and `ready` are more descriptive so I will keep using these in this FPGC redesign.

| Stage | Makes a request | The answer |
| --- | --- | --- |
| IF | at every clock edge where IF moves on, and at a redirect | the instruction |
| MEM | at the clock edge where a read or write enters MEM | the word that is read, or for a write: "go ahead" |

A block RAM is just a memory that always answers in the cycle right after the request, so with block RAMs, the CPU runs exactly like it did before. That is also how the caches will fit in later: a cache hit is a memory that answers right away, and a miss is one that takes longer.

The memory keeps its answer until the next request, and that turns out to be quite important. A stage can be stalled by a later stage at the moment its word arrives. If the memory would only show the word for one cycle, that stage would have to catch it in a register of its own, and remember that it already got it. Because the word simply stays there, a waiting stage never has to remember anything. This also differs from previous FPGC designs, and hopefully this decision will pay off.

## Waiting in IF

When the instruction memory has not answered yet, IF is busy. Just like any other busy stage, this stalls IF, and ID gets a bubble. Here is the end of the IF demo program, with an instruction memory that waits 3 cycles before every answer:

```text
cycle       0   1   2   3   4   5   6   7   8   9   10  11  12
jumpo 12    IF  IF  IF  IF  ID  EX  MEM WB
nop                         IF  IF  IF                          ; flushed
halt                                    IF  IF  IF  IF  IF  ID
```

The `jumpo` spends four cycles in IF: three waiting, and one with its word. Then IF asks for the next word, the `nop`. In cycle 6 the `jumpo` redirects from MEM, while the `nop` has not arrived yet. The PC becomes the jump target right away, but IF cannot ask for the `halt` yet, because the memory is still busy with the `nop`. So IF remembers that the word that is on its way is no longer needed. In cycle 7 the `nop` arrives, IF drops it, and only then asks for the `halt`.

It might seem simpler to just make the new request right away and let the memory forget about the old one. Fetching an instruction has no side effects, so that would be allowed. But then every memory behind IF would have to be able to drop a request halfway. A simple memory controller saves the address when the request comes in, goes through the steps of its state machine, and sets ready at the end. If the address could change in the meantime, it would give the word of the old address, unless it stopped in the middle or checked the address again at the end. Making the CPU wait for the old answer costs only one flag in IF, and a few cycles in the (hopefully, I actually did not check how often this happened in the previous FPGC design) rare case where a redirect comes while IF is waiting. In return, both memories of the CPU follow the same simple rule: a new request only comes after the answer to the last one.

## Waiting in MEM

A read or write to SDRAM makes its request at the clock edge where it enters MEM, and waits in MEM until the memory answers. A read then takes its word, and a write writes at the end of that cycle, just as before. A write also has to wait for the answer, because the memory might not be ready for it. For example, a cache might first have to fetch the rest of the data around it from the SDRAM.

While MEM waits, everything behind it waits too, and WB gets bubbles. Here is a write and a read, with a data memory that waits 3 cycles before every answer:

```text
cycle           0   1   2   3   4   5   6   7   8   9   10  11  12  13  14
write 0 r1 r2   IF  ID  EX  MEM MEM MEM MEM WB
read 0 r1 r3        IF  ID  EX  EX  EX  EX  EX  MEM MEM MEM MEM WB
add r3 r2 r4            IF  ID  ID  ID  ID  ID  ID  ID  ID  ID  EX  MEM WB
```

The `write` makes its request at the end of cycle 2, and the memory answers in cycle 6. The `write` writes at the end of that cycle. The `read` waits in EX all that time, and then one cycle more: in cycle 6 the `write` uses the address input of the memory, so that is the write-then-access stall from the memory post. The `read` makes its request at the end of cycle 7, gets its word in cycle 11, and the `add` that needs it waits in ID until then.

There is a reason the request is made exactly when the read or write enters MEM. At that point, all the instructions in front of it have left MEM, so nothing can flush it anymore. This means that every read or write makes exactly one request, and never one for an instruction that gets flushed afterwards. For SDRAM reads that would not really matter, but it will for the I/O devices later on: reading from a UART for example takes a byte out of its receive buffer, so reading twice, or reading for an instruction that should not have run, would lose data.

While needing to support both reading and writing to several types of memory makes it challenging to get this stage correct, the rule from IF comes for free here: the data memory never gets a new request before it answered the last one, because a read or write only leaves MEM with its answer.

## Why caches matter

Waiting is correct, but it is slow. The whole point of a pipeline is that a new instruction can start every cycle, and every cycle spent waiting for memory throws that away. Just one cycle of waiting per fetch and per read or write already makes the CPU almost twice as slow, and with real SDRAM latencies it will be even worse. You can see the impact already in the simulations below. That is exactly why caches are so important: they make most fetches and reads answer right away, so the pipeline can keep doing what it is good at.

Somewhere around FPGC6, I added my first form of caching, but only on the side of the SDRAM controller and still caused almost every instruction to stall at least one cycle. With the last FPGC version I was able to get L1i and L1d caching working to combat this, but with a bunch of inefficiencies and it made the hazard handling borderline unmanageable. Now, in B32P4 the idea is to make the L1i and L1d cache simpler and more efficient, which hopefully can give a further boost in performance.

## Simulation

To show this, the simulated memories can now wait a set number of cycles before they answer. Until they answer, they give an unknown value (`X`), so in GTKWave you can see exactly when the word arrives.

![IF stage with an instruction memory that waits in GTKWave](/images/sim-if-wait.png "IF stage with an instruction memory that waits in GTKWave")

Here you can see the demo program of IF, with an instruction memory that waits 3 cycles before every answer.

- After every request, the ready signal is 0 for three cycles, and the word is unknown until it comes. During that time IF is busy, so it stalls, and the PC stands still.
- When the `jumpo` redirects, IF is still waiting for the word after it. The PC becomes the jump target right away, and IF remembers that the word on its way is an old one. In the next cycle that word arrives, IF drops it, and makes the request for the `halt`.

![The pipeline with a data memory that waits in GTKWave](/images/sim-mem-wait.png "The pipeline with a data memory that waits in GTKWave")

Here you can see the whole pipeline running the example above, with a data memory that waits 3 cycles before every answer.

- The `write` makes its request as it enters MEM, and the memory is ready three cycles later. The `write` writes at the end of that cycle.
- While MEM waits, all stages stall, and WB gets bubbles.
- The `read` waits one more cycle in EX, then makes its request, and gets the `7` it reads three cycles later.

## Next steps

The CPU can now wait for its memories, which is the base for the caches later on. Before that, the next post is about instructions that take more than one cycle in EX themselves: multiplication and division.
