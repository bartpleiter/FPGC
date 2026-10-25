// Write-back stage (WB)
// The MEM/WB register, which writes the register file and drives the retire port.
module cpu_wb (
    input  wire        clk,
    input  wire        reset,
    input  wire        flush_wb,

    // From MEM
    input  wire        ex_mem_valid,
    input  wire [31:0] ex_mem_pc,
    input  wire [31:0] ex_mem_instr,
    input  wire        ex_mem_writes_dreg,
    input  wire [3:0]  ex_mem_dreg,
    input  wire [31:0] mem_value,
    input  wire        ex_mem_write,
    // verilator lint_off UNUSEDSIGNAL
    input  wire [31:0] ex_mem_addr,       // only the word, bits [31:2]
    // verilator lint_on UNUSEDSIGNAL
    input  wire [31:0] data_write_data,
    input  wire [3:0]  data_byte_enable,
    input  wire        redirect,
    input  wire [31:0] redirect_target,

    // The register write, to the register file in ID
    output wire        wb_writes_dreg,
    output reg  [3:0]  mem_wb_dreg,
    output reg  [31:0] mem_wb_value,

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

reg        mem_wb_valid;
reg [31:0] mem_wb_pc;
reg [31:0] mem_wb_instr;
reg        mem_wb_writes_dreg;
reg        mem_wb_write;
reg [31:0] mem_wb_write_addr;
reg [31:0] mem_wb_write_data;
reg [3:0]  mem_wb_write_be;
reg        mem_wb_redirect;
reg [31:0] mem_wb_target;

// WB never stalls.
always @(posedge clk) begin
    mem_wb_valid       <= ex_mem_valid && !flush_wb && !reset;
    mem_wb_pc          <= ex_mem_pc;
    mem_wb_instr       <= ex_mem_instr;
    mem_wb_writes_dreg <= ex_mem_writes_dreg;
    mem_wb_dreg        <= ex_mem_dreg;
    mem_wb_value       <= mem_value;
    mem_wb_write       <= ex_mem_write;
    mem_wb_write_addr  <= {ex_mem_addr[31:2], 2'b00};  // the trace shows the word
    mem_wb_write_data  <= data_write_data;
    mem_wb_write_be    <= data_byte_enable;
    mem_wb_redirect    <= redirect;
    mem_wb_target      <= redirect_target;
end

assign wb_writes_dreg = mem_wb_valid && mem_wb_writes_dreg;

assign retire_valid       = mem_wb_valid;
assign retire_pc          = mem_wb_pc;
assign retire_instr       = mem_wb_instr;
assign retire_writes_dreg = wb_writes_dreg;
assign retire_dreg        = mem_wb_dreg;
assign retire_dreg_value  = mem_wb_value;
assign retire_write       = mem_wb_valid && mem_wb_write;
assign retire_write_addr  = mem_wb_write_addr;
assign retire_write_data  = mem_wb_write_data;
assign retire_write_be    = mem_wb_write_be;
assign retire_push        = 1'b0;
assign retire_push_value  = 32'd0;
assign retire_pop         = 1'b0;
assign retire_pop_value   = 32'd0;
assign retire_redirect    = mem_wb_redirect;
assign retire_target      = mem_wb_target;
assign retire_irq         = 1'b0;
assign retire_irq_id      = 3'd0;

endmodule
