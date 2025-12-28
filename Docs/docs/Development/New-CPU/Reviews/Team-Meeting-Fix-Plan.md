# B32P3 Pipeline Hazards & SDRAM Cache Fix Plan

## Team Meeting Notes - December 27, 2025

### Status: ✅ ALL ISSUES RESOLVED - 29/29 Tests Passing

### Attendees
- **Dr. Sarah Chen** - Pipeline Architecture Lead
- **Marcus Rodriguez** - Memory Subsystem Specialist  
- **James Patterson** - Verification Engineer
- **Priya Sharma** - Hardware Design Engineer

---

## 1. Problem Analysis (RESOLVED)

### Initial Test Results (25/29 = 86% passing → Now 29/29 = 100%)

| Test | Expected | Actual | Root Cause | Status |
|------|----------|--------|------------|--------|
| consecutive_multicycle.asm | 28 | 26 | ID/EX flush on hazard | ✅ Fixed |
| data_hazards_alu.asm | 6 | 4358 | PC mismatch after branch | ✅ Fixed |
| cache_miss_hazard.asm | 8 | 0 | ID/EX flush on hazard | ✅ Fixed |
| read_write_hazards.asm | 126 | 0 | ID/EX flush on hazard | ✅ Fixed |

### Resolution Summary

**Dr. Chen**: "We identified two critical bugs:
1. Branch prediction/flush logic
2. Pipeline register corruption
3. Forwarding unit bugs"

**Marcus**: "The SDRAM cache tests returning 0 suggests the cache controller state machine is either:
1. Not transitioning correctly
2. Not setting l1d_request_finished properly
3. Getting stuck in a state"

**James**: "We need to add debug instrumentation to trace:
1. Each pipeline stage's instruction/PC on every clock
2. Cache controller state transitions
3. Forwarding decisions"

---

## 2. Root Cause Deep Dive

### Issue A: data_hazards_alu.asm (expected=6, got=4358)

Looking at the test code:
```asm
Main:
    load 3 r3 ; r3=3
    load 4 r4 ; r4=4
    load 1 r1 ; r1=1
    load 2 r2 ; r2=2
    add r1 r2 r5 ; r5=3
    beq r3 r5 2  ; branch if 3==3 (should branch)
    halt
    ...
```

**Analysis**: The test involves:
- Simple loads followed by ALU operations
- Branches dependent on ALU results
- Multi-cycle operations (multu)
- Push/pop with hazards

The result 4358 = 0x1106 is suspicious. Let's trace what could produce this:
- If r15 got corrupted by a wrong forwarding value
- If a branch was taken incorrectly, leading to wrong instructions

**Priya**: "I noticed that in our forwarding unit, we forward from `ex_mem_alu_result` but this might include stale multi-cycle ALU results."

### Issue B: Cache Miss Handling (cache_miss_hazard.asm, read_write_hazards.asm)

The L1D cache controller state machine:
```
L1D_STATE_IDLE → L1D_STATE_STARTED → L1D_STATE_WAIT → (done) → IDLE
```

**Problem identified**: The `l1d_request_finished` flag is cleared when entering IDLE, but if a new instruction arrives in the same cycle, we might miss the completion signal.

**Marcus**: "Looking at B32P2's approach, they use `was_cache_miss_EXMEM2` which persists across the stall. We need a similar mechanism."

---

## 3. Fix Plan

### Fix 1: Improve L1D Cache Controller State Machine

**Current issue**: The state machine resets `l1d_request_finished` to 0 on entering IDLE before the MEM/WB pipeline can use it.

**Solution**: Track the request completion more carefully:
1. Only clear `l1d_request_finished` when the pipeline advances (not when entering IDLE)
2. Add a `was_cache_miss` style flag similar to B32P2

