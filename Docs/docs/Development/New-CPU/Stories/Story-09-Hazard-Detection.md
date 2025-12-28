# Story 09: Hazard Detection Unit

**Sprint**: 2  
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

## Description

As a developer, I want to implement hazard detection so that load-use hazards are handled correctly with stalls.

## Acceptance Criteria

1. [ ] Load-use hazard detection
2. [ ] Pipeline stall generation
3. [ ] Bubble insertion on stall
4. [ ] Multi-cycle operation stall handling
5. [ ] Load-use hazard tests pass

## Technical Details

### Hazard Types

```
Type              | Detection                     | Resolution
------------------|-------------------------------|-------------
Load-use          | ID/EX.mem_read && rs/rt match | Stall + Bubble
Multi-cycle ALU   | Division/multiply in progress | Stall pipeline
Cache miss (IF)   | L1I miss                      | Stall pipeline
Cache miss (MEM)  | L1D miss                      | Stall pipeline
Memory Unit busy  | MU operation in progress      | Stall pipeline
```

### Hazard Detection Unit Module

```verilog
// =============================================================================
// HAZARD DETECTION UNIT
// =============================================================================

module HazardUnit(
    // ID stage operand registers
    input wire [3:0] id_rs,            // Source register 1 being read
    input wire [3:0] id_rt,            // Source register 2 being read
    
    // ID/EX stage info
    input wire       id_ex_mem_read,   // Is ID/EX a load instruction?
    input wire [3:0] id_ex_rd,         // Destination register in ID/EX
    
    // Multi-cycle operation status
    input wire       multicycle_busy,   // Division/multiply in progress
    
    // Cache/Memory status
    input wire       l1i_stall,         // L1I cache miss
    input wire       l1d_stall,         // L1D cache miss
    input wire       mu_stall,          // Memory unit busy
    
    // Hazard outputs
    output wire      stall_if,          // Stall IF stage
    output wire      stall_id,          // Stall ID stage
    output wire      flush_id_ex,       // Insert bubble in ID/EX
    output wire      pc_write,          // Enable PC update
    output wire      if_id_write        // Enable IF/ID register update
);

// Load-use hazard detection
wire load_use_hazard;
assign load_use_hazard = id_ex_mem_read && 
                         ((id_ex_rd == id_rs) || (id_ex_rd == id_rt)) &&
                         (id_ex_rd != 4'd0);  // r0 doesn't cause hazard

// Combined stall signal
wire pipeline_stall;
assign pipeline_stall = load_use_hazard || 
                        multicycle_busy || 
                        l1i_stall || 
                        l1d_stall || 
                        mu_stall;

// Output assignments
assign stall_if     = pipeline_stall;
assign stall_id     = pipeline_stall;
assign flush_id_ex  = load_use_hazard;  // Insert bubble only for load-use
assign pc_write     = !pipeline_stall;
assign if_id_write  = !pipeline_stall;

endmodule
```

### Stall and Bubble Logic

```verilog
// In main CPU module:

// Hazard unit instantiation
HazardUnit hazardUnit(
    .id_rs          (id_rs),
    .id_rt          (id_rt),
    .id_ex_mem_read (id_ex_mem_read),
    .id_ex_rd       (id_ex_rd),
    .multicycle_busy(multicycle_busy),
    .l1i_stall      (if_cache_stall),
    .l1d_stall      (mem_l1d_stall),
    .mu_stall       (mem_mu_stall),
    .stall_if       (stall_if),
    .stall_id       (stall_id),
    .flush_id_ex    (hazard_flush_id_ex),
    .pc_write       (pc_write),
    .if_id_write    (if_id_write)
);

// PC update with stall control
always @(posedge clk) begin
    if (reset) begin
        pc <= 32'h0000_0000;
    end else if (pc_write) begin
        if (branch_taken || jump_taken) begin
            pc <= branch_jump_target;
        end else begin
            pc <= pc + 1;
        end
    end
    // On stall: hold current PC (pc_write = 0)
end

// IF/ID register with stall control
always @(posedge clk) begin
    if (reset || flush_if_id) begin
        if_id_valid <= 1'b0;
        if_id_instr <= NOP;
    end else if (if_id_write) begin
        if_id_valid <= 1'b1;
        if_id_pc <= pc;
        if_id_instr <= if_instr;
    end
    // On stall: hold current values (if_id_write = 0)
end

// ID/EX register with bubble insertion
always @(posedge clk) begin
    if (reset || hazard_flush_id_ex) begin
        // Insert bubble (NOP)
        id_ex_valid <= 1'b0;
        id_ex_reg_write <= 1'b0;
        id_ex_mem_read <= 1'b0;
        id_ex_mem_write <= 1'b0;
        // ... clear all control signals
    end else if (!stall_id) begin
        // Normal pipeline advance
        id_ex_valid <= if_id_valid;
        id_ex_pc <= if_id_pc;
        // ... pass all values
    end
end
```

