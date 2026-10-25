// Instruction decode stage (ID)
// The IF/ID register, then decoding, reading the register file and the target adder.
// It decides where each value comes from once the instruction is in EX, and the
// instruction waits here while a value it reads isn't there in time (the late-result
// stall).
module cpu_id (
    input  wire        clk,
    input  wire        reset,
    input  wire        stall_id,
    input  wire        flush_id,

    // From IF
    input  wire [31:0] pc,
    input  wire [31:0] if_instr,

    // The instructions in EX and MEM, for forwarding and the late-result stall
    input  wire        id_ex_valid,
    input  wire        id_ex_writes_dreg,
    input  wire [3:0]  id_ex_dreg,
    input  wire        id_ex_late,
    input  wire        ex_mem_valid,
    input  wire        ex_mem_writes_dreg,
    input  wire [3:0]  ex_mem_dreg,

    // The register write from WB
    input  wire        wb_writes_dreg,
    input  wire [3:0]  mem_wb_dreg,
    input  wire [31:0] mem_wb_value,

    // The IF/ID register
    output reg         if_id_valid,
    output reg  [31:0] if_id_pc,
    output reg  [31:0] if_id_instr,

    // To EX
    output wire [31:0] id_a,              // the value of areg
    output wire [31:0] id_b,              // the value of breg
    output wire [31:0] id_constant,
    output wire        id_alu_use_const,
    output wire [3:0]  id_alu_op,
    output wire        id_writes_dreg,
    output wire [3:0]  id_dreg,
    output wire        id_late,
    output wire [2:0]  id_branch_op,
    output wire        id_branch_signed,
    output wire [31:0] id_target,         // of a branch, jump or halt
    output wire        id_branch,
    output wire        id_jump,
    output wire        id_jumpr,
    output wire        id_halt,
    output wire        id_read,
    output wire        id_write,
    output wire        id_byte,           // read or write size, a word if neither byte nor half
    output wire        id_half,
    output wire        id_sign_extend,    // readb, readh
    output wire        id_areg_from_mem,
    output wire        id_areg_from_wb,
    output wire        id_breg_from_mem,
    output wire        id_breg_from_wb,
    output wire        busy_id
);

always @(posedge clk) begin
    if (reset || flush_id)
        if_id_valid <= 1'b0;
    else if (!stall_id)
        if_id_valid <= 1'b1;  // IF always has an instruction
    if (!stall_id) begin
        if_id_pc    <= pc;
        if_id_instr <= if_instr;
    end
end

// Decode

wire [3:0]  areg;
wire [3:0]  breg;
wire [31:0] constant;
wire        const_add_pc;
wire        jump_absolute;
wire        reads_areg;
wire        reads_breg;

// The empty outputs are for instructions that come in later steps.
/* verilator lint_off PINCONNECTEMPTY */
cpu_decoder decoder (
    .instr(if_id_instr),
    .areg(areg),
    .breg(breg),
    .dreg(id_dreg),
    .alu_op(id_alu_op),
    .branch_op(id_branch_op),
    .branch_signed(id_branch_signed),
    .constant(constant),
    .alu_use_const(id_alu_use_const),
    .const_add_pc(const_add_pc),
    .jump_absolute(jump_absolute),
    .reads_areg(reads_areg),
    .reads_breg(reads_breg),
    .writes_dreg(id_writes_dreg),
    .late(id_late),
    .mem_byte(id_byte),
    .mem_half(id_half),
    .read_sign_extend(id_sign_extend),
    .ccache_data_only(),
    .arith(),
    .mult(),
    .div(),
    .read(id_read),
    .write(id_write),
    .push(),
    .pop(),
    .branch(id_branch),
    .jump(id_jump),
    .jumpr(id_jumpr),
    .halt(id_halt),
    .reti(),
    .ccache(),
    .intid(),
    .illegal()
);
/* verilator lint_on PINCONNECTEMPTY */

// The target adder. It also makes the constant of savpc (the PC) and jumpro (PC + c16),
// to which EX then adds the register.

wire [31:0] pc_plus_constant = if_id_pc + constant;

assign id_target   = jump_absolute ? constant : pc_plus_constant;
assign id_constant = const_add_pc ? pc_plus_constant : constant;

// Read the registers

cpu_regfile regfile (
    .clk(clk),
    .reset(reset),
    .areg(areg),
    .breg(breg),
    .areg_value(id_a),
    .breg_value(id_b),
    .writes_dreg(wb_writes_dreg),
    .dreg(mem_wb_dreg),
    .dreg_value(mem_wb_value)
);

// Forwarding and the late-result stall

wire late_result_stall;

cpu_forward forward (
    .areg(areg),
    .breg(breg),
    .reads_areg(reads_areg),
    .reads_breg(reads_breg),
    .ex_writes_dreg(id_ex_valid && id_ex_writes_dreg),
    .ex_dreg(id_ex_dreg),
    .ex_late(id_ex_late),
    .mem_writes_dreg(ex_mem_valid && ex_mem_writes_dreg),
    .mem_dreg(ex_mem_dreg),
    .areg_from_mem(id_areg_from_mem),
    .areg_from_wb(id_areg_from_wb),
    .breg_from_mem(id_breg_from_mem),
    .breg_from_wb(id_breg_from_wb),
    .late_result_stall(late_result_stall)
);

assign busy_id = if_id_valid && late_result_stall;

endmodule
