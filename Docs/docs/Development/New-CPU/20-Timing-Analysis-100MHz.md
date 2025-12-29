# B32P3 100MHz Timing Analysis Report

**Date:** December 28, 2025  
**Target:** Cyclone IV EP4CE40 at 100MHz  
**Initial Status:** Timing violations with worst negative slack of -5.615ns  
**Status After Optimizations:** Optimizations implemented, ready for re-synthesis

## Executive Summary

The B32P3 CPU initially failed timing at 100MHz on the Cyclone IV EP4CE40 FPGA. Analysis of the worst-case timing paths revealed that all critical paths converge on **PC register updates**, with data delays of approximately 15.9-16.0ns against a 10ns period requirement.

The root cause was a **multi-stage combinational path** through the forwarding logic, branch comparison, and PC redirect calculation that spans effectively 3+ pipeline stages worth of logic in a single clock cycle.

**Optimizations have been implemented** to reduce the critical path delay. Re-synthesis is needed to verify timing closure.

## Implemented Optimizations

### Optimization 1: Pre-computed Jump Register Address
**Location:** [B32P3.v](../../../Hardware/FPGA/Verilog/Modules/CPU/B32P3.v) and [BranchJumpUnit.v](../../../Hardware/FPGA/Verilog/Modules/CPU/BranchJumpUnit.v)

**Change:** Pre-compute the `jumpr` target address (`data_b + const16 ± pc`) in B32P3 and pass it to BranchJumpUnit as `pre_jumpr_addr`.

**Impact:** Removes the `data_b + const16 + pc` addition from inside the BranchJumpUnit, reducing combinational depth by ~1-2 levels.

```verilog
// Pre-compute register jump offsets (data_b + const16)
wire [31:0] pre_jumpr_offset = ex_breg_forwarded + id_ex_const16;
wire [31:0] pre_jumpr_addr = id_ex_oe ? (id_ex_pc + pre_jumpr_offset) : pre_jumpr_offset;
```

### Optimization 2: Parallel Branch Comparison
**Location:** [BranchJumpUnit.v](../../../Hardware/FPGA/Verilog/Modules/CPU/BranchJumpUnit.v)

**Change:** Compute ALL comparison results (eq, neq, gt, lt, ge, le for both signed and unsigned) in parallel, then select based on branchOP. This replaces the cascaded mux structure inside the case statement.

**Impact:** Reduces mux depth in comparison result path. All comparisons happen in parallel, and only the final result selection is serialized.

```verilog
// Compute all comparison results in parallel
wire cmp_eq  = (data_a == data_b);
wire cmp_neq = (data_a != data_b);
wire cmp_gt_u  = (data_a > data_b);
// ... etc

// Select based on sig
wire cmp_gt = sig ? cmp_gt_s : cmp_gt_u;

// Final selection based on branchOP (simple mux)
case (branchOP)
    BRANCH_OP_BEQ: branch_passed <= cmp_eq;
    // ...
endcase
```

### Optimization 3: Pre-computed PC Increment
**Location:** [B32P3.v](../../../Hardware/FPGA/Verilog/Modules/CPU/B32P3.v)

**Change:** Pre-compute `pc + 1` as a wire (`pc_plus_1`) and use it in the PC update logic instead of computing `pc + 32'd1` inline.

**Impact:** Removes the 32-bit adder from the PC update critical path, allowing the PC mux to select between pre-computed values.

```verilog
// TIMING OPTIMIZATION: Pre-computed PC + 1
wire [31:0] pc_plus_1 = pc + 32'd1;

// In PC update:
pc <= pc_plus_1;  // Instead of pc + 32'd1
```

### Optimization 4: Separate wb_data Path for ALU
**Location:** [B32P3.v](../../../Hardware/FPGA/Verilog/Modules/CPU/B32P3.v)

**Change:** Added documentation clarifying that `wb_data_alu` can be used for fast-path forwarding when the forwarding source is known to be an ALU result (not load/pop).

**Impact:** Enables future optimization if needed - MEM/WB ALU result forwarding can bypass the wb_data mux.

## Timing Report Analysis (Before Optimization)

### Worst Paths Summary

| Rank | Slack | From Node | To Node | Data Delay |
|------|-------|-----------|---------|------------|
| 1 | -5.615ns | ex_mem_dreg[1] | pc[23] | 15.972ns |
| 2 | -5.591ns | ex_mem_pop | pc[23] | 15.941ns |
| 3 | -5.547ns | id_ex_areg[0] | pc[23] | 15.904ns |
| 4 | -5.527ns | ex_mem_dreg[1] | pc[28] | 15.858ns |
| 5 | -5.519ns | ex_mem_dreg[1] | pc[26] | 15.874ns |

All 100 worst paths terminate at the **PC register** (various bits), indicating the PC update logic is the critical bottleneck.

### Source Node Patterns

