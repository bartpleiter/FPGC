# CPU Pipeline

The B32P3 uses a classic 5-stage pipeline: IF, ID, EX, MEM, WB. In ideal conditions, one instruction completes every cycle. In practice, hazards, cache misses, and multi-cycle operations insert stalls. This page explains how the pipeline actually works, with cycle-by-cycle timing examples for the interesting cases.

## The Five Stages

Each instruction flows through these stages in order:

| Stage | What happens |
|-------|-------------|
| **IF** | Read instruction from ROM or L1I cache. Advance PC. |
| **ID** | Decode opcode and register fields. Start register file read. |
| **EX** | ALU operation. Forwarding MUXes select operands. Memory address calculated. |
| **MEM** | Load/store to cache, VRAM, or I/O. Branch/jump resolved. |
| **WB** | Write result back to register file. |

A subtle detail: the register file has two pipeline stages internally. Addresses are captured in IF, and data arrives in EX (not ID). This means the register file read spans IF through EX, but the pipeline register naming still follows the traditional convention.

### Normal Flow

Here's what five back-to-back instructions look like when everything hits:

```text
Cycle:    1    2    3    4    5    6    7    8    9
instr 1: [IF] [ID] [EX] [MEM][WB]
instr 2:      [IF] [ID] [EX] [MEM][WB]
instr 3:           [IF] [ID] [EX] [MEM][WB]
instr 4:                [IF] [ID] [EX] [MEM][WB]
instr 5:                     [IF] [ID] [EX] [MEM][WB]
```

One instruction enters the pipeline each cycle, and one retires each cycle. The throughput is 1 IPC (instruction per clock).

## Data Forwarding

Without forwarding, an instruction that reads a register written by the previous instruction would get stale data. The pipeline has two forwarding paths to handle this:

- **EX/MEM forward**: The result from last cycle's EX stage (now in the EX/MEM pipeline register) is forwarded to this cycle's EX stage inputs.
- **MEM/WB forward**: The result from two cycles ago (now in MEM/WB) is forwarded similarly.

Each ALU input has a 3-way MUX:

```text
forward = 00 → use register file output (no forwarding needed)
forward = 01 → use EX/MEM result (1-cycle-ago instruction)
forward = 10 → use MEM/WB result (2-cycles-ago instruction)
```

EX/MEM forwarding has priority over MEM/WB, because it's the more recent value. There's one restriction: EX/MEM forwarding is disabled when the source instruction is a load or pop. Their results aren't available until after the MEM stage, so forwarding from EX/MEM would send the wrong value.

### Forwarding Example

```text
Instr:   add r1 r2 r3         ; r1 + r2 → r3
         sub r3 r4 r5         ; r3 - r4 → r5 (needs forwarding)

Cycle:    1    2    3    4    5    6
add:     [IF] [ID] [EX] [MEM][WB]
sub:          [IF] [ID] [EX] [MEM][WB]
                         ↑
                     sub is in EX, add is in MEM.
                     forward_a = 01 → r3 comes from EX/MEM register.
                     No stall needed.
```

Without forwarding, sub would need to wait 2 extra cycles for add to reach WB. With forwarding, zero stalls.

### Register File Write-Through

There's one more forwarding path inside the register file itself. When WB writes to a register at the same time ID reads it, the register file forwards the write data directly. This covers the case where an instruction depends on one from three cycles ago, which would otherwise read stale data from the register array.

## Hazards and Stalls

Some situations can't be solved by forwarding alone. The pipeline detects three types of hazards and inserts stalls (bubbles) to resolve them.

### Load-Use Hazard

When an instruction reads a register that's being loaded by the immediately preceding instruction, the loaded data won't be available until after MEM. Forwarding can't help because the data literally doesn't exist yet when EX needs it.

The pipeline stalls for one cycle, inserting a bubble:

```text
Instr:   read 0 r1 r3        ; load from [r1+0] into r3
         add r3 r4 r5        ; uses r3 (load-use hazard!)

Cycle:    1    2    3    4    5    6    7
read:    [IF] [ID] [EX] [MEM][WB]
add:          [IF] [ID] [  ] [EX] [MEM][WB]
                    ↑         ↑
              add stalls     add resumes in EX.
              in ID for      r3 is now available via
              1 cycle.       MEM/WB forwarding (forward=10).
```

The hazard detector sees that the instruction in EX (read) writes to a register that the instruction in ID (add) needs, and read is a memory load. It inserts a 1-cycle stall: IF and ID freeze, and a bubble (invalid instruction) enters EX.

### Pop-Use Hazard

Same concept as load-use, but for POP instructions. POP data comes from the hardware stack, which also has 1-cycle read latency. The detection and resolution are identical: 1-cycle stall with a bubble.

### Cache Line Hazard

