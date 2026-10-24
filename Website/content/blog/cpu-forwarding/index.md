---
title: CPU part 5 - Forwarding unit
date: 2026-10-24T10:00:00
authors:
  - name: Bart Pleiter
    link: https://github.com/bartpleiter
tags: [CPU]
---

The next component of the CPU is the forwarding unit. It makes sure that an instruction always works with the newest value of a register, even when that value has not been written into the register file yet.

<!--more-->

## The problem again

A quick recap, with the same two instructions as in the [pipeline post](/blog/cpu-pipeline):

```asm
add r1 r2 r3    ; r3 = r1 + r2
sub r3 r4 r5    ; r5 = r3 - r4: needs r3 right away
```

When `sub` reads `r3` in ID, `add` is only in EX, and the register file gets the new value of `r3` two cycles later. But the value does already exist somewhere in the pipeline, so we can take it from there instead. Seen from the instruction in ID, there are four places where the instruction that writes the register can be:

| Where the writing instruction is | Where the value comes from |
| --- | --- |
| in EX | the EX/MEM register, once we are in EX |
| in MEM | the MEM/WB register, once we are in EX |
| in WB | the register file, thanks to the write-through |
| further ahead | the register file, which already has it |

The third row is the write-through from the register file post: the instruction in WB writes the register in the same cycle we read it, and the read port already gives the new value. So the register file covers that case by itself, and the forwarding unit only has to look at the instructions in EX and MEM.

## What the forwarding unit does

For each of the two registers that an instruction reads, the forwarding unit checks whether one of the two instructions in front of it writes that register. That is just a comparison of 4-bit register numbers: the values themselves are not needed for this at all. The answer is one of three places:

| Instruction in EX writes it | Instruction in MEM writes it | Take the value from |
| --- | --- | --- |
| yes | yes or no | MEM |
| no | yes | WB |
| no | no | the register file |

Once the instruction is in EX, a multiplexer in front of the ALU picks the value from one of these three places, for each of the two operands. The forwarding unit is purely combinational logic, just like the ALU and the decoder, and it is one of the smallest modules of the CPU.

## Design choices

### Deciding in ID instead of EX

The textbook way is to do these comparisons in EX: compare the registers of the instruction in EX with the instructions in MEM and WB, and use the answer directly for the multiplexer. I do it one cycle earlier, in ID, which might look a bit odd at first: the forwarding unit looks at the instructions in EX and MEM, while its answer is about where those instructions will be one cycle later, in MEM and WB.

The reason is the ALU. As I wrote in the ALU post, EX is the stage with by far the most logic in it, so I want the inputs of the ALU to be ready as early as possible in the cycle. If the comparisons happened in EX, the ALU would first have to wait for the comparisons, and then for the multiplexer. By doing the comparisons in ID, the answer is stored in the ID/EX register with the rest of the instruction, and comes straight out of a flip-flop at the start of EX. Only the multiplexer is then left in front of the ALU. ID has time to spare for this, as decoding is mostly just picking fields from the instruction word.

This works because of the stall rule from the pipeline post: a stage can only move on if every stage after it moves on too. So at the moment the instruction moves from ID into EX, the instruction in EX moves into MEM, and the instruction in MEM moves into WB. The forwarding unit therefore knows in advance where each value will be. The answer is only stored when the instruction actually moves into EX, so while it waits in ID, the forwarding unit just looks again every cycle.

### The younger instruction wins

Both instructions in front can write the same register:

```asm
add r1 1 r1
add r1 1 r1
add r1 r0 r2    ; needs the r1 of the second add
```

In that case, the value of the instruction in EX is the newest one, as it comes later in the program. That is why the first row of the table says "yes or no" for MEM: as soon as the instruction in EX writes the register, the value comes from MEM.

### Only registers that are actually read

As I explained in the decoder post, the decoder always passes on the register numbers, even for instructions that do not use them. For example, a `jump` has a 27-bit constant, so the bits where the register numbers would be are just part of the address. Forwarding a value that is not used is harmless, as it is ignored anyway. But as we will see below, sometimes the instruction has to wait for a value, and waiting for a register that it does not even read would be a waste of a cycle. Luckily, the decoder already tells which registers an instruction really reads, so the forwarding unit only looks at those.

There is no special case for `r0` either. When an instruction writes to `r0`, the decoder already says that it does not write a register at all, because writing `r0` does nothing. So a write to `r0` is never forwarded, and reading `r0` keeps giving 0.

### Late results

There is one case forwarding can't solve: the load-use hazard from the pipeline post. Some results only exist at the end of MEM, like a value read from memory, a pop from the stack, or a multiplication. The decoder marks these instructions as late. If such an instruction is in EX while the instruction in ID needs its result, it would only have its value at the end of the next cycle, while our instruction needs it at the start of that cycle.

So in that case, the forwarding unit tells the instruction to wait one cycle in ID: the late-result stall. In the next cycle, the late instruction is in MEM, so the forwarding unit now picks WB, and by the time our instruction is in EX, the value is in the MEM/WB register. Only the instruction in EX matters for this: a late instruction that is already in MEM will have its value ready in time.

Nothing in the CPU produces a late result yet, as memory access comes later, but the forwarding unit is already prepared for it.

### A stalled EX keeps its values

There is one more catch, which is not in the forwarding unit itself but in EX. Later on, an instruction can get stuck in EX for a while, for example during a division that takes many cycles. The stages after it keep going in the meantime, so the instructions in MEM and WB leave, and the values that were being forwarded leave with them. To prevent this, a stalled EX stores the values it got from forwarding in its own pipeline register, and from then on uses those instead, no matter what happens in MEM and WB.

## Simulation

<!-- TODO: screenshot of `fpgc cpu wave cpu_forward`, saved as Website/static/images/sim-forward.png -->

![Forwarding unit simulation in GTKWave](/images/sim-forward.png "Forwarding unit simulation in GTKWave")

Here you can see the forwarding unit in GTKWave. In every case, the instruction in ID is an `add` that reads `r1` and `r2`, and the cases change which registers the instructions in EX and MEM write. Again, this is combinational logic, so the clk signal is just for spacing. Some things to look at:

- areg in EX: the instruction in EX writes `r1`, so `areg_from_mem` is set.
- both: EX writes `r2` and MEM writes `r1`, so `r2` comes from MEM and `r1` from WB at the same time.
- areg twice: EX and MEM both write `r1`, and only `areg_from_mem` is set, because the younger instruction wins.
- late in EX: the same as areg in EX, but now the instruction in EX is late, so `late_result_stall` is set as well.
- late unread: the late instruction writes `r3`, which the `add` does not read, so there is no reason to wait.
- breg unread: EX writes `r2`, but this time the instruction does not read its B register, so nothing is forwarded.

## Next steps

When an instruction has to wait in ID for a late result, IF has to wait too, and EX needs something else to work on in the meantime. Handling that for the whole pipeline is the job of the hazard unit, which is next.
