# Story 15: Final Integration and Testing

**Sprint**: 4  
**Priority**: P0 (Blocker)  
**Estimate**: 4 hours  
**Status**: Not Started

## Description

As a developer, I want to complete integration and run all tests so that the new CPU is verified to work correctly.

## Acceptance Criteria

1. [ ] All CPU tests pass (`make test-cpu`)
2. [ ] No Verilog compilation warnings
3. [ ] Debug output can be disabled for clean runs
4. [ ] Documentation updated
5. [ ] Code reviewed and cleaned up

## Technical Details

### Integration Checklist

```
□ All pipeline stages connected
□ All hazard detection working
□ All forwarding paths working
□ All memory interfaces connected
□ All control signals propagating
□ Reset behavior correct
□ Clock domain handling correct
```

### Test Categories to Pass

```
Tests/CPU/
├── 01_load_store/       # Load immediate, load from memory, store
├── 02_alu_basic/        # Add, sub, and, or, xor, shift
├── 03_compare/          # Comparison operations
├── 04_multiply/         # Signed/unsigned multiply
├── 05_jump/             # Unconditional jumps
├── 06_stack/            # Push/pop operations
├── 07_branch/           # Conditional branches
├── 08_memory_mapped/    # I/O operations
├── 09_pipeline_hazards/ # Data hazard handling
├── 10_sdram_cache/      # Cache/SDRAM access
├── 11_division/         # Division/modulo operations
```

### Running Tests

```bash
# Run all CPU tests
make test-cpu

# Run single test for debugging
make test-cpu-single file=Tests/CPU/01_load_store/load_immediate.asm

# Run with GTKWave for debugging
make debug-cpu file=Tests/CPU/01_load_store/load_immediate.asm
```

### Debug Output Control

```verilog
// Compile-time debug control
`define DEBUG_PIPELINE 0
`define DEBUG_HAZARDS 0
`define DEBUG_MEMORY 0
`define DEBUG_CACHE 0

// Conditional debug output
`ifdef DEBUG_PIPELINE
always @(posedge clk) begin
    if (!reset) begin
        $display("%0t PIPE: ...", $time);
    end
end
`endif

// Critical output that tests depend on (always enabled)
always @(posedge clk) begin
    if (!reset && mem_wb_valid && mem_wb_reg_write && mem_wb_rd == 4'd15) begin
        $display("%0t reg r15: %0d", $time, wb_data);
    end
end
```

### Known Issues to Watch For

```
1. Off-by-one errors in pipeline stage timing
2. Forwarding not covering all paths
3. Stall/flush interaction bugs
4. Cache miss handling edge cases
5. Multi-cycle operation completion timing
6. Stack push/pop ordering
7. Branch target calculation
8. Immediate sign extension
```

### Code Cleanup Tasks

```
□ Remove dead code
□ Add header comments to all modules
□ Document all port interfaces
□ Consistent naming convention
□ Align with existing codebase style
□ Remove magic numbers (use parameters)
□ Verify all debug statements helpful
```

### Documentation Updates

```
□ Update Architecture-Development-Guide.md
□ Document new pipeline stages
□ Document hazard handling
□ Document ISA compatibility
□ Update block diagrams if needed
```

### Performance Verification

```verilog
// Add cycle counter for performance monitoring
reg [31:0] cycle_count;
reg [31:0] instr_count;

always @(posedge clk) begin
    if (reset) begin
        cycle_count <= 0;
        instr_count <= 0;
    end else begin
        cycle_count <= cycle_count + 1;
        if (mem_wb_valid) begin
            instr_count <= instr_count + 1;
        end
    end
end

// CPI = cycle_count / instr_count
// Report at end of simulation
```

## Tasks

1. [ ] Run `make test-cpu` and note failures
2. [ ] Debug and fix each failing test category
3. [ ] Add missing functionality discovered in testing
4. [ ] Verify all edge cases handled
5. [ ] Clean up code
6. [ ] Disable verbose debug output
7. [ ] Final test run (all pass)
8. [ ] Update documentation
9. [ ] Expert team review

## Definition of Done

- `make test-cpu` shows all tests passing
- No compilation errors or warnings
- Code is clean and documented
- Documentation updated
- Ready for Phase 2 (100MHz optimization)

## Test Debugging Guide

### Test Failure Analysis

```bash
# 1. Run failing test in isolation
make test-cpu-single file=Tests/CPU/XX_category/failing_test.asm

# 2. Check output for "reg r15:" vs expected
# Output shows: reg r15: <actual>
# Expected: ; expected=<expected>

# 3. Enable debug output for that test
# Modify B32P3.v to enable relevant DEBUG_* defines
# Recompile and run again

# 4. Use GTKWave for cycle-by-cycle analysis
make debug-cpu file=Tests/CPU/XX_category/failing_test.asm
gtkwave cpu.vcd
```

### Common Failure Patterns

```
Symptom: r15 = 0 when expected non-zero
Cause: Instruction not completing, writeback not happening
Check: WB stage valid signal, reg_write enable

Symptom: r15 = wrong value
Cause: Forwarding path missing, hazard not detected
Check: Forwarding unit, hazard unit

Symptom: Test hangs
Cause: Pipeline stall never clears
Check: Stall conditions, cache hit signals

Symptom: Random corruption
Cause: Pipeline timing issue, race condition
Check: Clock edges, signal propagation
```

## Dependencies

- Story 1-14 (All previous implementation)

## Notes

- This is the final acceptance story
- All expert team members should review
- Any remaining issues become Phase 2 backlog items

## Review Checklist

- [ ] All 11 test categories pass
- [ ] No unexpected debug output
- [ ] Code follows project conventions
- [ ] Documentation complete
- [ ] Performance acceptable (CPI reasonable)
- [ ] Ready for synthesis attempt (Phase 2)
