// Memory access stage (MEM)
// The EX/MEM register, then the result, and the redirect. An instruction commits
// when it leaves MEM, so this is where the redirect happens.
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
    input  wire [31:0] ex_result,
    input  wire        ex_redirect,
    input  wire [31:0] ex_target,

    // The EX/MEM register, as far as other stages need it
    output reg         ex_mem_valid,
    output reg  [31:0] ex_mem_pc,
    output reg  [31:0] ex_mem_instr,
    output reg         ex_mem_writes_dreg,
    output reg  [3:0]  ex_mem_dreg,
    output reg  [31:0] ex_mem_result,

    // To WB, and the redirect to IF
    output wire [31:0] mem_value,
    output wire        redirect,
    output wire [31:0] redirect_target,
    output wire        busy_mem
);

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
        ex_mem_redirect    <= ex_redirect;
        ex_mem_target      <= ex_target;
    end
end

assign mem_value       = ex_mem_result;  // the ALU result is the only result yet
assign redirect        = ex_mem_valid && ex_mem_redirect && !busy_mem;
assign redirect_target = {ex_mem_target[31:2], 2'b00};  // the PC is a multiple of 4

assign busy_mem = 1'b0;  // memory accesses come in a later step

endmodule
