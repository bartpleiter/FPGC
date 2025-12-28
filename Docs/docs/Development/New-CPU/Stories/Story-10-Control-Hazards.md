# Story 10: Control Hazards (Branch/Jump)

**Sprint**: 2  
**Priority**: P0 (Blocker)  
**Estimate**: 2 hours  
**Status**: Not Started

## Description

As a developer, I want to implement control hazard handling so that branches and jumps correctly flush incorrect instructions from the pipeline.

## Acceptance Criteria

1. [ ] Branch taken detection in EX stage
2. [ ] Jump detection in EX stage
3. [ ] Pipeline flush on branch/jump taken
4. [ ] PC redirect to target address
5. [ ] Branch and jump tests pass

## Technical Details

### Control Hazard Scenarios

```
Cycle    IF       ID       EX       Action
------------------------------------------
1        BEQ      -        -        Fetch branch
2        instr1   BEQ      -        Fetch next (speculative)
3        instr2   instr1   BEQ      Branch resolves in EX
4        target   NOP      NOP      If taken: flush IF/ID, ID/EX, redirect PC
   OR
4        instr3   instr2   instr1   If not taken: continue normally
```

### Branch Penalty

With branch resolution in EX stage:
- **Not taken**: 0 cycle penalty (predict not taken)
- **Taken**: 2 cycle penalty (flush 2 instructions)

### Flush Logic

```verilog
// =============================================================================
// CONTROL HAZARD HANDLING
// =============================================================================

// Branch resolution in EX stage
wire ex_branch_resolved = id_ex_branch && id_ex_valid;
wire ex_branch_taken;  // From branch comparator (Story 5)

// Jump detection in EX stage
wire ex_jump = id_ex_jump && id_ex_valid;

// Control hazard - need to flush pipeline
wire control_hazard = (ex_branch_resolved && ex_branch_taken) || ex_jump;

// Flush signals for IF/ID and ID/EX
wire flush_if_id = control_hazard;
wire flush_id_ex = control_hazard;

// PC redirect
wire [31:0] pc_redirect_target = ex_branch_taken ? ex_branch_target : 
                                 ex_jump ? ex_jump_target : 
                                 32'h0;
wire pc_redirect = control_hazard;
```

### PC Redirect Implementation

```verilog
// PC source selection
localparam PC_NEXT      = 2'b00;  // pc + 1
localparam PC_BRANCH    = 2'b01;  // branch target
localparam PC_JUMP      = 2'b10;  // jump target
localparam PC_EXCEPTION = 2'b11;  // exception handler

wire [1:0] pc_src;
assign pc_src = control_hazard ? 
                (ex_jump ? PC_JUMP : PC_BRANCH) : 
                PC_NEXT;

// PC next value
wire [31:0] pc_next;
assign pc_next = (pc_src == PC_BRANCH)    ? ex_branch_target :
                 (pc_src == PC_JUMP)      ? ex_jump_target :
                 (pc_src == PC_EXCEPTION) ? exception_handler :
                 pc + 1;

// PC update
always @(posedge clk) begin
    if (reset) begin
        pc <= 32'h0000_0000;
    end else if (pc_write && !pipeline_stall) begin
        pc <= pc_next;
    end
end
```

### Pipeline Flush Implementation

```verilog
// IF/ID flush
always @(posedge clk) begin
    if (reset || flush_if_id) begin
        if_id_valid <= 1'b0;
        if_id_pc <= 32'b0;
        if_id_instr <= 32'b0;  // NOP
    end else if (if_id_write && !stall_if) begin
        // Normal operation
        if_id_valid <= 1'b1;
        if_id_pc <= pc;
        if_id_instr <= if_instr;
    end
end

// ID/EX flush
always @(posedge clk) begin
    if (reset || flush_id_ex) begin
        id_ex_valid <= 1'b0;
        id_ex_reg_write <= 1'b0;
        id_ex_mem_read <= 1'b0;
        id_ex_mem_write <= 1'b0;
        id_ex_branch <= 1'b0;
        id_ex_jump <= 1'b0;
        // All control signals to safe values
    end else if (!stall_id) begin
        // Normal operation
        id_ex_valid <= if_id_valid;
        id_ex_pc <= if_id_pc;
        // ... etc
    end
end
```

