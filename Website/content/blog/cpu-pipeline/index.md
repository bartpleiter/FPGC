---
title: CPU part 1 - Pipeline
date: 2026-10-19T10:00:00
authors:
  - name: Bart Pleiter
    link: https://github.com/bartpleiter
tags: [CPU]
---

The core of any computer is its CPU, and the same goes for the FPGC. It is the component I build everything around, so its design is very important in terms of both functionality and performance.
This is the first post in a series about how I designed and built the CPU for the FPGC project, which is called B32P4, short for Bart's 32-bit Pipelined processor version 4.

<!--more-->

This post will be about the general design and ISA of the B32P4 CPU, which dictates how the components of the CPU (like the ALU or instruction decoder) are structured and interact with each other.
To me, starting here makes the most sense because it you want to have clear what the CPU is supposed to do before you start writing any Verilog code. I wrote this blog post a bit like I have already finished the implementation, but that is mostly due to already having done most of this in the previous iteration: B32P3.

## General design

There are two types of CPUs: RISC (Reduced Instruction Set Computer) and CISC (Complex Instruction Set Computer). I like simplicity, so I opted for a RISC design for the FPGC's CPU. This also aligns with the classic textbook CPUs that you see online, or in my case as well with that processors course I took during my studies. Luckily, you can make a RISC CPU as complex and powerful as you want, as there are many optimizations possible through techniques like pipelining, out-of-order execution, and branch prediction. A good example of such optimized RISC CPU is the RISC-V architecture you start to hear more and more about these days. In any case, the B32P4 CPU follows the RISC philosophy, but keeps things relatively simple compared to more advanced designs.

Inspired by the classic textbook MIPS CPU design, the B32P4 CPU also follows a five-stage structure. These five stages, which it loops forever, are:

1. **Fetch:** read the instruction at the address in the program counter (PC).
2. **Decode:** figure out what the instruction is, and read the registers it needs.
3. **Execute:** do the calculation, or do something else like computing a memory address.
4. **Memory:** read or write memory, if the instruction accesses memory.
5. **Write back:** write the result into the destination register if needed.

Then the PC moves to the next instruction, and it starts again. There are many exceptions to this normal flow, such as instructions that take multiple cycles and use the Memory stage too, or jumps and branches that change the control flow, and more. However, in general, the CPU follows this five-stage loop. The idea is that each stage takes one or more clock cycles to complete. Splitting up tasks per clock cycle is often needed, because logic takes time to propagate through the circuit and needs to arrive in time on the next clock cycle. Also, synchronous memory and registers (flip-flops) require a clock edge to update their values.

The first few iterations of the FPGC CPU implemented these five stages in a way that causes the CPU to handle one stage per clock cycle. While this makes it easy to implement (and is what the first CPUs you learn about in textbooks do), it is not very efficient as for every clock cycle four of the five stages are just idle. To fully utilize the logic in your CPU design, you need to implement something called pipelining.

## Pipelining

By pipelining, the CPU can work on all the stages simultaneously. Every cycle, each instruction moves one stage further, and a new instruction enters at the front. With five stages, five instructions are being worked on at the same time, each in a different stage:

```text
cycle           1    2    3    4    5    6    7    8
add r1 r2 r3    IF   ID   EX   MEM  WB
sub r4 r5 r6         IF   ID   EX   MEM  WB
or  r7 r8 r9              IF   ID   EX   MEM  WB
xor r1 r9 r10                  IF   ID   EX   MEM  WB
```

Each instruction still takes five cycles from start to finish, but one instruction finishes every cycle. This is the main advantage of pipelining: it increases the instruction throughput without necessarily reducing the latency of individual instructions.

The catch is that the instructions in a pipeline are not independent. An instruction often needs the result of the one directly in front of it, which has not finished yet. Or a branch turns out to be taken, while the pipeline has already started on the instructions after it. These types of cases are called hazards, and this is where most of the difficulty of the CPU design comes from. There are different strategies to resolve these hazards. For example, you could let the software part (like a compiler) reorder instructions or add nops to prevent the hazards. However, I sincerely dislike this approach as it requires higher abstraction layers to know what happens at a lower level. My entire philosophy with this project is that once you build a stable layer (like the cpu, assembler, or c compiler), you should be able to build almost anything on top of it in a self-contained manner. Thinking about multiple layers quickly becomes very complex.

