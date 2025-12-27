# B32P2 CPU: Problem Statement and Analysis

## Executive Summary

This document outlines the critical challenges faced by the B32P2 CPU design in the FPGC project, specifically focusing on hazard complexity, timing constraints, and maintainability issues that currently limit the design to ~50MHz on Cyclone IV FPGAs when 100MHz should be achievable.

## Project Context

FPGC is a from-scratch computer system designed for educational purposes, implementing:
- Custom 32-bit RISC CPU (B32P2)
- Custom C compiler (B32CC)
- Custom assembler (ASMPY)
- Operating system (BDOS)
- GPU with pixel rendering
- SDRAM controller with L1 caches

**Primary Goal**: Run Doom on custom hardware while learning every abstraction layer from CPU ISA to operating system.

**Hard Requirements**:
1. Must be designed from scratch (no existing soft-CPU cores)
2. Must maintain RISC ISA (major assembler/compiler changes unacceptable)
3. Performance must equal or exceed current design
4. Must achieve ≥100MHz on Cyclone IV FPGA

## Current Architecture: B32P2

### Pipeline Structure

B32P2 implements a 6-stage pipeline:

```
┌─────┐    ┌─────┐    ┌─────┐    ┌────────┐    ┌────────┐    ┌────┐
│ FE1 │───►│ FE2 │───►│ REG │───►│ EXMEM1 │───►│ EXMEM2 │───►│ WB │
└─────┘    └─────┘    └─────┘    └────────┘    └────────┘    └────┘
  │           │          │           │             │           │
  ▼           ▼          ▼           ▼             ▼           ▼
L1i Cache  Cache Miss  Register   Execute +     Multi-cycle  Register
  Fetch      Handle     Read     L1d Access    Operations   Writeback
```

**Stage Responsibilities**:

| Stage | Primary Function | Secondary Functions |
|-------|-----------------|---------------------|
| FE1 | L1i cache fetch | ROM access, PC management |
| FE2 | Cache miss handling | Cache controller interface |
| REG | Register read | Instruction decode, address pre-computation |
| EXMEM1 | ALU execution | L1d cache access, branch resolution |
| EXMEM2 | Multi-cycle ops | Cache miss handling, IO, division |
| WB | Register writeback | Result selection |

### Variable-Latency Operations

The core problem stems from instructions with different execution latencies:

| Operation Type | Latency | Stage | Notes |
|---------------|---------|-------|-------|
| ALU (single-cycle) | 1 cycle | EXMEM1 | ADD, SUB, OR, AND, etc. |
| L1d cache hit (read) | 1 cycle | EXMEM1 | Data available in EXMEM2 |
| L1d cache miss (read) | 10-20+ cycles | EXMEM2 | SDRAM burst + cache line fill |
| L1d write | 10-20+ cycles | EXMEM2 | Write-through to SDRAM |
| Division | 32+ cycles | EXMEM2 | Iterative algorithm |
| Multiplication | 3-4 cycles | EXMEM2 | DSP block or behavioral |
| IO access | 2+ cycles | EXMEM2 | Memory-mapped peripherals |
| Stack push/pop | 1 cycle | EXMEM2 | Hardware stack, result in WB |

### Current Hazard Detection Complexity

The B32P2 implements three primary hazard detection mechanisms:

#### Hazard 1: Load-Use Hazard (Stall + Flush EXMEM1)
```verilog
wire hazard_load_use;
assign hazard_load_use =
    (exmem2_result_in_wb || exmem2_result_ready) && 
    dep_only_on_exmem2;
```

#### Hazard 2: Pop + WB Forwarding Conflict (Full Pipeline Flush)
```verilog
wire hazard_pop_wb_conflict;
assign hazard_pop_wb_conflict =
    exmem2_result_in_wb &&
    dep_exmem1_on_exmem2 &&
    dep_exmem1_on_wb;
```

#### Hazard 3: Multi-cycle Dependency (Flush Early Stages)
```verilog
wire hazard_multicycle_dep;
assign hazard_multicycle_dep =
    exmem2_multicycle_busy && (
        dep_exmem1_on_exmem2 ||
        dep_exmem1_on_wb
    );
```

