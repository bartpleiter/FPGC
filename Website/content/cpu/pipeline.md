---
title: Pipeline
weight: 4
reader: rtl
---

How instructions move through the five stages of B32P4, and how stalls, flushes, forwarding, redirects and interrupts work. The caches have their own page: [Caches](caches.md). The words used here are explained in [Terms](_index.md#terms).

## The stages

Each stage is one module. A stage module contains the pipeline register in front of it, and the logic of the stage.

| Stage | Module | Pipeline register | What it does |
| --- | --- | --- | --- |
| IF | `cpu_if` | the program counter `pc` | computes the next fetch address, reads the ROM and the L1I, and picks the instruction word |
| ID | `cpu_id` | IF/ID (`if_id_*`) | decodes the instruction, reads the register file, adds the PC to the constant for branch and jump targets, and computes the forwarding selects and the late-result stall |
| EX | `cpu_ex` | ID/EX (`id_ex_*`) | the forwarding mux, the ALU, the branch compare, the address adder and its region, the first half of a multiply, the divider, and the decision to take an interrupt |
| MEM | `cpu_mem` | EX/MEM (`ex_mem_*`) | the L1D, the bus, the stack, the CPU-internal I/O registers, the error register, the second half of a multiply, the result mux, and every side effect and redirect |
| WB | `cpu_wb` | MEM/WB (`mem_wb_*`) | writes the register file, and drives the retire port |

When nothing waits, one instruction enters the pipeline and one finishes in every cycle:

```text
cycle           1    2    3    4    5    6    7    8
add r1 r2 r3    IF   ID   EX   MEM  WB
sub r4 r5 r6         IF   ID   EX   MEM  WB
or  r7 r8 r9              IF   ID   EX   MEM  WB
xor r1 r9 r10                  IF   ID   EX   MEM  WB
```

## Valid bits

Every pipeline register has a valid bit (`if_id_valid`, `id_ex_valid`, `ex_mem_valid`, `mem_wb_valid`) that says whether it holds an instruction. A slot with valid 0 is a bubble.

- Throwing an instruction away only clears its valid bit. The rest of the pipeline register keeps whatever it had, and nothing looks at it.
- Every signal that does something is gated by the valid bit: the busy signals, the requests, the writes and the redirects.
- Reset clears all valid bits.

## Stall and flush

All stall and flush signals come from one small module, `cpu_hazard`. Each stage reports whether it's busy, and `cpu_mem` reports the redirect. Nothing else in the CPU decides about stalls.

| Signal | From | Busy when |
| --- | --- | --- |
| `busy_if` | IF | the PC is outside the ROM, and the L1I has no current hit and the word isn't arriving from SDRAM |
| `busy_id` | ID | the late-result stall |
| `busy_ex` | EX | the write-then-access stall, the push-then-pop stall, or the divider is running |
| `busy_mem` | MEM | a bus access or a read of a CPU-internal register isn't done yet, or `ccache` is waiting for the cache controller |
| `l1d_miss` | MEM | a read or write of SDRAM has no current hit, and its word isn't arriving from SDRAM |
| `l1d_wait` | MEM | the part of `l1d_miss` that is known from registers, for IF (see [below](#the-l1d-miss-and-if)) |

The stall rule is that a stage stalls when it, or any stage after it, is busy. So a busy MEM stalls everything before it, and a busy ID stalls ID and IF, while EX, MEM and WB move on. WB never stalls.

A pipeline register is flushed (its valid bit cleared) in two cases:

- A redirect flushes IF/ID, ID/EX and EX/MEM, so the three instructions in IF, ID and EX.
- When a stage stalls but the stage after it moves on, the stage after it gets a bubble instead of a copy of the stalled instruction. MEM/WB gets a bubble whenever MEM stalls.

For example, a late-result stall keeps `add` in ID for a cycle, while `read` moves on. EX gets a bubble in cycle 4:

```text
cycle            1    2    3    4    5    6    7
read 0 r2 r1     IF   ID   EX   MEM  WB
add r1 r3 r4          IF   ID   ID   EX   MEM  WB
```

A stall in EX works the same way, one stage later. A divide stays in EX for 34 cycles, while the instructions behind it wait in IF and ID, and MEM gets a bubble every cycle.

Two consequences of the stall rule that the rest of the design relies on:

- When an instruction moves from ID into EX, the instructions in EX and MEM move too. So when ID looks at the instructions in EX and MEM, it knows where they will be when its own instruction is in EX.
- A stalled MEM never redirects. Only an access to memory or `ccache` can stall MEM, and of those only `ccache` redirects, once it's done.

## Forwarding

A result is written into the register file in WB, but the next instructions need it earlier. Forwarding gives it to them from the pipeline registers:

| Where the producing instruction is, when the consumer is in EX | Where the value comes from |
| --- | --- |
| MEM (one instruction ahead) | `ex_mem_result`, the ALU result in EX/MEM |
| WB (two instructions ahead) | `mem_wb_value`, the result in MEM/WB |
| finished (three or more ahead) | the register file, which already has it |

- The selects are computed in ID and registered into ID/EX (`id_ex_areg_from_mem`, `id_ex_areg_from_wb`, and the same for breg). `cpu_forward` compares the source registers of the instruction in ID with the destination registers of the instructions in EX and MEM. Because of the stall rule, those instructions are in MEM and WB by the time the instruction is in EX. In EX, the forwarding mux is then a 3:1 mux with selects from registers.
- When both match, the younger instruction wins: a match with the instruction in EX goes before one with the instruction in MEM.
- The register file writes through: a register that WB writes in the same cycle as ID reads it reads as the new value. That covers the producer three instructions ahead.
- `r0` is never forwarded, because an instruction that writes `r0` doesn't count as writing a register.

A stalled EX keeps what was forwarded to it. EX can stall while MEM and WB move on (during a divide, or a write-then-access stall). The values it was forwarding then move on into the register file, and the mux would lose them. So while EX stalls, ID/EX takes in its own forwarded operands, and clears its selects. From then on, EX uses those operands.

## The late-result stall

Some results only exist at the end of MEM: reads, pops, multiplies, divides and `readintid`. These are late results. Forwarding from MEM gives the ALU result in EX/MEM, so it can't give a late result one instruction later. The instruction that needs it waits one cycle in ID (`busy_id`), and then gets it from WB:

```text
cycle            1    2    3    4    5    6    7
mults r1 r2 r3   IF   ID   EX   MEM  WB
add r3 r4 r5          IF   ID   ID   EX   MEM  WB      r3 from MEM/WB
```

- It only looks at the instruction directly ahead (in EX). Two instructions ahead, the late result is in MEM/WB in time.
- A write whose data is a late result waits too, even though it only needs the data in MEM. That's rare, and one rule is simpler than two.

## Stalls for memory

Two more stalls come from how the memories are built. Both are part of `busy_ex`, and both cost one cycle.

In the write-then-access stall, a read or write in EX waits while a write is in MEM. The L1D's port has one address. While a write is in MEM, that address is the write's own, so the L1D can't read the address of the read or write behind it in EX. This also orders memory: a read right behind a write to the same address sees the new value.

```text
cycle            1    2    3    4    5    6    7
write 0 r2 r1    IF   ID   EX   MEM  WB
read 4 r2 r3          IF   ID   EX   EX   MEM  WB
```

In the push-then-pop stall, a `pop` in EX waits while a `push` is in MEM. The stack's block RAM reads the word below the stack pointer at every edge, and a `push` writes that word at the same edge. The block RAM would give the old value.

## The L1D miss and IF

An L1D miss is only known late in the cycle, after the tag compare in MEM. Sending it all the way to IF's clock enables and program counter is too slow. So IF doesn't wait for the miss itself:

- IF stalls for `l1d_wait`: the part of the miss that is known from registers. That's when MEM's L1D output isn't current, which is the case in every cycle of a miss except the first.
- In the first cycle of a miss, IF may move on while ID stalls. The instruction that IF had is then lost, because ID doesn't take it.
- `cpu_hazard` sees this (`refetch_if`), and IF fetches that instruction again in the next cycle, at `pc - 4`.

The refetch happens while MEM still waits for its line, so it doesn't make the miss slower.

## Redirects

Every change of the program counter other than PC + 4 is a redirect, and every redirect comes from MEM, from registered bits:

| Instruction | Target |
| --- | --- |
| `jump`, `jumpr`, a taken branch | its target |
| `halt` | its own address |
| `ccache`, `ccached` | the next instruction, once the cache controller is done |
| `reti` | `PC_BACKUP` |
| an interrupt, in place of a jump, `jumpr`, taken branch or `halt` | `0x4` |

- The target is computed before MEM. ID computes PC + constant (for branches, `jumpo`, `jumpro` and `savpc`), or takes the constant itself (for `jump`). EX decides whether a branch is taken, and computes the target of `jumpr` and `jumpro` with the address adder. Both go into EX/MEM.
- A redirect flushes the three instructions behind it, in IF, ID and EX. So it costs 3 cycles.
- IF uses the target as its fetch address in the same cycle, so the instruction at the target is in IF in the next cycle.
- Bits [1:0] of the target are cleared before IF uses it. A target that isn't a multiple of 4, or that isn't in SDRAM or the ROM, sets the bad-jump-target flag of the error register.

```text
cycle            1    2    3    4    5    6    7    8
beq r1 r2 L      IF   ID   EX   MEM  WB
(next)                IF   ID   EX   -                 flushed
(next + 1)                 IF   ID   -                 flushed
(next + 2)                      IF   -                 flushed
L: ...                               IF   ID   EX   MEM
```

## Interrupts

- `cpu_interrupts` (in MEM) synchronizes the inputs, detects rising edges, and keeps a pending bit per interrupt. The lowest pending number goes first.
- The decision is made in EX: an interrupt is taken when the instruction in EX redirects (a jump, `jumpr`, taken branch or `halt`), an interrupt is pending, and interrupts are enabled. The decision is registered into EX/MEM.
- In MEM, the interrupt takes the place of the jump. The CPU redirects to `0x4` instead of the jump's target, sets `PC_BACKUP` to the address of the jump, disables interrupts, remembers the interrupt's number for `readintid`, and clears its pending bit. The jump itself doesn't execute, and runs again after `reti`.
- This is why interrupts are only taken at jumps and branches. A jump already flushes the instructions behind it, so taking an interrupt there needs no extra flushing, and no instruction is interrupted halfway.
- Interrupt entry and `reti` change the enable bit, and both redirect, which flushes the instruction in EX that might have decided with the old value. A write to `INT_ENABLE` doesn't redirect. So the instruction right behind it may still decide with the old value, which the instruction set allows.

## Commit and side effects

An instruction commits in the cycle it leaves MEM. Before that, it can still be flushed. After that, it can't be cancelled, and every older instruction has committed too. So all side effects happen in MEM or WB:

| Side effect | Instructions | When |
| --- | --- | --- |
| register write | ALU, `savpc`, reads, `pop`, multiply, divide, `readintid` | WB |
| L1D write | a write to SDRAM that hits | MEM, at commit |
| bus request | a read or write over the bus | MEM, in its first cycle, once |
| cache controller request | an L1D miss, `ccache`, `ccached` | MEM, in its first cycle, once |
| stack write and stack pointer | `push`, `pop`, a write to `SP` | MEM, at commit |
| `PC_BACKUP`, interrupt enable, interrupt number | interrupt entry, `reti`, writes to these registers | MEM, at commit |
| error register | any instruction with an illegal event | MEM, at commit |
| redirect | see [Redirects](#redirects) | MEM |

- IF, ID and EX never change anything that software can see. They may read block RAMs early, because a read of a block RAM has no side effect. They never start a bus access, because reading a device can have one.
- A request that takes several cycles is made once, in the first cycle in MEM, and a flag (`bus_started`, `clear_started`) keeps it from being made again while MEM waits.
- There are two commit signals. `commit` waits for the late L1D miss, and is only used for the L1D write itself. Every other side effect uses `commit_early`, which doesn't depend on the L1D miss. For those instructions the miss is 0 anyway, and this keeps the late tag compare away from the stack, the I/O registers and the error register.
- Reads of the CPU's own state happen in MEM: `SP`, `PC_BACKUP`, the error register, `INT_ENABLE` and `readintid`. Every older instruction has committed by then, so these reads need no forwarding.

## Reads in MEM

The value that MEM passes to WB comes from a result mux, a chain that picks the first that applies:

1. a read: the word from the read mux, aligned and extended for a byte or halfword read
2. a `pop`: the top of the stack
3. a multiply: the second half of the multiplier
4. a divide: the divider's result
5. `readintid`: the last interrupt number
6. anything else: the ALU result from EX/MEM

The read mux has three inputs:

| Input | When |
| --- | --- |
| the word on the SDRAM port | early restart: the read's word arrives from SDRAM in this cycle |
| the L1D | a read of SDRAM that hits |
| `slow_word` | every other read: the bus, a CPU-internal register, or 0 for an unmapped address |

The slow path handles everything outside SDRAM. It makes MEM busy until the word is in the register `slow_word`, and the read uses that register in the next cycle. A read of a CPU-internal register takes one extra cycle this way. A bus access takes two more than the device needs: one to make the request from a register, and one to use the answer. Writes outside SDRAM wait for the bus's answer too. Together with the word, the register `slow_error` keeps whether the answer had `bus_error`, and the instruction then sets the bad-address flag when it finishes, like any other error.

## Reset

The reset is synchronous.

- All valid bits are cleared, and IF fetches `0x04000000` during the reset.
- Cleared to 0: `SP`, `PC_BACKUP`, the interrupt number, the pending interrupts and the interrupt enable.
- Not reset: the register file (`r1`–`r15` are undefined) and the error register, which starts at 0 when the FPGA is configured and keeps its value over a reset.
- The cache controller empties both caches after the reset. See [Caches](caches.md#reset).