When two back-to-back SDRAM memory instructions target different cache lines, they would both need the cache controller, which can only handle one request at a time. The pipeline detects this by comparing cache line indices (bits [9:3] of the address) between the instruction in EX (about to enter MEM) and the instruction currently in MEM:

```text
Instr:   read 0 r1 r3        ; SDRAM access, cache line A
         read 0 r2 r5        ; SDRAM access, cache line B (different!)

Cycle:    1    2    3    4    5    6    7    8
read1:   [IF] [ID] [EX] [MEM][WB]
read2:        [IF] [ID] [  ] [EX] [MEM][WB]
                         ↑
                   Bubble inserted.
                   read1 must finish MEM
                   before read2 enters MEM.
```

Note that the detection happens in EX before the second instruction moves to MEM. Neither instruction has started its memory access in EX; the hazard is about preventing a conflict in MEM.

This hazard behaves differently from load-use. The cache line hazard stalls IF, ID, and EX, but lets MEM and WB continue. This is critical, because the first instruction needs to complete its memory access in MEM to clear the hazard. If MEM were also stalled, the pipeline would deadlock.

#### Shadow Registers: Preserving Forwarded Values During EX Stalls

The cache line hazard creates a subtle interaction with data forwarding. Because it stalls EX while letting MEM and WB advance, forwarding sources can disappear while EX still needs them.

Consider this sequence:

```text
Instr:   sub r1 1 r1         ; ALU write to r1
         write 0 r11 r1      ; SDRAM write (uses r1), cache line A
         write -1 r14 r1     ; SDRAM write (uses r1), cache line B

Cycle:    1    2    3    4    5    6    7    8    9   10
sub:     [IF] [ID] [EX] [MEM][WB]
write1:       [IF] [ID] [EX] [MEM][  ] [  ] [WB]
write2:            [IF] [ID] [EX] [  ] [  ] [EX] [MEM][WB]
                              ↑    ↑
                        EX stalled. MEM/WB advance.
```

Here's what happens step by step:

1. In cycle 4, sub is in MEM and write1 is in EX. Forward `b = 01` delivers sub's result from EX/MEM to write1. All correct.
2. In cycle 5, sub reaches WB. Write2 is in EX with forward `b = 10` (from MEM/WB). Write1 starts its SDRAM access in MEM.
3. In cycle 6, write1 is still in MEM (multi-cycle SDRAM write) and write2 is stalled in EX. But sub has left WB — its result is no longer in any pipeline register.
4. When the cache line hazard holds EX one more cycle (cycle 7), MEM/WB advances, overwriting sub's result.
5. In cycle 8, write2 finally enters EX with forward `b = 00` (no forwarding). It falls back to the register file, which holds the **stale** pre-sub value.

The fix is a pair of **shadow registers** (`ex_stall_saved_a` and `ex_stall_saved_b`) that capture forwarded values while EX is stalled:

- When `ex_pipeline_stall` is active and `forward_a` or `forward_b` is non-zero, the forwarded value is saved into the shadow register and a valid flag is set.
- When the stall clears, the valid flags are reset.
- The forwarding MUX chain becomes: active pipeline forward → shadow register (if valid) → register file output.

This ensures that once a forwarded value is delivered to EX, it's preserved even if the forwarding source advances out of the pipeline during a multi-cycle stall.

## The Three-Tier Stall Architecture

The pipeline has three separate stall signals, each freezing a different subset of stages:

```text
              IF     ID     EX     MEM    WB
             ─────  ─────  ─────  ─────  ─────
Front-end:     ■      ■
Mid-stage:                   ■
Back-end:                           ■      ■
```

The three stall signals are:

- **`pipeline_stall`** (front-end): Freezes IF and ID.
- **`ex_pipeline_stall`** (mid-stage): Freezes EX.
- **`backend_pipeline_stall`** (back-end): Freezes MEM and WB.

Each signal is a combination of lower-level stall sources:

- **`backend_pipeline_stall`** = cache miss (IF or MEM) | multi-cycle ALU | Memory Unit I/O | cache clear | VRAMPX FIFO full
- **`ex_pipeline_stall`** = `backend_pipeline_stall` | cache line hazard
- **`pipeline_stall`** = `ex_pipeline_stall` | load-use hazard | pop-use hazard

This hierarchy means a load-use hazard stalls IF+ID but lets EX, MEM, WB continue. A cache miss stalls everything. A cache line hazard stalls IF+ID+EX but lets MEM+WB finish. Each type of stall freezes exactly the right stages.

### Bubble Insertion

When a stage is stalled but the next stage isn't, a bubble (invalid instruction) must be inserted into the gap. The pipeline register update logic handles this:

