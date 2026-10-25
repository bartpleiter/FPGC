// Memory access stage (MEM)
// The EX/MEM register, then reading and writing the data memory, the stack and the
// CPU-internal I/O registers, the result, and the redirect. An instruction commits when
// it leaves MEM, so this is where a write writes, a push pushes, the error register
// records the events of an instruction, and where the redirect happens.
module cpu_mem (
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
    input  wire        id_ex_illegal,
    input  wire [31:0] ex_result,
    input  wire [31:0] ex_b,
    input  wire [31:0] ex_addr,
    input  wire        ex_sdram,
    input  wire        ex_rom,
    input  wire        ex_cpu_io,
    input  wire        ex_unmapped,
    input  wire        ex_redirect,
    input  wire [31:0] ex_target,

    // The data memory takes a request for the word at data_addr at a clock edge when
    // data_req is 1. It answers with data_ready, in the cycle after the request at the
    // earliest, and sdram_word is then the word until the next request. A write is
    // done at the clock edge when sdram_write is 1, which is only once the memory has
    // answered: it writes the bytes of data_write_data that data_byte_enable selects.
    output wire [31:0] data_addr,
    output wire        data_req,
    output wire [31:0] data_write_data,
    output wire [3:0]  data_byte_enable,
    output wire        sdram_write,
    input  wire [31:0] sdram_word,
    input  wire        data_ready,

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

    // To WB, and the redirect to IF
    output wire [31:0] mem_value,
    output wire        redirect,
    output wire [31:0] redirect_target,
    output wire        busy_mem
);

reg        ex_mem_read;
reg        ex_mem_byte;
reg        ex_mem_half;
reg        ex_mem_sign_extend;
reg        ex_mem_illegal;
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
        ex_mem_illegal     <= id_ex_illegal;
        ex_mem_addr        <= ex_addr;
        ex_mem_b           <= ex_b;
        ex_mem_sdram       <= ex_sdram;
        ex_mem_rom         <= ex_rom;
        ex_mem_cpu_io      <= ex_cpu_io;
        ex_mem_unmapped    <= ex_unmapped;
        ex_mem_redirect    <= ex_redirect;
        ex_mem_target      <= ex_target;
    end
end

// The instruction commits at the end of this cycle.
wire commit = ex_mem_valid && !stall_mem;

// A read or write to SDRAM makes its request at the clock edge where it enters MEM, with
// the address from EX, so a read can have its word in MEM right away. The instruction
// can't be cancelled any more once it's in MEM, so each one makes exactly one request.
// Only a write in MEM uses its own address, to write at the end of MEM.
wire enter_mem = id_ex_valid && !stall_mem && !flush_mem;

assign data_req  = enter_mem && (id_ex_read || id_ex_write) && ex_sdram;
assign data_addr = ex_mem_valid && ex_mem_write ? ex_mem_addr : ex_addr;

cpu_write_align write_align (
    .data(ex_mem_b),
    .addr(ex_mem_addr[1:0]),
    .mem_byte(ex_mem_byte),
    .mem_half(ex_mem_half),
    .write_data(data_write_data),
    .byte_enable(data_byte_enable)
);

assign sdram_write = commit && ex_mem_write && ex_mem_sdram;

// The stack

wire [7:0]  sp;
wire [31:0] stack_top;
wire        sp_write;
wire        overflow;
wire        underflow;

cpu_stack stack (
    .clk(clk),
    .reset(reset),
    .push(commit && ex_mem_push),
    .push_value(ex_mem_b),
    .pop(commit && ex_mem_pop),
    .sp_write(sp_write),
    .sp_write_value(ex_mem_b[7:0]),
    .sp(sp),
    .top(stack_top),
    .overflow(overflow),
    .underflow(underflow)
);

// The CPU-internal I/O registers, and the events for the error register

wire        word = !ex_mem_byte && !ex_mem_half;
wire        io_bad_read;
wire        io_bad_write;
wire [31:0] io_read_value;
wire        target_code;

// The jump target only needs to be in a region that can hold code.
/* verilator lint_off PINCONNECTEMPTY */
cpu_memory_map target_map (
    .addr(ex_mem_target),
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
wire bad_target  = ex_mem_redirect && (ex_mem_target[1:0] != 2'd0 || !target_code);

// Bit n is the event of kind n + 1. The stack gives its events only when it commits.
wire [5:0] events = {underflow, overflow,
                     {bad_target, bad_address, unaligned, ex_mem_illegal} & {4{commit}}};

cpu_io_regs io_regs (
    .clk(clk),
    .reset(reset),
    .addr(ex_mem_addr[3:2]),
    .word(word),
    .bad_read(io_bad_read),
    .bad_write(io_bad_write),
    .read_value(io_read_value),
    .write(commit && ex_mem_write && ex_mem_cpu_io),
    .write_data(ex_mem_b),
    .sp(sp),
    .sp_write(sp_write),
    .events(events),
    .events_pc(ex_mem_pc)
);

// The result. A read from a region without memory gives 0.

wire [31:0] mem_read_word = ex_mem_sdram  ? sdram_word    :
                            ex_mem_cpu_io ? io_read_value :
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

assign mem_value       = ex_mem_read ? mem_read_value :
                         ex_mem_pop  ? stack_top      :
                                       ex_mem_result;
assign redirect        = ex_mem_valid && ex_mem_redirect && !busy_mem;
assign redirect_target = {ex_mem_target[31:2], 2'b00};  // the PC is a multiple of 4

// A read or write to SDRAM waits in MEM until the data memory has answered.
assign busy_mem = ex_mem_valid && (ex_mem_read || ex_mem_write) && ex_mem_sdram &&
                  !data_ready;

endmodule
