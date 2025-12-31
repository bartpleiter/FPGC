# Pipeline Registers

The B32P3 CPU uses pipeline registers between each stage to hold instruction data and control signals. Unlike some designs that use a separate module for pipeline registers, B32P3 implements them directly as `reg` declarations within the main CPU module.

## Pipeline Register Sets

### IF/ID Register

Captures instruction fetch results:

```verilog
reg [31:0] if_id_pc = 32'd0;      // Program counter of instruction
reg [31:0] if_id_instr = 32'd0;   // Fetched instruction
reg        if_id_valid = 1'b0;    // Instruction validity flag
```

### ID/EX Register

Captures decoded instruction fields and control signals:

```verilog
// Instruction tracking
reg [31:0] id_ex_pc = 32'd0;
reg [31:0] id_ex_instr = 32'd0;
reg        id_ex_valid = 1'b0;

// Decoded fields
reg [3:0]  id_ex_dreg = 4'd0;
reg [3:0]  id_ex_areg = 4'd0;
reg [3:0]  id_ex_breg = 4'd0;
reg [3:0]  id_ex_instr_op = 4'd0;
reg [3:0]  id_ex_alu_op = 4'd0;
reg [2:0]  id_ex_branch_op = 3'd0;
reg [31:0] id_ex_const_alu = 32'd0;
reg [26:0] id_ex_const27 = 27'd0;
reg [31:0] id_ex_const16 = 32'd0;
reg        id_ex_he = 1'b0;
reg        id_ex_oe = 1'b0;
reg        id_ex_sig = 1'b0;

// Control signals
reg        id_ex_alu_use_const = 1'b0;
reg        id_ex_dreg_we = 1'b0;
reg        id_ex_mem_read = 1'b0;
reg        id_ex_mem_write = 1'b0;
reg        id_ex_is_branch = 1'b0;
reg        id_ex_is_jump = 1'b0;
reg        id_ex_is_jumpr = 1'b0;
reg        id_ex_push = 1'b0;
reg        id_ex_pop = 1'b0;
reg        id_ex_halt = 1'b0;
reg        id_ex_reti = 1'b0;
reg        id_ex_getIntID = 1'b0;
reg        id_ex_getPC = 1'b0;
reg        id_ex_clearCache = 1'b0;
reg        id_ex_arithm = 1'b0;
```

### EX/MEM Register

Captures execution results and memory operation parameters:

```verilog
reg [31:0] ex_mem_pc = 32'd0;
reg [31:0] ex_mem_instr = 32'd0;
reg        ex_mem_valid = 1'b0;

// Results
reg [31:0] ex_mem_alu_result = 32'd0;
reg [31:0] ex_mem_breg_data = 32'd0;   // For store operations
reg [31:0] ex_mem_mem_addr = 32'd0;    // Calculated memory address

// Control signals
reg [3:0]  ex_mem_dreg = 4'd0;
reg        ex_mem_dreg_we = 1'b0;
reg        ex_mem_mem_read = 1'b0;
reg        ex_mem_mem_write = 1'b0;
reg        ex_mem_push = 1'b0;
reg        ex_mem_pop = 1'b0;
// ... branch/jump signals for MEM-stage resolution
```

### MEM/WB Register

Captures final results for writeback:

```verilog
reg [31:0] mem_wb_pc = 32'd0;
reg [31:0] mem_wb_instr = 32'd0;
reg        mem_wb_valid = 1'b0;

// Results
reg [31:0] mem_wb_alu_result = 32'd0;
reg [31:0] mem_wb_mem_data = 32'd0;
reg [31:0] mem_wb_stack_data = 32'd0;
reg [31:0] mem_wb_result = 32'd0;      // Pre-selected result

// Control signals
reg [3:0]  mem_wb_dreg = 4'd0;
reg        mem_wb_dreg_we = 1'b0;
reg        mem_wb_mem_read = 1'b0;
reg        mem_wb_pop = 1'b0;
```

## Pipeline Register Updates

### Normal Operation

On each clock edge, data flows from one stage to the next:

```verilog
always @(posedge clk) begin
    if (!stall_id) begin
        id_ex_pc <= if_id_pc;
        id_ex_instr <= if_id_instr;
        id_ex_valid <= if_id_valid;
        // ... capture all decoded fields
    end
end
```

### Flush (Clear)

When a flush signal is asserted, the register is cleared to create a pipeline bubble:

```verilog
if (flush_id_ex) begin
    id_ex_valid <= 1'b0;
    id_ex_dreg_we <= 1'b0;
    id_ex_mem_read <= 1'b0;
    id_ex_mem_write <= 1'b0;
    // ... clear all control signals
end
```

Flush conditions include:

- `pc_redirect`: Branch/jump taken
- `interrupt_valid`: Interrupt triggered
- `reti_valid`: Return from interrupt

### Stall (Hold)

When a stall signal is asserted, the register maintains its current value:

```verilog
if (stall_id) begin
    // Don't update - hold current values
end else begin
    // Normal update
end
```

Stall conditions include:

- `hazard_stall`: Load-use, pop-use, or cache line hazard
- `backend_stall`: Cache miss, multi-cycle ALU, or Memory Unit busy

## Stall Signal Propagation

Different pipeline stages use different stall signals:

```verilog
// Front-end stall (IF, ID)
wire pipeline_stall = hazard_stall || backend_stall;

// EX stall - includes cache_line_hazard
wire ex_pipeline_stall = backend_stall || cache_line_hazard;

// Back-end stall (MEM, WB)
wire backend_pipeline_stall = backend_stall;
```

## Valid Bit Handling

The `valid` bit tracks whether an instruction in the pipeline is real or a bubble:

- Set when a valid instruction enters from the previous stage
- Cleared on flush or when instruction completes
- Used to gate side effects (memory writes, register writes)