### Load-Use Hazard Example

```
Cycle    IF       ID       EX       MEM      WB       Action
--------------------------------------------------------------
1        LOAD     -        -        -        -        
2        ADD      LOAD     -        -        -        Detect hazard
3        ADD      NOP      LOAD     -        -        Stall IF/ID, bubble ID/EX
4        SUB      ADD      NOP      LOAD     -        Resume, forward LOAD result
5        -        SUB      ADD      NOP      LOAD     
```

### Multi-Cycle Stall

```verilog
// Multi-cycle operations (div/mult) stall entire pipeline
// No bubble insertion - just hold everything
always @(posedge clk) begin
    if (multicycle_busy) begin
        // All registers hold current values
        // No stage advances
    end
end
```

### Debug Output

```verilog
always @(posedge clk) begin
    if (!reset) begin
        if (load_use_hazard) begin
            $display("%0t HAZARD: Load-use detected! ID/EX.rd=%d, ID.rs=%d, ID.rt=%d",
                     $time, id_ex_rd, id_rs, id_rt);
            $display("%0t HAZARD: Stalling IF/ID, inserting bubble in ID/EX", $time);
        end
        if (multicycle_busy) begin
            $display("%0t HAZARD: Multi-cycle operation in progress, stalling", $time);
        end
        if (l1i_stall) begin
            $display("%0t HAZARD: L1I cache miss, stalling", $time);
        end
        if (l1d_stall) begin
            $display("%0t HAZARD: L1D cache miss, stalling", $time);
        end
    end
end
```

## Tasks

1. [ ] Create HazardUnit.v module
2. [ ] Implement load-use hazard detection
3. [ ] Implement stall signal generation
4. [ ] Implement bubble insertion logic
5. [ ] Connect multi-cycle stall signals
6. [ ] Connect cache stall signals
7. [ ] Update PC logic to respect stall
8. [ ] Update IF/ID register to respect stall
9. [ ] Update ID/EX register for bubble insertion
10. [ ] Add debug output
11. [ ] Test with load-use hazard cases

## Definition of Done

- Load-use hazards detected correctly
- Pipeline stalls appropriately
- Bubbles inserted correctly
- No data corruption during stalls
- Load-use hazard tests pass

## Test Plan

### Test 1: Load-Use Hazard
```assembly
; Load followed immediately by use
load 0x200 r1        ; Address of memory
write r1 r0 0        ; Clear memory location
load 42 r2
write r1 r2 0        ; Write 42 to memory
read r1 r3 0         ; Load from memory into r3
add r3 r0 r15        ; Use r3 immediately - LOAD-USE HAZARD! ; expected=42
halt
```

### Test 2: No Hazard (2 cycle gap)
```assembly
read r1 r3 0         ; Load
nop
nop
add r3 r0 r15        ; Use - no hazard, forwarding works ; expected=X
halt
```

### Test 3: Load-Use with Forward
```assembly
; After stall, result should be available via forwarding
load 10 r1
load 20 r2  
add r1 r2 r3         ; r3 = 30
read 0x200 r4        ; Load something
add r4 r3 r15        ; Load-use on r4, stall, then forward ; expected=30+[0x200]
halt
```

## Dependencies

- Story 1-8 (Pipeline with forwarding)

## Notes

- Load-use hazard requires 1 stall cycle
- After stall, forwarding handles the data
- Multi-cycle ops may need multiple stall cycles
- Cache misses can cause many stall cycles

## Review Checklist

- [ ] Load-use hazard correctly detected
- [ ] Stall signals reach all relevant pipeline stages
- [ ] Bubble correctly inserted (control signals zeroed)
- [ ] PC holds during stall
- [ ] IF/ID holds during stall
- [ ] No double-stalling bugs
- [ ] r0 doesn't trigger false hazards
