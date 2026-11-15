// B32P4
// Top-level design of the CPU.
module b32p4 #(
    parameter INTERRUPTS = 7,    // the number of interrupt inputs, at most 255
    parameter L1I_LINES  = 128,  // the number of L1i cache lines, a power of two
    parameter L1D_LINES  = 512   // the number of L1d cache lines, a power of two
) (
    input  wire        clk,
    input  wire        reset,

    output wire [31:0] fetch_addr,
    output wire        fetch_en,
    input  wire [31:0] rom_word,

    output wire        sdram_req,
    output wire        sdram_write,
    output wire [25:5] sdram_line,
    output wire [2:0]  sdram_first,
    input  wire        sdram_take,
    input  wire        sdram_read_valid,
    input  wire [31:0] sdram_read_word,
    input  wire        sdram_next,
    output wire [31:0] sdram_write_word,

    output wire        bus_request,
    output wire        bus_write,
    output wire [31:0] bus_addr,
    output wire [31:0] bus_write_data,
    output wire [3:0]  bus_byte_enable,
    input  wire        bus_ready,
    input  wire [31:0] bus_read_word,

    input  wire [INTERRUPTS:1] interrupts,

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
    output wire [7:0]  retire_irq_id
);

// Stall and flush

wire busy_if, busy_id, busy_ex, busy_mem, l1d_miss, l1d_wait;
wire redirect;
wire stall_if, stall_id, stall_ex, stall_mem;
wire flush_id, flush_ex, flush_mem, flush_wb;
wire refetch_if;

cpu_hazard hazard (
    .busy_if(busy_if),
    .busy_id(busy_id),
    .busy_ex(busy_ex),
    .busy_mem(busy_mem),
    .l1d_miss(l1d_miss),
    .l1d_wait(l1d_wait),
    .redirect(redirect),
    .stall_if(stall_if),
    .stall_id(stall_id),
    .flush_id(flush_id),
    .stall_ex(stall_ex),
    .flush_ex(flush_ex),
    .stall_mem(stall_mem),
    .flush_mem(flush_mem),
    .flush_wb(flush_wb),
    .refetch_if(refetch_if)
);

// IF

wire [31:0] redirect_target;
wire [31:0] pc;
wire [31:0] if_instr;
wire [31:0] l1i_word;
wire [21-$clog2(L1I_LINES):0] l1i_entry;
wire        l1i_request;
wire        l1i_take;
wire        l1i_writing;
wire        l1i_filling;
wire [25:2] fill_addr;

cpu_if #(
    .L1I_LINES(L1I_LINES)
) if_stage (
    .clk(clk),
    .reset(reset),
    .stall_if(stall_if),
    .refetch_if(refetch_if),
    .redirect(redirect),
    .redirect_target(redirect_target),
    .fetch_addr(fetch_addr),
    .fetch_en(fetch_en),
    .rom_word(rom_word),
    .l1i_word(l1i_word),
    .l1i_entry(l1i_entry),
    .l1i_request(l1i_request),
    .l1i_take(l1i_take),
    .l1i_writing(l1i_writing),
    .l1i_filling(l1i_filling),
    .fill_addr(fill_addr),
    .sdram_read_valid(sdram_read_valid),
    .sdram_read_word(sdram_read_word),
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
wire        id_reti;
wire        id_ccache;
wire        id_ccached;
wire        id_mult;
wire        id_div;
wire        id_read;
wire        id_write;
wire        id_byte;
wire        id_half;
wire        id_sign_extend;
wire        id_push;
wire        id_pop;
wire        id_intid;
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
    .id_reti(id_reti),
    .id_ccache(id_ccache),
    .id_ccached(id_ccached),
    .id_mult(id_mult),
    .id_div(id_div),
    .id_read(id_read),
    .id_write(id_write),
    .id_byte(id_byte),
    .id_half(id_half),
    .id_sign_extend(id_sign_extend),
    .id_push(id_push),
    .id_pop(id_pop),
    .id_intid(id_intid),
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
wire        id_ex_intid;
wire        id_ex_reti;
wire        id_ex_ccache;
wire        id_ex_ccached;
wire        id_ex_illegal;
wire        id_ex_mult;
wire        id_ex_div;
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
wire        ex_interrupt;
wire [31:0] ex_mult_result;
wire [31:0] ex_div_result;
wire        interrupt_pending;
wire        interrupts_enabled;

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
    .id_reti(id_reti),
    .id_ccache(id_ccache),
    .id_ccached(id_ccached),
    .id_mult(id_mult),
    .id_div(id_div),
    .id_read(id_read),
    .id_write(id_write),
    .id_byte(id_byte),
    .id_half(id_half),
    .id_sign_extend(id_sign_extend),
    .id_push(id_push),
    .id_pop(id_pop),
    .id_intid(id_intid),
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
    .interrupt_pending(interrupt_pending),
    .interrupts_enabled(interrupts_enabled),
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
    .id_ex_intid(id_ex_intid),
    .id_ex_reti(id_ex_reti),
    .id_ex_ccache(id_ex_ccache),
    .id_ex_ccached(id_ex_ccached),
    .id_ex_illegal(id_ex_illegal),
    .id_ex_mult(id_ex_mult),
    .id_ex_div(id_ex_div),
    .ex_result(ex_result),
    .ex_b(ex_b),
    .ex_addr(ex_addr),
    .ex_sdram(ex_sdram),
    .ex_rom(ex_rom),
    .ex_cpu_io(ex_cpu_io),
    .ex_unmapped(ex_unmapped),
    .ex_redirect(ex_redirect),
    .ex_target(ex_target),
    .ex_interrupt(ex_interrupt),
    .ex_mult_result(ex_mult_result),
    .ex_div_result(ex_div_result),
    .busy_ex(busy_ex)
);

