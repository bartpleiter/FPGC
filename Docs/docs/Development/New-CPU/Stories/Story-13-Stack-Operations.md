# Story 13: Stack Operations (Push/Pop)

**Sprint**: 3  
**Priority**: P0 (Blocker)  
**Estimate**: 2 hours  
**Status**: Not Started

## Description

As a developer, I want to implement hardware stack operations so that push and pop instructions work correctly.

## Acceptance Criteria

1. [ ] Stack module instantiation
2. [ ] Push operation (write to stack)
3. [ ] Pop operation (read from stack)
4. [ ] Stack pointer management
5. [ ] Push and pop tests pass

## Technical Details

### Stack Module Interface (Existing)

```verilog
// From existing Stack.v
module Stack(
    input wire clk,
    input wire reset,
    input wire push,
    input wire pop,
    input wire [31:0] d,
    output wire [31:0] q
);
// 128-word hardware stack
// Push increments SP, writes data
// Pop reads data, decrements SP
```

### Stack Operations in Pipeline

```
Push: EX stage performs push (stack write)
Pop:  MEM stage performs pop (stack read), WB writes to register
```

### Stack Integration

```verilog
// =============================================================================
// HARDWARE STACK INTEGRATION
// =============================================================================

// Stack control signals from pipeline
wire stack_push = ex_mem_stack_push && ex_mem_valid && !pipeline_stall;
wire stack_pop  = ex_mem_stack_pop && ex_mem_valid && !pipeline_stall;

// Data to push (from rs register, with forwarding)
wire [31:0] stack_push_data = ex_mem_rt_data;  // Data from rt

// Stack instantiation
wire [31:0] stack_pop_data;

Stack stack(
    .clk    (clk),
    .reset  (reset),
    .push   (stack_push),
    .pop    (stack_pop),
    .d      (stack_push_data),
    .q      (stack_pop_data)
);
```

### Push Operation

```verilog
// PUSH instruction (opcode: PUSH)
// Pushes value from register onto stack
// No register writeback needed

// In ID stage:
wire id_is_push = (id_opcode == OP_PUSH);
wire id_stack_push = id_is_push;
wire id_reg_write_push = 1'b0;  // Push doesn't write to register

// In EX stage:
// Push data comes from forwarded rt
wire [31:0] ex_push_data = ex_forward_b_data;

// In MEM stage (or late EX):
// Actually perform push when instruction reaches this stage
// stack_push signal generated as shown above
```

### Pop Operation

```verilog
// POP instruction (opcode: POP)
// Pops value from stack into destination register
// Similar to load operation - data available in MEM stage

// In ID stage:
wire id_is_pop = (id_opcode == OP_POP);
wire id_stack_pop = id_is_pop;
wire id_reg_write_pop = 1'b1;  // Pop writes to destination register
wire id_stack_to_reg = id_is_pop;

// In MEM stage:
// Perform pop
// stack_pop signal generated as shown above
// stack_pop_data available combinationally

// In WB stage:
// Select stack data as writeback source
wire [31:0] wb_data = mem_wb_stack_to_reg ? mem_wb_stack_data : 
                      mem_wb_mem_to_reg ? mem_wb_mem_data :
                      mem_wb_alu_result;
```

### Pop-Use Hazard

```verilog
// Pop has similar hazard to Load - data not available until MEM stage
// If instruction immediately after POP uses the popped register, stall!

// In HazardUnit:
wire pop_use_hazard = id_ex_stack_pop && 
                      ((id_ex_rd == id_rs) || (id_ex_rd == id_rt)) &&
                      (id_ex_rd != 4'd0);

// Add to load-use hazard detection:
wire load_use_hazard = (id_ex_mem_read || id_ex_stack_pop) && 
                       ((id_ex_rd == id_rs) || (id_ex_rd == id_rt)) &&
                       (id_ex_rd != 4'd0);
```

### Stack Overflow/Underflow

```verilog
// Stack module should handle overflow/underflow internally
// Options:
// 1. Wrap around (SP % 128)
// 2. Saturate at max/min
// 3. Generate exception (if interrupt support)

// For Phase 1: assume no overflow/underflow (software responsibility)
// Stack.v should have appropriate behavior defined
```

### Call/Return Convenience

```verilog
// CALL instruction = PUSH PC+1, then JUMP
// RET instruction = POP into PC

// These may be macro instructions or special opcodes
// Check ISA specification for exact implementation

// If CALL/RET are separate instructions:
wire id_is_call = (id_opcode == OP_CALL);
wire id_is_ret = (id_opcode == OP_RET);

// CALL: push return address, then jump
// In EX: push pc+1 to stack, compute jump target
// Control hazard handling flushes instructions after CALL

// RET: pop address, then jump to it
// Special handling - pop address and redirect PC
```

### Debug Output

```verilog
always @(posedge clk) begin
    if (!reset) begin
        if (stack_push) begin
            $display("%0t STACK: PUSH data=%h", $time, stack_push_data);
        end
        if (stack_pop) begin
            $display("%0t STACK: POP data=%h", $time, stack_pop_data);
        end
    end
end
```

## Tasks

1. [ ] Verify Stack.v interface
2. [ ] Instantiate Stack module
3. [ ] Implement push signal generation
4. [ ] Implement pop signal generation
5. [ ] Connect push data path
6. [ ] Connect pop data to WB mux
7. [ ] Add pop-use hazard detection
8. [ ] Implement CALL/RET if needed
9. [ ] Add debug output
10. [ ] Test push/pop operations

## Definition of Done

- Push writes value to stack
- Pop reads value from stack
- Pop result written to register
- Pop-use hazard handled correctly
- Stack tests pass

## Test Plan

### Test 1: Simple Push/Pop
```assembly
; Tests/CPU/06_stack/push_pop.asm
load 42 r1
push r1          ; Push 42 onto stack
load 0 r1        ; Clear r1
pop r15          ; Pop into r15 ; expected=42
halt
```

### Test 2: Multiple Push/Pop
```assembly
load 1 r1
load 2 r2
load 3 r3
push r1          ; Stack: [1]
push r2          ; Stack: [1, 2]
push r3          ; Stack: [1, 2, 3]
pop r4           ; r4 = 3, Stack: [1, 2]
pop r5           ; r5 = 2, Stack: [1]
pop r6           ; r6 = 1, Stack: []
add r4 r5 r7     ; r7 = 5
add r7 r6 r15    ; r15 = 6 ; expected=6
halt
```

### Test 3: Pop-Use Hazard
```assembly
load 42 r1
push r1
pop r2           ; Pop into r2
add r2 r0 r15    ; Use r2 immediately - hazard! ; expected=42
halt
```

## Dependencies

- Story 1-10 (Complete pipeline with hazards)
- External: Stack.v (existing)

## Notes

- Stack is 128 words deep (sufficient for most use cases)
- LIFO order: last pushed = first popped
- Pop has same hazard timing as load

## Review Checklist

- [ ] Push writes correct data
- [ ] Pop returns correct data (LIFO order)
- [ ] Pop result correctly routed to WB
- [ ] Pop-use hazard detected and handled
- [ ] Stack operations work with forwarding
- [ ] No stack corruption during stalls
