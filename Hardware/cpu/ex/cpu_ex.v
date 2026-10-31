// Execute stage (EX)
// The ID/EX register, then the forwarding mux, the ALU, the address adder and the
// branch compare, which decide whether the instruction redirects, and where to, and
// whether an interrupt is taken instead. The address adder also gives the address of a
// read or write, and its region. A multiply starts here and finishes in MEM, and a
// divide waits here until it's done.
module cpu_ex (
    input  wire        clk,
    input  wire        reset,
    input  wire        stall_ex,
    input  wire        flush_ex,

    // From ID
    input  wire        if_id_valid,
    input  wire [31:0] if_id_pc,
    input  wire [31:0] if_id_instr,
    input  wire [31:0] id_a,
    input  wire [31:0] id_b,
    input  wire [31:0] id_constant,
    input  wire        id_alu_use_const,
    input  wire [3:0]  id_alu_op,
    input  wire        id_writes_dreg,
    input  wire [3:0]  id_dreg,
    input  wire        id_late,
    input  wire [2:0]  id_branch_op,
    input  wire        id_branch_signed,
    input  wire [31:0] id_target,
    input  wire        id_branch,
    input  wire        id_jump,
    input  wire        id_jumpr,
    input  wire        id_halt,
    input  wire        id_reti,
    input  wire        id_mult,
    input  wire        id_div,
    input  wire        id_read,
    input  wire        id_write,
    input  wire        id_byte,
    input  wire        id_half,
    input  wire        id_sign_extend,
    input  wire        id_push,
    input  wire        id_pop,
    input  wire        id_intid,
    input  wire        id_illegal,
    input  wire        id_areg_from_mem,
    input  wire        id_areg_from_wb,
    input  wire        id_breg_from_mem,
    input  wire        id_breg_from_wb,

    // The results to forward from MEM and WB
    input  wire [31:0] ex_mem_result,
    input  wire [31:0] mem_wb_value,

    // A write in MEM, which uses the data memory in this cycle, and a push in MEM,
    // which writes the word a pop would read
    input  wire        ex_mem_valid,
    input  wire        ex_mem_write,
    input  wire        ex_mem_push,

    // The interrupt state, from MEM
    input  wire        interrupt_pending,
    input  wire        interrupts_enabled,

    // The ID/EX register, as far as other stages need it
    output reg         id_ex_valid,
    output reg  [31:0] id_ex_pc,
    output reg  [31:0] id_ex_instr,
    output reg         id_ex_writes_dreg,
    output reg  [3:0]  id_ex_dreg,
    output reg         id_ex_late,
    output reg         id_ex_read,
    output reg         id_ex_write,
    output reg         id_ex_byte,
    output reg         id_ex_half,
    output reg         id_ex_sign_extend,
    output reg         id_ex_push,
    output reg         id_ex_pop,
    output reg         id_ex_intid,
    output reg         id_ex_reti,
    output reg         id_ex_illegal,
    output reg         id_ex_mult,
    output reg         id_ex_div,

    // To MEM
    output wire [31:0] ex_result,
    output wire [31:0] ex_b,           // the data of a write
    output wire [31:0] ex_addr,        // also the address of the data memory
    output wire        ex_sdram,       // the region of the address
    output wire        ex_rom,
    output wire        ex_cpu_io,
    output wire        ex_unmapped,
    output wire        ex_redirect,    // a jump, a taken branch or HALT
    output wire [31:0] ex_target,
    output wire        ex_interrupt,   // an interrupt is taken instead
    output wire [31:0] ex_mult_result, // of the multiply in MEM
    output wire [31:0] ex_div_result,  // once the divider is done
    output wire        busy_ex
);

reg [31:0] id_ex_a;
reg [31:0] id_ex_b;
reg [31:0] id_ex_constant;
reg        id_ex_alu_use_const;
reg [3:0]  id_ex_alu_op;
reg [2:0]  id_ex_branch_op;
reg        id_ex_branch_signed;
reg [31:0] id_ex_target;
reg        id_ex_branch;
reg        id_ex_jump;
reg        id_ex_jumpr;
reg        id_ex_halt;
reg        id_ex_areg_from_mem;
reg        id_ex_areg_from_wb;
reg        id_ex_breg_from_mem;
reg        id_ex_breg_from_wb;

// Forwarding mux. Selects operands from MEM, WB, or the register file
wire [31:0] ex_a = id_ex_areg_from_mem ? ex_mem_result :
                   id_ex_areg_from_wb  ? mem_wb_value  :
                                         id_ex_a;
assign ex_b      = id_ex_breg_from_mem ? ex_mem_result :
                   id_ex_breg_from_wb  ? mem_wb_value  :
                                         id_ex_b;