// MEM

wire [31:0] ex_mem_pc;
wire [31:0] ex_mem_instr;
wire [31:0] ex_mem_b;
wire        ex_mem_pop;
wire        ex_mem_interrupt;
wire [31:0] mem_value;
wire [7:0]  interrupt_id;
/* verilator lint_off UNUSEDSIGNAL */
wire [31:0] data_addr;
/* verilator lint_on UNUSEDSIGNAL */
wire [31:0] data_write_data;
wire [3:0]  data_byte_enable;
wire        l1d_write;
wire [22-$clog2(L1D_LINES):0] l1d_entry_write_value;
wire [31:0] l1d_word;
wire [22-$clog2(L1D_LINES):0] l1d_entry;
wire        l1d_request;
wire        victim_dirty;
wire [20-$clog2(L1D_LINES):0] victim_tag;
wire        l1d_take;
wire        l1d_writing;
wire        l1d_filling;
wire        clear_request;
wire        clear_data_only;
wire        clear_take;

cpu_mem #(
    .INTERRUPTS(INTERRUPTS),
    .L1D_LINES(L1D_LINES)
) mem_stage (
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
    .id_ex_intid(id_ex_intid),
    .id_ex_reti(id_ex_reti),
    .id_ex_ccache(id_ex_ccache),
    .id_ex_ccached(id_ex_ccached),
    .id_ex_illegal(id_ex_illegal),
    .id_ex_mult(id_ex_mult),
    .id_ex_div(id_ex_div),
    .ex_result(ex_result),
    .ex_b(ex_b),
    .ex_addr(ex_addr),
    .ex_sdram(ex_sdram),
    .ex_rom(ex_rom),
    .ex_cpu_io(ex_cpu_io),
    .ex_unmapped(ex_unmapped),
    .ex_redirect(ex_redirect),
    .ex_target(ex_target),
    .ex_interrupt(ex_interrupt),
    .ex_mult_result(ex_mult_result),
    .ex_div_result(ex_div_result),
    .data_addr(data_addr),
    .data_write_data(data_write_data),
    .data_byte_enable(data_byte_enable),
    .l1d_write(l1d_write),
    .l1d_entry_write_value(l1d_entry_write_value),
    .l1d_word(l1d_word),
    .l1d_entry(l1d_entry),
    .l1d_request(l1d_request),
    .victim_dirty(victim_dirty),
    .victim_tag(victim_tag),
    .l1d_take(l1d_take),
    .l1d_writing(l1d_writing),
    .l1d_filling(l1d_filling),
    .fill_addr(fill_addr),
    .sdram_read_valid(sdram_read_valid),
    .sdram_read_word(sdram_read_word),
    .clear_request(clear_request),
    .clear_data_only(clear_data_only),
    .clear_take(clear_take),
    .bus_request(bus_request),
    .bus_write(bus_write),
    .bus_addr(bus_addr),
    .bus_write_data(bus_write_data),
    .bus_byte_enable(bus_byte_enable),
    .bus_ready(bus_ready),
    .bus_read_word(bus_read_word),
    .interrupts(interrupts),
    .interrupt_pending(interrupt_pending),
    .interrupts_enabled(interrupts_enabled),
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
    .ex_mem_interrupt(ex_mem_interrupt),
    .mem_value(mem_value),
    .interrupt_id(interrupt_id),
    .redirect(redirect),
    .redirect_target(redirect_target),
    .busy_mem(busy_mem),
    .l1d_miss(l1d_miss),
    .l1d_wait(l1d_wait)
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
    .ex_mem_interrupt(ex_mem_interrupt),
    .interrupt_id(interrupt_id),
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

wire [31:0]                   b_write_data;
wire [$clog2(L1I_LINES)+4:2]  l1i_b_addr;
wire                          l1i_b_write;
wire                          l1i_b_entry_write;
wire [21-$clog2(L1I_LINES):0] l1i_b_entry_write_value;
wire [$clog2(L1D_LINES)+4:2]  l1d_b_addr;
wire [31:0]                   l1d_b_word;
wire [22-$clog2(L1D_LINES):0] l1d_b_entry;
wire                          l1d_b_write;
wire                          l1d_b_entry_write;
wire [22-$clog2(L1D_LINES):0] l1d_b_entry_write_value;

cpu_l1i #(
    .LINES(L1I_LINES)
) l1i (
    .clk(clk),
    .a_addr(fetch_addr[$clog2(L1I_LINES)+4:2]),
    .a_enable(fetch_en),
    .a_word(l1i_word),
    .a_entry(l1i_entry),
    .b_addr(l1i_b_addr),
    .b_write(l1i_b_write),
    .b_write_data(b_write_data),
    .b_entry_write(l1i_b_entry_write),
    .b_entry_write_value(l1i_b_entry_write_value)
);

