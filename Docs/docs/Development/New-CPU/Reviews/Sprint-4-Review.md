# Sprint 4 Review: RAM Execution Fixes

**Sprint Duration:** December 28, 2025  
**Sprint Goal:** Fix B32P3 RAM execution issues and redo performance comparison  
**Status:** ✅ Complete

## Issue Summary

The stakeholder identified that B32P3 was only tested from ROM, while real programs execute from RAM (SDRAM). After updating the test framework to use B32P3, 26 out of 29 tests failed when run from RAM.

## Root Cause Analysis

### Problem 1: Instruction Cache State Machine Missing

The original B32P3 implementation used a simplistic cache miss handling:
```verilog
// BROKEN: Too simplistic
assign cache_stall_if = if_use_cache && !l1i_hit && if_id_valid;
assign l1i_cache_controller_start = cache_stall_if && !l1i_cache_controller_done;
```

This had multiple issues:
1. `if_id_valid` dependency created chicken-and-egg problem on first RAM instruction
2. No state machine to track cache miss progress (IDLE→STARTED→WAIT→READY)
3. No saved cache result - data lost on handshake timing issues
4. No flush handling during cache operations

### Problem 2: Stall Signal Architecture

The original B32P3 included `cache_stall_if` in `backend_stall`:
```verilog
// BROKEN: L1i cache miss stalls entire pipeline
wire backend_stall = cache_stall_if || cache_stall_mem || multicycle_stall || ...;
```

This prevented jumps from resolving while waiting for instruction cache, causing:
- `flush_during_stall` test failure (jump during multi-cycle op)
- Various other timing issues

### Problem 3: IF/ID Pipeline Register Behavior

During cache miss, IF/ID was held, but ID kept consuming the same instruction:
```verilog
// BROKEN: IF/ID held on cache miss, but ID keeps running
end else if (!stall_if) begin  // stall_if includes cache_stall_if
    if_id_valid <= 1'b1;  // Stays valid, ID re-executes same instruction
```

## Fixes Applied

### Fix 1: Proper L1I Cache Miss State Machine

Added 4-state machine matching B32P2's design:
```verilog
localparam L1I_CACHE_IDLE         = 2'b00;
localparam L1I_CACHE_STARTED      = 2'b01;
localparam L1I_CACHE_WAIT_DONE    = 2'b10;
localparam L1I_CACHE_RESULT_READY = 2'b11;

// Proper state transitions with flush handling
case (l1i_cache_miss_state)
    L1I_CACHE_IDLE: begin
        if (l1i_cache_miss && !flush_if_id) begin
            l1i_cache_controller_start_reg <= 1'b1;
            l1i_cache_miss_state <= L1I_CACHE_STARTED;
        end
    end
    // ... full state machine implementation
endcase
```

Key improvements:
- Saved cache result register
- Proper flush handling in each state
- Clean start/done handshake
- Abort support on control flow change

### Fix 2: Separated Stall Signals

Created separate stall signals for front-end and downstream:
```verilog
// Backend stall - does NOT include L1i cache miss
wire backend_stall = cache_stall_mem || multicycle_stall || mu_stall || cc_stall;

// Pipeline stall for IF - includes cache_stall_if
wire pipeline_stall = hazard_stall || backend_stall || cache_stall_if;

// Pipeline stall for ID onwards - does NOT include cache_stall_if  
wire pipeline_stall_downstream = hazard_stall || backend_stall;
```