The timing report shows three main categories of source nodes:

1. **EX/MEM Pipeline Registers** (ex_mem_dreg, ex_mem_pop, ex_mem_dreg_we, ex_mem_mem_read)
2. **ID/EX Pipeline Registers** (id_ex_areg, id_ex_breg)
3. **MEM/WB Pipeline Registers** (mem_wb_dreg, mem_wb_pop, mem_wb_mem_read)
4. **Stack Module** (Stack:stack|useRamResult, Stack:stack|ramResult)

## Critical Path Analysis

### Path 1: Forward → Branch → PC Update

The longest combinational path follows this sequence:

```
ex_mem_dreg[n] → forward_a/b calculation → ex_alu_a/ex_breg_forwarded mux →
BranchJumpUnit comparison → branch_passed → jump_valid → pc_redirect →
pc register update
```

**Detailed breakdown:**

1. **Forwarding Detection** (~2ns)
   ```verilog
   wire ex_mem_can_forward = ex_mem_dreg_we && ex_mem_dreg != 4'd0 && 
                             !ex_mem_mem_read && !ex_mem_pop;
   assign forward_a = (ex_mem_can_forward && ex_mem_dreg == id_ex_areg) ? 2'b01 : ...
   ```

2. **Forwarding Mux** (~1.5ns)
   ```verilog
   assign ex_alu_a = (forward_a == 2'b01) ? ex_mem_alu_result :
                     (forward_a == 2'b10) ? wb_data :
                     ex_areg_data;
   ```

