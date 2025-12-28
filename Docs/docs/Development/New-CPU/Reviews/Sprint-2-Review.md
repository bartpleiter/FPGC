# Sprint 2 Review: Advanced Pipeline Features

**Sprint Duration:** December 27, 2024  
**Sprint Goal:** Implement forwarding, hazard detection, cache integration, multi-cycle ALU, and stack  
**Status:** ✅ Complete (merged with Sprint 1)

## Sprint Overview

Sprint 2 was planned to cover Stories 8-13 (forwarding unit, hazard detection, control hazards, cache integration, multi-cycle ALU, and stack operations). However, due to the integrated nature of pipeline design, most of these features were implemented during Sprint 1.

## Completed Work

### Story 8: Data Forwarding Unit ✅

Implemented full forwarding from EX/MEM and MEM/WB stages:

```verilog
// Forward from EX/MEM (most recent) or MEM/WB (older)
// forward_a/b: 00=no forward, 01=from EX/MEM, 10=from MEM/WB
assign forward_a = (ex_mem_dreg_we && ex_mem_dreg != 4'd0 && ex_mem_dreg == id_ex_areg) ? 2'b01 :
                   (mem_wb_dreg_we && mem_wb_dreg != 4'd0 && mem_wb_dreg == id_ex_areg) ? 2'b10 :
                   2'b00;
```

**Tests:** `data_hazards_alu.asm`, `consecutive_multicycle.asm`

### Story 9: Hazard Detection Unit ✅

Implemented three hazard types:
1. **Load-use hazard**: When an instruction needs data from a load that's still in MEM
2. **Pop-use hazard**: When an instruction needs data from a pop that's still in MEM
3. **Cache line hazard**: Back-to-back SDRAM accesses to different cache lines

**Key Implementation Insight:**
Load/pop hazards stall IF/ID (to hold the dependent instruction) but NOT EX/MEM (the load/pop must continue to complete). This required careful bubble insertion logic:

```verilog
end else if (!ex_pipeline_stall) begin
    // EX consumed the instruction but we're stalled (load-use or pop-use hazard)
    // Insert a bubble in ID/EX so the instruction doesn't execute twice
    id_ex_valid <= 1'b0;
    id_ex_dreg_we <= 1'b0;
    id_ex_mem_read <= 1'b0;
    // ... other control signals cleared
end
```

**Tests:** `pop_immediate_use.asm`, `pop_wb_conflict.asm`, `cache_miss_hazard.asm`

### Story 10: Control Hazards ✅

Implemented pipeline flush on branch/jump taken with ROM latency handling:

```verilog
// Flush on control hazard (branch/jump taken)
// Note: hazard_stall should NOT flush ID/EX - we want to HOLD the instruction
assign flush_if_id = pc_redirect;
assign flush_id_ex = pc_redirect;
```

**Key Fix:** Added `redirect_pending` flag to handle ROM's 1-cycle latency after branch redirect:

```verilog
reg redirect_pending = 1'b0;
always @(posedge clk) begin
    if (!pipeline_stall) begin
        if (pc_redirect) begin
            pc <= pc_redirect_target;
            redirect_pending <= 1'b1;
        end else if (redirect_pending) begin
            pc_delayed <= pc;
            pc <= pc + 32'd1;
            redirect_pending <= 1'b0;
        end
    end
end
```

**Tests:** `branch_all_conditions.asm`, `branch_during_stall.asm`, `flush_during_stall.asm`

### Story 11: Cache Integration ✅

Implemented L1D cache controller interface with 3-state machine:

```verilog
localparam L1D_STATE_IDLE    = 2'b00;
localparam L1D_STATE_STARTED = 2'b01;
localparam L1D_STATE_WAIT    = 2'b10;
```

Handles cache hits (single cycle) and misses (multi-cycle with SDRAM access).

**Tests:** `cache_hit_same_line.asm`, `cache_miss_hazard.asm`, `cache_dirty_eviction.asm`, `read_write_basic.asm`, `read_write_hazards.asm`

### Story 12: Multi-Cycle ALU Integration ✅

Implemented multi-cycle ALU for multiplication and division with 3-state machine:

```verilog
localparam MALU_IDLE    = 2'b00;
localparam MALU_STARTED = 2'b01;
localparam MALU_DONE    = 2'b10;
```

**Tests:** `multiply_fixed_point.asm`, `multiply_signed_unsigned.asm`, `signed_div_mod.asm`, `unsigned_div_mod.asm`, `fixed_point_div.asm`

### Story 13: Stack Operations ✅

Integrated hardware stack with proper push/pop timing:

```verilog
assign stack_push = ex_mem_valid && ex_mem_push && !backend_pipeline_stall;
assign stack_pop = ex_mem_valid && ex_mem_pop && !backend_pipeline_stall;
```

**Tests:** `push_pop_basic.asm`

## Test Results

All tests from Sprint 1 continue to pass with the integrated features:

| Category | Tests | Passing | Status |
|----------|-------|---------|--------|
| 01_load_store | 2 | 2 | ✅ |
| 02_alu_basic | 3 | 3 | ✅ |
| 03_compare | 2 | 2 | ✅ |
| 04_multiply | 2 | 2 | ✅ |
| 05_jump | 3 | 3 | ✅ |
| 06_stack | 1 | 1 | ✅ |
| 07_branch | 1 | 1 | ✅ |
| 08_memory_mapped | 1 | 1 | ✅ |
| 09_pipeline_hazards | 7 | 7 | ✅ |
| 10_sdram_cache | 6 | 6 | ✅ |
| 11_division | 3 | 3 | ✅ |
| **Total** | **29** | **29** | **100%** |

## Key Lessons Learned

1. **Stall vs Flush distinction is critical**: Hazard stalls should HOLD instructions in ID/EX, not FLUSH them. Only control hazards (branches) should flush.

2. **Bubble insertion requires careful condition handling**: When EX advances but ID is stalled, a bubble must be inserted in ID/EX to prevent duplicate execution.

3. **ROM latency complicates branch handling**: The 1-cycle ROM read latency requires tracking "stale" instruction fetches after a redirect.

## Remaining Work

### Story 14: Interrupt Support (Sprint 3)
- Interrupt input interface (8 lines)
- Priority encoder
- Pipeline flush on interrupt
- Handler jump
- Return address saving (RETI instruction)

### Story 15: Final Integration (Sprint 3)
- Full system integration
- Run complete CPU test suite (`make test-cpu`)
- Verify compatibility with existing software

## Metrics

| Metric | Value |
|--------|-------|
| Lines of Code (B32P3.v) | ~1100 |
| Test Pass Rate | 100% (29/29) |
| Forwarding Paths | 4 (2 from EX/MEM, 2 from MEM/WB) |
| Hazard Types | 3 (load-use, pop-use, cache-line) |
| State Machines | 3 (MALU, L1D cache, PC redirect) |

## Ready for Sprint 3

Sprint 2 work is complete. The pipeline correctly handles all data and structural hazards. Sprint 3 will focus on:
1. Interrupt support
2. Final integration with the complete system
3. Running the full CPU test suite

---

*Sprint 2 Review Completed: December 27, 2024*
