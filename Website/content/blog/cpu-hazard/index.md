---
title: CPU part 6 - Hazard unit
date: 2026-10-24T11:00:00
authors:
  - name: Bart Pleiter
    link: https://github.com/bartpleiter
tags: [CPU]
---

Forwarding solves most data hazards without having to wait, but not all of them. Sometimes the pipeline really has to wait (stall), and sometimes it has to throw away instructions it already started on (flush). The hazard unit is the module that decides this for the whole pipeline, and it turned out to be one of the smallest modules of the CPU.

<!--more-->

## Stall and flush

In the [pipeline post](/blog/cpu-pipeline) I wrote down a simple rule: a stage stalls if it is busy, or if any later stage stalls. A stage is busy when its instruction can't move on yet. Once the CPU is complete, every stage except WB has a reason to be busy:

- IF, when the instruction cache misses and has to wait for the SDRAM.
- ID, when a value it reads isn't there in time: the late-result stall from the previous post.
- EX, during instructions that take multiple cycles, like a division.
- MEM, when a memory access has to wait, for example for the SDRAM.

Each stage tells the hazard unit whether it is busy, and the hazard unit tells each stage whether to stall or flush the pipeline register in front of it (for IF, that is the PC). A stalled register keeps its instruction, and a flushed register gets `valid` 0, which turns its instruction into a bubble that is ignored from then on.

There is one more input: the redirect. When a branch or jump reaches MEM and turns out to be taken, the three instructions behind it have to be thrown away, as explained in the pipeline post.

## The rules

The whole hazard unit comes down to two rules:

1. A stage stalls when it, or any stage after it, is busy.
2. A stage is flushed on a redirect, or when the stage before it stalls while the stage itself does not.

Applying these rules to each stage being busy on its own gives a nice staircase:

| Busy stage | Stalls | Gets a bubble |
| --- | --- | --- |
| IF | IF | ID |
| ID | IF, ID | EX |
| EX | IF, ID, EX | MEM |
| MEM | IF, ID, EX, MEM | WB |

The later the busy stage, the more stages have to wait. And there is always exactly one bubble, right after the last stage that stalls.

## Design choices

### Bubbles

The stalls are the obvious part, but the second rule needs a bit more explanation. Take the load-use hazard from the pipeline post, where the `add` has to wait one cycle in ID:

```text
cycle           1   2   3   4   5   6   7
read 0 r2 r1    IF  ID  EX  MEM WB
add r1 r3 r4        IF  ID  ID  EX  MEM WB
sub r5 r6 r7            IF  IF  ID  EX  MEM
```

In cycle 3, ID is busy, so IF and ID stall. But EX and MEM do not stall, because they are after ID, so the `read` moves on into MEM. What should EX work on in cycle 4? If nothing is done, the ID/EX register takes over whatever is in ID, just like any other cycle, and the `add` ends up in both ID and EX: it would be executed twice. So the register right after the last stalled stage gets flushed instead, and EX gets a bubble.

### Everything in one place

Having all stall and flush decisions in one central place is one of the new things in B32P4. Hazard handling was one of the most challenging parts of my previous pipelined CPU designs, and doing it like this, centralized and as early as possible, hopefully keeps it a lot more manageable. A stage only has to say "I'm busy", and the rest follows from the two rules. Each stage combines its busy signal with its own valid bit, so a bubble can never be busy.

### A flush beats a stall

A redirect and a stall can happen at the same time, for example when the instruction in ID is waiting for a late result while the branch in front of it is taken. In that case, the waiting instruction is thrown away anyway, so the flush wins. The redirect itself only happens when MEM is not busy, so the branch is never stalled while it redirects.

The redirect also flushes only the instructions in IF, ID and EX, and not the one in MEM: that is the branch or jump that redirects, and it should of course still finish.

### WB never stalls

WB only writes the register file, which always takes one cycle, so there is nothing that can make it wait. The MEM/WB register therefore has no stall at all: when MEM stalls, WB simply gets a bubble.

## Simulation

<!-- TODO: screenshot of `fpgc cpu wave cpu_hazard`, saved as Website/static/images/sim-hazard.png -->

![Hazard unit simulation in GTKWave](/images/sim-hazard.png "Hazard unit simulation in GTKWave")

Here you can see the hazard unit in GTKWave, where first each stage is busy on its own, followed by a redirect and two combinations. Again, combinational logic, so the clk signal is just for spacing.

- The first four busy cases show the staircase from the table above.
- A redirect flushes ID, EX and MEM at the same time, without stalling anything.
- When IF and ID are both busy, there is still only one bubble, in EX. And when ID and MEM are both busy, MEM wins, as it stalls all stages before it anyway.

Right now, only the redirect really does anything in the CPU: the busy signals will come with memory access, division and the caches.

## Next steps

There is one more unit to go before the pipeline is complete enough to run its first programs: the branch unit, which decides whether a branch is taken, and where it goes.