cpu_l1d #(
    .LINES(L1D_LINES)
) l1d (
    .clk(clk),
    .a_addr(data_addr[$clog2(L1D_LINES)+4:2]),
    .a_word(l1d_word),
    .a_entry(l1d_entry),
    .a_write(l1d_write),
    .a_byte_enable(data_byte_enable),
    .a_write_data(data_write_data),
    .a_entry_write(l1d_write),
    .a_entry_write_value(l1d_entry_write_value),
    .b_addr(l1d_b_addr),
    .b_word(l1d_b_word),
    .b_entry(l1d_b_entry),
    .b_write(l1d_b_write),
    .b_write_data(b_write_data),
    .b_entry_write(l1d_b_entry_write),
    .b_entry_write_value(l1d_b_entry_write_value)
);

cpu_cache_controller #(
    .L1I_LINES(L1I_LINES),
    .L1D_LINES(L1D_LINES)
) cache_controller (
    .clk(clk),
    .reset(reset),
    .l1i_request(l1i_request),
    .l1i_addr(pc[25:2]),
    .l1i_take(l1i_take),
    .l1i_writing(l1i_writing),
    .l1d_request(l1d_request),
    .l1d_addr(ex_mem_addr[25:2]),
    .victim_dirty(victim_dirty),
    .victim_tag(victim_tag),
    .l1d_take(l1d_take),
    .l1d_writing(l1d_writing),
    .l1i_filling(l1i_filling),
    .l1d_filling(l1d_filling),
    .fill_addr(fill_addr),
    .clear_request(clear_request),
    .clear_data_only(clear_data_only),
    .clear_take(clear_take),
    .b_write_data(b_write_data),
    .l1i_b_addr(l1i_b_addr),
    .l1i_b_write(l1i_b_write),
    .l1i_b_entry_write(l1i_b_entry_write),
    .l1i_b_entry_write_value(l1i_b_entry_write_value),
    .l1d_b_addr(l1d_b_addr),
    .l1d_b_word(l1d_b_word),
    .l1d_b_entry(l1d_b_entry),
    .l1d_b_write(l1d_b_write),
    .l1d_b_entry_write(l1d_b_entry_write),
    .l1d_b_entry_write_value(l1d_b_entry_write_value),
    .sdram_req(sdram_req),
    .sdram_write(sdram_write),
    .sdram_line(sdram_line),
    .sdram_first(sdram_first),
    .sdram_take(sdram_take),
    .sdram_read_valid(sdram_read_valid),
    .sdram_read_word(sdram_read_word),
    .sdram_next(sdram_next),
    .sdram_write_word(sdram_write_word)
);

endmodule
