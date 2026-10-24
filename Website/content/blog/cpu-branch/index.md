---
title: CPU part 7 - Branch unit
date: 2026-10-24T12:00:00
authors:
  - name: Bart Pleiter
    link: https://github.com/bartpleiter
tags: [CPU]
---

Without branches and jumps, a program can only run from top to bottom once. They are what makes loops, if statements and function calls possible. In this post I go through how the CPU handles them, with the branch unit as the last new module before the pipeline can run its first programs.

<!--more-->

## Where to, and whether to go

For every branch or jump, the CPU needs to know two things: where to go (the target), and whether to go at all (the condition). Jumps always go, so only branches have a condition. The target is a bit more varied, as the instruction set has quite a few ways to describe it (the full details are on the [instruction set page](/cpu/isa)):

| Instruction | Target | Computed in |
| --- | --- | --- |
| `beq`, `bne`, ... | PC + constant | ID |
| `jump` | the constant itself | ID |
| `jumpo` | PC + constant | ID |
| `halt` | PC | ID |
| `jumpr` | register + constant | EX |
| `jumpro` | PC + register + constant | ID and EX |

## The target

Most targets are relative to the PC of the branch or jump itself. The PC and the constant are both known in ID, so ID has an adder that computes the PC plus the constant: the target adder. `halt` is simply a jump with constant 0, so it jumps to itself forever (at least until an interrupt comes in, but that is for a later post). And `jump` uses the constant as the target directly, without adding anything.

As I mentioned in the decoder post, this adder has one more use: `savpc` needs the PC as its constant, and `jumpro` needs PC + constant, so they both get it from the target adder too. This way, the ALU never needs the PC as an input.

The targets of `jumpr` and `jumpro` also depend on a register, so they can only be known in EX, once the register value is there (after forwarding, of course). For this, EX has a second adder that adds the register to the constant: the address adder. Later on, it will also compute the addresses for reading and writing memory, which is where its name comes from. I could have used the `add` of the ALU for this, but a single adder on its own is a lot faster than the ALU, where the add is only one of thirteen operations behind a big multiplexer. Memory addresses are on one of the most critical paths of the CPU, so this is worth the extra adder.

All instructions are at an address that is a multiple of 4, so the lowest two bits of the target are ignored when the PC is redirected. Later on, a target that is not a multiple of 4 will also be reported in the error register, as it most likely means something went wrong in the program.

## The condition

The condition is checked by the branch unit, a small new module in EX. It compares the two registers of the branch, which means a branch can directly use the result of the instruction right before it, thanks to forwarding.

There are six branch conditions, but the branch unit only needs two compares to decide all of them: whether the two registers are equal, and whether the first is less than the second. Everything else follows from those two:

| Branch | Taken when |
| --- | --- |
| `beq` | equal |
| `bne` | not equal |
| `blt` | less than |
| `bge` | not less than |
| `bgt` | not less than, and not equal |
| `ble` | less than, or equal |

Just like the `slt` and `sltu` operations of the ALU, the less-than compare can be signed or unsigned, so there are also signed versions of the last four: `blts`, `bges`, `bgts` and `bles`. For `beq` and `bne` there is no signed version, as two numbers are either equal or not, no matter how you look at their bits. Because B32P4 has no flags (as explained in the ALU post), a branch always does its own compare, instead of looking at the result of an earlier instruction.

Two of the eight possible condition codes are not used. The decoder marks those as illegal, so they never reach the branch unit as a branch, and the branch unit says they are never taken anyway.

## The redirect

Once EX knows the target and whether the branch is taken, it passes both on to MEM. When a jump, a taken branch or a `halt` reaches MEM, it redirects the PC to its target, and the hazard unit from the previous post flushes the three instructions behind it. Doing this from MEM instead of EX costs one cycle extra for every taken branch, but it keeps the rule that nothing changes the state of the machine before MEM, as explained in the pipeline post.

A branch that is not taken does nothing at all: the CPU always assumes a branch is not taken, and simply continues with the next instruction, which it already fetched.

## Simulation

<!-- TODO: screenshot of `fpgc cpu wave cpu_branch`, saved as Website/static/images/sim-branch.png -->

![Branch unit simulation in GTKWave](/images/sim-branch.png "Branch unit simulation in GTKWave")

Here you can see the branch unit in GTKWave. Each case is one branch condition with two values, and in the middle you can see the two compares that all conditions are built from. Like the other units, this is combinational logic, so the clk signal is just for spacing. Some things to look at:

- The first seven cases use the small numbers 1 and 2, where you can see how each condition combines the equal and less-than compares.
- The last four cases compare -1 with 1. As an unsigned number, -1 is `FFFFFFFF`, the largest number there is, so `bgt` is taken and `blt` is not. With the signed versions it is the other way around.

## Next steps

With the branch unit done, all parts are there to build the first version of the pipeline. In the next post, everything we have seen so far finally works together to run actual programs.