// A stalled EX keeps its operands, but MEM and WB may move on meanwhile, taking the
// forwarded values with them. So a stalled EX captures its operands, and from then on
// uses those.
always @(posedge clk) begin
    if (reset || flush_ex)
        id_ex_valid <= 1'b0;
    else if (!stall_ex)
        id_ex_valid <= if_id_valid;
    if (!stall_ex) begin
        id_ex_pc            <= if_id_pc;
        id_ex_instr         <= if_id_instr;
        id_ex_a             <= id_a;
        id_ex_b             <= id_b;
        id_ex_constant      <= id_constant;
        id_ex_alu_use_const <= id_alu_use_const;
        id_ex_alu_op        <= id_alu_op;
        id_ex_writes_dreg   <= id_writes_dreg;
        id_ex_dreg          <= id_dreg;
        id_ex_late          <= id_late;
        id_ex_branch_op     <= id_branch_op;
        id_ex_branch_signed <= id_branch_signed;
        id_ex_target        <= id_target;
        id_ex_branch        <= id_branch;
        id_ex_jump          <= id_jump;
        id_ex_jumpr         <= id_jumpr;
        id_ex_halt          <= id_halt;
        id_ex_reti          <= id_reti;
        id_ex_read          <= id_read;
        id_ex_write         <= id_write;
        id_ex_byte          <= id_byte;
        id_ex_half          <= id_half;
        id_ex_sign_extend   <= id_sign_extend;
        id_ex_push          <= id_push;
        id_ex_pop           <= id_pop;
        id_ex_intid         <= id_intid;
        id_ex_illegal       <= id_illegal;
        id_ex_mult          <= id_mult;
        id_ex_div           <= id_div;
        id_ex_areg_from_mem <= id_areg_from_mem;
        id_ex_areg_from_wb  <= id_areg_from_wb;
        id_ex_breg_from_mem <= id_breg_from_mem;
        id_ex_breg_from_wb  <= id_breg_from_wb;
    end else begin
        id_ex_a             <= ex_a;
        id_ex_b             <= ex_b;
        id_ex_areg_from_mem <= 1'b0;
        id_ex_areg_from_wb  <= 1'b0;
        id_ex_breg_from_mem <= 1'b0;
        id_ex_breg_from_wb  <= 1'b0;
    end
end

wire [31:0] alu_b = id_ex_alu_use_const ? id_ex_constant : ex_b;

cpu_alu alu (
    .op(id_ex_alu_op),
    .a(ex_a),
    .b(alu_b),
    .y(ex_result)
);

// The multiplier takes the operands of every instruction when it moves on to MEM, and
// has the result when the instruction is in MEM. Only a multiply uses it.
cpu_multiplier multiplier (
    .clk(clk),
    .enable(!stall_ex),
    .op(id_ex_alu_op),
    .a(ex_a),
    .b(alu_b),
    .y(ex_mult_result)
);

// A divide starts the divider in its first cycle in EX, and waits in EX until the
// divider is done. The flag makes sure it starts only once, also when EX has to wait
// for a later stage.
reg  div_started;
wire div_start = id_ex_valid && id_ex_div && !div_started;
wire div_busy;

always @(posedge clk) begin
    if (reset || !stall_ex)
        div_started <= 1'b0;
    else if (div_start)
        div_started <= 1'b1;
end

cpu_divider divider (
    .clk(clk),
    .start(div_start),
    .op(id_ex_alu_op),
    .a(ex_a),
    .b(alu_b),
    .busy(div_busy),
    .y(ex_div_result)
);

// The address adder, separate from the ALU. It gives the address of a read or write,
// and the target of a JUMPR.
assign ex_addr = ex_a + id_ex_constant;

// The empty outputs are for the regions that come in later steps.
/* verilator lint_off PINCONNECTEMPTY */
cpu_memory_map memory_map (
    .addr(ex_addr),
    .sdram(ex_sdram),
    .io(),
    .rom(ex_rom),
    .vram32(),
    .vram8(),
    .vrampx(),
    .palette(),
    .cpu_io(ex_cpu_io),
    .code(),
    .unmapped(ex_unmapped)
);
/* verilator lint_on PINCONNECTEMPTY */

wire ex_taken;

cpu_branch branch (
    .a(ex_a),
    .b(ex_b),
    .branch_op(id_ex_branch_op),
    .branch_signed(id_ex_branch_signed),
    .taken(ex_taken)
);

// A jump, a taken branch and HALT redirect. HALT's target is its own PC. (A RETI
// redirects too, but its target is only read in MEM.)
assign ex_redirect = id_ex_jump || id_ex_jumpr || id_ex_halt || (id_ex_branch && ex_taken);
assign ex_target   = id_ex_jumpr ? ex_addr : id_ex_target;

// An interrupt is taken instead of an instruction that redirects, when one is pending,
// interrupts are enabled, and the instruction is in SDRAM, so code in ROM is never
// interrupted. The decision goes to MEM with the instruction. Until the instruction
// commits, only an interrupt or a RETI can change whether interrupts are enabled, and
// both redirect, which cancels this instruction. A pending interrupt stays pending until
// it's taken, so the decision still holds when the instruction commits.
wire pc_in_sdram;

/* verilator lint_off PINCONNECTEMPTY */
cpu_memory_map pc_map (
    .addr(id_ex_pc),
    .sdram(pc_in_sdram),
    .io(),
    .rom(),
    .vram32(),
    .vram8(),
    .vrampx(),
    .palette(),
    .cpu_io(),
    .code(),
    .unmapped()
);
/* verilator lint_on PINCONNECTEMPTY */

assign ex_interrupt = ex_redirect && interrupt_pending && interrupts_enabled && pc_in_sdram;

// A write in MEM uses the address of the data memory, so a read or write in EX has to
// wait a cycle (the write-then-access stall). A read that waits like this also reads
// what the write wrote. A pop in EX waits for a push in MEM in the same way (the
// push-then-pop stall), because the stack can't read the word that is being written.
wire write_then_access = (id_ex_read || id_ex_write) && ex_mem_valid && ex_mem_write;
wire push_then_pop     = id_ex_pop && ex_mem_valid && ex_mem_push;

assign busy_ex = id_ex_valid &&
                 (write_then_access || push_then_pop || (id_ex_div && div_busy));

endmodule