### Forwarding Paths

Current forwarding logic creates long combinatorial paths:

```verilog
// EXMEM2 → EXMEM1 forwarding
assign forward_a = (areg_EXMEM1 == dreg_EXMEM2 && areg_EXMEM1 != 4'd0) ? 2'd1 :
                   (areg_EXMEM1 == dreg_WB && areg_EXMEM1 != 4'd0) ? 2'd2 :
                   2'd0;

// Input selection (forms critical timing path)
assign alu_a_EXMEM1 = (forward_a == 2'd1) ? data_d_EXMEM2 :
                      (forward_a == 2'd2) ? data_d_WB :
                      data_a_EXMEM1;
```

**Critical Path Analysis**:
```
dreg_EXMEM2 → forward_a → alu_a_EXMEM1 → ALU → alu_y_EXMEM1 → result_EXMEM1
     ↑                           ↑
     └── data_d_EXMEM2 ──────────┘ (large mux from cache/IO/ALU results)
```

## Problem 1: Hazard Complexity Explosion

### Root Cause
Variable-latency operations in EXMEM2 create a combinatorial explosion of possible pipeline states:

**State Space Analysis**:
- FE2 states: {normal, cache_miss_waiting, cache_miss_done}
- EXMEM2 states: {idle, cache_miss, cache_hit, malu_busy, malu_done, io_busy, io_done}
- WB states: {normal, has_result, bubble}

**Possible Hazard Combinations**: ~3 × 7 × 3 = 63 unique states

Each state requires analysis for:
- Forward dependency from EXMEM2
- Forward dependency from WB
- Stall propagation requirements
- Flush requirements

### Manifestation

1. **Bug-prone development**: Small changes can introduce subtle hazards
2. **Hard to verify**: 63+ states make exhaustive testing difficult
3. **Hard to extend**: Adding debugging support multiplies state space

### Evidence from Codebase

```verilog
// Stall signals - note the complex conditions
assign stall_FE1 = hazard_load_use || malu_stall || l1i_cache_miss_FE2 || 
                   l1d_cache_wait_EXMEM2 || mu_stall || cc_stall;

// Flush signals - different conditions than stalls
assign flush_FE1 = jump_valid_EXMEM1 || reti_EXMEM1 || interrupt_valid || 
                   hazard_multicycle_dep || hazard_pop_wb_conflict;

// Result availability - complex conditional
wire exmem2_result_ready = 
    (mem_read_EXMEM2 && (l1d_cache_hit_EXMEM2 || !mem_multicycle_EXMEM2)) ||
    was_cache_miss_EXMEM2 || mu_request_finished_EXMEM2;
```

## Problem 2: Timing Path Limitations

### Critical Timing Paths

1. **Forwarding Mux Chain**:
   ```
   Instruction Decode → Register Compare → Forward Select → Data Mux → ALU → Result Mux
   ```
   Estimated delay: 15-20ns (limits to ~50-60MHz)

2. **EXMEM2 Result Mux**:
   ```verilog
   assign data_d_EXMEM2 = (mem_sdram_EXMEM2 && was_cache_miss_EXMEM2) ? l1d_cache_controller_result :
                          (l1d_cache_hit_EXMEM2) ? l1d_cache_hit_q_EXMEM2 :
                          (arithm_EXMEM2 && malu_request_finished_EXMEM2) ? malu_q_EXMEM2 :
                          (mem_io_EXMEM2) ? mu_q_EXMEM2 :
                          alu_y_EXMEM2;
   ```
   6-input priority mux with 32-bit buses

3. **Branch Resolution Path**:
   ```
   Forwarded Data → Branch Comparator → Jump Valid → PC Update
   ```

### Why 100MHz is Achievable

Cyclone IV FPGA capabilities:
- DSP blocks: 3.5ns multiply
- Block RAM: 10ns access
- Logic delay: ~0.5ns per LE level
- Routing delay: ~2-3ns typical

