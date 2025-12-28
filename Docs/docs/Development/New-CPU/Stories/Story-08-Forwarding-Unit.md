# Story 08: Data Forwarding Unit

**Sprint**: 2  
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

## Description

As a developer, I want to implement data forwarding so that data hazards are resolved without stalling when possible.

## Acceptance Criteria

1. [ ] EX-to-EX forwarding (EX/MEM → EX inputs)
2. [ ] MEM-to-EX forwarding (MEM/WB → EX inputs)
3. [ ] Forwarding detection logic
4. [ ] Forwarding mux selection
5. [ ] Data hazard tests pass

## Technical Details

### Forwarding Scenarios

```
Instruction Sequence    | Hazard Type      | Resolution
------------------------|------------------|------------
ADD r1, r2, r3         |                  |
SUB r4, r1, r5         | EX/MEM → EX     | Forward from EX/MEM
                       |                  |
ADD r1, r2, r3         |                  |
NOP                    |                  |
SUB r4, r1, r5         | MEM/WB → EX     | Forward from MEM/WB
                       |                  |
LOAD r1, [r2]          |                  |
SUB r4, r1, r5         | Load-use        | STALL (Story 9)
```

### Forwarding Unit Module

```verilog
// =============================================================================
// FORWARDING UNIT
// =============================================================================

module ForwardingUnit(
    // EX stage operand registers
    input wire [3:0] ex_rs,           // Source register 1 in EX
    input wire [3:0] ex_rt,           // Source register 2 in EX
    
    // EX/MEM stage info
    input wire       ex_mem_reg_write, // Will EX/MEM write to reg?
    input wire [3:0] ex_mem_rd,        // Destination register in EX/MEM
    
    // MEM/WB stage info
    input wire       mem_wb_reg_write, // Will MEM/WB write to reg?
    input wire [3:0] mem_wb_rd,        // Destination register in MEM/WB
    
    // Forwarding control outputs
    output reg [1:0] forward_a,        // Mux select for ALU input A
    output reg [1:0] forward_b         // Mux select for ALU input B
);

// Forward select encoding:
// 2'b00 = No forwarding, use ID/EX register value
// 2'b01 = Forward from EX/MEM (most recent result)
// 2'b10 = Forward from MEM/WB (older result)

// Forwarding for operand A (rs)
always @(*) begin
    // Default: no forwarding
    forward_a = 2'b00;
    
    // EX hazard (priority over MEM hazard - more recent)
    if (ex_mem_reg_write && 
        (ex_mem_rd != 4'd0) && 
        (ex_mem_rd == ex_rs)) begin
        forward_a = 2'b01;  // Forward from EX/MEM
    end
    // MEM hazard
    else if (mem_wb_reg_write && 
             (mem_wb_rd != 4'd0) && 
             (mem_wb_rd == ex_rs)) begin
        forward_a = 2'b10;  // Forward from MEM/WB
    end
end

// Forwarding for operand B (rt)
always @(*) begin
    // Default: no forwarding
    forward_b = 2'b00;
    
    // EX hazard
    if (ex_mem_reg_write && 
        (ex_mem_rd != 4'd0) && 
        (ex_mem_rd == ex_rt)) begin
        forward_b = 2'b01;  // Forward from EX/MEM
    end
    // MEM hazard
    else if (mem_wb_reg_write && 
             (mem_wb_rd != 4'd0) && 
             (mem_wb_rd == ex_rt)) begin
        forward_b = 2'b10;  // Forward from MEM/WB
    end
end

endmodule
```

### Forwarding Muxes in EX Stage

