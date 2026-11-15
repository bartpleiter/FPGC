// Memory access stage (MEM)
module cpu_mem #(
    parameter INTERRUPTS = 7,   // the number of interrupt inputs
    parameter L1D_LINES  = 128  // the number of lines of the L1D, a power of two
) (
    input  wire        clk,
    input  wire        reset,
    input  wire        stall_mem,
    input  wire        flush_mem,

    // From EX
    input  wire        id_ex_valid,
    input  wire [31:0] id_ex_pc,
    input  wire [31:0] id_ex_instr,
    input  wire        id_ex_writes_dreg,
    input  wire [3:0]  id_ex_dreg,
    input  wire        id_ex_read,
    input  wire        id_ex_write,
    input  wire        id_ex_byte,
    input  wire        id_ex_half,
    input  wire        id_ex_sign_extend,
    input  wire        id_ex_push,
    input  wire        id_ex_pop,
    input  wire        id_ex_intid,
    input  wire        id_ex_reti,
    input  wire        id_ex_ccache,
    input  wire        id_ex_ccached,
    input  wire        id_ex_illegal,
    input  wire        id_ex_mult,
    input  wire        id_ex_div,
    input  wire [31:0] ex_result,
    input  wire [31:0] ex_b,
    input  wire [31:0] ex_addr,
    input  wire        ex_sdram,
    input  wire        ex_rom,
    input  wire        ex_cpu_io,
    input  wire        ex_unmapped,
    input  wire        ex_redirect,
    input  wire [31:0] ex_target,
    input  wire        ex_interrupt,
    input  wire [31:0] ex_mult_result,
    input  wire [31:0] ex_div_result,

    output wire [31:0]                    data_addr,
    output wire [31:0]                    data_write_data,
    output wire [3:0]                     data_byte_enable,
    output wire                           l1d_write,
    output wire [22-$clog2(L1D_LINES):0]  l1d_entry_write_value,
    input  wire [31:0]                    l1d_word,
    input  wire [22-$clog2(L1D_LINES):0]  l1d_entry,

    output reg                            l1d_request,
    output reg                            victim_dirty,
    output reg  [20-$clog2(L1D_LINES):0]  victim_tag,
    input  wire                           l1d_take,
    input  wire                           l1d_writing,

    input  wire                           l1d_filling,
    input  wire [25:2]                    fill_addr,
    input  wire                           sdram_read_valid,
    input  wire [31:0]                    sdram_read_word,

    output reg                            clear_request,
    output reg                            clear_data_only,
    input  wire                           clear_take,

    input  wire [INTERRUPTS:1] interrupts,
    output wire        interrupt_pending,
    output wire        interrupts_enabled,

    // The EX/MEM register, as far as other stages need it
    output reg         ex_mem_valid,
    output reg  [31:0] ex_mem_pc,
    output reg  [31:0] ex_mem_instr,
    output reg         ex_mem_writes_dreg,
    output reg  [3:0]  ex_mem_dreg,
    output reg  [31:0] ex_mem_result,
    output reg         ex_mem_write,
    output reg  [31:0] ex_mem_addr,
    output reg  [31:0] ex_mem_b,          // the data of a write or push
    output reg         ex_mem_push,
    output reg         ex_mem_pop,
    output reg         ex_mem_interrupt,

    // To WB, and the redirect to IF
    output wire [31:0] mem_value,
    output wire [7:0]  interrupt_id,
    output wire        redirect,
    output wire [31:0] redirect_target,
    output wire        busy_mem,
    output wire        l1d_miss,
    output wire        l1d_wait
);

localparam INDEX = $clog2(L1D_LINES);

reg        ex_mem_read;
reg        ex_mem_byte;
reg        ex_mem_half;
reg        ex_mem_sign_extend;
reg        ex_mem_intid;
reg        ex_mem_reti;
reg        ex_mem_ccache;
reg        ex_mem_ccached;
reg        ex_mem_illegal;
reg        ex_mem_mult;
reg        ex_mem_div;
reg [31:0] ex_mem_div_result;
reg        ex_mem_sdram;
reg        ex_mem_rom;
reg        ex_mem_cpu_io;
reg        ex_mem_unmapped;
reg        ex_mem_redirect;
reg [31:0] ex_mem_target;

