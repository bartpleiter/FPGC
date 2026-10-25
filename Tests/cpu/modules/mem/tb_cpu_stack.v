// Testbench of cpu_stack. See tb_unit.vh for how the vectors are applied.
//
// The outputs are written at the rising edge, just before it clocks the push, pop or SP
// write, because that's what the instruction in MEM sees. One reset cycle comes before
// the vectors, so the stack starts from a known state.
`timescale 1ns / 1ps
module tb_cpu_stack;

`include "tb_unit.vh"

reg         reset;
reg         push;
reg  [31:0] push_value;
reg         pop;
reg         sp_write;
reg  [7:0]  sp_write_value;
wire [7:0]  sp;
wire [31:0] top;
wire        overflow;
wire        underflow;

cpu_stack dut (
    .clk(clk),
    .reset(reset),
    .push(push),
    .push_value(push_value),
    .pop(pop),
    .sp_write(sp_write),
    .sp_write_value(sp_write_value),
    .sp(sp),
    .top(top),
    .overflow(overflow),
    .underflow(underflow)
);

initial begin
    tb_open;
    {reset, push, push_value, pop, sp_write, sp_write_value} = {1'b1, 43'd0};
    @(negedge clk);
    while ($fscanf(vectors_fd, "%h %h %h %h %h %h %h\n",
                   label, reset, push, push_value, pop, sp_write, sp_write_value) == 7) begin
        @(posedge clk) $fwrite(outputs_fd, "%h %h %h %h\n", sp, top, overflow, underflow);
        @(negedge clk);
    end
    tb_close;
end

endmodule
