// Execute stage (EX)
// The ID/EX register, then the forwarding mux, the ALU, the address adder and the
// branch compare, which decide whether the instruction redirects, and where to.
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
    input  wire        id_areg_from_mem,
    input  wire        id_areg_from_wb,
    input  wire        id_breg_from_mem,
    input  wire        id_breg_from_wb,

    // The results to forward from MEM and WB
    input  wire [31:0] ex_mem_result,
    input  wire [31:0] mem_wb_value,

    // The ID/EX register, as far as other stages need it
    output reg         id_ex_valid,
    output reg  [31:0] id_ex_pc,
    output reg  [31:0] id_ex_instr,
    output reg         id_ex_writes_dreg,
    output reg  [3:0]  id_ex_dreg,
    output reg         id_ex_late,

    // To MEM
    output wire [31:0] ex_result,
    output wire        ex_redirect,
    output wire [31:0] ex_target,
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
wire [31:0] ex_b = id_ex_breg_from_mem ? ex_mem_result :
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

cpu_alu alu (
    .op(id_ex_alu_op),
    .a(ex_a),
    .b(id_ex_alu_use_const ? id_ex_constant : ex_b),
    .y(ex_result)
);

// The address adder, separate from the ALU. For JUMPR it gives the target.
wire [31:0] ex_addr = ex_a + id_ex_constant;

wire ex_taken;

cpu_branch branch (
    .a(ex_a),
    .b(ex_b),
    .branch_op(id_ex_branch_op),
    .branch_signed(id_ex_branch_signed),
    .taken(ex_taken)
);

// A jump, a taken branch and HALT redirect. HALT's target is its own PC.
assign ex_redirect = id_ex_jump || id_ex_jumpr || id_ex_halt || (id_ex_branch && ex_taken);
assign ex_target   = id_ex_jumpr ? ex_addr : id_ex_target;

assign busy_ex = 1'b0;  // nothing in EX takes more than one cycle yet

endmodule
