# Story 05: Execute Stage (EX) - ALU Operations

**Sprint**: 1  
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

## Description

As a developer, I want to implement the EX stage so that ALU operations are performed and branch addresses are calculated.

## Acceptance Criteria

1. [ ] ALU instantiation with correct inputs
2. [ ] ALU source mux (register or immediate)
3. [ ] Branch address calculation
4. [ ] Jump address calculation
5. [ ] Branch condition evaluation
6. [ ] Basic ALU test passes (add, sub, or, and, etc.)

## Technical Details

### EX Stage Flow

```
ID/EX Register
    │
    ├─► ALU Source Mux ─► ALU ─► ALU Result
    │
    ├─► Branch Adder ─► Branch Target
    │
    └─► Branch Comparator ─► Branch Taken
    │
    ▼
EX/MEM Register
```

### ALU Interface (Existing Module)

```verilog
// ALU instantiation
ALU alu(
    .a          (ex_alu_a),
    .b          (ex_alu_b),
    .op         (id_ex_alu_op),
    
    .result     (ex_alu_result),
    .zero       (ex_alu_zero),
    .negative   (ex_alu_negative),
    .overflow   (ex_alu_overflow)
);
```

### ALU Source Selection

```verilog
// =============================================================================
// EXECUTE (EX) STAGE
// =============================================================================

// ALU input A - always from rs (possibly forwarded)
wire [31:0] ex_alu_a;
assign ex_alu_a = forward_a_sel ? forward_a_data : id_ex_rs_data;

// ALU input B - from rt or immediate
wire [31:0] ex_alu_b;
wire [31:0] ex_rt_forwarded = forward_b_sel ? forward_b_data : id_ex_rt_data;
assign ex_alu_b = id_ex_alu_src ? id_ex_imm_extended : ex_rt_forwarded;

// ALU outputs
wire [31:0] ex_alu_result;
wire        ex_alu_zero;
wire        ex_alu_negative;
wire        ex_alu_overflow;
```

### Branch Address Calculation

```verilog
// Branch target = PC + sign_extended_offset
// Note: PC in ID/EX is the instruction's PC, offset is relative
wire [31:0] ex_branch_target;
assign ex_branch_target = id_ex_pc + id_ex_imm_extended;

// Jump target - direct from instruction
wire [31:0] ex_jump_target;
assign ex_jump_target = {id_ex_pc[31:26], id_ex_const26};  // For direct jumps
// OR for register jumps:
wire [31:0] ex_jump_reg_target = ex_alu_a;  // Jump to address in register
```

### Branch Condition Evaluation

```verilog
// Branch conditions (based on opcode sub-type)
wire ex_branch_taken;

// Branch types from B32P ISA:
// BEQ: branch if rs == rt
// BNE: branch if rs != rt  
// BGT: branch if rs > rt (signed)
// BGE: branch if rs >= rt (signed)
// BLT: branch if rs < rt (signed)
// BLE: branch if rs <= rt (signed)
// BGTU, BGEU, BLTU, BLEU: unsigned versions

wire ex_eq  = (ex_alu_a == ex_rt_forwarded);
wire ex_lt  = $signed(ex_alu_a) < $signed(ex_rt_forwarded);
wire ex_ltu = ex_alu_a < ex_rt_forwarded;

// Branch taken based on branch type
always @(*) begin
    case (id_ex_branch_type)
        BR_BEQ:  ex_branch_taken = ex_eq;
        BR_BNE:  ex_branch_taken = !ex_eq;
        BR_BGT:  ex_branch_taken = !ex_lt && !ex_eq;
        BR_BGE:  ex_branch_taken = !ex_lt;
        BR_BLT:  ex_branch_taken = ex_lt;
        BR_BLE:  ex_branch_taken = ex_lt || ex_eq;
        BR_BGTU: ex_branch_taken = !ex_ltu && !ex_eq;
        BR_BGEU: ex_branch_taken = !ex_ltu;
        BR_BLTU: ex_branch_taken = ex_ltu;
        BR_BLEU: ex_branch_taken = ex_ltu || ex_eq;
        default: ex_branch_taken = 1'b0;
    endcase
end
```

### PC Source Signal Generation

```verilog
// Generate PC redirect signal for IF stage
wire ex_pc_redirect;
assign ex_pc_redirect = (id_ex_branch && ex_branch_taken) || id_ex_jump;

wire [31:0] ex_pc_target;
assign ex_pc_target = id_ex_jump ? 
    (id_ex_jump_reg ? ex_jump_reg_target : ex_jump_target) :
    ex_branch_target;
```

### Debug Output

```verilog
always @(posedge clk) begin
    if (!reset && id_ex_valid) begin
        $display("%0t EX: alu_a=%h alu_b=%h op=%h result=%h",
                 $time, ex_alu_a, ex_alu_b, id_ex_alu_op, ex_alu_result);
        if (id_ex_branch) begin
            $display("%0t EX: BRANCH taken=%b target=%h",
                     $time, ex_branch_taken, ex_branch_target);
        end
        if (id_ex_jump) begin
            $display("%0t EX: JUMP target=%h", $time, ex_pc_target);
        end
    end
end
```

## Tasks

1. [ ] Instantiate ALU with ID/EX register inputs
2. [ ] Implement ALU source A mux (with forwarding placeholder)
3. [ ] Implement ALU source B mux (register vs immediate)
4. [ ] Implement branch target adder
5. [ ] Implement jump target calculation
6. [ ] Implement branch condition logic
7. [ ] Generate PC redirect signals for IF stage
8. [ ] Wire results to EX/MEM pipeline register
9. [ ] Add debug `$display` statements
10. [ ] Test with basic ALU operations

## Definition of Done

- ALU computes correct results
- Branch targets calculated correctly
- Branch conditions evaluated correctly
- PC redirect signals generated
- Basic ALU tests pass (add, sub, and, or, xor)

## Test Plan

### Test 1: Basic ALU
```assembly
; Tests/CPU/02_alu_basic/add.asm
load 5 r1
load 10 r2
add r1 r2 r15  ; expected=15
halt
```

### Test 2: Branch Taken
```assembly
; Tests/CPU/07_branch/beq_taken.asm
load 5 r1
load 5 r2
beq r1 r2 skip
load 0 r15  ; should be skipped
jump end
skip:
load 1 r15  ; expected=1
end:
halt
```

## Dependencies

- Story 1: Project Setup
- Story 2: Pipeline Registers  
- Story 3: IF Stage
- Story 4: ID Stage

## Notes

- For the tests, make sure to use the project Makefile to run the test. Inspect the Makefile to understand how the test framework works.
- There are two testbenches: cpu_tb.v for `make sim-cpu` and cpu_tests_tb.v that is used with the `make test-cpu` commands.
- Forwarding will be added in Story 8 - use direct register values for now
- Branch resolution in EX means 2-cycle penalty (flush IF/ID and ID/EX)
- Some operations (multiply/divide) are multi-cycle - handled in Story 12

## Review Checklist

- [ ] All ALU operations supported
- [ ] Correct handling of signed vs unsigned operations
- [ ] Branch conditions match ISA specification
- [ ] Jump targets calculated correctly
- [ ] PC redirect timing correct (available for next IF)
