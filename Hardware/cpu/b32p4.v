// B32P4
// Top-level design of the CPU. It connects the five pipeline stages and the hazard unit,
// and has no logic of its own.
module b32p4 (
    input  wire        clk,
    input  wire        reset,

    // The instruction memory registers fetch_addr at the clock edge when fetch_en is 1,
    // and fetch_word is the word at that address from then on.
    output wire [31:0] fetch_addr,
    output wire        fetch_en,
    input  wire [31:0] fetch_word,

    // The data memory registers data_addr at every clock edge, and sdram_word is the
    // word at that address from then on. With sdram_write, it writes the bytes of
    // data_write_data that data_byte_enable selects at that edge.
    output wire [31:0] data_addr,
    output wire [31:0] data_write_data,
    output wire [3:0]  data_byte_enable,
    output wire        sdram_write,
    input  wire [31:0] sdram_word,

    // A debug port for observing finished (retired) instructions, currently used in simulation only
    output wire        retire_valid,
    output wire [31:0] retire_pc,
    output wire [31:0] retire_instr,
    output wire        retire_writes_dreg,
    output wire [3:0]  retire_dreg,
    output wire [31:0] retire_dreg_value,
    output wire        retire_write,
    output wire [31:0] retire_write_addr,
    output wire [31:0] retire_write_data,
    output wire [3:0]  retire_write_be,
    output wire        retire_push,
    output wire [31:0] retire_push_value,
    output wire        retire_pop,
    output wire [31:0] retire_pop_value,
    output wire        retire_redirect,
    output wire [31:0] retire_target,
    output wire        retire_irq,
    output wire [2:0]  retire_irq_id
);

// Stall and flush

wire busy_if, busy_id, busy_ex, busy_mem;
wire redirect;
wire stall_if, stall_id, stall_ex, stall_mem;
wire flush_id, flush_ex, flush_mem, flush_wb;

cpu_hazard hazard (
    .busy_if(busy_if),
    .busy_id(busy_id),
    .busy_ex(busy_ex),
    .busy_mem(busy_mem),
    .redirect(redirect),
    .stall_if(stall_if),
    .stall_id(stall_id),
    .flush_id(flush_id),
    .stall_ex(stall_ex),
    .flush_ex(flush_ex),
    .stall_mem(stall_mem),
    .flush_mem(flush_mem),
    .flush_wb(flush_wb)
);

// IF

wire [31:0] redirect_target;
wire [31:0] pc;
wire [31:0] if_instr;

cpu_if if_stage (
    .clk(clk),
    .reset(reset),
    .stall_if(stall_if),
    .redirect(redirect),
    .redirect_target(redirect_target),
    .fetch_addr(fetch_addr),
    .fetch_en(fetch_en),
    .fetch_word(fetch_word),
    .pc(pc),
    .if_instr(if_instr),
    .busy_if(busy_if)
);

// ID

wire        id_ex_valid;
wire        id_ex_writes_dreg;
wire [3:0]  id_ex_dreg;
wire        id_ex_late;
wire        ex_mem_valid;
wire        ex_mem_writes_dreg;
wire [3:0]  ex_mem_dreg;
wire        wb_writes_dreg;
wire [3:0]  mem_wb_dreg;
wire [31:0] mem_wb_value;

wire        if_id_valid;
wire [31:0] if_id_pc;
wire [31:0] if_id_instr;
wire [31:0] id_a;
wire [31:0] id_b;
wire [31:0] id_constant;
wire        id_alu_use_const;
wire [3:0]  id_alu_op;
wire        id_writes_dreg;
wire [3:0]  id_dreg;
wire        id_late;
wire [2:0]  id_branch_op;
wire        id_branch_signed;
wire [31:0] id_target;
wire        id_branch;
wire        id_jump;
wire        id_jumpr;
wire        id_halt;
wire        id_read;
wire        id_write;
wire        id_byte;
wire        id_half;
wire        id_sign_extend;
wire        id_push;
wire        id_pop;
wire        id_illegal;
wire        id_areg_from_mem;
wire        id_areg_from_wb;
wire        id_breg_from_mem;
wire        id_breg_from_wb;

