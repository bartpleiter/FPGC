# Story 12: Multi-Cycle Operations (Division/Multiplication)

**Sprint**: 3  
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

## Description

As a developer, I want to integrate multi-cycle ALU operations so that division and multiplication work correctly with the pipeline.

## Acceptance Criteria

1. [ ] MultiCycleALU instantiation
2. [ ] Start/done handshaking
3. [ ] Pipeline stall during multi-cycle operation
4. [ ] Result capture on completion
5. [ ] Division and multiplication tests pass

## Technical Details

### Multi-Cycle Operations

| Operation | Cycles | Description |
|-----------|--------|-------------|
| MULTS     | 4      | Signed multiply (32x32→32 lower) |
| MULTU     | 4      | Unsigned multiply |
| MULTHI    | 4      | Signed multiply high (upper 32 bits) |
| MULTHU    | 4      | Unsigned multiply high |
| DIVS      | 33     | Signed divide |
| DIVU      | 33     | Unsigned divide |
| MODS      | 33     | Signed modulo |
| MODU      | 33     | Unsigned modulo |

### MultiCycleALU Interface (Existing Module)

```verilog
// From existing MultiCycleALU.v
module MultiCycleALU(
    input wire clk,
    input wire reset,
    input wire start,
    input wire [31:0] a,
    input wire [31:0] b,
    input wire [3:0] op,
    output reg [31:0] result,
    output reg done
);
```

### Integration in EX Stage

```verilog
// =============================================================================
// MULTI-CYCLE ALU INTEGRATION
// =============================================================================

// Detect multi-cycle operation in ID/EX
wire ex_is_multicycle = id_ex_multi_cycle && id_ex_valid;

// Start signal - pulse when operation begins
reg multicycle_started;
wire multicycle_start = ex_is_multicycle && !multicycle_started && !multicycle_done;

always @(posedge clk) begin
    if (reset) begin
        multicycle_started <= 1'b0;
    end else if (multicycle_done) begin
        multicycle_started <= 1'b0;  // Reset for next operation
    end else if (multicycle_start) begin
        multicycle_started <= 1'b1;  // Operation started
    end
end

// MultiCycleALU instantiation
wire [31:0] multicycle_result;
wire multicycle_done;

MultiCycleALU multiAlu(
    .clk    (clk),
    .reset  (reset),
    .start  (multicycle_start),
    .a      (ex_forward_a_data),
    .b      (ex_forward_b_data),
    .op     (id_ex_alu_op),
    .result (multicycle_result),
    .done   (multicycle_done)
);
```

### Pipeline Stall During Multi-Cycle

```verilog
// Stall pipeline while multi-cycle operation is in progress
wire multicycle_busy = (multicycle_started || multicycle_start) && !multicycle_done;

// Connect to hazard unit
// This stalls IF, ID, and holds EX/MEM register
assign stall_multicycle = multicycle_busy;

// In hazard unit:
wire pipeline_stall = load_use_hazard || 
                      multicycle_busy ||    // <-- Multi-cycle stall
                      l1i_stall || 
                      l1d_stall || 
                      mu_stall;
```

### Result Selection

```verilog
// EX stage result selection
wire [31:0] ex_result;
assign ex_result = ex_is_multicycle ? multicycle_result : ex_alu_result;

// Result is valid when:
// - Single-cycle: immediately
// - Multi-cycle: when done signal asserts
wire ex_result_valid = ex_is_multicycle ? multicycle_done : id_ex_valid;
```

### EX/MEM Register Update

```verilog
// EX/MEM register only updates when result is valid
always @(posedge clk) begin
    if (reset || flush_ex_mem) begin
        ex_mem_valid <= 1'b0;
        // ... reset other fields
    end else if (!multicycle_busy || multicycle_done) begin
        // Only advance when not stalled OR operation just completed
        ex_mem_valid <= ex_result_valid;
        ex_mem_alu_result <= ex_result;
        ex_mem_rd <= id_ex_rd;
        // ... copy other fields
    end
    // When stalled: hold current values
end
```

### Division by Zero Handling

```verilog
// Division by zero produces defined result (implementation-specific)
// MultiCycleALU should handle this internally
// Options:
// 1. Return 0
// 2. Return MAX_INT
// 3. Return dividend unchanged
// 4. Set exception flag (if interrupt support exists)

// For now, let MultiCycleALU handle it
// Document the behavior for software compatibility
```

### Debug Output

```verilog
always @(posedge clk) begin
    if (!reset) begin
        if (multicycle_start) begin
            $display("%0t MULTICYCLE: START op=%h a=%h b=%h",
                     $time, id_ex_alu_op, ex_forward_a_data, ex_forward_b_data);
        end
        if (multicycle_busy) begin
            $display("%0t MULTICYCLE: BUSY (stalling pipeline)", $time);
        end
        if (multicycle_done) begin
            $display("%0t MULTICYCLE: DONE result=%h", $time, multicycle_result);
        end
    end
end
```

## Tasks

1. [ ] Identify multi-cycle operations in ControlUnit
2. [ ] Add multi_cycle control signal if not present
3. [ ] Instantiate MultiCycleALU in B32P3
4. [ ] Implement start signal generation
5. [ ] Implement busy tracking
6. [ ] Connect pipeline stall signal
7. [ ] Implement result selection mux
8. [ ] Update EX/MEM register logic
9. [ ] Add debug output
10. [ ] Test multiplication
11. [ ] Test division

## Definition of Done

- Multiply operations work (4 cycles)
- Divide operations work (33 cycles)
- Pipeline stalls during operation
- Result correctly written to destination register
- Division and multiplication tests pass

## Test Plan

### Test 1: Simple Multiply
```assembly
; Tests/CPU/04_multiply/mult_basic.asm
load 7 r1
load 6 r2
mults r1 r2 r15    ; expected=42
halt
```

### Test 2: Large Multiply
```assembly
load 1000 r1
load 1000 r2
mults r1 r2 r15    ; expected=1000000
halt
```

### Test 3: Simple Division
```assembly
; Tests/CPU/11_division/div_basic.asm
load 100 r1
load 7 r2
divs r1 r2 r15     ; 100/7 = 14 ; expected=14
halt
```

### Test 4: Modulo
```assembly
load 100 r1
load 7 r2
mods r1 r2 r15     ; 100 % 7 = 2 ; expected=2
halt
```

### Test 5: Division After ALU
```assembly
; Test that pipeline resumes correctly after multi-cycle
load 10 r1
load 2 r2
divs r1 r2 r3      ; r3 = 5
add r3 r3 r15      ; r15 = 10 ; expected=10
halt
```

## Dependencies

- Story 1-10 (Complete pipeline)
- External: MultiCycleALU.v (existing)

## Notes

- MultiCycleALU must be checked for interface compatibility
- Division takes 33 cycles - significant stall!
- Consider if sequential divisions can be optimized (not for Phase 1)
- Forwarding still works - result available same cycle as done

## Review Checklist

- [ ] Start signal correctly pulses once
- [ ] Busy signal covers entire operation
- [ ] Done signal correctly indicates completion
- [ ] Result captured on done cycle
- [ ] Pipeline stalls for correct duration
- [ ] No lost instructions after multi-cycle
- [ ] Forwarding works for multi-cycle result