```verilog
// Forwarding unit instantiation
wire [1:0] forward_a;
wire [1:0] forward_b;

ForwardingUnit forwardUnit(
    .ex_rs          (id_ex_rs),
    .ex_rt          (id_ex_rt),
    .ex_mem_reg_write(ex_mem_reg_write),
    .ex_mem_rd      (ex_mem_rd),
    .mem_wb_reg_write(mem_wb_reg_write),
    .mem_wb_rd      (mem_wb_rd),
    .forward_a      (forward_a),
    .forward_b      (forward_b)
);

// Forwarding mux for ALU input A
wire [31:0] ex_forward_a_data;
assign ex_forward_a_data = (forward_a == 2'b01) ? ex_mem_alu_result :  // From EX/MEM
                           (forward_a == 2'b10) ? wb_result :          // From MEM/WB
                           id_ex_rs_data;                              // No forward

// Forwarding mux for ALU input B
wire [31:0] ex_forward_b_data;
assign ex_forward_b_data = (forward_b == 2'b01) ? ex_mem_alu_result :  // From EX/MEM
                           (forward_b == 2'b10) ? wb_result :          // From MEM/WB
                           id_ex_rt_data;                              // No forward

// Connect to ALU
assign ex_alu_a = ex_forward_a_data;
assign ex_alu_b = id_ex_alu_src ? id_ex_imm_extended : ex_forward_b_data;
```

### Double Data Hazard

```verilog
// Special case: Both EX/MEM and MEM/WB have result for same register
// Example:
//   ADD r1, r2, r3    (now in MEM/WB)
//   ADD r1, r1, r4    (now in EX/MEM) 
//   ADD r5, r1, r6    (now in EX, needs r1)
//
// Solution: EX/MEM has priority (most recent value)
// This is already handled by the if-else priority in ForwardingUnit
```

### Forwarding for Branch Comparisons

```verilog
// Branch comparisons in EX also need forwarding
// Branch compares rs and rt, both may need forwarding

wire [31:0] ex_branch_a = ex_forward_a_data;  // rs for comparison
wire [31:0] ex_branch_b = ex_forward_b_data;  // rt for comparison
```

### Debug Output

```verilog
always @(posedge clk) begin
    if (!reset && id_ex_valid) begin
        if (forward_a != 2'b00) begin
            $display("%0t FWD: rs(r%0d) <- %s = %h", 
                     $time, id_ex_rs,
                     forward_a == 2'b01 ? "EX/MEM" : "MEM/WB",
                     ex_forward_a_data);
        end
        if (forward_b != 2'b00) begin
            $display("%0t FWD: rt(r%0d) <- %s = %h", 
                     $time, id_ex_rt,
                     forward_b == 2'b01 ? "EX/MEM" : "MEM/WB",
                     ex_forward_b_data);
        end
    end
end
```

## Tasks

1. [ ] Create ForwardingUnit.v module
2. [ ] Implement forward_a detection logic
3. [ ] Implement forward_b detection logic
4. [ ] Add forwarding muxes in EX stage
5. [ ] Update ALU input connections
6. [ ] Update branch comparison inputs
7. [ ] Add debug output
8. [ ] Test with data hazard scenarios

## Definition of Done

- EX-to-EX forwarding works
- MEM-to-EX forwarding works
- Double hazard handled correctly
- Data hazard tests pass (without stalls for ALU-ALU)

## Test Plan

### Test 1: Back-to-Back ALU (EX-EX Forward)
```assembly
; Tests/CPU/09_pipeline_hazards/data_hazards_alu.asm
load 10 r1
load 20 r2
add r1 r2 r3    ; r3 = 30
add r3 r1 r15   ; Forward r3 from EX/MEM ; expected=40
halt
```

### Test 2: Two-Cycle Gap (MEM-EX Forward)
```assembly
load 5 r1
load 10 r2
add r1 r2 r3    ; r3 = 15
nop
add r3 r2 r15   ; Forward r3 from MEM/WB ; expected=25
halt
```

### Test 3: Double Hazard
```assembly
load 10 r1
add r1 r1 r1    ; r1 = 20 (in MEM/WB next cycle)
add r1 r1 r1    ; r1 = 40 (in EX/MEM next cycle, needs EX/MEM forward)
add r1 r0 r15   ; Forward r1 from EX/MEM ; expected=80
halt
```

## Dependencies

- Story 1-7 (Complete pipeline stages)

## Notes

- r0 forwarding is disabled (r0 always reads as 0)
- Load-use hazards require stalling (Story 9)
- Forwarding from MEM stage may need to handle memory read results too

## Review Checklist

- [ ] EX/MEM forwarding detects correct hazard
- [ ] MEM/WB forwarding detects correct hazard
- [ ] EX/MEM has priority over MEM/WB
- [ ] r0 forwarding disabled
- [ ] Both operands can be forwarded independently
- [ ] Branch comparisons use forwarded values
