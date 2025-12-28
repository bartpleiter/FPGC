# Story 02: Pipeline Registers and Basic Flow

**Sprint**: 1  
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

## Description

As a developer, I want to implement the four pipeline registers (IF/ID, ID/EX, EX/MEM, MEM/WB) so that instructions can flow through the pipeline stages.

## Acceptance Criteria

1. [ ] IF/ID register captures instruction and PC on clock edge
2. [ ] ID/EX register captures decoded signals and register values
3. [ ] EX/MEM register captures ALU result and memory control
4. [ ] MEM/WB register captures memory data and writeback control
5. [ ] Pipeline flush capability for control hazards
6. [ ] Pipeline stall capability for data hazards

## Technical Details

### Pipeline Register Contents

```verilog
// =============================================================================
// IF/ID Pipeline Register
// =============================================================================
reg [31:0] if_id_pc;           // PC of this instruction
reg [31:0] if_id_pc_next;      // PC+1 for relative addressing
reg [31:0] if_id_instr;        // Raw instruction word
reg        if_id_valid;        // Is there a valid instruction?

// =============================================================================
// ID/EX Pipeline Register
// =============================================================================
reg [31:0] id_ex_pc;           // PC for branch calculation
reg [31:0] id_ex_pc_next;      // PC+1
reg [31:0] id_ex_instr;        // Instruction (for debugging)
reg        id_ex_valid;        // Valid instruction flag

// Register file outputs
reg [31:0] id_ex_rs_data;      // Source register 1 value
reg [31:0] id_ex_rt_data;      // Source register 2 value

// Decoded fields
reg [3:0]  id_ex_rd;           // Destination register address
reg [3:0]  id_ex_rs;           // Source register 1 address
reg [3:0]  id_ex_rt;           // Source register 2 address
reg [15:0] id_ex_imm;          // Immediate value (for I-type)
reg [3:0]  id_ex_areg;         // Address register (for load/store)
reg [3:0]  id_ex_opcode;       // Operation code

// Control signals
reg        id_ex_reg_write;    // Will write to register file
reg        id_ex_mem_read;     // Memory read operation
reg        id_ex_mem_write;    // Memory write operation
reg        id_ex_branch;       // Is this a branch?
reg        id_ex_jump;         // Is this a jump?
reg [3:0]  id_ex_alu_op;       // ALU operation selector
reg        id_ex_alu_src;      // ALU source: register or immediate
reg        id_ex_stack_push;   // Push to stack
reg        id_ex_stack_pop;    // Pop from stack
reg        id_ex_multi_cycle;  // Multi-cycle operation (div/mult)

// =============================================================================
// EX/MEM Pipeline Register
// =============================================================================
reg [31:0] ex_mem_pc;          // PC (for debugging)
reg [31:0] ex_mem_instr;       // Instruction (for debugging)
reg        ex_mem_valid;       // Valid flag

// ALU/EX results
reg [31:0] ex_mem_alu_result;  // ALU computation result
reg [31:0] ex_mem_rt_data;     // Data for memory store
reg        ex_mem_branch_taken;// Branch was taken
reg [31:0] ex_mem_branch_addr; // Target address if branch taken

// Decoded fields (forwarded)
reg [3:0]  ex_mem_rd;          // Destination register
reg [3:0]  ex_mem_opcode;      // Opcode (for debugging)

// Control signals
reg        ex_mem_reg_write;   // Will write to register file
reg        ex_mem_mem_read;    // Memory read operation
reg        ex_mem_mem_write;   // Memory write operation
reg        ex_mem_stack_push;  // Stack push
reg        ex_mem_stack_pop;   // Stack pop

// =============================================================================
// MEM/WB Pipeline Register
// =============================================================================
reg [31:0] mem_wb_pc;          // PC (for debugging)
reg [31:0] mem_wb_instr;       // Instruction (for debugging)
reg        mem_wb_valid;       // Valid flag

// Results
reg [31:0] mem_wb_alu_result;  // ALU result (if not memory op)
reg [31:0] mem_wb_mem_data;    // Data from memory read
reg [31:0] mem_wb_stack_data;  // Data from stack pop

// Decoded fields
reg [3:0]  mem_wb_rd;          // Destination register

// Control signals
reg        mem_wb_reg_write;   // Will write to register file
reg        mem_wb_mem_to_reg;  // Write memory data to register
reg        mem_wb_stack_to_reg;// Write stack data to register
```

