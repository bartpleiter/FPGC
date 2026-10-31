// Testbench of cpu_multiplier. See tb_unit.vh for how the vectors are applied.
//
// The rising edge clocks the first stage, so the output written at the next falling
// edge is the result of the operands in that same vector, when enable is 1.
`timescale 1ns / 1ps
module tb_cpu_multiplier;

`include "tb_unit.vh"

reg         enable;
reg  [3:0]  op;
reg  [31:0] a;
reg  [31:0] b;
wire [31:0] y;

cpu_multiplier dut (
    .clk(clk),
    .enable(enable),
    .op(op),
    .a(a),
    .b(b),
    .y(y)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h\n", label, enable, op, a, b) == 5)
        @(negedge clk) $fwrite(outputs_fd, "%h\n", y);
    tb_close;
end

endmodule