- **Load/pop hazard**: `pipeline_stall` is active but `ex_pipeline_stall` is not. The ID/EX register clears its valid/control signals, creating a bubble in EX. The instruction in ID stays put and re-enters EX next cycle.
- **Cache line hazard**: `ex_pipeline_stall` is active but `backend_stall` is not. The EX/MEM register clears its valid/control signals, creating a bubble in MEM. The instruction in EX stays put.

## Branches and Flushes

Branches and jumps are resolved in the MEM stage by the Branch/Jump Unit. This is later than some designs (which resolve in EX or even ID), and it means taken branches have a 3-cycle penalty: the three instructions that entered the pipeline after the branch must be flushed.

```text
Instr:   beq r1 r2 target     ; branch taken
         add r1 r2 r3         ; in pipeline, must be flushed
         sub r3 r4 r5         ; in pipeline, must be flushed
         or r5 r6 r7          ; in pipeline, must be flushed
         (target):            ; fetched after redirect

Cycle:    1    2    3    4    5    6    7    8    9
beq:     [IF] [ID] [EX] [MEM][WB]
add:          [IF] [ID] [EX]  ×     (flushed in cycle 5)
sub:               [IF] [ID]  ×     (flushed in cycle 5)
or:                      [IF]  ×     (flushed in cycle 5)
target:                       [IF] [ID] [EX] [MEM][WB]
```

When MEM detects a taken branch (via `pc_redirect`), three flush signals fire simultaneously:

- `flush_if_id`: Clears the IF/ID register (kills the instruction in ID)
- `flush_id_ex`: Clears the ID/EX register (kills the instruction in EX)
- `flush_ex_mem`: Clears the EX/MEM register (kills the instruction that just left EX)

Meanwhile, IF receives the redirect target and starts fetching from the new address. There's a 1-cycle delay before the first valid instruction from the target appears, because ROM and cache have 1-cycle read latency. The IF stage tracks this with a `redirect_pending` flag.

### The Redirect Pending Mechanism

When the PC changes due to a redirect, interrupt, or RETI, the instruction fetch pipeline needs one extra cycle before valid data appears:

1. **Redirect cycle**: PC is set to the new target. ROM/cache address inputs update.
2. **Next cycle** (`redirect_pending` = 1): ROM/cache outputs now contain the instruction at the new PC. The pipeline captures it and starts normal flow. `redirect_pending` clears.

During the redirect pending cycle, IF outputs an invalid instruction (the IF/ID register stays cleared). This is essentially a 1-cycle fetch bubble on every taken branch, built into the 3-cycle penalty.

### Why Resolve in MEM?

Resolving branches in MEM instead of EX costs an extra flush cycle, but it has two practical benefits. First, it gives the branch operands one more pipeline stage to become available through forwarding. In a design that resolves in EX, a branch depending on the immediately preceding instruction faces a data hazard. By pushing resolution to MEM, forward paths can supply the operands in almost all cases without additional stalls. Second, moving the branch logic out of EX shortened a critical path that was preventing the design from meeting 100 MHz timing on the Cyclone IV FPGA.

## Cache Miss Recovery

The most interesting pipeline interaction is what happens when a cache miss occurs mid-flight, especially when combined with a redirect. Let me walk through several scenarios.

### IF Cache Miss (Normal)

When the instruction fetch hits a cache miss in L1I, the pipeline stalls everything:

```text
Cycle:    1    2    3    4   ...   N   N+1  N+2
Instr A: [IF]  stall stall stall      [IF] [ID] [EX] ...
                                        ↑
                                  Cache controller returns data.
                                  cache_stall_if drops.
                                  IF captures the result and proceeds.
```

The cache stall signal (`cache_stall_if`) feeds into `backend_stall`, which freezes the entire pipeline. The L1I cache controller starts fetching the cache line from SDRAM. When it finishes (which could take many cycles depending on SDRAM latency and whether a dirty line needs writeback), the instruction data becomes available and the pipeline resumes.

During the stall, `pc_delayed` holds steady so the cache controller knows which address to fetch. When the controller signals done, IF selects the freshly fetched data through the instruction MUX rather than the (still-invalid) cache output.

### IF Cache Miss After Redirect

What happens when a branch redirects the PC to an address that misses in L1I, especially if there's already a cache fetch in flight?

```text
Cycle:    1    2    3    4    5    6   ...  N
beq:     [IF] [ID] [EX] [MEM]
add:          [IF] [ID]  [EX]  ×
sub:               [IF]  [ID]  ×
redirect:                      [IF]  stall ...  [IF] [ID] ...
                                ↑
                          PC set to target.
                          redirect_pending = 1.
                          Cache lookup starts on target address.
```

Here's the sequence of events:

1. In cycle 4, MEM detects the branch is taken. `pc_redirect` fires.
2. IF sets PC to the target and sets `redirect_pending = 1`.
3. In cycle 5, `redirect_pending` is active. IF checks the L1I cache for the target address. Two outcomes:
   - **Cache hit**: `redirect_pending` clears, PC advances, pipeline resumes normally.
   - **Cache miss**: `redirect_pending` stays set. `cache_stall_if` activates. The pipeline freezes while the cache controller fetches the line.
