// Memory access stage (MEM)
// The EX/MEM register, then reading and writing the data memory, the result, and the
// redirect. An instruction commits when it leaves MEM, so this is where a write writes,
// and where the redirect happens.
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
    input  wire [31:0] ex_result,
    input  wire [31:0] ex_b,
    input  wire [31:0] ex_addr,
    input  wire        ex_sdram,
    input  wire        ex_redirect,
    input  wire [31:0] ex_target,

    // The data memory. It registers data_addr at every clock edge, and sdram_word is
    // the word at that address from then on. With sdram_write, it writes the bytes of
    // data_write_data that data_byte_enable selects at that edge.
    output wire [31:0] data_addr,
    output wire [31:0] data_write_data,
    output wire [3:0]  data_byte_enable,
    output wire        sdram_write,
    input  wire [31:0] sdram_word,

    // The EX/MEM register, as far as other stages need it
    output reg         ex_mem_valid,
    output reg  [31:0] ex_mem_pc,
    output reg  [31:0] ex_mem_instr,
    output reg         ex_mem_writes_dreg,
    output reg  [3:0]  ex_mem_dreg,
    output reg  [31:0] ex_mem_result,
    output reg         ex_mem_write,
    output reg  [31:0] ex_mem_addr,

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
reg [31:0] ex_mem_b;
reg        ex_mem_sdram;
reg        ex_mem_redirect;
// verilator lint_off UNUSEDSIGNAL
reg [31:0] ex_mem_target;    // bits [1:0] are for the bad-target check, in a later step
// verilator lint_on UNUSEDSIGNAL

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
        ex_mem_addr        <= ex_addr;
        ex_mem_b           <= ex_b;
        ex_mem_sdram       <= ex_sdram;
        ex_mem_redirect    <= ex_redirect;
        ex_mem_target      <= ex_target;
    end
end

// The data memory reads at the address of the instruction in EX, so a read has its word
// in MEM. Only a write in MEM uses its own address, to write at the end of MEM.
assign data_addr = ex_mem_valid && ex_mem_write ? ex_mem_addr : ex_addr;

cpu_write_align write_align (
    .data(ex_mem_b),
    .addr(ex_mem_addr[1:0]),
    .mem_byte(ex_mem_byte),
    .mem_half(ex_mem_half),
    .write_data(data_write_data),
    .byte_enable(data_byte_enable)
);

assign sdram_write = ex_mem_valid && ex_mem_write && ex_mem_sdram && !stall_mem;

// A read from a region without memory gives 0.
wire [31:0] mem_read_word = ex_mem_sdram ? sdram_word : 32'd0;
wire [31:0] mem_read_value;

cpu_read_align read_align (
    .word(mem_read_word),
    .addr(ex_mem_addr[1:0]),
    .mem_byte(ex_mem_byte),
    .mem_half(ex_mem_half),
    .sign_extend(ex_mem_sign_extend),
    .value(mem_read_value)
);

assign mem_value       = ex_mem_read ? mem_read_value : ex_mem_result;
assign redirect        = ex_mem_valid && ex_mem_redirect && !busy_mem;
assign redirect_target = {ex_mem_target[31:2], 2'b00};  // the PC is a multiple of 4

assign busy_mem = 1'b0;  // the data memory always answers in one cycle yet

endmodule
