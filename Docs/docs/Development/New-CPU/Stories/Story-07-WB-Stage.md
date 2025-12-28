# Story 07: Writeback Stage (WB)

**Sprint**: 1  
**Priority**: P0 (Blocker)  
**Estimate**: 2 hours  
**Status**: Not Started

## Description

As a developer, I want to implement the WB stage so that results are written back to the register file.

## Acceptance Criteria

1. [ ] Result selection (ALU result vs memory data vs stack data)
2. [ ] Register file write enable generation
3. [ ] Register file write data path
4. [ ] End-to-end data flow test passes

## Technical Details

### WB Stage Flow

```
MEM/WB Register
    │
    ├─► Result Mux (ALU/MEM/Stack) ─► Write Data
    │
    └─► Write Enable & Destination ─► Regbank
```

### Result Selection

```verilog
// =============================================================================
// WRITEBACK (WB) STAGE
// =============================================================================

// Write data selection
wire [31:0] wb_result;

// Result sources:
// 1. ALU result (arithmetic/logic operations)
// 2. Memory read data (load operations)
// 3. Stack pop data (pop operations)
// 4. PC+1 (for JAL - jump and link)

reg [31:0] wb_data;
always @(*) begin
    case (1'b1)
        mem_wb_mem_to_reg:   wb_data = mem_wb_mem_data;    // Memory load
        mem_wb_stack_to_reg: wb_data = mem_wb_stack_data;  // Stack pop
        mem_wb_pc_to_reg:    wb_data = mem_wb_pc + 1;      // Link address
        default:             wb_data = mem_wb_alu_result;  // ALU result
    endcase
end

assign wb_result = wb_data;
```

### Register Write Control

```verilog
// Write enable - only if valid instruction and reg_write control set
wire wb_reg_write_en;
assign wb_reg_write_en = mem_wb_valid && mem_wb_reg_write && (mem_wb_rd != 4'd0);

// Destination register
wire [3:0] wb_rd = mem_wb_rd;

// Register file connection
// (Already instantiated in ID stage, connect write signals)
// regbank.addr_d = wb_rd
// regbank.d = wb_data
// regbank.we = wb_reg_write_en
```

### Write to r0 Handling

```verilog
// r0 is hardwired to 0 in most RISC architectures
// Option 1: Don't write to r0 (check in WB)
assign wb_reg_write_en = mem_wb_valid && mem_wb_reg_write && (mem_wb_rd != 4'd0);

// Option 2: Regbank ignores writes to r0 (check in module)
// Either approach works, but checking in WB is safer
```

### Debug Output

```verilog
always @(posedge clk) begin
    if (!reset && mem_wb_valid && mem_wb_reg_write) begin
        $display("%0t WB: r%0d = %h (from %s)", 
                 $time, mem_wb_rd, wb_data,
                 mem_wb_mem_to_reg ? "MEM" : 
                 mem_wb_stack_to_reg ? "STACK" :
                 mem_wb_pc_to_reg ? "PC+1" : "ALU");
    end
end

// Critical debug: This is what tests check!
// The test framework looks for "reg r15:" in output
always @(posedge clk) begin
    if (!reset && mem_wb_valid && mem_wb_reg_write && mem_wb_rd == 4'd15) begin
        $display("%0t reg r15: %0d", $time, wb_data);
    end
end
```

### Register File Interface (Complete)

```verilog
// Complete Regbank instantiation showing all connections
Regbank regbank(
    .clk        (clk),
    .reset      (reset),
    
    // Read port 1 - for ID stage (rs)
    .addr_a     (id_rs),
    .q_a        (id_rs_data),
    
    // Read port 2 - for ID stage (rt)
    .addr_b     (id_rt),
    .q_b        (id_rt_data),
    
    // Write port - from WB stage
    .addr_d     (wb_rd),
    .d          (wb_data),
    .we         (wb_reg_write_en)
);
```

### Write Collision Handling

```verilog
// If ID reads same register that WB writes in same cycle:
// - Regbank should forward write data to read (handled in Regbank)
// - OR forwarding unit handles it (Story 8)
// Check Regbank.v implementation for this behavior
```

## Tasks

1. [ ] Implement result selection mux
2. [ ] Implement write enable logic
3. [ ] Connect WB to Regbank write port
4. [ ] Handle r0 write suppression
5. [ ] Add critical "reg r15:" debug output (for tests!)
6. [ ] Add general debug output
7. [ ] Test end-to-end data flow

## Definition of Done

- Results correctly written to register file
- ALU, memory, and stack results all work
- r0 remains zero
- Test framework detects r15 value
- `load_immediate.asm` passes end-to-end

## Test Plan

### Test 1: Complete Data Flow
```assembly
; Complete test: IF -> ID -> EX -> MEM -> WB
load 37 r15  ; expected=37
halt
```

This tests:
- IF: Fetch from ROM
- ID: Decode LOAD instruction
- EX: Compute immediate value
- MEM: (no memory access for immediate load)
- WB: Write 37 to r15

### Test 2: Memory Load
```assembly
load 0x200 r1    ; VRAM32 address
write r1 r2 0    ; r2=0, write 0 to VRAM32[0] (init)
load 99 r3
write r1 r3 0    ; Write 99 to VRAM32[0]
read r1 r15 0    ; Load from VRAM32[0] ; expected=99
halt
```

## Dependencies

- Story 1-6 (All previous pipeline stages)

## Notes

- For the tests, make sure to use the project Makefile to run the test. Inspect the Makefile to understand how the test framework works.
- There are two testbenches: cpu_tb.v for `make sim-cpu` and cpu_tests_tb.v that is used with the `make test-cpu` commands.
- The "reg r15:" output format is CRITICAL - tests parse this!
- Write happens on rising clock edge
- Data is available for forwarding same cycle as write

## Review Checklist

- [ ] All result sources handled (ALU, MEM, STACK, PC+1)
- [ ] Write enable gated correctly
- [ ] r0 protection in place
- [ ] Debug output includes "reg r15:" format
- [ ] Write timing correct (posedge clk)
