# Story 04: Instruction Decode Stage (ID)

**Sprint**: 1  
**Priority**: P0 (Blocker)  
**Estimate**: 2 hours  
**Status**: Not Started

## Description

As a developer, I want to implement the ID stage so that instructions are decoded and register values are read.

## Acceptance Criteria

1. [ ] Instruction decoding using existing InstructionDecoder module
2. [ ] Control signal generation using existing ControlUnit module  
3. [ ] Register file read (2 source registers)
4. [ ] Sign/zero extension of immediate values
5. [ ] Pass decoded signals to ID/EX register

## Technical Details

### Instruction Decode Flow

```
IF/ID Register
    │
    ├─► InstructionDecoder ─► Decoded fields (rd, rs, rt, opcode, etc.)
    │
    ├─► ControlUnit ─► Control signals (reg_write, mem_read, etc.)
    │
    └─► Regbank ─► Register values (rs_data, rt_data)
    │
    ▼
ID/EX Register
```

### Instruction Decoder Interface (Existing Module)

```verilog
// Instantiate existing InstructionDecoder
InstructionDecoder instrDecoder(
    .instr      (if_id_instr),
    
    .opcode     (id_opcode),
    .rd         (id_rd),
    .rs         (id_rs),
    .rt         (id_rt),
    .areg       (id_areg),
    .const26    (id_const26),
    .const16    (id_const16),
    .const11    (id_const11)
);
```

### Control Unit Interface (Existing Module)

```verilog
// Instantiate existing ControlUnit  
ControlUnit controlUnit(
    .opcode         (id_opcode),
    
    .reg_write      (id_reg_write),
    .mem_read       (id_mem_read),
    .mem_write      (id_mem_write),
    .branch         (id_branch),
    .jump           (id_jump),
    .alu_op         (id_alu_op),
    .alu_src        (id_alu_src),
    .stack_push     (id_stack_push),
    .stack_pop      (id_stack_pop),
    .multi_cycle    (id_multi_cycle)
);
```

### Register File Interface

```verilog
// Regbank instantiation
// Note: Read is combinational, write is on clock edge
Regbank regbank(
    .clk        (clk),
    .reset      (reset),
    
    // Read port 1 (source register rs)
    .addr_a     (id_rs),
    .q_a        (id_rs_data),
    
    // Read port 2 (source register rt)  
    .addr_b     (id_rt),
    .q_b        (id_rt_data),
    
    // Write port (from WB stage)
    .addr_d     (wb_rd),
    .d          (wb_data),
    .we         (wb_reg_write)
);
```

### ID Stage Logic

```verilog
// =============================================================================
// INSTRUCTION DECODE (ID) STAGE
// =============================================================================

// Decoded instruction fields (from InstructionDecoder)
wire [3:0]  id_opcode;
wire [3:0]  id_rd;
wire [3:0]  id_rs;
wire [3:0]  id_rt;
wire [3:0]  id_areg;
wire [25:0] id_const26;
wire [15:0] id_const16;
wire [10:0] id_const11;

// Control signals (from ControlUnit)
wire        id_reg_write;
wire        id_mem_read;
wire        id_mem_write;
wire        id_branch;
wire        id_jump;
wire [3:0]  id_alu_op;
wire        id_alu_src;
wire        id_stack_push;
wire        id_stack_pop;
wire        id_multi_cycle;

// Register file read data
wire [31:0] id_rs_data;
wire [31:0] id_rt_data;

// Sign-extended immediate
wire [31:0] id_imm_extended;
assign id_imm_extended = {{16{id_const16[15]}}, id_const16};  // Sign extend

// Zero-extended immediate (for unsigned operations)
wire [31:0] id_imm_zero_extended;
assign id_imm_zero_extended = {16'b0, id_const16};
```

### Immediate Extension

Different instruction types need different extension:

```verilog
// Immediate selection based on operation
wire id_use_sign_ext = (id_opcode == OP_ADDI) ||
                       (id_opcode == OP_BRANCH_XX) ||
                       (id_opcode == OP_LOAD) ||
                       (id_opcode == OP_STORE);

wire [31:0] id_immediate = id_use_sign_ext ? id_imm_extended : id_imm_zero_extended;
```

### Debug Output

```verilog
always @(posedge clk) begin
    if (!reset && if_id_valid) begin
        $display("%0t ID: opcode=%h rd=%d rs=%d rt=%d rs_data=%h rt_data=%h",
                 $time, id_opcode, id_rd, id_rs, id_rt, id_rs_data, id_rt_data);
        $display("%0t ID: ctrl reg_write=%b mem_read=%b mem_write=%b branch=%b jump=%b",
                 $time, id_reg_write, id_mem_read, id_mem_write, id_branch, id_jump);
    end
end
```

## Tasks

1. [ ] Instantiate InstructionDecoder with IF/ID instruction
2. [ ] Instantiate ControlUnit with decoded opcode
3. [ ] Instantiate Regbank with decoded register addresses
4. [ ] Implement immediate sign/zero extension
5. [ ] Wire decoded signals to ID/EX pipeline register
6. [ ] Add debug `$display` statements
7. [ ] Test with load_immediate (no register reads needed)

## Definition of Done

- Instructions correctly decoded
- Control signals correctly generated
- Register values read combinatorially
- Immediate values properly extended
- All signals passed to ID/EX register
- Compiles without errors

## Dependencies

- Story 1: Project Setup
- Story 2: Pipeline Registers
- Story 3: IF Stage

## Notes

- For the tests, make sure to use the project Makefile to run the test. Inspect the Makefile to understand how the test framework works.
- There are two testbenches: cpu_tb.v for `make sim-cpu` and cpu_tests_tb.v that is used with the `make test-cpu` commands.
- Register read is combinational - data available same cycle
- Need to handle forwarding in later story (registers may have stale data)
- ControlUnit may need updates for new pipeline - check opcode coverage

## Review Checklist

- [ ] All instruction fields extracted
- [ ] All control signals generated
- [ ] Both register read ports used
- [ ] Sign extension correct for signed ops
- [ ] Zero extension for unsigned ops
- [ ] Proper handling of invalid/NOP instructions