To manage the pipeline hazards, we need to implement things like forwarding, stalling, flushing, and other control mechanisms to ensure that instructions are executed correctly despite the overlapping stages. To make this even worse, something that is often overlooked in textbook CPUs, is that memory access is often extremely slow compared to the speed of the CPU, and certain instructions like multiply and division take multiple cycles too. To make things even worse, a L1 cache to combat the low memory access times also introduces its own complexity. All of these together quickly explodes the amount of situations where you can get a hazard, and finding + fixing them in your CPU design becomes a very challenging task. This point is by far the most time and energy I have spent on this project, mostly because I have no prior experience with all of these things. To make things even worse, the design also still has to pass the timing analysis, and the CPU is by far the most timing critical part of the entire FPGC design.

## What I will do differently with B32P4

The previous CPU, B32P3, was also a five-stage pipeline, and somehow I got it to run quite stable barely at 100 MHz. However, the design clearly was reaching its limits and adding any new feature (like DMA) often caused timing issues. After many hotfixes on improving timings the CPU design became too messy. Therefore, with B32P4 I want to clean it up from scratch, and see if I can get make it a lot better now I know in advance this time what I want and what hazards I need to handle properly.

## The five stages

Here is in a bit more detail what each stage does in the B32P4 pipeline:

- **IF (instruction fetch)** holds the PC, and the instruction memory outputs the instruction at that address. IF also computes the address to fetch next: normally PC + 4, or a jump target.
- **ID (instruction decode)** decodes the instruction word into control signals, reads the two source registers from the register file, and computes the target of a branch or jump.
- **EX (execute)** runs the ALU, which does the arithmetic and logic operations. A separate adder computes memory addresses, and the branch compare decides whether a branch is taken.
- **MEM (memory)** handles memory access, and selects the instruction's final result. It's also the stage where an instruction commits: where it really changes the state of the machine. This means that until MEM, the instructions can be flushed or stalled without affecting the machine's state, which is important for hazard handling.
- **WB (write back)** writes the result into the register file (also commits, but this stage is easy and never needs to stall).

## Data hazards: forwarding

Take these two instructions:

```asm
add r1 r2 r3    ; r3 = r1 + r2
sub r3 r4 r5    ; r5 = r3 - r4: needs r3 right away
```

When `sub` reads its registers in ID, `add` is in EX and has only just started calculating `r3`. The register file only gets the new value when `add` reaches WB, two cycles later. So `sub` reads the old value of `r3`.

The obvious fix is to make `sub` wait until `add` is done, but that would cost two cycles on very common code. The better fix is forwarding: the result already exists, it's just not in the register file yet. When `sub` is in EX, `add` is in MEM, and its result sits in the EX/MEM pipeline register. So a multiplexer in front of the ALU picks the operand from one of three places:

| Where the producing instruction is | Where the value comes from |
| --- | --- |
| one instruction ahead (in MEM) | the EX/MEM register |
| two instructions ahead (in WB) | the MEM/WB register |
| three or more ahead | the register file, which already has it |

To control that multiplexer, you compare the source registers of the instruction in EX (or even in ID, which I ended up doing) with the destination registers of the instructions in MEM and WB.

### Load-use hazard

There's one case that forwarding can't solve:

```asm
read 0 r2 r1    ; r1 = memory[r2 + 0]
add r1 r3 r4    ; r4 = r1 + r3: needs the read value
```

The read (or "loaded" as they call in other architectures, which I do not like because read is more clear to me) value only exists at the end of MEM. But by then, `add` would already be in EX, needing it at the start of its cycle. Forwarding can't send a value back in time, so `add` has to wait one cycle in ID. This is called a load-use hazard. The same goes for the other results that are only ready at the end of MEM, such as pops from the stack and multiplications. In this case you cannot get around the one-cycle wait. If you have a really smart assembler/compiler, you can probably prevent this hazard by reordering instructions.

```text
read 0 r2 r1    IF  ID  EX  MEM WB
add r1 r3 r4        IF  ID  ID  EX  MEM WB     waits once in ID
```

## Control hazards: branches

When a branch is taken, the pipeline has already fetched the instructions after it. Those should not be executed, so they have to be thrown away. This is called a flush.

In B32P4, the branch compare happens in EX, and the redirect to the branch target comes from MEM. By then, three new instructions are in IF, ID and EX, and those get flushed:

```text
beq r1 r2 L         IF  ID  EX  MEM WB
(next 3 instr.)         IF  ID  EX  ✗       flushed by the redirect
                            IF  ID  ✗
                                IF  ✗
L: ...                              IF  ID  EX ...
```

So a taken branch costs 4 cycles instead of 1. This is quite a penalty, especially for tight loops. Two choices here are deliberate:

- The redirect comes from MEM, not EX. Redirecting from EX would save a cycle, but this adds complexity and having no committing instructions before MEM makes hazard handling simpler.
- There's no branch prediction. Well, technically we always predict that a branch is not taken. Modern CPUs guess where a branch will go and fetch from there. For modern CPUs this is even more important as their pipelines are typically longer. Perhaps in the future I might investigate adding a simple branch predictor, if timing budget allows it.

## Stalls and flushes

Sometimes a stage can't finish its work in one cycle, for example when a read from memory misses the L1d cache and has to wait for the SDRAM, or because a multiplication/division takes multiple cycles. The CPU is then "busy" (that is how I call the signal in my design to indicate such situation), and the stages behind it have to wait too.

We can think of a simple rule for this: a stage holds/stalls if it's busy, or if any later stage holds/stalls.

So if MEM is busy, MEM, EX, ID and IF all stall. A stalled stage keeps its instruction, but the stage following it might move on, and needs something to work on. That is solved with a valid bit in every pipeline register. When a stage stalls, it sends an instruction with valid = 0 forward. This is called a bubble, and basically just acts like a NOP.

The same bit handles flushes. A flushed instruction simply gets `valid` = 0. It stays in the pipeline, but it's ignored from then on.

Using valid bits instead of actually clearing all the data registers on a stall/flush simplifies timing a lot, because less logic is needed to indicate a stall/flush is happening.

## One commit point

Already mentioned above a little bit, B32P4 explicitly makes sure that committing instructions only happens from MEM and WB. In B32P3 I did not have this explicit and some commits happened in EX, which caused huge complexities with hazard management. Doing it now this way hopefully simplifies this most challenging part of the project a lot. As I am writing this I am still at the start of the CPU, so we will see in later blog posts whether this decision pays off, or if it just reduces performance by waiting until MEM to do something while it could have been done in EX instead.

Again, writing this as a simple rule: an instruction changes the state of the machine only in MEM, except for the register write, which happens in WB.

## Interrupts

Interrupts are a key mechanism in almost any CPU. However, interrupting a pipeline that is potentially stalling or in the middle of a multicycle operation, running the interrupt handler instructions, and restoring the state of the pipeline exactly like it was before sounds as complicated as it is. Therefore, in my pipelined CPU revisions I quickly opted to only act on incoming interrupts during a taken branch or jump. This, since interrupts are basically just jumps that need to return afterward, and jumps already flush the pipeline, completely bypassing this huge complexity (which is also just really hard to test as these interrupts happen practically randomly).

Interrupts are handled in the CPU by jumping to the static interrupt handler address `0x4`, which is in the SDRAM region. To prevent jumping here before a program with interrupt handles is loaded, interrupts are disabled until the program counter is in the SDRAM region. During this jump, the address of the jump that was interrupted is saved so that when the CPU comes across a `reti` (return from interrupt) instruction, it can just jump to that address to continue the normal flow. In the interrupt handler, you can read the ID of the interrupt using the `intid` instruction, which basically just returns a CPU internal register. Thinking about this now, it could actually be replaced by a generic CPU internal register read from the memory map, freeing an instruction opcode. I guess I will do that when I need it. Thinking about it more, a bunch of other instructions could also be replaced by reads or writes, like the `savpc` instruction.

## Error register

Something that is also new (or techincally, that will be new as I still am implementing the CPU) in B32P4 is the error register. Before, if an illegal instruction happened, the CPU would basically just try to continue execution, which mostly worked because lots of illegal instructions just act like a NOP. However, by registering where which illegal instruction occurred (storing the PC and reason/kind of error), it becomes easier to debug and catch hidden bugs. I could even attach an interrupt to this if I want it.

## Full instruction set and details

For the full details on the instruction set and other CPU specifics, you can look at [the CPU documentation](/cpu/isa)

## Next steps

Now the general CPU design is clear, we can start implementing the modules in Verilog. Of course, testing will be crucial and for this I will be using testbenches and simulations using small programs with expected outputs, but more on that in a later blog post.
