// Testbench of cpu_divider. See tb_unit.vh for how the vectors are applied.
//
// The outputs are written at the rising edge, just before it clocks the start, because
// that's what the divide in EX sees in that cycle. The result is only defined when the
// divider isn't busy, so while it's busy, the testbench writes 0 instead.
`timescale 1ns / 1ps
module tb_cpu_divider;

`include "tb_unit.vh"

reg         start;
reg  [3:0]  op;
reg  [31:0] a;
reg  [31:0] b;
wire        busy;
wire [31:0] y;

cpu_divider dut (
    .clk(clk),
    .start(start),
    .op(op),
    .a(a),
    .b(b),
    .busy(busy),
    .y(y)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h\n", label, start, op, a, b) == 5) begin
        @(posedge clk) $fwrite(outputs_fd, "%h %h\n", busy, busy ? 32'd0 : y);
        @(negedge clk);
    end
    tb_close;
end

endmodule