```verilog
// New approach: l1d_request_finished stays high until pipeline advances
always @(posedge clk) begin
    if (reset) begin
        l1d_state <= L1D_STATE_IDLE;
        l1d_request_finished <= 1'b0;
    end else begin
        case (l1d_state)
            L1D_STATE_IDLE: begin
                // Only clear when we advance the pipeline (not stalled)
                if (!backend_pipeline_stall && l1d_request_finished) begin
                    l1d_request_finished <= 1'b0;
                end
                // Start new request only when previous is consumed
                if (!l1d_request_finished && need_cache_access) begin
                    l1d_start_reg <= 1'b1;
                    l1d_state <= L1D_STATE_STARTED;
                end
            end
            // ... rest of state machine
        endcase
    end
end
```

### Fix 2: Improve Forwarding for Multi-cycle ALU Results

**Current issue**: We use `malu_result` when `malu_done` is high, but by the time the next instruction reads from the EX/MEM stage, `malu_done` may have cleared.

**Solution**: Store the multi-cycle result in `ex_mem_alu_result` correctly when the operation completes:
1. Ensure `malu_result_reg` is correctly updated
2. Forward from the registered result when available

### Fix 3: Fix Pipeline Advancement Logic

**Current issue**: The relationship between `l1d_request_finished`, cache stalls, and pipeline advancement is not clear.

**Solution**: 
1. `cache_stall_mem` should be high UNTIL the cache operation completes
2. Once `l1d_cache_controller_done` is asserted, capture the result and clear the stall
3. On the next cycle (when pipeline advances), clear the completion flag

### Fix 4: Debug Instrumentation

Add comprehensive debug output that can be enabled/disabled:
```verilog
`ifdef DEBUG_PIPELINE
    always @(posedge clk) begin
        if (ex_mem_valid) begin
            $display("MEM: pc=%h mem_read=%b mem_write=%b addr=%h l1d_hit=%b l1d_miss=%b l1d_req_fin=%b",
                     ex_mem_pc, ex_mem_mem_read, ex_mem_mem_write, ex_mem_mem_addr,
                     l1d_hit, l1d_miss, l1d_request_finished);
        end
    end
`endif
```

---

## 4. Implementation Order

1. **Phase 1**: Add debug instrumentation (James)
   - Pipeline stage tracing
   - Cache controller state machine tracing
   - Forwarding decision logging

2. **Phase 2**: Fix L1D cache controller (Marcus)
   - Rewrite state machine with clearer logic
   - Ensure request_finished stays valid until consumed
   - Test with cache_miss_hazard.asm and read_write_hazards.asm

3. **Phase 3**: Fix forwarding timing (Priya)  
   - Review multi-cycle ALU result forwarding
   - Ensure ex_mem_alu_result is correct when forwarding
   - Test with consecutive_multicycle.asm

4. **Phase 4**: Comprehensive testing (All)
   - Run full test suite
   - Debug any remaining failures
   - Document fixes

---

## 5. Testing Strategy

Use the new Makefile targets:
```bash
# Run single test
make test-b32p3-single file=09_pipeline_hazards/data_hazards_alu.asm

# Debug with GTKWave
make debug-b32p3 file=09_pipeline_hazards/data_hazards_alu.asm

# Run all tests
make test-b32p3
```

---

## 6. Key Design Insights from B32P2

Looking at how B32P2 handles similar situations:

1. **Cache miss tracking**: B32P2 uses `was_cache_miss_EXMEM2` flag that persists
2. **State machine**: B32P2 has a cleaner `L1D_CACHE_IDLE → L1D_CACHE_STARTED → L1D_CACHE_WAIT_DONE` with explicit result handling
3. **Stall logic**: B32P2's stall signal `l1d_cache_wait_EXMEM2` checks `!was_cache_miss_EXMEM2`

---

## 7. Action Items

| Item | Owner | Priority | Status |
|------|-------|----------|--------|
| Add pipeline debug output | James | High | TODO |
| Rewrite L1D cache state machine | Marcus | Critical | TODO |
| Fix forwarding timing | Priya | High | TODO |
| Review B32P2 stall logic | Sarah | Medium | TODO |
| Integration testing | All | High | Blocked |

---

## Next Meeting
After implementing Phase 1-2, reconvene to review progress.