A well-pipelined 32-bit CPU should achieve 100MHz with:
- 10ns cycle time
- 3-4 logic levels per stage
- Registered outputs between stages

### Current Limitation

The forwarding paths bypass pipeline registers, creating paths like:
```
EXMEM2 result (registered) → Mux (combinatorial) → ALU input (combinatorial) → 
ALU output (combinatorial) → Next stage input
```

This violates the principle of having only 3-4 logic levels between registers.

## Problem 3: Debugging and Extension Difficulty

### Current State

Adding CPU debugging (breakpoints, single-step) requires:

1. **Stopping the pipeline cleanly**: Must handle all 63+ states
2. **Preserving state**: Multi-cycle operations in progress
3. **Resume correctly**: Re-synchronize all pipeline stages
4. **No corruption**: Ensure forwarding paths remain valid

### Estimated Implementation Effort

For proper debug support:
- New pipeline states: ~10-15 per stage
- Hazard interactions: ~300+ new cases to verify
- Testing: ~100+ new test cases

**This complexity is why debugging hasn't been implemented yet.**

## Performance Analysis

### Current Performance Baseline

| Metric | Value | Notes |
|--------|-------|-------|
| Clock frequency | ~50MHz | Limited by forwarding paths |
| CPI (cache hits) | 1.0-1.5 | Good with forwarding |
| CPI (cache miss) | 15-25 | SDRAM burst latency |
| CPI (division) | 35-40 | Iterative divider |
| Branch penalty | 3-4 cycles | Pipeline flush |

### Performance Goals

| Metric | Target | Improvement |
|--------|--------|-------------|
| Clock frequency | 100MHz | 2x |
| CPI (cache hits) | 1.0-2.0 | Acceptable |
| CPI (cache miss) | 10-15 | Better cache/prefetch |
| CPI (division) | 35-40 | No change |
| Branch penalty | 2-3 cycles | Shorter pipeline |

### Key Insight

**Doubling clock frequency with even slightly worse CPI results in net performance gain.**

Example calculation:
- Current: 50MHz × 1/1.2 CPI = 41.7 MIPS
- Target: 100MHz × 1/1.5 CPI = 66.7 MIPS (60% improvement)

## Success Criteria for New Design

1. **Timing**: Achieve 100MHz on Cyclone IV (10ns critical path)
2. **Correctness**: Pass all existing CPU and compiler tests
3. **Maintainability**: Hazard logic should be comprehensible and localized
4. **Extensibility**: Debug support should be implementable in ~1 week
5. **Compatibility**: ISA unchanged, no assembler/compiler modifications
6. **Performance**: ≥60% improvement in MIPS (clock × 1/CPI)

## Constraints for Solutions

1. **No external IP**: Must be designed from scratch (learning requirement)
2. **RISC ISA**: Must maintain B32P2 instruction set
3. **Cyclone IV target**: Must work on EP4CE40 (or similar)
4. **Tool compatibility**: Quartus II / Quartus Prime
5. **Simulation**: Must work with Icarus Verilog for testing

## Next Steps

This problem statement will be reviewed by a team of experts who will propose architectural solutions. Each proposal will be documented as an RFC (Request for Comments) with:

1. Architectural overview
2. Pipeline design
3. Hazard handling strategy
4. Performance analysis
5. Implementation complexity
6. Risk assessment

See the following documents for expert proposals:
- [RFC-001: Classic 5-Stage In-Order Pipeline](01-RFC-001-Classic-5-Stage.md)
- [RFC-002: Decoupled Fetch-Execute Architecture](02-RFC-002-Decoupled-Architecture.md)
- [RFC-003: Scoreboard-Based Out-of-Order Execution](03-RFC-003-Scoreboard-OoO.md)
- [RFC-004: Stall-Free Pipeline with Compiler Support](04-RFC-004-Stall-Free-Pipeline.md)
- [RFC-005: Hybrid Pipeline with Result Queues](05-RFC-005-Hybrid-Result-Queues.md)