4. The cache controller processes the miss. Note that `l1i_cache_controller_flush` is hardwired to 0; the controller doesn't get flushed on redirects. There's no need to, because it only starts a new fetch when `cache_stall_if` is asserted, which already reflects the new address.
5. When the controller signals done, the instruction data is available, `redirect_pending` clears, and the pipeline resumes from the target.

The key insight is that `redirect_pending` prevents any instructions from entering the pipeline while the redirect is being processed. Even if the cache miss takes many cycles, no garbage instructions leak through. And the cache controller naturally handles the address change because `l1i_cache_controller_addr` always tracks `pc_delayed`, which updates to the new target on the first `redirect_pending` cycle.

### MEM Cache Miss (L1D)

Data cache misses work differently because they stall a later stage:

```text
Cycle:    1    2    3    4    5   ...  N   N+1
read:    [IF] [ID] [EX] [MEM] stall ...  [MEM][WB]
next:         [IF] [ID] [EX]  stall ...  [EX] [MEM] ...
next+1:            [IF] [ID]  stall ...  [ID] [EX]  ...
```

When MEM has a cache miss, `cache_stall_mem` fires, which is part of `backend_stall`. The entire pipeline freezes. The L1D cache controller fetches (and possibly writes back) the cache line. When it reports done, the stall drops and the instruction's result is available from either the freshly fetched data or the now-valid cache.

There's a subtlety with L1D cache timing: the cache DPRAM has 1-cycle read latency, so when an instruction first enters MEM, the cache output isn't valid yet. The pipeline waits one extra cycle (`l1d_cache_read_done` flag) before checking for a hit. This means every SDRAM access takes at least 2 cycles in MEM, even on a cache hit. On a miss, the additional latency depends on SDRAM controller response time and whether a dirty line needs writeback.

## Multi-Cycle ALU

Multiply and divide operations use a dedicated multi-cycle ALU. Division takes about 32 cycles (one bit per cycle, sequential divider). Multiplication takes about 4 cycles using the FPGA's built-in DSP multiplier blocks.

The multi-cycle ALU has its own state machine:

1. **IDLE**: Waiting for an `arithm`/`arithmc` instruction in EX.
2. **STARTED**: Start signal pulsed to the ALU for one cycle.
3. **DONE**: Waiting for the ALU to signal completion.

While the ALU is computing, `multicycle_stall` is asserted, which feeds into `backend_stall` and freezes the entire pipeline. When the ALU finishes, the result is captured and forwarded through the normal EX result path.

```text
Cycle:    1    2    3    ...  34   35
divs:    [IF] [ID] [EX]  stall   [EX] [MEM][WB]
next:         [IF] [ID]  stall        [EX] ...
```

A nice implementation detail: the multi-cycle ALU result is captured in `malu_result_reg`, but on the same cycle that `malu_done` fires, the pipeline uses `malu_result` directly (the combinational output) rather than the registered value. This avoids needing an extra cycle to register the result before the EX stage can use it.

## Interrupt Delivery

Interrupts are only accepted when a jump or branch instruction is being taken in MEM. This is a deliberate constraint that greatly simplifies interrupt handling in the pipeline.

The reasoning: if interrupts could fire at any time, the pipeline would need to handle partial instruction execution, speculative state rollback, and complex flush interactions. By restricting delivery to jump/branch cycles, the pipeline is already in a "known clean" state: the jump instruction is about to flush IF, ID, and EX anyway. The interrupt just redirects the flush target from the branch destination to the interrupt handler.

When an interrupt fires:

1. **Validation**: `interrupt_valid` checks all conditions (pending interrupt, not disabled, running from SDRAM, jump executing in MEM).
2. **PC backup**: The current `ex_mem_pc` is saved to `pc_backup` (accessible at `0x7C00000`).
3. **Redirect**: PC jumps to the interrupt handler address (`0x0000001`).
4. **Disable**: `int_disabled` is set, preventing nested interrupts.
5. **Flush**: IF/ID, ID/EX, and EX/MEM are all flushed (same flushes as a normal branch).

The interrupt handler runs, determines the interrupt source with INTID, services it, then executes RETI. RETI restores the PC from `pc_backup`, re-enables interrupts, and flushes the pipeline to start fetching from the return address.

### RETI and Branch Interaction

There's a subtle interaction: what if a taken branch is about to flush a RETI instruction? Without special handling, the RETI would execute (restoring the PC and re-enabling interrupts) even though the branch should have skipped it. The pipeline handles this by gating `reti_valid` with `!pc_redirect`. A RETI in EX is suppressed if a branch in MEM is simultaneously redirecting the PC.