cpu_id id_stage (
    .clk(clk),
    .reset(reset),
    .stall_id(stall_id),
    .flush_id(flush_id),
    .pc(pc),
    .if_instr(if_instr),
    .id_ex_valid(id_ex_valid),
    .id_ex_writes_dreg(id_ex_writes_dreg),
    .id_ex_dreg(id_ex_dreg),
    .id_ex_late(id_ex_late),
    .ex_mem_valid(ex_mem_valid),
    .ex_mem_writes_dreg(ex_mem_writes_dreg),
    .ex_mem_dreg(ex_mem_dreg),
    .wb_writes_dreg(wb_writes_dreg),
    .mem_wb_dreg(mem_wb_dreg),
    .mem_wb_value(mem_wb_value),
    .if_id_valid(if_id_valid),
    .if_id_pc(if_id_pc),
    .if_id_instr(if_id_instr),
    .id_a(id_a),
    .id_b(id_b),
    .id_constant(id_constant),
    .id_alu_use_const(id_alu_use_const),
    .id_alu_op(id_alu_op),
    .id_writes_dreg(id_writes_dreg),
    .id_dreg(id_dreg),
    .id_late(id_late),
    .id_branch_op(id_branch_op),
    .id_branch_signed(id_branch_signed),
    .id_target(id_target),
    .id_branch(id_branch),
    .id_jump(id_jump),
    .id_jumpr(id_jumpr),
    .id_halt(id_halt),
    .id_read(id_read),
    .id_write(id_write),
    .id_byte(id_byte),
    .id_half(id_half),
    .id_sign_extend(id_sign_extend),
    .id_push(id_push),
    .id_pop(id_pop),
    .id_illegal(id_illegal),
    .id_areg_from_mem(id_areg_from_mem),
    .id_areg_from_wb(id_areg_from_wb),
    .id_breg_from_mem(id_breg_from_mem),
    .id_breg_from_wb(id_breg_from_wb),
    .busy_id(busy_id)
);

// EX

wire [31:0] id_ex_pc;
wire [31:0] id_ex_instr;
wire        id_ex_read;
wire        id_ex_write;
wire        id_ex_byte;
wire        id_ex_half;
wire        id_ex_sign_extend;
wire        id_ex_push;
wire        id_ex_pop;
wire        id_ex_illegal;
wire [31:0] ex_mem_result;
wire        ex_mem_write;
wire        ex_mem_push;
wire [31:0] ex_mem_addr;
wire [31:0] ex_result;
wire [31:0] ex_b;
wire [31:0] ex_addr;
wire        ex_sdram;
wire        ex_rom;
wire        ex_cpu_io;
wire        ex_unmapped;
wire        ex_redirect;
wire [31:0] ex_target;

cpu_ex ex_stage (
    .clk(clk),
    .reset(reset),
    .stall_ex(stall_ex),
    .flush_ex(flush_ex),
    .if_id_valid(if_id_valid),
    .if_id_pc(if_id_pc),
    .if_id_instr(if_id_instr),
    .id_a(id_a),
    .id_b(id_b),
    .id_constant(id_constant),
    .id_alu_use_const(id_alu_use_const),
    .id_alu_op(id_alu_op),
    .id_writes_dreg(id_writes_dreg),
    .id_dreg(id_dreg),
    .id_late(id_late),
    .id_branch_op(id_branch_op),
    .id_branch_signed(id_branch_signed),
    .id_target(id_target),
    .id_branch(id_branch),
    .id_jump(id_jump),
    .id_jumpr(id_jumpr),
    .id_halt(id_halt),
    .id_read(id_read),
    .id_write(id_write),
    .id_byte(id_byte),
    .id_half(id_half),
    .id_sign_extend(id_sign_extend),
    .id_push(id_push),
    .id_pop(id_pop),
    .id_illegal(id_illegal),
    .id_areg_from_mem(id_areg_from_mem),
    .id_areg_from_wb(id_areg_from_wb),
    .id_breg_from_mem(id_breg_from_mem),
    .id_breg_from_wb(id_breg_from_wb),
    .ex_mem_result(ex_mem_result),
    .mem_wb_value(mem_wb_value),
    .ex_mem_valid(ex_mem_valid),
    .ex_mem_write(ex_mem_write),
    .ex_mem_push(ex_mem_push),
    .id_ex_valid(id_ex_valid),
    .id_ex_pc(id_ex_pc),
    .id_ex_instr(id_ex_instr),
    .id_ex_writes_dreg(id_ex_writes_dreg),
    .id_ex_dreg(id_ex_dreg),
    .id_ex_late(id_ex_late),
    .id_ex_read(id_ex_read),
    .id_ex_write(id_ex_write),
    .id_ex_byte(id_ex_byte),
    .id_ex_half(id_ex_half),
    .id_ex_sign_extend(id_ex_sign_extend),
    .id_ex_push(id_ex_push),
    .id_ex_pop(id_ex_pop),
    .id_ex_illegal(id_ex_illegal),
    .ex_result(ex_result),
    .ex_b(ex_b),
    .ex_addr(ex_addr),
    .ex_sdram(ex_sdram),
    .ex_rom(ex_rom),
    .ex_cpu_io(ex_cpu_io),
    .ex_unmapped(ex_unmapped),
    .ex_redirect(ex_redirect),
    .ex_target(ex_target),
    .busy_ex(busy_ex)
);