always @(posedge clk) begin
    if (reset || flush_mem)
        ex_mem_valid <= 1'b0;
    else if (!stall_mem)
        ex_mem_valid <= id_ex_valid;
    if (!stall_mem) begin
        ex_mem_pc          <= id_ex_pc;
        ex_mem_instr       <= id_ex_instr;
        ex_mem_writes_dreg <= id_ex_writes_dreg;
        ex_mem_dreg        <= id_ex_dreg;
        ex_mem_result      <= ex_result;
        ex_mem_read        <= id_ex_read;
        ex_mem_write       <= id_ex_write;
        ex_mem_byte        <= id_ex_byte;
        ex_mem_half        <= id_ex_half;
        ex_mem_sign_extend <= id_ex_sign_extend;
        ex_mem_push        <= id_ex_push;
        ex_mem_pop         <= id_ex_pop;
        ex_mem_intid       <= id_ex_intid;
        ex_mem_reti        <= id_ex_reti;
        ex_mem_ccache      <= id_ex_ccache;
        ex_mem_ccached     <= id_ex_ccached;
        ex_mem_illegal     <= id_ex_illegal;
        ex_mem_mult        <= id_ex_mult;
        ex_mem_div         <= id_ex_div;
        ex_mem_div_result  <= ex_div_result;
        ex_mem_addr        <= ex_addr;
        ex_mem_b           <= ex_b;
        ex_mem_sdram       <= ex_sdram;
        ex_mem_rom         <= ex_rom;
        ex_mem_cpu_io      <= ex_cpu_io;
        ex_mem_unmapped    <= ex_unmapped;
        ex_mem_redirect    <= ex_redirect;
        ex_mem_target      <= ex_target;
        ex_mem_interrupt   <= ex_interrupt;
    end
end

wire commit = ex_mem_valid && !stall_mem;
wire commit_early = ex_mem_valid && !busy_mem;

wire sdram_access = ex_mem_valid && (ex_mem_read || ex_mem_write) && ex_mem_sdram;

reg l1d_current;

wire l1d_match = l1d_entry[22-INDEX] && l1d_entry[20-INDEX:0] == ex_mem_addr[25:INDEX+5];
wire l1d_hit   = l1d_current && l1d_match;

// Early restart
wire l1d_bypass = ex_mem_valid && ex_mem_read && ex_mem_sdram && sdram_read_valid &&
                  l1d_filling && fill_addr == ex_mem_addr[25:2];

assign l1d_miss = sdram_access && !l1d_bypass && !l1d_hit;

assign l1d_wait = sdram_access && !l1d_bypass && !l1d_current;

wire l1d_reread = sdram_access && !l1d_current && !l1d_writing && !l1d_request;

assign data_addr = (ex_mem_valid && ex_mem_write) || l1d_reread ? ex_mem_addr : ex_addr;

always @(posedge clk) begin
    l1d_current <= (!stall_mem || l1d_reread) && !l1d_writing;

    if (reset || l1d_take) begin
        l1d_request <= 1'b0;
    end else if (sdram_access && l1d_current && !l1d_match) begin
        l1d_request  <= 1'b1;
        victim_dirty <= l1d_entry[22-INDEX] && l1d_entry[21-INDEX];
        victim_tag   <= l1d_entry[20-INDEX:0];
    end
end

cpu_write_align write_align (
    .data(ex_mem_b),
    .addr(ex_mem_addr[1:0]),
    .mem_byte(ex_mem_byte),
    .mem_half(ex_mem_half),
    .write_data(data_write_data),
    .byte_enable(data_byte_enable)
);

assign l1d_write             = commit && ex_mem_write && ex_mem_sdram;
assign l1d_entry_write_value = {2'b11, ex_mem_addr[25:INDEX+5]};

// CCACHE and CCACHED.
wire clear = ex_mem_valid && ex_mem_ccache;
reg  clear_started;

always @(posedge clk) begin
    if (reset || clear_take)
        clear_request <= 1'b0;
    else if (clear && !clear_started)
        clear_request <= 1'b1;
    if (clear && !clear_started)
        clear_data_only <= ex_mem_ccached;
    if (reset || !busy_mem)
        clear_started <= 1'b0;
    else if (clear)
        clear_started <= 1'b1;
end

wire [7:0]  sp;
wire [31:0] stack_top;
wire        sp_write;
wire        overflow;
wire        underflow;

cpu_stack stack (
    .clk(clk),
    .reset(reset),
    .push(commit_early && ex_mem_push),
    .push_value(ex_mem_b),
    .pop(commit_early && ex_mem_pop),
    .sp_write(sp_write),
    .sp_write_value(ex_mem_b[7:0]),
    .sp(sp),
    .top(stack_top),
    .overflow(overflow),
    .underflow(underflow)
);

wire take_interrupt = commit_early && ex_mem_interrupt;

cpu_interrupts #(
    .INTERRUPTS(INTERRUPTS)
) interrupt_controller (
    .clk(clk),
    .reset(reset),
    .interrupts(interrupts),
    .take(take_interrupt),
    .pending(interrupt_pending),
    .id(interrupt_id)
);