3. **WB Data Mux** (adds to path via forward_a == 2'b10) (~1.5ns)
   ```verilog
   assign wb_data = mem_wb_mem_read ? mem_wb_mem_data :
                    mem_wb_pop      ? stack_q :
                    mem_wb_alu_result;
   ```

4. **Branch Comparison** (~3-4ns for 32-bit comparison with signed variants)
   ```verilog
   branch_passed <= (sig) ? ($signed(data_a) > $signed(data_b)) : (data_a > data_b);
   ```

5. **Jump Valid Logic** (~0.5ns)
   ```verilog
   assign jump_valid = (jumpc | jumpr | (branch & branch_passed) | halt);
   ```

6. **PC Redirect Logic** (~0.5ns)
   ```verilog
   assign pc_redirect = id_ex_valid && jump_valid;
   ```

7. **PC Register Input Mux & Register** (~2ns)
   - Multiple priority conditions (reset, interrupt, reti, redirect, stall)
   - 32-bit register update

**Total estimated combinational delay: ~11-12ns** + routing/setup = ~16ns

### Path 2: Stack → WB Data → Forward → Branch → PC

This path is similar but starts from the stack's `useRamResult` flag:

```
Stack:stack|useRamResult → stack_q mux → wb_data mux → forward mux →
branch comparison → pc_redirect → pc
```

### Path 3: MEM/WB Register → Forward → Branch → PC

Another variant where MEM/WB stage values feed back through forwarding:

```
mem_wb_dreg[n] → forward_a/b detection → forward mux → branch → pc
```

## Root Cause Identification

### Problem 1: Forwarding Through Branch in Same Cycle

The classic 5-stage pipeline assumes branches are resolved in EX, with forwarding from MEM and WB. However, the **forwarding mux and branch comparison are both in the EX stage combinational logic**, creating a chain:

```
Pipeline Register → Forward Detect → Forward Mux → Branch Compare → Jump Valid → PC
```

This is **too much logic for a single cycle at 100MHz**.

### Problem 2: Stack Read Latency in Forwarding Path

The stack module has a complex output path through `useRamResult` and `ramResult` that feeds into `wb_data`, which then feeds into forwarding. This adds another mux level.

### Problem 3: Wide Comparison Logic

The 32-bit branch comparisons (especially signed comparisons like `$signed(data_a) > $signed(data_b)`) are inherently slow and sit in the middle of the critical path.

### Problem 4: PC Update Complexity

The PC register has many input sources with priority encoding:
- Reset
- Interrupt (with PC backup)
- RETI (restore PC)
- PC redirect (branch/jump)
- Normal increment
- Stall hold

## Comparison with B32P2

The B32P2 design had a different pipeline structure with EXMEM1 and EXMEM2 stages. This effectively **split the EX/MEM boundary into two stages**, giving an extra cycle for:
- Forwarding detection and muxing
- Memory address calculation

This is why B32P2 could run at 50MHz more comfortably than B32P3 at 100MHz.

## Recommended Optimizations

### Optimization 1: Pipeline the Branch Comparison

**Current:** Branch comparison happens in EX stage combinationally.

**Proposed:** Add a registered comparison result that's available at the start of the next cycle.

```verilog
// Pre-compute comparison results at the end of ID stage
// Register them so they're available at the start of EX
reg [5:0] id_ex_compare_results;  // One bit per comparison type
```

**Impact:** Removes ~3-4ns from critical path.
**Trade-off:** Adds one cycle branch penalty.

### Optimization 2: Separate Forwarding Mux from Branch Path

**Current:** `ex_alu_a` is used directly for both ALU operations AND branch comparison.

**Proposed:** Pre-register forwarded values at the ID/EX boundary specifically for branch comparison.

```verilog
// At ID/EX register stage, capture forwarded values for next cycle
reg [31:0] id_ex_areg_forwarded;
reg [31:0] id_ex_breg_forwarded;

// In EX stage, use these pre-captured values for branch
BranchJumpUnit uses id_ex_areg_forwarded, id_ex_breg_forwarded
```

**Impact:** Removes forwarding mux from branch critical path (~3ns savings).
**Trade-off:** Extra registers, slightly more complex hazard detection.

### Optimization 3: Simplify WB Data Mux Path

**Current:** `wb_data` has a 3-way mux including stack_q.

**Proposed:** Register the stack output earlier so it doesn't go through the mux in the critical path.

```verilog
// Stack output already registered in stack module
// Ensure mem_wb_stack_data captures it correctly
reg [31:0] mem_wb_stack_data;
// Use mem_wb_stack_data directly instead of stack_q
```

**Impact:** Removes stack mux from forwarding path (~1.5ns).

### Optimization 4: Pre-decode Jump Target Selection

**Current:** BranchJumpUnit selects between multiple jump targets combinationally.

**Proposed:** Pre-compute all possible jump targets in parallel and just select based on instruction type.

```verilog
// Pre-compute all targets in EX stage
wire [31:0] jump_target_const = pre_jump_const_addr;
wire [31:0] jump_target_reg = oe ? (pc + data_b + const16) : (data_b + const16);
wire [31:0] jump_target_branch = pre_branch_addr;

// Simple select based on instruction type
assign jump_addr = jumpc ? jump_target_const :
                   jumpr ? jump_target_reg :
                   jump_target_branch;
```

**Impact:** Removes one mux level (~0.5-1ns).

### Optimization 5: Speculative PC Increment

**Current:** PC update waits for complete branch resolution.

**Proposed:** Speculatively increment PC, then correct if branch taken.

This is essentially a **branch prediction** scheme (predict not-taken).

**Impact:** Allows PC logic to be simpler and faster.
**Trade-off:** Branch misprediction penalty.

### Optimization 6: Move Branch Resolution to MEM Stage

**Current:** Branches are resolved in EX stage.

**Proposed:** Move branch comparison to MEM stage, accepting a longer branch penalty.

**Impact:** Completely removes branch comparison from EX critical path.
**Trade-off:** 2-cycle branch penalty instead of 1-cycle.

## Status: Implemented Optimizations

| Priority | Optimization | Status | Expected Savings |
|----------|-------------|--------|------------------|
| **1** | Pre-compute jumpr address | ✅ Implemented | ~1-2ns |
| **2** | Parallel branch comparisons | ✅ Implemented | ~1-2ns |
| **3** | Pre-computed PC+1 | ✅ Implemented | ~0.5-1ns |
| **4** | Pre-register forwarded values for branch | ❌ Not implemented (complex) | ~3-4ns |
| **5** | Simplify WB data mux | ❌ Not implemented | ~1.5ns |
| **6** | Pipeline branch comparison | ❌ Not implemented | ~3-4ns |

**Expected improvement:** ~2.5-5ns from implemented optimizations.

**Target:** Reduce critical path from ~16ns to ~10ns requires **~6ns savings**.

The implemented optimizations should reduce the critical path by approximately 2.5-5ns. If timing still fails:
- Consider implementing pre-registered forwarding values (Optimization 4)
- Consider pipelining the branch comparison (adds 1-cycle branch penalty)

## Next Steps

1. **Re-synthesize** the design with 100MHz constraint in Quartus
2. **Analyze** the new timing report
3. **If timing fails**, implement additional optimizations:
   - Pre-register forwarded values at ID/EX boundary
   - Pipeline branch comparison (1-cycle penalty)
4. **Test on hardware** once timing closes

## Validation

All tests pass after optimization:
- ✅ 40 CPU tests (ROM and RAM modes)
- ✅ 86 B32CC tests

## Conclusion

The B32P3 timing violations at 100MHz are caused by excessive combinational logic in the forward→branch→PC path. The implemented optimizations reduce the critical path by pre-computing jump addresses, parallelizing branch comparisons, and pre-computing PC+1.

If timing still doesn't close after re-synthesis, the most effective remaining optimization is to pre-register forwarded values at the ID/EX boundary, which would remove the forwarding mux from the branch comparison critical path.

The key insight is that the current design tries to resolve too many dependencies in a single cycle. The implemented optimizations reduce the combinational depth without changing the pipeline architecture or adding cycle penalties.