// MEM

wire [31:0] ex_mem_pc;
wire [31:0] ex_mem_instr;
wire [31:0] ex_mem_b;
wire        ex_mem_pop;
wire [31:0] mem_value;

cpu_mem mem_stage (
    .clk(clk),
    .reset(reset),
    .stall_mem(stall_mem),
    .flush_mem(flush_mem),
    .id_ex_valid(id_ex_valid),
    .id_ex_pc(id_ex_pc),
    .id_ex_instr(id_ex_instr),
    .id_ex_writes_dreg(id_ex_writes_dreg),
    .id_ex_dreg(id_ex_dreg),
    .id_ex_read(id_ex_read),
    .id_ex_write(id_ex_write),
    .id_ex_byte(id_ex_byte),
    .id_ex_half(id_ex_half),
    .id_ex_sign_extend(id_ex_sign_extend),
    .id_ex_push(id_ex_push),
    .id_ex_pop(id_ex_pop),
    .id_ex_illegal(id_ex_illegal),
    .ex_result(ex_result),
    .ex_b(ex_b),
    .ex_addr(ex_addr),
    .ex_sdram(ex_sdram),
    .ex_rom(ex_rom),
    .ex_cpu_io(ex_cpu_io),
    .ex_unmapped(ex_unmapped),
    .ex_redirect(ex_redirect),
    .ex_target(ex_target),
    .data_addr(data_addr),
    .data_write_data(data_write_data),
    .data_byte_enable(data_byte_enable),
    .sdram_write(sdram_write),
    .sdram_word(sdram_word),
    .ex_mem_valid(ex_mem_valid),
    .ex_mem_pc(ex_mem_pc),
    .ex_mem_instr(ex_mem_instr),
    .ex_mem_writes_dreg(ex_mem_writes_dreg),
    .ex_mem_dreg(ex_mem_dreg),
    .ex_mem_result(ex_mem_result),
    .ex_mem_write(ex_mem_write),
    .ex_mem_addr(ex_mem_addr),
    .ex_mem_b(ex_mem_b),
    .ex_mem_push(ex_mem_push),
    .ex_mem_pop(ex_mem_pop),
    .mem_value(mem_value),
    .redirect(redirect),
    .redirect_target(redirect_target),
    .busy_mem(busy_mem)
);

// WB

cpu_wb wb_stage (
    .clk(clk),
    .reset(reset),
    .flush_wb(flush_wb),
    .ex_mem_valid(ex_mem_valid),
    .ex_mem_pc(ex_mem_pc),
    .ex_mem_instr(ex_mem_instr),
    .ex_mem_writes_dreg(ex_mem_writes_dreg),
    .ex_mem_dreg(ex_mem_dreg),
    .mem_value(mem_value),
    .ex_mem_write(ex_mem_write),
    .ex_mem_addr(ex_mem_addr),
    .data_write_data(data_write_data),
    .data_byte_enable(data_byte_enable),
    .ex_mem_push(ex_mem_push),
    .ex_mem_pop(ex_mem_pop),
    .ex_mem_b(ex_mem_b),
    .redirect(redirect),
    .redirect_target(redirect_target),
    .wb_writes_dreg(wb_writes_dreg),
    .mem_wb_dreg(mem_wb_dreg),
    .mem_wb_value(mem_wb_value),
    .retire_valid(retire_valid),
    .retire_pc(retire_pc),
    .retire_instr(retire_instr),
    .retire_writes_dreg(retire_writes_dreg),
    .retire_dreg(retire_dreg),
    .retire_dreg_value(retire_dreg_value),
    .retire_write(retire_write),
    .retire_write_addr(retire_write_addr),
    .retire_write_data(retire_write_data),
    .retire_write_be(retire_write_be),
    .retire_push(retire_push),
    .retire_push_value(retire_push_value),
    .retire_pop(retire_pop),
    .retire_pop_value(retire_pop_value),
    .retire_redirect(retire_redirect),
    .retire_target(retire_target),
    .retire_irq(retire_irq),
    .retire_irq_id(retire_irq_id)
);

endmodule