This allows:
- IF to stall on cache miss (PC doesn't advance)
- ID/EX/MEM/WB to keep running (jumps can resolve)
- Bubbles flow through from IF during cache miss

### Fix 3: IF/ID Register Consumes Bubbles

Changed IF/ID to pass bubbles through during cache miss:
```verilog
end else if (!pipeline_stall_downstream) begin
    if_id_pc <= pc_delayed;
    if_id_instr <= if_instr;  // 0 during cache miss (bubble)
    // Valid only if we have a real instruction
    if_id_valid <= !cache_stall_if && !redirect_pending;
end
```

This matches B32P2's behavior where invalid instructions (bubbles) flow through the pipeline during cache misses.

### Fix 4: PC Update Separated from IF Stall

PC updates use `pipeline_stall_downstream` for redirects, but `cache_stall_if` for normal increments:
```verilog
end else if (!pipeline_stall_downstream) begin
    if (pc_redirect) begin
        // Redirects work even during cache miss
        pc <= pc_redirect_target;
    end else if (!cache_stall_if) begin
        // Normal increment blocked during cache miss
        pc <= pc + 32'd1;
    end
end
```

## Test Results

### Before Fix
```
32 passed, 26 failed (across 29 tests × 2 scenarios)
```

### After Fix
```
All 29 tests passed both ROM and RAM!
```

## Performance Comparison (RAM Execution)

| Test | B32P2 | B32P3 | Diff |
|------|-------|-------|------|
| cpi_benchmark | 269 | 260 | **-3.3%** |
| add_sub_logic_shift | 77 | 77 | 0.0% |
| multiply_fixed_point | 72 | 69 | **-4.2%** |
| jump_offset | 62 | 62 | 0.0% |
| branch_all_conditions | 132 | 132 | 0.0% |
| consecutive_multicycle | 156 | 148 | **-5.1%** |
| data_hazards_alu | 158 | 146 | **-7.6%** |
| cache_miss_hazard | 452 | 411 | **-9.1%** |
| cache_hit_same_line | 80 | 81 | +1.2% |
| unsigned_div_mod | 317 | 307 | **-3.2%** |
| **TOTAL** | **1775** | **1693** | **-4.6%** |

### Key Performance Insights

1. **cache_miss_hazard (9.1% faster)**: Improved L1i cache miss handling reduces stall cycles
2. **data_hazards_alu (7.6% faster)**: Better forwarding and hazard detection
3. **consecutive_multicycle (5.1% faster)**: Cleaner multi-cycle ALU integration
4. **cache_hit_same_line (+1.2% slower)**: Minor regression in cache hit path

### RAM vs ROM Performance

| Scenario | B32P3 vs B32P2 |
|----------|----------------|
| ROM Execution | -7.3% faster |
| RAM Execution | -4.6% faster |

The 2.7% difference is expected because RAM execution incurs L1i cache overhead that ROM doesn't have. B32P3 still provides meaningful improvement in both scenarios.

## Architecture Summary (Final)

### Stall Signal Hierarchy

```
backend_stall = cache_stall_mem || multicycle_stall || mu_stall || cc_stall
    ↓
pipeline_stall_downstream = hazard_stall || backend_stall
    ↓                            (used by ID, EX, PC redirects)
pipeline_stall = pipeline_stall_downstream || cache_stall_if
                                 (used by IF, PC increment)
```

### L1I Cache Miss State Machine

```
IDLE ──(miss)──> STARTED ──> WAIT_DONE ──(done)──> RESULT_READY ──(!stall)──> IDLE
  ↑                 │             │                    │
  └──────(flush)────┴─────────────┴────────────────────┘
```

### Pipeline Behavior During Cache Miss

| Stage | Behavior |
|-------|----------|
| IF | Stalled - PC held, fetching from cache |
| IF/ID | Updated with bubbles (valid=0) |
| ID | Consumes bubbles, passes to EX |
| EX | Processes bubbles (no-op) |
| MEM/WB | Can complete pending operations |

## Code Changes

Files modified:
- `Hardware/FPGA/Verilog/Modules/CPU/B32P3.v`
  - Added L1I cache miss state machine (~80 lines)
  - Restructured stall signals (~20 lines)
  - Fixed IF/ID and PC update logic (~30 lines)

Total: ~130 lines changed, test suite now fully passing.

## Conclusion

Sprint 4 successfully fixed the critical RAM execution bugs:

1. ✅ All 29 tests pass from both ROM and RAM
2. ✅ B32P3 is 4.6% faster than B32P2 from RAM
3. ✅ B32P3 is 7.3% faster than B32P2 from ROM
4. ✅ Proper state machine architecture matching B32P2
5. ✅ Clean stall signal hierarchy allowing jump resolution during cache miss

The stakeholder's concern has been addressed - B32P3 is now production-ready for real-world RAM execution.

---

*Sprint 4 Review Completed: December 28, 2025*