wire        word = !ex_mem_byte && !ex_mem_half;
wire        io_bad_read;
wire        io_bad_write;
wire [31:0] io_read_value;
wire [31:0] pc_backup;
wire [7:0]  last_interrupt;
wire        target_code;

wire        jumps  = (ex_mem_redirect && !ex_mem_interrupt) || ex_mem_reti;
wire [31:0] target = ex_mem_reti ? pc_backup : ex_mem_target;

/* verilator lint_off PINCONNECTEMPTY */
cpu_memory_map target_map (
    .addr(target),
    .sdram(),
    .io(),
    .rom(),
    .vram32(),
    .vram8(),
    .vrampx(),
    .palette(),
    .cpu_io(),
    .code(target_code),
    .unmapped()
);
/* verilator lint_on PINCONNECTEMPTY */

wire unaligned   = (ex_mem_read || ex_mem_write) &&
                   (word ? ex_mem_addr[1:0] != 2'd0 : ex_mem_half && ex_mem_addr[0]);
wire bad_address = ((ex_mem_read || ex_mem_write) && ex_mem_unmapped) ||
                   (ex_mem_write && ex_mem_rom) ||
                   (ex_mem_cpu_io && ex_mem_read && io_bad_read) ||
                   (ex_mem_cpu_io && ex_mem_write && io_bad_write);
wire bad_target  = jumps && (target[1:0] != 2'd0 || !target_code);

wire [5:0] events = {underflow, overflow,
                     {bad_target, bad_address, unaligned, ex_mem_illegal} &
                     {4{commit_early}}};

cpu_io_regs io_regs (
    .clk(clk),
    .reset(reset),
    .addr(ex_mem_addr[4:2]),
    .word(word),
    .bad_read(io_bad_read),
    .bad_write(io_bad_write),
    .read_value(io_read_value),
    .write(commit_early && ex_mem_write && ex_mem_cpu_io),
    .write_data(ex_mem_b),
    .sp(sp),
    .sp_write(sp_write),
    .pc(ex_mem_pc),
    .events(events),
    .take_interrupt(take_interrupt),
    .interrupt_id(interrupt_id),
    .reti(commit_early && ex_mem_reti),
    .pc_backup(pc_backup),
    .interrupts_enabled(interrupts_enabled),
    .last_interrupt(last_interrupt)
);

wire [31:0] mem_read_word = l1d_bypass    ? sdram_read_word :
                            ex_mem_sdram  ? l1d_word        :
                            ex_mem_cpu_io ? io_read_value   :
                                            32'd0;
wire [31:0] mem_read_value;

cpu_read_align read_align (
    .word(mem_read_word),
    .addr(ex_mem_addr[1:0]),
    .mem_byte(ex_mem_byte),
    .mem_half(ex_mem_half),
    .sign_extend(ex_mem_sign_extend),
    .value(mem_read_value)
);

assign mem_value = ex_mem_read  ? mem_read_value          :
                   ex_mem_pop   ? stack_top               :
                   ex_mem_mult  ? ex_mult_result          :
                   ex_mem_div   ? ex_mem_div_result       :
                   ex_mem_intid ? {24'd0, last_interrupt} :
                                  ex_mem_result;

assign redirect        = ex_mem_valid && !busy_mem &&
                         (ex_mem_redirect || ex_mem_reti || ex_mem_ccache);
assign redirect_target = ex_mem_interrupt ? 32'h4 : {target[31:2], 2'b00};

assign busy_mem = clear && (!clear_started || clear_request || l1d_writing);

`ifndef SYNTHESIS
// Simulation only check to catch implementation bugs
always @(posedge clk)
    if (!reset && commit && sdram_access && !l1d_current && !l1d_bypass)
        $fatal(1, "cpu_mem moved on with an L1D output that isn't current");
`endif

endmodule