### Pipeline Control Signals

```verilog
// Hazard control
wire stall_if;           // Stall IF stage
wire stall_id;           // Stall ID stage
wire flush_if_id;        // Flush IF/ID register
wire flush_id_ex;        // Flush ID/EX register

// Pipeline advance control
wire if_id_write;        // Enable IF/ID register write
wire id_ex_write;        // Enable ID/EX register write
wire ex_mem_write;       // Enable EX/MEM register write
wire mem_wb_write;       // Enable MEM/WB register write
```

### Pipeline Register Update Logic

```verilog
// IF/ID Pipeline Register
always @(posedge clk) begin
    if (reset || flush_if_id) begin
        if_id_valid <= 1'b0;
        if_id_pc <= 32'b0;
        if_id_pc_next <= 32'b0;
        if_id_instr <= 32'b0;  // NOP
    end else if (if_id_write && !stall_id) begin
        if_id_valid <= 1'b1;
        if_id_pc <= if_pc;
        if_id_pc_next <= if_pc + 1;
        if_id_instr <= if_instr;
    end
    // On stall: hold current values
end

// Similar for other pipeline registers...
```

### Debug Output

```verilog
// Pipeline state debug
always @(posedge clk) begin
    if (!reset) begin
        $display("%0t PIPE IF:  pc=%h instr=%h valid=%b", 
                 $time, if_pc, if_instr, 1'b1);
        $display("%0t PIPE ID:  pc=%h instr=%h valid=%b", 
                 $time, if_id_pc, if_id_instr, if_id_valid);
        $display("%0t PIPE EX:  pc=%h instr=%h valid=%b rd=%d alu=%h", 
                 $time, id_ex_pc, id_ex_instr, id_ex_valid, id_ex_rd, ex_alu_result);
        $display("%0t PIPE MEM: pc=%h instr=%h valid=%b rd=%d result=%h", 
                 $time, ex_mem_pc, ex_mem_instr, ex_mem_valid, ex_mem_rd, ex_mem_alu_result);
        $display("%0t PIPE WB:  pc=%h instr=%h valid=%b rd=%d result=%h", 
                 $time, mem_wb_pc, mem_wb_instr, mem_wb_valid, mem_wb_rd, wb_result);
    end
end
```

## Tasks

1. [ ] Define all pipeline register fields
2. [ ] Implement IF/ID register with reset/flush/stall
3. [ ] Implement ID/EX register with reset/flush/stall
4. [ ] Implement EX/MEM register with reset/flush
5. [ ] Implement MEM/WB register with reset
6. [ ] Add pipeline control wire declarations
7. [ ] Add debug `$display` for each stage
8. [ ] Test compilation

## Definition of Done

- All four pipeline registers implemented
- Reset clears all registers
- Flush clears appropriate registers
- Stall holds values in appropriate registers
- Debug output shows pipeline state
- Compiles without errors

## Dependencies

- Story 1: Project Setup and Module Skeleton

## Notes

- Valid bit is crucial - NOP has valid=0
- Flush and stall must be coordinated (Story 9 will implement logic)
- Keep debug output conditional (can be disabled for synthesis)
- FPGC is word addressable, so PC increments by 1 per instruction

## Review Checklist

- [ ] All required fields present in each register
- [ ] Reset logic correct (all zeros/invalid)
- [ ] Flush logic correct (inserts bubble)
- [ ] Stall logic correct (holds values)
- [ ] Debug output helpful but not excessive