### Jump Types

```verilog
// Jump types in B32P ISA:
// 1. JAL  - Jump and link (save return address)
// 2. JUMP - Unconditional jump
// 3. JR   - Jump register (indirect)
// 4. JALR - Jump and link register (indirect)

// Jump target calculation
wire [31:0] ex_jump_target;

// Direct jump target (from instruction immediate)
wire [31:0] jump_direct_target = {id_ex_pc[31:26], id_ex_const26};

// Indirect jump target (from register)
wire [31:0] jump_indirect_target = ex_forward_a_data;  // rs with forwarding

assign ex_jump_target = id_ex_jump_reg ? jump_indirect_target : jump_direct_target;
```

### Link Address (for JAL/JALR)

```verilog
// Return address = PC + 1 (next instruction)
wire [31:0] ex_link_addr = id_ex_pc + 1;

// Pass to EX/MEM for writeback to register (typically r14/r15)
// In WB stage, select link_addr when instruction is JAL/JALR
```

### Debug Output

```verilog
always @(posedge clk) begin
    if (!reset) begin
        if (ex_branch_resolved) begin
            $display("%0t BRANCH: pc=%h taken=%b target=%h",
                     $time, id_ex_pc, ex_branch_taken, ex_branch_target);
        end
        if (ex_jump) begin
            $display("%0t JUMP: pc=%h target=%h reg=%b",
                     $time, id_ex_pc, ex_jump_target, id_ex_jump_reg);
        end
        if (control_hazard) begin
            $display("%0t CONTROL HAZARD: Flushing IF/ID and ID/EX, redirect to %h",
                     $time, pc_redirect_target);
        end
    end
end
```

## Tasks

1. [ ] Implement branch taken detection
2. [ ] Implement jump detection
3. [ ] Implement flush signal generation
4. [ ] Implement PC redirect logic
5. [ ] Update IF/ID register for flush
6. [ ] Update ID/EX register for flush
7. [ ] Handle JAL link address
8. [ ] Add debug output
9. [ ] Test branch taken/not taken
10. [ ] Test jump direct and indirect

## Definition of Done

- Branch taken causes 2-cycle flush
- Branch not taken continues normally
- Jumps cause 2-cycle flush
- PC correctly redirects to target
- JAL saves return address
- Branch and jump tests pass

## Test Plan

### Test 1: Branch Taken
```assembly
; Tests/CPU/07_branch/beq_taken.asm
load 5 r1
load 5 r2
beq r1 r2 target    ; Should branch
load 99 r15         ; Should be flushed!
halt
target:
load 1 r15          ; expected=1
halt
```

### Test 2: Branch Not Taken
```assembly
load 5 r1
load 10 r2
beq r1 r2 target    ; Should NOT branch
load 1 r15          ; Should execute ; expected=1
halt
target:
load 99 r15
halt
```

### Test 3: Jump
```assembly
; Tests/CPU/05_jump/jump.asm
jump target
load 99 r15         ; Should be flushed!
halt
target:
load 42 r15         ; expected=42
halt
```

### Test 4: Jump Register
```assembly
load target r1
jr r1               ; Jump to address in r1
load 99 r15         ; Should be flushed!
halt
target:
load 42 r15         ; expected=42
halt
```

## Dependencies

- Story 1-9 (Pipeline with data hazard handling)

## Notes

- 2-cycle branch penalty is acceptable for now
- Could optimize with branch prediction later
- Flush must happen same cycle as branch resolution

## Review Checklist

- [ ] Branch taken flushes 2 instructions
- [ ] Branch not taken continues normally
- [ ] All branch types work (BEQ, BNE, BGT, etc.)
- [ ] Direct jump works
- [ ] Indirect jump (JR) works
- [ ] JAL saves return address correctly
- [ ] No instructions executed after taken branch
